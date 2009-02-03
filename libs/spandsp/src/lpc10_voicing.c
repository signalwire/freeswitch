/*
 * SpanDSP - a series of DSP components for telephony
 *
 * lpc10_voicing.c - LPC10 low bit rate speech codec.
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
 * This code is based on the U.S. Department of Defense reference
 * implementation of the LPC-10 2400 bps Voice Coder. They do not
 * exert copyright claims on their code, and it may be freely used.
 *
 * $Id: lpc10_voicing.c,v 1.18 2009/02/03 16:28:39 steveu Exp $
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <memory.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/dc_restore.h"
#include "spandsp/lpc10.h"
#include "spandsp/private/lpc10.h"

#include "lpc10_encdecs.h"

static void vparms(int32_t vwin[],
                   float *inbuf,
                   float *lpbuf,
                   const int32_t buflim[],
                   int32_t half,
                   float *dither,
                   int32_t *mintau,
                   int32_t *zc, 
                   int32_t *lbe,
                   int32_t *fbe,
                   float *qs,
                   float *rc1,
                   float *ar_b,
                   float *ar_f)
{
    int32_t inbuf_offset;
    int32_t lpbuf_offset;
    int32_t vlen;
    int32_t stop;
    int32_t i;
    int32_t start;
    float r1;
    float r2;
    float e_pre;
    float ap_rms;
    float e_0;
    float oldsgn;
    float lp_rms;
    float e_b;
    float e_f;
    float r_b;
    float r_f;
    float e0ap;

    /* Calculate zero crossings (ZC) and several energy and correlation */
    /* measures on low band and full band speech.  Each measure is taken */
    /* over either the first or the second half of the voicing window, */
    /* depending on the variable HALF. */
    lpbuf_offset = buflim[2];
    lpbuf -= lpbuf_offset;
    inbuf_offset = buflim[0];
    inbuf -= inbuf_offset;

    lp_rms = 0.0f;
    ap_rms = 0.0f;
    e_pre = 0.0f;
    e0ap = 0.0f;
    *rc1 = 0.0f;
    e_0 = 0.0f;
    e_b = 0.0f;
    e_f = 0.0f;
    r_f = 0.0f;
    r_b = 0.0f;
    *zc = 0;
    vlen = vwin[1] - vwin[0] + 1;
    start = vwin[0] + half*vlen/2 + 1;
    stop = start + vlen/2 - 1;

    /* I'll use the symbol HVL in the table below to represent the value */
    /* VLEN/2.  Note that if VLEN is odd, then HVL should be rounded down, */
    /* i.e., HVL = (VLEN-1)/2. */

    /* HALF  START          STOP */

    /* 1     VWIN(1)+1      VWIN(1)+HVL */
    /* 2     VWIN(1)+HVL+1  VWIN(1)+2*HVL */
    oldsgn = r_sign(1.0f, inbuf[start - 1] - *dither);
    for (i = start;  i <= stop;  i++)
    {
        lp_rms += fabsf(lpbuf[i]);
        ap_rms += fabsf(inbuf[i]);
        e_pre += fabsf(inbuf[i] - inbuf[i - 1]);
        r1 = inbuf[i];
        e0ap += r1*r1;
        *rc1 += inbuf[i]*inbuf[i - 1];
        r1 = lpbuf[i];
        e_0 += r1*r1;
        r1 = lpbuf[i - *mintau];
        e_b += r1*r1;
        r1 = lpbuf[i + *mintau];
        e_f += r1*r1;
        r_f += lpbuf[i]*lpbuf[i + *mintau];
        r_b += lpbuf[i]*lpbuf[i - *mintau];
        r1 = inbuf[i] + *dither;
        if (r_sign(1.0f, r1) != oldsgn)
        {
            ++(*zc);
            oldsgn = -oldsgn;
        }
        *dither = -(*dither);
    }
    /* Normalized short-term autocovariance coefficient at unit sample delay */
    *rc1 /= max(e0ap, 1.0f);
    /* Ratio of the energy of the first difference signal (6 dB/oct preemphasis)*/
    /* to the energy of the full band signal */
    /* Computing MAX */
    r1 = ap_rms*2.0f;
    *qs = e_pre/max(r1, 1.0f);
    /* aR_b is the product of the forward and reverse prediction gains, */
    /* looking backward in time (the causal case). */
    *ar_b = r_b/max(e_b, 1.0f)*(r_b/max(e_0, 1.0f));
    /* aR_f is the same as aR_b, but looking forward in time (non causal case).*/
    *ar_f = r_f/max(e_f, 1.0f)*(r_f/max(e_0, 1.0f));
    /* Normalize ZC, LBE, and FBE to old fixed window length of 180. */
    /* (The fraction 90/VLEN has a range of 0.58 to 1) */
    r2 = (float) (*zc << 1);
    *zc = lfastrintf(r2*(90.0f/vlen));
    r1 = lp_rms/4*(90.0f/vlen);
    *lbe = min(lfastrintf(r1), 32767);
    r1 = ap_rms/4*(90.0f/vlen);
    *fbe = min(lfastrintf(r1), 32767);
}
/*- End of function --------------------------------------------------------*/

