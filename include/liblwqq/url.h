/**
 * @file   url.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Thu May 24 23:01:23 2012
 *
 * @brief  The Encode and Decode helper is based on
 * code where i download from http://www.geekhideout.com/urlcode.shtml
 * 
 * 
 */

#ifndef LWQQ_URL_H
#define LWQQ_URL_H

/** 
 * NB: be sure to free() the returned string after use
 * 
 * @param str 
 * 
 * @return A url-encoded version of str
 */
char *url_encode(char *str);

/** 
 * NB: be sure to free() the returned string after use
 * 
 * @param str 
 * 
 * @return A url-decoded version of str
 */
char *url_decode(char *str);

#endif  /* LWQQ_URL_H */
