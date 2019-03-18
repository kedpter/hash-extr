/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "shared.h"

static inline int get_msb32 (const u32 v)
{
  int i;

  for (i = 32; i > 0; i--) if ((v >> (i - 1)) & 1) break;

  return i;
}

static inline int get_msb64 (const u64 v)
{
  int i;

  for (i = 64; i > 0; i--) if ((v >> (i - 1)) & 1) break;

  return i;
}

bool overflow_check_u32_add (const u32 a, const u32 b)
{
  const int a_msb = get_msb32 (a);
  const int b_msb = get_msb32 (b);

  return ((a_msb < 32) && (b_msb < 32));
}

bool overflow_check_u32_mul (const u32 a, const u32 b)
{
  const int a_msb = get_msb32 (a);
  const int b_msb = get_msb32 (b);

  return ((a_msb + b_msb) < 32);
}

bool overflow_check_u64_add (const u64 a, const u64 b)
{
  const int a_msb = get_msb64 (a);
  const int b_msb = get_msb64 (b);

  return ((a_msb < 64) && (b_msb < 64));
}

bool overflow_check_u64_mul (const u64 a, const u64 b)
{
  const int a_msb = get_msb64 (a);
  const int b_msb = get_msb64 (b);

  return ((a_msb + b_msb) < 64);
}

bool is_power_of_2 (const u32 v)
{
  return (v && !(v & (v - 1)));
}

u32 mydivc32 (const u32 dividend, const u32 divisor)
{
  u32 quotient = dividend / divisor;

  if (dividend % divisor) quotient++;

  return quotient;
}

u64 mydivc64 (const u64 dividend, const u64 divisor)
{
  u64 quotient = dividend / divisor;

  if (dividend % divisor) quotient++;

  return quotient;
}

char *filename_from_filepath (char *filepath)
{
  char *ptr = NULL;

  if ((ptr = strrchr (filepath, '/')) != NULL)
  {
    ptr++;
  }
  else if ((ptr = strrchr (filepath, '\\')) != NULL)
  {
    ptr++;
  }
  else
  {
    ptr = filepath;
  }

  return ptr;
}

void naive_replace (char *s, const char key_char, const char replace_char)
{
  const size_t len = strlen (s);

  for (size_t in = 0; in < len; in++)
  {
    const char c = s[in];

    if (c == key_char)
    {
      s[in] = replace_char;
    }
  }
}

void naive_escape (char *s, size_t s_max, const char key_char, const char escape_char)
{
  char s_escaped[1024] = { 0 };

  size_t s_escaped_max = sizeof (s_escaped);

  const size_t len = strlen (s);

  for (size_t in = 0, out = 0; in < len; in++, out++)
  {
    const char c = s[in];

    if (c == key_char)
    {
      s_escaped[out] = escape_char;

      out++;
    }

    if (out == s_escaped_max - 2) break;

    s_escaped[out] = c;
  }

  strncpy (s, s_escaped, s_max - 1);
}

void hc_asprintf (char **strp, const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  int rc __attribute__((unused));
  rc = vasprintf (strp, fmt, args);
  va_end (args);
}

#if defined (_WIN)
int hc_stat (const char *pathname, hc_stat_t *buf)
{
  return stat64 (pathname, buf);
}

int hc_fstat (int fd, hc_stat_t *buf)
{
  return fstat64 (fd, buf);
}
#else
int hc_stat (const char *pathname, hc_stat_t *buf)
{
  return stat (pathname, buf);
}

int hc_fstat (int fd, hc_stat_t *buf)
{
  return fstat (fd, buf);
}
#endif

#if defined (_WIN)
#define __WINDOWS__
#endif
#include "sort_r.h"
#if defined (_WIN)
#undef __WINDOWS__
#endif

void hc_qsort_r (void *base, size_t nmemb, size_t size, int (*compar) (const void *, const void *, void *), void *arg)
{
  sort_r (base, nmemb, size, compar, arg);
}

void *hc_bsearch_r (const void *key, const void *base, size_t nmemb, size_t size, int (*compar) (const void *, const void *, void *), void *arg)
{
  for (size_t l = 0, r = nmemb; r; r >>= 1)
  {
    const size_t m = r >> 1;

    const size_t c = l + m;

    const char *next = (char *) base + (c * size);

    const int cmp = (*compar) (key, next, arg);

    if (cmp > 0)
    {
      l += m + 1;

      r--;
    }

    if (cmp == 0) return ((void *) next);
  }

  return (NULL);
}

bool hc_path_is_file (const char *path)
{
  hc_stat_t s;

  if (hc_stat (path, &s) == -1) return false;

  if (S_ISREG (s.st_mode)) return true;

  return false;
}

bool hc_path_is_directory (const char *path)
{
  hc_stat_t s;

  if (hc_stat (path, &s) == -1) return false;

  if (S_ISDIR (s.st_mode)) return true;

  return false;
}

bool hc_path_is_empty (const char *path)
{
  hc_stat_t s;

  if (hc_stat (path, &s) == -1) return false;

  if (s.st_size == 0) return true;

  return false;
}

bool hc_path_exist (const char *path)
{
  if (access (path, F_OK) == -1) return false;

  return true;
}

bool hc_path_read (const char *path)
{
  if (access (path, R_OK) == -1) return false;

  return true;
}

bool hc_path_write (const char *path)
{
  if (access (path, W_OK) == -1) return false;

  return true;
}

bool hc_path_create (const char *path)
{
  if (hc_path_exist (path) == true) return false;

  const int fd = creat (path, S_IRUSR | S_IWUSR);

  if (fd == -1) return false;

  close (fd);

  unlink (path);

  return true;
}

