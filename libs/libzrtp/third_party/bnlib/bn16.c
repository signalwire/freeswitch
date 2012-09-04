/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bn16.c - the high-level bignum interface
 *
 * Like lbn16.c, this reserves the string "16" for textual replacement.
 * The string must not appear anywhere unless it is intended to be replaced
 * to generate other bignum interface functions.
 */

#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H 0
#endif
#if HAVE_CONFIG_H
#include "bnconfig.h"
#endif

/*
 * Some compilers complain about #if FOO if FOO isn't defined,
 * so do the ANSI-mandated thing explicitly...
 */
#ifndef NO_ASSERT_H
#define NO_ASSERT_H 0
#endif
#ifndef NO_STRING_H
#define NO_STRING_H 0
#endif
#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 0
#endif

#if !NO_ASSERT_H
#include <assert.h>
#else
#define assert(x) (void)0
#endif

#if !NO_STRING_H
#include <string.h>	/* for memmove() in bnMakeOdd */
#elif HAVE_STRINGS_H
#include <strings.h>
#endif

/*
 * This was useful during debugging, so it's left in here.
 * You can ignore it.  DBMALLOC is generally undefined.
 */
#ifndef DBMALLOC
#define DBMALLOC 0
#endif
#if DBMALLOC
#include "../dbmalloc/malloc.h"
#define MALLOCDB malloc_chain_check(1)
#else
#define MALLOCDB (void)0
#endif

#include "lbn.h"
#include "lbn16.h"
#include "lbnmem.h"
#include "bn16.h"
#include "bn.h"

/* Work-arounds for some particularly broken systems */
#include "kludge.h"	/* For memmove() */

/* Functions */
void
bnInit_16(void)
{
	bnEnd = bnEnd_16;
	bnPrealloc = bnPrealloc_16;
	bnCopy = bnCopy_16;
	bnNorm = bnNorm_16;
	bnExtractBigBytes = bnExtractBigBytes_16;
	bnInsertBigBytes = bnInsertBigBytes_16;
	bnExtractLittleBytes = bnExtractLittleBytes_16;
	bnInsertLittleBytes = bnInsertLittleBytes_16;
	bnLSWord = bnLSWord_16;
	bnReadBit = bnReadBit_16;
	bnBits = bnBits_16;
	bnAdd = bnAdd_16;
	bnSub = bnSub_16;
	bnCmpQ = bnCmpQ_16;
	bnSetQ = bnSetQ_16;
	bnAddQ = bnAddQ_16;
	bnSubQ = bnSubQ_16;
	bnCmp = bnCmp_16;
	bnSquare = bnSquare_16;
	bnMul = bnMul_16;
	bnMulQ = bnMulQ_16;
	bnDivMod = bnDivMod_16;
	bnMod = bnMod_16;
	bnModQ = bnModQ_16;
	bnExpMod = bnExpMod_16;
	bnDoubleExpMod = bnDoubleExpMod_16;
	bnTwoExpMod = bnTwoExpMod_16;
	bnGcd = bnGcd_16;
	bnInv = bnInv_16;
	bnLShift = bnLShift_16;
	bnRShift = bnRShift_16;
	bnMakeOdd = bnMakeOdd_16;
	bnBasePrecompBegin = bnBasePrecompBegin_16;
	bnBasePrecompEnd = bnBasePrecompEnd_16;
	bnBasePrecompExpMod = bnBasePrecompExpMod_16;
	bnDoubleBasePrecompExpMod = bnDoubleBasePrecompExpMod_16;
}

void
bnEnd_16(struct BigNum *bn)
{
	if (bn->ptr) {
		LBNFREE((BNWORD16 *)bn->ptr, bn->allocated);
		bn->ptr = 0;
	}
	bn->size = 0;
	bn->allocated = 0;

	MALLOCDB;
}

/* Internal function.  It operates in words. */
static int
bnResize_16(struct BigNum *bn, unsigned len)
{
	void *p;

	/* Round size up: most mallocs impose 8-byte granularity anyway */
	len = (len + (8/sizeof(BNWORD16) - 1)) & ~(8/sizeof(BNWORD16) - 1);
	p = LBNREALLOC((BNWORD16 *)bn->ptr, bn->allocated, len);
	if (!p)
		return -1;
	bn->ptr = p;
	bn->allocated = len;

	MALLOCDB;

	return 0;
}

#define bnSizeCheck(bn, size) \
	if (bn->allocated < size && bnResize_16(bn, size) < 0) \
		return -1

/* Preallocate enough space in bn to hold "bits" bits. */
int
bnPrealloc_16(struct BigNum *bn, unsigned bits)
{
	bits = (bits + 16-1)/16;
	bnSizeCheck(bn, bits);
	MALLOCDB;
	return 0;
}

int
bnCopy_16(struct BigNum *dest, struct BigNum const *src)
{
	bnSizeCheck(dest, src->size);
	dest->size = src->size;
	lbnCopy_16((BNWORD16 *)dest->ptr, (BNWORD16 *)src->ptr, src->size);
	MALLOCDB;
	return 0;
}

