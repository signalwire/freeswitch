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
 * $Id: mailstream_socket.c,v 1.22 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailstream_socket.h"

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include <stdlib.h>
#include <fcntl.h>
#ifdef HAVE_SYS_SOCKET_H
#	include <sys/socket.h>
#endif

#ifdef HAVE_STRING_H
#  include <string.h>
#endif

/*
  these 3 headers MUST be included before <sys/select.h>
  to insure compatibility with Mac OS X (this is true for 10.2)
*/

#ifndef _MSC_VER
#	include <sys/time.h>
#	include <sys/types.h>
#	ifdef HAVE_SYS_SELECT_H
#		include <sys/select.h>
#	endif
#endif

/* mailstream_low, socket */

static int mailstream_low_socket_close(mailstream_low * s);
static ssize_t mailstream_low_socket_read(mailstream_low * s,
					  void * buf, size_t count);
static ssize_t mailstream_low_socket_write(mailstream_low * s,
					   const void * buf, size_t count);
static void mailstream_low_socket_free(mailstream_low * s);
static int mailstream_low_socket_get_fd(mailstream_low * s);

static mailstream_low_driver local_mailstream_socket_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
	/* SEB */
	mailstream_low_socket_read,
	mailstream_low_socket_write,
	mailstream_low_socket_close,
	mailstream_low_socket_get_fd,
	mailstream_low_socket_free
#else
  mailstream_read: mailstream_low_socket_read,
  mailstream_write: mailstream_low_socket_write,
  mailstream_close: mailstream_low_socket_close,
  mailstream_free: mailstream_low_socket_free,
  mailstream_get_fd: mailstream_low_socket_get_fd,
#endif
};

mailstream_low_driver * mailstream_socket_driver =
&local_mailstream_socket_driver;

/* file descriptor must be given in (default) blocking-mode */

static struct mailstream_socket_data * socket_data_new(int fd)
{
  struct mailstream_socket_data * socket_data;

  socket_data = (struct mailstream_socket_data * ) malloc(sizeof(* socket_data));
  if (socket_data == NULL)
    goto err;
  
  socket_data->fd = fd;

  return socket_data;
  
 err:
  return NULL;
}

static void socket_data_free(struct mailstream_socket_data * socket_data)
{
  free(socket_data);
}

static void socket_data_close(struct mailstream_socket_data * socket_data)
{
#ifdef _MSC_VER
/* SEB */ 
  closesocket(socket_data->fd);
#else
  close(socket_data->fd);
#endif
  socket_data->fd = -1;
}

mailstream_low * mailstream_low_socket_open(int fd)
{
  mailstream_low * s;
  struct mailstream_socket_data * socket_data;

  socket_data = socket_data_new(fd);
  if (socket_data == NULL)
    goto err;

  s = mailstream_low_new(socket_data, mailstream_socket_driver);
  if (s == NULL)
    goto free_socket_data;

  return s;

 free_socket_data:
  socket_data_free(socket_data);
 err:
  return NULL;
}

static int mailstream_low_socket_close(mailstream_low * s)
{
  struct mailstream_socket_data * socket_data;

  socket_data = (struct mailstream_socket_data *) s->data;
  socket_data_close(socket_data);

  return 0;
}

static void mailstream_low_socket_free(mailstream_low * s)
{
  struct mailstream_socket_data * socket_data;

  socket_data = (struct mailstream_socket_data *) s->data;
  socket_data_free(socket_data);
  s->data = NULL;

  free(s);
}

static int mailstream_low_socket_get_fd(mailstream_low * s)
{
  struct mailstream_socket_data * socket_data;

  socket_data = (struct mailstream_socket_data *) s->data;
  return socket_data->fd;
}


static ssize_t mailstream_low_socket_read(mailstream_low * s,
					  void * buf, size_t count)
{
  struct mailstream_socket_data * socket_data;
  
  socket_data = (struct mailstream_socket_data *) s->data;
  
  /* timeout */
  {
    fd_set fds_read;
    fd_set fds_excp;
    struct timeval timeout;
    int r;
    
    timeout = mailstream_network_delay;
    
    FD_ZERO(&fds_read);
    FD_SET(socket_data->fd, &fds_read);
    FD_ZERO(&fds_excp);
    FD_SET(socket_data->fd, &fds_excp);
    r = select(socket_data->fd + 1, &fds_read, NULL, &fds_excp, &timeout);
    if (r == 0)
      return -1;
    if (FD_ISSET(socket_data->fd, &fds_excp))
      return -1;
    if (!FD_ISSET(socket_data->fd, &fds_read))
      return 0;
  }
  
/* SEB why not for all os ? */
#ifdef WIN32
  return recv(socket_data->fd, buf, count, 0);
#else
  return read(socket_data->fd, buf, count);
#endif
}

static ssize_t mailstream_low_socket_write(mailstream_low * s,
					   const void * buf, size_t count)
{
  struct mailstream_socket_data * socket_data;

  socket_data = (struct mailstream_socket_data *) s->data;
  /* timeout */
  {
    fd_set fds_write;
    fd_set fds_excp;
    struct timeval timeout;
    int r;
    
    timeout = mailstream_network_delay;
    
    FD_ZERO(&fds_write);
    FD_SET(socket_data->fd, &fds_write);
    FD_ZERO(&fds_excp);
    FD_SET(socket_data->fd, &fds_excp);
    r = select(socket_data->fd + 1, NULL, &fds_write, &fds_excp, &timeout);
    if (r == 0)
      return -1;
    if (FD_ISSET(socket_data->fd, &fds_excp))
      return -1;
    if (!FD_ISSET(socket_data->fd, &fds_write))
      return 0;
  }
  
/* SEB (bis) why not for all os? */
#ifdef WIN32
  return send(socket_data->fd, buf, count, 0);
#else
  return write(socket_data->fd, buf, count);
#endif
}


/* mailstream */

mailstream * mailstream_socket_open(int fd)
{
  mailstream_low * low;
  mailstream * s;

  low = mailstream_low_socket_open(fd);
  if (low == NULL)
    goto err;

  s = mailstream_new(low, 8192);
  if (s == NULL)
    goto free_low;

  return s;

 free_low:
  mailstream_low_close(low);
 err:
  return NULL;
}

