/*
 * http_trans.h -- This file contains definitions for http transport functions
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 5-Aug-1998
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

#ifndef HTTP_TRANS_H
#define HTTP_TRANS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

typedef enum http_trans_err_type_tag {
  http_trans_err_type_host = 0,
  http_trans_err_type_errno
} http_trans_err_type;

typedef struct http_trans_conn_tag {
  struct hostent      *hostinfo;
  struct sockaddr_in   saddr;
  char                *host;
  char                *proxy_host;
  int                  sock;
  short                port;
  short                proxy_port;
  http_trans_err_type  error_type;
  int                  error;
  int                  sync;              /* sync or async? */
  char                *io_buf;            /* buffer */
  int                  io_buf_len;        /* how big is it? */
  int                  io_buf_alloc;      /* how much is used */
  int                  io_buf_io_done;    /* how much have we already moved? */
  int                  io_buf_io_left;    /* how much data do we have left? */
  int                  io_buf_chunksize;  /* how big should the chunks be that get
					    read in and out be? */
  int                  last_read;         /* the size of the last read */
  int                  chunk_len;         /* length of a chunk. */
  char                *errstr;            /* a hint as to an error */
} http_trans_conn;

http_trans_conn *
http_trans_conn_new(void);

void
http_trans_conn_destroy(http_trans_conn *a_conn);

void
http_trans_buf_reset(http_trans_conn *a_conn);

void
http_trans_buf_clip(http_trans_conn *a_conn, char *a_clip_to);

int
http_trans_connect(http_trans_conn *a_conn);

const char *
http_trans_get_host_error(int a_herror);

int
http_trans_append_data_to_buf(http_trans_conn *a_conn,
			      char *a_data,
			      int a_data_len);

int
http_trans_read_into_buf(http_trans_conn *a_conn);

int
http_trans_write_buf(http_trans_conn *a_conn);

char *
http_trans_buf_has_patt(char *a_buf, int a_len,
			char *a_pat, int a_patlen);

#endif /* HTTP_TRANS_H */
