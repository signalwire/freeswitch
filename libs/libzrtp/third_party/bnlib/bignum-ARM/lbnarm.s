@ lbnarm.s - 32-bit bignum primitives for ARM processors with 32x32-bit multiply
@
@ This uses the standard ARM calling convetion, which is that arguments
@ are passed, and results returned, in r0..r3.  r0..r3, r12 (IP) and r14 (LR)
@ are volatile across the function; all others are callee-save.
@ However, note that r14 (LR) is the return address, so it would be
@ wise to save it somewhere before trashing it.  Fortunately, there is
@ a neat trick possible, in that you can pop LR from the stack straight
@ into r15 (PC), effecting a return at the same time.
@
@ Also, r13 (SP) is probably best left alone, and r15 (PC) is obviously
@ reserved by hardware.  Temps should use lr, then r4..r9 in order.

	.text
	.align	2

@ out[0..len] = in[0..len-1] * k
@ void lbnMulN1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
	.global	lbnMulN1_32
	.type	lbnMulN1_32, %function
lbnMulN1_32:
	stmfd	sp!, {r4, r5, lr}
	ldr	lr, [r1], #4		@ lr = *in++
	umull	r5, r4, lr, r3		@ (r4,r5) = lr * r3
	str	r5, [r0], #4		@ *out++ = r5
	movs	r2, r2, lsr #1
	bcc	m32_even
	mov	r5, r4			@ Get carry in the right register
	beq	m32_done
m32_loop:
	@ carry is in r5
	ldr	lr, [r1], #4		@ lr = *in++
	mov	r4, #0
	umlal	r5, r4, lr, r3		@ (r4,r5) += lr * r3
	str	r5, [r0], #4		@ *out++ = r5
m32_even:
	@ carry is in r4
	ldr	lr, [r1], #4		@ lr = *in++
	mov	r5, #0
	umlal	r4, r5, lr, r3		@ (r5,r4) += lr * r3
	subs	r2, r2, #1
	str	r4, [r0], #4		@ *out++ = r4

	bne	m32_loop
m32_done:
	str	r5, [r0, #0]		@ store carry
	ldmfd	sp!, {r4, r5, pc}
	.size	lbnMulN1_32, .-lbnMulN1_32

@ out[0..len-1] += in[0..len-1] * k, return carry
@ BNWORD32
@ lbnMulAdd1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)

	.global	lbnMulAdd1_32
	.type	lbnMulAdd1_32, %function
lbnMulAdd1_32:
	stmfd	sp!, {r4, r5, lr}

	mov	r4, #0
	ldr	lr, [r1], #4		@ lr = *in++
	ldr	r5, [r0, #0]		@ r5 = *out
	mov	r4, #0
	umlal	r5, r4, lr, r3		@ (r4,r5) += lr * r3
	str	r5, [r0], #4		@ *out++ = r5
	movs	r2, r2, lsr #1
	bcc	ma32_even
	beq	ma32_done
ma32_loop:
	@ carry is in r4
	ldr	lr, [r1], #4		@ lr = *in++
	mov	r5, #0
	umlal	r4, r5, lr, r3		@ (r5,r4) += lr * r3
	ldr	lr, [r0, #0]		@ lr = *out
	adds	lr, lr, r4		@ lr += product.low
	str	lr, [r0], #4		@ *out++ = lr
	adc	r4, r5, #0		@ Compute carry and move back to r4
ma32_even:
	@ another unrolled copy
	ldr	lr, [r1], #4		@ lr = *in++
	mov	r5, #0
	umlal	r4, r5, lr, r3		@ (r5,r4) += lr * r3
	ldr	lr, [r0, #0]		@ lr = *out
	adds	lr, lr, r4		@ lr += product.low
	adc	r4, r5, #0		@ Compute carry and move back to r4
	str	lr, [r0], #4		@ *out++ = lr
	subs	r2, r2, #1

	bne	ma32_loop
ma32_done:
	mov	r0, r4
	ldmfd	sp!, {r4, r5, pc}
	.size	lbnMulAdd1_32, .-lbnMulAdd1_32

@@@ This is a bit messy... punt for now...
@ out[0..len-1] -= in[0..len-1] * k, return carry (borrow)
@ BNWORD32
@ lbnMulSub1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
	.global	lbnMulSub1_32
	.type	lbnMulSub1_32, %function
lbnMulSub1_32:
	stmfd	sp!, {r4, r5, lr}

	mov	r4, #0
	mov	r5, #0
	ldr	lr, [r1], #4		@ lr = *in++
	umull	r4, r5, lr, r3		@ (r5,r4) = lr * r3
	ldr	lr, [r0, #0]		@ lr = *out
	subs	lr, lr, r4		@ lr -= product.low
	str	lr, [r0], #4		@ *out++ = lr
	addcc	r5, r5, #1		@ propagate carry

	movs	r2, r2, lsr #1
	bcc	ms32_even
	mov	r4, r5
	beq	ms32_done
ms32_loop:
	@ carry is in r4
	ldr	lr, [r1], #4		@ lr = *in++
	mov	r5, #0
	umlal	r4, r5, lr, r3		@ (r5,r4) += lr * r3
	ldr	lr, [r0, #0]		@ lr = *out
	subs	lr, lr, r4		@ lr -= product.low
	str	lr, [r0], #4		@ *out++ = lr
	addcc	r5, r5, #1		@ propagate carry
ms32_even:
	@ carry is in r5
	ldr	lr, [r1], #4		@ lr = *in++
	mov	r4, #0
	umlal	r5, r4, lr, r3		@ (r4,r5) += lr * r3
	ldr	lr, [r0, #0]		@ lr = *out
	subs	lr, lr, r5		@ lr -= product.low
	str	lr, [r0], #4		@ *out++ = lr
	addcc	r4, r4, #1		@ Propagate carry

	subs	r2, r2, #1
	bne	ms32_loop
ms32_done:
	mov	r0, r4
	ldmfd	sp!, {r4, r5, pc}

	.size	lbnMulSub1_32, .-lbnMulSub1_32

@@
@@ It's possible to eliminate the store traffic by doing the multiplies
@@ in a different order, forming all the partial products in one column
@@ at a time.  But it requires 32x32 + 64 -> 65-bit MAC.  The
@@ ARM has the MAC, but no carry out.
@@
@@ The question is, is it faster to do the add directly (3 instructions),
@@ or can we compute the carry out in 1 instruction (+1 to do the add)?
@@ Well... it takes at least 1 instruction to copy the original accumulator,
@@ out of the way, and 1 to do a compare, so no.
@@
@@ Now, the overall loop... this is an nxn->2n multiply.  For i=0..n-1,
@@ we sum i+1 multiplies in each (plus the carry in from the
@@ previous one).  For i = n..2*n-1 we sum 2*n-1-i, plus the previous
@@ carry.
@@
@@ This "non-square" structure makes things more complicated.
@@
@@ void
@@ lbnMulX_32(BNWORD32 *prod, BNWORD32 const *num1, BNWORD32 const *num2,
@@ 	unsigned len)
@	.global	lbnMulX_32
@	.type	lbnMulX_32, %function
@lbnMulX_32:
@	stmfd	sp!, {r4, r5, r6, r7, lr}
@
@	mov	r4, #0
@	mov	r5, #0
@	mov	r0, r4
@	ldmfd	sp!, {r4, r5, pc}
@	.size	lbnMulX_32, .-lbnMulX_32
