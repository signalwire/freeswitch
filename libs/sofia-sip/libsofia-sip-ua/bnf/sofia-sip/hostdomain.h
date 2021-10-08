/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

#ifndef SOFIA_SIP_HOSTDOMAIN_H
/** Defined when <sofia-sip/hostdomain.h> has been included. */
#define SOFIA_SIP_HOSTDOMAIN_H

/**@file sofia-sip/hostdomain.h
 *
 * Predicates for handling host names: IP addresses or domain names.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Mar  9 16:15:22 EET 2006 ppessi
 */

#ifndef SU_CONFIG_H
#include <sofia-sip/su_config.h>
#endif

SOFIA_BEGIN_DECLS

SOFIAPUBFUN int host_is_ip4_address(char const *string);
SOFIAPUBFUN int host_is_ip6_address(char const *string);
SOFIAPUBFUN int host_is_ip6_reference(char const *string);
SOFIAPUBFUN int host_is_ip_address(char const *string);
SOFIAPUBFUN int host_is_domain(char const *string);
SOFIAPUBFUN int host_is_valid(char const *string);
SOFIAPUBFUN int host_is_local(char const *string);
SOFIAPUBFUN int host_has_domain_invalid(char const *string);
SOFIAPUBFUN int host_cmp(char const *a, char const *b);

/** This is typo. @deprecated Use host_is_ip6_reference() instead. */
SOFIAPUBFUN int host_ip6_reference(char const *string);


SOFIA_END_DECLS

#endif /* !defined(SOFIA_SIP_HOSTDOMAIN_H) */
