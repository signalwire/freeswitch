### Copyright (c) 1995, Colin Plumb.
### For licensing and other legal details, see the file legal.c.
###
### Assembly primitives for bignum library, 80386 family, 32-bit code.
###
### Several primitives are included here.  Only lbnMulAdd1 is *really*
### critical, but once that's written, lnmMulN1 and lbnMulSub1 are quite
### easy to write as well, so they are included here as well.
### lbnDiv21 and lbnModQ are so easy to write that they're included, too.
###
### All functions here are for 32-bit flat mode.  I.e. near code and
### near data, although the near offsets are 32 bits.
### Preserved registers are esp, ebp, esi, edi and ebx.  That last
### is needed by ELF for PIC, and differs from the IBM PC calling
### convention.

# Different assemblers have different conventions here
align4=4	# could be 2 or 4
align8=8	# could be 3 or 8
align16=16	# cound be 4 or 16


.text

# We declare each symbol with two names, to deal with ELF/a.out variances.
	.globl	lbnMulN1_32
	.globl	_lbnMulN1_32
	.globl	lbnMulAdd1_32
	.globl	_lbnMulAdd1_32
	.globl	lbnMulSub1_32
	.globl	_lbnMulSub1_32
	.globl	lbnDiv21_32
	.globl	_lbnDiv21_32
	.globl	lbnModQ_32
	.globl	_lbnModQ_32

## Register usage:
## %eax - low half of product
## %ebx - carry to next iteration
## %ecx - multiplier (k)
## %edx - high half of product
## %esi - source pointer
## %edi - dest pointer
## %ebp - loop counter
##
## Stack frame:
## +--------+ %esp+20  %esp+24  %esp+28  %esp+32  %esp+36
## |    k   |
## +--------+ %esp+16  %esp+20  %esp+24  %esp+28  %esp+32
## |   len  |
## +--------+ %esp+12  %esp+16  %esp+20  %esp+24  %esp+28
## |   in   |
## +--------+ %esp+8   %esp+12  %esp+16  %esp+20  %esp+24
## |   out  |
## +--------+ %esp+4   %esp+8   %esp+12  %esp+16  %esp+20
## | return |
## +--------+ %esp     %esp+4   %esp+8   %esp+12  %esp+16
## |  %esi  |
## +--------+          %esp     %esp+4   %esp+8   %esp+12
## |  %ebp  |
## +--------+                   %esp     %esp+4   %esp+8
## |  %ebx  |
## +--------+                            %esp     %esp+4
## |  %edi  |
## +--------+                                     %esp

	.align	align16
lbnMulN1_32:
_lbnMulN1_32:
	pushl	%esi		# U
	movl	12(%esp),%esi	#  V	load in
	pushl	%ebp		# U
	movl	20(%esp),%ebp	#  V	load len
	pushl	%ebx		# U
	movl	28(%esp),%ecx	#  V	load k
	pushl	%edi		# U
	movl	20(%esp),%edi	#  V	load out

## First multiply step has no carry in.
	movl	(%esi),%eax		#  V
	leal	-4(,%ebp,4),%ebx	# U	loop unrolling
	mull	%ecx			# NP	first multiply
	movl	%eax,(%edi)		# U
	andl	$12,%ebx		#  V	loop unrolling

	addl	%ebx,%esi		# U	loop unrolling
	addl	%ebx,%edi		#  V	loop unrolling

	jmp	*m32_jumptable(%ebx)	# NP	loop unrolling

	.align	align4
m32_jumptable:
	.long	m32_case0
	.long	m32_case1
	.long	m32_case2
	.long	m32_case3

	nop
	.align	align8
	nop
	nop
	nop	# Get loop nicely aligned

m32_case0:
	subl	$4,%ebp		# U
	jbe	m32_done	#  V

