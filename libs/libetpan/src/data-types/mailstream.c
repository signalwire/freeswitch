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
 * $Id: mailstream.c,v 1.18 2006/06/28 06:13:47 skunk Exp $
 */

#include "mailstream.h"
#include "maillock.h"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEFAULT_NETWORK_TIMEOUT 300

#if 0
#ifdef LIBETPAN_MAILSTREAM_DEBUG

#define STREAM_DEBUG

#include <stdio.h>

#define LOG_FILE "libetpan-stream-debug.log"

int mailstream_debug = 0;

#define STREAM_LOG_BUF(buf, size) \
  if (mailstream_debug) { \
    FILE * f; \
    mode_t old_mask; \
    \
    old_mask = umask(0077); \
    f = fopen(LOG_FILE, "a"); \
    umask(old_mask); \
    if (f != NULL) { \
      maillock_write_lock(LOG_FILE, fileno(f)); \
      fwrite((buf), 1, (size), f); \
      maillock_write_unlock(LOG_FILE, fileno(f)); \
      fclose(f); \
    } \
  }

#define STREAM_LOG(str) \
  if (mailstream_debug) { \
    FILE * f; \
    mode_t old_mask; \
    \
    old_mask = umask(0077); \
    f = fopen(LOG_FILE, "a"); \
    umask(old_mask); \
    if (f != NULL) { \
      maillock_write_lock(LOG_FILE, fileno(f)); \
      fputs((str), f); \
      maillock_write_unlock(LOG_FILE, fileno(f)); \
      fclose(f); \
    } \
  }

#else

#define STREAM_LOG_BUF(buf, size) do { } while (0)
#define STREAM_LOG(buf) do { } while (0)

#endif
#endif

#define STREAM_LOG_BUF(buf, size) do { } while (0)
#define STREAM_LOG(buf) do { } while (0)


mailstream * mailstream_new(mailstream_low * low, size_t buffer_size)
{
  mailstream * s;

  s = malloc(sizeof(* s));
  if (s == NULL)
    goto err;

  s->read_buffer = malloc(buffer_size);
  if (s->read_buffer == NULL)
    goto free_s;
  s->read_buffer_len = 0;

  s->write_buffer = malloc(buffer_size);
  if (s->write_buffer == NULL)
    goto free_read_buffer;
  s->write_buffer_len = 0;

  s->buffer_max_size = buffer_size;
  s->low = low;
  
  return s;

 free_read_buffer:
  free(s->read_buffer);
 free_s:
  free(s);
 err:
  return NULL;
}

static size_t write_to_internal_buffer(mailstream * s,
				       const void * buf, size_t count)
{
  memcpy(s->write_buffer + s->write_buffer_len, buf, count);
  s->write_buffer_len += count;

  return count;
}

static size_t write_direct(mailstream * s, const void * buf, size_t count)
{
  size_t left;
  const char * cur_buf;
  ssize_t written;
  
  cur_buf = buf;
  left = count;
  while (left > 0) {
    written = mailstream_low_write(s->low, cur_buf, left);

    if (written == -1) {
      if (count == left)
	return -1;
      else
	return count - left;
    }

    cur_buf += written;
    left -= written;
  }
  
  return count;
}

ssize_t mailstream_write(mailstream * s, const void * buf, size_t count)
{
  int r;

  if (s == NULL)
    return -1;

  if (count + s->write_buffer_len > s->buffer_max_size) {
    r = mailstream_flush(s);
    if (r == -1)
      return -1;

    if (count > s->buffer_max_size)
      return write_direct(s, buf, count);
  }

#ifdef STREAM_DEBUG
  STREAM_LOG(">>>>>>> send >>>>>>\n");
  STREAM_LOG_BUF(buf, count);
  STREAM_LOG("\n");
  STREAM_LOG(">>>>>>> end send >>>>>>\n");
#endif

  return write_to_internal_buffer(s, buf, count);
}

int mailstream_flush(mailstream * s)
{
  char * cur_buf;
  size_t left;
  ssize_t written;

  if (s == NULL)
    return -1;

  cur_buf = s->write_buffer;
  left = s->write_buffer_len;
  while (left > 0) {
    written = mailstream_low_write(s->low, cur_buf, left);

    if (written == -1)
      goto move_buffer;
    cur_buf += written;
    left -=  written;
  }

  s->write_buffer_len = 0;

  return 0;

 move_buffer:
  memmove(s->write_buffer, cur_buf, left);
  s->write_buffer_len = left;
  return -1;
}

