/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * randtest.c - FIPS 140 random number tests.
 * This performs all the tests required by the FIPS 140
 * standard on the raw random number pool.  If any fail,
 * with at least one bit of entropy in the input, the random
 * number generator is to be considered broken.
 *
 * The FIPS parameters are very loose, to guarantee that a
 * system will not, in practice, declare itself broken during
 * normal operation.  The results from any given run should
 * be *much* closer to centered in the allowed ranges.
 *
 * E.g. The expected sum of 20000 random bits is 10000,
 * with a standard deviation of 1/12 * sqrt(20000) = 11.785
 * the deviation at which an error is signalled of 346 from
 * this average is 29.359 standard deviations out.  *Very* unlikely.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>	/* For strtoul */
#include <string.h>	/* For memset */
#include "kludge.h"

#include "random.h"	/* Good random number generator */

/* Number of bits to check */
#define NBITS 20000
#define NBYTES ((NBITS+7)/8)

#define MAXRUNSTAT	20	/* Longest run accumulated */
#define MAXRUNCHECK	6	/* Longest run checked */
#define MAXRUNTOOLONG	34	/* A run this long is an error */

static unsigned
pokerstat(unsigned char const buf[NBYTES], unsigned counts[16])
{
	unsigned i;
	unsigned char c;

	for (i = 0; i < 16; i++)
		counts[i] = 0;

	for (i = 0; i < NBYTES; i++) {
		c = buf[i];
		counts[c & 15]++;
		counts[c>>4]++;
	}

	return counts[15] * 4 +
	       (counts[14] + counts[13] + counts[11] + counts[7]) * 3 +
	       (counts[12] + counts[10] + counts[9] +
	        counts[6] + counts[5] + counts[3]) * 2 +
	       counts[8] + counts[4] + counts[2] + counts[1];
}

static unsigned
countrunsbig(unsigned char const buf[NBYTES],
	unsigned zeros[MAXRUNSTAT], unsigned ones[MAXRUNSTAT])
{
	unsigned i;
	unsigned char c, mask;
	unsigned char state;	/* All 0s or all 1s */
	unsigned runlength;
	unsigned maxrun = 0;

	/* Initialize to zero */
	for (i = 0; i < MAXRUNSTAT; i++) {
		zeros[i] = 0;
		ones[i] = 0;
	}

	/* Start with a run of length 0 matching the first bit */
	state = (buf[0] & 0x80) ? 0xff : 0;
	runlength = 0;
	
	for (i = 0; i < NBYTES; i++) {
		c = buf[i];
		mask = 0x80;
		do {
			if ((c ^ state) & mask) {
				/* Change of state; update counters */
				if (maxrun < runlength)
					maxrun = runlength;
				if (runlength > MAXRUNSTAT)
					runlength = MAXRUNSTAT;
				(state ? ones : zeros)[runlength-1]++;
				state = ~state;
				runlength = 0;
			}
			runlength++;
		} while (mask >>= 1);
	}

	/* Add in final run */
	if (maxrun < runlength)
		maxrun = runlength;
	if (runlength > MAXRUNSTAT)
		runlength = MAXRUNSTAT;
	(state ? ones : zeros)[runlength-1]++;

	return maxrun;
}

static unsigned
countrunslittle(unsigned char const buf[NBYTES],
	unsigned zeros[MAXRUNSTAT], unsigned ones[MAXRUNSTAT])
{
	unsigned i;
	unsigned char c, mask;
	unsigned char state;	/* All 0s or all 1s */
	unsigned runlength;
	unsigned maxrun = 0;

	/* Initialize to zero */
	for (i = 0; i < MAXRUNSTAT; i++) {
		zeros[i] = 0;
		ones[i] = 0;
	}

	/* Start with a run of length 0 matching the first bit */
	state = (buf[0] & 1) ? 0xff : 0;
	runlength = 0;
	
	for (i = 0; i < NBYTES; i++) {
		c = buf[i];
		mask = 1;
		do {
			if ((c ^ state) & mask) {
				/* Change of state; update counters */
				if (maxrun < runlength)
					maxrun = runlength;
				if (runlength > MAXRUNSTAT)
					runlength = MAXRUNSTAT;
				(state ? ones : zeros)[runlength-1]++;
				state = ~state;
				runlength = 0;
			}
			runlength++;
		} while ((mask <<= 1) & 0xff);
	}

	/* Add in final run */
	if (maxrun < runlength)
		maxrun = runlength;
	if (runlength > MAXRUNSTAT)
		runlength = MAXRUNSTAT;
	(state ? ones : zeros)[runlength-1]++;

	return maxrun;
}

