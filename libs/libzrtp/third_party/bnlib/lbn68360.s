* Copyright (c) 1995  Colin Plumb.  All rights reserved.
* For licensing and other legal details, see the file legal.c.
*
* lbn68360.c - 32-bit bignum primitives for 683xx processors.
*
* This code is using InterTools calling convention, which is a bit odd.
* One minor note is that the default variable sizes are
* char = unsigned 8, short = 8 (in violation of ANSI!),
* int = 16, long = 32.  Longs (including on the stack) are 16-bit aligned.
* Arguments are apdded to 16 bits.
* A6 is used as a frame pointer, and globals are indexed off A5.
* Return valies are passes id D0 or A0 (or FP0), depending on type.
* D0, D1, A0 and A4 (!) are volatile across function calls.  A1
* must be preserved!
* 
* This code assumes 16-bit ints.  Code for 32-bit ints is commented out
* with "**".
*
* Regardless of UINT_MAX, only bignums up to 64K words (2 million bits)
* are supported.  (68k hackers will recognize this as a consequence of
* using dbra.)  This could be extended easily if anyone cares.
*
* These primitives use little-endian word order.
* (The order of bytes within words is irrelevant to this issue.)

* The Metrowerks C compiler (1.2.2) produces bad 68k code for the
* following input, which happens to be the inner loop of lbnSub1,
* so it has been rewritees in assembly, even though it is not terribly
* speed-critical.  (Optimizer on or off does not matter.)
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

* BNWORD32 lbnSub1_32(BNWORD32 *num, unsigned len, BNWORD32 borrow)
	SECTION	S_lbnSub1_32,,"code"
	XDEF	_lbnSub1_32
_lbnSub1_32:
	movea.l	4(sp),a0	* num
	move.l	10(sp),d0	* borrow
**	move.l	12(sp),d0	* borrow
	sub.l	d0,(a0)+
	bcc	sub_done
	move.w	8(sp),d0	* len
**	move.w	10(sp),d0	* len
	subq.w	#2,d0
	bcs	sub_done
sub_loop:
	subq.l	#1,(a0)+
	dbcc	d0,sub_loop
sub_done:
	moveq.l	#0,d0
	addx.w	d0,d0
	rts

* BNWORD32 lbnAdd1_32(BNWORD32 *num, unsigned len, BNWORD32 carry)
	SECTION	S_lbnAdd1_32,,"code"
	XDEF	_lbnAdd1_32
_lbnAdd1_32:
	movea.l	4(sp),a0	* num
	move.l	10(sp),d0	* carry
**	move.l	12(sp),d0	* carry
	add.l	d0,(a0)+
	bcc	add_done
	move.w	8(sp),d0	* len
**	move.w	10(sp),d0	* len
	subq.w	#2,d0
	bcs	add_done
add_loop:
	addq.l	#1,(a0)+
	dbcc	d0,add_loop
add_done:
	moveq.l	#0,d0
	addx.w	d0,d0
	rts

* void lbnMulN1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
	SECTION	S_lbnMulN1_32,,"code"
	XDEF	_lbnMulN1_32
_lbnMulN1_32:
	movem.l	d2-d5,-(sp)	* 16 bytes of extra data
	moveq.l	#0,d4
	move.l	20(sp),a4	* out
	move.l	24(sp),a0	* in
	move.w	28(sp),d5	* len
	move.l	30(sp),d2	* k
**	move.w	30(sp),d5	* len
**	move.l	32(sp),d2	* k

	move.l	(a0)+,d3	* First multiply
	mulu.l	d2,d1:d3	* dc.w    0x4c02, 0x3401
	move.l	d3,(a4)+

	subq.w	#1,d5		* Setup for loop unrolling
	lsr.w	#1,d5
	bcs.s	m32_even
	beq.s	m32_short
	
	subq.w	#1,d5		* Set up software pipeline properly
	move.l	d1,d0
	
m32_loop:
	move.l	(a0)+,d3
	mulu.l	d2,d1:d3	* dc.w    0x4c02, 0x3401
	add.l	d0,d3
	addx.l	d4,d1
	move.l	d3,(a4)+
m32_even:

	move.l	(a0)+,d3
	mulu.l	d2,d0:d3	* dc.w    0x4c02, 0x3400
	add.l	d1,d3
	addx.l	d4,d0
	move.l	d3,(a4)+

	dbra	d5,m32_loop
	
	move.l	d0,(a4)
	movem.l	(sp)+,d2-d5
	rts
m32_short:
	move.l	d1,(a4)
	movem.l	(sp)+,d2-d5
	rts

* BNWORD32
* lbnMulAdd1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
	SECTION	S_lbnMulAdd1_32,,"code"
	XDEF	_lbnMulAdd1_32
