;;; Copyright (c) 1995, Colin Plumb.
;;; For licensing and other legal details, see the file legal.c.
;;;
;;; Assembly primitives for bignum library, 80x86 family.
;;;
;;; Several primitives are included here.  Only lbnMulAdd1 is *really*
;;; critical, but once that's written, lnmMul1 and lbnSub1 are quite
;;; easy to write as well, so they are included here as well.
;;; lbnDiv21 and lbnModQ are so easy to write that they're included, too.
;;;
;;; All functions here are for large code, large data.
;;; All use standard "cdecl" calling convention: arguments pushed on the
;;; stack (ss:sp) right to left (the leftmost agrument at the lowest address)
;;; and popped by the caller, return values in ax or dx:ax, and register
;;; usage as follows:
;;;
;;; Callee-save (preserved by callee if needed):
;;;	ss, esp, cs, eip, ds, esi, edi, ebp, high byte of FLAGS except DF,
;;;	all other registers (CRx, DRx, TRx, IDT, GDT, LDT, TR, etc.).
;;; Caller-save (may be corrupted by callee):
;;;	es, eax, ebx, ecx, edx, low byte of flags (SF, ZF, AF, PF, CF)
;;;
;;; The direction flag (DF) is either preserved or cleared.
;;; I'm not sure what the calling convention is for fs and gs.  This
;;; code never alters them.

;; Not all of this code has to be '386 code, but STUPID FUCKING MASM (5.0)
;; gives an error if you change in the middle of a segment.  Rather than
;; fight the thing, just enable '386 instructions everywhere.  (And lose
;; the error checking.)
.386

_TEXT   segment para public use16 'CODE'	; 16-byte aligned because '486 cares
	assume  cs:_TEXT

	public  _lbnMulN1_16
	public  _lbnMulAdd1_16
	public  _lbnMulSub1_16
	public	_lbnDiv21_16
	public	_lbnModQ_16

	public  _lbnMulN1_32
	public  _lbnMulAdd1_32
	public  _lbnMulSub1_32
	public	_lbnDiv21_32
	public	_lbnModQ_32

	public	_not386


;; Prototype:
;; BNWORD16
;; lbnMulAdd_16(BNWORD16 *out, BNWORD16 *in, unsigned len, BNWORD16 k)
;;
;; Multiply len words of "in" by k and add to len words of "out";
;; return the len+1st word of carry.  All pointers are to the least-
;; significant ends of the appropriate arrays.  len is guaraneed > 0.
;;
;; This 16-bit code is optimized for an 8086/80286.  It will not be run
;; on 32-bit processors except for debugging during development.
;;
;; NOTE that it may be possible to assume that the direction flag is clear
;; on entry; this would avoid the need for the cld instructions.  Hoewever,
;; the Microsoft C libraries require that the direction flag be clear.
;; Thus, lbnModQ_16 clears it before returning.
;;
;; Stack frame:
;; +--------+ bp+18
;; |   k    |
;; +--------+ bp+16
;; |  len   |
;; +--------+ bp+14
;; |        |
;; +-  in  -+
;; |        |
;; +--------+ bp+10
;; |        |
;; +- out  -+
;; |        |
;; +--------+ bp+6
;; |        |
;; +-return-+
;; |        |
;; +--------+ bp+2
;; | old bp |
;; +--------+ bp
;;
;; Register usage for lbnMul1_16:
;; ds:[si]	in
;; es:[di]	out
;; bp		k
;; cx		loop counter (len/4)
;; dx,ax	high,low parts of product
;; bx		carry from previous multiply iteration
;;
;; Register usage for lbnMulAdd1_16 and lbnMulSub1_16:
;; ds:[si]	in
;; es:[bx+si]	out
;; bp		k
;; cx		loop counter (len/4)
;; dx,ax	high,low parts of product
;; di		carry from previous multiply iteration
;;
;; The reson for the difference is that straight mul can use stosw, but
;; the multiply and add or multiply and subtract add the result in, so
;; they have to reference es:[di] to add it in.
;;
;; The options are either "add ax,es:[di]; stosw" or "add es:[di],ax;
;; add di,2"; both take 10 cycles on an 80286, 27 on an 8086 and 35 on
;; an 8088 although the former is preferred since it's one byte smaller.
;; However, using [bx+si] is even faster; "add es:[bx+si],ax" takes
;; 7 cycles on an 80286, 25 on an 8086 and 33 on an 8088, as well as
;; being the smallest.  (Of course, stosw, at 3 on an 80286, 11 on an
;; 8086 amd 15 on an 8088 wins easily in the straight multiply case over
;; mov es:[bx+si],ax, which takes 3/18/22 cycles and is larger to boot.)
;;
;; Most of these register assignments are driven by the 8086's instruction
;; set.  The only really practical variation would be to put the multiplier
;; k into bx or di and use bp for carry, but if someone can make a faster
;; Duff's device using a lookup table, bx and di are useful because indexing
;; off them is more flexible than bp.
;;
;; Overview of code:
;;
;; len is guaranteed to be at least 1, so do the first multiply (with no
;; carry in) unconditionally.  Then go to a min loop unrolled 4 times,
;; jumping into the middle using a variant of Duff's device.
;;
;; The loop is constructed using the loop instruction, which does
;; "} while (--cnt)".  This means that we have to divide the count
;; by 4, and increment it so it doesn't start at 0.  To gain a little
;; bit more efficiency, we actually increment the count by 2, so the
;; minimum possible value is 3, which will be shifted down to produce 0.
;; usually in Duff's device, if the number of iterations is a multiple
;; of the unrolling factor, you branch to just before the loop conditional
;; and let it handle the case of 0.  Here, we have a special test for 0
;; at the head of the loop and fall through into the top of the loop
;; if it passes.
;;
;; Basically, with STEP being a multiply step, it's:
;;
;; 	STEP;
;;	count += 2;
;;	mod4 = count % 4;
;;	count /= 4;
;;	switch(mod4) {
;;	  case 3:
;;		if (count) {
;;	  		do {
;;				STEP;
;;	  case 2:
;;				STEP;
;;	  case 1:
;;				STEP;
;;	  case 0:
;;				STEP;
;;			} while (--count);
;;		}
;;	}
;;
;; The switch() is actually done by two levels of branch instructions
;; rather than a lookup table.

