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

#ifndef TEST_PROXY_H
#define TEST_PROXY_H

#include <sofia-sip/su_wait.h>
#include <sofia-sip/nta.h>

SOFIA_BEGIN_DECLS

struct proxy;
struct domain;

struct proxy *test_proxy_create(su_root_t *, tag_type_t, tag_value_t, ...);

void test_proxy_destroy(struct proxy *);

url_t const *test_proxy_uri(struct proxy const *);

char const *test_proxy_route_uri(struct proxy const *p,
				 sip_route_t const **return_route);

struct domain *test_proxy_add_domain(struct proxy *,
				     url_t const *domain,
				     tag_type_t, tag_value_t, ...);

void test_proxy_set_logging(struct proxy *, int logging);

void test_proxy_domain_set_expiration(struct domain *,
				      sip_time_t min_expires,
				      sip_time_t expires,
				      sip_time_t max_expires);

void test_proxy_domain_get_expiration(struct domain *,
				      sip_time_t *return_min_expires,
				      sip_time_t *return_expires,
				      sip_time_t *return_max_expires);

void test_proxy_set_session_timer(struct proxy *p,
				  sip_time_t session_expires,
				  sip_time_t min_se);

void test_proxy_get_session_timer(struct proxy *p,
				  sip_time_t *return_session_expires,
				  sip_time_t *return_min_se);

int test_proxy_domain_set_authorize(struct domain *, char const *realm);
int test_proxy_domain_get_authorize(struct domain *,
				    char const **return_realm);

void test_proxy_domain_set_outbound(struct domain *d,
				    int use_outbound);
void test_proxy_domain_get_outbound(struct domain *d,
				    int *return_use_outbound);

void test_proxy_domain_set_record_route(struct domain *d,
					int use_record_route);
void test_proxy_domain_get_record_route(struct domain *d,
					int *return_use_record_route);

int test_proxy_close_tports(struct proxy *p);

SOFIA_END_DECLS

#endif
