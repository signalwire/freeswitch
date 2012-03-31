/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * keygen.c - generate RSA key pairs using the bignum library.
 */
#include "first.h"
#include <assert.h>
#include <stdio.h>	/* For FILE type */
#include <string.h>	/* For memset */

#include "bn.h"
#include "prime.h"

#include "keygen.h"
#include "keys.h"	/* Key structures */
#include "random.h"	/* Good random number generator */

#if BNDEBUG
#include "bnprint.h"
#define bndPut(prompt, bn) bnPrint(stdout, prompt, bn, "\n")
#define bndPrintf printf
#else
#define bndPut(prompt, bn) ((void)(prompt),(void)(bn))
#define bndPrintf (void)
#endif

#include "kludge.h"


/*
 * Generate a random bignum of a specified length, with the given
 * high and low 8 bits. "High" is merged into the high 8 bits of the
 * number.  For example, set it to 0x80 to ensure that the number is
 * exactly "bits" bits long (i.e. 2^(bits-1) <= bn < 2^bits).
 * "Low" is merged into the low 8 bits.  For example, set it to
 * 1 to ensure that you generate an odd number.
 */
static int
genRandBn(struct BigNum *bn, unsigned bits, byte high, byte low)
{
	unsigned char buf[64];
	unsigned bytes;
	unsigned l;
	int err;

	bnSetQ(bn, 0);

	bytes = (bits+7) / 8;
	l = bytes < sizeof(buf) ? bytes : sizeof(buf);
	randBytes(buf, l);

	/* Mask off excess high bits */
	buf[0] &= 255 >> (-bits & 7);
	/* Merge in specified high bits */
	buf[0] |= high >> (-bits & 7);
	if (bits & 7)
		buf[1] |= high << (bits & 7);

	for (;;) {
		bytes -= l;
		if (!bytes)	/* Last word - merge in low bits */
			buf[l-1] |= low;
		err = bnInsertBigBytes(bn, buf, bytes, l);
		if (!bytes || err < 0)
			break;
		l = bytes < sizeof(buf) ? bytes : sizeof(buf);
		randBytes(buf, l);
	}

	memset(buf, 0, sizeof(buf));
	return err;
}


/*
 * Generate a new RSA key, with the specified number of bits and
 * public exponent.  The high two bits of each prime are always
 * set to make the number more difficult to factor by forcing the
 * number into the high end of the range.
 */

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

