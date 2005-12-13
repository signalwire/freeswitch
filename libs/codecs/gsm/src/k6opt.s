	.file	"k6opt.s"
	.version	"01.01"
/* gcc2_compiled.: */
.section	.rodata
	.align 4
	.type	 coefs,@object
	.size	 coefs,24
coefs:
	.value -134
	.value -374
	.value 0
	.value 2054
	.value 5741
	.value 8192
	.value 5741
	.value 2054
	.value 0
	.value -374
	.value -134
	.value 0
.text
	.align 4
/* void Weighting_filter (const short *e, short *x) */
.globl Weighting_filter
	.type	 Weighting_filter,@function
Weighting_filter:
	pushl %ebp
	movl %esp,%ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 12(%ebp),%edi
	movl 8(%ebp),%ebx
	addl $-10,%ebx
	emms
	movl $0x1000,%eax; movd %eax,%mm5  /* for rounding */
	movq coefs,%mm1
	movq coefs+8,%mm2
	movq coefs+16,%mm3
	xorl %esi,%esi
	.p2align 2
.L21:
	movq (%ebx,%esi,2),%mm0
	pmaddwd %mm1,%mm0

	movq 8(%ebx,%esi,2),%mm4
	pmaddwd %mm2,%mm4
	paddd %mm4,%mm0

	movq 16(%ebx,%esi,2),%mm4
	pmaddwd %mm3,%mm4
	paddd %mm4,%mm0

	movq %mm0,%mm4
	punpckhdq %mm0,%mm4  /* mm4 has high int32 of mm0 dup'd */
	paddd %mm4,%mm0;

	paddd %mm5,%mm0 /* add for roundoff */
	psrad $13,%mm0
	packssdw %mm0,%mm0	
	movd %mm0,%eax  /* ax has result */
	movw %ax,(%edi,%esi,2)
	incl %esi
	cmpl $39,%esi
	jle .L21
	emms
	popl %ebx
	popl %esi
	popl %edi
	leave
	ret
.Lfe1:
	.size	 Weighting_filter,.Lfe1-Weighting_filter

.macro ccstep n
.if \n
	movq \n(%edi),%mm1
	movq \n(%esi),%mm2
.else
	movq (%edi),%mm1
	movq (%esi),%mm2
.endif
	pmaddwd %mm2,%mm1
	paddd %mm1,%mm0
.endm

	.align 4
/* long k6maxcc(const short *wt, const short *dp, short *Nc_out) */
.globl k6maxcc
	.type	 k6maxcc,@function
k6maxcc:
	pushl %ebp
	movl %esp,%ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	emms
	movl 8(%ebp),%edi
	movl 12(%ebp),%esi
	movl $0,%edx  /* will be maximum inner-product */
	movl $40,%ebx
	movl %ebx,%ecx /* will be index of max inner-product */
	subl $80,%esi
	.p2align 2
.L41:
	movq (%edi),%mm0
	movq (%esi),%mm2
	pmaddwd %mm2,%mm0
	ccstep 8
	ccstep 16
	ccstep 24
	ccstep 32
	ccstep 40
	ccstep 48
	ccstep 56
	ccstep 64
	ccstep 72

	movq %mm0,%mm1
	punpckhdq %mm0,%mm1  /* mm1 has high int32 of mm0 dup'd */
	paddd %mm1,%mm0;
	movd %mm0,%eax  /* eax has result */

	cmpl %edx,%eax
	jle .L40
	movl %eax,%edx
	movl %ebx,%ecx
	.p2align 2
.L40:
	subl $2,%esi
	incl %ebx
	cmpl $120,%ebx
	jle .L41
	movl 16(%ebp),%eax
	movw %cx,(%eax)
	movl %edx,%eax
	emms
	popl %ebx
	popl %esi
	popl %edi
	leave
	ret
.Lfe2:
	.size	 k6maxcc,.Lfe2-k6maxcc


	.align 4
/* long k6iprod (const short *p, const short *q, int n) */
.globl k6iprod
	.type	 k6iprod,@function
k6iprod:
	pushl %ebp
	movl %esp,%ebp
	pushl %edi
	pushl %esi
	emms
	pxor %mm0,%mm0
	movl 8(%ebp),%esi
	movl 12(%ebp),%edi
	movl 16(%ebp),%eax
	leal -32(%esi,%eax,2),%edx /* edx = top - 32 */

	cmpl %edx,%esi; ja .L202

	.p2align 2
.L201:
	ccstep 0
	ccstep 8
	ccstep 16
	ccstep 24

	addl $32,%esi
	addl $32,%edi
	cmpl %edx,%esi; jbe .L201

	.p2align 2
