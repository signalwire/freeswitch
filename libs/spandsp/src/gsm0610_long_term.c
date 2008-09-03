/*
 * SpanDSP - a series of DSP components for telephony
 *
 * gsm0610_long_term.c - GSM 06.10 full rate speech codec.
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
 *
 * $Id: gsm0610_long_term.c,v 1.16 2008/07/02 14:48:25 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <assert.h>
#include <inttypes.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <stdlib.h>

#include "spandsp/telephony.h"
#include "spandsp/bitstream.h"
#include "spandsp/dc_restore.h"
#include "spandsp/gsm0610.h"

#include "gsm0610_local.h"

/* Table 4.3a  Decision level of the LTP gain quantizer */
static const int16_t gsm_DLB[4] =
{
    6554,    16384,    26214,     32767
};

/* Table 4.3b   Quantization levels of the LTP gain quantizer */
static const int16_t gsm_QLB[4] =
{
    3277,    11469,    21299,     32767
};

/* 4.2.11 .. 4.2.12 LONG TERM PREDICTOR (LTP) SECTION */

#if defined(__GNUC__)  &&  defined(__i386__)
int32_t gsm0610_max_cross_corr(const int16_t *wt, const int16_t *dp, int16_t *Nc_out)
{
    int32_t lmax;
    int32_t out;

    __asm__ __volatile__(
        " emms;\n"
        " pushl %%ebx;\n"
        " movl $0,%%edx;\n"             /* Will be maximum inner-product */
        " movl $40,%%ebx;\n"
        " movl %%ebx,%%ecx;\n"          /* Will be index of max inner-product */
        " subl $80,%%esi;\n"
        " .p2align 2;\n"
        "1:\n"
        " movq (%%edi),%%mm0;\n"
        " movq (%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm0;\n"
        " movq 8(%%edi),%%mm1;\n"
        " movq 8(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 16(%%edi),%%mm1;\n"
        " movq 16(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 24(%%edi),%%mm1;\n"
        " movq 24(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 32(%%edi),%%mm1;\n"
        " movq 32(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 40(%%edi),%%mm1;\n"
        " movq 40(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 48(%%edi),%%mm1;\n"
        " movq 48(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 56(%%edi),%%mm1;\n"
        " movq 56(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 64(%%edi),%%mm1;\n"
        " movq 64(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq 72(%%edi),%%mm1;\n"
        " movq 72(%%esi),%%mm2;\n"
        " pmaddwd %%mm2,%%mm1;\n"
        " paddd %%mm1,%%mm0;\n"
        " movq %%mm0,%%mm1;\n"
        " punpckhdq %%mm0,%%mm1;\n"        /* mm1 has high int32 of mm0 dup'd */
        " paddd %%mm1,%%mm0;\n"
        " movd %%mm0,%%eax;\n"                /* eax has result */
        " cmpl %%edx,%%eax;\n"
        " jle 2f;\n"
        " movl %%eax,%%edx;\n"
        " movl %%ebx,%%ecx;\n"
        " .p2align 2;\n"
        "2:\n"
        " subl $2,%%esi;\n"
        " incl %%ebx;\n"
        " cmpl $120,%%ebx;\n"
        " jle 1b;\n"
        " popl %%ebx;\n"
        " emms;\n"
        : "=d" (lmax), "=c" (out)
        : "D" (wt), "S" (dp)
        : "eax"
    );
    *Nc_out = out;
    return  lmax;
}
/*- End of function --------------------------------------------------------*/
#endif

/* This procedure computes the LTP gain (bc) and the LTP lag (Nc)
   for the long term analysis filter.   This is done by calculating a
   maximum of the cross-correlation function between the current
   sub-segment short term residual signal d[0..39] (output of
   the short term analysis filter; for simplification the index
   of this array begins at 0 and ends at 39 for each sub-segment of the
   RPE-LTP analysis) and the previous reconstructed short term
   residual signal dp[ -120 .. -1 ].  A dynamic scaling must be
   performed to avoid overflow. */

/* This procedure exists in three versions.  First, the integer
   version; then, the two floating point versions (as another
   function), with or without scaling. */

