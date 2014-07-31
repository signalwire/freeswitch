/*
 * SpanDSP - a series of DSP components for telephony
 *
 * gsm0610_rpe.c - GSM 06.10 full rate speech codec.
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

#include "mmx_sse_decs.h"

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/bitstream.h"
#include "spandsp/saturated.h"
#include "spandsp/gsm0610.h"

#include "gsm0610_local.h"

/* 4.2.13 .. 4.2.17  RPE ENCODING SECTION */

/* 4.2.13 */
static void weighting_filter(int16_t x[40],
                             const int16_t *e)      // signal [-5..0.39.44] IN)
{
#if defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)  &&  defined(__x86_64__) && !defined(__OpenBSD__)
    /* Table 4.4   Coefficients of the weighting filter */
    /* This must be padded to a multiple of 4 for MMX to work */
    static const union
    {
        int16_t gsm_H[12];
        __m64 x[3];
    } gsm_H =
    {
        {
            -134, -374, 0, 2054, 5741, 8192, 5741, 2054, 0, -374, -134, 0
        }

    };

    __asm__ __volatile__(
        " emms;\n"
        " addq $-10,%%rcx;\n"
        " leaq %[gsm_H],%%rax;\n"
        " movq (%%rax),%%mm1;\n"
        " movq 8(%%rax),%%mm2;\n"
        " movq 16(%%rax),%%mm3;\n"
        " movq $0x1000,%%rax;\n"
        " movq %%rax,%%mm5;\n"              /* For rounding */
        " xorq %%rsi,%%rsi;\n"
        " .p2align 2;\n"
        "1:\n"
        " movq (%%rcx,%%rsi,2),%%mm0;\n"
        " pmaddwd %%mm1,%%mm0;\n"

        " movq 8(%%rcx,%%rsi,2),%%mm4;\n"
        " pmaddwd %%mm2,%%mm4;\n"
        " paddd %%mm4,%%mm0;\n"

        " movq 16(%%rcx,%%rsi,2),%%mm4;\n"
        " pmaddwd %%mm3,%%mm4;\n"
        " paddd %%mm4,%%mm0;\n"

        " movq %%mm0,%%mm4;\n"
        " punpckhdq %%mm0,%%mm4;\n"         /* mm4 has high int32 of mm0 dup'd */
        " paddd %%mm4,%%mm0;\n"

        " paddd %%mm5,%%mm0;\n"             /* Add for roundoff */
        " psrad $13,%%mm0;\n"
        " packssdw %%mm0,%%mm0;\n"
        " movd %%mm0,%%eax;\n"              /* eax has result */
        " movw %%ax,(%%rdi,%%rsi,2);\n"
        " incq %%rsi;\n"
        " cmpq $39,%%rsi;\n"
        " jle 1b;\n"
        " emms;\n"
        :
        : "c" (e), "D" (x), [gsm_H] "X" (gsm_H)
        : "rax", "rdx", "rsi", "memory"
    );
#elif defined(__GNUC__)  &&  defined(SPANDSP_USE_MMX)  &&  defined(__i386__)
    /* Table 4.4   Coefficients of the weighting filter */
    /* This must be padded to a multiple of 4 for MMX to work */
    static const union
    {
        int16_t gsm_H[12];
        __m64 x[3];
    } gsm_H =
    {
        {
            -134, -374, 0, 2054, 5741, 8192, 5741, 2054, 0, -374, -134, 0
        }

    };

    __asm__ __volatile__(
        " emms;\n"
        " addl $-10,%%ecx;\n"
        " leal %[gsm_H],%%eax;\n"
        " movq (%%eax),%%mm1;\n"
        " movq 8(%%eax),%%mm2;\n"
        " movq 16(%%eax),%%mm3;\n"
        " movl $0x1000,%%eax;\n"
        " movd %%eax,%%mm5;\n"              /* For rounding */
        " xorl %%esi,%%esi;\n"
        " .p2align 2;\n"
        "1:\n"
        " movq (%%ecx,%%esi,2),%%mm0;\n"
        " pmaddwd %%mm1,%%mm0;\n"

        " movq 8(%%ecx,%%esi,2),%%mm4;\n"
        " pmaddwd %%mm2,%%mm4;\n"
        " paddd %%mm4,%%mm0;\n"

        " movq 16(%%ecx,%%esi,2),%%mm4;\n"
        " pmaddwd %%mm3,%%mm4;\n"
        " paddd %%mm4,%%mm0;\n"

        " movq %%mm0,%%mm4;\n"
        " punpckhdq %%mm0,%%mm4;\n"         /* mm4 has high int32 of mm0 dup'd */
        " paddd %%mm4,%%mm0;\n"

        " paddd %%mm5,%%mm0;\n"             /* Add for roundoff */
        " psrad $13,%%mm0;\n"
        " packssdw %%mm0,%%mm0;\n"
        " movd %%mm0,%%eax;\n"              /* eax has result */
        " movw %%ax,(%%edi,%%esi,2);\n"
        " incl %%esi;\n"
        " cmpl $39,%%esi;\n"
        " jle 1b;\n"
        " emms;\n"
        :
        : "c" (e), "D" (x), [gsm_H] "X" (gsm_H)
        : "eax", "edx", "esi", "memory"
    );
#else
    int32_t result;
    int k;

    /* The coefficients of the weighting filter are stored in a table
       (see table 4.4).  The following scaling is used:

        H[0..10] = integer(real_H[0..10] * 8192);
    */
    /* Initialization of a temporary working array wt[0...49] */

    /* for (k =  0;  k <=  4;  k++) wt[k] = 0;
     * for (k =  5;  k <= 44;  k++) wt[k] = *e++;
     * for (k = 45;  k <= 49;  k++) wt[k] = 0;
     *
     *  (e[-5..-1] and e[40..44] are allocated by the caller,
     *  are initially zero and are not written anywhere.)
     */
    e -= 5;

    /* Compute the signal x[0..39] */
    for (k = 0;  k < 40;  k++)
    {
        result = 8192 >> 1;

        /* for (i = 0; i <= 10; i++)
         * {
         *      temp   = sat_mul16_32(wt[k + i], gsm_H[i]);
         *      result = sat_add32(result, temp);
         * }
         */

#undef STEP
#define STEP(i,H) (e[k + i] * (int32_t) H)

        /* Every one of these multiplications is done twice,
           but I don't see an elegant way to optimize this.
           Do you?
        */
        result += STEP( 0,  -134);
        result += STEP( 1,  -374);
            /* += STEP( 2,  0   ); */
        result += STEP( 3,  2054);
        result += STEP( 4,  5741);
        result += STEP( 5,  8192);
        result += STEP( 6,  5741);
        result += STEP( 7,  2054);
            /* += STEP( 8,  0   ); */
        result += STEP( 9,  -374);
        result += STEP(10,  -134);

        /* 2 adds vs. >> 16 => 14, minus one shift to compensate for
           those we lost when replacing L_MULT by '*'. */
        result >>= 13;
        x[k] = saturate16(result);
    }
    /*endfor*/
#endif
}
/*- End of function --------------------------------------------------------*/

