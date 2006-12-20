/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: mailstream_helper.c,v 1.17 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailstream_helper.h"
#include <string.h>
#include <stdio.h>
#include "mail.h"

static void remove_trailing_eol(MMAPString * mmapstr)
{
  if (mmapstr->str[mmapstr->len - 1] == '\n') {
    mmapstr->len --;
    mmapstr->str[mmapstr->len] = '\0';
  }
  if (mmapstr->str[mmapstr->len - 1] == '\r') {
    mmapstr->len --;
    mmapstr->str[mmapstr->len] = '\0';
  }
}

char * mailstream_read_line(mailstream * stream, MMAPString * line)
{
  if (mmap_string_assign(line, "") == NULL)
    return NULL;

  return mailstream_read_line_append(stream, line);
}

static char * mailstream_read_len_append(mailstream * stream,
					 MMAPString * line,
					 size_t i)
{
  size_t cur_size;

  cur_size = line->len;
  if (mmap_string_set_size(line, line->len + i) == NULL)
    return NULL;
  if (mailstream_read(stream, line->str + cur_size, i) < 0)
    return NULL;
  return line->str;
}

char * mailstream_read_line_append(mailstream * stream, MMAPString * line)
{
  if (stream == NULL)
    return NULL;

  do {
    if (stream->read_buffer_len > 0) {
      size_t i;

      i = 0;
      while (i < stream->read_buffer_len) {
        if (stream->read_buffer[i] == '\n')
          return mailstream_read_len_append(stream, line, i + 1);
        i++;
      }
      if (mailstream_read_len_append(stream, line,
				     stream->read_buffer_len) == NULL)
        return NULL;
    }
    else {
      ssize_t r;
      
      r = mailstream_feed_read_buffer(stream);
      if (r == -1)
        return NULL;

      if (r == 0)
        break;
    }
  }
  while (1);

  return line->str;
}

char * mailstream_read_line_remove_eol(mailstream * stream, MMAPString * line)
{
  if (!mailstream_read_line(stream, line))
    return NULL;

  remove_trailing_eol(line);

  return line->str;
}

int mailstream_is_end_multiline(const char * line)
{
  if (line[0] != '.')
    return FALSE;
  if (line[1] != 0)
    return FALSE;
  return TRUE;
}

#if 1
char * mailstream_read_multiline(mailstream * s, size_t size,
				  MMAPString * stream_buffer,
				  MMAPString * multiline_buffer,
				  size_t progr_rate,
				  progress_function * progr_fun)
{
  size_t count;
  char * line;
  size_t last;

  if (mmap_string_assign(multiline_buffer, "") == NULL)
    return NULL;

  count = 0;
  last = 0;

  while ((line = mailstream_read_line_remove_eol(s, stream_buffer)) != NULL) {
    if (mailstream_is_end_multiline(line))
      return multiline_buffer->str;

    if (line[0] == '.') {
      if (mmap_string_append(multiline_buffer, line + 1) == NULL)
	return NULL;
    }
    else {
      if (mmap_string_append(multiline_buffer, line) == NULL)
	return NULL;
    }
    if (mmap_string_append(multiline_buffer, "\r\n") == NULL)
      return NULL;

    count += strlen(line);
    if ((size != 0) && (progr_rate != 0) && (progr_fun != NULL))
      if (count - last >= progr_rate) {
	(* progr_fun)(count, size);
	last = count;
      }
  }

  return NULL;
}

#else

/*
  high speed but don't replace the line break with '\n' and neither
  remove the '.'
*/

static gboolean end_of_multiline(const char * str, gint len)
{
  gint index;

  index = len - 1;

  if (str[index] != '\n')
    return FALSE;
  if (index == 0)
    return FALSE;

  index --;

  if (str[index] == '\r') {
    index --;
    if (index == 0)
      return FALSE;
  }

  if (str[index] != '.')
    return FALSE;
  if (index == 0)
    return FALSE;

  index--;

  if (str[index] != '\n')
    return FALSE;

  return TRUE;
}

char * mailstream_read_multiline(mailstream * stream, size_t size,
				 MMAPString * stream_buffer,
				 MMAPString * line,
				 size_t progr_rate,
				 progress_function * progr_fun)
{
  if (stream == NULL)
    return NULL;

  mmap_string_assign(line, "");

  do {
    if (stream->read_buffer_len > 0) {
      size_t i;

      i = 0;
      while (i < stream->read_buffer_len) {
	if (end_of_multiline(stream->read_buffer, i + 1))
	  return mailstream_read_len_append(stream, line, i + 1);
	i++;
      }
      if (mailstream_read_len_append(stream, line,
				     stream->read_buffer_len) == NULL)
	return NULL;
      if (end_of_multiline(line->str, line->len))
	return line->str;
    }
    else
      if (mailstream_feed_read_buffer(stream) == -1)
	return NULL;
  }
  while (1);

  return line->str;
}
#endif



