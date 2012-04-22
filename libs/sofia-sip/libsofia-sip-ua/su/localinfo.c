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

/**@page localinfo list local network addresses
 *
 * @section synopsis Synopsis
 *
 * <tt>localinfo [-imdn46gslh] [domainname]</tt>
 *
 * @section description Description
 *
 * The @em localinfo utility will obtain the local network addresses
 * and print them.
 *
 * @section options Options
 *
 * The @e localinfo utility accepts following command line options:
 * <dl>
 * <dt>-i</dt>
 * <dd>include interface name.</dd>
 * <dt>-m</dt>
 * <dd>map IPv4 addresses.</dd>
 * <dt>-d</dt>
 * <dd>require a reverse DNS entry.</dd>
 * <dt>-n</dt>
 * <dd>use numeric host names.</dd>
 * <dt>-4</dt>
 * <dd>IPv4 only.</dd>
 * <dt>-6</dt>
 * <dd>IPv6 only (but including mapped IPv4 addresses).</dd>
 * <dt>-g</dt>
 * <dd>Global addresses.</dd>
 * <dt>-s</dt>
 * <dd>Site-level addresses.</dd>
 * <dt>-l</dt>
 * <dd>Link-level addresses.</dd>
 * <dt>-h</dt>
 * <dd>Host-internal addresses.</dd>
 * </dl>
 *
 * @section examples Examples
 *
 * You want to find out local IPv6 addresses:
 * @code
 * $ localinfo -6
 * @endcode
 * You want to find out to link-local addresses
 * @code
 * $ localinfo -l -n
 * @endcode
 *
 * @section bugs Reporting Bugs
 * Report bugs to <sofia-sip-devel@lists.sourceforge.net>.
 *
 * @section author Author
 * Written by Pekka Pessi <pekka -dot pessi -at- nokia -dot- com>
 *
 * @section copyright Copyright
 * Copyright (C) 2005 Nokia Corporation.
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
#include "sofia-sip/su_localinfo.h"
#include "su_module_debug.h"

char const help[] =
"usage: localinfo [-imdn46gslh] [domainname]\n"
"\t-i include interface name\n"
"\t-m map IPv4 addresses\n"
"\t-d require DNS entry\n"
"\t-n use numeric host names\n"
"\t-4 IPv4 only\n"
"\t-6 IPv6 only (but including mapped IPv4 addresses)\n"
"\t-g Global addresses\n"
"\t-s Site-level addresses\n"
"\t-l Link-level addresses\n"
"\t-h Host-internal addresses\n";

int getopt(int argc, char * const argv[], char const *opstring);
extern int optind;

static void usage(int returncode)
{
  fputs(help, stderr);
  exit(returncode);
}

int main(int argc, char *argv[])
{
  char buffer[SU_ADDRSIZE];
  su_localinfo_t hints[1] = {{ LI_CANONNAME }};
  su_localinfo_t *li, *res = NULL;
  int error;
  int ifindex = 0;

  if (argv[1] && strcmp(argv[1], "--help") == 0)
    usage(0);

  for (;;) {
    switch(getopt(argc, argv, "iImdn46gslh")) {

    case 'I': ifindex = 1; break;
    case 'i': hints->li_flags |= LI_IFNAME; ifindex = 1; break;
    case 'm': hints->li_flags |= LI_V4MAPPED; break;
    case '4': hints->li_family = AF_INET; break;
#if SU_HAVE_IN6
    case '6': hints->li_family = AF_INET6; break;
#endif
    case 'd': hints->li_flags |= LI_NAMEREQD; break;
    case 'n': hints->li_flags |= LI_NUMERIC; break;
    case 'g': hints->li_scope |= LI_SCOPE_GLOBAL; break;
    case 's': hints->li_scope |= LI_SCOPE_SITE; break;
    case 'l': hints->li_scope |= LI_SCOPE_LINK; break;
    case 'h': hints->li_scope |= LI_SCOPE_HOST; break;

    case -1:
      goto main;

    default:
      usage(1);
    }
  }

 main:
  if (optind < argc)
    hints->li_canonname = argv[optind++];

  if (optind < argc)
    usage(1);

  su_init();

  if ((error = su_getlocalinfo(hints, &res)) == 0) {
    for (li = res; li; li = li->li_next) {
      if (li->li_flags & LI_NUMERIC) {
        fputs(li->li_canonname, stdout);
      }
      else {
        su_inet_ntop(li->li_family, SU_ADDR(li->li_addr),
		     buffer, sizeof(buffer));
        printf("%s maddr=[%s]", li->li_canonname, buffer);
      }
      if (li->li_scope & LI_SCOPE_GLOBAL)
	fputs(" scope=global", stdout);
      else if (li->li_scope & LI_SCOPE_SITE)
	fputs(" scope=site", stdout);
      else if (li->li_scope & LI_SCOPE_LINK)
	fputs(" scope=link", stdout);
      else if (li->li_scope & LI_SCOPE_HOST)
	fputs(" scope=host", stdout);
      if (ifindex) {
	if (li->li_ifname)
	  printf(" if[%d]=%s", li->li_index, li->li_ifname);
	else
	  printf(" if[%d]", li->li_index);
      }
      puts("");
    }
    su_freelocalinfo(res);
  }
  else {
    fprintf(stderr, "localinfo: %s\n", su_gli_strerror(error));
    error = 1;
  }

  su_deinit();

  return error;
}
