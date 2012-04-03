/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * rsaglue.c - The interface between bignum math and RSA operations.
 * This layer's primary reason for existence is to allow adaptation
 * to other RSA math libraries for legal reasons.
 */

#include "first.h"

#include "bn.h"

#include "keys.h"
#include "random.h"
#include "rsaglue.h"
#include "usuals.h"

/*#define BNDEBUG 1*/

#if BNDEBUG
/* Some debugging hooks which have been left in for now. */
#include "bn/bnprint.h"
#define bndPut(prompt, bn) bnPrint(stdout, prompt, bn, "\n")
#define bndPrintf printf
#else
#define bndPut(prompt, bn) ((void)(prompt),(void)(bn))
#define bndPrintf(x) (void)0
#endif


/*
 * This returns TRUE if the key is too big, returning the
 * maximum number of bits that the library can accept.  It
 * is used if you want to use something icky from RSADSI, whose
 * code is known to have satatic limits on key sizes.  (BSAFE 2.1
 * advertises 2048-bit key sizes.  It lies.  It's talking about
 * conventional RC4 keys, whicah are useless to make anything like
 * that large.  RSA keys are limited to 1024 bits.
 */
int
rsaKeyTooBig(struct PubKey const *pub, struct SecKey const *sec)
{
	(void)pub;
	(void)sec;
	return 0;	/* Never too big! */
}

/*
 * Fill the given bignum, from bytes high-1 through low (where 0 is
 * the least significant byte), with non-zero random data.
 */
static int
randomPad(struct BigNum *bn, unsigned high, unsigned low)
{
	unsigned i, l;
	byte padding[64];   /* This can be any size (>0) whatsoever */

	high -= low;
	while (high) {
		l = high < sizeof(padding) ? high : sizeof(padding);
		randBytes(padding, l);
		for (i = 0; i < l; i++) {	/* Replace all zero bytes */
			while(padding[i] == 0)
				randBytes(padding+i, 1);
		}
		high -= l;
		if (bnInsertBigBytes(bn, padding, high+low, l) < 0)
			return RSAGLUE_NOMEM;
	}

	memset(padding, 0, sizeof(padding));
	return 0;
}

/*
 * Fill the given bignum, from bytes high-1 through low (where 0 is
 * the least significant byte), with all ones (0xFF) data.
 */
static int
onesPad(struct BigNum *bn, unsigned high, unsigned low)
{
	unsigned l;
	static byte const padding[] = {
		255,255,255,255,255,255,255,255,
		255,255,255,255,255,255,255,255
	};

	high -= low;
	while (high) {
		l = high < sizeof(padding) ? high : sizeof(padding);
		high -= l;
		if (bnInsertBigBytes(bn, padding, high+low, l) < 0)
			return RSAGLUE_NOMEM;
	}
	return 0;
}

/*
 * Wrap a PKCS type 2 wrapper around some data and RSA encrypt it.
 * If the modulus is n bytes long, with the most significant byte
 * being n-1 and the least significant, 0, the wrapper looks like:
 *
 * Position     Value   Function
 * n-1           0      This is needed to ensure that the padded number
 *                      is less than the modulus.
 * n-2           2      The padding type (non-zero random).
 * n-3..len+1   ???     Non-zero random padding bytes to "salt" the
 *                      output and prevent duplicate plaintext attacks.
 * len           0      Zero byte to mark the end of the padding
 * len-1..0     data    Supplied payload data.
 *
 * There really should be several bytes of padding, although this
 * routine will not fail to encrypt unless it will not fit, even
 * with no padding bytes.
 */

static byte const encryptedType = 2;

int
rsaPublicEncrypt(struct BigNum *bn, byte const *in, unsigned len,
	struct PubKey const *pub)
{
	unsigned bytes = (bnBits(&pub->n)+7)/8;

	if (len+3 > bytes)
		return RSAGLUE_TOOSMALL;	/* Won't fit! */

	/* Set the entire number to 0 to start */
	(void)bnSetQ(bn, 0);

	if (bnInsertBigBytes(bn, &encryptedType, bytes-2, 1) < 0)
		return RSAGLUE_NOMEM;
	if (randomPad(bn, bytes-2, len+1) < 0)
		return RSAGLUE_NOMEM;

