/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef LBN64_H
#define LBN64_H

#include "lbn.h"

#ifndef BNWORD64
#error 64-bit bignum library requires a 64-bit data type
#endif

#ifndef lbnCopy_64
void lbnCopy_64(BNWORD64 *dest, BNWORD64 const *src, unsigned len);
#endif
#ifndef lbnZero_64
void lbnZero_64(BNWORD64 *num, unsigned len);
#endif
#ifndef lbnNeg_64
void lbnNeg_64(BNWORD64 *num, unsigned len);
#endif

#ifndef lbnAdd1_64
BNWORD64 lbnAdd1_64(BNWORD64 *num, unsigned len, BNWORD64 carry);
#endif
#ifndef lbnSub1_64
BNWORD64 lbnSub1_64(BNWORD64 *num, unsigned len, BNWORD64 borrow);
#endif

#ifndef lbnAddN_64
BNWORD64 lbnAddN_64(BNWORD64 *num1, BNWORD64 const *num2, unsigned len);
#endif
#ifndef lbnSubN_64
BNWORD64 lbnSubN_64(BNWORD64 *num1, BNWORD64 const *num2, unsigned len);
#endif

#ifndef lbnCmp_64
int lbnCmp_64(BNWORD64 const *num1, BNWORD64 const *num2, unsigned len);
#endif

#ifndef lbnMulN1_64
void lbnMulN1_64(BNWORD64 *out, BNWORD64 const *in, unsigned len, BNWORD64 k);
#endif
#ifndef lbnMulAdd1_64
BNWORD64
lbnMulAdd1_64(BNWORD64 *out, BNWORD64 const *in, unsigned len, BNWORD64 k);
#endif
#ifndef lbnMulSub1_64
BNWORD64 lbnMulSub1_64(BNWORD64 *out, BNWORD64 const *in, unsigned len, BNWORD64 k);
#endif

#ifndef lbnLshift_64
BNWORD64 lbnLshift_64(BNWORD64 *num, unsigned len, unsigned shift);
#endif
#ifndef lbnDouble_64
BNWORD64 lbnDouble_64(BNWORD64 *num, unsigned len);
#endif
#ifndef lbnRshift_64
BNWORD64 lbnRshift_64(BNWORD64 *num, unsigned len, unsigned shift);
#endif

#ifndef lbnMul_64
void lbnMul_64(BNWORD64 *prod, BNWORD64 const *num1, unsigned len1,
	BNWORD64 const *num2, unsigned len2);
#endif
#ifndef lbnSquare_64
void lbnSquare_64(BNWORD64 *prod, BNWORD64 const *num, unsigned len);
#endif

#ifndef lbnNorm_64
unsigned lbnNorm_64(BNWORD64 const *num, unsigned len);
#endif
#ifndef lbnBits_64
unsigned lbnBits_64(BNWORD64 const *num, unsigned len);
#endif

#ifndef lbnExtractBigBytes_64
void lbnExtractBigBytes_64(BNWORD64 const *bn, unsigned char *buf,
	unsigned lsbyte, unsigned buflen);
#endif
#ifndef lbnInsertBigytes_64
void lbnInsertBigBytes_64(BNWORD64 *n, unsigned char const *buf,
	unsigned lsbyte,  unsigned buflen);
#endif
#ifndef lbnExtractLittleBytes_64
void lbnExtractLittleBytes_64(BNWORD64 const *bn, unsigned char *buf,
	unsigned lsbyte, unsigned buflen);
#endif
#ifndef lbnInsertLittleBytes_64
void lbnInsertLittleBytes_64(BNWORD64 *n, unsigned char const *buf,
	unsigned lsbyte,  unsigned buflen);
#endif

#ifndef lbnDiv21_64
BNWORD64 lbnDiv21_64(BNWORD64 *q, BNWORD64 nh, BNWORD64 nl, BNWORD64 d);
#endif
#ifndef lbnDiv1_64
BNWORD64 lbnDiv1_64(BNWORD64 *q, BNWORD64 *rem,
	BNWORD64 const *n, unsigned len, BNWORD64 d);
#endif
#ifndef lbnModQ_64
unsigned lbnModQ_64(BNWORD64 const *n, unsigned len, unsigned d);
#endif
#ifndef lbnDiv_64
BNWORD64
lbnDiv_64(BNWORD64 *q, BNWORD64 *n, unsigned nlen, BNWORD64 *d, unsigned dlen);
#endif

#ifndef lbnMontInv1_64
BNWORD64 lbnMontInv1_64(BNWORD64 const x);
#endif
#ifndef lbnMontReduce_64
void lbnMontReduce_64(BNWORD64 *n, BNWORD64 const *mod, unsigned const mlen,
                BNWORD64 inv);
#endif
#ifndef lbnToMont_64
void lbnToMont_64(BNWORD64 *n, unsigned nlen, BNWORD64 *mod, unsigned mlen);
#endif
#ifndef lbnFromMont_64
void lbnFromMont_64(BNWORD64 *n, BNWORD64 *mod, unsigned len);
#endif

#ifndef lbnExpMod_64
int lbnExpMod_64(BNWORD64 *result, BNWORD64 const *n, unsigned nlen,
	BNWORD64 const *exp, unsigned elen, BNWORD64 *mod, unsigned mlen);
#endif
#ifndef lbnDoubleExpMod_64
int lbnDoubleExpMod_64(BNWORD64 *result,
	BNWORD64 const *n1, unsigned n1len, BNWORD64 const *e1, unsigned e1len,
	BNWORD64 const *n2, unsigned n2len, BNWORD64 const *e2, unsigned e2len,
	BNWORD64 *mod, unsigned mlen);
#endif
#ifndef lbnTwoExpMod_64
int lbnTwoExpMod_64(BNWORD64 *n, BNWORD64 const *exp, unsigned elen,
	BNWORD64 *mod, unsigned mlen);
#endif
#ifndef lbnGcd_64
int lbnGcd_64(BNWORD64 *a, unsigned alen, BNWORD64 *b, unsigned blen,
	unsigned *rlen);
#endif
#ifndef lbnInv_64
int lbnInv_64(BNWORD64 *a, unsigned alen, BNWORD64 const *mod, unsigned mlen);
#endif

int lbnBasePrecompBegin_64(BNWORD64 **array, unsigned n, unsigned bits,
	BNWORD64 const *g, unsigned glen, BNWORD64 *mod, unsigned mlen);
int lbnBasePrecompExp_64(BNWORD64 *result, BNWORD64 const * const *array,
       unsigned bits, BNWORD64 const *exp, unsigned elen,
       BNWORD64 const *mod, unsigned mlen);
int lbnDoubleBasePrecompExp_64(BNWORD64 *result, unsigned bits,
       BNWORD64 const * const *array1, BNWORD64 const *exp1, unsigned elen1,
       BNWORD64 const * const *array2, BNWORD64 const *exp2,
       unsigned elen2, BNWORD64 const *mod, unsigned mlen);

#endif /* LBN64_H */
