/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "memory.h"
#include "event.h"
#include "convert.h"
#include "restore.h"
#include "thread.h"
#include "timer.h"
#include "interface.h"
#include "hwmon.h"
#include "outfile.h"
#include "monitor.h"
#include "mpsp.h"
#include "terminal.h"
#include "status.h"
#include "shared.h"

static const char ST_0000[] = "Initializing";
static const char ST_0001[] = "Autotuning";
static const char ST_0002[] = "Selftest";
static const char ST_0003[] = "Running";
static const char ST_0004[] = "Paused";
static const char ST_0005[] = "Exhausted";
static const char ST_0006[] = "Cracked";
static const char ST_0007[] = "Aborted";
static const char ST_0008[] = "Quit";
static const char ST_0009[] = "Bypass";
static const char ST_0010[] = "Aborted (Checkpoint)";
static const char ST_0011[] = "Aborted (Runtime)";
static const char ST_0012[] = "Running (Checkpoint Quit requested)";
static const char ST_9999[] = "Unknown! Bug!";

static const char UNITS[7] = { ' ', 'k', 'M', 'G', 'T', 'P', 'E' };

static const char ETA_ABSOLUTE_MAX_EXCEEDED[] = "Next Big Bang"; // in honor of ighashgpu
static const char ETA_RELATIVE_MAX_EXCEEDED[] = "> 10 years";

static char *status_get_rules_file (const hashcat_ctx_t *hashcat_ctx)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->rp_files_cnt > 0)
  {
    char *tmp_buf = (char *) malloc (HCBUFSIZ_TINY);

    int tmp_len = 0;

    u32 i;

    for (i = 0; i < user_options->rp_files_cnt - 1; i++)
    {
      tmp_len += snprintf (tmp_buf + tmp_len, HCBUFSIZ_TINY - tmp_len - 1, "%s, ", user_options->rp_files[i]);
    }

    tmp_len += snprintf (tmp_buf + tmp_len, HCBUFSIZ_TINY - tmp_len - 1, "%s", user_options->rp_files[i]);

    tmp_buf[tmp_len] = 0;

    return tmp_buf; // yes, user need to free()
  }

  return NULL;
}

void format_timer_display (struct tm *tm, char *buf, size_t len)
{
  const char *time_entities_s[] = { "year",  "day",  "hour",  "min",  "sec"  };
  const char *time_entities_m[] = { "years", "days", "hours", "mins", "secs" };

  if (tm->tm_year - 70)
  {
    char *time_entity1 = ((tm->tm_year - 70) == 1) ? (char *) time_entities_s[0] : (char *) time_entities_m[0];
    char *time_entity2 = ( tm->tm_yday       == 1) ? (char *) time_entities_s[1] : (char *) time_entities_m[1];

    snprintf (buf, len - 1, "%d %s, %d %s", tm->tm_year - 70, time_entity1, tm->tm_yday, time_entity2);
  }
  else if (tm->tm_yday)
  {
    char *time_entity1 = (tm->tm_yday == 1) ? (char *) time_entities_s[1] : (char *) time_entities_m[1];
    char *time_entity2 = (tm->tm_hour == 1) ? (char *) time_entities_s[2] : (char *) time_entities_m[2];

    snprintf (buf, len - 1, "%d %s, %d %s", tm->tm_yday, time_entity1, tm->tm_hour, time_entity2);
  }
  else if (tm->tm_hour)
  {
    char *time_entity1 = (tm->tm_hour == 1) ? (char *) time_entities_s[2] : (char *) time_entities_m[2];
    char *time_entity2 = (tm->tm_min  == 1) ? (char *) time_entities_s[3] : (char *) time_entities_m[3];

    snprintf (buf, len - 1, "%d %s, %d %s", tm->tm_hour, time_entity1, tm->tm_min, time_entity2);
  }
  else if (tm->tm_min)
  {
    char *time_entity1 = (tm->tm_min == 1) ? (char *) time_entities_s[3] : (char *) time_entities_m[3];
    char *time_entity2 = (tm->tm_sec == 1) ? (char *) time_entities_s[4] : (char *) time_entities_m[4];

    snprintf (buf, len - 1, "%d %s, %d %s", tm->tm_min, time_entity1, tm->tm_sec, time_entity2);
  }
  else
  {
    char *time_entity1 = (tm->tm_sec == 1) ? (char *) time_entities_s[4] : (char *) time_entities_m[4];

    snprintf (buf, len - 1, "%d %s", tm->tm_sec, time_entity1);
  }
}

void format_speed_display (double val, char *buf, size_t len)
{
  if (val <= 0)
  {
    buf[0] = '0';
    buf[1] = ' ';
    buf[2] = 0;

    return;
  }

  u32 level = 0;

  while (val > 99999)
  {
    val /= 1000;

    level++;
  }

  /* generate output */

  if (level == 0)
  {
    snprintf (buf, len - 1, "%.0f ", val);
  }
  else
  {
    snprintf (buf, len - 1, "%.1f %c", val, UNITS[level]);
  }
}

double get_avg_exec_time (hc_device_param_t *device_param, const int last_num_entries)
{
  int exec_pos = (int) device_param->exec_pos - last_num_entries;

  if (exec_pos < 0) exec_pos += EXEC_CACHE;

  double exec_msec_sum = 0;

  int exec_msec_cnt = 0;

  for (int i = 0; i < last_num_entries; i++)
  {
    double exec_msec = device_param->exec_msec[(exec_pos + i) % EXEC_CACHE];

    if (exec_msec > 0)
    {
      exec_msec_sum += exec_msec;

      exec_msec_cnt++;
    }
  }

  if (exec_msec_cnt == 0) return 0;

  return exec_msec_sum / exec_msec_cnt;
}

int status_get_device_info_cnt (const hashcat_ctx_t *hashcat_ctx)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  return opencl_ctx->devices_cnt;
}

int status_get_device_info_active (const hashcat_ctx_t *hashcat_ctx)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  return opencl_ctx->devices_active;
}

bool status_get_skipped_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  return device_param->skipped;
}

char *status_get_session (const hashcat_ctx_t *hashcat_ctx)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  return strdup (user_options->session);
}

char *status_get_status_string (const hashcat_ctx_t *hashcat_ctx)
{
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  const int devices_status = status_ctx->devices_status;

  // special case: running but checkpoint quit requested

  if (devices_status == STATUS_RUNNING)
  {
    if (status_ctx->checkpoint_shutdown == true)
    {
      return ((char *) ST_0012);
    }
  }

  switch (devices_status)
  {
    case STATUS_INIT:               return ((char *) ST_0000);
    case STATUS_AUTOTUNE:           return ((char *) ST_0001);
    case STATUS_SELFTEST:           return ((char *) ST_0002);
    case STATUS_RUNNING:            return ((char *) ST_0003);
    case STATUS_PAUSED:             return ((char *) ST_0004);
    case STATUS_EXHAUSTED:          return ((char *) ST_0005);
    case STATUS_CRACKED:            return ((char *) ST_0006);
    case STATUS_ABORTED:            return ((char *) ST_0007);
    case STATUS_QUIT:               return ((char *) ST_0008);
    case STATUS_BYPASS:             return ((char *) ST_0009);
    case STATUS_ABORTED_CHECKPOINT: return ((char *) ST_0010);
    case STATUS_ABORTED_RUNTIME:    return ((char *) ST_0011);
  }

  return ((char *) ST_9999);
}

