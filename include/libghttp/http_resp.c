/*
 * http_resp.c -- routines for reading http_responses
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "http_resp.h"
#include "http_global.h"

typedef enum header_state_tag
{
  reading_header = 0,
  reading_value,
  reading_sep,
  reading_eol
} header_state;

static int
string_is_number(char *a_string);

static int
read_chunk(http_trans_conn *a_conn);

static int
read_body_chunked(http_resp *a_resp,
		  http_req *a_req,
		  http_trans_conn *a_conn);

static int
read_body_content_length(http_resp *a_resp,
			 http_req *a_req,
			 http_trans_conn *a_conn);

static int
read_body_standard(http_resp *a_resp,
		   http_req *a_req,
		   http_trans_conn *a_conn);

http_resp *
http_resp_new(void)
{
  http_resp *l_return = NULL;

  l_return = (http_resp *)malloc(sizeof(http_resp));
  memset(l_return, 0, sizeof(http_resp));
  l_return->headers = http_hdr_list_new();
  return l_return;
}

void
http_resp_destroy(http_resp *a_resp)
{
  if (!a_resp)
    return;
  if (a_resp->reason_phrase)
    free(a_resp->reason_phrase);
  if (a_resp->headers)
    http_hdr_list_destroy(a_resp->headers);
  if (a_resp->body)
    free(a_resp->body);
  free(a_resp);
  return;
}

int
http_resp_read_headers(http_resp *a_resp, http_trans_conn *a_conn)
{
  char          *l_start_body = NULL;
  int            l_rv = 0;
  int            l_done = 0;
  int            l_return = HTTP_TRANS_DONE;
  char          *l_start_header = NULL;
  int            l_header_len = 0;
  char          *l_last_header = NULL;
  int            l_last_header_len = 0;
  char          *l_start_value = NULL;
  int            l_value_len = 0;
  char          *l_cur_ptr = NULL;
  char          *l_ptr = NULL;
  header_state   l_state = reading_header;

  /* check to see if we need to jump in somewhere */
  if (a_conn->sync == HTTP_TRANS_ASYNC)
    {
      if (a_resp->header_state == http_resp_reading_header)
	goto http_resp_reading_header_jump;
    }
  /* start reading headers */
  do
    {
      a_resp->header_state = http_resp_reading_header;
    http_resp_reading_header_jump:
      /* read in the buffer */
      l_rv = http_trans_read_into_buf(a_conn);
      /* check for an error */
      if (l_rv == HTTP_TRANS_ERR)
	{
	  a_conn->errstr = "Failed to read http response line";
	  l_return = HTTP_TRANS_ERR;
	  goto ec;
	}
      /* check to see if the end of headers string is in the buffer */
      l_start_body = http_trans_buf_has_patt(a_conn->io_buf,
					     a_conn->io_buf_alloc,
					     "\r\n\r\n", 4);
      if (l_start_body != NULL)
	l_done = 1;
      if ((l_done == 0) && (a_conn->sync == HTTP_TRANS_ASYNC) && (l_rv == HTTP_TRANS_NOT_DONE))
	return HTTP_TRANS_NOT_DONE;
      /* yes, that !l_done is ther because in the case of a 100
         continue we well get back up to this loop without doing a
         successful read. */
      if ((!l_done) && (l_rv == HTTP_TRANS_DONE) && (a_conn->last_read == 0))
	{
	  a_conn->errstr = "Short read while reading http response headers";
	  return HTTP_TRANS_ERR;
	}
    } while (l_done == 0);
  /* parse out the response header */
  /* check to make sure that there's enough that came back */
  if ((a_conn->io_buf_alloc) < 14)
    {
      a_conn->errstr = "The http response line was too short.";
      l_return = HTTP_TRANS_ERR;
      goto ec;
    }
  l_ptr = a_conn->io_buf;
  /* can you say PARANOID?  I thought you could. */
  if (strncmp(l_ptr, "HTTP", 4) != 0)
    {
      a_conn->errstr = "The http response line did not begin with \"HTTP\"";
      l_return = HTTP_TRANS_ERR;
      goto ec;
    }
  if ((isdigit(l_ptr[5]) == 0) || /* http ver */
      (l_ptr[6] != '.') ||
      (isdigit(l_ptr[7]) == 0) ||
      (l_ptr[8] != ' ') ||        /* space */
      (isdigit(l_ptr[9]) == 0) || /* code */
      (isdigit(l_ptr[10]) == 0) ||
      (isdigit(l_ptr[11]) == 0) ||
      (l_ptr[12] != ' '))          /* space */
    {
      a_conn->errstr = "Error parsing http response line";
      l_return = HTTP_TRANS_ERR;
      goto ec;
    }
  /* convert char into int */
  a_resp->http_ver = (l_ptr[5] - 0x30);
  a_resp->http_ver += ((l_ptr[7] - 0x30) / 10.0);
  /* convert the response into an int */
  a_resp->status_code = ((l_ptr[9] - 0x30)*100);
  a_resp->status_code += ((l_ptr[10] - 0x30)*10);
  a_resp->status_code += (l_ptr[11] - 0x30);
  /* get the length of the reason_phrase */
  l_cur_ptr = &l_ptr[13];
  /* you can't overrun this because you already know that
     there has to be a '\r' above from searching from the
     end of the headers */
  while (*l_cur_ptr != '\r')
    l_cur_ptr++;
  /* make sure to free the buffer if it's already allocated */
  if (a_resp->reason_phrase) {
    free(a_resp->reason_phrase);
    a_resp->reason_phrase = NULL;
  }
  /* allocate space for the reason phrase */
  a_resp->reason_phrase =
    malloc((l_cur_ptr - (&l_ptr[13])) + 1);
  memset(a_resp->reason_phrase, 0,
	 ((l_cur_ptr - (&l_ptr[13])) + 1));
  memcpy(a_resp->reason_phrase,
	 &l_ptr[13], (l_cur_ptr - (&l_ptr[13])));
  /* see if there are any headers.  If there aren't any headers
     then the end of the reason phrase is the same as the start body
     as above.  If that's the case then skip reading any headers. */
  if (l_cur_ptr == l_start_body)
    l_done = 1;
  else
    l_done = 0;
  /* make sure that it's not a continue. */
  if (a_resp->status_code == 100)
    {
      /* look for the next \r\n\r\n and cut it off */
      char *l_end_continue = http_trans_buf_has_patt(a_conn->io_buf,
						     a_conn->io_buf_alloc,
						     "\r\n\r\n", 4);
      if (!l_end_continue)
	return HTTP_TRANS_ERR;
      http_trans_buf_clip(a_conn, l_end_continue + 4);
      l_start_body = NULL;
      a_resp->status_code = 0;
      l_done = 0;
      if (a_conn->sync == HTTP_TRANS_ASYNC)
	return HTTP_TRANS_NOT_DONE;
      else
	goto http_resp_reading_header_jump;
    }
  /* set the start past the end of the reason phrase,
     checking if there's a CRLF after it. */
  while ((*l_cur_ptr == '\r') ||
	 (*l_cur_ptr == '\n'))
    l_cur_ptr++;

  /* start parsing out the headers */
  /* start at the beginning */
  l_start_header = l_cur_ptr;
  while (l_done == 0)
    {
      /* check to see if we're at the end of the
	 headers as determined above by the _patt() call */
      if (l_cur_ptr == (l_start_body + 1))
	break;
      /* reading the header name */
      if (l_state == reading_header)
	{
	  /* check to see if there's leading whitespace.
	     If that's the case then it needs to be combined
	     from the previous header */
	  if (l_header_len == 0)
	    {
	      if ((*l_cur_ptr == ' ') || (*l_cur_ptr == '\t'))
		{
		  /* bomb if it's the first header.  That's not valid */
		  if ((l_last_header == NULL) || (l_last_header_len == 0))
		    {
		      a_conn->errstr = "The first http response header began with whitespace";
		      l_return = HTTP_TRANS_ERR;
		      goto ec;
		    }
		  l_cur_ptr++;
		  /* set it reading sep.  sep will eat all of the write space */
		  l_state = reading_sep;
		  continue;
		}
	    }
	  if (*l_cur_ptr == ':')
	    {
	      /* make sure that there's some header there */
	      if (l_header_len == 0)
		{
		  a_conn->errstr = "An http response header was zero length";
		  l_return = HTTP_TRANS_ERR;
		  goto ec;
		}
	      /* start reading the seperator */
	      l_state = reading_sep;
	      l_cur_ptr++;
	    }
	  /* make sure there's a seperator in
	     there somewhere */
	  else if (*l_cur_ptr == '\r')
	    {
	      a_conn->errstr = "Failed to find seperator in http response headers";
	      l_return = HTTP_TRANS_ERR;
	      goto ec;
	    }
	  else
	    {
	      l_cur_ptr++;
	      l_header_len++;
	    }
	}
      /* read the seperator */
      else if(l_state == reading_sep)
	{
	  /* walk until you get a non-whitespace character */
	  if ((*l_cur_ptr == ' ') || (*l_cur_ptr == '\t'))
	    l_cur_ptr++;
	  else
	    {
	      l_state = reading_value;
	      l_start_value = l_cur_ptr;
	      l_value_len = 0;
	    }
	}
      /* read the value */
      else if(l_state == reading_value)
	{
	  /* check to see if we've reached the end of the
	     value */
	  if ((*l_cur_ptr == '\r') || (*l_cur_ptr == '\n'))
	    {
	      /* check to see if this is a continuation of the last
		 header. If the header len is 0 and we've gotten to
		 this point then that's the case */
	      if (l_header_len == 0)
		{
		  http_hdr_set_value_no_nts(a_resp->headers,
					    l_last_header,
					    l_last_header_len,
					    l_start_value,
					    l_value_len);
		}
	      else
		{
		  http_hdr_set_value_no_nts(a_resp->headers,
					    l_start_header,
					    l_header_len,
					    l_start_value,
					    l_value_len);
		  
		  /* set the last header and the length so that a new line
		     that starts with spaces can figure out what header it
		     applies to */
		  l_last_header = l_start_header;
		  l_last_header_len = l_header_len;
		}
	      /* start eating the end of line */
	      l_state = reading_eol;
	    }
	  else
	    {
	      l_cur_ptr++;
	      l_value_len++;
	    }
	}
      /* read the eof */
      else if(l_state == reading_eol)
	{
	  /* eat the eol */
	  if ((*l_cur_ptr == '\r') || (*l_cur_ptr == '\n'))
	    l_cur_ptr++;
 	  else
	    {
	      /* start reading a new header again. */
	      l_state = reading_header;
	      l_start_header = l_cur_ptr;
	      l_header_len = 0;
	    }
	}
      /* what state is this? */
      else
	{
	  a_conn->errstr = "Unknown state while reading http response headers";
	  l_return = HTTP_TRANS_ERR;
	  goto ec;
	}
    }
  /* clip the buffer */
  http_trans_buf_clip(a_conn, l_start_body + 4);
 ec:
  a_resp->header_state = http_resp_header_start;
  return l_return;
}