/* Is this ever needed?  Normalize the bn by deleting high-order 0 words */
void
bnNorm_16(struct BigNum *bn)
{
	bn->size = lbnNorm_16((BNWORD16 *)bn->ptr, bn->size);
}

/*
 * Convert a bignum to big-endian bytes.  Returns, in big-endian form, a
 * substring of the bignum starting from lsbyte and "len" bytes long.
 * Unused high-order (leading) bytes are filled with 0.
 */
void
bnExtractBigBytes_16(struct BigNum const *bn, unsigned char *dest,
                  unsigned lsbyte, unsigned len)
{
	unsigned s = bn->size * (16 / 8);

	/* Fill unused leading bytes with 0 */
	while (s < lsbyte + len) {
		*dest++ = 0;
		len--;
	}

	if (len)
		lbnExtractBigBytes_16((BNWORD16 *)bn->ptr, dest, lsbyte, len);
	MALLOCDB;
}

/* The inverse of the above. */
int
bnInsertBigBytes_16(struct BigNum *bn, unsigned char const *src,
                 unsigned lsbyte, unsigned len)
{
	unsigned s = bn->size;
	unsigned words = (len+lsbyte+sizeof(BNWORD16)-1) / sizeof(BNWORD16);

	/* Pad with zeros as required */
	bnSizeCheck(bn, words);

	if (s < words) {
		lbnZero_16((BNWORD16 *)bn->ptr BIGLITTLE(-s,+s), words-s);
		s = words;
	}

	lbnInsertBigBytes_16((BNWORD16 *)bn->ptr, src, lsbyte, len);

	bn->size = lbnNorm_16((BNWORD16 *)bn->ptr, s);

	MALLOCDB;
	return 0;
}


/*
 * Convert a bignum to little-endian bytes.  Returns, in little-endian form, a
 * substring of the bignum starting from lsbyte and "len" bytes long.
 * Unused high-order (trailing) bytes are filled with 0.
 */
void
bnExtractLittleBytes_16(struct BigNum const *bn, unsigned char *dest,
                  unsigned lsbyte, unsigned len)
{
	unsigned s = bn->size * (16 / 8);

	/* Fill unused leading bytes with 0 */
	while (s < lsbyte + len)
		dest[--len] = 0;

	if (len)
		lbnExtractLittleBytes_16((BNWORD16 *)bn->ptr, dest,
		                         lsbyte, len);
	MALLOCDB;
}

/* The inverse of the above */
int
bnInsertLittleBytes_16(struct BigNum *bn, unsigned char const *src,
                       unsigned lsbyte, unsigned len)
{
	unsigned s = bn->size;
	unsigned words = (len+lsbyte+sizeof(BNWORD16)-1) / sizeof(BNWORD16);

	/* Pad with zeros as required */
	bnSizeCheck(bn, words);

	if (s < words) {
		lbnZero_16((BNWORD16 *)bn->ptr BIGLITTLE(-s,+s), words-s);
		s = words;
	}

	lbnInsertLittleBytes_16((BNWORD16 *)bn->ptr, src, lsbyte, len);

	bn->size = lbnNorm_16((BNWORD16 *)bn->ptr, s);

	MALLOCDB;
	return 0;
}

/* Return the least-significant word of the input. */
unsigned
bnLSWord_16(struct BigNum const *bn)
{
	return bn->size ? (unsigned)((BNWORD16 *)bn->ptr)[BIGLITTLE(-1,0)]: 0;
}

/* Return a selected bit of the data */
int
bnReadBit_16(struct BigNum const *bn, unsigned bit)
{
	BNWORD16 word;
	if (bit/16 >= bn->size)
		return 0;
	word = ((BNWORD16 *)bn->ptr)[BIGLITTLE(-1-bit/16,bit/16)];
	return (int)(word >> (bit % 16) & 1);
}

/* Count the number of significant bits. */
unsigned
bnBits_16(struct BigNum const *bn)
{
	return lbnBits_16((BNWORD16 *)bn->ptr, bn->size);
}

/* dest += src */
int
bnAdd_16(struct BigNum *dest, struct BigNum const *src)
{
	unsigned s = src->size, d = dest->size;
	BNWORD16 t;

	if (!s)
		return 0;

	bnSizeCheck(dest, s);

	if (d < s) {
		lbnZero_16((BNWORD16 *)dest->ptr BIGLITTLE(-d,+d), s-d);
		dest->size = d = s;
		MALLOCDB;
	}
	t = lbnAddN_16((BNWORD16 *)dest->ptr, (BNWORD16 *)src->ptr, s);
	MALLOCDB;
	if (t) {
		if (d > s) {
			t = lbnAdd1_16((BNWORD16 *)dest->ptr BIGLITTLE(-s,+s),
			               d-s, t);
			MALLOCDB;
		}
		if (t) {
			bnSizeCheck(dest, d+1);
			((BNWORD16 *)dest->ptr)[BIGLITTLE(-1-d,d)] = t;
			dest->size = d+1;
		}
	}
	return 0;
}

/*
 * dest -= src.
 * If dest goes negative, this produces the absolute value of
 * the difference (the negative of the true value) and returns 1.
 * Otherwise, it returls 0.
 */