/* 4.2.14 */
static void rpe_grid_selection(int16_t x[40], int16_t xM[13], int16_t *Mc_out)
{
    int i;
    int32_t L_result;
    int32_t L_temp;
    int32_t EM;     /* xxx should be L_EM? */
    int16_t Mc;
    int32_t L_common_0_3;

    /* The signal x[0..39] is used to select the RPE grid which is
       represented by Mc. */
    EM = 0;
    Mc = 0;

#undef STEP
#define STEP(m,i)                           \
    L_temp = x[m + 3*i] >> 2;               \
    L_result += L_temp*L_temp;

    /* Common part of 0 and 3 */
    L_result = 0;
    STEP(0, 1);
    STEP(0, 2);
    STEP(0, 3);
    STEP(0, 4);
    STEP(0, 5);
    STEP(0, 6);
    STEP(0, 7);
    STEP(0, 8);
    STEP(0, 9);
    STEP(0, 10);
    STEP(0, 11);
    STEP(0, 12);
    L_common_0_3 = L_result;

    /* i = 0 */

    STEP(0, 0);
    L_result <<= 1; /* implicit in L_MULT */
    EM = L_result;

    /* i = 1 */

    L_result = 0;
    STEP(1, 0);
    STEP(1, 1);
    STEP(1, 2);
    STEP(1, 3);
    STEP(1, 4);
    STEP(1, 5);
    STEP(1, 6);
    STEP(1, 7);
    STEP(1, 8);
    STEP(1, 9);
    STEP(1, 10);
    STEP(1, 11);
    STEP(1, 12);
    L_result <<= 1;
    if (L_result > EM)
    {
        Mc = 1;
        EM = L_result;
    }
    /*endif*/

    /* i = 2 */

    L_result = 0;
    STEP(2, 0);
    STEP(2, 1);
    STEP(2, 2);
    STEP(2, 3);
    STEP(2, 4);
    STEP(2, 5);
    STEP(2, 6);
    STEP(2, 7);
    STEP(2, 8);
    STEP(2, 9);
    STEP(2, 10);
    STEP(2, 11);
    STEP(2, 12);
    L_result <<= 1;
    if (L_result > EM)
    {
        Mc = 2;
        EM = L_result;
    }
    /*endif*/

    /* i = 3 */

    L_result = L_common_0_3;
    STEP(3, 12);
    L_result <<= 1;
    if (L_result > EM)
        Mc = 3;
    /*endif*/

    /* Down-sampling by a factor 3 to get the selected xM[0..12]
       RPE sequence. */
    for (i = 0;  i < 13;  i++)
        xM[i] = x[Mc + 3*i];
    /*endfor*/
    *Mc_out = Mc;
}
/*- End of function --------------------------------------------------------*/

