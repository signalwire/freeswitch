/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * Prime generation using the bignum library and sieving.
 */
#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H 0
#endif
#if HAVE_CONFIG_H
#include "bnconfig.h"
#endif

/*
 * Some compilers complain about #if FOO if FOO isn't defined,
 * so do the ANSI-mandated thing explicitly...
 */
#ifndef NO_ASSERT_H
#define NO_ASSERT_H 0
#endif
#if !NO_ASSERT_H
#include <assert.h>
#else
#define assert(x) (void)0
#endif

#include <stdarg.h>	/* We just can't live without this... */

#ifndef BNDEBUG
#define BNDEBUG 1
#endif
#if BNDEBUG
#include <stdio.h>
#endif

#include "bn.h"
#include "lbnmem.h"
#include "prime.h"
#include "sieve.h"

#include "kludge.h"

/* Size of the shuffle table */
#define SHUFFLE	256
/* Size of the sieve area */
#define SIEVE 32768u/16

/* Confirmation tests.  The first one *must* be 2 */
static unsigned const confirm[] = {2, 3, 5, 7, 11, 13, 17};
#define CONFIRMTESTS (sizeof(confirm)/sizeof(*confirm))

/*
 * Helper function that does the slow primality test.
 * bn is the input bignum; a and e are temporary buffers that are
 * allocated by the caller to save overhead.
 *
 * Returns 0 if prime, >0 if not prime, and -1 on error (out of memory).
 * If not prime, returns the number of modular exponentiations performed.
 * Calls the given progress function with a '*' for each primality test
 * that is passed.
 *
 * The testing consists of strong pseudoprimality tests, to the bases given
 * in the confirm[] array above.  (Also called Miller-Rabin, although that's
 * not technically correct if we're using fixed bases.)  Some people worry
 * that this might not be enough.  Number theorists may wish to generate
 * primality proofs, but for random inputs, this returns non-primes with
 * a probability which is quite negligible, which is good enough.
 *
 * It has been proved (see Carl Pomerance, "On the Distribution of
 * Pseudoprimes", Math. Comp. v.37 (1981) pp. 587-593) that the number of
 * pseudoprimes (composite numbers that pass a Fermat test to the base 2)
 * less than x is bounded by:
 * exp(ln(x)^(5/14)) <= P_2(x)	### CHECK THIS FORMULA - it looks wrong! ###
 * P_2(x) <= x * exp(-1/2 * ln(x) * ln(ln(ln(x))) / ln(ln(x))).
 * Thus, the local density of Pseudoprimes near x is at most
 * exp(-1/2 * ln(x) * ln(ln(ln(x))) / ln(ln(x))), and at least
 * exp(ln(x)^(5/14) - ln(x)).  Here are some values of this function
 * for various k-bit numbers x = 2^k:
 * Bits	Density <=	Bit equivalent	Density >=	Bit equivalent
 *  128	3.577869e-07	 21.414396	4.202213e-37	 120.840190
 *  192	4.175629e-10	 31.157288	4.936250e-56	 183.724558
 *  256 5.804314e-13	 40.647940	4.977813e-75	 246.829095
 *  384 1.578039e-18	 59.136573	3.938861e-113	 373.400096
 *  512 5.858255e-24	 77.175803	2.563353e-151	 500.253110
 *  768 1.489276e-34	112.370944	7.872825e-228	 754.422724
 * 1024 6.633188e-45	146.757062	1.882404e-304	1008.953565
 *
 * As you can see, there's quite a bit of slop between these estimates.
 * In fact, the density of pseudoprimes is conjectured to be closer to the
 * square of that upper bound.  E.g. the density of pseudoprimes of size
 * 256 is around 3 * 10^-27.  The density of primes is very high, from
 * 0.005636 at 256 bits to 0.001409 at 1024 bits, i.e.  more than 10^-3.
 *
 * For those people used to cryptographic levels of security where the
 * 56 bits of DES key space is too small because it's exhaustible with
 * custom hardware searching engines, note that you are not generating
 * 50,000,000 primes per second on each of 56,000 custom hardware chips
 * for several hours.  The chances that another Dinosaur Killer asteroid
 * will land today is about 10^-11 or 2^-36, so it would be better to
 * spend your time worrying about *that*.  Well, okay, there should be
 * some derating for the chance that astronomers haven't seen it yet,
 * but I think you get the idea.  For a good feel about the probability
 * of various events, I have heard that a good book is by E'mile Borel,
 * "Les Probabilite's et la vie".  (The 's are accents, not apostrophes.)
 *
 * For more on the subject, try "Finding Four Million Large Random Primes",
 * by Ronald Rivest, in Advancess in Cryptology: Proceedings of Crypto
 * '90.  He used a small-divisor test, then a Fermat test to the base 2,
 * and then 8 iterations of a Miller-Rabin test.  About 718 million random
 * 256-bit integers were generated, 43,741,404 passed the small divisor
 * test, 4,058,000 passed the Fermat test, and all 4,058,000 passed all
 * 8 iterations of the Miller-Rabin test, proving their primality beyond
 * most reasonable doubts.
 *
 * If the probability of getting a pseudoprime is some small p, then the
 * probability of not getting it in t trials is (1-p)^t.  Remember that,
 * for small p, (1-p)^(1/p) ~ 1/e, the base of natural logarithms.
 * (This is more commonly expressed as e = lim_{x\to\infty} (1+1/x)^x.)
 * Thus, (1-p)^t ~ e^(-p*t) = exp(-p*t).  So the odds of being able to
 * do this many tests without seeing a pseudoprime if you assume that
 * p = 10^-6 (one in a million) is one in 57.86.  If you assume that
 * p = 2*10^-6, it's one in 3347.6.  So it's implausible that the density
 * of pseudoprimes is much more than one millionth the density of primes.
 *
 * He also gives a theoretical argument that the chance of finding a
 * 256-bit non-prime which satisfies one Fermat test to the base 2 is
 * less than 10^-22.  The small divisor test improves this number, and
 * if the numbers are 512 bits (as needed for a 1024-bit key) the odds
 * of failure shrink to about 10^-44.  Thus, he concludes, for practical
 * purposes *one* Fermat test to the base 2 is sufficient.
 */