int
bnSub_16(struct BigNum *dest, struct BigNum const *src)
{
	unsigned s = src->size, d = dest->size;
	BNWORD16 t;

	if (d < s  &&  d < (s = lbnNorm_16((BNWORD16 *)src->ptr, s))) {
		bnSizeCheck(dest, s);
		lbnZero_16((BNWORD16 *)dest->ptr BIGLITTLE(-d,+d), s-d);
		dest->size = d = s;
		MALLOCDB;
	}
	if (!s)
		return 0;
	t = lbnSubN_16((BNWORD16 *)dest->ptr, (BNWORD16 *)src->ptr, s);
	MALLOCDB;
	if (t) {
		if (d > s) {
			t = lbnSub1_16((BNWORD16 *)dest->ptr BIGLITTLE(-s,+s),
			               d-s, t);
			MALLOCDB;
		}
		if (t) {
			lbnNeg_16((BNWORD16 *)dest->ptr, d);
			dest->size = lbnNorm_16((BNWORD16 *)dest->ptr,
			                        dest->size);
			MALLOCDB;
			return 1;
		}
	}
	dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, dest->size);
	return 0;
}

/*
 * Compare the BigNum to the given value, which must be < 65536.
 * Returns -1. 0 or 1 if a<b, a == b or a>b.
 * a <=> b --> bnCmpQ(a,b) <=> 0
 */
int
bnCmpQ_16(struct BigNum const *a, unsigned b)
{
	unsigned t;
	BNWORD16 v;

	t = lbnNorm_16((BNWORD16 *)a->ptr, a->size);
	/* If a is more than one word long or zero, it's easy... */
	if (t != 1)
		return (t > 1) ? 1 : (b ? -1 : 0);
	v = (unsigned)((BNWORD16 *)a->ptr)[BIGLITTLE(-1,0)];
	return (v > b) ? 1 : ((v < b) ? -1 : 0);
}

/* Set dest to a small value */
int
bnSetQ_16(struct BigNum *dest, unsigned src)
{
	if (src) {
		bnSizeCheck(dest, 1);

		((BNWORD16 *)dest->ptr)[BIGLITTLE(-1,0)] = (BNWORD16)src;
		dest->size = 1;
	} else {
		dest->size = 0;
	}
	return 0;
}

/* dest += src */
int
bnAddQ_16(struct BigNum *dest, unsigned src)
{
	BNWORD16 t;

	if (!dest->size)
		return bnSetQ(dest, src);

	t = lbnAdd1_16((BNWORD16 *)dest->ptr, dest->size, (BNWORD16)src);
	MALLOCDB;
	if (t) {
		src = dest->size;
		bnSizeCheck(dest, src+1);
		((BNWORD16 *)dest->ptr)[BIGLITTLE(-1-src,src)] = t;
		dest->size = src+1;
	}
	return 0;
}

/*
 * Return value as for bnSub: 1 if subtract underflowed, in which
 * case the return is the negative of the computed value.
 */
int
bnSubQ_16(struct BigNum *dest, unsigned src)
{
	BNWORD16 t;

	if (!dest->size)
		return bnSetQ(dest, src) < 0 ? -1 : (src != 0);

	t = lbnSub1_16((BNWORD16 *)dest->ptr, dest->size, src);
	MALLOCDB;
	if (t) {
		/* Underflow. <= 1 word, so do it simply. */
		lbnNeg_16((BNWORD16 *)dest->ptr, 1);
		dest->size = 1;
		return 1;
	}
/* Try to normalize?  Needing this is going to be pretty damn rare. */
/*		dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, dest->size); */
	return 0;
}

/*
 * Compare two BigNums.  Returns -1. 0 or 1 if a<b, a == b or a>b.
 * a <=> b --> bnCmp(a,b) <=> 0
 */
int
bnCmp_16(struct BigNum const *a, struct BigNum const *b)
{
	unsigned s, t;

	s = lbnNorm_16((BNWORD16 *)a->ptr, a->size);
	t = lbnNorm_16((BNWORD16 *)b->ptr, b->size);

	if (s != t)
		return s > t ? 1 : -1;
	return lbnCmp_16((BNWORD16 *)a->ptr, (BNWORD16 *)b->ptr, s);
}

/* dest = src*src.  This is more efficient than bnMul. */
int
bnSquare_16(struct BigNum *dest, struct BigNum const *src)
{
	unsigned s;
	BNWORD16 *srcbuf;

	s = lbnNorm_16((BNWORD16 *)src->ptr, src->size);
	if (!s) {
		dest->size = 0;
		return 0;
	}
	bnSizeCheck(dest, 2*s);

	if (src == dest) {
		LBNALLOC(srcbuf, BNWORD16, s);
		if (!srcbuf)
			return -1;
		lbnCopy_16(srcbuf, (BNWORD16 *)src->ptr, s);
		lbnSquare_16((BNWORD16 *)dest->ptr, (BNWORD16 *)srcbuf, s);
		LBNFREE(srcbuf, s);
	} else {
		lbnSquare_16((BNWORD16 *)dest->ptr, (BNWORD16 *)src->ptr, s);
	}

	dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, 2*s);
	MALLOCDB;
	return 0;
}

