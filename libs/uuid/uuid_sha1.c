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
**  uuid_sha1.c: SHA-1 API implementation
*/

/* own headers (part 1/2) */
#include "uuid_ac.h"

/* system headers */
#include <stdlib.h>
#include <string.h>

/* own headers (part 2/2) */
#include "uuid_sha1.h"

/*
 *  This is a RFC 3174 compliant Secure Hash Function (SHA-1) algorithm
 *  implementation. It is directly derived from the SHA-1 reference
 *  code published in RFC 3174 with just the following functionality
 *  preserving changes:
 *  - reformatted C style to conform with OSSP C style
 *  - added own OSSP style frontend API
 *  - added Autoconf based determination of sha1_uintX_t types
 */

/*
** ==== BEGIN RFC 3174 CODE ====
*/

/*
 *  This implements the Secure Hashing Algorithm 1 as defined in
 *  FIPS PUB 180-1 published April 17, 1995.
 *
 *  The SHA-1, produces a 160-bit message digest for a given data
 *  stream. It should take about 2**n steps to find a message with the
 *  same digest as a given message and 2**(n/2) to find any two messages
 *  with the same digest, when n is the digest size in bits. Therefore,
 *  this algorithm can serve as a means of providing a "fingerprint" for
 *  a message.
 *
 *  Caveats: SHA-1 is designed to work with messages less than 2^64 bits
 *  long. Although SHA-1 allows a message digest to be generated for
 *  messages of any number of bits less than 2^64, this implementation
 *  only works with messages with a length that is a multiple of the
 *  size of an 8-bit character.
 */

typedef unsigned char sha1_uint8_t;

#if SIZEOF_SHORT  > 2
typedef short int sha1_int16plus_t;
#elif SIZEOF_INT  > 2
typedef int       sha1_int16plus_t;
#elif SIZEOF_LONG > 2
typedef long int  sha1_int16plus_t;
#else
#error ERROR: unable to determine sha1_int16plus_t type (at least two byte word)
#endif

#if SIZEOF_UNSIGNED_SHORT       == 4
typedef unsigned short int     sha1_uint32_t;
#elif SIZEOF_UNSIGNED_INT       == 4
typedef unsigned int           sha1_uint32_t;
#elif SIZEOF_UNSIGNED_LONG      == 4
typedef unsigned long int      sha1_uint32_t;
#elif SIZEOF_UNSIGNED_LONG_LONG == 4
typedef unsigned long long int sha1_uint32_t;
#else
#error ERROR: unable to determine sha1_uint32_t type (four byte word)
#endif

enum {
    shaSuccess = 0,
    shaNull,            /* null pointer parameter */
    shaStateError       /* called Input after Result */
};

#define SHA1HashSize 20

/* This structure will hold context information for the SHA-1 hashing operation */
typedef struct SHA1Context {
    sha1_uint32_t Intermediate_Hash[SHA1HashSize/4]; /* Message Digest */
    sha1_uint32_t Length_Low;                        /* Message length in bits */
    sha1_uint32_t Length_High;                       /* Message length in bits */
    sha1_int16plus_t Message_Block_Index;            /* Index into message block array */
    sha1_uint8_t Message_Block[64];                  /* 512-bit message blocks */
    int Computed;                                    /* Is the digest computed? */
    int Corrupted;                                   /* Is the message digest corrupted? */
} SHA1Context;

/* Function Prototypes */
static int SHA1Reset  (SHA1Context *);
static int SHA1Input  (SHA1Context *, const sha1_uint8_t *, unsigned int);
static int SHA1Result (SHA1Context *, sha1_uint8_t Message_Digest[]);

/* Local Function Prototyptes */
static void SHA1PadMessage         (SHA1Context *);
static void SHA1ProcessMessageBlock(SHA1Context *);

/* Define the SHA1 circular left shift macro */
#define SHA1CircularShift(bits,word) \
    (((word) << (bits)) | ((word) >> (32-(bits))))

/*
 *  This function will initialize the SHA1Context in preparation for
 *  computing a new SHA1 message digest.
 */
static int SHA1Reset(SHA1Context *context)
{
    if (context == NULL)
        return shaNull;

    context->Length_Low             = 0;
    context->Length_High            = 0;
    context->Message_Block_Index    = 0;

    context->Intermediate_Hash[0]   = 0x67452301;
    context->Intermediate_Hash[1]   = 0xEFCDAB89;
    context->Intermediate_Hash[2]   = 0x98BADCFE;
    context->Intermediate_Hash[3]   = 0x10325476;
    context->Intermediate_Hash[4]   = 0xC3D2E1F0;

    context->Computed   = 0;
    context->Corrupted  = 0;

    return shaSuccess;
}

/*
 *  This function will return the 160-bit message digest into the
 *  Message_Digest array provided by the caller. NOTE: The first octet
 *  of hash is stored in the 0th element, the last octet of hash in the
 *  19th element.
 */
