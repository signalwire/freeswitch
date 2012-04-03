/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbn80386.h - This file defines the interfaces to the 80386
 * assembly primitives.  It is intended to be included in "lbn.h"
 * via the "#include BNINCLUDE" mechanism.
 */
 
#define BN_LITTLE_ENDIAN 1

typedef unsigned long bnword32;
#define BNWORD32 bnword32

/* MS-DOS needs the calling convention described to it. */
#ifndef MSDOS
#ifdef __MSDOS
#define MSDOS 1
#endif
#endif
#ifndef MSDOS
#ifdef __MSDOS__
#define MSDOS 1
#endif
#endif

/* By MS-DOS, we mean 16-bit brain-dead MS-DOS.  Not 32-bit good things. */
#ifdef __GO32
#undef MSDOS
#endif
#ifdef __GO32__
#undef MSDOS
#endif

#ifdef MSDOS
#define CDECL __cdecl
#else
#define CDECL /*nothing*/
#endif

#ifdef __cplusplus
/* These assembly-language primitives use C names */
extern "C" {
#endif

/* Function prototypes for the asm routines */
void CDECL
lbnMulN1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
#define lbnMulN1_32 lbnMulN1_32
            
bnword32 CDECL
lbnMulAdd1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
#define lbnMulAdd1_32 lbnMulAdd1_32
       
bnword32 CDECL
lbnMulSub1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
#define lbnMulSub1_32 lbnMulSub1_32

bnword32 CDECL
lbnDiv21_32(bnword32 *q, bnword32 nh, bnword32 nl, bnword32 d);
#define lbnDiv21_32 lbnDiv21_32

unsigned CDECL
lbnModQ_32(bnword32 const *n, unsigned len, bnword32 d);
#define lbnModQ_32 lbnModQ_32

#ifdef __cplusplus
}
#endif


#if __GNUC__
/*
 * Use the (massively cool) GNU inline-assembler extension to define
 * inline expansions for various operations.
 *
 * The massively cool part is that the assembler can have inputs
 * and outputs, and you specify the operands and which effective
 * addresses are legal and they get substituted into the code.
 * (For example, some of the code requires a zero.  Rather than
 * specify an immediate constant, the expansion specifies an operand
 * of zero which can be in various places.  This lets GCC use an
 * immediate zero, or a register which contains zero if it's available.)
 *
 * The syntax is asm("asm_code" : outputs : inputs : trashed)
 * %0, %1 and so on in the asm code are substituted by the operands
 * in left-to-right order (outputs, then inputs).
 * The operands contain constraint strings and values to use.
 * Outputs must be lvalues, inputs may be rvalues.  In the constraints:
 * "a" means that the operand must be in eax.
 * "d" means that the operand must be in edx.
 * "g" means that the operand may be any effective address.
 * "=" means that the operand is assigned to.
 * "%" means that this operand and the following one may be
 *     interchanged if desirable.
 * "bcDSmn" means that the operand must be in ebx, ecx, esi, edi, memory,
 *          or an immediate constant.  (This is almost the same as "g"
 *          but allowing it in eax wouldn't help because x is already
 *          assigned there, and it must not be in edx, since edx is
 *          overwritten by the multiply before a and b are read.)
 *
 * Note that GCC uses AT&T assembler syntax, which is rather
 * different from Intel syntax.  The length (b, w or l) of the
 * operation is appended to the opcode, and the *second* operand
 * is the destination, not the first.  Finally, the register names
 * are all preceded with "%".  (Doubled here because % is a
 * magic character.)
 */

/* (ph<<32) + pl = x*y */
#define mul32_ppmm(ph,pl,x,y)	\
	__asm__("mull %3" : "=d"(ph), "=a"(pl) : "%a"(x), "g"(y))

/* (ph<<32) + pl = x*y + a */
#define mul32_ppmma(ph,pl,x,y,a)	\
	__asm__("mull %3\n\t"		\
	        "addl %4,%%eax\n\t"	\
	        "adcl %5,%%edx"		\
	        : "=&d"(ph), "=a"(pl)	\
	        : "%a"(x), "g"(y), "bcDSmn"(a), "bcDSmn"(0))

/* (ph<<32) + pl = x*y + a + b */
#define mul32_ppmmaa(ph,pl,x,y,a,b)	\
	__asm__("mull %3\n\t"		\
	        "addl %4,%%eax\n\t"	\
	        "adcl %6,%%edx\n\t"	\
	        "addl %5,%%eax\n\t"	\
	        "adcl %6,%%edx"		\
	        : "=&d"(ph), "=a"(pl)	\
	        : "%a"(x), "g"(y), "%bcDSmn"(a), "bcDSmn"(b), "bcDSmn"(0))

/* q = ((nh<<32) + nl) / d, return remainder.  nh guaranteed < d. */
#undef lbnDiv21_32
#define lbnDiv21_32(q,nh,nl,d)	\
	({unsigned _;	\
	  __asm__("divl %4" : "=d"(_), "=a"(*q)	: "d"(nh), "a"(nl), "g"(d)); \
	  _;})

/* No quotient, just return remainder ((nh<<32) + nl) % d */
#define lbnMod21_32(nh,nl,d)	\
	({unsigned _;	\
	  __asm__("divl %3" : "=d"(_) : "d"(nh), "a"(nl), "g"(d) : "ax"); \
	  _;})

#endif /* __GNUC__ */
