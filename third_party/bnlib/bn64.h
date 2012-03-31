/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bn64.h - interface to 64-bit bignum routines.
 */
struct BigNum;
struct BnBasePrecomp;

void bnInit_64(void);
void bnEnd_64(struct BigNum *bn);
int bnPrealloc_64(struct BigNum *bn, unsigned bits);
int bnCopy_64(struct BigNum *dest, struct BigNum const *src);
int bnSwap_64(struct BigNum *a, struct BigNum *b);
void bnNorm_64(struct BigNum *bn);
void bnExtractBigBytes_64(struct BigNum const *bn, unsigned char *dest,
	unsigned lsbyte, unsigned dlen);
int bnInsertBigBytes_64(struct BigNum *bn, unsigned char const *src,
	unsigned lsbyte, unsigned len);
void bnExtractLittleBytes_64(struct BigNum const *bn, unsigned char *dest,
	unsigned lsbyte, unsigned dlen);
int bnInsertLittleBytes_64(struct BigNum *bn, unsigned char const *src,
	unsigned lsbyte, unsigned len);
unsigned bnLSWord_64(struct BigNum const *src);
int bnReadBit_64(struct BigNum const *bn, unsigned bit);
unsigned bnBits_64(struct BigNum const *src);
int bnAdd_64(struct BigNum *dest, struct BigNum const *src);
int bnSub_64(struct BigNum *dest, struct BigNum const *src);
int bnCmpQ_64(struct BigNum const *a, unsigned b);
int bnSetQ_64(struct BigNum *dest, unsigned src);
int bnAddQ_64(struct BigNum *dest, unsigned src);
int bnSubQ_64(struct BigNum *dest, unsigned src);
int bnCmp_64(struct BigNum const *a, struct BigNum const *b);
int bnSquare_64(struct BigNum *dest, struct BigNum const *src);
int bnMul_64(struct BigNum *dest, struct BigNum const *a,
	struct BigNum const *b);
int bnMulQ_64(struct BigNum *dest, struct BigNum const *a, unsigned b);
int bnDivMod_64(struct BigNum *q, struct BigNum *r, struct BigNum const *n,
	struct BigNum const *d);
int bnMod_64(struct BigNum *dest, struct BigNum const *src,
	struct BigNum const *d);
unsigned bnModQ_64(struct BigNum const *src, unsigned d);
int bnExpMod_64(struct BigNum *dest, struct BigNum const *n,
	struct BigNum const *exp, struct BigNum const *mod);
int bnDoubleExpMod_64(struct BigNum *dest,
	struct BigNum const *n1, struct BigNum const *e1,
	struct BigNum const *n2, struct BigNum const *e2,
	struct BigNum const *mod);
int bnTwoExpMod_64(struct BigNum *n, struct BigNum const *exp,
	struct BigNum const *mod);
int bnGcd_64(struct BigNum *dest, struct BigNum const *a,
	struct BigNum const *b);
int bnInv_64(struct BigNum *dest, struct BigNum const *src,
	struct BigNum const *mod);
int bnLShift_64(struct BigNum *dest, unsigned amt);
void bnRShift_64(struct BigNum *dest, unsigned amt);
unsigned bnMakeOdd_64(struct BigNum *n);
int bnBasePrecompBegin_64(struct BnBasePrecomp *pre, struct BigNum const *base,
	struct BigNum const *mod, unsigned maxebits);
void bnBasePrecompEnd_64(struct BnBasePrecomp *pre);
int bnBasePrecompExpMod_64(struct BigNum *dest, struct BnBasePrecomp const *pre,
	struct BigNum const *exp, struct BigNum const *mod);
int bnDoubleBasePrecompExpMod_64(struct BigNum *dest,
	struct BnBasePrecomp const *pre1, struct BigNum const *exp1,
	struct BnBasePrecomp const *pre2, struct BigNum const *exp2,
	struct BigNum const *mod);
