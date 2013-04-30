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
