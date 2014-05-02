/*
 * SpanDSP - a series of DSP components for telephony
 *
 * gsm0610_short_term.c - GSM 06.10 full rate speech codec.
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

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/bitstream.h"
#include "spandsp/saturated.h"
#include "spandsp/gsm0610.h"

#include "gsm0610_local.h"

/* SHORT TERM ANALYSIS FILTERING SECTION */

/* 4.2.8 */
static void decode_log_area_ratios(int16_t LARc[8], int16_t *LARpp)
{
    int16_t temp1;

    /* This procedure requires for efficient implementation
       two tables.
       INVA[1..8] = integer((32768*8)/real_A[1..8])
       MIC[1..8]  = minimum value of the LARc[1..8]
    */

    /* Compute the LARpp[1..8] */

#undef STEP
#define STEP(B,MIC,INVA)                            \
    temp1 = sat_add16(*LARc++, MIC) << 10;          \
    temp1 = sat_sub16(temp1, B << 1);               \
    temp1 = gsm_mult_r(INVA, temp1);                \
    *LARpp++ = sat_add16(temp1, temp1);

    STEP(    0,  -32,  13107);
    STEP(    0,  -32,  13107);
    STEP( 2048,  -16,  13107);
    STEP(-2560,  -16,  13107);

    STEP(   94,   -8,  19223);
    STEP(-1792,   -8,  17476);
    STEP( -341,   -4,  31454);
    STEP(-1144,   -4,  29708);

    /* NOTE: the addition of *MIC is used to restore the sign of *LARc. */
}
/*- End of function --------------------------------------------------------*/

/* 4.2.9 */

/* Computation of the quantized reflection coefficients */

/* 4.2.9.1  Interpolation of the LARpp[1..8] to get the LARp[1..8] */

/* Within each frame of 160 analyzed speech samples the short term
   analysis and synthesis filters operate with four different sets of
   coefficients, derived from the previous set of decoded LARs(LARpp(j - 1))
   and the actual set of decoded LARs (LARpp(j))

   (Initial value: LARpp(j - 1)[1..8] = 0.)
*/

