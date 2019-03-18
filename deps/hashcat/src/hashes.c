/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "memory.h"
#include "event.h"
#include "hashes.h"

#include "convert.h"
#include "debugfile.h"
#include "filehandling.h"
#include "hlfmt.h"
#include "interface.h"
#include "logfile.h"
#include "loopback.h"
#include "mpsp.h"
#include "opencl.h"
#include "outfile.h"
#include "potfile.h"
#include "rp.h"
#include "shared.h"
#include "thread.h"
#include "timer.h"
#include "locking.h"

int sort_by_digest_p0p1 (const void *v1, const void *v2, void *v3)
{
  const u32 *d1 = (const u32 *) v1;
  const u32 *d2 = (const u32 *) v2;

  hashconfig_t *hashconfig = (hashconfig_t *) v3;

  const u32 dgst_pos0 = hashconfig->dgst_pos0;
  const u32 dgst_pos1 = hashconfig->dgst_pos1;
  const u32 dgst_pos2 = hashconfig->dgst_pos2;
  const u32 dgst_pos3 = hashconfig->dgst_pos3;

  if (d1[dgst_pos3] > d2[dgst_pos3]) return  1;
  if (d1[dgst_pos3] < d2[dgst_pos3]) return -1;
  if (d1[dgst_pos2] > d2[dgst_pos2]) return  1;
  if (d1[dgst_pos2] < d2[dgst_pos2]) return -1;
  if (d1[dgst_pos1] > d2[dgst_pos1]) return  1;
  if (d1[dgst_pos1] < d2[dgst_pos1]) return -1;
  if (d1[dgst_pos0] > d2[dgst_pos0]) return  1;
  if (d1[dgst_pos0] < d2[dgst_pos0]) return -1;

  return 0;
}

int sort_by_salt (const void *v1, const void *v2)
{
  const salt_t *s1 = (const salt_t *) v1;
  const salt_t *s2 = (const salt_t *) v2;

  const int res1 = (int) s1->salt_len - (int) s2->salt_len;

  if (res1 != 0) return (res1);

  const int res2 = (int) s1->salt_iter - (int) s2->salt_iter;

  if (res2 != 0) return (res2);

  for (int n = 0; n < 64; n++)
  {
    if (s1->salt_buf[n] > s2->salt_buf[n]) return  1;
    if (s1->salt_buf[n] < s2->salt_buf[n]) return -1;
  }

  for (int n = 0; n < 64; n++)
  {
    if (s1->salt_buf_pc[n] > s2->salt_buf_pc[n]) return  1;
    if (s1->salt_buf_pc[n] < s2->salt_buf_pc[n]) return -1;
  }

  return 0;
}

int sort_by_hash (const void *v1, const void *v2, void *v3)
{
  const hash_t *h1 = (const hash_t *) v1;
  const hash_t *h2 = (const hash_t *) v2;

  hashconfig_t *hashconfig = (hashconfig_t *) v3;

  if (hashconfig->is_salted == true)
  {
    const salt_t *s1 = h1->salt;
    const salt_t *s2 = h2->salt;

    int res = sort_by_salt (s1, s2);

    if (res != 0) return (res);
  }

  const void *d1 = h1->digest;
  const void *d2 = h2->digest;

  return sort_by_digest_p0p1 (d1, d2, v3);
}

int sort_by_hash_no_salt (const void *v1, const void *v2, void *v3)
{
  const hash_t *h1 = (const hash_t *) v1;
  const hash_t *h2 = (const hash_t *) v2;

  const void *d1 = h1->digest;
  const void *d2 = h2->digest;

  return sort_by_digest_p0p1 (d1, d2, v3);
}

int save_hash (hashcat_ctx_t *hashcat_ctx)
{
  hashes_t        *hashes       = hashcat_ctx->hashes;
  hashconfig_t    *hashconfig   = hashcat_ctx->hashconfig;
  user_options_t  *user_options = hashcat_ctx->user_options;

  const char *hashfile = hashes->hashfile;

  char *new_hashfile;
  char *old_hashfile;

  hc_asprintf (&new_hashfile, "%s.new", hashfile);
  hc_asprintf (&old_hashfile, "%s.old", hashfile);

  unlink (new_hashfile);

  char separator = hashconfig->separator;

  FILE *fp = fopen (new_hashfile, "wb");

  if (fp == NULL)
  {
    event_log_error (hashcat_ctx, "%s: %s", new_hashfile, strerror (errno));

    free (new_hashfile);
    free (old_hashfile);

    return -1;
  }

  if (lock_file (fp) == -1)
  {
    fclose (fp);

    event_log_error (hashcat_ctx, "%s: %s", new_hashfile, strerror (errno));

    free (new_hashfile);
    free (old_hashfile);

    return -1;
  }

  u8 *out_buf = (u8 *) hcmalloc (HCBUFSIZ_LARGE);

  for (u32 salt_pos = 0; salt_pos < hashes->salts_cnt; salt_pos++)
  {
    if (hashes->salts_shown[salt_pos] == 1) continue;

    salt_t *salt_buf = &hashes->salts_buf[salt_pos];

    for (u32 digest_pos = 0; digest_pos < salt_buf->digests_cnt; digest_pos++)
    {
      u32 idx = salt_buf->digests_offset + digest_pos;

      if (hashes->digests_shown[idx] == 1) continue;

      if (hashconfig->opts_type & OPTS_TYPE_BINARY_HASHFILE)
      {
        if ((hashconfig->hash_mode == 2500) || (hashconfig->hash_mode == 2501))
        {
          hccapx_t hccapx;

          to_hccapx_t (hashcat_ctx, &hccapx, salt_pos, digest_pos);

          hc_fwrite (&hccapx, sizeof (hccapx_t), 1, fp);
        }
        else
        {
          // TODO
        }
      }
      else
      {
        if (user_options->username == true)
        {
          user_t *user = hashes->hash_info[idx]->user;

          u32 i;

          for (i = 0; i < user->user_len; i++) fputc (user->user_name[i], fp);

          fputc (separator, fp);
        }

        out_buf[0] = 0;

        ascii_digest (hashcat_ctx, (char *) out_buf, HCBUFSIZ_LARGE, salt_pos, digest_pos);

        fprintf (fp, "%s" EOL, out_buf);
      }
    }
  }

  hcfree (out_buf);

  fflush (fp);

  fclose (fp);

  unlink (old_hashfile);

  if (rename (hashfile, old_hashfile) != 0)
  {
    event_log_error (hashcat_ctx, "Rename file '%s' to '%s': %s", hashfile, old_hashfile, strerror (errno));

    free (new_hashfile);
    free (old_hashfile);

    return -1;
  }

  unlink (hashfile);

  if (rename (new_hashfile, hashfile) != 0)
  {
    event_log_error (hashcat_ctx, "Rename file '%s' to '%s': %s", new_hashfile, hashfile, strerror (errno));

    free (new_hashfile);
    free (old_hashfile);

    return -1;
  }

  unlink (old_hashfile);

  free (new_hashfile);
  free (old_hashfile);

  return 0;
}

