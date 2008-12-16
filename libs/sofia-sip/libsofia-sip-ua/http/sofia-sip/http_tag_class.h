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

#ifndef HTTP_TAG_CLASS_H
/**Defined when http_tag_class.h have been included*/
#define HTTP_TAG_CLASS_H

/**@file sofia-sip/http_tag_class.h
 * @brief Tag classes for HTTP headers.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 21 11:01:45 2001 ppessi
 */

#ifndef SU_TAG_CLASS_H
#include <sofia-sip/su_tag_class.h>
#endif

#ifndef MSG_TAG_CLASS_H
#include <sofia-sip/msg_tag_class.h>
#endif

SOFIA_BEGIN_DECLS

/** Define a named tag type for HTTP header @a t. */
#define HTTPHDRTAG_NAMED_TYPEDEF(n, t) \
{{ TAG_NAMESPACE, #n, httphdrtag_class, \
  (tag_value_t)http_##t##_class }}

/** Define a tag type for HTTP header @a t. */
#define HTTPHDRTAG_TYPEDEF(t) HTTPHDRTAG_NAMED_TYPEDEF(t, t)

/** Define a string tag type for HTTP header @a t. */
#define HTTPSTRTAG_TYPEDEF(t) \
{{ TAG_NAMESPACE, #t "_str", httpstrtag_class, \
  (tag_value_t)http_##t##_class }}

/** Define a tag type for HTTP message @a t. */
#define HTTPMSGTAG_TYPEDEF(t) \
  {{ TAG_NAMESPACE, #t, httpmsgtag_class, \
     (tag_value_t)HTTP_PROTOCOL_TAG }}

/**@internal Filter HTTP header tag items. */
SOFIAPUBFUN tagi_t *httptag_filter(tagi_t *dst, tagi_t const f[],
				   tagi_t const *src,
				   void **bb);

SOFIA_END_DECLS

#endif /* !defined(HTTP_TAG_CLASS_H) */