int
genRsaKey(struct PubKey *pub, struct SecKey *sec,
	  unsigned bits, unsigned exp, FILE *file)
{
	int modexps = 0;
	struct BigNum t;	/* Temporary */
	int i;
	struct Progress progress;

	progress.f = file;
	progress.column = 0;
	progress.wrap = 78;

	if (bnSetQ(&pub->e, exp))
		return -1;

	/* Find p - choose a starting place */
	if (genRandBn(&sec->p, bits/2, 0xC0, 1) < 0)
		return -1;
	/* And search for a prime */
	i = primeGen(&sec->p, randRange, file ? genProgress : 0, &progress,
	             exp, 0);
	if (i < 0)
		goto error;
	modexps = i;
	assert(bnModQ(&sec->p, exp) != 1);
bndPut("p = ", &sec->p);

	do {
		/* Visual separator between the two progress indicators */
		if (file)
			genProgress(&progress, ' ');

		if (genRandBn(&sec->q, (bits+1)/2, 0xC0, 1) < 0)
			goto error;
		if (bnCopy(&pub->n, &sec->q) < 0)
			goto error;
		if (bnSub(&pub->n, &sec->p) < 0)
			goto error;
		/* Note that bnSub(a,b) returns abs(a-b) */
	} while (bnBits(&pub->n) < bits/2-5);

	if (file)
		fflush(file);	/* Ensure the separators are visible */

	i = primeGen(&sec->q, randRange, file ? genProgress : 0, &progress,
	             exp, 0);
	if (i < 0)
		goto error;
	modexps += i;
	assert(bnModQ(&sec->p, exp) != 1);
bndPut("q = ", &sec->q);

	/* Wash the random number pool. */
	randFlush();

	/* Ensure that q is larger */
	if (bnCmp(&sec->p, &sec->q) > 0)
		bnSwap(&sec->p, &sec->q);
bndPut("p = ", &sec->p);
bndPut("q = ", &sec->q);


	/*
	 * Now we dive into a large amount of fiddling to compute d,
	 * the decryption exponent, from the encryption exponent.
	 * We require that e*d == 1 (mod p-1) and e*d == 1 (mod q-1).
	 * This can alomost be done via the Chinese Remainder Algorithm,
	 * but it doesn't quite apply, because p-1 and q-1 are not
	 * realitvely prime.  Our task is to massage these into
	 * two numbers a and b such that a*b = lcm(p-1,q-1) and
	 * gcd(a,b) = 1.  The technique is not well documented,
	 * so I'll describe it here.
	 * First, let d = gcd(p-1,q-1), then let a' = (p-1)/d and
	 * b' = (q-1)/d.  By the definition of the gcd, gcd(a',b') at
	 * this point is 1, but a'*b' is a factor of d shy of the desired
	 * value.  We have to produce a = a' * d1 and b = b' * d2 such
	 * d1*d2 = d and gcd(a,b) is 1.  This will be the case iff
	 * gcd(a,d2) = gcd(b,d1) = 1.  Since GCD is associative and
	 * (gcd(x,y,z) = gcd(x,gcd(y,z)) = gcd(gcd(x,y),z), etc.),
	 * gcd(a',b') = 1 implies that gcd(a',b',d) = 1 which implies
	 * that gcd(a',gcd(b',d)) = gcd(gcd(a',d),b') = 1.  So you can
	 * extract gcd(b',d) from d and make it part of d2, and the
	 * same for d1.  And iterate?  A pessimal example is x = 2*6^k
	 * and y = 3*6^k.  gcd(x,y) = 6^k and we have to divvy it up
	 * somehow so that all the factors of 2 go to x and all the
	 * factors of 3 go to y, ending up with a = 2*2^k and b = 3*3^k.
	 *
	 * Aah, fuck it.  It's simpler to do one big inverse for now.
	 * Later I'll figure out how to get this to work properly.
	 */

	/* Decrement q temporarily */
	(void)bnSubQ(&sec->q, 1);
	/* And u = p-1, to be divided by gcd(p-1,q-1) */
	if (bnCopy(&sec->u, &sec->p) < 0)
		goto error;
	(void)bnSubQ(&sec->u, 1);
bndPut("p-1 = ", &sec->u);
bndPut("q-1 = ", &sec->q);
	/* Use t to store gcd(p-1,q-1) */
	bnBegin(&t);
	if (bnGcd(&t, &sec->q, &sec->u) < 0) {
		bnEnd(&t);
		goto error;
	}
bndPut("t = gcd(p-1,q-1) = ", &t);

	/* Let d = (p-1) / gcd(p-1,q-1) (n is scratch for the remainder) */
	i = bnDivMod(&sec->d, &pub->n, &sec->u, &t);
bndPut("(p-1)/t = ", &sec->d);
bndPut("(p-1)%t = ", &pub->n);
	bnEnd(&t);
	if (i < 0)
		goto error;
	assert(bnBits(&pub->n) == 0);
	/* Now we have q-1 and d = (p-1) / gcd(p-1,q-1) */
	/* Find the product, n = lcm(p-1,q-1) = c * d */
	if (bnMul(&pub->n, &sec->q, &sec->d) < 0)
		goto error;
bndPut("(p-1)*(q-1)/t = ", &pub->n);
	/* Find the inverse of the exponent mod n */
	i = bnInv(&sec->d, &pub->e, &pub->n);
bndPut("e = ", &pub->e);
bndPut("d = ", &sec->d);
	if (i < 0)
		goto error;
	assert(!i);	/* We should NOT get an error here */
	/*
	 * Now we have the comparatively simple task of computing
	 * u = p^-1 mod q.
	 */
#if BNDEBUG
	bnMul(&sec->u, &sec->d, &pub->e);
bndPut("d * e = ", &sec->u);
	bnMod(&pub->n, &sec->u, &sec->q);
bndPut("d * e = ", &sec->u);
bndPut("q-1 = ", &sec->q);
bndPut("d * e % (q-1)= ", &pub->n);
	bnNorm(&pub->n);
	bnSubQ(&sec->p, 1);
bndPut("d * e = ", &sec->u);
	bnMod(&sec->u, &sec->u, &sec->p);
bndPut("p-1 = ", &sec->p);
bndPut("d * e % (p-1)= ", &sec->u);
	bnNorm(&sec->u);
	bnAddQ(&sec->p, 1);
#endif

	/* But it *would* be nice to have q back first. */
	(void)bnAddQ(&sec->q, 1);

bndPut("p = ", &sec->p);
bndPut("q = ", &sec->q);

	/* Now compute u = p^-1 mod q */
	i = bnInv(&sec->u, &sec->p, &sec->q);
	if (i < 0)
		goto error;
bndPut("u = p^-1 % q = ", &sec->u);
	assert(!i);	/* p and q had better be relatively prime! */

#if BNDEBUG
	bnMul(&pub->n, &sec->u, &sec->p);
bndPut("u * p = ", &pub->n);
	bnMod(&pub->n, &pub->n, &sec->q);
bndPut("u * p % q = ", &pub->n);
	bnNorm(&pub->n);
#endif
	/* And finally,  n = p * q */
	if (bnMul(&pub->n, &sec->p, &sec->q) < 0)
		goto error;
bndPut("n = p * q = ", &pub->n);
	/* And that's it... success! */
	if (file)
		putc('\n', file);	/* Signal done */
	return modexps;

error:
	if (file)
		fputs("?\n", file);	/* Signal error */

	return -1;
}