.L202:
	addl $24,%edx  /* now edx = top-8 */
	cmpl %edx,%esi; ja .L205

	.p2align 2
.L203:
	ccstep 0

	addl $8,%esi
	addl $8,%edi
	cmpl %edx,%esi; jbe .L203

	.p2align 2
.L205:
	addl $4,%edx  /* now edx = top-4 */
	cmpl %edx,%esi; ja .L207

	movd (%edi),%mm1
	movd (%esi),%mm2
	pmaddwd %mm2,%mm1
	paddd %mm1,%mm0

	addl $4,%esi
	addl $4,%edi

	.p2align 2
.L207:
	addl $2,%edx  /* now edx = top-2 */
	cmpl %edx,%esi; ja .L209

	movswl (%edi),%eax
	movd %eax,%mm1
	movswl (%esi),%eax
	movd %eax,%mm2
	pmaddwd %mm2,%mm1
	paddd %mm1,%mm0

	.p2align 2
.L209:
	movq %mm0,%mm1
	punpckhdq %mm0,%mm1  /* mm1 has high int32 of mm0 dup'd */
	paddd %mm1,%mm0;
	movd %mm0,%eax  /* eax has result */

	emms
	popl %esi
	popl %edi
	leave
	ret
.Lfe3:
	.size	 k6iprod,.Lfe3-k6iprod


	.align 4
/* void k6vsraw P3((short *p, int n, int bits) */
.globl k6vsraw
	.type	 k6vsraw,@function
k6vsraw:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	movl 8(%ebp),%esi
	movl 16(%ebp),%ecx
	andl %ecx,%ecx; jle .L399
	movl 12(%ebp),%eax
	leal -16(%esi,%eax,2),%edx /* edx = top - 16 */
	emms
	movd %ecx,%mm3
	movq ones,%mm2
	psllw %mm3,%mm2; psrlw $1,%mm2
	cmpl %edx,%esi; ja .L306

	.p2align 2
.L302: /* 8 words per iteration */
	movq (%esi),%mm0
	movq 8(%esi),%mm1
	paddsw %mm2,%mm0
	psraw %mm3,%mm0;
	paddsw %mm2,%mm1
	psraw %mm3,%mm1;
	movq %mm0,(%esi)
	movq %mm1,8(%esi)
	addl $16,%esi
	cmpl %edx,%esi
	jbe .L302

	.p2align 2
.L306:
	addl $12,%edx /* now edx = top-4 */
	cmpl %edx,%esi; ja .L310

	.p2align 2
.L308: /* do up to 6 words, two at a time */
	movd  (%esi),%mm0
	paddsw %mm2,%mm0
	psraw %mm3,%mm0;
	movd %mm0,(%esi)
	addl $4,%esi
	cmpl %edx,%esi
	jbe .L308

	.p2align 2
.L310:
	addl $2,%edx /* now edx = top-2 */
	cmpl %edx,%esi; ja .L315
	
	movzwl (%esi),%eax
	movd %eax,%mm0
	paddsw %mm2,%mm0
	psraw %mm3,%mm0;
	movd %mm0,%eax
	movw %ax,(%esi)

	.p2align 2
.L315:
	emms
.L399:
	popl %esi
	leave
	ret
.Lfe4:
	.size	 k6vsraw,.Lfe4-k6vsraw
	
	.align 4
/* void k6vsllw P3((short *p, int n, int bits) */
.globl k6vsllw
	.type	 k6vsllw,@function
k6vsllw:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	movl 8(%ebp),%esi
	movl 16(%ebp),%ecx
	andl %ecx,%ecx; jle .L499
	movl 12(%ebp),%eax
	leal -16(%esi,%eax,2),%edx /* edx = top - 16 */
	emms
	movd %ecx,%mm3
	cmpl %edx,%esi; ja .L406

	.p2align 2
.L402: /* 8 words per iteration */
	movq (%esi),%mm0
	movq 8(%esi),%mm1
	psllw %mm3,%mm0;
	psllw %mm3,%mm1;
	movq %mm0,(%esi)
	movq %mm1,8(%esi)
	addl $16,%esi
	cmpl %edx,%esi
	jbe .L402

	.p2align 2
.L406:
	addl $12,%edx /* now edx = top-4 */
	cmpl %edx,%esi; ja .L410

	.p2align 2
.L408: /* do up to 6 words, two at a time */
	movd (%esi),%mm0
	psllw %mm3,%mm0;
	movd %mm0,(%esi)
	addl $4,%esi
	cmpl %edx,%esi
	jbe .L408

	.p2align 2