_lbnMulN1_16	proc	far

	push	bp
	mov	bp,sp
	push	ds
	push	si
	push	di
	cld

	les	di,[bp+6]	; out
	lds	si,[bp+10]	; in
	mov	cx,[bp+14]	; len
	mov     bp,[bp+16]	; k

;; First multiply step has no carry in
	lodsw
	mul	bp
	stosw

;; The switch() for Duff's device starts here
;; Note: this *is* faster than a jump table for an 8086 and '286.
;; 8086:  jump table: 44 cycles; this: 27/29/31/41
;; 80286: jump table: 25 cycles; this: 17/17/20/22
	shr	cx,1
	jc	SHORT m16_odd

	inc	cx
	shr	cx,1
	jc	SHORT m16_case2
	jmp	SHORT m16_case0

	nop			; To align loop
m16_odd:
	inc	cx
	shr	cx,1
	jnc	SHORT m16_case1
	jz	SHORT m16_done	; Avoid entire loop in this case

m16_loop:
	lodsw
	mov	bx,dx		; Remember carry for later
	mul	bp
	add	ax,bx		; Add carry in from previous word
	adc	dx,0
	stosw
m16_case2:
	lodsw
	mov	bx,dx		; Remember carry for later
	mul	bp
	add	ax,bx		; Add carry in from previous word
	adc	dx,0
	stosw
m16_case1:
	lodsw
	mov	bx,dx		; Remember carry for later
	mul	bp
	add	ax,bx		; Add carry in from previous word
	adc	dx,0
	stosw
m16_case0:
	lodsw
	mov	bx,dx		; Remember carry for later
	mul	bp
	add	ax,bx		; Add carry in from previous word
	adc	dx,0
	stosw

	loop	m16_loop

m16_done:
	mov	ax,dx
	stosw			; Store last word
	pop	di
	pop	si
	pop	ds
	pop	bp
	ret

_lbnMulN1_16	endp


	align	2
_lbnMulAdd1_16	proc	far

	push	bp
	mov	bp,sp
	push	ds
	push	si
	push	di
	cld

	les	bx,[bp+6]	; out
	lds	si,[bp+10]	; in
	mov	cx,[bp+14]	; len
	mov     bp,[bp+16]	; k

;; First multiply step has no carry in
	lodsw
	mul	bp
	add	es:[bx],ax	; This time, store in [bx] directly
	adc	dx,0
	sub	bx,si		; Prepare to use [bx+si].