void check_hash (hashcat_ctx_t *hashcat_ctx, hc_device_param_t *device_param, plain_t *plain)
{
  debugfile_ctx_t *debugfile_ctx = hashcat_ctx->debugfile_ctx;
  loopback_ctx_t  *loopback_ctx  = hashcat_ctx->loopback_ctx;
  hashes_t        *hashes        = hashcat_ctx->hashes;

  const u32 salt_pos    = plain->salt_pos;
  const u32 digest_pos  = plain->digest_pos;  // relative

  // hash

  u8 *out_buf = hashes->out_buf;

  out_buf[0] = 0;

  ascii_digest (hashcat_ctx, (char *) out_buf, HCBUFSIZ_LARGE, salt_pos, digest_pos);

  // plain

  u32 plain_buf[64] = { 0 };

  u8 *plain_ptr = (u8 *) plain_buf;
  int plain_len = 0;

  build_plain (hashcat_ctx, device_param, plain, plain_buf, &plain_len);

  // crackpos

  u64 crackpos = 0;

  build_crackpos (hashcat_ctx, device_param, plain, &crackpos);

  // debug

  u8  debug_rule_buf[RP_PASSWORD_SIZE] = { 0 };
  int debug_rule_len  = 0; // -1 error

  u8  debug_plain_ptr[RP_PASSWORD_SIZE] = { 0 };
  int debug_plain_len = 0;

  build_debugdata (hashcat_ctx, device_param, plain, debug_rule_buf, &debug_rule_len, debug_plain_ptr, &debug_plain_len);

  // no need for locking, we're in a mutex protected function

  potfile_write_append (hashcat_ctx, (char *) out_buf, plain_ptr, plain_len);

  // outfile, can be either to file or stdout
  // if an error occurs opening the file, send to stdout as fallback
  // the fp gets opened for each cracked hash so that the user can modify (move) the outfile while hashcat runs

  outfile_write_open (hashcat_ctx);

  u8 *tmp_buf = hashes->tmp_buf;

  tmp_buf[0] = 0;

  const int tmp_len = outfile_write (hashcat_ctx, (char *) out_buf, plain_ptr, plain_len, crackpos, NULL, 0, (char *) tmp_buf);

  outfile_write_close (hashcat_ctx);

  EVENT_DATA (EVENT_CRACKER_HASH_CRACKED, tmp_buf, tmp_len);

  // if enabled, update also the loopback file

  if (loopback_ctx->fp != NULL)
  {
    loopback_write_append (hashcat_ctx, plain_ptr, plain_len);
  }

  // if enabled, update also the (rule) debug file

  if (debugfile_ctx->fp != NULL)
  {
    // the next check implies that:
    // - (user_options->attack_mode == ATTACK_MODE_STRAIGHT)
    // - debug_mode > 0

    if ((debug_plain_len > 0) || (debug_rule_len > 0))
    {
      debugfile_write_append (hashcat_ctx, debug_rule_buf, debug_rule_len, plain_ptr, plain_len, debug_plain_ptr, debug_plain_len);
    }
  }
}

int check_cracked (hashcat_ctx_t *hashcat_ctx, hc_device_param_t *device_param, const u32 salt_pos)
{
  cpt_ctx_t    *cpt_ctx    = hashcat_ctx->cpt_ctx;
  hashconfig_t *hashconfig = hashcat_ctx->hashconfig;
  hashes_t     *hashes     = hashcat_ctx->hashes;
  status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  salt_t *salt_buf = &hashes->salts_buf[salt_pos];

  u32 num_cracked;

  cl_int CL_err;

  CL_err = hc_clEnqueueReadBuffer (hashcat_ctx, device_param->command_queue, device_param->d_result, CL_TRUE, 0, sizeof (u32), &num_cracked, 0, NULL, NULL);

  if (CL_err != CL_SUCCESS)
  {
    event_log_error (hashcat_ctx, "clEnqueueReadBuffer(): %s", val2cstr_cl (CL_err));

    return -1;
  }

  if (num_cracked)
  {
    plain_t *cracked = (plain_t *) hccalloc (num_cracked, sizeof (plain_t));

    CL_err = hc_clEnqueueReadBuffer (hashcat_ctx, device_param->command_queue, device_param->d_plain_bufs, CL_TRUE, 0, num_cracked * sizeof (plain_t), cracked, 0, NULL, NULL);

    if (CL_err != CL_SUCCESS)
    {
      event_log_error (hashcat_ctx, "clEnqueueReadBuffer(): %s", val2cstr_cl (CL_err));

      return -1;
    }

    u32 cpt_cracked = 0;

    hc_thread_mutex_lock (status_ctx->mux_display);

    for (u32 i = 0; i < num_cracked; i++)
    {
      const u32 hash_pos = cracked[i].hash_pos;

      if (hashes->digests_shown[hash_pos] == 1) continue;

      if ((hashconfig->opts_type & OPTS_TYPE_PT_NEVERCRACK) == 0)
      {
        hashes->digests_shown[hash_pos] = 1;

        hashes->digests_done++;

        cpt_cracked++;

        salt_buf->digests_done++;

        if (salt_buf->digests_done == salt_buf->digests_cnt)
        {
          hashes->salts_shown[salt_pos] = 1;

          hashes->salts_done++;
        }
      }

      if (hashes->salts_done == hashes->salts_cnt) mycracked (hashcat_ctx);

      check_hash (hashcat_ctx, device_param, &cracked[i]);
    }

    hc_thread_mutex_unlock (status_ctx->mux_display);

    hcfree (cracked);

    if (cpt_cracked > 0)
    {
      hc_thread_mutex_lock (status_ctx->mux_display);

      cpt_ctx->cpt_buf[cpt_ctx->cpt_pos].timestamp = hc_time (NULL);
      cpt_ctx->cpt_buf[cpt_ctx->cpt_pos].cracked   = cpt_cracked;

      cpt_ctx->cpt_pos++;

      cpt_ctx->cpt_total += cpt_cracked;

      if (cpt_ctx->cpt_pos == CPT_CACHE) cpt_ctx->cpt_pos = 0;

      hc_thread_mutex_unlock (status_ctx->mux_display);
    }

    if (hashconfig->opts_type & OPTS_TYPE_PT_NEVERCRACK)
    {
      // we need to reset cracked state on the device
      // otherwise host thinks again and again the hash was cracked
      // and returns invalid password each time

      memset (hashes->digests_shown_tmp, 0, salt_buf->digests_cnt * sizeof (u32));

      CL_err = hc_clEnqueueWriteBuffer (hashcat_ctx, device_param->command_queue, device_param->d_digests_shown, CL_TRUE, salt_buf->digests_offset * sizeof (u32), salt_buf->digests_cnt * sizeof (u32), &hashes->digests_shown_tmp[salt_buf->digests_offset], 0, NULL, NULL);

      if (CL_err != CL_SUCCESS)
      {
        event_log_error (hashcat_ctx, "clEnqueueWriteBuffer(): %s", val2cstr_cl (CL_err));

        return -1;
      }
    }

    num_cracked = 0;

    CL_err = hc_clEnqueueWriteBuffer (hashcat_ctx, device_param->command_queue, device_param->d_result, CL_TRUE, 0, sizeof (u32), &num_cracked, 0, NULL, NULL);

    if (CL_err != CL_SUCCESS)
    {
      event_log_error (hashcat_ctx, "clEnqueueWriteBuffer(): %s", val2cstr_cl (CL_err));

      return -1;
    }
  }

  return 0;
}

