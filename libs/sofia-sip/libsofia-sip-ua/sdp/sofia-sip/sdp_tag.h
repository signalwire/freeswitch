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

#ifndef SDP_TAG_H
/** Defined when <sofia-sip/sdp_tag.h> has been included. */
#define SDP_TAG_H

/**@file sofia-sip/sdp_tag.h
 * @brief SDP tags
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon May 12 12:06:20 EEST 2003 ppessi
 * @{
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif
#ifndef SU_TAG_CLASS_H
#include <sofia-sip/su_tag_class.h>
#endif

SOFIA_BEGIN_DECLS

struct sdp_session_s;

/** Filter tag matching any sdp tag. */
#define SDPTAG_ANY()         sdptag_any, ((tag_value_t)0)
SDP_DLL extern tag_typedef_t sdptag_any;

/* Tags for parameters */

SDP_DLL extern tag_typedef_t sdptag_session;
/** SDP session description. @HI */
#define SDPTAG_SESSION(x) \
sdptag_session, sdptag_session_v((x))

SDP_DLL extern tag_typedef_t sdptag_session_ref;
#define SDPTAG_SESSION_REF(x) \
sdptag_session_ref, sdptag_session_vr(&(x))

/* Functions for typesafe parameter passing */

#if SU_HAVE_INLINE
su_inline
tag_value_t sdptag_session_v(struct sdp_session_s const *v) {
  return (tag_value_t)v;
}
su_inline
tag_value_t sdptag_session_vr(struct sdp_session_s const **vp) {
  return (tag_value_t)vp;
}
#else
#define sdptag_session_v(v)   (tag_value_t)(v)
#define sdptag_session_vr(vp) (tag_value_t)(vp)
#endif

/* Tag classes */

extern tag_class_t sdptag_session_class[];

#define SDPTAG_TYPEDEF(name) \
  {{ TAG_NAMESPACE, #name, sdptag_session_class }}

SOFIA_END_DECLS

#endif /* !defined(SDP_TAG_H) */