/* dest = a * b.  Any overlap between operands is allowed. */
int
bnMul_16(struct BigNum *dest, struct BigNum const *a, struct BigNum const *b)
{
	unsigned s, t;
	BNWORD16 *srcbuf;

	s = lbnNorm_16((BNWORD16 *)a->ptr, a->size);
	t = lbnNorm_16((BNWORD16 *)b->ptr, b->size);

	if (!s || !t) {
		dest->size = 0;
		return 0;
	}

	if (a == b)
		return bnSquare_16(dest, a);

	bnSizeCheck(dest, s+t);

	if (dest == a) {
		LBNALLOC(srcbuf, BNWORD16, s);
		if (!srcbuf)
			return -1;
		lbnCopy_16(srcbuf, (BNWORD16 *)a->ptr, s);
		lbnMul_16((BNWORD16 *)dest->ptr, srcbuf, s,
		                                 (BNWORD16 *)b->ptr, t);
		LBNFREE(srcbuf, s);
	} else if (dest == b) {
		LBNALLOC(srcbuf, BNWORD16, t);
		if (!srcbuf)
			return -1;
		lbnCopy_16(srcbuf, (BNWORD16 *)b->ptr, t);
		lbnMul_16((BNWORD16 *)dest->ptr, (BNWORD16 *)a->ptr, s,
		                                 srcbuf, t);
		LBNFREE(srcbuf, t);
	} else {
		lbnMul_16((BNWORD16 *)dest->ptr, (BNWORD16 *)a->ptr, s,
		                                 (BNWORD16 *)b->ptr, t);
	}
	dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, s+t);
	MALLOCDB;
	return 0;
}

/* dest = a * b */
int
bnMulQ_16(struct BigNum *dest, struct BigNum const *a, unsigned b)
{
	unsigned s;

	s = lbnNorm_16((BNWORD16 *)a->ptr, a->size);
	if (!s || !b) {
		dest->size = 0;
		return 0;
	}
	if (b == 1)
		return bnCopy_16(dest, a);
	bnSizeCheck(dest, s+1);
	lbnMulN1_16((BNWORD16 *)dest->ptr, (BNWORD16 *)a->ptr, s, b);
	dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, s+1);
	MALLOCDB;
	return 0;
}

/* q = n/d, r = n % d */
int
bnDivMod_16(struct BigNum *q, struct BigNum *r, struct BigNum const *n,
            struct BigNum const *d)
{
	unsigned dsize, nsize;
	BNWORD16 qhigh;

	dsize = lbnNorm_16((BNWORD16 *)d->ptr, d->size);
	nsize = lbnNorm_16((BNWORD16 *)n->ptr, n->size);

	if (nsize < dsize) {
		q->size = 0;	/* No quotient */
		r->size = nsize;
		return 0;	/* Success */
	}

	bnSizeCheck(q, nsize-dsize);

	if (r != n) {	/* You are allowed to reduce in place */
		bnSizeCheck(r, nsize);
		lbnCopy_16((BNWORD16 *)r->ptr, (BNWORD16 *)n->ptr, nsize);
	}

	qhigh = lbnDiv_16((BNWORD16 *)q->ptr, (BNWORD16 *)r->ptr, nsize,
	                  (BNWORD16 *)d->ptr, dsize);
	nsize -= dsize;
	if (qhigh) {
		bnSizeCheck(q, nsize+1);
		*((BNWORD16 *)q->ptr BIGLITTLE(-nsize-1,+nsize)) = qhigh;
		q->size = nsize+1;
	} else {
		q->size = lbnNorm_16((BNWORD16 *)q->ptr, nsize);
	}
	r->size = lbnNorm_16((BNWORD16 *)r->ptr, dsize);
	MALLOCDB;
	return 0;
}

/* det = src % d */
int
bnMod_16(struct BigNum *dest, struct BigNum const *src, struct BigNum const *d)
{
	unsigned dsize, nsize;

	nsize = lbnNorm_16((BNWORD16 *)src->ptr, src->size);
	dsize = lbnNorm_16((BNWORD16 *)d->ptr, d->size);


	if (dest != src) {
		bnSizeCheck(dest, nsize);
		lbnCopy_16((BNWORD16 *)dest->ptr, (BNWORD16 *)src->ptr, nsize);
	}

	if (nsize < dsize) {
		dest->size = nsize;	/* No quotient */
		return 0;
	}

	(void)lbnDiv_16((BNWORD16 *)dest->ptr BIGLITTLE(-dsize,+dsize),
	                (BNWORD16 *)dest->ptr, nsize,
	                (BNWORD16 *)d->ptr, dsize);
	dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, dsize);
	MALLOCDB;
	return 0;
}

/* return src % d. */
unsigned
bnModQ_16(struct BigNum const *src, unsigned d)
{
	unsigned s;

	s = lbnNorm_16((BNWORD16 *)src->ptr, src->size);
	if (!s)
		return 0;

	if (d & (d-1))	/* Not a power of 2 */
		d = lbnModQ_16((BNWORD16 *)src->ptr, s, d);
	else
		d = (unsigned)((BNWORD16 *)src->ptr)[BIGLITTLE(-1,0)] & (d-1);
	return d;
}

