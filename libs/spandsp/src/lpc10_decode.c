/*
 * SpanDSP - a series of DSP components for telephony
 *
 * lpc10_decode.c - LPC10 low bit rate speech codec.
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
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <memory.h>

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/lpc10.h"
#include "spandsp/private/lpc10.h"

#define LPC10_ORDER     10

#if !defined(min)
#define min(a,b) ((a) <= (b) ? (a) : (b))
#endif
#if !defined(max)
#define max(a,b) ((a) >= (b) ? (a) : (b))
#endif

/* Pseudo random number generator based on Knuth, Vol 2, p. 27. */
/* lpc10_random - int32_t variable, uniformly distributed over -32768 to 32767 */
static int32_t lpc10_random(lpc10_decode_state_t *s)
{
    int32_t ret_val;

    /* The following is a 16 bit 2's complement addition,
       with overflow checking disabled */
    s->y[s->k] += s->y[s->j];
    ret_val = s->y[s->k];
    if (--s->k < 0)
        s->k = 4;
    if (--s->j < 0)
        s->j = 4;
    return ret_val;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t pow_ii(int32_t x, int32_t n)
{
    int32_t pow;
    uint32_t u;

    if (n <= 0)
    {
        if (n == 0  ||  x == 1)
            return 1;
        if (x != -1)
            return (x != 0)  ?  1/x  :  0;
        n = -n;
    }
    u = n;
    for (pow = 1;  ;  )
    {
        if ((u & 1))
            pow *= x;
        if ((u >>= 1) == 0)
            break;
        x *= x;
    }
    return pow;
}
/*- End of function --------------------------------------------------------*/

/* Synthesize one pitch epoch */
static void bsynz(lpc10_decode_state_t *s,
                  float coef[],
                  int32_t ip,
                  int32_t *iv,
                  float sout[],
                  float rms,
                  float ratio,
                  float g2pass)
{
    static const int32_t kexc[25] =
    {
          8,  -16,   26, -48,  86, -162, 294, -502, 718, -728, 184, 
        672, -610, -672, 184, 728,  718, 502,  294, 162,   86,  48, 26, 16, 8
    };
    int32_t i;
    int32_t j;
    int32_t k;
    int32_t px;
    float noise[166];
    float pulse;
    float r1;
    float gain;
    float xssq;
    float sscale;
    float xy;
    float sum;
    float ssq;
    float lpi0;
    float hpi0;

    /* MAXPIT + MAXORD = 166 */
    /* Calculate history scale factor XY and scale filter state */
    /* Computing MIN */
    r1 = s->rmso_bsynz/(rms + 1.0e-6f);
    xy = min(r1, 8.0f);
    s->rmso_bsynz = rms;
    for (i = 0;  i < LPC10_ORDER;  i++)
        s->exc2[i] = s->exc2[s->ipo + i]*xy;
    s->ipo = ip;
    if (*iv == 0)
    {
        /* Generate white noise for unvoiced */
        for (i = 0;  i < ip;  i++)
            s->exc[LPC10_ORDER + i] = (float) (lpc10_random(s)/64);
        /* Impulse double excitation for plosives */
        px = (lpc10_random(s) + 32768)*(ip - 1)/65536 + LPC10_ORDER + 1;
        r1 = ratio/4.0f;
        pulse = r1*342;
        if (pulse > 2.0e3f)
            pulse = 2.0e3f;
        s->exc[px - 1] += pulse;
        s->exc[px] -= pulse;
    }
    else
    {
        sscale = sqrtf((float) ip)/6.928f;
        for (i = 0;  i < ip;  i++)
        {
            s->exc[LPC10_ORDER + i] = 0.0f;
            if (i < 25)
                s->exc[LPC10_ORDER + i] = sscale*kexc[i];
            lpi0 = s->exc[LPC10_ORDER + i];
            s->exc[LPC10_ORDER + i] = s->exc[LPC10_ORDER + i]*0.125f + s->lpi[0]*0.75f + s->lpi[1]*0.125f;
            s->lpi[1] = s->lpi[0];
            s->lpi[0] = lpi0;
        }
        for (i = 0;  i < ip;  i++)
        {
            noise[LPC10_ORDER + i] = lpc10_random(s)/64.0f;
            hpi0 = noise[LPC10_ORDER + i];
            noise[LPC10_ORDER + i] = noise[LPC10_ORDER + i]*-0.125f + s->hpi[0]*0.25f + s->hpi[1]*-0.125f;
            s->hpi[1] = s->hpi[0];
            s->hpi[0] = hpi0;
        }
        for (i = 0;  i < ip;  i++)
            s->exc[LPC10_ORDER + i] += noise[LPC10_ORDER + i];
    }
    /* Synthesis filters: */
    /* Modify the excitation with all-zero filter 1 + G*SUM */
    xssq = 0.0f;
    for (i = 0;  i < ip;  i++)
    {
        k = LPC10_ORDER + i;
        sum = 0.0f;
        for (j = 0;  j < LPC10_ORDER;  j++)
            sum += coef[j]*s->exc[k - j - 1];
        sum *= g2pass;
        s->exc2[k] = sum + s->exc[k];
    }
    /* Synthesize using the all pole filter 1/(1 - SUM) */
    for (i = 0;  i < ip;  i++)
    {
        k = LPC10_ORDER + i;
        sum = 0.0f;
        for (j = 0;  j < LPC10_ORDER;  j++)
            sum += coef[j]*s->exc2[k - j - 1];
        s->exc2[k] = sum + s->exc2[k];
        xssq += s->exc2[k]*s->exc2[k];
    }
    /* Save filter history for next epoch */
    for (i = 0;  i < LPC10_ORDER;  i++)
    {
        s->exc[i] = s->exc[ip + i];
        s->exc2[i] = s->exc2[ip + i];
    }
    /* Apply gain to match RMS */
    ssq = rms*rms*ip;
    gain = sqrtf(ssq/xssq);
    for (i = 0;  i < ip;  i++)
        sout[i] = gain*s->exc2[LPC10_ORDER + i];
}
/*- End of function --------------------------------------------------------*/

/* Synthesize a single pitch epoch */
static int pitsyn(lpc10_decode_state_t *s,
                  int voice[], 
                  int32_t *pitch,
                  float *rms,
                  float *rc,
                  int32_t ivuv[], 
                  int32_t ipiti[],
                  float *rmsi,
                  float *rci,
                  int32_t *nout,
                  float *ratio)
{
    int32_t rci_dim1;
    int32_t rci_offset;
    int32_t i1;
    int32_t i;
    int32_t j;
    int32_t vflag;
    int32_t jused;
    int32_t lsamp;
    int32_t ip;
    int32_t nl;
    int32_t ivoice;
    int32_t istart;
    float r1;
    float alrn;
    float alro;
    float yarc[10];
    float prop;
    float slope;
    float uvpit;
    float xxy;
    float msix;

    rci_dim1 = LPC10_ORDER;
    rci_offset = rci_dim1 + 1;
    rci -= rci_offset;

    if (*rms < 1.0f)
        *rms = 1.0f;
    if (s->rmso < 1.0f)
        s->rmso = 1.0f;
    uvpit = 0.0f;
    *ratio = *rms/(s->rmso + 8.0f);
    if (s->first_pitsyn)
    {
        lsamp = 0;
        ivoice = voice[1];
        if (ivoice == 0)
            *pitch = LPC10_SAMPLES_PER_FRAME/4;
        *nout = LPC10_SAMPLES_PER_FRAME / *pitch;
        s->jsamp = LPC10_SAMPLES_PER_FRAME - *nout * *pitch;

        i1 = *nout;
        for (i = 0;  i < i1;  i++)
        {
            for (j = 0;  j < LPC10_ORDER;  j++)
                rci[j + (i + 1)*rci_dim1 + 1] = rc[j];
            ivuv[i] = ivoice;
            ipiti[i] = *pitch;
            rmsi[i] = *rms;
        }
        s->first_pitsyn = FALSE;
    }
    else
    {
        vflag = 0;
        lsamp = LPC10_SAMPLES_PER_FRAME + s->jsamp;
        slope = (*pitch - s->ipito)/(float) lsamp;
        *nout = 0;
        jused = 0;
        istart = 1;
        if (voice[0] == s->ivoico  &&  voice[1] == voice[0])
        {
            if (voice[1] == 0)
            {
                /* SSUV - -   0  ,  0  ,  0 */
                *pitch = LPC10_SAMPLES_PER_FRAME/4;
                s->ipito = *pitch;
                if (*ratio > 8.0f)
                    s->rmso = *rms;
            }
            /* SSVC - -   1  ,  1  ,  1 */
            slope = (*pitch - s->ipito)/(float) lsamp;
            ivoice = voice[1];
        }
        else
        {
            if (s->ivoico != 1)
            {
                if (s->ivoico == voice[0])
                {
                    /* UV2VC2 - -  0  ,  0  ,  1 */
                    nl = lsamp - LPC10_SAMPLES_PER_FRAME/4;
                }
                else
                {
                    /* UV2VC1 - -  0  ,  1  ,  1 */
                    nl = lsamp - LPC10_SAMPLES_PER_FRAME*3/4;
                }
                ipiti[0] = nl/2;
                ipiti[1] = nl - ipiti[0];
                ivuv[0] = 0;
                ivuv[1] = 0;
                rmsi[0] = s->rmso;
                rmsi[1] = s->rmso;
                for (i = 0;  i < LPC10_ORDER;  i++)
                {
                    rci[i + rci_dim1 + 1] = s->rco[i];
                    rci[i + (rci_dim1 << 1) + 1] = s->rco[i];
                    s->rco[i] = rc[i];
                }
                slope = 0.0f;
                *nout = 2;
                s->ipito = *pitch;
                jused = nl;
                istart = nl + 1;
                ivoice = 1;
            }
            else
            {
                if (s->ivoico != voice[0])
                {
                    /* VC2UV1 - -   1  ,  0  ,  0 */
                    lsamp = LPC10_SAMPLES_PER_FRAME/4 + s->jsamp;
                }
                else
                {
                    /* VC2UV2 - -   1  ,  1  ,  0 */
                    lsamp = LPC10_SAMPLES_PER_FRAME*3/4 + s->jsamp;
                }
                for (i = 0;  i < LPC10_ORDER;  i++)
                {
                    yarc[i] = rc[i];
                    rc[i] = s->rco[i];
                }
                ivoice = 1;
                slope = 0.0f;
                vflag = 1;
            }
        }
        /* Here is the value of most variables that are used below, depending on */
        /* the values of IVOICO, VOICE(1), and VOICE(2).  VOICE(1) and VOICE(2) */
        /* are input arguments, and IVOICO is the value of VOICE(2) on the */
        /* previous call (see notes for the IF (NOUT .NE. 0) statement near the */
        /* end).  Each of these three values is either 0 or 1.  These three */
        /* values below are given as 3-bit long strings, in the order IVOICO, */
        /* VOICE(1), and VOICE(2).  It appears that the code above assumes that */
        /* the bit sequences 010 and 101 never occur, but I wonder whether a */
        /* large enough number of bit errors in the channel could cause such a */
        /* thing to happen, and if so, could that cause NOUT to ever go over 11? */

        /* Note that all of the 180 values in the table are floatly LFRAME, but */
        /* 180 has fewer characters, and it makes the table a little more */
        /* concrete.  If LFRAME is ever changed, keep this in mind.  Similarly, */
        /* 135's are 3*LFRAME/4, and 45's are LFRAME/4.  If LFRAME is not a */
        /* multiple of 4, then the 135 for NL-JSAMP is actually LFRAME-LFRAME/4, */
        /* and the 45 for NL-JSAMP is actually LFRAME-3*LFRAME/4. */

        /* Note that LSAMP-JSAMP is given as the variable.  This was just for */
        /* brevity, to avoid adding "+JSAMP" to all of the column entries. */
        /* Similarly for NL-JSAMP. */

        /* Variable    | 000  001    011,010  111       110       100,101 */
        /* ------------+-------------------------------------------------- */
        /* ISTART      | 1    NL+1   NL+1     1         1         1 */
        /* LSAMP-JSAMP | 180  180    180      180       135       45 */
        /* IPITO       | 45   PITCH  PITCH    oldPITCH  oldPITCH  oldPITCH */
        /* SLOPE       | 0    0      0        seebelow  0         0 */
        /* JUSED       | 0    NL     NL       0         0         0 */
        /* PITCH       | 45   PITCH  PITCH    PITCH     PITCH     PITCH */
        /* NL-JSAMP    | --   135    45       --        --        -- */
        /* VFLAG       | 0    0      0        0         1         1 */
        /* NOUT        | 0    2      2        0         0         0 */
        /* IVOICE      | 0    1      1        1         1         1 */

        /* while_loop  | once once   once     once      twice     twice */

        /* ISTART      | --   --     --       --        JUSED+1   JUSED+1 */
        /* LSAMP-JSAMP | --   --     --       --        180       180 */
        /* IPITO       | --   --     --       --        oldPITCH  oldPITCH */
        /* SLOPE       | --   --     --       --        0         0 */
        /* JUSED       | --   --     --       --        ??        ?? */
        /* PITCH       | --   --     --       --        PITCH     PITCH */
        /* NL-JSAMP    | --   --     --       --        --        -- */
        /* VFLAG       | --   --     --       --        0         0 */
        /* NOUT        | --   --     --       --        ??        ?? */
        /* IVOICE      | --   --     --       --        0         0 */

        /* UVPIT is always 0.0 on the first pass through the DO WHILE (TRUE)
           loop below. */

        /* The only possible non-0 value of SLOPE (in column 111) is
           (PITCH-IPITO)/FLOAT(LSAMP) */

        /* Column 101 is identical to 100.  Any good properties we can prove
           for 100 will also hold for 101.  Similarly for 010 and 011. */

        /* synths() calls this subroutine with PITCH restricted to the range 20 to
           156.  IPITO is similarly restricted to this range, after the first
           call.  IP below is also restricted to this range, given the
           definitions of IPITO, SLOPE, UVPIT, and that I is in the range ISTART
           to LSAMP. */

        for (;;)
        {
            for (i = istart;  i <= lsamp;  i++)
            {
                r1 = s->ipito + slope*i;
                ip = (int32_t) (r1 + 0.5f);
                if (uvpit != 0.0f)
                    ip = (int32_t) uvpit;
                if (ip <= i - jused)
                {
                    ++(*nout);
                    ipiti[*nout - 1] = ip;
                    *pitch = ip;
                    ivuv[*nout - 1] = ivoice;
                    jused += ip;
                    prop = (jused - ip/2)/(float) lsamp;
                    for (j = 0;  j < LPC10_ORDER;  j++)
                    {
                        alro = logf((s->rco[j] + 1)/(1 - s->rco[j]));
                        alrn = logf((rc[j] + 1)/(1 - rc[j]));
                        xxy = alro + prop*(alrn - alro);
                        xxy = expf(xxy);
                        rci[j + *nout*rci_dim1 + 1] = (xxy - 1.0f)/(xxy + 1.0f);
                    }
                    msix = logf(*rms) - logf(s->rmso);
                    msix = prop*msix;
                    msix = logf(s->rmso) + msix;
                    rmsi[*nout - 1] = expf(msix);
                }
            }
            if (vflag != 1)
                break;

            vflag = 0;
            istart = jused + 1;
            lsamp = LPC10_SAMPLES_PER_FRAME + s->jsamp;
            slope = 0.0f;
            ivoice = 0;
            uvpit = (float) ((lsamp - istart)/2);
            if (uvpit > 90.0f)
                uvpit /= 2;
            s->rmso = *rms;
            for (i = 0;  i < LPC10_ORDER;  i++)
            {
                rc[i] = yarc[i];
                s->rco[i] = yarc[i];
            }
        }
        s->jsamp = lsamp - jused;
    }
    if (*nout != 0)
    {
        s->ivoico = voice[1];
        s->ipito = *pitch;
        s->rmso = *rms;
        for (i = 0;  i < LPC10_ORDER;  i++)
            s->rco[i] = rc[i];
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void deemp(lpc10_decode_state_t *s, float x[], int len)
{
    int i;
    float r1;
    float dei0;

    for (i = 0;  i < len;  i++)
    {
        dei0 = x[i];
        r1 = x[i] - s->dei[0]*1.9998f + s->dei[1];
        x[i] = r1 + s->deo[0]*2.5f - s->deo[1]*2.0925f + s->deo[2]*0.585f;
        s->dei[1] = s->dei[0];
        s->dei[0] = dei0;
        s->deo[2] = s->deo[1];
        s->deo[1] = s->deo[0];
        s->deo[0] = x[i];
    }
}
/*- End of function --------------------------------------------------------*/

/* Convert reflection coefficients to predictor coefficients */
static float reflection_coeffs_to_predictor_coeffs(float rc[], float pc[], float gprime)
{
    float temp[10];
    float g2pass;
    int i;
    int j;

    g2pass = 1.0f;
    for (i = 0;  i < LPC10_ORDER;  i++)
        g2pass *= 1.0f - rc[i]*rc[i];
    g2pass = gprime*sqrtf(g2pass);
    pc[0] = rc[0];
    for (i = 1;  i < LPC10_ORDER;  i++)
    {
        for (j = 0;  j < i;  j++)
            temp[j] = pc[j] - rc[i]*pc[i - j - 1];
        for (j = 0;  j < i;  j++)
            pc[j] = temp[j];
        pc[i] = rc[i];
    }
    return g2pass;
}
/*- End of function --------------------------------------------------------*/

static int synths(lpc10_decode_state_t *s,
                  int voice[],
                  int32_t *pitch,
                  float *rms,
                  float *rc,
                  float speech[])
{
    int32_t i1;
    int32_t ivuv[16];
    int32_t ipiti[16];
    int32_t nout;
    int32_t i;
    int32_t j;
    float rmsi[16];
    float ratio;
    float g2pass;
    float pc[10];
    float rci[160];

    i1 = min(*pitch, 156);
    *pitch = max(i1, 20);
    for (i = 0;  i < LPC10_ORDER;  i++)
        rc[i] = max(min(rc[i], 0.99f), -0.99f);
    pitsyn(s, voice, pitch, rms, rc, ivuv, ipiti, rmsi, rci, &nout, &ratio);
    if (nout > 0)
    {
        for (j = 0;  j < nout;  j++)
        {
            /* Add synthesized speech for pitch period J to the end of s->buf. */
            g2pass = reflection_coeffs_to_predictor_coeffs(&rci[j*10], pc, 0.7f);
            bsynz(s, pc, ipiti[j], &ivuv[j], &s->buf[s->buflen], rmsi[j], ratio, g2pass);
            deemp(s, &s->buf[s->buflen], ipiti[j]);
            s->buflen += ipiti[j];
        }
        /* Copy first MAXFRM samples from BUF to output array speech (scaling them),
           and then remove them from the beginning of s->buf. */

        for (i = 0;  i < LPC10_SAMPLES_PER_FRAME;  i++)
            speech[i] = s->buf[i]/4096.0f;
        s->buflen -= LPC10_SAMPLES_PER_FRAME;
        for (i = 0;  i < s->buflen;  i++)
            s->buf[i] = s->buf[i + LPC10_SAMPLES_PER_FRAME];
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void lpc10_unpack(lpc10_frame_t *t, const uint8_t ibits[])
{
    static const int bit[10] =
    {
        2, 4, 8, 8, 8, 8, 16, 16, 16, 16
    };
    static const int iblist[53] =
    {
        13, 12, 11,  1,  2, 13, 12, 11,  1,  2,
        13, 10, 11,  2,  1, 10, 13, 12, 11, 10,
         2, 13, 12, 11, 10,  2,  1, 12,  7,  6,
         1, 10,  9,  8,  7,  4,  6,  9,  8,  7,
         5,  1,  9,  8,  4,  6,  1,  5,  9,  8,
         7,  5,  6
    };
    int32_t itab[13];
    int x;
    int i;

    /* ibits is 54 bits of LPC data ordered as follows: */
    /*     R1-0, R2-0, R3-0,  P-0,  A-0, */
    /*     R1-1, R2-1, R3-1,  P-1,  A-1, */
    /*     R1-2, R4-0, R3-2,  A-2,  P-2, R4-1, */
    /*     R1-3, R2-2, R3-3, R4-2,  A-3, */
    /*     R1-4, R2-3, R3-4, R4-3,  A-4, */
    /*      P-3, R2-4, R7-0, R8-0,  P-4, R4-4, */
    /*     R5-0, R6-0, R7-1,R10-0, R8-1, */
    /*     R5-1, R6-1, R7-2, R9-0,  P-5, */
    /*     R5-2, R6-2,R10-1, R8-2,  P-6, R9-1, */
    /*     R5-3, R6-3, R7-3, R9-2, R8-3, SYNC */

    /* Reconstruct ITAB */
    for (i = 0;  i < 13;  i++)
        itab[i] = 0;
    for (i = 0;  i < 53;  i++)
    {
        x = 52 - i;
        x = (ibits[x >> 3] >> (7 - (x & 7))) & 1;
        itab[iblist[52 - i] - 1] = (itab[iblist[52 - i] - 1] << 1) | x;
    }
    /* Sign extend the RC's */
    for (i = 0;  i < LPC10_ORDER;  i++)
    {
        if ((itab[i + 3] & bit[i]))
            itab[i + 3] -= (bit[i] << 1);
    }
    /* Restore variables */
    t->ipitch = itab[0];
    t->irms = itab[1];
    for (i = 0;  i < LPC10_ORDER;  i++)
        t->irc[i] = itab[LPC10_ORDER - 1 - i + 3];
}
/*- End of function --------------------------------------------------------*/

/* Hamming 8, 4 decoder - can correct 1 out of seven bits
   and can detect up to two errors. */

/* This subroutine is entered with an eight bit word in INPUT.  The 8th */
/* bit is parity and is stripped off.  The remaining 7 bits address the */
/* hamming 8, 4 table and the output OUTPUT from the table gives the 4 */
/* bits of corrected data.  If bit 4 is set,  no error was detected. */
/* ERRCNT is the number of errors counted. */

static int32_t hamming_84_decode(int32_t input, int *errcnt)
{
    static const uint8_t dactab[128] =
    {
        16,  0,  0,  3,  0,  5, 14,  7,  0,  9, 14, 11, 14, 13, 30, 14,
         0,  9,  2,  7,  4,  7,  7, 23,  9, 25, 10,  9, 12,  9, 14,  7,
         0,  5,  2, 11,  5, 21,  6,  5,  8, 11, 11, 27, 12,  5, 14, 11,
         2,  1, 18,  2, 12,  5,  2,  7, 12,  9,  2, 11, 28, 12, 12, 15,
         0,  3,  3, 19,  4, 13,  6,  3,  8, 13, 10,  3, 13, 29, 14, 13,
         4,  1, 10,  3, 20,  4,  4,  7, 10,  9, 26, 10,  4, 13, 10, 15,
         8,  1,  6,  3,  6,  5, 22,  6, 24,  8,  8, 11,  8, 13,  6, 15,
         1, 17 , 2,  1,  4,  1,  6, 15,  8,  1, 10, 15, 12, 15, 15, 31
    };
    int i;
    int parity;
    int32_t output;

    parity = input & 255;
    parity ^= parity >> 4;
    parity ^= parity >> 2;
    parity ^= parity >> 1;
    parity &= 1;
    i = dactab[input & 127];
    output = i & 15;
    if ((i & 16))
    {
        /* No errors detected in seven bits */
        if (parity)
            (*errcnt)++;
    }
    else
    {
        /* One or two errors detected */
        (*errcnt)++;
        if (parity == 0)
        {
            /* Two errors detected */
            (*errcnt)++;
            output = -1;
        }
    }
    return output;
}
/*- End of function --------------------------------------------------------*/

static int32_t median(int32_t d1, int32_t d2, int32_t d3)
{
    int32_t ret_val;

    ret_val = d2;
    if (d2 > d1  &&  d2 > d3)
    {
        ret_val = d1;
        if (d3 > d1)
            ret_val = d3;
    }
    else if (d2 < d1  &&  d2 < d3)
    {
        ret_val = d1;
        if (d3 < d1)
            ret_val = d3;
    }
    return ret_val;
}
/*- End of function --------------------------------------------------------*/

static void decode(lpc10_decode_state_t *s,
                   lpc10_frame_t *t,
                   int voice[],
                   int32_t *pitch,
                   float *rms,
                   float rc[])
{
    static const int32_t ivtab[32] =
    {
        24960, 24960, 24960, 24960, 25480, 25480, 25483, 25480,
        16640,  1560,  1560,  1560, 16640,  1816,  1563,  1560,
        24960, 24960, 24859, 24856, 26001, 25881, 25915, 25913,
         1560,  1560,  7800,  3640,  1561,  1561,  3643,  3641
    };
    static const float corth[32] =
    {
        32767.0f, 10.0f, 5.0f, 0.0f, 32767.0f,  8.0f, 4.0f, 0.0f,
           32.0f,  6.4f, 3.2f, 0.0f,    32.0f,  6.4f, 3.2f, 0.0f,
           32.0f, 11.2f, 6.4f, 0.0f,    32.0f, 11.2f, 6.4f, 0.0f,
           16.0f,  5.6f, 3.2f, 0.0f,    16.0f,  5.6f, 3.2f, 0.0f
    };
    static const int32_t detau[128] =
    {
          0,   0,   0,   3,   0,   3,   3,  31,
          0,   3,   3,  21,   3,   3,  29,  30,
          0,   3,   3,  20,   3,  25,  27,  26,
          3,  23,  58,  22,   3,  24,  28,   3,
          0,   3,   3,   3,   3,  39,  33,  32,
          3,  37,  35,  36,   3,  38,  34,   3,
          3,  42,  46,  44,  50,  40,  48,   3,
         54,   3,  56,   3,  52,   3,   3,   1,
          0,   3,   3, 108,   3,  78, 100, 104,
          3,  84,  92,  88, 156,  80,  96,   3,
          3,  74,  70,  72,  66,  76,  68,   3,
         62,   3,  60,   3,  64,   3,   3,   1,
          3, 116, 132, 112, 148, 152,   3,   3,
        140,   3, 136,   3, 144,   3,   3,   1,
        124, 120, 128,   3,   3,   3,   3,   1,
          3,   3,   3,   1,   3,   1,   1,   1
    };
    static const int32_t rmst[64] =
    {
        1024,  936,  856,  784,  718,  656,  600,  550,
         502,  460,  420,  384,  352,  328,  294,  270,
         246,  226,  206,  188,  172,  158,  144,  132,
         120,  110,  102,   92,   84,   78,   70,   64,
          60,   54,   50,   46,   42,   38,   34,   32,
          30,   26,   24,   22,   20,   18,   17,   16,
          15,   14,   13,   12,   11,   10,    9,    8,
           7,    6,    5,    4,    3,    2,    1,    0
    };
    static const int32_t detab7[32] =
    {
          4,  11,  18,  25,  32,  39,  46,  53,
         60,  66,  72,  77,  82,  87,  92,  96,
        101, 104, 108, 111, 114, 115, 117, 119,
        121, 122, 123, 124, 125, 126, 127, 127
    };
    static const float descl[8] =
    {
        0.6953f, 0.625f, 0.5781f, 0.5469f, 0.5312f, 0.5391f, 0.4688f, 0.3828f
    };
    static const int32_t deadd[8] =
    {
        1152, -2816, -1536, -3584, -1280, -2432, 768, -1920
    };
    static const int32_t qb[8] =
    {
        511, 511, 1023, 1023, 1023, 1023, 2047, 4095
    };
    static const int32_t nbit[10] =
    {
        8, 8, 5, 5, 4, 4, 4, 4, 3, 2
    };
    static const int32_t zrc[10] =
    {
        0, 0, 0, 0, 0, 3, 0, 2, 0, 0
    };
    static const int32_t bit[5] =
    {
        2, 4, 8, 16, 32
    };
    int32_t ipit;
    int32_t iout;
    int32_t i;
    int32_t icorf;
    int32_t index;
    int32_t ivoic;
    int32_t ixcor;
    int32_t i1;
    int32_t i2;
    int32_t i4;
    int32_t ishift;
    int32_t lsb;
    int errcnt;

    /* If no error correction, do pitch and voicing then jump to decode */
    i4 = detau[t->ipitch];
    if (!s->error_correction)
    {
        voice[0] = 1;
        voice[1] = 1;
        if (t->ipitch <= 1)
            voice[0] = 0;
        if (t->ipitch == 0  ||  t->ipitch == 2)
            voice[1] = 0;
        if (i4 <= 4)
            i4 = s->iptold;
        *pitch = i4;
        if (voice[0] == 1  &&  voice[1] == 1)
            s->iptold = *pitch;
        if (voice[0] != voice[1])
            *pitch = s->iptold;
    }
    else
    {
        /* Do error correction pitch and voicing */
        if (i4 > 4)
        {
            s->dpit[0] = i4;
            ivoic = 2;
            s->iavgp = (s->iavgp*15 + i4 + 8)/16;
        }
        else
        {
            s->dpit[0] = s->iavgp;
            ivoic = i4;
        }
        s->drms[0] = t->irms;
        for (i = 0;  i < LPC10_ORDER;  i++)
            s->drc[i][0] = t->irc[i];
        /* Determine index to IVTAB from V/UV decision */
        /* If error rate is high then use alternate table */
        index = (s->ivp2h << 4) + (s->iovoic << 2) + ivoic + 1;
        i1 = ivtab[index - 1];
        ipit = i1 & 3;
        icorf = i1 >> 3;
        if (s->erate < 2048)
            icorf /= 64;
        /* Determine error rate:  4=high    1=low */
        ixcor = 4;
        if (s->erate < 2048)
            ixcor = 3;
        if (s->erate < 1024)
            ixcor = 2;
        if (s->erate < 128)
            ixcor = 1;
        /* Voice/unvoice decision determined from bits 0 and 1 of IVTAB */
        voice[0] = icorf/2 & 1;
        voice[1] = icorf & 1;
        /* Skip decoding on first frame because present data not yet available */
        if (s->first)
        {
            s->first = FALSE;
            /* Assign PITCH a "default" value on the first call, since */
            /* otherwise it would be left uninitialized.  The two lines */
            /* below were copied from above, since it seemed like a */
            /* reasonable thing to do for the first call. */
            if (i4 <= 4)
                i4 = s->iptold;
            *pitch = i4;
        }
        else
        {
            /* If bit 4 of ICORF is set then correct RMS and RC(1) - RC(4). */
            /* Determine error rate and correct errors using a Hamming 8,4 code */
            /* during transition of unvoiced frames.  If IOUT is negative, */
            /* more than 1 error occurred, use previous frame's parameters. */
            if ((icorf & bit[3]) != 0)
            {
                errcnt = 0;
                lsb = s->drms[1] & 1;
                index = (s->drc[7][1] << 4) + s->drms[1]/2;
                iout = hamming_84_decode(index, &errcnt);
                s->drms[1] = s->drms[2];
                if (iout >= 0)
                    s->drms[1] = (iout << 1) + lsb;
                for (i = 1;  i <= 4;  i++)
                {
                    if (i == 1)
                        i1 = ((s->drc[8][1] & 7) << 1) + (s->drc[9][1] & 1);
                    else
                        i1 = s->drc[8 - i][1] & 15;
                    i2 = s->drc[4 - i][1] & 31;
                    lsb = i2 & 1;
                    index = (i1 << 4) + (i2 >> 1);
                    iout = hamming_84_decode(index, &errcnt);
                    if (iout >= 0)
                    {
                        iout = (iout << 1) + lsb;
                        if ((iout & 16) == 16)
                            iout -= 32;
                    }
                    else
                    {
                        iout = s->drc[4 - i][2];
                    }
                    s->drc[4 - i][1] = iout;
                }
                /* Determine error rate */
                s->erate = (int32_t) (s->erate*0.96875f + errcnt*102.0f);
            }
            /* Get unsmoothed RMS, RC's, and PITCH */
            t->irms = s->drms[1];
            for (i = 0;  i < LPC10_ORDER;  i++)
                t->irc[i] = s->drc[i][1];
            if (ipit == 1)
                s->dpit[1] = s->dpit[2];
            if (ipit == 3)
                s->dpit[1] = s->dpit[0];
            *pitch = s->dpit[1];
            /* If bit 2 of ICORF is set then smooth RMS and RC's, */
            if ((icorf & bit[1]) != 0)
            {
                if ((float) abs(s->drms[1] - s->drms[0]) >= corth[ixcor + 3] 
                    &&
                    (float) abs(s->drms[1] - s->drms[2]) >= corth[ixcor + 3])
                {
                    t->irms = median(s->drms[2], s->drms[1], s->drms[0]);
                }
                for (i = 0;  i < 6;  i++)
                {
                    if ((float) abs(s->drc[i][1] - s->drc[i][0]) >= corth[ixcor + ((i + 3) << 2) - 5]
                        &&
                        (float) abs(s->drc[i][1] - s->drc[i][2]) >= corth[ixcor + ((i + 3) << 2) - 5])
                    {
                        t->irc[i] = median(s->drc[i][2], s->drc[i][1], s->drc[i][0]);
                    }
                }
            }
            /* If bit 3 of ICORF is set then smooth pitch */
            if ((icorf & bit[2]) != 0)
            {
                if ((float) abs(s->dpit[1] - s->dpit[0]) >= corth[ixcor - 1] 
                    &&
                    (float) abs(s->dpit[1] - s->dpit[2]) >= corth[ixcor - 1])
                {
                    *pitch = median(s->dpit[2], s->dpit[1], s->dpit[0]);
                }
            }
            /* If bit 5 of ICORF is set then RC(5) - RC(10) are loaded with
               values so that after quantization bias is removed in decode
               the values will be zero. */
        }
        if ((icorf & bit[4]) != 0)
        {
            for (i = 4;  i < LPC10_ORDER;  i++)
                t->irc[i] = zrc[i];
        }
        /* Housekeeping  - one frame delay */
        s->iovoic = ivoic;
        s->ivp2h = voice[1];
        s->dpit[2] = s->dpit[1];
        s->dpit[1] = s->dpit[0];
        s->drms[2] = s->drms[1];
        s->drms[1] = s->drms[0];
        for (i = 0;  i < LPC10_ORDER;  i++)
        {
            s->drc[i][2] = s->drc[i][1];
            s->drc[i][1] = s->drc[i][0];
        }
    }
    /* Decode RMS */
    t->irms = rmst[(31 - t->irms)*2];
    /* Decode RC(1) and RC(2) from log-area-ratios */
    /* Protect from illegal coded value (-16) caused by bit errors */
    for (i = 0;  i < 2;  i++)
    {
        i2 = t->irc[i];
        i1 = 0;
        if (i2 < 0)
        {
            i1 = 1;
            i2 = -i2;
            if (i2 > 15)
                i2 = 0;
        }
        i2 = detab7[i2*2];
        if (i1 == 1)
            i2 = -i2;
        ishift = 15 - nbit[i];
        t->irc[i] = i2*pow_ii(2, ishift);
    }
    /* Decode RC(3)-RC(10) to sign plus 14 bits */
    for (i = 2;  i < LPC10_ORDER;  i++)
    {
        ishift = 15 - nbit[i];
        i2 = t->irc[i]*pow_ii(2, ishift) + qb[i - 2];
        t->irc[i] = (int32_t) (i2*descl[i - 2] + deadd[i - 2]);
    }
    /* Scale RMS and RC's to floats */
    *rms = (float) t->irms;
    for (i = 0;  i < LPC10_ORDER;  i++)
        rc[i] = t->irc[i]/16384.0f;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(lpc10_decode_state_t *) lpc10_decode_init(lpc10_decode_state_t *s, int error_correction)
{
    static const int16_t rand_init[] =
    {
        -21161,
         -8478,
         30892,
        -10216,
         16950
    };
    int i;
    int j;

    if (s == NULL)
    {
        if ((s = (lpc10_decode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }

    s->error_correction = error_correction;

    /* State used by function decode */
    s->iptold = 60;
    s->first = TRUE;
    s->ivp2h = 0;
    s->iovoic = 0;
    s->iavgp = 60;
    s->erate = 0;
    for (i = 0;  i < 3;  i++)
    {
        for (j = 0;  j < 10;  j++)
            s->drc[j][i] = 0;
        s->dpit[i] = 0;
        s->drms[i] = 0;
    }

    /* State used by function synths */
    for (i = 0;  i < 360;  i++)
        s->buf[i] = 0.0f;
    s->buflen = LPC10_SAMPLES_PER_FRAME;

    /* State used by function pitsyn */
    s->rmso = 1.0f;
    s->first_pitsyn = TRUE;

    /* State used by function bsynz */
    s->ipo = 0;
    for (i = 0;  i < 166;  i++)
    {
        s->exc[i] = 0.0f;
        s->exc2[i] = 0.0f;
    }
    for (i = 0;  i < 3;  i++)
    {
        s->lpi[i] = 0.0f;
        s->hpi[i] = 0.0f;
    }
    s->rmso_bsynz = 0.0f;

    /* State used by function lpc10_random */
    s->j = 1;
    s->k = 4;
    for (i = 0;  i < 5;  i++)
        s->y[i] = rand_init[i];

    /* State used by function deemp */
    for (i = 0;  i < 2;  i++)
        s->dei[i] = 0.0f;
    for (i = 0;  i < 3;  i++)
        s->deo[i] = 0.0f;
    
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lpc10_decode_release(lpc10_decode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lpc10_decode_free(lpc10_decode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lpc10_decode(lpc10_decode_state_t *s, int16_t amp[], const uint8_t code[], int len)
{
    int voice[2];
    int32_t pitch;
    float speech[LPC10_SAMPLES_PER_FRAME];
    float rc[LPC10_ORDER];
    lpc10_frame_t frame;
    float rms;
    int i;
    int j;
    int base;

    /* Decode 54 bits in 7 bytes to LPC10_SAMPLES_PER_FRAME speech samples. */
    len /= 7;
    for (i = 0;  i < len;  i++)
    {
        lpc10_unpack(&frame, &code[i*7]);
        decode(s, &frame, voice, &pitch, &rms, rc);
        synths(s, voice, &pitch, &rms, rc, speech);
        base = i*LPC10_SAMPLES_PER_FRAME;
        for (j = 0;  j < LPC10_SAMPLES_PER_FRAME;  j++)
            amp[base + j] = (int16_t) lfastrintf(32768.0f*speech[j]);
    }

    return len*LPC10_SAMPLES_PER_FRAME;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
