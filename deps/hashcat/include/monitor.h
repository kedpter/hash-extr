/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _MONITOR_H
#define _MONITOR_H
#ifdef __cplusplus 
extern "C" { 
#endif 

int get_runtime_left (const hashcat_ctx_t *hashcat_ctx);

void *thread_monitor (void *p);

#ifdef __cplusplus 
} 
#endif 
#endif // _MONITOR_H
