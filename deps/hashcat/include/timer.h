/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _TIMER_H
#define _TIMER_H
#ifdef __cplusplus 
extern "C" { 
#endif 

void   hc_timer_set (hc_timer_t *a);
double hc_timer_get (hc_timer_t a);

#ifdef __cplusplus 
} 
#endif 
#endif // _TIMER_H
