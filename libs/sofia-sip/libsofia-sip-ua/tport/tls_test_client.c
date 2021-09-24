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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "ssl.h"
#include "tls.h"

void write_file(FILE *fp, int size);

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

void tls_write_buffer(TLS_CONTEXT ctx, unsigned char *buf, int size)
{
  int ret, err;
  int pos = 0;

  while (pos < size) {
    int bytes = pos + 30000 < size ? 30000 : size - pos;

    printf("writing %d bytes\n", bytes);

    ret = tls_write(ctx, buf + pos, bytes);
    err = tls_get_error(ctx, ret);

    switch(err) {
    case SSL_ERROR_NONE:
      pos += ret;
      printf("%d bytes written\n", ret);
      break;

    case SSL_ERROR_WANT_WRITE:
      printf("want write\n");
      break;

    case SSL_ERROR_WANT_READ:
      printf("want read\n");
      break;

    default:
      printf("TLS error code %d\n", err);
    }
  }
}

int main (int argc, char *argv[])
{
  int i, pid;
  FILE *fp;
  struct stat stats;

  if (argc != 2) {
    printf("Usage: tls_test_client <file>\n");
  }

  for (i = 0; i < 10; i++) {
    pid = fork();
    if (pid == 0)
      break;
  }

  fp = fopen(argv[1], "r");

  if (fp == NULL) {
    perror("fopen");
    exit(1);
  }

  if (fstat(fileno(fp), &stats) < 0) {
    perror("fstat");
    exit(1);
  }

  for (i = 0; i < 20; i++)
    write_file(fp, stats.st_size);

  return 0;
}

void write_file(FILE *fp, int size)
{
  TLS_ISSUES  tls_issues = {0};
  TLS_CONTEXT ctx;
  int sock, posFile = 0;

  printf("tls_client connecting\n");

  sock = init_tls_client(tls_issues, &ctx);

  printf("tls_client initialized\n");

  if (sock < 0)
    exit (1);

  tls_setblocking(sock, 0);

  rewind(fp);

  while (posFile < size) {
    int posBuffer = 0;
    char buf[150000];

    while (posBuffer < sizeof(buf)) {
      int bytes;

      bytes = read(fileno(fp), buf + posBuffer, sizeof(buf));

      if (bytes == 0)
        break;

      if (bytes < 0) {
        perror("read");
      }

      else {
        printf("writing %d buffer\n", bytes);
        tls_write_buffer(ctx, buf, bytes);
        printf("wrote %d buffer\n", bytes);
        posBuffer += bytes;
        posFile += bytes;
      }

      if (posFile >= size)
        break;
    }
  }

  printf("wrote total %d bytes\n", posFile);
}