int hashes_init_stage1 (hashcat_ctx_t *hashcat_ctx)
{
  hashconfig_t         *hashconfig         = hashcat_ctx->hashconfig;
  hashes_t             *hashes             = hashcat_ctx->hashes;
  user_options_t       *user_options       = hashcat_ctx->user_options;
  user_options_extra_t *user_options_extra = hashcat_ctx->user_options_extra;

  char *hash_or_file = user_options_extra->hc_hash;

  /**
   * load hashes, part I: find input mode, count hashes
   */

  u32 hashlist_mode   = 0;
  u32 hashlist_format = HLFMT_HASHCAT;

  u32 hashes_avail = 0;

  if ((user_options->benchmark == false) && (user_options->stdout_flag == false) && (user_options->keyspace == false))
  {
    hashlist_mode = (hc_path_exist (hash_or_file) == true) ? HL_MODE_FILE : HL_MODE_ARG;

    if (hashconfig->opts_type & OPTS_TYPE_BINARY_HASHFILE)
    {
      hashlist_mode = HL_MODE_ARG;

      char *hashfile = hash_or_file;

      hashes->hashfile = hashfile;
    }

    if (hashlist_mode == HL_MODE_ARG)
    {
      if ((hashconfig->hash_mode == 2500) || (hashconfig->hash_mode == 2501))
      {
        hc_stat_t st;

        if (hc_stat (hashes->hashfile, &st) == -1)
        {
          event_log_error (hashcat_ctx, "%s: %s", hashes->hashfile, strerror (errno));

          return -1;
        }

        // 392 = old hccap_t size

        if ((st.st_size % 392) == 0)
        {
          const int rc = check_old_hccap (hashes->hashfile);

          if (rc == 1)
          {
            event_log_error (hashcat_ctx, "%s: Old hccap format detected! You need to update: https://hashcat.net/q/hccapx", hashes->hashfile);

            return -1;
          }
        }

        hashes_avail = st.st_size / sizeof (hccapx_t);
      }
      else if (hashconfig->hash_mode == 14600)
      {
        hc_stat_t st;

        if (hc_stat (hashes->hashfile, &st) == -1)
        {
          event_log_error (hashcat_ctx, "%s: %s", hashes->hashfile, strerror (errno));

          return -1;
        }

        hashes_avail = LUKS_NUMKEYS;
      }
      else
      {
        hashes_avail = 1;
      }
    }
    else if (hashlist_mode == HL_MODE_FILE)
    {
      char *hashfile = hash_or_file;

      hashes->hashfile = hashfile;

      FILE *fp = NULL;

      if ((fp = fopen (hashfile, "rb")) == NULL)
      {
        event_log_error (hashcat_ctx, "%s: %s", hashfile, strerror (errno));

        return -1;
      }

      EVENT_DATA (EVENT_HASHLIST_COUNT_LINES_PRE, hashfile, strlen (hashfile));

      hashes_avail = count_lines (fp);

      EVENT_DATA (EVENT_HASHLIST_COUNT_LINES_POST, hashfile, strlen (hashfile));

      rewind (fp);

      if (hashes_avail == 0)
      {
        event_log_error (hashcat_ctx, "hashfile is empty or corrupt.");

        fclose (fp);

        return -1;
      }

      hashlist_format = hlfmt_detect (hashcat_ctx, fp, 100); // 100 = max numbers to "scan". could be hashes_avail, too

      if ((user_options->remove == true) && (hashlist_format != HLFMT_HASHCAT))
      {
        event_log_error (hashcat_ctx, "Use of --remove is not supported in native hashfile-format mode.");

        fclose (fp);

        return -1;
      }

      fclose (fp);
    }
  }
  else
  {
    hashlist_mode = HL_MODE_ARG;

    hashes_avail = 1;
  }

  if (hashconfig->hash_mode == 3000) hashes_avail *= 2;

  hashes->hashlist_mode   = hashlist_mode;
  hashes->hashlist_format = hashlist_format;

  /**
   * load hashes, part II: allocate required memory, set pointers
   */

  hash_t *hashes_buf     = (hash_t *) hccalloc (hashes_avail, sizeof (hash_t));
  void   *digests_buf    =            hccalloc (hashes_avail, hashconfig->dgst_size);
  salt_t *salts_buf      = NULL;
  void   *esalts_buf     = NULL;
  void   *hook_salts_buf = NULL;

  if ((user_options->username == true) || (hashconfig->opts_type & OPTS_TYPE_HASH_COPY) || (hashconfig->opts_type & OPTS_TYPE_HASH_SPLIT))
  {
    u32 hash_pos;

    for (hash_pos = 0; hash_pos < hashes_avail; hash_pos++)
    {
      hashinfo_t *hash_info = (hashinfo_t *) hcmalloc (sizeof (hashinfo_t));

      hashes_buf[hash_pos].hash_info = hash_info;

      if (user_options->username == true)
      {
        hash_info->user = (user_t *) hcmalloc (sizeof (user_t));
      }

      if (hashconfig->opts_type & OPTS_TYPE_HASH_COPY)
      {
        if (user_options->benchmark == false)
        {
          hash_info->orighash = (char *) hcmalloc (256);
        }
      }

      if (hashconfig->opts_type & OPTS_TYPE_HASH_SPLIT)
      {
        hash_info->split = (split_t *) hcmalloc (sizeof (split_t));
      }
    }
  }

  if (hashconfig->is_salted == true)
  {
    salts_buf = (salt_t *) hccalloc (hashes_avail, sizeof (salt_t));

    if (hashconfig->esalt_size > 0)
    {
      esalts_buf = hccalloc (hashes_avail, hashconfig->esalt_size);
    }

    if (hashconfig->hook_salt_size > 0)
    {
      hook_salts_buf = (seven_zip_hook_salt_t *) hccalloc (hashes_avail, hashconfig->hook_salt_size);
    }
  }
  else
  {
    salts_buf = (salt_t *) hccalloc (1, sizeof (salt_t));
  }

  for (u32 hash_pos = 0; hash_pos < hashes_avail; hash_pos++)
  {
    hashes_buf[hash_pos].digest = ((char *) digests_buf) + (hash_pos * hashconfig->dgst_size);

    if (hashconfig->is_salted == true)
    {
      hashes_buf[hash_pos].salt = &salts_buf[hash_pos];

      if (hashconfig->esalt_size > 0)
      {
        hashes_buf[hash_pos].esalt = ((char *) esalts_buf) + (hash_pos * hashconfig->esalt_size);
      }

      if (hashconfig->hook_salt_size > 0)
      {
        hashes_buf[hash_pos].hook_salt = ((char *) hook_salts_buf) + (hash_pos * hashconfig->hook_salt_size);
      }
    }
    else
    {
      hashes_buf[hash_pos].salt = &salts_buf[0];
    }
  }

  hashes->hashes_buf     = hashes_buf;
  hashes->digests_buf    = digests_buf;
  hashes->salts_buf      = salts_buf;
  hashes->esalts_buf     = esalts_buf;
  hashes->hook_salts_buf = hook_salts_buf;

  /**
   * load hashes, part III: parse hashes or generate them if benchmark
   */

  u32 hashes_cnt = 0;

  if (user_options->benchmark == true)
  {
    hashconfig_benchmark_defaults (hashcat_ctx, hashes_buf[0].salt, hashes_buf[0].esalt, hashes_buf[0].hook_salt);

    hashes->hashfile = "-";

    hashes_cnt = 1;
  }
  else if (user_options->example_hashes == true)
  {
  }
  else if (user_options->keyspace == true)
  {
  }
  else if (user_options->stdout_flag == true)
  {
  }
  else if (user_options->opencl_info == true)
  {
  }
  else
  {
    if (hashes_avail == 0)
    {
      // ???
    }
    else if (hashlist_mode == HL_MODE_ARG)
    {
      char *input_buf = hash_or_file;

      u32 input_len = strlen (input_buf);

      char *hash_buf = NULL;
      int   hash_len = 0;

      hlfmt_hash (hashcat_ctx, hashlist_format, input_buf, input_len, &hash_buf, &hash_len);

      bool hash_fmt_error = 0;

      if (hash_len < 1)     hash_fmt_error = 1;
      if (hash_buf == NULL) hash_fmt_error = 1;

      if (hash_fmt_error)
      {
        event_log_warning (hashcat_ctx, "Failed to parse hashes using the '%s' format.", strhlfmt (hashlist_format));
      }
      else
      {
        if (hashconfig->opts_type & OPTS_TYPE_HASH_COPY)
        {
          hashinfo_t *hash_info_tmp = hashes_buf[hashes_cnt].hash_info;

          hash_info_tmp->orighash = hcstrdup (hash_buf);
        }

        if (hashconfig->is_salted == true)
        {
          memset (hashes_buf[0].salt, 0, sizeof (salt_t));
        }

        if (hashconfig->esalt_size > 0)
        {
          memset (hashes_buf[0].esalt, 0, hashconfig->esalt_size);
        }

        if (hashconfig->hook_salt_size > 0)
        {
          memset (hashes_buf[0].hook_salt, 0, hashconfig->hook_salt_size);
        }

        int parser_status = PARSER_OK;

        if ((hashconfig->hash_mode == 2500) || (hashconfig->hash_mode == 2501))
        {
          hashlist_mode = HL_MODE_FILE;

          hashes->hashlist_mode = hashlist_mode;

          FILE *fp = fopen (hash_buf, "rb");

          if (fp == NULL)
          {
            event_log_error (hashcat_ctx, "%s: %s", hash_buf, strerror (errno));

            return -1;
          }

          char *in = (char *) hcmalloc (sizeof (hccapx_t));

          while (!feof (fp))
          {
            const size_t nread = hc_fread (in, sizeof (hccapx_t), 1, fp);

            if (nread == 0) break;

            if (hashes_avail == hashes_cnt)
            {
              event_log_warning (hashcat_ctx, "Hashfile '%s': File changed during runtime. Skipping new data.", hash_buf);

              break;
            }

            if (hashconfig->is_salted == true)
            {
              memset (hashes_buf[hashes_cnt].salt, 0, sizeof (salt_t));
            }

            if (hashconfig->esalt_size > 0)
            {
              memset (hashes_buf[hashes_cnt].esalt, 0, hashconfig->esalt_size);

              if ((user_options->hash_mode == 2500) || (user_options->hash_mode == 2501))
              {
                wpa_t *wpa = (wpa_t *) hashes_buf[hashes_cnt].esalt;

                if (user_options->hccapx_message_pair_chgd == true)
                {
                  wpa->message_pair_chgd = (int) user_options->hccapx_message_pair_chgd;
                  wpa->message_pair      = (u8)  user_options->hccapx_message_pair;
                }

                wpa->nonce_error_corrections = user_options->nonce_error_corrections;
              }
            }

            if (hashconfig->hook_salt_size > 0)
            {
              memset (hashes_buf[hashes_cnt].hook_salt, 0, hashconfig->hook_salt_size);
            }

            parser_status = hashconfig->parse_func ((u8 *) in, sizeof (hccapx_t), &hashes_buf[hashes_cnt], hashconfig);

            if (parser_status != PARSER_OK)
            {
              event_log_warning (hashcat_ctx, "Hashfile '%s': %s", hash_buf, strparser (parser_status));

              continue;
            }

            hashes_cnt++;
          }

          hcfree (in);

          fclose (fp);
        }
        else if (hashconfig->hash_mode == 3000)
        {
          if (hash_len == 32)
          {
            parser_status = hashconfig->parse_func ((u8 *) hash_buf, 16, &hashes_buf[hashes_cnt], hashconfig);

            if (parser_status == PARSER_OK)
            {
              hashes_buf[hashes_cnt].hash_info->split->split_group  = 0;
              hashes_buf[hashes_cnt].hash_info->split->split_origin = SPLIT_ORIGIN_LEFT;

              hashes_cnt++;
            }
            else
            {
              event_log_warning (hashcat_ctx, "Hash '%s': %s", input_buf, strparser (parser_status));
            }

            parser_status = hashconfig->parse_func ((u8 *) hash_buf + 16, 16, &hashes_buf[hashes_cnt], hashconfig);

            if (parser_status == PARSER_OK)
            {
              hashes_buf[hashes_cnt].hash_info->split->split_group  = 0;
              hashes_buf[hashes_cnt].hash_info->split->split_origin = SPLIT_ORIGIN_RIGHT;

              hashes_cnt++;
            }
            else
            {
              event_log_warning (hashcat_ctx, "Hash '%s': %s", input_buf, strparser (parser_status));
            }
          }
          else
          {
            parser_status = hashconfig->parse_func ((u8 *) hash_buf, hash_len, &hashes_buf[hashes_cnt], hashconfig);

            if (parser_status == PARSER_OK)
            {
              hashes_buf[hashes_cnt].hash_info->split->split_group  = 0;
              hashes_buf[hashes_cnt].hash_info->split->split_origin = SPLIT_ORIGIN_NONE;

              hashes_cnt++;
            }
            else
            {
              event_log_warning (hashcat_ctx, "Hash '%s': %s", input_buf, strparser (parser_status));
            }
          }
        }
        else if (hashconfig->hash_mode == 14600)
        {
          hashlist_mode = HL_MODE_FILE;

          hashes->hashlist_mode = hashlist_mode;

          for (int keyslot_idx = 0; keyslot_idx < LUKS_NUMKEYS; keyslot_idx++)
          {
            parser_status = luks_parse_hash ((u8 *) hash_buf, hash_len, &hashes_buf[hashes_cnt], hashconfig, keyslot_idx);

            if (parser_status != PARSER_OK)
            {
              if (parser_status != PARSER_LUKS_KEY_DISABLED)
              {
                event_log_warning (hashcat_ctx, "Hashfile '%s': %s", hash_buf, strparser (parser_status));
              }

              continue;
            }

            hashes_cnt++;
          }
        }
        else
        {
          parser_status = hashconfig->parse_func ((u8 *) hash_buf, hash_len, &hashes_buf[hashes_cnt], hashconfig);

          if (parser_status == PARSER_OK)
          {
            hashes_cnt++;
          }
          else
          {
            event_log_warning (hashcat_ctx, "Hash '%s': %s", input_buf, strparser (parser_status));
          }
        }
      }
    }
    else if (hashlist_mode == HL_MODE_FILE)
    {
      const char *hashfile = hashes->hashfile;

      FILE *fp;

      if ((fp = fopen (hashfile, "rb")) == NULL)
      {
        event_log_error (hashcat_ctx, "%s: %s", hashfile, strerror (errno));

        return -1;
      }

      u32 line_num = 0;

      char *line_buf = (char *) hcmalloc (HCBUFSIZ_LARGE);

      hc_time_t prev = 0;
      hc_time_t now  = 0;

      while (!feof (fp))
      {
        line_num++;

        int line_len = fgetl (fp, line_buf);

        if (line_len == 0) continue;

        if (hashes_avail == hashes_cnt)
        {
          event_log_warning (hashcat_ctx, "Hashfile '%s' on line %u: File changed during runtime. Skipping new data.", hashes->hashfile, line_num);

          break;
        }

        char *hash_buf = NULL;
        int   hash_len = 0;

        hlfmt_hash (hashcat_ctx, hashlist_format, line_buf, line_len, &hash_buf, &hash_len);

        bool hash_fmt_error = 0;

        if (hash_len < 1)     hash_fmt_error = 1;
        if (hash_buf == NULL) hash_fmt_error = 1;

        if (hash_fmt_error)
        {
          event_log_warning (hashcat_ctx, "Failed to parse hashes using the '%s' format.", strhlfmt (hashlist_format));

          continue;
        }

        if (user_options->username == true)
        {
          char *user_buf = NULL;
          int   user_len = 0;

          hlfmt_user (hashcat_ctx, hashlist_format, line_buf, line_len, &user_buf, &user_len);

          // special case:
          // both hash_t need to have the username info if the pwdump format is used (i.e. we have 2 hashes for 3000, both with same user)

          u32 hashes_per_user = 1;

          if (hashconfig->hash_mode == 3000) // the following conditions should be true if (hashlist_format == HLFMT_PWDUMP)
          {
            if (hash_len == 32)
            {
              hashes_per_user = 2;
            }
          }

          for (u32 i = 0; i < hashes_per_user; i++)
          {
            user_t **user = &hashes_buf[hashes_cnt + i].hash_info->user;

            *user = (user_t *) hcmalloc (sizeof (user_t));

            user_t *user_ptr = *user;

            if (user_buf != NULL)
            {
              user_ptr->user_name = hcstrdup (user_buf);
            }
            else
            {
              user_ptr->user_name = hcstrdup ("");
            }

            user_ptr->user_len = user_len;
          }
        }

        if (hashconfig->opts_type & OPTS_TYPE_HASH_COPY)
        {
          hashinfo_t *hash_info_tmp = hashes_buf[hashes_cnt].hash_info;

          hash_info_tmp->orighash = hcstrdup (hash_buf);
        }

        if (hashconfig->is_salted == true)
        {
          memset (hashes_buf[hashes_cnt].salt, 0, sizeof (salt_t));
        }

        if (hashconfig->esalt_size > 0)
        {
          memset (hashes_buf[hashes_cnt].esalt, 0, hashconfig->esalt_size);
        }

        if (hashconfig->hook_salt_size > 0)
        {
          memset (hashes_buf[hashes_cnt].hook_salt, 0, hashconfig->hook_salt_size);
        }

        if (hashconfig->hash_mode == 3000)
        {
          if (hash_len == 32)
          {
            int parser_status = hashconfig->parse_func ((u8 *) hash_buf, 16, &hashes_buf[hashes_cnt], hashconfig);

            if (parser_status < PARSER_GLOBAL_ZERO)
            {
              event_log_warning (hashcat_ctx, "Hashfile '%s' on line %u (%s): %s", hashes->hashfile, line_num, line_buf, strparser (parser_status));

              continue;
            }

            hashes_buf[hashes_cnt].hash_info->split->split_group  = line_num;
            hashes_buf[hashes_cnt].hash_info->split->split_origin = SPLIT_ORIGIN_LEFT;

            hashes_cnt++;

            parser_status = hashconfig->parse_func ((u8 *) hash_buf + 16, 16, &hashes_buf[hashes_cnt], hashconfig);

            if (parser_status < PARSER_GLOBAL_ZERO)
            {
              event_log_warning (hashcat_ctx, "Hashfile '%s' on line %u (%s): %s", hashes->hashfile, line_num, line_buf, strparser (parser_status));

              continue;
            }

            hashes_buf[hashes_cnt].hash_info->split->split_group  = line_num;
            hashes_buf[hashes_cnt].hash_info->split->split_origin = SPLIT_ORIGIN_RIGHT;

            hashes_cnt++;
          }
          else
          {
            int parser_status = hashconfig->parse_func ((u8 *) hash_buf, hash_len, &hashes_buf[hashes_cnt], hashconfig);

            if (parser_status < PARSER_GLOBAL_ZERO)
            {
              event_log_warning (hashcat_ctx, "Hashfile '%s' on line %u (%s): %s", hashes->hashfile, line_num, line_buf, strparser (parser_status));

              continue;
            }

            hashes_buf[hashes_cnt].hash_info->split->split_group  = line_num;
            hashes_buf[hashes_cnt].hash_info->split->split_origin = SPLIT_ORIGIN_NONE;

            hashes_cnt++;
          }
        }
        else
        {
          int parser_status = hashconfig->parse_func ((u8 *) hash_buf, hash_len, &hashes_buf[hashes_cnt], hashconfig);

          if (parser_status < PARSER_GLOBAL_ZERO)
          {
            event_log_warning (hashcat_ctx, "Hashfile '%s' on line %u (%s): %s", hashes->hashfile, line_num, line_buf, strparser (parser_status));

            continue;
          }

          hashes_cnt++;
        }

        hc_time (&now);

        if ((now - prev) == 0) continue;

        hc_time (&prev);

        hashlist_parse_t hashlist_parse;

        hashlist_parse.hashes_cnt   = hashes_cnt;
        hashlist_parse.hashes_avail = hashes_avail;

        EVENT_DATA (EVENT_HASHLIST_PARSE_HASH, &hashlist_parse, sizeof (hashlist_parse_t));
      }

      hashlist_parse_t hashlist_parse;

      hashlist_parse.hashes_cnt   = hashes_cnt;
      hashlist_parse.hashes_avail = hashes_avail;

      EVENT_DATA (EVENT_HASHLIST_PARSE_HASH, &hashlist_parse, sizeof (hashlist_parse_t));

      hcfree (line_buf);

      fclose (fp);
    }
  }

  hashes->hashes_cnt = hashes_cnt;

  if (hashes_cnt)
  {
    EVENT (EVENT_HASHLIST_SORT_HASH_PRE);

    if (hashconfig->is_salted == true)
    {
      hc_qsort_r (hashes_buf, hashes_cnt, sizeof (hash_t), sort_by_hash, (void *) hashconfig);
    }
    else
    {
      hc_qsort_r (hashes_buf, hashes_cnt, sizeof (hash_t), sort_by_hash_no_salt, (void *) hashconfig);
    }

    EVENT (EVENT_HASHLIST_SORT_HASH_POST);
  }

  if (hashconfig->hash_mode == 3000)
  {
    // update split split_neighbor after sorting
    // see https://github.com/hashcat/hashcat/issues/1034 for good examples for testing

    for (u32 i = 0; i < hashes_cnt; i++)
    {
      split_t *split1 = hashes_buf[i].hash_info->split;

      if (split1->split_origin != SPLIT_ORIGIN_LEFT) continue;

      for (u32 j = 0; j < hashes_cnt; j++)
      {
        split_t *split2 = hashes_buf[j].hash_info->split;

        if (split2->split_origin != SPLIT_ORIGIN_RIGHT) continue;

        if (split1->split_group != split2->split_group) continue;

        split1->split_neighbor = j;
        split2->split_neighbor = i;

        break;
      }
    }
  }

  return 0;
}