/* dest = n^exp (mod mod) */
int
bnExpMod_16(struct BigNum *dest, struct BigNum const *n,
	struct BigNum const *exp, struct BigNum const *mod)
{
	unsigned nsize, esize, msize;

	nsize = lbnNorm_16((BNWORD16 *)n->ptr, n->size);
	esize = lbnNorm_16((BNWORD16 *)exp->ptr, exp->size);
	msize = lbnNorm_16((BNWORD16 *)mod->ptr, mod->size);

	if (!msize || (((BNWORD16 *)mod->ptr)[BIGLITTLE(-1,0)] & 1) == 0)
		return -1;	/* Illegal modulus! */

	bnSizeCheck(dest, msize);

	/* Special-case base of 2 */
	if (nsize == 1 && ((BNWORD16 *)n->ptr)[BIGLITTLE(-1,0)] == 2) {
		if (lbnTwoExpMod_16((BNWORD16 *)dest->ptr,
				    (BNWORD16 *)exp->ptr, esize,
				    (BNWORD16 *)mod->ptr, msize) < 0)
			return -1;
	} else {
		if (lbnExpMod_16((BNWORD16 *)dest->ptr,
		                 (BNWORD16 *)n->ptr, nsize,
				 (BNWORD16 *)exp->ptr, esize,
				 (BNWORD16 *)mod->ptr, msize) < 0)
		return -1;
	}

	dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, msize);
	MALLOCDB;
	return 0;
}

/*
 * dest = n1^e1 * n2^e2 (mod mod).  This is more efficient than two
 * separate modular exponentiations, and in fact asymptotically approaches
 * the cost of one.
 */
int
bnDoubleExpMod_16(struct BigNum *dest,
	struct BigNum const *n1, struct BigNum const *e1,
	struct BigNum const *n2, struct BigNum const *e2,
	struct BigNum const *mod)
{
	unsigned n1size, e1size, n2size, e2size, msize;

	n1size = lbnNorm_16((BNWORD16 *)n1->ptr, n1->size);
	e1size = lbnNorm_16((BNWORD16 *)e1->ptr, e1->size);
	n2size = lbnNorm_16((BNWORD16 *)n2->ptr, n2->size);
	e2size = lbnNorm_16((BNWORD16 *)e2->ptr, e2->size);
	msize = lbnNorm_16((BNWORD16 *)mod->ptr, mod->size);

	if (!msize || (((BNWORD16 *)mod->ptr)[BIGLITTLE(-1,0)] & 1) == 0)
		return -1;	/* Illegal modulus! */

	bnSizeCheck(dest, msize);

	if (lbnDoubleExpMod_16((BNWORD16 *)dest->ptr,
		(BNWORD16 *)n1->ptr, n1size, (BNWORD16 *)e1->ptr, e1size,
		(BNWORD16 *)n2->ptr, n2size, (BNWORD16 *)e2->ptr, e2size,
		(BNWORD16 *)mod->ptr, msize) < 0)
		return -1;

	dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, msize);
	MALLOCDB;
	return 0;
}

/* n = 2^exp (mod mod) */
int
bnTwoExpMod_16(struct BigNum *n, struct BigNum const *exp,
	struct BigNum const *mod)
{
	unsigned esize, msize;

	esize = lbnNorm_16((BNWORD16 *)exp->ptr, exp->size);
	msize = lbnNorm_16((BNWORD16 *)mod->ptr, mod->size);

	if (!msize || (((BNWORD16 *)mod->ptr)[BIGLITTLE(-1,0)] & 1) == 0)
		return -1;	/* Illegal modulus! */

	bnSizeCheck(n, msize);

	if (lbnTwoExpMod_16((BNWORD16 *)n->ptr, (BNWORD16 *)exp->ptr, esize,
	                    (BNWORD16 *)mod->ptr, msize) < 0)
		return -1;

	n->size = lbnNorm_16((BNWORD16 *)n->ptr, msize);
	MALLOCDB;
	return 0;
}

/* dest = gcd(a, b) */
int
bnGcd_16(struct BigNum *dest, struct BigNum const *a, struct BigNum const *b)
{
	BNWORD16 *tmp;
	unsigned asize, bsize;
	int i;

	/* Kind of silly, but we might as well permit it... */
	if (a == b)
		return dest == a ? 0 : bnCopy(dest, a);

	/* Ensure a is not the same as "dest" */
	if (a == dest) {
		a = b;
		b = dest;
	}

	asize = lbnNorm_16((BNWORD16 *)a->ptr, a->size);
	bsize = lbnNorm_16((BNWORD16 *)b->ptr, b->size);

	bnSizeCheck(dest, bsize+1);

	/* Copy a to tmp */
	LBNALLOC(tmp, BNWORD16, asize+1);
	if (!tmp)
		return -1;
	lbnCopy_16(tmp, (BNWORD16 *)a->ptr, asize);

	/* Copy b to dest, if necessary */
	if (dest != b)
		lbnCopy_16((BNWORD16 *)dest->ptr,
			   (BNWORD16 *)b->ptr, bsize);
	if (bsize > asize || (bsize == asize &&
	        lbnCmp_16((BNWORD16 *)b->ptr, (BNWORD16 *)a->ptr, asize) > 0))
	{
		i = lbnGcd_16((BNWORD16 *)dest->ptr, bsize, tmp, asize,
			&dest->size);
		if (i > 0)	/* Result in tmp, not dest */
			lbnCopy_16((BNWORD16 *)dest->ptr, tmp, dest->size);
	} else {
		i = lbnGcd_16(tmp, asize, (BNWORD16 *)dest->ptr, bsize,
			&dest->size);
		if (i == 0)	/* Result in tmp, not dest */
			lbnCopy_16((BNWORD16 *)dest->ptr, tmp, dest->size);
	}
	LBNFREE(tmp, asize+1);
	MALLOCDB;
	return (i < 0) ? i : 0;
}

