/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbn32.c - Low-level bignum routines, 32-bit version.
 *
 * NOTE: the magic constants "32" and "64" appear in many places in this
 * file, including inside identifiers.  Because it is not possible to
 * ask "#ifdef" of a macro expansion, it is not possible to use the
 * preprocessor to conditionalize these properly.  Thus, this file is
 * intended to be edited with textual search and replace to produce
 * alternate word size versions.  Any reference to the number of bits
 * in a word must be the string "32", and that string must not appear
 * otherwise.  Any reference to twice this number must appear as "64",
 * which likewise must not appear otherwise.  Is that clear?
 *
 * Remember, when doubling the bit size replace the larger number (64)
 * first, then the smaller (32).  When halving the bit size, do the
 * opposite.  Otherwise, things will get wierd.  Also, be sure to replace
 * every instance that appears.  (:%s/foo/bar/g in vi)
 *
 * These routines work with a pointer to the least-significant end of
 * an array of WORD32s.  The BIG(x), LITTLE(y) and BIGLTTLE(x,y) macros
 * defined in lbn.h (which expand to x on a big-edian machine and y on a
 * little-endian machine) are used to conditionalize the code to work
 * either way.  If you have no assembly primitives, it doesn't matter.
 * Note that on a big-endian machine, the least-significant-end pointer
 * is ONE PAST THE END.  The bytes are ptr[-1] through ptr[-len].
 * On little-endian, they are ptr[0] through ptr[len-1].  This makes
 * perfect sense if you consider pointers to point *between* bytes rather
 * than at them.
 *
 * Because the array index values are unsigned integers, ptr[-i]
 * may not work properly, since the index -i is evaluated as an unsigned,
 * and if pointers are wider, zero-extension will produce a positive
 * number rahter than the needed negative.  The expression used in this
 * code, *(ptr-i) will, however, work.  (The array syntax is equivalent
 * to *(ptr+-i), which is a pretty subtle difference.)
 *
 * Many of these routines will get very unhappy if fed zero-length inputs.
 * They use assert() to enforce this.  An higher layer of code must make
 * sure that these aren't called with zero-length inputs.
 *
 * Any of these routines can be replaced with more efficient versions
 * elsewhere, by just #defining their names.  If one of the names
 * is #defined, the C code is not compiled in and no declaration is
 * made.  Use the BNINCLUDE file to do that.  Typically, you compile
 * asm subroutines with the same name and just, e.g.
 * #define lbnMulAdd1_32 lbnMulAdd1_32
 *
 * If you want to write asm routines, start with lbnMulAdd1_32().
 * This is the workhorse of modular exponentiation.  lbnMulN1_32() is
 * also used a fair bit, although not as much and it's defined in terms
 * of lbnMulAdd1_32 if that has a custom version.  lbnMulSub1_32 and
 * lbnDiv21_32 are used in the usual division and remainder finding.
 * (Not the Montgomery reduction used in modular exponentiation, though.)
 * Once you have lbnMulAdd1_32 defined, writing the other two should
 * be pretty easy.  (Just make sure you get the sign of the subtraction
 * in lbnMulSub1_32 right - it's dest = dest - source * k.)
 *
 * The only definitions that absolutely need a double-word (BNWORD64)
 * type are lbnMulAdd1_32 and lbnMulSub1_32; if those are provided,
 * the rest follows.  lbnDiv21_32, however, is a lot slower unless you
 * have them, and lbnModQ_32 takes after it.  That one is used quite a
 * bit for prime sieving.
 */

#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H 0
#endif
#if HAVE_CONFIG_H
#include "bnconfig.h"
#endif

/*
 * Some compilers complain about #if FOO if FOO isn't defined,
 * so do the ANSI-mandated thing explicitly...
 */
#ifndef NO_ASSERT_H
#define NO_ASSERT_H 0
#endif
#ifndef NO_STRING_H
#define NO_STRING_H 0
#endif
#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 0
#endif

#if !NO_ASSERT_H
#include <assert.h>
#else
#define assert(x) (void)0
#endif

#if !NO_STRING_H
#include <string.h>	/* For memcpy */
#elif HAVE_STRINGS_H
#include <strings.h>
#endif

#include "lbn.h"
#include "lbn32.h"
#include "lbnmem.h"

#include "kludge.h"

#ifndef BNWORD32
#error 32-bit bignum library requires a 32-bit data type
#endif

/* If this is defined, include bnYield() calls */
#if BNYIELD
extern int (*bnYield)(void);	/* From bn.c */
#endif

/*
 * Most of the multiply (and Montgomery reduce) routines use an outer
 * loop that iterates over one of the operands - a so-called operand
 * scanning approach.  One big advantage of this is that the assembly
 * support routines are simpler.  The loops can be rearranged to have
 * an outer loop that iterates over the product, a so-called product
 * scanning approach.  This has the advantage of writing less data
 * and doing fewer adds to memory, so is supposedly faster.  Some
 * code has been written using a product-scanning approach, but
 * it appears to be slower, so it is turned off by default.  Some
 * experimentation would be appreciated.
 *
 * (The code is also annoying to get right and not very well commented,
 * one of my pet peeves about math libraries.  I'm sorry.)
 */
#ifndef PRODUCT_SCAN
#define PRODUCT_SCAN 0
#endif

/*
 * Copy an array of words.  <Marvin mode on>  Thrilling, isn't it? </Marvin>
 * This is a good example of how the byte offsets and BIGLITTLE() macros work.
 * Another alternative would have been
 * memcpy(dest BIG(-len), src BIG(-len), len*sizeof(BNWORD32)), but I find that
 * putting operators into conditional macros is confusing.
 */
#ifndef lbnCopy_32
void
lbnCopy_32(BNWORD32 *dest, BNWORD32 const *src, unsigned len)
{
	memcpy(BIGLITTLE(dest-len,dest), BIGLITTLE(src-len,src),
	       len * sizeof(*src));
}
#endif /* !lbnCopy_32 */

/*
 * Fill n words with zero.  This does it manually rather than calling
 * memset because it can assume alignment to make things faster while
 * memset can't.  Note how big-endian numbers are naturally addressed
 * using predecrement, while little-endian is postincrement.
 */
#ifndef lbnZero_32
void
lbnZero_32(BNWORD32 *num, unsigned len)
{
	while (len--)
		BIGLITTLE(*--num,*num++) = 0;
}
#endif /* !lbnZero_32 */

/*
 * Negate an array of words.
 * Negation is subtraction from zero.  Negating low-order words
 * entails doing nothing until a non-zero word is hit.  Once that
 * is negated, a borrow is generated and never dies until the end
 * of the number is hit.  Negation with borrow, -x-1, is the same as ~x.
 * Repeat that until the end of the number.
 *
 * Doesn't return borrow out because that's pretty useless - it's
 * always set unless the input is 0, which is easy to notice in
 * normalized form.
 */
#ifndef lbnNeg_32
void
lbnNeg_32(BNWORD32 *num, unsigned len)
{
	assert(len);

	/* Skip low-order zero words */
	while (BIGLITTLE(*--num,*num) == 0) {
		if (!--len)
			return;
		LITTLE(num++;)
	}
	/* Negate the lowest-order non-zero word */
	*num = -*num;
	/* Complement all the higher-order words */
	while (--len) {
		BIGLITTLE(--num,++num);
		*num = ~*num;
	}
}
#endif /* !lbnNeg_32 */


/*
 * lbnAdd1_32: add the single-word "carry" to the given number.
 * Used for minor increments and propagating the carry after
 * adding in a shorter bignum.
 *
 * Technique: If we have a double-width word, presumably the compiler
 * can add using its carry in inline code, so we just use a larger
 * accumulator to compute the carry from the first addition.
 * If not, it's more complex.  After adding the first carry, which may
 * be > 1, compare the sum and the carry.  If the sum wraps (causing a
 * carry out from the addition), the result will be less than each of the
 * inputs, since the wrap subtracts a number (2^32) which is larger than
 * the other input can possibly be.  If the sum is >= the carry input,
 * return success immediately.
 * In either case, if there is a carry, enter a loop incrementing words
 * until one does not wrap.  Since we are adding 1 each time, the wrap
 * will be to 0 and we can test for equality.
 */
#ifndef lbnAdd1_32	/* If defined, it's provided as an asm subroutine */
#ifdef BNWORD64
BNWORD32
lbnAdd1_32(BNWORD32 *num, unsigned len, BNWORD32 carry)
{
	BNWORD64 t;
	assert(len > 0);	/* Alternative: if (!len) return carry */

	t = (BNWORD64)BIGLITTLE(*--num,*num) + carry;
	BIGLITTLE(*num,*num++) = (BNWORD32)t;
	if ((t >> 32) == 0)
		return 0;
	while (--len) {
		if (++BIGLITTLE(*--num,*num++) != 0)
			return 0;
	}
	return 1;
}
#else /* no BNWORD64 */
BNWORD32
lbnAdd1_32(BNWORD32 *num, unsigned len, BNWORD32 carry)
{
	assert(len > 0);	/* Alternative: if (!len) return carry */

	if ((BIGLITTLE(*--num,*num++) += carry) >= carry)
		return 0;
	while (--len) {
		if (++BIGLITTLE(*--num,*num++) != 0)
			return 0;
	}
	return 1;
}
#endif
#endif/* !lbnAdd1_32 */

/*
 * lbnSub1_32: subtract the single-word "borrow" from the given number.
 * Used for minor decrements and propagating the borrow after
 * subtracting a shorter bignum.
 *
 * Technique: Similar to the add, above.  If there is a double-length type,
 * use that to generate the first borrow.
 * If not, after subtracting the first borrow, which may be > 1, compare
 * the difference and the *negative* of the carry.  If the subtract wraps
 * (causing a borrow out from the subtraction), the result will be at least
 * as large as -borrow.  If the result < -borrow, then no borrow out has
 * appeared and we may return immediately, except when borrow == 0.  To
 * deal with that case, use the identity that -x = ~x+1, and instead of
 * comparing < -borrow, compare for <= ~borrow.
 * Either way, if there is a borrow out, enter a loop decrementing words
 * until a non-zero word is reached.
 *
 * Note the cast of ~borrow to (BNWORD32).  If the size of an int is larger
 * than BNWORD32, C rules say the number is expanded for the arithmetic, so
 * the inversion will be done on an int and the value won't be quite what
 * is expected.
 */
#ifndef lbnSub1_32	/* If defined, it's provided as an asm subroutine */
#ifdef BNWORD64
BNWORD32
lbnSub1_32(BNWORD32 *num, unsigned len, BNWORD32 borrow)
{
	BNWORD64 t;
	assert(len > 0);	/* Alternative: if (!len) return borrow */

	t = (BNWORD64)BIGLITTLE(*--num,*num) - borrow;
	BIGLITTLE(*num,*num++) = (BNWORD32)t;
	if ((t >> 32) == 0)
		return 0;
	while (--len) {
		if ((BIGLITTLE(*--num,*num++))-- != 0)
			return 0;
	}
	return 1;
}
#else /* no BNWORD64 */
BNWORD32
lbnSub1_32(BNWORD32 *num, unsigned len, BNWORD32 borrow)
{
	assert(len > 0);	/* Alternative: if (!len) return borrow */

	if ((BIGLITTLE(*--num,*num++) -= borrow) <= (BNWORD32)~borrow)
		return 0;
	while (--len) {
		if ((BIGLITTLE(*--num,*num++))-- != 0)
			return 0;
	}
	return 1;
}
#endif
#endif /* !lbnSub1_32 */

/*
 * lbnAddN_32: add two bignums of the same length, returning the carry (0 or 1).
 * One of the building blocks, along with lbnAdd1, of adding two bignums of
 * differing lengths.
 *
 * Technique: Maintain a word of carry.  If there is no double-width type,
 * use the same technique as in lbnAdd1, above, to maintain the carry by
 * comparing the inputs.  Adding the carry sources is used as an OR operator;
 * at most one of the two comparisons can possibly be true.  The first can
 * only be true if carry == 1 and x, the result, is 0.  In that case the
 * second can't possibly be true.
 */
#ifndef lbnAddN_32
#ifdef BNWORD64
BNWORD32
lbnAddN_32(BNWORD32 *num1, BNWORD32 const *num2, unsigned len)
{
	BNWORD64 t;

	assert(len > 0);

	t = (BNWORD64)BIGLITTLE(*--num1,*num1) + BIGLITTLE(*--num2,*num2++);
	BIGLITTLE(*num1,*num1++) = (BNWORD32)t;
	while (--len) {
		t = (BNWORD64)BIGLITTLE(*--num1,*num1) +
		    (BNWORD64)BIGLITTLE(*--num2,*num2++) + (t >> 32);
		BIGLITTLE(*num1,*num1++) = (BNWORD32)t;
	}

	return (BNWORD32)(t>>32);
}
#else /* no BNWORD64 */
BNWORD32
lbnAddN_32(BNWORD32 *num1, BNWORD32 const *num2, unsigned len)
{
	BNWORD32 x, carry = 0;

	assert(len > 0);	/* Alternative: change loop to test at start */

	do {
		x = BIGLITTLE(*--num2,*num2++);
		carry = (x += carry) < carry;
		carry += (BIGLITTLE(*--num1,*num1++) += x) < x;
	} while (--len);

	return carry;
}
#endif
#endif /* !lbnAddN_32 */

/*
 * lbnSubN_32: add two bignums of the same length, returning the carry (0 or 1).
 * One of the building blocks, along with subn1, of subtracting two bignums of
 * differing lengths.
 *
 * Technique: If no double-width type is availble, maintain a word of borrow.
 * First, add the borrow to the subtrahend (did you have to learn all those
 * awful words in elementary school, too?), and if it overflows, set the
 * borrow again.  Then subtract the modified subtrahend from the next word
 * of input, using the same technique as in subn1, above.
 * Adding the borrows is used as an OR operator; at most one of the two
 * comparisons can possibly be true.  The first can only be true if
 * borrow == 1 and x, the result, is 0.  In that case the second can't
 * possibly be true.
 *
 * In the double-word case, (BNWORD32)-(t>>32) is subtracted, rather than
 * adding t>>32, because the shift would need to sign-extend and that's
 * not guaranteed to happen in ANSI C, even with signed types.
 */
#ifndef lbnSubN_32
#ifdef BNWORD64
BNWORD32
lbnSubN_32(BNWORD32 *num1, BNWORD32 const *num2, unsigned len)
{
	BNWORD64 t;

	assert(len > 0);

	t = (BNWORD64)BIGLITTLE(*--num1,*num1) - BIGLITTLE(*--num2,*num2++);
	BIGLITTLE(*num1,*num1++) = (BNWORD32)t;

	while (--len) {
		t = (BNWORD64)BIGLITTLE(*--num1,*num1) -
		    (BNWORD64)BIGLITTLE(*--num2,*num2++) - (BNWORD32)-(t >> 32);
		BIGLITTLE(*num1,*num1++) = (BNWORD32)t;
	}

	return -(BNWORD32)(t>>32);
}
#else
BNWORD32
lbnSubN_32(BNWORD32 *num1, BNWORD32 const *num2, unsigned len)
{
	BNWORD32 x, borrow = 0;

	assert(len > 0);	/* Alternative: change loop to test at start */

	do {
		x = BIGLITTLE(*--num2,*num2++);
		borrow = (x += borrow) < borrow;
		borrow += (BIGLITTLE(*--num1,*num1++) -= x) > (BNWORD32)~x;
	} while (--len);

	return borrow;
}
#endif
#endif /* !lbnSubN_32 */

#ifndef lbnCmp_32
/*
 * lbnCmp_32: compare two bignums of equal length, returning the sign of
 * num1 - num2. (-1, 0 or +1).
 * 
 * Technique: Change the little-endian pointers to big-endian pointers
 * and compare from the most-significant end until a difference if found.
 * When it is, figure out the sign of the difference and return it.
 */
int
lbnCmp_32(BNWORD32 const *num1, BNWORD32 const *num2, unsigned len)
{
	BIGLITTLE(num1 -= len, num1 += len);
	BIGLITTLE(num2 -= len, num2 += len);

	while (len--) {
		if (BIGLITTLE(*num1++ != *num2++, *--num1 != *--num2)) {
			if (BIGLITTLE(num1[-1] < num2[-1], *num1 < *num2))
				return -1;
			else
				return 1;
		}
	}
	return 0;
}
#endif /* !lbnCmp_32 */

/*
 * mul32_ppmmaa(ph,pl,x,y,a,b) is an optional routine that
 * computes (ph,pl) = x * y + a + b.  mul32_ppmma and mul32_ppmm
 * are simpler versions.  If you want to be lazy, all of these
 * can be defined in terms of the others, so here we create any
 * that have not been defined in terms of the ones that have been.
 */

/* Define ones with fewer a's in terms of ones with more a's */
#if !defined(mul32_ppmma) && defined(mul32_ppmmaa)
#define mul32_ppmma(ph,pl,x,y,a) mul32_ppmmaa(ph,pl,x,y,a,0)
#endif

#if !defined(mul32_ppmm) && defined(mul32_ppmma)
#define mul32_ppmm(ph,pl,x,y) mul32_ppmma(ph,pl,x,y,0)
#endif

/*
 * Use this definition to test the mul32_ppmm-based operations on machines
 * that do not provide mul32_ppmm.  Change the final "0" to a "1" to
 * enable it.
 */
#if !defined(mul32_ppmm) && defined(BNWORD64) && 0	/* Debugging */
#define mul32_ppmm(ph,pl,x,y) \
	({BNWORD64 _ = (BNWORD64)(x)*(y); (pl) = _; (ph) = _>>32;})
#endif

#if defined(mul32_ppmm) && !defined(mul32_ppmma)
#define mul32_ppmma(ph,pl,x,y,a) \
	(mul32_ppmm(ph,pl,x,y), (ph) += ((pl) += (a)) < (a))
#endif

#if defined(mul32_ppmma) && !defined(mul32_ppmmaa)
#define mul32_ppmmaa(ph,pl,x,y,a,b) \
	(mul32_ppmma(ph,pl,x,y,a), (ph) += ((pl) += (b)) < (b))
#endif

/*
 * lbnMulN1_32: Multiply an n-word input by a 1-word input and store the
 * n+1-word product.  This uses either the mul32_ppmm and mul32_ppmma
 * macros, or C multiplication with the BNWORD64 type.  This uses mul32_ppmma
 * if available, assuming you won't bother defining it unless you can do
 * better than the normal multiplication.
 */
#ifndef lbnMulN1_32
#ifdef lbnMulAdd1_32	/* If we have this asm primitive, use it. */
void
lbnMulN1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
	lbnZero_32(out, len);
	BIGLITTLE(*(out-len-1),*(out+len)) = lbnMulAdd1_32(out, in, len, k);
}
#elif defined(mul32_ppmm)
void
lbnMulN1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
	BNWORD32 carry, carryin;

	assert(len > 0);

	BIG(--out;--in;);
	mul32_ppmm(carry, *out, *in, k);
	LITTLE(out++;in++;)

	while (--len) {
		BIG(--out;--in;)
		carryin = carry;
		mul32_ppmma(carry, *out, *in, k, carryin);
		LITTLE(out++;in++;)
	}
	BIGLITTLE(*--out,*out) = carry;
}
#elif defined(BNWORD64)
void
lbnMulN1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
	BNWORD64 p;

	assert(len > 0);

	p = (BNWORD64)BIGLITTLE(*--in,*in++) * k;
	BIGLITTLE(*--out,*out++) = (BNWORD32)p;

	while (--len) {
		p = (BNWORD64)BIGLITTLE(*--in,*in++) * k + (BNWORD32)(p >> 32);
		BIGLITTLE(*--out,*out++) = (BNWORD32)p;
	}
	BIGLITTLE(*--out,*out) = (BNWORD32)(p >> 32);
}
#else
#error No 32x32 -> 64 multiply available for 32-bit bignum package
#endif
#endif /* lbnMulN1_32 */