int status_get_status_number (const hashcat_ctx_t *hashcat_ctx)
{
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  return status_ctx->devices_status;
}

char *status_get_hash_type (const hashcat_ctx_t *hashcat_ctx)
{
  const hashconfig_t *hashconfig = hashcat_ctx->hashconfig;

  return strhashtype (hashconfig->hash_mode);
}

const char *status_get_hash_target (const hashcat_ctx_t *hashcat_ctx)
{
  const hashconfig_t *hashconfig = hashcat_ctx->hashconfig;
  const hashes_t     *hashes     = hashcat_ctx->hashes;

  if (hashes->digests_cnt == 1)
  {
    if ((hashconfig->hash_mode == 2500) || (hashconfig->hash_mode == 2501))
    {
      char *tmp_buf;

      wpa_t *wpa = (wpa_t *) hashes->esalts_buf;

      hc_asprintf (&tmp_buf, "%s (AP:%02x:%02x:%02x:%02x:%02x:%02x STA:%02x:%02x:%02x:%02x:%02x:%02x)",
        (char *) hashes->salts_buf[0].salt_buf,
        wpa->orig_mac_ap[0],
        wpa->orig_mac_ap[1],
        wpa->orig_mac_ap[2],
        wpa->orig_mac_ap[3],
        wpa->orig_mac_ap[4],
        wpa->orig_mac_ap[5],
        wpa->orig_mac_sta[0],
        wpa->orig_mac_sta[1],
        wpa->orig_mac_sta[2],
        wpa->orig_mac_sta[3],
        wpa->orig_mac_sta[4],
        wpa->orig_mac_sta[5]);

      return tmp_buf;
    }
    else if (hashconfig->hash_mode == 5200)
    {
      return hashes->hashfile;
    }
    else if (hashconfig->hash_mode == 9000)
    {
      return hashes->hashfile;
    }
    else if ((hashconfig->hash_mode >= 6200) && (hashconfig->hash_mode <= 6299))
    {
      return hashes->hashfile;
    }
    else if ((hashconfig->hash_mode >= 13700) && (hashconfig->hash_mode <= 13799))
    {
      return hashes->hashfile;
    }
    else
    {
      char *tmp_buf = (char *) malloc (HCBUFSIZ_LARGE);

      tmp_buf[0] = 0;

      ascii_digest ((hashcat_ctx_t *) hashcat_ctx, tmp_buf, HCBUFSIZ_LARGE, 0, 0);

      compress_terminal_line_length (tmp_buf, 19, 6); // 19 = strlen ("Hash.Target......: ")

      char *tmp_buf2 = strdup (tmp_buf);

      free (tmp_buf);

      return tmp_buf2;
    }
  }
  else
  {
    if (hashconfig->hash_mode == 3000)
    {
      char *tmp_buf;

      char out_buf1[64] = { 0 };
      char out_buf2[64] = { 0 };

      ascii_digest ((hashcat_ctx_t *) hashcat_ctx, out_buf1, sizeof (out_buf1), 0, 0);
      ascii_digest ((hashcat_ctx_t *) hashcat_ctx, out_buf2, sizeof (out_buf2), 0, 1);

      hc_asprintf (&tmp_buf, "%s, %s", out_buf1, out_buf2);

      return tmp_buf;
    }
    else
    {
      return hashes->hashfile;
    }
  }
}

int status_get_guess_mode (const hashcat_ctx_t *hashcat_ctx)
{
  const combinator_ctx_t     *combinator_ctx     = hashcat_ctx->combinator_ctx;
  const user_options_t       *user_options       = hashcat_ctx->user_options;
  const user_options_extra_t *user_options_extra = hashcat_ctx->user_options_extra;

  bool has_wordlist   = false;
  bool has_rule_file  = false;
  bool has_rule_gen   = false;
  bool has_base_left  = false;
  bool has_mask_cs    = false;

  if (user_options_extra->wordlist_mode == WL_MODE_FILE) has_wordlist = true;

  if (user_options->rp_files_cnt > 0) has_rule_file = true;
  if (user_options->rp_gen       > 0) has_rule_gen  = true;

  if (combinator_ctx->combs_mode == COMBINATOR_MODE_BASE_LEFT) has_base_left = true;

  if (user_options->custom_charset_1) has_mask_cs = true;
  if (user_options->custom_charset_2) has_mask_cs = true;
  if (user_options->custom_charset_3) has_mask_cs = true;
  if (user_options->custom_charset_4) has_mask_cs = true;

  if (user_options->attack_mode == ATTACK_MODE_STRAIGHT)
  {
    if (has_wordlist == true)
    {
      if (has_rule_file == true)
      {
        return GUESS_MODE_STRAIGHT_FILE_RULES_FILE;
      }
      else if (has_rule_gen == true)
      {
        return GUESS_MODE_STRAIGHT_FILE_RULES_GEN;
      }
      else
      {
        return GUESS_MODE_STRAIGHT_FILE;
      }
    }
    else
    {
      if (has_rule_file == true)
      {
        return GUESS_MODE_STRAIGHT_STDIN_RULES_FILE;
      }
      else if (has_rule_gen == true)
      {
        return GUESS_MODE_STRAIGHT_STDIN_RULES_GEN;
      }
      else
      {
        return GUESS_MODE_STRAIGHT_STDIN;
      }
    }
  }

  if (user_options->attack_mode == ATTACK_MODE_COMBI)
  {
    if (has_base_left == true)
    {
      return GUESS_MODE_COMBINATOR_BASE_LEFT;
    }
    else
    {
      return GUESS_MODE_COMBINATOR_BASE_RIGHT;
    }
  }

  if (user_options->attack_mode == ATTACK_MODE_BF)
  {
    if (has_mask_cs == true)
    {
      return GUESS_MODE_MASK_CS;
    }
    else
    {
      return GUESS_MODE_MASK;
    }
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID1)
  {
    if (has_mask_cs == true)
    {
      return GUESS_MODE_HYBRID1_CS;
    }
    else
    {
      return GUESS_MODE_HYBRID1;
    }
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID2)
  {
    if (has_mask_cs == true)
    {
      return GUESS_MODE_HYBRID2_CS;
    }
    else
    {
      return GUESS_MODE_HYBRID2;
    }
  }

  return GUESS_MODE_NONE;
}

