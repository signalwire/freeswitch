/*
 * lbnarm.h - This file defines the interfaces to the ARM
 * assembly primitives.  It is intended to be included in "lbn.h"
 * via the "#include BNINCLUDE" mechanism.
 */
 
#define BN_LITTLE_ENDIAN 1

typedef unsigned bnword32;
#define BNWORD32 bnword32

/* Function prototypes for the asm routines */
void
lbnMulN1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
#define lbnMulN1_32 lbnMulN1_32

bnword32
lbnMulAdd1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
#define lbnMulAdd1_32 lbnMulAdd1_32

/* Not implemented yet */
bnword32
lbnMulSub1_32(bnword32 *out, bnword32 const *in, unsigned len, bnword32 k);
#define lbnMulSub1_32 lbnMulSub1_32

#if __GNUC__ && 0
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
 * "r" means that the operand may be in a register.
 * "=" means that the operand is assigned to.
 * "%" means that this operand and the following one may be
 *     interchanged if desirable.
 * "&" means that this output operand is written before the input operands
 *     are read, so it may NOT overlap with any input operands.
 * "0" and "1" mean that this operand may be in the same place as the
 *     given operand.
 * Multiple sets of constraints may be listed, separated by commas.
 *
 * Note that ARM multi-precision multiply syntax lists destLo before destHi.
 * Also, the first source (%2) may not be the same as %0 or %1.
 * The second source, however, may be.
 */

/* (ph<<32) + pl = x*y */
#define mul32_ppmm(ph,pl,x,y)	\
	__asm__("umull	%1,%0,%2,%3" : "=&r,&r"(ph), "=&r,&r"(pl) \
				     : "%r,%r"(x), "r0,r1"(y))

/* (ph<<32) + pl = x*y + a */
#define mul32_ppmma(ph,pl,x,y,a)	\
	__asm__("umlal	%1,%0,%2,%3" : "=&r"(ph), "=&r"(pl) \
				     : "%r"(x), "r"(y), "0"(0), "1"(a))

/* (ph<<32) + pl = x*y + a + b */
/* %4 (a) may share a register with %0, but nothing else may. */
#define mul32_ppmmaa(ph,pl,x,y,a,b)	\
	__asm__("adds	%1, %4, %5\n\t"	\
		"movcc	%0, #0\n\t"	\
		"movcs	%0, #1\n\t"	\
		"umlal	%1,%0,%2,%3"	\
		: "=&r"(ph), "=&r"(pl)	\
		: "%r"(x), "r"(y), "%r"(a), "r1"(b))

#endif /* __GNUC__ */
