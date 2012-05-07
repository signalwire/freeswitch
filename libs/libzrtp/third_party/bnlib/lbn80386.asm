;;; Copyright (c) 1995, Colin Plumb.
;;; For licensing and other legal details, see the file legal.c.
;;;
;;; Assembly primitives for bignum library, 80386 family, 32-bit code.
;;;
;;; Several primitives are included here.  Only lbnMulAdd1 is *really*
;;; critical, but once that's written, lnmMulN1 and lbnMulSub1 are quite
;;; easy to write as well, so they are included here as well.
;;; lbnDiv21 and lbnModQ are so easy to write that they're included, too.
;;;
;;; All functions here are for 32-bit flat mode.  I.e. near code and
;;; near data, although the near offsets are 32 bits.
;;;
;;; The usual 80x86 calling conventions have AX, BX, CX and DX
;;; volatile, and SI, DI, SP and BP preserved across calls.
;;; This includes the "E"xtended forms of all of those registers
;;; 
;;; However, just to be confusing, recent 32-bit DOS compilers have
;;; quietly changed that to require EBX preserved across calls, too.
;;; Joy.

.386
;_TEXT   segment para public use32 'CODE' ; 16-byte aligned because 486 cares
;_TEXT	ends

ifdef @Version
if @Version le 510
FLAT	group	_TEXT
endif
else
FLAT	group	_TEXT
endif
	assume	cs:FLAT, ds:FLAT, ss:FLAT
_TEXT   segment para public use32 'CODE' ; 16-byte aligned because 486 cares

	public  _lbnMulN1_32
	public  _lbnMulAdd1_32
	public  _lbnMulSub1_32
	public	_lbnDiv21_32
	public	_lbnModQ_32

;; Register usage:
;; eax - low half of product
;; ebx - carry to next iteration
;; ecx - multiplier (k)
;; edx - high half of product
;; esi - source pointer
;; edi - dest pointer
;; ebp - loop counter
;;
;; Stack frame:
;; +--------+ esp+20  esp+24  esp+28  esp+32  esp+36
;; |    k   |
;; +--------+ esp+16  esp+20  esp+24  esp+28  esp+32
;; |   len  |
;; +--------+ esp+12  esp+16  esp+20  esp+24  esp+28
;; |   in   |
;; +--------+ esp+8   esp+12  esp+16  esp+20  esp+24
;; |   out  |
;; +--------+ esp+4   esp+8   esp+12  esp+16  esp+20
;; | return |
;; +--------+ esp     esp+4   esp+8   esp+12  esp+16
;; |   esi  |
;; +--------+         esp     esp+4   esp+8   esp+12
;; |   ebp  |
;; +--------+                 esp     esp+4   esp+8
;; |   ebx  |
;; +--------+                         esp     esp+4
;; |   edi  |
;; +--------+                                 esp

	align	16
_lbnMulN1_32	proc	near

	push	esi		; U
	mov	esi,[esp+12]	;  V	load in
	push	ebp		; U
	mov	ebp,[esp+20]	;  V	load len
	push	ebx		; U
	mov	ecx,[esp+28]	;  V	load k
	push	edi		; U
	mov	edi,[esp+20]	;  V	load out

;; First multiply step has no carry in.
	mov	eax,[esi]	; U
	lea	ebx,[ebp*4-4]	;  V	loop unrolling
	mul	ecx		; NP	first multiply
	mov	[edi],eax	; U
	and	ebx,12		;  V	loop unrolling

	add	esi,ebx		; U	loop unrolling
	add	edi,ebx		;  V	loop unrolling

	jmp	DWORD PTR m32_jumptable[ebx]	; NP	loop unrolling

	align	4
m32_jumptable:
	dd	m32_case0
	dd	m32_case1
	dd	m32_case2
	dd	m32_case3

	nop
	align	8
	nop
	nop
	nop	; Get loop nicely aligned

m32_case0:
	sub	ebp,4		; U
	jbe	SHORT m32_done	;  V

