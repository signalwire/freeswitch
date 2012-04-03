/*
 * Copyright (c) 1994, 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * rsatest.c - Test driver for RSA key generation.
 */

#include "first.h"
#include <stdio.h>
#include <stdlib.h>	/* For strtoul() */
#include <string.h>	/* For strerror */

#include "bnprint.h"
#include "cputime.h"

#include "keygen.h"
#include "keys.h"
#include "random.h"
#include "rsaglue.h"
#include "userio.h"

#include "kludge.h"

#define bnPut(prompt, bn) bnPrint(stdout, prompt, bn, "\n")

static int
rsaTest(struct PubKey const *pub, struct SecKey const *sec)
{
	struct BigNum bn;
	char const buf1[25] = "abcdefghijklmnopqrstuvwxy";
	char buf2[64];
	int i, j;
#if CLOCK_AVAIL
	timetype start, stop;
	unsigned long cursec, encsec = 0, decsec = 0, sigsec = 0, versec = 0;
	unsigned curms, encms = 0, decms = 0, sigms = 0, verms = 0;
#endif

	if (rsaKeyTooBig(pub, sec)) {
		printf("Key too large for RSA library - not testing.\n");
		return 0;
	}

	puts("\tEncrypt\t\tDecrypt\t\tSign\t\tVerify\tStatus");
	bnBegin(&bn);

	for (j = 0; j < (int)sizeof(buf1); j++) {
#if CLOCK_AVAIL
		gettime(&start);
#endif
		i = rsaPublicEncrypt(&bn, (byte const *)buf1, (size_t)j+1, pub);
		if (i < 0) {
			printf("RSA encryption failed, i = %dn", i);
			return i;
		}
#if CLOCK_AVAIL
		gettime(&stop);
		subtime(stop, start);
		encsec += cursec = sec(stop);
		encms += curms = msec(stop);
		printf("\t%lu.%03u\t", cursec, curms);
#else
		printf("\t*\t");
#endif
		fflush(stdout);
#if CLOCK_AVAIL
		gettime(&start);
#endif
		i = rsaPrivateDecrypt((byte *)buf2, sizeof(buf2), &bn,
				      pub, sec);
#if CLOCK_AVAIL
		gettime(&stop);
		subtime(stop, start);
		decsec += cursec = sec(stop);
		decms += curms = msec(stop);
		printf("\t%lu.%03u\t", cursec, curms);
#else
		printf("\t*\t");
#endif
		fflush(stdout);
		if (i != j+1 || memcmp(buf1, buf2, (size_t)j+1) != 0) {
			printf("RSA Decryption failed, i = %d\n", i);
			return i;
		}
#if CLOCK_AVAIL
		gettime(&start);
#endif
		i = rsaPrivateEncrypt(&bn, (byte const *)buf1, (size_t)j+1,
		                      pub, sec);
#if CLOCK_AVAIL
		gettime(&stop);
		subtime(stop, start);
		sigsec += cursec = sec(stop);
		sigms += curms = msec(stop);
		printf("\t%lu.%03u\t", cursec, curms);
#else
		printf("\t*\t");
#endif
		fflush(stdout);
		if (i < 0) {
			printf("RSA signing failed, i = %d\n", i);
			return i;
		}

#if CLOCK_AVAIL
		gettime(&start);
#endif
		i = rsaPublicDecrypt((byte *)buf2, sizeof(buf2), &bn, pub);
#if CLOCK_AVAIL
		gettime(&stop);
		subtime(stop, start);
		versec += cursec = sec(stop);
		verms += curms = msec(stop);
		printf("\t%lu.%03u\t", cursec, curms);
#else
		printf("\t*\t");
#endif
		fflush(stdout);
		if (i != j+1 || memcmp(buf1, buf2, (size_t)j+1) != 0) {
			printf("RSA verify failed i = %d != %d\n", i, j+1);
			return i;
		}
		printf("Succeeded\n");
		fflush(stdout);
	}
#if CLOCK_AVAIL
	encms += 1000 * (encsec % j);
	encsec /= j;
	encms /= j;
	encsec += encms / 1000;
	encms %= 1000;
	decms += 1000 * (decsec % j);
	decsec /= j;
	decms /= j;
	decsec += decms / 1000;
	decms %= 1000;
	sigms += 1000 * (sigsec % j);
	sigsec /= j;
	sigms /= j;
	sigsec += sigms / 1000;
	sigms %= 1000;
	verms += 1000 * (versec % j);
	versec /= j;
	verms /= j;
	versec += verms / 1000;
	verms %= 1000;
	printf("\t%lu.%03u\t\t%lu.%03u\t\t%lu.%03u\t\t%lu.%03u\tAVERAGE %u\n",
	       encsec, encms, decsec, decms,
	       sigsec, sigms, versec, verms, bnBits(&pub->n));
#endif

	return 0;
}

