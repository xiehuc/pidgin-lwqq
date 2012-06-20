/*
 * http_uri.h --- Contains routines for working with uri's
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 9-Jul-98
 *
 * Copyright (C) 1998 Free Software Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef HTTP_URI_H
#define HTTP_URI_H

/* strings that are used all over the place */

typedef struct http_uri_tag
{
  char             *full;                          /* full URL */
  char             *proto;                         /* protocol */
  char             *host;                          /* copy semantics */
  unsigned short    port;
  char             *resource;                      /* copy semantics */
} http_uri;

http_uri *
http_uri_new(void);
   
void
http_uri_destroy(http_uri *a_uri);

int
http_uri_parse(char *a_uri,
	       http_uri *a_request);

#endif



