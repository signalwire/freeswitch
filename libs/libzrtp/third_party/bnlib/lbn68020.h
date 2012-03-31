/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbn68020.h - 32-bit bignum primitives for the 68020 (or 683xx) processors.
 *
 * These primitives use little-endian word order.
 * (The order of bytes within words is irrelevant.)
 */
#define BN_LITTLE_ENDIAN 1

typedef unsigned long bnword32;
#define BNWORD32 bnword32

bnword32 lbnSub1_32(bnword32 *num, unsigned len, bnword32 borrow);
bnword32 lbnAdd1_32(bnword32 *num, unsigned len, bnword32 carry);
void lbnMulN1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
bnword32
lbnMulAdd1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
bnword32
lbnMulSub1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
bnword32 lbnDiv21_32(bnword32 *q, bnword32 nh, bnword32 nl, bnword32 d);
unsigned lbnModQ_32(bnword32 const *n, unsigned len, unsigned d);

/* #define the values to exclude the C versions */
#define lbnSub1_32 lbnSub1_32
#define lbnAdd1_32 lbnAdd1_32
#define lbnMulN1_32 lbnMulN1_32
#define lbnMulAdd1_32 lbnMulAdd1_32
#define lbnMulSub1_32 lbnMulSub1_32
#define lbnDiv21_32 lbnDiv21_32
#define lbnModQ_32 lbnModQ_32