/*
 * lbnMulAdd1_32: Multiply an n-word input by a 1-word input and add the
 * low n words of the product to the destination.  *Returns the n+1st word
 * of the product.*  (That turns out to be more convenient than adding
 * it into the destination and dealing with a possible unit carry out
 * of *that*.)  This uses either the mul32_ppmma and mul32_ppmmaa macros,
 * or C multiplication with the BNWORD64 type.
 *
 * If you're going to write assembly primitives, this is the one to
 * start with.  It is by far the most commonly called function.
 */
#ifndef lbnMulAdd1_32
#if defined(mul32_ppmm)
BNWORD32
lbnMulAdd1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
	BNWORD32 prod, carry, carryin;

	assert(len > 0);

	BIG(--out;--in;);
	carryin = *out;
	mul32_ppmma(carry, *out, *in, k, carryin);
	LITTLE(out++;in++;)

	while (--len) {
		BIG(--out;--in;);
		carryin = carry;
		mul32_ppmmaa(carry, prod, *in, k, carryin, *out);
		*out = prod;
		LITTLE(out++;in++;)
	}

	return carry;
}
#elif defined(BNWORD64)
BNWORD32
lbnMulAdd1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
	BNWORD64 p;

	assert(len > 0);

	p = (BNWORD64)BIGLITTLE(*--in,*in++) * k + BIGLITTLE(*--out,*out);
	BIGLITTLE(*out,*out++) = (BNWORD32)p;

	while (--len) {
		p = (BNWORD64)BIGLITTLE(*--in,*in++) * k +
		    (BNWORD32)(p >> 32) + BIGLITTLE(*--out,*out);
		BIGLITTLE(*out,*out++) = (BNWORD32)p;
	}

	return (BNWORD32)(p >> 32);
}
#else
#error No 32x32 -> 64 multiply available for 32-bit bignum package
#endif
#endif /* lbnMulAdd1_32 */

/*
 * lbnMulSub1_32: Multiply an n-word input by a 1-word input and subtract the
 * n-word product from the destination.  Returns the n+1st word of the product.
 * This uses either the mul32_ppmm and mul32_ppmma macros, or
 * C multiplication with the BNWORD64 type.
 *
 * This is rather uglier than adding, but fortunately it's only used in
 * division which is not used too heavily.
 */
#ifndef lbnMulSub1_32
#if defined(mul32_ppmm)
BNWORD32
lbnMulSub1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
	BNWORD32 prod, carry, carryin;

	assert(len > 0);

	BIG(--in;)
	mul32_ppmm(carry, prod, *in, k);
	LITTLE(in++;)
	carry += (BIGLITTLE(*--out,*out++) -= prod) > (BNWORD32)~prod;

	while (--len) {
		BIG(--in;);
		carryin = carry;
		mul32_ppmma(carry, prod, *in, k, carryin);
		LITTLE(in++;)
		carry += (BIGLITTLE(*--out,*out++) -= prod) > (BNWORD32)~prod;
	}

	return carry;
}
#elif defined(BNWORD64)
BNWORD32
lbnMulSub1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
	BNWORD64 p;
	BNWORD32 carry, t;

	assert(len > 0);

	p = (BNWORD64)BIGLITTLE(*--in,*in++) * k;
	t = BIGLITTLE(*--out,*out);
	carry = (BNWORD32)(p>>32) + ((BIGLITTLE(*out,*out++)=t-(BNWORD32)p) > t);

	while (--len) {
		p = (BNWORD64)BIGLITTLE(*--in,*in++) * k + carry;
		t = BIGLITTLE(*--out,*out);
		carry = (BNWORD32)(p>>32) +
			( (BIGLITTLE(*out,*out++)=t-(BNWORD32)p) > t );
	}

	return carry;
}
#else
#error No 32x32 -> 64 multiply available for 32-bit bignum package
#endif
#endif /* !lbnMulSub1_32 */

/*
 * Shift n words left "shift" bits.  0 < shift < 32.  Returns the
 * carry, any bits shifted off the left-hand side (0 <= carry < 2^shift).
 */
#ifndef lbnLshift_32
BNWORD32
lbnLshift_32(BNWORD32 *num, unsigned len, unsigned shift)
{
	BNWORD32 x, carry;

	assert(shift > 0);
	assert(shift < 32);

	carry = 0;
	while (len--) {
		BIG(--num;)
		x = *num;
		*num = (x<<shift) | carry;
		LITTLE(num++;)
		carry = x >> (32-shift);
	}
	return carry;
}
#endif /* !lbnLshift_32 */

/*
 * An optimized version of the above, for shifts of 1.
 * Some machines can use add-with-carry tricks for this.
 */
#ifndef lbnDouble_32
BNWORD32
lbnDouble_32(BNWORD32 *num, unsigned len)
{
	BNWORD32 x, carry;

	carry = 0;
	while (len--) {
		BIG(--num;)
		x = *num;
		*num = (x<<1) | carry;
		LITTLE(num++;)
		carry = x >> (32-1);
	}
	return carry;
}
#endif /* !lbnDouble_32 */

/*
 * Shift n words right "shift" bits.  0 < shift < 32.  Returns the
 * carry, any bits shifted off the right-hand side (0 <= carry < 2^shift).
 */
#ifndef lbnRshift_32
BNWORD32
lbnRshift_32(BNWORD32 *num, unsigned len, unsigned shift)
{
	BNWORD32 x, carry = 0;

	assert(shift > 0);
	assert(shift < 32);

	BIGLITTLE(num -= len, num += len);

	while (len--) {
		LITTLE(--num;)
		x = *num;
		*num = (x>>shift) | carry;
		BIG(num++;)
		carry = x << (32-shift);
	}
	return carry >> (32-shift);
}
#endif /* !lbnRshift_32 */

/* 
 * Multiply two numbers of the given lengths.  prod and num2 may overlap,
 * provided that the low len1 bits of prod are free.  (This corresponds
 * nicely to the place the result is returned from lbnMontReduce_32.)
 *
 * TODO: Use Karatsuba multiply.  The overlap constraints may have
 * to get rewhacked.
 */
#ifndef lbnMul_32
void
lbnMul_32(BNWORD32 *prod, BNWORD32 const *num1, unsigned len1,
                          BNWORD32 const *num2, unsigned len2)
{
	/* Special case of zero */
	if (!len1 || !len2) {
		lbnZero_32(prod, len1+len2);
		return;
	}

	/* Multiply first word */
	lbnMulN1_32(prod, num1, len1, BIGLITTLE(*--num2,*num2++));

	/*
	 * Add in subsequent words, storing the most significant word,
	 * which is new each time.
	 */
	while (--len2) {
		BIGLITTLE(--prod,prod++);
		BIGLITTLE(*(prod-len1-1),*(prod+len1)) =
		    lbnMulAdd1_32(prod, num1, len1, BIGLITTLE(*--num2,*num2++));
	}
}
#endif /* !lbnMul_32 */

/*
 * lbnMulX_32 is a square multiply - both inputs are the same length.
 * It's normally just a macro wrapper around the general multiply,
 * but might be implementable in assembly more efficiently (such as
 * when product scanning).
 */
#ifndef lbnMulX_32
#if defined(BNWORD64) && PRODUCT_SCAN
/*
 * Test code to see whether product scanning is any faster.  It seems
 * to make the C code slower, so PRODUCT_SCAN is not defined.
 */
static void
lbnMulX_32(BNWORD32 *prod, BNWORD32 const *num1, BNWORD32 const *num2,
	unsigned len)
{
	BNWORD64 x, y;
	BNWORD32 const *p1, *p2;
	unsigned carry;
	unsigned i, j;

	/* Special case of zero */
	if (!len)
		return;

	x = (BNWORD64)BIGLITTLE(num1[-1] * num2[-1], num1[0] * num2[0]);
	BIGLITTLE(*--prod, *prod++) = (BNWORD32)x;
	x >>= 32;

	for (i = 1; i < len; i++) {
		carry = 0;
		p1 = num1;
		p2 = BIGLITTLE(num2-i-1,num2+i+1);
		for (j = 0; j <= i; j++) {
			BIG(y = (BNWORD64)*--p1 * *p2++;)
			LITTLE(y = (BNWORD64)*p1++ * *--p2;)
			x += y;
			carry += (x < y);
		}
		BIGLITTLE(*--prod,*prod++) = (BNWORD32)x;
		x = (x >> 32) | (BNWORD64)carry << 32;
	}
	for (i = 1; i < len; i++) {
		carry = 0;
		p1 = BIGLITTLE(num1-i,num1+i);
		p2 = BIGLITTLE(num2-len,num2+len);
		for (j = i; j < len; j++) {
			BIG(y = (BNWORD64)*--p1 * *p2++;)
			LITTLE(y = (BNWORD64)*p1++ * *--p2;)
			x += y;
			carry += (x < y);
		}
		BIGLITTLE(*--prod,*prod++) = (BNWORD32)x;
		x = (x >> 32) | (BNWORD64)carry << 32;
	}
	
	BIGLITTLE(*--prod,*prod) = (BNWORD32)x;
}
#else /* !defined(BNWORD64) || !PRODUCT_SCAN */
/* Default trivial macro definition */
#define lbnMulX_32(prod, num1, num2, len) lbnMul_32(prod, num1, len, num2, len)
#endif /* !defined(BNWORD64) || !PRODUCT_SCAN */
#endif /* !lbmMulX_32 */

#if !defined(lbnMontMul_32) && defined(BNWORD64) && PRODUCT_SCAN
/*
 * Test code for product-scanning multiply.  This seems to slow the C
 * code down rather than speed it up.
 * This does a multiply and Montgomery reduction together, using the
 * same loops.  The outer loop scans across the product, twice.
 * The first pass computes the low half of the product and the
 * Montgomery multipliers.  These are stored in the product array,
 * which contains no data as of yet.  x and carry add up the columns
 * and propagate carries forward.
 *
 * The second half multiplies the upper half, adding in the modulus
 * times the Montgomery multipliers.  The results of this multiply
 * are stored.
 */
static void
lbnMontMul_32(BNWORD32 *prod, BNWORD32 const *num1, BNWORD32 const *num2,
	BNWORD32 const *mod, unsigned len, BNWORD32 inv)
{
	BNWORD64 x, y;
	BNWORD32 const *p1, *p2, *pm;
	BNWORD32 *pp;
	BNWORD32 t;
	unsigned carry;
	unsigned i, j;

	/* Special case of zero */
	if (!len)
		return;

	/*
	 * This computes directly into the high half of prod, so just
	 * shift the pointer and consider prod only "len" elements long
	 * for the rest of the code.
	 */
	BIGLITTLE(prod -= len, prod += len);

	/* Pass 1 - compute Montgomery multipliers */
	/* First iteration can have certain simplifications. */
	x = (BNWORD64)BIGLITTLE(num1[-1] * num2[-1], num1[0] * num2[0]);
	BIGLITTLE(prod[-1], prod[0]) = t = inv * (BNWORD32)x;
	y = (BNWORD64)t * BIGLITTLE(mod[-1],mod[0]);
	x += y;
	/* Note: GCC 2.6.3 has a bug if you try to eliminate "carry" */
	carry = (x < y);
	assert((BNWORD32)x == 0);
	x = x >> 32 | (BNWORD64)carry << 32;

	for (i = 1; i < len; i++) {
		carry = 0;
		p1 = num1;
		p2 = BIGLITTLE(num2-i-1,num2+i+1);
		pp = prod;
		pm = BIGLITTLE(mod-i-1,mod+i+1);
		for (j = 0; j < i; j++) {
			y = (BNWORD64)BIGLITTLE(*--p1 * *p2++, *p1++ * *--p2);
			x += y;
			carry += (x < y);
			y = (BNWORD64)BIGLITTLE(*--pp * *pm++, *pp++ * *--pm);
			x += y;
			carry += (x < y);
		}
		y = (BNWORD64)BIGLITTLE(p1[-1] * p2[0], p1[0] * p2[-1]);
		x += y;
		carry += (x < y);
		assert(BIGLITTLE(pp == prod-i, pp == prod+i));
		BIGLITTLE(pp[-1], pp[0]) = t = inv * (BNWORD32)x;
		assert(BIGLITTLE(pm == mod-1, pm == mod+1));
		y = (BNWORD64)t * BIGLITTLE(pm[0],pm[-1]);
		x += y;
		carry += (x < y);
		assert((BNWORD32)x == 0);
		x = x >> 32 | (BNWORD64)carry << 32;
	}

	/* Pass 2 - compute reduced product and store */
	for (i = 1; i < len; i++) {
		carry = 0;
		p1 = BIGLITTLE(num1-i,num1+i);
		p2 = BIGLITTLE(num2-len,num2+len);
		pm = BIGLITTLE(mod-i,mod+i);
		pp = BIGLITTLE(prod-len,prod+len);
		for (j = i; j < len; j++) {
			y = (BNWORD64)BIGLITTLE(*--p1 * *p2++, *p1++ * *--p2);
			x += y;
			carry += (x < y);
			y = (BNWORD64)BIGLITTLE(*--pm * *pp++, *pm++ * *--pp);
			x += y;
			carry += (x < y);
		}
		assert(BIGLITTLE(pm == mod-len, pm == mod+len));
		assert(BIGLITTLE(pp == prod-i, pp == prod+i));
		BIGLITTLE(pp[0],pp[-1]) = (BNWORD32)x;
		x = (x >> 32) | (BNWORD64)carry << 32;
	}

	/* Last round of second half, simplified. */
	BIGLITTLE(*(prod-len),*(prod+len-1)) = (BNWORD32)x;
	carry = (x >> 32);

	while (carry)
		carry -= lbnSubN_32(prod, mod, len);
	while (lbnCmp_32(prod, mod, len) >= 0)
		(void)lbnSubN_32(prod, mod, len);
}
/* Suppress later definition */
#define lbnMontMul_32 lbnMontMul_32
#endif

#if !defined(lbnSquare_32) && defined(BNWORD64) && PRODUCT_SCAN
/*
 * Trial code for product-scanning squaring.  This seems to slow the C
 * code down rather than speed it up.
 */
void
lbnSquare_32(BNWORD32 *prod, BNWORD32 const *num, unsigned len)
{
	BNWORD64 x, y, z;
	BNWORD32 const *p1, *p2;
	unsigned carry;
	unsigned i, j;

	/* Special case of zero */
	if (!len)
		return;

	/* Word 0 of product */
	x = (BNWORD64)BIGLITTLE(num[-1] * num[-1], num[0] * num[0]);
	BIGLITTLE(*--prod, *prod++) = (BNWORD32)x;
	x >>= 32;

	/* Words 1 through len-1 */
	for (i = 1; i < len; i++) {
		carry = 0;
		y = 0;
		p1 = num;
		p2 = BIGLITTLE(num-i-1,num+i+1);
		for (j = 0; j < (i+1)/2; j++) {
			BIG(z = (BNWORD64)*--p1 * *p2++;)
			LITTLE(z = (BNWORD64)*p1++ * *--p2;)
			y += z;
			carry += (y < z);
		}
		y += z = y;
		carry += carry + (y < z);
		if ((i & 1) == 0) {
			assert(BIGLITTLE(--p1 == p2, p1 == --p2));
			BIG(z = (BNWORD64)*p2 * *p2;)
			LITTLE(z = (BNWORD64)*p1 * *p1;)
			y += z;
			carry += (y < z);
		}
		x += y;
		carry += (x < y);
		BIGLITTLE(*--prod,*prod++) = (BNWORD32)x;
		x = (x >> 32) | (BNWORD64)carry << 32;
	}
	/* Words len through 2*len-2 */
	for (i = 1; i < len; i++) {
		carry = 0;
		y = 0;
		p1 = BIGLITTLE(num-i,num+i);
		p2 = BIGLITTLE(num-len,num+len);
		for (j = 0; j < (len-i)/2; j++) {
			BIG(z = (BNWORD64)*--p1 * *p2++;)
			LITTLE(z = (BNWORD64)*p1++ * *--p2;)
			y += z;
			carry += (y < z);
		}
		y += z = y;
		carry += carry + (y < z);
		if ((len-i) & 1) {
			assert(BIGLITTLE(--p1 == p2, p1 == --p2));
			BIG(z = (BNWORD64)*p2 * *p2;)
			LITTLE(z = (BNWORD64)*p1 * *p1;)
			y += z;
			carry += (y < z);
		}
		x += y;
		carry += (x < y);
		BIGLITTLE(*--prod,*prod++) = (BNWORD32)x;
		x = (x >> 32) | (BNWORD64)carry << 32;
	}
	
	/* Word 2*len-1 */
	BIGLITTLE(*--prod,*prod) = (BNWORD32)x;
}
/* Suppress later definition */
#define lbnSquare_32 lbnSquare_32
#endif