static int16_t evaluate_ltp_parameters(int16_t d[40],
                                       int16_t *dp,   // [-120..-1]   IN
                                       int16_t *Nc_out)
{
    int k;
    int16_t Nc;
    int16_t bc;
    int16_t wt[40];
    int32_t L_max;
    int32_t L_power;
    int16_t R;
    int16_t S;
    int16_t dmax;
    int16_t scale;
    int16_t temp;
    int32_t L_temp;
#if !(defined(__GNUC__)  &&  defined(__i386__))
    int16_t lambda;
#endif

    /* Search of the optimum scaling of d[0..39]. */
    dmax = 0;
    for (k = 0;  k < 40;  k++)
    {
        temp = d[k];
        temp = gsm_abs(temp);
        if (temp > dmax)
            dmax = temp;
        /*endif*/
    }
    /*endfor*/

    if (dmax == 0)
    {
        temp = 0;
    }
    else
    {
        assert(dmax > 0);
        temp = gsm0610_norm((int32_t) dmax << 16);
    }
    /*endif*/

    if (temp > 6)
        scale = 0;
    else
        scale = (int16_t) (6 - temp);
    /*endif*/
    assert(scale >= 0);

    /* Initialization of a working array wt */
    for (k = 0;  k < 40;  k++)
        wt[k] = d[k] >> scale;
    /*endfor*/

    /* Search for the maximum cross-correlation and coding of the LTP lag */
#if defined(__GNUC__)  &&  defined(__i386__)
    L_max = gsm0610_max_cross_corr(wt, dp, &Nc);
#else
    L_max = 0;
    Nc = 40; /* index for the maximum cross-correlation */

    for (lambda = 40;  lambda <= 120;  lambda++)
    {
        int32_t L_result;

        L_result  = (wt[0]*dp[0 - lambda])
                  + (wt[1]*dp[1 - lambda])
                  + (wt[2]*dp[2 - lambda])
                  + (wt[3]*dp[3 - lambda])
                  + (wt[4]*dp[4 - lambda])
                  + (wt[5]*dp[5 - lambda])
                  + (wt[6]*dp[6 - lambda])
                  + (wt[7]*dp[7 - lambda])
                  + (wt[8]*dp[8 - lambda])
                  + (wt[9]*dp[9 - lambda])
                  + (wt[10]*dp[10 - lambda])
                  + (wt[11]*dp[11 - lambda])
                  + (wt[12]*dp[12 - lambda])
                  + (wt[13]*dp[13 - lambda])
                  + (wt[14]*dp[14 - lambda])
                  + (wt[15]*dp[15 - lambda])
                  + (wt[16]*dp[16 - lambda])
                  + (wt[17]*dp[17 - lambda])
                  + (wt[18]*dp[18 - lambda])
                  + (wt[19]*dp[19 - lambda])
                  + (wt[20]*dp[20 - lambda])
                  + (wt[21]*dp[21 - lambda])
                  + (wt[22]*dp[22 - lambda])
                  + (wt[23]*dp[23 - lambda])
                  + (wt[24]*dp[24 - lambda])
                  + (wt[25]*dp[25 - lambda])
                  + (wt[26]*dp[26 - lambda])
                  + (wt[27]*dp[27 - lambda])
                  + (wt[28]*dp[28 - lambda])
                  + (wt[29]*dp[29 - lambda])
                  + (wt[30]*dp[30 - lambda])
                  + (wt[31]*dp[31 - lambda])
                  + (wt[32]*dp[32 - lambda])
                  + (wt[33]*dp[33 - lambda])
                  + (wt[34]*dp[34 - lambda])
                  + (wt[35]*dp[35 - lambda])
                  + (wt[36]*dp[36 - lambda])
                  + (wt[37]*dp[37 - lambda])
                  + (wt[38]*dp[38 - lambda])
                  + (wt[39]*dp[39 - lambda]);

        if (L_result > L_max)
        {
            Nc = lambda;
            L_max = L_result;
        }
        /*endif*/
    }
    /*endfor*/
#endif
    *Nc_out = Nc;

    L_max <<= 1;

    /* Rescaling of L_max */
    assert(scale <= 100  &&  scale >=  -100);
    L_max = L_max >> (6 - scale);

    assert(Nc <= 120  &&  Nc >= 40);

    /* Compute the power of the reconstructed short term residual signal dp[..] */
    L_power = 0;
    for (k = 0;  k < 40;  k++)
    {
        L_temp = dp[k - Nc] >> 3;
        L_power += L_temp*L_temp;
    }
    /*endfor*/
    L_power <<= 1;  /* from L_MULT */

    /* Normalization of L_max and L_power */
    if (L_max <= 0)
        return  0;
    /*endif*/
    if (L_max >= L_power)
        return  3;
    /*endif*/
    temp = gsm0610_norm(L_power);

    R = (int16_t) ((L_max << temp) >> 16);
    S = (int16_t) ((L_power << temp) >> 16);

    /* Coding of the LTP gain */

    /* Table 4.3a must be used to obtain the level DLB[i] for the
       quantization of the LTP gain b to get the coded version bc. */
    for (bc = 0;  bc <= 2;  bc++)
    {
        if (R <= gsm_mult(S, gsm_DLB[bc]))
            break;
        /*endif*/
    }
    /*endfor*/
    return  bc;
}
/*- End of function --------------------------------------------------------*/

