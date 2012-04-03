@ ARM procedure call convention:
@ r0..r3, r12 (ip) and r14 (lr) are volatile.  Args are passed in r0..r3,
@ and the return address in r14.
@
@ All other registers must be preserved by the callee.  r13 (sp) and r15 (pc)
@ are as expected.
@
@ The usual convention is to push all the needed registers, including r14,
@ on the stack, and the restore them at the end, but to r15 rather than r14.
@ This, however, WILL NOT WORK for Thumb code.  You have to use the "bx"
@ instruction for that, so you need one more trailing instruction.

	.text
	.align	2
	.type	k_table, %object
k_table:
	.word	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b
	.word	0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01
	.word	0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7
	.word	0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
	.word	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152
	.word	0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147
	.word	0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc
	.word	0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
	.word	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819
	.word	0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08
	.word	0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f
	.word	0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
	.word	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	.size	k_table, .-k_table

@ We use 13 local variables:
pp	.req r0		@ The argument, points to the IV and w[] space
aa	.req r1		@ Working variable.
bb	.req r2
cc	.req r3
dd	.req r4
ee	.req r5
ff	.req r6
gg	.req r7
hh	.req r8
ii	.req r9		@ Loop index
tt	.req r10	@ General purpose temp
kk	.req r11	@ k+64 (k_table+256)
ww	.req r12	@ Actually, w+64 much of the time
@ We could use r14 as well, but don't need to.
@ (The names are doubled because a sigle b is "branch"!
@
@ This function takes a pointer to an array of 72 32-bit words:
@ The first 8 are the state vector a..h
@ The next 16 are the input data words w[0..15], in native byte order.
@ The next 48 are used to hold the rest of the key schedule w[16..63].

	.global	sha256_transform
	.type	sha256_transform, %function
sha256_transform:
	stmfd	sp!, {r4,r5,r6,r7,r8,r9,r10,r11}
	add	ww, pp, #4*(8+16)	@ w + 16 = p + 8 + 16
	mov	ii, #64-16		@ loop counter

	@ Fill in words 16..63 of the w[] array, at p+24..p+71
1:
	@ ww[i] = w[i-16] + s0(w[i-15]) + w[i-7] + s1(w[i-2])
	ldr	aa, [ww, #-64]		@ a = w[i-16]
	ldr	bb, [ww, #-60]		@ b = w[i-15]
	ldr	cc, [ww, #-28]		@ c = w[i-7]
	add	aa, aa, cc		@ a += c (= w[i-7])

	@ s0(x) = (x >>> 7) ^ (x >>> 18) ^ (x >> 3)
	mov	cc, bb, ror #18		@ c = b>>>18
	eor	cc, cc, bb, ror #7	@ c ^= b>>>7
	eor	cc, cc, bb, lsr #3	@ c ^= b>>3
	ldr	bb, [ww, #-8]		@ b = w[i-2]
	add	aa, aa, cc		@ a += c (= s0(w[i-15]))
	@ s1(x) = (x >>> 17) ^ (x >>> 19) ^ (x >> 10)
	mov	cc, bb, ror #19		@ c = b>>>19
	eor	cc, cc, bb, ror #17	@ c ^= b>>>17
	eor	cc, cc, bb, lsr #10	@ c ^= b>>10
	add	aa, aa, cc		@ a += c (= s1(w[i-2]))

	subs	ii, ii, #1		@ --i
	str	aa, [ww], #4		@ w[i++] = a
	bne	1b


	@ The main loop.  Arrays are indexed with i, which starts at -256
	@ and counts up to 0.  In addition to t, we use h as a working
	@ variable for the first part of the loop, until doing the
	@ big register rotation, then a as a temp for the last part.

	ldmia	pp, {aa,bb,cc,dd,ee,ff,gg,hh}	@ Load a..h
	mov	ii, #-256		@ i = -64 (*4 strength-reduced)
	adr	kk, k_table+256		@ Load up r12 to the END of k
2:
	@ t = h + S1(e) + Ch(e,f,g) + k[i] + w[i]
	@ Form t = Ch(e,f,g) = (g ^ (e & (f ^ g))
	eor	tt, ff, gg		@ t = f^g
	and	tt, tt, ee		@ t &= e
	eor	tt, tt, gg		@ t ^= g

	add	tt, tt, hh		@ t += h

	@ Form t += S1(e) = (e >>> 6) ^ (e >>> 11) ^ (e >>> 25)
	eor	hh, ee, ee, ror #25-6	@ h = e ^ e>>>(25-6)
	eor	hh, hh, ee, ror #11-6	@ h = h ^ e>>>(11-6)
	add	tt, tt, hh, ror #6	@ t += h>>>6

	@ Add k[i] and w[i].  Note that -64 <= i < 0.
	ldr	hh, [ww, ii]		@ h = w[64+i]
	add	tt, tt, hh
	ldr	hh, [kk, ii]		@ h = k[64+i]
	add	tt, tt, hh
	adds	ii, ii, #4		@ ++i (*4 strength-reduced)

	@ Copy (h,g,f,e,d,c,b) = (g,f,e,d+t1,c,b,a)
	@ This could be shrunk with aa big stm/ldm pair, but that
	@ seems terribly wasteful...
	mov	hh, gg			@ h = g
	mov	gg, ff			@ g = f
	mov	ff, ee			@ f = e
	add	ee, dd, tt		@ e = d + t
	mov	dd, cc			@ d = c
	mov	cc, bb			@ c = b
	mov	bb, aa			@ b = a

	@ a = t + S0(b) + Maj(b,c,d)
	@ Form t += S0(b) = (b >>> 2) ^ (b >>> 13) ^ (b >>> 22) */
	eor	aa, bb, bb, ror #22-2	@ a = b ^ b>>>(22-2)
	eor	aa, aa, bb, ror #13-2	@ a = a ^ b>>>(13-2)
	add	tt, tt, aa, ror #2	@ t += a>>>2

	@ Form a = t + Maj(b,c,d) = (c & d) + (b & (c ^ d))
	and	aa, cc, dd		@ a = c & d
	add	tt, tt, aa		@ t += a
	eor	aa, cc, dd		@ a = c ^ d
	and	aa, aa, bb		@ a &= b
	add	aa, aa, tt		@ a += t

	bne	2b			@ while (i != 0)

	@ Now, the final summation.  Minimum code size is tricky...
	ldmia	pp!, {ii,tt,kk,ww}	@ Load old iv[0..3]
	add	aa, aa, ii		@ a += iv[0]
	add	bb, bb, tt		@ b += iv[1]
	add	cc, cc, kk		@ c += iv[2]
	add	dd, dd, ww		@ d += iv[3]
	ldmia	pp!, {ii,tt,kk,ww}	@ Load old iv[4..7]
	add	ee, ee, ii		@ e += iv[4]
	add	ff, ff, tt		@ f += iv[5]
	add	gg, gg, kk		@ g += iv[6]
	add	hh, hh, ww		@ h += iv[7]
	stmfd	pp, {aa,bb,cc,dd,ee,ff,gg,hh}	@ Store new iv[0..7]

	ldmfd	sp!, {r4,r5,r6,r7,r8,r9,r10,r11}
	bx	lr

	.size	sha256_transform, .-sha256_transform
