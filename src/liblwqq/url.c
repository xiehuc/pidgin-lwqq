/**
 * @file   test.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Thu May 24 22:56:52 2012
 * 
 * @brief  The Encode and Decode helper is based on
 * code where i download from http://www.geekhideout.com/urlcode.shtml
 * 
 * 
 */

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <iconv.h>

#include "smemory.h"

/* Converts a hex character to its integer value */
static char from_hex(char ch)
{
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'A' + 10;
}

/* Converts an integer value to its hex character*/
static char to_hex(char code)
{
  static char hex[] = "0123456789ABCDEF";
  return hex[code & 15];
}

/** 
 * NB: be sure to free() the returned string after use
 * 
 * @param str 
 * 
 * @return A url-encoded version of str
 */
char *url_encode(char *str)
{
    if (!str)
        return NULL;
    
    char *pstr = str, *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~' ) 
            *pbuf++ = *pstr;
        else 
            *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}
char *url_whole_encode(char *str)
{
    if (!str)
        return NULL;
    
    char *pstr = str, *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~' ||*pstr == ':' ||*pstr == '/'
                || *pstr == '&'|| *pstr == '=' || *pstr == '?') 
            *pbuf++ = *pstr;
        else 
            *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

/** 
 * NB: be sure to free() the returned string after use
 * 
 * @param str 
 * 
 * @return A url-decoded version of str
 */
char *url_decode(char *str)
{
    if (!str) {
        return NULL;
    }
    char *pstr = str, *buf = malloc(strlen(str) + 1), *pbuf = buf;
    while (*pstr) {
        if (*pstr == '%') {
            if (pstr[1] && pstr[2]) {
                *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
                pstr += 2;
            }
        } else if (*pstr == '+') { 
            *pbuf++ = ' ';
        } else {
            *pbuf++ = *pstr;
        }
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}
/*
char* to_gbk(char* utf8)
{
    char buf[512];
    iconv_t cd = iconv_open("gbk","utf-8");

    char* inbuf = utf8;
    size_t inlen = strlen(utf8);
    char* outbuf = buf;
    size_t outlen = sizeof(buf);
    iconv(cd,&inbuf,&inlen,&outbuf,&outlen);
    iconv_close(cd);
    if(inlen!=0) return NULL;
    *outbuf = '\0';
    return s_strdup(buf);
}

*/