static void coefficients_0_12(int16_t *LARpp_j_1,
                              int16_t *LARpp_j,
                              int16_t *LARp)
{
    int i;

    for (i = 1;  i <= 8;  i++, LARp++, LARpp_j_1++, LARpp_j++)
    {
        *LARp = sat_add16(*LARpp_j_1 >> 2, *LARpp_j >> 2);
        *LARp = sat_add16(*LARp, *LARpp_j_1 >> 1);
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void coefficients_13_26(int16_t *LARpp_j_1,
                               int16_t *LARpp_j,
                               int16_t *LARp)
{
    int i;

    for (i = 1;  i <= 8;  i++, LARpp_j_1++, LARpp_j++, LARp++)
        *LARp = sat_add16(*LARpp_j_1 >> 1, *LARpp_j >> 1);
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void coefficients_27_39(int16_t *LARpp_j_1,
                               int16_t *LARpp_j,
                               int16_t *LARp)
{
    int i;

    for (i = 1;  i <= 8;  i++, LARpp_j_1++, LARpp_j++, LARp++)
    {
        *LARp = sat_add16(*LARpp_j_1 >> 2, *LARpp_j >> 2);
        *LARp = sat_add16(*LARp, *LARpp_j >> 1);
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void coefficients_40_159(int16_t *LARpp_j, int16_t *LARp)
{
    int i;

    for (i = 1;  i <= 8;  i++)
        *LARp++ = *LARpp_j++;
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

/* 4.2.9.2 */
static void larp_to_rp(int16_t LARp[8])
{
    int i;
    int16_t *LARpx;
    int16_t temp;

    /* The input to this procedure is the interpolated LARp[0..7] array.
       The reflection coefficients, rp[i], are used in the analysis
       filter and in the synthesis filter.
    */

    LARpx = LARp;
    for (i = 1;  i <= 8;  i++, LARpx++)
    {
        temp = *LARpx;
        if (temp < 0)
        {
            if (temp == INT16_MIN)
                temp = INT16_MAX;
            else
                temp = -temp;
            /*endif*/
            if (temp < 11059)
                temp <<= 1;
            else if (temp < 20070)
                temp += 11059;
            else
                temp = sat_add16(temp >> 2, 26112);
            /*endif*/
            *LARpx = -temp;
        }
        else
        {
            if (temp < 11059)
                temp <<= 1;
            else if (temp < 20070)
                temp += 11059;
            else
                temp = sat_add16(temp >> 2, 26112);
            /*endif*/
            *LARpx = temp;
        }
        /*endif*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

/* 4.2.10 */
static void short_term_analysis_filtering(gsm0610_state_t *s,
                                          int16_t rp[8],
                                          int k_n,          // k_end - k_start
                                          int16_t amp[])    // [0..n-1]     IN/OUT
{
    /* This procedure computes the short term residual signal d[..] to be fed
       to the RPE-LTP loop from the s[..] signal and from the local rp[..]
       array (quantized reflection coefficients).  As the call of this
       procedure can be done in many ways (see the interpolation of the LAR
       coefficient), it is assumed that the computation begins with index
       k_start (for arrays d[..] and s[..]) and stops with index k_end
       (k_start and k_end are defined in 4.2.9.1).  This procedure also
       needs to keep the array u[0..7] in memory for each call.
    */
    int16_t *u0;
    int16_t *u_top;
    int i;
    int16_t *u;
    int16_t *rpx;
    int32_t di;
    int32_t u_out;

    u0 = s->u;
    u_top = u0 + 8;

    for (i = 0;  i < k_n;  i++)
    {
        di =
        u_out = amp[i];
        for (rpx = rp, u = u0;  u < u_top;  )
        {
            int32_t ui;
            int32_t rpi;

            ui = *u;
            *u++ = (int16_t) u_out;
            rpi = *rpx++;
            u_out = ui + (((rpi*di) + 0x4000) >> 15);
            di = di + (((rpi*ui) + 0x4000) >> 15);
            u_out = saturate16(u_out);
            di = saturate16(di);
        }
        /*endfor*/
        amp[i] = (int16_t) di;
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void short_term_synthesis_filtering(gsm0610_state_t *s,
                                           int16_t rrp[8],
                                           int k,          // k_end - k_start
                                           int16_t *wt,    // [0..k - 1]
                                           int16_t *sr)    // [0..k - 1]
{
    int16_t *v;
    int i;
    int16_t sri;
    int16_t tmp1;
    int16_t tmp2;

    v = s->v;
    while (k--)
    {
        sri = *wt++;
        for (i = 8;  i--;  )
        {
            tmp1 = rrp[i];
            tmp2 = v[i];
            tmp2 = ((tmp1 == INT16_MIN  &&  tmp2 == INT16_MIN)
                   ?
                   INT16_MAX
                   :
                   (int16_t) (((int32_t) tmp1*(int32_t) tmp2 + 16384) >> 15) & 0xFFFF);

            sri = sat_sub16(sri, tmp2);

            tmp1 = ((tmp1 == INT16_MIN  &&  sri == INT16_MIN)
                    ?
                    INT16_MAX
                    :
                    (int16_t) (((int32_t) tmp1*(int32_t) sri + 16384) >> 15) & 0xFFFF);

            v[i + 1] = sat_add16(v[i], tmp1);
        }
        /*endfor*/
        *sr++ =
        v[0] = sri;
    }
    /*endwhile*/
}
/*- End of function --------------------------------------------------------*/

void gsm0610_short_term_analysis_filter(gsm0610_state_t *s,
                                        int16_t LARc[8],
                                        int16_t amp[GSM0610_FRAME_LEN])
{
    int16_t *LARpp_j;
    int16_t *LARpp_j_1;
    int16_t LARp[8];

    LARpp_j = s->LARpp[s->j];
    LARpp_j_1 = s->LARpp[s->j ^= 1];

    decode_log_area_ratios(LARc, LARpp_j);

    coefficients_0_12(LARpp_j_1, LARpp_j, LARp);
    larp_to_rp(LARp);
    short_term_analysis_filtering(s, LARp, 13, amp);

    coefficients_13_26(LARpp_j_1, LARpp_j, LARp);
    larp_to_rp(LARp);
    short_term_analysis_filtering(s, LARp, 14, amp + 13);

    coefficients_27_39(LARpp_j_1, LARpp_j, LARp);
    larp_to_rp(LARp);
    short_term_analysis_filtering(s, LARp, 13, amp + 27);

    coefficients_40_159(LARpp_j, LARp);
    larp_to_rp(LARp);
    short_term_analysis_filtering(s, LARp, 120, amp + 40);
}
/*- End of function --------------------------------------------------------*/

void gsm0610_short_term_synthesis_filter(gsm0610_state_t *s,
                                         int16_t LARcr[8],
                                         int16_t wt[GSM0610_FRAME_LEN],
                                         int16_t amp[GSM0610_FRAME_LEN])
{
    int16_t *LARpp_j;
    int16_t *LARpp_j_1;
    int16_t LARp[8];

    LARpp_j = s->LARpp[s->j];
    LARpp_j_1 = s->LARpp[s->j ^= 1];

    decode_log_area_ratios(LARcr, LARpp_j);

    coefficients_0_12(LARpp_j_1, LARpp_j, LARp);
    larp_to_rp(LARp);
    short_term_synthesis_filtering(s, LARp, 13, wt, amp);

    coefficients_13_26(LARpp_j_1, LARpp_j, LARp);
    larp_to_rp(LARp);
    short_term_synthesis_filtering(s, LARp, 14, wt + 13, amp + 13);

    coefficients_27_39(LARpp_j_1, LARpp_j, LARp);
    larp_to_rp(LARp);
    short_term_synthesis_filtering(s, LARp, 13, wt + 27, amp + 27);

    coefficients_40_159(LARpp_j, LARp);
    larp_to_rp(LARp);
    short_term_synthesis_filtering(s, LARp, 120, wt + 40, amp + 40);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