char *status_get_guess_base (const hashcat_ctx_t *hashcat_ctx)
{
  const hashconfig_t         *hashconfig         = hashcat_ctx->hashconfig;
  const user_options_t       *user_options       = hashcat_ctx->user_options;
  const user_options_extra_t *user_options_extra = hashcat_ctx->user_options_extra;

  if (user_options->attack_mode == ATTACK_MODE_STRAIGHT)
  {
    if (user_options_extra->wordlist_mode == WL_MODE_FILE)
    {
      const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

      return strdup (straight_ctx->dict);
    }
  }

  if (user_options->attack_mode == ATTACK_MODE_COMBI)
  {
    const combinator_ctx_t *combinator_ctx = hashcat_ctx->combinator_ctx;

    if (combinator_ctx->combs_mode == COMBINATOR_MODE_BASE_LEFT)
    {
      return strdup (combinator_ctx->dict1);
    }
    else
    {
      return strdup (combinator_ctx->dict2);
    }
  }

  if (user_options->attack_mode == ATTACK_MODE_BF)
  {
    const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

    return strdup (mask_ctx->mask);
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID1)
  {
    const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

    return strdup (straight_ctx->dict);
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID2)
  {
    if (hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL)
    {
      const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

      return strdup (mask_ctx->mask);
    }
    else
    {
      const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

      return strdup (straight_ctx->dict);
    }
  }

  return NULL;
}

int status_get_guess_base_offset (const hashcat_ctx_t *hashcat_ctx)
{
  const hashconfig_t   *hashconfig   = hashcat_ctx->hashconfig;
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->attack_mode == ATTACK_MODE_STRAIGHT)
  {
    const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

    return straight_ctx->dicts_pos + 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_COMBI)
  {
    return 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_BF)
  {
    const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

    return mask_ctx->masks_pos + 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID1)
  {
    const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

    return straight_ctx->dicts_pos + 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID2)
  {
    if (hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL)
    {
      const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

      return mask_ctx->masks_pos + 1;
    }
    else
    {
      const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

      return straight_ctx->dicts_pos + 1;
    }
  }

  return 0;
}

int status_get_guess_base_count (const hashcat_ctx_t *hashcat_ctx)
{
  const hashconfig_t   *hashconfig   = hashcat_ctx->hashconfig;
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->attack_mode == ATTACK_MODE_STRAIGHT)
  {
    const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

    return straight_ctx->dicts_cnt;
  }

  if (user_options->attack_mode == ATTACK_MODE_COMBI)
  {
    return 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_BF)
  {
    const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

    return mask_ctx->masks_cnt;
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID1)
  {
    const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

    return straight_ctx->dicts_cnt;
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID2)
  {
    if (hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL)
    {
      const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

      return mask_ctx->masks_cnt;
    }
    else
    {
      const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

      return straight_ctx->dicts_cnt;
    }
  }

  return 0;
}

double status_get_guess_base_percent (const hashcat_ctx_t *hashcat_ctx)
{
  const int guess_base_offset = status_get_guess_base_offset (hashcat_ctx);
  const int guess_base_count  = status_get_guess_base_count (hashcat_ctx);

  if (guess_base_count == 0) return 0;

  return ((double) guess_base_offset / (double) guess_base_count) * 100;
}

char *status_get_guess_mod (const hashcat_ctx_t *hashcat_ctx)
{
  const hashconfig_t   *hashconfig   = hashcat_ctx->hashconfig;
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->attack_mode == ATTACK_MODE_STRAIGHT)
  {
    return status_get_rules_file (hashcat_ctx);
  }

  if (user_options->attack_mode == ATTACK_MODE_COMBI)
  {
    const combinator_ctx_t *combinator_ctx = hashcat_ctx->combinator_ctx;

    if (combinator_ctx->combs_mode == COMBINATOR_MODE_BASE_LEFT)
    {
      return strdup (combinator_ctx->dict2);
    }
    else
    {
      return strdup (combinator_ctx->dict1);
    }
  }

  if (user_options->attack_mode == ATTACK_MODE_BF)
  {

  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID1)
  {
    const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

    return strdup (mask_ctx->mask);
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID2)
  {
    if (hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL)
    {
      const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

      return strdup (straight_ctx->dict);
    }
    else
    {
      const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

      return strdup (mask_ctx->mask);
    }
  }

  return NULL;
}

int status_get_guess_mod_offset (const hashcat_ctx_t *hashcat_ctx)
{
  const hashconfig_t   *hashconfig   = hashcat_ctx->hashconfig;
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->attack_mode == ATTACK_MODE_STRAIGHT)
  {
    return 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_COMBI)
  {
    return 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_BF)
  {
    return 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID1)
  {
    const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

    return mask_ctx->masks_pos + 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID2)
  {
    if (hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL)
    {
      const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

      return straight_ctx->dicts_pos + 1;
    }
    else
    {
      const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

      return mask_ctx->masks_pos + 1;
    }
  }

  return 0;
}

int status_get_guess_mod_count (const hashcat_ctx_t *hashcat_ctx)
{
  const hashconfig_t   *hashconfig   = hashcat_ctx->hashconfig;
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->attack_mode == ATTACK_MODE_STRAIGHT)
  {
    return 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_COMBI)
  {
    return 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_BF)
  {
    return 1;
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID1)
  {
    const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

    return mask_ctx->masks_cnt;
  }

  if (user_options->attack_mode == ATTACK_MODE_HYBRID2)
  {
    if (hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL)
    {
      const straight_ctx_t *straight_ctx = hashcat_ctx->straight_ctx;

      return straight_ctx->dicts_cnt;
    }
    else
    {
      const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

      return mask_ctx->masks_cnt;
    }
  }

  return 0;
}

double status_get_guess_mod_percent (const hashcat_ctx_t *hashcat_ctx)
{
  const int guess_mod_offset = status_get_guess_mod_offset (hashcat_ctx);
  const int guess_mod_count  = status_get_guess_mod_count  (hashcat_ctx);

  if (guess_mod_count == 0) return 0;

  return ((double) guess_mod_offset / (double) guess_mod_count) * 100;
}

char *status_get_guess_charset (const hashcat_ctx_t *hashcat_ctx)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  const char *custom_charset_1 = user_options->custom_charset_1;
  const char *custom_charset_2 = user_options->custom_charset_2;
  const char *custom_charset_3 = user_options->custom_charset_3;
  const char *custom_charset_4 = user_options->custom_charset_4;

  if ((custom_charset_1 != NULL) || (custom_charset_2 != NULL) || (custom_charset_3 != NULL) || (custom_charset_4 != NULL))
  {
    char *tmp_buf;

    if (custom_charset_1 == NULL) custom_charset_1 = "Undefined";
    if (custom_charset_2 == NULL) custom_charset_2 = "Undefined";
    if (custom_charset_3 == NULL) custom_charset_3 = "Undefined";
    if (custom_charset_4 == NULL) custom_charset_4 = "Undefined";

    hc_asprintf (&tmp_buf, "-1 %s, -2 %s, -3 %s, -4 %s", custom_charset_1, custom_charset_2, custom_charset_3, custom_charset_4);

    return tmp_buf;
  }

  return NULL;
}