/*
 * Chinese Remainder Theorem refresher.
 * The theorem is actually that, "given x mod a, x mod b, x mod c, x mod d,
 * etc., the value of x mod lcm(a, b, c, d, ...) is uniquely determined",
 * But everyone seems to use the name "theorem" to refer to the algorithm
 * to put the number back together.
 *
 * Doing it for multiple numbers efficiently is a bit hairier, so I'll
 * just consider it for two moduli, a and b.  We assume that the inputs
 * are in the canonical equivalence class (0 <= xa = x mod a < a, and
 * 0 <= xb = x mod b < b), and we want the output in the same form.
 *
 * First, divide one or the other by gcd(a,b) to reduce the problem to
 * one of relatively prime numbers.  You'll have to reduce the corresponding
 * xa or xb modulo the new modulus.
 *
 * Then, note that if xa == x (mod a), then x = xa + a*k.  The problem
 * lies in finding k so that xa + a*k == xb (mod b).  Rearranging
 * gives a*k == xb - xa (mod b), and then multiplying both sides by
 * a^-1, the inverse of a mod b, gives k == a^-1 * (xb-xa) (mod b).
 * If k is reduced mod b, then xa + a*k <= (a-1) + a * (b-1) =
 * a + a*(b-1) - 1 = a*b - 1, which is exactly as it should be to
 * be reduced mod a*b.  And if all the inputs are >= 0, the output
 * will be non-negative.
 *
 * For multiple numbers, you can get the number into a similar mixed-
 * radix form x = xa + a*(k1 + b*(k2 + c*(k3 +...))).  All the math
 * to do this is modulo the small numbers (and thus faster); only the
 * final summing has to be performed at large sizes.  For the greatest
 * efficiency, order the numbers so a > b > c >..., so as many computations
 * as possible are small.
 *
 * So the total procedure for two numbers is:
 * - Let a be the larger and b be the smaller of the numbers.
 * - Divide b by gcd(a,b) to make it even smaller.
 * - Find a^-1 mod b.
 * - Find (xb-xa) mod b.
 * - Multiply (xb-xa) by a^-1, modulo b.
 * - Multiply that by a, without any modular reduction
 * - Add xa.
 */

#if 0
/* A simple test driver */

#include "bnprint.h"
#include <time.h>
int
main(void)
{
	struct BigNum p, q, d, u;
	int i;
	clock_t interval;
	static unsigned const sizetable[] = {
		384, 512, 513, 514, 515, 768, 1024, 1536, 2048, 0
	};
	unsigned const *sizeptr = sizetable;

	bnInit();
	bnRandSeed(1);
	bnBegin(&p);
	bnBegin(&q);
	bnBegin(&d);
	bnBegin(&u);

	while (*sizeptr) {
		printf("Generating a %u-bit RSA key\n", *sizeptr);

		interval = clock();
		i = genRsaKey(&p, &q, &d, &u, *sizeptr, 17, stdout);
		interval = clock() - interval;
		printf("genRsaKey returned %d.  %ld.%06ld s\n", i,
			interval / 1000000, interval % 1000000);
		fputs("p = ", stdout);
		bnPrint(stdout, &p);
		fputs("\nq = ", stdout);
		bnPrint(stdout, &q);
		fputs("\nd = ", stdout);
		bnPrint(stdout, &d);
		fputs("\nu = ", stdout);
		bnPrint(stdout, &u);
		putchar('\n');

		sizeptr++;
	}

	bnEnd(&p);
	bnEnd(&q);
	bnEnd(&d);
	bnEnd(&u);

	return 0;
}
#endif