;; The switch() for Duff's device starts here
;; Note: this *is* faster than a jump table for an 8086 and '286.
;; 8086:  jump table: 44 cycles; this: 27/29/31/41
;; 80286: jump table: 25 cycles; this: 17/17/20/22
	shr	cx,1
	jc	SHORT ma16_odd

	inc	cx
	shr	cx,1
	jc	SHORT ma16_case2
	jmp	SHORT ma16_case0

ma16_odd:
	inc	cx
	shr	cx,1
	jnc	SHORT ma16_case1
	jz	SHORT ma16_done	; Avoid entire loop in this case

ma16_loop:
	lodsw
	mov	di,dx		; Remember carry for later
	mul	bp
	add	ax,di		; Add carry in from previous word
	adc	dx,0
	add	es:[bx+si],ax
	adc	dx,0
ma16_case2:
	lodsw
	mov	di,dx		; Remember carry for later
	mul	bp
	add	ax,di		; Add carry in from previous word
	adc	dx,0
	add	es:[bx+si],ax
	adc	dx,0
ma16_case1:
	lodsw
	mov	di,dx		; Remember carry for later
	mul	bp
	add	ax,di		; Add carry in from previous word
	adc	dx,0
	add	es:[bx+si],ax
	adc	dx,0
ma16_case0:
	lodsw
	mov	di,dx		; Remember carry for later
	mul	bp
	add	ax,di		; Add carry in from previous word
	adc	dx,0
	add	es:[bx+si],ax
	adc	dx,0

	loop	ma16_loop

ma16_done:
	mov	ax,dx
	pop	di
	pop	si
	pop	ds
	pop	bp
	ret

_lbnMulAdd1_16	endp

	align	2
_lbnMulSub1_16	proc	far

	push	bp
	mov	bp,sp
	push	ds
	push	si
	push	di
	cld

	les	bx,[bp+6]	; out
	lds	si,[bp+10]	; in
	mov	cx,[bp+14]	; len
	mov     bp,[bp+16]	; k

;; First multiply step has no carry in
	lodsw
	mul	bp
	sub	es:[bx],ax	; This time, store in [bx] directly
	adc	dx,0
	sub	bx,si		; Prepare to use [bx+si].

;; The switch() for Duff's device starts here
;; Note: this *is* faster than a jump table for an 8086 and '286.
;; 8086:  jump table: 44 cycles; this: 27/29/31/41
;; 80286: jump table: 25 cycles; this: 17/17/20/22
	shr	cx,1
	jc	SHORT ms16_odd

	inc	cx
	shr	cx,1
	jc	SHORT ms16_case2
	jmp	SHORT ms16_case0

ms16_odd:
	inc	cx
	shr	cx,1
	jnc	SHORT ms16_case1
	jz	SHORT ms16_done	; Avoid entire loop in this case

ms16_loop:
	lodsw
	mov	di,dx		; Remember carry for later
	mul	bp
	add	ax,di		; Add carry in from previous word
	adc	dx,0
	sub	es:[bx+si],ax
	adc	dx,0
ms16_case2:
	lodsw
	mov	di,dx		; Remember carry for later
	mul	bp
	add	ax,di		; Add carry in from previous word
	adc	dx,0
	sub	es:[bx+si],ax
	adc	dx,0
ms16_case1:
	lodsw
	mov	di,dx		; Remember carry for later
	mul	bp
	add	ax,di		; Add carry in from previous word
	adc	dx,0
	sub	es:[bx+si],ax
	adc	dx,0
ms16_case0:
	lodsw
	mov	di,dx		; Remember carry for later
	mul	bp
	add	ax,di		; Add carry in from previous word
	adc	dx,0
	sub	es:[bx+si],ax
	adc	dx,0

	loop	ms16_loop

ms16_done:
	mov	ax,dx
	pop	di
	pop	si
	pop	ds
	pop	bp
	ret

_lbnMulSub1_16	endp

;; Two-word by one-word divide.  Stores quotient, returns remainder.
;; BNWORD16 lbnDiv21_16(BNWORD16 *q, BNWORD16 nh, BNWORD16 nl, BNWORD16 d)
;;                      4            8            10           12
	align	2
_lbnDiv21_16	proc	far
	mov	cx,bp		; bp NOT pushed; note change in offsets
	mov	bp,sp
	mov	dx,[bp+8]
	mov	ax,[bp+10]
	div	WORD PTR [bp+12]
	les	bx,[bp+4]
	mov	es:[bx],ax
	mov	ax,dx
	mov	bp,cx
	ret

	nop		; To align loop in lbnModQ properly