m32_loop:
	mov	eax,[esi+4]	; U
	mov	ebx,edx		;  V	Remember carry for later
	add	esi,16		; U
	add	edi,16		;  V
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	mov	[edi-12],eax	;  V
m32_case3:
	mov	eax,[esi-8]	; U
	mov	ebx,edx		;  V	Remember carry for later
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	mov	[edi-8],eax	;  V
m32_case2:
	mov	eax,[esi-4]	; U
	mov	ebx,edx		;  V	Remember carry for later
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	mov	[edi-4],eax	;  V
m32_case1:
	mov	eax,[esi]	; U
	mov	ebx,edx		;  V	Remember carry for later
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	adc	edx,0		; U
	mov	[edi],eax	;  V

	sub	ebp,4		; U
	ja	SHORT m32_loop	;  V

m32_done:
	mov	[edi+4],edx	; U
	pop	edi		;  V
	pop	ebx		; U
	pop	ebp		;  V
	pop	esi		; U
	ret			; NP
_lbnMulN1_32	endp


	align	16
_lbnMulAdd1_32	proc	near

	push	esi		; U
	mov	esi,[esp+12]	;  V	load in
	push	edi		; U
	mov	edi,[esp+12]	;  V	load out
	push	ebp		; U
	mov	ebp,[esp+24]	;  V	load len
	push	ebx		; U
	mov	ecx,[esp+32]	;  V	load k

;; First multiply step has no carry in.
	mov	eax,[esi]	; U
	mov	ebx,[edi]	;  V
	mul	ecx		; NP	first multiply
	add	ebx,eax		; U
	lea	eax,[ebp*4-4]	;  V	loop unrolling
	adc	edx,0		; U
	and	eax,12		;  V	loop unrolling
	mov	[edi],ebx	; U

	add	esi,eax		;  V	loop unrolling
	add	edi,eax		; U	loop unrolling

	jmp	DWORD PTR ma32_jumptable[eax]	; NP	loop unrolling

	align	4
ma32_jumptable:
	dd	ma32_case0
	dd	ma32_case1
	dd	ma32_case2
	dd	ma32_case3

	nop
	align	8
	nop
	nop
	nop			; To align loop properly


ma32_case0:
	sub	ebp,4		; U
	jbe	SHORT ma32_done	;  V

ma32_loop:
	mov	eax,[esi+4]	; U
	mov	ebx,edx		;  V	Remember carry for later
	add	esi,16		; U
	add	edi,16		;  V
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	mov	ebx,[edi-12]	;  V
	adc	edx,0		; U
	add	ebx,eax		;  V
	adc	edx,0		; U
	mov	[edi-12],ebx	;  V
ma32_case3:
	mov	eax,[esi-8]	; U
	mov	ebx,edx		;  V	Remember carry for later
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	mov	ebx,[edi-8]	;  V
	adc	edx,0		; U
	add	ebx,eax		;  V
	adc	edx,0		; U
	mov	[edi-8],ebx	;  V
ma32_case2:
	mov	eax,[esi-4]	; U
	mov	ebx,edx		;  V	Remember carry for later
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	mov	ebx,[edi-4]	;  V
	adc	edx,0		; U
	add	ebx,eax		;  V
	adc	edx,0		; U
	mov	[edi-4],ebx	;  V
ma32_case1:
	mov	eax,[esi]	; U
	mov	ebx,edx		;  V	Remember carry for later
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	mov	ebx,[edi]	;  V
	adc	edx,0		; U
	add	ebx,eax		;  V
	adc	edx,0		; U
	mov	[edi],ebx	;  V

	sub	ebp,4		; U
	ja	SHORT ma32_loop	;  V

ma32_done:
	pop	ebx		; U
	pop	ebp		;  V
	mov	eax,edx		; U
	pop	edi		;  V
	pop	esi		; U
	ret			; NP
_lbnMulAdd1_32	endp


	align	16
_lbnMulSub1_32	proc	near
	push	esi		; U
	mov	esi,[esp+12]	;  V	load in
	push	edi		; U
	mov	edi,[esp+12]	;  V	load out
	push	ebp		; U
	mov	ebp,[esp+24]	;  V	load len
	push	ebx		; U
	mov	ecx,[esp+32]	;  V	load k

;; First multiply step has no carry in.
	push	esi		; U
	mov	esi,[esp+12]	;  V	load in
	push	edi		; U
	mov	edi,[esp+12]	;  V	load out
	push	ebp		; U
	mov	ebp,[esp+24]	;  V	load len
	mov	ecx,[esp+28]	; U	load k

