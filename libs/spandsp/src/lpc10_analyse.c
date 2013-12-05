/*
 * SpanDSP - a series of DSP components for telephony
 *
 * lpc10_analyse.c - LPC10 low bit rate speech codec.
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
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/lpc10.h"
#include "spandsp/private/lpc10.h"

#include "lpc10_encdecs.h"

static __inline__ float energyf(float amp[], int len)
{
    int i;
    float rms;

    rms = 0.0f;
    for (i = 0;  i < len;  i++)
        rms += amp[i]*amp[i];
    rms = sqrtf(rms/len);
    return rms;
}
/*- End of function --------------------------------------------------------*/

static void remove_dc_bias(float speech[], int len, float sigout[])
{
    float bias;
    int i;

    bias = 0.0f;
    for (i = 0;  i < len;  i++)
        bias += speech[i];
    bias /= len;
    for (i = 0; i < len;  i++)
        sigout[i] = speech[i] - bias;
}
/*- End of function --------------------------------------------------------*/

static void eval_amdf(float speech[],
                      int32_t lpita,
                      const int32_t tau[],
                      int32_t ltau,
                      int32_t maxlag,
                      float amdf[],
                      int32_t *minptr,
                      int32_t *maxptr)
{
    float sum;
    int i;
    int j;
    int n1;
    int n2;

    *minptr = 0;
    *maxptr = 0;
    for (i = 0;  i < ltau;  i++)
    {
        n1 = (maxlag - tau[i])/2 + 1;
        n2 = n1 + lpita - 1;
        sum = 0.0f;
        for (j = n1;  j <= n2;  j += 4)
            sum += fabsf(speech[j - 1] - speech[j + tau[i] - 1]);
        amdf[i] = sum;
        if (amdf[i] < amdf[*minptr])
            *minptr = i;
        if (amdf[i] > amdf[*maxptr])
            *maxptr = i;
    }
}
/*- End of function --------------------------------------------------------*/

static void eval_highres_amdf(float speech[],
                              int32_t lpita,
                              const int32_t tau[],
                              int32_t ltau,
                              float amdf[],
                              int32_t *minptr,
                              int32_t *maxptr,
                              int32_t *mintau)
{
    float amdf2[6];
    int32_t tau2[6];
    int32_t minp2;
    int32_t ltau2;
    int32_t maxp2;
    int32_t minamd;
    int i;
    int i2;
    int ptr;

    /* Compute full AMDF using log spaced lags, find coarse minimum */
    eval_amdf(speech, lpita, tau, ltau, tau[ltau - 1], amdf, minptr, maxptr);
    *mintau = tau[*minptr];
    minamd = (int32_t) amdf[*minptr];

    /* Build table containing all lags within +/- 3 of the AMDF minimum,
       excluding all that have already been computed */
    ltau2 = 0;
    ptr = *minptr - 2;
    i2 = min(*mintau + 4, tau[ltau - 1]);
    for (i = max(*mintau - 3, 41);  i < i2;  i++)
    {
        while (tau[ptr] < i)
            ptr++;
        if (tau[ptr] != i)
            tau2[ltau2++] = i;
    }
    /* Compute AMDF of the new lags, if there are any, and choose one
       if it is better than the coarse minimum */
    if (ltau2 > 0)
    {
        eval_amdf(speech, lpita, tau2, ltau2, tau[ltau - 1], amdf2, &minp2, &maxp2);
        if (amdf2[minp2] < (float) minamd)
        {
            *mintau = tau2[minp2];
            minamd = (int32_t) amdf2[minp2];
        }
    }
    /* Check one octave up, if there are any lags not yet computed */
    if (*mintau >= 80)
    {
        i = *mintau/2;
        if ((i & 1) == 0)
        {
            ltau2 = 2;
            tau2[0] = i - 1;
            tau2[1] = i + 1;
        }
        else
        {
            ltau2 = 1;
            tau2[0] = i;
        }
        eval_amdf(speech, lpita, tau2, ltau2, tau[ltau - 1], amdf2, &minp2, &maxp2);
        if (amdf2[minp2] < (float) minamd)
        {
            *mintau = tau2[minp2];
            minamd = (int32_t) amdf2[minp2];
            *minptr -= 20;
        }
    }
    /* Force minimum of the AMDF array to the high resolution minimum */
    amdf[*minptr] = (float) minamd;
    /* Find maximum of AMDF within 1/2 octave of minimum */
    *maxptr = max(*minptr - 5, 0);
    i2 = min(*minptr + 6, ltau);
    for (i = *maxptr;  i < i2;  i++)
    {
        if (amdf[i] > amdf[*maxptr])
            *maxptr = i;
    }
}
/*- End of function --------------------------------------------------------*/