/*
 * dest = 1/src (mod mod).  Returns >0 if gcd(src, mod) != 1 (in which case
 * the inverse does not exist).
 */
int
bnInv_16(struct BigNum *dest, struct BigNum const *src,
         struct BigNum const *mod)
{
	unsigned s, m;
	int i;

	s = lbnNorm_16((BNWORD16 *)src->ptr, src->size);
	m = lbnNorm_16((BNWORD16 *)mod->ptr, mod->size);

	/* lbnInv_16 requires that the input be less than the modulus */
	if (m < s ||
	    (m==s && lbnCmp_16((BNWORD16 *)src->ptr, (BNWORD16 *)mod->ptr, s)))
	{
		bnSizeCheck(dest, s + (m==s));
		if (dest != src)
			lbnCopy_16((BNWORD16 *)dest->ptr,
			           (BNWORD16 *)src->ptr, s);
		/* Pre-reduce modulo the modulus */
		(void)lbnDiv_16((BNWORD16 *)dest->ptr BIGLITTLE(-m,+m),
			        (BNWORD16 *)dest->ptr, s,
		                (BNWORD16 *)mod->ptr, m);
		s = lbnNorm_16((BNWORD16 *)dest->ptr, m);
		MALLOCDB;
	} else {
		bnSizeCheck(dest, m+1);
		if (dest != src)
			lbnCopy_16((BNWORD16 *)dest->ptr,
			           (BNWORD16 *)src->ptr, s);
	}

	i = lbnInv_16((BNWORD16 *)dest->ptr, s, (BNWORD16 *)mod->ptr, m);
	if (i == 0)
		dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, m);

	MALLOCDB;
	return i;
}

/*
 * Shift a bignum left the appropriate number of bits,
 * multiplying by 2^amt.
 */
int
bnLShift_16(struct BigNum *dest, unsigned amt)
{
	unsigned s = dest->size;
	BNWORD16 carry;

	if (amt % 16) {
		carry = lbnLshift_16((BNWORD16 *)dest->ptr, s, amt % 16);
		if (carry) {
			s++;
			bnSizeCheck(dest, s);
			((BNWORD16 *)dest->ptr)[BIGLITTLE(-s,s-1)] = carry;
		}
	}

	amt /= 16;
	if (amt) {
		bnSizeCheck(dest, s+amt);
		memmove((BNWORD16 *)dest->ptr BIGLITTLE(-s-amt, +amt),
		        (BNWORD16 *)dest->ptr BIG(-s),
			s * sizeof(BNWORD16));
		lbnZero_16((BNWORD16 *)dest->ptr, amt);
		s += amt;
	}
	dest->size = s;
	MALLOCDB;
	return 0;
}

/*
 * Shift a bignum right the appropriate number of bits,
 * dividing by 2^amt.
 */
void
bnRShift_16(struct BigNum *dest, unsigned amt)
{
	unsigned s = dest->size;

	if (amt >= 16) {
		memmove(
		        (BNWORD16 *)dest->ptr BIG(-s+amt/16),
			(BNWORD16 *)dest->ptr BIGLITTLE(-s, +amt/16),
			(s-amt/16) * sizeof(BNWORD16));
		s -= amt/16;
		amt %= 16;
	}

	if (amt)
		(void)lbnRshift_16((BNWORD16 *)dest->ptr, s, amt);

	dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, s);
	MALLOCDB;
}

/*
 * Shift a bignum right until it is odd, and return the number of
 * bits shifted.  n = d * 2^s.  Replaces n with d and returns s.
 * Returns 0 when given 0.  (Another valid answer is infinity.)
 */
unsigned
bnMakeOdd_16(struct BigNum *n)
{
	unsigned size;
	unsigned s;	/* shift amount */
	BNWORD16 *p;
	BNWORD16 t;

	p = (BNWORD16 *)n->ptr;
	size = lbnNorm_16(p, n->size);
	if (!size)
		return 0;

	t = BIGLITTLE(p[-1],p[0]);
	s = 0;

	/* See how many words we have to shift */
	if (!t) {
		/* Shift by words */
		do {
			s++;
			BIGLITTLE(--p,p++);
		} while ((t = BIGLITTLE(p[-1],p[0])) == 0);
		size -= s;
		s *= 16;
		memmove((BNWORD16 *)n->ptr BIG(-size), p BIG(-size),
			size * sizeof(BNWORD16));
		p = (BNWORD16 *)n->ptr;
		MALLOCDB;
	}

	assert(t);

	if (!(t & 1)) {
		/* Now count the bits */
		do {
			t >>= 1;
			s++;
		} while ((t & 1) == 0);

		/* Shift the bits */
		lbnRshift_16(p, size, s & (16-1));
		/* Renormalize */
		if (BIGLITTLE(*(p-size),*(p+(size-1))) == 0)
			--size;
	}
	n->size = size;

	MALLOCDB;
	return s;
}

