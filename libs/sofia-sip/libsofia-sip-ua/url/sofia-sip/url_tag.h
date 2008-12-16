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

#ifndef URL_TAG_H
/** Defined when <sofia-sip/url_tag.h> has been included. */
#define URL_TAG_H
/**@file  url_tag.h
 * @brief Tags for URLs
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 21 11:01:45 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

#ifndef URL_H
#define URL_H
#include <sofia-sip/url.h>		/* Include only types */
#undef URL_H
#endif

SOFIA_BEGIN_DECLS

/** Filter tag matching any url tag. */
#define URLTAG_ANY()         urltag_any, ((tag_value_t)0)
SOFIAPUBVAR tag_typedef_t urltag_any;

SOFIAPUBVAR tag_typedef_t urltag_url;

/** Tag list item for an URL. */
#define URLTAG_URL(u)      urltag_url, urltag_url_v(u)

SOFIAPUBVAR tag_typedef_t urltag_url_ref;

#define URLTAG_URL_REF(u)  urltag_url_ref, urltag_url_vr(&(u))

#if SU_HAVE_INLINE
su_inline
tag_value_t urltag_url_v(void const *v) { return (tag_value_t)v; }
su_inline
tag_value_t urltag_url_vr(url_string_t const **vp) { return(tag_value_t)vp; }
#else
#define urltag_url_v(v)   (tag_value_t)(v)
#define urltag_url_vr(vr) (tag_value_t)(vr)
#endif


SOFIA_END_DECLS
#endif

