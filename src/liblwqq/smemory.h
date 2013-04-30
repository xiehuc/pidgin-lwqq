/**
 * @file   smemory.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Tue May 22 00:41:48 2012
 *
 * @brief  Small Memory Wrapper
 * 
 * 
 */


#ifndef SMEMORY_H
#define SMEMORY_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int s_vasprintf(char **buf, const char * format, va_list arg);
int s_asprintf(char **buf, const char *format, ...);
#define s_malloc(sz) ((sz==0)?NULL:malloc(sz))
#define s_malloc0(sz) ((sz==0)?NULL:memset(malloc(sz),0,sz))
#define s_calloc(num,sz) calloc(num,sz)
#define s_realloc(ptr,sz) realloc(ptr,sz)
#define s_strdup(s) (s!=NULL?strdup(s):NULL)
#define s_strndup(s,n)  (s!=NULL?strndup(s,n):NULL)
#define s_atol(s,init) (s!=NULL?strtol(s,NULL,10):init)
#define s_atoi(s,init) s_atol(s,init)
#define s_free(p) (p=(p!=NULL)?free(p),NULL:NULL)

#endif  /* SMEMORY_H */
