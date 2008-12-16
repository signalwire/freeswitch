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

/**@internal @file portbind.c
 * @brief Bind a socket to an UDP/TCP port and return the port number
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Mar 25 12:12:25 2004 ppessi
 */

#include "config.h"

char const name[] = "portbind";
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#if HAVE_SYS_SOCKET_T
#include <sys/socket.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include "sofia-sip/su.h"

#if !defined(IPPROTO_SCTP)
#define IPPROTO_SCTP (132)
#endif

static
char const helptext[] =
  "usage: portbind OPTIONS [port]\n"
  "  where OPTIONS are\n"
  "    [-t] bind to TCP\n"
  "    [-u] bind to UDP\n"
  "    [-s] bind to SCTP\n"
#if HAVE_SIN6
  "    [-6] bind to IPv6 only\n"
  "    [-4] bind to IPv4 only\n"
  "  By default, portbind binds to UDP and TCP on IPv6 and IPv4.\n"
#else
  "  By default, portbind binds to UDP and TCP.\n"
#endif
;

void usage(int returncode)
{
  fprintf(returncode ? stderr : stdout, helptext);
  exit(returncode);
}

int main(int argc, char *argv[])
{
  char *o_port = "0", *o_protocol = NULL;
  int o_tcp = 0, o_udp = 0, o_sctp = 0;
  char *names[5] = { NULL };
  int protos[5] = { 0 };
  int types[5] = { 0 };
  int n, N = 0;
  unsigned long portno;
  unsigned short port;
  int af;
  su_socket_t s;
  socklen_t salen;
  struct sockaddr_storage ss[1];
  struct sockaddr *sa = (void *)ss;
  struct sockaddr_in *sin = (void *)ss;
#if HAVE_SIN6
  int o_ip6 = 0, o_ip4 = 0;
  struct sockaddr_in6 *sin6 = (void *)ss;
#endif

  for (argv++; *argv && **argv == '-';) {
    char *opt = *argv++ + 1;
    if (strcmp(opt, "-") == 0)
      break;
    else if (strcmp(opt, "P") == 0 && *argv)
      o_protocol = *argv++;
    else if (strcmp(opt, "u") == 0)
      o_udp = 1;
    else if (strcmp(opt, "t") == 0)
      o_tcp = 1;
    else if (strcmp(opt, "s") == 0)
      o_sctp = 1;
    else if (strcmp(opt, "h") == 0)
      usage(0);
    else if (strcmp(opt, "-help") == 0)
      usage(0);
#if HAVE_SIN6
    else if (strcmp(opt, "6") == 0)
      o_ip6 = AF_INET6;
    else if (strcmp(opt, "4") == 0)
      o_ip4 = 0;
#endif
    else
      usage(1);
  }

  if (argv[0]) {
    char *s = NULL;

    portno = strtoul(argv[0], &s, 10);

    if (portno != (portno & 65535) || s == argv[0] || *s) {
      fprintf(stderr, "%s: invalid port %s\n", name, argv[0]);
      exit(1);
    }
  } else {
    portno = 0;
  }

#if HAVE_SIN6
  if (o_ip6)
    af = AF_INET6;
  else if (o_ip4)
    af = AF_INET;
  else
    af = AF_INET6;
#else
  af = AF_INET;
#endif

  memset(ss, 0, sizeof ss);

  if (!o_tcp || !o_udp || !o_sctp || !o_protocol)
    o_tcp = o_udp = 1;

  if (o_protocol) {
    struct protoent *pent = getprotobyname(o_protocol);

    if (!pent) {
      fprintf(stderr, "%s: %s\n", o_protocol, "unknown protocol");
      exit(1);
    }
    names[N] = pent->p_name, protos[N] = pent->p_proto, types[N++] = SOCK_RAW;
  }

  if (o_tcp)
    names[N] = "TCP", protos[N] = IPPROTO_TCP, types[N++] = SOCK_STREAM;
  if (o_udp)
    names[N] = "UDP", protos[N] = IPPROTO_UDP, types[N++] = SOCK_DGRAM;
  if (o_sctp)
    names[N] = "SCTP", protos[N] = IPPROTO_SCTP, types[N++] = SOCK_SEQPACKET;

  assert(N != 0);

  port = portno;

  for (n = 0; n < N;) {
    s = su_socket(sa->sa_family = af, types[n], protos[n]);

#if HAVE_SIN6
    if (s == INVALID_SOCKET && af == AF_INET6 && !o_ip6)
      s = su_socket(sa->sa_family = af = AF_INET, types[n], protos[n]);
#endif

    if (s == INVALID_SOCKET) {
      fprintf(stderr, "%s: socket(AF_INET%s, 0, %s): %s\n",
	      name, af == AF_INET ? "" : "6", names[n], strerror(errno));
      exit(1);
    }

#if HAVE_SIN6
    if (af == AF_INET6 && !o_ip6 && !o_ip4)
      o_ip6 = o_ip4;
    if (af == AF_INET6)
      salen = sizeof *sin6;
    else
#endif
      salen = sizeof *sin;

    sin->sin_port = htons(port);

    if (bind(s, sa, salen) == -1) {
      if (errno == EADDRINUSE) {
	if (++port == 0)
	  port = 1024;
	if (port != portno) {
	  close(s);
	  n = 0;
	  continue;
	}
      }

      fprintf(stderr, "%s: bind(%s): %s\n", name, o_port, strerror(errno));
      exit(1);
    }

    if (portno == 0) {
      struct sockaddr_storage ss[1];
      struct sockaddr *sa = (void *)ss;
      struct sockaddr_in *sin = (void *)ss;

      salen = sizeof *ss;
      if (getsockname(s, sa, &salen) == -1) {
	fprintf(stderr, "%s: getsockname(): %s\n", name, strerror(errno));
	exit(1);
      }

      portno = port = ntohs(sin->sin_port);
    }

    close(s);

    n++;
  }

  printf("%u\n", ntohs(sin->sin_port));

  return 0;
}

