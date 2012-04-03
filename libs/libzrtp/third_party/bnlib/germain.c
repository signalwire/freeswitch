/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * Sophie Germain prime generation using the bignum library and sieving.
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

#define BNDEBUG 1
#ifndef BNDEBUG
#define BNDEBUG 0
#endif
#if BNDEBUG
#include <stdio.h>
#endif

#include "bn.h"
#include "germain.h"
#include "jacobi.h"
#include "lbnmem.h"	/* For lbnMemWipe */
#include "sieve.h"

#include "kludge.h"

/* Size of the sieve area (can be up to 65536/8 = 8192) */
#define SIEVE 8192

static unsigned const confirm[] = {2, 3, 5, 7, 11, 13, 17};
#define CONFIRMTESTS (sizeof(confirm)/sizeof(*confirm))

#if BNDEBUG
/*
 * For sanity checking the sieve, we check for small divisors of the numbers
 * we get back.  This takes "rem", a partially reduced form of the prime,
 * "div" a divisor to check for, and "order", a parameter of the "order"
 * of Sophie Germain primes (0 = normal primes, 1 = Sophie Germain primes,
 * 2 = 4*p+3 is also prime, etc.) and does the check.  It just complains
 * to stdout if the check fails.
 */
static void
germainSanity(unsigned rem, unsigned div, unsigned order)
{
	unsigned mul = 1;

	rem %= div;
	if (!rem)
		printf("bn div by %u!\n", div);
	while (order--) {
		rem += rem+1;
		if (rem >= div)
			rem -= div;
		mul += mul;
		if (!rem)
			printf("%u*bn+%u div by %u!\n", mul, mul-1, div);
	}
}
#endif /* BNDEBUG */

/*
 * Helper function that does the slow primality test.
 * bn is the input bignum; a, e and bn2 are temporary buffers that are
 * allocated by the caller to save overhead.  bn2 is filled with
 * a copy of 2^order*bn+2^order-1 if bn is found to be prime.
 *
 * Returns 0 if both bn and bn2 are prime, >0 if not prime, and -1 on
 * error (out of memory).  If not prime, the return value is the number
 * of modular exponentiations performed.   Prints a '+' or '-' on the
 * given FILE (if any) for each test that is passed by bn, and a '*'
 * for each test that is passed by bn2.
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
germainPrimeTest(struct BigNum const *bn, struct BigNum *bn2, struct BigNum *e,
	struct BigNum *a, unsigned order, int (*f)(void *arg, int c), void *arg)
{
	int err;
	unsigned i;
	int j;
	unsigned k, l, n;

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
	germainSanity(i, 3, order);
	germainSanity(i, 7, order);
	germainSanity(i, 11, order);
	germainSanity(i, 13, order);
	germainSanity(i, 17, order);
	i = bnModQ(bn, 63365);	/* 63365 = 5 * 19 * 23 * 29 */
	germainSanity(i, 5, order);
	germainSanity(i, 19, order);
	germainSanity(i, 23, order);
	germainSanity(i, 29, order);
	i = bnModQ(bn, 47027);	/* 47027 = 31 * 37 * 41 */
	germainSanity(i, 31, order);
	germainSanity(i, 37, order);
	germainSanity(i, 41, order);