/* 4.2.12 */
static void long_term_analysis_filtering(int16_t bc,
                                         int16_t Nc,
                                         int16_t *dp,    // previous d   [-120..-1]      IN
                                         int16_t d[40],
                                         int16_t dpp[40],
                                         int16_t e[40])
{
    int k;

    /* In this part, we have to decode the bc parameter to compute
       the samples of the estimate dpp[0..39].  The decoding of bc needs the
       use of table 4.3b.  The long term residual signal e[0..39]
       is then calculated to be fed to the RPE encoding section. */
    for (k = 0;  k < 40;  k++)
    {
        dpp[k] = gsm_mult_r(gsm_QLB[bc], dp[k - Nc]);
        e[k] = gsm_sub(d[k], dpp[k]);
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

/* 4x for 160 samples */
void gsm0610_long_term_predictor(gsm0610_state_t *s,
                                 int16_t d[40],
                                 int16_t *dp,        // [-120..-1] d'    IN
                                 int16_t e[40],
                                 int16_t dpp[40],
                                 int16_t *Nc,
                                 int16_t *bc)
{
#if 0
    assert(d);
    assert(dp);
    assert(e);
    assert(dpp);
    assert(Nc);
    assert(bc);
#endif

    *bc = evaluate_ltp_parameters(d, dp, Nc);
    long_term_analysis_filtering(*bc, *Nc, dp, d, dpp, e);
}
/*- End of function --------------------------------------------------------*/

/* 4.3.2 */
void gsm0610_long_term_synthesis_filtering(gsm0610_state_t *s,
                                           int16_t Ncr,
                                           int16_t bcr,
                                           int16_t erp[40],
                                           int16_t *drp)    // [-120..-1] IN, [0..40] OUT
{
    int k;
    int16_t brp;
    int16_t drpp;
    int16_t Nr;

    /* This procedure uses the bcr and Ncr parameter to realize the
       long term synthesis filter.  The decoding of bcr needs
       table 4.3b. */

    /* Check the limits of Nr. */
    Nr = (Ncr < 40  ||  Ncr > 120)  ?  s->nrp  :  Ncr;
    s->nrp = Nr;
    assert (Nr >= 40  &&  Nr <= 120);

    /* Decode the LTP gain, bcr */
    brp = gsm_QLB[bcr];

    /* Compute the reconstructed short term residual signal, drp[0..39] */
    assert(brp != INT16_MIN);
    for (k = 0;  k < 40;  k++)
    {
        drpp = gsm_mult_r(brp, drp[k - Nr]);
        drp[k] = gsm_add(erp[k], drpp);
    }
    /*endfor*/

    /* Update the reconstructed short term residual signal, drp[-1..-120] */
    for (k = 0;  k < 120;  k++)
        drp[k - 120] = drp[k - 80];
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
