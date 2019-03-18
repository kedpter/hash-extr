/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _STDOUT_H
#define _STDOUT_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <search.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>

#if defined (_POSIX)
#include <pthread.h>
#include <pwd.h>
#endif // _POSIX

int process_stdout (hashcat_ctx_t *hashcat_ctx, hc_device_param_t *device_param, const u32 pws_cnt);

#ifdef __cplusplus 
} 
#endif 
#endif // _STDOUT_H
