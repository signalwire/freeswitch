#define compile \
{ gcc -o su_md5 -O2 -g -Wall -DTEST -I. su_md5.c } ; exit 0
/* -*- c-style: java -*- */

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

/*
 * This code implements the MD5 message-digest algorithm. The algorithm is
 * due to Ron Rivest. This code was initially written by Colin Plumb in
 * 1993, no copyright is claimed. This code is in the public domain; do with
 * it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.  This code has
 * been tested against that, and is equivalent, except that you don't need
 * to include two pages of legalese with every copy.
 */

/** @ingroup su_md5
 *
 * @CFILE su_md5.c MD5 Implementation
 *
 * To compute the message digest of a chunk of bytes, declare an su_md5_t
 * context structure, pass it to su_md5_init(), call su_md5_update() as
 * needed on buffers full of bytes, and then call su_md5_digest(), which
 * will fill a supplied 16-byte array with the current digest.
 *
 * @note
 * This code was modified in 1997 by Jim Kingdon of Cyclic Software to
 * not require an integer type which is exactly 32 bits.  This work
 * draws on the changes for the same purpose by Tatu Ylonen
 * <ylo@cs.hut.fi> as part of SSH, but since I didn't actually use
 * that code, there is no copyright issue.  I hereby disclaim
 * copyright in any changes I have made; this code remains in the
 * public domain.
 *
 * @note Regarding su_* namespace: this avoids potential conflicts
 * with libraries such as some versions of Kerberos.  No particular
 * need to worry about whether the system supplies an MD5 library, as
 * this file is only about 3k of object code.
 *
 */

#include <string.h>	/* for memcpy() and memset() */

#include "sofia-sip/su_md5.h"

static void su_md5_transform(uint32_t buf[4], const unsigned char inraw[64]);

/* Little-endian byte-swapping routines.  Note that these do not depend on
   the size of datatypes such as cvs_uint32, nor do they require us to
   detect the endianness of the machine we are running on.  It is possible
   they should be macros for speed, but I would be surprised if they were a
   performance bottleneck for MD5. These are inlined by any sane compiler,
   anyways. */

static uint32_t getu32(const unsigned char *addr)
{
  return (((((unsigned long)addr[3] << 8) | addr[2]) << 8)
	  | addr[1]) << 8 | addr[0];
}

static void putu32(uint32_t data, unsigned char *addr)
{
  addr[0] = (unsigned char)data;
  addr[1] = (unsigned char)(data >> 8);
  addr[2] = (unsigned char)(data >> 16);
  addr[3] = (unsigned char)(data >> 24);
}

/** Initialize MD5 context.
 *
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 *
 * @param ctx Pointer to context structure.
 */
void
su_md5_init(su_md5_t *ctx)
{
  ctx->buf[0] = 0x67452301;
  ctx->buf[1] = 0xefcdab89;
  ctx->buf[2] = 0x98badcfe;
  ctx->buf[3] = 0x10325476;

  ctx->bits[0] = 0;
  ctx->bits[1] = 0;
}

/** Clear MD5 context.
 *
 * The function su_md5_deinit() clears MD5 context.
 *
 * @param context  Pointer to MD5 context structure.
 */
void su_md5_deinit(su_md5_t *context)
{
  memset(context, 0, sizeof *context);
}

/** Update MD5 context.
 *
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 *
 * @param ctx Pointer to context structure
 * @param b   Pointer to data
 * @param len Length of @a b as bytes
 */
void
su_md5_update(su_md5_t *ctx,
	      void const *b,
	      usize_t len)
{
  unsigned char const *buf = (unsigned char const *)b;
  uint32_t t;

  /* Update bitcount */

  t = ctx->bits[0];
  if ((ctx->bits[0] = (t + ((uint32_t)len << 3)) & 0xffffffff) < t)
    ctx->bits[1]++;	/* Carry from low to high */
  ctx->bits[1] += (uint32_t)(len >> 29);

  t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

  /* Handle any leading odd-sized chunks */

  if ( t ) {
    unsigned char *p = ctx->in + t;

    t = 64 - t;

    if (len < t) {
      memcpy(p, buf, len);
      return;
    }

    memcpy(p, buf, t);
    su_md5_transform (ctx->buf, ctx->in);
    buf += t;
    len -= t;
  }

  /* Process data in 64-byte chunks */

  while (len >= 64) {
    su_md5_transform(ctx->buf, buf);
    buf += 64;
    len -= 64;
  }

  /* Handle any remaining bytes of data. */
  memcpy(ctx->in, buf, len);
}