	if (bnInsertBigBytes(bn, in, 0, len) < 0)
		return RSAGLUE_NOMEM;
bndPrintf("RSA encrypting.\n");
bndPut("plaintext = ", bn);
	return bnExpMod(bn, bn, &pub->e, &pub->n);
}

/*
 * This performs a modular exponentiation using the Chinese Remainder
 * Algorithm when the modulus is known to have two relatively prime
 * factors n = p * q, and u = p^-1 (mod q) has been precomputed.
 *
 * The chinese remainder algorithm lets a computation mod n be performed
 * mod p and mod q, and the results combined.  Since it takes
 * (considerably) more than twice as long to perform modular exponentiation
 * mod n as it does to perform it mod p and mod q, time is saved.
 *
 * If x is the desired result, let xp and xq be the values of x mod p
 * and mod q, respectively.  Obviously, x = xp + p * k for some k.
 * Taking this mod q, xq == xp + p*k (mod q), so p*k == xq-xp (mod q)
 * and k == p^-1 * (xq-xp) (mod q), so k = u * (xq-xp mod q) mod q.
 * After that, x = xp + p * k.
 *
 * Another savings comes from reducing the exponent d modulo phi(p)
 * and phi(q).  Here, we assume that p and q are prime, so phi(p) = p-1
 * and phi(q) = q-1.
 */
static int
bnExpModCRA(struct BigNum *x, struct BigNum const *d,
	struct BigNum const *p, struct BigNum const *q, struct BigNum const *u)
{
	struct BigNum xp, xq, k;
	int i;

bndPrintf("Performing CRA\n");
bndPut("x = ", x);
bndPut("p = ", p);
bndPut("q = ", q);
bndPut("d = ", d);
bndPut("u = ", u);

	bnBegin(&xp);
	bnBegin(&xq);
	bnBegin(&k);

	/* Compute xp = (x mod p) ^ (d mod p-1) mod p */
	if (bnCopy(&xp, p) < 0)	/* First, use xp to hold p-1 */
		goto fail;
	(void)bnSubQ(&xp, 1);	/* p > 1, so subtracting is safe. */
	if (bnMod(&k, d, &xp) < 0)	/* Use k to hold the exponent */
		goto fail;
bndPut("d mod p-1 = ", &k);
	if (bnMod(&xp, x, p) < 0)	/* Now xp = (x mod p) */
		goto fail;
bndPut("x mod p = ", &xp);
	if (bnExpMod(&xp, &xp, &k, p) < 0)	/* xp = (x mod p)^k mod p */
		goto fail;
bndPut("xp = x^d mod p = ", &xp);

	/* Compute xq = (x mod q) ^ (d mod q-1) mod q */
	if (bnCopy(&xq, q) < 0)	/* First, use xq to hold q-1 */
		goto fail;
	(void)bnSubQ(&xq, 1);	/* q > 1, so subtracting is safe. */
	if (bnMod(&k, d, &xq) < 0)	/* Use k to hold the exponent */
		goto fail;
bndPut("d mod q-1 = ", &k);
	if (bnMod(&xq, x, q) < 0)	/* Now xq = (x mod q) */
		goto fail;
bndPut("x mod q = ", &xq);
	if (bnExpMod(&xq, &xq, &k, q) < 0)	/* xq = (x mod q)^k mod q */
		goto fail;
bndPut("xq = x^d mod q = ", &xq);

	i = bnSub(&xq, &xp);
bndPut("xq - xp = ", &xq);
bndPrintf(("With sign %d\n", i));
	if (i < 0)
		goto fail;
	if (i) {
		/*
		 * Borrow out - xq-xp is negative, so bnSub returned
		 * xp-xq instead, the negative of the true answer.
		 * Add q back (which is subtracting from the negative)
		 * until the sign flips again.  If p is much greater
		 * than q, this step could take annoyingly long.
		 * PGP requires that p < q, so it'll only happen once.
		 * You could get this stuck in a very lengthy loop by
		 * feeding this function a p >> q, but it seems fair
		 * to assume that secret keys are not constructed
		 * maliciously.
		 *
		 * If this becomes a concern, you can fix it up with a
		 * bnMod.  (But watch out for the case that the correct
		 * answer is zero!)
		 */
		do {
			i = bnSub(&xq, q);
bndPut("xq - xp mod q = ", &xq);
			if (i < 0)
				goto fail;
		} while (!i);
	}