/*
 * Square a number, using optimized squaring to reduce the number of
 * primitive multiples that are executed.  There may not be any
 * overlap of the input and output.
 *
 * Technique: Consider the partial products in the multiplication
 * of "abcde" by itself:
 *
 *               a  b  c  d  e
 *            *  a  b  c  d  e
 *          ==================
 *              ae be ce de ee
 *           ad bd cd dd de
 *        ac bc cc cd ce
 *     ab bb bc bd be
 *  aa ab ac ad ae
 *
 * Note that everything above the main diagonal:
 *              ae be ce de = (abcd) * e
 *           ad bd cd       = (abc) * d
 *        ac bc             = (ab) * c
 *     ab                   = (a) * b
 *
 * is a copy of everything below the main diagonal:
 *                       de
 *                 cd ce
 *           bc bd be
 *     ab ac ad ae
 *
 * Thus, the sum is 2 * (off the diagonal) + diagonal.
 *
 * This is accumulated beginning with the diagonal (which
 * consist of the squares of the digits of the input), which is then
 * divided by two, the off-diagonal added, and multiplied by two
 * again.  The low bit is simply a copy of the low bit of the
 * input, so it doesn't need special care.
 *
 * TODO: Merge the shift by 1 with the squaring loop.
 * TODO: Use Karatsuba.  (a*W+b)^2 = a^2 * (W^2+W) + b^2 * (W+1) - (a-b)^2 * W.
 */
#ifndef lbnSquare_32
void
lbnSquare_32(BNWORD32 *prod, BNWORD32 const *num, unsigned len)
{
	BNWORD32 t;
	BNWORD32 *prodx = prod;		/* Working copy of the argument */
	BNWORD32 const *numx = num;	/* Working copy of the argument */
	unsigned lenx = len;		/* Working copy of the argument */

	if (!len)
		return;

	/* First, store all the squares */
	while (lenx--) {
#ifdef mul32_ppmm
		BNWORD32 ph, pl;
		t = BIGLITTLE(*--numx,*numx++);
		mul32_ppmm(ph,pl,t,t);
		BIGLITTLE(*--prodx,*prodx++) = pl;
		BIGLITTLE(*--prodx,*prodx++) = ph;
#elif defined(BNWORD64) /* use BNWORD64 */
		BNWORD64 p;
		t = BIGLITTLE(*--numx,*numx++);
		p = (BNWORD64)t * t;
		BIGLITTLE(*--prodx,*prodx++) = (BNWORD32)p;
		BIGLITTLE(*--prodx,*prodx++) = (BNWORD32)(p>>32);
#else	/* Use lbnMulN1_32 */
		t = BIGLITTLE(numx[-1],*numx);
		lbnMulN1_32(prodx, numx, 1, t);
		BIGLITTLE(--numx,numx++);
		BIGLITTLE(prodx -= 2, prodx += 2);
#endif
	}
	/* Then, shift right 1 bit */
	(void)lbnRshift_32(prod, 2*len, 1);

	/* Then, add in the off-diagonal sums */
	lenx = len;
	numx = num;
	prodx = prod;
	while (--lenx) {
		t = BIGLITTLE(*--numx,*numx++);
		BIGLITTLE(--prodx,prodx++);
		t = lbnMulAdd1_32(prodx, numx, lenx, t);
		lbnAdd1_32(BIGLITTLE(prodx-lenx,prodx+lenx), lenx+1, t);
		BIGLITTLE(--prodx,prodx++);
	}

	/* Shift it back up */
	lbnDouble_32(prod, 2*len);

	/* And set the low bit appropriately */
	BIGLITTLE(prod[-1],prod[0]) |= BIGLITTLE(num[-1],num[0]) & 1;
}
#endif /* !lbnSquare_32 */

/*
 * lbnNorm_32 - given a number, return a modified length such that the
 * most significant digit is non-zero.  Zero-length input is okay.
 */
#ifndef lbnNorm_32
unsigned
lbnNorm_32(BNWORD32 const *num, unsigned len)
{
	BIGLITTLE(num -= len,num += len);
	while (len && BIGLITTLE(*num++,*--num) == 0)
		--len;
	return len;
}
#endif /* lbnNorm_32 */

/*
 * lbnBits_32 - return the number of significant bits in the array.
 * It starts by normalizing the array.  Zero-length input is okay.
 * Then assuming there's anything to it, it fetches the high word,
 * generates a bit length by multiplying the word length by 32, and
 * subtracts off 32/2, 32/4, 32/8, ... bits if the high bits are clear.
 */
#ifndef lbnBits_32
unsigned
lbnBits_32(BNWORD32 const *num, unsigned len)
{
	BNWORD32 t;
	unsigned i;

	len = lbnNorm_32(num, len);
	if (len) {
		t = BIGLITTLE(*(num-len),*(num+(len-1)));
		assert(t);
		len *= 32;
		i = 32/2;
		do {
			if (t >> i)
				t >>= i;
			else
				len -= i;
		} while ((i /= 2) != 0);
	}
	return len;
}
#endif /* lbnBits_32 */

/*
 * If defined, use hand-rolled divide rather than compiler's native.
 * If the machine doesn't do it in line, the manual code is probably
 * faster, since it can assume normalization and the fact that the
 * quotient will fit into 32 bits, which a general 64-bit divide
 * in a compiler's run-time library can't do.
 */
#ifndef BN_SLOW_DIVIDE_64
/* Assume that divisors of more than thirty-two bits are slow */
#define BN_SLOW_DIVIDE_64 (64 > 0x20)
#endif

/*
 * Return (nh<<32|nl) % d, and place the quotient digit into *q.
 * It is guaranteed that nh < d, and that d is normalized (with its high
 * bit set).  If we have a double-width type, it's easy.  If not, ooh,
 * yuk!
 */
#ifndef lbnDiv21_32
#if defined(BNWORD64) && !BN_SLOW_DIVIDE_64
BNWORD32
lbnDiv21_32(BNWORD32 *q, BNWORD32 nh, BNWORD32 nl, BNWORD32 d)
{
	BNWORD64 n = (BNWORD64)nh << 32 | nl;

	/* Divisor must be normalized */
	assert(d >> (32-1) == 1);

	*q = n / d;
	return n % d;
}
#else
/*
 * This is where it gets ugly.
 *
 * Do the division in two halves, using Algorithm D from section 4.3.1
 * of Knuth.  Note Theorem B from that section, that the quotient estimate
 * is never more than the true quotient, and is never more than two
 * too low.
 *
 * The mapping onto conventional long division is (everything a half word):
 *        _____________qh___ql_
 * dh dl ) nh.h nh.l nl.h nl.l
 *             - (qh * d)
 *            -----------
 *              rrrr rrrr nl.l
 *                  - (ql * d)
 *                -----------
 *                  rrrr rrrr
 *
 * The implicit 3/2-digit d*qh and d*ql subtractors are computed this way:
 *   First, estimate a q digit so that nh/dh works.  Subtracting qh*dh from
 *   the (nh.h nh.l) list leaves a 1/2-word remainder r.  Then compute the
 *   low part of the subtractor, qh * dl.   This also needs to be subtracted
 *   from (nh.h nh.l nl.h) to get the final remainder.  So we take the
 *   remainder, which is (nh.h nh.l) - qh*dl, shift it and add in nl.h, and
 *   try to subtract qh * dl from that.  Since the remainder is 1/2-word
 *   long, shifting and adding nl.h results in a single word r.
 *   It is possible that the remainder we're working with, r, is less than
 *   the product qh * dl, if we estimated qh too high.  The estimation
 *   technique can produce a qh that is too large (never too small), leading
 *   to r which is too small.  In that case, decrement the digit qh, add
 *   shifted dh to r (to correct for that error), and subtract dl from the
 *   product we're comparing r with.  That's the "correct" way to do it, but
 *   just adding dl to r instead of subtracting it from the product is
 *   equivalent and a lot simpler.  You just have to watch out for overflow.
 *
 *   The process is repeated with (rrrr rrrr nl.l) for the low digit of the
 *   quotient ql.
 *
 * The various uses of 32/2 for shifts are because of the note about
 * automatic editing of this file at the very top of the file.
 */
#define highhalf(x) ( (x) >> 32/2 )
#define lowhalf(x) ( (x) & (((BNWORD32)1 << 32/2)-1) )
BNWORD32
lbnDiv21_32(BNWORD32 *q, BNWORD32 nh, BNWORD32 nl, BNWORD32 d)
{
	BNWORD32 dh = highhalf(d), dl = lowhalf(d);
	BNWORD32 qh, ql, prod, r;

	/* Divisor must be normalized */
	assert((d >> (32-1)) == 1);

	/* Do first half-word of division */
	qh = nh / dh;
	r = nh % dh;
	prod = qh * dl;

	/*
	 * Add next half-word of numerator to remainder and correct.
	 * qh may be up to two too large.
	 */
	r = (r << (32/2)) | highhalf(nl);
	if (r < prod) {
		--qh; r += d;
		if (r >= d && r < prod) {
			--qh; r += d; 
		}
	}
	r -= prod;

	/* Do second half-word of division */
	ql = r / dh;
	r = r % dh;
	prod = ql * dl;

	r = (r << (32/2)) | lowhalf(nl);
	if (r < prod) {
		--ql; r += d;
		if (r >= d && r < prod) {
			--ql; r += d;
		}
	}
	r -= prod;

	*q = (qh << (32/2)) | ql;

	return r;
}
#endif
#endif /* lbnDiv21_32 */


/*
 * In the division functions, the dividend and divisor are referred to
 * as "n" and "d", which stand for "numerator" and "denominator".
 *
 * The quotient is (nlen-dlen+1) digits long.  It may be overlapped with
 * the high (nlen-dlen) words of the dividend, but one extra word is needed
 * on top to hold the top word.
 */

/*
 * Divide an n-word number by a 1-word number, storing the remainder
 * and n-1 words of the n-word quotient.  The high word is returned.
 * It IS legal for rem to point to the same address as n, and for
 * q to point one word higher.
 *
 * TODO: If BN_SLOW_DIVIDE_64, add a divnhalf_32 which uses 32-bit
 *       dividends if the divisor is half that long.
 * TODO: Shift the dividend on the fly to avoid the last division and
 *       instead have a remainder that needs shifting.
 * TODO: Use reciprocals rather than dividing.
 */
#ifndef lbnDiv1_32
BNWORD32
lbnDiv1_32(BNWORD32 *q, BNWORD32 *rem, BNWORD32 const *n, unsigned len,
	BNWORD32 d)
{
	unsigned shift;
	unsigned xlen;
	BNWORD32 r;
	BNWORD32 qhigh;

	assert(len > 0);
	assert(d);

	if (len == 1) {
		r = *n;
		*rem = r%d;
		return r/d;
	}

	shift = 0;
	r = d;
	xlen = 32/2;
	do {
		if (r >> xlen)
			r >>= xlen;
		else
			shift += xlen;
	} while ((xlen /= 2) != 0);
	assert((d >> (32-1-shift)) == 1);
	d <<= shift;

	BIGLITTLE(q -= len-1,q += len-1);
	BIGLITTLE(n -= len,n += len);

	r = BIGLITTLE(*n++,*--n);
	if (r < d) {
		qhigh = 0;
	} else {
		qhigh = r/d;
		r %= d;
	}

	xlen = len;
	while (--xlen)
		r = lbnDiv21_32(BIGLITTLE(q++,--q), r, BIGLITTLE(*n++,*--n), d);

	/*
	 * Final correction for shift - shift the quotient up "shift"
	 * bits, and merge in the extra bits of quotient.  Then reduce
	 * the final remainder mod the real d.
	 */
	if (shift) {
		d >>= shift;
		qhigh = (qhigh << shift) | lbnLshift_32(q, len-1, shift);
		BIGLITTLE(q[-1],*q) |= r/d;
		r %= d;
	}
	*rem = r;

	return qhigh;
}
#endif

/*
 * This function performs a "quick" modulus of a number with a divisor
 * d which is guaranteed to be at most sixteen bits, i.e. less than 65536.
 * This applies regardless of the word size the library is compiled with.
 *
 * This function is important to prime generation, for sieving.
 */
#ifndef lbnModQ_32
/* If there's a custom lbnMod21_32, no normalization needed */
#ifdef lbnMod21_32
unsigned
lbnModQ_32(BNWORD32 const *n, unsigned len, unsigned d)
{
	unsigned i, shift;
	BNWORD32 r;

	assert(len > 0);

	BIGLITTLE(n -= len,n += len);

	/* Try using a compare to avoid the first divide */
	r = BIGLITTLE(*n++,*--n);
	if (r >= d)
		r %= d;
	while (--len)
		r = lbnMod21_32(r, BIGLITTLE(*n++,*--n), d);

	return r;
}
#elif defined(BNWORD64) && !BN_SLOW_DIVIDE_64
unsigned
lbnModQ_32(BNWORD32 const *n, unsigned len, unsigned d)
{
	BNWORD32 r;

	if (!--len)
		return BIGLITTLE(n[-1],n[0]) % d;

	BIGLITTLE(n -= len,n += len);
	r = BIGLITTLE(n[-1],n[0]);

	do {
		r = (BNWORD32)((((BNWORD64)r<<32) | BIGLITTLE(*n++,*--n)) % d);
	} while (--len);

	return r;
}
#elif 32 >= 0x20
/*
 * If the single word size can hold 65535*65536, then this function
 * is avilable.
 */
#ifndef highhalf
#define highhalf(x) ( (x) >> 32/2 )
#define lowhalf(x) ( (x) & ((1 << 32/2)-1) )
#endif
unsigned
lbnModQ_32(BNWORD32 const *n, unsigned len, unsigned d)
{
	BNWORD32 r, x;

	BIGLITTLE(n -= len,n += len);

	r = BIGLITTLE(*n++,*--n);
	while (--len) {
		x = BIGLITTLE(*n++,*--n);
		r = (r%d << 32/2) | highhalf(x);
		r = (r%d << 32/2) | lowhalf(x);
	}

	return r%d;
}
#else
/* Default case - use lbnDiv21_32 */
unsigned
lbnModQ_32(BNWORD32 const *n, unsigned len, unsigned d)
{
	unsigned i, shift;
	BNWORD32 r;
	BNWORD32 q;

	assert(len > 0);

	shift = 0;
	r = d;
	i = 32;
	while (i /= 2) {
		if (r >> i)
			r >>= i;
		else
			shift += i;
	}
	assert(d >> (32-1-shift) == 1);
	d <<= shift;

	BIGLITTLE(n -= len,n += len);

	r = BIGLITTLE(*n++,*--n);
	if (r >= d)
		r %= d;

	while (--len)
		r = lbnDiv21_32(&q, r, BIGLITTLE(*n++,*--n), d);

	/*
	 * Final correction for shift - shift the quotient up "shift"
	 * bits, and merge in the extra bits of quotient.  Then reduce
	 * the final remainder mod the real d.
	 */
	if (shift)
		r %= d >> shift;

	return r;
}
#endif
#endif /* lbnModQ_32 */

/*
 * Reduce n mod d and return the quotient.  That is, find:
 * q = n / d;
 * n = n % d;
 * d is altered during the execution of this subroutine by normalizing it.
 * It must already have its most significant word non-zero; it is shifted
 * so its most significant bit is non-zero.
 *
 * The quotient q is nlen-dlen+1 words long.  To make it possible to
 * overlap the quptient with the input (you can store it in the high dlen
 * words), the high word of the quotient is *not* stored, but is returned.
 * (If all you want is the remainder, you don't care about it, anyway.)
 *
 * This uses algorithm D from Knuth (4.3.1), except that we do binary
 * (shift) normalization of the divisor.  WARNING: This is hairy!
 *
 * This function is used for some modular reduction, but it is not used in
 * the modular exponentiation loops; they use Montgomery form and the
 * corresponding, more efficient, Montgomery reduction.  This code
 * is needed for the conversion to Montgomery form, however, so it
 * has to be here and it might as well be reasonably efficient.
 *
 * The overall operation is as follows ("top" and "up" refer to the
 * most significant end of the number; "bottom" and "down", the least):
 *
 * - Shift the divisor up until the most significant bit is set.
 * - Shift the dividend up the same amount.  This will produce the
 *   correct quotient, and the remainder can be recovered by shifting
 *   it back down the same number of bits.  This may produce an overflow
 *   word, but the word is always strictly less than the most significant
 *   divisor word.
 * - Estimate the first quotient digit qhat:
 *   - First take the top two words (one of which is the overflow) of the
 *     dividend and divide by the top word of the divisor:
 *     qhat = (nh,nm)/dh.  This qhat is >= the correct quotient digit
 *     and, since dh is normalized, it is at most two over.
 *   - Second, correct by comparing the top three words.  If
 *     (dh,dl) * qhat > (nh,nm,ml), decrease qhat and try again.
 *     The second iteration can be simpler because there can't be a third.
 *     The computation can be simplified by subtracting dh*qhat from
 *     both sides, suitably shifted.  This reduces the left side to
 *     dl*qhat.  On the right, (nh,nm)-dh*qhat is simply the
 *     remainder r from (nh,nm)%dh, so the right is (r,nl).
 *     This produces qhat that is almost always correct and at
 *     most (prob ~ 2/2^32) one too high.
 * - Subtract qhat times the divisor (suitably shifted) from the dividend.
 *   If there is a borrow, qhat was wrong, so decrement it
 *   and add the divisor back in (once).
 * - Store the final quotient digit qhat in the quotient array q.
 *
 * Repeat the quotient digit computation for successive digits of the
 * quotient until the whole quotient has been computed.  Then shift the
 * divisor and the remainder down to correct for the normalization.
 *
 * TODO: Special case 2-word divisors.
 * TODO: Use reciprocals rather than dividing.
 */