;; First multiply step has no carry in.
	mov	eax,[esi]	;  V
	mov	ebx,[edi]	; U
	mul	ecx		; NP	first multiply
	sub	ebx,eax		; U
	lea	eax,[ebp*4-4]	;  V	loop unrolling
	adc	edx,0		; U
	and	eax,12		;  V	loop unrolling
	mov	[edi],ebx	; U

	add	esi,eax		;  V	loop unrolling
	add	edi,eax		; U	loop unrolling

	jmp	DWORD PTR ms32_jumptable[eax]	; NP	loop unrolling

	align	4
ms32_jumptable:
	dd	ms32_case0
	dd	ms32_case1
	dd	ms32_case2
	dd	ms32_case3

	nop
	align	8
	nop
	nop
	nop

ms32_case0:
	sub	ebp,4		; U
	jbe	SHORT ms32_done	;  V

ms32_loop:
	mov	eax,[esi+4]	; U
	mov	ebx,edx		;  V	Remember carry for later
	add	esi,16		; U
	add	edi,16		;  V
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	mov	ebx,[edi-12]	;  V
	adc	edx,0		; U
	sub	ebx,eax		;  V
	adc	edx,0		; U
	mov	[edi-12],ebx	;  V
ms32_case3:
	mov	eax,[esi-8]	; U
	mov	ebx,edx		;  V	Remember carry for later
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	mov	ebx,[edi-8]	;  V
	adc	edx,0		; U
	sub	ebx,eax		;  V
	adc	edx,0		; U
	mov	[edi-8],ebx	;  V
ms32_case2:
	mov	eax,[esi-4]	; U
	mov	ebx,edx		;  V	Remember carry for later
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	mov	ebx,[edi-4]	;  V
	adc	edx,0		; U
	sub	ebx,eax		;  V
	adc	edx,0		; U
	mov	[edi-4],ebx	;  V
ms32_case1:
	mov	eax,[esi]	; U
	mov	ebx,edx		;  V	Remember carry for later
	mul	ecx		; NP
	add	eax,ebx		; U	Add carry in from previous word
	mov	ebx,[edi]	;  V
	adc	edx,0		; U
	sub	ebx,eax		;  V
	adc	edx,0		; U
	mov	[edi],ebx	;  V

	sub	ebp,4		; U
	ja	SHORT ms32_loop	;  V

ms32_done:
	pop	ebx		; U
	pop	ebp		;  V
	mov	eax,edx		; U
	pop	edi		;  V
	pop	esi		; U
	ret			; NP
_lbnMulSub1_32	endp



;; Two-word by one-word divide.  Stores quotient, returns remainder.
;; BNWORD32 lbnDiv21_32(BNWORD32 *q, BNWORD32 nh, BNWORD32 nl, BNWORD32 d)
;;                      4            8            12           16
align 4
_lbnDiv21_32	proc	near
	mov	edx,[esp+8]		; U	Load nh
	mov	eax,[esp+12]		;  V	Load nl
	mov	ecx,[esp+4]		; U	Load q
	div	DWORD PTR [esp+16]	; NP
	mov	[ecx],eax		; U	Store quotient
	mov	eax,edx			;  V	Return remainder
	ret
_lbnDiv21_32	endp

;; Multi-word by one-word remainder.
;; This speeds up key generation.  It's not worth unrolling and so on;
;; using 32-bit divides is enough of a speedup.
;;
;; The modulus (in ebp) is often 16 bits.  Given that the dividend is 32
;; bits, the chances of saving the first divide because the high word of the
;; dividend is less than the modulus are low enough it's not worth taking
;; the cycles to test for it.
;;
;; unsigned lbnModQ_32(BNWORD32 const *n, unsigned len, unsigned d)
;;                     4                  8             12
align 4
_lbnModQ_32	proc	near
	mov	eax,[esp+4]		; U	Load n
	push	ebp			;  V
	mov	ebp,[esp+12]		; U	Load len
	push	esi			;  V
	lea	esi,[ebp*4+eax-4]	; U
	mov	ecx,[esp+20]		;  V	Load d
	xor	edx,edx			; U	Clear edx for first iteration
modq32_loop:
	mov	eax,[esi]		; U	Load new low word for divide
	sub	esi,4			;  V
	div	ecx			; NP	edx = edx:eax % ecx
	dec	ebp			; U
	jnz	SHORT modq32_loop	;  V

	pop	esi			; U
	mov	eax,edx			;  V	Return remainder in eax
	pop	ebp			; U
	ret				; NP
_lbnModQ_32	endp

	end
