/*
 * Test driver for low-level bignum library (16-bit version).
 * This access the low-level library directly.  It is NOT an example of
 * how to program with the library normally!  By accessing the library
 * at a low level, it is possible to exercise the smallest components
 * and thus localize bugs more accurately.  This is especially useful
 * when writing assembly-language primitives.
 *
 * This also does timing tests on modular exponentiation.  Modular
 * exponentiation is so computationally expensive that the fact that this
 * code omits one level of interface glue has no perceptible effect on
 * the results.
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
#ifndef NO_STDLIB_H
#define NO_STDLIB_H 0
#endif
#ifndef NO_STRING_H
#define NO_STRING_H 0
#endif
#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 0
#endif

#include <stdio.h>

#if !NO_STDLIB_H
#include <stdlib.h>	/* For strtol */
#else
long strtol(const char *, char **, int);
#endif

#if !NO_STRING_H
#include <string.h>	/* For memcpy */
#elif HAVE_STRINGS_H
#include <strings.h>
#endif

#include "cputime.h"
#include "lbn16.h"

#include "kludge.h"

#if BNYIELD
int (*bnYield)(void) = 0;
#endif

/* Work with up to 2048-bit numbers */
#define MAXBITS 3072
#define SIZE (MAXBITS/16 + 1)

/* Additive congruential random number generator, x[i] = x[i-24] + x[i-55] */
static BNWORD16 randp[55];
static BNWORD16 *randp1 = randp, *randp2 = randp+24;

static BNWORD16
rand16(void)
{
    if (++randp2 == randp+55) {
	randp2 = randp;
	randp1++;
    } else if (++randp1 == randp+55) {
	randp1 = randp;
    }

    return  *randp1 += *randp2;
}

/*
 * CRC-3_2: x^3_2+x^26+x^23+x^22+x^1_6+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1
 *
 * The additive congruential RNG is seeded with a single integer,
 * which is shuffled with a CRC polynomial to generate the initial
 * table values.  The Polynomial is the same size as the words being
 * used.
 *
 * Thus, in the various versions of this library, we actually use this
 * polynomial as-is, this polynomial mod x^17, and this polynomial with
 * the leading coefficient deleted and replaced with x^6_4.  As-is,
 * it's irreducible, so it has a long period.  Modulo x^17, it factors as
 * (x^4+x^3+x^2+x+1) * (x^12+x^11+x^8+x^7+x^6+x^5+x^4+x^3+1),
 * which still has a large enough period (4095) for the use it's put to.
 * With the leading coefficient moved up, it factors as
 * (x^50+x^49+x^48+x^47+x^46+x^43+x^41+x^40+x^38+x^37+x^36+x^35+x^34+x^33+
 *  x^31+x^30+x^29+x^28+x^27+x^25+x^23+x^18+x^1_6+x^15+x^14+x^13+x^11+x^9+
 *  x^8+x^7+x^6+x^5+x^3+x^2+1)*(x^11+x^10+x^9+x^5+x^4+x^3+1)*(x^3+x+1),
 * which definitely has a long enough period to serve for initialization.
 * 
 * The effort put into this PRNG is kind of unwarranted given the trivial
 * use it's being put to, but oh, well.  It does have the nice advantage
 * of producing numbers that are portable between platforms, so if there's
 * a problem with one platform, you can compare all the intermediate
 * results with another platform.
 */
#define POLY (BNWORD16)0x04c11db7

static void
srand16(BNWORD16 seed)
{
    int i, j;

    for (i = 0; i < 55; i++) {
	for (j = 0; j < 16; j++)
	    if (seed >> (16-1))
		seed = (seed << 1) ^ POLY;
	    else
		seed <<= 1;
	randp[i] = seed;
    }
    for (i = 0; i < 3*55; i ++)
	rand16();
}

static void
randnum(BNWORD16 *num, unsigned len)
{
    while (len--)
	BIGLITTLE(*--num,*num++) = rand16();
}