int status_get_guess_mask_length (const hashcat_ctx_t *hashcat_ctx)
{
  const mask_ctx_t *mask_ctx = hashcat_ctx->mask_ctx;

  if (mask_ctx == NULL) return -1;

  if (mask_ctx->mask == NULL) return -1;

  return mp_get_length (mask_ctx->mask);
}

char *status_get_guess_candidates_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const hashconfig_t         *hashconfig         = hashcat_ctx->hashconfig;
  const opencl_ctx_t         *opencl_ctx         = hashcat_ctx->opencl_ctx;
  const status_ctx_t         *status_ctx         = hashcat_ctx->status_ctx;
  const user_options_extra_t *user_options_extra = hashcat_ctx->user_options_extra;

  if (status_ctx->accessible == false) return NULL;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  char *display = (char *) malloc (HCBUFSIZ_TINY);

  if (user_options_extra->attack_kern == ATTACK_KERN_BF)
  {
    snprintf (display, HCBUFSIZ_TINY - 1, "[Generating]");
  }
  else
  {
    snprintf (display, HCBUFSIZ_TINY - 1, "[Copying]");
  }

  if (device_param->skipped == true) return display;

  if ((device_param->outerloop_left == 0) || (device_param->innerloop_left == 0)) return display;

  const u32 outerloop_first = 0;
  const u32 outerloop_last  = device_param->outerloop_left - 1;

  const u32 innerloop_first = 0;
  const u32 innerloop_last  = device_param->innerloop_left - 1;

  plain_t plain1 = { 0, 0, 0, outerloop_first, innerloop_first };
  plain_t plain2 = { 0, 0, 0, outerloop_last,  innerloop_last  };

  u32 plain_buf1[(64 * 2) + 2] = { 0 };
  u32 plain_buf2[(64 * 2) + 2] = { 0 };

  u8 *plain_ptr1 = (u8 *) plain_buf1;
  u8 *plain_ptr2 = (u8 *) plain_buf2;

  int plain_len1 = 0;
  int plain_len2 = 0;

  build_plain ((hashcat_ctx_t *) hashcat_ctx, device_param, &plain1, plain_buf1, &plain_len1);
  build_plain ((hashcat_ctx_t *) hashcat_ctx, device_param, &plain2, plain_buf2, &plain_len2);

  const bool always_ascii = (hashconfig->hash_type & OPTS_TYPE_PT_ALWAYS_ASCII) ? true : false;

  const bool need_hex1 = need_hexify (plain_ptr1, plain_len1, 0, always_ascii);
  const bool need_hex2 = need_hexify (plain_ptr2, plain_len2, 0, always_ascii);

  if ((need_hex1 == true) || (need_hex2 == true))
  {
    exec_hexify (plain_ptr1, plain_len1, plain_ptr1);
    exec_hexify (plain_ptr2, plain_len2, plain_ptr2);

    plain_ptr1[plain_len1 * 2] = 0;
    plain_ptr2[plain_len2 * 2] = 0;

    snprintf (display, HCBUFSIZ_TINY - 1, "$HEX[%s] -> $HEX[%s]", plain_ptr1, plain_ptr2);
  }
  else
  {
    plain_ptr1[plain_len1] = 0;
    plain_ptr2[plain_len2] = 0;

    snprintf (display, HCBUFSIZ_TINY - 1, "%s -> %s", plain_ptr1, plain_ptr2);
  }

  return display;
}

int status_get_digests_done (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t *hashes = hashcat_ctx->hashes;

  return hashes->digests_done;
}

int status_get_digests_cnt (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t *hashes = hashcat_ctx->hashes;

  return hashes->digests_cnt;
}

double status_get_digests_percent (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t *hashes = hashcat_ctx->hashes;

  if (hashes->digests_cnt == 0) return 0;

  return ((double) hashes->digests_done / (double) hashes->digests_cnt) * 100;
}

int status_get_salts_done (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t *hashes = hashcat_ctx->hashes;

  return hashes->salts_done;
}

int status_get_salts_cnt (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t *hashes = hashcat_ctx->hashes;

  return hashes->salts_cnt;
}

double status_get_salts_percent (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t *hashes = hashcat_ctx->hashes;

  if (hashes->salts_cnt == 0) return 0;

  return ((double) hashes->salts_done / (double) hashes->salts_cnt) * 100;
}

double status_get_msec_running (const hashcat_ctx_t *hashcat_ctx)
{
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  double msec_running = hc_timer_get (status_ctx->timer_running);

  return msec_running;
}

double status_get_msec_paused (const hashcat_ctx_t *hashcat_ctx)
{
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  double msec_paused = status_ctx->msec_paused;

  if (status_ctx->devices_status == STATUS_PAUSED)
  {
    double msec_paused_tmp = hc_timer_get (status_ctx->timer_paused);

    msec_paused += msec_paused_tmp;
  }

  return msec_paused;
}

double status_get_msec_real (const hashcat_ctx_t *hashcat_ctx)
{
  const double msec_running = status_get_msec_running (hashcat_ctx);
  const double msec_paused  = status_get_msec_paused  (hashcat_ctx);

  const double msec_real = msec_running - msec_paused;

  return msec_real;
}

char *status_get_time_started_absolute (const hashcat_ctx_t *hashcat_ctx)
{
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  const hc_time_t time_start = status_ctx->runtime_start;

  char buf[32] = { 0 };

  char *start = hc_ctime (&time_start, buf, 32);

  const size_t start_len = strlen (start);

  if (start[start_len - 1] == '\n') start[start_len - 1] = 0;
  if (start[start_len - 2] == '\r') start[start_len - 2] = 0;

  return strdup (start);
}

char *status_get_time_started_relative (const hashcat_ctx_t *hashcat_ctx)
{
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  hc_time_t time_now;

  hc_time (&time_now);

  const hc_time_t time_start = status_ctx->runtime_start;

  hc_time_t sec_run = time_now - time_start;

  struct tm *tmp;
  struct tm  tm;

  tmp = hc_gmtime (&sec_run, &tm);

  char *display_run = (char *) malloc (HCBUFSIZ_TINY);

  format_timer_display (tmp, display_run, HCBUFSIZ_TINY);

  return display_run;
}

