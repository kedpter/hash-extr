/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _CPT_H
#define _CPT_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <stdio.h>
#include <errno.h>
#include <time.h>

int  cpt_ctx_init    (hashcat_ctx_t *hashcat_ctx);
void cpt_ctx_destroy (hashcat_ctx_t *hashcat_ctx);
void cpt_ctx_reset   (hashcat_ctx_t *hashcat_ctx);

#ifdef __cplusplus 
} 
#endif 
#endif // _CPT_H