static inline ssize_t send_data_line(mailstream * s,
    const char * line, size_t length)
{
  int fix_eol;
  const char * start;
  size_t count;

  start = line;

  fix_eol = 0;
  count = 0;

  while (1) {
    if (length == 0)
      break;

    if (* line == '\r') {
      line ++;

      count ++;
      length --;

      if (length == 0) {
        fix_eol = 1;
        break;
      }
      
      if (* line == '\n') {
	line ++;

	count ++;
	length --;
	
	break;
      }
      else {
        fix_eol = 1;
        break;
      }
    }
    else if (* line == '\n') {
      line ++;

      count ++;
      length --;

      fix_eol = 1;
      break;
    }

    line ++;
    length --;
    count ++;
  }

  if (fix_eol) {
    if (mailstream_write(s, start, count - 1) == -1)
      goto err;
    if (mailstream_write(s, "\r\n", 2) == -1)
      goto err;
  }
  else {
    if (mailstream_write(s, start, count) == -1)
      goto err;
  }
  

#if 0
  while (* line != '\n') {
    if (* line == '\r')
      pos = line;
    if (* line == '\0')
      return line;
    if (mailstream_write(s, line, 1) == -1)
      goto err;
    line ++;
  }
  if (pos + 1 == line) {
    if (mailstream_write(s, line, 1) == -1)
      goto err;
  }
  else {
    if (mailstream_write(s, "\r\n", 2) == -1)
      goto err;
  }
  line ++;
#endif

  return count;
  
 err:
  return -1;
}

static inline int send_data_crlf(mailstream * s, const char * message,
    size_t size,
    int quoted,
    size_t progr_rate,
    progress_function * progr_fun)
{
  const char * current;
  size_t count;
  size_t last;
  size_t remaining;

  count = 0;
  last = 0;

  current = message;
  remaining = size;

  while (remaining > 0) {
    ssize_t length;
    
    if (quoted) {
      if (current[0] == '.')
        if (mailstream_write(s, ".", 1) == -1)
          goto err;
    }
    
    length = send_data_line(s, current, remaining);
    if (length < 0)
      goto err;

    current += length;

    count += length;
    if ((progr_rate != 0) && (progr_fun != NULL))
      if (count - last >= progr_rate) {
	(* progr_fun)(count, size);
	last = count;
      }

    remaining -= length;
  }
  
  return 0;
  
 err:
  return -1;
}

int mailstream_send_data_crlf(mailstream * s, const char * message,
    size_t size,
    size_t progr_rate,
    progress_function * progr_fun)
{
  return send_data_crlf(s, message, size, 0, progr_rate, progr_fun);
}

int mailstream_send_data(mailstream * s, const char * message,
			 size_t size,
			 size_t progr_rate,
			 progress_function * progr_fun)
{
  if (send_data_crlf(s, message, size, 1, progr_rate, progr_fun) == -1)
    goto err;
  
  if (mailstream_write(s, "\r\n.\r\n", 5) == -1)
    goto err;

  if (mailstream_flush(s) == -1)
    goto err;

  return 0;

 err:
  return -1;
}

static inline ssize_t get_data_size(const char * line, size_t length,
    size_t * result)
{
  int fix_eol;
  const char * start;
  size_t count;
  size_t fixed_count;
  
  start = line;

  fix_eol = 0;
  count = 0;
  fixed_count = 0;
  
  while (1) {
    if (length == 0)
      break;

    if (* line == '\r') {
      line ++;

      count ++;
      length --;

      if (length == 0) {
        fix_eol = 1;
        fixed_count ++;
        break;
      }
      
      if (* line == '\n') {
	line ++;

	count ++;
	length --;
	
	break;
      }
      else {
        fix_eol = 1;
        fixed_count ++;
        break;
      }
    }
    else if (* line == '\n') {
      line ++;

      count ++;
      length --;

      fix_eol = 1;
      fixed_count ++;
      break;
    }

    line ++;
    length --;
    count ++;
  }
  
  * result = count + fixed_count;
  
  return count;
}

size_t mailstream_get_data_crlf_size(const char * message, size_t size)
{
  const char * current;
  size_t count;
  size_t last;
  size_t remaining;
  size_t fixed_count;
  
  count = 0;
  last = 0;
  fixed_count = 0;
  
  current = message;
  remaining = size;

  while (remaining > 0) {
    ssize_t length;
    size_t line_count;
    
    length = get_data_size(current, remaining, &line_count);
    
    fixed_count += line_count;
    current += length;

    count += length;
    
    remaining -= length;
  }
  
  return fixed_count;
}