.L410:
	addl $2,%edx /* now edx = top-2 */
	cmpl %edx,%esi; ja .L415
	
	movzwl (%esi),%eax
	movd %eax,%mm0
	psllw %mm3,%mm0;
	movd %mm0,%eax
	movw %ax,(%esi)

	.p2align 2
.L415:
	emms
.L499:
	popl %esi
	leave
	ret
.Lfe5:
	.size	 k6vsllw,.Lfe5-k6vsllw


.section	.rodata
	.align 4
	.type	 extremes,@object
	.size	 extremes,8
extremes:
	.long 0x80008000
	.long 0x7fff7fff
	.type	 ones,@object
	.size	 ones,8
ones:
	.long 0x00010001
	.long 0x00010001

.text
	.align 4
/* long k6maxmin (const short *p, int n, short *out) */
.globl k6maxmin
	.type	 k6maxmin,@function
k6maxmin:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	emms
	movl 8(%ebp),%esi
	movl 12(%ebp),%eax
	leal -8(%esi,%eax,2),%edx

	cmpl %edx,%esi
	jbe .L52
	movd extremes,%mm0
	movd extremes+4,%mm1
	jmp .L58

	.p2align 2
.L52:
	movq (%esi),%mm0   /* mm0 will be max's */
	movq %mm0,%mm1     /* mm1 will be min's */
	addl $8,%esi
	cmpl %edx,%esi
	ja .L56

	.p2align 2
.L54:
	movq (%esi),%mm2

	movq %mm2,%mm3
	pcmpgtw %mm0,%mm3  /* mm3 is bitmask for words where mm2 > mm0 */ 
	movq %mm3,%mm4
	pand %mm2,%mm3     /* mm3 is mm2 masked to new max's */
	pandn %mm0,%mm4    /* mm4 is mm0 masked to its max's */
	por %mm3,%mm4
	movq %mm4,%mm0     /* now mm0 is updated max's */
	
	movq %mm1,%mm3
	pcmpgtw %mm2,%mm3  /* mm3 is bitmask for words where mm2 < mm1 */ 
	pand %mm3,%mm2     /* mm2 is mm2 masked to new min's */
	pandn %mm1,%mm3    /* mm3 is mm1 masked to its min's */
	por %mm3,%mm2
	movq %mm2,%mm1     /* now mm1 is updated min's */

	addl $8,%esi
	cmpl %edx,%esi
	jbe .L54

	.p2align 2
.L56: /* merge down the 4-word max/mins to lower 2 words */

	movq %mm0,%mm2
	psrlq $32,%mm2
	movq %mm2,%mm3
	pcmpgtw %mm0,%mm3  /* mm3 is bitmask for words where mm2 > mm0 */ 
	pand %mm3,%mm2     /* mm2 is mm2 masked to new max's */
	pandn %mm0,%mm3    /* mm3 is mm0 masked to its max's */
	por %mm3,%mm2
	movq %mm2,%mm0     /* now mm0 is updated max's */

	movq %mm1,%mm2
	psrlq $32,%mm2
	movq %mm1,%mm3
	pcmpgtw %mm2,%mm3  /* mm3 is bitmask for words where mm2 < mm1 */ 
	pand %mm3,%mm2     /* mm2 is mm2 masked to new min's */
	pandn %mm1,%mm3    /* mm3 is mm1 masked to its min's */
	por %mm3,%mm2
	movq %mm2,%mm1     /* now mm1 is updated min's */

	.p2align 2
.L58:
	addl $4,%edx       /* now dx = top-4 */
	cmpl %edx,%esi
	ja .L62
	/* here, there are >= 2 words of input remaining */
	movd (%esi),%mm2

	movq %mm2,%mm3
	pcmpgtw %mm0,%mm3  /* mm3 is bitmask for words where mm2 > mm0 */ 
	movq %mm3,%mm4
	pand %mm2,%mm3     /* mm3 is mm2 masked to new max's */
	pandn %mm0,%mm4    /* mm4 is mm0 masked to its max's */
	por %mm3,%mm4
	movq %mm4,%mm0     /* now mm0 is updated max's */
	
	movq %mm1,%mm3
	pcmpgtw %mm2,%mm3  /* mm3 is bitmask for words where mm2 < mm1 */ 
	pand %mm3,%mm2     /* mm2 is mm2 masked to new min's */
	pandn %mm1,%mm3    /* mm3 is mm1 masked to its min's */
	por %mm3,%mm2
	movq %mm2,%mm1     /* now mm1 is updated min's */

	addl $4,%esi

	.p2align 2
