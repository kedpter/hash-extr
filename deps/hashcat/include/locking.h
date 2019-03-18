/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _LOCKING_H
#define _LOCKING_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

int lock_file   (FILE *fp);
int unlock_file (FILE *fp);

#ifdef __cplusplus 
} 
#endif 
#endif // _LOCKING_H