static int SHA1Result(SHA1Context *context, sha1_uint8_t Message_Digest[])
{
    int i;

    if (context == NULL || Message_Digest == NULL)
        return shaNull;
    if (context->Corrupted)
        return context->Corrupted;

    if (!context->Computed) {
        SHA1PadMessage(context);
        for (i = 0; i < 64; i++) {
            /* message may be sensitive, clear it out */
            context->Message_Block[i] = (sha1_uint8_t)0;
        }
        context->Length_Low  = 0; /* and clear length */
        context->Length_High = 0;
        context->Computed    = 1;
    }
    for (i = 0; i < SHA1HashSize; i++)
        Message_Digest[i] = (sha1_uint8_t)(context->Intermediate_Hash[i>>2] >> (8 * (3 - (i & 0x03))));

    return shaSuccess;
}

/*
 *  This function accepts an array of octets as the next portion of the
 *  message.
 */
static int SHA1Input(SHA1Context *context, const sha1_uint8_t *message_array, unsigned int length)
{
    if (length == 0)
        return shaSuccess;
    if (context == NULL || message_array == NULL)
        return shaNull;

    if (context->Computed) {
        context->Corrupted = shaStateError;
        return shaStateError;
    }
    if (context->Corrupted)
        return context->Corrupted;
    while (length-- && !context->Corrupted) {
        context->Message_Block[context->Message_Block_Index++] = (*message_array & 0xFF);
        context->Length_Low += 8;
        if (context->Length_Low == 0) {
            context->Length_High++;
            if (context->Length_High == 0)
                context->Corrupted = 1; /* Message is too long */
        }
        if (context->Message_Block_Index == 64)
            SHA1ProcessMessageBlock(context);
        message_array++;
    }

    return shaSuccess;
}

/*
 *  This function will process the next 512 bits of the message stored
 *  in the Message_Block array. NOTICE: Many of the variable names in
 *  this code, especially the single character names, were used because
 *  those were the names used in the publication.
 */
