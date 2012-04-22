/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bn32.h - interface to 32-bit bignum routines.
 */
struct BigNum;
struct BnBasePrecomp;

void bnInit_32(void);
void bnEnd_32(struct BigNum *bn);
int bnPrealloc_32(struct BigNum *bn, unsigned bits);
int bnCopy_32(struct BigNum *dest, struct BigNum const *src);
int bnSwap_32(struct BigNum *a, struct BigNum *b);
void bnNorm_32(struct BigNum *bn);
void bnExtractBigBytes_32(struct BigNum const *bn, unsigned char *dest,
	unsigned lsbyte, unsigned dlen);
int bnInsertBigBytes_32(struct BigNum *bn, unsigned char const *src,
	unsigned lsbyte, unsigned len);
void bnExtractLittleBytes_32(struct BigNum const *bn, unsigned char *dest,
	unsigned lsbyte, unsigned dlen);
int bnInsertLittleBytes_32(struct BigNum *bn, unsigned char const *src,
	unsigned lsbyte, unsigned len);
unsigned bnLSWord_32(struct BigNum const *src);
int bnReadBit_32(struct BigNum const *bn, unsigned bit);
unsigned bnBits_32(struct BigNum const *src);
int bnAdd_32(struct BigNum *dest, struct BigNum const *src);
int bnSub_32(struct BigNum *dest, struct BigNum const *src);
int bnCmpQ_32(struct BigNum const *a, unsigned b);
int bnSetQ_32(struct BigNum *dest, unsigned src);
int bnAddQ_32(struct BigNum *dest, unsigned src);
int bnSubQ_32(struct BigNum *dest, unsigned src);
int bnCmp_32(struct BigNum const *a, struct BigNum const *b);
int bnSquare_32(struct BigNum *dest, struct BigNum const *src);
int bnMul_32(struct BigNum *dest, struct BigNum const *a,
	struct BigNum const *b);
int bnMulQ_32(struct BigNum *dest, struct BigNum const *a, unsigned b);
int bnDivMod_32(struct BigNum *q, struct BigNum *r, struct BigNum const *n,
	struct BigNum const *d);
int bnMod_32(struct BigNum *dest, struct BigNum const *src,
	struct BigNum const *d);
unsigned bnModQ_32(struct BigNum const *src, unsigned d);
int bnExpMod_32(struct BigNum *dest, struct BigNum const *n,
	struct BigNum const *exp, struct BigNum const *mod);
int bnDoubleExpMod_32(struct BigNum *dest,
	struct BigNum const *n1, struct BigNum const *e1,
	struct BigNum const *n2, struct BigNum const *e2,
	struct BigNum const *mod);
int bnTwoExpMod_32(struct BigNum *n, struct BigNum const *exp,
	struct BigNum const *mod);
int bnGcd_32(struct BigNum *dest, struct BigNum const *a,
	struct BigNum const *b);
int bnInv_32(struct BigNum *dest, struct BigNum const *src,
	struct BigNum const *mod);
int bnLShift_32(struct BigNum *dest, unsigned amt);
void bnRShift_32(struct BigNum *dest, unsigned amt);
unsigned bnMakeOdd_32(struct BigNum *n);
int bnBasePrecompBegin_32(struct BnBasePrecomp *pre, struct BigNum const *base,
	struct BigNum const *mod, unsigned maxebits);
void bnBasePrecompEnd_32(struct BnBasePrecomp *pre);
int bnBasePrecompExpMod_32(struct BigNum *dest, struct BnBasePrecomp const *pre,
	struct BigNum const *exp, struct BigNum const *mod);
int bnDoubleBasePrecompExpMod_32(struct BigNum *dest,
	struct BnBasePrecomp const *pre1, struct BigNum const *exp1,
	struct BnBasePrecomp const *pre2, struct BigNum const *exp2,
	struct BigNum const *mod);
