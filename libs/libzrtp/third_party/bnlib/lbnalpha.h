/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbnalpha.h - header file that declares the Alpha assembly-language
 * subroutines.  It is intended to be included via the BNINCLUDE
 * mechanism.
 */

#define BN_LITTLE_ENDIAN 1

typedef unsigned long bnword64;
#define BNWORD64 bnword64

#ifdef __cplusplus
/* These assembly-language primitives use C names */
extern "C" {
#endif

void lbnMulN1_64(bnword64 *out, bnword64 const *in, unsigned len, bnword64 k);
#define lbnMulN1_64 lbnMulN1_64

bnword64
lbnMulAdd1_64(bnword64 *out, bnword64 const *in, unsigned len, bnword64 k);
#define lbnMulAdd1_64 lbnMulAdd1_64

bnword64
lbnMulSub1_64(bnword64 *out, bnword64 const *in, unsigned len, bnword64 k);
#define lbnMulSub1_64 lbnMulSub1_64

#ifdef __cplusplus
}
#endif
