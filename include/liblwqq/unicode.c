/**
 * @file   unicode.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Mon Jun 18 23:03:31 2012
 * 
 * @brief  ucs to utf-8, this file is written by kernelhcy in gtkqq
 * i rewrite to undependent glib
 * 
 * 
 */

#include <stdio.h>
#include <string.h>
#include "smemory.h"

static unsigned int hex2char(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
        
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
        
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    
    return 0;       
}

/** 
 * decode UCS-4 to utf8
 *      UCS-4 code              UTF-8
 * U+00000000–U+0000007F 0xxxxxxx
 * U+00000080–U+000007FF 110xxxxx 10xxxxxx
 * U+00000800–U+0000FFFF 1110xxxx 10xxxxxx 10xxxxxx
 * U+00010000–U+001FFFFF 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * U+00200000–U+03FFFFFF 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * U+04000000–U+7FFFFFFF 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * 
 * Chinese are all in  U+00000800–U+0000FFFF.
 * So only decode to the second and third format.
 * 
 * @param to 
 * @param from 
 */
static char *do_ucs4toutf8(const char *from)
{
    char str[5] = {0};
    int i;
    
    static unsigned int E = 0xe0;
    static unsigned int T = 0x02;
    static unsigned int sep1 = 0x80;
    static unsigned int sep2 = 0x800;
    static unsigned int sep3 = 0x10000;
    static unsigned int sep4 = 0x200000;
    static unsigned int sep5 = 0x4000000;
        
    unsigned int tmp[4];
    char re[6];
    unsigned int ivalue = 0;
        
    for (i = 0; i < 4; ++i) {
        tmp[i] = 0xf & hex2char(from[i + 2]);
        ivalue *= 16;
        ivalue += tmp[i];
    }
    
    /* decode */
    if (ivalue < sep1) {
        /* 0xxxxxxx */
        re[0] = 0x7f & tmp[3];
        str[0] = re[0];
    } else if (ivalue < sep2) {
        /* 110xxxxx 10xxxxxx */
        re[0] = (0x3 << 6) | ((tmp[1] & 7) << 2) | (tmp[2] >> 2);
        re[1] = (0x1 << 7) | ((tmp[2] & 3) << 4) | tmp[3];
        str[0] = re[0];
        str[1] = re[1];
    } else if (ivalue < sep3) {
        /* 1110xxxx 10xxxxxx 10xxxxxx */
        re[0] = E | (tmp[0] & 0xf);
        re[1] = (T << 6) | (tmp[1] << 2) | ((tmp[2] >> 2) & 0x3);
        re[2] = (T << 6) | (tmp[2] & 0x3) << 4 | tmp[3];
        /* copy to @to. */
        for (i = 0; i < 3; ++i){
            str[i] = re[i];
        }
    } else if (ivalue < sep4) {
        /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        re[0] = 0xf0 | ((tmp[1] >> 2) & 0x7);
        re[1] = 0x2 << 6 | ((tmp[1] & 0x3) << 4) | ((tmp[2] >> 4) & 0xf);
        re[2] = 0x2 << 6 | ((tmp[2] & 0xf) << 4) | ((tmp[3] >> 6) & 0x3);
        re[3] = 0x2 << 6 | (tmp[2] & 0x3f);
        /* copy to @to. */
        for (i = 0; i < 4; ++i) {
            str[i] = re[i];
        }
    } else if (ivalue < sep5) {
        /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        return NULL;
    } else {
        /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        return NULL;
    }
    
    return s_strdup(str);
}

char *ucs4toutf8(const char *from)
{
    if (!from) {
        return NULL;
    }
    
    char *out = NULL;
    int outlen = 0;
    const char *c;
    
    for (c = from; *c != '\0'; ++c) {
        char *s;
        if (*c == '\\' && *(c + 1) == 'u') {
            s = do_ucs4toutf8(c);
            out = s_realloc(out, outlen + strlen(s) + 1);
            snprintf(out + outlen, strlen(s) + 1, "%s", s);
            outlen = strlen(out);
            s_free(s);
            c += 5;
        } else {
            out = s_realloc(out, outlen + 2);
            out[outlen] = *c;
            out[outlen + 1] = '\0';
            outlen++;
            continue;
        }
    }

    return out;
}
