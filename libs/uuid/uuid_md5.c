/*
**  OSSP uuid - Universally Unique Identifier
**  Copyright (c) 2004-2008 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2004-2008 The OSSP Project <http://www.ossp.org/>
**
**  This file is part of OSSP uuid, a library for the generation
**  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
**
**  Permission to use, copy, modify, and distribute this software for
**  any purpose with or without fee is hereby granted, provided that
**  the above copyright notice and this permission notice appear in all
**  copies.
**
**  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
**  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
**  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
**  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
**  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
**  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
**  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
**  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
**
**  uuid_md5.c: MD5 API implementation
*/

/* own headers (part 1/2) */
#include "uuid_ac.h"

/* system headers */
#include <stdlib.h>
#include <string.h>

/* own headers (part 2/2) */
#include "uuid_md5.h"

/*
 * This is a RFC 1321 compliant Message Digest 5 (MD5) algorithm
 * implementation. It is directly derived from the RSA code published in
 * RFC 1321 with just the following functionality preserving changes:
 * - converted function definitions from K&R to ANSI C
 * - included contents of the "global.h" and "md5.h" headers
 * - moved the SXX defines into the MD5Transform function
 * - replaced MD5_memcpy() with memcpy(3) and MD5_memset() with memset(3)
 * - renamed "index" variables to "idx" to avoid namespace conflicts
 * - reformatted C style to conform with OSSP C style
 * - added own OSSP style frontend API
 */

/*
** ==== BEGIN RFC 1321 CODE ====
*/

/*
 * RSA Data Security, Inc., MD5 message-digest algorithm
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991.
 * All rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT4 defines a four byte word */
#if SIZEOF_UNSIGNED_SHORT       == 4
typedef unsigned short int     UINT4;
#elif SIZEOF_UNSIGNED_INT       == 4
typedef unsigned int           UINT4;
#elif SIZEOF_UNSIGNED_LONG      == 4
typedef unsigned long int      UINT4;
#elif SIZEOF_UNSIGNED_LONG_LONG == 4
typedef unsigned long long int UINT4;
#else
#error ERROR: unable to determine UINT4 type (four byte word)
#endif

/* MD5 context. */
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

/* prototypes for internal functions */
static void MD5Init      (MD5_CTX *_ctx);
static void MD5Update    (MD5_CTX *_ctx, unsigned char *, unsigned int);
static void MD5Final     (unsigned char [], MD5_CTX *);
static void MD5Transform (UINT4 [], unsigned char []);
static void Encode       (unsigned char *, UINT4 *, unsigned int);
static void Decode       (UINT4 *, unsigned char *, unsigned int);

/* finalization padding */
static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions. */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits. */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
   Rotation is separate from addition to prevent recomputation. */
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
}
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
}

/* MD5 initialization. Begins an MD5 operation, writing a new context. */
static void MD5Init(
    MD5_CTX *context)
{
    context->count[0] = context->count[1] = 0;

    /* Load magic initialization constants. */
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
    return;
}

/* MD5 block update operation. Continues an MD5 message-digest
   operation, processing another message block, and updating the
   context. */
static void MD5Update(
    MD5_CTX *context,                                        /* context */
    unsigned char *input,                                /* input block */
    unsigned int inputLen)                     /* length of input block */
{
    unsigned int i, idx, partLen;

    /* Compute number of bytes mod 64 */
    idx = (unsigned int)((context->count[0] >> 3) & 0x3F);

    /* Update number of bits */
    if ((context->count[0] += ((UINT4)inputLen << 3)) < ((UINT4)inputLen << 3))
        context->count[1]++;
    context->count[1] += ((UINT4)inputLen >> 29);

    partLen = (unsigned int)64 - idx;

    /* Transform as many times as possible.  */
    if (inputLen >= partLen) {
        memcpy((POINTER)&context->buffer[idx], (POINTER)input, (size_t)partLen);
        MD5Transform(context->state, context->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64)
            MD5Transform(context->state, &input[i]);
        idx = 0;
    }
    else
        i = 0;

    /* Buffer remaining input */
    memcpy((POINTER)&context->buffer[idx], (POINTER)&input[i], (size_t)(inputLen - i));
}

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
   the message digest and zeroizing the context. */
static void MD5Final(
    unsigned char digest[],                                 /* message digest */
    MD5_CTX *context)                                       /* context */
{
    unsigned char bits[8];
    unsigned int idx, padLen;

    /* Save number of bits */
    Encode(bits, context->count, 8);

    /* Pad out to 56 mod 64. */
    idx = (unsigned int)((context->count[0] >> 3) & 0x3f);
    padLen = (idx < 56) ? ((unsigned int)56 - idx) : ((unsigned int)120 - idx);
    MD5Update(context, PADDING, padLen);

    /* Append length (before padding) */
    MD5Update(context, bits, 8);

    /* Store state in digest */
    Encode(digest, context->state, 16);

    /* Zeroize sensitive information. */
    memset((POINTER)context, 0, sizeof(*context));
}