.L62:
	/* merge down the 2-word max/mins to 1 word */

	movq %mm0,%mm2
	psrlq $16,%mm2
	movq %mm2,%mm3
	pcmpgtw %mm0,%mm3  /* mm3 is bitmask for words where mm2 > mm0 */ 
	pand %mm3,%mm2     /* mm2 is mm2 masked to new max's */
	pandn %mm0,%mm3    /* mm3 is mm0 masked to its max's */
	por %mm3,%mm2
	movd %mm2,%ecx     /* cx is max so far */

	movq %mm1,%mm2
	psrlq $16,%mm2
	movq %mm1,%mm3
	pcmpgtw %mm2,%mm3  /* mm3 is bitmask for words where mm2 < mm1 */ 
	pand %mm3,%mm2     /* mm2 is mm2 masked to new min's */
	pandn %mm1,%mm3    /* mm3 is mm1 masked to its min's */
	por %mm3,%mm2
	movd %mm2,%eax     /* ax is min so far */
	
	addl $2,%edx       /* now dx = top-2 */
	cmpl %edx,%esi
	ja .L65

	/* here, there is one word of input left */
	cmpw (%esi),%cx
	jge .L64
	movw (%esi),%cx
	.p2align 2
.L64:
	cmpw (%esi),%ax
	jle .L65
	movw (%esi),%ax

	.p2align 2
.L65:  /* (finally!) cx is the max, ax the min */
	movswl %cx,%ecx
	movswl %ax,%eax

	movl 16(%ebp),%edx /* ptr to output max,min vals */
	andl %edx,%edx; jz .L77
	movw %cx,(%edx)  /* max */
	movw %ax,2(%edx) /* min */
	.p2align 2
.L77:
	/* now calculate max absolute val */
	negl %eax
	cmpl %ecx,%eax
	jge .L81
	movl %ecx,%eax
	.p2align 2
.L81:
	emms
	popl %esi
	leave
	ret
.Lfe6:
	.size	 k6maxmin,.Lfe6-k6maxmin

/* void Short_term_analysis_filtering (short *u0, const short *rp0, int kn, short *s) */
	.equiv pm_u0,8
	.equiv pm_rp0,12
	.equiv pm_kn,16
	.equiv pm_s,20
	.equiv lv_u_top,-4
	.equiv lv_s_top,-8
	.equiv lv_rp,-40 /* local version of rp0 with each word twice */
	.align 4
.globl Short_term_analysis_filteringx
	.type	 Short_term_analysis_filteringx,@function
Short_term_analysis_filteringx:
	pushl %ebp
	movl %esp,%ebp
	subl $40,%esp
	pushl %edi
	pushl %esi

	movl pm_rp0(%ebp),%esi;
	leal lv_rp(%ebp),%edi;
	cld
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	emms
	movl $0x4000,%eax;
	movd %eax,%mm4;
	punpckldq %mm4,%mm4 /* (0x00004000,0x00004000) for rounding dword product pairs */

	movl pm_u0(%ebp),%eax
	addl $16,%eax
	movl %eax,lv_u_top(%ebp) /* UTOP */
	movl pm_s(%ebp),%edx  /* edx is local s ptr throughout below */
	movl pm_kn(%ebp),%eax
	leal (%edx,%eax,2),%eax
	movl %eax,lv_s_top(%ebp)
	cmpl %eax,%edx
	jae .L179
	.p2align 2
.L181:
	leal lv_rp(%ebp),%esi  /* RP */
	movl pm_u0(%ebp),%edi  /* U  */
	movw (%edx),%ax /* (0,DI) */
	roll $16,%eax
	movw (%edx),%ax /* (DI,DI) */
	.p2align 2
.L185: /* RP is %esi */
	movl %eax,%ecx
	movw (%edi),%ax  /* (DI,U) */
	movd (%esi),%mm3 /* mm3 is (0,0,RP,RP) */
	movw %cx,(%edi)

	movd %eax,%mm2   /* mm2 is (0,0,DI,U) */
	rorl $16,%eax 
	movd %eax,%mm1   /* mm1 is (0,0,U,DI) */

	movq %mm1,%mm0
	pmullw %mm3,%mm0
	pmulhw %mm3,%mm1
	punpcklwd %mm1,%mm0 /* mm0 is (RP*U,RP*DI) */
	paddd %mm4,%mm0     /* mm4 is 0x00004000,0x00004000 */
	psrad $15,%mm0      /* (RP*U,RP*DI) adjusted */
	packssdw %mm0,%mm0  /* (*,*,RP*U,RP*DI) adjusted and saturated to word */
	paddsw %mm2,%mm0    /* mm0 is (?,?, DI', U') */
	movd %mm0,%eax      /* (DI,U') */

	addl $2,%edi
	addl $4,%esi
	cmpl lv_u_top(%ebp),%edi
	jb .L185

	rorl $16,%eax
	movw %ax,(%edx) /* last DI goes to *s */
	addl $2,%edx    /* next s */
	cmpl lv_s_top(%ebp),%edx
	jb .L181
	.p2align 2