int hashes_init_stage2 (hashcat_ctx_t *hashcat_ctx)
{
  hashconfig_t   *hashconfig   = hashcat_ctx->hashconfig;
  hashes_t       *hashes       = hashcat_ctx->hashes;
  user_options_t *user_options = hashcat_ctx->user_options;
  potfile_ctx_t  *potfile_ctx  = hashcat_ctx->potfile_ctx;

  hash_t *hashes_buf = hashes->hashes_buf;
  u32     hashes_cnt = hashes->hashes_cnt;

  /**
   * Remove duplicates
   */

  EVENT (EVENT_HASHLIST_UNIQUE_HASH_PRE);

  u32 hashes_cnt_new = 1;

  for (u32 hashes_pos = 1; hashes_pos < hashes_cnt; hashes_pos++)
  {
    if (potfile_ctx->keep_all_hashes == true)
    {
      // do not sort, because we need to keep all hashes in this particular case
    }
    else if (hashconfig->is_salted == true)
    {
      if (sort_by_salt (hashes_buf[hashes_pos].salt, hashes_buf[hashes_pos - 1].salt) == 0)
      {
        if (sort_by_digest_p0p1 (hashes_buf[hashes_pos].digest, hashes_buf[hashes_pos - 1].digest, (void *) hashconfig) == 0) continue;
      }
    }
    else
    {
      if (sort_by_digest_p0p1 (hashes_buf[hashes_pos].digest, hashes_buf[hashes_pos - 1].digest, (void *) hashconfig) == 0) continue;
    }

    hash_t tmp;

    memcpy (&tmp, &hashes_buf[hashes_pos], sizeof (hash_t));

    memcpy (&hashes_buf[hashes_cnt_new], &tmp, sizeof (hash_t));

    hashes_cnt_new++;
  }

  for (u32 i = hashes_cnt_new; i < hashes->hashes_cnt; i++)
  {
    memset (&hashes_buf[i], 0, sizeof (hash_t));
  }

  hashes_cnt = hashes_cnt_new;

  hashes->hashes_cnt = hashes_cnt;

  EVENT (EVENT_HASHLIST_UNIQUE_HASH_POST);

  /**
   * Now generate all the buffers required for later
   */

  void   *digests_buf_new    = hccalloc (hashes_cnt, hashconfig->dgst_size);
  salt_t *salts_buf_new      = NULL;
  void   *esalts_buf_new     = NULL;
  void   *hook_salts_buf_new = NULL;

  if (hashconfig->is_salted == true)
  {
    salts_buf_new = (salt_t *) hccalloc (hashes_cnt, sizeof (salt_t));
  }
  else
  {
    salts_buf_new = (salt_t *) hccalloc (1, sizeof (salt_t));
  }

  if (hashconfig->esalt_size > 0)
  {
    esalts_buf_new = hccalloc (hashes_cnt, hashconfig->esalt_size);
  }

  if (hashconfig->hook_salt_size > 0)
  {
    hook_salts_buf_new = hccalloc (hashes_cnt, hashconfig->hook_salt_size);
  }

  EVENT (EVENT_HASHLIST_SORT_SALT_PRE);

  u32 digests_cnt  = hashes_cnt;
  u32 digests_done = 0;

  u32 *digests_shown     = (u32 *) hccalloc (digests_cnt, sizeof (u32));
  u32 *digests_shown_tmp = (u32 *) hccalloc (digests_cnt, sizeof (u32));

  u32 salts_cnt   = 0;
  u32 salts_done  = 0;

  hashinfo_t **hash_info = NULL;

  if ((user_options->username == true) || (hashconfig->opts_type & OPTS_TYPE_HASH_COPY) || (hashconfig->opts_type & OPTS_TYPE_HASH_SPLIT))
  {
    hash_info = (hashinfo_t **) hccalloc (hashes_cnt, sizeof (hashinfo_t *));
  }

  u32 *salts_shown = (u32 *) hccalloc (digests_cnt, sizeof (u32));

  salt_t *salt_buf;

  {
    // copied from inner loop

    salt_buf = &salts_buf_new[salts_cnt];

    memcpy (salt_buf, hashes_buf[0].salt, sizeof (salt_t));

    hashes_buf[0].salt = salt_buf;

    if (hashconfig->hook_salt_size > 0)
    {
      char *hook_salts_buf_new_ptr = ((char *) hook_salts_buf_new) + (salts_cnt * hashconfig->hook_salt_size);

      memcpy (hook_salts_buf_new_ptr, hashes_buf[0].hook_salt, hashconfig->hook_salt_size);

      hashes_buf[0].hook_salt = hook_salts_buf_new_ptr;
    }

    salt_buf->digests_cnt    = 0;
    salt_buf->digests_done   = 0;
    salt_buf->digests_offset = 0;

    salts_cnt++;
  }

  salt_buf->digests_cnt++;

  char *digests_buf_new_ptr = ((char *) digests_buf_new) + (0 * hashconfig->dgst_size);

  memcpy (digests_buf_new_ptr, hashes_buf[0].digest, hashconfig->dgst_size);

  hashes_buf[0].digest = digests_buf_new_ptr;

  if (hashconfig->esalt_size > 0)
  {
    char *esalts_buf_new_ptr = ((char *) esalts_buf_new) + (0 * hashconfig->esalt_size);

    memcpy (esalts_buf_new_ptr, hashes_buf[0].esalt, hashconfig->esalt_size);

    hashes_buf[0].esalt = esalts_buf_new_ptr;
  }

  if ((user_options->username == true) || (hashconfig->opts_type & OPTS_TYPE_HASH_COPY) || (hashconfig->opts_type & OPTS_TYPE_HASH_SPLIT))
  {
    hash_info[0] = hashes_buf[0].hash_info;
  }

  // copy from inner loop

  for (u32 hashes_pos = 1; hashes_pos < hashes_cnt; hashes_pos++)
  {
    if (hashconfig->is_salted == true)
    {
      if (sort_by_salt (hashes_buf[hashes_pos].salt, hashes_buf[hashes_pos - 1].salt) != 0)
      {
        salt_buf = &salts_buf_new[salts_cnt];

        memcpy (salt_buf, hashes_buf[hashes_pos].salt, sizeof (salt_t));

        hashes_buf[hashes_pos].salt = salt_buf;

        if (hashconfig->hook_salt_size > 0)
        {
          char *hook_salts_buf_new_ptr = ((char *) hook_salts_buf_new) + (salts_cnt * hashconfig->hook_salt_size);

          memcpy (hook_salts_buf_new_ptr, hashes_buf[hashes_pos].hook_salt, hashconfig->hook_salt_size);

          hashes_buf[hashes_pos].hook_salt = hook_salts_buf_new_ptr;
        }

        salt_buf->digests_cnt    = 0;
        salt_buf->digests_done   = 0;
        salt_buf->digests_offset = hashes_pos;

        salts_cnt++;
      }

      hashes_buf[hashes_pos].salt = salt_buf;

      if (hashconfig->hook_salt_size > 0)
      {
        char *hook_salts_buf_new_ptr = ((char *) hook_salts_buf_new) + (salts_cnt * hashconfig->hook_salt_size);

        hashes_buf[hashes_pos].hook_salt = hook_salts_buf_new_ptr;
      }
    }

    salt_buf->digests_cnt++;

    digests_buf_new_ptr = ((char *) digests_buf_new) + (hashes_pos * hashconfig->dgst_size);

    memcpy (digests_buf_new_ptr, hashes_buf[hashes_pos].digest, hashconfig->dgst_size);

    hashes_buf[hashes_pos].digest = digests_buf_new_ptr;

    if (hashconfig->esalt_size > 0)
    {
      char *esalts_buf_new_ptr = ((char *) esalts_buf_new) + (hashes_pos * hashconfig->esalt_size);

      memcpy (esalts_buf_new_ptr, hashes_buf[hashes_pos].esalt, hashconfig->esalt_size);

      hashes_buf[hashes_pos].esalt = esalts_buf_new_ptr;
    }

    if ((user_options->username == true) || (hashconfig->opts_type & OPTS_TYPE_HASH_COPY) || (hashconfig->opts_type & OPTS_TYPE_HASH_SPLIT))
    {
      hash_info[hashes_pos] = hashes_buf[hashes_pos].hash_info;
    }
  }

  EVENT (EVENT_HASHLIST_SORT_SALT_POST);

  hcfree (hashes->digests_buf);
  hcfree (hashes->salts_buf);
  hcfree (hashes->esalts_buf);
  hcfree (hashes->hook_salts_buf);

  hashes->digests_cnt       = digests_cnt;
  hashes->digests_done      = digests_done;
  hashes->digests_buf       = digests_buf_new;
  hashes->digests_shown     = digests_shown;
  hashes->digests_shown_tmp = digests_shown_tmp;

  hashes->salts_cnt         = salts_cnt;
  hashes->salts_done        = salts_done;
  hashes->salts_buf         = salts_buf_new;
  hashes->salts_shown       = salts_shown;

  hashes->esalts_buf        = esalts_buf_new;
  hashes->hook_salts_buf    = hook_salts_buf_new;

  hashes->hash_info         = hash_info;

  return 0;
}

