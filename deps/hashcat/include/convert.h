/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _CONVERT_H
#define _CONVERT_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <ctype.h>

bool is_hexify (const u8 *buf, const int len);
int exec_unhexify (const u8 *in_buf, const int in_len, u8 *out_buf, const int out_sz);

bool need_hexify (const u8 *buf, const int len, const char separator, bool always_ascii);
void exec_hexify (const u8 *buf, const int len, u8 *out);

bool is_valid_hex_string (const u8 *s, const int len);
bool is_valid_hex_char   (const u8 c);

u8 hex_convert (const u8 c);

u8  hex_to_u8  (const u8 hex[2]);
u32 hex_to_u32 (const u8 hex[8]);
u64 hex_to_u64 (const u8 hex[16]);

void u8_to_hex_lower  (const u8  v, u8 hex[2]);
void u32_to_hex_lower (const u32 v, u8 hex[8]);
void u64_to_hex_lower (const u64 v, u8 hex[16]);

u8 int_to_base32  (const u8 c);
u8 base32_to_int  (const u8 c);
u8 int_to_base64  (const u8 c);
u8 base64_to_int  (const u8 c);

u8 int_to_itoa32  (const u8 c);
u8 itoa32_to_int  (const u8 c);
u8 int_to_itoa64  (const u8 c);
u8 itoa64_to_int  (const u8 c);

u8 int_to_bf64    (const u8 c);
u8 bf64_to_int    (const u8 c);

u8 int_to_lotus64 (const u8 c);
u8 lotus64_to_int (const u8 c);

int base32_decode (u8 (*f) (const u8), const u8 *in_buf, int in_len, u8 *out_buf);
int base32_encode (u8 (*f) (const u8), const u8 *in_buf, int in_len, u8 *out_buf);

int base64_decode (u8 (*f) (const u8), const u8 *in_buf, int in_len, u8 *out_buf);
int base64_encode (u8 (*f) (const u8), const u8 *in_buf, int in_len, u8 *out_buf);

void lowercase (u8 *buf, int len);
void uppercase (u8 *buf, int len);

#ifdef __cplusplus 
} 
#endif 
#endif // _CONVERT_H
