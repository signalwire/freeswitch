/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * sieve.c - Trial division for prime finding.
 *
 * Finding primes:
 * - Sieve 1 to find the small primes for
 * - Sieve 2 to find the candidate large primes, then
 * - Pseudo-primality test.
 *
 * An important question is how much trial division by small primes
 * should we do?  The answer is a LOT.  Even a heavily optimized
 * Fermat test to the base 2 (the simplest pseudoprimality test)
 * is much more expensive than a division.
 *
 * For an prime of n k-bit words, a Fermat test to the base 2 requires n*k
 * modular squarings, each of which involves n*(n+1)/2 signle-word multiplies
 * in the squaring and n*(n+1) multiplies in the modular reduction, plus
 * some overhead to get into and out of Montgomery form.  This is a total
 * of 3/2 * k * n^2 * (n+1).  Equivalently, if n*k = b bits, it's
 * 3/2 * (b/k+1) * b^2 / k.
 *
 * A modulo operation requires n single-word divides.  Let's assume that
 * a divide is 4 times the cost of a multiply.  That's 4*n multiplies.
 * However, you only have to do the division once for your entire
 * search.  It can be amortized over 10-15 primes.  So it's
 * really more like n/3 multiplies.  This is b/3k.
 *
 * Now, let's suppose you have a candidate prime t.  Your options
 * are to a) do trial division by a prime p, then do a Fermat test,
 * or to do the Fermat test directly.  Doing the trial division
 * costs b/3k multiplies, but a certain fraction of the time (1/p), it
 * saves you 3/2 b^3 / k^2 multiplies.  Thus, it's worth it doing the
 * division as long as b/3k < 3/2 * (b/k+1) * b^2 / k / p.
 * I.e. p < 9/2 * (b/k + 1) * b = 9/2 * (b^2/k + b).
 * E.g. for k=16 and b=256, p < 9/2 * 17 * 256 = 19584.
 * Solving for k=16 and k=32 at a few interesting value of b:
 *
 * k=16, b=256: p <  19584	k=32, b=256: p <  10368
 * k=16, b=384: p <  43200	k=32, b=384; p <  22464
 * k=16, b=512: p <  76032	k=32, b=512: p <  39168
 * k=16, b=640: p < 118080	k=32, b=640: p <  60480
 *
 * H'm... before using the highly-optimized Fermat test, I got much larger
 * numbers (64K to 256K), and designed the sieve for that.  Maybe it needs
 * to be reduced.  It *is* true that the desirable sieve size increases
 * rapidly with increasing prime size, and it's the larger primes that are
 * worrisome in any case.  I'll leave it as is (64K) for now while I
 * think about it.
 *
 * A bit of tweaking the division (we can compute a reciprocal and do
 * multiplies instead, turning 4*n into 4 + 2*n) would increase all the
 * numbers by a factor of 2 or so.
 *
 *
 * Bit k in a sieve corresponds to the number a + k*b.
 * For a given a and b, the sieve's job is to find the values of
 * k for which a + k*b == 0 (mod p).  Multiplying by b^-1 and
 * isolating k, you get k == -a*b^-1 (mod p).  So the values of
 * k which should be worked on are k = (-a*b^-1 mod p) + i * p,
 * for i = 0, 1, 2,...
 *
 * Note how this is still easy to use with very large b, if you need it.
 * It just requires computing (b mod p) and then finding the multiplicative
 * inverse of that.
 *
 *
 * How large a space to search to ensure that one will hit a prime?
 * The average density is known, but the primes behave oddly, and sometimes
 * there are large gaps.  It is conjectured by shanks that the first gap
 * of size "delta" will occur at approximately exp(sqrt(delta)), so a delta
 * of 65536 is conjectured to be to contain a prime up to e^256.
 * Remembering the handy 2<->e conversion ratios:
 * ln(2) = 0.693147   log2(e) = 1.442695
 * This covers up to 369 bits.  Damn, not enough!  Still, it'll have to do.
 *
 * Cramer's conjecture (he proved it for "most" cases) is that in the limit,
 * as p goes to infinity, the largest gap after a prime p tends to (ln(p))^2.
 * So, for a 1024-bit p, the interval to the next prime is expected to be
 * about 709.78^2, or 503791.  We'd need to enlarge our space by a factor of
 * 8 to be sure.  It isn't worth the hassle.
 *
 * Note that a span of this size is expected to contain 92 primes even
 * in the vicinity of 2^1024 (it's 369 at 256 bits and 492 at 192 bits).
 * So the probability of failure is pretty low.
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
#ifndef NO_LIMITS_H
#define NO_LIMITS_H 0
#endif
#ifndef NO_STRING_H
#define NO_STRING_H 0
#endif
#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 0
#endif

#if !NO_ASSERT_H
#include <assert.h>
#else
#define assert(x) (void)0
#endif

#if !NO_LIMITS_H
#include <limits.h>	/* For UINT_MAX */
#endif			/* If not avail, default value of 0 is safe */

#if !NO_STRING_H
#include <string.h>	/* for memset() */
#elif HAVE_STRINGS_H
#include <strings.h>
#endif

#include "bn.h"
#include "sieve.h"
#ifdef MSDOS
#include "lbnmem.h"
#endif

#include "kludge.h"

/*
 * Each array stores potential primes as 1 bits in little-endian bytes.
 * Bit k in an array represents a + k*b, for some parameters a and b
 * of the sieve.  Currently, b is hardcoded to 2.
 *
 * Various factors of 16 arise because these are all *byte* sizes, and
 * skipping even numbers, 16 numbers fit into a byte's worth of bitmap.
 */

/*
 * The first number in the small prime sieve.  This could be raised to
 * 3 if you want to squeeze bytes out aggressively for a smaller SMALL
 * table, and doing so would let one more prime into the end of the array,
 * but there is no sense making it larger if you're generating small
 * primes up to the limit if 2^16, since it doesn't save any memory and
 * would require extra code to ignore 65537 in the last byte, which is
 * over the 16-bit limit.
 */
#define SMALLSTART 1

/*
 * Size of sieve used to find large primes, in bytes.  For compatibility
 * with 16-bit-int systems, the largest prime that can appear in it,
 * SMALL * 16 + SMALLSTART - 2, must be < 65536.  Since 65537 is a prime,
 * this is the absolute maximum table size.
 */
#define SMALL (65536/16)

/*
 * Compute the multiplicative inverse of x, modulo mod, using the extended
 * Euclidean algorithm.  The classical EEA returns two results, traditionally
 * named s and t, but only one (t) is needed or computed here.
 * It is unrolled twice to avoid some variable-swapping, and because negating
 * t every other round makes all the number positive and less than the
 * modulus, which makes fixed-length arithmetic easier.
 *
 * If gcd(x, mod) != 1, then this will return 0.
 */
static unsigned
sieveModInvert(unsigned x, unsigned mod)
{
	unsigned y;
	unsigned t0, t1;
	unsigned q;

	if (x <= 1)
		return x;	/* 0 and 1 are self-inverse */
	/*
	 * The first round is simplified based on the
	 * initial conditions t0 = 1 and t1 = 0.
	 */
	t1 = mod / x;
	y = mod % x;
	if (y <= 1)
		return y ? mod - t1 : 0;
	t0 = 1;

	do {
		q = x / y;
		x = x % y;
		t0 += q * t1;
		if (x <= 1)
			return x ? t0 : 0;
		q = y / x;
		y = y % x;
		t1 += q * t0;
	} while (y > 1);
	return y ? mod - t1 : 0;
}


/*
 * Perform a single sieving operation on an array.  Clear bits "start",
 * "start+step", "start+2*step", etc. from the array, up to the size
 * limit (in BYTES) "size".  All of the arguments must fit into 16 bits
 * for portability.
 *
 * This is the core of the sieving operation.  In addition to being
 * called from the sieving functions, it is useful to call directly if,
 * say, you want to exclude primes congruent to 1 mod 3, or whatever.
 * (Although in that case, it would be better to change the sieving to
 * use a step size of 6 and start == 5 (mod 6).)
 *
 * Originally, this was inlined in the code below (with various checks
 * turned off where they could be inferred from the environment), but it
 * turns out that all the sieving is so fast that it makes a negligible
 * speed difference and smaller, cleaner code was preferred.
 *
 * Rather than increment a bit index through the array and clear
 * the corresponding bit, this code takes advantage of the fact that
 * every eighth increment must use the same bit position in a byte.
 * I.e. start + k*step == start + (k+8)*step (mod 8).  Thus, a bitmask
 * can be computed only eight times and used for all multiples.  Thus, the
 * outer loop is over (k mod 8) while the inner loop is over (k div 8).
 *
 * The only further trickiness is that this code is designed to accept
 * start, step, and size up to 65535 on 16-bit machines.  On such a
 * machine, the computation "start+step" can overflow, so we need to
 * insert an extra check for that situation.
 */
void
sieveSingle(unsigned char *array, unsigned size, unsigned start, unsigned step)
{
	unsigned bit;
	unsigned char mask;
	unsigned i;

#if UINT_MAX < 0x1ffff
	/* Unsigned is small; add checks for wrap */
	for (bit = 0; bit < 8; bit++) {
		i = start/8;
		if (i >= size)
			break;
		mask = ~(1 << (start & 7));
		do {
			array[i] &= mask;
			i += step;
		} while (i >= step && i < size);
		start += step;
		if (start < step)	/* Overflow test */
			break;
	}
#else
	/* Unsigned has the range - no overflow possible */
	for (bit = 0; bit < 8; bit++) {
		i = start/8;
		if (i >= size)
			break;
		mask = ~(1 << (start & 7));
		do {
			array[i] &= mask;
			i += step;
		} while (i < size);
		start += step;
	}
#endif
}

/*
 * Returns the index of the next bit set in the given array.  The search
 * begins after the specified bit, so if you care about bit 0, you need
 * to check it explicitly yourself.  This returns 0 if no bits are found.
 *
 * Note that the size is in bytes, and that it takes and returns BIT
 * positions.  If the array represents odd numbers only, as usual, the
 * returned values must be doubled to turn them into offsets from the
 * initial number.
 */
unsigned
sieveSearch(unsigned char const *array, unsigned size, unsigned start)
{
	unsigned i;	/* Loop index */
	unsigned char t;	/* Temp */

	if (!++start)
		return 0;
	i = start/8;
	if (i >= size)
		return 0;	/* Done! */

	/* Deal with odd-bit beginnings => search the first byte */
	if (start & 7) {
		t = array[i++] >> (start & 7);
		if (t) {
			if (!(t & 15)) {
				t >>= 4;
				start += 4;
			}
			if (!(t & 3)) {
				t >>= 2;
				start += 2;
			}
			if (!(t & 1))
				start += 1;
			return start;
		} else if (i == size) {
			return 0;	/* Done */
		}
	}

	/* Now the main search loop */

	do {
		if ((t = array[i]) != 0) {
			start = 8*i;
			if (!(t & 15)) {
				t >>= 4;
				start += 4;
			}
			if (!(t & 3)) {
				t >>= 2;
				start += 2;
			}
			if (!(t & 1))
				start += 1;
			return start;
		}
	} while (++i < size);

	/* Failed */
	return 0;
}

/*
 * Build a table of small primes for sieving larger primes with.  This
 * could be cached between calls to sieveBuild, but it's so fast that
 * it's really not worth it.  This code takes a few milliseconds to run.
 */
static void
sieveSmall(unsigned char *array, unsigned size)
{
	unsigned i;		/* Loop index */
	unsigned p;		/* The current prime */

	/* Initialize to all 1s */
	memset(array, 0xFF, size);

#if SMALLSTART == 1
	/* Mark 1 as NOT prime */
	array[0] = 0xfe;
	i = 1;	/* Index of first prime */
#else
	i = 0;	/* Index of first prime */
#endif

	/*
	 * Okay, now sieve via the primes up to 256, obtained from the
	 * table itself.  We know the maximum possible table size is
	 * 65536, and sieveSingle() can cope with out-of-range inputs
	 * safely, and the time required is trivial, so it isn't adaptive
	 * based on the array size.
	 *
	 * Convert each bit position into a prime, compute a starting
	 * sieve position (the square of the prime), and remove multiples
	 * from the table, using sieveSingle().  I used to have that
	 * code in line here, but the speed difference was so small it
	 * wasn't worth it.  If a compiler really wants to waste memory,
	 * it can inline it.
	 */
	do {
		p = 2 * i + SMALLSTART;
		if (p > 256)
			break;
		/* Start at square of p */
		sieveSingle(array, size, (p*p-SMALLSTART)/2, p);

		/* And find the next prime */
		i = sieveSearch(array, 16, i);
	} while (i);
}


/*
 * This is the primary sieving function.  It fills in the array with
 * a sieve (multiples of small primes removed) beginning at bn and
 * proceeding in steps of "step".
 *
 * It generates a small array to get the primes to sieve by.  It's
 * generated on the fly - sieveSmall is fast enough to make that
 * perfectly acceptable.
 *
 * The caller should take the array, walk it with sieveSearch, and
 * apply a stronger primality test to the numbers that are returned.
 *
 * If the "dbl" flag non-zero (at least 1), this also sieves 2*bn+1, in
 * steps of 2*step.  If dbl is 2 or more, this also sieve 4*bn+3,
 * in steps of 4*step, and so on for arbitrarily high values of "dbl".
 * This is convenient for finding primes such that (p-1)/2 is also prime.
 * This is particularly efficient because sieveSingle is controlled by the
 * parameter s = -n/step (mod p).  (In fact, we find t = -1/step (mod p)
 * and multiply that by n (mod p).)  If you have -n/step (mod p), then
 * finding -(2*n+1)/(2*step) (mod p), which is -n/step - 1/(2*step) (mod p),
 * reduces to finding -1/(2*step) (mod p), or t/2 (mod p), and adding that
 * to s = -n/step (mod p).  Dividing by 2 modulo an odd p is easy -
 * if even, divide directly.  Otherwise, add p (which produces an even
 * sum), and divide by 2.  Very simple.  And this produces s' and t'
 * for step' = 2*step.  It can be repeated for step'' = 4*step and so on.
 *
 * Note that some of the math is complicated by the fact that 2*p might
 * not fit into an unsigned, so rather than if (odd(x)) x = (x+p)/2,
 * we do if (odd(x)) x = x/2 + p/2 + 1;
 *
 * TODO: Do the double-sieving by sieving the larger number, and then
 * just subtract one from the remainder to get the other parameter.
 * (bn-1)/2 is divisible by an odd p iff bn-1 is divisible, which is
 * true iff bn == 1 mod p.  This requires using a step size of 4.
 */
int
sieveBuild(unsigned char *array, unsigned size, struct BigNum const *bn,
	unsigned step, unsigned dbl)
{
	unsigned i, j;	/* Loop index */
	unsigned p;	/* Current small prime */
	unsigned s;	/* Where to start operations in the big sieve */
	unsigned t;	/* Step modulo p, the current prime */
#ifdef MSDOS	/* Use dynamic allocation rather than on the stack */
	unsigned char *small;
#else
	unsigned char small[SMALL];
#endif

	assert(array);

#ifdef MSDOS
	small = lbnMemAlloc(SMALL);	/* Which allocator?  Not secure. */
	if (!small)
		return -1;	/* Failed */
#endif

	/*
	 * An odd step is a special case, since we must sieve by 2,
	 * which isn't in the small prime array and has a few other
	 * special properties.  These are:
	 * - Since the numbers are stored in binary, we don't need to
	 *   use bnModQ to find the remainder.
	 * - If step is odd, then t = step % 2 is 1, which allows
	 *   the elimination of a lot of math.  Inverting and negating
	 *   t don't change it, and multiplying s by 1 is a no-op,
	 *   so t isn't actually mentioned.
	 * - Since this is the first sieving, instead of calling
	 *   sieveSingle, we can just use memset to fill the array
	 *   with 0x55 or 0xAA.  Since a 1 bit means possible prime
	 *   (i.e. NOT divisible by 2), and the least significant bit
	 *   is first, if bn % 2 == 0, we use 0xAA (bit 0 = bn is NOT
	 *   prime), while if bn % 2 == 1, use 0x55.
	 *   (If step is even, bn must be odd, so fill the array with 0xFF.)
	 * - Any doublings need not be considered, since 2*bn+1 is odd, and
	 *   2*step is even, so none of these numbers are divisible by 2.
	 */
	if (step & 1) {
		s = bnLSWord(bn) & 1;
		memset(array, 0xAA >> s, size);
	} else {
		/* Initialize the array to all 1's */
		memset(array, 255, size);
		assert(bnLSWord(bn) & 1);
	}

	/*
	 * This could be cached between calls to sieveBuild, but
	 * it's really not worth it; sieveSmall is *very* fast.
	 * sieveSmall returns a sieve of odd primes.
	 */
	sieveSmall(small, SMALL);

	/*
	 * Okay, now sieve via the primes up to ssize*16+SMALLSTART-1,
	 * obtained from the small table.
	 */
	i = (small[0] & 1) ? 0 : sieveSearch(small, SMALL, 0);
	do {
		p = 2 * i + SMALLSTART;

		/*
		 * Modulo is usually very expensive, but step is usually
		 * small, so this conditional is worth it.
		 */
		t = (step < p) ? step : step % p;
		if (!t) {
			/*
			 * Instead of assert failing, returning all zero
			 * bits is the "correct" thing to do, but I think
			 * that the caller should take care of that
			 * themselves before starting.
			 */
			assert(bnModQ(bn, p) != 0);
			continue;
		}
		/*
		 * Get inverse of step mod p.  0 < t < p, and p is prime,
		 * so it has an inverse and sieveModInvert can't return 0.
		 */
		t = sieveModInvert(t, p);
		assert(t);
		/* Negate t, so now t == -1/step (mod p) */
		t = p - t;

		/* Now get the bignum modulo the prime. */
		s = bnModQ(bn, p);

		/* Multiply by t, the negative inverse of step size */
#if UINT_MAX/0xffff < 0xffff
		s = (unsigned)(((unsigned long)s * t) % p);
#else
		s = (s * t) % p;
#endif

		/* s is now the starting bit position, so sieve */
		sieveSingle(array, size, s, p);

		/* Now do the double sieves as desired. */
		for (j = 0; j < dbl; j++) {
			/* Halve t modulo p */
#if UINT_MAX < 0x1ffff
			t = (t & 1) ? p/2 + t/2 + 1 : t/2;
			/* Add t to s, modulo p with overflow checks. */
			s += t;
			if (s >= p || s < t)
				s -= p;
#else
			if (t & 1)
				t += p;
			t /= 2;
			/* Add t to s, modulo p */
			s += t;
			if (s >= p)
				s -= p;
#endif
			sieveSingle(array, size, s, p);
		}

		/* And find the next prime */
	} while ((i = sieveSearch(small, SMALL, i)) != 0);

#ifdef MSDOS
	lbnMemFree(small, SMALL);
#endif
	return 0;	/* Success */
}

/*
 * Similar to the above, but use "step" (which must be even) as a step
 * size rather than a fixed value of 2.  If "step" has any small divisors
 * other than 2, this will blow up.
 *
 * Returns -1 on out of memory (MSDOS only, actually), and -2
 * if step is found to be non-prime.
 */
int
sieveBuildBig(unsigned char *array, unsigned size, struct BigNum const *bn,
	struct BigNum const *step, unsigned dbl)
{
	unsigned i, j;	/* Loop index */
	unsigned p;	/* Current small prime */
	unsigned s;	/* Where to start operations in the big sieve */
	unsigned t;	/* step modulo p, the current prime */
#ifdef MSDOS	/* Use dynamic allocation rather than on the stack */
	unsigned char *small;
#else
	unsigned char small[SMALL];
#endif

	assert(array);

#ifdef MSDOS
	small = lbnMemAlloc(SMALL);	/* Which allocator?  Not secure. */
	if (!small)
		return -1;	/* Failed */
#endif
	/*
	 * An odd step is a special case, since we must sieve by 2,
	 * which isn't in the small prime array and has a few other
	 * special properties.  These are:
	 * - Since the numbers are stored in binary, we don't need to
	 *   use bnModQ to find the remainder.
	 * - If step is odd, then t = step % 2 is 1, which allows
	 *   the elimination of a lot of math.  Inverting and negating
	 *   t don't change it, and multiplying s by 1 is a no-op,
	 *   so t isn't actually mentioned.
	 * - Since this is the first sieving, instead of calling
	 *   sieveSingle, we can just use memset to fill the array
	 *   with 0x55 or 0xAA.  Since a 1 bit means possible prime
	 *   (i.e. NOT divisible by 2), and the least significant bit
	 *   is first, if bn % 2 == 0, we use 0xAA (bit 0 = bn is NOT
	 *   prime), while if bn % 2 == 1, use 0x55.
	 *   (If step is even, bn must be odd, so fill the array with 0xFF.)
	 * - Any doublings need not be considered, since 2*bn+1 is odd, and
	 *   2*step is even, so none of these numbers are divisible by 2.
	 */
	if (bnLSWord(step) & 1) {
		s = bnLSWord(bn) & 1;
		memset(array, 0xAA >> s, size);
	} else {
		/* Initialize the array to all 1's */
		memset(array, 255, size);
		assert(bnLSWord(bn) & 1);
	}

	/*
	 * This could be cached between calls to sieveBuild, but
	 * it's really not worth it; sieveSmall is *very* fast.
	 * sieveSmall returns a sieve of the odd primes.
	 */
	sieveSmall(small, SMALL);

	/*
	 * Okay, now sieve via the primes up to ssize*16+SMALLSTART-1,
	 * obtained from the small table.
	 */
	i = (small[0] & 1) ? 0 : sieveSearch(small, SMALL, 0);
	do {
		p = 2 * i + SMALLSTART;

		t = bnModQ(step, p);
		if (!t) {
			assert(bnModQ(bn, p) != 0);
			continue;
		}
		/* Get negative inverse of step */
		t = sieveModInvert(bnModQ(step, p), p);
		assert(t);
		t = p-t;

		/* Okay, we have a prime - get the remainder */
		s = bnModQ(bn, p);

		/* Now multiply s by the negative inverse of step (mod p) */
#if UINT_MAX/0xffff < 0xffff
		s = (unsigned)(((unsigned long)s * t) % p);
#else
		s = (s * t) % p;
#endif
		/* We now have the starting bit pos */
		sieveSingle(array, size, s, p);

		/* Now do the double sieves as desired. */
		for (j = 0; j < dbl; j++) {
			/* Halve t modulo p */
#if UINT_MAX < 0x1ffff
			t = (t & 1) ? p/2 + t/2 + 1 : t/2;
			/* Add t to s, modulo p with overflow checks. */
			s += t;
			if (s >= p || s < t)
				s -= p;
#else
			if (t & 1)
				t += p;
			t /= 2;
			/* Add t to s, modulo p */
			s += t;
			if (s >= p)
				s -= p;
#endif
			sieveSingle(array, size, s, p);
		}

		/* And find the next prime */
	} while ((i = sieveSearch(small, SMALL, i)) != 0);

#ifdef MSDOS
	lbnMemFree(small, SMALL);
#endif
	return 0;	/* Success */
}