/* Voicing detection makes voicing decisions for each half */
/* frame of input speech.  Tentative voicing decisions are made two frames*/
/* in the future (2F) for each half frame.  These decisions are carried */
/* through one frame in the future (1F) to the present (P) frame where */
/* they are examined and smoothed, resulting in the final voicing */
/* decisions for each half frame. */

/* The voicing parameter (signal measurement) column vector (VALUE) */
/* is based on a rectangular window of speech samples determined by the */
/* window placement algorithm.  The voicing parameter vector contains the*/
/* AMDF windowed maximum-to-minimum ratio, the zero crossing rate, energy*/
/* measures, reflection coefficients, and prediction gains.  The voicing */
/* window is placed to avoid contamination of the voicing parameter vector*/
/* with speech onsets. */

/* The input signal is then classified as unvoiced (including */
/* silence) or voiced.  This decision is made by a linear discriminant */
/* function consisting of a dot product of the voicing decision */
/* coefficient (VDC) row vector with the measurement column vector */
/* (VALUE).  The VDC vector is 2-dimensional, each row vector is optimized*/
/* for a particular signal-to-noise ratio (SNR).  So, before the dot */
/* product is performed, the SNR is estimated to select the appropriate */
/* VDC vector. */

/* The smoothing algorithm is a modified median smoother.  The */
/* voicing discriminant function is used by the smoother to determine how*/
/* strongly voiced or unvoiced a signal is.  The smoothing is further */
/* modified if a speech onset and a voicing decision transition occur */
/* within one half frame.  In this case, the voicing decision transition */
/* is extended to the speech onset.  For transmission purposes, there are*/
/* constraints on the duration and transition of voicing decisions.  The */
/* smoother takes these constraints into account. */

/* Finally, the energy estimates are updated along with the dither */
/* threshold used to calculate the zero crossing rate (ZC). */

