/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "convert.h"
#include "event.h"
#include "memory.h"
#include "opencl.h"
#include "wordlist.h"
#include "shared.h"
#include "thread.h"
#include "filehandling.h"
#include "rp.h"
#include "rp_cpu.h"
#include "dispatch.h"

static u64 get_lowest_words_done (const hashcat_ctx_t *hashcat_ctx)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  u64 words_cur = 0xffffffffffffffff;

  for (u32 device_id = 0; device_id < opencl_ctx->devices_cnt; device_id++)
  {
    hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

    if (device_param->skipped == true) continue;

    const u64 words_done = device_param->words_done;

    if (words_done < words_cur) words_cur = words_done;
  }

  // It's possible that a device's workload isn't finished right after a restore-case.
  // In that case, this function would return 0 and overwrite the real restore point

  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  if (words_cur < status_ctx->words_cur) words_cur = status_ctx->words_cur;

  return words_cur;
}

static int set_kernel_power_final (hashcat_ctx_t *hashcat_ctx, const u32 kernel_power_final)
{
  EVENT (EVENT_SET_KERNEL_POWER_FINAL);

  opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  opencl_ctx->kernel_power_final = kernel_power_final;

  return 0;
}

static u32 get_power (opencl_ctx_t *opencl_ctx, hc_device_param_t *device_param)
{
  const u64 kernel_power_final = opencl_ctx->kernel_power_final;

  if (kernel_power_final)
  {
    const double device_factor = (double) device_param->hardware_power / opencl_ctx->hardware_power_all;

    const u64 words_left_device = (u64) CEIL (kernel_power_final * device_factor);

    // work should be at least the hardware power available without any accelerator

    const u64 work = MAX (words_left_device, device_param->hardware_power);

    // we need to make sure the value is not larger than the regular kernel_power

    const u64 work_final = MIN (work, device_param->kernel_power);

    return work_final;
  }

  return device_param->kernel_power;
}

static u32 get_work (hashcat_ctx_t *hashcat_ctx, hc_device_param_t *device_param, const u32 max)
{
  opencl_ctx_t   *opencl_ctx   = hashcat_ctx->opencl_ctx;
  status_ctx_t   *status_ctx   = hashcat_ctx->status_ctx;
  user_options_t *user_options = hashcat_ctx->user_options;

  hc_thread_mutex_lock (status_ctx->mux_dispatcher);

  const u64 words_off  = status_ctx->words_off;
  const u64 words_base = (user_options->limit == 0) ? status_ctx->words_base : MIN (user_options->limit, status_ctx->words_base);

  device_param->words_off = words_off;

  const u64 kernel_power_all = opencl_ctx->kernel_power_all;

  const u64 words_left = words_base - words_off;

  if (words_left < kernel_power_all)
  {
    if (opencl_ctx->kernel_power_final == 0)
    {
      set_kernel_power_final (hashcat_ctx, words_left);
    }
  }

  const u32 kernel_power = get_power (opencl_ctx, device_param);

  u32 work = MIN (words_left, kernel_power);

  work = MIN (work, max);

  status_ctx->words_off += work;

  hc_thread_mutex_unlock (status_ctx->mux_dispatcher);

  return work;
}