.L179:
	emms
	popl %esi
	popl %edi
	leave
	ret
.Lfe7:
	.size	 Short_term_analysis_filteringx,.Lfe7-Short_term_analysis_filteringx

.end

/* 'as' macro's seem to be case-insensitive */
.macro STEP n
.if \n
	movd \n(%esi),%mm3 /* mm3 is (0,0,RP,RP) */
.else
	movd (%esi),%mm3 /* mm3 is (0,0,RP,RP) */
.endif
	movq %mm5,%mm1;
	movd %mm4,%ecx; movw %cx,%ax  /* (DI,U) */
	psllq $48,%mm1; psrlq $16,%mm4; por %mm1,%mm4
	psllq $48,%mm0; psrlq $16,%mm5; por %mm0,%mm5

	movd %eax,%mm2   /* mm2 is (0,0,DI,U) */
	rorl $16,%eax 
	movd %eax,%mm1   /* mm1 is (0,0,U,DI) */

	movq %mm1,%mm0
	pmullw %mm3,%mm0
	pmulhw %mm3,%mm1
	punpcklwd %mm1,%mm0 /* mm0 is (RP*U,RP*DI) */
	paddd %mm6,%mm0     /* mm6 is 0x00004000,0x00004000 */
	psrad $15,%mm0      /* (RP*U,RP*DI) adjusted */
	packssdw %mm0,%mm0  /* (*,*,RP*U,RP*DI) adjusted and saturated to word */
	paddsw %mm2,%mm0    /* mm0 is (?,?, DI', U') */
	movd %mm0,%eax      /* (DI,U') */
.endm

/* void Short_term_analysis_filtering (short *u0, const short *rp0, int kn, short *s) */
	.equiv pm_u0,8
	.equiv pm_rp0,12
	.equiv pm_kn,16
	.equiv pm_s,20
	.equiv lv_rp_top,-4
	.equiv lv_s_top,-8
	.equiv lv_rp,-40 /* local version of rp0 with each word twice */
	.align 4
.globl Short_term_analysis_filteringx
	.type	 Short_term_analysis_filteringx,@function
Short_term_analysis_filteringx:
	pushl %ebp
	movl %esp,%ebp
	subl $56,%esp
	pushl %edi
	pushl %esi
	pushl %ebx

	movl pm_rp0(%ebp),%esi;
	leal lv_rp(%ebp),%edi;
	cld
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	lodsw; stosw; stosw
	movl %edi,lv_rp_top(%ebp)
	emms

	movl $0x4000,%eax;
	movd %eax,%mm6;
	punpckldq %mm6,%mm6 /* (0x00004000,0x00004000) for rounding dword product pairs */

	movl pm_u0(%ebp),%ebx
	movq (%ebx),%mm4; movq 8(%ebx),%mm5 /* the 8 u's */
	movl pm_s(%ebp),%edx  /* edx is local s ptr throughout below */
	movl pm_kn(%ebp),%eax
	leal (%edx,%eax,2),%eax
	movl %eax,lv_s_top(%ebp)
	cmpl %eax,%edx
	jae .L179
	.p2align 2
.L181:
	leal lv_rp(%ebp),%esi  /* RP */
	movw (%edx),%ax /* (0,DI) */
	roll $16,%eax
	movw (%edx),%ax /* (DI,DI) */
	movd %eax,%mm0
	.p2align 2
.L185: /* RP is %esi */
	step 0
	step 4
	step 8
	step 12
/*
	step 16
	step 20
	step 24
	step 28
*/
	addl $16,%esi
	cmpl lv_rp_top(%ebp),%esi 
	jb .L185

	rorl $16,%eax
	movw %ax,(%edx) /* last DI goes to *s */
	addl $2,%edx    /* next s */
	cmpl lv_s_top(%ebp),%edx
	jb .L181
.L179:
	movq %mm4,(%ebx); movq %mm5,8(%ebx) /* the 8 u's */
	emms
	popl %ebx
	popl %esi
	popl %edi
	leave
	ret
.Lfe7:
	.size	 Short_term_analysis_filteringx,.Lfe7-Short_term_analysis_filteringx
	.ident	"GCC: (GNU) 2.95.2 19991109 (Debian GNU/Linux)"