static int
primeTest(struct BigNum const *bn, struct BigNum *e, struct BigNum *a,
	int (*f)(void *arg, int c), void *arg)
{
	unsigned i, j;
	unsigned k, l;
	int err;

#if BNDEBUG	/* Debugging */
	/*
	 * This is debugging code to test the sieving stage.
	 * If the sieving is wrong, it will let past numbers with
	 * small divisors.  The prime test here will still work, and
	 * weed them out, but you'll be doing a lot more slow tests,
	 * and presumably excluding from consideration some other numbers
	 * which might be prime.  This check just verifies that none
	 * of the candidates have any small divisors.  If this
	 * code is enabled and never triggers, you can feel quite
	 * confident that the sieving is doing its job.
	 */
	i = bnLSWord(bn);
	if (!(i % 2)) printf("bn div by 2!");
	i = bnModQ(bn, 51051);	/* 51051 = 3 * 7 * 11 * 13 * 17 */
	if (!(i % 3)) printf("bn div by 3!");
	if (!(i % 7)) printf("bn div by 7!");
	if (!(i % 11)) printf("bn div by 11!");
	if (!(i % 13)) printf("bn div by 13!");
	if (!(i % 17)) printf("bn div by 17!");
	i = bnModQ(bn, 63365);	/* 63365 = 5 * 19 * 23 * 29 */
	if (!(i % 5)) printf("bn div by 5!");
	if (!(i % 19)) printf("bn div by 19!");
	if (!(i % 23)) printf("bn div by 23!");
	if (!(i % 29)) printf("bn div by 29!");
	i = bnModQ(bn, 47027);	/* 47027 = 31 * 37 * 41 */
	if (!(i % 31)) printf("bn div by 31!");
	if (!(i % 37)) printf("bn div by 37!");
	if (!(i % 41)) printf("bn div by 41!");
#endif

	/*
	 * Now, check that bn is prime.  If it passes to the base 2,
	 * it's prime beyond all reasonable doubt, and everything else
	 * is just gravy, but it gives people warm fuzzies to do it.
	 *
	 * This starts with verifying Euler's criterion for a base of 2.
	 * This is the fastest pseudoprimality test that I know of,
	 * saving a modular squaring over a Fermat test, as well as
	 * being stronger.  7/8 of the time, it's as strong as a strong
	 * pseudoprimality test, too.  (The exception being when bn ==
	 * 1 mod 8 and 2 is a quartic residue, i.e. bn is of the form
	 * a^2 + (8*b)^2.)  The precise series of tricks used here is
	 * not documented anywhere, so here's an explanation.
	 * Euler's criterion states that if p is prime then a^((p-1)/2)
	 * is congruent to Jacobi(a,p), modulo p.  Jacobi(a,p) is
	 * a function which is +1 if a is a square modulo p, and -1 if
	 * it is not.  For a = 2, this is particularly simple.  It's
	 * +1 if p == +/-1 (mod 8), and -1 if m == +/-3 (mod 8).
	 * If p == 3 mod 4, then all a strong test does is compute
	 * 2^((p-1)/2). and see if it's +1 or -1.  (Euler's criterion
	 * says *which* it should be.)  If p == 5 (mod 8), then
	 * 2^((p-1)/2) is -1, so the initial step in a strong test,
	 * looking at 2^((p-1)/4), is wasted - you're not going to
	 * find a +/-1 before then if it *is* prime, and it shouldn't
	 * have either of those values if it isn't.  So don't bother.
	 *
	 * The remaining case is p == 1 (mod 8).  In this case, we
	 * expect 2^((p-1)/2) == 1 (mod p), so we expect that the
	 * square root of this, 2^((p-1)/4), will be +/-1 (mod p).
	 * Evaluating this saves us a modular squaring 1/4 of the time.
	 * If it's -1, a strong pseudoprimality test would call p
	 * prime as well.  Only if the result is +1, indicating that
	 * 2 is not only a quadratic residue, but a quartic one as well,
	 * does a strong pseudoprimality test verify more things than
	 * this test does.  Good enough.
	 *
	 * We could back that down another step, looking at 2^((p-1)/8)
	 * if there was a cheap way to determine if 2 were expected to
	 * be a quartic residue or not.  Dirichlet proved that 2 is
	 * a quartic residue iff p is of the form a^2 + (8*b^2).
	 * All primes == 1 (mod 4) can be expressed as a^2 + (2*b)^2,
	 * but I see no cheap way to evaluate this condition.
	 */
	if (bnCopy(e, bn) < 0)
		return -1;
	(void)bnSubQ(e, 1);
	l = bnLSWord(e);

	j = 1;	/* Where to start in prime array for strong prime tests */

	if (l & 7) {
		bnRShift(e, 1);
		if (bnTwoExpMod(a, e, bn) < 0)
			return -1;
		if ((l & 7) == 6) {
			/* bn == 7 mod 8, expect +1 */
			if (bnBits(a) != 1)
				return 1;	/* Not prime */
			k = 1;
		} else {
			/* bn == 3 or 5 mod 8, expect -1 == bn-1 */
			if (bnAddQ(a, 1) < 0)
				return -1;
			if (bnCmp(a, bn) != 0)
				return 1;	/* Not prime */
			k = 1;
			if (l & 4) {
				/* bn == 5 mod 8, make odd for strong tests */
				bnRShift(e, 1);
				k = 2;
			}
		}
	} else {
		/* bn == 1 mod 8, expect 2^((bn-1)/4) == +/-1 mod bn */
		bnRShift(e, 2);
		if (bnTwoExpMod(a, e, bn) < 0)
			return -1;
		if (bnBits(a) == 1) {
			j = 0;	/* Re-do strong prime test to base 2 */
		} else {
			if (bnAddQ(a, 1) < 0)
				return -1;
			if (bnCmp(a, bn) != 0)
				return 1;	/* Not prime */
		}
		k = 2 + bnMakeOdd(e);
	}
	/* It's prime!  Now go on to confirmation tests */

	/*
	 * Now, e = (bn-1)/2^k is odd.  k >= 1, and has a given value
	 * with probability 2^-k, so its expected value is 2.
	 * j = 1 in the usual case when the previous test was as good as
	 * a strong prime test, but 1/8 of the time, j = 0 because
	 * the strong prime test to the base 2 needs to be re-done.
	 */
	for (i = j; i < CONFIRMTESTS; i++) {
		if (f && (err = f(arg, '*')) < 0)
			return err;
		(void)bnSetQ(a, confirm[i]);
		if (bnExpMod(a, a, e, bn) < 0)
			return -1;
		if (bnBits(a) == 1)
			continue;	/* Passed this test */

		l = k;
		for (;;) {
			if (bnAddQ(a, 1) < 0)
				return -1;
			if (bnCmp(a, bn) == 0)	/* Was result bn-1? */
				break;	/* Prime */
			if (!--l)	/* Reached end, not -1? luck? */
				return i+2-j;	/* Failed, not prime */
			/* This portion is executed, on average, once. */
			(void)bnSubQ(a, 1);	/* Put a back where it was. */
			if (bnSquare(a, a) < 0 || bnMod(a, a, bn) < 0)
				return -1;
			if (bnBits(a) == 1)
				return i+2-j;	/* Failed, not prime */
		}
		/* It worked (to the base confirm[i]) */
	}
	
	/* Yes, we've decided that it's prime. */
	if (f && (err = f(arg, '*')) < 0)
		return err;
	return 0;	/* Prime! */
}