int
http_resp_read_body(http_resp *a_resp,
		    http_req *a_req,
		    http_trans_conn *a_conn)
{
  int      l_return = 0;
  char    *l_content_length = NULL;
  char    *l_transfer_encoding = NULL;
  char    *l_connection = NULL;

  /* check to see if we have to jump in anywhere. */
  if (a_conn->sync == HTTP_TRANS_ASYNC)
    {
      if (a_resp->body_state == http_resp_body_read_content_length)
	goto http_resp_body_read_content_length_jump;
      if (a_resp->body_state == http_resp_body_read_chunked)
	goto http_resp_body_read_chunked_jump;
      if (a_resp->body_state == http_resp_body_read_standard)
	goto http_resp_body_read_standard_jump;
    }
  /* check to make sure that things are ok. */
  if ((!a_resp) || (!a_conn))
    return -1;
  /* check to see if there should be an entity body. */
  /* check to see if there's a content length */
  l_content_length =
    http_hdr_get_value(a_resp->headers,
		       http_hdr_Content_Length);
  /* check to see if there's a transfer encoding */
  l_transfer_encoding =
    http_hdr_get_value(a_resp->headers,
		       http_hdr_Transfer_Encoding);
  /* check to see if the connection header is set */
  l_connection = 
    http_hdr_get_value(a_resp->headers,
		       http_hdr_Connection);
  /* if there's a content length, do your stuff */
  if (l_content_length && (a_req->type != http_req_type_head))
    {
      if (string_is_number(l_content_length) == 0)
	{
	  a_conn->errstr = "Content length in http response was not a number";
	  return -1;
	}
      a_resp->content_length = atoi(l_content_length);
      /* set the state */
      a_resp->body_state = http_resp_body_read_content_length;
    http_resp_body_read_content_length_jump:
      l_return = read_body_content_length(a_resp, a_req, a_conn);
    }
  else if (l_content_length)
    {
      /* this happens in a head request with content length. */
      return HTTP_TRANS_DONE;
    }
  else if (l_transfer_encoding)
    {
      /* check to see if it's using chunked transfer encoding */
      if (!strcasecmp(l_transfer_encoding, "chunked"))
	{
	  /* set the state */
	  a_resp->body_state = http_resp_body_read_chunked;
	http_resp_body_read_chunked_jump:
	  l_return = read_body_chunked(a_resp, a_req, a_conn);
	}
      else
	{
	  /* what kind of encoding? */
	  a_conn->errstr = "Unknown encoding type in http response";
	  return -1;
	}
    }
  else
    {
      a_resp->body_state = http_resp_body_read_standard;
      /* set the state */
    http_resp_body_read_standard_jump:
      l_return = read_body_standard(a_resp, a_req, a_conn);
      /* after that the connection gets closed */
      if (l_return == HTTP_TRANS_DONE)
	{
	  close(a_conn->sock);
	  a_conn->sock = -1;
	}
    }
  /* check to see if the connection should be closed */
  if (l_connection && (l_return != HTTP_TRANS_NOT_DONE))
    {
      if (!strcasecmp(l_connection, "close"))
	{
	  close (a_conn->sock);
	  a_conn->sock = -1;
	}
    }
  if (l_return == HTTP_TRANS_DONE)
    a_resp->body_state = http_resp_body_start;
  return l_return;
}

