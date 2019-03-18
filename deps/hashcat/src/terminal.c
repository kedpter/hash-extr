/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "memory.h"
#include "event.h"
#include "convert.h"
#include "thread.h"
#include "timer.h"
#include "status.h"
#include "restore.h"
#include "shared.h"
#include "hwmon.h"
#include "interface.h"
#include "outfile.h"
#include "terminal.h"
#include "hashcat.h"

static const size_t TERMINAL_LINE_LENGTH = 79;

static const char *PROMPT = "[s]tatus [p]ause [r]esume [b]ypass [c]heckpoint [q]uit => ";

void welcome_screen (hashcat_ctx_t *hashcat_ctx, const char *version_tag)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->quiet       == true) return;
  if (user_options->keyspace    == true) return;
  if (user_options->stdout_flag == true) return;
  if (user_options->show        == true) return;
  if (user_options->left        == true) return;

  if (user_options->benchmark == true)
  {
    if (user_options->machine_readable == false)
    {
      event_log_info (hashcat_ctx, "%s (%s) starting in benchmark mode...", PROGNAME, version_tag);

      event_log_info (hashcat_ctx, NULL);

      if (user_options->workload_profile_chgd == false)
      {
        event_log_advice (hashcat_ctx, "Benchmarking uses hand-optimized kernel code by default.");
        event_log_advice (hashcat_ctx, "You can use it in your cracking session by setting the -O option.");
        event_log_advice (hashcat_ctx, "Note: Using optimized kernel code limits the maximum supported password length.");
        event_log_advice (hashcat_ctx, "To disable the optimized kernel code in benchmark mode, use the -w option.");
        event_log_advice (hashcat_ctx, NULL);
      }
    }
    else
    {
      event_log_info (hashcat_ctx, "# version: %s", version_tag);
    }
  }
  else if (user_options->restore == true)
  {
    event_log_info (hashcat_ctx, "%s (%s) starting in restore mode...", PROGNAME, version_tag);
    event_log_info (hashcat_ctx, NULL);
  }
  else if (user_options->speed_only == true)
  {
    event_log_info (hashcat_ctx, "%s (%s) starting in speed-only mode...", PROGNAME, version_tag);
    event_log_info (hashcat_ctx, NULL);
  }
  else if (user_options->progress_only == true)
  {
    event_log_info (hashcat_ctx, "%s (%s) starting in progress-only mode...", PROGNAME, version_tag);
    event_log_info (hashcat_ctx, NULL);
  }
  else
  {
    event_log_info (hashcat_ctx, "%s (%s) starting...", PROGNAME, version_tag);
    event_log_info (hashcat_ctx, NULL);
  }
}

void goodbye_screen (hashcat_ctx_t *hashcat_ctx, const hc_time_t proc_start, const hc_time_t proc_stop)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->quiet       == true) return;
  if (user_options->keyspace    == true) return;
  if (user_options->stdout_flag == true) return;
  if (user_options->show        == true) return;
  if (user_options->left        == true) return;

  char start_buf[32]; memset (start_buf, 0, sizeof (start_buf));
  char stop_buf[32];  memset (start_buf, 0, sizeof (stop_buf));

  event_log_info_nn (hashcat_ctx, "Started: %s", hc_ctime (&proc_start, start_buf, 32));
  event_log_info_nn (hashcat_ctx, "Stopped: %s", hc_ctime (&proc_stop,  stop_buf, 32));
}

int setup_console ()
{
  #if defined (_WIN)
  SetConsoleWindowSize (132);

  if (_setmode (_fileno (stdin), _O_BINARY) == -1)
  {
    __mingw_fprintf (stderr, "%s: %m", "stdin");

    return -1;
  }

  if (_setmode (_fileno (stdout), _O_BINARY) == -1)
  {
    __mingw_fprintf (stderr, "%s: %m", "stdin");

    return -1;
  }

  if (_setmode (_fileno (stderr), _O_BINARY) == -1)
  {
    __mingw_fprintf (stderr, "%s: %m", "stdin");

    return -1;
  }
  #endif

  return 0;
}

void send_prompt ()
{
  fprintf (stdout, "%s", PROMPT);

  fflush (stdout);
}

void clear_prompt ()
{
  fputc ('\r', stdout);

  for (size_t i = 0; i < strlen (PROMPT); i++)
  {
    fputc (' ', stdout);
  }

  fputc ('\r', stdout);

  fflush (stdout);
}

