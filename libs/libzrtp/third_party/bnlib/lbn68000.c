/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbn68000.c - 16-bit bignum primitives for the 68000 (or 68010) processors.
 *
 * This was written for Metrowerks C, and while it should be reasonably
 * portable, NOTE that Metrowerks lets a callee trash a0, a1, d0, d1, and d2.
 * Some 680x0 compilers make d2 callee-save, so instructions to save it
 * will have to be added.
 * 
 * This code supports 16 or 32-bit ints, based on UINT_MAX.
 * Regardless of UINT_MAX, only bignums up to 64K words (1 million bits)
 * are supported.  (68k hackers will recognize this as a consequence of
 * using dbra.)
 *
 * These primitives use little-endian word order.
 * (The order of bytes within words is irrelevant to this issue.)
 */

#include <limits.h>

#include "lbn.h"        /* Should include lbn68000.h */

/*
 * The Metrowerks C compiler (1.2.2) produces bad 68k code for the
 * following input, which happens to be the inner loop of lbnSub1,
 * so a few less than critical routines have been recoded in assembly
 * to avoid the bug.  (Optimizer on or off does not matter.)
 * 
 * unsigned
 * decrement(unsigned *num, unsigned len)
 * {
 *      do {
 *              if ((*num++)-- != 0)
 *                      return 0;
 *      } while (--len);
 *      return 1;
 * }
 */
asm BNWORD16
lbnSub1_16(BNWORD16 *num, unsigned len, BNWORD16 borrow)
{
        movea.l 4(sp),a0        /* num */
#if UINT_MAX == 0xffff
        move.w  10(sp),d0       /* borrow */
#else
        move.w  12(sp),d0       /* borrow */
#endif
        sub.w   d0,(a0)+
        bcc             done
#if UINT_MAX == 0xffff
        move.w  8(sp),d0        /* len */
#else
        move.w  10(sp),d0       /* len */
#endif
        subq.w  #2,d0
        bcs             done
loop:
        subq.w  #1,(a0)+
        dbcc    d0,loop
done:
        moveq.l #0,d0
        addx.w  d0,d0
        rts
}

asm BNWORD16
lbnAdd1_16(BNWORD16 *num, unsigned len, BNWORD16 carry)
{
        movea.l 4(sp),a0        /* num */
#if UINT_MAX == 0xffff
        move.w  10(sp),d0       /* carry */
#else
        move.w  12(sp),d0       /* carry */
#endif
        add.w   d0,(a0)+
        bcc             done
#if UINT_MAX == 0xffff
        move.w  8(sp),d0        /* len */
#else
        move.w  10(sp),d0       /* len */
#endif
        subq.w  #2,d0
        bcs             done
loop:
        addq.w  #1,(a0)+
        dbcc    d0,loop
done:
        moveq.l #0,d0
        addx.w  d0,d0
        rts
}

asm void
lbnMulN1_16(BNWORD16 *out, BNWORD16 const *in, unsigned len, BNWORD16 k)
{
        move.w  d3,-(sp)        /* 2 bytes of stack frame */
        move.l  2+4(sp),a1      /* out */
        move.l  2+8(sp),a0      /* in */
#if UINT_MAX == 0xffff
        move.w  2+12(sp),d3     /* len */
        move.w  2+14(sp),d2     /* k */
#else
        move.w  2+14(sp),d3     /* len (low 16 bits) */
        move.w  2+16(sp),d2     /* k */
#endif

        move.w  (a0)+,d1        /* First multiply */
        mulu.w  d2,d1
        move.w  d1,(a1)+
        clr.w   d1
        swap    d1

        subq.w  #1,d3           /* Setup for loop unrolling */
        lsr.w   #1,d3
        bcs.s   m16_even
        beq.s   m16_short
        
        subq.w  #1,d3           /* Set up software pipeline properly */
        move.l  d1,d0
        
m16_loop:
        move.w  (a0)+,d1
        mulu.w  d2,d1
        add.l   d0,d1
        move.w  d1,(a1)+
        clr.w	d1
        swap	d1
m16_even:

        move.w  (a0)+,d0
        mulu.w  d2,d0
        add.l   d1,d0
        move.w  d0,(a1)+
        clr.w   d0
        swap    d0

        dbra    d3,m16_loop
        
        move.w  d0,(a1)
        move.w  (sp)+,d3
        rts
m16_short:
        move.w  d1,(a1)
        move.w  (sp)+,d3
        rts
}