#ifndef divn_32
BNWORD32
lbnDiv_32(BNWORD32 *q, BNWORD32 *n, unsigned nlen, BNWORD32 *d, unsigned dlen)
{
	BNWORD32 nh,nm,nl;	/* Top three words of the dividend */
	BNWORD32 dh,dl;	/* Top two words of the divisor */
	BNWORD32 qhat;	/* Extimate of quotient word */
	BNWORD32 r;	/* Remainder from quotient estimate division */
	BNWORD32 qhigh;	/* High word of quotient */
	unsigned i;	/* Temp */
	unsigned shift;	/* Bits shifted by normalization */
	unsigned qlen = nlen-dlen; /* Size of quotient (less 1) */
#ifdef mul32_ppmm
	BNWORD32 t32;
#elif defined(BNWORD64)
	BNWORD64 t64;
#else /* use lbnMulN1_32 */
	BNWORD32 t2[2];
#define t2high BIGLITTLE(t2[0],t2[1])
#define t2low BIGLITTLE(t2[1],t2[0])
#endif

	assert(dlen);
	assert(nlen >= dlen);

	/*
	 * Special cases for short divisors.  The general case uses the
	 * top top 2 digits of the divisor (d) to estimate a quotient digit,
	 * so it breaks if there are fewer digits available.  Thus, we need
	 * special cases for a divisor of length 1.  A divisor of length
	 * 2 can have a *lot* of administrivia overhead removed removed,
	 * so it's probably worth special-casing that case, too.
	 */
	if (dlen == 1)
		return lbnDiv1_32(q, BIGLITTLE(n-1,n), n, nlen,
		                  BIGLITTLE(d[-1],d[0]));

#if 0
	/*
	 * @@@ This is not yet written...  The general loop will do,
	 * albeit less efficiently
	 */
	if (dlen == 2) {
		/*
		 * divisor two digits long:
		 * use the 3/2 technique from Knuth, but we know
		 * it's exact.
		 */
		dh = BIGLITTLE(d[-1],d[0]);
		dl = BIGLITTLE(d[-2],d[1]);
		shift = 0;
		if ((sh & ((BNWORD32)1 << 32-1-shift)) == 0) {
			do {
				shift++;
			} while (dh & (BNWORD32)1<<32-1-shift) == 0);
			dh = dh << shift | dl >> (32-shift);
			dl <<= shift;


		}


		for (shift = 0; (dh & (BNWORD32)1 << 32-1-shift)) == 0; shift++)
			;
		if (shift) {
		}
		dh = dh << shift | dl >> (32-shift);
		shift = 0;
		while (dh
	}
#endif

	dh = BIGLITTLE(*(d-dlen),*(d+(dlen-1)));
	assert(dh);

	/* Normalize the divisor */
	shift = 0;
	r = dh;
	i = 32/2;
	do {
		if (r >> i)
			r >>= i;
		else
			shift += i;
	} while ((i /= 2) != 0);

	nh = 0;
	if (shift) {
		lbnLshift_32(d, dlen, shift);
		dh = BIGLITTLE(*(d-dlen),*(d+(dlen-1)));
		nh = lbnLshift_32(n, nlen, shift);
	}

	/* Assert that dh is now normalized */
	assert(dh >> (32-1));

	/* Also get the second-most significant word of the divisor */
	dl = BIGLITTLE(*(d-(dlen-1)),*(d+(dlen-2)));

	/*
	 * Adjust pointers: n to point to least significant end of first
	 * first subtract, and q to one the most-significant end of the
	 * quotient array.
	 */
	BIGLITTLE(n -= qlen,n += qlen);
	BIGLITTLE(q -= qlen,q += qlen);

	/* Fetch the most significant stored word of the dividend */
	nm = BIGLITTLE(*(n-dlen),*(n+(dlen-1)));

	/*
	 * Compute the first digit of the quotient, based on the
	 * first two words of the dividend (the most significant of which
	 * is the overflow word h).
	 */
	if (nh) {
		assert(nh < dh);
		r = lbnDiv21_32(&qhat, nh, nm, dh);
	} else if (nm >= dh) {
		qhat = nm/dh;
		r = nm % dh;
	} else {	/* Quotient is zero */
		qhigh = 0;
		goto divloop;
	}

	/* Now get the third most significant word of the dividend */
	nl = BIGLITTLE(*(n-(dlen-1)),*(n+(dlen-2)));

	/*
	 * Correct qhat, the estimate of quotient digit.
	 * qhat can only be high, and at most two words high,
	 * so the loop can be unrolled and abbreviated.
	 */
#ifdef mul32_ppmm
	mul32_ppmm(nm, t32, qhat, dl);
	if (nm > r || (nm == r && t32 > nl)) {
		/* Decrement qhat and adjust comparison parameters */
		qhat--;
		if ((r += dh) >= dh) {
			nm -= (t32 < dl);
			t32 -= dl;
			if (nm > r || (nm == r && t32 > nl))
				qhat--;
		}
	}
#elif defined(BNWORD64)
	t64 = (BNWORD64)qhat * dl;
	if (t64 > ((BNWORD64)r << 32) + nl) {
		/* Decrement qhat and adjust comparison parameters */
		qhat--;
		if ((r += dh) > dh) {
			t64 -= dl;
			if (t64 > ((BNWORD64)r << 32) + nl)
				qhat--;
		}
	}
#else /* Use lbnMulN1_32 */
	lbnMulN1_32(BIGLITTLE(t2+2,t2), &dl, 1, qhat);
	if (t2high > r || (t2high == r && t2low > nl)) {
		/* Decrement qhat and adjust comparison parameters */
		qhat--;
		if ((r += dh) >= dh) {
			t2high -= (t2low < dl);
			t2low -= dl;
			if (t2high > r || (t2high == r && t2low > nl))
				qhat--;
		}
	}
#endif

	/* Do the multiply and subtract */
	r = lbnMulSub1_32(n, d, dlen, qhat);
	/* If there was a borrow, add back once. */
	if (r > nh) {	/* Borrow? */
		(void)lbnAddN_32(n, d, dlen);
		qhat--;
	}

	/* Remember the first quotient digit. */
	qhigh = qhat;

	/* Now, the main division loop: */
divloop:
	while (qlen--) {

		/* Advance n */
		nh = BIGLITTLE(*(n-dlen),*(n+(dlen-1)));
		BIGLITTLE(++n,--n);
		nm = BIGLITTLE(*(n-dlen),*(n+(dlen-1)));

		if (nh == dh) {
			qhat = ~(BNWORD32)0;
			/* Optimized computation of r = (nh,nm) - qhat * dh */
			r = nh + nm;
			if (r < nh)
				goto subtract;
		} else {
			assert(nh < dh);
			r = lbnDiv21_32(&qhat, nh, nm, dh);
		}

		nl = BIGLITTLE(*(n-(dlen-1)),*(n+(dlen-2)));
#ifdef mul32_ppmm
		mul32_ppmm(nm, t32, qhat, dl);
		if (nm > r || (nm == r && t32 > nl)) {
			/* Decrement qhat and adjust comparison parameters */
			qhat--;
			if ((r += dh) >= dh) {
				nm -= (t32 < dl);
				t32 -= dl;
				if (nm > r || (nm == r && t32 > nl))
					qhat--;
			}
		}
#elif defined(BNWORD64)
		t64 = (BNWORD64)qhat * dl;
		if (t64 > ((BNWORD64)r<<32) + nl) {
			/* Decrement qhat and adjust comparison parameters */
			qhat--;
			if ((r += dh) >= dh) {
				t64 -= dl;
				if (t64 > ((BNWORD64)r << 32) + nl)
					qhat--;
			}
		}
#else /* Use lbnMulN1_32 */
		lbnMulN1_32(BIGLITTLE(t2+2,t2), &dl, 1, qhat);
		if (t2high > r || (t2high == r && t2low > nl)) {
			/* Decrement qhat and adjust comparison parameters */
			qhat--;
			if ((r += dh) >= dh) {
				t2high -= (t2low < dl);
				t2low -= dl;
				if (t2high > r || (t2high == r && t2low > nl))
					qhat--;
			}
		}
#endif

		/*
		 * As a point of interest, note that it is not worth checking
		 * for qhat of 0 or 1 and installing special-case code.  These
		 * occur with probability 2^-32, so spending 1 cycle to check
		 * for them is only worth it if we save more than 2^15 cycles,
		 * and a multiply-and-subtract for numbers in the 1024-bit
		 * range just doesn't take that long.
		 */
subtract:
		/*
		 * n points to the least significant end of the substring
		 * of n to be subtracted from.  qhat is either exact or
		 * one too large.  If the subtract gets a borrow, it was
		 * one too large and the divisor is added back in.  It's
		 * a dlen+1 word add which is guaranteed to produce a
		 * carry out, so it can be done very simply.
		 */
		r = lbnMulSub1_32(n, d, dlen, qhat);
		if (r > nh) {	/* Borrow? */
			(void)lbnAddN_32(n, d, dlen);
			qhat--;
		}
		/* Store the quotient digit */
		BIGLITTLE(*q++,*--q) = qhat;
	}
	/* Tah dah! */

	if (shift) {
		lbnRshift_32(d, dlen, shift);
		lbnRshift_32(n, dlen, shift);
	}

	return qhigh;
}
#endif

/*
 * Find the negative multiplicative inverse of x (x must be odd!) modulo 2^32.
 *
 * This just performs Newton's iteration until it gets the
 * inverse.  The initial estimate is always correct to 3 bits, and
 * sometimes 4.  The number of valid bits doubles each iteration.
 * (To prove it, assume x * y == 1 (mod 2^n), and introduce a variable
 * for the error mod 2^2n.  x * y == 1 + k*2^n (mod 2^2n) and follow
 * the iteration through.)
 */
#ifndef lbnMontInv1_32
BNWORD32
lbnMontInv1_32(BNWORD32 const x)
{
        BNWORD32 y = x, z;

	assert(x & 1);
 
        while ((z = x*y) != 1)
                y *= 2 - z;
        return -y;
}
#endif /* !lbnMontInv1_32 */

#if defined(BNWORD64) && PRODUCT_SCAN
/*
 * Test code for product-scanning Montgomery reduction.
 * This seems to slow the C code down rather than speed it up.
 *
 * The first loop computes the Montgomery multipliers, storing them over
 * the low half of the number n.
 *
 * The second half multiplies the upper half, adding in the modulus
 * times the Montgomery multipliers.  The results of this multiply
 * are stored.
 */
void
lbnMontReduce_32(BNWORD32 *n, BNWORD32 const *mod, unsigned mlen, BNWORD32 inv)
{
	BNWORD64 x, y;
	BNWORD32 const *pm;
	BNWORD32 *pn;
	BNWORD32 t;
	unsigned carry;
	unsigned i, j;

	/* Special case of zero */
	if (!mlen)
		return;

	/* Pass 1 - compute Montgomery multipliers */
	/* First iteration can have certain simplifications. */
	t = BIGLITTLE(n[-1],n[0]);
	x = t;
	t *= inv;
	BIGLITTLE(n[-1], n[0]) = t;
	x += (BNWORD64)t * BIGLITTLE(mod[-1],mod[0]); /* Can't overflow */
	assert((BNWORD32)x == 0);
	x = x >> 32;

	for (i = 1; i < mlen; i++) {
		carry = 0;
		pn = n;
		pm = BIGLITTLE(mod-i-1,mod+i+1);
		for (j = 0; j < i; j++) {
			y = (BNWORD64)BIGLITTLE(*--pn * *pm++, *pn++ * *--pm);
			x += y;
			carry += (x < y);
		}
		assert(BIGLITTLE(pn == n-i, pn == n+i));
		y = t = BIGLITTLE(pn[-1], pn[0]);
		x += y;
		carry += (x < y);
		BIGLITTLE(pn[-1], pn[0]) = t = inv * (BNWORD32)x;
		assert(BIGLITTLE(pm == mod-1, pm == mod+1));
		y = (BNWORD64)t * BIGLITTLE(pm[0],pm[-1]);
		x += y;
		carry += (x < y);
		assert((BNWORD32)x == 0);
		x = x >> 32 | (BNWORD64)carry << 32;
	}

	BIGLITTLE(n -= mlen, n += mlen);

	/* Pass 2 - compute upper words and add to n */
	for (i = 1; i < mlen; i++) {
		carry = 0;
		pm = BIGLITTLE(mod-i,mod+i);
		pn = n;
		for (j = i; j < mlen; j++) {
			y = (BNWORD64)BIGLITTLE(*--pm * *pn++, *pm++ * *--pn);
			x += y;
			carry += (x < y);
		}
		assert(BIGLITTLE(pm == mod-mlen, pm == mod+mlen));
		assert(BIGLITTLE(pn == n+mlen-i, pn == n-mlen+i));
		y = t = BIGLITTLE(*(n-i),*(n+i-1));
		x += y;
		carry += (x < y);
		BIGLITTLE(*(n-i),*(n+i-1)) = (BNWORD32)x;
		x = (x >> 32) | (BNWORD64)carry << 32;
	}

	/* Last round of second half, simplified. */
	t = BIGLITTLE(*(n-mlen),*(n+mlen-1));
	x += t;
	BIGLITTLE(*(n-mlen),*(n+mlen-1)) = (BNWORD32)x;
	carry = (unsigned)(x >> 32);

	while (carry)
		carry -= lbnSubN_32(n, mod, mlen);
	while (lbnCmp_32(n, mod, mlen) >= 0)
		(void)lbnSubN_32(n, mod, mlen);
}
#define lbnMontReduce_32 lbnMontReduce_32
#endif

/*
 * Montgomery reduce n, modulo mod.  This reduces modulo mod and divides by
 * 2^(32*mlen).  Returns the result in the *top* mlen words of the argument n.
 * This is ready for another multiplication using lbnMul_32.
 *
 * Montgomery representation is a very useful way to encode numbers when
 * you're doing lots of modular reduction.  What you do is pick a multiplier
 * R which is relatively prime to the modulus and very easy to divide by.
 * Since the modulus is odd, R is closen as a power of 2, so the division
 * is a shift.  In fact, it's a shift of an integral number of words,
 * so the shift can be implicit - just drop the low-order words.
 *
 * Now, choose R *larger* than the modulus m, 2^(32*mlen).  Then convert
 * all numbers a, b, etc. to Montgomery form M(a), M(b), etc using the
 * relationship M(a) = a*R mod m, M(b) = b*R mod m, etc.  Note that:
 * - The Montgomery form of a number depends on the modulus m.
 *   A fixed modulus m is assumed throughout this discussion.
 * - Since R is relaitvely prime to m, multiplication by R is invertible;
 *   no information about the numbers is lost, they're just scrambled.
 * - Adding (and subtracting) numbers in this form works just as usual.
 *   M(a+b) = (a+b)*R mod m = (a*R + b*R) mod m = (M(a) + M(b)) mod m
 * - Multiplying numbers in this form produces a*b*R*R.  The problem
 *   is to divide out the excess factor of R, modulo m as well as to
 *   reduce to the given length mlen.  It turns out that this can be
 *   done *faster* than a normal divide, which is where the speedup
 *   in Montgomery division comes from.
 *
 * Normal reduction chooses a most-significant quotient digit q and then
 * subtracts q*m from the number to be reduced.  Choosing q is tricky
 * and involved (just look at lbnDiv_32 to see!) and is usually
 * imperfect, requiring a check for correction after the subtraction.
 *
 * Montgomery reduction *adds* a multiple of m to the *low-order* part
 * of the number to be reduced.  This multiple is chosen to make the
 * low-order part of the number come out to zero.  This can be done
 * with no trickery or error using a precomputed inverse of the modulus.
 * In this code, the "part" is one word, but any width can be used.
 *
 * Repeating this step sufficiently often results in a value which
 * is a multiple of R (a power of two, remember) but is still (since
 * the additions were to the low-order part and thus did not increase
 * the value of the number being reduced very much) still not much
 * larger than m*R.  Then implicitly divide by R and subtract off
 * m until the result is in the correct range.
 *
 * Since the low-order part being cancelled is less than R, the
 * multiple of m added must have a multiplier which is at most R-1.
 * Assuming that the input is at most m*R-1, the final number is
 * at most m*(2*R-1)-1 = 2*m*R - m - 1, so subtracting m once from
 * the high-order part, equivalent to subtracting m*R from the
 * while number, produces a result which is at most m*R - m - 1,
 * which divided by R is at most m-1.
 *
 * To convert *to* Montgomery form, you need a regular remainder
 * routine, although you can just compute R*R (mod m) and do the
 * conversion using Montgomery multiplication.  To convert *from*
 * Montgomery form, just Montgomery reduce the number to
 * remove the extra factor of R.
 * 
 * TODO: Change to a full inverse and use Karatsuba's multiplication
 * rather than this word-at-a-time.
 */
#ifndef lbnMontReduce_32
void
lbnMontReduce_32(BNWORD32 *n, BNWORD32 const *mod, unsigned const mlen,
                BNWORD32 inv)
{
	BNWORD32 t;
	BNWORD32 c = 0;
	unsigned len = mlen;

	/* inv must be the negative inverse of mod's least significant word */
	assert((BNWORD32)(inv * BIGLITTLE(mod[-1],mod[0])) == (BNWORD32)-1);

	assert(len);

	do {
		t = lbnMulAdd1_32(n, mod, mlen, inv * BIGLITTLE(n[-1],n[0]));
		c += lbnAdd1_32(BIGLITTLE(n-mlen,n+mlen), len, t);
		BIGLITTLE(--n,++n);
	} while (--len);

	/*
	 * All that adding can cause an overflow past the modulus size,
	 * but it's unusual, and never by much, so a subtraction loop
	 * is the right way to deal with it.
	 * This subtraction happens infrequently - I've only ever seen it
	 * invoked once per reduction, and then just under 22.5% of the time.
	 */
	while (c)
		c -= lbnSubN_32(n, mod, mlen);
	while (lbnCmp_32(n, mod, mlen) >= 0)
		(void)lbnSubN_32(n, mod, mlen);
}
#endif /* !lbnMontReduce_32 */

/*
 * A couple of helpers that you might want to implement atomically
 * in asm sometime.
 */
#ifndef lbnMontMul_32
/*
 * Multiply "num1" by "num2", modulo "mod", all of length "len", and
 * place the result in the high half of "prod".  "inv" is the inverse
 * of the least-significant word of the modulus, modulo 2^32.
 * This uses numbers in Montgomery form.  Reduce using "len" and "inv".
 *
 * This is implemented as a macro to win on compilers that don't do
 * inlining, since it's so trivial.
 */
#define lbnMontMul_32(prod, n1, n2, mod, len, inv) \
	(lbnMulX_32(prod, n1, n2, len), lbnMontReduce_32(prod, mod, len, inv))
