/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _RP_KERNEL_ON_CPU_H
#define _RP_KERNEL_ON_CPU_H
#ifdef __cplusplus 
extern "C" { 
#endif 

int apply_rules (const u32 *cmds, u32 *buf, const int in_len);

#ifdef __cplusplus 
} 
#endif 
#endif // _RP_KERNEL_ON_CPU_H
