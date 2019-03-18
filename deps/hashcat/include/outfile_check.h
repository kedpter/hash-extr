/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _OUTFILE_CHECK_H
#define _OUTFILE_CHECK_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <unistd.h>
#include <errno.h>

#define OUTFILES_DIR "outfiles"

void *thread_outfile_remove (void *p);

int  outcheck_ctx_init    (hashcat_ctx_t *hashcat_ctx);
void outcheck_ctx_destroy (hashcat_ctx_t *hashcat_ctx);

#ifdef __cplusplus 
} 
#endif 
#endif // _OUTFILE_CHECK_H
