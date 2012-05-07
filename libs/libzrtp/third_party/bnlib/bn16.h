/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bn16.h - interface to 16-bit bignum routines.
 */
struct BigNum;
struct BnBasePrecomp;

void bnInit_16(void);
void bnEnd_16(struct BigNum *bn);
int bnPrealloc_16(struct BigNum *bn, unsigned bits);
int bnCopy_16(struct BigNum *dest, struct BigNum const *src);
int bnSwap_16(struct BigNum *a, struct BigNum *b);
void bnNorm_16(struct BigNum *bn);
void bnExtractBigBytes_16(struct BigNum const *bn, unsigned char *dest,
	unsigned lsbyte, unsigned dlen);
int bnInsertBigBytes_16(struct BigNum *bn, unsigned char const *src,
	unsigned lsbyte, unsigned len);
void bnExtractLittleBytes_16(struct BigNum const *bn, unsigned char *dest,
	unsigned lsbyte, unsigned dlen);
int bnInsertLittleBytes_16(struct BigNum *bn, unsigned char const *src,
	unsigned lsbyte, unsigned len);
unsigned bnLSWord_16(struct BigNum const *src);
int bnReadBit_16(struct BigNum const *bn, unsigned bit);
unsigned bnBits_16(struct BigNum const *src);
int bnAdd_16(struct BigNum *dest, struct BigNum const *src);
int bnSub_16(struct BigNum *dest, struct BigNum const *src);
int bnCmpQ_16(struct BigNum const *a, unsigned b);
int bnSetQ_16(struct BigNum *dest, unsigned src);
int bnAddQ_16(struct BigNum *dest, unsigned src);
int bnSubQ_16(struct BigNum *dest, unsigned src);
int bnCmp_16(struct BigNum const *a, struct BigNum const *b);
int bnSquare_16(struct BigNum *dest, struct BigNum const *src);
int bnMul_16(struct BigNum *dest, struct BigNum const *a,
	struct BigNum const *b);
int bnMulQ_16(struct BigNum *dest, struct BigNum const *a, unsigned b);
int bnDivMod_16(struct BigNum *q, struct BigNum *r, struct BigNum const *n,
	struct BigNum const *d);
int bnMod_16(struct BigNum *dest, struct BigNum const *src,
	struct BigNum const *d);
unsigned bnModQ_16(struct BigNum const *src, unsigned d);
int bnExpMod_16(struct BigNum *dest, struct BigNum const *n,
	struct BigNum const *exp, struct BigNum const *mod);
int bnDoubleExpMod_16(struct BigNum *dest,
	struct BigNum const *n1, struct BigNum const *e1,
	struct BigNum const *n2, struct BigNum const *e2,
	struct BigNum const *mod);
int bnTwoExpMod_16(struct BigNum *n, struct BigNum const *exp,
	struct BigNum const *mod);
int bnGcd_16(struct BigNum *dest, struct BigNum const *a,
	struct BigNum const *b);
int bnInv_16(struct BigNum *dest, struct BigNum const *src,
	struct BigNum const *mod);
int bnLShift_16(struct BigNum *dest, unsigned amt);
void bnRShift_16(struct BigNum *dest, unsigned amt);
unsigned bnMakeOdd_16(struct BigNum *n);
int bnBasePrecompBegin_16(struct BnBasePrecomp *pre, struct BigNum const *base,
	struct BigNum const *mod, unsigned maxebits);
void bnBasePrecompEnd_16(struct BnBasePrecomp *pre);
int bnBasePrecompExpMod_16(struct BigNum *dest, struct BnBasePrecomp const *pre,
	struct BigNum const *exp, struct BigNum const *mod);
int bnDoubleBasePrecompExpMod_16(struct BigNum *dest,
	struct BnBasePrecomp const *pre1, struct BigNum const *exp1,
	struct BnBasePrecomp const *pre2, struct BigNum const *exp2,
	struct BigNum const *mod);