#endif
	/*
	 * First, check whether bn is prime.  This uses a fast primality
	 * test which usually obviates the need to do one of the
	 * confirmation tests later.  See prime.c for a full explanation.
	 * We check bn first because it's one bit smaller, saving one
	 * modular squaring, and because we might be able to save another
	 * when testing it.  (1/4 of the time.)  A small speed hack,
	 * but finding big Sophie Germain primes is *slow*.
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


	/*
	 * It's prime!  Now check higher-order forms bn2 = 2*bn+1, 4*bn+3,
	 * etc.  Since bn2 == 3 mod 4, a strong pseudoprimality test boils
	 * down to looking at a^((bn2-1)/2) mod bn and seeing if it's +/-1.
	 * (+1 if bn2 is == 7 mod 8, -1 if it's == 3)
	 * Of course, that exponent is just the previous bn2 or bn...
	 */
	if (bnCopy(bn2, bn) < 0)
			return -1;
	for (n = 0; n < order; n++) {
		/*
		 * Print a success indicator: the sign of Jacobi(2,bn2),
		 * which is available to us in l.  bn2 = 2*bn + 1.  Since bn
		 * is odd, bn2 must be == 3 mod 4, so the options modulo 8
		 * are 3 and 7.  3 if l == 1 mod 4, 7 if l == 3 mod 4.
		 * The sign of the Jacobi symbol is - and + for these cases,
		 * respectively.
		 */
		if (f && (err = f(arg, "-+"[(l >> 1) & 1])) < 0)
			return err;
		/* Exponent is previous bn2 */
		if (bnCopy(e, bn2) < 0 || bnLShift(bn2, 1) < 0)
			return -1;
		(void)bnAddQ(bn2, 1);	/* Can't overflow */
		if (bnTwoExpMod(a, e, bn2) < 0)
			return -1;
		if (n | l) {	/* Expect + */
			if (bnBits(a) != 1)
				return 2+n;	/* Not prime */
		} else {
			if (bnAddQ(a, 1) < 0)
				return -1;
			if (bnCmp(a, bn2) != 0)
				return 2+n;	/* Not prime */
		}
		l = bnLSWord(bn2);
	}

	/* Final success indicator - it's in the bag. */
	if (f && (err = f(arg, '*')) < 0)
		return err;
	
	/*
	 * Success!  We have found a prime!  Now go on to confirmation
	 * tests...  k is an amount by which we know it's safe to shift
	 * down e.  j = 1 unless the test to the base 2 could stand to be
	 * re-done (it wasn't *quite* a strong test), in which case it's 0.
	 *
	 * Here, we do the full strong pseudoprimality test.  This proves
	 * that a number is composite, or says that it's probably prime.
	 *
	 * For the given base a, find bn-1 = 2^k * e, then find
	 * x == a^e (mod bn).
	 * If x == +1 -> strong pseudoprime to base a
	 * Otherwise, repeat k times:
	 *   If x == -1, -> strong pseudoprime
	 *   x = x^2 (mod bn)
	 *   If x = +1 -> composite
	 * If we reach the end of the iteration and x is *not* +1, at the
	 * end, it is composite.  But it's also composite if the result
	 * *is* +1.  Which means that the squaring actually only has to
	 * proceed k-1 times.  If x is not -1 by then, it's composite
	 * no matter what the result of the squaring is.
	 *
	 * For the multiples 2*bn+1, 4*bn+3, etc. then k = 1 (and e is
	 * the previous multiple of bn) so the squaring loop is never
	 * actually executed at all.
	 */
	for (i = j; i < CONFIRMTESTS; i++) {
		if (bnCopy(e, bn) < 0)
				return -1;
		bnRShift(e, k);
		k += bnMakeOdd(e);
		(void)bnSetQ(a, confirm[i]);
		if (bnExpMod(a, a, e, bn) < 0)
			return -1;

		if (bnBits(a) != 1) {
			l = k;
			for (;;) {
				if (bnAddQ(a, 1) < 0)
					return -1;
				if (bnCmp(a, bn) == 0)	/* Was result bn-1? */
					break;	/* Prime */
				if (!--l)
					return (1+order)*i+2;	/* Fail */
				/* This part is executed once, on average. */
				(void)bnSubQ(a, 1);	/* Restore a */
				if (bnSquare(a, a) < 0 || bnMod(a, a, bn) < 0)
					return -1;
				if (bnBits(a) == 1)
					return (1+order)*i+1;	/* Fail */
			}
		}

		if (bnCopy(bn2, bn) < 0)
			return -1;
	
		/* Only do the following if we're not re-doing base 2 */
		if (i) for (n = 0; n < order; n++) {
			if (bnCopy(e, bn2) < 0 || bnLShift(bn2, 1) < 0)
				return -1;
			(void)bnAddQ(bn2, 1);

			/* Print success indicator for previous test */
			j = bnJacobiQ(confirm[i], bn2);
			if (f && (err = f(arg, j < 0 ? '-' : '+')) < 0)
				return err;

			/* Check that p^e == Jacobi(p,bn2) (mod bn2) */
			(void)bnSetQ(a, confirm[i]);
			if (bnExpMod(a, a, e, bn2) < 0)
				return -1;
			/*
			 * FIXME:  Actually, we don't need to compute the
			 * Jacobi symbol externally... it never happens that
			 * a = +/-1 but it's the wrong one.  So we can just
			 * look at a and use its sign.  Find a proof somewhere.
			 */
			if (j < 0) {
				/* Not a Q.R., should have a =  bn2-1 */
				if (bnAddQ(a, 1) < 0)
					return -1;
				if (bnCmp(a, bn2) != 0)	/* Was result bn2-1? */
					return (1+order)*i+n+2;	/* Fail */
			} else {
				/* Quadratic residue, should have a = 1 */
				if (bnBits(a) != 1)
					return (1+order)*i+n+2;	/* Fail */
			}
		}
		/* Final success indicator for the base confirm[i]. */
		if (f && (err = f(arg, '*')) < 0)
			return err;
	}

	return 0;	/* Prime! */
}

/*
 * Add x*y to bn, which is usually (but not always) < 65536.
 * Do it in a simple linear manner.
 */
static int
bnAddMult(struct BigNum *bn, unsigned long x, unsigned y)
{
	unsigned long z = (unsigned long)x * y;

	while (z > 65535) {
		if (bnAddQ(bn, 65535) < 0)
			return -1;
		z -= 65535;
	}
	return bnAddQ(bn, (unsigned)z);
}

