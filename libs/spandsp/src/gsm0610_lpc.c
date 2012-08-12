/*
 * SpanDSP - a series of DSP components for telephony
 *
 * gsm0610_lpc.c - GSM 06.10 full rate speech codec.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This code is based on the widely used GSM 06.10 code available from
 * http://kbs.cs.tu-berlin.de/~jutta/toast.html
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <inttypes.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <stdlib.h>
#include <memory.h>

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/bitstream.h"
#include "spandsp/bit_operations.h"
#include "spandsp/saturated.h"
#include "spandsp/vector_int.h"
#include "spandsp/gsm0610.h"

#include "gsm0610_local.h"

/* 4.2.4 .. 4.2.7 LPC ANALYSIS SECTION */

/* The number of left shifts needed to normalize the 32 bit
   variable x for positive values on the interval
   with minimum of
   minimum of 1073741824  (01000000000000000000000000000000) and 
   maximum of 2147483647  (01111111111111111111111111111111)
   and for negative values on the interval with
   minimum of -2147483648 (-10000000000000000000000000000000) and
   maximum of -1073741824 ( -1000000000000000000000000000000).
  
   In order to normalize the result, the following
   operation must be done: norm_var1 = x << gsm0610_norm(x);
  
   (That's 'ffs', only from the left, not the right..)
*/

int16_t gsm0610_norm(int32_t x)
{
    assert(x != 0);

    if (x < 0)
    {
        if (x <= -1073741824)
            return 0;
        /*endif*/
        x = ~x;
    }
    /*endif*/
    return (int16_t) (30 - top_bit(x));
}
/*- End of function --------------------------------------------------------*/

/*
   (From p. 46, end of section 4.2.5)
  
   NOTE: The following lines gives [sic] one correct implementation
         of the div(num, denum) arithmetic operation.  Compute div
         which is the integer division of num by denom: with
         denom >= num > 0
*/
static int16_t gsm_div(int16_t num, int16_t denom)
{
    int32_t num32;
    int32_t denom32;
    int16_t div;
    int k;

    /* The parameter num sometimes becomes zero.
       Although this is explicitly guarded against in 4.2.5,
       we assume that the result should then be zero as well. */

    assert(num >= 0  &&  denom >= num);
    if (num == 0)
        return 0;
    /*endif*/
    num32 = num;
    denom32 = denom;
    div = 0;
    k = 15;
    while (k--)
    {
        div <<= 1;
        num32 <<= 1;

        if (num32 >= denom32)
        {
            num32 -= denom32;
            div++;
        }
        /*endif*/
    }
    /*endwhile*/

    return div;
}
/*- End of function --------------------------------------------------------*/

