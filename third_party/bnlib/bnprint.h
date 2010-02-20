#ifndef BNPRINT_H
#define BNPRINT_H

#include <stdio.h>
struct BigNum;

int bnPrint(FILE *f, char const *prefix, struct BigNum const *bn,
	char const *suffix);

#endif /* BNPRINT_H */
