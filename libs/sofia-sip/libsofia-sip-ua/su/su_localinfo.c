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

/**@ingroup su_socket
 * @CFILE su_localinfo.c
 *
 * Obtain list of local addresses.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Wed Oct  4 14:09:29 EET 2000 ppessi
 */

#include "config.h"

#if HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include <sofia-sip/su.h>
#include <sofia-sip/su_localinfo.h>
#include <sofia-sip/su_string.h>
#include "su_module_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <sys/types.h>

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#if HAVE_NET_IF_TYPES_H
#include <net/if_types.h>
#endif

#if SU_LOCALINFO_TEST

#undef USE_LOCALINFO0

#elif HAVE_GETIFADDRS

#define USE_LOCALINFO0 1
#define localinfo0 bsd_localinfo
static int bsd_localinfo(su_localinfo_t const *, su_localinfo_t **);

#elif HAVE_IPHLPAPI_H

#include <iphlpapi.h>
#define USE_LOCALINFO0 1
#define localinfo0 win_localinfo
static int win_localinfo(su_localinfo_t const *, su_localinfo_t **);

#else
/* No localinfo0(), use localinfo4() and localinfo6() */

#undef USE_LOCALINFO0
static int localinfo4(su_localinfo_t const *, su_localinfo_t **);
#  if SU_HAVE_IN6
static int localinfo6(su_localinfo_t const *, su_localinfo_t **);
#  endif

#endif

static int li_scope4(uint32_t ip4);
#if SU_HAVE_IN6
static int li_scope6(struct in6_addr const *ip6);
#endif

#if !SU_LOCALINFO_TEST

static int li_name(su_localinfo_t const*, int, su_sockaddr_t const*, char **);
static void li_sort(su_localinfo_t *i, su_localinfo_t **rresult);

/** @brief Request local address information.
 *
 * Gather the network interfaces and the addresses corresponding to them,
 * check if they match to the search criteria specifed by @a hints and
 * return a list of matching local address information in the @a
 * return_localinfo. The local address information may include IPv4/IPv6
 * addresses, interface name, interface index, address scope, and domain
 * names corresponding to the local addresses.
 *
 * @param[in] hints specifies selection criteria
 * @param[out] return_localinfo   return list of local addresses
 *
 * @par Selection criteria - hints
 *
 * The selection criteria @a hints is used to select which addresses are
 * returned and what kind of information is included in the @a res list.
 *
 * @par Selection by flags - hints->li_flags
 *
 * The @a hints->li_flags contain flags, which can be combined with bit-wise
 * or.  The currently defined flags are as follows:
 *
 * - #LI_V4MAPPED: when returning IPv4 addresses, map them as IPv6
 *   addresses.  If this flag is specified, IPv4 addresses are returned even
 *   if @a hints->li_family is set to @c AF_INET6.
 * - #LI_CANONNAME: return the domain name (DNS PTR) corresponding to the
 *   local address in @a li_canonname.
 * - #LI_NAMEREQD: Do not return addresses not in DNS.
 * - #LI_NUMERIC: instead of domain name, return the text presentation of
 *   the addresss in @a li_canonname.
 * - #LI_DOWN: include interfaces and their addresses even if the interfaces
 *   are down. New in @VERSION_1_12_2.
 * - #LI_IFNAME: return the interface name in @a li_ifname.
 *
 * @par Selection by address family - hints->li_family
 *
 * The address family can have three values: 0, AF_INET and AF_INET6.  If
 * address family @a hints->li_family, both IPv4 and IPv6 addresses are
 * returned.
 *
 * @par Selection by interface index - hints->li_index
 *
 * If the field @a hints->li_index is non-zero, only the addresses assigned
 * to the interface with given index are returned.  The list of interface
 * indices and names can be obtained by the function @c su_if_names().
 *
 * @par Selection by interface name - hints->li_ifname
 *
 * If the field @a hints->li_ifname is not NULL, only the addresses assigned
 * to the named interface are returned.  The list of interface names can be
 * obtained by the function @c su_if_names().
 *
 * @par Selection by address scope - hints->li_scope
 *
 * If the field @a hints->li_scope is nonzero, only the addresses with
 * matching scope are returned. The different address scopes can be combined
 * with bitwise or. They are defined as follows
 * - #LI_SCOPE_HOST: host-local address, valid within host (::1, 127.0.0.1/8)
 * - #LI_SCOPE_LINK: link-local address, valid within link
 *   (IP6 addresses with prefix fe80::/10,
 *    IP4 addresses in net 169.254.0.0/16).
 * - #LI_SCOPE_SITE: site-local address, addresses valid within organization
 *   (IPv6 addresses with prefix  fec::/10,
 *    private IPv4 addresses in nets 10.0.0.0/8, 172.16.0.0/12,
 *    and 192.168.0.0/16 as defined in @RFC1918)
 * - #LI_SCOPE_GLOBAL: global address.
 *
 * For instance, setting @a hints->li_scope to @c LI_SCOPE_GLOBAL | @c
 * LI_SCOPE_SITE, both the @e global and @e site-local addresses are
 * returned.
 *
 * @sa @RFC1918, @RFC4291, su_sockaddr_scope()
 *
 * @par Selection by domain name - hints->li_canonname
 *
 * If this field is non-null, the domain name (DNS PTR) corresponding to
 * local IP addresses should match to the name given in this field.
 *
 * @return Zero (#ELI_NOERROR) when successful, or negative error code when
 * failed.
 *
 * @par Diagnostics
 * Use su_gli_strerror() in order to obtain a string describing the error
 * code returned by su_getlocalinfo().
 *
 */
int su_getlocalinfo(su_localinfo_t const *hints,
		    su_localinfo_t **return_localinfo)
{
  int error = 0, ip4 = 0, ip6 = 0;
  su_localinfo_t *result = NULL, **rr = &result;
  su_localinfo_t hh[1] = {{ 0 }};

  assert(return_localinfo);

  *return_localinfo = NULL;

  if (hints) {
    /* Copy hints so that it can be modified */
    *hh = *hints;
    if (hh->li_canonname)
      hh->li_flags |= LI_CANONNAME;
#if 0
    /* hints->li_ifname is used to select by interface,
       li_ifname is returned with LI_IFNAME flag
    */
    if ((hh->li_flags & LI_IFNAME) && hh->li_ifname == NULL)
      return ELI_BADHINTS;
#endif
  }

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

#if USE_LOCALINFO0
  error = localinfo0(hh, rr);
#else

#  if SU_HAVE_IN6
  if (ip6) {
    error = localinfo6(hh, rr);
    if (error == ELI_NOADDRESS && ip4)
      error = 0;

    if (!error)
      /* Search end of list */
      for (; *rr; rr = &(*rr)->li_next)
	;
  }
#  endif
  if (ip4 && !error) {
    /* Append IPv4 addresses */
    error = localinfo4(hh, rr);
  }
#endif

  if (!result)
    error = ELI_NOADDRESS;

  if (!error)
    li_sort(result, return_localinfo);
  else
    su_freelocalinfo(result);

  return error;
}