/** Copy memory, fix case to lower. */
static
void mem_i_cpy(unsigned char *d, unsigned char const *s, size_t len)
{
  size_t i;

  for (i = 0; i < len; i++)
    if (s[i] >= 'A' && s[i] <= 'Z')
      d[i] = s[i] + ('a' - 'A');
    else
      d[i] = s[i];
}

/**Update MD5 context.
 *
 * The function su_md5_iupdate() updates context to reflect the
 * concatenation of another buffer full of case-independent characters.
 *
 * @param ctx Pointer to context structure
 * @param b   Pointer to data
 * @param len Length of @a b as bytes
 */
void
su_md5_iupdate(su_md5_t *ctx,
	       void const *b,
	       usize_t len)
{
  unsigned char const *buf = (unsigned char const *)b;
  uint32_t t;

  /* Update bitcount */

  t = ctx->bits[0];
  if ((ctx->bits[0] = (t + ((uint32_t)len << 3)) & 0xffffffff) < t)
    ctx->bits[1]++;	/* Carry from low to high */
  ctx->bits[1] += (uint32_t)(len >> 29);

  t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

  /* Handle any leading odd-sized chunks */

  if ( t ) {
    unsigned char *p = ctx->in + t;

    t = sizeof(ctx->in) - t;

    if (len < t) {
      mem_i_cpy(p, buf, len);
      return;
    }
    mem_i_cpy(p, buf, t);
    su_md5_transform (ctx->buf, ctx->in);
    buf += t;
    len -= t;
  }

  /* Process data in 64-byte chunks */
  while (len >= sizeof(ctx->in)) {
    mem_i_cpy(ctx->in, buf, sizeof(ctx->in));
    su_md5_transform(ctx->buf, ctx->in);
    buf += sizeof(ctx->in);
    len -= sizeof(ctx->in);
  }

  /* Handle any remaining bytes of data. */
  mem_i_cpy(ctx->in, buf, len);
}

/** Update MD5 context with contents of string.
 *
 * The function su_md5_strupdate() updates context to reflect the
 * concatenation of NUL-terminated string.
 *
 * @param ctx Pointer to context structure
 * @param s   Pointer to string
 */
void su_md5_strupdate(su_md5_t *ctx, char const *s)
{
  if (s)
    su_md5_update(ctx, s, strlen(s));
}

/** Update MD5 context with contents of string, including final NUL.
 *
 * The function su_md5_str0update() updates context to reflect the
 * concatenation of NUL-terminated string, including the final NUL.
 *
 * @param ctx Pointer to context structure
 * @param s   Pointer to string
 */
void su_md5_str0update(su_md5_t *ctx, char const *s)
{
  if (!s)
    s = "";

  su_md5_update(ctx, s, strlen(s) + 1);
}

/** Update MD5 context with contents of case-independent string.
 *
 * The function su_md5_striupdate() updates context to reflect the
 * concatenation of NUL-terminated string.
 *
 * @param ctx Pointer to context structure
 * @param s   Pointer to string
 */
void su_md5_striupdate(su_md5_t *ctx, char const *s)
{
  if (s)
    su_md5_iupdate(ctx, s, strlen(s));
}

/** Update MD5 context with contents of case-independent string, including
 * final NUL.
 *
 * The function su_md5_stri0update() updates context to reflect the
 * concatenation of NUL-terminated string, including the final NUL.
 *
 * @param ctx Pointer to context structure
 * @param s   Pointer to string
 */
void su_md5_stri0update(su_md5_t *ctx, char const *s)
{
  if (!s)
    s = "";

  su_md5_iupdate(ctx, s, strlen(s) + 1);
}


/** Generate digest.
 *
 * Final wrapup. Pad message to 64-byte boundary with the bit pattern 1 0*
 * (64-bit count of bits processed, MSB-first), then concatenate message
 * with its length (measured in bits) as 64-byte big-endian integer.
 *
 * @param context  Pointer to context structure
 * @param digest   Digest array to be filled
 */
