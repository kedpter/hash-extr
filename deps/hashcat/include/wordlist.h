/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _WORDLIST_H
#define _WORDLIST_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <time.h>
#include <inttypes.h>

u32 convert_from_hex (hashcat_ctx_t *hashcat_ctx, char *line_buf, const u32 line_len);

void pw_add (hc_device_param_t *device_param, const u8 *pw_buf, const int pw_len);

void get_next_word_lm  (char *buf, u64 sz, u64 *len, u64 *off);
void get_next_word_uc  (char *buf, u64 sz, u64 *len, u64 *off);
void get_next_word_std (char *buf, u64 sz, u64 *len, u64 *off);

void get_next_word   (hashcat_ctx_t *hashcat_ctx, FILE *fd, char **out_buf, u32 *out_len);
int  load_segment    (hashcat_ctx_t *hashcat_ctx, FILE *fd);
int  count_words     (hashcat_ctx_t *hashcat_ctx, FILE *fd, const char *dictfile, u64 *result);
int  wl_data_init    (hashcat_ctx_t *hashcat_ctx);
void wl_data_destroy (hashcat_ctx_t *hashcat_ctx);

#ifdef __cplusplus 
} 
#endif 
#endif // _WORDLIST_H
