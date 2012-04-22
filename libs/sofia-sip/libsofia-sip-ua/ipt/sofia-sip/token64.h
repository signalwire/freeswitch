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

#ifndef TOKEN64_H
#define TOKEN64_H

/**@file sofia-sip/token64.h
 *
 * @brief Token64 encoding.
 *
 * This module contains token64 encoding functions. Token64 encodes
 * arbitrary octet strings as http header tokens containing only characters
 * in range @c [-+A-Za-z0-9].
 *
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

SOFIAPUBFUN isize_t token64_e(char b[], isize_t bsiz,
			      void const *data, isize_t dlen);

/** Calculate size of n bytes encoded in token-64 */
#define TOKEN64_SIZE(n) (((n + 2) / 3) * 4)

SOFIA_END_DECLS

#endif /* !TOKEN64_H */
