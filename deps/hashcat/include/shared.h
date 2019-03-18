/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _SHARED_H
#define _SHARED_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>

bool overflow_check_u32_add (const u32 a, const u32 b);
bool overflow_check_u32_mul (const u32 a, const u32 b);
bool overflow_check_u64_add (const u64 a, const u64 b);
bool overflow_check_u64_mul (const u64 a, const u64 b);

bool is_power_of_2 (const u32 v);

u32 get_random_num (const u32 min, const u32 max);

u32 mydivc32 (const u32 dividend, const u32 divisor);
u64 mydivc64 (const u64 dividend, const u64 divisor);

char *filename_from_filepath (char *filepath);

void naive_replace (char *s, const char key_char, const char replace_char);
void naive_escape (char *s, size_t s_max, const char key_char, const char escape_char);

__attribute__ ((format (printf, 2, 3))) void hc_asprintf (char **strp, const char *fmt, ...);

void setup_environment_variables (void);
void setup_umask (void);
void setup_seeding (const bool rp_gen_seed_chgd, const u32 rp_gen_seed);

int hc_stat (const char *pathname, hc_stat_t *buf);
int hc_fstat (int fd, hc_stat_t *buf);

void  hc_qsort_r (void *base, size_t nmemb, size_t size, int (*compar) (const void *, const void *, void *), void *arg);
void *hc_bsearch_r (const void *key, const void *base, size_t nmemb, size_t size, int (*compar) (const void *, const void *, void *), void *arg);

bool hc_path_is_file (const char *path);
bool hc_path_is_directory (const char *path);
bool hc_path_is_empty (const char *path);
bool hc_path_exist (const char *path);
bool hc_path_read (const char *path);
bool hc_path_write (const char *path);
bool hc_path_create (const char *path);

bool hc_string_is_digit (const char *s);

void hc_string_trim_trailing (char *s);
void hc_string_trim_leading (char *s);

size_t hc_fread (void *ptr, size_t size, size_t nmemb, FILE *stream);
void   hc_fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream);

hc_time_t  hc_time   (hc_time_t *t);
struct tm *hc_gmtime (const hc_time_t *t, MAYBE_UNUSED struct tm *result);
char      *hc_ctime  (const hc_time_t *t, char *buf, MAYBE_UNUSED const size_t buf_size);

bool hc_same_files (char *file1, char *file2);

#ifdef __cplusplus 
} 
#endif 
#endif // _SHARED_H
