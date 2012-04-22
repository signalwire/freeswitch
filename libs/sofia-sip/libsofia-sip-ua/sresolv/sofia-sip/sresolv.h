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

#ifndef SRESOLV_H
/** Defined when <sofia-sip/sresolv.h> has been included. */
#define SRESOLV_H

/**
 * @file sofia-sip/sresolv.h Easy API for Sofia DNS Resolver.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>,
 * @author Teemu Jalava <Teemu.Jalava@nokia.com>,
 * @author Mikko Haataja <ext-Mikko.A.Haataja@nokia.com>.
 *
 */

#include <sofia-sip/su.h>
#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_tag.h>

#include <sofia-resolv/sres.h>
#include <sofia-resolv/sres_record.h>
#include <sofia-resolv/sres_async.h>

SOFIA_BEGIN_DECLS

/* Sofia-specific reactor interface for asynchronous operation */

/** Filter tag matching any sresolv tag. */
#define SRESOLVTAG_ANY()         srestag_any, ((tag_value_t)0)
SOFIAPUBVAR tag_typedef_t srestag_any;

SOFIAPUBVAR tag_typedef_t srestag_resolv_conf;
/** Path of resolv.conf file. */
#define SRESTAG_RESOLV_CONF(x) srestag_resolv_conf, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t srestag_resolv_conf_ref;
#define SRESTAG_RESOLV_CONF_REF(x) srestag_resolv_conf_ref, tag_str_vr(&(x))

SOFIAPUBVAR tag_typedef_t srestag_cache;
/** Pointer to existing #sres_cache_t object. */
#define SRESTAG_CACHE(x) srestag_cache, tag_ptr_v((x))
SOFIAPUBVAR tag_typedef_t srestag_cache_ref;
#define SRESTAG_CACHE_REF(x) srestag_cache_ref, tag_ptr_vr(&(x), (x))

/** Create a resolver object using @a root reactor. */
SOFIAPUBFUN sres_resolver_t *sres_resolver_create(su_root_t *root,
						  char const *resolv_conf,
				                  tag_type_t, tag_value_t,
						  ...);
/** Destroy a resolver object. */
SOFIAPUBFUN int sres_resolver_destroy(sres_resolver_t *res);

/* Return socket used by root. @deprecated */
SOFIAPUBFUN su_socket_t sres_resolver_root_socket(sres_resolver_t *res);

SOFIA_END_DECLS

#endif