m32_loop:
	movl	4(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	addl	$16,%esi	# U
	addl	$16,%edi	#  V
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	adcl	$0,%edx		# U
	movl	%eax,-12(%edi)	#  V
m32_case3:
	movl	-8(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	adcl	$0,%edx		# U
	movl	%eax,-8(%edi)	#  V
m32_case2:
	movl	-4(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	adcl	$0,%edx		# U
	movl	%eax,-4(%edi)	#  V
m32_case1:
	movl	(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	adcl	$0,%edx		# U
	movl	%eax,(%edi)	#  V

	subl	$4,%ebp		# U
	ja	m32_loop	#  V

m32_done:
	movl	%edx,4(%edi)	# U
	popl	%edi		#  V
	popl	%ebx		# U
	popl	%ebp		#  V
	popl	%esi		# U
	ret			# NP


	.align	align16
lbnMulAdd1_32:
_lbnMulAdd1_32:

	pushl	%esi		# U
	movl	12(%esp),%esi	#  V	load in
	pushl	%edi		# U
	movl	12(%esp),%edi	#  V	load out
	pushl	%ebp		# U
	movl	24(%esp),%ebp	#  V	load len
	pushl	%ebx		# U
	movl	32(%esp),%ecx	#  V	load k

## First multiply step has no carry in.
	movl	(%esi),%eax		#  V
	movl	(%edi),%ebx		# U
	mull	%ecx			# NP	first multiply
	addl	%eax,%ebx		# U
	leal	-4(,%ebp,4),%eax	#  V	loop unrolling
	adcl	$0,%edx			# U
	andl	$12,%eax		#  V	loop unrolling
	movl	%ebx,(%edi)		# U

	addl	%eax,%esi		#  V	loop unrolling
	addl	%eax,%edi		# U	loop unrolling

	jmp	*ma32_jumptable(%eax)	# NP	loop unrolling

	.align	align4
ma32_jumptable:
	.long	ma32_case0
	.long	ma32_case1
	.long	ma32_case2
	.long	ma32_case3

	.align	align8
	nop
	nop
	nop			# To align loop properly


ma32_case0:
	subl	$4,%ebp		# U
	jbe	ma32_done	#  V

ma32_loop:
	movl	4(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	addl	$16,%esi	# U
	addl	$16,%edi	#  V
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	movl	-12(%edi),%ebx	#  V
	adcl	$0,%edx		# U
	addl	%eax,%ebx	#  V
	adcl	$0,%edx		# U
	movl	%ebx,-12(%edi)	#  V
ma32_case3:
	movl	-8(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	movl	-8(%edi),%ebx	#  V
	adcl	$0,%edx		# U
	addl	%eax,%ebx	#  V
	adcl	$0,%edx		# U
	movl	%ebx,-8(%edi)	#  V
ma32_case2:
	movl	-4(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	movl	-4(%edi),%ebx	#  V
	adcl	$0,%edx		# U
	addl	%eax,%ebx	#  V
	adcl	$0,%edx		# U
	movl	%ebx,-4(%edi)	#  V
ma32_case1:
	movl	(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	movl	(%edi),%ebx	#  V
	adcl	$0,%edx		# U
	addl	%eax,%ebx	#  V
	adcl	$0,%edx		# U
	movl	%ebx,(%edi)	#  V

	subl	$4,%ebp		# U
	ja	ma32_loop	#  V

ma32_done:
	popl	%ebx		# U
	popl	%ebp		#  V
	movl	%edx,%eax	# U
	popl	%edi		#  V
	popl	%esi		# U
	ret			# NP


	.align	align16
lbnMulSub1_32:
_lbnMulSub1_32:
	pushl	%esi		# U
	movl	12(%esp),%esi	#  V	load in
	pushl	%edi		# U
	movl	12(%esp),%edi	#  V	load out
	pushl	%ebp		# U
	movl	24(%esp),%ebp	#  V	load len
	pushl	%ebx		# U
	movl	32(%esp),%ecx	#  V	load k

/* First multiply step has no carry in. */
	movl	(%esi),%eax		#  V
	movl	(%edi),%ebx		# U
	mull	%ecx			# NP	first multiply
	subl	%eax,%ebx		# U
	leal	-4(,%ebp,4),%eax	#  V	loop unrolling
	adcl	$0,%edx			# U
	andl	$12,%eax		#  V	loop unrolling
	movl	%ebx,(%edi)		# U

	addl	%eax,%esi		#  V	loop unrolling
	addl	%eax,%edi		# U	loop unrolling

	jmp	*ms32_jumptable(%eax)	# NP	loop unrolling

	.align	align4
ms32_jumptable:
	.long	ms32_case0
	.long	ms32_case1
	.long	ms32_case2
	.long	ms32_case3

	.align	align8
	nop
	nop
	nop

ms32_case0:
	subl	$4,%ebp		# U
	jbe	ms32_done	#  V

ms32_loop:
	movl	4(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	addl	$16,%esi	# U
	addl	$16,%edi	#  V
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	movl	-12(%edi),%ebx	#  V
	adcl	$0,%edx		# U
	subl	%eax,%ebx	#  V
	adcl	$0,%edx		# U
	movl	%ebx,-12(%edi)	#  V
ms32_case3:
	movl	-8(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	movl	-8(%edi),%ebx	#  V
	adcl	$0,%edx		# U
	subl	%eax,%ebx	#  V
	adcl	$0,%edx		# U
	movl	%ebx,-8(%edi)	#  V
ms32_case2:
	movl	-4(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	movl	-4(%edi),%ebx	#  V
	adcl	$0,%edx		# U
	subl	%eax,%ebx	#  V
	adcl	$0,%edx		# U
	movl	%ebx,-4(%edi)	#  V
ms32_case1:
	movl	(%esi),%eax	# U
	movl	%edx,%ebx	#  V	Remember carry for later
	mull	%ecx		# NP
	addl	%ebx,%eax	# U	Add carry in from previous word
	movl	(%edi),%ebx	#  V
	adcl	$0,%edx		# U
	subl	%eax,%ebx	#  V
	adcl	$0,%edx		# U
	movl	%ebx,(%edi)	#  V

	subl	$4,%ebp		# U
	ja	ms32_loop	#  V

ms32_done:
	popl	%ebx		# U
	popl	%ebp		#  V
	movl	%edx,%eax	# U
	popl	%edi		#  V
	popl	%esi		# U
	ret			# NP

## Two-word by one-word divide.  Stores quotient, returns remainder.
## BNWORD32 lbnDiv21_32(BNWORD32 *q, BNWORD32 nh, BNWORD32 nl, BNWORD32 d)
##                      4            8            12           16

	.align align16
lbnDiv21_32:
_lbnDiv21_32:
	movl	8(%esp),%edx	# U	Load nh
	movl	12(%esp),%eax	#  V	Load nl
	movl	4(%esp),%ecx	# U	Load q
	divl	16(%esp)	# NP
	movl	%eax,(%ecx)	# U	Store quotient
	movl	%edx,%eax	#  V	Return remainder
	ret

## Multi-word by one-word remainder.
## This speeds up key generation.  It's not worth unrolling and so on;
## using 32-bit divides is enough of a speedup.
##
## The modulus (in %ebp) is often 16 bits.  Given that the dividend is 32
## bits, the chances of saving the first divide because the high word of the
## dividend is less than the modulus are low enough it's not worth taking
## the cycles to test for it.
##
## unsigned lbnModQ_32(BNWORD32 const *n, unsigned len, unsigned d)
##                     4                  8             12
	.align align16
lbnModQ_32:
_lbnModQ_32:
	movl	4(%esp),%eax		# U	Load n
	pushl	%ebp			#  V
	movl	12(%esp),%ebp		# U	Load len
	pushl	%esi			#  V
	leal	-4(%eax,%ebp,4),%esi	# U
	movl	20(%esp),%ecx		#  V	Load d
	xorl	%edx,%edx		# U	Clear MSW for first divide
modq32_loop:
	movl	(%esi),%eax		# U
	subl	$4,%esi			#  V
	divl	%ecx			# NP
	decl	%ebp			# U
	jnz	modq32_loop		#  V

	popl	%esi			# U
	movl	%edx,%eax		#  V
	popl	%ebp			# U
	ret				# NP
