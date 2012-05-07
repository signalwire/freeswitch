/*
 * Copyright (c) 1993, 1994  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * True random number computation and storage
 *
 */

#include "first.h"
#include <stdlib.h>
#include <string.h>

#include "md5.h"
#include "randpool.h"
#include "usuals.h"

/* This is a parameter of the MD5 algorithm */
#define RANDKEYWORDS 16

/* The pool must be a multiple of the 16-byte (128-bit) MD5 block size */
#define RANDPOOLWORDS (((RANDPOOLBITS+127) & ~127) >> 5)

#if RANDPOOLWORDS <= RANDKEYWORDS
#error Random pool too small - please increase RANDPOOLBITS in randpool.h
#endif

/* Must be word-aligned, so make it words.  Cast to bytes as needed. */
static word32 randPool[RANDPOOLWORDS];	/* Random pool */
static word32 randKey[RANDKEYWORDS];	/* Random pool */
static unsigned randKeyAddPos = 0;	/* Position to add to */
static unsigned randPoolGetPos = 16; /* Position to get from */

/*
 * Destroys already-used random numbers.  Ensures no sensitive data
 * remains in memory that can be recovered later.  This is also
 * called to "stir in" newly acquired environmental noise bits before
 * removing any random bytes.
 *
 * The transformation is carried out by "encrypting" the data in CFB
 * mode with MD5 as the block cipher.  Then, to make certain the stirring
 * operation is strictly one-way, we destroy the key, getting 64 bytes
 * from the beginning of the pool and using them to reinitialize the
 * key.  These bytes are not returned by randPoolGetBytes().
 *
 * The key for the stirring operation is the XOR of some bytes from the
 * previous pool contents (not provably necessary, but it produces uniformly
 * distributed keys, which "feels better") and the newly added raw noise,
 * which will have a profound effect on every bit in the pool.
 *
 * To make this useful for pseudo-random (that is, repeatable) operations,
 * the MD5 transformation is always done with a consistent byte order.
 * MD5Transform itself works with 32-bit words, not bytes, so the pool,
 * usually an array of bytes, is transformed into an array of 32-bit words,
 * taking each group of 4 bytes in big-endian order.  At the end of the
 * stirring, the transformation is reversed.
 */
void
randPoolStir(void)
{
	int i;
	word32 iv[4];

	/* Convert to word32s for stirring operation */
	byteSwap(randPool, RANDPOOLWORDS);
	byteSwap(randKey, RANDKEYWORDS);

	/* Start IV from last block of randPool */
	memcpy(iv, randPool+RANDPOOLWORDS-4, sizeof(iv));

	/* CFB pass */
	for (i = 0; i < RANDPOOLWORDS; i += 4) {
		MD5Transform(iv, randKey);
		iv[0] = randPool[i  ] ^= iv[0];
		iv[1] = randPool[i+1] ^= iv[1];
		iv[2] = randPool[i+2] ^= iv[2];
		iv[3] = randPool[i+3] ^= iv[3];
	}

	/* Wipe iv from memory */
	iv[3] = iv[2] = iv[1] = iv[0] = 0;

	/* Convert randPool back to bytes for further use */
	byteSwap(randPool, RANDPOOLWORDS);

	/* Get new key */
	memcpy(randKey, randPool, sizeof(randKey));

	/* Set up pointers for future addition or removal of random bytes */
	randKeyAddPos = 0;
	randPoolGetPos = sizeof(randKey);
}

/*
 * Make a deposit of information (entropy) into the pool.  This is done by
 * XORing them into the key which is used to encrypt the pool.  Before any
 * bytes are retrieved from the pool, the altered key will be used to encrypt
 * the whole pool, causing all bits in the pool to depend on the new
 * information.
 *
 * The bits deposited need not have any particular distribution; the stirring
 * operation transforms them to uniformly-distributed bits.
 */
void
randPoolAddBytes(byte const *buf, unsigned len)
{
	byte *p = (byte *)randKey + randKeyAddPos;
	unsigned t = sizeof(randKey) - randKeyAddPos;

	while (len > t) {
		len -= t;
		while (t--)
			*p++ ^= *buf++;
		randPoolStir();		/* sets randKeyAddPos to 0 */
		p = (byte *)randKey;
		t = sizeof(randKey);
	}

	if (len) {
		randKeyAddPos += len;
		do
			*p++ ^= *buf++;
		while (--len);
		randPoolGetPos = sizeof(randPool); /* Force stir on get */
	}
}

/*
 * Withdraw some bits from the pool.  Regardless of the distribution of the
 * input bits, the bits returned are uniformly distributed, although they
 * cannot, of course, contain more Shannon entropy than the input bits.
 */
void
randPoolGetBytes(byte *buf, unsigned len)
{
	unsigned t;

	while (len > (t = sizeof(randPool) - randPoolGetPos)) {
		memcpy(buf, (byte *)randPool+randPoolGetPos, t);
		buf += t;
		len -= t;
		randPoolStir();
	}

	if (len) {
		memcpy(buf, (byte *)randPool+randPoolGetPos, len);
		randPoolGetPos += len;
		buf += len;
	}
}

byte
randPoolGetByte(void)
{
	if (randPoolGetPos == sizeof(randPool))
		randPoolStir();

	return ((byte *)randPool)[randPoolGetPos++];
}