asm BNWORD16
lbnMulAdd1_16(BNWORD16 *out, BNWORD16 const *in, unsigned len, BNWORD16 k)
{
        move.w  d4,-(sp) 
        clr.w   d4
        move.w  d3,-(sp)        /* 4 bytes of stack frame */
        move.l  4+4(sp),a1      /* out */
        move.l  4+8(sp),a0      /* in */
#if UINT_MAX == 0xffff
        move.w  4+12(sp),d3     /* len */
        move.w  4+14(sp),d2     /* k */
#else
        move.w  4+14(sp),d3     /* len (low 16 bits) */
        move.w  4+16(sp),d2     /* k */
#endif

        move.w  (a0)+,d1        /* First multiply */
        mulu.w  d2,d1
        add.w   d1,(a1)+
        clr.w   d1
        swap    d1
        addx.w  d4,d1

        subq.w  #1,d3           /* Setup for loop unrolling */
        lsr.w   #1,d3
        bcs.s   ma16_even
        beq.s   ma16_short
        
        subq.w  #1,d3           /* Set up software pipeline properly */
        move.l  d1,d0
        
ma16_loop:
        move.w  (a0)+,d1
        mulu.w  d2,d1
        add.l   d0,d1
        add.w   d1,(a1)+
        clr.w   d1
        swap    d1
        addx.w  d4,d1
ma16_even:

        move.w  (a0)+,d0
        mulu.w  d2,d0
        add.l   d1,d0
        add.w   d0,(a1)+
        clr.w   d0
        swap    d0
        addx.w  d4,d0

        dbra    d3,ma16_loop
        
        move.w  (sp)+,d3
        move.w  (sp)+,d4
        rts
ma16_short:
        move.w  (sp)+,d3
        move.l  d1,d0   
        move.w  (sp)+,d4
        rts
}



asm BNWORD16
lbnMulSub1_16(BNWORD16 *out, BNWORD16 const *in, unsigned len, BNWORD16 k)
{
        move.w  d4,-(sp) 
        clr.w   d4
        move.w  d3,-(sp)        /* 4 bytes of stack frame */
        move.l  4+4(sp),a1      /* out */
        move.l  4+8(sp),a0      /* in */
#if UINT_MAX == 0xffff
        move.w  4+12(sp),d3     /* len */
        move.w  4+14(sp),d2     /* k */
#else
        move.w  4+14(sp),d3     /* len (low 16 bits) */
        move.w  4+16(sp),d2     /* k */
#endif

        move.w  (a0)+,d1        /* First multiply */
        mulu.w  d2,d1
        sub.w   d1,(a1)+
        clr.w   d1
        swap    d1
        addx.w  d4,d1

        subq.w  #1,d3           /* Setup for loop unrolling */
        lsr.w   #1,d3
        bcs.s   ms16_even
        beq.s   ms16_short
        
        subq.w  #1,d3           /* Set up software pipeline properly */
        move.l  d1,d0
        
ms16_loop:
        move.w  (a0)+,d1
        mulu.w  d2,d1
        add.l   d0,d1
        sub.w   d1,(a1)+
        clr.w   d1
        swap    d1
        addx.w  d4,d1
ms16_even:

        move.w  (a0)+,d0
        mulu.w  d2,d0
        add.l   d1,d0
        sub.w   d0,(a1)+
        clr.w   d0
        swap    d0
        addx.w  d4,d0

        dbra    d3,ms16_loop
        
        move.w  (sp)+,d3
        move.w  (sp)+,d4
        rts
ms16_short:
        move.w  (sp)+,d3
        move.l  d1,d0   
        move.w  (sp)+,d4
        rts
}

/* The generic long/short divide doesn't know that nh < d */
asm BNWORD16
lbnDiv21_16(BNWORD16 *q, BNWORD16 nh, BNWORD16 nl, BNWORD16 d)
{
        move.l  8(sp),d0		/* nh *and* nl */
        divu.w	12(sp),d0
        move.l	4(sp),a0
        move.w	d0,(a0)
        clr.w	d0
        swap	d0
        rts
}