static void
bnprint16(BNWORD16 const *num, unsigned len)
{
    BIGLITTLE(num -= len, num += len);

    while (len--)
	printf("%0*lX", 16/4, (unsigned long)BIGLITTLE(*num++,*--num));
}

static void
bnput16(char const *prompt, BNWORD16 const *num, unsigned len)
{
    fputs(prompt, stdout);
    bnprint16(num, len);
    putchar('\n');
}

/*
 * One of our tests uses a known prime.  The following selections were
 * taken from the tables at the end of Hans Reisel's "Prime Numbers and
 * Computer Methods for Factorization", second edition - an excellent book.
 * (ISBN 0-8176-3743-5 ISBN 3-7323-3743-5)
 */
#if 0
/* P31=1839605 17620282 38179967 87333633 from the factors of 3^256+2^256 */
static unsigned char const prime[] = {
	0x17,0x38,0x15,0xBC,0x8B,0xBB,0xE9,0xEF,0x01,0xA9,0xFD,0x3A,0x01
};
#elif 0
/* P48=40554942 04557502 46193993 36199835 4279613_2 73199617 from the same */
static unsigned char const prime[] = {
	0x47,0x09,0x77,0x07,0xCF,0xFD,0xE1,0x54,0x3E,0x24,
	0xF7,0xF1,0x7A,0x3E,0x91,0x51,0xCC,0xC7,0xD4,0x01
};
#elif 0
/*
 * P75 = 450 55287320 97906895 47687014 5808213_2
 *  05219565 99525911 39967932 66003_258 91979521
 * from the factors of 4^128+3+128
 * (The "026" and "062" are to prevent a Bad String from appearing here.)
 */
static unsigned char const prime[] = {
	0xFF,0x00,0xFF,0x00,0xFF,0x01,0x06,0x4F,0xF8,0xED,
	0xA3,0x37,0x23,0x2A,0x04,0xEA,0xF9,0x5F,0x30,0x4C,
	0xAE,0xCD, 026,0x4E, 062,0x10,0x04,0x7D,0x0D,0x79,
	0x01
};
#else
/*
 * P75 = 632 85659796 45277755 9123_2190 67300940
 *  51844953 78793489 59444670 35675855 57440257
 * from the factors of 5^128+4^128
 * (The "026" is to prevent a Bad String from appearing here.)
 */
static unsigned char const prime[] = {
	0x01,0x78,0x4B,0xA5,0xD3,0x30,0x03,0xEB,0x73,0xE6,
	0x0F,0x4E,0x31,0x7D,0xBC,0xE2,0xA0,0xD4, 026,0x3F,
	0x3C,0xEA,0x1B,0x44,0xAD,0x39,0xE7,0xE5,0xAD,0x19,
	0x67,0x01
};
#endif

static int
usage(char const *name)
{
	fprintf(stderr, "Usage: %s [modbits [expbits [expbits2]]\n"
"With no arguments, just runs test suite.  If modbits is given, runs\n"
"quick validation test, then runs timing tests of modular exponentiation.\n"
"If expbits is given, it is used as an exponent size, otherwise it defaults\n"
"to the same as modbits.  If expbits2 is given it is used as the second\n"
"exponent size in the double-exponentiation tests, otherwise it defaults\n"
"to the same as expbits.  All are limited to %u bits.\n",
		name, (unsigned)MAXBITS);
	return 1;
}