hc_time_t status_get_sec_etc (const hashcat_ctx_t *hashcat_ctx)
{
  const status_ctx_t         *status_ctx         = hashcat_ctx->status_ctx;
  const user_options_extra_t *user_options_extra = hashcat_ctx->user_options_extra;

  hc_time_t sec_etc = 0;

  if ((user_options_extra->wordlist_mode == WL_MODE_FILE) || (user_options_extra->wordlist_mode == WL_MODE_MASK))
  {
    if (status_ctx->devices_status != STATUS_CRACKED)
    {
      const u64 progress_cur_relative_skip = status_get_progress_cur_relative_skip (hashcat_ctx);
      const u64 progress_end_relative_skip = status_get_progress_end_relative_skip (hashcat_ctx);

      const u64 progress_ignore = status_get_progress_ignore (hashcat_ctx);

      const double hashes_msec_all = status_get_hashes_msec_all (hashcat_ctx);

      if (hashes_msec_all > 0)
      {
        const u64 progress_left_relative_skip = progress_end_relative_skip - progress_cur_relative_skip;

        u64 msec_left = (u64) ((progress_left_relative_skip - progress_ignore) / hashes_msec_all);

        sec_etc = msec_left / 1000;
      }
    }
  }

  return sec_etc;
}

char *status_get_time_estimated_absolute (const hashcat_ctx_t *hashcat_ctx)
{
  hc_time_t sec_etc = status_get_sec_etc (hashcat_ctx);

  hc_time_t now;
  hc_time (&now);


  char buf[32] = { 0 };

  char *etc;

  if (overflow_check_u64_add (now, sec_etc) == false)
  {
    etc = (char *) ETA_ABSOLUTE_MAX_EXCEEDED;
  }
  else
  {
    hc_time_t end = now + sec_etc;

    etc = hc_ctime (&end, buf, sizeof (buf));

    if (etc == NULL) etc = (char *) ETA_ABSOLUTE_MAX_EXCEEDED;
  }

  const size_t etc_len = strlen (etc);

  if (etc[etc_len - 1] == '\n') etc[etc_len - 1] = 0;
  if (etc[etc_len - 2] == '\r') etc[etc_len - 2] = 0;

  return strdup (etc);
}

char *status_get_time_estimated_relative (const hashcat_ctx_t *hashcat_ctx)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  char *display = (char *) malloc (HCBUFSIZ_TINY);

  hc_time_t sec_etc = status_get_sec_etc (hashcat_ctx);

  struct tm *tmp;
  struct tm  tm;

  tmp = hc_gmtime (&sec_etc, &tm);

  if (tmp == NULL)
  {
    snprintf (display, HCBUFSIZ_TINY, "%s", ETA_RELATIVE_MAX_EXCEEDED);
  }
  else
  {
    format_timer_display (tmp, display, HCBUFSIZ_TINY);
  }

  if (user_options->runtime > 0)
  {
    const int runtime_left = get_runtime_left (hashcat_ctx);

    char *tmp_display = strdup (display);

    if (runtime_left > 0)
    {
      hc_time_t sec_left = runtime_left;

      struct tm *tmp_left;
      struct tm  tm_left;

      tmp_left = hc_gmtime (&sec_left, &tm_left);

      char *display_left = (char *) malloc (HCBUFSIZ_TINY);

      format_timer_display (tmp_left, display_left, HCBUFSIZ_TINY);

      snprintf (display, HCBUFSIZ_TINY - 1, "%s; Runtime limited: %s", tmp_display, display_left);

      free (display_left);
    }
    else
    {
      snprintf (display, HCBUFSIZ_TINY - 1, "%s; Runtime limit exceeded", tmp_display);
    }

    free (tmp_display);
  }

  return display;
}

u64 status_get_restore_point (const hashcat_ctx_t *hashcat_ctx)
{
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  const u64 restore_point = status_ctx->words_cur;

  return restore_point;
}

u64 status_get_restore_total (const hashcat_ctx_t *hashcat_ctx)
{
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  const u64 restore_total = status_ctx->words_base;

  return restore_total;
}

double status_get_restore_percent (const hashcat_ctx_t *hashcat_ctx)
{
  double restore_percent = 0;

  const u64 restore_point = status_get_restore_point (hashcat_ctx);
  const u64 restore_total = status_get_restore_total (hashcat_ctx);

  if (restore_total > 0)
  {
    restore_percent = ((double) restore_point / (double) restore_total) * 100;
  }

  return restore_percent;
}

int status_get_progress_mode (const hashcat_ctx_t *hashcat_ctx)
{
  const u64 progress_end_relative_skip = status_get_progress_end_relative_skip (hashcat_ctx);

  if (progress_end_relative_skip > 0)
  {
    return PROGRESS_MODE_KEYSPACE_KNOWN;
  }
  else
  {
    return PROGRESS_MODE_KEYSPACE_UNKNOWN;
  }
}

double status_get_progress_finished_percent (const hashcat_ctx_t *hashcat_ctx)
{
  const u64 progress_cur_relative_skip = status_get_progress_cur_relative_skip (hashcat_ctx);
  const u64 progress_end_relative_skip = status_get_progress_end_relative_skip (hashcat_ctx);

  double progress_finished_percent = 0;

  if (progress_end_relative_skip > 0)
  {
    progress_finished_percent = ((double) progress_cur_relative_skip / (double) progress_end_relative_skip) * 100;
  }

  return progress_finished_percent;
}

u64 status_get_progress_done (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t     *hashes     = hashcat_ctx->hashes;
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  u64 progress_done = 0;

  for (u32 salt_pos = 0; salt_pos < hashes->salts_cnt; salt_pos++)
  {
    progress_done += status_ctx->words_progress_done[salt_pos];
  }

  return progress_done;
}

u64 status_get_progress_rejected (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t     *hashes     = hashcat_ctx->hashes;
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  u64 progress_rejected = 0;

  for (u32 salt_pos = 0; salt_pos < hashes->salts_cnt; salt_pos++)
  {
    progress_rejected += status_ctx->words_progress_rejected[salt_pos];
  }

  return progress_rejected;
}

double status_get_progress_rejected_percent (const hashcat_ctx_t *hashcat_ctx)
{
  const u64 progress_cur      = status_get_progress_cur      (hashcat_ctx);
  const u64 progress_rejected = status_get_progress_rejected (hashcat_ctx);

  double percent_rejected = 0;

  if (progress_cur)
  {
    percent_rejected = ((double) (progress_rejected) / (double) progress_cur) * 100;
  }

  return percent_rejected;
}

u64 status_get_progress_restored (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t     *hashes     = hashcat_ctx->hashes;
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  u64 progress_restored = 0;

  for (u32 salt_pos = 0; salt_pos < hashes->salts_cnt; salt_pos++)
  {
    progress_restored += status_ctx->words_progress_restored[salt_pos];
  }

  return progress_restored;
}

u64 status_get_progress_cur (const hashcat_ctx_t *hashcat_ctx)
{
  const u64 progress_done     = status_get_progress_done     (hashcat_ctx);
  const u64 progress_rejected = status_get_progress_rejected (hashcat_ctx);
  const u64 progress_restored = status_get_progress_restored (hashcat_ctx);

  const u64 progress_cur = progress_done + progress_rejected + progress_restored;

  return progress_cur;
}