_lbnMulAdd1_32:
	movem.l	d2-d5,-(sp)	* 16 bytes of extra data
	moveq.l	#0,d4
	move.l	20(sp),a4	* out
	move.l	24(sp),a0	* in
	move.w	28(sp),d5	* len
	move.l	30(sp),d2	* k
**	move.w	30(sp),d5	* len
**	move.l	32(sp),d2	* k

	move.l	(a0)+,d3	* First multiply
	mulu.l	d2,d1:d3	* dc.w    0x4c02, 0x3401
	add.l	d3,(a4)+
	addx.l	d4,d1

	subq.w	#1,d5	* Setup for loop unrolling
	lsr.w	#1,d5
	bcs.s	ma32_even
	beq.s	ma32_short
	
	subq.w	#1,d5	* Set up software pipeline properly
	move.l	d1,d0
	
ma32_loop:
	move.l	(a0)+,d3
	mulu.l	d2,d1:d3	* dc.w    0x4c02, 0x3401
	add.l	d0,d3
	addx.l	d4,d1
	add.l	d3,(a4)+
	addx.l	d4,d1
ma32_even:

	move.l	(a0)+,d3
	mulu.l	d2,d0:d3	* dc.w    0x4c02, 0x3400
	add.l	d1,d3
	addx.l	d4,d0
	add.l	d3,(a4)+
	addx.l	d4,d0

	dbra	d5,ma32_loop
	
	movem.l	(sp)+,d2-d5
	rts
ma32_short:
	move.l	d1,d0   
	movem.l	(sp)+,d2-d5
	rts

* BNWORD32
* lbnMulSub1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
	SECTION	S_lbnMulSub1_32,,"code"
	XDEF	_lbnMulSub1_32
_lbnMulSub1_32:
	movem.l	d2-d5,-(sp)	* 16 bytes of extra data
	moveq.l	#0,d4
	move.l	20(sp),a4	* out
	move.l	24(sp),a0	* in
	move.w	28(sp),d5	* len
	move.l	30(sp),d2	* k
**	move.w	30(sp),d5	* len
**	move.l	32(sp),d2	* k

	move.l	(a0)+,d3	* First multiply
	mulu.l	d2,d1:d3	* dc.w    0x4c02, 0x3401
	sub.l	d3,(a4)+
	addx.l	d4,d1

	subq.w	#1,d5	* Setup for loop unrolling
	lsr.w	#1,d5
	bcs.s	ms32_even
	beq.s	ms32_short
	
	subq.w	#1,d5	* Set up software pipeline properly
	move.l	d1,d0
	
ms32_loop:
	move.l	(a0)+,d3
	mulu.l	d2,d1:d3	* dc.w	0x4c02, 0x3401
	add.l	d0,d3
	addx.l	d4,d1
	sub.l	d3,(a4)+
	addx.l	d4,d1
ms32_even:

	move.l	(a0)+,d3
	mulu.l	d2,d0:d3	* dc.w	0x4c02, 0x3400
	add.l	d1,d3
	addx.l	d4,d0
	sub.l	d3,(a4)+
	addx.l	d4,d0

	dbra	d5,ms32_loop
	
	movem.l	(sp)+,d2-d5
	rts
	
ms32_short:
	move.l	d1,d0
	movem.l	(sp)+,d2-d5
	rts


* BNWORD32 lbnDiv21_32(BNWORD32 *q, BNWORD32 nh, BNWORD32 nl, BNWORD32 d)
	SECTION	S_lbnDiv21_32,,"code"
	XDEF	_lbnDiv21_32
_lbnDiv21_32:
	move.l	8(sp),d0
	move.l	12(sp),d1
	move.l	4(sp),a0
	divu.l	16(sp),d0:d1	*  dc.w	0x4c6f, 0x1400, 16
	move.l	d1,(a0)
	rts

* unsigned lbnModQ_32(BNWORD32 const *n, unsigned len, unsigned d)
	SECTION	S_lbnModQ_32,,"code"
	XDEF	_lbnModQ_32
_lbnModQ_32:
	move.l	4(sp),a0	* n
	move.l	d2,-(sp)
	move.l	d3,a4
	moveq.l	#0,d1
	moveq.l	#0,d2
	move.w	12(sp),d1	* len
	move.w	14(sp),d2	* d
**	move.l	12(sp),d1	* len
**	move.l	16(sp),d2	* d
	lea  -4(a0,d1.L*4),a0	* dc.w	0x41f0, 0x1cfc

* First time, divide 32/32 - may be faster than 64/32
	move.l	(a0),d3
	divul.l	d2,d0:d3	* dc.w    0x4c02, 0x3000
	subq.w	#2,d1
	bmi	mq32_done

mq32_loop:
	move.l	-(a0),d3
	divu.l	d2,d0:d3	* dc.w    0x4c02,0x3400
	dbra	d1,mq32_loop    
	                
mq32_done:
	move.l	(sp)+,d2
	move.l	a4,d3
	rts

	end
