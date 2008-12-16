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

#ifndef MSG_TAG_CLASS_H
/** Defined when <sofia-sip/msg_tag_class.h> has been included */
#define MSG_TAG_CLASS_H

/**@file sofia-sip/msg_tag_class.h
 * @brief Functions for constructing per-protocol tag classes.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 21 11:01:45 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

SOFIA_BEGIN_DECLS

SOFIAPUBFUN int msghdrtag_snprintf(tagi_t const *t, char b[], size_t size);
SOFIAPUBFUN size_t msghdrtag_xtra(tagi_t const *t, size_t offset);
SOFIAPUBFUN tagi_t *msghdrtag_dup(tagi_t *dst, tagi_t const *src,
				  void **inout_buffer);
SOFIAPUBFUN int msghdrtag_scan(tag_type_t tt, su_home_t *home,
			       char const *s,
			       tag_value_t *return_value);
SOFIAPUBFUN tagi_t *msghdrtag_filter(tagi_t *dst, tagi_t const f[],
				     tagi_t const *src,
				     void **inout_buffer);

SOFIAPUBFUN tagi_t *msgstrtag_filter(tagi_t *dst, tagi_t const f[],
				     tagi_t const *src,
				     void **inout_buffer);

SOFIAPUBFUN int msgobjtag_snprintf(tagi_t const *t, char b[], size_t size);
SOFIAPUBFUN size_t msgobjtag_xtra(tagi_t const *t, size_t offset);
SOFIAPUBFUN tagi_t *msgobjtag_dup(tagi_t *dst, tagi_t const *src,
				  void **inout_buffer);

SOFIA_END_DECLS

#endif /** !defined(MSG_TAG_CLASS_H) */
