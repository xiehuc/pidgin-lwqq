/**
 * @file   smemory.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Tue May 22 00:41:24 2012
 * 
 * @brief  Small Memory Wrapper
 * 
 * 
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

void *s_malloc(size_t size)
{
	if(size == 0)
		return NULL;

    return malloc(size);
}

void *s_malloc0(size_t size)
{
    if (size == 0)
        return NULL;
    
    void *ptr = malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    
    return ptr;
}

void *s_calloc(size_t nmemb, size_t lsize)
{
    return calloc(nmemb, lsize);
}

void *s_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

char *s_strdup(const char *s1)
{
    if (!s1)
        return NULL;

    return strdup(s1);
}

char *s_strndup(const char *s1, size_t n)
{
    if (!s1)
        return NULL;

    return strndup(s1, n);
}

int s_vasprintf(char **buf, const char * format,
				 va_list arg)
{
    return vasprintf(buf, format, arg);
}

int s_asprintf(char **buf, const char *format, ...)
{
    va_list arg;
	int rv;

	va_start(arg, format);
	rv = s_vasprintf(buf, format, arg);
	va_end(arg);

    return rv;
}
int s_atoi(const char* s)
{
    if(s) return atoi(s);
    printf("atoi failed:%s\n",s);
    return 0;
}
int s_atol(const char* s)
{
    if(s) return atol(s);
    printf("atoi failed:%s\n",s);
    return 0;
}
