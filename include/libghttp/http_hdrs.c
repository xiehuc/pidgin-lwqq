/*
 * http_hdrs.c -- This file contains declarations for http headers
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 3-Aug-1998
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
#include "http_hdrs.h"

/* entity headers */

const char http_hdr_Allow[] = "Allow";
const char http_hdr_Content_Encoding[] = "Content-Encoding";
const char http_hdr_Content_Language[] = "Content-Language";
const char http_hdr_Content_Length[] = "Content-Length";
const char http_hdr_Content_Location[] = "Content-Location";
const char http_hdr_Content_MD5[] = "Content-MD5";
const char http_hdr_Content_Range[] = "Content-Range";
const char http_hdr_Content_Type[] = "Content-Type";
const char http_hdr_Expires[] = "Expires";
const char http_hdr_Last_Modified[] = "Last-Modified";

/* general headers */

const char http_hdr_Cache_Control[] = "Cache-Control";
const char http_hdr_Connection[] = "Connection";
const char http_hdr_Date[] = "Date";
const char http_hdr_Pragma[] = "Pragma";
const char http_hdr_Transfer_Encoding[] = "Transfer-Encoding";
const char http_hdr_Update[] = "Update";
const char http_hdr_Trailer[] = "Trailer";
const char http_hdr_Via[] = "Via";

/* request headers */

const char http_hdr_Accept[] = "Accept";
const char http_hdr_Accept_Charset[] = "Accept-Charset";
const char http_hdr_Accept_Encoding[] = "Accept-Encoding";
const char http_hdr_Accept_Language[] = "Accept-Language";
const char http_hdr_Authorization[] = "Authorization";
const char http_hdr_Expect[] = "Expect";
const char http_hdr_From[] = "From";
const char http_hdr_Host[] = "Host";
const char http_hdr_If_Modified_Since[] = "If-Modified-Since";
const char http_hdr_If_Match[] = "If-Match";
const char http_hdr_If_None_Match[] = "If-None-Match";
const char http_hdr_If_Range[] = "If-Range";
const char http_hdr_If_Unmodified_Since[] = "If-Unmodified-Since";
const char http_hdr_Max_Forwards[] = "Max-Forwards";
const char http_hdr_Proxy_Authorization[] = "Proxy-Authorization";
const char http_hdr_Range[] = "Range";
const char http_hdr_Referrer[] = "Referrer";
const char http_hdr_TE[] = "TE";
const char http_hdr_User_Agent[] = "User-Agent";

/* response headers */

const char http_hdr_Accept_Ranges[] = "Accept-Ranges";
const char http_hdr_Age[] = "Age";
const char http_hdr_ETag[] = "ETag";
const char http_hdr_Location[] = "Location";
const char http_hdr_Retry_After[] = "Retry-After";
const char http_hdr_Server[] = "Server";
const char http_hdr_Vary[] = "Vary";
const char http_hdr_Warning[] = "Warning";
const char http_hdr_WWW_Authenticate[] = "WWW-Authenticate";

/* Other headers */

const char http_hdr_Set_Cookie[] = "Set-Cookie";

/* WebDAV headers */

const char http_hdr_DAV[] = "DAV";
const char http_hdr_Depth[] = "Depth";
const char http_hdr_Destination[] = "Destination";
const char http_hdr_If[] = "If";
const char http_hdr_Lock_Token[] = "Lock-Token";
const char http_hdr_Overwrite[] = "Overwrite";
const char http_hdr_Status_URI[] = "Status-URI";
const char http_hdr_Timeout[] = "Timeout";

const char *http_hdr_known_list[] = 
{
  /* entity headers */
  http_hdr_Allow,
  http_hdr_Content_Encoding,
  http_hdr_Content_Language,
  http_hdr_Content_Length,
  http_hdr_Content_Location,
  http_hdr_Content_MD5,
  http_hdr_Content_Range,
  http_hdr_Content_Type,
  http_hdr_Expires,
  http_hdr_Last_Modified,
  /* general headers */
  http_hdr_Cache_Control,
  http_hdr_Connection,
  http_hdr_Date,
  http_hdr_Pragma,
  http_hdr_Transfer_Encoding,
  http_hdr_Update,
  http_hdr_Trailer,
  http_hdr_Via,
  /* request headers */
  http_hdr_Accept,
  http_hdr_Accept_Charset,
  http_hdr_Accept_Encoding,
  http_hdr_Accept_Language,
  http_hdr_Authorization,
  http_hdr_Expect,
  http_hdr_From,
  http_hdr_Host,
  http_hdr_If_Modified_Since,
  http_hdr_If_Match,
  http_hdr_If_None_Match,
  http_hdr_If_Range,
  http_hdr_If_Unmodified_Since,
  http_hdr_Max_Forwards,
  http_hdr_Proxy_Authorization,
  http_hdr_Range,
  http_hdr_Referrer,
  http_hdr_TE,
  http_hdr_User_Agent,
  /* response headers */
  http_hdr_Accept_Ranges,
  http_hdr_Age,
  http_hdr_ETag,
  http_hdr_Location,
  http_hdr_Retry_After,
  http_hdr_Server,
  http_hdr_Vary,
  http_hdr_Warning,
  http_hdr_WWW_Authenticate,
  NULL
};