#endif

/** Free local address information.
 *
 * Free a list of su_localinfo_t structures obtained with su_getlocalinfo()
 * or su_copylocalinfo() along with socket addresses and strings associated
 * with them.
 *
 * @sa su_getlocalinfo(), su_copylocalinfo(), #su_localinfo_t
 */
void su_freelocalinfo(su_localinfo_t *tbf)
{
  su_localinfo_t *li;

  for (li = tbf; li; li = tbf) {
    tbf = li->li_next;
    if (li->li_canonname)
      free(li->li_canonname);
    free(li);
  }
}

/** Describe su_localinfo errors.
 *
 * The function su_gli_strerror() returns a string describing the error
 * condition indicated by the code that was returned by the function
 * su_getlocalinfo().
 *
 * @param error error code returned by su_getlocalinfo()
 *
 * @return
 * A pointer to string describing the error condition.
 */
char const *su_gli_strerror(int error)
{
  switch (error) {
  case ELI_NOERROR:   return "No error";
  case ELI_NOADDRESS: return "No matching address";
  case ELI_MEMORY:    return "Memory allocation error";
  case ELI_FAMILY:    return "Unknown address family";
  case ELI_RESOLVER:  return "Error when resolving address";
  case ELI_SYSTEM:    return "System error";
  case ELI_BADHINTS:  return "Invalid value for hints";
  default:            return "Unknown error";
  }
}

/** Duplicate su_localinfo structure.
 */
su_localinfo_t *su_copylocalinfo(su_localinfo_t const *li0)
{
  size_t n;
  su_localinfo_t *li, *retval = NULL, **lli = &retval;

# define SLEN(s) ((s) ? strlen(s) + 1 : 0)

  for (; li0 ; li0 = li0->li_next) {
    n = sizeof(*li0) + li0->li_addrlen + SLEN(li0->li_ifname);
    if (!(li = calloc(1, n))) {
      su_freelocalinfo(retval);
      return NULL;
    }
    *lli = li;
    lli = &li->li_next;
    li->li_flags = li0->li_flags;
    li->li_family = li0->li_family;
    li->li_index = li0->li_index;
    li->li_scope = li0->li_scope;
    li->li_addrlen = li0->li_addrlen;
    li->li_addr = memcpy(li + 1, li0->li_addr, li0->li_addrlen);

    if (li0->li_canonname) {
      if (!(li->li_canonname = malloc(SLEN(li0->li_canonname)))) {
	su_freelocalinfo(retval);
	return NULL;
      }
      strcpy(li->li_canonname, li0->li_canonname);
    }

    if (li0->li_ifname)
      li->li_ifname = strcpy(li->li_addrlen + (char *)li->li_addr,
			     li0->li_ifname);
  }

  return retval;
}


/** Return IPv4 address scope */
static int
li_scope4(uint32_t ip4)
{
  ip4 = ntohl(ip4);

  if (0x7f000000 == (ip4 & 0xff000000))
    return LI_SCOPE_HOST;
  /* draft-ietf-zeroconf-ipv4-linklocal-02.txt - 169.254/16. */
  else if (0xa9fe0000 == (ip4 & 0xffff0000))
    return LI_SCOPE_LINK;
  /* RFC1918 - 10/8, 172.16/12, 192.168/16. */
  else if (0x0a000000 == (ip4 & 0xff000000) ||
	   0xac100000 == (ip4 & 0xfff00000) ||
	   0xc0a80000 == (ip4 & 0xffff0000))
    return LI_SCOPE_SITE;
  else
    return LI_SCOPE_GLOBAL;
}

#if SU_HAVE_IN6

#if HAVE_WINSOCK2_H
#define IN6_IS_ADDR_LOOPBACK SU_IN6_IS_ADDR_LOOPBACK
su_inline int
IN6_IS_ADDR_LOOPBACK(void const *ip6)
{
  uint8_t const *u = ip6;

  return
    u[0] == 0 && u[1] == 0 && u[2] == 0 && u[3] == 0 &&
    u[4] == 0 && u[5] == 0 && u[6] == 0 && u[7] == 0 &&
    u[8] == 0 && u[9] == 0 && u[10] == 0 && u[11] == 0 &&
    u[12] == 0 && u[13] == 0 && u[14] == 0 && u[15] == 1;
}
#endif

/** Return IPv6 address scope */
static int
li_scope6(struct in6_addr const *ip6)
{
  if (IN6_IS_ADDR_V4MAPPED(ip6) || IN6_IS_ADDR_V4COMPAT(ip6)) {
    uint32_t ip4 = *(uint32_t *)(ip6->s6_addr + 12);
    return li_scope4(ip4);
  }
  else if (IN6_IS_ADDR_LOOPBACK(ip6))
    return LI_SCOPE_HOST;
  else if (IN6_IS_ADDR_LINKLOCAL(ip6))
    return LI_SCOPE_LINK;
  else if (IN6_IS_ADDR_SITELOCAL(ip6))
    return LI_SCOPE_SITE;
  else
    return LI_SCOPE_GLOBAL;
}
#endif

/** Return the scope of address in the sockaddr structure */
int su_sockaddr_scope(su_sockaddr_t const *su, socklen_t sulen)
{
  if (sulen >= (sizeof su->su_sin) && su->su_family == AF_INET)
    return li_scope4(su->su_sin.sin_addr.s_addr);

#if SU_HAVE_IN6
  if (sulen >= (sizeof su->su_sin6) && su->su_family == AF_INET6)
    return li_scope6(&su->su_sin6.sin6_addr);
#endif

  return 0;
}

#if HAVE_OPEN_C
extern int su_get_local_ip_addr(su_sockaddr_t *su);
#endif

#if SU_LOCALINFO_TEST

