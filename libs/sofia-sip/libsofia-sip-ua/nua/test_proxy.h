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

struct proxy *test_proxy_create(su_root_t *, tag_type_t, tag_value_t, ...);

void test_proxy_destroy(struct proxy *);

url_t const *test_proxy_uri(struct proxy const *);

void test_proxy_set_expiration(struct proxy *,
			       sip_time_t min_expires, 
			       sip_time_t expires, 
			       sip_time_t max_expires);

void test_proxy_get_expiration(struct proxy *,
			       sip_time_t *return_min_expires, 
			       sip_time_t *return_expires, 
			       sip_time_t *return_max_expires);

SOFIA_END_DECLS

#endif