#endif /* !lbnMontMul_32 */

#ifndef lbnMontSquare_32
/*
 * Square "n", modulo "mod", both of length "len", and place the result
 * in the high half of "prod".  "inv" is the inverse of the least-significant
 * word of the modulus, modulo 2^32.
 * This uses numbers in Montgomery form.  Reduce using "len" and "inv".
 *
 * This is implemented as a macro to win on compilers that don't do
 * inlining, since it's so trivial.
 */
#define lbnMontSquare_32(prod, n, mod, len, inv) \
	(lbnSquare_32(prod, n, len), lbnMontReduce_32(prod, mod, len, inv))
	
#endif /* !lbnMontSquare_32 */

/*
 * Convert a number to Montgomery form - requires mlen + nlen words
 * of memory in "n".
 */
void
lbnToMont_32(BNWORD32 *n, unsigned nlen, BNWORD32 *mod, unsigned mlen)
{
	/* Move n up "mlen" words */
	lbnCopy_32(BIGLITTLE(n-mlen,n+mlen), n, nlen);
	lbnZero_32(n, mlen);
	/* Do the division - dump the quotient in the high-order words */
	(void)lbnDiv_32(BIGLITTLE(n-mlen,n+mlen), n, mlen+nlen, mod, mlen);
}

/*
 * Convert from Montgomery form.  Montgomery reduction is all that is
 * needed.
 */
void
lbnFromMont_32(BNWORD32 *n, BNWORD32 *mod, unsigned len)
{
	/* Zero the high words of n */
	lbnZero_32(BIGLITTLE(n-len,n+len), len);
	lbnMontReduce_32(n, mod, len, lbnMontInv1_32(mod[BIGLITTLE(-1,0)]));
	/* Move n down len words */
	lbnCopy_32(n, BIGLITTLE(n-len,n+len), len);
}

/*
 * The windowed exponentiation algorithm, precomputes a table of odd
 * powers of n up to 2^k.  See the comment in bnExpMod_32 below for
 * an explanation of how it actually works works.
 *
 * It takes 2^(k-1)-1 multiplies to compute the table, and (e-1)/(k+1)
 * multiplies (on average) to perform the exponentiation.  To minimize
 * the sum, k must vary with e.  The optimal window sizes vary with the
 * exponent length.  Here are some selected values and the boundary cases.
 * (An underscore _ has been inserted into some of the numbers to ensure
 * that magic strings like 32 do not appear in this table.  It should be
 * ignored.)
 *
 * At e =    1 bits, k=1   (0.000000) is best
 * At e =    2 bits, k=1   (0.500000) is best
 * At e =    4 bits, k=1   (1.500000) is best
 * At e =    8 bits, k=2   (3.333333) < k=1   (3.500000)
 * At e =  1_6 bits, k=2   (6.000000) is best
 * At e =   26 bits, k=3   (9.250000) < k=2   (9.333333)
 * At e =  3_2 bits, k=3  (10.750000) is best
 * At e =  6_4 bits, k=3  (18.750000) is best
 * At e =   82 bits, k=4  (23.200000) < k=3  (23.250000)
 * At e =  128 bits, k=4 (3_2.400000) is best
 * At e =  242 bits, k=5  (55.1_66667) < k=4 (55.200000)
 * At e =  256 bits, k=5  (57.500000) is best
 * At e =  512 bits, k=5 (100.1_66667) is best
 * At e =  674 bits, k=6 (127.142857) < k=5 (127.1_66667)
 * At e = 1024 bits, k=6 (177.142857) is best
 * At e = 1794 bits, k=7 (287.125000) < k=6 (287.142857)
 * At e = 2048 bits, k=7 (318.875000) is best
 * At e = 4096 bits, k=7 (574.875000) is best
 *
 * The numbers in parentheses are the expected number of multiplications
 * needed to do the computation.  The normal russian-peasant modular
 * exponentiation technique always uses (e-1)/2.  For exponents as
 * small as 192 bits (below the range of current factoring algorithms),
 * half of the multiplies are eliminated, 45.2 as opposed to the naive
 * 95.5.  Counting the 191 squarings as 3/4 a multiply each (squaring
 * proper is just over half of multiplying, but the Montgomery
 * reduction in each case is also a multiply), that's 143.25
 * multiplies, for totals of 188.45 vs. 238.75 - a 21% savings.
 * For larger exponents (like 512 bits), it's 483.92 vs. 639.25, a
 * 24.3% savings.  It asymptotically approaches 25%.
 *
 * Um, actually there's a slightly more accurate way to count, which
 * really is the average number of multiplies required, averaged
 * uniformly over all 2^(e-1) e-bit numbers, from 2^(e-1) to (2^e)-1.
 * It's based on the recurrence that for the last b bits, b <= k, at
 * most one multiply is needed (and none at all 1/2^b of the time),
 * while when b > k, the odds are 1/2 each way that the bit will be
 * 0 (meaning no multiplies to reduce it to the b-1-bit case) and
 * 1/2 that the bit will be 1, starting a k-bit window and requiring
 * 1 multiply beyond the b-k-bit case.  Since the most significant
 * bit is always 1, a k-bit window always starts there, and that
 * multiply is by 1, so it isn't a multiply at all.  Thus, the
 * number of multiplies is simply that needed for the last e-k bits.
 * This recurrence produces:
 *
 * At e =    1 bits, k=1   (0.000000) is best
 * At e =    2 bits, k=1   (0.500000) is best
 * At e =    4 bits, k=1   (1.500000) is best
 * At e =    6 bits, k=2   (2.437500) < k=1   (2.500000)
 * At e =    8 bits, k=2   (3.109375) is best
 * At e =  1_6 bits, k=2   (5.777771) is best
 * At e =   24 bits, k=3   (8.437629) < k=2   (8.444444)
 * At e =  3_2 bits, k=3  (10.437492) is best
 * At e =  6_4 bits, k=3  (18.437500) is best
 * At e =   81 bits, k=4  (22.6_40000) < k=3  (22.687500)
 * At e =  128 bits, k=4 (3_2.040000) is best
 * At e =  241 bits, k=5  (54.611111) < k=4  (54.6_40000)
 * At e =  256 bits, k=5  (57.111111) is best
 * At e =  512 bits, k=5  (99.777778) is best
 * At e =  673 bits, k=6 (126.591837) < k=5 (126.611111)
 * At e = 1024 bits, k=6 (176.734694) is best
 * At e = 1793 bits, k=7 (286.578125) < k=6 (286.591837)
 * At e = 2048 bits, k=7 (318.453125) is best
 * At e = 4096 bits, k=7 (574.453125) is best
 *
 * This has the rollover points at 6, 24, 81, 241, 673 and 1793 instead
 * of 8, 26, 82, 242, 674, and 1794.  Not a very big difference.
 * (The numbers past that are k=8 at 4609 and k=9 at 11521,
 * vs. one more in each case for the approximation.)
 *
 * Given that exponents for which k>7 are useful are uncommon,
 * a fixed size table for k <= 7 is used for simplicity.
 *
 * The basic number of squarings needed is e-1, although a k-bit
 * window (for k > 1) can save, on average, k-2 of those, too.
 * That savings currently isn't counted here.  It would drive the
 * crossover points slightly lower.
 * (Actually, this win is also reduced in the DoubleExpMod case,
 * meaning we'd have to split the tables.  Except for that, the
 * multiplies by powers of the two bases are independent, so
 * the same logic applies to each as the single case.)
 *
 * Table entry i is the largest number of bits in an exponent to
 * process with a window size of i+1.  Entry 6 is the largest
 * possible unsigned number, so the window will never be more
 * than 7 bits, requiring 2^6 = 0x40 slots.
 */
#define BNEXPMOD_MAX_WINDOW	7
static unsigned const bnExpModThreshTable[BNEXPMOD_MAX_WINDOW] = {
	5, 23, 80, 240, 672, 1792, (unsigned)-1
/*	7, 25, 81, 241, 673, 1793, (unsigned)-1	 ### The old approximations */
};

/*
 * Perform modular exponentiation, as fast as possible!  This uses
 * Montgomery reduction, optimized squaring, and windowed exponentiation.
 * The modulus "mod" MUST be odd!
 *
 * This returns 0 on success, -1 on out of memory.
 *
 * The window algorithm:
 * The idea is to keep a running product of b1 = n^(high-order bits of exp),
 * and then keep appending exponent bits to it.  The following patterns
 * apply to a 3-bit window (k = 3):
 * To append   0: square
 * To append   1: square, multiply by n^1
 * To append  10: square, multiply by n^1, square
 * To append  11: square, square, multiply by n^3
 * To append 100: square, multiply by n^1, square, square
 * To append 101: square, square, square, multiply by n^5
 * To append 110: square, square, multiply by n^3, square
 * To append 111: square, square, square, multiply by n^7
 *
 * Since each pattern involves only one multiply, the longer the pattern
 * the better, except that a 0 (no multiplies) can be appended directly.
 * We precompute a table of odd powers of n, up to 2^k, and can then
 * multiply k bits of exponent at a time.  Actually, assuming random
 * exponents, there is on average one zero bit between needs to
 * multiply (1/2 of the time there's none, 1/4 of the time there's 1,
 * 1/8 of the time, there's 2, 1/32 of the time, there's 3, etc.), so
 * you have to do one multiply per k+1 bits of exponent.
 *
 * The loop walks down the exponent, squaring the result buffer as
 * it goes.  There is a wbits+1 bit lookahead buffer, buf, that is
 * filled with the upcoming exponent bits.  (What is read after the
 * end of the exponent is unimportant, but it is filled with zero here.)
 * When the most-significant bit of this buffer becomes set, i.e.
 * (buf & tblmask) != 0, we have to decide what pattern to multiply
 * by, and when to do it.  We decide, remember to do it in future
 * after a suitable number of squarings have passed (e.g. a pattern
 * of "100" in the buffer requires that we multiply by n^1 immediately;
 * a pattern of "110" calls for multiplying by n^3 after one more
 * squaring), clear the buffer, and continue.
 *
 * When we start, there is one more optimization: the result buffer
 * is implcitly one, so squaring it or multiplying by it can be
 * optimized away.  Further, if we start with a pattern like "100"
 * in the lookahead window, rather than placing n into the buffer
 * and then starting to square it, we have already computed n^2
 * to compute the odd-powers table, so we can place that into
 * the buffer and save a squaring.
 *
 * This means that if you have a k-bit window, to compute n^z,
 * where z is the high k bits of the exponent, 1/2 of the time
 * it requires no squarings.  1/4 of the time, it requires 1
 * squaring, ... 1/2^(k-1) of the time, it reqires k-2 squarings.
 * And the remaining 1/2^(k-1) of the time, the top k bits are a
 * 1 followed by k-1 0 bits, so it again only requires k-2
 * squarings, not k-1.  The average of these is 1.  Add that
 * to the one squaring we have to do to compute the table,
 * and you'll see that a k-bit window saves k-2 squarings
 * as well as reducing the multiplies.  (It actually doesn't
 * hurt in the case k = 1, either.)
 *
 * n must have mlen words allocated.  Although fewer may be in use
 * when n is passed in, all are in use on exit.
 */
int
lbnExpMod_32(BNWORD32 *result, BNWORD32 const *n, unsigned nlen,
	BNWORD32 const *e, unsigned elen, BNWORD32 *mod, unsigned mlen)
{
	BNWORD32 *table[1 << (BNEXPMOD_MAX_WINDOW-1)];
				/* Table of odd powers of n */
	unsigned ebits;		/* Exponent bits */
	unsigned wbits;		/* Window size */
	unsigned tblmask;	/* Mask of exponentiation window */
	BNWORD32 bitpos;	/* Mask of current look-ahead bit */
	unsigned buf;		/* Buffer of exponent bits */
	unsigned multpos;	/* Where to do pending multiply */
	BNWORD32 const *mult;	/* What to multiply by */
	unsigned i;		/* Loop counter */
	int isone;		/* Flag: accum. is implicitly one */
	BNWORD32 *a, *b;	/* Working buffers/accumulators */
	BNWORD32 *t;		/* Pointer into the working buffers */
	BNWORD32 inv;		/* mod^-1 modulo 2^32 */
	int y;			/* bnYield() result */

	assert(mlen);
	assert(nlen <= mlen);

	/* First, a couple of trivial cases. */
	elen = lbnNorm_32(e, elen);
	if (!elen) {
		/* x ^ 0 == 1 */
		lbnZero_32(result, mlen);
		BIGLITTLE(result[-1],result[0]) = 1;
		return 0;
	}
	ebits = lbnBits_32(e, elen);
	if (ebits == 1) {
		/* x ^ 1 == x */
		if (n != result)
			lbnCopy_32(result, n, nlen);
		if (mlen > nlen)
			lbnZero_32(BIGLITTLE(result-nlen,result+nlen),
			           mlen-nlen);
		return 0;
	}

	/* Okay, now move the exponent pointer to the most-significant word */
	e = BIGLITTLE(e-elen, e+elen-1);

	/* Look up appropriate k-1 for the exponent - tblmask = 1<<(k-1) */
	wbits = 0;
	while (ebits > bnExpModThreshTable[wbits])
		wbits++;

	/* Allocate working storage: two product buffers and the tables. */
	LBNALLOC(a, BNWORD32, 2*mlen);
	if (!a)
		return -1;
	LBNALLOC(b, BNWORD32, 2*mlen);
	if (!b) {
		LBNFREE(a, 2*mlen);
		return -1;
	}

	/* Convert to the appropriate table size: tblmask = 1<<(k-1) */
	tblmask = 1u << wbits;

	/* We have the result buffer available, so use it. */
	table[0] = result;

	/*
	 * Okay, we now have a minimal-sized table - expand it.
	 * This is allowed to fail!  If so, scale back the table size
	 * and proceed.
	 */
	for (i = 1; i < tblmask; i++) {
		LBNALLOC(t, BNWORD32, mlen);
		if (!t)	/* Out of memory!  Quit the loop. */
			break;
		table[i] = t;
	}

	/* If we stopped, with i < tblmask, shrink the tables appropriately */
	while (tblmask > i) {
		wbits--;
		tblmask >>= 1;
	}
	/* Free up our overallocations */
	while (--i > tblmask)
		LBNFREE(table[i], mlen);

	/* Okay, fill in the table */

	/* Compute the necessary modular inverse */
	inv = lbnMontInv1_32(mod[BIGLITTLE(-1,0)]);	/* LSW of modulus */

	/* Convert n to Montgomery form */

	/* Move n up "mlen" words into a */
	t = BIGLITTLE(a-mlen, a+mlen);
	lbnCopy_32(t, n, nlen);
	lbnZero_32(a, mlen);
	/* Do the division - lose the quotient into the high-order words */
	(void)lbnDiv_32(t, a, mlen+nlen, mod, mlen);
	/* Copy into first table entry */
	lbnCopy_32(table[0], a, mlen);

	/* Square a into b */
	lbnMontSquare_32(b, a, mod, mlen, inv);

	/* Use high half of b to initialize the table */
	t = BIGLITTLE(b-mlen, b+mlen);
	for (i = 1; i < tblmask; i++) {
		lbnMontMul_32(a, t, table[i-1], mod, mlen, inv);
		lbnCopy_32(table[i], BIGLITTLE(a-mlen, a+mlen), mlen);
#if BNYIELD
		if (bnYield && (y = bnYield()) < 0)
			goto yield;
#endif
	}

	/* We might use b = n^2 later... */

	/* Initialze the fetch pointer */
	bitpos = (BNWORD32)1 << ((ebits-1) & (32-1));	/* Initialize mask */

	/* This should point to the msbit of e */
	assert((*e & bitpos) != 0);

	/*
	 * Pre-load the window.  Becuase the window size is
	 * never larger than the exponent size, there is no need to
	 * detect running off the end of e in here.
	 *
	 * The read-ahead is controlled by elen and the bitpos mask.
	 * Note that this is *ahead* of ebits, which tracks the
	 * most significant end of the window.  The purpose of this
	 * initialization is to get the two wbits+1 bits apart,
	 * like they should be.
	 *
	 * Note that bitpos and e1len together keep track of the
	 * lookahead read pointer in the exponent that is used here.
	 */
	buf = 0;
	for (i = 0; i <= wbits; i++) {
		buf = (buf << 1) | ((*e & bitpos) != 0);
		bitpos >>= 1;
		if (!bitpos) {
			BIGLITTLE(e++,e--);
			bitpos = (BNWORD32)1 << (32-1);
			elen--;
		}
	}
	assert(buf & tblmask);

	/*
	 * Set the pending multiply positions to a location that will
	 * never be encountered, thus ensuring that nothing will happen
	 * until the need for a multiply appears and one is scheduled.
	 */
	multpos = ebits;	/* A NULL value */
	mult = 0;	/* Force a crash if we use these */

	/*
	 * Okay, now begins the real work.  The first step is
	 * slightly magic, so it's done outside the main loop,
	 * but it's very similar to what's inside.
	 */
	ebits--;	/* Start processing the first bit... */
	isone = 1;

	/*
	 * This is just like the multiply in the loop, except that
	 * - We know the msbit of buf is set, and
	 * - We have the extra value n^2 floating around.
	 * So, do the usual computation, and if the result is that
	 * the buffer should be multiplied by n^1 immediately
	 * (which we'd normally then square), we multiply it
	 * (which reduces to a copy, which reduces to setting a flag)
	 * by n^2 and skip the squaring.  Thus, we do the
	 * multiply and the squaring in one step.
	 */
	assert(buf & tblmask);
	multpos = ebits - wbits;
	while ((buf & 1) == 0) {
		buf >>= 1;
		multpos++;
	}
	/* Intermediates can wrap, but final must NOT */
	assert(multpos <= ebits);
	mult = table[buf>>1];
	buf = 0;

	/* Special case: use already-computed value sitting in buffer */
	if (multpos == ebits)
		isone = 0;

	/*
	 * At this point, the buffer (which is the high half of b) holds
	 * either 1 (implicitly, as the "isone" flag is set), or n^2.
	 */

	/*
	 * The main loop.  The procedure is:
	 * - Advance the window
	 * - If the most-significant bit of the window is set,
	 *   schedule a multiply for the appropriate time in the
	 *   future (may be immediately)
	 * - Perform any pending multiples
	 * - Check for termination
	 * - Square the buffer
	 *
	 * At any given time, the acumulated product is held in
	 * the high half of b.
	 */
	for (;;) {
		ebits--;

		/* Advance the window */
		assert(buf < tblmask);
		buf <<= 1;
		/*
		 * This reads ahead of the current exponent position
		 * (controlled by ebits), so we have to be able to read
		 * past the lsb of the exponents without error.
		 */
		if (elen) {
			buf |= ((*e & bitpos) != 0);
			bitpos >>= 1;
			if (!bitpos) {
				BIGLITTLE(e++,e--);
				bitpos = (BNWORD32)1 << (32-1);
				elen--;
			}
		}

		/* Examine the window for pending multiplies */
		if (buf & tblmask) {
			multpos = ebits - wbits;
			while ((buf & 1) == 0) {
				buf >>= 1;
				multpos++;
			}
			/* Intermediates can wrap, but final must NOT */
			assert(multpos <= ebits);
			mult = table[buf>>1];
			buf = 0;
		}

		/* If we have a pending multiply, do it */
		if (ebits == multpos) {
			/* Multiply by the table entry remembered previously */
			t = BIGLITTLE(b-mlen, b+mlen);
			if (isone) {
				/* Multiply by 1 is a trivial case */
				lbnCopy_32(t, mult, mlen);
				isone = 0;
			} else {
				lbnMontMul_32(a, t, mult, mod, mlen, inv);
				/* Swap a and b */
				t = a; a = b; b = t;
			}
		}

		/* Are we done? */
		if (!ebits)
			break;

		/* Square the input */
		if (!isone) {
			t = BIGLITTLE(b-mlen, b+mlen);
			lbnMontSquare_32(a, t, mod, mlen, inv);
			/* Swap a and b */
			t = a; a = b; b = t;
		}
#if BNYIELD
		if (bnYield && (y = bnYield()) < 0)
			goto yield;
#endif
	} /* for (;;) */

	assert(!isone);
	assert(!buf);

	/* DONE! */

	/* Convert result out of Montgomery form */
	t = BIGLITTLE(b-mlen, b+mlen);
	lbnCopy_32(b, t, mlen);
	lbnZero_32(t, mlen);
	lbnMontReduce_32(b, mod, mlen, inv);
	lbnCopy_32(result, t, mlen);
	/*
	 * Clean up - free intermediate storage.
	 * Do NOT free table[0], which is the result
	 * buffer.
	 */
	y = 0;
#if BNYIELD
yield:
#endif
	while (--tblmask)
		LBNFREE(table[tblmask], mlen);
	LBNFREE(b, 2*mlen);
	LBNFREE(a, 2*mlen);

	return y;	/* Success */
}