#elif USE_LOCALINFO0
/* no localinfo4 */
#elif HAVE_IFCONF
#if __APPLE_CC__
/** Build a list of local IPv4 addresses and append it to *rresult. */
static
int localinfo4(su_localinfo_t const *hints, su_localinfo_t **rresult)
{
  su_localinfo_t *li = NULL;
  su_sockaddr_t *su;
  int error = ELI_NOADDRESS;
  char *canonname = NULL;
  su_socket_t s;

#if SU_HAVE_IN6
  int su_xtra = (hints->li_flags & LI_V4MAPPED) ? sizeof(*su) : 0;
#else
  int const su_xtra = 0;
#endif

  struct ifconf ifc;
  int numifs;
  char *buffer;
  struct ifreq *ifr, *ifr_next;

  su_sockaddr_t *sa;
  socklen_t salen = sizeof(*sa);
  int scope = 0, gni_flags = 0;

  s = su_socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1) {
    SU_DEBUG_1(("su_localinfo: su_socket failed: %s\n",
		su_strerror(su_errno())));
    return ELI_SYSTEM;
  }


  li = calloc(1, (sizeof *li) + (sizeof *sa));
  sa = (void *)(li + 1);

  error = getsockname(s, (struct sockaddr *) sa, &salen);
  if (error < 0 && errno == SOCKET_ERROR) {
    SU_DEBUG_1(("%s: getsockname() failed: %s\n", __func__,
                su_strerror(su_errno())));
  }

  error = bind(s, (struct sockaddr *) sa, salen);

  if (error < 0) {
    SU_DEBUG_1(("%s: bind() failed: %s\n", __func__,
                su_strerror(su_errno())));
    goto err;
  }

  su_close(s);

  scope = li_scope4(sa->su_sin.sin_addr.s_addr);

  if (scope == LI_SCOPE_HOST || scope == LI_SCOPE_LINK)
    gni_flags = NI_NUMERICHOST;

  if (su_xtra) {
    /* Map IPv4 address to IPv6 address */
    memset(sa, 0, sizeof(*sa));
    sa->su_family = AF_INET6;
    ((int32_t*)&sa->su_sin6.sin6_addr)[3] = sa->su_sin.sin_addr.s_addr;
      ((int32_t*)&sa->su_sin6.sin6_addr)[2] = htonl(0xffff);
  }

  li->li_family = sa->su_family;
  li->li_scope = scope;
  li->li_index = 0;
  li->li_addrlen = su_sockaddr_size(sa);
  li->li_addr = sa;

  if ((error = li_name(hints, gni_flags, sa, &canonname)) < 0)
    goto err;

  if (canonname) {
    if (strchr(canonname, ':') ||
       strspn(canonname, "0123456789.") == strlen(canonname))
	  li->li_flags |= LI_NUMERIC;
  }
  else
    li->li_flags = 0;

  li->li_canonname = canonname;

  canonname = NULL;

  *rresult = li;
  return 0;

err:
  if (canonname) free(canonname);
  if (li) free(li);
  su_close(s);

  return error;
}
#else /* !__APPLE_CC__ */
/** Build a list of local IPv4 addresses and append it to *rresult. */
static
int localinfo4(su_localinfo_t const *hints, su_localinfo_t **rresult)
{
  su_localinfo_t *tbf = NULL, **lli = &tbf;
  su_localinfo_t *li = NULL, *li_first = NULL;
  su_sockaddr_t *su;
  int error = ELI_NOADDRESS;
  char *canonname = NULL;
  su_socket_t s;

#if SU_HAVE_IN6
  int su_xtra = (hints->li_flags & LI_V4MAPPED) ? sizeof(*su) : 0;
#else
  int const su_xtra = 0;
#endif

  struct ifconf ifc;
  int numifs;
  char *buffer;
  struct ifreq *ifr, *ifr_next;

#if HAVE_OPEN_C
    su_sockaddr_t *sa;
    socklen_t salen = sizeof(*sa);
#endif

  s = su_socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1) {
    SU_DEBUG_1(("su_localinfo: su_socket failed: %s\n",
		su_strerror(su_errno())));
    return ELI_SYSTEM;
  }

# if HAVE_IFNUM
  /* Get the list of known IP address from the kernel */
  if (ioctl(s, SIOCGIFNUM, (char *) &numifs) < 0) {
    /* can't get number of interfaces -- fall back */
    SU_DEBUG_1(("su_localinfo: SIOCGIFNUM failed: %s\n",
		su_strerror(su_errno())));
    error = ELI_SYSTEM;
    goto err;
  }

  SU_DEBUG_9(("su_localinfo: %d active interfaces according to SIOCGIFNUM\n",
	      numifs));

  if (numifs < 0)
# endif
    /* Default to 64 interfaces. Enough? */
    numifs = 64;

  if (numifs == 0)
    return 0;

  /*
   * Allocate memory for SIOCGIFCONF ioctl buffer. This memory block is also
   * used as li_first, first localinfo struct that is returned, so it can be
   * freed by freelocalinfo() without any complications.
   */
  ifc.ifc_len = numifs * sizeof (struct ifreq);
  buffer = malloc(sizeof(su_localinfo_t) + ifc.ifc_len + su_xtra);
  if (!buffer) {
    SU_DEBUG_1(("su_localinfo: memory exhausted\n"));
    error = ELI_MEMORY;
    goto err;
  }

  li_first = (su_localinfo_t *)buffer;
  memset(li_first, 0, sizeof(su_localinfo_t) + su_xtra);
  ifc.ifc_buf = buffer + sizeof(su_localinfo_t) + su_xtra;
#if HAVE_OPEN_C
  if (ioctl(s, SIOCGIFACTIVECONF, (char *)&ifc) < 0) {
    SU_DEBUG_1(("su_localinfo: SIOCGIFCONF failed: %s\n",
		su_strerror(su_errno())));
    error = ELI_SYSTEM;
    goto err;
  }
#else
  if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
    SU_DEBUG_1(("su_localinfo: SIOCGIFCONF failed: %s\n",
		su_strerror(su_errno())));
    error = ELI_SYSTEM;
    goto err;
  }
