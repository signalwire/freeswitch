struct BigNum;

/* Generate a prime >= bn. leaving the result in bn. */
int primeGen(struct BigNum *bn, unsigned (*randfunc)(unsigned),
	int (*f)(void *arg, int c), void *arg, unsigned exponent, ...);

/*
 * Generate a prime of the form bn + k*step.  Step must be even and
 * bn must be odd.
 */
int primeGenStrong(struct BigNum *bn, struct BigNum const *step,
	int (*f)(void *arg, int c), void *arg);