/*
 * Compute and return n1^e1 * n2^e2 mod "mod".
 * result may be either input buffer, or something separate.
 * It must be "mlen" words long.
 *
 * There is a current position in the exponents, which is kept in e1bits.
 * (The exponents are swapped if necessary so e1 is the longer of the two.)
 * At any given time, the value in the accumulator is
 * n1^(e1>>e1bits) * n2^(e2>>e1bits) mod "mod".
 * As e1bits is counted down, this is updated, by squaring it and doing
 * any necessary multiplies.
 * To decide on the necessary multiplies, two windows, each w1bits+1 bits
 * wide, are maintained in buf1 and buf2, which read *ahead* of the
 * e1bits position (with appropriate handling of the case when e1bits
 * drops below w1bits+1).  When the most-significant bit of either window
 * becomes set, indicating that something needs to be multiplied by
 * the accumulator or it will get out of sync, the window is examined
 * to see which power of n1 or n2 to multiply by, and when (possibly
 * later, if the power is greater than 1) the multiply should take
 * place.  Then the multiply and its location are remembered and the
 * window is cleared.
 *
 * If we had every power of n1 in the table, the multiply would always
 * be w1bits steps in the future.  But we only keep the odd powers,
 * so instead of waiting w1bits squarings and then multiplying
 * by n1^k, we wait w1bits-k squarings and multiply by n1.
 *
 * Actually, w2bits can be less than w1bits, but the window is the same
 * size, to make it easier to keep track of where we're reading.  The
 * appropriate number of low-order bits of the window are just ignored.
 */
int
lbnDoubleExpMod_32(BNWORD32 *result,
                   BNWORD32 const *n1, unsigned n1len,
                   BNWORD32 const *e1, unsigned e1len,
                   BNWORD32 const *n2, unsigned n2len,
                   BNWORD32 const *e2, unsigned e2len,
                   BNWORD32 *mod, unsigned mlen)
{
	BNWORD32 *table1[1 << (BNEXPMOD_MAX_WINDOW-1)];
					/* Table of odd powers of n1 */
	BNWORD32 *table2[1 << (BNEXPMOD_MAX_WINDOW-1)];
					/* Table of odd powers of n2 */
	unsigned e1bits, e2bits;	/* Exponent bits */
	unsigned w1bits, w2bits;	/* Window sizes */
	unsigned tblmask;		/* Mask of exponentiation window */
	BNWORD32 bitpos;		/* Mask of current look-ahead bit */
	unsigned buf1, buf2;		/* Buffer of exponent bits */
	unsigned mult1pos, mult2pos;	/* Where to do pending multiply */
	BNWORD32 const *mult1, *mult2;	/* What to multiply by */
	unsigned i;			/* Loop counter */
	int isone;			/* Flag: accum. is implicitly one */
	BNWORD32 *a, *b;		/* Working buffers/accumulators */
	BNWORD32 *t;			/* Pointer into the working buffers */
	BNWORD32 inv;			/* mod^-1 modulo 2^32 */
	int y;				/* bnYield() result */

	assert(mlen);
	assert(n1len <= mlen);
	assert(n2len <= mlen);

	/* First, a couple of trivial cases. */
	e1len = lbnNorm_32(e1, e1len);
	e2len = lbnNorm_32(e2, e2len);

	/* Ensure that the first exponent is the longer */
	e1bits = lbnBits_32(e1, e1len);
	e2bits = lbnBits_32(e2, e2len);
	if (e1bits < e2bits) {
		i = e1len; e1len = e2len; e2len = i;
		i = e1bits; e1bits = e2bits; e2bits = i;
		t = (BNWORD32 *)n1; n1 = n2; n2 = t; 
		t = (BNWORD32 *)e1; e1 = e2; e2 = t; 
	}
	assert(e1bits >= e2bits);

	/* Handle a trivial case */
	if (!e2len)
		return lbnExpMod_32(result, n1, n1len, e1, e1len, mod, mlen);
	assert(e2bits);

	/* The code below fucks up if the exponents aren't at least 2 bits */
	if (e1bits == 1) {
		assert(e2bits == 1);

		LBNALLOC(a, BNWORD32, n1len+n2len);
		if (!a)
			return -1;

		lbnMul_32(a, n1, n1len, n2, n2len);
		/* Do a direct modular reduction */
		if (n1len + n2len >= mlen)
			(void)lbnDiv_32(a+mlen, a, n1len+n2len, mod, mlen);
		lbnCopy_32(result, a, mlen);
		LBNFREE(a, n1len+n2len);
		return 0;
	}

	/* Okay, now move the exponent pointers to the most-significant word */
	e1 = BIGLITTLE(e1-e1len, e1+e1len-1);
	e2 = BIGLITTLE(e2-e2len, e2+e2len-1);

	/* Look up appropriate k-1 for the exponent - tblmask = 1<<(k-1) */
	w1bits = 0;
	while (e1bits > bnExpModThreshTable[w1bits])
		w1bits++;
	w2bits = 0;
	while (e2bits > bnExpModThreshTable[w2bits])
		w2bits++;

	assert(w1bits >= w2bits);

	/* Allocate working storage: two product buffers and the tables. */
	LBNALLOC(a, BNWORD32, 2*mlen);
	if (!a)
		return -1;
	LBNALLOC(b, BNWORD32, 2*mlen);
	if (!b) {
		LBNFREE(a, 2*mlen);
		return -1;
	}

	/* Convert to the appropriate table size: tblmask = 1<<(k-1) */
	tblmask = 1u << w1bits;
	/* Use buf2 for its size, temporarily */
	buf2 = 1u << w2bits;

	LBNALLOC(t, BNWORD32, mlen);
	if (!t) {
		LBNFREE(b, 2*mlen);
		LBNFREE(a, 2*mlen);
		return -1;
	}
	table1[0] = t;
	table2[0] = result;

	/*
	 * Okay, we now have some minimal-sized tables - expand them.
	 * This is allowed to fail!  If so, scale back the table sizes
	 * and proceed.  We allocate both tables at the same time
	 * so if it fails partway through, they'll both be a reasonable
	 * size rather than one huge and one tiny.
	 * When i passes buf2 (the number of entries in the e2 window,
	 * which may be less than the number of entries in the e1 window),
	 * stop allocating e2 space.
	 */
	for (i = 1; i < tblmask; i++) {
		LBNALLOC(t, BNWORD32, mlen);
		if (!t)	/* Out of memory!  Quit the loop. */
			break;
		table1[i] = t;
		if (i < buf2) {
			LBNALLOC(t, BNWORD32, mlen);
			if (!t) {
				LBNFREE(table1[i], mlen);
				break;
			}
			table2[i] = t;
		}
	}

	/* If we stopped, with i < tblmask, shrink the tables appropriately */
	while (tblmask > i) {
		w1bits--;
		tblmask >>= 1;
	}
	/* Free up our overallocations */
	while (--i > tblmask) {
		if (i < buf2)
			LBNFREE(table2[i], mlen);
		LBNFREE(table1[i], mlen);
	}
	/* And shrink the second window too, if needed */
	if (w2bits > w1bits) {
		w2bits = w1bits;
		buf2 = tblmask;
	}

	/*
	 * From now on, use the w2bits variable for the difference
	 * between w1bits and w2bits.
	 */
	w2bits = w1bits-w2bits;

	/* Okay, fill in the tables */

	/* Compute the necessary modular inverse */
	inv = lbnMontInv1_32(mod[BIGLITTLE(-1,0)]);	/* LSW of modulus */

	/* Convert n1 to Montgomery form */

	/* Move n1 up "mlen" words into a */
	t = BIGLITTLE(a-mlen, a+mlen);
	lbnCopy_32(t, n1, n1len);
	lbnZero_32(a, mlen);
	/* Do the division - lose the quotient into the high-order words */
	(void)lbnDiv_32(t, a, mlen+n1len, mod, mlen);
	/* Copy into first table entry */
	lbnCopy_32(table1[0], a, mlen);

	/* Square a into b */
	lbnMontSquare_32(b, a, mod, mlen, inv);

	/* Use high half of b to initialize the first table */
	t = BIGLITTLE(b-mlen, b+mlen);
	for (i = 1; i < tblmask; i++) {
		lbnMontMul_32(a, t, table1[i-1], mod, mlen, inv);
		lbnCopy_32(table1[i], BIGLITTLE(a-mlen, a+mlen), mlen);
#if BNYIELD
		if (bnYield && (y = bnYield()) < 0)
			goto yield;
#endif
	}

	/* Convert n2 to Montgomery form */

	t = BIGLITTLE(a-mlen, a+mlen);
	/* Move n2 up "mlen" words into a */
	lbnCopy_32(t, n2, n2len);
	lbnZero_32(a, mlen);
	/* Do the division - lose the quotient into the high-order words */
	(void)lbnDiv_32(t, a, mlen+n2len, mod, mlen);
	/* Copy into first table entry */
	lbnCopy_32(table2[0], a, mlen);

	/* Square it into a */
	lbnMontSquare_32(a, table2[0], mod, mlen, inv);
	/* Copy to b, low half */
	lbnCopy_32(b, t, mlen);

	/* Use b to initialize the second table */
	for (i = 1; i < buf2; i++) {
		lbnMontMul_32(a, b, table2[i-1], mod, mlen, inv);
		lbnCopy_32(table2[i], t, mlen);
#if BNYIELD
		if (bnYield && (y = bnYield()) < 0)
			goto yield;
#endif
	}

	/*
	 * Okay, a recap: at this point, the low part of b holds
	 * n2^2, the high part holds n1^2, and the tables are
	 * initialized with the odd powers of n1 and n2 from 1
	 * through 2*tblmask-1 and 2*buf2-1.
	 *
	 * We might use those squares in b later, or we might not.
	 */

	/* Initialze the fetch pointer */
	bitpos = (BNWORD32)1 << ((e1bits-1) & (32-1));	/* Initialize mask */

	/* This should point to the msbit of e1 */
	assert((*e1 & bitpos) != 0);

	/*
	 * Pre-load the windows.  Becuase the window size is
	 * never larger than the exponent size, there is no need to
	 * detect running off the end of e1 in here.
	 *
	 * The read-ahead is controlled by e1len and the bitpos mask.
	 * Note that this is *ahead* of e1bits, which tracks the
	 * most significant end of the window.  The purpose of this
	 * initialization is to get the two w1bits+1 bits apart,
	 * like they should be.
	 *
	 * Note that bitpos and e1len together keep track of the
	 * lookahead read pointer in the exponent that is used here.
	 * e2len is not decremented, it is only ever compared with
	 * e1len as *that* is decremented.
	 */
	buf1 = buf2 = 0;
	for (i = 0; i <= w1bits; i++) {
		buf1 = (buf1 << 1) | ((*e1 & bitpos) != 0);
		if (e1len <= e2len)
			buf2 = (buf2 << 1) | ((*e2 & bitpos) != 0);
		bitpos >>= 1;
		if (!bitpos) {
			BIGLITTLE(e1++,e1--);
			if (e1len <= e2len)
				BIGLITTLE(e2++,e2--);
			bitpos = (BNWORD32)1 << (32-1);
			e1len--;
		}
	}
	assert(buf1 & tblmask);

	/*
	 * Set the pending multiply positions to a location that will
	 * never be encountered, thus ensuring that nothing will happen
	 * until the need for a multiply appears and one is scheduled.
	 */
	mult1pos = mult2pos = e1bits;	/* A NULL value */
	mult1 = mult2 = 0;	/* Force a crash if we use these */

	/*
	 * Okay, now begins the real work.  The first step is
	 * slightly magic, so it's done outside the main loop,
	 * but it's very similar to what's inside.
	 */
	isone = 1;	/* Buffer is implicitly 1, so replace * by copy */
	e1bits--;	/* Start processing the first bit... */

	/*
	 * This is just like the multiply in the loop, except that
	 * - We know the msbit of buf1 is set, and
	 * - We have the extra value n1^2 floating around.
	 * So, do the usual computation, and if the result is that
	 * the buffer should be multiplied by n1^1 immediately
	 * (which we'd normally then square), we multiply it
	 * (which reduces to a copy, which reduces to setting a flag)
	 * by n1^2 and skip the squaring.  Thus, we do the
	 * multiply and the squaring in one step.
	 */
	assert(buf1 & tblmask);
	mult1pos = e1bits - w1bits;
	while ((buf1 & 1) == 0) {
		buf1 >>= 1;
		mult1pos++;
	}
	/* Intermediates can wrap, but final must NOT */
	assert(mult1pos <= e1bits);
	mult1 = table1[buf1>>1];
	buf1 = 0;

	/* Special case: use already-computed value sitting in buffer */
	if (mult1pos == e1bits)
		isone = 0;

	/*
	 * The first multiply by a power of n2.  Similar, but
	 * we might not even want to schedule a multiply if e2 is
	 * shorter than e1, and the window might be shorter so
	 * we have to leave the low w2bits bits alone.
	 */
	if (buf2 & tblmask) {
		/* Remember low-order bits for later */
		i = buf2 & ((1u << w2bits) - 1);
		buf2 >>= w2bits;
		mult2pos = e1bits - w1bits + w2bits;
		while ((buf2 & 1) == 0) {
			buf2 >>= 1;
			mult2pos++;
		}
		assert(mult2pos <= e1bits);
		mult2 = table2[buf2>>1];
		buf2 = i;

		if (mult2pos == e1bits) {
			t = BIGLITTLE(b-mlen, b+mlen);
			if (isone) {
				lbnCopy_32(t, b, mlen);	/* Copy low to high */
				isone = 0;
			} else {
				lbnMontMul_32(a, t, b, mod, mlen, inv);
				t = a; a = b; b = t;
			}
		}
	}

	/*
	 * At this point, the buffer (which is the high half of b)
	 * holds either 1 (implicitly, as the "isone" flag is set),
	 * n1^2, n2^2 or n1^2 * n2^2.
	 */

	/*
	 * The main loop.  The procedure is:
	 * - Advance the windows
	 * - If the most-significant bit of a window is set,
	 *   schedule a multiply for the appropriate time in the
	 *   future (may be immediately)
	 * - Perform any pending multiples
	 * - Check for termination
	 * - Square the buffers
	 *
	 * At any given time, the acumulated product is held in
	 * the high half of b.
	 */
	for (;;) {
		e1bits--;

		/* Advance the windows */
		assert(buf1 < tblmask);
		buf1 <<= 1;
		assert(buf2 < tblmask);
		buf2 <<= 1;
		/*
		 * This reads ahead of the current exponent position
		 * (controlled by e1bits), so we have to be able to read
		 * past the lsb of the exponents without error.
		 */
		if (e1len) {
			buf1 |= ((*e1 & bitpos) != 0);
			if (e1len <= e2len)
				buf2 |= ((*e2 & bitpos) != 0);
			bitpos >>= 1;
			if (!bitpos) {
				BIGLITTLE(e1++,e1--);
				if (e1len <= e2len)
					BIGLITTLE(e2++,e2--);
				bitpos = (BNWORD32)1 << (32-1);
				e1len--;
			}
		}

		/* Examine the first window for pending multiplies */
		if (buf1 & tblmask) {
			mult1pos = e1bits - w1bits;
			while ((buf1 & 1) == 0) {
				buf1 >>= 1;
				mult1pos++;
			}
			/* Intermediates can wrap, but final must NOT */
			assert(mult1pos <= e1bits);
			mult1 = table1[buf1>>1];
			buf1 = 0;
		}

		/*
		 * Examine the second window for pending multiplies.
		 * Window 2 can be smaller than window 1, but we
		 * keep the same number of bits in buf2, so we need
		 * to ignore any low-order bits in the buffer when
		 * computing what to multiply by, and recompute them
		 * later.
		 */
		if (buf2 & tblmask) {
			/* Remember low-order bits for later */
			i = buf2 & ((1u << w2bits) - 1);
			buf2 >>= w2bits;
			mult2pos = e1bits - w1bits + w2bits;
			while ((buf2 & 1) == 0) {
				buf2 >>= 1;
				mult2pos++;
			}
			assert(mult2pos <= e1bits);
			mult2 = table2[buf2>>1];
			buf2 = i;
		}


		/* If we have a pending multiply for e1, do it */
		if (e1bits == mult1pos) {
			/* Multiply by the table entry remembered previously */
			t = BIGLITTLE(b-mlen, b+mlen);
			if (isone) {
				/* Multiply by 1 is a trivial case */
				lbnCopy_32(t, mult1, mlen);
				isone = 0;
			} else {
				lbnMontMul_32(a, t, mult1, mod, mlen, inv);
				/* Swap a and b */
				t = a; a = b; b = t;
			}
		}

		/* If we have a pending multiply for e2, do it */
		if (e1bits == mult2pos) {
			/* Multiply by the table entry remembered previously */
			t = BIGLITTLE(b-mlen, b+mlen);
			if (isone) {
				/* Multiply by 1 is a trivial case */
				lbnCopy_32(t, mult2, mlen);
				isone = 0;
			} else {
				lbnMontMul_32(a, t, mult2, mod, mlen, inv);
				/* Swap a and b */
				t = a; a = b; b = t;
			}
		}

		/* Are we done? */
		if (!e1bits)
			break;

		/* Square the buffer */
		if (!isone) {
			t = BIGLITTLE(b-mlen, b+mlen);
			lbnMontSquare_32(a, t, mod, mlen, inv);
			/* Swap a and b */
			t = a; a = b; b = t;
		}
#if BNYIELD
		if (bnYield && (y = bnYield()) < 0)
			goto yield;
#endif
	} /* for (;;) */

	assert(!isone);
	assert(!buf1);
	assert(!buf2);

	/* DONE! */

	/* Convert result out of Montgomery form */
	t = BIGLITTLE(b-mlen, b+mlen);
	lbnCopy_32(b, t, mlen);
	lbnZero_32(t, mlen);
	lbnMontReduce_32(b, mod, mlen, inv);
	lbnCopy_32(result, t, mlen);

	/* Clean up - free intermediate storage */
	y = 0;
#if BNYIELD
yield:
#endif
	buf2 = tblmask >> w2bits;
	while (--tblmask) {
		if (tblmask < buf2)
			LBNFREE(table2[tblmask], mlen);
		LBNFREE(table1[tblmask], mlen);
	}
	t = table1[0];
	LBNFREE(t, mlen);
	LBNFREE(b, 2*mlen);
	LBNFREE(a, 2*mlen);

	return y;	/* Success */
}

