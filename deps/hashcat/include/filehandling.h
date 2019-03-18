/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef _FILEHANDLING_H
#define _FILEHANDLING_H
#ifdef __cplusplus 
extern "C" { 
#endif 

#include <stdio.h>
#include <string.h>
#include <errno.h>

u64 count_lines (FILE *fd);

int fgetl (FILE *fp, char *line_buf);

size_t superchop_with_length (char *buf, const size_t len);

int in_superchop (char *buf);

#ifdef __cplusplus 
} 
#endif 
#endif // _FILEHANDLING_H
