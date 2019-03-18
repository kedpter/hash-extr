/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _COMBINATOR_H
#define _COMBINATOR_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <stdio.h>
#include <errno.h>

int  combinator_ctx_init    (hashcat_ctx_t *hashcat_ctx);
void combinator_ctx_destroy (hashcat_ctx_t *hashcat_ctx);

#ifdef __cplusplus 
} 
#endif 
#endif // _COMBINATOR_H
