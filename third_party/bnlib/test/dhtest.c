/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * dhtest.c - Diffie-Hellman prime generator.
 *
 * This generates Diffie-Hellman primes using a (hopefully) clearly
 * defined algorithm, based on David Kravitz's "kosherizer".
 * This takes a seed in the form of a byte string, usually ASCII.
 * The byte string is hashed with SHA.  This forms the low 160 bits
 * of the search start number.  If the desired start number is longer
 * than this, the byte string is treated as a big-endian number and
 * incremented, which increments the last byte, propagating carry.
 * (Modulo the size of the seed itself, which is not an issue in
 * practice for any seed at least one byte long.)
 * This incremented value is hashed to produce the next most significant
 * 160 bits, and so on.
 * After enough bits have been accumulated, the low bit is set, the extra
 * high bits are masked off to zero, and the two high bits of the
 * search start number are set.  This is used as a starting seed for a
 * sequential (increasing) search for a suitable prime.
 *
 * A suitable prime P is itself prime, and (P-1)/2 is also prime.
 */
#include <stdio.h>
#include <string.h>

#include "bn.h"
#include "germain.h"
#include "sieve.h"

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
 * The bignum is generated using the given seed string.  The
 * technique is from David Kravitz (of the NSA)'s "kosherizer".
 * The string is hashed, and that (with the low bit forced to 1)
 * is used for the low 160 bits of the number.  Then the string,
 * considered as a big-endian array of bytes, is incremented
 * and the incremented value is hashed to produce the next most
 * significant 160 bits, and so on.  The increment is performed
 * modulo the size of the seed string.
 *
 * The most significant *two* bits are forced to 1, the first to
 * ensure that the number is long enough, and the second just to
 * place the prime in the high half of the range to make breaking
 * it slightly more difficult, since it makes essentially no
 * difference to the use of the number.
 */
