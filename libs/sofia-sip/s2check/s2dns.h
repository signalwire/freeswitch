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

#ifndef S2DNS_H
/** Defined when <s2dns.h> has been included. */
#define S2DNS_H

/**@internal @file s2dns.h
 *
 * @brief Internal DNS server for testing
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */

#include <sofia-sip/su_wait.h>

SOFIA_BEGIN_DECLS

void s2_dns_setup(su_root_t *root);
void s2_dns_set_filter(int (*filter)(void *data, size_t len, void *userdata),
		       void *userdata);
void s2_dns_teardown(void);

char const *s2_dns_default(char const *domain);

extern uint32_t s2_dns_ttl;

void s2_dns_domain(char const *domain, int use_naptr,
		   /* char *prefix, int priority, url_t const *uri, */
		   ...);

void s2_dns_record(char const *domain, unsigned qtype,
		   /* unsigned atype, domain, */
		   ...);

SOFIA_END_DECLS

#endif /* S2DNS_H */