static void keypress (hashcat_ctx_t *hashcat_ctx)
{
  status_ctx_t   *status_ctx   = hashcat_ctx->status_ctx;
  user_options_t *user_options = hashcat_ctx->user_options;

  // this is required, because some of the variables down there are not initialized at that point
  while (status_ctx->devices_status == STATUS_INIT) usleep (100000);

  const bool quiet = user_options->quiet;

  tty_break ();

  while (status_ctx->shutdown_outer == false)
  {
    int ch = tty_getchar ();

    if (ch == -1) break;

    if (ch ==  0) continue;

    //https://github.com/hashcat/hashcat/issues/302
    //#if defined (_POSIX)
    //if (ch != '\n')
    //#endif

    hc_thread_mutex_lock (status_ctx->mux_display);

    event_log_info (hashcat_ctx, NULL);

    switch (ch)
    {
      case 's':
      case '\r':
      case '\n':

        event_log_info (hashcat_ctx, NULL);

        status_display (hashcat_ctx);

        event_log_info (hashcat_ctx, NULL);

        if (quiet == false) send_prompt ();

        break;

      case 'b':

        event_log_info (hashcat_ctx, NULL);

        bypass (hashcat_ctx);

        event_log_info (hashcat_ctx, "Next dictionary / mask in queue selected. Bypassing current one.");

        event_log_info (hashcat_ctx, NULL);

        if (quiet == false) send_prompt ();

        break;

      case 'p':

        event_log_info (hashcat_ctx, NULL);

        SuspendThreads (hashcat_ctx);

        if (status_ctx->devices_status == STATUS_PAUSED)
        {
          event_log_info (hashcat_ctx, "Paused");
        }

        event_log_info (hashcat_ctx, NULL);

        if (quiet == false) send_prompt ();

        break;

      case 'r':

        event_log_info (hashcat_ctx, NULL);

        ResumeThreads (hashcat_ctx);

        if (status_ctx->devices_status == STATUS_RUNNING)
        {
          event_log_info (hashcat_ctx, "Resumed");
        }

        event_log_info (hashcat_ctx, NULL);

        if (quiet == false) send_prompt ();

        break;

      case 'c':

        event_log_info (hashcat_ctx, NULL);

        stop_at_checkpoint (hashcat_ctx);

        if (status_ctx->checkpoint_shutdown == true)
        {
          event_log_info (hashcat_ctx, "Checkpoint enabled. Will quit at next restore-point update.");
        }
        else
        {
          event_log_info (hashcat_ctx, "Checkpoint disabled. Restore-point updates will no longer be monitored.");
        }

        event_log_info (hashcat_ctx, NULL);

        if (quiet == false) send_prompt ();

        break;

      case 'q':

        event_log_info (hashcat_ctx, NULL);

        myquit (hashcat_ctx);

        break;
    }

    //https://github.com/hashcat/hashcat/issues/302
    //#if defined (_POSIX)
    //if (ch != '\n')
    //#endif

    hc_thread_mutex_unlock (status_ctx->mux_display);
  }

  tty_fix ();
}

void *thread_keypress (void *p)
{
  hashcat_ctx_t *hashcat_ctx = (hashcat_ctx_t *) p;

  keypress (hashcat_ctx);

  return NULL;
}

#if defined (_WIN)
void SetConsoleWindowSize (const int x)
{
  HANDLE h = GetStdHandle (STD_OUTPUT_HANDLE);

  if (h == INVALID_HANDLE_VALUE) return;

  CONSOLE_SCREEN_BUFFER_INFO bufferInfo;

  if (!GetConsoleScreenBufferInfo (h, &bufferInfo)) return;

  SMALL_RECT *sr = &bufferInfo.srWindow;

  sr->Right = MAX (sr->Right, x - 1);

  COORD co;

  co.X = sr->Right + 1;
  co.Y = 9999;

  if (!SetConsoleScreenBufferSize (h, co)) return;

  if (!SetConsoleWindowInfo (h, TRUE, sr)) return;
}
#endif

#if defined (__linux__) || defined (__CYGWIN__)
static struct termios savemodes;
static int havemodes = 0;

int tty_break()
{
  struct termios modmodes;

  if (tcgetattr (fileno (stdin), &savemodes) < 0) return -1;

  havemodes = 1;

  modmodes = savemodes;
  modmodes.c_lflag &= ~ICANON;
  modmodes.c_cc[VMIN] = 1;
  modmodes.c_cc[VTIME] = 0;

  return tcsetattr (fileno (stdin), TCSANOW, &modmodes);
}

int tty_getchar()
{
  fd_set rfds;

  FD_ZERO (&rfds);

  FD_SET (fileno (stdin), &rfds);

  struct timeval tv;

  tv.tv_sec  = 1;
  tv.tv_usec = 0;

  int retval = select (1, &rfds, NULL, NULL, &tv);

  if (retval ==  0) return  0;
  if (retval == -1) return -1;

  return getchar();
}

int tty_fix()
{
  if (!havemodes) return 0;

  return tcsetattr (fileno (stdin), TCSADRAIN, &savemodes);
}
#endif

#if defined (__APPLE__) || defined (__FreeBSD__)
static struct termios savemodes;
static int havemodes = 0;

int tty_break()
{
  struct termios modmodes;

  if (ioctl (fileno (stdin), TIOCGETA, &savemodes) < 0) return -1;

  havemodes = 1;

  modmodes = savemodes;
  modmodes.c_lflag &= ~ICANON;
  modmodes.c_cc[VMIN] = 1;
  modmodes.c_cc[VTIME] = 0;

  return ioctl (fileno (stdin), TIOCSETAW, &modmodes);
}

int tty_getchar()
{
  fd_set rfds;

  FD_ZERO (&rfds);

  FD_SET (fileno (stdin), &rfds);

  struct timeval tv;

  tv.tv_sec  = 1;
  tv.tv_usec = 0;

  int retval = select (1, &rfds, NULL, NULL, &tv);

  if (retval ==  0) return  0;
  if (retval == -1) return -1;

  return getchar();
}

int tty_fix()
{
  if (!havemodes) return 0;

  return ioctl (fileno (stdin), TIOCSETAW, &savemodes);
}
#endif

#if defined (_WIN)
static DWORD saveMode = 0;

