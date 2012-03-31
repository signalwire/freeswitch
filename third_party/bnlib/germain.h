/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
struct BigNum;

/* Generate a Sophie Germain prime */
int germainPrimeGen(struct BigNum *bn, unsigned order,
	int (*f)(void *arg, int c), void *arg);
/* The same, but search for using the given step size */
int germainPrimeGenStrong(struct BigNum *bn, struct BigNum const *step,
	unsigned order, int (*f)(void *arg, int c), void *arg);