u64 status_get_progress_ignore (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t     *hashes     = hashcat_ctx->hashes;
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  // Important for ETA only

  u64 progress_ignore = 0;

  for (u32 salt_pos = 0; salt_pos < hashes->salts_cnt; salt_pos++)
  {
    if (hashes->salts_shown[salt_pos] == 1)
    {
      const u64 all = status_ctx->words_progress_done[salt_pos]
                    + status_ctx->words_progress_rejected[salt_pos]
                    + status_ctx->words_progress_restored[salt_pos];

      const u64 left = status_ctx->words_cnt - all;

      progress_ignore += left;
    }
  }

  return progress_ignore;
}

u64 status_get_progress_end (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t             *hashes             = hashcat_ctx->hashes;
  const status_ctx_t         *status_ctx         = hashcat_ctx->status_ctx;
  const user_options_t       *user_options       = hashcat_ctx->user_options;
  const user_options_extra_t *user_options_extra = hashcat_ctx->user_options_extra;

  u64 progress_end = status_ctx->words_cnt * hashes->salts_cnt;

  if (user_options->limit)
  {
    const combinator_ctx_t *combinator_ctx = hashcat_ctx->combinator_ctx;
    const mask_ctx_t       *mask_ctx       = hashcat_ctx->mask_ctx;
    const straight_ctx_t   *straight_ctx   = hashcat_ctx->straight_ctx;

    progress_end = MIN (user_options->limit, status_ctx->words_base) * hashes->salts_cnt;

    if      (user_options_extra->attack_kern == ATTACK_KERN_STRAIGHT) progress_end  *= straight_ctx->kernel_rules_cnt;
    else if (user_options_extra->attack_kern == ATTACK_KERN_COMBI)    progress_end  *= combinator_ctx->combs_cnt;
    else if (user_options_extra->attack_kern == ATTACK_KERN_BF)       progress_end  *= mask_ctx->bfs_cnt;
  }

  return progress_end;
}

u64 status_get_progress_skip (const hashcat_ctx_t *hashcat_ctx)
{
  const hashes_t             *hashes             = hashcat_ctx->hashes;
  const status_ctx_t         *status_ctx         = hashcat_ctx->status_ctx;
  const user_options_t       *user_options       = hashcat_ctx->user_options;
  const user_options_extra_t *user_options_extra = hashcat_ctx->user_options_extra;

  u64 progress_skip = 0;

  if (user_options->skip)
  {
    const combinator_ctx_t *combinator_ctx = hashcat_ctx->combinator_ctx;
    const mask_ctx_t       *mask_ctx       = hashcat_ctx->mask_ctx;
    const straight_ctx_t   *straight_ctx   = hashcat_ctx->straight_ctx;

    progress_skip = MIN (user_options->skip, status_ctx->words_base) * hashes->salts_cnt;

    if      (user_options_extra->attack_kern == ATTACK_KERN_STRAIGHT) progress_skip *= straight_ctx->kernel_rules_cnt;
    else if (user_options_extra->attack_kern == ATTACK_KERN_COMBI)    progress_skip *= combinator_ctx->combs_cnt;
    else if (user_options_extra->attack_kern == ATTACK_KERN_BF)       progress_skip *= mask_ctx->bfs_cnt;
  }

  return progress_skip;
}

u64 status_get_progress_cur_relative_skip (const hashcat_ctx_t *hashcat_ctx)
{
  const u64 progress_skip = status_get_progress_skip (hashcat_ctx);
  const u64 progress_cur  = status_get_progress_cur  (hashcat_ctx);

  u64 progress_cur_relative_skip = 0;

  if (progress_cur > 0)
  {
    progress_cur_relative_skip = progress_cur - progress_skip;
  }

  return progress_cur_relative_skip;
}

u64 status_get_progress_end_relative_skip (const hashcat_ctx_t *hashcat_ctx)
{
  const u64 progress_skip = status_get_progress_skip (hashcat_ctx);
  const u64 progress_end  = status_get_progress_end  (hashcat_ctx);

  u64 progress_end_relative_skip = 0;

  if (progress_end > 0)
  {
    progress_end_relative_skip = progress_end - progress_skip;
  }

  return progress_end_relative_skip;
}

double status_get_hashes_msec_all (const hashcat_ctx_t *hashcat_ctx)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  double hashes_all_msec = 0;

  for (u32 device_id = 0; device_id < opencl_ctx->devices_cnt; device_id++)
  {
    hashes_all_msec += status_get_hashes_msec_dev (hashcat_ctx, device_id);
  }

  return hashes_all_msec;
}

double status_get_hashes_msec_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  u64    speed_cnt  = 0;
  double speed_msec = 0;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  if (device_param->skipped == false)
  {
    for (int i = 0; i < SPEED_CACHE; i++)
    {
      speed_cnt  += device_param->speed_cnt[i];
      speed_msec += device_param->speed_msec[i];
    }
  }

  speed_cnt  /= SPEED_CACHE;
  speed_msec /= SPEED_CACHE;

  double hashes_dev_msec = 0;

  if (speed_msec > 0)
  {
    hashes_dev_msec = (double) speed_cnt / speed_msec;
  }

  return hashes_dev_msec;
}

double status_get_hashes_msec_dev_benchmark (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  // this function increases accuracy for benchmark modes

  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  u64    speed_cnt  = 0;
  double speed_msec = 0;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  if (device_param->skipped == false)
  {
    speed_cnt  += device_param->speed_cnt[0];
    speed_msec += device_param->speed_msec[0];
  }

  double hashes_dev_msec = 0;

  if (speed_msec > 0)
  {
    hashes_dev_msec = (double) speed_cnt / speed_msec;
  }

  return hashes_dev_msec;
}

double status_get_exec_msec_all (const hashcat_ctx_t *hashcat_ctx)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  double exec_all_msec = 0;

  for (u32 device_id = 0; device_id < opencl_ctx->devices_cnt; device_id++)
  {
    exec_all_msec += status_get_exec_msec_dev (hashcat_ctx, device_id);
  }

  return exec_all_msec;
}

double status_get_exec_msec_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  double exec_dev_msec = 0;

  if (device_param->skipped == false)
  {
    exec_dev_msec = get_avg_exec_time (device_param, EXEC_CACHE);
  }

  return exec_dev_msec;
}

char *status_get_speed_sec_all (const hashcat_ctx_t *hashcat_ctx)
{
  const double hashes_msec_all = status_get_hashes_msec_all (hashcat_ctx);

  char *display = (char *) malloc (HCBUFSIZ_TINY);

  format_speed_display (hashes_msec_all * 1000, display, HCBUFSIZ_TINY);

  return display;
}

char *status_get_speed_sec_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const double hashes_msec_dev = status_get_hashes_msec_dev (hashcat_ctx, device_id);

  char *display = (char *) malloc (HCBUFSIZ_TINY);

  format_speed_display (hashes_msec_dev * 1000, display, HCBUFSIZ_TINY);

  return display;
}