int tty_break()
{
  HANDLE stdinHandle = GetStdHandle (STD_INPUT_HANDLE);

  GetConsoleMode (stdinHandle, &saveMode);
  SetConsoleMode (stdinHandle, ENABLE_PROCESSED_INPUT);

  return 0;
}

int tty_getchar()
{
  HANDLE stdinHandle = GetStdHandle (STD_INPUT_HANDLE);

  DWORD rc = WaitForSingleObject (stdinHandle, 1000);

  if (rc == WAIT_TIMEOUT)   return  0;
  if (rc == WAIT_ABANDONED) return -1;
  if (rc == WAIT_FAILED)    return -1;

  // The whole ReadConsoleInput () part is a workaround.
  // For some unknown reason, maybe a mingw bug, a random signal
  // is sent to stdin which unblocks WaitForSingleObject () and sets rc 0.
  // Then it wants to read with getche () a keyboard input
  // which has never been made.

  INPUT_RECORD buf[100];

  DWORD num = 0;

  memset (buf, 0, sizeof (buf));

  ReadConsoleInput (stdinHandle, buf, 100, &num);

  FlushConsoleInputBuffer (stdinHandle);

  for (DWORD i = 0; i < num; i++)
  {
    if (buf[i].EventType != KEY_EVENT) continue;

    KEY_EVENT_RECORD KeyEvent = buf[i].Event.KeyEvent;

    if (KeyEvent.bKeyDown != TRUE) continue;

    return KeyEvent.uChar.AsciiChar;
  }

  return 0;
}

int tty_fix()
{
  HANDLE stdinHandle = GetStdHandle (STD_INPUT_HANDLE);

  SetConsoleMode (stdinHandle, saveMode);

  return 0;
}
#endif

void compress_terminal_line_length (char *out_buf, const size_t keep_from_beginning, const size_t keep_from_end)
{
  const size_t target_len = TERMINAL_LINE_LENGTH - keep_from_beginning;

  const size_t out_len = strlen (out_buf);

  if (out_len < target_len) return;

  char *ptr1 = out_buf + target_len - 3 - keep_from_end;
  char *ptr2 = out_buf + out_len - keep_from_end;

  *ptr1++ = '.';
  *ptr1++ = '.';
  *ptr1++ = '.';

  for (size_t i = 0; i < keep_from_end; i++)
  {
    *ptr1++ = *ptr2++;
  }

  *ptr1 = 0;
}

void example_hashes (hashcat_ctx_t *hashcat_ctx)
{
  user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->hash_mode_chgd == true)
  {
    const int rc = hashconfig_init (hashcat_ctx);

    if (rc == 0)
    {
      hashconfig_t *hashconfig = hashcat_ctx->hashconfig;

      event_log_info (hashcat_ctx, "MODE: %u", hashconfig->hash_mode);
      event_log_info (hashcat_ctx, "TYPE: %s", strhashtype (hashconfig->hash_mode));

      if ((hashconfig->st_hash != NULL) && (hashconfig->st_pass != NULL))
      {
        event_log_info (hashcat_ctx, "HASH: %s", hashconfig->st_hash);

        if (need_hexify ((const u8 *) hashconfig->st_pass, strlen (hashconfig->st_pass), user_options->separator, 0))
        {
          char tmp_buf[HCBUFSIZ_LARGE];

          int tmp_len = 0;

          tmp_buf[tmp_len++] = '$';
          tmp_buf[tmp_len++] = 'H';
          tmp_buf[tmp_len++] = 'E';
          tmp_buf[tmp_len++] = 'X';
          tmp_buf[tmp_len++] = '[';

          exec_hexify ((const u8 *) hashconfig->st_pass, strlen (hashconfig->st_pass), (u8 *) tmp_buf + tmp_len);

          tmp_len += strlen (hashconfig->st_pass) * 2;

          tmp_buf[tmp_len++] = ']';
          tmp_buf[tmp_len++] = 0;

          event_log_info (hashcat_ctx, "PASS: %s", tmp_buf);
        }
        else
        {
          event_log_info (hashcat_ctx, "PASS: %s", hashconfig->st_pass);
        }
      }
      else
      {
        event_log_info (hashcat_ctx, "HASH: not stored");
        event_log_info (hashcat_ctx, "PASS: not stored");
      }

      event_log_info (hashcat_ctx, NULL);
    }

    hashconfig_destroy (hashcat_ctx);
  }
  else
  {
    for (int i = 0; i < 100000; i++)
    {
      user_options->hash_mode = i;

      const int rc = hashconfig_init (hashcat_ctx);

      if (rc == 0)
      {
        hashconfig_t *hashconfig = hashcat_ctx->hashconfig;

        event_log_info (hashcat_ctx, "MODE: %u", hashconfig->hash_mode);
        event_log_info (hashcat_ctx, "TYPE: %s", strhashtype (hashconfig->hash_mode));

        if ((hashconfig->st_hash != NULL) && (hashconfig->st_pass != NULL))
        {
          event_log_info (hashcat_ctx, "HASH: %s", hashconfig->st_hash);

          if (need_hexify ((const u8 *) hashconfig->st_pass, strlen (hashconfig->st_pass), user_options->separator, 0))
          {
            char tmp_buf[HCBUFSIZ_LARGE];

            int tmp_len = 0;

            tmp_buf[tmp_len++] = '$';
            tmp_buf[tmp_len++] = 'H';
            tmp_buf[tmp_len++] = 'E';
            tmp_buf[tmp_len++] = 'X';
            tmp_buf[tmp_len++] = '[';

            exec_hexify ((const u8 *) hashconfig->st_pass, strlen (hashconfig->st_pass), (u8 *) tmp_buf + tmp_len);

            tmp_len += strlen (hashconfig->st_pass) * 2;

            tmp_buf[tmp_len++] = ']';
            tmp_buf[tmp_len++] = 0;

            event_log_info (hashcat_ctx, "PASS: %s", tmp_buf);
          }
          else
          {
            event_log_info (hashcat_ctx, "PASS: %s", hashconfig->st_pass);
          }
        }
        else
        {
          event_log_info (hashcat_ctx, "HASH: not stored");
          event_log_info (hashcat_ctx, "PASS: not stored");
        }

        event_log_info (hashcat_ctx, NULL);
      }

      hashconfig_destroy (hashcat_ctx);
    }
  }
}

