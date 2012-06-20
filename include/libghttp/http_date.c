/*
 * http_date.c -- Routines for parsing and generating http dates
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 16-Aug-1998
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

#include <string.h>
#include <ctype.h>
#include "http_date.h"

static int
month_from_string_short(const char *a_month);

/* 
 *  date formats come in one of the following three flavors according to
 *  rfc 2068 and later drafts:
 *  
 *  Sun, 06 Nov 1994 08:49:37 GMT     ; RFC 822, updated by RFC 1123
 *  Sunday, 06-Nov-1994, 08:49:37 GMT ; RFC 850, updated by RFC 1036
 *  Sun Nov  6 08:49:37 1994          ; ANSI C's asctime() format
 *
 *  the first format is preferred however all must be supported.
*/
   

time_t
http_date_to_time(const char *a_date)
{
  struct tm       l_tm_time;
  time_t          l_return = 0;
  char            l_buf[12];
  const char     *l_start_date = NULL;
  int             i = 0;

  /* make sure we can use it */
  if (!a_date)
    return -1;
  memset(&l_tm_time, 0, sizeof(struct tm));
  memset(l_buf, 0, 12);
  /* try to figure out which format it's in */
  /* rfc 1123 */
  if (a_date[3] == ',')
    {
      if (strlen(a_date) != 29)
	return -1;
      /* make sure that everything is legal */
      if (a_date[4] != ' ')
	return -1;
      /* 06 */
      if ((isdigit(a_date[5]) == 0) ||
	  (isdigit(a_date[6]) == 0))
	return -1;
      /* Nov */
      if ((l_tm_time.tm_mon = month_from_string_short(&a_date[8])) < 0)
	return -1;
      /* 1994 */
      if ((isdigit(a_date[12]) == 0) ||
	  (isdigit(a_date[13]) == 0) ||
	  (isdigit(a_date[14]) == 0) ||
	  (isdigit(a_date[15]) == 0))
	return -1;
      if (a_date[16] != ' ')
	return -1;
      /* 08:49:37 */
      if ((isdigit(a_date[17]) == 0) ||
	  (isdigit(a_date[18]) == 0) ||
	  (a_date[19] != ':') ||
	  (isdigit(a_date[20]) == 0) ||
	  (isdigit(a_date[21]) == 0) ||
	  (a_date[22] != ':') ||
	  (isdigit(a_date[23]) == 0) ||
	  (isdigit(a_date[24]) == 0))
	return -1;
      if (a_date[25] != ' ')
	return -1;
      /* GMT */
      if (strncmp(&a_date[26], "GMT", 3) != 0)
	return -1;
      /* ok, it's valid.  Do it */
      /* parse out the day of the month */
      l_tm_time.tm_mday += (a_date[5] - 0x30) * 10;
      l_tm_time.tm_mday += (a_date[6] - 0x30);
      /* already got the month from above */
      /* parse out the year */
      l_tm_time.tm_year += (a_date[12] - 0x30) * 1000;
      l_tm_time.tm_year += (a_date[13] - 0x30) * 100;
      l_tm_time.tm_year += (a_date[14] - 0x30) * 10;
      l_tm_time.tm_year += (a_date[15] - 0x30);
      l_tm_time.tm_year -= 1900;
      /* parse out the time */
      l_tm_time.tm_hour += (a_date[17] - 0x30) * 10;
      l_tm_time.tm_hour += (a_date[18] - 0x30);
      l_tm_time.tm_min  += (a_date[20] - 0x30) * 10;
      l_tm_time.tm_min  += (a_date[21] - 0x30);
      l_tm_time.tm_sec  += (a_date[23] - 0x30) * 10;
      l_tm_time.tm_sec  += (a_date[24] - 0x30);
      /* ok, generate the result */
      l_return = mktime(&l_tm_time);
    }
  /* ansi C */
  else if (a_date[3] == ' ')
    {
      if (strlen(a_date) != 24)
	return -1;
      /* Nov */
      if ((l_tm_time.tm_mon =
	   month_from_string_short(&a_date[4])) < 0)
	return -1;
      if (a_date[7] != ' ')
	return -1;
      /* "10" or " 6" */
      if (((a_date[8] != ' ') && (isdigit(a_date[8]) == 0)) ||
	  (isdigit(a_date[9]) == 0))
	return -1;
      if (a_date[10] != ' ')
	return -1;
      /* 08:49:37 */
      if ((isdigit(a_date[11]) == 0) ||
	  (isdigit(a_date[12]) == 0) ||
	  (a_date[13] != ':') ||
	  (isdigit(a_date[14]) == 0) ||
	  (isdigit(a_date[15]) == 0) ||
	  (a_date[16] != ':') ||
	  (isdigit(a_date[17]) == 0) ||
	  (isdigit(a_date[18]) == 0))
	return -1;
      if (a_date[19] != ' ')
	return -1;
      /* 1994 */
      if ((isdigit(a_date[20]) == 0) ||
	  (isdigit(a_date[21]) == 0) ||
	  (isdigit(a_date[22]) == 0) ||
	  (isdigit(a_date[23]) == 0))
	return -1;
      /* looks good */
      /* got the month from above */
      /* parse out the day of the month */
      if (a_date[8] != ' ')
	l_tm_time.tm_mday += (a_date[8] - 0x30) * 10;
      l_tm_time.tm_mday += (a_date[9] - 0x30);
      /* parse out the time */
      l_tm_time.tm_hour += (a_date[11] - 0x30) * 10;
      l_tm_time.tm_hour += (a_date[12] - 0x30);
      l_tm_time.tm_min  += (a_date[14] - 0x30) * 10;
      l_tm_time.tm_min  += (a_date[15] - 0x30);
      l_tm_time.tm_sec  += (a_date[17] - 0x30) * 10;
      l_tm_time.tm_sec  += (a_date[18] - 0x30);
      /* parse out the year */
      l_tm_time.tm_year += (a_date[20] - 0x30) * 1000;
      l_tm_time.tm_year += (a_date[21] - 0x30) * 100;
      l_tm_time.tm_year += (a_date[22] - 0x30) * 10;
      l_tm_time.tm_year += (a_date[23] - 0x30);
      l_tm_time.tm_year -= 1900;
      /* generate the result */
      l_return = mktime(&l_tm_time);
    }
  /* must be the 1036... */
  else
    {
      /* check to make sure we won't rn out of any bounds */
      if (strlen(a_date) < 11)
	return -1;
      while (a_date[i])
	{
	  if (a_date[i] == ' ')
	    {
	      l_start_date = &a_date[i+1];
	      break;
	    }
	  i++;
	}
      /* check to make sure there was a space found */
      if (l_start_date == NULL)
	return -1;
      /* check to make sure that we don't overrun anything */
      if (strlen(l_start_date) != 22)
	return -1;
      /* make sure that the rest of the date was valid */
      /* 06- */
      if ((isdigit(l_start_date[0]) == 0) ||
	  (isdigit(l_start_date[1]) == 0) ||
	  (l_start_date[2] != '-'))
	return -1;
      /* Nov */
      if ((l_tm_time.tm_mon =
	   month_from_string_short(&l_start_date[3])) < 0)
	return -1;
      /* -94 */
      if ((l_start_date[6] != '-') ||
	  (isdigit(l_start_date[7]) == 0) ||
	  (isdigit(l_start_date[8]) == 0))
	return -1;
      if (l_start_date[9] != ' ')
	return -1;
      /* 08:49:37 */
      if ((isdigit(l_start_date[10]) == 0) ||
	  (isdigit(l_start_date[11]) == 0) ||
	  (l_start_date[12] != ':') ||
	  (isdigit(l_start_date[13]) == 0) ||
	  (isdigit(l_start_date[14]) == 0) ||
	  (l_start_date[15] != ':') ||
	  (isdigit(l_start_date[16]) == 0) ||
	  (isdigit(l_start_date[17]) == 0))
	return -1;
      if (l_start_date[18] != ' ')
	return -1;
      if (strncmp(&l_start_date[19], "GMT", 3) != 0)
	return -1;
      /* looks ok to parse */
      /* parse out the day of the month */
      l_tm_time.tm_mday += (l_start_date[0] - 0x30) * 10;
      l_tm_time.tm_mday += (l_start_date[1] - 0x30);
      /* have the month from above */
      /* parse out the year */
      l_tm_time.tm_year += (l_start_date[7] - 0x30) * 10;
      l_tm_time.tm_year += (l_start_date[8] - 0x30);
      /* check for y2k */
      if (l_tm_time.tm_year < 20)
	l_tm_time.tm_year += 100;
      /* parse out the time */
      l_tm_time.tm_hour += (l_start_date[10] - 0x30) * 10;
      l_tm_time.tm_hour += (l_start_date[11] - 0x30);
      l_tm_time.tm_min  += (l_start_date[13] - 0x30) * 10;
      l_tm_time.tm_min  += (l_start_date[14] - 0x30);
      l_tm_time.tm_sec  += (l_start_date[16] - 0x30) * 10;
      l_tm_time.tm_sec  += (l_start_date[17] - 0x30);
      /* generate the result */
      l_return = mktime(&l_tm_time);
    }
  return l_return;
}

static int
month_from_string_short(const char *a_month)
{
  if (strncmp(a_month, "Jan", 3) == 0)
    return 0;
  if (strncmp(a_month, "Feb", 3) == 0)
    return 1;
  if (strncmp(a_month, "Mar", 3) == 0)
    return 2;
  if (strncmp(a_month, "Apr", 3) == 0)
    return 3;
  if (strncmp(a_month, "May", 3) == 0)
    return 4;
  if (strncmp(a_month, "Jun", 3) == 0)
    return 5;
  if (strncmp(a_month, "Jul", 3) == 0)
    return 6;
  if (strncmp(a_month, "Aug", 3) == 0)
    return 7;
  if (strncmp(a_month, "Sep", 3) == 0)
    return 8;
  if (strncmp(a_month, "Oct", 3) == 0)
    return 9;
  if (strncmp(a_month, "Nov", 3) == 0)
    return 10;
  if (strncmp(a_month, "Dec", 3) == 0)
    return 11;
  /* not a valid date */
  return -1;
}

