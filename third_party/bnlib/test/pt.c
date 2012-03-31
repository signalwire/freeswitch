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
#include "noise.h"

#include "kludge.h"

#define bnPut(prompt, bn) bnPrint(stdout, prompt, bn, "\n")

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
hextoval(char c)
{
	if (c < '0')
		return -1;
	c -= '0';
	if (c < 10)
		return c;
	c -= 'A'-'0';
	c &= ~('a'-'A');
	if (c >= 0 && c < 6)
		return c+10;
	return -1;
}

static int
stringToBn(struct BigNum *bn, char const *string)
{
	size_t len = strlen(string);
	char buf;
	int i, j;

	(void)bnSetQ(bn, 0);

	if (len & 1) {
		i = hextoval(*string++);
		if (i < 0)
			return 0;
		buf = i;
		if (bnInsertBigBytes(bn, &buf, len/2, 1) < 0)
			return -1;
	}
	len /= 2;
	while (len--) {
		i = hextoval(*string++);
		if (i < 0)
			return 0;
		j = hextoval(*string++);
		if (j < 0)
			return 0;
		buf = i*16 + j;
		if (bnInsertBigBytes(bn, &buf, len, 1) < 0)
			return -1;
	}
	return 1;	/* Success */
}

static int
primeTest(char const *string)
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
	i = stringToBn(&bn, string);
	if (i < 1) {
		if (i < 0)
			goto error;
		printf("Malformed string: \"%s\"\n", string);
		bnEnd(&bn);
		return 0;
	}

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
	return 1;
error:
	puts("\nError!");
	bnEnd(&bn);
	return -1;
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <hex>...\n", argv[0]);
		fputs("\
This finds the next primes after the given hex strings.\n", stderr);
		return 1;
	}

	bnInit();

	while (--argc)
		primeTest(*++argv);

	return 0;
}