static int calc_stdin (hashcat_ctx_t *hashcat_ctx, hc_device_param_t *device_param)
{
  user_options_t       *user_options       = hashcat_ctx->user_options;
  user_options_extra_t *user_options_extra = hashcat_ctx->user_options_extra;
  hashconfig_t         *hashconfig         = hashcat_ctx->hashconfig;
  hashes_t             *hashes             = hashcat_ctx->hashes;
  straight_ctx_t       *straight_ctx       = hashcat_ctx->straight_ctx;
  status_ctx_t         *status_ctx         = hashcat_ctx->status_ctx;

  char *buf = (char *) hcmalloc (HCBUFSIZ_LARGE);

  bool iconv_enabled = false;

  iconv_t iconv_ctx = NULL;

  char *iconv_tmp = NULL;

  if (strcmp (user_options->encoding_from, user_options->encoding_to) != 0)
  {
    iconv_enabled = true;

    iconv_ctx = iconv_open (user_options->encoding_to, user_options->encoding_from);

    if (iconv_ctx == (iconv_t) -1) return -1;

    iconv_tmp = (char *) hcmalloc (HCBUFSIZ_TINY);
  }

  while (status_ctx->run_thread_level1 == true)
  {
    hc_thread_mutex_lock (status_ctx->mux_dispatcher);

    if (feof (stdin) != 0)
    {
      hc_thread_mutex_unlock (status_ctx->mux_dispatcher);

      break;
    }

    u32 words_extra_total = 0;

    memset (device_param->pws_buf, 0, device_param->size_pws);

    while (device_param->pws_cnt < device_param->kernel_power)
    {
      char *line_buf = fgets (buf, HCBUFSIZ_LARGE - 1, stdin);

      if (line_buf == NULL) break;

      size_t line_len = in_superchop (line_buf);

      line_len = convert_from_hex (hashcat_ctx, line_buf, line_len);

      // do the on-the-fly encoding

      if (iconv_enabled == true)
      {
        char  *iconv_ptr = iconv_tmp;
        size_t iconv_sz  = HCBUFSIZ_TINY;

        const size_t iconv_rc = iconv (iconv_ctx, &line_buf, &line_len, &iconv_ptr, &iconv_sz);

        if (iconv_rc == (size_t) -1) continue;

        line_buf = iconv_tmp;
        line_len = HCBUFSIZ_TINY - iconv_sz;
      }

      // post-process rule engine

      char rule_buf_out[RP_PASSWORD_SIZE];

      if (run_rule_engine ((int) user_options_extra->rule_len_l, user_options->rule_buf_l))
      {
        if (line_len >= RP_PASSWORD_SIZE) continue;

        memset (rule_buf_out, 0, sizeof (rule_buf_out));

        const int rule_len_out = _old_apply_rule (user_options->rule_buf_l, (int) user_options_extra->rule_len_l, line_buf, (int) line_len, rule_buf_out);

        if (rule_len_out < 0) continue;

        line_buf = rule_buf_out;
        line_len = (size_t) rule_len_out;
      }

      if (line_len >= PW_MAX) continue;

      // hmm that's always the case, or?

      const u32 attack_kern = user_options_extra->attack_kern;

      if (attack_kern == ATTACK_KERN_STRAIGHT)
      {
        if ((line_len < hashconfig->pw_min) || (line_len > hashconfig->pw_max))
        {
          words_extra_total++;

          continue;
        }
      }

      pw_add (device_param, (u8 *) line_buf, (int) line_len);

      while (status_ctx->run_thread_level1 == false) break;
    }

    hc_thread_mutex_unlock (status_ctx->mux_dispatcher);

    if (words_extra_total > 0)
    {
      hc_thread_mutex_lock (status_ctx->mux_counter);

      for (u32 salt_pos = 0; salt_pos < hashes->salts_cnt; salt_pos++)
      {
        status_ctx->words_progress_rejected[salt_pos] += words_extra_total * straight_ctx->kernel_rules_cnt;
      }

      hc_thread_mutex_unlock (status_ctx->mux_counter);
    }

    if (status_ctx->run_thread_level1 == false) break;

    if (device_param->pws_cnt == 0) break;

    // flush

    int CL_rc;

    CL_rc = run_copy (hashcat_ctx, device_param, device_param->pws_cnt);

    if (CL_rc == -1)
    {
      hcfree (buf);

      return -1;
    }

    CL_rc = run_cracker (hashcat_ctx, device_param, device_param->pws_cnt);

    if (CL_rc == -1)
    {
      hcfree (buf);

      return -1;
    }

    device_param->pws_cnt = 0;

    if (status_ctx->run_thread_level1 == false) break;

    if (user_options->speed_only == true) break;
  }

  device_param->kernel_accel = 0;
  device_param->kernel_loops = 0;

  if (iconv_enabled == true)
  {
    iconv_close (iconv_ctx);

    hcfree (iconv_tmp);
  }

  hcfree (buf);

  return 0;
}

void *thread_calc_stdin (void *p)
{
  thread_param_t *thread_param = (thread_param_t *) p;

  hashcat_ctx_t *hashcat_ctx = thread_param->hashcat_ctx;

  opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  if (opencl_ctx->enabled == false) return NULL;

  hc_device_param_t *device_param = opencl_ctx->devices_param + thread_param->tid;

  if (device_param->skipped) return NULL;

  calc_stdin (hashcat_ctx, device_param); // we should check the RC here

  return NULL;
}