static void dynamic_pitch_tracking(lpc10_encode_state_t *s,
                                   float amdf[],
                                   int32_t ltau,
                                   int32_t *minptr,
                                   int32_t voice,
                                   int32_t *pitch,
                                   int32_t *midx)
{
    int32_t pbar;
    float sbar;
    int32_t i;
    int32_t j;
    float alpha;
    float minsc;
    float maxsc;

    /* Calculate the confidence factor ALPHA, used as a threshold slope in */
    /* SEESAW.  If unvoiced, set high slope so that every point in P array */
    /*is marked as a potential pitch frequency.  A scaled up version (ALPHAX )*/
    /* is used to maintain arithmetic precision. */
    if (voice == 1)
        s->alphax = s->alphax*0.75f + amdf[*minptr - 1]*0.5f;
    else
        s->alphax *= 0.984375f;
    alpha = s->alphax/16;
    if (voice == 0  &&  s->alphax < 128.0f)
        alpha = 8.0f;
    /* SEESAW: Construct a pitch pointer array and intermediate winner function */
    /* Left to right pass: */
    s->p[s->ipoint][0] = 1;
    pbar = 1;
    sbar = s->s[0];
    for (i = 0;  i < ltau;  i++)
    {
        sbar += alpha;
        if (sbar < s->s[i])
        {
            s->s[i] = sbar;
        }
        else
        {
            pbar = i + 1;
            sbar = s->s[i];
        }
        s->p[s->ipoint][i] = pbar;
    }
    /* Right to left pass: */
    sbar = s->s[pbar - 1];
    for (i = pbar - 2;  i >= 0;  i--)
    {
        sbar += alpha;
        if (sbar < s->s[i])
        {
            s->s[i] = sbar;
            s->p[s->ipoint][i] = pbar;
        }
        else
        {
            pbar = s->p[s->ipoint][i];
            i = pbar - 1;
            sbar = s->s[i];
        }
    }
    /* Update S using AMDF */
    /* Find maximum, minimum, and location of minimum */
    s->s[0] += amdf[0]/2;
    minsc = s->s[0];
    maxsc = minsc;
    *midx = 1;
    for (i = 1;  i < ltau;  i++)
    {
        s->s[i] += amdf[i]/2;
        if (s->s[i] > maxsc)
            maxsc = s->s[i];
        if (s->s[i] < minsc)
        {
            *midx = i + 1;
            minsc = s->s[i];
        }
    }
    /* Subtract MINSC from S to prevent overflow */
    for (i = 0;  i < ltau;  i++)
        s->s[i] -= minsc;
    maxsc -= minsc;
    /* Use higher octave pitch if significant null there */
    j = 0;
    for (i = 20;  i <= 40;  i += 10)
    {
        if (*midx > i)
        {
            if (s->s[*midx - i - 1] < maxsc / 4)
                j = i;
        }
    }
    *midx -= j;
    /* TRACE: look back two frames to find minimum cost pitch estimate */
    *pitch = *midx;
    for (i = 0, j = s->ipoint;  i < 2;  i++, j++)
        *pitch = s->p[j & 1][*pitch - 1];

    /* The following statement subtracts one from IPOINT, mod DEPTH.  I */
    /* think the author chose to add DEPTH-1, instead of subtracting 1, */
    /* because then it will work even if MOD doesn't work as desired on */
    /* negative arguments. */
    s->ipoint = (s->ipoint + 1) & 1;
}
/*- End of function --------------------------------------------------------*/