static int
string_is_number(char *a_string)
{
  int i = 0;
  
  if (strlen(a_string) < 1)
    return 0;
  while (a_string[i])
    {
      if (isdigit(a_string[i]) == 0)
	return 0;
      i++;
    }
  return 1;
}

static int
read_chunk(http_trans_conn *a_conn)
{
  char *l_end_chunk_hdr = NULL;
  int   l_len = 0;
  int   i = 0;
  int   j = 0;
  char *l_ptr = NULL;
  int   l_left_to_read = 0;
  int   l_rv = 0;

  if (a_conn->chunk_len == 0)
    {
      /* check to make sure that the pattern is in the
	 buffer */
      do {
	if ((l_end_chunk_hdr =
	     http_trans_buf_has_patt(a_conn->io_buf, a_conn->io_buf_alloc,
				     "\r\n", 2)) == NULL)
	  {
	    l_rv = http_trans_read_into_buf(a_conn);
	    if (l_rv == HTTP_TRANS_ERR)
	      return HTTP_TRANS_ERR;
	    /* check to see if the remote end hung up. */
	    if ((l_rv == HTTP_TRANS_DONE) && (a_conn->last_read == 0))
	      return HTTP_TRANS_ERR;
	    if ((a_conn->sync == HTTP_TRANS_ASYNC) && (l_rv == HTTP_TRANS_NOT_DONE))
	      return HTTP_TRANS_NOT_DONE;
	  }
      } while (l_end_chunk_hdr == NULL);
      /* set the begining at the start of the buffer */
      l_ptr = a_conn->io_buf;
      /* eat the hex value of the chunk */
      while ((l_ptr < l_end_chunk_hdr) &&
	     (((tolower(*l_ptr) <= 'f') && (tolower(*l_ptr) >= 'a')) ||
	      ((*l_ptr <= '9') && (*l_ptr >= '0'))))
	l_ptr++;
      /* get the length of the hex number */
      l_len = l_ptr - a_conn->io_buf;
      if (l_len == 0)
	{
	  a_conn->chunk_len = -1;
	  return HTTP_TRANS_ERR;
	}
      /* walk the size adding the values as you go along. */
      for (i=0, j=l_len-1; i < l_len; i++, j--)
	{
	  if ((tolower(a_conn->io_buf[i]) <= 'f') && (tolower(a_conn->io_buf[i]) >= 'a'))
	    a_conn->chunk_len += (tolower(a_conn->io_buf[i]) - 0x57) << (4*j);
	  else
	    a_conn->chunk_len += (tolower(a_conn->io_buf[i]) - 0x30) << (4*j);
	}
      /* reset the pointer past the end of the header */
      http_trans_buf_clip(a_conn, l_end_chunk_hdr + 2);
    }
  /* check to see if it's the last chunk.  If not then add it.*/
  if (a_conn->chunk_len != 0)
    {
      /* figure out how much we need to read */
      /* the + 2 is for the \r\n that always follows a chunk. */
      l_left_to_read = a_conn->chunk_len - a_conn->io_buf_alloc + 2;
      /* check to make sure that we actually need to read anything in. */
      if (l_left_to_read > 0)
	{
	  /* set the vars in the struct */
	  a_conn->io_buf_io_left = l_left_to_read;
	  a_conn->io_buf_io_done = 0;
	  /* append it */
	  do {
	    l_rv = http_trans_read_into_buf(a_conn);
	    if (l_rv == HTTP_TRANS_ERR)
	      return HTTP_TRANS_ERR;
	    /* check and see if the server hung up. */
	    if ((l_rv == HTTP_TRANS_DONE) && (a_conn->last_read == 0))
	      return HTTP_TRANS_ERR;
	    if ((a_conn->sync == HTTP_TRANS_ASYNC) && (l_rv == HTTP_TRANS_NOT_DONE))
	      return HTTP_TRANS_NOT_DONE;
	  } while (l_rv == HTTP_TRANS_NOT_DONE);
	}
    }
  if (a_conn->io_buf_alloc >= a_conn->chunk_len + 2)
    return HTTP_TRANS_DONE;
  /* we only get here if there was an error. */
  if (a_conn->chunk_len == 0)
    return HTTP_TRANS_DONE;
  return HTTP_TRANS_ERR;
}

