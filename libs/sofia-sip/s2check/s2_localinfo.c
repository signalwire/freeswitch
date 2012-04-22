/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2009 Nokia Corporation.
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

/**@CFILE s2_localinfo.c
 *
 * @brief s2_localinfo() stub returning well-known addresses for testing.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */

#include "config.h"

#include <sofia-sip/hostdomain.h>
#include <sofia-sip/su_string.h>
#include <sofia-sip/su_alloc.h>

#include <stdlib.h>
#include <assert.h>

#define SU_LOCALINFO_TEST 1

#define su_getlocalinfo s2_getlocalinfo
#define su_freelocalinfo s2_freelocalinfo
#define su_gli_strerror s2_gli_strerror
#define su_copylocalinfo s2_copylocalinfo
#define su_sockaddr_scope s2_sockaddr_scope
#define su_getlocalip s2_getlocalip

#include "su_localinfo.c"

static char const *default_ifaces[] = {
  "lo0\0" "127.0.0.1\0" "::1\0",
  NULL
};

static char const **ifaces = default_ifaces;

void s2_localinfo_ifaces(char const **replace_ifaces)
{
  ifaces = replace_ifaces;
}

int
s2_getlocalinfo(su_localinfo_t const *hints,
		su_localinfo_t **return_localinfo)
{
  int error = 0, ip4 = 0, ip6 = 0, i;
  su_localinfo_t *result = NULL, **rr = &result;
  su_localinfo_t hh[1] = {{ 0 }};

  assert(return_localinfo);

  *return_localinfo = NULL;

  if (hints) {
    /* Copy hints so that it can be modified */
    *hh = *hints;
    if (hh->li_canonname)
      hh->li_flags |= LI_CANONNAME;
  }

  hints = hh;

  switch (hh->li_family) {
#if SU_HAVE_IN6
  case AF_INET6:
    if (hh->li_flags & LI_V4MAPPED)
      ip6 = ip4 = 1, hh->li_family = 0;
    else
      ip6 = 1;
    break;
#endif

  case AF_INET:
    ip4 = 1;
    break;

  case 0:
    ip6 = ip4 = 1;
    break;

  default:
    return -1;
  }

  for (i = 0; ifaces[i]; i++) {
    char const *iface = ifaces[i], *address = iface;
    su_sockaddr_t su[1];

    for (;(address += strlen(address) + 1)[0];) {
      su_localinfo_t *li = NULL;
      int scope = 0;

      memset(su, 0, sizeof su);

      if (0)
	;
#if SU_HAVE_IN6
      else if (ip4 &&
	       (hints->li_flags & LI_V4MAPPED) != 0 &&
	       host_is_ip4_address(address)) {
	int32_t mapped;
	su->su_len = (sizeof su->su_sin6);
	su_inet_pton(su->su_family = AF_INET6,
		     address,
		     &mapped);
	((int32_t*)&su->su_sin6.sin6_addr)[2] = htonl(0xffff);
	((int32_t*)&su->su_sin6.sin6_addr)[3] = mapped;
	scope = li_scope4(mapped);
      }
      else if (ip6 && host_is_ip6_address(address)) {
	su->su_len = (sizeof su->su_sin6);
	su_inet_pton(su->su_family = AF_INET6,
		     address,
		     &su->su_sin6.sin6_addr);
	scope = li_scope6(&su->su_sin6.sin6_addr);
      }
#endif
      else if (ip4 && host_is_ip4_address(address)) {
	su->su_len = (sizeof su->su_sin);
	su_inet_pton(su->su_family = AF_INET,
		     address,
		     &su->su_sin.sin_addr);
	scope = li_scope4(su->su_sin.sin_addr.s_addr);
      }
      else
	continue;

      if (scope == 0)
	continue;
      if (hints->li_scope && (hints->li_scope & scope) == 0)
	continue;
      if (hints->li_index && hints->li_index != i + 1)
	continue;
      if (hints->li_ifname && strcmp(hints->li_ifname, iface) != 0)
	continue;
      if (hints->li_family && hints->li_family != su->su_family)
	continue;
      if (hints->li_canonname && !su_casematch(address, hints->li_canonname))
	continue;

      li = calloc(1, (sizeof *li) + (sizeof *su) + strlen(iface) + 1);
      li->li_family = su->su_family;
      li->li_scope = scope;
      li->li_index = i + 1;
      li->li_addrlen = su_sockaddr_size(su);
      li->li_addr = memcpy((li + 1), su, (sizeof *su));
      if (hints->li_flags & LI_IFNAME)
	li->li_ifname = strcpy((char *)li->li_addr + li->li_addrlen, iface);

      if ((hints->li_flags & LI_CANONNAME) || hints->li_canonname) {
	li->li_flags |= LI_NUMERIC;
	li->li_canonname = su_strdup(NULL, address);
      }

#define LI_MAPPED(li) \
  ((li)->li_family == AF_INET6 &&					\
   (IN6_IS_ADDR_V4MAPPED(&(li)->li_addr->su_sin6.sin6_addr) ||		\
    IN6_IS_ADDR_V4COMPAT(&(li)->li_addr->su_sin6.sin6_addr)))

      /* Insert according to scope, mappedness and family */
      for (rr = &result; *rr; rr = &(*rr)->li_next) {
	if ((*rr)->li_scope < li->li_scope)
	  break;
#if SU_HAVE_IN6
	if (LI_MAPPED(*rr) > LI_MAPPED(li))
	  break;
#endif
	if ((*rr)->li_family < li->li_family)
	  break;
      }
      li->li_next = *rr;
      *rr = li;
    }
  }

  *return_localinfo = result;

  if (result == NULL)
    error = ELI_NOADDRESS;

  return error;
}

