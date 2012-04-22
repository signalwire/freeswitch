/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * dsatest.c - DSA key generator and test driver.
 *
 * This generates DSA primes using a (hopefully) clearly
 * defined algorithm, based on David Kravitz's "kosherizer".
 * It is not, however, identical.
 */
#include <stdio.h>
#include <string.h>

#include "bn.h"
#include "prime.h"

#include "cputime.h"
#include "sha.h"

#define BNDEBUG 1

#if BNDEBUG
#include "bnprint.h"
#define bndPut(prompt, bn) bnPrint(stdout, prompt, bn, "\n")
#define bndPrintf printf
#else
#define bndPut(prompt, bn) ((void)(prompt),(void)(bn))
#define bndPrintf (void)
#endif

/*
 * Generate a bignum of a specified length, with the given
 * high and low 8 bits. "High" is merged into the high 8 bits of the
 * number.  For example, set it to 0x80 to ensure that the number is
 * exactly "bits" bits long (i.e. 2^(bits-1) <= bn < 2^bits).
 * "Low" is merged into the low 8 bits.  For example, set it to
 * 1 to ensure that you generate an odd number.
 *
 * Then XOR the result into the input bignum.  This is to
 * accomodate the kosherizer in all its generality.
 *
 * The bignum is generated using the given seed string.  The
 * technique is from David Kravitz (of the NSA)'s "kosherizer".
 * The string is hashed, and that (with the low bit forced to 1)
 * is used for the low 160 bits of the number.  Then the string,
 * considered as a big-endian array of bytes, is incremented
 * and the incremented value is hashed to produce the next most
 * significant 160 bits, and so on.  The increment is performed
 * modulo the size of the seed string.
 *
 * The seed is returned incremented so that it may be used to generate
 * subsequent numbers.
 *
 * The most and least significant 8 bits of the returned number are forced
 * to the values passed in "high" and "low", respectively.  Typically,
 * high would be set to 0x80 to force the most significant bit to 1.
 */
static int
genRandBn(struct BigNum *bn, unsigned bits, unsigned char high,
unsigned char low, unsigned char *seed, unsigned len)
{
	unsigned char buf1[SHA_DIGESTSIZE];
	unsigned char buf2[SHA_DIGESTSIZE];
	unsigned bytes = (bits+7)/8;
	unsigned l = 0;	/* Current position */
	unsigned i;
	struct SHAContext sha;

	if (!bits)
		return 0;

	/* Generate the first bunch of hashed data */
	shaInit(&sha);
	shaUpdate(&sha, seed, len);
	shaFinal(&sha, buf1);
	/* Increment the seed, ignoring carry out. */
	i = len;
	while (i-- && (++seed[i] & 255) == 0)
		;
	/* XOR in the existing bytes */
	bnExtractBigBytes(bn, buf2, l, SHA_DIGESTSIZE);
	for (i = 0; i < SHA_DIGESTSIZE; i++)
		buf1[i] ^= buf2[i];
	buf1[SHA_DIGESTSIZE-1] |= low;

	while (bytes > SHA_DIGESTSIZE) {
		bytes -= SHA_DIGESTSIZE;
		/* Merge in low half of high bits, if necessary */
		if (bytes == 1 && (bits & 7))
			buf1[0] |= high << (bits & 7);
		if (bnInsertBigBytes(bn, buf1, l, SHA_DIGESTSIZE) < 0)
			return -1;
		l += SHA_DIGESTSIZE;

		/* Compute the next hash we need */
		shaInit(&sha);
		shaUpdate(&sha, seed, len);
		shaFinal(&sha, buf1);
		/* Increment the seed, ignoring carry out. */
		i = len;
		while (i-- && (++seed[i] & 255) == 0)
			;
		/* XOR in the existing bytes */
		bnExtractBigBytes(bn, buf2, l, SHA_DIGESTSIZE);
		for (i = 0; i < SHA_DIGESTSIZE; i++)
			buf1[i] ^= buf2[i];
	}

	/* Do the final "bytes"-long section, using the tail bytes in buf1 */
	/* Mask off excess high bits */
	buf1[SHA_DIGESTSIZE-bytes] &= 255 >> (-bits & 7);
	/* Merge in specified high bits */
	buf1[SHA_DIGESTSIZE-bytes] |= high >> (-bits & 7);
	if (bytes > 1 && (bits & 7))
		buf1[SHA_DIGESTSIZE-bytes+1] |= high << (bits & 7);
	/* Merge in the appropriate bytes of the buffer */
	if (bnInsertBigBytes(bn, buf1+SHA_DIGESTSIZE-bytes, l, bytes) < 0)
		return -1;

	return 0;
}

