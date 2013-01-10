/**
 * @file   logger.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Sun May 20 23:25:33 2012
 * 
 * @brief  Linux WebQQ Logger API
 * 
 * 
 */

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "logger.h"

static char *levels[] = {
	"DEBUG",
	"NOTICE",
	"WARNING",
	"ERROR",
};

/** 
 * This is standard logger function
 * 
 * @param level Which level of this message, e.g. debug
 * @param file Which file this function called in
 * @param line Which line this function call at
 * @param function Which function call this function 
 * @param msg Log message
 */
void lwqq_log(int level, const char *file, int line,
              const char *function, const char* msg, ...)
{

    char buf[128] = {0};
	va_list  va;
	time_t  t = time(NULL);
	struct tm *tm;
	char date[256];

	tm = localtime(&t);
	strftime(date, sizeof(date), "%b %e %T", tm);
	
    if(level > 1){
        snprintf(buf, sizeof(buf), "[%s] %s[%ld]: %s:%d %s: \n\t", date, levels[level], (long)getpid(), file, line, function);
        fprintf(stderr,"%s",buf);
    }

    //support long long msg printout
	va_start (va, msg);
    vfprintf(stderr,msg,va);
	va_end(va);
}

const char* lwqq_log_time()
{
    static char tm_str[64];
    time_t t_ = time(NULL);
    struct tm *tm_ = localtime(&t_);
    strftime(tm_str,sizeof(tm_str),"%X",tm_);
    return tm_str;
}