_lbnDiv21_16	endp

;; Multi-word by one-word remainder.
;; BNWORD16 lbnModQ_16(BNWORD16 *q, unsigned len, unsigned d)
;;                     6            10            12
_lbnModQ_16	proc	far
	push	bp
	mov	bp,sp
	push	ds
	mov	bx,si
	mov	cx,10[bp]	; load len
	lds	si,6[bp]	; load q
	std			; loop MSW to LSW
	add	si,cx
	mov	bp,12[bp]	; load d
	add	si,cx
	xor	dx,dx		; Set up for first divide
	sub	si,2		; Adjust pointer to point to MSW

	lodsw			; Load first word

	cmp	ax,bp		; See if we can skip first divide
	jnc	SHORT modq16_inner	; No such luck
	mov	dx,ax		; Yes!  Modulus > input, so remainder = input
	dec	cx		; Do loop
	jz	SHORT modq16_done

modq16_loop:
	lodsw
modq16_inner:
	div	bp
	loop	modq16_loop
modq16_done:
	pop	ds
	mov	ax,dx	; Return remainder
	pop	bp
	mov	si,bx
	cld		; Microsoft C's libraries assume this
	ret

_lbnModQ_16	endp


;; Similar, but using 32-bit operations.
;;
;; The differences are that the switch() in Duff's device is done using
;; a jump table, and lods is not used because it's slower than load and
;; increment.  The pointers are only updated once per loop; offset
;; addressing modes are used, since they're no slower.  [di] is used
;; instead of [bx+si] because the extra increment of di take only one
;; cycle per loop a '486, while [bx+si] takes one extra cycle per multiply.
;;
;; The register assignments are also slightly different:
;;
;; es:[si]	in
;; ds:[di]	out
;; ecx		k
;; bp		loop counter (len/4)
;; edx,eax	high,low parts of product
;; ebx		carry word from previous multiply iteration
;;
;; The use of bp for a loop counter lets all the 32-bit values go
;; in caller-save registers, so there's no need to do any 32-bit
;; saves and restores.  Using ds:di for the destination saves one
;; segment override in the lbnMulN1_32 code, since there's one more
;; store to [di] than load from es:[si].
;;
;; Given the number of 32-bit references that this code uses, optimizing
;; it for the Pentium is interesting, because the Pentium has a very
;; inefficient implementation of prefix bytes.  Each prefix byte, with
;; the exception of 0x0f *>> on conditional branch instructions ONLY <<*
;; is a 1-cycle non-pairiable instruction.  Which has the effect of
;; forcing the instruction it's on into the U pipe.  But this code uses
;; *lots* of prefix bytes, notably the 0x66 operand size override.
;;
;; For example "add [di],eax" is advised against in Intel's optimization
;; papers, because it takes 3 cycles and 2 of them are not pairable.
;; But any longer sequence would have a prefix byte on every instruction,
;; resulting in even more non-pairable cycles.  Also, only two instructions
;; in the multiply kernel can go in the V pipe (the increments of si and
;; di), and they're already there, so the pairable cycles would be wasted.
;;
;; Things would be *quite* different in native 32-bit mode.
;;
;; All instructions that could go in the V pipe that aren't there are
;; marked.
;;
;; The setup code is quite intricately interleaved to get the best possible
;; performance out of a Pentium.  If you want to follow the code,
;; pretend that the sections actually come in the following order:
;; 1) prologue (push registers)
;; 2) load (fetch arguments)
;; 3) first multiply
;; 4) loop unrolling
;;
;; The loop unrolling setup consists of taking the count, adjusting
;; it to account for the first multiply, and splitting it into
;; two parts: the high bits are a loop count, while the low bits are
;; used to find the right entry in the Duff's device jump table and
;; to adjust the initial data pointers.
;;
;; Known slack: There is one instruction in the prologue and one in
;; the epilogue that could go in the V pipe if I could find a U-pipe
;; instruction to pair them with, but all the U-pipe instructions
;; are already paired, so it looks difficult.
;;
;; There is a cycle of Address Generation Interlock in the lbnMulN1_32
;; code on the Pentium (not on a '486).  I can't figure out how to
;; get rid of it without wasting time elsewhere.  The problem is that
;; the load of bx needs to be done as soon as possible to let it
;; be set up in time for the switch().  The other problem is the
;; epilogue code which can waste time if the order of the pushed
;; registers is diddled with so that ds doesn't come between si and di.
;;
;; The increment of si after the last load is redundant, and the
;; copy of the high word of the product to the carry after the last
;; multiply is likewise unnecessary.
;;
;; In these cases, the operations were done that way in order to remove
;; cycles from the loop on the '486 and/or Pentium, even though it costs
;; a few overhead cycles on a '386.
;; The increment fo si has to be done early because a load based on si
;; is the first thing in any given multiply step, and the address
;; generation interlock on the '486 and Pentium requires that a full
;; cycle (i.e. possibly two instructions on a Pentium) pass between
;; incrementing a register and using it in an address.
;; This saves one cycle per multiply on a '486 and Pentium, and costs
;; 2 cycles per call to the function on a '386 and 1 cycle on a '486.
;;
;; The carry word is copied where it is so that the decrement of the loop
;; counter happens in the V pipe.  The instruction between the decrement
;; of the loop counter and the branch should be a U-pipe instruction that
;; doesn't affect the flags.  Thus, the "mov" was rotated down from
;; the top of the loop to fill the slot.
;; This is a bit more marginal: it saves one cycle per loop iteration on
;; a Pentium, and costs 2 cycles per call on a '386, '486 or Pentium.
;;
;; The same logic applies to the copy of the carry and increment of si
;; before the test, in case 0, for skipping the loop entirely.
;; It makes no difference in speed if the loop is executed, but
;; incrementing si before saves an address generation interlock cycle
;; On a '486 and Pentium in the case that the loop is executed.
;; And the loop is executed more often than not.
;;
;; Given that just one multiply on a '386 takes 12 to 41 cycles (with the
;; average being very much at the high end of that) 4 cycles of additional
;; overhead per call is not a big deal.
;;
;; On a Pentium, it would actually be easier to *not* unroll the loop
;; at all, since the decrement and compare are completely hidden
;; in the V-pipe and it wouldn't cost anything to do them more often.
;; That would save the setup for the unrolling and Duff's device at the
;; beginning.  But the overhead for that is pretty minor: ignoring what's
;; hidden in the V pipe, it's two cycles plus the indirect jump.
;; Not too much, and special-casing the pentium is quite a hassle.
;; (For starters, you have to detect it, and since you're probably in
;; V86 mode, without access to the EFLAGS register to test the CPUID bit.)


	align	16