#if defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)
static void gsm0610_vec_vsraw(const int16_t *p, int n, int bits)
{
    static const int64_t ones = 0x0001000100010001LL;

    if (n == 0)
        return;
    /*endif*/
#if defined(__x86_64__)
    __asm__ __volatile__(
        " leaq -16(%%rsi,%%rax,2),%%rdx;\n"         /* edx = top - 16 */
        " emms;\n"
        " movd %%ecx,%%mm3;\n"
        " movq %[ones],%%mm2;\n"
        " psllw %%mm3,%%mm2;\n"
        " psrlw $1,%%mm2;\n"
        " cmpq %%rdx,%%rsi;"
        " ja 4f;\n"

         " .p2align 2;\n"
        /* 8 words per iteration */
        "6:\n"
        " movq (%%rsi),%%mm0;\n"
        " movq 8(%%rsi),%%mm1;\n"
        " paddsw %%mm2,%%mm0;\n"
        " psraw %%mm3,%%mm0;\n"
        " paddsw %%mm2,%%mm1;\n"
        " psraw %%mm3,%%mm1;\n"
        " movq %%mm0,(%%rsi);\n"
        " movq %%mm1,8(%%rsi);\n"
        " addq $16,%%rsi;\n"
        " cmpq %%rdx,%%rsi;\n"
        " jbe 6b;\n"

        " .p2align 2;\n"
        "4:\n"
        " addq $12,%%rdx;\n"                        /* now edx = top-4 */
        " cmpq %%rdx,%%rsi;\n"
        " ja 3f;\n"

        " .p2align 2;\n"
        /* do up to 6 words, two per iteration */
        "5:\n"
        " movd (%%rsi),%%mm0;\n"
        " paddsw %%mm2,%%mm0;\n"
        " psraw %%mm3,%%mm0;\n"
        " movd %%mm0,(%%rsi);\n"
        " addq $4,%%rsi;\n"
        " cmpq %%rdx,%%rsi;\n"
        " jbe 5b;\n"

        " .p2align 2;\n"
        "3:\n"
        " addq $2,%%rdx;\n"                        /* now edx = top-2 */
        " cmpq %%rdx,%%rsi;\n"
        " ja 2f;\n"
        
        " movzwl (%%rsi),%%eax;\n"
        " movd %%eax,%%mm0;\n"
        " paddsw %%mm2,%%mm0;\n"
        " psraw %%mm3,%%mm0;\n"
        " movd %%mm0,%%eax;\n"
        " movw %%ax,(%%rsi);\n"

        " .p2align 2;\n"
        "2:\n"
        " emms;\n"
        :
        : "S" (p), "a" (n), "c" (bits), [ones] "m" (ones)
        : "edx"
    );
#else
    __asm__ __volatile__(
        " leal -16(%%esi,%%eax,2),%%edx;\n"         /* edx = top - 16 */
        " emms;\n"
        " movd %%ecx,%%mm3;\n"
        " movq %[ones],%%mm2;\n"
        " psllw %%mm3,%%mm2;\n"
        " psrlw $1,%%mm2;\n"
        " cmpl %%edx,%%esi;"
        " ja 4f;\n"

         " .p2align 2;\n"
        /* 8 words per iteration */
        "6:\n"
        " movq (%%esi),%%mm0;\n"
        " movq 8(%%esi),%%mm1;\n"
        " paddsw %%mm2,%%mm0;\n"
        " psraw %%mm3,%%mm0;\n"
        " paddsw %%mm2,%%mm1;\n"
        " psraw %%mm3,%%mm1;\n"
        " movq %%mm0,(%%esi);\n"
        " movq %%mm1,8(%%esi);\n"
        " addl $16,%%esi;\n"
        " cmpl %%edx,%%esi;\n"
        " jbe 6b;\n"

        " .p2align 2;\n"
        "4:\n"
        " addl $12,%%edx;\n"                        /* now edx = top-4 */
        " cmpl %%edx,%%esi;\n"
        " ja 3f;\n"

        " .p2align 2;\n"
        /* do up to 6 words, two per iteration */
        "5:\n"
        " movd (%%esi),%%mm0;\n"
        " paddsw %%mm2,%%mm0;\n"
        " psraw %%mm3,%%mm0;\n"
        " movd %%mm0,(%%esi);\n"
        " addl $4,%%esi;\n"
        " cmpl %%edx,%%esi;\n"
        " jbe 5b;\n"

        " .p2align 2;\n"
        "3:\n"
        " addl $2,%%edx;\n"                        /* now edx = top-2 */
        " cmpl %%edx,%%esi;\n"
        " ja 2f;\n"
        
        " movzwl (%%esi),%%eax;\n"
        " movd %%eax,%%mm0;\n"
        " paddsw %%mm2,%%mm0;\n"
        " psraw %%mm3,%%mm0;\n"
        " movd %%mm0,%%eax;\n"
        " movw %%ax,(%%esi);\n"

        " .p2align 2;\n"
        "2:\n"
        " emms;\n"
        :
        : "S" (p), "a" (n), "c" (bits), [ones] "m" (ones)
        : "edx"
    );
#endif
}
/*- End of function --------------------------------------------------------*/
#endif

