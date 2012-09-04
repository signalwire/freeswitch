/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * germtest.c - Random Sophie Germain prime generator.
 *
 * This generates random Sophie Germain primes using the command line
 * as a seed value.  It uses George Marsaglia's "mother of all random
 * number generators" to (using the command line as a seed) to pick the
 * starting search value and then searches sequentially for the next
 * Sophie Germain prime p (a prime such that 2*p+1 is also prime).
 *
 * This is a really good way to burn a lot of CPU cycles.
 */
#if HAVE_CONFIG_H
#include "bnconfig.h"
#endif

#include <stdio.h>
#if !NO_STRING_H
#include <string.h>
#elif HAVE_STRINGS_H
#include <strings.h>
#endif

#include <stdlib.h>	/* For malloc() */

#include "bn.h"
#include "germain.h"
#include "sieve.h"

#include "cputime.h"

#define BNDEBUG 1

#include "bnprint.h"
#define bnPut(prompt, bn) bnPrint(stdout, prompt, bn, "\n")

/*
 * Generate random numbers according to George Marsaglia's
 * Mother Of All Random Number Generators.  This has a
 * period of 0x17768215025F82EA0378038A03A203CA7FFF,
 * or decimal 2043908804452974490458343567652678881935359.
 */
static unsigned mstate[8];
static unsigned mcarry;
static unsigned mindex;

static unsigned
mRandom_16(void)
{
	unsigned long t;

	t = mcarry +
	    mstate[ mindex     ] * 1941ul +
	    mstate[(mindex+1)&7] * 1860ul +
	    mstate[(mindex+2)&7] * 1812ul +
	    mstate[(mindex+3)&7] * 1776ul +
	    mstate[(mindex+4)&7] * 1492ul +
	    mstate[(mindex+5)&7] * 1215ul +
	    mstate[(mindex+6)&7] * 1066ul +
	    mstate[(mindex+7)&7] * 12013ul;
	mcarry = (unsigned)(t >> 16);	/* 0 <= mcarry <= 0x5a87 */
	mindex = (mindex-1) & 7;
	return mstate[mindex] = (unsigned)(t & 0xffff);
}

/*
 * Initialize the RNG based on the given seed.
 * A zero-length seed will produce pretty lousy numbers,
 * but it will work.
 */
static void
mSeed(unsigned char const *seed, unsigned len)
{
	unsigned i;

	for (i = 0; i < 8; i++)
		mstate[i] = 0;
	mcarry = 1;
	while (len--) {
		mcarry += *seed++;
		(void)mRandom_16();
	}
}


/*
 * Generate a bignum of a specified length, with the given
 * high and low 8 bits. "High" is merged into the high 8 bits of the
 * number.  For example, set it to 0x80 to ensure that the number is
 * exactly "bits" bits long (i.e. 2^(bits-1) <= bn < 2^bits).
 * "Low" is merged into the low 8 bits.  For example, set it to
 * 1 to ensure that you generate an odd number.  "High" is merged
 * into the high bits; set it to 0x80 to ensure that the high bit
 * is set in the returned value.
 */
static int
genRandBn(struct BigNum *bn, unsigned bits, unsigned char high,
unsigned char low, unsigned char const *seed, unsigned len)
{
	unsigned char buf[64];
	unsigned bytes;
	unsigned l = 0;	/* Current position */
	unsigned t, i;

	bnSetQ(bn, 0);
	if (bnPrealloc(bn, bits) < 0)
		return -1;
	mSeed(seed, len);

	bytes = (bits+7) / 8;	/* Number of bytes to use */

	for (i = 0; i < sizeof(buf); i += 2) {
		t = mRandom_16();
		buf[i] = (unsigned char)(t >> 8);
		buf[i+1] = (unsigned char)t;
	}
	buf[sizeof(buf)-1] |= low;

	while (bytes > sizeof(buf)) {
		bytes -= sizeof(buf);
		/* Merge in low half of high bits, if necessary */
		if (bytes == 1 && (bits & 7))
			buf[0] |= high << (bits & 7);
		if (bnInsertBigBytes(bn, buf, l, sizeof(buf)) < 0)
			return -1;
		l += sizeof(buf);
		for (i = 0; i < sizeof(buf); i += 2) {
			t = mRandom_16();
			buf[i] = (unsigned char)t;
			buf[i+1] = (unsigned char)(t >> 8);
		}
	}

	/* Do the final "bytes"-long section, using the tail bytes in buf */
	/* Mask off excess high bits */
	buf[sizeof(buf)-bytes] &= 255 >> (-bits & 7);
	/* Merge in specified high bits */
	buf[sizeof(buf)-bytes] |= high >> (-bits & 7);
	if (bytes > 1 && (bits & 7))
		buf[sizeof(buf)-bytes+1] |= high << (bits & 7);
	/* Merge in the appropriate bytes of the buffer */
	if (bnInsertBigBytes(bn, buf+sizeof(buf)-bytes, l, bytes) < 0)
		return -1;
	return 0;
}

struct Progress {
	FILE *f;
	unsigned column;
	unsigned wrap;
};