/*
 * Add x*y to bn, which is usually (but not always) < 65536.
 * Do it in a simple linear manner.
 */
static int
bnAddMult(struct BigNum *bn, unsigned x, unsigned y)
{
	unsigned long z = (unsigned long)x * y;

	while (z > 65535) {
		if (bnAddQ(bn, 65535) < 0)
			return -1;
		z -= 65535;
	}
	return bnAddQ(bn, (unsigned)z);
}

static int
bnSubMult(struct BigNum *bn, unsigned x, unsigned y)
{
	unsigned long z = (unsigned long)x * y;

	while (z > 65535) {
		if (bnSubQ(bn, 65535) < 0)
			return -1;
		z -= 65535;
	}
	return bnSubQ(bn, (unsigned)z);
}

/*
 * Modifies the bignum to return a nearby (slightly larger) number which
 * is a probable prime.  Returns >=0 on success or -1 on failure (out of
 * memory).  The return value is the number of unsuccessful modular
 * exponentiations performed.  This never gives up searching.
 *
 * All other arguments are optional.  They may be NULL.  They are:
 *
 * unsigned (*rand)(unsigned limit)
 * For better distributed numbers, supply a non-null pointer to a
 * function which returns a random x, 0 <= x < limit.  (It may make it
 * simpler to know that 0 < limit <= SHUFFLE, so you need at most a byte.)
 * The program generates a large window of sieve data and then does
 * pseudoprimality tests on the data.  If a rand function is supplied,
 * the candidates which survive sieving are shuffled with a window of
 * size SHUFFLE before testing to increase the uniformity of the prime
 * selection.  This isn't perfect, but it reduces the correlation between
 * the size of the prime-free gap before a prime and the probability
 * that that prime will be found by a sequential search.
 *
 * If rand is NULL, sequential search is used.  If you want sequential
 * search, note that the search begins with the given number; if you're
 * trying to generate consecutive primes, you must increment the previous
 * one by two before calling this again.
 *
 * int (*f)(void *arg, int c), void *arg
 * The function f argument, if non-NULL, is called with progress indicator
 * characters for printing.  A dot (.) is written every time a primality test
 * is failed, a star (*) every time one is passed, and a slash (/) in the
 * (very rare) case that the sieve was emptied without finding a prime
 * and is being refilled.  f is also passed the void *arg argument for
 * private context storage.  If f returns < 0, the test aborts and returns
 * that value immediately.  (bn is set to the last value tested, so you
 * can increment bn and continue.)
 *
 * The "exponent" argument, and following unsigned numbers, are exponents
 * for which an inverse is desired, modulo p.  For a d to exist such that
 * (x^e)^d == x (mod p), then d*e == 1 (mod p-1), so gcd(e,p-1) must be 1.
 * The prime returned is constrained to not be congruent to 1 modulo
 * any of the zero-terminated list of 16-bit numbers.  Note that this list
 * should contain all the small prime factors of e.  (You'll have to test
 * for large prime factors of e elsewhere, but the chances of needing to
 * generate another prime are low.)
 *
 * The list is terminated by a 0, and may be empty.
 */
