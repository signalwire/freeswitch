/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * DEC Alpha 64-bit math primitives.  These use 64-bit words
 * unless otherwise noted.
 *
 * The DEC assembler apparently does some instruction scheduling,
 * but I tried to do some of my own, and tries to spread things
 * out over the register file to give the assembler more room
 * to schedule things.
 *
 * Alpha OSF/1 register usage conventions:
 * r0       - v0      - Temp, holds integer return value
 * r1..r8   - t0..t7  - Temp, trashed by procedure call
 * r9..r14  - s0..s5  - Saved across procedure calls
 * r15      - s6/FP   - Frame pointer, saved across procedure calls
 * r16..r21 - a0..a5  - Argument registers, all trashed by procedure call
 * r22..r25 - t8..t11 - Temp, trashed by procedure call
 * r26      - ra      - Return address
 * r27      - t12/pv  - Procedure value, trashed by procedure call
 * r28      - at      - Assembler temp, trashed by procedure call
 * r29      - gp      - Global pointer
 * r30      - sp      - Stack pointer
 * r31      - zero    - hardwired to zero
 */
	.text
	.align	4
	.globl	lbnMulN1_64
/* I have no idea what the '2' at the end of the .ent line means. */
	.ent	lbnMulN1_64 2
/*
 * Arguments: $16 = out, $17 = in, $18 = len<32>, $19 = k
 * Other registers: $0 = carry word, $1 = product low,
 * $2 = product high, $3 = input word
 */
lbnMulN1_64:
	ldq	$3,0($17)	/* Load first word of input */
	subl	$18,1,$18
	mulq	$3,$19,$1	/* Do low half of first multiply */
	umulh	$3,$19,$0	/* Do second half of first multiply */
	stq	$1,0($16)
	beq	$18,m64_done
m64_loop:
	ldq	$3,8($17)
	addq	$17,8,$17
	mulq	$3,$19,$1	/* Do bottom half of multiply */
	subl	$18,1,$18
	umulh	$3,$19,$2	/* Do top half of multiply */
	addq	$0,$1,$1	/* Add carry word from previous multiply */
	stq	$1,8($16)
	cmpult	$1,$0,$0	/* Compute carry bit from add */
	addq	$16,8,$16
	addq	$2,$0,$0	/* Add carry bit to carry word */
 
	beq	$18,m64_done

	ldq	$3,8($17)
	addq	$17,8,$17
	mulq	$3,$19,$1	/* Do bottom half of multiply */
	subl	$18,1,$18
	umulh	$3,$19,$2	/* Do top half of multiply */
	addq	$0,$1,$1	/* Add carry word from previous multiply */
	stq	$1,8($16)
	cmpult	$1,$0,$0	/* Compute carry bit from add */
	addq	$16,8,$16
	addq	$2,$0,$0	/* Add carry bit to carry word */
 
	bne	$18,m64_loop
m64_done:
	stq	$0,8($16)	/* Store last word of result */
	ret	$31,($26),1
/* The '1' in the hint field means procedure return - software convention */
	.end lbnMulN1_64
 
 
	.text
	.align	4
	.globl	lbnMulAdd1_64
	.ent	lbnMulAdd1_64 2
/*
 * Arguments: $16 = out, $17 = in, $18 = len<32>, $19 = k
 * Other registers: $0 = product high, $1 = product low,
 * $2 = product high temp, $3 = input word, $4 = output word
 * $5 = carry bit from add to out
 */
lbnMulAdd1_64:
	ldq	$3,0($17)	/* Load first word of input */
	subl	$18,1,$18
	mulq	$3,$19,$1	/* Do low half of first multiply */
	ldq	$4,0($16)	/* Load first word of output */
	umulh	$3,$19,$2	/* Do second half of first multiply */
	addq	$4,$1,$4
	cmpult	$4,$1,$5	/* Compute borrow bit from subtract */
	stq	$4,0($16)
	addq	$5,$2,$0	/* Add carry bit to high word */
	beq	$18,ma64_done