static int
checkruns(unsigned const zeros[MAXRUNSTAT], unsigned const ones[MAXRUNSTAT],
	unsigned maxrun)
{
	int passed, numfailed;
	unsigned i, j;
	unsigned sumones, sumzeros;
	static unsigned const lowlimit[MAXRUNCHECK] =
		{ 2267, 1079, 502, 223, 90, 90 };
	static unsigned const highlimit[MAXRUNCHECK] =
		{ 2733, 1421, 748, 402, 223, 223 };

	numfailed = 0;

	j = MAXRUNSTAT;
	while (j--) {
		if (zeros[j] || ones[j])
			break;
	}

	for (i = 0; i < MAXRUNCHECK - 1; i++) {
		passed = (lowlimit[i] < zeros[i]) && (zeros[i] < highlimit[i]);
		numfailed += !passed;
		printf("%2u zeros: %4u <%5u < %4u: %s\t",
			i+1, lowlimit[i], zeros[i], highlimit[i],
			passed ? "Pass  " : "FAIL *");

		passed = (lowlimit[i] < ones[i]) && (ones[i] < highlimit[i]);
		numfailed += !passed;
		printf("%2u ones: %4u <%5u < %4u: %s\n",
			i+1, lowlimit[i], ones[i], highlimit[i],
			passed ? "Pass  " : "FAIL *");
	}
	for (sumzeros = 0, sumones = 0; i <= j; i++) {
		printf("%2u zeros:        %4u      \t\t",
		       i+1, zeros[i]);
		sumzeros += zeros[i];
		printf("%2u ones:       %4u\n", i+1, ones[i]);
		sumones += ones[i];
	}

	i = MAXRUNCHECK-1;
	passed = (lowlimit[i] < sumzeros) && (sumzeros < highlimit[i]);
	numfailed += !passed;
	printf("%u+ zeros: %4u < %4u < %4u: %s\t",
		i+1, lowlimit[i], sumzeros, highlimit[i],
		passed ? "Pass  " : "FAIL *");
	passed = (lowlimit[i] < sumones) && (sumones < highlimit[i]);
	numfailed += !passed;
	printf("%u+ zeros: %4u < %4u < %4u: %s\n",
		i+1, lowlimit[i], sumones, highlimit[i],
		passed ? "Pass  " : "FAIL *");

	passed = maxrun < MAXRUNTOOLONG;
	numfailed += !passed;
	printf("Longest run: %u < %u: %s\n", maxrun, (unsigned)MAXRUNTOOLONG, 
	       passed ? "Pass  " : "FAIL *");

	return numfailed;
}

int
main(int argc, char **argv)
{
	unsigned char buf[NBYTES];
	unsigned poker[16];
	unsigned onebits;
	unsigned runzero[MAXRUNSTAT], runone[MAXRUNSTAT];
	unsigned maxrun;
	unsigned long t;
	unsigned i;
	int passed;
	int numfailed = 0;
	char *p;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <bits>\n"
"Accumulate random bits and then do randomness tests on the RNG output.\n",
			argv[0]);
		return 1;
	}
	t = strtoul(argv[1], &p, 0);
	if (t > 3072 || *p) {
		fprintf(stderr, "Illegal number of bits: \"%s\"\n", argv[1]);
		return 1;
	}
	randAccum(t);

	randBytes(buf, sizeof(buf));
	onebits = pokerstat(buf, poker);

	passed = (9654 < onebits) && (onebits < 10346);
	numfailed += !passed;
	printf("\nNumber of one bits: 9654 < %u < 10346:  %s\n", onebits,
	       passed ? "Pass  " : "FAIL *");
	/*
	 * Original test asks for
	 * X = (16/5000) * sum(poker[i]^2, i = 0..15) - 5000,
	 * and requires that 1.03 < X < 57.4.
	 * This test uses t = 5000/16 * X, and requires that
	 * 321.875 < t < 17937.5.  Note that if the distribution
	 * were totally flat, t would be 0, which is *also* bad.
	 */
	t = 0;
	for (i = 0; i < 16; i++) {
		printf("poker[%u%u%u%u] =%4u  %c",
		       i>>3, i>>2 & 1, i>>1 & 1, i & 1,
		       poker[i],(~i & 3) ? ' ' : '\n');
		t += (unsigned long)poker[i] * poker[i];
	}
	t -= 5000ul * 5000 / 16;
	passed = (321 < t) && (t < 17938);
	numfailed += !passed;
	printf("Poker parameter: 321.875 < %lu < 17937.5: %s\n", t,
	       passed ? "Pass  " : "FAIL *");

	/*
	 * Next, we're asked to count runs of consecutive ones and
	 * zeroes.  The shortest possible run is of length 1.
	 * The longest, 20000.  Since the byte ordering is not defined,
	 * do it both ways!  This tallies the run lengths of all
	 * zeros and all ones, giving totals for the short runs
	 * and the longest run of either size encountered.
	 */
	printf("\nBig-endian run tests:\n");
	maxrun = countrunsbig(buf, runzero, runone);
	numfailed += checkruns(runzero, runone, maxrun);

	printf("\nLittle-endian run tests:\n");
	maxrun = countrunslittle(buf, runzero, runone);
	numfailed += checkruns(runzero, runone, maxrun);

	/*
	 * Tests are:
	 *  1 - Number of one bits
	 *  1 - Poker test
	 * 12 - Big-endian run length tests
	 *  1 - Big-endian maximum run length test
	 * 12 - Little-endian run length tests
	 *  1 - Little-endian maximum run length test
	 */
	printf("\nOut of 28 tests, %d tests failed.\n", numfailed);

	return numfailed;
}