static void SHA1ProcessMessageBlock(SHA1Context *context)
{
    const sha1_uint32_t K[] = {   /* Constants defined in SHA-1   */
        0x5A827999,
        0x6ED9EBA1,
        0x8F1BBCDC,
        0xCA62C1D6
    };
    int            t;             /* Loop counter                */
    sha1_uint32_t  temp;          /* Temporary word value        */
    sha1_uint32_t  W[80];         /* Word sequence               */
    sha1_uint32_t  A, B, C, D, E; /* Word buffers                */

    /* Initialize the first 16 words in the array W */
    for (t = 0; t < 16; t++) {
        W[t]  = (sha1_uint32_t)(context->Message_Block[t * 4    ] << 24);
        W[t] |= (sha1_uint32_t)(context->Message_Block[t * 4 + 1] << 16);
        W[t] |= (sha1_uint32_t)(context->Message_Block[t * 4 + 2] <<  8);
        W[t] |= (sha1_uint32_t)(context->Message_Block[t * 4 + 3]      );
    }

    for (t = 16; t < 80; t++)
       W[t] = SHA1CircularShift(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);

    A = context->Intermediate_Hash[0];
    B = context->Intermediate_Hash[1];
    C = context->Intermediate_Hash[2];
    D = context->Intermediate_Hash[3];
    E = context->Intermediate_Hash[4];

    for (t = 0; t < 20; t++) {
        temp =  SHA1CircularShift(5, A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (t = 20; t < 40; t++) {
        temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (t = 40; t < 60; t++) {
        temp = SHA1CircularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (t = 60; t < 80; t++) {
        temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    context->Intermediate_Hash[0] += A;
    context->Intermediate_Hash[1] += B;
    context->Intermediate_Hash[2] += C;
    context->Intermediate_Hash[3] += D;
    context->Intermediate_Hash[4] += E;

    context->Message_Block_Index = 0;

    return;
}

/*
 *  According to the standard, the message must be padded to an even
 *  512 bits. The first padding bit must be a '1'. The last 64 bits
 *  represent the length of the original message. All bits in between
 *  should be 0. This function will pad the message according to those
 *  rules by filling the Message_Block array accordingly. It will also
 *  call the ProcessMessageBlock function provided appropriately. When
 *  it returns, it can be assumed that the message digest has been
 *  computed.
 */
static void SHA1PadMessage(SHA1Context *context)
{
    /* Check to see if the current message block is too small to hold
       the initial padding bits and length. If so, we will pad the block,
       process it, and then continue padding into a second block. */
    if (context->Message_Block_Index > 55) {
        context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0x80;
        while (context->Message_Block_Index < 64)
            context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0;
        SHA1ProcessMessageBlock(context);
        while(context->Message_Block_Index < 56)
            context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0;
    }
    else {
        context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0x80;
        while(context->Message_Block_Index < 56)
            context->Message_Block[context->Message_Block_Index++] = (sha1_uint8_t)0;
    }

    /* Store the message length as the last 8 octets */
    context->Message_Block[56] = (sha1_uint8_t)(context->Length_High >> 24);
    context->Message_Block[57] = (sha1_uint8_t)(context->Length_High >> 16);
    context->Message_Block[58] = (sha1_uint8_t)(context->Length_High >>  8);
    context->Message_Block[59] = (sha1_uint8_t)(context->Length_High      );
    context->Message_Block[60] = (sha1_uint8_t)(context->Length_Low  >> 24);
    context->Message_Block[61] = (sha1_uint8_t)(context->Length_Low  >> 16);
    context->Message_Block[62] = (sha1_uint8_t)(context->Length_Low  >>  8);
    context->Message_Block[63] = (sha1_uint8_t)(context->Length_Low       );

    SHA1ProcessMessageBlock(context);
    return;
}

/*
** ==== END RFC 3174 CODE ====
*/

struct sha1_st {
    SHA1Context ctx;
};

sha1_rc_t sha1_create(sha1_t **sha1)
{
    if (sha1 == NULL)
        return SHA1_RC_ARG;
    if ((*sha1 = (sha1_t *)malloc(sizeof(sha1_t))) == NULL)
        return SHA1_RC_MEM;
    if (SHA1Reset(&((*sha1)->ctx)) != shaSuccess)
        return SHA1_RC_INT;
    return SHA1_RC_OK;
}

sha1_rc_t sha1_init(sha1_t *sha1)
{
    if (sha1 == NULL)
        return SHA1_RC_ARG;
    if (SHA1Reset(&(sha1->ctx)) != shaSuccess)
        return SHA1_RC_INT;
    return SHA1_RC_OK;
}

sha1_rc_t sha1_update(sha1_t *sha1, const void *data_ptr, size_t data_len)
{
    if (sha1 == NULL)
        return SHA1_RC_ARG;
    if (SHA1Input(&(sha1->ctx), (unsigned char *)data_ptr, (unsigned int)data_len) != shaSuccess)
        return SHA1_RC_INT;
    return SHA1_RC_OK;
}

sha1_rc_t sha1_store(sha1_t *sha1, void **data_ptr, size_t *data_len)
{
    SHA1Context ctx;

    if (sha1 == NULL || data_ptr == NULL)
        return SHA1_RC_ARG;
    if (*data_ptr == NULL) {
        if ((*data_ptr = malloc(SHA1_LEN_BIN)) == NULL)
            return SHA1_RC_MEM;
        if (data_len != NULL)
            *data_len = SHA1_LEN_BIN;
    }
    else {
        if (data_len != NULL) {
            if (*data_len < SHA1_LEN_BIN)
                return SHA1_RC_MEM;
            *data_len = SHA1_LEN_BIN;
        }
    }
    memcpy((void *)(&ctx), (void *)(&(sha1->ctx)), sizeof(SHA1Context));
    if (SHA1Result(&(ctx), (unsigned char *)(*data_ptr)) != shaSuccess)
        return SHA1_RC_INT;
    return SHA1_RC_OK;
}

sha1_rc_t sha1_format(sha1_t *sha1, char **data_ptr, size_t *data_len)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char buf[SHA1_LEN_BIN];
    unsigned char *bufptr;
    size_t buflen;
    sha1_rc_t rc;
    int i;

    if (sha1 == NULL || data_ptr == NULL)
        return SHA1_RC_ARG;
    if (*data_ptr == NULL) {
        if ((*data_ptr = (char *)malloc(SHA1_LEN_STR+1)) == NULL)
            return SHA1_RC_MEM;
        if (data_len != NULL)
            *data_len = SHA1_LEN_STR+1;
    }
    else {
        if (data_len != NULL) {
            if (*data_len < SHA1_LEN_STR+1)
                return SHA1_RC_MEM;
            *data_len = SHA1_LEN_STR+1;
        }
    }

    bufptr = buf;
    buflen = sizeof(buf);
    if ((rc = sha1_store(sha1, (void **)((void *)&bufptr), &buflen)) != SHA1_RC_OK)
        return rc;

    for (i = 0; i < (int)buflen; i++) {
	    (*data_ptr)[(i*2)+0] = hex[(int)(bufptr[i] >> 4)];
	    (*data_ptr)[(i*2)+1] = hex[(int)(bufptr[i] & 0x0f)];
    }
    (*data_ptr)[(i*2)] = '\0';
    return SHA1_RC_OK;
}

sha1_rc_t sha1_destroy(sha1_t *sha1)
{
    if (sha1 == NULL)
        return SHA1_RC_ARG;
    free(sha1);
    return SHA1_RC_OK;
}

