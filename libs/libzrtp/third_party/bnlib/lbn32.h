/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef LBN32_H
#define LBN32_H

#include "lbn.h"

#ifndef BNWORD32
#error 32-bit bignum library requires a 32-bit data type
#endif

#ifndef lbnCopy_32
void lbnCopy_32(BNWORD32 *dest, BNWORD32 const *src, unsigned len);
#endif
#ifndef lbnZero_32
void lbnZero_32(BNWORD32 *num, unsigned len);
#endif
#ifndef lbnNeg_32
void lbnNeg_32(BNWORD32 *num, unsigned len);
#endif

#ifndef lbnAdd1_32
BNWORD32 lbnAdd1_32(BNWORD32 *num, unsigned len, BNWORD32 carry);
#endif
#ifndef lbnSub1_32
BNWORD32 lbnSub1_32(BNWORD32 *num, unsigned len, BNWORD32 borrow);
#endif

#ifndef lbnAddN_32
BNWORD32 lbnAddN_32(BNWORD32 *num1, BNWORD32 const *num2, unsigned len);
#endif
#ifndef lbnSubN_32
BNWORD32 lbnSubN_32(BNWORD32 *num1, BNWORD32 const *num2, unsigned len);
#endif

#ifndef lbnCmp_32
int lbnCmp_32(BNWORD32 const *num1, BNWORD32 const *num2, unsigned len);
#endif

#ifndef lbnMulN1_32
void lbnMulN1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k);
#endif
#ifndef lbnMulAdd1_32
BNWORD32
lbnMulAdd1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k);
#endif
#ifndef lbnMulSub1_32
BNWORD32 lbnMulSub1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k);
#endif

#ifndef lbnLshift_32
BNWORD32 lbnLshift_32(BNWORD32 *num, unsigned len, unsigned shift);
#endif
#ifndef lbnDouble_32
BNWORD32 lbnDouble_32(BNWORD32 *num, unsigned len);
#endif
#ifndef lbnRshift_32
BNWORD32 lbnRshift_32(BNWORD32 *num, unsigned len, unsigned shift);
#endif

#ifndef lbnMul_32
void lbnMul_32(BNWORD32 *prod, BNWORD32 const *num1, unsigned len1,
	BNWORD32 const *num2, unsigned len2);
#endif
#ifndef lbnSquare_32
void lbnSquare_32(BNWORD32 *prod, BNWORD32 const *num, unsigned len);
#endif

#ifndef lbnNorm_32
unsigned lbnNorm_32(BNWORD32 const *num, unsigned len);
#endif
#ifndef lbnBits_32
unsigned lbnBits_32(BNWORD32 const *num, unsigned len);
#endif

#ifndef lbnExtractBigBytes_32
void lbnExtractBigBytes_32(BNWORD32 const *bn, unsigned char *buf,
	unsigned lsbyte, unsigned buflen);
#endif
#ifndef lbnInsertBigytes_32
void lbnInsertBigBytes_32(BNWORD32 *n, unsigned char const *buf,
	unsigned lsbyte,  unsigned buflen);
#endif
#ifndef lbnExtractLittleBytes_32
void lbnExtractLittleBytes_32(BNWORD32 const *bn, unsigned char *buf,
	unsigned lsbyte, unsigned buflen);
#endif
#ifndef lbnInsertLittleBytes_32
void lbnInsertLittleBytes_32(BNWORD32 *n, unsigned char const *buf,
	unsigned lsbyte,  unsigned buflen);
#endif

#ifndef lbnDiv21_32
BNWORD32 lbnDiv21_32(BNWORD32 *q, BNWORD32 nh, BNWORD32 nl, BNWORD32 d);
#endif
#ifndef lbnDiv1_32
BNWORD32 lbnDiv1_32(BNWORD32 *q, BNWORD32 *rem,
	BNWORD32 const *n, unsigned len, BNWORD32 d);
#endif
#ifndef lbnModQ_32
unsigned lbnModQ_32(BNWORD32 const *n, unsigned len, unsigned d);
#endif
#ifndef lbnDiv_32
BNWORD32
lbnDiv_32(BNWORD32 *q, BNWORD32 *n, unsigned nlen, BNWORD32 *d, unsigned dlen);
#endif

#ifndef lbnMontInv1_32
BNWORD32 lbnMontInv1_32(BNWORD32 const x);
#endif
#ifndef lbnMontReduce_32
void lbnMontReduce_32(BNWORD32 *n, BNWORD32 const *mod, unsigned const mlen,
                BNWORD32 inv);
#endif
#ifndef lbnToMont_32
void lbnToMont_32(BNWORD32 *n, unsigned nlen, BNWORD32 *mod, unsigned mlen);
#endif
#ifndef lbnFromMont_32
void lbnFromMont_32(BNWORD32 *n, BNWORD32 *mod, unsigned len);
#endif

#ifndef lbnExpMod_32
int lbnExpMod_32(BNWORD32 *result, BNWORD32 const *n, unsigned nlen,
	BNWORD32 const *exp, unsigned elen, BNWORD32 *mod, unsigned mlen);
#endif
#ifndef lbnDoubleExpMod_32
int lbnDoubleExpMod_32(BNWORD32 *result,
	BNWORD32 const *n1, unsigned n1len, BNWORD32 const *e1, unsigned e1len,
	BNWORD32 const *n2, unsigned n2len, BNWORD32 const *e2, unsigned e2len,
	BNWORD32 *mod, unsigned mlen);
#endif
#ifndef lbnTwoExpMod_32
int lbnTwoExpMod_32(BNWORD32 *n, BNWORD32 const *exp, unsigned elen,
	BNWORD32 *mod, unsigned mlen);
#endif
#ifndef lbnGcd_32
int lbnGcd_32(BNWORD32 *a, unsigned alen, BNWORD32 *b, unsigned blen,
	unsigned *rlen);
#endif
#ifndef lbnInv_32
int lbnInv_32(BNWORD32 *a, unsigned alen, BNWORD32 const *mod, unsigned mlen);
#endif

int lbnBasePrecompBegin_32(BNWORD32 **array, unsigned n, unsigned bits,
	BNWORD32 const *g, unsigned glen, BNWORD32 *mod, unsigned mlen);
int lbnBasePrecompExp_32(BNWORD32 *result, BNWORD32 const * const *array,
       unsigned bits, BNWORD32 const *exp, unsigned elen,
       BNWORD32 const *mod, unsigned mlen);
int lbnDoubleBasePrecompExp_32(BNWORD32 *result, unsigned bits,
       BNWORD32 const * const *array1, BNWORD32 const *exp1, unsigned elen1,
       BNWORD32 const * const *array2, BNWORD32 const *exp2,
       unsigned elen2, BNWORD32 const *mod, unsigned mlen);

#endif /* LBN32_H */