asm unsigned
lbnModQ_16(BNWORD16 const *n, unsigned len, BNWORD16 d)
{
        move.l  4(sp),a0        /* n */
        moveq.l	#0,d1
#if UINT_MAX == 0xffff
        move.w  8(sp),d1        /* len */
        move.w  10(sp),d2       /* d */
#else
        move.w  10(sp),d1       /* len (low 16 bits) */
        move.w  12(sp),d2       /* d */
#endif

		add.l	d1,a0
		add.l	d1,a0			/* n += len */
		moveq.l	#0,d0
        subq.w  #1,d1

mq16_loop:
        move.w  -(a0),d0		/* Assemble remainder and new word */
        divu.w  d2,d0        	/* Put remainder in high half of d0 */
        dbra    d1,mq16_loop    
                        
mq16_done:
        clr.w   d0
        swap    d0
        rts
}

/*
 * Detect if this is a 32-bit processor (68020+ *or* CPU32).
 * Both the 68020+ and CPU32 processors (which have 32x32->64-bit
 * multiply, what the 32-bit math library wants) support scaled indexed
 * addressing.  The 68000 and 68010 ignore the scale selection
 * bits, treating it as *1 all the time.  So a 32-bit processor
 * will evaluate -2(a0,a0.w*2) as 1+1*2-2 = 1.
 * A 16-bit processor will compute 1+1-2 = 0.
 *
 * Thus, the return value will indicate whether the chip this is
 * running on supports 32x32->64-bit multiply (mulu.l).
 */