#endif

  buffer = ifc.ifc_buf + ifc.ifc_len;

  for (ifr = ifc.ifc_req;
       (void *)ifr < (void *)buffer;
       ifr = ifr_next) {
    struct ifreq ifreq[1];
    int scope, if_index, flags = 0, gni_flags = 0;
    char *if_name;
#if SU_HAVE_IN6
    su_sockaddr_t su2[1];
#endif

#if SA_LEN
    if (ifr->ifr_addr.sa_len > sizeof(ifr->ifr_addr))
      ifr_next = (struct ifreq *)
	(ifr->ifr_addr.sa_len + (char *)(&ifr->ifr_addr));
    else
#else
      ifr_next = ifr + 1;
#endif

    if_name = ifr->ifr_name;

#if defined(SIOCGIFINDEX)
    ifreq[0] = *ifr;
    if (ioctl(s, SIOCGIFINDEX, ifreq) < 0) {
      SU_DEBUG_1(("su_localinfo: SIOCGIFINDEX failed: %s\n",
		  su_strerror(su_errno())));
      error = ELI_SYSTEM;
      goto err;
    }
#if HAVE_IFR_INDEX
    if_index = ifreq->ifr_index;
#elif HAVE_IFR_IFINDEX
    if_index = ifreq->ifr_ifindex;
#else
#error Unknown index field in struct ifreq
#endif

#else
#warning su_localinfo() cannot map interface name to number
    if_index = 0;
#endif

    SU_DEBUG_9(("su_localinfo: if %s with index %d\n", if_name, if_index));


#if HAVE_OPEN_C
    su_close(s);

    li = calloc(1, sizeof(su_localinfo_t));
    sa = calloc(1, sizeof(su_sockaddr_t));

    if (su_get_local_ip_addr(sa) < 0)
      goto err;

    li->li_family = sa->su_family;
    li->li_scope = LI_SCOPE_GLOBAL /* scope */;
    li->li_index = if_index;
    li->li_addrlen = su_sockaddr_size(sa);
    li->li_addr = sa;

    if ((error = li_name(hints, gni_flags, sa, &canonname)) < 0)
      goto err;

    if (canonname) {
      if (strchr(canonname, ':') ||
	  strspn(canonname, "0123456789.") == strlen(canonname))
	    li->li_flags |= LI_NUMERIC;
    }
    else
      li->li_flags = 0;

    li->li_canonname = canonname;

    canonname = NULL;

    *rresult = li;

    return 0;
#endif

#if defined(SIOCGIFFLAGS)
    ifreq[0] = *ifr;
    if (ioctl(s, SIOCGIFFLAGS, ifreq) < 0) {
      SU_DEBUG_1(("su_localinfo: SIOCGIFFLAGS failed: %s\n",
		  su_strerror(su_errno())));
      error = ELI_SYSTEM;
      goto err;
    }
    /* Do not include interfaces that are down unless explicitly asked */
    if ((ifreq->ifr_flags & IFF_UP) == 0 && (hints->li_flags & LI_DOWN) == 0) {
      SU_DEBUG_9(("su_localinfo: if %s with index %d is down\n",
		  if_name, if_index));
      continue;
    }
#elif defined(SIOCGIFACTIVECONF)
/* Handled above in SIOCGIFACTIVECONF vs. SIOCGIFCONF*/
#else
#error su_localinfo() cannot determine interface status
#endif

#if 0
    *ifreq = *ifr;
    ifreq->ifr_addr.sa_family = AF_INET;
    if (ioctl(s, SIOCGIFADDR, ifreq) < 0) {
      SU_DEBUG_1(("su_localinfo: SIOCGIFADDR failed: %s\n",
		  su_strerror(su_errno())));
      error = ELI_SYSTEM;
      goto err;
    }
    ifr->ifr_addr = ifreq->ifr_addr;
#endif

    su = (su_sockaddr_t *)&ifr->ifr_addr;

    if (SU_HAS_INADDR_ANY(su))
      continue;

    scope = li_scope4(su->su_sin.sin_addr.s_addr);

    if ((hints->li_scope && (hints->li_scope & scope) == 0) ||
	(hints->li_ifname && strcmp(hints->li_ifname, if_name) != 0) ||
	(hints->li_index && hints->li_index != if_index))
      continue;

#if SU_HAVE_IN6
    if (su_xtra) {
      /* Map IPv4 address to IPv6 address */
      memset(su2, 0, sizeof(*su2));
      su2->su_family = AF_INET6;
      ((int32_t*)&su2->su_sin6.sin6_addr)[2] = htonl(0xffff);
      ((int32_t*)&su2->su_sin6.sin6_addr)[3] = su->su_sin.sin_addr.s_addr;
      su = su2;
    }
#endif

    if (scope == LI_SCOPE_HOST || scope == LI_SCOPE_LINK)
      gni_flags = NI_NUMERICHOST;

    if ((error = li_name(hints, gni_flags, su, &canonname)) < 0)
      goto err;
    else if (error > 0)
      continue;

    if (canonname)
      if (strchr(canonname, ':') ||
	  strspn(canonname, "0123456789.") == strlen(canonname))
	flags |= LI_NUMERIC;

    if (li_first)
      li = li_first;      /* Use li_first with all ifr structs to be freed */
    else if (!(li = calloc(1, (sizeof *li) + su_xtra))) {
      error = ELI_MEMORY;
      goto err;
    }
    if (!tbf) tbf = li;
    *lli = li; lli = &li->li_next;

    if (su_xtra)
      su = (su_sockaddr_t *)memcpy(li + 1, su, su_xtra);

    li->li_flags = flags;
    li->li_family = su->su_family;
    li->li_scope = scope;
    li->li_index = if_index;
    li->li_addrlen = su_sockaddr_size(su);
    li->li_addr = su;
    li->li_canonname = canonname;
    li->li_ifname = if_name;

    canonname = NULL;
    li_first = NULL;
  }

  if (canonname) free(canonname);
  if (li_first) free(li_first);
  su_close(s);

  if (tbf) *rresult = tbf;
  return 0;

err:
  if (canonname) free(canonname);
  if (li_first) free(li_first);
  su_freelocalinfo(tbf);
  su_close(s);

  return error;
}
#endif /* __APPLE_CC__ */
#else
static
int localinfo4(su_localinfo_t const *hints, su_localinfo_t **rresult)
{
  /* Kikka #3: resolve hostname */
  char hostname[SU_MAXHOST] = "";
  char *name, *ifname;
  struct hostent *h;
  int i, flags, error, gni_flags = 0;
  su_localinfo_t *tbf = NULL;
  su_localinfo_t *li = NULL, **lli = &tbf;
  su_sockaddr_t *su;
#if SU_HAVE_IN6
  socklen_t su_sockaddr_size =
    (hints->li_flags & LI_V4MAPPED) ? sizeof(*su) : sizeof(struct sockaddr_in);
  flags = hints->li_flags & (LI_V4MAPPED|LI_CANONNAME|LI_NUMERIC|LI_IFNAME);
#else
  socklen_t su_sockaddr_size = sizeof(struct sockaddr_in);
  flags = hints->li_flags & (LI_CANONNAME|LI_NUMERIC|LI_IFNAME);
#endif

  error = ELI_NOERROR;

  if (hints->li_scope == 0 || (hints->li_scope & LI_SCOPE_GLOBAL)) {
    if (hints->li_canonname)
      name = hints->li_canonname;
    else if (gethostname(name = hostname, sizeof(hostname)) != 0)
      return ELI_SYSTEM;

    h = gethostbyname(name);

    if (name)
      if (strchr(name, ':') ||
	  strspn(name, "0123456789.") == strlen(name))
	flags |= LI_NUMERIC;

    for (i = 0; h && h->h_addr_list[i]; i++) {
      if ((li = calloc(1, sizeof(*li) + su_sockaddr_size)) == NULL) {
	error = ELI_MEMORY;
	goto err;
      }
      li->li_flags = flags;

      li->li_scope = li_scope4(*(uint32_t *)h->h_addr_list[i]);
      if (li->li_scope == LI_SCOPE_HOST)
	li->li_index = 1, ifname = "lo";
      else
	li->li_index = 2, ifname = "eth";

      li->li_addrlen = su_sockaddr_size;
      li->li_addr = su = (su_sockaddr_t *)(li + 1);
      su->su_family = li->li_family =
	li->li_flags & LI_V4MAPPED ? AF_INET6 : AF_INET;

#if SU_HAVE_IN6
      if (li->li_flags & LI_V4MAPPED) {
	((int32_t*)&su->su_sin6.sin6_addr)[2] = htonl(0xffff);
	memcpy(&((int32_t*)&su->su_sin6.sin6_addr)[3],
	       h->h_addr_list[i], h->h_length);
      }
      else
#endif
	memcpy(&su->su_sin.sin_addr.s_addr, h->h_addr_list[i], h->h_length);
      if (li->li_flags & LI_IFNAME)
	li->li_ifname = ifname;
      if (li->li_scope == LI_SCOPE_HOST || li->li_scope == LI_SCOPE_LINK)
	gni_flags = NI_NUMERICHOST;
      if ((error = li_name(hints, gni_flags, su, &li->li_canonname)) < 0)
	goto err;
      else if (error > 0) {
	free(li); li = NULL; continue;
      } else
	error = ELI_NOADDRESS;
      *lli = li; lli = &li->li_next; li = NULL;
    }
  }

  if (hints->li_scope == 0 || (hints->li_scope & LI_SCOPE_HOST)) {
    if ((li = calloc(1, sizeof(*li) + su_sockaddr_size)) == NULL) {
      error = ELI_MEMORY;
      goto err;
    }
    li->li_flags = hints->li_flags &
      (LI_V4MAPPED|LI_CANONNAME|LI_NUMERIC|LI_IFNAME);
    li->li_scope = LI_SCOPE_HOST, li->li_index = 1;
    if (li->li_flags & LI_IFNAME)
      li->li_ifname = "lo";
    li->li_addrlen = su_sockaddr_size;
    li->li_addr = su = (su_sockaddr_t *)(li + 1);
#if SU_HAVE_IN6
    if (li->li_flags & LI_V4MAPPED) {
      su->su_family = li->li_family = AF_INET6;
      ((int32_t*)&su->su_sin6.sin6_addr)[2] = htonl(0xffff);
      ((int32_t*)&su->su_sin6.sin6_addr)[3] = htonl(0x7f000001);
    }
    else
#endif
      su->su_family = li->li_family = AF_INET,
      su->su_sin.sin_addr.s_addr = htonl(0x7f000001);

    if ((error = li_name(hints, NI_NUMERICHOST, su, &li->li_canonname)) < 0) {
      goto err;
    } else if (error > 0) {
      free(li); li = NULL;
    } else {
      *lli = li; lli = &li->li_next; li = NULL;
    }
  }

  *rresult = tbf;

  return 0;

err:
  if (li) su_freelocalinfo(li);
  su_freelocalinfo(tbf);

  return error;
}

