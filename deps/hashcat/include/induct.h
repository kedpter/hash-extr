/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _INDUCT_H
#define _INDUCT_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static const char INDUCT_DIR[] = "induct";

int  induct_ctx_init    (hashcat_ctx_t *hashcat_ctx);
void induct_ctx_scan    (hashcat_ctx_t *hashcat_ctx);
void induct_ctx_destroy (hashcat_ctx_t *hashcat_ctx);

#ifdef __cplusplus 
} 
#endif 
#endif // _INDUCT_H