void opencl_info (hashcat_ctx_t *hashcat_ctx)
{
  const opencl_ctx_t *opencl_ctx = hashcat_ctx->opencl_ctx;

  event_log_info (hashcat_ctx, "OpenCL Info:");
  event_log_info (hashcat_ctx, NULL);

  cl_uint         platforms_cnt         = opencl_ctx->platforms_cnt;
  cl_platform_id *platforms             = opencl_ctx->platforms;
  char          **platforms_vendor      = opencl_ctx->platforms_vendor;
  char          **platforms_name        = opencl_ctx->platforms_name;
  char          **platforms_version     = opencl_ctx->platforms_version;
  cl_uint         devices_cnt           = opencl_ctx->devices_cnt;

  for (cl_uint platforms_idx = 0; platforms_idx < platforms_cnt; platforms_idx++)
  {
    cl_platform_id platform_id       = platforms[platforms_idx];
    char          *platform_vendor   = platforms_vendor[platforms_idx];
    char          *platform_name     = platforms_name[platforms_idx];
    char          *platform_version  = platforms_version[platforms_idx];

    event_log_info (hashcat_ctx, "Platform ID #%u", platforms_idx + 1);
    event_log_info (hashcat_ctx, "  Vendor  : %s",  platform_vendor);
    event_log_info (hashcat_ctx, "  Name    : %s",  platform_name);
    event_log_info (hashcat_ctx, "  Version : %s",  platform_version);
    event_log_info (hashcat_ctx, NULL);

    for (cl_uint devices_idx = 0; devices_idx < devices_cnt; devices_idx++)
    {
      const hc_device_param_t *device_param = opencl_ctx->devices_param + devices_idx;

      if (device_param->platform != platform_id) continue;

      cl_device_type device_type                = device_param->device_type;
      cl_uint        device_vendor_id           = device_param->device_vendor_id;
      char          *device_vendor              = device_param->device_vendor;
      char          *device_name                = device_param->device_name;
      u32            device_processors          = device_param->device_processors;
      u32            device_maxclock_frequency  = device_param->device_maxclock_frequency;
      u64            device_maxmem_alloc        = device_param->device_maxmem_alloc;
      u64            device_global_mem          = device_param->device_global_mem;
      char          *device_opencl_version      = device_param->device_opencl_version;
      char          *device_version             = device_param->device_version;
      char          *driver_version             = device_param->driver_version;

      event_log_info (hashcat_ctx, "  Device ID #%u",         devices_idx + 1);
      event_log_info (hashcat_ctx, "    Type           : %s", ((device_type & CL_DEVICE_TYPE_CPU) ? "CPU" : ((device_type & CL_DEVICE_TYPE_GPU) ? "GPU" : "Accelerator")));
      event_log_info (hashcat_ctx, "    Vendor ID      : %u", device_vendor_id);
      event_log_info (hashcat_ctx, "    Vendor         : %s", device_vendor);
      event_log_info (hashcat_ctx, "    Name           : %s", device_name);
      event_log_info (hashcat_ctx, "    Version        : %s", device_version);
      event_log_info (hashcat_ctx, "    Processor(s)   : %u", device_processors);
      event_log_info (hashcat_ctx, "    Clock          : %u", device_maxclock_frequency);
      event_log_info (hashcat_ctx, "    Memory         : %" PRIu64 "/%" PRIu64 " MB allocatable", device_maxmem_alloc / 1024 / 1024, device_global_mem / 1024 / 1024);
      event_log_info (hashcat_ctx, "    OpenCL Version : %s", device_opencl_version);
      event_log_info (hashcat_ctx, "    Driver Version : %s", driver_version);
      event_log_info (hashcat_ctx, NULL);
    }
  }
}