static int
genRandBn(struct BigNum *bn, unsigned bits, unsigned char high,
unsigned char low, unsigned char *seed, unsigned len)
{
	unsigned char buf[SHA_DIGESTSIZE];
	unsigned bytes;
	unsigned l = 0;	/* Current position */
	unsigned i;
	struct SHAContext sha;

	bnSetQ(bn, 0);

	bytes = (bits+7) / 8;	/* Number of bytes to use */
	shaInit(&sha);
	shaUpdate(&sha, seed, len);
	shaFinal(&sha, buf);
	buf[SHA_DIGESTSIZE-1] |= low;

	while (bytes > SHA_DIGESTSIZE) {
		bytes -= SHA_DIGESTSIZE;
		/* Merge in low half of high bits, if necessary */
		if (bytes == 1 && (bits & 7))
			buf[0] |= high << (bits & 7);
		if (bnInsertBigBytes(bn, buf, l, SHA_DIGESTSIZE) < 0)
			return -1;
		l += SHA_DIGESTSIZE;

		/* Increment the seed, ignoring carry out. */
		i = len;
		while (i--) {
			if (++seed[i] & 255)
				break;	/* Didn't wrap; done */
		}
		shaInit(&sha);
		shaUpdate(&sha, seed, len);
		shaFinal(&sha, buf);
	}

	/* Do the final "bytes"-long section, using the tail bytes in buf */
	/* Mask off excess high bits */
	buf[SHA_DIGESTSIZE-bytes] &= 255 >> (-bits & 7);
	/* Merge in specified high bits */
	buf[SHA_DIGESTSIZE-bytes] |= high >> (-bits & 7);
	if (bytes > 1 && (bits & 7))
		buf[SHA_DIGESTSIZE-bytes+1] |= high << (bits & 7);
	/* Merge in the appropriate bytes of the buffer */
	if (bnInsertBigBytes(bn, buf+SHA_DIGESTSIZE-bytes, l, bytes) < 0)
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
genDH(struct BigNum *bn, unsigned bits, unsigned char *seed, unsigned len,
	FILE *f)
{
#if CLOCK_AVAIL
	timetype start, stop;
	unsigned long s;
#endif
	int i;
	unsigned char s1[1024], s2[1024];
	unsigned p1, p2;
	struct BigNum step;
	struct Progress progress;

	if (f)
		fprintf(f, "Generating a %u-bit D-H prime with \"%.*s\"\n",
			bits, (int)len, (char *)seed);
	progress.f = f;
	progress.column = 0;
	progress.wrap = 78;

	/* Find p - choose a starting place */
	if (genRandBn(bn, bits, 0xC0, 3, seed, len) < 0)
		return -1;
#if BNDEBUG /* DEBUG - check that sieve works properly */
	bnBegin(&step);
	bnSetQ(&step, 2);
	sieveBuild(s1, 1024, bn, 2, 0);
	sieveBuildBig(s2, 1024, bn, &step, 0);
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
	i = germainPrimeGen(bn, 1, f ? genProgress : 0, (void *)&progress);
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
	bndPrintf("%u-bit time = %lu.%03u sec.", bits, s, msec(stop));
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

	bndPut("p = ", bn);

	return 0;
}

static int
testDH(struct BigNum *bn)
{
	struct BigNum pub1, pub2, sec1, sec2;
	unsigned bits;
	int i = 0;
	char buf[4];

	bnBegin(&pub1);
	bnBegin(&pub2);
	bnBegin(&sec1);
	bnBegin(&sec2);

	/* Bits of secret - add a few to ensure an even distribution */
	bits = bnBits(bn)+4;
	/* Temporarily decrement bn for some operations */
	(void)bnSubQ(bn, 1);

	strcpy(buf, "foo");
	i = genRandBn(&sec1, bits, 0, 0, (unsigned char *)buf, 4);
	if (i < 0)
		goto done;
	/* Reduce sec1 to the correct range */
	i = bnMod(&sec1, &sec1, bn);
	if (i < 0)
		goto done;

	strcpy(buf, "bar");
	i = genRandBn(&sec2, bits, 0, 0, (unsigned char *)buf, 4);
	if (i < 0)
		goto done;
	/* Reduce sec2 to the correct range */
	i = bnMod(&sec2, &sec2, bn);
	if (i < 0)
		goto done;

	/* Re-increment bn */
	(void)bnAddQ(bn, 1);

	puts("Doing first half for party 1");
	i = bnTwoExpMod(&pub1, &sec1, bn);
	if (i < 0)
		goto done;
	puts("Doing first half for party 2");
	i = bnTwoExpMod(&pub2, &sec2, bn);
	if (i < 0)
		goto done;

	/* In a real protocol, pub1 and pub2 are now exchanged */

	puts("Doing second half for party 1");
	i = bnExpMod(&pub2, &pub2, &sec1, bn);
	if (i < 0)
		goto done;
	bndPut("shared = ", &pub2);
	puts("Doing second half for party 2");
	i = bnExpMod(&pub1, &pub1, &sec2, bn);
	if (i < 0)
		goto done;
	bndPut("shared = ", &pub1);

	if (bnCmp(&pub1, &pub2) != 0) {
		puts("Diffie-Hellman failed!");
		i = -1;
	} else {
		puts("Test successful.");
	}
done:
	bnEnd(&sec2);
	bnEnd(&sec1);
	bnEnd(&pub2);
	bnEnd(&pub1);

	return i;
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
	struct BigNum bn;
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
	bnBegin(&bn);
	
	len = copy(buf, argc, argv);
	genDH(&bn, 0x100, buf, len, stdout);
	testDH(&bn);

	len = copy(buf, argc, argv);
	genDH(&bn, 0x200, buf, len, stdout);
	testDH(&bn);

	len = copy(buf, argc, argv);
	genDH(&bn, 0x300, buf, len, stdout);
	testDH(&bn);

	len = copy(buf, argc, argv);
	genDH(&bn, 0x400, buf, len, stdout);
	testDH(&bn);

	len = copy(buf, argc, argv);
	genDH(&bn, 0x500, buf, len, stdout);
	testDH(&bn);

#if 0
	/* These get *really* slow */
	len = copy(buf, argc, argv);
	genDH(&bn, 0x600, buf, len, stdout);
	testDH(&bn);

	len = copy(buf, argc, argv);
	genDH(&bn, 0x800, buf, len, stdout);
	testDH(&bn);

	len = copy(buf, argc, argv);
	genDH(&bn, 0xc00, buf, len, stdout);
	testDH(&bn);

	/* Like, plan on a *week* or more for this one. */
	len = copy(buf, argc, argv);
	genDH(&bn, 0x1000, buf, len, stdout);
	testDH(&bn);
#endif

	bnEnd(&bn);

	return 0;
}
