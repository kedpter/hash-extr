/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _DISPATCH_H
#define _DISPATCH_H
#ifdef __cplusplus 
extern "C" { 
#endif 

void *thread_calc_stdin (void *p);
void *thread_calc (void *p);

#ifdef __cplusplus 
} 
#endif 
#endif // _DISPATCH_H