static ssize_t read_from_internal_buffer(mailstream * s,
					 void * buf, size_t count)
{
  if (count >= s->read_buffer_len)
    count = s->read_buffer_len;
  if (count != 0)
    memcpy(buf, s->read_buffer, count);

  s->read_buffer_len -= count;
  if (s->read_buffer_len != 0)
    memmove(s->read_buffer, s->read_buffer + count,
	    s->read_buffer_len);

  return count;
}

static ssize_t read_through_buffer(mailstream * s, void * buf, size_t count)
{
  size_t left;
  char * cur_buf;
  ssize_t bytes_read;

  cur_buf = buf;
  left = count;

  while (left > 0) {
    bytes_read = mailstream_low_read(s->low, cur_buf, left);

    if (bytes_read == -1) {
      if (count == left)
	return -1;
      else
	return count - left;
    }
    else if (bytes_read == 0)
      return count - left;

    cur_buf += bytes_read;
    left -= bytes_read;
  }

  return count;
}

ssize_t mailstream_read(mailstream * s, void * buf, size_t count)
{
  ssize_t read_bytes;
  char * cur_buf;
  size_t left;
  
  if (s == NULL)
    return -1;

  left = count;
  cur_buf = buf;
  read_bytes = read_from_internal_buffer(s, cur_buf, left);
  cur_buf += read_bytes;
  left -= read_bytes;

  if (left == 0) {
#ifdef STREAM_DEBUG
    STREAM_LOG("<<<<<<< read <<<<<<\n");
    STREAM_LOG_BUF(buf, read_bytes);
    STREAM_LOG("\n");
    STREAM_LOG("<<<<<<< end read <<<<<<\n");
#endif

    return read_bytes;
  }

  if (left > s->buffer_max_size) {
    read_bytes = read_through_buffer(s, cur_buf, left);
    if (read_bytes == -1) {
      if (count == left)
	return -1;
      else {

#ifdef STREAM_DEBUG
        STREAM_LOG("<<<<<<< read <<<<<<\n");
        STREAM_LOG_BUF(buf, count - left);
        STREAM_LOG("\n");
        STREAM_LOG("<<<<<<< end read <<<<<<\n");
#endif

	return count - left;
      }
    }

    cur_buf += read_bytes;
    left -= read_bytes;

#ifdef STREAM_DEBUG
    STREAM_LOG("<<<<<<< read <<<<<<\n");
    STREAM_LOG_BUF(buf, count - left);
    STREAM_LOG("\n");
    STREAM_LOG("<<<<<<< end read <<<<<<\n");
#endif

    return count - left;
  }

  read_bytes = mailstream_low_read(s->low, s->read_buffer, s->buffer_max_size);
  if (read_bytes == -1) {
    if (left == count)
      return -1;
    else {
#ifdef STREAM_DEBUG
      STREAM_LOG("<<<<<<< read <<<<<<\n");
      STREAM_LOG_BUF(buf, count - left);
      STREAM_LOG("\n");
      STREAM_LOG("<<<<<<< end read <<<<<<\n");
#endif
      
      return count - left;
    }
  }
  else
    s->read_buffer_len += read_bytes;

  read_bytes = read_from_internal_buffer(s, cur_buf, left);
  cur_buf += read_bytes;
  left -= read_bytes;

#ifdef STREAM_DEBUG
  STREAM_LOG("<<<<<<< read <<<<<<\n");
  STREAM_LOG_BUF(buf, count - left);
  STREAM_LOG("\n");
  STREAM_LOG("<<<<<<< end read <<<<<<\n");
#endif

  return count - left;
}

mailstream_low * mailstream_get_low(mailstream * s)
{
  return s->low;
}

void mailstream_set_low(mailstream * s, mailstream_low * low)
{
  s->low = low;
}

int mailstream_close(mailstream * s)
{
  mailstream_low_close(s->low);
  mailstream_low_free(s->low);
  
  free(s->read_buffer);
  free(s->write_buffer);
  
  free(s);

  return 0;
}



ssize_t mailstream_feed_read_buffer(mailstream * s)
{
  ssize_t read_bytes;
  
  if (s == NULL)
    return -1;

  if (s->read_buffer_len == 0) {
    read_bytes = mailstream_low_read(s->low, s->read_buffer,
				     s->buffer_max_size);
    if (read_bytes == -1)
      return -1;
    s->read_buffer_len += read_bytes;
  }

  return s->read_buffer_len;
}

#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
struct timeval mailstream_network_delay =
{  DEFAULT_NETWORK_TIMEOUT, 0 };
#else
struct timeval mailstream_network_delay =
  { .tv_sec = DEFAULT_NETWORK_TIMEOUT, .tv_usec = 0 };
#endif