void
su_md5_digest(su_md5_t const *context, uint8_t digest[16])
{
  unsigned count;
  unsigned char *p;

  su_md5_t ctx[1];

  ctx[0] = context[0];

  /* Compute number of bytes mod 64 */
  count = (ctx->bits[0] >> 3) & 0x3F;

  /* Set the first char of padding to 0x80.  This is safe since there is
     always at least one byte free */
  p = ctx->in + count;
  *p++ = 0x80;

  /* Bytes of padding needed to make 64 bytes */
  count = 64 - 1 - count;

  /* Pad out to 56 mod 64 */
  if (count < 8) {
    /* Two lots of padding:  Pad the first block to 64 bytes */
    memset(p, 0, count);
    su_md5_transform (ctx->buf, ctx->in);

    /* Now fill the next block with 56 bytes */
    memset(ctx->in, 0, 56);
  } else {
    /* Pad block to 56 bytes */
    memset(p, 0, count-8);
  }

  /* Append length in bits and transform */
  putu32(ctx->bits[0], ctx->in + 56);
  putu32(ctx->bits[1], ctx->in + 60);

  su_md5_transform(ctx->buf, ctx->in);
  putu32(ctx->buf[0], digest);
  putu32(ctx->buf[1], digest + 4);
  putu32(ctx->buf[2], digest + 8);
  putu32(ctx->buf[3], digest + 12);
  memset(ctx, 0, sizeof(ctx));	/* In case it's sensitive */
}

void su_md5_hexdigest(su_md5_t const *ctx,
		      char digest[2 * SU_MD5_DIGEST_SIZE + 1])
{
  uint8_t b, bin[SU_MD5_DIGEST_SIZE];
  short i, j;

  su_md5_digest(ctx, bin);

  for (i = j = 0; i < 16; i++) {
    b = (bin[i] >> 4) & 15;
    digest[j++] = b + (b > 9 ? 'a' - 10 : '0');
    b = bin[i] & 15;
    digest[j++] = b + (b > 9 ? 'a' - 10 : '0');
  }

  digest[j] = '\0';
}

#ifndef ASM_MD5

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data, w &= 0xffffffff, w = w<<s | w>>(32-s), w += x )