/* Print a progress indicator, with line-wrap */
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
genSophieGermain(struct BigNum *bn, unsigned bits, unsigned order,
	unsigned char const *seed, unsigned len, FILE *f)
{
#if CLOCK_AVAIL
	timetype start, stop;
	unsigned long s;
#endif
	int i;
#if BNDEBUG
	unsigned char s1[1024], s2[1024];
#endif
	char buf[40];
	unsigned p1, p2;
	struct BigNum step;
	struct Progress progress;

	if (f)
		fprintf(f, "Generating a %u-bit order-%u Sophie Germain prime with \"%.*s\"\n",
			bits, order, (int)len, (char *)seed);
	progress.f = f;
	progress.column = 0;
	progress.wrap = 78;

	/* Find p - choose a starting place */
	if (genRandBn(bn, bits, 0xC0, 3, seed, len) < 0)
		return -1;
#if BNDEBUG /* DEBUG - check that sieve works properly */
	bnBegin(&step);
	bnSetQ(&step, 2);
	sieveBuild(s1, 1024, bn, 2, order);
	sieveBuildBig(s2, 1024, bn, &step, order);
	p1 = p2 = 0;
	if (s1[0] != s2[0])
		printf("Difference: s1[0] = %x s2[0] = %x\n", s1[0], s2[0]);
	do {
		p1 = sieveSearch(s1, 1024, p1);
		p2 = sieveSearch(s2, 1024, p2);

		if (p1 != p2)
			printf("Difference: p1 = %u p2 = %u\n", p1, p2);
	} while (p1 && p2);

	bnEnd(&step);
#endif
	/* And search for a prime */
#if CLOCK_AVAIL
	gettime(&start);
#endif
	i = germainPrimeGen(bn, order, f ? genProgress : 0, (void *)&progress);
	if (i < 0)
		return -1;
#if CLOCK_AVAIL
	gettime(&stop);
#endif
	if (f) {
		putc('\n', f);
		fprintf(f, "%d modular exponentiations performed.\n", i);
	}
#if CLOCK_AVAIL
	subtime(stop, start);
	s = sec(stop);
	printf("%u-bit time = %lu.%03u sec.", bits, s, msec(stop));
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

	bnPut("  p   = ", bn);
	for (p1 = 0; p1 < order; p1++) {
		if (bnLShift(bn, 1) <0)
			return -1;
		(void)bnAddQ(bn, 1);
		sprintf(buf, "%u*p+%u = ", 2u<<p1, (2u<<p1) - 1);
		bnPut(buf, bn);
	}
	return 0;
}

/* Copy the command line to the buffer. */
static unsigned char *
copy(int argc, char **argv, size_t *lenp)
{
	size_t len;
	int i;
	unsigned char *buf, *p;
	
	len = argc > 2 ? (size_t)(argc-2) : 0;
	for (i = 1; i < argc; i++)
		len += strlen(argv[i]);
	*lenp = len;
	buf = malloc(len+!len);	/* Can't malloc 0 bytes... */
	if (buf) {
		p = buf;
		for (i = 1; i < argc; i++) {
			if (i > 1)
				*p++ = ' ';
			len = strlen(argv[i]);
			memcpy(p, argv[i], len);
			p += len;
		}
	}
	return buf;
}

int
main(int argc, char **argv)
{
	unsigned len;
	struct BigNum bn;
	unsigned char *buf;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <seed>\n", argv[0]);
		fputs("\
<seed> should be a a string of bytes to be hashed to seed the prime\n\
generator.  Note that unquoted whitespace between words will be counted\n\
as a single space.  To include multiple spaces, quote them.\n", stderr);
		return 1;
	}

	buf = copy(argc, argv, &len);
	if (!buf) {
		fprintf(stderr, "Out of memory!\n");
		return 1;
	}

	bnBegin(&bn);
	
	genSophieGermain(&bn, 0x100, 0, buf, len, stdout);
	genSophieGermain(&bn, 0x100, 1, buf, len, stdout);
	genSophieGermain(&bn, 0x100, 2, buf, len, stdout);
	genSophieGermain(&bn, 0x100, 3, buf, len, stdout);
	genSophieGermain(&bn, 0x200, 0, buf, len, stdout);
	genSophieGermain(&bn, 0x200, 1, buf, len, stdout);
	genSophieGermain(&bn, 0x200, 2, buf, len, stdout);
	genSophieGermain(&bn, 0x300, 0, buf, len, stdout);
	genSophieGermain(&bn, 0x300, 1, buf, len, stdout);
	genSophieGermain(&bn, 0x400, 0, buf, len, stdout);
	genSophieGermain(&bn, 0x400, 1, buf, len, stdout);
	genSophieGermain(&bn, 0x500, 0, buf, len, stdout);
	genSophieGermain(&bn, 0x500, 1, buf, len, stdout);
	genSophieGermain(&bn, 0x600, 0, buf, len, stdout);
	genSophieGermain(&bn, 0x600, 1, buf, len, stdout);
#if 0
	/* These get *really* slow */
	genSophieGermain(&bn, 0x800, 0, buf, len, stdout);
	genSophieGermain(&bn, 0x800, 1, buf, len, stdout);
	genSophieGermain(&bn, 0xc00, 0, buf, len, stdout);
	genSophieGermain(&bn, 0xc00, 1, buf, len, stdout);
	/* Like, plan on a *week* or more for this one. */
	genSophieGermain(&bn, 0x1000, 0, buf, len, stdout);
	genSophieGermain(&bn, 0x1000, 1, buf, len, stdout);
#endif

	bnEnd(&bn);
	free(buf);

	return 0;
}