int hashes_init_stage3 (hashcat_ctx_t *hashcat_ctx)
{
  hashes_t *hashes = hashcat_ctx->hashes;

  u32  digests_done  = hashes->digests_done;
  u32 *digests_shown = hashes->digests_shown;

  u32  salts_cnt     = hashes->salts_cnt;
  u32  salts_done    = hashes->salts_done;
  u32 *salts_shown   = hashes->salts_shown;

  hash_t *hashes_buf = hashes->hashes_buf;

  salt_t *salts_buf  = hashes->salts_buf;

  for (u32 salt_idx = 0; salt_idx < salts_cnt; salt_idx++)
  {
    salt_t *salt_buf = salts_buf + salt_idx;

    u32 digests_cnt = salt_buf->digests_cnt;

    for (u32 digest_idx = 0; digest_idx < digests_cnt; digest_idx++)
    {
      const u32 hashes_idx = salt_buf->digests_offset + digest_idx;

      if (hashes_buf[hashes_idx].cracked == 1)
      {
        digests_shown[hashes_idx] = 1;

        digests_done++;

        salt_buf->digests_done++;
      }
    }

    if (salt_buf->digests_done == salt_buf->digests_cnt)
    {
      salts_shown[salt_idx] = 1;

      salts_done++;
    }

    if (salts_done == salts_cnt) mycracked (hashcat_ctx);
  }

  hashes->digests_done = digests_done;

  hashes->salts_cnt   = salts_cnt;
  hashes->salts_done  = salts_done;

  return 0;
}