/** @internal
 *
 * Add 64 bytes of data to hash.
 *
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void
su_md5_transform(uint32_t buf[4], const unsigned char inraw[64])
{
  register uint32_t a, b, c, d;
  uint32_t in[16];
  int i;

  for (i = 0; i < 16; ++i)
    in[i] = getu32 (inraw + 4 * i);

  a = buf[0];
  b = buf[1];
  c = buf[2];
  d = buf[3];

  MD5STEP(F1, a, b, c, d, in[ 0]+0xd76aa478,  7);
  MD5STEP(F1, d, a, b, c, in[ 1]+0xe8c7b756, 12);
  MD5STEP(F1, c, d, a, b, in[ 2]+0x242070db, 17);
  MD5STEP(F1, b, c, d, a, in[ 3]+0xc1bdceee, 22);
  MD5STEP(F1, a, b, c, d, in[ 4]+0xf57c0faf,  7);
  MD5STEP(F1, d, a, b, c, in[ 5]+0x4787c62a, 12);
  MD5STEP(F1, c, d, a, b, in[ 6]+0xa8304613, 17);
  MD5STEP(F1, b, c, d, a, in[ 7]+0xfd469501, 22);
  MD5STEP(F1, a, b, c, d, in[ 8]+0x698098d8,  7);
  MD5STEP(F1, d, a, b, c, in[ 9]+0x8b44f7af, 12);
  MD5STEP(F1, c, d, a, b, in[10]+0xffff5bb1, 17);
  MD5STEP(F1, b, c, d, a, in[11]+0x895cd7be, 22);
  MD5STEP(F1, a, b, c, d, in[12]+0x6b901122,  7);
  MD5STEP(F1, d, a, b, c, in[13]+0xfd987193, 12);
  MD5STEP(F1, c, d, a, b, in[14]+0xa679438e, 17);
  MD5STEP(F1, b, c, d, a, in[15]+0x49b40821, 22);

  MD5STEP(F2, a, b, c, d, in[ 1]+0xf61e2562,  5);
  MD5STEP(F2, d, a, b, c, in[ 6]+0xc040b340,  9);
  MD5STEP(F2, c, d, a, b, in[11]+0x265e5a51, 14);
  MD5STEP(F2, b, c, d, a, in[ 0]+0xe9b6c7aa, 20);
  MD5STEP(F2, a, b, c, d, in[ 5]+0xd62f105d,  5);
  MD5STEP(F2, d, a, b, c, in[10]+0x02441453,  9);
  MD5STEP(F2, c, d, a, b, in[15]+0xd8a1e681, 14);
  MD5STEP(F2, b, c, d, a, in[ 4]+0xe7d3fbc8, 20);
  MD5STEP(F2, a, b, c, d, in[ 9]+0x21e1cde6,  5);
  MD5STEP(F2, d, a, b, c, in[14]+0xc33707d6,  9);
  MD5STEP(F2, c, d, a, b, in[ 3]+0xf4d50d87, 14);
  MD5STEP(F2, b, c, d, a, in[ 8]+0x455a14ed, 20);
  MD5STEP(F2, a, b, c, d, in[13]+0xa9e3e905,  5);
  MD5STEP(F2, d, a, b, c, in[ 2]+0xfcefa3f8,  9);
  MD5STEP(F2, c, d, a, b, in[ 7]+0x676f02d9, 14);
  MD5STEP(F2, b, c, d, a, in[12]+0x8d2a4c8a, 20);

  MD5STEP(F3, a, b, c, d, in[ 5]+0xfffa3942,  4);
  MD5STEP(F3, d, a, b, c, in[ 8]+0x8771f681, 11);
  MD5STEP(F3, c, d, a, b, in[11]+0x6d9d6122, 16);
  MD5STEP(F3, b, c, d, a, in[14]+0xfde5380c, 23);
  MD5STEP(F3, a, b, c, d, in[ 1]+0xa4beea44,  4);
  MD5STEP(F3, d, a, b, c, in[ 4]+0x4bdecfa9, 11);
  MD5STEP(F3, c, d, a, b, in[ 7]+0xf6bb4b60, 16);
  MD5STEP(F3, b, c, d, a, in[10]+0xbebfbc70, 23);
  MD5STEP(F3, a, b, c, d, in[13]+0x289b7ec6,  4);
  MD5STEP(F3, d, a, b, c, in[ 0]+0xeaa127fa, 11);
  MD5STEP(F3, c, d, a, b, in[ 3]+0xd4ef3085, 16);
  MD5STEP(F3, b, c, d, a, in[ 6]+0x04881d05, 23);
  MD5STEP(F3, a, b, c, d, in[ 9]+0xd9d4d039,  4);
  MD5STEP(F3, d, a, b, c, in[12]+0xe6db99e5, 11);
  MD5STEP(F3, c, d, a, b, in[15]+0x1fa27cf8, 16);
  MD5STEP(F3, b, c, d, a, in[ 2]+0xc4ac5665, 23);

  MD5STEP(F4, a, b, c, d, in[ 0]+0xf4292244,  6);
  MD5STEP(F4, d, a, b, c, in[ 7]+0x432aff97, 10);
  MD5STEP(F4, c, d, a, b, in[14]+0xab9423a7, 15);
  MD5STEP(F4, b, c, d, a, in[ 5]+0xfc93a039, 21);
  MD5STEP(F4, a, b, c, d, in[12]+0x655b59c3,  6);
  MD5STEP(F4, d, a, b, c, in[ 3]+0x8f0ccc92, 10);
  MD5STEP(F4, c, d, a, b, in[10]+0xffeff47d, 15);
  MD5STEP(F4, b, c, d, a, in[ 1]+0x85845dd1, 21);
  MD5STEP(F4, a, b, c, d, in[ 8]+0x6fa87e4f,  6);
  MD5STEP(F4, d, a, b, c, in[15]+0xfe2ce6e0, 10);
  MD5STEP(F4, c, d, a, b, in[ 6]+0xa3014314, 15);
  MD5STEP(F4, b, c, d, a, in[13]+0x4e0811a1, 21);
  MD5STEP(F4, a, b, c, d, in[ 4]+0xf7537e82,  6);
  MD5STEP(F4, d, a, b, c, in[11]+0xbd3af235, 10);
  MD5STEP(F4, c, d, a, b, in[ 2]+0x2ad7d2bb, 15);
  MD5STEP(F4, b, c, d, a, in[ 9]+0xeb86d391, 21);

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}
#endif