asm int
is68020(void)
{
        machine 68020
        lea     1,a0
#if 0
        lea     -2(a0,a0.w*2),a0	/* Metrowerks won't assemble this, arrgh */
#else
        dc.w    0x41f0, 0x82fe
#endif
        move.l	a0,d0
        rts
}
/*
 * Since I had to hand-assemble that fancy addressing mode, I had to study
 * up on 680x0 addressing modes.
 * A summary of 680x0 addressing modes.
 * A 68000 effective address specifies an operand on an instruction, which
 * may be a register or in memory.  It is made up of a 3-bit mode and a
 * 3-bit register specifier.  The meanings of the various modes are:
 *
 * 000 reg - Dn, n specified by "reg"
 * 001 reg - An, n specified by "reg"
 * 010 reg - (An)
 * 011 reg - (An)+
 * 100 reg - -(An)
 * 101 reg - d16(An), one 16-bit displacement word follows, sign-extended
 * 110 reg - Fancy addressing mode off of An, see extension word below
 * 111 000 - abs.W, one 16-bit signed absolute address follows
 * 111 001 - abs.L, one 32-bit absolute address follows
 * 111 010 - d16(PC), one 16-bit displacemnt word follows, sign-extended
 * 111 011 - Fancy addressing mode off of PC, see extension word below
 * 111 100 - #immediate, followed by 16 or 32 bits of immediate value
 * 111 101 - unused, reserved
 * 111 110 - unused, reserved
 * 111 111 - unused, reserved
 *
 * Memory references are to data space, except that PC-relative references
 * are to program space, and are read-only.
 *
 * Fancy addressing modes are followed by a 16-bit extension word, and come
 * in "brief" and "full" forms.
 * The "brief" form looks like this.  Bit 8 is 0 to indicate this form:
 *
 * 1   1   1   1   1   1   1  
 * 6   5   4   3   2   1   0   9   8   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * |A/D|  register |L/W| scale | 0 |   8-bit signed displacement   |
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *
 * The basic effective address specifies a 32-bit base register - A0 through
 * A7 or PC (the address of the following instruction).
 * The A/D and register fields specify an index register.  A/D is 1 for
 * address registers, and 0 for data registers.  L/W specifies the length
 * of the index register, 1 for 32 bits, and 0 for 16 bits (sign-extended).
 * The scale field is a left shift amount (0 to 3 bits) to apply to the
 * sign-extended index register.  The final address is d8(An,Rn.X*SCALE),
 * also written (d8,An,Rn.X*SCALE).  X is "W" or "L", SCALE is 1, 2, 4 or 8.
 * "*1" may be omitted, as may a d8 of 0.
 *
 * The 68000 supports this form, but only with a scale field of 0.
 * It does NOT (says the MC68030 User's Manual MC68030UM/AD, section 2.7)
 * decode the scale field and the following format bit.  They are treated
 * as 0.
 * I recall (I don't have the data book handy) that the CPU32 processor
 * core used in the 683xx series processors supports variable scales,
 * but only the brief extension word form.  I suspect it decodes the
 * format bit and traps if it is not zero, but I don't recall.
 *
 * The "full" form (680x0, x >= 2 processors only) looks like this: 
 *
 * 1   1   1   1   1   1   1  
 * 6   5   4   3   2   1   0   9   8   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * |A/D|  register |L/W| scale | 1 | BS| IS|BD size| 0 | P |OD size|
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *
 * The first 8 bits are interpreted the same way as in the brief form,
 * except that bit 8 is set to 1 to indicate the full form.
 * BS, Base Suppress, if set, causes a value of 0 to be used in place of
 * the base register value.  If this is set, the base register
 * specified is irrelevant, except that if it is the PC, the fetch is
 * still done from program space.  The specifier "ZPC" can be used in
 * place of "PC" in the effective address mnemonic to represent this
 * case.
 * IS, Index Suppress, if set, causes a value of 0 to be used in place
 * of the scaled index register. In this case, the first 7 bits of the
 * extension word are irrelevant.
 * BD size specifies the base displacement size.  A value of 00
 * in this field is illegal, while 01, 10 and 11 indicate that the
 * extension word is followed by 0, 1 or 2 16-bit words of base displacement
 * (zero, sign-extended to 32 bits, and most-significant word first,
 * respectively) to add to the base register value.
 * Bit 3 is unused.
 * The P bit is the pre/post indexing bit, and only applies if an outer
 * displacement is used.  This is explained later.
 * OD size specifies the size of an outer displacement.  In the simple
 * case, this field is set to 00 and the effective address is
 * (disp,An,Rn.X*SCALE) or (disp,PC,Rn.X*SCALE).
 * In this case the P bit must be 0.  Any of those compnents may be
 * suppressed, with a BD size of 01, the BS bit, or the IS bit.
 * If the OD size is not 00, it encodes an outer displacement in the same
 * manner as the BD size, and 0, 1 or 2 16-bit words of outer displacement
 * follow the base displacement in the instruction stream.  In this case,
 * this is a double-indirect addressing mode.  The base, base displacement,
 * and possibly the index, specify a 32-bit memory word which holds a value
 * which is fetched, and the outer displacement and possibly the index are
 * added to produce the address of the operand.
 * If the P bit is 0, this is pre-indexed, and the index value is added
 * before the fetch of the indirect word, producing an effective address
 * of ([disp,An,Rn.X*SCALE],disp).  If the P bit is 1, the post-indexed case,
 * the memory word is fectched from base+base displacement, then the index
 * and outer displacement are added to compute the address of the operand.
 * This effective address is written ([disp,An],Rn.X*SCALE,disp).
 * (In both cases, "An" may also be "PC" or "ZPC".)
 * Any of the components may be omitted.  If the index is omitted (using the
 * IS bit), the P bit is irrelevant, but must be written as 0.
 * Thus, legal combinations of IS, P and OD size are:
 * 0 0 00 - (disp,An,Rn.X*SCALE), also written disp(An,Rn.X*SCALE)
 * 0 0 01 - ([disp,An,Rn.X*SCALE])
 * 0 0 10 - ([disp,An,Rn.X*SCALE],d16)
 * 0 0 11 - ([disp,An,Rn.X*SCALE],d32)
 * 0 1 01 - ([disp,An],Rn.X*SCALE)
 * 0 1 10 - ([disp,An],Rn.X*SCALE,d16)
 * 0 1 11 - ([disp,An],Rn.X*SCALE,d32)
 * 1 0 00 - (disp,An), also written disp(An)
 * 1 0 01 - ([disp,An])
 * 1 0 10 - ([disp,An],d16)
 * 1 0 11 - ([disp,An],d32)
 */ 

/* 45678901234567890123456789012345678901234567890123456789012345678901234567 */