int hashes_init_stage4 (hashcat_ctx_t *hashcat_ctx)
{
  hashconfig_t   *hashconfig   = hashcat_ctx->hashconfig;
  hashes_t       *hashes       = hashcat_ctx->hashes;
  user_options_t *user_options = hashcat_ctx->user_options;

  const int rc_defaults = hashconfig_general_defaults (hashcat_ctx);

  if (rc_defaults == -1) return -1;

  if (hashes->salts_cnt == 1)
    hashconfig->opti_type |= OPTI_TYPE_SINGLE_SALT;

  if (hashes->digests_cnt == 1)
    hashconfig->opti_type |= OPTI_TYPE_SINGLE_HASH;

  if (hashconfig->attack_exec == ATTACK_EXEC_INSIDE_KERNEL)
    hashconfig->opti_type |= OPTI_TYPE_NOT_ITERATED;

  if (user_options->attack_mode == ATTACK_MODE_BF)
    hashconfig->opti_type |= OPTI_TYPE_BRUTE_FORCE;

  if (hashconfig->opti_type & OPTI_TYPE_BRUTE_FORCE)
  {
    if (hashconfig->opti_type & OPTI_TYPE_SINGLE_HASH)
    {
      if (hashconfig->opti_type & OPTI_TYPE_APPENDED_SALT)
      {
        if (hashconfig->opts_type & OPTS_TYPE_ST_ADD80)
        {
          hashconfig->opts_type &= ~OPTS_TYPE_ST_ADD80;
          hashconfig->opts_type |=  OPTS_TYPE_PT_ADD80;
        }

        if (hashconfig->opts_type & OPTS_TYPE_ST_ADDBITS14)
        {
          hashconfig->opts_type &= ~OPTS_TYPE_ST_ADDBITS14;
          hashconfig->opts_type |=  OPTS_TYPE_PT_ADDBITS14;
        }

        if (hashconfig->opts_type & OPTS_TYPE_ST_ADDBITS15)
        {
          hashconfig->opts_type &= ~OPTS_TYPE_ST_ADDBITS15;
          hashconfig->opts_type |=  OPTS_TYPE_PT_ADDBITS15;
        }
      }
    }
  }

  // at this point we no longer need hash_t* structure

  hash_t *hashes_buf = hashes->hashes_buf;

  hcfree (hashes_buf);

  hashes->hashes_cnt = 0;
  hashes->hashes_buf = NULL;

  // starting from here, we should allocate some scratch buffer for later use

  u8 *out_buf = (u8 *) hcmalloc (HCBUFSIZ_LARGE);

  hashes->out_buf = out_buf;

  // we need two buffers in parallel

  u8 *tmp_buf = (u8 *) hcmalloc (HCBUFSIZ_LARGE);

  hashes->tmp_buf = tmp_buf;

  return 0;
}