/* for libzrtp support */
int
bntest_main(int argc, char **argv)
{
    unsigned i, j, k, l, m;
    int z;
    BNWORD16 t, carry, borrow;
    BNWORD16 a[SIZE], b[SIZE], c[SIZE], d[SIZE];
    BNWORD16 e[SIZE], f[SIZE];
    static BNWORD16 entries[sizeof(prime)*2][(sizeof(prime)-1)/(16/8)+1];
    BNWORD16 *array[sizeof(prime)*2];
    unsigned long modbits = 0, expbits = 0, expbits2 = 0;
    char *p;
#define A BIGLITTLE((a+SIZE),a)
#define B BIGLITTLE((b+SIZE),b)
#define C BIGLITTLE((c+SIZE),c)
#define D BIGLITTLE((d+SIZE),d)
#define E BIGLITTLE((e+SIZE),e)
#define F BIGLITTLE((f+SIZE),f)
    static unsigned const smallprimes[] = {
	2, 3, 5, 7, 11, 13, 17, 19, 23, 27, 29, 31, 37, 41, 43
    };
	
    /* Set up array for precomputed modexp */
    for (i = 0; i < sizeof(array)/sizeof(*array); i++)
	array[i] = entries[i] BIG(+ SIZE);

    srand16(1);

    puts(BIGLITTLE("Big-endian machine","Little-endian machine"));

    if (argc >= 2) {
	    modbits = strtoul(argv[1], &p, 0);
	    if (!modbits || *p) {
		    fprintf(stderr, "Invalid modbits: %s\n", argv[1]);
		    return usage(argv[0]);
	    }
    }
    if (argc >= 3) {
	    expbits = strtoul(argv[2], &p, 0);
	    if (!expbits || *p) {
		    fprintf(stderr, "Invalid expbits: %s\n", argv[2]);
		    return usage(argv[0]);
	    }
	    expbits2 = expbits;
    }
    if (argc >= 4) {
	    expbits2 = strtoul(argv[3], &p, 0);
	    if (!expbits2 || *p) {
		    fprintf(stderr, "Invalid expbits2: %s\n", argv[3]);
		    return usage(argv[0]);
	    }
    }
    if (argc >= 5) {
	    fprintf(stderr, "Too many arguments: %s\n", argv[4]);
	    return usage(argv[0]);
    }
   	
    /* B is a nice not-so-little prime */
    lbnInsertBigBytes_16(B, prime, 0, sizeof(prime));
    ((unsigned char *)c)[0] = 0;
    lbnInsertBigBytes_16(B, (unsigned char *)c, sizeof(prime), 1);
    lbnExtractBigBytes_16(B, (unsigned char *)c, 0, sizeof(prime)+1);
    i = (sizeof(prime)-1)/(16/8)+1;        /* Size of array in words */
    if (((unsigned char *)c)[0] ||
	memcmp(prime, (unsigned char *)c+1, sizeof(prime)) != 0)
    {
	printf("Input != output!:\n   ");
	for (k = 0; k < sizeof(prime); k++)
	    printf("%02X ", prime[k]);
	putchar('\n');
	for (k = 0; k < sizeof(prime)+1; k++)
	    printf("%02X ", ((unsigned char *)c)[k]);
	putchar('\n');
	bnput16("p = ", B, i);

    }

    /* Timing test code - only if requested on the command line */
    if (modbits) {
	timetype start, stop;
	unsigned long cursec, expsec, twoexpsec, dblexpsec;
	unsigned curms, expms, twoexpms, dblexpms;

	expsec = twoexpsec = dblexpsec = 0;
	expms = twoexpms = dblexpms = 0;

	lbnCopy_16(C,B,i);
	lbnSub1_16(C,i,1);        /* C is exponent: p-1 */

	puts("Testing modexp with a known prime.  "
	     "All results should be 1.");
	bnput16("p   = ", B, i);
	bnput16("p-1 = ", C, i);
	z = lbnTwoExpMod_16(A, C, i, B, i);
	if (z < 0)
	    goto nomem;
	bnput16("2^(p-1) mod p = ", A, i);
	for (j = 0; j < 10; j++) {
	    randnum(A,i);
	    (void)lbnDiv_16(D,A,i,B,i);

	    bnput16("a = ", A, i);
	    z = lbnExpMod_16(D, A, i, C, i, B, i);
	    if (z < 0)
		goto nomem;
	    bnput16("a^(p-1) mod p = ", D, i);
#if 0		
	    z = lbnBasePrecompBegin_16(array, (sizeof(prime)*8+4)/5, 5,
				       A, i, B, i);
	    if (z < 0)
		goto nomem;
	    BIGLITTLE(D[-1],D[0]) = -1;
	    z = lbnBasePrecompExp_16(D, (BNWORD16 const * const *)array,
			   	     5, C, i, B, i);
	    if (z < 0)
		goto nomem;
	    bnput16("a^(p-1) mod p = ", D, i);
#endif		
	    for (k = 0; k < 5; k++) {
		randnum(E,i);
		bnput16("e = ", E, i);
		z = lbnExpMod_16(D, A, i, E, i, B, i);
		if (z < 0)
		    goto nomem;
		bnput16("a^e mod p = ", D, i);
#if 0
		z = lbnBasePrecompExp_16(D, (BNWORD16 const * const *)array,
					 5, E, i, B, i);
		if (z < 0)
		    goto nomem;
		bnput16("a^e mod p = ", D, i);
#endif
	    }	
	}

	printf("\n"
	       "Timing exponentiations modulo a %d-bit modulus, i.e.\n"
	       "2^<%d> mod <%d> bits, <%d>^<%d> mod <%d> bits and\n"
	       "<%d>^<%d> * <%d>^<%d> mod <%d> bits\n",
	       (int)modbits, (int)expbits, (int)modbits,
	       (int)modbits, (int)expbits, (int)modbits,
	       (int)modbits, (int)expbits, (int)modbits, (int)expbits2,
	       (int)modbits);

	i = ((int)modbits-1)/16+1;
	k = ((int)expbits-1)/16+1;
	l = ((int)expbits2-1)/16+1;
	for (j = 0; j < 25; j++) {
	    randnum(A,i);        /* Base */
	    randnum(B,k);        /* Exponent */
	    randnum(C,i);        /* Modulus */
	    randnum(D,i);        /* Base2 */
	    randnum(E,l);        /* Exponent */
	    /* Clip bases and mod to appropriate number of bits */
	    t = ((BNWORD16)2<<((modbits-1)%16)) - 1;
	    *(BIGLITTLE(A-i,A+i-1)) &= t;
	    *(BIGLITTLE(C-i,C+i-1)) &= t;
	    *(BIGLITTLE(D-i,D+i-1)) &= t;
	    /* Make modulus large (msbit set) and odd (lsbit set) */
	    *(BIGLITTLE(C-i,C+i-1)) |= (t >> 1) + 1;
	    BIGLITTLE(C[-1],C[0]) |= 1;

	    /* Clip exponent to appropriate number of bits */
	    t = ((BNWORD16)2<<((expbits-1)%16)) - 1;
	    *(BIGLITTLE(B-k,B+k-1)) &= t;
	    /* Make exponent large (msbit set) */
	    *(BIGLITTLE(B-k,B+k-1)) |= (t >> 1) + 1;
	    /* The same for exponent 2 */
	    t = ((BNWORD16)2<<((expbits2-1)%16)) - 1;
	    *(BIGLITTLE(E-l,E+l-1)) &= t;
	    *(BIGLITTLE(E-l,E+l-1)) |= (t >> 1) + 1;

	    m = lbnBits_16(A, i);
	    if (m > (unsigned)modbits) {
		bnput16("a = ", a, i);
		printf("%u bits, should be <= %d\n",
		       m, (int)modbits);
	    }
	    m = lbnBits_16(B, k);
	    if (m != (unsigned)expbits) {
		bnput16("b = ", b, i);
		printf("%u bits, should be %d\n",
		       m, (int)expbits);
	    }
	    m = lbnBits_16(C, i);
	    if (m != (unsigned)modbits) {
		bnput16("c = ", c, k);
		printf("%u bits, should be %d\n",
		       m, (int)modbits);
	    }
	    m = lbnBits_16(D, i);
	    if (m > (unsigned)modbits) {
		bnput16("d = ", d, i);
		printf("%u bits, should be <= %d\n",
		       m, (int)modbits);
	    }
	    m = lbnBits_16(E, l);
	    if (m != (unsigned)expbits2) {
		bnput16("e = ", e, i);
		printf("%u bits, should be %d\n",
		       m, (int)expbits2);
	    }
	    gettime(&start);
	    z = lbnTwoExpMod_16(A, B, k, C, i);
	    if (z < 0)
		goto nomem;
	    gettime(&stop);
	    subtime(stop, start);
	    twoexpsec += cursec = sec(stop);
	    twoexpms += curms = msec(stop);

	    printf("2^<%d>:%4lu.%03u   ", (int)expbits, cursec, curms);
	    fflush(stdout);

	    gettime(&start);
	    z = lbnExpMod_16(A, A, i, B, k, C, i);
	    if (z < 0)
		goto nomem;
	    gettime(&stop);
	    subtime(stop, start);
	    expsec += cursec = sec(stop);
	    expms += curms = msec(stop);
	    printf("<%d>^<%d>:%4lu.%03u   ",(int)modbits, (int)expbits,
			    cursec, curms);
	    fflush(stdout);

#if 0
	    gettime(&start);
	    z = lbnDoubleExpMod_16(D, A, i, B, k, D, i, E, l,C,i);
	    if (z < 0)
		goto nomem;
	    gettime(&stop);
	    subtime(stop, start);
	    dblexpsec += cursec = sec(stop);
	    dblexpms += curms = msec(stop);
	    printf("<%d>^<%d>*<%d>^<%d>:%4lu.%03u\n",
		   (int)modbits, (int)expbits,
		   (int)modbits, (int)expbits2,
		   cursec, curms);
#else
	    putchar('\n');
#endif
	}
	twoexpms += (twoexpsec % j) * 1000;
	printf("2^<%d> mod <%d> bits AVERAGE: %4lu.%03u s\n",
	       (int)expbits, (int)modbits, twoexpsec/j, twoexpms/j);
	expms += (expsec % j) * 1000;
	printf("<%d>^<%d> mod <%d> bits AVERAGE: %4lu.%03u s\n",
	       (int)modbits, (int)expbits, (int)modbits, expsec/j, expms/j);
#if 0
	dblexpms += (dblexpsec % j) * 1000;
	printf("<%d>^<%d> * <%d>^<%d> mod <%d> bits AVERAGE:"
	       " %4lu.%03u s\n",
	       (int)modbits, (int)expbits, (int)modbits, 
	       (int)expbits2,
	       (int)modbits, dblexpsec/j, dblexpms/j);
#endif
	putchar('\n');
    }

    printf("Beginning 1000 interations of sanity checking.\n");
    printf("Any output indicates a bug.  No output is very strong\n");
    printf("evidence that all the important low-level bignum routines\n");
    printf("are working properly.\n");

    /*
     * If you change this loop to have an iteration 0, all results
     * are primted on that iteration.  Useful to see what's going
     * on in case of major wierdness, but it produces a *lot* of
     * output.
     */
    for (j = 1; j <= 1000; j++) {
	/* Do the tests for lots of different number sizes. */
	for (i = 1; i <= SIZE/2; i++) {
	    /* Make a random number i words long */
	    do {
		randnum(A,i);
	    } while (lbnNorm_16(A,i) < i);

	    /* Checl lbnCmp - does a == a? */
	    if (lbnCmp_16(A,A,i) || !j) {
		bnput16("a = ", A, i);
		printf("(a <=> a) = %d\n", lbnCmp_16(A,A,i));
	    }

	    memcpy(c, a, sizeof(a));

	    /* Check that the difference, after copy, is good. */
	    if (lbnCmp_16(A,C,i) || !j) {
		bnput16("a = ", A, i);
		bnput16("c = ", C, i);
		printf("(a <=> c) = %d\n", lbnCmp_16(A,C,i));
	    }

	    /* Generate a non-zero random t */
	    do {
		t = rand16();
	    } while (!t);

	    /*
	     * Add t to A.  Check that:
	     * - lbnCmp works in both directions, and
	     * - A + t is greater than A.  If there was a carry,
	     *   the result, less the carry, should be *less*
	     *   than A.
	     */
	    carry = lbnAdd1_16(A,i,t);
	    if (lbnCmp_16(A,C,i) + lbnCmp_16(C,A,i) != 0 ||
		lbnCmp_16(A,C,i) != (carry ? -1 : 1) || !j)
	    {
		bnput16("c       = ", C, i);
		printf("t = %lX\n", (unsigned long)t);
		bnput16("a = c+t = ", A, i);
		printf("carry = %lX\n", (unsigned long)carry);
		printf("(a <=> c) = %d\n", lbnCmp_16(A,C,i));
		printf("(c <=> a) = %d\n", lbnCmp_16(C,A,i));
	    }

	    /* Subtract t again */
	    memcpy(d, a, sizeof(a));
	    borrow = lbnSub1_16(A,i,t);

	    if (carry != borrow || lbnCmp_16(A,C,i) || !j) {
		bnput16("a = ", C, i);
		printf("t = %lX\n", (unsigned long)t);
		lbnAdd1_16(A,i,t);
		bnput16("a += t = ", A, i);
		printf("Carry = %lX\n", (unsigned long)carry);
		lbnSub1_16(A,i,t);
		bnput16("a -= t = ", A, i);
		printf("Borrow = %lX\n", (unsigned long)borrow);
		printf("(a <=> c) = %d\n", lbnCmp_16(A,C,i));
	    }

	    /* Generate a random B */
	    do {
		randnum(B,i);
	    } while (lbnNorm_16(B,i) < i);

	    carry = lbnAddN_16(A,B,i);
	    memcpy(d, a, sizeof(a));
	    borrow = lbnSubN_16(A,B,i);

	    if (carry != borrow || lbnCmp_16(A,C,i) || !j) {
		bnput16("a = ", C, i);
		bnput16("b = ", B, i);
		bnput16("a += b = ", D, i);
		printf("Carry = %lX\n", (unsigned long)carry);
		bnput16("a -= b = ", A, i);
		printf("Borrow = %lX\n", (unsigned long)borrow);
		printf("(a <=> c) = %d\n", lbnCmp_16(A,C,i));
	    }

	    /* D = B * t */
	    lbnMulN1_16(D, B, i, t);
	    memcpy(e, d, sizeof(e));
	    /* D = A + B * t, "carry" is overflow */
	    borrow = *(BIGLITTLE(D-i-1,D+i)) += lbnAddN_16(D,A,i);

	    carry = lbnMulAdd1_16(A, B, i, t);

	    /* Did MulAdd get the same answer as mul then add? */
	    if (carry != borrow || lbnCmp_16(A, D, i) || !j) {
		bnput16("a = ", C, i);
		bnput16("b = ", B, i);
		printf("t = %lX\n", (unsigned long)t);
		bnput16("e = b * t = ", E, i+1);
		bnput16("    a + e = ", D, i+1);
		bnput16("a + b * t = ", A, i);
		printf("carry = %lX\n", (unsigned long)carry);
	    }

	    memcpy(d, a, sizeof(a));
	    borrow = lbnMulSub1_16(A, B, i, t);

	    /* Did MulSub perform the inverse of MulAdd */
	    if (carry != borrow || lbnCmp_16(A,C,i) || !j) {
		bnput16("       a = ", C, i);
		bnput16("       b = ", B, i);
		bnput16("a += b*t = ", D, i);
		printf("Carry = %lX\n", (unsigned long)carry);
		bnput16("a -= b*t = ", A, i);
		printf("Borrow = %lX\n", (unsigned long)borrow);
		printf("(a <=> c) = %d\n", lbnCmp_16(A,C,i));
		bnput16("b*t = ", E, i+1);
	    }
	    /* At this point we're done with t, so it's scratch */
#if 0
/* Extra debug code */
	    lbnMulN1_16(C, A, i, BIGLITTLE(B[-1],B[0]));
	    bnput16("a * b[0] = ", C, i+1);
	    for (k = 1; k < i; k++) {
		carry = lbnMulAdd1_16(BIGLITTLE(C-k,C+k), A, i, 
				      *(BIGLITTLE(B-1-k,B+k)));
		*(BIGLITTLE(C-i-k,C+i+k)) = carry;
		bnput16("a * b[x] = ", C, i+k+1);
	    }

	    lbnMulN1_16(D, B, i, BIGLITTLE(A[-1],A[0]));
	    bnput16("b * a[0] = ", D, i+1);
	    for (k = 1; k < i; k++) {
		carry = lbnMulAdd1_16(BIGLITTLE(D-k,D+k), B, i, 
				      *(BIGLITTLE(A-1-k,A+k)));
		*(BIGLITTLE(D-i-k,D+i+k)) = carry;
		bnput16("b * a[x] = ", D, i+k+1);
	    }
#endif
	    /* Does Mul work both ways symmetrically */
	    lbnMul_16(C,A,i,B,i);
	    lbnMul_16(D,B,i,A,i);
	    if (lbnCmp_16(C,D,i+i) || !j) {
		bnput16("a = ", A, i);
		bnput16("b = ", B, i);
		bnput16("a * b = ", C, i+i);
		bnput16("b * a = ", D, i+i);
		printf("(a*b <=> b*a) = %d\n",
		       lbnCmp_16(C,D,i+i));
	    }
	    /* Check multiplication modulo some small things */
	    /* 30030 = 2*3*5*11*13 */
	    k = lbnModQ_16(C, i+i, 30030);
	    for (l = 0;
		 l < sizeof(smallprimes)/sizeof(*smallprimes);
		 l++)
	    {
		m = smallprimes[l];
		t = lbnModQ_16(C, i+i, m);
		carry = lbnModQ_16(A, i, m);
		borrow = lbnModQ_16(B, i, m);
		if (t != (carry * borrow) % m) {
		    bnput16("a = ", A, i);
		    printf("a mod %u = %u\n", m,
			   (unsigned)carry);
		    bnput16("b = ", B, i);
		    printf("b mod %u = %u\n", m,
			   (unsigned)borrow);
		    bnput16("a*b = ", C, i+i);
		    printf("a*b mod %u = %u\n", m,
			   (unsigned)t);
		    printf("expected %u\n",
			   (unsigned)((carry*borrow)%m));
		}
				/* Verify that (C % 30030) % m == C % m */
		if (m <= 13 && t != k % m) {
		    printf("c mod 30030 = %u mod %u= %u\n",
			   k, m, k%m);
		    printf("c mod %u = %u\n",
			   m, (unsigned)t);
		}
	    }

	    /* Generate an F less than A and B */
	    do {
		randnum(F,i);
	    } while (lbnCmp_16(F,A,i) >= 0 ||
		     lbnCmp_16(F,B,i) >= 0);

	    /* Add F to D (remember, D = A*B) */
	    lbnAdd1_16(BIGLITTLE(D-i,D+i), i, lbnAddN_16(D, F, i));
	    memcpy(c, d, sizeof(d));

	    /*
	     * Divide by A and check that quotient and remainder
	     * match (remainder should be F, quotient should be B)
	     */
	    t = lbnDiv_16(E,C,i+i,A,i);
	    if (t || lbnCmp_16(E,B,i) || lbnCmp_16(C, F, i) || !j) {
		bnput16("a = ", A, i);
		bnput16("b = ", B, i);
		bnput16("f = ", F, i);
		bnput16("a * b + f = ", D, i+i);
		printf("qhigh = %lX\n", (unsigned long)t);
		bnput16("(a*b+f) / a = ", E, i);
		bnput16("(a*b+f) % a = ", C, i);
	    }

	    memcpy(c, d, sizeof(d));

	    /* Divide by B and check similarly */
	    t = lbnDiv_16(E,C,i+i,B,i);
	    if (lbnCmp_16(E,A,i) || lbnCmp_16(C, F, i) || !j) {
		bnput16("a = ", A, i);
		bnput16("b = ", B, i);
		bnput16("f = ", F, i);
		bnput16("a * b + f = ", D, i+i);
		printf("qhigh = %lX\n", (unsigned long)t);
		bnput16("(a*b+f) / b = ", E, i);
		bnput16("(a*b+f) % b = ", C, i);
	    }

	    /* Check that A*A == A^2 */
	    lbnMul_16(C,A,i,A,i);
	    lbnSquare_16(D,A,i);
	    if (lbnCmp_16(C,D,i+i) || !j) {
		bnput16("a*a = ", C, i+i);
		bnput16("a^2 = ", D, i+i);
		printf("(a * a == a^2) = %d\n",
		       lbnCmp_16(C,D,i+i));
	    }
#if 0
	    /* Compute a GCD */
	    lbnCopy_16(C,A,i);
	    lbnCopy_16(D,B,i);
	    z = lbnGcd_16(C, i, D, i, &k);
	    if (z < 0)
		goto nomem;
	    /* z = 1 if GCD in D; z = 0 if GCD in C */

	    /* Approximate check that the GCD came out right */
	    for (l = 0;
		 l < sizeof(smallprimes)/sizeof(*smallprimes);
		 l++)
	    {
		m = smallprimes[l];
		t = lbnModQ_16(z ? D : C, k, m);
		carry = lbnModQ_16(A, i, m);
		borrow = lbnModQ_16(B, i, m);
		if (!t != (!carry && !borrow)) {
		    bnput16("a = ", A, i);
		    printf("a mod %u = %u\n", m,
			   (unsigned)carry);
		    bnput16("b = ", B, i);
		    printf("b mod %u = %u\n", m,
			   (unsigned)borrow);
		    bnput16("gcd(a,b) = ", z ? D : C, k);
		    printf("gcd(a,b) mod %u = %u\n", m,
			   (unsigned)t);
		}
	    }
#endif

	    /*
	     * Do some Montgomery operations
	     * Start with A > B, and also place a copy of B into C.
	     * Then make A odd so it can be a Montgomery modulus.
	     */
	    if (lbnCmp_16(A, B, i) < 0) {
		memcpy(c, a, sizeof(c));
		memcpy(a, b, sizeof(a));
		memcpy(b, c, sizeof(b));
	    } else {
		memcpy(c, b, sizeof(c));
	    }
	    BIGLITTLE(A[-1],A[0]) |= 1;
			
	    /* Convert to and from */
	    lbnToMont_16(B, i, A, i);
	    lbnFromMont_16(B, A, i);
	    if (lbnCmp_16(B, C, i)) {
		memcpy(b, c, sizeof(c));
		bnput16("mod = ", A, i);
		bnput16("input = ", B, i);
		lbnToMont_16(B, i, A, i);
		bnput16("mont = ", B, i);
		lbnFromMont_16(B, A, i);
		bnput16("output = ", B, i);
	    }
	    /* E = B^5 (mod A), no Montgomery ops */
	    lbnSquare_16(E, B, i);
	    (void)lbnDiv_16(BIGLITTLE(E-i,E+i),E,i+i,A,i);
	    lbnSquare_16(D, E, i);
	    (void)lbnDiv_16(BIGLITTLE(D-i,D+i),D,i+i,A,i);
	    lbnMul_16(E, D, i, B, i);
	    (void)lbnDiv_16(BIGLITTLE(E-i,E+i),E,i+i,A,i);

	    /* D = B^5, using ExpMod */
	    BIGLITTLE(F[-1],F[0]) = 5;
	    z = lbnExpMod_16(D, B, i, F, 1, A, i);
	    if (z < 0)
		goto nomem;
	    if (lbnCmp_16(D, E, i)  || !j) {
		bnput16("mod = ", A, i);
		bnput16("input = ", B, i);
		bnput16("input^5 = ", E, i);
		bnput16("input^5 = ", D, i);
		printf("a>b (x <=> y) = %d\n",
		       lbnCmp_16(D,E,i));
	    }
	    /* TODO: Test lbnTwoExpMod, lbnDoubleExpMod */
	} /* for (i) */
	printf("\r%d ", j);
	fflush(stdout);
    } /* for (j) */
    printf("%d iterations of up to %d 16-bit words completed.\n",
	   j-1, i-1);
    return 0;
 nomem:
    printf("Out of memory\n");
    return 1;
}
