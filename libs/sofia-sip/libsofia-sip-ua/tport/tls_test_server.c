/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "ssl.h"
#include "tls.h"

typedef int (*IO_HANDLER)(int i);

typedef struct __tls_mngr {
  int         sock;
  TLS_CONTEXT ctx;
  IO_HANDLER  handler;
} TLS_MNGR;

static TLS_MNGR tls_mngr[100] = {{0}};
static int mngr_size = 0;
static int width = 0;
static char *file_prefix = "";
static fd_set regifds;
static int master_socket = 0;

void init_event_mngr (void)
{
  mngr_size = 0;
  FD_ZERO (&regifds);
}

int tls_setblocking(int s, int blocking)
{
  unsigned mode = fcntl(s, F_GETFL, 0);

  if (mode < 0)
     return -1;

  if (blocking)
    mode &= ~(O_NDELAY | O_NONBLOCK);
  else
    mode |= O_NDELAY | O_NONBLOCK;

  return fcntl(s, F_SETFL, mode);
}

void regi_sock (int sock, TLS_CONTEXT ctx, IO_HANDLER handler)
{
  FD_SET (sock, &regifds);

  if (sock + 1 > width)
    width = sock + 1;

  tls_mngr[mngr_size].sock = sock;
  tls_mngr[mngr_size].ctx = ctx;
  tls_mngr[mngr_size].handler = handler;
  mngr_size++;

  tls_setblocking(sock, 0);

  printf("socket %d registered, ctx = %p, mngr_size = %d, width = %d\n",
         sock, ctx, mngr_size, width);
}

int get_width (void)
{
  int i;
  int lwidth = 1;

  for (i = 0; i < mngr_size; i++) {
    if (tls_mngr[i].sock + 1 > lwidth)
      lwidth = tls_mngr[i].sock + 1;
  }

  return lwidth;
}

int tls_read_buffer(TLS_MNGR mngr, char *buf, int size)
{
  int bytes = 0;
  TLS_CONTEXT ctx = mngr.ctx;

  do {
    int ret = tls_read(ctx, buf + bytes, size - bytes);
    int err = tls_get_error(ctx, ret);

    printf("tls_read returned %d\n", ret);

    switch (err) {
    case SSL_ERROR_NONE:
      bytes += ret;
      break;

    case SSL_ERROR_WANT_READ:
      printf("ssl error want read\n");
      return bytes;
      break;

    case SSL_ERROR_WANT_WRITE:
      break;

    case SSL_ERROR_ZERO_RETURN:
      printf("shutdown\n");
      tls_shutdown(ctx);
      return bytes;

    case SSL_ERROR_SYSCALL:
      perror("tls_syscall");
      tls_shutdown(ctx);
      FD_CLR(mngr.sock, &regifds);
      return bytes;

    default:
      printf("TLS error code %d\n", err);
      return bytes;
    }

    if (tls_pending(ctx)) {
      printf("read pending\n");
    }

  } while (tls_pending(ctx));

  printf("normal read %d bytes\n", bytes);
  return bytes;
}

int tls_slave_handler(int i)
{
  unsigned char buf[50000];
  char fname[100];
  FILE *fp;

  int bytes = tls_read_buffer(tls_mngr[i], buf, sizeof(buf));

  printf("buffer read %d bytes\n", bytes);

  if (bytes > 0) {
    sprintf(fname, "%s%02d.txt", file_prefix, i);
    fp = fopen(fname, "a");

    if (fp == NULL)
      perror("tls_slave_handler fopen");

    else {
      int ret = fwrite(buf, sizeof(char), bytes, fp);

      if (ret != bytes)
        perror("tls_slave_handler write");

      fclose(fp);
    }
  }

  return 0;
}

int tls_master_handler(int i)
{
  TLS_CONTEXT ctx_slave;
  TLS_ISSUES tls_issues = {0};
  int         sock;

  printf("tls_master_handler\n");

  tls_issues.master_socket = master_socket;

  sock = init_tls_slave(tls_issues, tls_mngr[i].ctx, &ctx_slave);

  if (sock < 0) {
    perror("init_tls_slave");
    return -1;
  }

  regi_sock(sock, ctx_slave, tls_slave_handler);

  return 0;
}

int event_mngr ()
{
  struct timeval tv;
  int            retv;
  int            i;
  fd_set readfds;

  tv.tv_sec = 5;
  tv.tv_usec = 0;

  readfds = regifds;

  retv = select(width, &readfds, NULL, NULL, &tv);
  printf("select returned %d\n", retv);

  if (retv > 0) {
    for (i=0; i < mngr_size; i++) {
      if (FD_ISSET(tls_mngr[i].sock, &readfds)) {
        tls_mngr[i].handler(i);
      }
    }
  }

  if (retv < 0)
    perror("select");

  return retv;
}

int main (int argc, char *argv[])
{
  TLS_ISSUES  tls_issues = {0};
  TLS_CONTEXT ctx;
  fd_set readfds;

  if (argc != 2) {
    printf("Usage: tls_test_server <file prefix>\n");
    exit(0);
  }

  file_prefix = strdup(argv[1]);

  init_event_mngr();

  if (init_tls_master(&master_socket,
                      tls_issues,
                      &ctx) < 0) {
    printf("init_tls_master failed\n");
    exit (1);
  }

  regi_sock(master_socket, ctx, tls_master_handler);

  for (;;) {
    readfds = regifds;
    event_mngr();
    usleep(100000L);
  }

  return 0;
}