/* functions dealing with headers */

const char *
http_hdr_is_known(const char *a_hdr)
{
  int l_pos = 0;
  const char *l_return = NULL;

  if (!a_hdr)
    goto ec;
  while(http_hdr_known_list[l_pos] != NULL)
    {
      if (strcasecmp(a_hdr, http_hdr_known_list[l_pos]) == 0)
	{
	  l_return = http_hdr_known_list[l_pos];
	  break;
	}
      l_pos++;
    }
 ec:
  return l_return;
}

http_hdr_list *
http_hdr_list_new(void)
{
  http_hdr_list *l_return = NULL;
  
  l_return = (http_hdr_list *)malloc(sizeof(http_hdr_list));
  memset(l_return, 0, sizeof(http_hdr_list));
  return l_return;
}

void
http_hdr_list_destroy(http_hdr_list *a_list)
{
  int i = 0;

  if (a_list == NULL)
    return;
  for(i=0; i < HTTP_HDRS_MAX; i++)
    {
      if (a_list->header[i] &&
	  (http_hdr_is_known(a_list->header[i]) == NULL))
	free(a_list->header[i]);
      if (a_list->value[i])
	free(a_list->value[i]);
    }
  free (a_list);
}

int
http_hdr_set_value_no_nts(http_hdr_list *a_list,
			  const char *a_name_start,
			  int a_name_len,
			  const char *a_val_start,
			  int a_val_len)
{
  int l_return = 0;
  char *l_temp_name = NULL;
  char *l_temp_val = NULL;

  /* note that a zero len value is valid... */
  if ((a_list == NULL) ||
      (a_name_start == NULL) ||
      (a_val_start == NULL) ||
      (a_name_len == 0))
    goto ec;
  l_temp_name = (char *)malloc(a_name_len + 1);
  memset(l_temp_name, 0, a_name_len + 1);
  memcpy(l_temp_name, a_name_start, a_name_len);
  l_temp_val = (char *)malloc(a_val_len + 1);
  memset(l_temp_val, 0, a_val_len + 1);
  memcpy(l_temp_val, a_val_start, a_val_len);
  /* set the value */
  l_return = http_hdr_set_value(a_list,
				l_temp_name,
				l_temp_val);
  free(l_temp_name);
  free(l_temp_val);
 ec:
  return l_return;
    
}

/** 
 * NB: if the key of header is "cookie", things to be more complex,
 * cause cookie may be more than one.
 * like:
 * Set-Cookie: a=b; c=d;
 * Set-Cookie: e=f; g=m;
 * So, we set cookie here
 * 
 * @param a_list 
 * @param a_name 
 * @param a_val 
 * 
 * @return 
 */
static int
http_hdr_set_cookie_value(http_hdr_list *a_list,
                   const char *a_val)
{
    int i = 0;
    char *l_temp_value = NULL;
    int l_return = 0;
  
    for (i = 0; i < HTTP_HDRS_MAX; i++) {
        if (a_list->header[i])
            continue;
        
        /* I promise not to mess with this value. */
        l_temp_value = (char *)http_hdr_is_known("Set-Cookie");
        if (l_temp_value) {
            a_list->header[i] = l_temp_value;
            /* dont free this later... */
        } else{
            a_list->header[i] = strdup("Set-Cookie");
        }
        a_list->value[i] = strdup(a_val);
        l_return = 1;
        break;
	}
    return l_return;
}