int
primeGen(struct BigNum *bn, unsigned (*rand)(unsigned),
         int (*f)(void *arg, int c), void *arg, unsigned exponent, ...)
{
	int retval;
	int modexps = 0;
	unsigned short offsets[SHUFFLE];
	unsigned i, j;
	unsigned p, q, prev;
	struct BigNum a, e;
#ifdef MSDOS
	unsigned char *sieve;
#else
	unsigned char sieve[SIEVE];
#endif

#ifdef MSDOS
	sieve = lbnMemAlloc(SIEVE);
	if (!sieve)
		return -1;
#endif

	bnBegin(&a);
	bnBegin(&e);

#if 0	/* Self-test (not used for production) */
{
	struct BigNum t;
	static unsigned char const prime1[] = {5};
	static unsigned char const prime2[] = {7};
	static unsigned char const prime3[] = {11};
	static unsigned char const prime4[] = {1, 1}; /* 257 */
	static unsigned char const prime5[] = {0xFF, 0xF1}; /* 65521 */
	static unsigned char const prime6[] = {1, 0, 1}; /* 65537 */
	static unsigned char const prime7[] = {1, 0, 3}; /* 65539 */
	/* A small prime: 1234567891 */
	static unsigned char const prime8[] = {0x49, 0x96, 0x02, 0xD3};
	/* A slightly larger prime: 12345678901234567891 */
	static unsigned char const prime9[] = {
		0xAB, 0x54, 0xA9, 0x8C, 0xEB, 0x1F, 0x0A, 0xD3 };
	/*
	 * No, 123456789012345678901234567891 isn't prime; it's just a
	 * lucky, easy-to-remember conicidence.  (You have to go to
	 * ...4567907 for a prime.)
	 */
	static struct {
		unsigned char const *prime;
		unsigned size;
	} const primelist[] = {
		{ prime1, sizeof(prime1) },
		{ prime2, sizeof(prime2) },
		{ prime3, sizeof(prime3) },
		{ prime4, sizeof(prime4) },
		{ prime5, sizeof(prime5) },
		{ prime6, sizeof(prime6) },
		{ prime7, sizeof(prime7) },
		{ prime8, sizeof(prime8) },
		{ prime9, sizeof(prime9) } };

	bnBegin(&t);

	for (i = 0; i < sizeof(primelist)/sizeof(primelist[0]); i++) {
			bnInsertBytes(&t, primelist[i].prime, 0,
				      primelist[i].size);
			bnCopy(&e, &t);
			(void)bnSubQ(&e, 1);
			bnTwoExpMod(&a, &e, &t);
			p = bnBits(&a);
			if (p != 1) {
				printf(
			"Bug: Fermat(2) %u-bit output (1 expected)\n", p);
				fputs("Prime = 0x", stdout);
				for (j = 0; j < primelist[i].size; j++)
					printf("%02X", primelist[i].prime[j]);
				putchar('\n');
			}
			bnSetQ(&a, 3);
			bnExpMod(&a, &a, &e, &t);
			p = bnBits(&a);
			if (p != 1) {
				printf(
			"Bug: Fermat(3) %u-bit output (1 expected)\n", p);
				fputs("Prime = 0x", stdout);
				for (j = 0; j < primelist[i].size; j++)
					printf("%02X", primelist[i].prime[j]);
				putchar('\n');
			}
		}

	bnEnd(&t);
}
#endif

	/* First, make sure that bn is odd. */
	if ((bnLSWord(bn) & 1) == 0)
		(void)bnAddQ(bn, 1);

retry:
	/* Then build a sieve starting at bn. */
	sieveBuild(sieve, SIEVE, bn, 2, 0);

	/* Do the extra exponent sieving */
	if (exponent) {
		va_list ap;
		unsigned t = exponent;

		va_start(ap, exponent);

		do {
			/* The exponent had better be odd! */
			assert(t & 1);

			i = bnModQ(bn, t);
			/* Find 1-i */
			if (i == 0)
				i = 1;
			else if (--i)
				i = t - i;

			/* Divide by 2, modulo the exponent */
			i = (i & 1) ? i/2 + t/2 + 1 : i/2;

			/* Remove all following multiples from the sieve. */
			sieveSingle(sieve, SIEVE, i, t);

			/* Get the next exponent value */
			t = va_arg(ap, unsigned);
		} while (t);

		va_end(ap);
	}

	/* Fill up the offsets array with the first SHUFFLE candidates */
	i = p = 0;
	/* Get first prime */
	if (sieve[0] & 1 || (p = sieveSearch(sieve, SIEVE, p)) != 0) {
		offsets[i++] = p;
		p = sieveSearch(sieve, SIEVE, p);
	}
	/*
	 * Okay, from this point onwards, p is always the next entry
	 * from the sieve, that has not been added to the shuffle table,
	 * and is 0 iff the sieve has been exhausted.
	 *
	 * If we want to shuffle, then fill the shuffle table until the
	 * sieve is exhausted or the table is full.
	 */
	if (rand && p) {
		do {
			offsets[i++] = p;
			p = sieveSearch(sieve, SIEVE, p);
		} while (p && i < SHUFFLE);
	}

	/* Choose a random candidate for experimentation */
	prev = 0;
	while (i) {
		/* Pick a random entry from the shuffle table */
		j = rand ? rand(i) : 0;
		q = offsets[j];	/* The entry to use */

		/* Replace the entry with some more data, if possible */
		if (p) {
			offsets[j] = p;
			p = sieveSearch(sieve, SIEVE, p);
		} else {
			offsets[j] = offsets[--i];
			offsets[i] = 0;
		}

		/* Adjust bn to have the right value */
		if ((q > prev ? bnAddMult(bn, q-prev, 2)
		              : bnSubMult(bn, prev-q, 2)) < 0)
			goto failed;
		prev = q;

		/* Now do the Fermat tests */
		retval = primeTest(bn, &e, &a, f, arg);
		if (retval <= 0)
			goto done;	/* Success or error */
		modexps += retval;
		if (f && (retval = f(arg, '.')) < 0)
			goto done;
	}

	/* Ran out of sieve space - increase bn and keep trying. */
	if (bnAddMult(bn, SIEVE*8-prev, 2) < 0)
		goto failed;
	if (f && (retval = f(arg, '/')) < 0)
		goto done;
	goto retry;

failed:
	retval = -1;
done:
	bnEnd(&e);
	bnEnd(&a);
	lbnMemWipe(offsets, sizeof(offsets));
#ifdef MSDOS
	lbnMemFree(sieve, SIEVE);
#else
	lbnMemWipe(sieve, sizeof(sieve));
#endif

	return retval < 0 ? retval : modexps + CONFIRMTESTS;
}

