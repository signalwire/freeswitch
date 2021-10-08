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

#ifndef SU_LOCALINFO_H
/** Defined when <sofia-sip/su_localinfo.h> has been included. */
#define SU_LOCALINFO_H


/**@ingroup su_socket
 * @file sofia-sip/su_localinfo.h  Interface for obtaining local addresses.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Aug 10 18:58:01 2000 ppessi
 */

#ifndef SU_H
#include <sofia-sip/su.h>
#endif

SOFIA_BEGIN_DECLS

/** Local address info structure. */
typedef struct su_localinfo_s su_localinfo_t;

/** Local address info structure. */
struct su_localinfo_s {
  /**Bitwise or of flags:
   * #LI_V4MAPPED, #LI_CANONNAME, #LI_NAMEREQD,
   * #LI_NUMERIC, #LI_DOWN, #LI_IFNAME */
  int li_flags;
  int li_family;		/**< Address family. */
  int li_index;			/**< Network interface index. */
  int li_scope;			/**< Address scope. */
  socklen_t li_addrlen;		/**< Length of li_addr. */
  su_sockaddr_t *li_addr;	/**< Binary address. */
  char *li_canonname;		/**< Canonical name for address. */
  su_localinfo_t *li_next;	/**< Next structure in linked list. */
  char *li_ifname;		/**< Network interface name. */
};

/** Flags for su_getlocalinfo() - li_flags */
enum {
  /** IPv4 addresses will be mapped as IPv6 */
  LI_V4MAPPED     = 1,
  /** Get domain name corresponding to the local address */
  LI_CANONNAME    = 2,
  /** Do not return addresses not in DNS. Implies LI_CANONNAME. */
  LI_NAMEREQD     = 6,
  /** Instead of domain name, use numeric form */
  LI_NUMERIC      = 8,
  /** Include interfaces even if they are down (new in @VERSION_1_12_2). */
  LI_DOWN         = 16,
  /** Get interface name */
  LI_IFNAME       = 256
};

/** Localinfo scope - li_scope */
enum {
  LI_SCOPE_HOST   = 0x10,	/**< Host-local address, valid within host. */
  LI_SCOPE_LINK   = 0x20,	/**< Link-local address. */
  LI_SCOPE_SITE   = 0x40,	/**< Site-local address. */
  LI_SCOPE_GLOBAL = 0x80	/**< Global address. */
};

/** Localinfo error codes. */
enum {
  ELI_NOADDRESS  = -1,		/**< No matching address. */
  ELI_MEMORY     = -2,		/**< Memory allocation error. */
  ELI_FAMILY     = -3,		/**< Unknown address family. */
  ELI_RESOLVER   = -4,		/**< Error when resolving address. */
  ELI_SYSTEM     = -5,		/**< System error. */
  ELI_BADHINTS   = -6,		/**< Invalid value for hints. */
  ELI_NOERROR    = 0		/**< No error. */
};

/** Request local address information */
SOFIAPUBFUN int su_getlocalinfo(su_localinfo_t const *hints,
				su_localinfo_t **res);
/** Free local address information */
SOFIAPUBFUN void su_freelocalinfo(su_localinfo_t *);
/** Describe su_localinfo errors. */
SOFIAPUBFUN char const *su_gli_strerror(int error);
/** Copy a localinfo structure */
SOFIAPUBFUN su_localinfo_t *su_copylocalinfo(su_localinfo_t const *li0);

/** Return the scope of address in the sockaddr structure */
SOFIAPUBFUN int su_sockaddr_scope(su_sockaddr_t const *su, socklen_t sulen);

SOFIA_END_DECLS

#endif /* !defined(SU_LOCALINFO_H) */