/* MD5 basic transformation. Transforms state based on block. */
static void MD5Transform(
    UINT4 state[],
    unsigned char block[])
{
    UINT4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    Decode(x, block, 64);

    /* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
    FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
    FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
    FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
    FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
    FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
    FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
    FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
    FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

   /* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
    GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
    GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
    GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
    GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
    GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
    GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
    GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
    GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
    GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
    HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
    HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
    HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
    HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
    HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
    HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
    HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
    HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
    II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
    II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
    II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
    II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
    II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
    II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
    II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
    II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    /* Zeroize sensitive information. */
    memset((POINTER)x, 0, sizeof(x));
}

/* Encodes input (UINT4) into output (unsigned char).
   Assumes len is a multiple of 4. */
static void Encode(
    unsigned char *output,
    UINT4 *input,
    unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j]   = (unsigned char)( input[i]        & 0xff);
        output[j+1] = (unsigned char)((input[i] >> 8)  & 0xff);
        output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
        output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
    }
    return;
}

/* Decodes input (unsigned char) into output (UINT4).
   Assumes len is a multiple of 4. */
static void Decode(
    UINT4 *output,
    unsigned char *input,
    unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] =   ( (UINT4)input[j])
                    | (((UINT4)input[j+1]) << 8 )
                    | (((UINT4)input[j+2]) << 16)
                    | (((UINT4)input[j+3]) << 24);
    return;
}

/*
** ==== END RFC 1321 CODE ====
*/

struct md5_st {
    MD5_CTX ctx;
};

md5_rc_t md5_create(md5_t **md5)
{
    if (md5 == NULL)
        return MD5_RC_ARG;
    if ((*md5 = (md5_t *)malloc(sizeof(md5_t))) == NULL)
        return MD5_RC_MEM;
    MD5Init(&((*md5)->ctx));
    return MD5_RC_OK;
}

md5_rc_t md5_init(md5_t *md5)
{
    if (md5 == NULL)
        return MD5_RC_ARG;
    MD5Init(&(md5->ctx));
    return MD5_RC_OK;
}

md5_rc_t md5_update(md5_t *md5, const void *data_ptr, size_t data_len)
{
    if (md5 == NULL)
        return MD5_RC_ARG;
    MD5Update(&(md5->ctx), (unsigned char *)data_ptr, (unsigned int)data_len);
    return MD5_RC_OK;
}

md5_rc_t md5_store(md5_t *md5, void **data_ptr, size_t *data_len)
{
    MD5_CTX ctx;

    if (md5 == NULL || data_ptr == NULL)
        return MD5_RC_ARG;
    if (*data_ptr == NULL) {
        if ((*data_ptr = malloc(MD5_LEN_BIN)) == NULL)
            return MD5_RC_MEM;
        if (data_len != NULL)
            *data_len = MD5_LEN_BIN;
    }
    else {
        if (data_len != NULL) {
            if (*data_len < MD5_LEN_BIN)
                return MD5_RC_MEM;
            *data_len = MD5_LEN_BIN;
        }
    }
    memcpy((void *)(&ctx), (void *)(&(md5->ctx)), sizeof(MD5_CTX));
    MD5Final((unsigned char *)(*data_ptr), &(ctx));
    return MD5_RC_OK;
}

md5_rc_t md5_format(md5_t *md5, char **data_ptr, size_t *data_len)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char buf[MD5_LEN_BIN];
    unsigned char *bufptr;
    size_t buflen;
    md5_rc_t rc;
    int i;

    if (md5 == NULL || data_ptr == NULL)
        return MD5_RC_ARG;
    if (*data_ptr == NULL) {
        if ((*data_ptr = (char *)malloc(MD5_LEN_STR+1)) == NULL)
            return MD5_RC_MEM;
        if (data_len != NULL)
            *data_len = MD5_LEN_STR+1;
    }
    else {
        if (data_len != NULL) {
            if (*data_len < MD5_LEN_STR+1)
                return MD5_RC_MEM;
            *data_len = MD5_LEN_STR+1;
        }
    }

    bufptr = buf;
    buflen = sizeof(buf);
    if ((rc = md5_store(md5, (void **)((void *)&bufptr), &buflen)) != MD5_RC_OK)
        return rc;

    for (i = 0; i < (int)buflen; i++) {
	    (*data_ptr)[(i*2)+0] = hex[(int)(bufptr[i] >> 4)];
	    (*data_ptr)[(i*2)+1] = hex[(int)(bufptr[i] & 0x0f)];
    }
    (*data_ptr)[(i*2)] = '\0';
    return MD5_RC_OK;
}

md5_rc_t md5_destroy(md5_t *md5)
{
    if (md5 == NULL)
        return MD5_RC_ARG;
    free(md5);
    return MD5_RC_OK;
}