/* Detection of onsets in (or slightly preceding) the futuremost frame of speech. */
static void onset(lpc10_encode_state_t *s,
                  float *pebuf,
                  int32_t osbuf[],
                  int32_t *osptr,
                  int32_t oslen,
                  int32_t sbufl,
                  int32_t sbufh,
                  int32_t lframe)
{
    int32_t i;
    float r1;
    float l2sum2;

    pebuf -= sbufl;

    if (s->hyst)
        s->lasti -= lframe;
    for (i = sbufh - lframe + 1;  i <= sbufh;  i++)
    {
        /* Compute FPC; Use old FPC on divide by zero; Clamp FPC to +/- 1. */
        s->n = (pebuf[i]*pebuf[i - 1] + s->n*63.0f)/64.0f;
        /* Computing 2nd power */
        r1 = pebuf[i - 1];
        s->d__ = (r1*r1 + s->d__*63.0f)/64.0f;
        if (s->d__ != 0.0f)
        {
            if (fabsf(s->n) > s->d__)
                s->fpc = r_sign(1.0f, s->n);
            else
                s->fpc = s->n/s->d__;
        }
        /* Filter FPC */
        l2sum2 = s->l2buf[s->l2ptr1 - 1];
        s->l2sum1 = s->l2sum1 - s->l2buf[s->l2ptr2 - 1] + s->fpc;
        s->l2buf[s->l2ptr2 - 1] = s->l2sum1;
        s->l2buf[s->l2ptr1 - 1] = s->fpc;
        s->l2ptr1 = (s->l2ptr1 & 0xF) + 1;
        s->l2ptr2 = (s->l2ptr2 & 0xF) + 1;
        if (fabsf(s->l2sum1 - l2sum2) > 1.7f)
        {
            if (!s->hyst)
            {
                /* Ignore if buffer full */
                if (*osptr <= oslen)
                {
                    osbuf[*osptr - 1] = i - 9;
                    (*osptr)++;
                }
                s->hyst = true;
            }
            s->lasti = i;
            /* After one onset detection, at least OSHYST sample times must go */
            /* by before another is allowed to occur. */
        }
        else if (s->hyst  &&  i - s->lasti >= 10)
        {
            s->hyst = false;
        }
    }
}
/*- End of function --------------------------------------------------------*/

/* Load a covariance matrix. */
static void mload(int32_t order, int32_t awins, int32_t awinf, float speech[], float phi[], float psi[])
{
    int32_t start;
    int i;
    int r;

    start = awins + order;
    for (r = 1;  r <= order;  r++)
    {
        phi[r - 1] = 0.0f;
        for (i = start;  i <= awinf;  i++)
            phi[r - 1] += speech[i - 2]*speech[i - r - 1];
    }

    /* Load last element of vector PSI */
    psi[order - 1] = 0.0f;
    for (i = start - 1;  i < awinf;  i++)
        psi[order - 1] += speech[i]*speech[i - order];
    /* End correct to get additional columns of phi */
    for (r = 1;  r < order;  r++)
    {
        for (i = 1;  i <= r;  i++)
        {
            phi[i*order + r] = phi[(i - 1)*order + r - 1]
                             - speech[awinf - (r + 1)]*speech[awinf - (i + 1)]
                             + speech[start - (r + 2)]*speech[start - (i + 2)];
        }
    }
    /* End correct to get additional elements of PSI */
    for (i = 0;  i < order - 1;  i++)
    {
        psi[i] = phi[i + 1]
               - speech[start - 2]*speech[start - i - 3]
               + speech[awinf - 1]*speech[awinf - i - 2];
    }
}
/*- End of function --------------------------------------------------------*/