/*
 * Modifies the bignum to return the next Sophie Germain prime >= the
 * input value.  Sohpie Germain primes are number such that p is
 * prime and 2*p+1 is also prime.
 *
 * This is actually parameterized: it generates primes p such that "order"
 * multiples-plus-two are also prime, 2*p+1, 2*(2*p+1)+1 = 4*p+3, etc.
 *
 * Returns >=0 on success or -1 on failure (out of memory).  On success,
 * the return value is the number of modular exponentiations performed
 * (excluding the final confirmations).  This never gives up searching.
 *
 * The FILE *f argument, if non-NULL, has progress indicators written
 * to it.  A dot (.) is written every time a primeality test is failed,
 * a plus (+) or minus (-) when the smaller prime of the pair passes a
 * test, and a star (*) when the larger one does.  Finally, a slash (/)
 * is printed when the sieve was emptied without finding a prime and is
 * being refilled.
 *
 * Apologies to structured programmers for all the GOTOs.
 */
int
germainPrimeGen(struct BigNum *bn, unsigned order,
	int (*f)(void *arg, int c), void *arg)
{
	int retval;
	unsigned p, prev;
	unsigned inc;
	struct BigNum a, e, bn2;
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

	bnBegin(&a);
	bnBegin(&e);
	bnBegin(&bn2);

	/*
	 * Obviously, the prime we find must be odd.  Further, if 2*p+1
	 * is also to be prime (order > 0) then p != 1 (mod 3), lest
	 * 2*p+1 == 3 (mod 3).  Added to p != 3 (mod 3), p == 2 (mod 3)
	 * and p == 5 (mod 6).
	 * If order > 2 and we care about 4*p+3 and 8*p+7, then similarly
	 * p == 4 (mod 5), so p == 29 (mod 30).
	 * So pick the step size for searching based on the order
	 * and increse bn until it's == -1 (mod inc).
	 *
	 * mod 7 doesn't have a unique value for p because 2 -> 5 -> 4 -> 2,
	 * nor does mod 11, and I don't want to think about things past
	 * that.  The required order would be impractically high, in any case.
	 */
	inc = order ? ((order > 2) ? 30 : 6) : 2;
	if (bnAddQ(bn, inc-1 - bnModQ(bn, inc)) < 0)
		goto failed;

	for (;;) {
		if (sieveBuild(sieve, SIEVE, bn, inc, order) < 0)
			goto failed;

		p = prev = 0;
		if (sieve[0] & 1 || (p = sieveSearch(sieve, SIEVE, p)) != 0) {
			do {
				/* Adjust bn to have the right value. */
				assert(p >= prev);
				if (bnAddMult(bn, p-prev, inc) < 0)
					goto failed;
				prev = p;

				/* Okay, do the strong tests. */
				retval = germainPrimeTest(bn, &bn2, &e, &a,
				                          order, f, arg);
				if (retval <= 0)
					goto done;
				modexps += retval;
				if (f && (retval = f(arg, '.')) < 0)
					goto done;

				/* And try again */
				p = sieveSearch(sieve, SIEVE, p);
			} while (p);
		}

		/* Ran out of sieve space - increase bn and keep trying. */
		if (bnAddMult(bn, (unsigned long)SIEVE*8-prev, inc) < 0)
			goto failed;
		if (f && (retval = f(arg, '/')) < 0)
			goto done;
	} /* for (;;) */

failed:
	retval = -1;
done:
	bnEnd(&bn2);
	bnEnd(&e);
	bnEnd(&a);
#ifdef MSDOS
	lbnMemFree(sieve, SIEVE);
#else
	lbnMemWipe(sieve, sizeof(sieve));
#endif
	return retval < 0 ? retval : modexps+(order+1)*CONFIRMTESTS;
}

int
germainPrimeGenStrong(struct BigNum *bn, struct BigNum const *step,
	unsigned order, int (*f)(void *arg, int c), void *arg)
{
	int retval;
	unsigned p, prev;
	struct BigNum a, e, bn2;
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
	bnBegin(&a);
	bnBegin(&e);
	bnBegin(&bn2);

	for (;;) {
		if (sieveBuildBig(sieve, SIEVE, bn, step, order) < 0)
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

				/* Okay, do the strong tests. */
				retval = germainPrimeTest(bn, &bn2, &e, &a,
				                          order, f, arg);
				if (retval <= 0)
					goto done;
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
	bnEnd(&bn2);
	bnEnd(&e);
	bnEnd(&a);
#ifdef MSDOS
	lbnMemFree(sieve, SIEVE);
#else
	lbnMemWipe(sieve, sizeof(sieve));
#endif
	return retval < 0 ? retval : modexps+(order+1)*CONFIRMTESTS;
}