_lbnMulN1_32	proc	far

	push	bp		; U	prologue	** Could be V
	mov	bp,sp		; V	prologue
	push	si		; U	prologue	** Could be V
	mov	bx,[bp+14]	; U	load len	** Could be V (AGI!)r
	push	ds		; NP	prologue
	les	si,[bp+10]	; NP	load in
	mov     ecx,[bp+16]	; U	load k
	dec	bx		; V	loop unrolling
	shl	bx,2		; U	loop unrolling
	push	di		; V	prologue
	lds	di,[bp+6]	; NP	load out
	mov	bp,bx		; U	loop unrolling	** Could be V
	and	bx,12		; V	loop unrolling

;; First multiply step has no carry in.
	mov	eax,es:[si]	; U	first multiply
	add	si,bx		; V	loop unrolling
	mul	ecx		; NP	first multiply
	mov	[di],eax	; U	first multiply
	add	di,bx		; V	loop unrolling

;; The switch() for Duff's device.  This jump table is (slightly!) faster
;; than a bunch of branches on a '386 and '486, and is probably better yet
;; on higher processors.
	jmp	WORD PTR cs:m32_jumptable[bx]	; NP	loop unrolling
	align 2
m32_jumptable:
	dw	OFFSET m32_case0, 0
	dw	OFFSET m32_case1, 0
	dw	OFFSET m32_case2, 0
	dw	OFFSET m32_case3, 0, 0, 0, 0	; Get loop aligned properly

m32_case0:
	add	si,16		; U	Fix up si	** Could be V
	test	bp,bp		; V
	mov	ebx,edx		; U	Remember carry for later
	jbe	SHORT m32_done	; V	Avoid entire loop if loop count is 0