/*
 * Do base- and modulus-dependent precomputation for rapid computation of
 * base^exp (mod mod) with various exponents.
 *
 * See lbn16.c for the details on how the algorithm works.  Basically,
 * it involves precomputing a table of powers of base, base^(order^k),
 * for a suitable range 0 <= k < n detemined by the maximum exponent size
 * desired.  To do eht exponentiation, the exponent is expressed in base
 * "order" (sorry for the confusing terminology) and the precomputed powers
 * are combined.
 * 
 * This implementation allows only power-of-2 values for "order".  Using
 * other numbers can be more efficient, but it's more work and for the
 * popular exponent size of 160 bits, an order of 8 is optimal, so it
 * hasn't seemed worth it to implement.
 * 
 * Here's a table of the optimal power-of-2 order for various exponent
 * sizes and the associated (average) cost for an exponentiation.
 * Note that *higher* orders are more memory-efficient; the number
 * of precomputed values required is ceil(ebits/order).  (Ignore the
 * underscores in the middle of numbers; they're harmless.)
 *
 * At     2 bits, order   2 uses    0.000000 multiplies
 * At     4 bits, order   2 uses    1.000000 multiplies
 * At     8 bits, order   2 uses    3.000000 multiplies
 * At   1_6 bits, order   2 uses    7.000000 multiplies
 * At   3_2 bits, order   2 uses   15.000000 multiplies
 * At    34 bits, 15.750000 (order 4) < 1_6.000000 (order 2)
 * At   6_4 bits, order   4 uses   27.000000 multiplies
 * At    99 bits, 39.875000 (order 8) < 40.250000 (order 4)
 * At   128 bits, order   8 uses   48.500000 multiplies
 * At   256 bits, order   8 uses   85.875000 multiplies
 * At   280 bits, 92.625000 (order 1_6) < 92.875000 (order 8)
 * At   512 bits, order 1_6 uses  147.000000 multiplies
 * At   785 bits, 211.093750 (order 3_2) < 211.250000 (order 1_6)
 * At  1024 bits, order 3_2 uses  257.562500 multiplies
 * At  2048 bits, order 3_2 uses  456.093750 multiplies
 * At  2148 bits, 475.406250 (order 6_4) < 475.468750 (order 3_2)
 * At  4096 bits, order 6_4 uses  795.281250 multiplies
 * At  5726 bits, 1062.609375 (order 128) < 1062.843750 (order 6_4)
 * At  8192 bits, order 128 uses 1412.609375 multiplies
 * At 14848 bits, 2355.750000 (order 256) < 2355.929688 (order 128)
 * At 37593 bits, 5187.841797 (order 512) < 5188.144531 (order 256)
 */
int
bnBasePrecompBegin_16(struct BnBasePrecomp *pre, struct BigNum const *base,
	struct BigNum const *mod, unsigned maxebits)
{
	int i;
	BNWORD16 **array;	/* Array of precomputed powers of base */
	unsigned n;	/* Number of entries in array (needed) */
	unsigned m;	/* Number of entries in array (non-NULL) */
	unsigned arraysize; /* Number of entries in array (allocated) */
	unsigned bits;	/* log2(order) */
	unsigned msize = lbnNorm_16((BNWORD16 *)mod->ptr, mod->size);
	static unsigned const bnBasePrecompThreshTable[] = {
		33, 98, 279, 784, 2147, 5725, 14847, 37592, (unsigned)-1
	};

	/* Clear pre in case of failure */
	pre->array = 0;
	pre->msize = 0;
	pre->bits = 0;
	pre->maxebits = 0;
	pre->arraysize = 0;
	pre->entries = 0;

	/* Find the correct bit-window size */
	bits = 0;
	do
		bits++;
	while (maxebits > bnBasePrecompThreshTable[bits]);

	/* Now the number of precomputed values we need */
	n = (maxebits+bits-1)/bits;
	assert(n*bits >= maxebits);

	arraysize = n+1;	/* Add one trailing NULL for safety */
	array = lbnMemAlloc(arraysize * sizeof(*array));
	if (!array)
		return -1;	/* Out of memory */

	/* Now allocate the entries (precomputed powers of base) */
	for (m = 0; m < n; m++) {
		BNWORD16 *entry;

		LBNALLOC(entry, BNWORD16, msize);
		if (!entry)
			break;
		array[m] = entry;
	}
	
