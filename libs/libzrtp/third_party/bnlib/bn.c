/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bn.c - the high-level bignum interface
 */

#include "bn.h"

/* Functions */
void
bnBegin(struct BigNum *bn)
{
	static int bninit = 0;

	if (!bninit) {
		bnInit();
		bninit = 1;
	}

	bn->ptr = 0;
	bn->size = 0;
	bn->allocated = 0;
}

void
bnSwap(struct BigNum *a, struct BigNum *b)
{
	void *p;
	unsigned t;

	p = a->ptr;
	a->ptr = b->ptr;
	b->ptr = p;

	t = a->size;
	a->size = b->size;
	b->size = t;

	t = a->allocated;
	a->allocated = b->allocated;
	b->allocated = t;
}

int (*bnYield)(void);

void (*bnEnd)(struct BigNum *bn);
int (*bnPrealloc)(struct BigNum *bn, unsigned bits);
int (*bnCopy)(struct BigNum *dest, struct BigNum const *src);
void (*bnNorm)(struct BigNum *bn);
void (*bnExtractBigBytes)(struct BigNum const *bn, unsigned char *dest,
	unsigned lsbyte, unsigned len);
int (*bnInsertBigBytes)(struct BigNum *bn, unsigned char const *src,
	unsigned lsbyte, unsigned len);
void (*bnExtractLittleBytes)(struct BigNum const *bn, unsigned char *dest,
	unsigned lsbyte, unsigned len);
int (*bnInsertLittleBytes)(struct BigNum *bn, unsigned char const *src,
	unsigned lsbyte, unsigned len);
unsigned (*bnLSWord)(struct BigNum const *src);
int (*bnReadBit)(struct BigNum const *bn, unsigned bit);
unsigned (*bnBits)(struct BigNum const *src);
int (*bnAdd)(struct BigNum *dest, struct BigNum const *src);
int (*bnSub)(struct BigNum *dest, struct BigNum const *src);
int (*bnCmpQ)(struct BigNum const *a, unsigned b);
int (*bnSetQ)(struct BigNum *dest, unsigned src);
int (*bnAddQ)(struct BigNum *dest, unsigned src);
int (*bnSubQ)(struct BigNum *dest, unsigned src);
int (*bnCmp)(struct BigNum const *a, struct BigNum const *b);
int (*bnSquare)(struct BigNum *dest, struct BigNum const *src);
int (*bnMul)(struct BigNum *dest, struct BigNum const *a,
	struct BigNum const *b);
int (*bnMulQ)(struct BigNum *dest, struct BigNum const *a, unsigned b);
int (*bnDivMod)(struct BigNum *q, struct BigNum *r, struct BigNum const *n,
	struct BigNum const *d);
int (*bnMod)(struct BigNum *dest, struct BigNum const *src,
	struct BigNum const *d);
unsigned (*bnModQ)(struct BigNum const *src, unsigned d);
int (*bnExpMod)(struct BigNum *result, struct BigNum const *n,
	struct BigNum const *exp, struct BigNum const *mod);
int (*bnDoubleExpMod)(struct BigNum *dest,
	struct BigNum const *n1, struct BigNum const *e1,
	struct BigNum const *n2, struct BigNum const *e2,
	struct BigNum const *mod);
int (*bnTwoExpMod)(struct BigNum *n, struct BigNum const *exp,
	struct BigNum const *mod);
int (*bnGcd)(struct BigNum *dest, struct BigNum const *a,
	struct BigNum const *b);
int (*bnInv)(struct BigNum *dest, struct BigNum const *src,
	struct BigNum const *mod);
int (*bnLShift)(struct BigNum *dest, unsigned amt);
void (*bnRShift)(struct BigNum *dest, unsigned amt);
unsigned (*bnMakeOdd)(struct BigNum *n);
int (*bnBasePrecompBegin)(struct BnBasePrecomp *pre, struct BigNum const *base,
	struct BigNum const *mod, unsigned maxebits);
int (*bnBasePrecompCopy)(struct BnBasePrecomp *dst,
	struct BnBasePrecomp const *src);
void (*bnBasePrecompEnd)(struct BnBasePrecomp *pre);
int (*bnBasePrecompExpMod)(struct BigNum *dest,
	struct BnBasePrecomp const *pre, struct BigNum const *exp,
	struct BigNum const *mod);
int (*bnDoubleBasePrecompExpMod)(struct BigNum *dest,
	struct BnBasePrecomp const *pre1, struct BigNum const *exp1,
	struct BnBasePrecomp const *pre2, struct BigNum const *exp2,
	struct BigNum const *mod);
