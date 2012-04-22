/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * sieve.h - Trial division for prime finding.
 *
 * This is generally not intended for direct use by a user of the library;
 * the prime.c and dhprime.c functions. are more likely to be used.
 * However, a special application may need these.
 */
struct BigNum;

/* Remove multiples of a single number from the sieve */
void
sieveSingle(unsigned char *array, unsigned size, unsigned start, unsigned step);

/* Build a sieve starting at the number and incrementing by "step". */
int sieveBuild(unsigned char *array, unsigned size, struct BigNum const *bn,
	unsigned step, unsigned dbl);

/* Similar, but uses a >16-bit step size */
int sieveBuildBig(unsigned char *array, unsigned size, struct BigNum const *bn,
	struct BigNum const *step, unsigned dbl);

/* Return the next bit set in the sieve (or 0 on failure) */
unsigned sieveSearch(unsigned char const *array, unsigned size, unsigned start);
