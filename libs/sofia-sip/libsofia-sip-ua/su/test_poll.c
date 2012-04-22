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

/**@internal
 * @file test_poll.c
 * Example code for <sofia-sip/su_wait.h>.
 *
 * This file illustrates how the asynchronous connect can be used with @b su.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "sofia-sip/su.h"
#include "sofia-sip/su_wait.h"
#include "sofia-sip/su_types.h"
#include "sofia-sip/su_log.h"

void usage(char const *name)
{
  fprintf(stderr, "usage: %s [host]\n", name);
  exit(1);
}

int main(int argc, char *argv[])
{
  su_socket_t s;
  su_sockaddr_t su = { 0 };
  char *host = argv[1];
  char *port = host ? argv[2] : NULL;
  su_addrinfo_t *ai = NULL, hints[1] = {{ 0 }};
  int error;

  if (argv[1] && (strcmp(argv[1], "--help") == 0 ||
		  strcmp(argv[1], "-?") == 0))
    usage(argv[0]);

  if (!port) port = "echo";

  if ((error = su_getaddrinfo(host, port, hints, &ai))) {
    fprintf(stderr, "poll_test: su_getaddrinfo(): %s\n",
	    su_gai_strerror(error));
    exit(1);
  }

  memcpy(SU_ADDR(&su), ai->ai_addr, ai->ai_addrlen);

  s = su_socket(ai->ai_family, SOCK_STREAM, 0);
  if (s == INVALID_SOCKET) {
    su_perror("socket");
    exit(1);
  }

  su_freeaddrinfo(ai);

  su_setblocking(s, 0);		/* Don't block */

  if (connect(s, &su.su_sa, su_sockaddr_size(&su)) == -1) {
    if (errno != EINPROGRESS) {
      su_perror("connect");
      exit(1);
    }
  }

  {
    su_wait_t w;
    int n, err;

    su_wait_create(&w, s, SU_WAIT_OUT);

    n = su_wait(&w, 1, SU_WAIT_FOREVER);

    printf("su_wait returned %d\n", n);

    err = su_soerror(s);

    printf("connect: %s\n", su_strerror(err));
  }

  exit(0);
}
