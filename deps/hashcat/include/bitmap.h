/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _BITMAP_H
#define _BITMAP_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <string.h>

int  bitmap_ctx_init    (hashcat_ctx_t *hashcat_ctx);
void bitmap_ctx_destroy (hashcat_ctx_t *hashcat_ctx);

#ifdef __cplusplus 
} 
#endif 
#endif // _BITMAP_H