#endif

#if USE_LOCALINFO0 || !SU_HAVE_IN6 || SU_LOCALINFO_TEST
/* No localinfo6() */
#elif HAVE_PROC_NET_IF_INET6
/** Build a list of local IPv6 addresses and append it to *return_result. */
static
int localinfo6(su_localinfo_t const *hints, su_localinfo_t **return_result)
{
  su_localinfo_t *li = NULL;
  su_sockaddr_t su[1] = {{ 0 }}, *addr;
  int error = ELI_NOADDRESS;
  char *canonname = NULL;
  char line[80];
  FILE *f;

  if ((f = fopen("/proc/net/if_inet6", "r"))) {
    for (;error;) {
      struct in6_addr in6;
      unsigned if_index, prefix_len, scope, flags;
      int addrlen, if_namelen;
      char ifname[16];

      if (!fgets(line, sizeof(line), f)) {
	if (feof(f))
	  error = ELI_NOERROR;
	break;
      }

      if (sscanf(line, "%08x%08x%08x%08x %2x %2x %2x %02x %016s\n",
		 &in6.s6_addr32[0],
		 &in6.s6_addr32[1],
		 &in6.s6_addr32[2],
		 &in6.s6_addr32[3],
		 &if_index, &prefix_len, &scope, &flags, ifname) != 9)
	break;

      flags = 0;

      /* Fix global scope (it is 0) */
      if (!scope) scope = LI_SCOPE_GLOBAL;

      in6.s6_addr32[0] = htonl(in6.s6_addr32[0]);
      in6.s6_addr32[1] = htonl(in6.s6_addr32[1]);
      in6.s6_addr32[2] = htonl(in6.s6_addr32[2]);
      in6.s6_addr32[3] = htonl(in6.s6_addr32[3]);

      if (IN6_IS_ADDR_V4MAPPED(&in6) || IN6_IS_ADDR_V4COMPAT(&in6)) {
	uint32_t ip4 = *(uint32_t *)(in6.s6_addr + 12);
	scope = li_scope4(ip4);
      }

      if ((hints->li_scope && (hints->li_scope & scope) == 0) ||
	  (hints->li_index && hints->li_index != if_index) ||
	  (hints->li_ifname && strcmp(hints->li_ifname, ifname) != 0))
	continue;

      su->su_family = AF_INET6;
      su->su_sin6.sin6_addr = in6;

      addrlen = su_sockaddr_size(su);

      if ((error = li_name(hints, 0, su, &canonname)) < 0)
	break;
      else if (error > 0)
	continue;
      else
	error = ELI_NOADDRESS;

      if (canonname &&
	  (strchr(canonname, ':') ||
	   strspn(canonname, "0123456789.") == strlen(canonname)))
	flags |= LI_NUMERIC;

      if (hints->li_flags & LI_IFNAME)
	if_namelen = strlen(ifname) + 1;
      else
	if_namelen = 0;

      if ((li = calloc(1, sizeof *li + addrlen + if_namelen)) == NULL) {
	error = ELI_MEMORY;
	break;
      }
      addr = (su_sockaddr_t*)memcpy((li + 1), su, addrlen);
      *return_result = li; return_result = &li->li_next;

      li->li_flags = flags;
      li->li_family = AF_INET6;
      li->li_scope = scope;
      li->li_index = if_index;
      li->li_addr = addr;
      li->li_addrlen = addrlen;
      li->li_canonname = canonname;
      if (if_namelen)
	li->li_ifname = memcpy(addrlen + (char *)addr, ifname, if_namelen);

      canonname = NULL;
    }

    fclose(f);
  }

  if (canonname)
    free(canonname);

  return error;
}
#else
/* Use HOSTADDR6 */
static
int localinfo6(su_localinfo_t const *hints, su_localinfo_t **rresult)
{
  char *addr, *ifname;
  int flags, error;
  su_localinfo_t *li = NULL;
  su_sockaddr_t *su;
  int const su_sockaddr_size = sizeof(*su);

  error = ELI_NOADDRESS;

#if defined(__APPLE_CC__)
  {
    su_sockaddr_t *sa;
    int salen = sizeof(*sa);
    int s;

    if (hints->li_scope == 0 || (hints->li_scope & LI_SCOPE_GLOBAL)) {
      if ((addr = getenv("HOSTADDR6"))) {

	li = calloc(1, sizeof(su_localinfo_t));
	sa = calloc(1, sizeof(su_sockaddr_t));

	sa->su_family = AF_INET6;
	if (su_inet_pton(AF_INET6, addr, &sa->su_sin6.sin6_addr) <= 0)
	  goto err;

	s = su_socket(AF_INET6, SOCK_DGRAM, 0);
	if (s == -1) {
	  SU_DEBUG_1(("su_localinfo: su_socket failed: %s\n",
		      su_strerror(su_errno())));
	  return ELI_SYSTEM;
	}

	error = getsockname(s, (struct sockaddr *) sa, &salen);
	if (error < 0 && errno == SOCKET_ERROR) {
	  SU_DEBUG_1(("%s: getsockname() failed: %s\n", __func__,
		      su_strerror(su_errno())));
	}

	error = bind(s, (struct sockaddr *) sa, salen);

	if (error < 0) {
	  SU_DEBUG_1(("%s: bind() failed: %s\n", __func__,
		      su_strerror(su_errno())));
	  goto err;
	}

	su_close(s);

	li->li_family = sa->su_family;
	li->li_scope = LI_SCOPE_GLOBAL;
	li->li_index = 0;
	li->li_addrlen = su_sockaddr_size(sa);
	li->li_addr = sa;

	if ((error = li_name(hints, NI_NUMERICHOST, sa, &li->li_canonname)) < 0)
	  goto err;

	li->li_flags = NI_NUMERICHOST;
      }
    }

    *rresult = li;
    return 0;
  }
#endif


  if (hints->li_scope == 0 || (hints->li_scope & LI_SCOPE_GLOBAL)) {
    if ((addr = getenv("HOSTADDR6"))) {
      flags = hints->li_flags & (LI_CANONNAME|LI_NUMERIC|LI_IFNAME);

      if ((li = calloc(1, sizeof(*li) + su_sockaddr_size)) == NULL) {
	error = ELI_MEMORY;
	goto err;
      }
      li->li_flags = flags;
      li->li_scope = LI_SCOPE_GLOBAL, li->li_index = 2, ifname = "eth";
      li->li_addrlen = sizeof(*su);
      li->li_addr = su = (su_sockaddr_t *)(li + 1);
      su->su_family = li->li_family = AF_INET6;
      if (su_inet_pton(AF_INET6, addr, &su->su_sin6.sin6_addr) <= 0)
	goto err;
      if (li->li_flags & LI_IFNAME)
	li->li_ifname = ifname;
      if ((error = li_name(hints, NI_NUMERICHOST, su, &li->li_canonname)) < 0)
	goto err;
      else if (error > 0) {
	free(li); li = NULL;
      }
    }
  }

  *rresult = li;

  return 0;

err:
  if (li) su_freelocalinfo(li);
  return error;
}
#endif