	/* "m" is the number of successfully allocated entries */
	if (m < n) {
		/* Ran out of memory; see if we can use a smaller array */
		BNWORD16 **newarray;

		if (m < 2) {
			n = 0;	/* Forget it */
		} else {
			/* How few bits can we use with what's allocated? */
			bits = (maxebits + m - 1) / m;
retry:
			n = (maxebits + bits - 1) / bits;
			if (! (n >> bits) )
				n = 0; /* Not enough to amount to anything */
		}
		/* Free excess allocated array entries */
		while (m > n) {
			BNWORD16 *entry = array[--m];
			LBNFREE(entry, msize);
		}
		if (!n) {
			/* Give it up */
			lbnMemFree(array, arraysize * sizeof(*array));
			return -1;
		}
		/*
		 * Try to shrink the pointer array.  This might fail, but
		 * it's not critical.  lbnMemRealloc isn't guarnateed to
		 * exist, so we may have to allocate, copy, and free.
		 */
#ifdef lbnMemRealloc
		newarray = lbnMemRealloc(array, arraysize * sizeof(*array),
			       (n+1) * sizeof(*array));
		if (newarray) {
			array = newarray;
			arraysize = n+1;
		}
#else
		newarray = lbnMemAlloc((n+1) * sizeof(*array));
		if (newarray) {
			memcpy(newarray, array, n * sizeof(*array));
			lbnMemFree(array, arraysize * sizeof(*array));
			array = newarray;
			arraysize = n+1;
		}
#endif
	}

	/* Pad with null pointers */
	while (m < arraysize)
		array[m++] = 0;

	/* Okay, we have our array, now initialize it */
	i = lbnBasePrecompBegin_16(array, n, bits,
		(BNWORD16 *)base->ptr, base->size,
		(BNWORD16 *)mod->ptr, msize);
	if (i < 0) {
		/* Ack, still out of memory */
		bits++;
		m = n;
		goto retry;
	}
	/* Finally, totoal success */
	pre->array = array;
	pre->bits = bits;
	pre->msize = msize;
	pre->maxebits = n * bits;
	pre->arraysize = arraysize;
	pre->entries = n;
	return 0;
}

/* Free everything preallocated */
void
bnBasePrecompEnd_16(struct BnBasePrecomp *pre)
{
	BNWORD16 **array = pre->array;

	if (array) {
		unsigned entries = pre->entries;
		unsigned msize = pre->msize;
		unsigned m;

		for (m = 0; m < entries; m++) {
			BNWORD16 *entry = array[m];
			if (entry)
				LBNFREE(entry, msize);
		}
		lbnMemFree(array, pre->arraysize * sizeof(array));
	}
	pre->array = 0;
	pre->bits = 0;
	pre->msize = 0;
	pre->maxebits = 0;
	pre->arraysize = 0;
	pre->entries = 0;
}

int
bnBasePrecompExpMod_16(struct BigNum *dest, struct BnBasePrecomp const *pre,
	struct BigNum const *exp, struct BigNum const *mod)
{
	unsigned msize = lbnNorm_16((BNWORD16 *)mod->ptr, mod->size);
	unsigned esize = lbnNorm_16((BNWORD16 *)exp->ptr, exp->size);
	BNWORD16 const * const *array = pre->array;
	int i;

	assert(msize == pre->msize);
	assert(((BNWORD16 *)mod->ptr)[BIGLITTLE(-1,0)] & 1);
	assert(lbnBits_16((BNWORD16 *)exp->ptr, esize) <= pre->maxebits);

	bnSizeCheck(dest, msize);
	
	i = lbnBasePrecompExp_16(dest->ptr, array, pre->bits,
		       	exp->ptr, esize, mod->ptr, msize);
	if (i == 0)
		dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, msize);
	return i;
}

int
bnDoubleBasePrecompExpMod_16(struct BigNum *dest,
	struct BnBasePrecomp const *pre1, struct BigNum const *exp1,
	struct BnBasePrecomp const *pre2, struct BigNum const *exp2,
	struct BigNum const *mod)
{
	unsigned msize = lbnNorm_16((BNWORD16 *)mod->ptr, mod->size);
	unsigned e1size = lbnNorm_16((BNWORD16 *)exp1->ptr, exp1->size);
	unsigned e2size = lbnNorm_16((BNWORD16 *)exp1->ptr, exp2->size);
	BNWORD16 const * const *array1 = pre1->array;
	BNWORD16 const * const *array2 = pre2->array;
	int i;

	assert(msize == pre1->msize);
	assert(msize == pre2->msize);
	assert(((BNWORD16 *)mod->ptr)[BIGLITTLE(-1,0)] & 1);
	assert(lbnBits_16((BNWORD16 *)exp1->ptr, e1size) <= pre1->maxebits);
	assert(lbnBits_16((BNWORD16 *)exp2->ptr, e2size) <= pre2->maxebits);
	assert(pre1->bits == pre2->bits);

	bnSizeCheck(dest, msize);
	
	i = lbnDoubleBasePrecompExp_16(dest->ptr, pre1->bits, array1,
		       	exp1->ptr, e1size, array2, exp2->ptr, e2size,
			mod->ptr, msize);
	if (i == 0)
		dest->size = lbnNorm_16((BNWORD16 *)dest->ptr, msize);
	return i;
}
