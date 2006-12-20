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
 * $Id: connect.c,v 1.16 2006/07/15 12:24:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "connect.h"

#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#ifdef _MSC_VER
#	include "win_etpan.h"
#else
#	include <netdb.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#	include <unistd.h>
#endif

uint16_t mail_get_service_port(const char * name, char * protocol)
{
  struct servent * service;

  service = getservbyname(name, protocol);

  if (service == NULL)
    return 0;

  return ntohs(service->s_port);
}

int mail_tcp_connect(const char * server, uint16_t port)
{
#ifndef HAVE_IPV6
  struct hostent * remotehost;
  struct sockaddr_in sa;
#else /* HAVE_IPV6 */
  struct addrinfo hints, *res, *ai;
  char port_str[6];
#endif
#ifdef WIN32
  SOCKET s;
#else
  int s;
#endif
  int r;

#ifndef HAVE_IPV6
  s = socket(PF_INET, SOCK_STREAM, 0);
  if (s == -1)
    goto err;

  remotehost = gethostbyname(server);
  if (remotehost == NULL)
    goto close_socket;

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  memcpy(&sa.sin_addr, remotehost->h_addr, remotehost->h_length);

  r = connect(s, (struct sockaddr *) &sa, sizeof(struct sockaddr_in));
  if (r == -1)
    goto close_socket;
#else /* HAVE_IPV6 */
  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  /* convert port from integer to string. */
  snprintf(port_str, sizeof(port_str), "%d", port);

  if (getaddrinfo(server, port_str, &hints, &res) != 0)
    goto err;

  for (ai = res; ai != NULL; ai = ai->ai_next) {
    s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (s == -1)
      continue;
    
    r = connect(s, ai->ai_addr, ai->ai_addrlen);
    if (r == 0)
      break;
  }
  
  if (res != NULL)
    freeaddrinfo(res);

  if (ai == NULL)
    goto err;
#endif

  return s;
  
#ifndef HAVE_IPV6
 close_socket:
#ifdef WIN32
  closesocket(s);
#else
  close(s);
#endif
#endif
 err:
  return -1;
}
