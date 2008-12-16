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

#ifndef SIP_TAG_CLASS_H
/** Defined when <sip_tag_class.h> have been included */
#define SIP_TAG_CLASS_H


/**@SIP_TAG @{ */
/**@file sofia-sip/sip_tag_class.h
 *
 * @brief Tag classes for SIP headers.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
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

/** Define a named tag type for SIP header @a t. */
#define SIPHDRTAG_NAMED_TYPEDEF(n, t) \
{{ TAG_NAMESPACE, #n, siphdrtag_class, \
  (tag_value_t)sip_##t##_class }}

/** Define a tag type for SIP header @a t. @HIDE */
#define SIPHDRTAG_TYPEDEF(t) SIPHDRTAG_NAMED_TYPEDEF(t, t)

/** Define a string tag type for SIP header @a t. @HIDE */
#define SIPSTRTAG_TYPEDEF(t) \
{{ TAG_NAMESPACE, #t "_str", sipstrtag_class, \
  (tag_value_t)sip_##t##_class }}

/** Define a tag type for SIP message @a t. @HIDE */
#define SIPMSGTAG_TYPEDEF(t) \
  {{ TAG_NAMESPACE, #t, sipmsgtag_class, \
     (tag_value_t)SIP_PROTOCOL_TAG }}

/** Tag class for SIP headers */
SOFIAPUBVAR tag_class_t siphdrtag_class[1];
/** Tag class for string values of SIP headers */
SOFIAPUBVAR tag_class_t sipstrtag_class[1];
/** Tag class for SIP message */
SOFIAPUBVAR tag_class_t sipmsgtag_class[1];

/** Define a named tag type using structure of SIP header @a t. */
#define SIPEXTHDRTAG_TYPEDEF(n, t) \
{{ TAG_NAMESPACE, #n, sipexthdrtag_class, \
  (tag_value_t)sip_##t##_class }}

/** Tag class using SIP header structure */
SOFIAPUBVAR tag_class_t sipexthdrtag_class[1];


/**@internal Filter SIP header tag items. */
SOFIAPUBFUN tagi_t *siptag_filter(tagi_t *dst, tagi_t const f[],
				  tagi_t const *src,
				  void **bb);

SOFIA_END_DECLS

#endif
