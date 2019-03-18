/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _STRAIGHT_H
#define _STRAIGHT_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <string.h>

#define INCR_DICTS 1000

int  straight_ctx_update_loop (hashcat_ctx_t *hashcat_ctx);
int  straight_ctx_init        (hashcat_ctx_t *hashcat_ctx);
void straight_ctx_destroy     (hashcat_ctx_t *hashcat_ctx);

#ifdef __cplusplus 
} 
#endif 
#endif // _STRAIGHT_H