struct Progress {
	FILE *f;
	unsigned column;
	unsigned wrap;
};

static int
genProgress(void *arg, int c)
{
	struct Progress *p = arg;
	if (++p->column > p->wrap) {
		putc('\n', p->f);
		p->column = 1;
	}
	putc(c, p->f);
	fflush(p->f);
	return 0;
}

static int
dsaGen(struct BigNum *p, unsigned pbits, struct BigNum *q, unsigned qbits,
	struct BigNum *g, struct BigNum *x, struct BigNum *y,
	unsigned char *seed, unsigned len, FILE *f)
{
	struct BigNum h, e;
	int i;
#if CLOCK_AVAIL
	timetype start, stop;
	unsigned long s;
#endif
	struct Progress progress;

	if (f)
		fprintf(f,
		   "Generating a DSA key pair with %u-bit p and %u-bit q,\n"
	           "seed = \"%.*s\"\n", pbits, qbits, (int)len, (char *)seed);
	progress.f = f;
	progress.column = 0;
	progress.wrap = 78;

#if CLOCK_AVAIL
	gettime(&start);
#endif

	/*
	 * Choose a random starting place for q
	 * Starting place is SHA(seed) XOR SHA(seed+1),
	 * With the high *8* bits set to 1.
	 */
	(void)bnSetQ(q, 0);
	if (genRandBn(q, qbits, 0xFF, 0, seed, len) < 0)
		return -1;
	bndPut("q1 = ", q);
	if (genRandBn(q, qbits, 0xFF, 1, seed, len) < 0)
		return -1;
	bndPut("q2 = ", q);
	/* And search for a prime */
	i = primeGen(q, (unsigned (*)(unsigned))0, f ? genProgress : 0,
	             (void *)&progress, 0);
	bndPut("q  = ", q);
	if (i < 0)
		return -1;
	
	/* ...and for p */
	(void)bnSetQ(p, 0);
	if (genRandBn(p, pbits, 0xC0, 1, seed, len) < 0)
		return -1;
	bndPut("p1 = ", p);

	/* Temporarily double q */
	if (bnLShift(q, 1) < 0)
		return -1;

	bnBegin(&h);
	bnBegin(&e);

	/* Set p = p - (p mod q) + 1, i.e. congruent to 1 mod 2*q */
	if (bnMod(&e, p, q) < 0)
		goto failed;
	if (bnSub(p, &e) < 0 || bnAddQ(p, 1) < 0)
		goto failed;
	bndPut("p2 = ", p);

	if (f)
		genProgress(&progress, ' ');

	/* And search for a prime */
	i = primeGenStrong(p, q, f ? genProgress : 0, (void *)&progress);
	if (i < 0)
		return -1;
	bndPut("p  = ", p);

	/* Reduce q again */
	bnRShift(q, 1);

	/* Now hunt for a suitable g - first, find (p-1)/q */
	if (bnDivMod(&e, &h, p, q) < 0)
		goto failed;
	/* e is now the exponent (p-1)/q, and h is the remainder (one!) */
	if (bnBits(&h) != 1) {
		bndPut("Huh? p % q = ", &h);
		goto failed;
	}

	if (f)
		genProgress(&progress, ' ');

	/* Search for a suitable h */
	if (bnSetQ(&h, 2) < 0 || bnTwoExpMod(g, &e, p) < 0)
		goto failed;
	i++;
	while (bnBits(g) < 2) {
		if (f)
			genProgress(&progress, '.');
		if (bnAddQ(&h, 1) < 0 || bnExpMod(g, &h, &e, p) < 0)
			goto failed;
		i++;
	}
	if (f)
		genProgress(&progress, '*');
#if CLOCK_AVAIL
	gettime(&stop);
#endif

	/*
	 * Now pick the secret, x.  Choose it a bit larger than q and do
	 * modular reduction to make it uniformly distributed.
	 */
	bnSetQ(x, 0);
	/* XXX SECURITY ALERT Replace with a real RNG! SECURITY ALERT XXX */
	if (genRandBn(x, qbits+8, 0, 0, seed, len) < 0)
		goto failed;
	if (bnMod(x, x, q) < 0 || bnExpMod(y, g, x, p) < 0)
		goto failed;
	i++;
	if (f)
		putc('\n', f);

	printf("%d modular exponentiations performed.\n", i);

#if CLOCK_AVAIL
	subtime(stop, start);
	s = sec(stop);
	bndPrintf("%u/%u-bit time = %lu.%03u sec.", pbits, qbits,
	          s, msec(stop));
	if (s > 60) {
		putchar(' ');
		putchar('(');
		if (s > 3600)
			printf("%u:%02u", (unsigned)(s/3600),
			       (unsigned)(s/60%60));
		else
			printf("%u", (unsigned)(s/60));
		printf(":%02u)", (unsigned)(s%60));
	}
	putchar('\n');
#endif

	bndPut("q = ", q);
	bndPut("p = ", p);
	bndPut("h = ", &h);
	bndPut("g = ", g);
	bndPut("x = ", x);
	bndPut("y = ", y);

	bnEnd(&h);
	bnEnd(&e);

	return 0;

failed:
	bnEnd(&h);
	bnEnd(&e);
	return -1;
}

