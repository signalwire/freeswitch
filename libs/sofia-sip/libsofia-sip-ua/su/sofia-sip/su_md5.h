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

#ifndef SU_MD5_H /**  Defined when su_md5.h has been included. */
#define SU_MD5_H

/**@defgroup su_md5 MD5 Digest
 *
 * The MD5 message digest algorithm is described in RFC 1321 by R. Rivest,
 * 1992.  The algorithm takes as input a octet string of arbitrary length
 * and generates a 128-bit hash value, "message digest" from the message
 * contents.
 *
 * While some message collisions (different messages with same MD5 digest)
 * has been generated, using collisions in an actual attack is much harder
 * and MD5 can be considered as cryptographically strong.
 */

/** @file sofia-sip/su_md5.h MD5 digest interface.
 *
 * @author <Pekka.Pessi@nokia.com>
 */

#ifndef SU_TYPES_H
#include "sofia-sip/su_types.h"
#endif

SOFIA_BEGIN_DECLS

/** MD5 context. */
typedef struct su_md5_t {
  uint32_t buf[4];
  uint32_t bits[2];
  uint8_t  in[64];
} su_md5_t;

#define SU_MD5_DIGEST_SIZE 16

SOFIAPUBFUN void su_md5_init(su_md5_t *context);
SOFIAPUBFUN void su_md5_deinit(su_md5_t *context);
SOFIAPUBFUN void su_md5_update(su_md5_t *context,
			       void const *buf, usize_t len);
SOFIAPUBFUN void su_md5_strupdate(su_md5_t *ctx, char const *s);
SOFIAPUBFUN void su_md5_str0update(su_md5_t *ctx, char const *s);

SOFIAPUBFUN void su_md5_iupdate(su_md5_t *context,
				void const *buf, usize_t len);
SOFIAPUBFUN void su_md5_striupdate(su_md5_t *ctx, char const *s);
SOFIAPUBFUN void su_md5_stri0update(su_md5_t *ctx, char const *s);

SOFIAPUBFUN void su_md5_digest(su_md5_t const *ctx,
			       uint8_t digest[SU_MD5_DIGEST_SIZE]);
SOFIAPUBFUN void su_md5_hexdigest(su_md5_t const *ctx,
				  char digest[2 * SU_MD5_DIGEST_SIZE + 1]);

#define SU_MD5_STRUPDATE(ctx, s) \
 ((s) ? su_md5_update(ctx, (s), strlen(s)) : (void)0)
#define SU_MD5_STR0UPDATE(ctx, s) \
 su_md5_update(ctx, (s) ? (s) : "", (s) ? strlen(s) + 1 : 1)
#define SU_MD5_STRIUPDATE(ctx, s) \
  ((s) ? su_md5_iupdate(ctx, (s), strlen(s)) : (void)0)
#define SU_MD5_STRI0UPDATE(ctx, s) \
  su_md5_iupdate(ctx, (s) ? (s) : "", (s) ? strlen(s) : 1)

SOFIA_END_DECLS

#endif /* !defined(MD5_H) */