void opencl_info_compact (hashcat_ctx_t *hashcat_ctx)
{
  const opencl_ctx_t   *opencl_ctx   = hashcat_ctx->opencl_ctx;
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->quiet            == true) return;
  if (user_options->machine_readable == true) return;

  cl_uint         platforms_cnt         = opencl_ctx->platforms_cnt;
  cl_platform_id *platforms             = opencl_ctx->platforms;
  char          **platforms_vendor      = opencl_ctx->platforms_vendor;
  bool           *platforms_skipped     = opencl_ctx->platforms_skipped;
  cl_uint         devices_cnt           = opencl_ctx->devices_cnt;

  for (cl_uint platforms_idx = 0; platforms_idx < platforms_cnt; platforms_idx++)
  {
    cl_platform_id platform_id       = platforms[platforms_idx];
    char          *platform_vendor   = platforms_vendor[platforms_idx];
    bool           platform_skipped  = platforms_skipped[platforms_idx];

    if (platform_skipped == false)
    {
      const int len = event_log_info (hashcat_ctx, "OpenCL Platform #%u: %s", platforms_idx + 1, platform_vendor);

      char line[HCBUFSIZ_TINY];

      memset (line, '=', len);

      line[len] = 0;

      event_log_info (hashcat_ctx, "%s", line);
    }
    else
    {
      event_log_info (hashcat_ctx, "OpenCL Platform #%u: %s, skipped or no OpenCL compatible devices found.", platforms_idx + 1, platform_vendor);
    }

    for (cl_uint devices_idx = 0; devices_idx < devices_cnt; devices_idx++)
    {
      const hc_device_param_t *device_param = opencl_ctx->devices_param + devices_idx;

      if (device_param->platform != platform_id) continue;

      char *device_name         = device_param->device_name;
      u32   device_processors   = device_param->device_processors;
      u64   device_maxmem_alloc = device_param->device_maxmem_alloc;
      u64   device_global_mem   = device_param->device_global_mem;

      if (device_param->skipped == false)
      {
        event_log_info (hashcat_ctx, "* Device #%u: %s, %" PRIu64 "/%" PRIu64 " MB allocatable, %uMCU",
                  devices_idx + 1,
                  device_name,
                  device_maxmem_alloc / 1024 / 1024,
                  device_global_mem   / 1024 / 1024,
                  device_processors);
      }
      else
      {
        event_log_info (hashcat_ctx, "* Device #%u: %s, skipped.",
                  devices_idx + 1,
                  device_name);
      }
    }

    event_log_info (hashcat_ctx, NULL);
  }
}

void status_display_machine_readable (hashcat_ctx_t *hashcat_ctx)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  hashcat_status_t *hashcat_status = (hashcat_status_t *) hcmalloc (sizeof (hashcat_status_t));

  const int rc_status = hashcat_get_status (hashcat_ctx, hashcat_status);

  if (rc_status == -1)
  {
    hcfree (hashcat_status);

    return;
  }

  printf ("STATUS\t%d\t", hashcat_status->status_number);

  printf ("SPEED\t");

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    printf ("%" PRIu64 "\t", (u64) (device_info->hashes_msec_dev * 1000));

    // that 1\t is for backward compatibility
    printf ("1000\t");
  }

  printf ("EXEC_RUNTIME\t");

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    printf ("%f\t", device_info->exec_msec_dev);
  }

  printf ("CURKU\t%" PRIu64 "\t", hashcat_status->restore_point);

  printf ("PROGRESS\t%" PRIu64 "\t%" PRIu64 "\t", hashcat_status->progress_cur_relative_skip, hashcat_status->progress_end_relative_skip);

  printf ("RECHASH\t%d\t%d\t", hashcat_status->digests_done, hashcat_status->digests_cnt);

  printf ("RECSALT\t%d\t%d\t", hashcat_status->salts_done, hashcat_status->salts_cnt);

  if (user_options->gpu_temp_disable == false)
  {
    printf ("TEMP\t");

    for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
    {
      const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

      if (device_info->skipped_dev == true) continue;

      // ok, little cheat here...

      const int temp = hm_get_temperature_with_device_id (hashcat_ctx, device_id);

      printf ("%d\t", temp);
    }
  }

  printf ("REJECTED\t%" PRIu64 "\t", hashcat_status->progress_rejected);

  hc_fwrite (EOL, strlen (EOL), 1, stdout);

  fflush (stdout);

  status_status_destroy (hashcat_ctx, hashcat_status);

  hcfree (hashcat_status);
}