static int
dsaSign(struct BigNum const *p, struct BigNum const *q, struct BigNum const *g,
	struct BigNum const *x, struct BigNum const *y,
	struct BigNum const *hash, struct BigNum const *k,
	struct BigNum *r, struct BigNum *s)
{
	int retval = -1;
	struct BigNum t;

	(void)y;

	bnBegin(&t);
	/* Make the signature...  first the precomputation */

	/* Compute r = (g^k mod p) mod q */
	if (bnExpMod(r, g, k, p) < 0 || bnMod(r, r, q) < 0)
		goto failed;

	/* Compute s = k^-1 * (hash + x*r) mod q */
	if (bnInv(&t, k, q) < 0)
		goto failed;
 	if (bnMul(s, x, r) < 0 || bnMod(s, s, q) < 0)
		goto failed;

	/* End of precomputation.  Steps after this require the hash. */

	if (bnAdd(s, hash) < 0)
		goto failed;
	if (bnCmp(s, q) > 0 && bnSub(s, q) < 0)
		goto failed;
	if (bnMul(s, s, &t) < 0 || bnMod(s, s, q) < 0)
		goto failed;
	/* Okay, r and s are the signature! */

	retval = 0;

failed:
	bnEnd(&t);
	return retval;
}

/* Faster version, using precomputed tables */
static int
dsaSignFast(struct BigNum const *p, struct BigNum const *q,
	    struct BnBasePrecomp const *pre,
	    struct BigNum const *x, struct BigNum const *y,
	    struct BigNum const *hash, struct BigNum const *k,
	    struct BigNum *r, struct BigNum *s)
{
	int retval = -1;
	struct BigNum t;

	(void)y;

	bnBegin(&t);
	/* Make the signature...  first the precomputation */

	/* Compute r = (g^k mod p) mod q */
	if (bnBasePrecompExpMod(r, pre, k, p) < 0 || bnMod(r, r, q) < 0)
		goto failed;

	/* Compute s = k^-1 * (hash + x*r) mod q */
	if (bnInv(&t, k, q) < 0)
		goto failed;
 	if (bnMul(s, x, r) < 0 || bnMod(s, s, q) < 0)
		goto failed;

	/* End of precomputation.  Steps after this require the hash. */

	if (bnAdd(s, hash) < 0)
		goto failed;
	if (bnCmp(s, q) > 0 && bnSub(s, q) < 0)
		goto failed;
	if (bnMul(s, s, &t) < 0 || bnMod(s, s, q) < 0)
		goto failed;
	/* Okay, r and s are the signature! */

	retval = 0;

failed:
	bnEnd(&t);
	return retval;
}

/*
 * Returns 1 for a good signature, 0 for bad, and -1 on error.
 */
static int
dsaVerify(struct BigNum const *p, struct BigNum const *q,
          struct BigNum const *g, struct BigNum const *y,
          struct BigNum const *r, struct BigNum const *s,
          struct BigNum const *hash)
{
	struct BigNum w, u1, u2;
	int retval = -1;

	bnBegin(&w);
	bnBegin(&u1);
	bnBegin(&u2);

	if (bnInv(&w, s, q) < 0)
		goto failed;

	if (bnMul(&u1, hash, &w) < 0 || bnMod(&u1, &u1, q) < 0)
		goto failed;
	if (bnMul(&u2, r, &w) < 0 || bnMod(&u2, &u2, q) < 0)
		goto failed;

