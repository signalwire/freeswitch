/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbn8086.h - This file defines the interfaces to the 8086
 * assembly primitives for 16-bit MS-DOS environments.
 * It is intended to be included in "lbn.h"
 * via the "#include BNINCLUDE" mechanism.
 */
 
#define BN_LITTLE_ENDIAN 1

#ifdef __cplusplus
/* These assembly-language primitives use C names */
extern "C" {
#endif

/* Set up the appropriate types */
typedef unsigned short bnword16;
#define BNWORD16 bnword16
typedef unsigned long bnword32;
#define BNWORD32 bnword32

void __cdecl __far
lbnMulN1_16(bnword16 __far *out, bnword16 const __far *in,
            unsigned len, bnword16 k);
#define lbnMulN1_16 lbnMulN1_16
            
bnword16 __cdecl __far
lbnMulAdd1_16(bnword16 __far *out, bnword16 const __far *in,
              unsigned len, bnword16 k);
#define lbnMulAdd1_16 lbnMulAdd1_16
       
bnword16 __cdecl __far
lbnMulSub1_16(bnword16 __far *out, bnword16 const __far *in,
              unsigned len, bnword16 k);
#define lbnMulSub1_16 lbnMulSub1_16

bnword16 __cdecl __far
lbnDiv21_16(bnword16 __far *q, bnword16 nh, bnword16 nl, bnword16 d);
#define lbnDiv21_16 lbnDiv21_16

bnword16 __cdecl __far
lbnModQ_16(bnword16 const __far *n, unsigned len, bnword16 d);
#define lbnModQ_16 lbnModQ_16



void __cdecl __far
lbnMulN1_32(bnword32 __far *out, bnword32 const __far *in,
            unsigned len, bnword32 k);
#define lbnMulN1_32 lbnMulN1_32
            
bnword32 __cdecl __far
lbnMulAdd1_32(bnword32 __far *out, bnword32 const __far *in,
              unsigned len, bnword32 k);
#define lbnMulAdd1_32 lbnMulAdd1_32
       
bnword32 __cdecl __far
lbnMulSub1_32(bnword32 __far *out, bnword32 const __far *in,
              unsigned len, bnword32 k);
#define lbnMulSub1_32 lbnMulSub1_32

bnword32 __cdecl __far
lbnDiv21_32(bnword32 __far *q, bnword32 nh, bnword32 nl, bnword32 d);
#define lbnDiv21_32 lbnDiv21_32

bnword16 __cdecl __far
lbnModQ_32(bnword32 const __far *n, unsigned len, bnword32 d);
#define lbnModQ_32 lbnModQ_32

int __cdecl __far not386(void);

#ifdef __cplusplus
}
#endif
