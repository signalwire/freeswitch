/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#include <stdio.h>
struct PubKey;
struct SecKey;

int
genRsaKey(struct PubKey *pub, struct SecKey *sec,
	  unsigned bits, unsigned exp, FILE *file);