void status_display (hashcat_ctx_t *hashcat_ctx)
{
  const hashconfig_t   *hashconfig   = hashcat_ctx->hashconfig;
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->machine_readable == true)
  {
    status_display_machine_readable (hashcat_ctx);

    return;
  }

  hashcat_status_t *hashcat_status = (hashcat_status_t *) hcmalloc (sizeof (hashcat_status_t));

  const int rc_status = hashcat_get_status (hashcat_ctx, hashcat_status);

  if (rc_status == -1)
  {
    hcfree (hashcat_status);

    return;
  }

  /**
   * show something
   */

  event_log_info (hashcat_ctx,
    "Session..........: %s",
    hashcat_status->session);

  event_log_info (hashcat_ctx,
    "Status...........: %s",
    hashcat_status->status_string);

  event_log_info (hashcat_ctx,
    "Hash.Type........: %s",
    hashcat_status->hash_type);

  event_log_info (hashcat_ctx,
    "Hash.Target......: %s",
    hashcat_status->hash_target);

  event_log_info (hashcat_ctx,
    "Time.Started.....: %s (%s)",
    hashcat_status->time_started_absolute,
    hashcat_status->time_started_relative);

  event_log_info (hashcat_ctx,
    "Time.Estimated...: %s (%s)",
    hashcat_status->time_estimated_absolute,
    hashcat_status->time_estimated_relative);

  switch (hashcat_status->guess_mode)
  {
    case GUESS_MODE_STRAIGHT_FILE:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: File (%s)",
        hashcat_status->guess_base);

      break;

    case GUESS_MODE_STRAIGHT_FILE_RULES_FILE:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: File (%s)",
        hashcat_status->guess_base);

      event_log_info (hashcat_ctx,
        "Guess.Mod........: Rules (%s)",
        hashcat_status->guess_mod);

      break;

    case GUESS_MODE_STRAIGHT_FILE_RULES_GEN:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: File (%s)",
        hashcat_status->guess_base);

      event_log_info (hashcat_ctx,
        "Guess.Mod........: Rules (Generated)");

      break;

    case GUESS_MODE_STRAIGHT_STDIN:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: Pipe");

      break;

    case GUESS_MODE_STRAIGHT_STDIN_RULES_FILE:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: Pipe");

      event_log_info (hashcat_ctx,
        "Guess.Mod........: Rules (%s)",
        hashcat_status->guess_mod);

      break;

    case GUESS_MODE_STRAIGHT_STDIN_RULES_GEN:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: Pipe");

      event_log_info (hashcat_ctx,
        "Guess.Mod........: Rules (Generated)");

      break;

    case GUESS_MODE_COMBINATOR_BASE_LEFT:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: File (%s), Left Side",
        hashcat_status->guess_base);

      event_log_info (hashcat_ctx,
        "Guess.Mod........: File (%s), Right Side",
        hashcat_status->guess_mod);

      break;

    case GUESS_MODE_COMBINATOR_BASE_RIGHT:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: File (%s), Right Side",
        hashcat_status->guess_base);

      event_log_info (hashcat_ctx,
        "Guess.Mod........: File (%s), Left Side",
        hashcat_status->guess_mod);

      break;

    case GUESS_MODE_MASK:

      event_log_info (hashcat_ctx,
        "Guess.Mask.......: %s [%d]",
        hashcat_status->guess_base,
        hashcat_status->guess_mask_length);

      break;

    case GUESS_MODE_MASK_CS:

      event_log_info (hashcat_ctx,
        "Guess.Mask.......: %s [%d]",
        hashcat_status->guess_base,
        hashcat_status->guess_mask_length);

      event_log_info (hashcat_ctx,
        "Guess.Charset....: %s ",
        hashcat_status->guess_charset);

      break;

    case GUESS_MODE_HYBRID1:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: File (%s), Left Side",
        hashcat_status->guess_base);

      event_log_info (hashcat_ctx,
        "Guess.Mod........: Mask (%s) [%d], Right Side",
        hashcat_status->guess_mod,
        hashcat_status->guess_mask_length);

      break;

    case GUESS_MODE_HYBRID1_CS:

      event_log_info (hashcat_ctx,
        "Guess.Base.......: File (%s), Left Side",
        hashcat_status->guess_base);

      event_log_info (hashcat_ctx,
        "Guess.Mod........: Mask (%s) [%d], Right Side",
        hashcat_status->guess_mod,
        hashcat_status->guess_mask_length);

      event_log_info (hashcat_ctx,
        "Guess.Charset....: %s",
        hashcat_status->guess_charset);

      break;

    case GUESS_MODE_HYBRID2:

      if (hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL)
      {
        event_log_info (hashcat_ctx,
          "Guess.Base.......: Mask (%s) [%d], Left Side",
          hashcat_status->guess_base,
          hashcat_status->guess_mask_length);

        event_log_info (hashcat_ctx,
          "Guess.Mod........: File (%s), Right Side",
          hashcat_status->guess_mod);
      }
      else
      {
        event_log_info (hashcat_ctx,
          "Guess.Base.......: File (%s), Right Side",
          hashcat_status->guess_base);

        event_log_info (hashcat_ctx,
          "Guess.Mod........: Mask (%s) [%d], Left Side",
          hashcat_status->guess_mod,
          hashcat_status->guess_mask_length);
      }

      break;

    case GUESS_MODE_HYBRID2_CS:

      if (hashconfig->opti_type & OPTI_TYPE_OPTIMIZED_KERNEL)
      {
        event_log_info (hashcat_ctx,
          "Guess.Base.......: Mask (%s) [%d], Left Side",
          hashcat_status->guess_base,
          hashcat_status->guess_mask_length);

        event_log_info (hashcat_ctx,
          "Guess.Mod........: File (%s), Right Side",
          hashcat_status->guess_mod);

        event_log_info (hashcat_ctx,
          "Guess.Charset....: %s",
          hashcat_status->guess_charset);
      }
      else
      {
        event_log_info (hashcat_ctx,
          "Guess.Base.......: File (%s), Right Side",
          hashcat_status->guess_base);

        event_log_info (hashcat_ctx,
          "Guess.Mod........: Mask (%s) [%d], Left Side",
          hashcat_status->guess_mod,
          hashcat_status->guess_mask_length);

        event_log_info (hashcat_ctx,
          "Guess.Charset....: %s",
          hashcat_status->guess_charset);
      }

      break;
  }

  switch (hashcat_status->guess_mode)
  {
    case GUESS_MODE_STRAIGHT_FILE:

      event_log_info (hashcat_ctx,
        "Guess.Queue......: %d/%d (%.02f%%)",
        hashcat_status->guess_base_offset,
        hashcat_status->guess_base_count,
        hashcat_status->guess_base_percent);

      break;

    case GUESS_MODE_STRAIGHT_FILE_RULES_FILE:

      event_log_info (hashcat_ctx,
        "Guess.Queue......: %d/%d (%.02f%%)",
        hashcat_status->guess_base_offset,
        hashcat_status->guess_base_count,
        hashcat_status->guess_base_percent);

      break;

    case GUESS_MODE_STRAIGHT_FILE_RULES_GEN:

      event_log_info (hashcat_ctx,
        "Guess.Queue......: %d/%d (%.02f%%)",
        hashcat_status->guess_base_offset,
        hashcat_status->guess_base_count,
        hashcat_status->guess_base_percent);

      break;

    case GUESS_MODE_MASK:

      event_log_info (hashcat_ctx,
        "Guess.Queue......: %d/%d (%.02f%%)",
        hashcat_status->guess_base_offset,
        hashcat_status->guess_base_count,
        hashcat_status->guess_base_percent);

      break;

    case GUESS_MODE_MASK_CS:

      event_log_info (hashcat_ctx,
        "Guess.Queue......: %d/%d (%.02f%%)",
        hashcat_status->guess_base_offset,
        hashcat_status->guess_base_count,
        hashcat_status->guess_base_percent);

      break;

    case GUESS_MODE_HYBRID1:

      event_log_info (hashcat_ctx,
        "Guess.Queue.Base.: %d/%d (%.02f%%)",
        hashcat_status->guess_base_offset,
        hashcat_status->guess_base_count,
        hashcat_status->guess_base_percent);

      event_log_info (hashcat_ctx,
        "Guess.Queue.Mod..: %d/%d (%.02f%%)",
        hashcat_status->guess_mod_offset,
        hashcat_status->guess_mod_count,
        hashcat_status->guess_mod_percent);

      break;

    case GUESS_MODE_HYBRID2:

      event_log_info (hashcat_ctx,
        "Guess.Queue.Base.: %d/%d (%.02f%%)",
        hashcat_status->guess_base_offset,
        hashcat_status->guess_base_count,
        hashcat_status->guess_base_percent);

      event_log_info (hashcat_ctx,
        "Guess.Queue.Mod..: %d/%d (%.02f%%)",
        hashcat_status->guess_mod_offset,
        hashcat_status->guess_mod_count,
        hashcat_status->guess_mod_percent);

      break;
  }

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    event_log_info (hashcat_ctx,
      "Speed.Dev.#%d.....: %9sH/s (%0.2fms)", device_id + 1,
      device_info->speed_sec_dev,
      device_info->exec_msec_dev);
  }

  if (hashcat_status->device_info_active > 1)
  {
    event_log_info (hashcat_ctx,
      "Speed.Dev.#*.....: %9sH/s",
      hashcat_status->speed_sec_all);
  }

  event_log_info (hashcat_ctx,
    "Recovered........: %d/%d (%.2f%%) Digests, %d/%d (%.2f%%) Salts",
    hashcat_status->digests_done,
    hashcat_status->digests_cnt,
    hashcat_status->digests_percent,
    hashcat_status->salts_done,
    hashcat_status->salts_cnt,
    hashcat_status->salts_percent);

  if (hashcat_status->digests_cnt > 1000)
  {
    event_log_info (hashcat_ctx,
      "Recovered/Time...: %s",
      hashcat_status->cpt);
  }

  switch (hashcat_status->progress_mode)
  {
    case PROGRESS_MODE_KEYSPACE_KNOWN:

      event_log_info (hashcat_ctx,
        "Progress.........: %" PRIu64 "/%" PRIu64 " (%.02f%%)",
        hashcat_status->progress_cur_relative_skip,
        hashcat_status->progress_end_relative_skip,
        hashcat_status->progress_finished_percent);

      event_log_info (hashcat_ctx,
        "Rejected.........: %" PRIu64 "/%" PRIu64 " (%.02f%%)",
        hashcat_status->progress_rejected,
        hashcat_status->progress_cur_relative_skip,
        hashcat_status->progress_rejected_percent);

      event_log_info (hashcat_ctx,
        "Restore.Point....: %" PRIu64 "/%" PRIu64 " (%.02f%%)",
        hashcat_status->restore_point,
        hashcat_status->restore_total,
        hashcat_status->restore_percent);

      break;

    case PROGRESS_MODE_KEYSPACE_UNKNOWN:

      event_log_info (hashcat_ctx,
        "Progress.........: %" PRIu64,
        hashcat_status->progress_cur_relative_skip);

      event_log_info (hashcat_ctx,
        "Rejected.........: %" PRIu64,
        hashcat_status->progress_rejected);

      event_log_info (hashcat_ctx,
        "Restore.Point....: %" PRIu64,
        hashcat_status->restore_point);

      break;
  }

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    if (device_info->guess_candidates_dev == NULL) continue;

    event_log_info (hashcat_ctx,
      "Candidates.#%d....: %s", device_id + 1,
      device_info->guess_candidates_dev);
  }

  if (user_options->gpu_temp_disable == false)
  {
    for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
    {
      const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

      if (device_info->skipped_dev == true) continue;

      if (device_info->hwmon_dev == NULL) continue;

      event_log_info (hashcat_ctx,
        "HWMon.Dev.#%d.....: %s", device_id + 1,
        device_info->hwmon_dev);
    }
  }

  status_status_destroy (hashcat_ctx, hashcat_status);

  hcfree (hashcat_status);
}

