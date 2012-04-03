/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbn960jx.h - This file defines the interfaces to assembly primitives
 * for the the Intel i960Jx series of processors.  In fact, these thould
 * work on any i960 series processor, but haven't been tested.
 * It is intended to be included in "lbn.h"
 * via the "#include BNINCLUDE" mechanism.
 */
 
#define BN_LITTLE_ENDIAN 1

typedef unsigned long bnword32;
#define BNWORD32 bnword32;


#ifdef __cplusplus
/* These assembly-language primitives use C names */
extern "C" {
#endif

/* Function prototypes for the asm routines */
void
lbnMulN1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
#define lbnMulN1_32 lbnMulN1_32
            
bnword32
lbnMulAdd1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
#define lbnMulAdd1_32 lbnMulAdd1_32
       
bnword32
lbnMulSub1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
#define lbnMulSub1_32 lbnMulSub1_32

#ifdef __cplusplus
}
#endif