void lpc10_voicing(lpc10_encode_state_t *s,
                   int32_t vwin[],
                   float *inbuf,
                   float *lpbuf,
                   const int32_t buflim[],
                   int32_t half,
                   float *minamd,
                   float *maxamd, 
                   int32_t *mintau,
                   float ivrc[],
                   int32_t obound[])
{
    static const float vdc[100] =
    {
        0.0f, 1714.0f, -110.0f, 334.0f, -4096.0f,  -654.0f, 3752.0f, 3769.0f, 0.0f,  1181.0f,
        0.0f,  874.0f,  -97.0f, 300.0f, -4096.0f, -1021.0f, 2451.0f, 2527.0f, 0.0f,  -500.0f,
        0.0f,  510.0f,  -70.0f, 250.0f, -4096.0f, -1270.0f, 2194.0f, 2491.0f, 0.0f, -1500.0f,
        0.0f,  500.0f,  -10.0f, 200.0f, -4096.0f, -1300.0f,  2.0e3f,  2.0e3f, 0.0f,  -2.0e3f,
        0.0f,  500.0f,    0.0f,   0.0f, -4096.0f, -1300.0f,  2.0e3f,  2.0e3f, 0.0f, -2500.0f,
        0.0f,    0.0f,    0.0f,   0.0f,     0.0f,     0.0f,    0.0f,    0.0f, 0.0f,     0.0f,
        0.0f,    0.0f,    0.0f,   0.0f,     0.0f,     0.0f,    0.0f,    0.0f, 0.0f,     0.0f,
        0.0f,    0.0f,    0.0f,   0.0f,     0.0f,     0.0f,    0.0f,    0.0f, 0.0f,     0.0f,
        0.0f,    0.0f,    0.0f,   0.0f,     0.0f,     0.0f,    0.0f,    0.0f, 0.0f,     0.0f,
        0.0f,    0.0f,    0.0f,   0.0f,     0.0f,     0.0f,    0.0f,    0.0f, 0.0f,     0.0f
    };
    static const int nvdcl = 5;
    static const float vdcl[10] =
    {
        600.0f, 450.0f, 300.0f, 200.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };

    int32_t inbuf_offset;
    int32_t lpbuf_offset;
    int32_t i1;
    float r1;
    float r2;
    float ar_b;
    float ar_f;
    int32_t snrl;
    int32_t i;
    float value[9];
    int32_t zc;
    int ot;
    float qs;
    int32_t vstate;
    float rc1;
    int32_t fbe;
    int32_t lbe;
    float snr2;

#if (_MSC_VER >= 1400) 
    __analysis_assume(half >= 0  &&  half < 2);
#endif
    inbuf_offset = 0;
    lpbuf_offset = 0;
    if (inbuf)
    {
        inbuf_offset = buflim[0];
        inbuf -= inbuf_offset;
    }
    if (lpbuf)
    {
        lpbuf_offset = buflim[2];
        lpbuf -= lpbuf_offset;
    }

    /* Voicing Decision Parameter vector (* denotes zero coefficient): */

    /*     * MAXMIN */
    /*       LBE/LBVE */
    /*       ZC */
    /*       RC1 */
    /*       QS */
    /*       IVRC2 */
    /*       aR_B */
    /*       aR_F */
    /*     * LOG(LBE/LBVE) */
    /* Define 2-D voicing decision coefficient vector according to the voicing */
    /* parameter order above.  Each row (VDC vector) is optimized for a specific */
    /*   SNR.  The last element of the vector is the constant. */
    /*              E    ZC    RC1    Qs   IVRC2  aRb   aRf        c */

    /* The VOICE array contains the result of the linear discriminant function*/
    /* (analog values).  The VOIBUF array contains the hard-limited binary */
    /* voicing decisions.  The VOICE and VOIBUF arrays, according to FORTRAN */
    /* memory allocation, are addressed as: */

    /*        (half-frame number, future-frame number) */

    /*        |   Past    |  Present  |  Future1  |  Future2  | */
    /*        | 1,0 | 2,0 | 1,1 | 2,1 | 1,2 | 2,2 | 1,3 | 2,3 |  --->  time */

    /* Update linear discriminant function history each frame: */
    if (half == 0)
    {
        s->voice[0][0] = s->voice[1][0];
        s->voice[0][1] = s->voice[1][1];
        s->voice[1][0] = s->voice[2][0];
        s->voice[1][1] = s->voice[2][1];
        s->maxmin = *maxamd / max(*minamd, 1.0f);
    }
    /* Calculate voicing parameters twice per frame */
    vparms(vwin,
           &inbuf[inbuf_offset],
           &lpbuf[lpbuf_offset],
           buflim, 
           half,
           &s->dither,
           mintau,
           &zc,
           &lbe,
           &fbe,
           &qs,
           &rc1,
           &ar_b,
           &ar_f);
    /* Estimate signal-to-noise ratio to select the appropriate VDC vector. */
    /* The SNR is estimated as the running average of the ratio of the */
    /* running average full-band voiced energy to the running average */
    /* full-band unvoiced energy. SNR filter has gain of 63. */
    r1 = (s->snr + s->fbve/(float) max(s->fbue, 1))*63/64.0f;
    s->snr = (float) lfastrintf(r1);
    snr2 = s->snr*s->fbue/max(s->lbue, 1);
    /* Quantize SNR to SNRL according to VDCL thresholds. */
    i1 = nvdcl - 1;
    for (snrl = 0;  snrl < i1;  snrl++)
    {
        if (snr2 > vdcl[snrl])
            break;
    }
    /* (Note:  SNRL = NVDCL here) */
    /* Linear discriminant voicing parameters: */
    value[0] = s->maxmin;
    value[1] = (float) lbe/max(s->lbve, 1);
    value[2] = (float) zc;
    value[3] = rc1;
    value[4] = qs;
    value[5] = ivrc[1];
    value[6] = ar_b;
    value[7] = ar_f;
    /* Evaluation of linear discriminant function: */
    s->voice[2][half] = vdc[snrl*10 + 9];
    for (i = 0;  i < 8;  i++)
        s->voice[2][half] += vdc[snrl*10 + i]*value[i];
    /* Classify as voiced if discriminant > 0, otherwise unvoiced */
    /* Voicing decision for current half-frame:  1 = Voiced; 0 = Unvoiced */
    s->voibuf[3][half] = (s->voice[2][half] > 0.0f)  ?  1  :  0;
    /* Skip voicing decision smoothing in first half-frame: */
    /* Give a value to VSTATE, so that trace statements below will print */
    /* a consistent value from one call to the next when HALF .EQ. 1. */
    /* The value of VSTATE is not used for any other purpose when this is */
    /* true. */
    vstate = -1;
    if (half != 0)
    {
        /* Voicing decision smoothing rules (override of linear combination): */

        /*     Unvoiced half-frames:  At least two in a row. */
        /*     -------------------- */

        /*     Voiced half-frames:    At least two in a row in one frame. */
        /*     -------------------    Otherwise at least three in a row. */
        /*                    (Due to the way transition frames are encoded) */

        /* In many cases, the discriminant function determines how to smooth. */
        /* In the following chart, the decisions marked with a * may be overridden. */

        /* Voicing override of transitions at onsets: */
        /* If a V/UV or UV/V voicing decision transition occurs within one-half */
        /* frame of an onset bounding a voicing window, then the transition is */
        /* moved to occur at the onset. */

        /*     P    1F */
        /*     -----    ----- */
        /*     0   0   0   0 */
        /*     0   0   0*  1    (If there is an onset there) */
        /*     0   0   1*  0*    (Based on 2F and discriminant distance) */
        /*     0   0   1   1 */
        /*     0   1*  0   0    (Always) */
        /*     0   1*  0*  1    (Based on discriminant distance) */
        /*     0*  1   1   0*    (Based on past, 2F, and discriminant distance) */
        /*     0   1*  1   1    (If there is an onset there) */
        /*     1   0*  0   0    (If there is an onset there) */
        /*     1   0   0   1 */
        /*     1   0*  1*  0    (Based on discriminant distance) */
        /*     1   0*  1   1    (Always) */
        /*     1   1   0   0 */
        /*     1   1   0*  1*    (Based on 2F and discriminant distance) */
        /*     1   1   1*  0    (If there is an onset there) */
        /*     1   1   1   1 */

        /* Determine if there is an onset transition between P and 1F. */
        /* OT (Onset Transition) is true if there is an onset between */
        /* P and 1F but not after 1F. */
        ot = ((obound[0] & 2) != 0  ||  obound[1] == 1)  &&  (obound[2] & 1) == 0;
        /* Multi-way dispatch on voicing decision history: */
        vstate = (s->voibuf[1][0] << 3) + (s->voibuf[1][1] << 2) + (s->voibuf[2][0] << 1) + s->voibuf[2][1];
        switch (vstate + 1)
        {
        case 2:
            if (ot  &&  s->voibuf[3][0] == 1)
                s->voibuf[2][0] = 1;
            break;
        case 3:
            if (s->voibuf[3][0] == 0  ||  s->voice[1][0] < -s->voice[1][1])
                s->voibuf[2][0] = 0;
            else
                s->voibuf[2][1] = 1;
            break;
        case 5:
            s->voibuf[1][1] = 0;
            break;
        case 6:
            if (s->voice[0][1] < -s->voice[1][0])
                s->voibuf[1][1] = 0;
            else
                s->voibuf[2][0] = 1;
            break;
        case 7:
            if (s->voibuf[0][0] == 1  ||  s->voibuf[3][0] == 1  ||  s->voice[1][1] > s->voice[0][0])
                s->voibuf[2][1] = 1;
            else
                s->voibuf[1][0] = 1;
            break;
        case 8:
            if (ot)
                s->voibuf[1][1] = 0;
            break;
        case 9:
            if (ot)
                s->voibuf[1][1] = 1;
            break;
        case 11:
            if (s->voice[1][9] < -s->voice[0][1])
                s->voibuf[2][0] = 0;
            else
                s->voibuf[1][1] = 1;
            break;
        case 12:
            s->voibuf[1][1] = 1;
            break;
        case 14:
            if (s->voibuf[3][0] == 0  &&  s->voice[1][1] < -s->voice[1][0])
                s->voibuf[2][1] = 0;
            else
                s->voibuf[2][0] = 1;
            break;
        case 15:
            if (ot  &&  s->voibuf[3][0] == 0)
                s->voibuf[2][0] = 0;
            break;
        }
    }
    /* During unvoiced half-frames, update the low band and full band unvoiced*/
    /* energy estimates (LBUE and FBUE) and also the zero crossing */
    /* threshold (DITHER).  (The input to the unvoiced energy filters is */
    /* restricted to be less than 10dB above the previous inputs of the */
    /* filters.) */
    /* During voiced half-frames, update the low-pass (LBVE) and all-pass */
    /* (FBVE) voiced energy estimates. */
    if (s->voibuf[3][half] == 0)
    {
        r1 = (s->sfbue*63 + (min(fbe, s->ofbue*3) << 3))/64.0f;
        s->sfbue = lfastrintf(r1);
        s->fbue = s->sfbue/8;
        s->ofbue = fbe;
        r1 = (s->slbue*63 + (min(lbe, s->olbue*3) << 3))/64.0f;
        s->slbue = lfastrintf(r1);
        s->lbue = s->slbue/8;
        s->olbue = lbe;
    }
    else
    {
        s->lbve = lfastrintf((s->lbve*63 + lbe)/64.0f);
        s->fbve = lfastrintf((s->fbve*63 + fbe)/64.0f);
    }
    /* Set dither threshold to yield proper zero crossing rates in the */
    /* presence of low frequency noise and low level signal input. */
    /* NOTE: The divisor is a function of REF, the expected energies. */
    /* Computing MIN */
    /* Computing MAX */
    r2 = sqrtf((float) (s->lbue*s->lbve))*64/3000;
    r1 = max(r2, 1.0f);
    s->dither = min(r1, 20.0f);
    /* Voicing decisions are returned in VOIBUF. */
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