void status_benchmark_machine_readable (hashcat_ctx_t *hashcat_ctx)
{
  hashconfig_t *hashconfig = hashcat_ctx->hashconfig;

  const u32 hash_mode = hashconfig->hash_mode;

  hashcat_status_t *hashcat_status = (hashcat_status_t *) hcmalloc (sizeof (hashcat_status_t));

  const int rc_status = hashcat_get_status (hashcat_ctx, hashcat_status);

  if (rc_status == -1)
  {
    hcfree (hashcat_status);

    return;
  }

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    event_log_info (hashcat_ctx, "%d:%u:%d:%d:%.2f:%" PRIu64, device_id + 1, hash_mode, device_info->corespeed_dev, device_info->memoryspeed_dev, device_info->exec_msec_dev, (u64) (device_info->hashes_msec_dev_benchmark * 1000));
  }

  status_status_destroy (hashcat_ctx, hashcat_status);

  hcfree (hashcat_status);
}

void status_benchmark (hashcat_ctx_t *hashcat_ctx)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->machine_readable == true)
  {
    status_benchmark_machine_readable (hashcat_ctx);

    return;
  }

  hashcat_status_t *hashcat_status = (hashcat_status_t *) hcmalloc (sizeof (hashcat_status_t));

  const int rc_status = hashcat_get_status (hashcat_ctx, hashcat_status);

  if (rc_status == -1)
  {
    hcfree (hashcat_status);

    return;
  }

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    event_log_info (hashcat_ctx,
      "Speed.Dev.#%d.....: %9sH/s (%0.2fms)", device_id + 1,
      device_info->speed_sec_dev,
      device_info->exec_msec_dev);
  }

  if (hashcat_status->device_info_active > 1)
  {
    event_log_info (hashcat_ctx,
      "Speed.Dev.#*.....: %9sH/s",
      hashcat_status->speed_sec_all);
  }

  status_status_destroy (hashcat_ctx, hashcat_status);

  hcfree (hashcat_status);
}