/* Preemphasize speech with a single-zero filter. */
/* (When coef = .9375, preemphasis is as in LPC43.) */
static float preemp(float inbuf[], float pebuf[], int nsamp, float coeff, float z)
{
    float temp;
    int i;

    for (i = 0;  i < nsamp;  i++)
    {
        temp = inbuf[i] - coeff*z;
        z = inbuf[i];
        pebuf[i] = temp;
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

/* Invert a covariance matrix using Choleski decomposition method. */
static void invert(int32_t order, float phi[], float psi[], float rc[])
{
    float r1;
    int32_t i;
    int32_t j;
    int32_t k;
    float v[10][10];

    for (j = 0;  j < order;  j++)
    {
        for (i = j;  i < order;  i++)
            v[j][i] = phi[i + j*order];
        for (k = 0;  k < j;  k++)
        {
            r1 = v[k][j]*v[k][k];
            for (i = j;  i <= order;  i++)
                v[j][i] -= v[k][i]*r1;
        }
        /* Compute intermediate results, which are similar to RC's */
        if (fabsf(v[j][j]) < 1.0e-10f)
        {
            for (i = j;  i < order;  i++)
                rc[i] = 0.0f;
            return;
        }
        rc[j] = psi[j];
        for (k = 0;  k < j;  k++)
            rc[j] -= rc[k]*v[k][j];
        v[j][j] = 1.0f/v[j][j];
        rc[j] *= v[j][j];
        r1 = min(rc[j], 0.999f);
        rc[j] = max(r1, -0.999f);
    }
}
/*- End of function --------------------------------------------------------*/

/* Check RC's, repeat previous frame's RC's if unstable */
static int rcchk(int order, float rc1f[], float rc2f[])
{
    int i;

    for (i = 0;  i < order;  i++)
    {
        if (fabsf(rc2f[i]) > 0.99f)
        {
            for (i = 0;  i < order;  i++)
                rc2f[i] = rc1f[i];
            break;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void lpfilt(float inbuf[], float lpbuf[], int32_t len, int32_t nsamp)
{
    int32_t j;
    float t;

    /* 31 point equiripple FIR LPF */
    /* Linear phase, delay = 15 samples */
    /* Passband:  ripple = 0.25 dB, cutoff =  800 Hz */
    /* Stopband:  atten. =  40. dB, cutoff = 1240 Hz */

    for (j = len - nsamp;  j < len;  j++)
    {
        t = (inbuf[j] + inbuf[j - 30]) * -0.0097201988f;
        t += (inbuf[j - 1] + inbuf[j - 29]) * -0.0105179986f;
        t += (inbuf[j - 2] + inbuf[j - 28]) * -0.0083479648f;
        t += (inbuf[j - 3] + inbuf[j - 27]) * 5.860774e-4f;
        t += (inbuf[j - 4] + inbuf[j - 26]) * 0.0130892089f;
        t += (inbuf[j - 5] + inbuf[j - 25]) * 0.0217052232f;
        t += (inbuf[j - 6] + inbuf[j - 24]) * 0.0184161253f;
        t += (inbuf[j - 7] + inbuf[j - 23]) * 3.39723e-4f;
        t += (inbuf[j - 8] + inbuf[j - 22]) * -0.0260797087f;
        t += (inbuf[j - 9] + inbuf[j - 21]) * -0.0455563702f;
        t += (inbuf[j - 10] + inbuf[j - 20]) * -0.040306855f;
        t += (inbuf[j - 11] + inbuf[j - 19]) * 5.029835e-4f;
        t += (inbuf[j - 12] + inbuf[j - 18]) * 0.0729262903f;
        t += (inbuf[j - 13] + inbuf[j - 17]) * 0.1572008878f;
        t += (inbuf[j - 14] + inbuf[j - 16]) * 0.2247288674f;
        t += inbuf[j - 15] * 0.250535965f;
        lpbuf[j] = t;
    }
}
/*- End of function --------------------------------------------------------*/

/* 2nd order inverse filter, speech is decimated 4:1 */
static void ivfilt(float lpbuf[], float ivbuf[], int32_t len, int32_t nsamp, float ivrc[])
{
    int32_t i;
    int32_t j;
    int32_t k;
    float r[3];
    float pc1;
    float pc2;

    /* Calculate autocorrelations */
    for (i = 1;  i <= 3;  i++)
    {
        r[i - 1] = 0.0f;
        k = (i - 1) << 2;
        for (j = (i << 2) + len - nsamp;  j <= len;  j += 2)
            r[i - 1] += lpbuf[j - 1]*lpbuf[j - k - 1];
    }
    /* Calculate predictor coefficients */
    pc1 = 0.0f;
    pc2 = 0.0f;
    ivrc[0] = 0.0f;
    ivrc[1] = 0.0f;
    if (r[0] > 1.0e-10f)
    {
        ivrc[0] = r[1]/r[0];
        ivrc[1] = (r[2] - ivrc[0]*r[1])/(r[0] - ivrc[0]*r[1]);
        pc1 = ivrc[0] - ivrc[0]*ivrc[1];
        pc2 = ivrc[1];
    }
    /* Inverse filter LPBUF into IVBUF */
    for (i = len - nsamp;  i < len;  i++)
        ivbuf[i] = lpbuf[i] - pc1*lpbuf[i - 4] - pc2*lpbuf[i - 8];
}
/*- End of function --------------------------------------------------------*/

void lpc10_analyse(lpc10_encode_state_t *s, float speech[], int32_t voice[], int32_t *pitch, float *rms, float rc[])
{
    static const int32_t tau[60] =
    {
         20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,
         35,  36,  37,  38,  39,  40,  42,  44,  46,  48,  50,  52,  54,  56,  58,
         60,  62,  64,  66,  68,  70,  72,  74,  76,  78,  80,  84,  88,  92,  96,
        100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152, 156
    };
    static const int32_t buflim[4] =
    {
        181, 720, 25, 720
    };
    static const float precoef = 0.9375f;

    float amdf[60];
    float abuf[156];
    float ivrc[2];
    float temp;
    float phi[100]    /* was [10][10] */;
    float psi[10];
    int32_t half;
    int32_t midx;
    int32_t ewin[3][2];
    int32_t i;
    int32_t j;
    int32_t lanal;
    int32_t ipitch;
    int32_t mintau;
    int32_t minptr;
    int32_t maxptr;

    /* Calculations are done on future frame due to requirements
       of the pitch tracker.  Delay RMS and RC's 2 frames to give
       current frame parameters on return. */

    for (i = 0;  i <= 720 - LPC10_SAMPLES_PER_FRAME - 181;  i++)
    {
        s->inbuf[i] = s->inbuf[LPC10_SAMPLES_PER_FRAME + i];
        s->pebuf[i] = s->pebuf[LPC10_SAMPLES_PER_FRAME + i];
    }
    for (i = 0;  i <= 540 - LPC10_SAMPLES_PER_FRAME - 229;  i++)
        s->ivbuf[i] = s->ivbuf[LPC10_SAMPLES_PER_FRAME + i];
    for (i = 0;  i <= 720 - LPC10_SAMPLES_PER_FRAME - 25;  i++)
        s->lpbuf[i] = s->lpbuf[LPC10_SAMPLES_PER_FRAME + i];
    for (i = 0, j = 0;  i < s->osptr - 1;  i++)
    {
        if (s->osbuf[i] > LPC10_SAMPLES_PER_FRAME)
            s->osbuf[j++] = s->osbuf[i] - LPC10_SAMPLES_PER_FRAME;
    }
    s->osptr = j + 1;
    s->voibuf[0][0] = s->voibuf[1][0];
    s->voibuf[0][1] = s->voibuf[1][1];
    for (i = 0;  i < 2;  i++)
    {
        s->vwin[i][0] = s->vwin[i + 1][0] - LPC10_SAMPLES_PER_FRAME;
        s->vwin[i][1] = s->vwin[i + 1][1] - LPC10_SAMPLES_PER_FRAME;
        s->awin[i][0] = s->awin[i + 1][0] - LPC10_SAMPLES_PER_FRAME;
        s->awin[i][1] = s->awin[i + 1][1] - LPC10_SAMPLES_PER_FRAME;
        s->obound[i] = s->obound[i + 1];
        s->voibuf[i + 1][0] = s->voibuf[i + 2][0];
        s->voibuf[i + 1][1] = s->voibuf[i + 2][1];
        s->rmsbuf[i] = s->rmsbuf[i + 1];
        for (j = 0;  j < LPC10_ORDER;  j++)
            s->rcbuf[i][j] = s->rcbuf[i + 1][j];
    }
    /* If the average value in the frame was over 1/4096 (after current
       BIAS correction), then subtract that much more from samples in the
       next frame.  If the average value in the frame was under
       -1/4096, add 1/4096 more to samples in next frame.  In all other
       cases, keep BIAS the same. */
    temp = 0.0f;
    for (i = 0;  i < LPC10_SAMPLES_PER_FRAME;  i++)
    {
        s->inbuf[720 - 2*LPC10_SAMPLES_PER_FRAME + i] = speech[i]*4096.0f - s->bias;
        temp += s->inbuf[720 - 2*LPC10_SAMPLES_PER_FRAME + i];
    }
    if (temp > (float) LPC10_SAMPLES_PER_FRAME)
        s->bias++;
    else if (temp < (float) (-LPC10_SAMPLES_PER_FRAME))
        s->bias--;
    /* Place voicing window */
    i = 721 - LPC10_SAMPLES_PER_FRAME;
    s->zpre = preemp(&s->inbuf[i - 181], &s->pebuf[i - 181], LPC10_SAMPLES_PER_FRAME, precoef, s->zpre);
    onset(s, s->pebuf, s->osbuf, &s->osptr, 10, 181, 720, LPC10_SAMPLES_PER_FRAME);

    lpc10_placev(s->osbuf, &s->osptr, 10, &s->obound[2], s->vwin, 3, LPC10_SAMPLES_PER_FRAME, 90, 156, 307, 462);
    /* The Pitch Extraction algorithm estimates the pitch for a frame
       of speech by locating the minimum of the average magnitude difference
       function (AMDF).  The AMDF operates on low-pass, inverse filtered
       speech.  (The low-pass filter is an 800 Hz, 19 tap, equiripple, FIR
       filter and the inverse filter is a 2nd-order LPC filter.)  The pitch
       estimate is later refined by dynamic tracking.  However, since some
       of the tracking parameters are a function of the voicing decisions,
       a voicing decision must precede the final pitch estimation. */
    /* See subroutines LPFILT, IVFILT, and eval_highres_amdf. */
    /* LPFILT reads indices LBUFH-LFRAME-29 = 511 through LBUFH = 720
       of INBUF, and writes indices LBUFH+1-LFRAME = 541 through LBUFH
       = 720 of LPBUF. */
    lpfilt(&s->inbuf[228], &s->lpbuf[384], 312, LPC10_SAMPLES_PER_FRAME);
    /* IVFILT reads indices (PWINH-LFRAME-7) = 353 through PWINH = 540
       of LPBUF, and writes indices (PWINH-LFRAME+1) = 361 through
       PWINH = 540 of IVBUF. */
    ivfilt(&s->lpbuf[204], s->ivbuf, 312, LPC10_SAMPLES_PER_FRAME, ivrc);
    /* eval_highres_amdf reads indices PWINL = 229 through
       (PWINL-1)+MAXWIN+(TAU(LTAU)-TAU(1))/2 = 452 of IVBUF, and writes
       indices 1 through LTAU = 60 of AMDF. */
    eval_highres_amdf(s->ivbuf, 156, tau, 60, amdf, &minptr, &maxptr, &mintau);
    /* Voicing decisions are made for each half frame of input speech.
       An initial voicing classification is made for each half of the
       analysis frame, and the voicing decisions for the present frame
       are finalized.  See subroutine VOICIN. */
    /*        The voicing detector (VOICIN) classifies the input signal as
       unvoiced (including silence) or voiced using the AMDF windowed
       maximum-to-minimum ratio, the zero crossing rate, energy measures,
       reflection coefficients, and prediction gains. */
    /*        The pitch and voicing rules apply smoothing and isolated
       corrections to the pitch and voicing estimates and, in the process,
       introduce two frames of delay into the corrected pitch estimates and
       voicing decisions. */
    for (half = 0;  half < 2;  half++)
    {
        lpc10_voicing(s,
                      &s->vwin[2][0],
                      s->inbuf,
                      s->lpbuf,
                      buflim,
                      half,
                      &amdf[minptr],
                      &amdf[maxptr],
                      &mintau,
                      ivrc,
                      s->obound);
    }
    /* Find the minimum cost pitch decision over several frames,
       given the current voicing decision and the AMDF array */
    minptr++;
    dynamic_pitch_tracking(s, amdf, 60, &minptr, s->voibuf[3][1], pitch, &midx);
    ipitch = tau[midx - 1];
    /* Place spectrum analysis and energy windows */
    lpc10_placea(&ipitch, s->voibuf, &s->obound[2], 3, s->vwin, s->awin, ewin, LPC10_SAMPLES_PER_FRAME, 156);
    /* Remove short term DC bias over the analysis window. */
    lanal = s->awin[2][1] + 1 - s->awin[2][0];
    remove_dc_bias(&s->pebuf[s->awin[2][0] - 181], lanal, abuf);
    /* Compute RMS over integer number of pitch periods within the analysis window. */
    /* Note that in a hardware implementation this computation may be
       simplified by using diagonal elements of phi computed by mload(). */
    s->rmsbuf[2] = energyf(&abuf[ewin[2][0] - s->awin[2][0]], ewin[2][1] - ewin[2][0] + 1);
    /* Matrix load and invert, check RC's for stability */
    mload(LPC10_ORDER, 1, lanal, abuf, phi, psi);
    invert(LPC10_ORDER, phi, psi, &s->rcbuf[2][0]);
    rcchk(LPC10_ORDER, &s->rcbuf[1][0], &s->rcbuf[2][0]);
    /* Set return parameters */
    voice[0] = s->voibuf[1][0];
    voice[1] = s->voibuf[1][1];
    *rms = s->rmsbuf[0];
    for (i = 0;  i < LPC10_ORDER;  i++)
        rc[i] = s->rcbuf[0][i];
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