/* 4.12.15 */
static void apcm_quantization_xmaxc_to_exp_mant(int16_t xmaxc,
                                                int16_t *exp_out,
                                                int16_t *mant_out)
{
    int16_t exp;
    int16_t mant;

    /* Compute exponent and mantissa of the decoded version of xmaxc */
    exp = 0;
    if (xmaxc > 15)
        exp = (int16_t) ((xmaxc >> 3) - 1);
    /*endif*/
    mant = xmaxc - (exp << 3);

    if (mant == 0)
    {
        exp = -4;
        mant = 7;
    }
    else
    {
        while (mant <= 7)
        {
            mant = (int16_t) (mant << 1 | 1);
            exp--;
        }
        /*endwhile*/
        mant -= 8;
    }
    /*endif*/

    assert(exp >= -4  &&  exp <= 6);
    assert(mant >= 0  &&  mant <= 7);

    *exp_out  = exp;
    *mant_out = mant;
}
/*- End of function --------------------------------------------------------*/

static void apcm_quantization(int16_t xM[13],
                              int16_t xMc[13],
                              int16_t *mant_out,
                              int16_t *exp_out,
                              int16_t *xmaxc_out)
{
    /* Table 4.5   Normalized inverse mantissa used to compute xM/xmax */
    static const int16_t gsm_NRFAC[8] =
    {
        29128, 26215, 23832, 21846, 20165, 18725, 17476, 16384
    };
    int i;
    int itest;
    int16_t xmax;
    int16_t xmaxc;
    int16_t temp;
    int16_t temp1;
    int16_t temp2;
    int16_t exp;
    int16_t mant;

    /* Find the maximum absolute value xmax of xM[0..12]. */
    xmax = 0;
    for (i = 0;  i < 13;  i++)
    {
        temp = xM[i];
        temp = sat_abs16(temp);
        if (temp > xmax)
            xmax = temp;
        /*endif*/
    }
    /*endfor*/

    /* Quantizing and coding of xmax to get xmaxc. */
    exp = 0;
    temp = xmax >> 9;
    itest = 0;

    for (i = 0;  i <= 5;  i++)
    {
        itest |= (temp <= 0);
        temp >>= 1;

        assert(exp <= 5);
        if (itest == 0)
            exp++;
        /*endif*/
    }
    /*endfor*/

    assert(exp <= 6  &&  exp >= 0);
    temp = (int16_t) (exp + 5);

    assert(temp <= 11  &&  temp >= 0);
    xmaxc = sat_add16((xmax >> temp), exp << 3);

    /* Quantizing and coding of the xM[0..12] RPE sequence
       to get the xMc[0..12] */
    apcm_quantization_xmaxc_to_exp_mant(xmaxc, &exp, &mant);

    /* This computation uses the fact that the decoded version of xmaxc
       can be calculated by using the exponent and the mantissa part of
       xmaxc (logarithmic table).
       So, this method avoids any division and uses only a scaling
       of the RPE samples by a function of the exponent.  A direct
       multiplication by the inverse of the mantissa (NRFAC[0..7]
       found in table 4.5) gives the 3 bit coded version xMc[0..12]
       of the RPE samples.
    */
    /* Direct computation of xMc[0..12] using table 4.5 */
    assert(exp <= 4096  &&  exp >= -4096);
    assert(mant >= 0  &&  mant <= 7);

    temp1 = (int16_t) (6 - exp);    /* Normalization by the exponent */
    temp2 = gsm_NRFAC[mant];        /* Inverse mantissa */

    for (i = 0;  i < 13;  i++)
    {
        assert(temp1 >= 0  &&  temp1 < 16);

        temp = xM[i] << temp1;
        temp = sat_mul16(temp, temp2);
        temp >>= 12;
        xMc[i] = (int16_t) (temp + 4);      /* See note below */
    }
    /*endfor*/

    /* NOTE: This equation is used to make all the xMc[i] positive. */
    *mant_out  = mant;
    *exp_out   = exp;
    *xmaxc_out = xmaxc;
}
/*- End of function --------------------------------------------------------*/

