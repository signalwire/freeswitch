/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbn68000.h - 16-bit bignum primitives for the 68000 (or 68010) processors.
 *
 * These primitives use little-endian word order.
 * (The order of bytes within words is irrelevant.)
 */
#define BN_LITTLE_ENDIAN 1

typedef unsigned short bnword16
#define BNWORD16 bnword16

bnword16 lbnSub1_16(bnword16 *num, unsigned len, bnword16 borrow);
bnword16 lbnAdd1_16(bnword16 *num, unsigned len, bnword16 carry);
void lbnMulN1_16(bnword16 *out, bnword16 const *in, unsigned len, bnword16 k);
bnword16
lbnMulAdd1_16(bnword16 *out, bnword16 const *in, unsigned len, bnword16 k);
bnword16
lbnMulSub1_16(bnword16 *out, bnword16 const *in, unsigned len, bnword16 k);
bnword16 lbnDiv21_16(bnword16 *q, bnword16 nh, bnword16 nl, bnword16 d);
unsigned lbnModQ_16(bnword16 const *n, unsigned len, bnword16 d);

int is68020(void);

/* #define the values to exclude the C versions */
#define lbnSub1_16 lbnSub1_16
#define lbnAdd1_16 lbnAdd1_16
#define lbnMulN1_16 lbnMulN1_16
#define lbnMulAdd1_16 lbnMulAdd1_16
#define lbnMulSub1_16 lbnMulSub1_16
#define lbnDiv21_16 lbnDiv21_16
#define lbnModQ_16 lbnModQ_16

/* Also include the 68020 definitions for 16/32 bit switching versions. */
#include <lbn68020.h>