int hashes_init_selftest (hashcat_ctx_t *hashcat_ctx)
{
  folder_config_t *folder_config = hashcat_ctx->folder_config;
  hashconfig_t    *hashconfig    = hashcat_ctx->hashconfig;
  hashes_t        *hashes        = hashcat_ctx->hashes;
  user_options_t  *user_options  = hashcat_ctx->user_options;

  if (hashconfig->st_hash == NULL) return 0;

  void   *st_digests_buf    = NULL;
  salt_t *st_salts_buf      = NULL;
  void   *st_esalts_buf     = NULL;
  void   *st_hook_salts_buf = NULL;

  st_digests_buf =          hccalloc (1, hashconfig->dgst_size);

  st_salts_buf = (salt_t *) hccalloc (1, sizeof (salt_t));

  if (hashconfig->esalt_size > 0)
  {
    st_esalts_buf = hccalloc (1, hashconfig->esalt_size);
  }

  if (hashconfig->hook_salt_size > 0)
  {
    st_hook_salts_buf = hccalloc (1, hashconfig->hook_salt_size);
  }

  hash_t hash;

  hash.digest    = st_digests_buf;
  hash.salt      = st_salts_buf;
  hash.esalt     = st_esalts_buf;
  hash.hook_salt = st_hook_salts_buf;
  hash.cracked   = 0;
  hash.hash_info = NULL;
  hash.pw_buf    = NULL;
  hash.pw_len    = 0;

  int parser_status;

  if ((hashconfig->hash_mode == 2500) || (hashconfig->hash_mode == 2501))
  {
    char *tmpdata = (char *) hcmalloc (sizeof (hccapx_t));

    const int st_hash_len = strlen (hashconfig->st_hash);

    for (int i = 0, j = 0; j < st_hash_len; i += 1, j += 2)
    {
      const u8 c = hex_to_u8 ((u8 *) hashconfig->st_hash + j);

      tmpdata[i] = c;
    }

    parser_status = hashconfig->parse_func ((u8 *) tmpdata, sizeof (hccapx_t), &hash, hashconfig);

    hcfree (tmpdata);

    wpa_t *wpa = (wpa_t *) st_esalts_buf;

    wpa->nonce_error_corrections = 3;
  }
  else if (hashconfig->opts_type & OPTS_TYPE_BINARY_HASHFILE)
  {
    char *tmpfile_bin;

    hc_asprintf (&tmpfile_bin, "%s/selftest.hash", folder_config->session_dir);

    FILE *fp = fopen (tmpfile_bin, "wb");

    const int st_hash_len = strlen (hashconfig->st_hash);

    for (int i = 0; i < st_hash_len; i += 2)
    {
      const u8 c = hex_to_u8 ((u8 *) hashconfig->st_hash + i);

      fputc (c, fp);
    }

    fclose (fp);

    parser_status = hashconfig->parse_func ((u8 *) tmpfile_bin, strlen (tmpfile_bin), &hash, hashconfig);

    unlink (tmpfile_bin);

    hcfree (tmpfile_bin);
  }
  else
  {
    hashconfig_t *hashconfig_st = (hashconfig_t *) hcmalloc (sizeof (hashconfig_t));

    memcpy (hashconfig_st, hashconfig, sizeof (hashconfig_t));

    hashconfig_st->separator = SEPARATOR;

    if (user_options->hex_salt)
    {
      if (hashconfig->salt_type == SALT_TYPE_GENERIC)
      {
        // this is save as there's no hash mode that has both SALT_TYPE_GENERIC and OPTS_TYPE_ST_HEX by default

        hashconfig_st->opts_type &= ~OPTS_TYPE_ST_HEX;
      }
    }

    parser_status = hashconfig->parse_func ((u8 *) hashconfig->st_hash, strlen (hashconfig->st_hash), &hash, hashconfig_st);

    hcfree (hashconfig_st);
  }

  if (parser_status == PARSER_OK)
  {
    // nothing to do
  }
  else
  {
    event_log_error (hashcat_ctx, "Self-test hash parsing error: %s", strparser (parser_status));

    return -1;
  }

  hashes->st_digests_buf    = st_digests_buf;
  hashes->st_salts_buf      = st_salts_buf;
  hashes->st_esalts_buf     = st_esalts_buf;
  hashes->st_hook_salts_buf = st_hook_salts_buf;

  return 0;
}

