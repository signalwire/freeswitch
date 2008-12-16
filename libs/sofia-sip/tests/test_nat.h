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

#ifndef TEST_NAT_H
#define TEST_NAT_H

#include <sofia-sip/su_wait.h>
#include <sofia-sip/nta.h>

SOFIA_BEGIN_DECLS

struct nat;
struct nat_filter;

struct nat *test_nat_create(su_root_t *, int family,
			    tag_type_t, tag_value_t, ...);

void test_nat_destroy(struct nat *);

int test_nat_private(struct nat *nat, void *address, socklen_t *return_addrlen);
int test_nat_public(struct nat *nat, void const *address, int addrlen);

int test_nat_flush(struct nat *nat);

struct nat_filter *test_nat_add_filter(struct nat *nat,
				       size_t (*condition)(void *arg,
							   void *message,
							   size_t len),
				       void *arg,
				       int outbound);

enum { nat_inbound, nat_outbound };

int test_nat_remove_filter(struct nat *nat,
			   struct nat_filter *filter);

/* Tags */

/** If true, act as symmetric nat. */
#define TESTNATTAG_SYMMETRIC(x) testnattag_symmetric, tag_bool_v((x))
#define TESTNATTAG_SYMMETRIC_REF(x) testnattag_symmetric_ref, tag_bool_vr(&(x))
extern tag_typedef_t testnattag_symmetric;
extern tag_typedef_t testnattag_symmetric_ref;

/** If true, print information about connections. */
#define TESTNATTAG_LOGGING(x) testnattag_logging, tag_bool_v((x))
#define TESTNATTAG_LOGGING_REF(x) testnattag_logging_ref, tag_bool_vr(&(x))
extern tag_typedef_t testnattag_logging;
extern tag_typedef_t testnattag_logging_ref;

SOFIA_END_DECLS

#endif