	/* Compute k = xq * u mod q */
	if (bnMul(&k, u, &xq) < 0)
		goto fail;
bndPut("(xq-xp) * u = ", &k);
	if (bnMod(&k, &k, q) < 0)
		goto fail;
bndPut("k = (xq-xp)*u % q = ", &k);

#if BNDEBUG	/* @@@ DEBUG - do it the slow way for comparison */
	if (bnMul(&xq, p, q) < 0)
		goto fail;
bndPut("n = p*q = ", &xq);
	if (bnExpMod(x, x, d, &xq) < 0)
		goto fail;
	if (bnCopy(&xq, x) < 0)
		goto fail;
bndPut("x^d mod n = ", &xq);
#endif

	/* Now x = k * p + xp is the final answer */
	if (bnMul(x, &k, p) < 0)
		goto fail;
bndPut("k * p = ", x);
	if (bnAdd(x, &xp) < 0)
		goto fail;
bndPut("k*p + xp = ", x);
#if BNDEBUG
	if (bnCmp(x, &xq) != 0) {
bndPrintf(("Nasty!!!\n"));
		goto fail;
	}
	bnSetQ(&k, 17);
	bnMul(&xp, p, q);
	bnExpMod(&xq, &xq, &k, &xp);
bndPut("x^17 mod n = ", &xq);
#endif
	bnEnd(&xp);
	bnEnd(&xq);
	bnEnd(&k);
	return 0;

fail:
	bnEnd(&xp);
	bnEnd(&xq);
	bnEnd(&k);
	return RSAGLUE_NOMEM;
}

/*
 * This does an RSA signing operation, which is very similar, except
 * that the padding differs.  The type is 1, and the padding is all 1's
 * (hex 0xFF).
 *
 * To summarize, the format is:
 *
 * Position     Value   Function
 * n-1           0      This is needed to ensure that the padded number
 *                      is less than the modulus.
 * n-2           1      The padding type (all ones).
 * n-3..len+1   255     All ones padding to ensure signatures are rare.
 * len           0      Zero byte to mark the end of the padding
 * len-1..0     data    The payload
 *
 *
 * The reason for the all 1's padding is an extra consistency check.
 * A randomly invented signature will not decrypt to have the long
 * run of ones necessary for acceptance.
 *
 * Oh... the public key isn't needed to decrypt, but it's passed in
 * because a different glue library may need it for some reason.
 */
static const byte signedType = 1;

int
rsaPrivateEncrypt(struct BigNum *bn, byte const *in, unsigned len,
	struct PubKey const *pub, struct SecKey const *sec)
{
	unsigned bytes = (bnBits(&pub->n)+7)/8;

	/* Set the entire number to 0 to start */
	(void)bnSetQ(bn, 0);

	if (len+3 > bytes)
		return RSAGLUE_TOOSMALL;	/* Won't fit */
	if (bnInsertBigBytes(bn, &signedType, bytes-2, 1) < 0)
		return RSAGLUE_NOMEM;
	if (onesPad(bn, bytes-2, len+1) < 0)
		return RSAGLUE_NOMEM;
	if (bnInsertBigBytes(bn, in, 0, len) < 0)
		return RSAGLUE_NOMEM;

bndPrintf(("RSA signing.\n"));
bndPut("plaintext = ", bn);
	return bnExpModCRA(bn, &sec->d, &sec->p, &sec->q, &sec->u);
}

/*
 * Searches bytes, beginning with start-1 and progressing to 0,
 * until one that is not 0xff is found.  The idex of the last 0xff
 * byte is returned (or start if start-1 is not 0xff.)
 */