static int
rsaGen(unsigned keybits)
{
	struct PubKey pub;
	struct SecKey sec;
	int i;
#if CLOCK_AVAIL
	timetype start, stop;
	unsigned long s;
#endif

	if (keybits < 384)
		keybits = 384;
	userPrintf("Generating an RSA key with a %u-bit modulus.\n", keybits);

	randAccum(1);
/*	randAccum(keybits); */

	/*
	 * One dot is printed per pseudoprimality test that fails.
	 * the density of primes of length "keybits/2" is about
	 * ln(2^(keybits/2)), or keybits/2*ln(2), so if we were to
	 * naively test numbers at random, we'd expect to print
	 * keybits/2*ln(2) dots per number, or keybits*ln(2) for
	 * both.  This is keybits/1.44.
	 * However, the sieve removes all multiples of 2, 3, 5, 7, 11, 13,
	 * etc (up to 65521, the largest prime < 65536) from the candidates.
	 * (1-1/2)*(1-1/3)*(1-1/5)*(1-1/7)*(1-1/11)*...*(1-1/65521) is
	 * about 0.05061325.  So we only actually print keybits*ln(2)*0.0506
	 * from the numbers we test, 0.035 of them, or about 1/28.5.
	 * We round this up to 0.04, or 1/25, because it produces nice
	 * round numbers and people don't get as impatient if we're a
	 * little pessimistic.  (The Poisson distribution has a long
	 * tail.)  If you really want to know, it's a 14% overestimate.
	 */
	userPrintf("\n\
Key generation takes a little while.  This program prints dots as it\n\
searches for each of the two primes it needs for a key.  How long it will\n\
have to search is unpredictable, but expect an average of %u dots total.\n",
	           keybits/25);

	pubKeyBegin(&pub);
	secKeyBegin(&sec);
#if CLOCK_AVAIL
	gettime(&start);
#endif
	i = genRsaKey(&pub, &sec, keybits, 17, stdout);
#if CLOCK_AVAIL
	gettime(&stop);
	subtime(stop, start);
	s = sec(stop);
	printf("%u-bit time = %lu.%03u sec.", keybits, s, msec(stop));
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
	if (i < 0) {
		userPuts("\a\nKeygen failed!\n");
	} else {
		userPrintf("%d modular exponentiations performed.\n", i);
		bnPut("n = ", &pub.n);
		bnPut("e = ", &pub.e);
		bnPut("d = ", &sec.d);
		bnPut("p = ", &sec.p);
		bnPut("q = ", &sec.q);
		bnPut("u = ", &sec.u);
		i = rsaTest(&pub, &sec);
	}

	pubKeyEnd(&pub);
	secKeyEnd(&sec);
	return i;
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

	bnInit();

	while (--argc) {
		t = strtoul(*++argv, &p, 0);
		if (t < 384 || t > 65536 || *p) {
			fprintf(stderr, "Illegal modulus size: \"%s\"\n",
			        *argv);
			return 1;
		}

		rsaGen((unsigned)t);
	}

	return 0;
}
