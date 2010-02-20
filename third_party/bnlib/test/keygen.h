#include <stdio.h>
struct PubKey;
struct SecKey;

int
genRsaKey(struct PubKey *pub, struct SecKey *sec,
	  unsigned bits, unsigned exp, FILE *file);