static unsigned
bnSearchNonOneFromHigh(struct BigNum const *bn, unsigned start)
{
	byte buf[16];	/* Size is arbitrary */
	unsigned l;
	unsigned i;

	while (start) {
		l = start < sizeof(buf) ? start : sizeof(buf);
		start -= l;
		bnExtractBigBytes(bn, buf, start, l);
		for (i = 0; i < l; i++) {
			if (buf[i] != 0xff) {
				memset(buf, 0, sizeof(buf));
				return start + l - i;
			}
		}
	}
	/* Nothing found */
	memset(buf, 0, sizeof(buf));
	return 0;
}

/*
 * Decrypt a message with a public key.
 * These destroy (actually, replace with a decrypted version) the
 * input bignum bn.
 *
 * Performs an RSA signature check.  Returns a prefix of the unwrapped
 * data in the given buf.  Returns the length of the untruncated
 * data, which may exceed "len". Returns <0 on error.
 */
int
rsaPublicDecrypt(byte *buf, unsigned len, struct BigNum *bn,
	struct PubKey const *pub)
{
	byte tmp[1];
	unsigned bytes;

bndPrintf(("RSA signature checking.\n"));
	if (bnExpMod(bn, bn, &pub->e, &pub->n) < 0)
		return RSAGLUE_NOMEM;
bndPut("decrypted = ", bn);
	bytes = (bnBits(&pub->n)+7)/8;

	bnExtractBigBytes(bn, tmp, bytes-2, 2);
	if (tmp[0] != 0 || tmp[1] != signedType) {
		memset(tmp, 0, 2);
		return RSAGLUE_CORRUPT;
	}

	bytes = bnSearchNonOneFromHigh(bn, bytes-2);
	if (bytes < 1)
		return RSAGLUE_CORRUPT;
	bytes--;
	bnExtractBigBytes(bn, tmp, bytes, 1);
	if (tmp[0] != 0) {
		tmp[0] = 0;
		return RSAGLUE_CORRUPT;
	}
	/* Note: tmp isn't sensitive any more because its a constant! */
	/* Success! Return the data */
	if (len > bytes)
		len = bytes;
	bnExtractBigBytes(bn, buf, bytes-len, len);
	return bytes;
}


/*
 * Searches bytes, beginning with start-1 and progressing to 0,
 * until finding one that is zero, or the end of the array.
 * The index of the last non-zero byte is returned (0 if the array
 * is all non-zero, or start if start-1 is zero).
 */
static unsigned
bnSearchZeroFromHigh(struct BigNum const *bn, unsigned start)
{
	byte buf[16];	/* Size is arbitrary */
	unsigned l;
	unsigned i;

	while (start) {
		l = start < sizeof(buf) ? start : sizeof(buf);
		start -= l;
		bnExtractBigBytes(bn, buf, start, l);
		for (i = 0; i < l; i++) {
			if (buf[i] == 0) {
				memset(buf, 0, sizeof(buf));
				return start + l - i;
			}
		}
	}
	/* Nothing found */
	memset(buf, 0, sizeof(buf));
	return 0;
}

/*
 * Performs an RSA decryption.  Returns a prefix of the unwrapped
 * data in the given buf.  Returns the length of the untruncated
 * data, which may exceed "len". Returns <0 on error.
 */
int
rsaPrivateDecrypt(byte *buf, unsigned len, struct BigNum *bn,
	struct PubKey const *pub, struct SecKey const *sec)
{
	unsigned bytes;
	byte tmp[2];

bndPrintf(("RSA decrypting\n"));
	if (bnExpModCRA(bn, &sec->d, &sec->p, &sec->q, &sec->u) < 0)
		return RSAGLUE_NOMEM;
bndPut("decrypted = ", bn);
	bytes = (bnBits(&pub->n)+7)/8;

	bnExtractBigBytes(bn, tmp, bytes-2, 2);
	if (tmp[0] != 0 || tmp[1] != 2) {
		memset(tmp, 0, 2);
		return RSAGLUE_CORRUPT;
	}

	bytes = bnSearchZeroFromHigh(bn, bytes-2);
	if (bytes-- == 0)
		return RSAGLUE_CORRUPT;

	if (len > bytes)
		len = bytes;
	bnExtractBigBytes(bn, buf, bytes-len, len);
	return bytes;
}