	/* Now for the expensive part... */

	if (bnDoubleExpMod(&w, g, &u1, y, &u2, p) < 0)
		goto failed;
	if (bnMod(&w, &w, q) < 0)
		goto failed;
	retval = (bnCmp(r, &w) == 0);
failed:
	bnEnd(&u2);
	bnEnd(&u1);
	bnEnd(&w);
	return retval;
}

#define divide_by_n(sec, msec, n)	\
	( msec += 1000 * (sec % n),	\
	sec /= n, msec /= n,	\
	sec += msec / 1000,	\
	msec %= 1000 )

static int
dsaTest(struct BigNum const *p, struct BigNum const *q, struct BigNum const *g,
	struct BigNum const *x, struct BigNum const *y)
{
	struct BigNum hash, r, s, k;
	struct BigNum r1, s1;
	struct BnBasePrecomp pre;
	unsigned bits;
	unsigned i;
	int verified;
	int retval = -1;
	unsigned char foo[4], bar[4];
#if CLOCK_AVAIL
	timetype start, stop;
	unsigned long cursec, sigsec = 0, sig1sec = 0, versec = 0;
	unsigned curms, sigms = 0, sig1ms = 0, verms = 0;
	unsigned j, n, m = 0;
#endif

	bnBegin(&hash);
	bnBegin(&r); bnBegin(&r1);
	bnBegin(&s); bnBegin(&s1);
	bnBegin(&k);

	bits = bnBits(q);
	strcpy((char *)foo, "foo");
	strcpy((char *)bar, "bar");

	/* Precompute powers of g */
	if (bnBasePrecompBegin(&pre, g, p, bits) < 0)
		goto failed;

	bndPrintf(" N\tSigning \tSigning1\tVerifying\tStatus\n");
	for (i = 0; i < 25; i++) {
		/* Pick a random hash, the right length. */
		(void)bnSetQ(&k, 0);
		if (genRandBn(&hash, bits, 0, 0, foo, 4) < 0)
			goto failed;

		/* Make the signature... */

		/*
		 * XXX      SECURITY ALERT      XXX
		 * XXX Replace with a real RNG! XXX
		 * XXX      SECURITY ALERT      XXX
		 */
		(void)bnSetQ(&k, 0);
		if (genRandBn(&k, bnBits(q)+8, 0, 0, bar, 4) < 0)
			goto failed;
		/* Reduce k to the correct range */
		if (bnMod(&k, &k, q) < 0)
			goto failed;
#if CLOCK_AVAIL
		/* Decide on a number of iterations to perform... */
		m += n = i+1;	/* This goes from 1 to 325 */
		bndPrintf("%3d", n);
		gettime(&start);
		for (j = 0; j < n; j++)
#endif
			if (dsaSign(p, q, g, x, y, &hash, &k, &r, &s) < 0)
				goto failed;
#if CLOCK_AVAIL
		gettime(&stop);
		subtime(stop, start);
		sigsec += cursec = sec(stop);
		sigms += curms = msec(stop);
		divide_by_n(cursec, curms, n);
		bndPrintf("\t%lu.%03u\t\t", cursec, curms);
#else
		bndPrintf("\t*\t\t");
#endif
		fflush(stdout);

#if CLOCK_AVAIL
		gettime(&start);
		for (j = 0; j < n; j++)
#endif
			if (dsaSignFast(p, q, &pre, x, y, &hash, &k, &r1, &s1) < 0)
				goto failed;
#if CLOCK_AVAIL
		gettime(&stop);
		subtime(stop, start);
		sig1sec += cursec = sec(stop);
		sig1ms += curms = msec(stop);
		divide_by_n(cursec, curms, n);
		bndPrintf("%lu.%03u\t\t", cursec, curms);
#else
		bndPrintf("*\t\t");
#endif
		fflush(stdout);
		if (bnCmp(&r, &r1) != 0) {
			printf("\a** Error r != r1");
			bndPut("g = ", g);
			bndPut("k = ", &k);
			bndPut("r = ", &r);
			bndPut("r1= ", &r1);
		}
		if (bnCmp(&s, &s1) != 0) {
			printf("\a** Error r != r1");
			bndPut("g = ", g);
			bndPut("k = ", &k);
			bndPut("s = ", &s);
			bndPut("s1= ", &s1);
		}

		/* Okay, r and s are the signature!  Now, verify it.  */

#if CLOCK_AVAIL
		gettime(&start);
		verified = 0;	/* To silence warning */
		for (j = 0; j < n; j++) {
#endif
			verified = dsaVerify(p, q, g, y, &r, &s, &hash);
			if (verified <= 0)
				break;
		}
#if CLOCK_AVAIL
		gettime(&stop);
		subtime(stop, start);
		versec += cursec = sec(stop);
		verms += curms = msec(stop);
		divide_by_n(cursec, curms, j);
		bndPrintf("%lu.%03u\t\t", cursec, curms);
#else
		bndPrintf("*\t\t");
#endif
		if (verified > 0) {
			printf("Test successful.\n");
		} else if (verified == 0) {
			printf("\aSignature did NOT check!.\n");
			bndPut("hash = ", &hash);
			bndPut("k = ", &k);
			bndPut("r = ", &r);
			bndPut("s = ", &s);
			getchar();
		} else {
			printf("\a** Error while verifying");
			bndPut("hash = ", &hash);
			bndPut("k = ", &k);
			bndPut("r = ", &r);
			bndPut("s = ", &s);
			getchar();
			goto failed;
		}
	}
#if CLOCK_AVAIL
	divide_by_n(sigsec, sigms, m);
	divide_by_n(sig1sec, sig1ms, m);
	divide_by_n(versec, verms, m);

