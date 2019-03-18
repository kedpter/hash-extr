/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "memory.h"
#include "bitops.h"
#include "event.h"
#include "dictstat.h"
#include "locking.h"
#include "shared.h"

int sort_by_dictstat (const void *s1, const void *s2)
{
  dictstat_t *d1 = (dictstat_t *) s1;
  dictstat_t *d2 = (dictstat_t *) s2;

  d2->stat.st_atime = d1->stat.st_atime;

  #if defined (STAT_NANOSECONDS_ACCESS_TIME)
  d2->stat.STAT_NANOSECONDS_ACCESS_TIME = d1->stat.STAT_NANOSECONDS_ACCESS_TIME;
  #endif

  const int rc_from = strcmp (d1->encoding_from, d2->encoding_from);

  if (rc_from != 0) return rc_from;

  const int rc_to = strcmp (d1->encoding_to, d2->encoding_to);

  if (rc_to != 0) return rc_to;

  return memcmp (&d1->stat, &d2->stat, sizeof (struct stat));
}

int dictstat_init (hashcat_ctx_t *hashcat_ctx)
{
  dictstat_ctx_t  *dictstat_ctx  = hashcat_ctx->dictstat_ctx;
  folder_config_t *folder_config = hashcat_ctx->folder_config;
  user_options_t  *user_options  = hashcat_ctx->user_options;

  dictstat_ctx->enabled = false;

  if (user_options->benchmark      == true) return 0;
  if (user_options->example_hashes == true) return 0;
  if (user_options->keyspace       == true) return 0;
  if (user_options->left           == true) return 0;
  if (user_options->opencl_info    == true) return 0;
  if (user_options->show           == true) return 0;
  if (user_options->usage          == true) return 0;
  if (user_options->version        == true) return 0;

  if (user_options->attack_mode == ATTACK_MODE_BF) return 0;

  if (user_options->hash_mode == 3000) return 0; // this mode virtually creates words in the wordlists

  dictstat_ctx->enabled  = true;
  dictstat_ctx->base     = (dictstat_t *) hccalloc (MAX_DICTSTAT, sizeof (dictstat_t));
  dictstat_ctx->cnt      = 0;

  hc_asprintf (&dictstat_ctx->filename, "%s/%s", folder_config->profile_dir, DICTSTAT_FILENAME);

  return 0;
}

void dictstat_destroy (hashcat_ctx_t *hashcat_ctx)
{
  dictstat_ctx_t *dictstat_ctx = hashcat_ctx->dictstat_ctx;

  if (dictstat_ctx->enabled == false) return;

  hcfree (dictstat_ctx->filename);
  hcfree (dictstat_ctx->base);

  memset (dictstat_ctx, 0, sizeof (dictstat_ctx_t));
}

void dictstat_read (hashcat_ctx_t *hashcat_ctx)
{
  dictstat_ctx_t *dictstat_ctx = hashcat_ctx->dictstat_ctx;

  if (dictstat_ctx->enabled == false) return;

  FILE *fp = fopen (dictstat_ctx->filename, "rb");

  if (fp == NULL)
  {
    // first run, file does not exist, do not error out

    return;
  }

  // parse header

  u64 v;
  u64 z;

  const size_t nread1 = hc_fread (&v, sizeof (u64), 1, fp);
  const size_t nread2 = hc_fread (&z, sizeof (u64), 1, fp);

  if ((nread1 != 1) || (nread2 != 1))
  {
    event_log_error (hashcat_ctx, "%s: Invalid header", dictstat_ctx->filename);

    fclose (fp);

    return;
  }

  v = byte_swap_64 (v);
  z = byte_swap_64 (z);

  if ((v & 0xffffffffffffff00) != (DICTSTAT_VERSION & 0xffffffffffffff00))
  {
    event_log_error (hashcat_ctx, "%s: Invalid header, ignoring content", dictstat_ctx->filename);

    fclose (fp);

    return;
  }

  if (z != 0)
  {
    event_log_error (hashcat_ctx, "%s: Invalid header, ignoring content", dictstat_ctx->filename);

    fclose (fp);

    return;
  }

  if ((v & 0xff) < (DICTSTAT_VERSION & 0xff))
  {
    event_log_warning (hashcat_ctx, "%s: Outdated header version, ignoring content", dictstat_ctx->filename);

    fclose (fp);

    return;
  }

  // parse data

  while (!feof (fp))
  {
    dictstat_t d;

    const size_t nread = hc_fread (&d, sizeof (dictstat_t), 1, fp);

    if (nread == 0) continue;

    lsearch (&d, dictstat_ctx->base, &dictstat_ctx->cnt, sizeof (dictstat_t), sort_by_dictstat);

    if (dictstat_ctx->cnt == MAX_DICTSTAT)
    {
      event_log_error (hashcat_ctx, "There are too many entries in the %s database. You have to remove/rename it.", dictstat_ctx->filename);

      break;
    }
  }

  fclose (fp);
}

int dictstat_write (hashcat_ctx_t *hashcat_ctx)
{
  dictstat_ctx_t *dictstat_ctx = hashcat_ctx->dictstat_ctx;

  if (dictstat_ctx->enabled == false) return 0;

  FILE *fp = fopen (dictstat_ctx->filename, "wb");

  if (fp == NULL)
  {
    event_log_error (hashcat_ctx, "%s: %s", dictstat_ctx->filename, strerror (errno));

    return -1;
  }

  if (lock_file (fp) == -1)
  {
    fclose (fp);

    event_log_error (hashcat_ctx, "%s: %s", dictstat_ctx->filename, strerror (errno));

    return -1;
  }

  // header

  u64 v = DICTSTAT_VERSION;
  u64 z = 0;

  v = byte_swap_64 (v);
  z = byte_swap_64 (z);

  hc_fwrite (&v, sizeof (u64), 1, fp);
  hc_fwrite (&z, sizeof (u64), 1, fp);

  // data

  hc_fwrite (dictstat_ctx->base, sizeof (dictstat_t), dictstat_ctx->cnt, fp);

  fclose (fp);

  return 0;
}

u64 dictstat_find (hashcat_ctx_t *hashcat_ctx, dictstat_t *d)
{
  dictstat_ctx_t *dictstat_ctx = hashcat_ctx->dictstat_ctx;

  if (dictstat_ctx->enabled == false) return 0;

  dictstat_t *d_cache = (dictstat_t *) lfind (d, dictstat_ctx->base, &dictstat_ctx->cnt, sizeof (dictstat_t), sort_by_dictstat);

  if (d_cache == NULL) return 0;

  return d_cache->cnt;
}

void dictstat_append (hashcat_ctx_t *hashcat_ctx, dictstat_t *d)
{
  dictstat_ctx_t *dictstat_ctx = hashcat_ctx->dictstat_ctx;

  if (dictstat_ctx->enabled == false) return;

  if (dictstat_ctx->cnt == MAX_DICTSTAT)
  {
    event_log_error (hashcat_ctx, "There are too many entries in the %s database. You have to remove/rename it.", dictstat_ctx->filename);

    return;
  }

  lsearch (d, dictstat_ctx->base, &dictstat_ctx->cnt, sizeof (dictstat_t), sort_by_dictstat);
}