static int
read_body_chunked(http_resp *a_resp,
		  http_req *a_req,
		  http_trans_conn *a_conn)
{
  int   l_rv = 0;
  int   l_done = 0;
  
  do
    {
      /* read a chunk */
      l_rv = read_chunk(a_conn);
      if (l_rv == HTTP_TRANS_ERR)
	return HTTP_TRANS_ERR;
      if ((a_conn->sync == HTTP_TRANS_ASYNC) && (l_rv == HTTP_TRANS_NOT_DONE))
	return HTTP_TRANS_NOT_DONE;
      /* see if it's the first time */
      if (a_conn->chunk_len > 0)
	{
	  if (a_resp->body == NULL)
	    {
	      a_resp->body = malloc(a_conn->chunk_len);
	      memcpy(a_resp->body, a_conn->io_buf, a_conn->chunk_len);
	      a_resp->body_len = a_conn->chunk_len;
	    }
	  /* append it to the body */
	  else
	    {
	      a_resp->body = realloc(a_resp->body,
				     (a_resp->body_len + a_conn->chunk_len));
	      memcpy(&a_resp->body[a_resp->body_len], a_conn->io_buf, a_conn->chunk_len);
	      a_resp->body_len += a_conn->chunk_len;
	    }
	}
      /* make sure there's at least 2 bytes in the buffer.
	 This happens when a read was 3 bytes as in 0\r\n
	 and there is still 2 bytes ( \r\n ) in the read queue. */
      if ((a_conn->chunk_len == 0) && (a_conn->io_buf_alloc < 2))
	{
	  a_conn->io_buf_io_left = ( 2 - a_conn->io_buf_alloc );
	  a_conn->io_buf_io_done = 0;
	  do {
	    l_rv = http_trans_read_into_buf(a_conn);
	  } while (l_rv == HTTP_TRANS_NOT_DONE);
	  /* check for an error */
	  if (l_rv == HTTP_TRANS_ERR)
	    return HTTP_TRANS_ERR;
	}
      if (a_conn->chunk_len == 0)
	l_done = 1;
      else
	{
	  /* clip the buffer */
	  http_trans_buf_clip(a_conn, &a_conn->io_buf[a_conn->chunk_len + 2]);
	}
      a_conn->chunk_len = 0;
    } while (l_done == 0);
  return HTTP_TRANS_DONE;
}