#if !USE_LOCALINFO0
/* no localinfo0() or bsd_localinfo() */
#elif HAVE_GETIFADDRS

#include <ifaddrs.h>

static
int bsd_localinfo(su_localinfo_t const hints[1],
		  su_localinfo_t **return_result)
{
  struct ifaddrs *ifa, *results;
  int error = 0;
#if SU_HAVE_IN6
  int v4_mapped = (hints->li_flags & LI_V4MAPPED) != 0;
#endif
  char *canonname = NULL;

  if (getifaddrs(&results) < 0) {
    if (errno == ENOMEM)
      return ELI_MEMORY;
    else
      return ELI_SYSTEM;
  }

  for (ifa = results; ifa; ifa = ifa->ifa_next) {
    su_localinfo_t *li;
    su_sockaddr_t *su;
#if SU_HAVE_IN6
    su_sockaddr_t su2[1];
#endif
    socklen_t sulen;
    int scope, flags = 0, gni_flags = 0, if_index = 0;
    char const *ifname = 0;
    size_t ifnamelen = 0;

    /* no ip address from if that is down */
    if ((ifa->ifa_flags & IFF_UP) == 0 && (hints->li_flags & LI_DOWN) == 0)
      continue;

    su = (su_sockaddr_t *)ifa->ifa_addr;

    if (!su)
      continue;

    if (su->su_family == AF_INET) {
      sulen = sizeof(su->su_sin);
      scope = li_scope4(su->su_sin.sin_addr.s_addr);
#if SU_HAVE_IN6
      if (v4_mapped)
	sulen = sizeof(su->su_sin6);
#endif
    }
#if SU_HAVE_IN6
    else if (su->su_family == AF_INET6) {
      if (IN6_IS_ADDR_MULTICAST(&su->su_sin6.sin6_addr))
	continue;
      sulen = sizeof(su->su_sin6);
      scope = li_scope6(&su->su_sin6.sin6_addr);
    }
#endif
    else
      continue;

    if (hints->li_flags & LI_IFNAME) {
      ifname = ifa->ifa_name;
      if (ifname)
	ifnamelen = strlen(ifname) + 1;
    }

    if ((hints->li_scope && (hints->li_scope & scope) == 0) ||
	(hints->li_family && hints->li_family != su->su_family) ||
	(hints->li_ifname && (!ifname || strcmp(hints->li_ifname, ifname))) ||
	(hints->li_index && hints->li_index != if_index))
      continue;

    if (scope == LI_SCOPE_HOST || scope == LI_SCOPE_LINK)
      gni_flags = NI_NUMERICHOST;

#if SU_HAVE_IN6
    if (v4_mapped && su->su_family == AF_INET) {
      /* Map IPv4 address to IPv6 address */
      memset(su2, 0, sizeof(*su2));
      su2->su_family = AF_INET6;
      ((int32_t*)&su2->su_sin6.sin6_addr)[2] = htonl(0xffff);
      ((int32_t*)&su2->su_sin6.sin6_addr)[3] = su->su_sin.sin_addr.s_addr;
      su = su2;
    }
#endif

    if ((error = li_name(hints, gni_flags, su, &canonname)) < 0)
      break;

    if (error > 0) {
      error = 0;
      continue;
    }

    if (canonname)
      if (strchr(canonname, ':') ||
	  canonname[strspn(canonname, "0123456789.")] == '\0')
	flags |= LI_NUMERIC;

    if (!(li = calloc(1, sizeof(*li) + sulen + ifnamelen))) {
      SU_DEBUG_1(("su_getlocalinfo: memory exhausted\n"));
      error = ELI_MEMORY;
      break;
    }
    *return_result = li, return_result = &li->li_next;

    li->li_flags = flags;
    li->li_family = su->su_family;
    li->li_scope = scope;
    li->li_index = if_index;
    li->li_addrlen = sulen;
    li->li_addr = memcpy(li + 1, su, sulen);
    li->li_canonname = canonname;
    if (ifnamelen) {
      li->li_ifname = strcpy((char *)(li + 1) + sulen, ifname);
    }

    canonname = NULL;
  }

  if (canonname)
    free(canonname);

  freeifaddrs(results);

  return error;
}

#elif USE_LOCALINFO0 && HAVE_IPHLPAPI_H && SU_HAVE_IN6

