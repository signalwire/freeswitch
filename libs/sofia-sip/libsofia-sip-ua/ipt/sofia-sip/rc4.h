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

#ifndef RC4_H
/** Defined when <sofia-sip/rc4.h> has been included. */
#define RC4_H

/**@file sofia-sip/rc4.h
 * @brief Arcfour random number generator.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Sun Jun  9 14:32:58 1996 ppessi
 */

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

/** Byte. */
typedef uint8_t rc4_u8;

/** RC4 context.
 *
 * The RC4 context is accessed and modified through rc4_init() and rc4()
 * functions only.
 */
typedef struct {
  uint8_t rc4_i;
  uint8_t rc4_j;
  uint8_t rc4_array[256];
} rc4_t;

/** Key RC4 context. */
SOFIAPUBFUN void rc4_init(const void *seed, isize_t seed_len, rc4_t *state);

/** Generate RC4 stream. */
SOFIAPUBFUN void rc4(void *buffer, isize_t len, rc4_t *state);

#if defined(__cplusplus)
}
#endif

#endif /* !defined RC4_H */