static void
flush_response_body(http_resp *a_resp,
                    http_trans_conn *a_conn)
{
  if (a_resp->body != NULL) {
    free(a_resp->body);
  }
  a_resp->flushed_length += a_resp->body_len;
  a_resp->body_len = a_conn->io_buf_alloc;
  a_resp->body = malloc(a_conn->io_buf_alloc + 1);
  memset(a_resp->body, 0, a_conn->io_buf_alloc + 1);
  memcpy(a_resp->body, a_conn->io_buf, a_conn->io_buf_alloc);
  /* clean the buffer */
  http_trans_buf_reset(a_conn);
}

void
http_resp_flush(http_resp *a_resp,
                http_trans_conn *a_conn)
{
  flush_response_body(a_resp, a_conn);
}

static int
read_body_content_length(http_resp *a_resp,
			 http_req *a_req,
			 http_trans_conn *a_conn)
{
  int l_len = 0;
  int l_left_to_read = 0;
  int l_rv = 0;

  l_len = a_resp->content_length;
  if (l_len == 0)
    return HTTP_TRANS_DONE;

  /* find out how much more we have to read */
  l_left_to_read = l_len - a_conn->io_buf_alloc - a_resp->flushed_length - a_resp->body_len;
  /* set the variables */
  a_conn->io_buf_io_left = l_left_to_read;
  a_conn->io_buf_io_done = 0;
  if (l_left_to_read > 0)
    {
      /* append the rest of the body to the
	 buffer */
      do {
	l_rv = http_trans_read_into_buf(a_conn);
	if ((l_rv == HTTP_TRANS_NOT_DONE) && 
	    (a_conn->sync == HTTP_TRANS_ASYNC))
	  return HTTP_TRANS_NOT_DONE;
	if ((l_rv == HTTP_TRANS_DONE) && (a_conn->last_read == 0))
	  return HTTP_TRANS_ERR;
      } while(l_rv == HTTP_TRANS_NOT_DONE);
      if (l_rv == HTTP_TRANS_ERR)
	return HTTP_TRANS_ERR;
    }
  /* write it into the body */
  flush_response_body (a_resp, a_conn);
  return HTTP_TRANS_DONE;
}

static int
read_body_standard(http_resp *a_resp,
		   http_req *a_req,
		   http_trans_conn *a_conn)
{
  int l_rv = 0;
  /* anything without a content length or chunked encoding */
  do {
    l_rv = http_trans_read_into_buf(a_conn);
    if (a_conn->sync == HTTP_TRANS_ASYNC)
      {
	if ((l_rv == HTTP_TRANS_NOT_DONE) || (a_conn->last_read != 0))
	  return HTTP_TRANS_NOT_DONE;
      }
  } while ((l_rv == HTTP_TRANS_NOT_DONE) || (a_conn->last_read > 0));

  if (l_rv == HTTP_TRANS_ERR)
    return HTTP_TRANS_ERR;

  flush_response_body(a_resp, a_conn);
  return HTTP_TRANS_DONE;
}
