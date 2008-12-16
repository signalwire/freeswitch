/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005,2007 Nokia Corporation.
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

/**@page addrinfo Resolve network services
 *
 * @section synopsis Synopsis
 *
 * <tt>addrinfo [-pcn46] service-name host</tt>
 *
 * @section description Description
 *
 * The @em addrinfo utility will use su_getaddrinfo() to resolve the network
 * services and print resolved names. See sect 6.1 of RFC3493 and the getaddrinfo(3)
 * manual page of POSIX 1003.1g, for more information.
 *
 * @section options Options
 *
 * The @e addrinfo utility accepts following ccommand line options:
 * <dl>
 * <dt>-p</dt>
 * <dd>use passive open.</dd>
 * <dt>-c</dt>
 * <dd>get canonic name.</dd>
 * <dt>-n</dt>
 * <dd>use numeric host names.</dd>
 * <dt>-4</dt>
 * <dd>IPv4 only.</dd>
 * <dt>-6</dt>
 * <dd>IPv6 only (but including mapped IPv4 addresses).</dd>
 * </dl>
 *
 * @section bugs Reporting Bugs
 * Report bugs to <sofia-sip-devel@lists.sourceforge.net>.
 *
 * @section author Author
 * Written by Pekka Pessi <pekka -dot pessi -at- nokia -dot- com>
 *
 * @section copyright Copyright
 * Copyright (C) 2005,2007 Nokia Corporation.
 *
 * This program is free software; see the source for copying conditions.
 * There is @b NO warranty; not even for @b MERCHANTABILITY or <b>FITNESS
 * FOR A PARTICULAR PURPOSE</b>.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sofia-sip/su.h"

char const help[] =
"usage: addrinfo [-pnc46] <servicename> <domainname>\n"
"\t-p query for passive open\n"
"\t-n use numeric host names\n"
"\t-c ask for canonic names\n"
"\t-4 IPv4 only\n"
"\t-6 IPv6 only (but including mapped IPv4 addresses)\n"
;

int getopt(int argc, char * const argv[], char const *opstring);
extern int optind;

static void usage(void)
{
  fputs(help, stderr);
  exit(1);
}

int main(int argc, char *argv[])
{
#if SU_HAVE_IN6
  char buffer[INET6_ADDRSTRLEN];
#else
  char buffer[20];
#endif
  su_addrinfo_t hints[1] = {{ 0 }};
  su_addrinfo_t *ai, *res = NULL;
  char const *host, *service;
  int error;

  for (;;) {
    switch(getopt(argc, argv, "ndp4c")) {
    case '4': hints->ai_family = AF_INET; break;
#if SU_HAVE_IN6
    case '6': hints->ai_family = AF_INET6; break;
#endif
    case 'p': hints->ai_flags |= AI_PASSIVE; break;
    case 'n': hints->ai_flags |= AI_NUMERICHOST; break;
    case 'c': hints->ai_flags |= AI_CANONNAME; break;

    case -1:
      goto main;

    default:
      usage();
    }
  }

 main:
  if (optind + 1 >= argc)
    usage();

  service = argv[optind++];
  host = argv[optind++];

  su_init();

  if ((error = su_getaddrinfo(host, service, hints, &res)) == 0) {
    for (ai = res; ai; ai = ai->ai_next) {
      su_sockaddr_t const *su = (su_sockaddr_t const *)ai->ai_addr;
      unsigned port;

#if SU_HAVE_IN6
      if (su->su_family != AF_INET6 && su->su_family != AF_INET)
	continue;
#else
      if (su->su_family != AF_INET)
	continue;
#endif

      port = ntohs(su->su_port);
      su_inet_ntop(ai->ai_family, SU_ADDR(su), buffer, sizeof(buffer));
      printf("%d@[%s]:%u", ai->ai_protocol, buffer, port);

      if (ai->ai_flags & AI_CANONNAME)
        printf(" canon=%s", ai->ai_canonname);

      puts("");
    }
    su_freeaddrinfo(res);
  }
  else {
    fprintf(stderr, "addrinfo: %s\n", su_gai_strerror(error));
    error = 1;
  }

  su_deinit();

  return error;
}
