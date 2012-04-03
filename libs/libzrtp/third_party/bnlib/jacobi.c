/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * Compute the Jacobi symbol (small prime case only).
 */
#include "bn.h"
#include "jacobi.h"

/*
 * For a small (usually prime, but not necessarily) prime p,
 * compute Jacobi(p,bn), which is -1, 0 or +1, using the following rules:
 * Jacobi(x, y) = Jacobi(x mod y, y)
 * Jacobi(0, y) = 0
 * Jacobi(1, y) = 1
 * Jacobi(2, y) = 0 if y is even, +1 if y is +/-1 mod 8, -1 if y = +/-3 mod 8
 * Jacobi(x1*x2, y) = Jacobi(x1, y) * Jacobi(x2, y) (used with x1 = 2 & x1 = 4)
 * If x and y are both odd, then
 * Jacobi(x, y) = Jacobi(y, x) * (-1 if x = y = 3 mod 4, +1 otherwise)
 */
int
bnJacobiQ(unsigned p, struct BigNum const *bn)
{
	int j = 1;
	unsigned u = bnLSWord(bn);

	if (!(u & 1))
		return 0;	/* Don't *do* that */

	/* First, get rid of factors of 2 in p */
	while ((p & 3) == 0)
		p >>= 2;
	if ((p & 1) == 0) {
		p >>= 1;
		if ((u ^ u>>1) & 2)
			j = -j;		/* 3 (011) or 5 (101) mod 8 */
	}
	if (p == 1)
		return j;
	/* Then, apply quadratic reciprocity */
	if (p & u & 2)	/* p = u = 3 (mod 4? */
		j = -j;
	/* And reduce u mod p */
	u = bnModQ(bn, p);

	/* Now compute Jacobi(u,p), u < p */
	while (u) {
		while ((u & 3) == 0)
			u >>= 2;
		if ((u & 1) == 0) {
			u >>= 1;
			if ((p ^ p>>1) & 2)
				j = -j;	/* 3 (011) or 5 (101) mod 8 */
		}
		if (u == 1)
			return j;
		/* Now both u and p are odd, so use quadratic reciprocity */
		if (u < p) {
			unsigned t = u; u = p; p = t;
			if (u & p & 2)	/* u = p = 3 (mod 4? */
				j = -j;
		}
		/* Now u >= p, so it can be reduced */
		u %= p;
	}
	return 0;
}
