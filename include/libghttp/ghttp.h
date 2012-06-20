/*
 * ghttp.h -- A public interface to common http functions
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 21-Aug-1998
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

#ifndef GHTTP_H
#define GHTTP_H

#include <ghttp_constants.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _ghttp_request ghttp_request;

typedef enum ghttp_type_tag
{
  ghttp_type_get = 0,
  ghttp_type_options,
  ghttp_type_head,
  ghttp_type_post,
  ghttp_type_put,
  ghttp_type_delete,
  ghttp_type_trace,
  ghttp_type_connect,
  ghttp_type_propfind,
  ghttp_type_proppatch,
  ghttp_type_mkcol,
  ghttp_type_copy,
  ghttp_type_move,
  ghttp_type_lock,
  ghttp_type_unlock
} ghttp_type;

typedef enum ghttp_sync_mode_tag
{
  ghttp_sync = 0,
  ghttp_async
} ghttp_sync_mode;

typedef enum ghttp_status_tag
{
  ghttp_error = -1,
  ghttp_not_done,
  ghttp_done
} ghttp_status;

typedef enum ghttp_proc_tag
{
  ghttp_proc_none = 0,
  ghttp_proc_request,
  ghttp_proc_response_hdrs,
  ghttp_proc_response
} ghttp_proc;

typedef struct ghttp_current_status_tag
{
  ghttp_proc         proc;        /* what's it doing? */
  int                bytes_read;  /* how many bytes have been read? */
  int                bytes_total; /* how many total */
} ghttp_current_status;

/* create a new request object */
ghttp_request *
ghttp_request_new(void);

/* delete a current request object */
void
ghttp_request_destroy(ghttp_request *a_request);

/* Validate a uri
 * This will return -1 if a uri is invalid
 */
int
ghttp_uri_validate(char *a_uri);

/* Set a uri in a request
 * This will return -1 if the uri is invalid
 */

int
ghttp_set_uri(ghttp_request *a_request, char *a_uri);

/* Set a proxy for a request
 * This will return -1 if the uri is invalid
 */

int
ghttp_set_proxy(ghttp_request *a_request, char *a_uri);

/* Set a request type
 * This will return -1 if the request type is invalid or
 * unsupported
 */

int
ghttp_set_type(ghttp_request *a_request, ghttp_type a_type);

/* Set the body.
 * This will return -1 if the request type doesn't support it
 */

int
ghttp_set_body(ghttp_request *a_request, char *a_body, int a_len);

/* Set whether or not you want to use sync or async mode.
 */

int
ghttp_set_sync(ghttp_request *a_request,
	       ghttp_sync_mode a_mode);

/* Prepare a request.
 * Call this before trying to process a request or if you change the
 * uri.
 */

int
ghttp_prepare(ghttp_request *a_request);

/* Set the chunk size
 * You might want to do this to optimize for different connection speeds.
 */

void
ghttp_set_chunksize(ghttp_request *a_request, int a_size);

/* Set a random request header
 */

void
ghttp_set_header(ghttp_request *a_request,
		 const char *a_hdr, const char *a_val);

/* Process a request
 */

ghttp_status
ghttp_process(ghttp_request *a_request);

/* Get the status of a request
 */

ghttp_current_status
ghttp_get_status(ghttp_request *a_request);

/* Flush the received data (so far) into the response body.  This is
 * useful for asynchronous requests with large responses: you can
 * periodically flush the response buffer and parse the data that's
 * arrived so far.
 */

void
ghttp_flush_response_buffer(ghttp_request *a_request);

/* Get the value of a random response header
 */

const char *
ghttp_get_header(ghttp_request *a_request,
		 const char *a_hdr);

/* Get a_name's cookie, caller must free the return string */
char * ghttp_get_cookie(ghttp_request *a_request, const char *a_hdr);

/* Get the list of headers that were returned in the response.  You
   must free the returned string values.  This function will return 0
   on success, -1 on some kind of error. */
int
ghttp_get_header_names(ghttp_request *a_request,
		       char ***a_hdrs, int *a_num_hdrs);

/* Abort a currently running request.  */
int
ghttp_close(ghttp_request *a_request);

/* Clean a request
 */
void
ghttp_clean(ghttp_request *a_request);

/* Get the socket associated with a particular connection
 */

int
ghttp_get_socket(ghttp_request *a_request);

/* get the return entity body
 */

char *
ghttp_get_body(ghttp_request *a_request);

/* get the returned length
 */

int
ghttp_get_body_len(ghttp_request *a_request);

/* Get an error message for a request that has failed.
 */

const char *
ghttp_get_error(ghttp_request *a_request);

/* Parse a date string that is one of the standard
 * date formats
 */

time_t
ghttp_parse_date(char *a_date);

/* Return the status code.
 */

int
ghttp_status_code(ghttp_request *a_request);

/* Return the reason phrase.
 */

const char *
ghttp_reason_phrase(ghttp_request *a_request);

/* Set your username/password pair 
 */

int
ghttp_set_authinfo(ghttp_request *a_request,
		   const char *a_user,
		   const char *a_pass);
		   

 /* Set your username/password pair for proxy
  */
 
int
ghttp_set_proxy_authinfo(ghttp_request *a_request,
			 const char *a_user,
			 const char *a_pass);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* GHTTP_H */