/*
 * Similar, but searches forward from the given starting value in steps of
 * "step" rather than 1.  The step size must be even, and bn must be odd.
 * Among other possibilities, this can be used to generate "strong"
 * primes, where p-1 has a large prime factor.
 */
int
primeGenStrong(struct BigNum *bn, struct BigNum const *step,
	int (*f)(void *arg, int c), void *arg)
{
	int retval;
	unsigned p, prev;
	struct BigNum a, e;
	int modexps = 0;
#ifdef MSDOS
	unsigned char *sieve;
#else
	unsigned char sieve[SIEVE];
#endif

#ifdef MSDOS
	sieve = lbnMemAlloc(SIEVE);
	if (!sieve)
		return -1;
#endif

	/* Step must be even and bn must be odd */
	assert((bnLSWord(step) & 1) == 0);
	assert((bnLSWord(bn) & 1) == 1);

	bnBegin(&a);
	bnBegin(&e);

	for (;;) {
		if (sieveBuildBig(sieve, SIEVE, bn, step, 0) < 0)
			goto failed;

		p = prev = 0;
		if (sieve[0] & 1 || (p = sieveSearch(sieve, SIEVE, p)) != 0) {
			do {
				/*
				 * Adjust bn to have the right value,
				 * adding (p-prev) * 2*step.
				 */
				assert(p >= prev);
				/* Compute delta into a */
				if (bnMulQ(&a, step, p-prev) < 0)
					goto failed;
				if (bnAdd(bn, &a) < 0)
					goto failed;
				prev = p;

				retval = primeTest(bn, &e, &a, f, arg);
				if (retval <= 0)
					goto done;	/* Success! */
				modexps += retval;
				if (f && (retval = f(arg, '.')) < 0)
					goto done;

				/* And try again */
				p = sieveSearch(sieve, SIEVE, p);
			} while (p);
		}

		/* Ran out of sieve space - increase bn and keep trying. */
#if SIEVE*8 == 65536
		/* Corner case that will never actually happen */
		if (!prev) {
			if (bnAdd(bn, step) < 0)
				goto failed;
			p = 65535;
		} else {
			p = (unsigned)(SIEVE*8 - prev);
		}
#else
		p = SIEVE*8 - prev;
#endif
		if (bnMulQ(&a, step, p) < 0 || bnAdd(bn, &a) < 0)
			goto failed;
		if (f && (retval = f(arg, '/')) < 0)
			goto done;
	} /* for (;;) */

failed:
	retval = -1;

done:

	bnEnd(&e);
	bnEnd(&a);
#ifdef MSDOS
	lbnMemFree(sieve, SIEVE);
#else
	lbnMemWipe(sieve, sizeof(sieve));
#endif
	return retval < 0 ? retval : modexps + CONFIRMTESTS;
}