/*
 * 2^exp (mod mod).  This is an optimized version for use in Fermat
 * tests.  The input value of n is ignored; it is returned with
 * "mlen" words valid.
 */
int
lbnTwoExpMod_32(BNWORD32 *n, BNWORD32 const *exp, unsigned elen,
	BNWORD32 *mod, unsigned mlen)
{
	unsigned e;	/* Copy of high words of the exponent */
	unsigned bits;	/* Assorted counter of bits */
	BNWORD32 const *bitptr;
	BNWORD32 bitword, bitpos;
	BNWORD32 *a, *b, *a1;
	BNWORD32 inv;
	int y;		/* Result of bnYield() */

	assert(mlen);

	bitptr = BIGLITTLE(exp-elen, exp+elen-1);
	bitword = *bitptr;
	assert(bitword);

	/* Clear n for future use. */
	lbnZero_32(n, mlen);

	bits = lbnBits_32(exp, elen);
	
	/* First, a couple of trivial cases. */
	if (bits <= 1) {
		/* 2 ^ 0 == 1,  2 ^ 1 == 2 */
		BIGLITTLE(n[-1],n[0]) = (BNWORD32)1<<elen;
		return 0;
	}

	/* Set bitpos to the most significant bit */
	bitpos = (BNWORD32)1 << ((bits-1) & (32-1));

	/* Now, count the bits in the modulus. */
	bits = lbnBits_32(mod, mlen);
	assert(bits > 1);	/* a 1-bit modulus is just stupid... */

	/*
	 * We start with 1<<e, where "e" is as many high bits of the
	 * exponent as we can manage without going over the modulus.
	 * This first loop finds "e".
	 */
	e = 1;
	while (elen) {
		/* Consume the first bit */
		bitpos >>= 1;
		if (!bitpos) {
			if (!--elen)
				break;
			bitword = BIGLITTLE(*++bitptr,*--bitptr);
			bitpos = (BNWORD32)1<<(32-1);
		}
		e = (e << 1) | ((bitpos & bitword) != 0);
		if (e >= bits) {	/* Overflow!  Back out. */
			e >>= 1;
			break;
		}
	}
	/*
	 * The bit in "bitpos" being examined by the bit buffer has NOT
	 * been consumed yet.  This may be past the end of the exponent,
	 * in which case elen == 1.
	 */

	/* Okay, now, set bit "e" in n.  n is already zero. */
	inv = (BNWORD32)1 << (e & (32-1));
	e /= 32;
	BIGLITTLE(n[-e-1],n[e]) = inv;
	/*
	 * The effective length of n in words is now "e+1".
	 * This is used a little bit later.
	 */

	if (!elen)
		return 0;	/* That was easy! */

	/*
	 * We have now processed the first few bits.  The next step
	 * is to convert this to Montgomery form for further squaring.
	 */

	/* Allocate working storage: two product buffers */
	LBNALLOC(a, BNWORD32, 2*mlen);
	if (!a)
		return -1;
	LBNALLOC(b, BNWORD32, 2*mlen);
	if (!b) {
		LBNFREE(a, 2*mlen);
		return -1;
	}

	/* Convert n to Montgomery form */
	inv = BIGLITTLE(mod[-1],mod[0]);	/* LSW of modulus */
	assert(inv & 1);	/* Modulus must be odd */
	inv = lbnMontInv1_32(inv);
	/* Move n (length e+1, remember?) up "mlen" words into b */
	/* Note that we lie about a1 for a bit - it's pointing to b */
	a1 = BIGLITTLE(b-mlen,b+mlen);
	lbnCopy_32(a1, n, e+1);
	lbnZero_32(b, mlen);
	/* Do the division - dump the quotient into the high-order words */
	(void)lbnDiv_32(a1, b, mlen+e+1, mod, mlen);
	/*
	 * Now do the first squaring and modular reduction to put
	 * the number up in a1 where it belongs.
	 */
	lbnMontSquare_32(a, b, mod, mlen, inv);
	/* Fix up a1 to point to where it should go. */
	a1 = BIGLITTLE(a-mlen,a+mlen);

	/*
	 * Okay, now, a1 holds the number being accumulated, and
	 * b is a scratch register.  Start working:
	 */
	for (;;) {
		/*
		 * Is the bit set?  If so, double a1 as well.
		 * A modular doubling like this is very cheap.
		 */
		if (bitpos & bitword) {
			/*
			 * Double the number.  If there was a carry out OR
			 * the result is greater than the modulus, subract
			 * the modulus.
			 */
			if (lbnDouble_32(a1, mlen) ||
			    lbnCmp_32(a1, mod, mlen) > 0)
				(void)lbnSubN_32(a1, mod, mlen);
		}

		/* Advance to the next exponent bit */
		bitpos >>= 1;
		if (!bitpos) {
			if (!--elen)
				break;	/* Done! */
			bitword = BIGLITTLE(*++bitptr,*--bitptr);
			bitpos = (BNWORD32)1<<(32-1);
		}

		/*
		 * The elen/bitword/bitpos bit buffer is known to be
		 * non-empty, i.e. there is at least one more unconsumed bit.
		 * Thus, it's safe to square the number.
		 */
		lbnMontSquare_32(b, a1, mod, mlen, inv);
		/* Rename result (in b) back to a (a1, really). */
		a1 = b; b = a; a = a1;
		a1 = BIGLITTLE(a-mlen,a+mlen);
#if BNYIELD
		if (bnYield && (y = bnYield()) < 0)
			goto yield;
#endif
	}

	/* DONE!  Just a little bit of cleanup... */

	/*
	 * Convert result out of Montgomery form... this is
	 * just a Montgomery reduction.
	 */
	lbnCopy_32(a, a1, mlen);
	lbnZero_32(a1, mlen);
	lbnMontReduce_32(a, mod, mlen, inv);
	lbnCopy_32(n, a1, mlen);

	/* Clean up - free intermediate storage */
	y = 0;
#if BNYIELD
yield:
#endif
	LBNFREE(b, 2*mlen);
	LBNFREE(a, 2*mlen);

	return y;	/* Success */
}


/*
 * Returns a substring of the big-endian array of bytes representation
 * of the bignum array based on two parameters, the least significant
 * byte number (0 to start with the least significant byte) and the
 * length.  I.e. the number returned is a representation of
 * (bn / 2^(8*lsbyte)) % 2 ^ (8*buflen).
 *
 * It is an error if the bignum is not at least buflen + lsbyte bytes
 * long.
 *
 * This code assumes that the compiler has the minimal intelligence 
 * neded to optimize divides and modulo operations on an unsigned data
 * type with a power of two.
 */
void
lbnExtractBigBytes_32(BNWORD32 const *n, unsigned char *buf,
	unsigned lsbyte, unsigned buflen)
{
	BNWORD32 t = 0;	/* Needed to shut up uninitialized var warnings */
	unsigned shift;

	lsbyte += buflen;

	shift = (8 * lsbyte) % 32;
	lsbyte /= (32/8);	/* Convert to word offset */
	BIGLITTLE(n -= lsbyte, n += lsbyte);

	if (shift)
		t = BIGLITTLE(n[-1],n[0]);

	while (buflen--) {
		if (!shift) {
			t = BIGLITTLE(*n++,*--n);
			shift = 32;
		}
		shift -= 8;
		*buf++ = (unsigned char)(t>>shift);
	}
}

/*
 * Merge a big-endian array of bytes into a bignum array.
 * The array had better be big enough.  This is
 * equivalent to extracting the entire bignum into a
 * large byte array, copying the input buffer into the
 * middle of it, and converting back to a bignum.
 *
 * The buf is "len" bytes long, and its *last* byte is at
 * position "lsbyte" from the end of the bignum.
 *
 * Note that this is a pain to get right.  Fortunately, it's hardly
 * critical for efficiency.
 */
void
lbnInsertBigBytes_32(BNWORD32 *n, unsigned char const *buf,
                  unsigned lsbyte,  unsigned buflen)
{
	BNWORD32 t = 0;	/* Shut up uninitialized varibale warnings */

	lsbyte += buflen;

	BIGLITTLE(n -= lsbyte/(32/8), n += lsbyte/(32/8));

	/* Load up leading odd bytes */
	if (lsbyte % (32/8)) {
		t = BIGLITTLE(*--n,*n++);
		t >>= (lsbyte * 8) % 32;
	}

	/* The main loop - merge into t, storing at each word boundary. */
	while (buflen--) {
		t = (t << 8) | *buf++;
		if ((--lsbyte % (32/8)) == 0)
			BIGLITTLE(*n++,*--n) = t;
	}

	/* Merge odd bytes in t into last word */
	lsbyte = (lsbyte * 8) % 32;
	if (lsbyte) {
		t <<= lsbyte;
		t |= (((BNWORD32)1 << lsbyte) - 1) & BIGLITTLE(n[0],n[-1]);
		BIGLITTLE(n[0],n[-1]) = t;
	}

	return;
}

/*
 * Returns a substring of the little-endian array of bytes representation
 * of the bignum array based on two parameters, the least significant
 * byte number (0 to start with the least significant byte) and the
 * length.  I.e. the number returned is a representation of
 * (bn / 2^(8*lsbyte)) % 2 ^ (8*buflen).
 *
 * It is an error if the bignum is not at least buflen + lsbyte bytes
 * long.
 *
 * This code assumes that the compiler has the minimal intelligence 
 * neded to optimize divides and modulo operations on an unsigned data
 * type with a power of two.
 */
void
lbnExtractLittleBytes_32(BNWORD32 const *n, unsigned char *buf,
	unsigned lsbyte, unsigned buflen)
{
	BNWORD32 t = 0;	/* Needed to shut up uninitialized var warnings */

	BIGLITTLE(n -= lsbyte/(32/8), n += lsbyte/(32/8));

	if (lsbyte % (32/8)) {
		t = BIGLITTLE(*--n,*n++);
		t >>= (lsbyte % (32/8)) * 8 ;
	}

	while (buflen--) {
		if ((lsbyte++ % (32/8)) == 0)
			t = BIGLITTLE(*--n,*n++);
		*buf++ = (unsigned char)t;
		t >>= 8;
	}
}

/*
 * Merge a little-endian array of bytes into a bignum array.
 * The array had better be big enough.  This is
 * equivalent to extracting the entire bignum into a
 * large byte array, copying the input buffer into the
 * middle of it, and converting back to a bignum.
 *
 * The buf is "len" bytes long, and its first byte is at
 * position "lsbyte" from the end of the bignum.
 *
 * Note that this is a pain to get right.  Fortunately, it's hardly
 * critical for efficiency.
 */
void
lbnInsertLittleBytes_32(BNWORD32 *n, unsigned char const *buf,
                  unsigned lsbyte,  unsigned buflen)
{
	BNWORD32 t = 0;	/* Shut up uninitialized varibale warnings */

	/* Move to most-significant end */
	lsbyte += buflen;
	buf += buflen;

	BIGLITTLE(n -= lsbyte/(32/8), n += lsbyte/(32/8));

	/* Load up leading odd bytes */
	if (lsbyte % (32/8)) {
		t = BIGLITTLE(*--n,*n++);
		t >>= (lsbyte * 8) % 32;
	}

	/* The main loop - merge into t, storing at each word boundary. */
	while (buflen--) {
		t = (t << 8) | *--buf;
		if ((--lsbyte % (32/8)) == 0)
			BIGLITTLE(*n++,*--n) = t;
	}

	/* Merge odd bytes in t into last word */
	lsbyte = (lsbyte * 8) % 32;
	if (lsbyte) {
		t <<= lsbyte;
		t |= (((BNWORD32)1 << lsbyte) - 1) & BIGLITTLE(n[0],n[-1]);
		BIGLITTLE(n[0],n[-1]) = t;
	}

	return;
}

#ifdef DEADCODE	/* This was a precursor to the more flexible lbnExtractBytes */
/*
 * Convert a big-endian array of bytes to a bignum.
 * Returns the number of words in the bignum.
 * Note the expression "32/8" for the number of bytes per word.
 * This is so the word-size adjustment will work.
 */
unsigned
lbnFromBytes_32(BNWORD32 *a, unsigned char const *b, unsigned blen)
{
	BNWORD32 t;
	unsigned alen = (blen + (32/8-1))/(32/8);
	BIGLITTLE(a -= alen, a += alen);

	while (blen) {
		t = 0;
		do {
			t = t << 8 | *b++;
		} while (--blen & (32/8-1));
		BIGLITTLE(*a++,*--a) = t;
	}
	return alen;
}
#endif

/*
 * Computes the GCD of a and b.  Modifies both arguments; when it returns,
 * one of them is the GCD and the other is trash.  The return value
 * indicates which: 0 for a, and 1 for b.  The length of the retult is
 * returned in rlen.  Both inputs must have one extra word of precision.
 * alen must be >= blen.
 *
 * TODO: use the binary algorithm (Knuth section 4.5.2, algorithm B).
 * This is based on taking out common powers of 2, then repeatedly:
 * gcd(2*u,v) = gcd(u,2*v) = gcd(u,v) - isolated powers of 2 can be deleted.
 * gcd(u,v) = gcd(u-v,v) - the numbers can be easily reduced.
 * It gets less reduction per step, but the steps are much faster than
 * the division case.
 */
int
lbnGcd_32(BNWORD32 *a, unsigned alen, BNWORD32 *b, unsigned blen,
	unsigned *rlen)
{
#if BNYIELD
	int y;
#endif
	assert(alen >= blen);

	while (blen != 0) {
		(void)lbnDiv_32(BIGLITTLE(a-blen,a+blen), a, alen, b, blen);
		alen = lbnNorm_32(a, blen);
		if (alen == 0) {
			*rlen = blen;
			return 1;
		}
		(void)lbnDiv_32(BIGLITTLE(b-alen,b+alen), b, blen, a, alen);
		blen = lbnNorm_32(b, alen);
#if BNYIELD
		if (bnYield && (y = bnYield()) < 0)
			return y;
#endif
	}
	*rlen = alen;
	return 0;
}

/*
 * Invert "a" modulo "mod" using the extended Euclidean algorithm.
 * Note that this only computes one of the cosequences, and uses the
 * theorem that the signs flip every step and the absolute value of
 * the cosequence values are always bounded by the modulus to avoid
 * having to work with negative numbers.
 * gcd(a,mod) had better equal 1.  Returns 1 if the GCD is NOT 1.
 * a must be one word longer than "mod".  It is overwritten with the
 * result.
 * TODO: Use Richard Schroeppel's *much* faster algorithm.
 */