int status_get_cpt_cur_min (const hashcat_ctx_t *hashcat_ctx)
{
  const cpt_ctx_t    *cpt_ctx    = hashcat_ctx->cpt_ctx;
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  if (status_ctx->accessible == false) return 0;

  const hc_time_t now = hc_time (NULL);

  int cpt_cur_min = 0;

  for (int i = 0; i < CPT_CACHE; i++)
  {
    const u32       cracked   = cpt_ctx->cpt_buf[i].cracked;
    const hc_time_t timestamp = cpt_ctx->cpt_buf[i].timestamp;

    if ((timestamp + 60) > now)
    {
      cpt_cur_min += cracked;
    }
  }

  return cpt_cur_min;
}

int status_get_cpt_cur_hour (const hashcat_ctx_t *hashcat_ctx)
{
  const cpt_ctx_t    *cpt_ctx    = hashcat_ctx->cpt_ctx;
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  if (status_ctx->accessible == false) return 0;

  const hc_time_t now = hc_time (NULL);

  int cpt_cur_hour = 0;

  for (int i = 0; i < CPT_CACHE; i++)
  {
    const u32       cracked   = cpt_ctx->cpt_buf[i].cracked;
    const hc_time_t timestamp = cpt_ctx->cpt_buf[i].timestamp;

    if ((timestamp + 3600) > now)
    {
      cpt_cur_hour += cracked;
    }
  }

  return cpt_cur_hour;
}

int status_get_cpt_cur_day (const hashcat_ctx_t *hashcat_ctx)
{
  const cpt_ctx_t    *cpt_ctx    = hashcat_ctx->cpt_ctx;
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  if (status_ctx->accessible == false) return 0;

  const hc_time_t now = hc_time (NULL);

  int cpt_cur_day = 0;

  for (int i = 0; i < CPT_CACHE; i++)
  {
    const u32       cracked   = cpt_ctx->cpt_buf[i].cracked;
    const hc_time_t timestamp = cpt_ctx->cpt_buf[i].timestamp;

    if ((timestamp + 86400) > now)
    {
      cpt_cur_day += cracked;
    }
  }

  return cpt_cur_day;
}

int status_get_cpt_avg_min (const hashcat_ctx_t *hashcat_ctx)
{
  const cpt_ctx_t *cpt_ctx = hashcat_ctx->cpt_ctx;

  const double msec_real = status_get_msec_real (hashcat_ctx);

  const double cpt_avg_min = (double) cpt_ctx->cpt_total / ((msec_real / 1000) / 60);

  return (int) cpt_avg_min;
}

int status_get_cpt_avg_hour (const hashcat_ctx_t *hashcat_ctx)
{
  const cpt_ctx_t *cpt_ctx = hashcat_ctx->cpt_ctx;

  const double msec_real = status_get_msec_real (hashcat_ctx);

  const double cpt_avg_hour = (double) cpt_ctx->cpt_total / ((msec_real / 1000) / 3600);

  return (int) cpt_avg_hour;
}

int status_get_cpt_avg_day (const hashcat_ctx_t *hashcat_ctx)
{
  const cpt_ctx_t *cpt_ctx = hashcat_ctx->cpt_ctx;

  const double msec_real = status_get_msec_real (hashcat_ctx);

  const double cpt_avg_day = (double) cpt_ctx->cpt_total / ((msec_real / 1000) / 86400);

  return (int) cpt_avg_day;
}

char *status_get_cpt (const hashcat_ctx_t *hashcat_ctx)
{
  const cpt_ctx_t *cpt_ctx = hashcat_ctx->cpt_ctx;

  const hc_time_t now = hc_time (NULL);

  char *cpt;

  const int cpt_cur_min  = status_get_cpt_cur_min  (hashcat_ctx);
  const int cpt_cur_hour = status_get_cpt_cur_hour (hashcat_ctx);
  const int cpt_cur_day  = status_get_cpt_cur_day  (hashcat_ctx);

  const int cpt_avg_min  = status_get_cpt_avg_min  (hashcat_ctx);
  const int cpt_avg_hour = status_get_cpt_avg_hour (hashcat_ctx);
  const int cpt_avg_day  = status_get_cpt_avg_day  (hashcat_ctx);

  if ((cpt_ctx->cpt_start + 86400) < now)
  {
    hc_asprintf (&cpt, "CUR:%d,%d,%d AVG:%d,%d,%d (Min,Hour,Day)",
      cpt_cur_min,
      cpt_cur_hour,
      cpt_cur_day,
      cpt_avg_min,
      cpt_avg_hour,
      cpt_avg_day);
  }
  else if ((cpt_ctx->cpt_start + 3600) < now)
  {
    hc_asprintf (&cpt, "CUR:%d,%d,N/A AVG:%d,%d,%d (Min,Hour,Day)",
      cpt_cur_min,
      cpt_cur_hour,
      cpt_avg_min,
      cpt_avg_hour,
      cpt_avg_day);
  }
  else if ((cpt_ctx->cpt_start + 60) < now)
  {
    hc_asprintf (&cpt, "CUR:%d,N/A,N/A AVG:%d,%d,%d (Min,Hour,Day)",
      cpt_cur_min,
      cpt_avg_min,
      cpt_avg_hour,
      cpt_avg_day);
  }
  else
  {
    hc_asprintf (&cpt, "CUR:N/A,N/A,N/A AVG:%d,%d,%d (Min,Hour,Day)",
      cpt_avg_min,
      cpt_avg_hour,
      cpt_avg_day);
  }

  return cpt;
}

char *status_get_hwmon_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  char *output_buf = (char *) malloc (HCBUFSIZ_TINY);

  snprintf (output_buf, HCBUFSIZ_TINY - 1, "N/A");

  if (device_param->skipped == true) return output_buf;

  status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  hc_thread_mutex_lock (status_ctx->mux_hwmon);

  const int num_temperature = hm_get_temperature_with_device_id ((hashcat_ctx_t *) hashcat_ctx, device_id);
  const int num_fanspeed    = hm_get_fanspeed_with_device_id    ((hashcat_ctx_t *) hashcat_ctx, device_id);
  const int num_utilization = hm_get_utilization_with_device_id ((hashcat_ctx_t *) hashcat_ctx, device_id);
  const int num_corespeed   = hm_get_corespeed_with_device_id   ((hashcat_ctx_t *) hashcat_ctx, device_id);
  const int num_memoryspeed = hm_get_memoryspeed_with_device_id ((hashcat_ctx_t *) hashcat_ctx, device_id);
  const int num_buslanes    = hm_get_buslanes_with_device_id    ((hashcat_ctx_t *) hashcat_ctx, device_id);

  int output_len = 0;

  if (num_temperature >= 0)
  {
    output_len += snprintf (output_buf + output_len, HCBUFSIZ_TINY - output_len, "Temp:%3dc ", num_temperature);
  }

  if (num_fanspeed >= 0)
  {
    output_len += snprintf (output_buf + output_len, HCBUFSIZ_TINY - output_len, "Fan:%3d%% ", num_fanspeed);
  }

  if (num_utilization >= 0)
  {
    output_len += snprintf (output_buf + output_len, HCBUFSIZ_TINY - output_len, "Util:%3d%% ", num_utilization);
  }

  if (num_corespeed >= 0)
  {
    output_len += snprintf (output_buf + output_len, HCBUFSIZ_TINY - output_len, "Core:%4dMHz ", num_corespeed);
  }

  if (num_memoryspeed >= 0)
  {
    output_len += snprintf (output_buf + output_len, HCBUFSIZ_TINY - output_len, "Mem:%4dMHz ", num_memoryspeed);
  }

  if (num_buslanes >= 0)
  {
    output_len += snprintf (output_buf + output_len, HCBUFSIZ_TINY - output_len, "Bus:%d ", num_buslanes);
  }

  if (output_len > 0)
  {
    // trims the trailing space

    output_buf[output_len - 1] = 0;
  }
  else
  {
    snprintf (output_buf, HCBUFSIZ_TINY - 1, "N/A");
  }

  hc_thread_mutex_unlock (status_ctx->mux_hwmon);

  return output_buf;
}