m32_loop:
	mov	eax,es:[si-12]	; U
	add	di, 16		; V
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	mov	[di-12],eax	; U
m32_case3:
	mov	ebx,edx		; U	Remember carry for later
	mov	eax,es:[si-8]	; U
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	mov	[di-8],eax	; U
m32_case2:
	mov	ebx,edx		; U	Remember carry for later
	mov	eax,es:[si-4]	; U
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	mov	[di-4],eax	; U
m32_case1:
	mov	ebx,edx		; U	Remember carry for later
	mov	eax,es:[si]	; U
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	add	si,16  		; V
	mov	[di],eax	; U

	sub	bp,16		; V
	mov	ebx,edx		; U	Remember carry for later
	ja	m32_loop	; V

m32_done:
	mov	[di+4],edx	; U
	pop	di		; V
	pop	ds		; NP
	pop	si		; U	** Could be V
	pop	bp		; V
	ret			; NP

_lbnMulN1_32	endp


	align	16
_lbnMulAdd1_32	proc	far

	push	bp		; U	prologue	** Could be V
	mov	bp,sp		; V	prologue
	push	ds		; NP	prologue

	mov     ecx,[bp+16]	; U	load k
	mov	bx,[bp+14]	; V	load len
	push	di		; U	prologue	** Could be V
	dec	bx		; V	loop unrolling
	lds	di,[bp+6]	; NP	load out
	shl	bx,2		; U	loop unrolling
	push	si		; V	prologue
	les	si,[bp+10]	; NP	load in

	mov	bp,bx		; U	loop unrolling	** Could be V
	and	bx,12		; V	loop unrolling

;; First multiply step has no carry in.
	mov	eax,es:[si]	; U	first multiply
	add	si,bx		; V	loop unrolling
	mul	ecx		; NP	first multiply
	add	[di],eax	; U	first multiply
	adc	edx,0		; U	first multiply
	add	di,bx		; V	loop unrolling

;; The switch() for Duff's device.  This jump table is (slightly!) faster
;; than a bunch of branches on a '386 and '486, and is probably better yet
;; on higher processors.
	jmp	WORD PTR cs:ma32_jumptable[bx]	; NP	loop unrolling
	align 2
ma32_jumptable:
	dw	OFFSET ma32_case0, 0
	dw	OFFSET ma32_case1, 0
	dw	OFFSET ma32_case2, 0
	dw	OFFSET ma32_case3, 0, 0	; To get loop aligned properly

ma32_case0:
	add	si,16		; U	Fix up si	** Could be V
	test	bp,bp		; V
	mov	ebx,edx		; U	Remember carry for later
	jbe	SHORT ma32_done	; V	Avoid entire loop if loop count is 0

ma32_loop:
	mov	eax,es:[si-12]	; U
	add	di, 16		; V
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	add	[di-12],eax	; U
	adc	edx,0		; U
ma32_case3:
	mov	ebx,edx		; U	Remember carry for later
	mov	eax,es:[si-8]	; U
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	add	[di-8],eax	; U
	adc	edx,0		; U
ma32_case2:
	mov	ebx,edx		; U	Remember carry for later
	mov	eax,es:[si-4]	; U
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	add	[di-4],eax	; U
	adc	edx,0		; U
ma32_case1:
	mov	ebx,edx		; U	Remember carry for later
	mov	eax,es:[si]	; U
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	add	si,16  		; V
	add	[di],eax	; U
	adc	edx,0		; U

	sub	bp,16		; V
	mov	ebx,edx		; U	Remember carry for later
	ja	ma32_loop	; V

ma32_done:
	pop	si	; U	** Could be V
	pop	di	; V
	mov	ax,dx	; U	return value low	** Could be V
	pop	ds	; NP
	shr	edx,16	; U	return value high
	pop	bp	; V
	ret		; NP

_lbnMulAdd1_32	endp


	align	16
_lbnMulSub1_32	proc	far

	push	bp		; U	prologue	** Could be V
	mov	bp,sp		; V	prologue
	push	ds		; NP	prologue

	mov     ecx,[bp+16]	; U	load k
	mov	bx,[bp+14]	; V	load len
	push	di		; U	prologue	** Could be V
	dec	bx		; V	loop unrolling
	lds	di,[bp+6]	; NP	load out
	shl	bx,2		; U	loop unrolling
	push	si		; V	prologue
	les	si,[bp+10]	; NP	load in

	mov	bp,bx		; U	loop unrolling	** Could be V
	and	bx,12		; V	loop unrolling

