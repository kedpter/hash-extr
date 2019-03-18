/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _RP_CPU_H
#define _RP_CPU_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <string.h>

#define RULE_RC_SYNTAX_ERROR -1
#define RULE_RC_REJECT_ERROR -2

int _old_apply_rule (char *rule, int rule_len, char in[RP_PASSWORD_SIZE], int in_len, char out[RP_PASSWORD_SIZE]);

int run_rule_engine (const int rule_len, const char *rule_buf);

#ifdef __cplusplus 
} 
#endif 
#endif // _RP_CPU_H