int
http_hdr_set_value(http_hdr_list *a_list,
                   const char *a_name,
                   const char *a_val)
{
    int i = 0;
    char *l_temp_value = NULL;
    int l_return = 0;

    if ((a_list == NULL) || (a_name == NULL) || (a_val == NULL))
        goto ec;

    /* If the key is "Set-Cookie", go to a different branch */
    if (!strcmp(a_name, "Set-Cookie")) {
        return http_hdr_set_cookie_value(a_list, a_val);
    }
    
    l_temp_value = http_hdr_get_value(a_list, a_name);
    if (l_temp_value == NULL)
    {
        for (i=0; i < HTTP_HDRS_MAX; i++)
        {
            if (a_list->header[i] == NULL)
            {
                /* I promise not to mess with this value. */
                l_temp_value = (char *)http_hdr_is_known(a_name);
                if (l_temp_value)
                {
                    a_list->header[i] = l_temp_value;
                    /* dont free this later... */
                }
                else
                    a_list->header[i] = strdup(a_name);
                a_list->value[i] = strdup(a_val);
                l_return = 1;
                break;
            }
        }
    }
    else
    {
        for(i = 0; i < HTTP_HDRS_MAX; i++)
        {
            if (a_list->value[i] == l_temp_value)
            {
                free(a_list->value[i]);
                a_list->value[i] = strdup(a_val);
                l_return = 1;
                break;
            }
        }
    }
ec:
    return l_return;
}

char *http_hdr_get_cookie(http_hdr_list *a_list, const char *a_name)
{
    int i = 0;
    char *l_return = NULL;

    if (a_name == NULL)
        goto ec;
    for (i=0; i < HTTP_HDRS_MAX; i++) {
        if (a_list->header[i] == NULL ||
            a_list->value[i] == NULL||
            strcasecmp(a_list->header[i], "Set-Cookie")) {
            continue;
        }
        if (strstr(a_list->value[i], a_name)) {
            char cookie[256] = {0};
            char *start;
            char *end;

            strncpy(cookie, a_list->value[i], sizeof(cookie) - 1);
            start = strstr(cookie, a_name);
            if(!start){
                goto ec;
            }
            start += strlen(a_name) + 1;
            end = strstr(start, ";");
            if (end) {
                *end = '\0';
            }

            l_return = strdup(start);
            break;
        }
    }
ec:
    return l_return;
}

char *
http_hdr_get_value(http_hdr_list *a_list,
		   const char *a_name)
{
  int i = 0;
  char *l_return = NULL;

  if (a_name == NULL)
    goto ec;
  for (i=0; i < HTTP_HDRS_MAX; i++)
    {
      if (a_list->header[i] &&
	  (strcasecmp(a_list->header[i], a_name) == 0))
	{
	  if (a_list->value[i] == NULL)
	    goto ec;
	  l_return = a_list->value[i];
	  break;
	}
    }
 ec:
  return l_return;
}

int
http_hdr_get_headers(http_hdr_list *a_list, char ***a_names,
		     int *a_num_names)
{
  int i = 0;
  int l_num_names = 0;
  char **l_names;

  if (a_num_names == NULL)
    return -1;
  if (a_names == NULL)
    return -1;

  /* set our return values */
  *a_names = NULL;
  *a_num_names = 0;
  
  /* make a pass to find out how many headers we have. */
  for (i=0; i < HTTP_HDRS_MAX; i++)
    {
      if (a_list->header[i])
	l_num_names++;
    }

  /* return if there are no headers */
  if (l_num_names == 0)
    return 0;

  /* now that we know how many headers we have allocate the number of
     slots in the return */
  l_names = malloc(sizeof(char *) * l_num_names);
  if (l_names == NULL)
    return -1;
  
  /* zero the list so that we can clean up later if we have to */
  memset(l_names, 0, l_num_names);

  /* copy the headers */
  for (i=0; i < HTTP_HDRS_MAX; i++)
    {
      if (a_list->header[i])
	{
	  l_names[i] = strdup(a_list->header[i]);
	  if (l_names[i] == NULL)
	    goto ec;
	}
    }
  
  *a_names = l_names;
  *a_num_names = l_num_names;
  return 0;

  /* something bad happened.  Try to free up as much as possible. */
 ec:
  if (l_names)
    {
      for (i=0; i < l_num_names; i++)
	{
	  if (l_names[i])
	    {
	      free(l_names[i]);
	      l_names[i] = NULL;
	    }
	}
      free(l_names);
      *a_names = 0;
    }
  *a_num_names = 0;
  return -1;
}

int
http_hdr_clear_value(http_hdr_list *a_list,
		     const char *a_name)
{
  int i = 0;
  int l_return = 0;

  if ((a_list == NULL) || (a_name == NULL))
    goto ec;
  for (i=0; i < HTTP_HDRS_MAX; i++)
    {
      if (a_name && a_list->header[i] && 
	  (strcasecmp(a_list->header[i], a_name) == 0))
	{
	  if (http_hdr_is_known(a_name) == NULL)
	    free(a_list->header[i]);
	  a_list->header[i] = NULL;
	  free(a_list->value[i]);
	  a_list->value[i] = NULL;
	}
    }
 ec:
  return l_return;
}