;; First multiply step has no carry in.
	mov	eax,es:[si]	; U	first multiply
	add	si,bx		; V	loop unrolling
	mul	ecx		; NP	first multiply
	sub	[di],eax	; U	first multiply
	adc	edx,0		; U	first multiply
	add	di,bx		; V	loop unrolling

;; The switch() for Duff's device.  This jump table is (slightly!) faster
;; than a bunch of branches on a '386 and '486, and is probably better yet
;; on higher processors.
	jmp	WORD PTR cs:ms32_jumptable[bx]	; NP	loop unrolling
	align 2
ms32_jumptable:
	dw	OFFSET ms32_case0, 0
	dw	OFFSET ms32_case1, 0
	dw	OFFSET ms32_case2, 0
	dw	OFFSET ms32_case3, 0, 0	; To get loop aligned properly

ms32_case0:
	add	si,16		; U	Fix up si	** Could be V
	test	bp,bp		; V
	mov	ebx,edx		; U	Remember carry for later
	jbe	SHORT ms32_done	; V	Avoid entire loop if loop count is 0

ms32_loop:
	mov	eax,es:[si-12]	; U
	add	di, 16		; V
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	sub	[di-12],eax	; U
	adc	edx,0		; U
ms32_case3:
	mov	ebx,edx		; U	Remember carry for later
	mov	eax,es:[si-8]	; U
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	sub	[di-8],eax	; U
	adc	edx,0		; U
ms32_case2:
	mov	ebx,edx		; U	Remember carry for later
	mov	eax,es:[si-4]	; U
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	sub	[di-4],eax	; U
	adc	edx,0		; U
ms32_case1:
	mov	ebx,edx		; U	Remember carry for later
	mov	eax,es:[si]	; U
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	add	si,16  		; V
	sub	[di],eax	; U
	adc	edx,0		; U

	sub	bp,16		; V
	mov	ebx,edx		; U	Remember carry for later
	ja	ms32_loop	; V

ms32_done:
	pop	si	; U	** Could be V
	pop	di	; V
	mov	ax,dx	; U	return value low	** Could be V
	pop	ds	; NP
	shr	edx,16	; U	return value high
	pop	bp	; V
	ret		; NP

_lbnMulSub1_32	endp



;; Just for interest's sake, here's a completely Pentium-optimized version.
;; In addition to being smaller, it takes 8 + (8+mul_time)*n cycles, as
;; compared to the 10 + jmp_time + (8+mul_time)*n cycles for the loop above.
;; (I don't know how long a 32x32->64 bit multiply or an indirect jump
;; take on a Pentium, so plug those numbers in.)
;	align	2
;	nop	; To align loop nicely
;P_lbnMulAdd1_32	proc	far
;
;	push	bp		; U	prologue	** Could be V
;	mov	bp,sp		; V	prologue
;	push	ds		; NP	prologue
;	mov     ecx,[bp+16]	; U	load k
;	push	si		; V	prologue
;	lds	si,[bp+10]	; NP	load in
;	mov	eax,[si]	; U	first multiply
;	push	di		; V	prologue
;	mul	ecx		; NP	first multiply
;	les	di,[bp+6]	; NP	load out
;	add	es:[di],eax	; U	first multiply
;	mov	bp,[bp+14]	; V	load len
;	adc	edx,0		; U	first multiply
;	dec     bp		; V
;	mov	ebx,edx		; U	Remember carry for later
;	je	Pma32_done	; V
;Pma32_loop:
;	mov	eax,[si+4]	; U
;	add	di,4		; V
;	mul	ecx		; NP
;	add	eax,ebx		; U	Add carry in from previous word
;	adc	edx,0		; U
;	add	si,4		; V
;	add	es:[di],eax	; U
;	adc	edx,0		; U
;	dec	bp		; V
;	mov	ebx,edx		; U	Remember carry for later
;	jne	Pma32_loop	; V
;Pma32_done:
;	pop	di	; U	** Could be V
;	pop	si	; V
;	pop	ds	; NP
;	mov	ax,dx	; U	return value low	** Could be V
;	pop	bp	; V
;	shr	edx,16	; U	return value high
;	ret		; NP
;
;P_lbnMulAdd1_32	endp



;; Two-word by one-word divide.  Stores quotient, returns remainder.
;; BNWORD32 lbnDiv21_32(BNWORD32 *q, BNWORD32 nh, BNWORD32 nl, BNWORD32 d)
;;                      4            8            12           16
	align	16
