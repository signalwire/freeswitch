#ifndef LBN16_H
#define LBN16_H

#include "lbn.h"

#ifndef BNWORD16
#error 16-bit bignum library requires a 16-bit data type
#endif

#ifndef lbnCopy_16
void lbnCopy_16(BNWORD16 *dest, BNWORD16 const *src, unsigned len);
#endif
#ifndef lbnZero_16
void lbnZero_16(BNWORD16 *num, unsigned len);
#endif
#ifndef lbnNeg_16
void lbnNeg_16(BNWORD16 *num, unsigned len);
#endif

#ifndef lbnAdd1_16
BNWORD16 lbnAdd1_16(BNWORD16 *num, unsigned len, BNWORD16 carry);
#endif
#ifndef lbnSub1_16
BNWORD16 lbnSub1_16(BNWORD16 *num, unsigned len, BNWORD16 borrow);
#endif

#ifndef lbnAddN_16
BNWORD16 lbnAddN_16(BNWORD16 *num1, BNWORD16 const *num2, unsigned len);
#endif
#ifndef lbnSubN_16
BNWORD16 lbnSubN_16(BNWORD16 *num1, BNWORD16 const *num2, unsigned len);
#endif

#ifndef lbnCmp_16
int lbnCmp_16(BNWORD16 const *num1, BNWORD16 const *num2, unsigned len);
#endif

#ifndef lbnMulN1_16
void lbnMulN1_16(BNWORD16 *out, BNWORD16 const *in, unsigned len, BNWORD16 k);
#endif
#ifndef lbnMulAdd1_16
BNWORD16
lbnMulAdd1_16(BNWORD16 *out, BNWORD16 const *in, unsigned len, BNWORD16 k);
#endif
#ifndef lbnMulSub1_16
BNWORD16 lbnMulSub1_16(BNWORD16 *out, BNWORD16 const *in, unsigned len, BNWORD16 k);
#endif

#ifndef lbnLshift_16
BNWORD16 lbnLshift_16(BNWORD16 *num, unsigned len, unsigned shift);
#endif
#ifndef lbnDouble_16
BNWORD16 lbnDouble_16(BNWORD16 *num, unsigned len);
#endif
#ifndef lbnRshift_16
BNWORD16 lbnRshift_16(BNWORD16 *num, unsigned len, unsigned shift);
#endif

#ifndef lbnMul_16
void lbnMul_16(BNWORD16 *prod, BNWORD16 const *num1, unsigned len1,
	BNWORD16 const *num2, unsigned len2);
#endif
#ifndef lbnSquare_16
void lbnSquare_16(BNWORD16 *prod, BNWORD16 const *num, unsigned len);
#endif

#ifndef lbnNorm_16
unsigned lbnNorm_16(BNWORD16 const *num, unsigned len);
#endif
#ifndef lbnBits_16
unsigned lbnBits_16(BNWORD16 const *num, unsigned len);
#endif

#ifndef lbnExtractBigBytes_16
void lbnExtractBigBytes_16(BNWORD16 const *bn, unsigned char *buf,
	unsigned lsbyte, unsigned buflen);
#endif
#ifndef lbnInsertBigytes_16
void lbnInsertBigBytes_16(BNWORD16 *n, unsigned char const *buf,
	unsigned lsbyte,  unsigned buflen);
#endif
#ifndef lbnExtractLittleBytes_16
void lbnExtractLittleBytes_16(BNWORD16 const *bn, unsigned char *buf,
	unsigned lsbyte, unsigned buflen);
#endif
#ifndef lbnInsertLittleBytes_16
void lbnInsertLittleBytes_16(BNWORD16 *n, unsigned char const *buf,
	unsigned lsbyte,  unsigned buflen);
#endif

#ifndef lbnDiv21_16
BNWORD16 lbnDiv21_16(BNWORD16 *q, BNWORD16 nh, BNWORD16 nl, BNWORD16 d);
#endif
#ifndef lbnDiv1_16
BNWORD16 lbnDiv1_16(BNWORD16 *q, BNWORD16 *rem,
	BNWORD16 const *n, unsigned len, BNWORD16 d);
#endif
#ifndef lbnModQ_16
unsigned lbnModQ_16(BNWORD16 const *n, unsigned len, unsigned d);
#endif
#ifndef lbnDiv_16
BNWORD16
lbnDiv_16(BNWORD16 *q, BNWORD16 *n, unsigned nlen, BNWORD16 *d, unsigned dlen);
#endif

#ifndef lbnMontInv1_16
BNWORD16 lbnMontInv1_16(BNWORD16 const x);
#endif
#ifndef lbnMontReduce_16
void lbnMontReduce_16(BNWORD16 *n, BNWORD16 const *mod, unsigned const mlen,
                BNWORD16 inv);
#endif
#ifndef lbnToMont_16
void lbnToMont_16(BNWORD16 *n, unsigned nlen, BNWORD16 *mod, unsigned mlen);
#endif
#ifndef lbnFromMont_16
void lbnFromMont_16(BNWORD16 *n, BNWORD16 *mod, unsigned len);
#endif

#ifndef lbnExpMod_16
int lbnExpMod_16(BNWORD16 *result, BNWORD16 const *n, unsigned nlen,
	BNWORD16 const *exp, unsigned elen, BNWORD16 *mod, unsigned mlen);
#endif
#if 0
#ifndef lbnDoubleExpMod_16
int lbnDoubleExpMod_16(BNWORD16 *result,
	BNWORD16 const *n1, unsigned n1len, BNWORD16 const *e1, unsigned e1len,
	BNWORD16 const *n2, unsigned n2len, BNWORD16 const *e2, unsigned e2len,
	BNWORD16 *mod, unsigned mlen);
#endif
#endif
#ifndef lbnTwoExpMod_16
int lbnTwoExpMod_16(BNWORD16 *n, BNWORD16 const *exp, unsigned elen,
	BNWORD16 *mod, unsigned mlen);
#endif
#if 0
#ifndef lbnGcd_16
int lbnGcd_16(BNWORD16 *a, unsigned alen, BNWORD16 *b, unsigned blen,
	unsigned *rlen);
#endif
#ifndef lbnInv_16
int lbnInv_16(BNWORD16 *a, unsigned alen, BNWORD16 const *mod, unsigned mlen);
#endif

int lbnBasePrecompBegin_16(BNWORD16 **array, unsigned n, unsigned bits,
	BNWORD16 const *g, unsigned glen, BNWORD16 *mod, unsigned mlen);
int lbnBasePrecompExp_16(BNWORD16 *result, BNWORD16 const * const *array,
       unsigned bits, BNWORD16 const *exp, unsigned elen,
       BNWORD16 const *mod, unsigned mlen);
int lbnDoubleBasePrecompExp_16(BNWORD16 *result, unsigned bits,
       BNWORD16 const * const *array1, BNWORD16 const *exp1, unsigned elen1,
       BNWORD16 const * const *array2, BNWORD16 const *exp2,
       unsigned elen2, BNWORD16 const *mod, unsigned mlen);
#endif

#endif /* LBN16_H */
