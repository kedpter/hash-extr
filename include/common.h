// to prevent common.h conflict
#ifndef _JM_COMMON_H
#define _JM_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <inttypes.h>

#include "types.h"


#define MAIN_VERSION "1"
#define SUB_VERSION "01"

#define GPU_SPEED_TABLE_JSON_LEN 65536
#define MSG_ENOMEM "Insufficient memory available"

#if defined (__unix__) || defined (__APPLE__)
#define _POSIX
#elif defined (_WIN32)
#define _WIN 1
#else
#error Your Operating System is not supported or detected
#endif

#if defined (__BYTE_ORDER__) && defined (__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error "compiling for big-endian architecture not supported"
#endif
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// needed for *time_r functions under MinGW
#ifndef _POSIX_THREAD_SAFE_FUNCTIONS
#define _POSIX_THREAD_SAFE_FUNCTIONS 200809L
#endif

// needed for 64-bit off_t on 32-bit OSes
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#ifndef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 2
#endif

#define NOMINMAX 1

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define CEIL(a) ((a - (int) (a)) > 0 ? a + 1 : a)
#define CEILDIV(a,b) (((a) + (b) - 1) / (b))

#if defined (__APPLE__)
#define __stdcall
#endif

#if defined (__MSC_VER)
#define HC_API_CALL __cdecl
#elif defined (_WIN32) || defined (__WIN32__)
#define HC_API_CALL __stdcall
#else
#define HC_API_CALL
#endif

#if defined (_WIN)
#define WIN32_LEAN_AND_MEAN
#endif

/* The C++ standard denies redefinition of keywords,
but this is nededed for VS compiler which doesn't have inline keyword but has __inline
*/
#ifndef __cplusplus
#if defined (_MSC_VER)
#define inline __inline
#endif
#endif

#define MAYBE_UNUSED __attribute__((unused))

/*
 * Check if the system uses nanoseconds for file timestamps.
 * In case the system uses nanoseconds we set some custom macros here,
 * e.g. the (nanosecond) access time macros for dictstat
 */

#if defined (__linux__)
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,48)
#define STAT_NANOSECONDS_ACCESS_TIME st_atim.tv_nsec
#endif
#endif

#if defined (__APPLE__)
#define STAT_NANOSECONDS_ACCESS_TIME st_atimespec.tv_nsec
#endif

// config section
// do not try to simply change this, it will not work

#define PW_MIN              0
#define PW_MAX              256
#define PW_MAX_OLD          55

#define SALT_MIN            0
#define SALT_MAX            256
#define SALT_MAX_OLD        51

#define HCBUFSIZ_TINY       0x1000
#define HCBUFSIZ_LARGE      0x50000

#define CPT_CACHE           0x20000
#define PARAMCNT            64
#define DEVICES_MAX         128
#define EXEC_CACHE          128
#define SPEED_CACHE         128
#define SPEED_MAXAGE        4096
#define EXPECTED_ITERATIONS 10000

#if defined (_WIN)
#define EOL "\r\n"
#else
#define EOL "\n"
#endif

// #if defined (_WIN)
// #define HASHCAT_PROGRAM "deps/hashcat/hashcat32.exe"
// #else
// #define HASHCAT_PROGRAM "hashcat"
// #endif

// #if defined (_POSIX)
// #ifndef
// #ifdef __MINGW32__
// #define HAVE_STRSEP
// #endif

int strlist_init(char ***outputs, int num_of_outputs, int output_len);

void strlist_copy(char ***outputs, char **old, int start, int end, int output_len);

void strlist_setempty(char ***outputs, int num_of_outputs, int output_len);

void strlist_free(char ***outputs, int num_of_outputs);

char *trimdquote(char *str);
char *remove_ext (char* mystr, char dot, char sep);

void remove_substring(char *s,const char *toremove);

char * k_strsep(char **sp, const char *sep);

// memory

void *jmcalloc (const size_t nmemb, const size_t sz);

void *jmmalloc (const size_t sz);

void jmfree (void *ptr);

// file

int copy_file (char *old_filename, char *new_filename);

int get_file_len (const char *fpath, int *file_len);

char *jm_base_to_absolute_path(const char *base, const char *relative);

char *get_filename_nosuffix(char *path);

int is_file_exist (char *path);

// linux and windows compatible

FILE *jmpopen(const char *cmd, const char *mode);

FILE *jmpclose(FILE *p);

#ifdef __cplusplus
}
#endif

#endif // JM_COMMON_H
