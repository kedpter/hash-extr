/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _HLFMT_H
#define _HLFMT_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <stdio.h>

#define HLFMTS_CNT 11

char *strhlfmt (const u32 hashfile_format);

void hlfmt_hash (hashcat_ctx_t *hashcat_ctx, u32 hashfile_format, char *line_buf, int line_len, char **hashbuf_pos, int *hashbuf_len);
void hlfmt_user (hashcat_ctx_t *hashcat_ctx, u32 hashfile_format, char *line_buf, int line_len, char **userbuf_pos, int *userbuf_len);

u32 hlfmt_detect (hashcat_ctx_t *hashcat_ctx, FILE *fp, u32 max_check);

#ifdef __cplusplus 
} 
#endif 
#endif // _HLFMT_H
