/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef BNPRINT_H
#define BNPRINT_H

#include <stdio.h>
struct BigNum;

int bnPrint(FILE *f, char const *prefix, struct BigNum const *bn,
	char const *suffix);

#endif /* BNPRINT_H */
