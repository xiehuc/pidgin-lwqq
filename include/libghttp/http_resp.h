/*
 * http_resp.h -- routines for reading http responses
 * Created: Christopher Blizzard <blizzard@appliedtheory.com> 9-Aug-1998
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

#ifndef HTTP_RESP_H
#define HTTP_RESP_H

#include "http_hdrs.h"
#include "http_trans.h"
#include "http_req.h"

#define HTTP_RESP_INFORMATIONAL(x) (x >=100 && < 200)
#define HTTP_RESP_SUCCESS(x) (x >= 200 && x < 300)
#define HTTP_RESP_REDIR(x) (x >= 300 && x < 400)
#define HTTP_RESP_CLIENT_ERR(x) (x >= 400 && x < 500)
#define HTTP_RESP_SERVER_ERR(x) (x >= 500 && x < 600)

typedef enum http_resp_header_state_tag
{
  http_resp_header_start = 0,
  http_resp_reading_header
} http_resp_header_state;

typedef enum http_resp_body_state_tag
{
  http_resp_body_start = 0,
  http_resp_body_read_content_length,
  http_resp_body_read_chunked,
  http_resp_body_read_standard
} http_resp_body_state;



typedef struct http_resp_tag
{
  float                                http_ver;
  int                                  status_code;
  char                                *reason_phrase;
  http_hdr_list                       *headers;
  char                                *body;
  int                                  body_len;
  int                                  content_length;
  int                                  flushed_length;
  http_resp_header_state               header_state;
  http_resp_body_state                 body_state;
} http_resp;

http_resp *
http_resp_new(void);

void
http_resp_destroy(http_resp *a_resp);

int
http_resp_read_body(http_resp *a_resp,
		    http_req *a_req,
		    http_trans_conn *a_conn);

int
http_resp_read_headers(http_resp *a_resp, http_trans_conn *a_conn);

void
http_resp_flush(http_resp *a_resp,
                http_trans_conn *a_conn);

#endif /* HTTP_RESP_H */
