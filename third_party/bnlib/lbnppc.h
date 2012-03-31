/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef LBNPPC_H
#define LBNPPC_H

/*
 * Assembly-language routines for the Power PC processor.
 * Annoyingly, the Power PC does not have 64/32->32 bit divide,
 * so the C code should be reasonably fast.  But it does have
 * 32x32->64-bit multiplies, and these routines provide access
 * to that.
 *
 * In versions of CodeWarrior before 8.0, there was no PPC assembler,
 * so a kludged-up one in CPP is used.  This requires casting an
 * array of unsigneds to function pointer type, and a function pointer
 * is not a pointer to the code, but rather a pointer to a (code,TOC)
 * pointer pair which we fake up.
 *
 * CodeWarrior 8.0 supports PCC assembly, which is used directly.
 */

/*
 * Bignums are stored in arrays of 32-bit words, and the least
 * significant 32-bit word has the lowest address, thus "little-endian".
 * The C code is slightly more efficient this way, so unless the
 * processor cares (the PowerPC, like most RISCs, doesn't), it is
 * best to use BN_LITTLE_ENDIAN.
 * Note that this has NOTHING to do with the order of bytes within a 32-bit
 * word; the math library is insensitive to that.
 */
#define BN_LITTLE_ENDIAN 1

typedef unsigned bnword32;
#define BNWORD32 bnword32

#if __MWERKS__ < 0x800

/* Shared transition vector array */
extern unsigned const * const lbnPPC_tv[];

/* A function pointer on the PowerPC is a pointer to a transition vector */
#define lbnMulN1_32 \
((void (*)(bnword32 *, bnword32 const *, unsigned, bnword32))(lbnPPC_tv+0))
#define lbnMulAdd1_32 \
((bnword32 (*)(bnword32 *, bnword32 const *, unsigned, bnword32))(lbnPPC_tv+1))
#define lbnMulSub1_32 \
((bnword32 (*)(bnword32 *, bnword32 const *, unsigned, bnword32))(lbnPPC_tv+2))

#else /* __MWERKS__ >= 0x800 */

void lbnMulN1_32(bnword32 *, bnword32 const *, unsigned, bnword32);
#define lbnMulN1_32 lbnMulN1_32
bnword32 lbnMulAdd1_32(bnword32 *, bnword32 const *, unsigned, bnword32);
#define lbnMulAdd1_32 lbnMulAdd1_32
bnword32 lbnMulSub1_32(bnword32 *, bnword32 const *, unsigned, bnword32);
#define lbnMulSub1_32 lbnMulSub1_32

#endif /* __MWERKS__ >= 0x800 */

#endif /* LBNPPC_H */