void hashes_destroy (hashcat_ctx_t *hashcat_ctx)
{
  hashconfig_t   *hashconfig   = hashcat_ctx->hashconfig;
  hashes_t       *hashes       = hashcat_ctx->hashes;
  user_options_t *user_options = hashcat_ctx->user_options;

  hcfree (hashes->digests_buf);
  hcfree (hashes->digests_shown);
  hcfree (hashes->digests_shown_tmp);

  hcfree (hashes->salts_buf);
  hcfree (hashes->salts_shown);

  if ((user_options->username == true) || (hashconfig->opts_type & OPTS_TYPE_HASH_COPY))
  {
    for (u32 hash_pos = 0; hash_pos < hashes->hashes_cnt; hash_pos++)
    {
      if (user_options->username == true)
      {
        hcfree (hashes->hash_info[hash_pos]->user);
      }

      if (hashconfig->opts_type & OPTS_TYPE_HASH_COPY)
      {
        hcfree (hashes->hash_info[hash_pos]->orighash);
      }

      if (hashconfig->opts_type & OPTS_TYPE_HASH_SPLIT)
      {
        hcfree (hashes->hash_info[hash_pos]->split);
      }
    }
  }

  hcfree (hashes->hash_info);

  hcfree (hashes->esalts_buf);
  hcfree (hashes->hook_salts_buf);

  hcfree (hashes->out_buf);
  hcfree (hashes->tmp_buf);

  hcfree (hashes->st_digests_buf);
  hcfree (hashes->st_salts_buf);
  hcfree (hashes->st_esalts_buf);
  hcfree (hashes->st_hook_salts_buf);

  memset (hashes, 0, sizeof (hashes_t));
}

void hashes_logger (hashcat_ctx_t *hashcat_ctx)
{
  hashes_t      *hashes      = hashcat_ctx->hashes;
  logfile_ctx_t *logfile_ctx = hashcat_ctx->logfile_ctx;

  logfile_top_string (hashes->hashfile);
  logfile_top_uint   (hashes->hashlist_mode);
  logfile_top_uint   (hashes->hashlist_format);
  logfile_top_uint   (hashes->hashes_cnt);
  logfile_top_uint   (hashes->digests_cnt);
  logfile_top_uint   (hashes->digests_done);
  logfile_top_uint   (hashes->salts_cnt);
  logfile_top_uint   (hashes->salts_done);
}