static int calc (hashcat_ctx_t *hashcat_ctx, hc_device_param_t *device_param)
{
  user_options_t       *user_options       = hashcat_ctx->user_options;
  user_options_extra_t *user_options_extra = hashcat_ctx->user_options_extra;
  hashconfig_t         *hashconfig         = hashcat_ctx->hashconfig;
  hashes_t             *hashes             = hashcat_ctx->hashes;
  straight_ctx_t       *straight_ctx       = hashcat_ctx->straight_ctx;
  combinator_ctx_t     *combinator_ctx     = hashcat_ctx->combinator_ctx;
  status_ctx_t         *status_ctx         = hashcat_ctx->status_ctx;

  const u32 attack_mode = user_options->attack_mode;
  const u32 attack_kern = user_options_extra->attack_kern;

  if ((attack_mode == ATTACK_MODE_BF) || (((hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL) == 0) && (attack_mode == ATTACK_MODE_HYBRID2)))
  {
    if (((hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL) == 0) && (attack_mode == ATTACK_MODE_HYBRID2))
    {
      char *dictfile = straight_ctx->dict;

      FILE *combs_fp = fopen (dictfile, "rb");

      if (combs_fp == NULL)
      {
        event_log_error (hashcat_ctx, "%s: %s", dictfile, strerror (errno));

        return -1;
      }

      device_param->combs_fp = combs_fp;
    }

    while (status_ctx->run_thread_level1 == true)
    {
      const u32 work = get_work (hashcat_ctx, device_param, -1u);

      if (work == 0) break;

      const u64 words_off = device_param->words_off;
      const u64 words_fin = words_off + work;

      device_param->pws_cnt = work;

      int CL_rc;

      CL_rc = run_copy (hashcat_ctx, device_param, device_param->pws_cnt);

      if (CL_rc == -1) return -1;

      CL_rc = run_cracker (hashcat_ctx, device_param, device_param->pws_cnt);

      if (CL_rc == -1) return -1;

      device_param->pws_cnt = 0;

      if (user_options->speed_only == true) break;

      if (status_ctx->run_thread_level2 == true)
      {
        device_param->words_done = words_fin;

        status_ctx->words_cur = get_lowest_words_done (hashcat_ctx);
      }

      if (status_ctx->run_thread_level1 == false) break;
    }
  }
  else
  {
    char *dictfile = straight_ctx->dict;

    if (attack_mode == ATTACK_MODE_COMBI)
    {
      if (combinator_ctx->combs_mode == COMBINATOR_MODE_BASE_LEFT)
      {
        dictfile = combinator_ctx->dict1;
      }
      else
      {
        dictfile = combinator_ctx->dict2;
      }

      const u32 combs_mode = combinator_ctx->combs_mode;

      if (combs_mode == COMBINATOR_MODE_BASE_LEFT)
      {
        const char *dictfilec = combinator_ctx->dict2;

        FILE *combs_fp = fopen (dictfilec, "rb");

        if (combs_fp == NULL)
        {
          event_log_error (hashcat_ctx, "%s: %s", combinator_ctx->dict2, strerror (errno));

          return -1;
        }

        device_param->combs_fp = combs_fp;
      }
      else if (combs_mode == COMBINATOR_MODE_BASE_RIGHT)
      {
        const char *dictfilec = combinator_ctx->dict1;

        FILE *combs_fp = fopen (dictfilec, "rb");

        if (combs_fp == NULL)
        {
          event_log_error (hashcat_ctx, "%s: %s", dictfilec, strerror (errno));

          return -1;
        }

        device_param->combs_fp = combs_fp;
      }
    }

    FILE *fd = fopen (dictfile, "rb");

    if (fd == NULL)
    {
      event_log_error (hashcat_ctx, "%s: %s", dictfile, strerror (errno));

      return -1;
    }

    hashcat_ctx_t *hashcat_ctx_tmp = (hashcat_ctx_t *) hcmalloc (sizeof (hashcat_ctx_t));

    /*
    hashcat_ctx_tmp->bitmap_ctx         = hashcat_ctx->bitmap_ctx;
    hashcat_ctx_tmp->combinator_ctx     = hashcat_ctx->combinator_ctx;
    hashcat_ctx_tmp->cpt_ctx            = hashcat_ctx->cpt_ctx;
    hashcat_ctx_tmp->debugfile_ctx      = hashcat_ctx->debugfile_ctx;
    hashcat_ctx_tmp->dictstat_ctx       = hashcat_ctx->dictstat_ctx;
    hashcat_ctx_tmp->folder_config      = hashcat_ctx->folder_config;
    hashcat_ctx_tmp->hashconfig         = hashcat_ctx->hashconfig;
    hashcat_ctx_tmp->hashes             = hashcat_ctx->hashes;
    hashcat_ctx_tmp->hwmon_ctx          = hashcat_ctx->hwmon_ctx;
    hashcat_ctx_tmp->induct_ctx         = hashcat_ctx->induct_ctx;
    hashcat_ctx_tmp->logfile_ctx        = hashcat_ctx->logfile_ctx;
    hashcat_ctx_tmp->loopback_ctx       = hashcat_ctx->loopback_ctx;
    hashcat_ctx_tmp->mask_ctx           = hashcat_ctx->mask_ctx;
    hashcat_ctx_tmp->opencl_ctx         = hashcat_ctx->opencl_ctx;
    hashcat_ctx_tmp->outcheck_ctx       = hashcat_ctx->outcheck_ctx;
    hashcat_ctx_tmp->outfile_ctx        = hashcat_ctx->outfile_ctx;
    hashcat_ctx_tmp->potfile_ctx        = hashcat_ctx->potfile_ctx;
    hashcat_ctx_tmp->restore_ctx        = hashcat_ctx->restore_ctx;
    hashcat_ctx_tmp->status_ctx         = hashcat_ctx->status_ctx;
    hashcat_ctx_tmp->straight_ctx       = hashcat_ctx->straight_ctx;
    hashcat_ctx_tmp->tuning_db          = hashcat_ctx->tuning_db;
    hashcat_ctx_tmp->user_options_extra = hashcat_ctx->user_options_extra;
    hashcat_ctx_tmp->user_options       = hashcat_ctx->user_options;
    */

    memcpy (hashcat_ctx_tmp, hashcat_ctx, sizeof (hashcat_ctx_t)); // yes we actually want to copy these pointers

    hashcat_ctx_tmp->wl_data = (wl_data_t *) hcmalloc (sizeof (wl_data_t));

    const int rc_wl_data_init = wl_data_init (hashcat_ctx_tmp);

    if (rc_wl_data_init == -1)
    {
      if (attack_mode == ATTACK_MODE_COMBI) fclose (device_param->combs_fp);

      fclose (fd);

      hcfree (hashcat_ctx_tmp->wl_data);

      hcfree (hashcat_ctx_tmp);

      return -1;
    }

    u64 words_cur = 0;

    while (status_ctx->run_thread_level1 == true)
    {
      u64 words_off = 0;
      u64 words_fin = 0;

      u32 words_extra = -1u;

      u32 words_extra_total = 0;

      memset (device_param->pws_buf, 0, device_param->size_pws);

      while (words_extra)
      {
        const u32 work = get_work (hashcat_ctx, device_param, words_extra);

        if (work == 0) break;

        words_extra = 0;

        words_off = device_param->words_off;
        words_fin = words_off + work;

        char *line_buf;
        u32   line_len;

        char rule_buf_out[RP_PASSWORD_SIZE];

        for ( ; words_cur < words_off; words_cur++) get_next_word (hashcat_ctx_tmp, fd, &line_buf, &line_len);

        for ( ; words_cur < words_fin; words_cur++)
        {
          get_next_word (hashcat_ctx_tmp, fd, &line_buf, &line_len);

          line_len = convert_from_hex (hashcat_ctx, line_buf, line_len);

          // post-process rule engine

          if (run_rule_engine ((int) user_options_extra->rule_len_l, user_options->rule_buf_l))
          {
            if (line_len >= RP_PASSWORD_SIZE) continue;

            memset (rule_buf_out, 0, sizeof (rule_buf_out));

            const int rule_len_out = _old_apply_rule (user_options->rule_buf_l, (int) user_options_extra->rule_len_l, line_buf, (int) line_len, rule_buf_out);

            if (rule_len_out < 0) continue;

            line_buf = rule_buf_out;
            line_len = (u32) rule_len_out;
          }

          if (attack_kern == ATTACK_KERN_STRAIGHT)
          {
            if ((line_len < hashconfig->pw_min) || (line_len > hashconfig->pw_max))
            {
              words_extra++;

              continue;
            }
          }
          else if (attack_kern == ATTACK_KERN_COMBI)
          {
            // do not check if minimum restriction is satisfied (line_len >= hashconfig->pw_min) here
            // since we still need to combine the plains

            if (line_len > hashconfig->pw_max)
            {
              words_extra++;

              continue;
            }
          }

          pw_add (device_param, (u8 *) line_buf, (int) line_len);

          if (status_ctx->run_thread_level1 == false) break;
        }

        words_extra_total += words_extra;

        if (status_ctx->run_thread_level1 == false) break;
      }

      if (status_ctx->run_thread_level1 == false) break;

      if (words_extra_total > 0)
      {
        hc_thread_mutex_lock (status_ctx->mux_counter);

        for (u32 salt_pos = 0; salt_pos < hashes->salts_cnt; salt_pos++)
        {
          if (attack_kern == ATTACK_KERN_STRAIGHT)
          {
            status_ctx->words_progress_rejected[salt_pos] += words_extra_total * straight_ctx->kernel_rules_cnt;
          }
          else if (attack_kern == ATTACK_KERN_COMBI)
          {
            status_ctx->words_progress_rejected[salt_pos] += words_extra_total * combinator_ctx->combs_cnt;
          }
        }

        hc_thread_mutex_unlock (status_ctx->mux_counter);
      }

      //
      // flush
      //

      const u32 pws_cnt = device_param->pws_cnt;

      if (pws_cnt)
      {
        int CL_rc;

        CL_rc = run_copy (hashcat_ctx, device_param, pws_cnt);

        if (CL_rc == -1)
        {
          if (attack_mode == ATTACK_MODE_COMBI) fclose (device_param->combs_fp);

          fclose (fd);

          hcfree (hashcat_ctx_tmp->wl_data);

          hcfree (hashcat_ctx_tmp);

          return -1;
        }

        CL_rc = run_cracker (hashcat_ctx, device_param, pws_cnt);

        if (CL_rc == -1)
        {
          if (attack_mode == ATTACK_MODE_COMBI) fclose (device_param->combs_fp);

          fclose (fd);

          hcfree (hashcat_ctx_tmp->wl_data);

          hcfree (hashcat_ctx_tmp);

          return -1;
        }

        device_param->pws_cnt = 0;

        /*
        still required?
        if (attack_kern == ATTACK_KERN_STRAIGHT)
        {
          CL_rc = run_kernel_bzero (device_param, device_param->d_rules_c, device_param->size_rules_c);

          if (CL_rc == -1)
          {
            if (attack_mode == ATTACK_MODE_COMBI) fclose (device_param->combs_fp);

            fclose (fd);

            hcfree (hashcat_ctx_tmp->wl_data);

            hcfree (hashcat_ctx_tmp);

            return -1;
          }
        }
        else if (attack_kern == ATTACK_KERN_COMBI)
        {
          CL_rc = run_kernel_bzero (device_param, device_param->d_combs_c, device_param->size_combs);

          if (CL_rc == -1)
          {
            if (attack_mode == ATTACK_MODE_COMBI) fclose (device_param->combs_fp);

            fclose (fd);

            hcfree (hashcat_ctx_tmp->wl_data);

            hcfree (hashcat_ctx_tmp);

            return -1;
          }
        }
        */
      }

      if (user_options->speed_only == true) break;

      if (status_ctx->run_thread_level2 == true)
      {
        device_param->words_done = words_fin;

        status_ctx->words_cur = get_lowest_words_done (hashcat_ctx);
      }

      if (status_ctx->run_thread_level1 == false) break;

      if (words_fin == 0) break;
    }

    if (attack_mode == ATTACK_MODE_COMBI) fclose (device_param->combs_fp);

    fclose (fd);

    wl_data_destroy (hashcat_ctx_tmp);

    hcfree (hashcat_ctx_tmp->wl_data);

    hcfree (hashcat_ctx_tmp);
  }

  device_param->kernel_accel = 0;
  device_param->kernel_loops = 0;

  return 0;
}

void *thread_calc (void *p)
{
  thread_param_t *thread_param = (thread_param_t *) p;

  hashcat_ctx_t *hashcat_ctx = thread_param->hashcat_ctx;

  opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  if (opencl_ctx->enabled == false) return NULL;

  hc_device_param_t *device_param = opencl_ctx->devices_param + thread_param->tid;

  if (device_param->skipped) return NULL;

  calc (hashcat_ctx, device_param); // we should check the RC here

  return NULL;
}