_lbnDiv21_32	proc	far
	mov	cx,bp			; U	bp NOT pushed; offsets differ
	mov	bp,sp			; V
					; AGI
	mov	edx,[bp+8]		; U
	mov	eax,[bp+12]		; U
	div	DWORD PTR [bp+16]	; NP
	les	bx,[bp+4]		; NP
	mov	es:[bx],eax		; U
	mov	ax,dx			; V
	shr	edx,16			; U
	mov	bp,cx			; V
	ret				; NP

	nop
	nop
	nop
	nop				; Get lbnModQ_32 aligned properly

_lbnDiv21_32	endp

;; Multi-word by one-word remainder.
;; This speeds up key generation.  It's not worth unrolling and so on;
;; using 32-bit divides is enough of a speedup.
;;
;; bp is used as a counter so that all the 32-bit values can be in
;; caller-save registers (eax, ecx, edx).  bx is needed as a pointer.
;;
;; The modulus (in ebp) is 16 bits.  Given that the dividend is 32 bits,
;; the chances of saving the first divide because the high word of the
;; dividend is less than the modulus are low enough it's not worth taking
;; the cycles to test for it.
;;
;; unsigned lbnModQ_32(BNWORD16 *q, unsigned len, unsigned d)
;;                     6            10            12
_lbnModQ_32	proc	far
	xor	ecx,ecx		; U	Clear ecx (really, the high half)
	push	bp		; V
	mov	edx,ecx		; U	Clear high word for first divide
	mov	bp,sp		; V
	push	ds		; NP
	lds	ax,[bp+6]	; NP	Load dividend pointer
	mov	bx,[bp+10]	; U	Load count	** Could be V
	sub	ax,4		; V	Offset dividend pointer
	mov	cx,[bp+12]	; U	Load modulus	** Could be V
	mov	bp,bx		; V	Copy count
	shl	bx,2		; U	Shift index
	add	bx,ax		; U	Add base	** Could be V
;	lea	bx,[eax+ebp*4-4]; U	Move pointer to high word

modq32_loop:
	mov	eax,[bx]	; U
	sub	bx,4		; V
	div	ecx		; NP
	dec	bp		; U	** Could be V
	jnz	modq32_loop	; V
modq32_done:
	pop	ds		; NP
	mov	ax,dx		; U	** Could be V
	pop	bp		; V
	ret			; NP

_lbnModQ_32	endp


;; int not386(void) returns 0 on a 32-bit (386 or better) processor;
;; non-zero if an 80286 or lower.  The Z flag is set to reflect
;; ax on return.  This is only called once, so it doesn't matter how
;; it's aligned.

_not386 proc	far
;;
;; This first test detects 80x86 for x < 2.  On the 8086 and '186,
;; "push sp" does "--sp; sp[0] = sp".  On all later processors, it does
;; "sp[-1] = sp; --sp".
;;
	push	sp
	pop	ax
	sub	ax,sp
	jne	SHORT return

;; This test is the key one.  It will probably detect 8086, V30 and 80186
;; as well as 80286, but I haven't had access to test it on any of those,
;; so it's protected by the well-known test above.  It has been tested
;; on the 80286, 80386, 80486, Pentium and AMD tested it on their K5.
;; I have not been able to confirm effectiveness on the P6 yet, although
;; someone I spoke to at Intel said it should work.
;;
;; This test uses the fact that the '386 and above have a barrel shifter
;; to do shifts, while the '286 does left shifts by releated adds.
;; That means that on the '286, the auxilliary carry gets a copy of
;; bit 4 of the shift output, while on the '386 and up, it's trashed
;; (as it happens, set to 1) independent of the result.  (It's documented
;; as undefined.)
;;
;; We do two shifts, which should produce different auxilliary carries
;; on a '286 and XOR them to see if they are different.  Even on a
;; future processor that does something different with the aux carry
;; flag, it probably does something data-independent, so this will still
;; work.  Note that all flags except aux carry are defined for shl
;; output and will be the same for both cases.

	mov	al,4
	shl	al,1	; Expected to produce ac = 0 on a '286
	lahf
	shl	al,1	; Expected to produce ac = 1 on a '286
	mov	al,ah
	lahf
	xor	al,ah	; Xor the flags together to detect the difference
	mov	ah,al	; Clear ah if al is clear, leave Z flag alone
return:
	ret

_not386	endp

_TEXT	ends

	end
