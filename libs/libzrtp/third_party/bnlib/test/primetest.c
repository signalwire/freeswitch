/*
 * Copyright (c) 1994, 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * primetest.c - Test driver for prime generation.
 */

#include "first.h"
#include <stdio.h>
#include <stdlib.h>	/* For strtoul() */

#include "bn.h"
#include "bnprint.h"
#include "cputime.h"
#include "prime.h"
#include "random.h"	/* Good random number generator */
#include "noise.h"

#include "kludge.h"

#define bnPut(prompt, bn) bnPrint(stdout, prompt, bn, "\n")

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
primeProgress(void *arg, int c)
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
primeTest(unsigned bits)
{
	int modexps = 0;
	struct BigNum bn;	/* Temporary */
	int i, j;
	struct Progress progress;
#if CLOCK_AVAIL
	timetype start, stop;
	unsigned long curs, tots = 0;
	unsigned curms, totms = 0;
#endif
	progress.f = stdout;
	progress.wrap = 78;

	bnBegin(&bn);

	/* Find p - choose a starting place */
	i = genRandBn(&bn, bits, 0x80, 1);
	if (i < 0)
		goto error;

	/* And search for primes */
	for (j = 0; j < 40; j++) {
		progress.column = 0;
#if CLOCK_AVAIL
		gettime(&start);
#endif
		i = primeGen(&bn, 0, primeProgress, &progress, 0);
		if (i < 0)
			goto error;
#if CLOCK_AVAIL
		gettime(&stop);
		subtime(stop, start);
		tots += curs = sec(stop);
		totms += curms = msec(stop);
#endif
		modexps += i;
		putchar('\n');	/* Signal done */
		printf("%d modular exponentiations performed", i);
#if CLOCK_AVAIL
		printf(" in %lu.%03u s", curs, curms);
#endif
		putchar('\n');
		bnPut("n = ", &bn);
		if (bnAddQ(&bn, 2) < 0)
			goto error;
	}

	bnEnd(&bn);
	printf("Total %d modular exponentiations performed", modexps);
#if CLOCK_AVAIL
	tots += totms/1000;
	totms %= 1000;
	printf(" in %lu.%03u s\n", tots, totms);
	totms += 1000 * (tots % j);
	tots /= j;
	totms /= j;
	tots += totms / 1000;
	totms %= 1000;
	printf("Average time: %lu.%03u s", tots, totms);
#endif
	putchar('\n');

	/* And that's it... success! */
	return 0;
error:
	puts("\nError!");
	bnEnd(&bn);
	return -1;
}


int
main(int argc, char **argv)
{
	unsigned long t;
	char *p;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <bits>...\n", argv[0]);
		fputs("\
This generates a random RSA key pair and prints its value.  <bits>\n\
is the size of the modulus to use.\n", stderr);
		return 1;
	}

	noise();
	bnInit();

	while (--argc) {
		t = strtoul(*++argv, &p, 0);
		if (t < 17 || t > 65536 || *p) {
			fprintf(stderr, "Illegal prime size: \"%s\"\n",
			        *argv);
			return 1;
		}

		primeTest((unsigned)t);
	}

	return 0;
}
