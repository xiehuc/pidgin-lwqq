/*
 * luau (Lib Update/Auto-Update): Simple Update Library
 * Copyright (C) 2003  David Eklund
 *
 * - This library is free software; you can redistribute it and/or             -
 * - modify it under the terms of the GNU Lesser General Public                -
 * - License as published by the Free Software Foundation; either              -
 * - version 2.1 of the License, or (at your option) any later version.        -
 * -                                                                           -
 * - This library is distributed in the hope that it will be useful,           -
 * - but WITHOUT ANY WARRANTY; without even the implied warranty of            -
 * - MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         -
 * - Lesser General Public License for more details.                           -
 * -                                                                           -
 * - You should have received a copy of the GNU Lesser General Public          -
 * - License along with this library; if not, write to the Free Software       -
 * - Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA -
 */

/*
 * md5.h and md5.c are based off of md5hl.c, md5c.c, and md5.h from libmd, which in turn are
 * based off the FreeBSD libmd library.  Their respective copyright notices follow:
 */

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 */

/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: md5.h,v 1.1.1.1 2004/04/02 05:11:38 deklund2 Exp $
 *
 */

#ifndef MD5_H
#define MD5_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>

#define MD5_HASHBYTES 16

typedef struct MD5Context {
    u_int32_t buf[4];
    u_int32_t bits[2];
    unsigned char in[64];
} MD5_CTX;

char* lutil_md5_file(const char *filename, char *buf);
char * lutil_md5_digest(const unsigned char * data, unsigned int len , char *buf);
char* lutil_md5_data(const unsigned char *data, unsigned int len, char *buf);

#endif /* MD5_H */
