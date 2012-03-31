/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef KEYS_H
#define KEYS_H

/*
 * Structures for keys.
 */

#include "bn.h"

/* A structure to hold a public key */
struct PubKey {
	struct BigNum n;	/* The public modulus */
	struct BigNum e;	/* The public exponent */
};

/* A structure to hold a secret key */
struct SecKey {
	struct BigNum d;	/* Decryption exponent */
	struct BigNum p;	/* The smaller factor of n */
	struct BigNum q;	/* The larger factor of n */
	struct BigNum u;	/* 1/p (mod q) */
};

void pubKeyBegin(struct PubKey *pub);
void pubKeyEnd(struct PubKey *pub);

void secKeyBegin(struct SecKey *sec);
void secKeyEnd(struct SecKey *sec);

#endif /* KEYS_H */