int status_get_corespeed_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  if (device_param->skipped == true) return -1;

  status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  hc_thread_mutex_lock (status_ctx->mux_hwmon);

  const int num_corespeed = hm_get_corespeed_with_device_id ((hashcat_ctx_t *) hashcat_ctx, device_id);

  hc_thread_mutex_unlock (status_ctx->mux_hwmon);

  return num_corespeed;
}

int status_get_memoryspeed_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  if (device_param->skipped == true) return -1;

  status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  hc_thread_mutex_lock (status_ctx->mux_hwmon);

  const int num_memoryspeed = hm_get_memoryspeed_with_device_id ((hashcat_ctx_t *) hashcat_ctx, device_id);

  hc_thread_mutex_unlock (status_ctx->mux_hwmon);

  return num_memoryspeed;
}

int status_get_progress_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  if (device_param->skipped == true) return 0;

  return device_param->outerloop_left;
}

double status_get_runtime_msec_dev (const hashcat_ctx_t *hashcat_ctx, const int device_id)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  hc_device_param_t *device_param = &opencl_ctx->devices_param[device_id];

  if (device_param->skipped == true) return 0;

  return device_param->outerloop_msec;
}

int status_progress_init (hashcat_ctx_t *hashcat_ctx)
{
  status_ctx_t *status_ctx = hashcat_ctx->status_ctx;
  hashes_t     *hashes     = hashcat_ctx->hashes;

  status_ctx->words_progress_done     = (u64 *) hccalloc (hashes->salts_cnt, sizeof (u64));
  status_ctx->words_progress_rejected = (u64 *) hccalloc (hashes->salts_cnt, sizeof (u64));
  status_ctx->words_progress_restored = (u64 *) hccalloc (hashes->salts_cnt, sizeof (u64));

  return 0;
}

void status_progress_destroy (hashcat_ctx_t *hashcat_ctx)
{
  status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  hcfree (status_ctx->words_progress_done);
  hcfree (status_ctx->words_progress_rejected);
  hcfree (status_ctx->words_progress_restored);

  status_ctx->words_progress_done     = NULL;
  status_ctx->words_progress_rejected = NULL;
  status_ctx->words_progress_restored = NULL;
}

void status_progress_reset (hashcat_ctx_t *hashcat_ctx)
{
  status_ctx_t *status_ctx = hashcat_ctx->status_ctx;
  hashes_t     *hashes     = hashcat_ctx->hashes;

  memset (status_ctx->words_progress_done,     0, hashes->salts_cnt * sizeof (u64));
  memset (status_ctx->words_progress_rejected, 0, hashes->salts_cnt * sizeof (u64));
  memset (status_ctx->words_progress_restored, 0, hashes->salts_cnt * sizeof (u64));
}

int status_ctx_init (hashcat_ctx_t *hashcat_ctx)
{
  status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  status_ctx->devices_status = STATUS_INIT;

  status_ctx->run_main_level1     = true;
  status_ctx->run_main_level2     = true;
  status_ctx->run_main_level3     = true;
  status_ctx->run_thread_level1   = true;
  status_ctx->run_thread_level2   = true;

  status_ctx->shutdown_inner      = false;
  status_ctx->shutdown_outer      = false;

  status_ctx->checkpoint_shutdown = false;

  status_ctx->hashcat_status_final = (hashcat_status_t *) hcmalloc (sizeof (hashcat_status_t));

  hc_thread_mutex_init (status_ctx->mux_dispatcher);
  hc_thread_mutex_init (status_ctx->mux_counter);
  hc_thread_mutex_init (status_ctx->mux_display);
  hc_thread_mutex_init (status_ctx->mux_hwmon);

  return 0;
}

void status_ctx_destroy (hashcat_ctx_t *hashcat_ctx)
{
  status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  hc_thread_mutex_delete (status_ctx->mux_dispatcher);
  hc_thread_mutex_delete (status_ctx->mux_counter);
  hc_thread_mutex_delete (status_ctx->mux_display);
  hc_thread_mutex_delete (status_ctx->mux_hwmon);

  hcfree (status_ctx->hashcat_status_final);

  memset (status_ctx, 0, sizeof (status_ctx_t));
}


void status_status_destroy (hashcat_ctx_t *hashcat_ctx, hashcat_status_t *hashcat_status)
{
  const status_ctx_t *status_ctx = hashcat_ctx->status_ctx;

  if (status_ctx == NULL) return;

  if (status_ctx->accessible == false) return;

  hcfree (hashcat_status->session);
  hcfree (hashcat_status->time_estimated_absolute);
  hcfree (hashcat_status->time_estimated_relative);
  hcfree (hashcat_status->time_started_absolute);
  hcfree (hashcat_status->time_started_relative);
  hcfree (hashcat_status->speed_sec_all);
  hcfree (hashcat_status->guess_base);
  hcfree (hashcat_status->guess_mod);
  hcfree (hashcat_status->guess_charset);
  hcfree (hashcat_status->cpt);

  hashcat_status->session                 = NULL;
  hashcat_status->time_estimated_absolute = NULL;
  hashcat_status->time_estimated_relative = NULL;
  hashcat_status->time_started_absolute   = NULL;
  hashcat_status->time_started_relative   = NULL;
  hashcat_status->speed_sec_all           = NULL;
  hashcat_status->guess_base              = NULL;
  hashcat_status->guess_mod               = NULL;
  hashcat_status->guess_charset           = NULL;
  hashcat_status->cpt                     = NULL;

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    hcfree (device_info->speed_sec_dev);
    hcfree (device_info->guess_candidates_dev);
    hcfree (device_info->hwmon_dev);

    device_info->speed_sec_dev        = NULL;
    device_info->guess_candidates_dev = NULL;
    device_info->hwmon_dev            = NULL;
  }
}