ma64_loop:
	ldq	$3,8($17)	/* Load next word of input */
	addq	$17,8,$17
	ldq	$4,8($16)	/* Load next word of output */
	mulq	$3,$19,$1	/* Do bottom half of multiply */
	subl	$18,1,$18
	addq	$0,$1,$1	/* Add carry word from previous multiply */
	umulh	$3,$19,$2	/* Do top half of multiply */
	cmpult	$1,$0,$0	/* Compute carry bit from add */
	addq	$4,$1,$4	/* Add product to loaded word */
	cmpult	$4,$1,$5	/* Compute carry bit from add */
	stq	$4,8($16)
	addq	$5,$0,$5	/* Add carry bits together */
	addq	$16,8,$16
	addq	$5,$2,$0	/* Add carry bits to carry word */
 
	beq	$18,ma64_done

	ldq	$3,8($17)	/* Load next word of input */
	addq	$17,8,$17
	ldq	$4,8($16)	/* Load next word of output */
	mulq	$3,$19,$1	/* Do bottom half of multiply */
	subl	$18,1,$18
	addq	$0,$1,$1	/* Add carry word from previous multiply */
	umulh	$3,$19,$2	/* Do top half of multiply */
	cmpult	$1,$0,$0	/* Compute carry bit from add */
	addq	$4,$1,$4	/* Add product to loaded word */
	cmpult	$4,$1,$5	/* Compute carry bit from add */
	stq	$4,8($16)
	addq	$5,$0,$5	/* Add carry bits together */
	addq	$16,8,$16
	addq	$5,$2,$0	/* Add carry bits to carry word */
 
	bne	$18,ma64_loop
ma64_done:
	ret	$31,($26),1
	.end lbnMulAdd1_64
 
 
	.text
	.align	4
	.globl	lbnMulSub1_64
	.ent	lbnMulSub1_64 2
/*
 * Arguments: $16 = out, $17 = in, $18 = len<32>, $19 = k
 * Other registers: $0 = carry word, $1 = product low,
 * $2 = product high temp, $3 = input word, $4 = output word
 * $5 = borrow bit from subtract
 */
lbnMulSub1_64:
	ldq	$3,0($17)	/* Load first word of input */
	subl	$18,1,$18
	mulq	$3,$19,$1	/* Do low half of first multiply */
	ldq	$4,0($16)	/* Load first word of output */
	umulh	$3,$19,$2	/* Do second half of first multiply */
	cmpult	$4,$1,$5	/* Compute borrow bit from subtract */
	subq	$4,$1,$4
	addq	$5,$2,$0	/* Add carry bit to high word */
	stq	$4,0($16)
	beq	$18,ms64_done
ms64_loop:
	ldq	$3,8($17)	/* Load next word of input */
	addq	$17,8,$17
	ldq	$4,8($16)	/* Load next word of output */
	mulq	$3,$19,$1	/* Do bottom half of multiply */
	subl	$18,1,$18
	addq	$0,$1,$1	/* Add carry word from previous multiply */
	umulh	$3,$19,$2	/* Do top half of multiply */
	cmpult	$1,$0,$0	/* Compute carry bit from add */
	cmpult	$4,$1,$5	/* Compute borrow bit from subtract */
	subq	$4,$1,$4
	addq	$5,$0,$5	/* Add carry bits together */
	stq	$4,8($16)
	addq	$5,$2,$0	/* Add carry bits to carry word */
	addq	$16,8,$16
 
	beq	$18,ms64_done

	ldq	$3,8($17)	/* Load next word of input */
	addq	$17,8,$17
	ldq	$4,8($16)	/* Load next word of output */
	mulq	$3,$19,$1	/* Do bottom half of multiply */
	subl	$18,1,$18
	addq	$0,$1,$1	/* Add carry word from previous multiply */
	umulh	$3,$19,$2	/* Do top half of multiply */
	cmpult	$1,$0,$0	/* Compute carry bit from add */
	cmpult	$4,$1,$5	/* Compute borrow bit from subtract */
	subq	$4,$1,$4
	addq	$5,$0,$5	/* Add carry bits together */
	stq	$4,8($16)
	addq	$5,$2,$0	/* Add carry bits to carry word */
	addq	$16,8,$16
 
	bne	$18,ms64_loop
ms64_done:
	ret	$31,($26),1
	.end lbnMulSub1_64