/* 4.2.16 */
static void apcm_inverse_quantization(int16_t xMc[13],
                                      int16_t mant,
                                      int16_t exp,
                                      int16_t xMp[13])
{
    /* Table 4.6   Normalized direct mantissa used to compute xM/xmax */
    static const int16_t gsm_fac[8] =
    {
        18431, 20479, 22527, 24575, 26623, 28671, 30719, 32767
    };
    int i;
    int16_t temp;
    int16_t temp1;
    int16_t temp2;
    int16_t temp3;

    /* This part is for decoding the RPE sequence of coded xMc[0..12]
       samples to obtain the xMp[0..12] array.  Table 4.6 is used to get
       the mantissa of xmaxc (FAC[0..7]).
    */
#if 0
    assert(mant >= 0  &&  mant <= 7);
#endif

    temp1 = gsm_fac[mant];                      /* See 4.2-15 for mant */
    temp2 = sat_sub16(6, exp);                  /* See 4.2-15 for exp */
    temp3 = gsm_asl(1, sat_sub16(temp2, 1));

    for (i = 0;  i < 13;  i++)
    {
        assert(xMc[i] >= 0  &&  xMc[i] <= 7);   /* 3 bit unsigned */

        temp = (int16_t) ((xMc[i] << 1) - 7);   /* Restore sign */
        assert(temp <= 7  &&  temp >= -7);      /* 4 bit signed */

        temp <<= 12;                            /* 16 bit signed */
        temp = gsm_mult_r(temp1, temp);
        temp = sat_add16(temp, temp3);
        xMp[i] = gsm_asr(temp, temp2);
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

/* 4.2.17 */
static void rpe_grid_positioning(int16_t Mc,
                                 int16_t xMp[13],
                                 int16_t ep[40])
{
    int i = 13;

    /* This procedure computes the reconstructed long term residual signal
       ep[0..39] for the LTP analysis filter.  The inputs are the Mc
       which is the grid position selection and the xMp[0..12] decoded
       RPE samples which are upsampled by a factor of 3 by inserting zero
       values.
    */
    assert(0 <= Mc  &&  Mc <= 3);

    switch (Mc)
    {
    case 3:
        *ep++ = 0;
    case 2:
        do
        {
            *ep++ = 0;
    case 1:
            *ep++ = 0;
    case 0:
            *ep++ = *xMp++;
        }
        while (--i);
    }
    /*endswitch*/
    while (++Mc < 4)
        *ep++ = 0;
    /*endwhile*/
}
/*- End of function --------------------------------------------------------*/

void gsm0610_rpe_encoding(gsm0610_state_t *s,
                          int16_t *e,          // [-5..-1][0..39][40..44]
                          int16_t *xmaxc,
                          int16_t *Mc,
                          int16_t xMc[13])
{
    int16_t x[40];
    int16_t xM[13];
    int16_t xMp[13];
    int16_t mant;
    int16_t exp;

    weighting_filter(x, e);
    rpe_grid_selection(x, xM, Mc);

    apcm_quantization(xM, xMc, &mant, &exp, xmaxc);
    apcm_inverse_quantization(xMc, mant, exp, xMp);

    rpe_grid_positioning(*Mc, xMp, e);
}
/*- End of function --------------------------------------------------------*/

void gsm0610_rpe_decoding(gsm0610_state_t *s,
                          int16_t xmaxc,
                          int16_t Mcr,
                          int16_t xMcr[13],
                          int16_t erp[40])
{
    int16_t exp;
    int16_t mant;
    int16_t xMp[13];

    apcm_quantization_xmaxc_to_exp_mant(xmaxc, &exp, &mant);
    apcm_inverse_quantization(xMcr, mant, exp, xMp);
    rpe_grid_positioning(Mcr, xMp, erp);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