void status_speed_machine_readable (hashcat_ctx_t *hashcat_ctx)
{
  hashcat_status_t *hashcat_status = (hashcat_status_t *) hcmalloc (sizeof (hashcat_status_t));

  const int rc_status = hashcat_get_status (hashcat_ctx, hashcat_status);

  if (rc_status == -1)
  {
    hcfree (hashcat_status);

    return;
  }

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    event_log_info (hashcat_ctx, "%d:%" PRIu64, device_id + 1, (u64) (device_info->hashes_msec_dev_benchmark * 1000));
  }

  status_status_destroy (hashcat_ctx, hashcat_status);

  hcfree (hashcat_status);
}

void status_speed (hashcat_ctx_t *hashcat_ctx)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->machine_readable == true)
  {
    status_speed_machine_readable (hashcat_ctx);

    return;
  }

  hashcat_status_t *hashcat_status = (hashcat_status_t *) hcmalloc (sizeof (hashcat_status_t));

  const int rc_status = hashcat_get_status (hashcat_ctx, hashcat_status);

  if (rc_status == -1)
  {
    hcfree (hashcat_status);

    return;
  }

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    event_log_info (hashcat_ctx,
      "Speed.Dev.#%d.....: %9sH/s (%0.2fms)", device_id + 1,
      device_info->speed_sec_dev,
      device_info->exec_msec_dev);
  }

  if (hashcat_status->device_info_active > 1)
  {
    event_log_info (hashcat_ctx,
      "Speed.Dev.#*.....: %9sH/s",
      hashcat_status->speed_sec_all);
  }

  status_status_destroy (hashcat_ctx, hashcat_status);

  hcfree (hashcat_status);
}

void status_progress_machine_readable (hashcat_ctx_t *hashcat_ctx)
{
  hashcat_status_t *hashcat_status = (hashcat_status_t *) hcmalloc (sizeof (hashcat_status_t));

  const int rc_status = hashcat_get_status (hashcat_ctx, hashcat_status);

  if (rc_status == -1)
  {
    hcfree (hashcat_status);

    return;
  }

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    event_log_info (hashcat_ctx, "%d:%d:%0.2f", device_id + 1, device_info->progress_dev, device_info->runtime_msec_dev);
  }

  status_status_destroy (hashcat_ctx, hashcat_status);

  hcfree (hashcat_status);
}

void status_progress (hashcat_ctx_t *hashcat_ctx)
{
  const user_options_t *user_options = hashcat_ctx->user_options;

  if (user_options->machine_readable == true)
  {
    status_progress_machine_readable (hashcat_ctx);

    return;
  }

  hashcat_status_t *hashcat_status = (hashcat_status_t *) hcmalloc (sizeof (hashcat_status_t));

  const int rc_status = hashcat_get_status (hashcat_ctx, hashcat_status);

  if (rc_status == -1)
  {
    hcfree (hashcat_status);

    return;
  }

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    event_log_info (hashcat_ctx,
      "Progress.Dev.#%d..: %d", device_id + 1,
      device_info->progress_dev);
  }

  for (int device_id = 0; device_id < hashcat_status->device_info_cnt; device_id++)
  {
    const device_info_t *device_info = hashcat_status->device_info_buf + device_id;

    if (device_info->skipped_dev == true) continue;

    event_log_info (hashcat_ctx,
      "Runtime.Dev.#%d...: %0.2fms", device_id + 1,
      device_info->runtime_msec_dev);
  }

  status_status_destroy (hashcat_ctx, hashcat_status);

  hcfree (hashcat_status);
}