/* 4.2.4 */
static void autocorrelation(int16_t amp[GSM0610_FRAME_LEN], int32_t L_ACF[9])
{
    int k;
    int16_t smax;
    int16_t scalauto;
#if !(defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX))
    int i;
    int temp;
    int16_t *sp;
    int16_t sl;
#endif
    
    /* The goal is to compute the array L_ACF[k].  The signal s[i] must
       be scaled in order to avoid an overflow situation. */

    /* Dynamic scaling of the array  s[0..159] */
    /* Search for the maximum. */
#if defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)
    smax = saturate(vec_min_maxi16(amp, GSM0610_FRAME_LEN, NULL));
#else
    for (smax = 0, k = 0;  k < GSM0610_FRAME_LEN;  k++)
    {
        temp = saturated_abs16(amp[k]);
        if (temp > smax)
            smax = (int16_t) temp;
        /*endif*/
    }
    /*endfor*/
#endif

    /* Computation of the scaling factor. */
    if (smax == 0)
    {
        scalauto = 0;
    }
    else
    {
        assert(smax > 0);
        scalauto = (int16_t) (4 - gsm0610_norm((int32_t) smax << 16));
    }
    /*endif*/

    /* Scaling of the array s[0...159] */
#if defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)
    if (scalauto > 0)
        gsm0610_vec_vsraw(amp, GSM0610_FRAME_LEN, scalauto);
    /*endif*/
#else
    if (scalauto > 0)
    {
        for (k = 0;  k < GSM0610_FRAME_LEN;  k++)
            amp[k] = gsm_mult_r(amp[k], 16384 >> (scalauto - 1));
        /*endfor*/
    }
    /*endif*/
#endif

    /* Compute the L_ACF[..]. */
#if defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)
    for (k = 0;  k < 9;  k++)
        L_ACF[k] = vec_dot_prodi16(amp, amp + k, GSM0610_FRAME_LEN - k) << 1;
    /*endfor*/
#else
    sp = amp;
    sl = *sp;
    L_ACF[0] = ((int32_t) sl*(int32_t) sp[0]);
    sl = *++sp;
    L_ACF[0] += ((int32_t) sl*(int32_t) sp[0]);
    L_ACF[1] = ((int32_t) sl*(int32_t) sp[-1]);
    sl = *++sp;
    L_ACF[0] += ((int32_t) sl*(int32_t) sp[0]);
    L_ACF[1] += ((int32_t) sl*(int32_t) sp[-1]);
    L_ACF[2] = ((int32_t) sl*(int32_t) sp[-2]);
    sl = *++sp;
    L_ACF[0] += ((int32_t) sl*(int32_t) sp[0]);
    L_ACF[1] += ((int32_t) sl*(int32_t) sp[-1]);
    L_ACF[2] += ((int32_t) sl*(int32_t) sp[-2]);
    L_ACF[3] = ((int32_t) sl*(int32_t) sp[-3]);
    sl = *++sp;
    L_ACF[0] += ((int32_t) sl*(int32_t) sp[0]);
    L_ACF[1] += ((int32_t) sl*(int32_t) sp[-1]);
    L_ACF[2] += ((int32_t) sl*(int32_t) sp[-2]);
    L_ACF[3] += ((int32_t) sl*(int32_t) sp[-3]);
    L_ACF[4] = ((int32_t) sl*(int32_t) sp[-4]);
    sl = *++sp;
    L_ACF[0] += ((int32_t) sl*(int32_t) sp[0]);
    L_ACF[1] += ((int32_t) sl*(int32_t) sp[-1]);
    L_ACF[2] += ((int32_t) sl*(int32_t) sp[-2]);
    L_ACF[3] += ((int32_t) sl*(int32_t) sp[-3]);
    L_ACF[4] += ((int32_t) sl*(int32_t) sp[-4]);
    L_ACF[5] = ((int32_t) sl*(int32_t) sp[-5]);
    sl = *++sp;
    L_ACF[0] += ((int32_t) sl*(int32_t) sp[0]);
    L_ACF[1] += ((int32_t) sl*(int32_t) sp[-1]);
    L_ACF[2] += ((int32_t) sl*(int32_t) sp[-2]);
    L_ACF[3] += ((int32_t) sl*(int32_t) sp[-3]);
    L_ACF[4] += ((int32_t) sl*(int32_t) sp[-4]);
    L_ACF[5] += ((int32_t) sl*(int32_t) sp[-5]);
    L_ACF[6] = ((int32_t) sl*(int32_t) sp[-6]);
    sl = *++sp;
    L_ACF[0] += ((int32_t) sl*(int32_t) sp[0]);
    L_ACF[1] += ((int32_t) sl*(int32_t) sp[-1]);
    L_ACF[2] += ((int32_t) sl*(int32_t) sp[-2]);
    L_ACF[3] += ((int32_t) sl*(int32_t) sp[-3]);
    L_ACF[4] += ((int32_t) sl*(int32_t) sp[-4]);
    L_ACF[5] += ((int32_t) sl*(int32_t) sp[-5]);
    L_ACF[6] += ((int32_t) sl*(int32_t) sp[-6]);
    L_ACF[7] = ((int32_t) sl*(int32_t) sp[-7]);
    sl = *++sp;
    L_ACF[0] += ((int32_t) sl*(int32_t) sp[0]);
    L_ACF[1] += ((int32_t) sl*(int32_t) sp[-1]);
    L_ACF[2] += ((int32_t) sl*(int32_t) sp[-2]);
    L_ACF[3] += ((int32_t) sl*(int32_t) sp[-3]);
    L_ACF[4] += ((int32_t) sl*(int32_t) sp[-4]);
    L_ACF[5] += ((int32_t) sl*(int32_t) sp[-5]);
    L_ACF[6] += ((int32_t) sl*(int32_t) sp[-6]);
    L_ACF[7] += ((int32_t) sl*(int32_t) sp[-7]);
    L_ACF[8] = ((int32_t) sl*(int32_t) sp[-8]);
    for (i = 9;  i < GSM0610_FRAME_LEN;  i++)
    {
        sl = *++sp;
        L_ACF[0] += ((int32_t) sl*(int32_t) sp[0]);
        L_ACF[1] += ((int32_t) sl*(int32_t) sp[-1]);
        L_ACF[2] += ((int32_t) sl*(int32_t) sp[-2]);
        L_ACF[3] += ((int32_t) sl*(int32_t) sp[-3]);
        L_ACF[4] += ((int32_t) sl*(int32_t) sp[-4]);
        L_ACF[5] += ((int32_t) sl*(int32_t) sp[-5]);
        L_ACF[6] += ((int32_t) sl*(int32_t) sp[-6]);
        L_ACF[7] += ((int32_t) sl*(int32_t) sp[-7]);
        L_ACF[8] += ((int32_t) sl*(int32_t) sp[-8]);
    }
    /*endfor*/
    for (k = 0;  k < 9;  k++)
        L_ACF[k] <<= 1;
    /*endfor*/
#endif
    /* Rescaling of the array s[0..159] */
    if (scalauto > 0)
    {
        assert(scalauto <= 4); 
        for (k = 0;  k < GSM0610_FRAME_LEN;  k++)
            amp[k] <<= scalauto;
        /*endfor*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

/* 4.2.5 */
static void reflection_coefficients(int32_t L_ACF[9], int16_t r[8])
{
    int i;
    int m;
    int n;
    int16_t temp;
    int16_t ACF[9];
    int16_t P[9];
    int16_t K[9];

    /* Schur recursion with 16 bits arithmetic. */
    if (L_ACF[0] == 0)
    {
        for (i = 8;  i--;  *r++ = 0)
            ;
        /*endfor*/
        return;
    }
    /*endif*/

    assert(L_ACF[0] != 0);
    temp = gsm0610_norm(L_ACF[0]);

    assert(temp >= 0  &&  temp < 32);

    /* ? overflow ? */
    for (i = 0;  i <= 8;  i++)
        ACF[i] = (int16_t) ((L_ACF[i] << temp) >> 16);
    /*endfor*/

    /* Initialize array P[..] and K[..] for the recursion. */
    for (i = 1;  i <= 7;  i++)
        K[i] = ACF[i];
    /*endfor*/
    for (i = 0;  i <= 8;  i++)
        P[i] = ACF[i];
    /*endfor*/
    /* Compute reflection coefficients */
    for (n = 1;  n <= 8;  n++, r++)
    {
        temp = P[1];
        temp = saturated_abs16(temp);
        if (P[0] < temp)
        {
            for (i = n;  i <= 8;  i++)
                *r++ = 0;
            /*endfor*/
            return;
        }
        /*endif*/

        *r = gsm_div(temp, P[0]);

        assert(*r >= 0);
        if (P[1] > 0)
            *r = -*r;     /* r[n] = sub(0, r[n]) */
        /*endif*/
        assert(*r != INT16_MIN);
        if (n == 8)
            return; 
        /*endif*/

        /* Schur recursion */
        temp = gsm_mult_r(P[1], *r);
        P[0] = saturated_add16(P[0], temp);

        for (m = 1;  m <= 8 - n;  m++)
        {
            temp = gsm_mult_r(K[m], *r);
            P[m] = saturated_add16(P[m + 1], temp);

            temp = gsm_mult_r(P[m + 1], *r);
            K[m] = saturated_add16(K[m], temp);
        }
        /*endfor*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

/* 4.2.6 */
static void transform_to_log_area_ratios(int16_t r[8])
{
    int16_t temp;
    int i;

    /* The following scaling for r[..] and LAR[..] has been used:
      
       r[..]   = integer (real_r[..]*32768.); -1 <= real_r < 1.
       LAR[..] = integer (real_LAR[..] * 16384);
       with -1.625 <= real_LAR <= 1.625
    */

    /* Computation of the LAR[0..7] from the r[0..7] */
    for (i = 1;  i <= 8;  i++, r++)
    {
        temp = saturated_abs16(*r);
        assert(temp >= 0);

        if (temp < 22118)
        {
            temp >>= 1;
        }
        else if (temp < 31130)
        {
            assert(temp >= 11059);
            temp -= 11059;
        }
        else
        {
            assert(temp >= 26112);
            temp -= 26112;
            temp <<= 2;
        }
        /*endif*/

        *r = (*r < 0)  ?  -temp  :  temp;
        assert(*r != INT16_MIN);
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

/* 4.2.7 */
static void quantization_and_coding(int16_t LAR[8])
{
    int16_t temp;

    /* This procedure needs four tables; the following equations
       give the optimum scaling for the constants:
        
       A[0..7] = integer(real_A[0..7] * 1024)
       B[0..7] = integer(real_B[0..7] *  512)
       MAC[0..7] = maximum of the LARc[0..7]
       MIC[0..7] = minimum of the LARc[0..7] */

#undef STEP
#define STEP(A,B,MAC,MIC)                                       \
        temp = saturated_mul16(A, *LAR);                        \
        temp = saturated_add16(temp, B);                        \
        temp = saturated_add16(temp, 256);                      \
        temp >>= 9;                                             \
        *LAR  = (int16_t) ((temp > MAC)                         \
                         ?                                      \
                         MAC - MIC                              \
                         :                                      \
                         ((temp < MIC)  ?  0  :  temp - MIC));  \
        LAR++;

    STEP(20480,     0,  31, -32);
    STEP(20480,     0,  31, -32);
    STEP(20480,  2048,  15, -16);
    STEP(20480, -2560,  15, -16);

    STEP(13964,    94,   7,  -8);
    STEP(15360, -1792,   7,  -8);
    STEP( 8534,  -341,   3,  -4);
    STEP( 9036, -1144,   3,  -4);
#undef STEP
}
/*- End of function --------------------------------------------------------*/

void gsm0610_lpc_analysis(gsm0610_state_t *s,
                          int16_t amp[GSM0610_FRAME_LEN],
                          int16_t LARc[8])
{
    int32_t L_ACF[9];

    autocorrelation(amp, L_ACF);
    reflection_coefficients(L_ACF, LARc);
    transform_to_log_area_ratios(LARc);
    quantization_and_coding(LARc);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
