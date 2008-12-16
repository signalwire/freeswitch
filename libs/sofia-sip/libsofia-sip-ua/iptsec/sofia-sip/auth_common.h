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

#ifndef AUTH_COMMON_H
/** Defined when <sofia-sip/auth_common.h> has been included. */
#define AUTH_COMMON_H

/**@file sofia-sip/auth_common.h
 *
 * Functions common for client/server.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri May 19 15:54:08 EEST 2006 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

SOFIA_BEGIN_DECLS

SOFIAPUBFUN issize_t auth_get_params(su_home_t *home,
				     char const * const params[], ...
 			          /* char const * name,
				     char const **return_value */);

SOFIAPUBFUN int auth_struct_copy(void *dst, void const *src, isize_t s_size);

SOFIAPUBFUN int auth_strcmp(char const *quoted, char const *unquoted);

SOFIA_END_DECLS

#endif
