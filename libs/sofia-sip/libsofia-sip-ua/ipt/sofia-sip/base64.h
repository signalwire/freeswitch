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

#ifndef BASE64_H
#define BASE64_H

/**@file sofia-sip/base64.h
 *
 * @brief Base64 encoding and decoding functions.
 *
 * This module contains base64 encoding and decoding functions.  Base64
 * encodes arbitrary octet strings as strings containing characters @c
 * [A-Za-z0-9+/=]. Base64 is defined as part of MIME mail format, but it is
 * used widely by other text-based protocols as well.
 *
 * @sa <a href="http://www.ietf.org/rfc/rfc2045.txt">RFC 2045</a>,
 * <i>"Multipurpose Internet Mail Extensions (MIME) Part One:
 * Format of Internet Message Bodies"</i>,
 * N. Freed, N. Borenstein,
 * November 1996.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

/** Decode a BASE64-encoded string. */
SOFIAPUBFUN isize_t base64_d(char buf[], isize_t bsiz, char const *b64s);
/** Encode data with BASE64. */
SOFIAPUBFUN isize_t base64_e(char buf[], isize_t bsiz, void *data, isize_t dsiz);

/** Calculate size of n bytes encoded in base64 */
#define BASE64_SIZE(n) ((((n) + 2) / 3) * 4)

/** Calculate size of n bytes encoded in base64 sans trailing =. @NEW_1_12_5. */
#define BASE64_MINSIZE(n) ((n * 4 + 2) / 3)

SOFIA_END_DECLS

#endif /* !BASE_64 */
