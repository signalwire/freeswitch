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

#ifndef SU_TAG_IO_H
/** Defined when <sofia-sip/su_tag_io.h> has been included */
#define SU_TAG_IO_H

/**@SU_TAG
 * @file sofia-sip/su_tag_io.h
 * @brief I/O interface for tag lists
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 21 12:10:06 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif
#ifndef SU_H
#include <sofia-sip/su.h>
#endif

#include <stdio.h>

SOFIA_BEGIN_DECLS

SOFIAPUBFUN void tl_print(FILE *f, char const *title, tagi_t const lst[]);

#if SU_INLINE_TAG_CAST
su_inline tag_value_t tag_socket_v(su_socket_t v) {
  return (tag_value_t)v;
}
su_inline tag_value_t tag_socket_vr(su_socket_t *vp) {
  return (tag_value_t)vp;
}
#else
#define tag_socket_v(v)   (tag_value_t)(v)
#define tag_socket_vr(v)  (tag_value_t)(v)
#endif

SOFIA_END_DECLS

#endif
