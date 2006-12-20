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
 * $Id: mailstream_low.c,v 1.17 2006/05/22 13:39:40 hoa Exp $
 */

#include "mailstream_low.h"
#include <stdlib.h>

#ifdef LIBETPAN_MAILSTREAM_DEBUG

#define STREAM_DEBUG

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include "maillock.h"
#ifdef _MSC_VER
#	include "win_etpan.h"
#endif

#define LOG_FILE "libetpan-stream-debug.log"

LIBETPAN_EXPORT
int mailstream_debug = 0;

LIBETPAN_EXPORT
void (* mailstream_logger)(int direction,
    const char * str, size_t size) = NULL;

#define STREAM_LOG_BUF(direction, buf, size) \
  if (mailstream_debug) { \
    if (mailstream_logger != NULL) { \
      mailstream_logger(direction, buf, size); \
    } \
    else { \
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
    } \
  }

#define STREAM_LOG(direction, str) \
  if (mailstream_debug) { \
    if (mailstream_logger != NULL) { \
      mailstream_logger(direction, str, strlen(str) + 1); \
    } \
    else { \
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
    } \
  }

#else

#define STREAM_LOG_BUF(direction, buf, size) do { } while (0)
#define STREAM_LOG(direction, buf) do { } while (0)

#endif


/* general functions */

mailstream_low * mailstream_low_new(void * data,
				    mailstream_low_driver * driver)
{
  mailstream_low * s;

  s = malloc(sizeof(* s));
  if (s == NULL)
    return NULL;

  s->data = data;
  s->driver = driver;

  return s;
}

int mailstream_low_close(mailstream_low * s)
{
  if (s == NULL)
    return -1;
  s->driver->mailstream_close(s);

  return 0;
}

int mailstream_low_get_fd(mailstream_low * s)
{
  if (s == NULL)
    return -1;
  return s->driver->mailstream_get_fd(s);
}

void mailstream_low_free(mailstream_low * s)
{
  s->driver->mailstream_free(s);
}

ssize_t mailstream_low_read(mailstream_low * s, void * buf, size_t count)
{
  ssize_t r;
  
  if (s == NULL)
    return -1;
  r = s->driver->mailstream_read(s, buf, count);
  
#ifdef STREAM_DEBUG
  if (r > 0) {
    STREAM_LOG(0, "<<<<<<< read <<<<<<\n");
    STREAM_LOG_BUF(0, buf, r);
    STREAM_LOG(0, "\n");
    STREAM_LOG(0, "<<<<<<< end read <<<<<<\n");
  }
#endif
  
  return r;
}

ssize_t mailstream_low_write(mailstream_low * s,
    const void * buf, size_t count)
{
  if (s == NULL)
    return -1;

#ifdef STREAM_DEBUG
  STREAM_LOG(1, ">>>>>>> send >>>>>>\n");
  STREAM_LOG_BUF(1, buf, count);
  STREAM_LOG(1, "\n");
  STREAM_LOG(1, ">>>>>>> end send >>>>>>\n");
#endif

  return s->driver->mailstream_write(s, buf, count);
}