static
char const *ws2ifname(DWORD iftype)
{
  switch (iftype) {
  case IF_TYPE_ETHERNET_CSMACD:         return "eth";
  case IF_TYPE_IEEE80212:               return "eth";
  case IF_TYPE_FASTETHER:               return "eth";
  case IF_TYPE_GIGABITETHERNET:         return "eth";
  case IF_TYPE_ISO88025_TOKENRING:      return "token";
  case IF_TYPE_FDDI:                    return "fddi";
  case IF_TYPE_PPP:                     return "ppp";
  case IF_TYPE_SOFTWARE_LOOPBACK:       return "lo";
  case IF_TYPE_SLIP:                    return "sl";
  case IF_TYPE_FRAMERELAY:              return "fr";
  case IF_TYPE_ATM:                     return "atm";
  case IF_TYPE_HIPPI:                   return "hippi";
  case IF_TYPE_ISDN:                    return "isdn";
  case IF_TYPE_IEEE80211:               return "wlan";
  case IF_TYPE_ADSL:                    return "adsl";
  case IF_TYPE_RADSL:                   return "radsl";
  case IF_TYPE_SDSL:                    return "sdsl";
  case IF_TYPE_VDSL:                    return "vdsl";
  case IF_TYPE_TUNNEL:                  return "tunnel";
  case IF_TYPE_IEEE1394:                return "fw";
  case IF_TYPE_OTHER:
  default:                              return "other";
  }
}

static
int win_localinfo(su_localinfo_t const hints[1], su_localinfo_t **rresult)
{
  /* This is Windows XP code, for both IPv6 and IPv4. */
  ULONG iaa_size = 2048;
  IP_ADAPTER_ADDRESSES *iaa0, *iaa;
  int error, loopback_seen = 0;
  int v4_mapped = (hints->li_flags & LI_V4MAPPED) != 0;
  char *canonname = NULL;
  su_localinfo_t *li, **next;
  int flags = GAA_FLAG_SKIP_MULTICAST;
  *rresult = NULL; next = rresult;

  iaa0 = malloc((size_t)iaa_size);
  if (!iaa0) {
    SU_DEBUG_1(("su_localinfo: memory exhausted\n"));
    error = ELI_MEMORY;
    goto err;
  }
  error = GetAdaptersAddresses(hints->li_family, flags, NULL, iaa0, &iaa_size);
  if (error == ERROR_BUFFER_OVERFLOW) {
    if ((iaa0 = realloc(iaa0, iaa_size)))
      error = GetAdaptersAddresses(hints->li_family, flags, NULL, iaa0, &iaa_size);
  }
  if (error) {
    char const *empty = "";
    LPTSTR msg = empty;

    if (error == ERROR_NO_DATA) {
      error = ELI_NOADDRESS;
      goto err;
    }

    if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	               FORMAT_MESSAGE_FROM_SYSTEM |
	               FORMAT_MESSAGE_IGNORE_INSERTS,
	  	       NULL,
	  	       error,
		       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		       msg, 0, NULL))
      msg = empty;

    SU_DEBUG_1(("su_localinfo: GetAdaptersAddresses: %s (%d)\n", msg, error));
    if (msg != empty) LocalFree((LPVOID)msg);
    error = ELI_SYSTEM;
    goto err;
  }

  for (iaa = iaa0; iaa; iaa = iaa->Next) {
    IP_ADAPTER_UNICAST_ADDRESS *ua;
    IP_ADAPTER_UNICAST_ADDRESS lua[1];
    int if_index = iaa->IfIndex;
    size_t ifnamelen = 0;
    char ifname[16];

    for (ua = iaa->FirstUnicastAddress; ;ua = ua->Next) {
      su_sockaddr_t *su;
      socklen_t sulen;
      su_sockaddr_t su2[1];
      int scope, flags = 0, gni_flags = 0;

      if (ua == NULL) {
	/* There is no loopback interface in windows */
	if (!loopback_seen && iaa->Next == NULL) {
	  struct sockaddr_in loopback_sin = { AF_INET, 0, {{ 127, 0, 0, 1 }}};

	  lua->Address.lpSockaddr = (struct sockaddr *)&loopback_sin;
	  lua->Address.iSockaddrLength = sizeof(loopback_sin);
	  lua->Next = NULL;

	  iaa->IfType = IF_TYPE_SOFTWARE_LOOPBACK;
	  if_index = 1;

	  ua = lua;
	}
	else
	  break;
      }

      su = (su_sockaddr_t *)ua->Address.lpSockaddr;
      sulen = ua->Address.iSockaddrLength;

      if (su->su_family == AF_INET) {
	scope = li_scope4(su->su_sin.sin_addr.s_addr);
	if (v4_mapped)
	  sulen = sizeof(su->su_sin6);
	if (scope == LI_SCOPE_HOST)
	  loopback_seen = 1;
      }
      else if (su->su_family == AF_INET6) {
	if (IN6_IS_ADDR_MULTICAST(&su->su_sin6.sin6_addr))
	  continue;
	scope = li_scope6(&su->su_sin6.sin6_addr);
      }
      else
	continue;

      if (hints->li_flags & LI_IFNAME) {
	snprintf(ifname, sizeof(ifname), "%s%u",
		 ws2ifname(iaa->IfType), if_index);
	ifnamelen = strlen(ifname) + 1;
      }

      if ((hints->li_scope && (hints->li_scope & scope) == 0) ||
	  (hints->li_family && hints->li_family != su->su_family) ||
	  /* (hints->li_ifname && strcmp(hints->li_ifname, ifname) != 0) || */
	  (hints->li_index && hints->li_index != if_index))
	continue;

      if (scope == LI_SCOPE_HOST || scope == LI_SCOPE_LINK)
	gni_flags = NI_NUMERICHOST;

      if (v4_mapped && su->su_family == AF_INET) {
	/* Map IPv4 address to IPv6 address */
	memset(su2, 0, sizeof(*su2));
	su2->su_family = AF_INET6;
	((int32_t*)&su2->su_sin6.sin6_addr)[2] = htonl(0xffff);
	((int32_t*)&su2->su_sin6.sin6_addr)[3] = su->su_sin.sin_addr.s_addr;
	su = su2;
      }

      if ((error = li_name(hints, gni_flags, su, &canonname)) < 0)
	goto err;
      else if (error > 0)
	continue;

      if (canonname)
	if (strchr(canonname, ':') ||
	    strspn(canonname, "0123456789.") == strlen(canonname))
	  flags |= LI_NUMERIC;

      if (!(li = calloc(1, sizeof(*li) + sulen + ifnamelen))) {
	SU_DEBUG_1(("su_getlocalinfo: memory exhausted\n"));
	error = ELI_MEMORY; goto err;
      }
      *next = li, next = &li->li_next;

      li->li_flags = flags;
      li->li_family = su->su_family;
      li->li_scope = scope;
      li->li_index = if_index;
      li->li_addrlen = sulen;
      li->li_addr = memcpy(li + 1, su, sulen);
      li->li_canonname = canonname;
      if (ifnamelen) {
	li->li_ifname = strcpy((char *)(li + 1) + sulen, ifname);
	/* WideCharToMultiByte(CP_ACP, 0,
			    ifname, -1, (char *)(li + 1) + sulen, ifnamelen,
			    NULL, NULL); */
      }

      canonname = NULL;
    }
  }

  if (iaa0) free(iaa0);
  return 0;

err:
  if (iaa0) free(iaa0);
  if (canonname) free(canonname);
  su_freelocalinfo(*rresult), *rresult = NULL;
  return error;
}

#elif HAVE_SIO_ADDRESS_LIST_QUERY