int
lbnInv_32(BNWORD32 *a, unsigned alen, BNWORD32 const *mod, unsigned mlen)
{
	BNWORD32 *b;	/* Hold a copy of mod during GCD reduction */
	BNWORD32 *p;	/* Temporary for products added to t0 and t1 */
	BNWORD32 *t0, *t1;	/* Inverse accumulators */
	BNWORD32 cy;
	unsigned blen, t0len, t1len, plen;
	int y;

	alen = lbnNorm_32(a, alen);
	if (!alen)
		return 1;	/* No inverse */

	mlen = lbnNorm_32(mod, mlen);

	assert (alen <= mlen);

	/* Inverse of 1 is 1 */
	if (alen == 1 && BIGLITTLE(a[-1],a[0]) == 1) {
		lbnZero_32(BIGLITTLE(a-alen,a+alen), mlen-alen);
		return 0;
	}

	/* Allocate a pile of space */
	LBNALLOC(b, BNWORD32, mlen+1);
	if (b) {
		/*
		 * Although products are guaranteed to always be less than the
		 * modulus, it can involve multiplying two 3-word numbers to
		 * get a 5-word result, requiring a 6th word to store a 0
		 * temporarily.  Thus, mlen + 1.
		 */
		LBNALLOC(p, BNWORD32, mlen+1);
		if (p) {
			LBNALLOC(t0, BNWORD32, mlen);
			if (t0) {
				LBNALLOC(t1, BNWORD32, mlen);
				if (t1)
						goto allocated;
				LBNFREE(t0, mlen);
			}
			LBNFREE(p, mlen+1);
		}
		LBNFREE(b, mlen+1);
	}
	return -1;

allocated:

	/* Set t0 to 1 */
	t0len = 1;
	BIGLITTLE(t0[-1],t0[0]) = 1;
	
	/* b = mod */
	lbnCopy_32(b, mod, mlen);
	/* blen = mlen (implicitly) */
	
	/* t1 = b / a; b = b % a */
	cy = lbnDiv_32(t1, b, mlen, a, alen);
	*(BIGLITTLE(t1-(mlen-alen)-1,t1+(mlen-alen))) = cy;
	t1len = lbnNorm_32(t1, mlen-alen+1);
	blen = lbnNorm_32(b, alen);

	/* while (b > 1) */
	while (blen > 1 || BIGLITTLE(b[-1],b[0]) != (BNWORD32)1) {
		/* q = a / b; a = a % b; */
		if (alen < blen || (alen == blen && lbnCmp_32(a, a, alen) < 0))
			assert(0);
		cy = lbnDiv_32(BIGLITTLE(a-blen,a+blen), a, alen, b, blen);
		*(BIGLITTLE(a-alen-1,a+alen)) = cy;
		plen = lbnNorm_32(BIGLITTLE(a-blen,a+blen), alen-blen+1);
		assert(plen);
		alen = lbnNorm_32(a, blen);
		if (!alen)
			goto failure;	/* GCD not 1 */

		/* t0 += q * t1; */
		assert(plen+t1len <= mlen+1);
		lbnMul_32(p, BIGLITTLE(a-blen,a+blen), plen, t1, t1len);
		plen = lbnNorm_32(p, plen + t1len);
		assert(plen <= mlen);
		if (plen > t0len) {
			lbnZero_32(BIGLITTLE(t0-t0len,t0+t0len), plen-t0len);
			t0len = plen;
		}
		cy = lbnAddN_32(t0, p, plen);
		if (cy) {
			if (t0len > plen) {
				cy = lbnAdd1_32(BIGLITTLE(t0-plen,t0+plen),
						t0len-plen, cy);
			}
			if (cy) {
				BIGLITTLE(t0[-t0len-1],t0[t0len]) = cy;
				t0len++;
			}
		}

		/* if (a <= 1) return a ? t0 : FAIL; */
		if (alen <= 1 && BIGLITTLE(a[-1],a[0]) == (BNWORD32)1) {
			if (alen == 0)
				goto failure;	/* FAIL */
			assert(t0len <= mlen);
			lbnCopy_32(a, t0, t0len);
			lbnZero_32(BIGLITTLE(a-t0len, a+t0len), mlen-t0len);
			goto success;
		}

		/* q = b / a; b = b % a; */
		if (blen < alen || (blen == alen && lbnCmp_32(b, a, alen) < 0))
			assert(0);
		cy = lbnDiv_32(BIGLITTLE(b-alen,b+alen), b, blen, a, alen);
		*(BIGLITTLE(b-blen-1,b+blen)) = cy;
		plen = lbnNorm_32(BIGLITTLE(b-alen,b+alen), blen-alen+1);
		assert(plen);
		blen = lbnNorm_32(b, alen);
		if (!blen)
			goto failure;	/* GCD not 1 */

		/* t1 += q * t0; */
		assert(plen+t0len <= mlen+1);
		lbnMul_32(p, BIGLITTLE(b-alen,b+alen), plen, t0, t0len);
		plen = lbnNorm_32(p, plen + t0len);
		assert(plen <= mlen);
		if (plen > t1len) {
			lbnZero_32(BIGLITTLE(t1-t1len,t1+t1len), plen-t1len);
			t1len = plen;
		}
		cy = lbnAddN_32(t1, p, plen);
		if (cy) {
			if (t1len > plen) {
				cy = lbnAdd1_32(BIGLITTLE(t1-plen,t0+plen),
						t1len-plen, cy);
			}
			if (cy) {
				BIGLITTLE(t1[-t1len-1],t1[t1len]) = cy;
				t1len++;
			}
		}
#if BNYIELD
		if (bnYield && (y = bnYield() < 0))
			goto yield;
#endif
	}

	if (!blen)
		goto failure;	/* gcd(a, mod) != 1 -- FAIL */

	/* return mod-t1 */
	lbnCopy_32(a, mod, mlen);
	assert(t1len <= mlen);
	cy = lbnSubN_32(a, t1, t1len);
	if (cy) {
		assert(mlen > t1len);
		cy = lbnSub1_32(BIGLITTLE(a-t1len, a+t1len), mlen-t1len, cy);
		assert(!cy);
	}

success:
	LBNFREE(t1, mlen);
	LBNFREE(t0, mlen);
	LBNFREE(p, mlen+1);
	LBNFREE(b, mlen+1);
	
	return 0;

failure:		/* GCD is not 1 - no inverse exists! */
	y = 1;
#if BNYIELD
yield:
#endif
	LBNFREE(t1, mlen);
	LBNFREE(t0, mlen);
	LBNFREE(p, mlen+1);
	LBNFREE(b, mlen+1);
	
	return y;
}

/*
 * Precompute powers of "a" mod "mod".  Compute them every "bits"
 * for "n" steps.  This is sufficient to compute powers of g with
 * exponents up to n*bits bits long, i.e. less than 2^(n*bits).
 * 
 * This assumes that the caller has already initialized "array" to point
 * to "n" buffers of size "mlen".
 */
int
lbnBasePrecompBegin_32(BNWORD32 **array, unsigned n, unsigned bits,
	BNWORD32 const *g, unsigned glen, BNWORD32 *mod, unsigned mlen)
{
	BNWORD32 *a, *b;	/* Temporary double-width accumulators */
	BNWORD32 *a1;	/* Pointer to high half of a*/
	BNWORD32 inv;	/* Montgomery inverse of LSW of mod */
	BNWORD32 *t;
	unsigned i;

	glen = lbnNorm_32(g, glen);
	assert(glen);

	assert (mlen == lbnNorm_32(mod, mlen));
	assert (glen <= mlen);

	/* Allocate two temporary buffers, and the array slots */
	LBNALLOC(a, BNWORD32, mlen*2);
	if (!a)
		return -1;
	LBNALLOC(b, BNWORD32, mlen*2);
	if (!b) {
		LBNFREE(a, 2*mlen);
		return -1;
	}

	/* Okay, all ready */

	/* Convert n to Montgomery form */
	inv = BIGLITTLE(mod[-1],mod[0]);	/* LSW of modulus */
	assert(inv & 1);	/* Modulus must be odd */
	inv = lbnMontInv1_32(inv);
	/* Move g up "mlen" words into a (clearing the low mlen words) */
	a1 = BIGLITTLE(a-mlen,a+mlen);
	lbnCopy_32(a1, g, glen);
	lbnZero_32(a, mlen);

	/* Do the division - dump the quotient into the high-order words */
	(void)lbnDiv_32(a1, a, mlen+glen, mod, mlen);

	/* Copy the first value into the array */
	t = *array;
	lbnCopy_32(t, a, mlen);
	a1 = a;	/* This first value is *not* shifted up */
	
	/* Now compute the remaining n-1 array entries */
	assert(bits);
	assert(n);
	while (--n) {
		i = bits;
		do {
			/* Square a1 into b1 */
			lbnMontSquare_32(b, a1, mod, mlen, inv);
			t = b; b = a; a = t;
			a1 = BIGLITTLE(a-mlen, a+mlen);
		} while (--i);
		t = *++array;
		lbnCopy_32(t, a1, mlen);
	}

	/* Hooray, we're done. */
	LBNFREE(b, 2*mlen);
	LBNFREE(a, 2*mlen);
	return 0;
}

/*
 * result = base^exp (mod mod).  "array" is a an array of pointers
 * to procomputed powers of base, each 2^bits apart.  (I.e. array[i]
 * is base^(2^(i*bits))).
 * 
 * The algorithm consists of:
 * a  = b  = (powers of g to be raised to the power 2^bits-1)
 * a *= b *= (powers of g to be raised to the power 2^bits-2)
 * ...
 * a *= b *= (powers of g to be raised to the power 1)
 * 
 * All we do is walk the exponent 2^bits-1 times in groups of "bits" bits,
 */
int
lbnBasePrecompExp_32(BNWORD32 *result, BNWORD32 const * const *array,
       unsigned bits, BNWORD32 const *exp, unsigned elen,
       BNWORD32 const *mod, unsigned mlen)
{
	BNWORD32 *a, *b, *c, *t;
	BNWORD32 *a1, *b1;
	int anull, bnull;	/* Null flags: values are implicitly 1 */
	unsigned i, j;				/* Loop counters */
	unsigned mask;				/* Exponent bits to examime */
	BNWORD32 const *eptr;			/* Pointer into exp */
	BNWORD32 buf, curbits, nextword;	/* Bit-buffer varaibles */
	BNWORD32 inv;				/* Inverse of LSW of modulus */
	unsigned ewords;			/* Words of exponent left */
	int bufbits;				/* Number of valid bits */
	int y = 0;

	mlen = lbnNorm_32(mod, mlen);
	assert (mlen);

	elen = lbnNorm_32(exp, elen);
	if (!elen) {
		lbnZero_32(result, mlen);
		BIGLITTLE(result[-1],result[0]) = 1;
		return 0;
	}
	/*
	 * This could be precomputed, but it's so cheap, and it would require
	 * making the precomputation structure word-size dependent.
	 */
	inv = lbnMontInv1_32(mod[BIGLITTLE(-1,0)]);	/* LSW of modulus */

	assert(elen);

	/*
	 * Allocate three temporary buffers.  The current numbers generally
	 * live in the upper halves of these buffers.
	 */
	LBNALLOC(a, BNWORD32, mlen*2);
	if (a) {
		LBNALLOC(b, BNWORD32, mlen*2);
		if (b) {
			LBNALLOC(c, BNWORD32, mlen*2);
			if (c)
				goto allocated;
			LBNFREE(b, 2*mlen);
		}
		LBNFREE(a, 2*mlen);
	}
	return -1;

allocated:

	anull = bnull = 1;

	mask = (1u<<bits) - 1;
	for (i = mask; i; --i) {
		/* Set up bit buffer for walking the exponent */
		eptr = exp;
		buf = BIGLITTLE(*--eptr, *eptr++);
		ewords = elen-1;
		bufbits = 32;
		for (j = 0; ewords || buf; j++) {
			/* Shift down current buffer */
			curbits = buf;
			buf >>= bits;
			/* If necessary, add next word */
			bufbits -= bits;
			if (bufbits < 0 && ewords > 0) {
				nextword = BIGLITTLE(*--eptr, *eptr++);
				ewords--;
				curbits |= nextword << (bufbits+bits);
				buf = nextword >> -bufbits;
				bufbits += 32;
			}
			/* If appropriate, multiply b *= array[j] */
			if ((curbits & mask) == i) {
				BNWORD32 const *d = array[j];

				b1 = BIGLITTLE(b-mlen-1,b+mlen);
				if (bnull) {
					lbnCopy_32(b1, d, mlen);
					bnull = 0;
				} else {
					lbnMontMul_32(c, b1, d, mod, mlen, inv);
					t = c; c = b; b = t;
				}
#if BNYIELD
				if (bnYield && (y = bnYield() < 0))
					goto yield;
#endif
			}
		}

		/* Multiply a *= b */
		if (!bnull) {
			a1 = BIGLITTLE(a-mlen-1,a+mlen);
			b1 = BIGLITTLE(b-mlen-1,b+mlen);
			if (anull) {
				lbnCopy_32(a1, b1, mlen);
				anull = 0;
			} else {
				lbnMontMul_32(c, a1, b1, mod, mlen, inv);
				t = c; c = a; a = t;
			}
		}
	}

	assert(!anull);	/* If it were, elen would have been 0 */

	/* Convert out of Montgomery form and return */
	a1 = BIGLITTLE(a-mlen-1,a+mlen);
	lbnCopy_32(a, a1, mlen);
	lbnZero_32(a1, mlen);
	lbnMontReduce_32(a, mod, mlen, inv);
	lbnCopy_32(result, a1, mlen);

#if BNYIELD
yield:
#endif
	LBNFREE(c, 2*mlen);
	LBNFREE(b, 2*mlen);
	LBNFREE(a, 2*mlen);

	return y;
}

/*
 * result = base1^exp1 *base2^exp2 (mod mod).  "array1" and "array2" are
 * arrays of pointers to procomputed powers of the corresponding bases,
 * each 2^bits apart.  (I.e. array1[i] is base1^(2^(i*bits))).
 * 
 * Bits must be the same in both.  (It could be made adjustable, but it's
 * a bit of a pain.  Just make them both equal to the larger one.)
 * 
 * The algorithm consists of:
 * a  = b  = (powers of base1 and base2  to be raised to the power 2^bits-1)
 * a *= b *= (powers of base1 and base2 to be raised to the power 2^bits-2)
 * ...
 * a *= b *= (powers of base1 and base2 to be raised to the power 1)
 * 
 * All we do is walk the exponent 2^bits-1 times in groups of "bits" bits,
 */
int
lbnDoubleBasePrecompExp_32(BNWORD32 *result, unsigned bits,
       BNWORD32 const * const *array1, BNWORD32 const *exp1, unsigned elen1,
       BNWORD32 const * const *array2, BNWORD32 const *exp2,
       unsigned elen2, BNWORD32 const *mod, unsigned mlen)
{
	BNWORD32 *a, *b, *c, *t;
	BNWORD32 *a1, *b1;
	int anull, bnull;	/* Null flags: values are implicitly 1 */
	unsigned i, j, k;				/* Loop counters */
	unsigned mask;				/* Exponent bits to examime */
	BNWORD32 const *eptr;			/* Pointer into exp */
	BNWORD32 buf, curbits, nextword;	/* Bit-buffer varaibles */
	BNWORD32 inv;				/* Inverse of LSW of modulus */
	unsigned ewords;			/* Words of exponent left */
	int bufbits;				/* Number of valid bits */
	int y = 0;
	BNWORD32 const * const *array;

	mlen = lbnNorm_32(mod, mlen);
	assert (mlen);

	elen1 = lbnNorm_32(exp1, elen1);
	if (!elen1) {
		return lbnBasePrecompExp_32(result, array2, bits, exp2, elen2,
		                            mod, mlen);
	}
	elen2 = lbnNorm_32(exp2, elen2);
	if (!elen2) {
		return lbnBasePrecompExp_32(result, array1, bits, exp1, elen1,
		                            mod, mlen);
	}
	/*
	 * This could be precomputed, but it's so cheap, and it would require
	 * making the precomputation structure word-size dependent.
	 */
	inv = lbnMontInv1_32(mod[BIGLITTLE(-1,0)]);	/* LSW of modulus */

	assert(elen1);
	assert(elen2);

	/*
	 * Allocate three temporary buffers.  The current numbers generally
	 * live in the upper halves of these buffers.
	 */
	LBNALLOC(a, BNWORD32, mlen*2);
	if (a) {
		LBNALLOC(b, BNWORD32, mlen*2);
		if (b) {
			LBNALLOC(c, BNWORD32, mlen*2);
			if (c)
				goto allocated;
			LBNFREE(b, 2*mlen);
		}
		LBNFREE(a, 2*mlen);
	}
	return -1;

allocated:

	anull = bnull = 1;

	mask = (1u<<bits) - 1;
	for (i = mask; i; --i) {
		/* Walk each exponent in turn */
		for (k = 0; k < 2; k++) {
			/* Set up the exponent for walking */
			array = k ? array2 : array1;
			eptr = k ? exp2 : exp1;
			ewords = (k ? elen2 : elen1) - 1;
			/* Set up bit buffer for walking the exponent */
			buf = BIGLITTLE(*--eptr, *eptr++);
			bufbits = 32;
			for (j = 0; ewords || buf; j++) {
				/* Shift down current buffer */
				curbits = buf;
				buf >>= bits;
				/* If necessary, add next word */
				bufbits -= bits;
				if (bufbits < 0 && ewords > 0) {
					nextword = BIGLITTLE(*--eptr, *eptr++);
					ewords--;
					curbits |= nextword << (bufbits+bits);
					buf = nextword >> -bufbits;
					bufbits += 32;
				}
				/* If appropriate, multiply b *= array[j] */
				if ((curbits & mask) == i) {
					BNWORD32 const *d = array[j];

					b1 = BIGLITTLE(b-mlen-1,b+mlen);
					if (bnull) {
						lbnCopy_32(b1, d, mlen);
						bnull = 0;
					} else {
						lbnMontMul_32(c, b1, d, mod, mlen, inv);
						t = c; c = b; b = t;
					}
#if BNYIELD
					if (bnYield && (y = bnYield() < 0))
						goto yield;
#endif
				}
			}
		}

		/* Multiply a *= b */
		if (!bnull) {
			a1 = BIGLITTLE(a-mlen-1,a+mlen);
			b1 = BIGLITTLE(b-mlen-1,b+mlen);
			if (anull) {
				lbnCopy_32(a1, b1, mlen);
				anull = 0;
			} else {
				lbnMontMul_32(c, a1, b1, mod, mlen, inv);
				t = c; c = a; a = t;
			}
		}
	}

	assert(!anull);	/* If it were, elen would have been 0 */

	/* Convert out of Montgomery form and return */
	a1 = BIGLITTLE(a-mlen-1,a+mlen);
	lbnCopy_32(a, a1, mlen);
	lbnZero_32(a1, mlen);
	lbnMontReduce_32(a, mod, mlen, inv);
	lbnCopy_32(result, a1, mlen);

#if BNYIELD
yield:
#endif
	LBNFREE(c, 2*mlen);
	LBNFREE(b, 2*mlen);
	LBNFREE(a, 2*mlen);

	return y;
}