	bndPrintf("%3u\t%lu.%03u\t\t%lu.%03u\t\t%lu.%03u\t\tAVERAGE %u/%u\n",
	          m, sigsec, sigms, sig1sec, sig1ms, versec, verms,
		  bnBits(p), bnBits(q));
#endif
	/* Success */
	retval = 0;

failed:
	bnBasePrecompEnd(&pre);
	bnEnd(&k);
	bnEnd(&s1); bnEnd(&s);
	bnEnd(&r1); bnEnd(&r);
	bnEnd(&hash);

	return retval;
}

/* Copy the command line to the buffer. */
static unsigned
copy(unsigned char *buf, int argc, char **argv)
{
	unsigned pos, len;
	
	pos = 0;
	while (--argc) {
		len = strlen(*++argv);
		memcpy(buf, *argv, len);
		buf += len;
		pos += len;
		if (argc > 1) {
			*buf++ = ' ';
			pos++;
		}
	}
	return pos;
}

int
main(int argc, char **argv)
{
	unsigned len;
	struct BigNum p, q, g, x, y;
	unsigned char buf[1024];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <seed>\n", argv[0]);
		fputs("\
<seed> should be a a string of bytes to be hashed to seed the prime\n\
generator.  Note that unquoted whitespace between words will be counted\n\
as a single space.  To include multiple spaces, quote them.\n", stderr);
		return 1;
	}

	bnInit();
	bnBegin(&p);
	bnBegin(&q);
	bnBegin(&g);
	bnBegin(&x);
	bnBegin(&y);

	len = copy(buf, argc, argv);
	dsaGen(&p, 512, &q, 160, &g, &x, &y, buf, len, stdout);
	dsaTest(&p, &q, &g, &x, &y);

	len = copy(buf, argc, argv);
	dsaGen(&p, 768, &q, 160, &g, &x, &y, buf, len, stdout);
	dsaTest(&p, &q, &g, &x, &y);

	len = copy(buf, argc, argv);
	dsaGen(&p, 1024, &q, 160, &g, &x, &y, buf, len, stdout);
	dsaTest(&p, &q, &g, &x, &y);

	len = copy(buf, argc, argv);
	dsaGen(&p, 1536, &q, 192, &g, &x, &y, buf, len, stdout);
	dsaTest(&p, &q, &g, &x, &y);

	len = copy(buf, argc, argv);
	dsaGen(&p, 2048, &q, 224, &g, &x, &y, buf, len, stdout);
	dsaTest(&p, &q, &g, &x, &y);

	len = copy(buf, argc, argv);
	dsaGen(&p, 3072, &q, 256, &g, &x, &y, buf, len, stdout);
	dsaTest(&p, &q, &g, &x, &y);

	len = copy(buf, argc, argv);
	dsaGen(&p, 4096, &q, 288, &g, &x, &y, buf, len, stdout);
	dsaTest(&p, &q, &g, &x, &y);

	bnEnd(&y);
	bnEnd(&x);
	bnEnd(&g);
	bnEnd(&q);
	bnEnd(&p);

	return 0;
}