bool hc_string_is_digit (const char *s)
{
  if (s == NULL) return false;

  const size_t len = strlen (s);

  if (len == 0) return false;

  for (size_t i = 0; i < len; i++)
  {
    const int c = (const int) s[i];

    if (isdigit (c) == 0) return false;
  }

  return true;
}

void setup_environment_variables ()
{
  char *compute = getenv ("COMPUTE");

  if (compute)
  {
    char *display;

    hc_asprintf (&display, "DISPLAY=%s", compute);

    putenv (display);

    free (display);
  }
  else
  {
    if (getenv ("DISPLAY") == NULL)
      putenv ((char *) "DISPLAY=:0");
  }

  if (getenv ("OCL_CODE_CACHE_ENABLE") == NULL)
    putenv ((char *) "OCL_CODE_CACHE_ENABLE=0");

  if (getenv ("CUDA_CACHE_DISABLE") == NULL)
    putenv ((char *) "CUDA_CACHE_DISABLE=1");

  if (getenv ("POCL_KERNEL_CACHE") == NULL)
    putenv ((char *) "POCL_KERNEL_CACHE=0");
}

void setup_umask ()
{
  umask (077);
}

void setup_seeding (const bool rp_gen_seed_chgd, const u32 rp_gen_seed)
{
  if (rp_gen_seed_chgd == true)
  {
    srand (rp_gen_seed);
  }
  else
  {
    hc_time_t ts;

    hc_time (&ts);

    srand (ts);
  }
}

u32 get_random_num (const u32 min, const u32 max)
{
  if (min == max) return (min);

  const u32 low = max - min;

  if (low == 0) return (0);

  #if defined (_WIN)

  return (((u32) rand () % (max - min)) + min);

  #else

  return (((u32) random () % (max - min)) + min);

  #endif
}

void hc_string_trim_leading (char *s)
{
  int skip = 0;

  const int len = (int) strlen (s);

  for (int i = 0; i < len; i++)
  {
    const int c = (const int) s[i];

    if (isspace (c) == 0) break;

    skip++;
  }

  if (skip == 0) return;

  const int new_len = len - skip;

  memmove (s, s + skip, new_len);

  s[new_len] = 0;
}

void hc_string_trim_trailing (char *s)
{
  int skip = 0;

  const int len = (int) strlen (s);

  for (int i = len - 1; i >= 0; i--)
  {
    const int c = (const int) s[i];

    if (isspace (c) == 0) break;

    skip++;
  }

  if (skip == 0) return;

  const size_t new_len = len - skip;

  s[new_len] = 0;
}

size_t hc_fread (void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  return fread (ptr, size, nmemb, stream);
}

void hc_fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t rc = fwrite (ptr, size, nmemb, stream);

  if (rc == 0) rc = 0;
}


hc_time_t hc_time (hc_time_t *t)
{
  #if defined (_WIN)
  return _time64 (t);
  #else
  return time (t);
  #endif
}

struct tm *hc_gmtime (const hc_time_t *t, MAYBE_UNUSED struct tm *result)
{
  #if defined (_WIN)
  return _gmtime64 (t);
  #else
  return gmtime_r (t, result);
  #endif
}

char *hc_ctime (const hc_time_t *t, char *buf, MAYBE_UNUSED const size_t buf_size)
{
  char *etc = NULL;

  #if defined (_WIN)
  etc = _ctime64 (t);

  if (etc != NULL)
  {
    snprintf (buf, buf_size, "%s", etc);
  }
  #else
  etc = ctime_r (t, buf); // buf should have room for at least 26 bytes
  #endif

  return etc;
}

bool hc_same_files (char *file1, char *file2)
{
  if ((file1 != NULL) && (file2 != NULL))
  {
    hc_stat_t tmpstat_file1;
    hc_stat_t tmpstat_file2;

    int do_check = 0;

    FILE *fp;

    fp = fopen (file1, "r");

    if (fp)
    {
      if (hc_fstat (fileno (fp), &tmpstat_file1))
      {
        fclose (fp);

        return false;
      }

      fclose (fp);

      do_check++;
    }

    fp = fopen (file2, "r");

    if (fp)
    {
      if (hc_fstat (fileno (fp), &tmpstat_file2))
      {
        fclose (fp);

        return false;
      }

      fclose (fp);

      do_check++;
    }

    if (do_check == 2)
    {
      tmpstat_file1.st_mode     = 0;
      tmpstat_file1.st_nlink    = 0;
      tmpstat_file1.st_uid      = 0;
      tmpstat_file1.st_gid      = 0;
      tmpstat_file1.st_rdev     = 0;
      tmpstat_file1.st_atime    = 0;

      #if defined (STAT_NANOSECONDS_ACCESS_TIME)
      tmpstat_file1.STAT_NANOSECONDS_ACCESS_TIME = 0;
      #endif

      #if defined (_POSIX)
      tmpstat_file1.st_blksize  = 0;
      tmpstat_file1.st_blocks   = 0;
      #endif

      tmpstat_file2.st_mode     = 0;
      tmpstat_file2.st_nlink    = 0;
      tmpstat_file2.st_uid      = 0;
      tmpstat_file2.st_gid      = 0;
      tmpstat_file2.st_rdev     = 0;
      tmpstat_file2.st_atime    = 0;

      #if defined (STAT_NANOSECONDS_ACCESS_TIME)
      tmpstat_file2.STAT_NANOSECONDS_ACCESS_TIME = 0;
      #endif

      #if defined (_POSIX)
      tmpstat_file2.st_blksize  = 0;
      tmpstat_file2.st_blocks   = 0;
      #endif

      if (memcmp (&tmpstat_file1, &tmpstat_file2, sizeof (hc_stat_t)) == 0)
      {
        return true;
      }
    }
  }

  return false;
}