static
int localinfo0(su_localinfo_t const *hints, su_localinfo_t **rresult)
{
  /* This is Windows IPv4 code */
  short family = AF_INET;
  su_socket_t s;
  union {
    SOCKET_ADDRESS_LIST sal[1];
#if HAVE_INTERFACE_INFO_EX
    INTERFACE_INFO_EX   ii[1];
#else
    INTERFACE_INFO      ii[1];
#endif
    char buffer[2048];
  } b = {{ 1 }};
  DWORD salen = sizeof(b);
  int i, error = -1;
#if SU_HAVE_IN6
  int v4_mapped = (hints->li_flags & LI_V4MAPPED) != 0;
#endif
  su_localinfo_t *li, *head = NULL, **next = &head;
  char *canonname = NULL, *if_name = NULL;

  *rresult = NULL;

  s = su_socket(family, SOCK_DGRAM, 0);
  if (s == INVALID_SOCKET) {
    SU_DEBUG_1(("su_getlocalinfo: %s: %s\n", "su_socket",
		            su_strerror(su_errno())));
    return -1;
  }

  /* get the list of known IP address (NT5 and up) */
  if (WSAIoctl(s, SIO_ADDRESS_LIST_QUERY, NULL, 0,
               &b, sizeof(b), &salen, NULL, NULL) == SOCKET_ERROR) {
    SU_DEBUG_1(("su_getlocalinfo: %s: %s\n", "SIO_ADDRESS_LIST_QUERY",
		su_strerror(su_errno())));
    error = -1; goto err;
  }
  if (b.sal->iAddressCount < 1) {
    SU_DEBUG_1(("su_getlocalinfo: no local addresses\n"));
    error = -1; goto err;
  }

  for (i = 0; i < b.sal->iAddressCount; i++) {
    su_sockaddr_t *su = (su_sockaddr_t *)b.sal->Address[i].lpSockaddr;
#if SU_HAVE_IN6
    socklen_t sulen = v4_mapped ? sizeof(*su) : b.sal->Address[i].iSockaddrLength;
    su_sockaddr_t su2[1];
#else
    socklen_t sulen = b.sal->Address[i].iSockaddrLength;
#endif
    int scope, flags = 0, gni_flags = 0;

    scope = li_scope4(su->su_sin.sin_addr.s_addr);

    if (hints->li_scope && (hints->li_scope & scope) == 0)
      continue;

    if (scope == LI_SCOPE_HOST || scope == LI_SCOPE_LINK)
      gni_flags = NI_NUMERICHOST;

    if (!(li = calloc(1, sizeof(*li) + sulen))) {
      SU_DEBUG_1(("su_getlocalinfo: memory exhausted\n"));
      error = -1; goto err;
    }
    *next = li, next = &li->li_next;

#if SU_HAVE_IN6
    if (v4_mapped) {
      /* Map IPv4 address to IPv6 address */
      memset(su2, 0, sizeof(*su2));
      su2->su_family = AF_INET6;
      ((int32_t*)&su2->su_sin6.sin6_addr)[2] = htonl(0xffff);
      ((int32_t*)&su2->su_sin6.sin6_addr)[3] = su->su_sin.sin_addr.s_addr;
      su = su2;
    }
#endif

    if ((error = li_name(hints, gni_flags, su, &canonname)) < 0)
      goto err;
    else if (error > 0)
      continue;

    if (canonname)
      if (strchr(canonname, ':') ||
	  strspn(canonname, "0123456789.") == strlen(canonname))
	flags |= LI_NUMERIC;

    li->li_flags = flags;
    li->li_family = su->su_family;
    li->li_scope = scope;
    li->li_index = i;
    li->li_addrlen = su_sockaddr_size(su);
    li->li_addr = su;
    li->li_canonname = canonname, canonname = NULL;
    if (hints->li_flags & LI_IFNAME)
      li->li_ifname = if_name;
    li->li_addr = (su_sockaddr_t *)(li + 1);
    li->li_addrlen = sulen;
    memcpy(li->li_addr, su, sulen);
  }

  *rresult = head;
  su_close(s);

  return 0;

err:
  if (canonname) free(canonname);
  su_freelocalinfo(head);
  su_close(s);

  return error;
}
#endif

#if !SU_LOCALINFO_TEST

static
int li_name(su_localinfo_t const *hints,
	    int gni_flags,
	    su_sockaddr_t const *su,
	    char **ccanonname)
{
  char name[SU_MAXHOST];
  int error;
  int flags = hints->li_flags;

  *ccanonname = NULL;

  if ((flags & LI_CANONNAME) || hints->li_canonname) {
    if ((flags & LI_NAMEREQD) == LI_NAMEREQD)
      gni_flags |= NI_NAMEREQD;
    if (flags & LI_NUMERIC)
      gni_flags |= NI_NUMERICHOST;

    error = su_getnameinfo(su, su_sockaddr_size(su),
			   name, sizeof(name), NULL, 0,
			   gni_flags);
    if (error) {
      if ((flags & LI_NAMEREQD) == LI_NAMEREQD)
	return 1;
      SU_DEBUG_7(("li_name: getnameinfo() failed\n"));
      if (!su_inet_ntop(su->su_family, SU_ADDR(su), name, sizeof name))
	return ELI_RESOLVER;
    }

    if (hints->li_canonname && !su_casematch(name, hints->li_canonname))
      return 1;

    if (!(flags & LI_CANONNAME))
      return 0;

    if (!(*ccanonname = strdup(name)))
      return ELI_MEMORY;
  }
  return 0;
}

static
void li_sort(su_localinfo_t *i, su_localinfo_t **rresult)
{
  su_localinfo_t *li, **lli;

#define LI_MAPPED(li) \
  ((li)->li_family == AF_INET6 &&					\
   (IN6_IS_ADDR_V4MAPPED(&(li)->li_addr->su_sin6.sin6_addr) ||		\
    IN6_IS_ADDR_V4COMPAT(&(li)->li_addr->su_sin6.sin6_addr)))

  /* Sort addresses according to scope (and mappedness) */
  for (li = i; li; li = i) {
    i = li->li_next;
    for (lli = rresult; *lli; lli = &(*lli)->li_next) {
      if ((*lli)->li_scope < li->li_scope)
	break;
#if SU_HAVE_IN6
      if (LI_MAPPED(*lli) > LI_MAPPED(li))
	break;
#endif
    }
    li->li_next = *lli;
    *lli = li;
  }
}

#endif

/**Get local IP address.
 *
 * @deprecated
 * Use su_getlocalinfo() instead.
 */
int su_getlocalip(su_sockaddr_t *sa)
{
  su_localinfo_t *li = NULL, hints[1] = {{ 0 }};

  hints->li_family = sa->su_sa.sa_family ? sa->su_sa.sa_family : AF_INET;

  if (su_getlocalinfo(hints, &li) == 0) {
    memcpy(sa, li->li_addr, li->li_addrlen);
    su_freelocalinfo(li);
    return 0;
  }
  else
    return -1;
}
