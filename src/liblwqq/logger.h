/**
 * @file   logger.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Sun May 20 23:27:05 2012
 * 
 * @brief  Linux WebQQ Logger API
 * 
 * 
 */

#ifndef LWQQ_LOGGER_H
#define LWQQ_LOGGER_H
#include <string.h>

#define _FILE_NAME_ (strrchr(__FILE__,'/')?strrchr(__FILE__,'/')+1:__FILE__)

#define _A_ _FILE_NAME_, __LINE__, __PRETTY_FUNCTION__
#define _LOG_DEBUG		0
#define LOG_DEBUG		_LOG_DEBUG, _A_

#define __LOG_NOTICE	1
#define LOG_NOTICE		__LOG_NOTICE, _A_

#define __LOG_WARNING	2
#define LOG_WARNING		__LOG_WARNING, _A_

#define __LOG_ERROR		3
#define LOG_ERROR		__LOG_ERROR, _A_

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
              const char *function, const char* msg, ...);
const char* lwqq_log_time();
#define TIME_ lwqq_log_time()
/**============VERBOSE LEVEL=============**/
/* 0        No Verbose
 * 1        Normal Verbose
 * 2        Poll Verbose
 * 3        Request Verbose
 * 4        Timeout Verbose
 * 5        Extra Verbose
 */
//#define lwqq_verbose(l,str,...) if(l<=LWQQ_VERBOSE_LEVEL) fprintf(stderr,str,__VA_ARGS__)
void lwqq_verbose(int l,const char* str,...);
#define lwqq_puts(str) lwqq_verbose(1,"%s\n",str)

void lwqq_log_set_level(int level);
int lwqq_log_get_level();
#define LWQQ_VERBOSE_LEVEL lwqq_log_get_level()


#endif  /* LWQQ_LOGGER_H */
