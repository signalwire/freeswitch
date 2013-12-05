/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g726.c - The ITU G.726 codec.
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
 * Based on G.721/G.723 code which is:
 *
 * This source code is a product of Sun Microsystems, Inc. and is provided
 * for unrestricted use.  Users may copy or modify this source code without
 * charge.
 *
 * SUN SOURCE CODE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING
 * THE WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun source code is provided with no support and without any obligation on
 * the part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS SOFTWARE
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <memory.h>
#include <stdlib.h>
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
#include "spandsp/alloc.h"
#include "spandsp/bitstream.h"
#include "spandsp/bit_operations.h"
#include "spandsp/g711.h"
#include "spandsp/g726.h"

#include "spandsp/private/bitstream.h"
#include "spandsp/private/g726.h"

/*
 * Maps G.726_16 code word to reconstructed scale factor normalized log
 * magnitude values.
 */
static const int g726_16_dqlntab[4] =
{
    116, 365, 365, 116
};

/* Maps G.726_16 code word to log of scale factor multiplier. */
static const int g726_16_witab[4] =
{
    -704, 14048, 14048, -704
};

/*
 * Maps G.726_16 code words to a set of values whose long and short
 * term averages are computed and then compared to give an indication
 * how stationary (steady state) the signal is.
 */
static const int g726_16_fitab[4] =
{
    0x000, 0xE00, 0xE00, 0x000
};

static const int qtab_726_16[1] =
{
    261
};

/*
 * Maps G.726_24 code word to reconstructed scale factor normalized log
 * magnitude values.
 */
static const int g726_24_dqlntab[8] =
{
    -2048, 135, 273, 373, 373, 273, 135, -2048
};

/* Maps G.726_24 code word to log of scale factor multiplier. */
static const int g726_24_witab[8] =
{
    -128, 960, 4384, 18624, 18624, 4384, 960, -128
};

/*
 * Maps G.726_24 code words to a set of values whose long and short
 * term averages are computed and then compared to give an indication
 * how stationary (steady state) the signal is.
 */
static const int g726_24_fitab[8] =
{
    0x000, 0x200, 0x400, 0xE00, 0xE00, 0x400, 0x200, 0x000
};

static const int qtab_726_24[3] =
{
    8, 218, 331
};

/*
 * Maps G.726_32 code word to reconstructed scale factor normalized log
 * magnitude values.
 */
static const int g726_32_dqlntab[16] =
{
    -2048,   4, 135, 213, 273, 323, 373,   425,
      425, 373, 323, 273, 213, 135,   4, -2048
};

/* Maps G.726_32 code word to log of scale factor multiplier. */
static const int g726_32_witab[16] =
{
     -384,   576,  1312,  2048,  3584,  6336, 11360, 35904,
    35904, 11360,  6336,  3584,  2048,  1312,   576,  -384
};

/*
 * Maps G.726_32 code words to a set of values whose long and short
 * term averages are computed and then compared to give an indication
 * how stationary (steady state) the signal is.
 */
static const int g726_32_fitab[16] =
{
    0x000, 0x000, 0x000, 0x200, 0x200, 0x200, 0x600, 0xE00,
    0xE00, 0x600, 0x200, 0x200, 0x200, 0x000, 0x000, 0x000
};

static const int qtab_726_32[7] =
{
    -124, 80, 178, 246, 300, 349, 400
};

/*
 * Maps G.726_40 code word to ructeconstructed scale factor normalized log
 * magnitude values.
 */
static const int g726_40_dqlntab[32] =
{
    -2048, -66, 28, 104, 169, 224, 274, 318,
      358, 395, 429, 459, 488, 514, 539, 566,
      566, 539, 514, 488, 459, 429, 395, 358,
      318, 274, 224, 169, 104, 28, -66, -2048
};

/* Maps G.726_40 code word to log of scale factor multiplier. */
static const int g726_40_witab[32] =
{
      448,   448,   768,  1248,  1280,  1312,  1856,  3200,
     4512,  5728,  7008,  8960, 11456, 14080, 16928, 22272,
    22272, 16928, 14080, 11456,  8960,  7008,  5728,  4512,
     3200,  1856,  1312,  1280,  1248,   768,   448,   448
};

/*
 * Maps G.726_40 code words to a set of values whose long and short
 * term averages are computed and then compared to give an indication
 * how stationary (steady state) the signal is.
 */
static const int g726_40_fitab[32] =
{
    0x000, 0x000, 0x000, 0x000, 0x000, 0x200, 0x200, 0x200,
    0x200, 0x200, 0x400, 0x600, 0x800, 0xA00, 0xC00, 0xC00,
    0xC00, 0xC00, 0xA00, 0x800, 0x600, 0x400, 0x200, 0x200,
    0x200, 0x200, 0x200, 0x000, 0x000, 0x000, 0x000, 0x000
};

static const int qtab_726_40[15] =
{
    -122, -16,  68, 139, 198, 250, 298, 339,
     378, 413, 445, 475, 502, 528, 553
};

/*
 * returns the integer product of the 14-bit integer "an" and
 * "floating point" representation (4-bit exponent, 6-bit mantessa) "srn".
 */
static int16_t fmult(int16_t an, int16_t srn)
{
    int16_t anmag;
    int16_t anexp;
    int16_t anmant;
    int16_t wanexp;
    int16_t wanmant;
    int16_t retval;

    anmag = (an > 0)  ?  an  :  ((-an) & 0x1FFF);
    anexp = (int16_t) (top_bit(anmag) - 5);
    anmant = (anmag == 0)  ?  32  :  (anexp >= 0)  ?  (anmag >> anexp)  :  (anmag << -anexp);
    wanexp = anexp + ((srn >> 6) & 0xF) - 13;

    wanmant = (anmant*(srn & 0x3F) + 0x30) >> 4;
    retval = (wanexp >= 0)  ?  ((wanmant << wanexp) & 0x7FFF)  :  (wanmant >> -wanexp);

    return (((an ^ srn) < 0)  ?  -retval  :  retval);
}
/*- End of function --------------------------------------------------------*/

/*
 * Compute the estimated signal from the 6-zero predictor.
 */
static __inline__ int16_t predictor_zero(g726_state_t *s)
{
    int i;
    int sezi;

    sezi = fmult(s->b[0] >> 2, s->dq[0]);
    /* ACCUM */
    for (i = 1;  i < 6;  i++)
        sezi += fmult(s->b[i] >> 2, s->dq[i]);
    return (int16_t) sezi;
}
/*- End of function --------------------------------------------------------*/

/*
 * Computes the estimated signal from the 2-pole predictor.
 */
static __inline__ int16_t predictor_pole(g726_state_t *s)
{
    return (fmult(s->a[1] >> 2, s->sr[1]) + fmult(s->a[0] >> 2, s->sr[0]));
}
/*- End of function --------------------------------------------------------*/

/*
 * Computes the quantization step size of the adaptive quantizer.
 */
static int step_size(g726_state_t *s)
{
    int y;
    int dif;
    int al;

    if (s->ap >= 256)
        return s->yu;
    y = s->yl >> 6;
    dif = s->yu - y;
    al = s->ap >> 2;
    if (dif > 0)
        y += (dif*al) >> 6;
    else if (dif < 0)
        y += (dif*al + 0x3F) >> 6;
    return y;
}
/*- End of function --------------------------------------------------------*/

/*
 * Given a raw sample, 'd', of the difference signal and a
 * quantization step size scale factor, 'y', this routine returns the
 * ADPCM codeword to which that sample gets quantized.  The step
 * size scale factor division operation is done in the log base 2 domain
 * as a subtraction.
 */
static int16_t quantize(int d,                  /* Raw difference signal sample */
                        int y,                  /* Step size multiplier */
                        const int table[],     /* quantization table */
                        int quantizer_states)   /* table size of int16_t integers */
{
    int16_t dqm;    /* Magnitude of 'd' */
    int16_t exp;    /* Integer part of base 2 log of 'd' */
    int16_t mant;   /* Fractional part of base 2 log */
    int16_t dl;     /* Log of magnitude of 'd' */
    int16_t dln;    /* Step size scale factor normalized log */
    int i;
    int size;

    /*
     * LOG
     *
     * Compute base 2 log of 'd', and store in 'dl'.
     */
    dqm = (int16_t) abs(d);
    exp = (int16_t) (top_bit(dqm >> 1) + 1);
    /* Fractional portion. */
    mant = ((dqm << 7) >> exp) & 0x7F;
    dl = (exp << 7) + mant;

    /*
     * SUBTB
     *
     * "Divide" by step size multiplier.
     */
    dln = dl - (int16_t) (y >> 2);

    /*
     * QUAN
     *
     * Search for codword i for 'dln'.
     */
    size = (quantizer_states - 1) >> 1;
    for (i = 0;  i < size;  i++)
    {
        if (dln < table[i])
            break;
    }
    if (d < 0)
    {
        /* Take 1's complement of i */
        return (int16_t) ((size << 1) + 1 - i);
    }
    if (i == 0  &&  (quantizer_states & 1))
    {
        /* Zero is only valid if there are an even number of states, so
           take the 1's complement if the code is zero. */
        return (int16_t) quantizer_states;
    }
    return (int16_t) i;
}
/*- End of function --------------------------------------------------------*/

/*
 * Returns reconstructed difference signal 'dq' obtained from
 * codeword 'i' and quantization step size scale factor 'y'.
 * Multiplication is performed in log base 2 domain as addition.
 */
static int16_t reconstruct(int sign,    /* 0 for non-negative value */
                           int dqln,    /* G.72x codeword */
                           int y)       /* Step size multiplier */
{
    int16_t dql;    /* Log of 'dq' magnitude */
    int16_t dex;    /* Integer part of log */
    int16_t dqt;
    int16_t dq;     /* Reconstructed difference signal sample */

    dql = (int16_t) (dqln + (y >> 2));  /* ADDA */

    if (dql < 0)
        return ((sign)  ?  -0x8000  :  0);
    /* ANTILOG */
    dex = (dql >> 7) & 15;
    dqt = 128 + (dql & 127);
    dq = (dqt << 7) >> (14 - dex);
    return ((sign)  ?  (dq - 0x8000)  :  dq);
}
/*- End of function --------------------------------------------------------*/

/*
 * updates the state variables for each output code
 */
static void update(g726_state_t *s,
                   int y,       /* quantizer step size */
                   int wi,      /* scale factor multiplier */
                   int fi,      /* for long/short term energies */
                   int dq,      /* quantized prediction difference */
                   int sr,      /* reconstructed signal */
                   int dqsez)   /* difference from 2-pole predictor */
{
    int16_t mag;
    int16_t exp;
    int16_t a2p;        /* LIMC */
    int16_t a1ul;       /* UPA1 */
    int16_t pks1;       /* UPA2 */
    int16_t fa1;
    int16_t ylint;
    int16_t dqthr;
    int16_t ylfrac;
    int16_t thr;
    int16_t pk0;
    int i;
    bool tr;

    a2p = 0;
    /* Needed in updating predictor poles */
    pk0 = (dqsez < 0)  ?  1  :  0;

    /* prediction difference magnitude */
    mag = (int16_t) (dq & 0x7FFF);
    /* TRANS */
    ylint = (int16_t) (s->yl >> 15);            /* exponent part of yl */
    ylfrac = (int16_t) ((s->yl >> 10) & 0x1F);  /* fractional part of yl */
    /* Limit threshold to 31 << 10 */
    thr = (ylint > 9)  ?  (31 << 10)  :  ((32 + ylfrac) << ylint);
    dqthr = (thr + (thr >> 1)) >> 1;            /* dqthr = 0.75 * thr */
    if (!s->td)                                 /* signal supposed voice */
        tr = false;
    else if (mag <= dqthr)                      /* supposed data, but small mag */
        tr = false;                             /* treated as voice */
    else                                        /* signal is data (modem) */
        tr = true;

    /*
     * Quantizer scale factor adaptation.
     */

    /* FUNCTW & FILTD & DELAY */
    /* update non-steady state step size multiplier */
    s->yu = (int16_t) (y + ((wi - y) >> 5));

    /* LIMB */
    if (s->yu < 544)
        s->yu = 544;
    else if (s->yu > 5120)
        s->yu = 5120;

    /* FILTE & DELAY */
    /* update steady state step size multiplier */
    s->yl += s->yu + ((-s->yl) >> 6);

    /*
     * Adaptive predictor coefficients.
     */
    if (tr)
    {
        /* Reset the a's and b's for a modem signal */
        s->a[0] = 0;
        s->a[1] = 0;
        s->b[0] = 0;
        s->b[1] = 0;
        s->b[2] = 0;
        s->b[3] = 0;
        s->b[4] = 0;
        s->b[5] = 0;
    }
    else
    {
        /* Update the a's and b's */
        /* UPA2 */
        pks1 = pk0 ^ s->pk[0];

        /* Update predictor pole a[1] */
        a2p = s->a[1] - (s->a[1] >> 7);
        if (dqsez != 0)
        {
            fa1 = (pks1)  ?  s->a[0]  :  -s->a[0];
            /* a2p = function of fa1 */
            if (fa1 < -8191)
                a2p -= 0x100;
            else if (fa1 > 8191)
                a2p += 0xFF;
            else
                a2p += fa1 >> 5;

            if (pk0 ^ s->pk[1])
            {
                /* LIMC */
                if (a2p <= -12160)
                    a2p = -12288;
                else if (a2p >= 12416)
                    a2p = 12288;
                else
                    a2p -= 0x80;
            }
            else if (a2p <= -12416)
                a2p = -12288;
            else if (a2p >= 12160)
                a2p = 12288;
            else
                a2p += 0x80;
        }

        /* TRIGB & DELAY */
        s->a[1] = a2p;

        /* UPA1 */
        /* Update predictor pole a[0] */
        s->a[0] -= s->a[0] >> 8;
        if (dqsez != 0)
        {
            if (pks1 == 0)
                s->a[0] += 192;
            else
                s->a[0] -= 192;
        }
        /* LIMD */
        a1ul = 15360 - a2p;
        if (s->a[0] < -a1ul)
            s->a[0] = -a1ul;
        else if (s->a[0] > a1ul)
            s->a[0] = a1ul;

        /* UPB : update predictor zeros b[6] */
        for (i = 0;  i < 6;  i++)
        {
            /* Distinguish 40Kbps mode from the others */
            s->b[i] -= s->b[i] >> ((s->bits_per_sample == 5)  ?  9  :  8);
            if (dq & 0x7FFF)
            {
                /* XOR */
                if ((dq ^ s->dq[i]) >= 0)
                    s->b[i] += 128;
                else
                    s->b[i] -= 128;
            }
        }
    }

    for (i = 5;  i > 0;  i--)
        s->dq[i] = s->dq[i - 1];
    /* FLOAT A : convert dq[0] to 4-bit exp, 6-bit mantissa f.p. */
    if (mag == 0)
    {
        s->dq[0] = (dq >= 0)  ?  0x20  :  0xFC20;
    }
    else
    {
        exp = (int16_t) (top_bit(mag) + 1);
        s->dq[0] = (dq >= 0)
                 ?  ((exp << 6) + ((mag << 6) >> exp))
                 :  ((exp << 6) + ((mag << 6) >> exp) - 0x400);
    }

    s->sr[1] = s->sr[0];
    /* FLOAT B : convert sr to 4-bit exp., 6-bit mantissa f.p. */
    if (sr == 0)
    {
        s->sr[0] = 0x20;
    }
    else if (sr > 0)
    {
        exp = (int16_t) (top_bit(sr) + 1);
        s->sr[0] = (int16_t) ((exp << 6) + ((sr << 6) >> exp));
    }
    else if (sr > -32768)
    {
        mag = (int16_t) -sr;
        exp = (int16_t) (top_bit(mag) + 1);
        s->sr[0] =  (exp << 6) + ((mag << 6) >> exp) - 0x400;
    }
    else
    {
        s->sr[0] = (uint16_t) 0xFC20;
    }

    /* DELAY A */
    s->pk[1] = s->pk[0];
    s->pk[0] = pk0;

    /* TONE */
    if (tr)                 /* this sample has been treated as data */
        s->td = false;      /* next one will be treated as voice */
    else if (a2p < -11776)  /* small sample-to-sample correlation */
        s->td = true;       /* signal may be data */
    else                    /* signal is voice */
        s->td = false;

    /* Adaptation speed control. */
    /* FILTA */
    s->dms += ((int16_t) fi - s->dms) >> 5;
    /* FILTB */
    s->dml += (((int16_t) (fi << 2) - s->dml) >> 7);

    if (tr)
        s->ap = 256;
    else if (y < 1536)                      /* SUBTC */
        s->ap += (0x200 - s->ap) >> 4;
    else if (s->td)
        s->ap += (0x200 - s->ap) >> 4;
    else if (abs((s->dms << 2) - s->dml) >= (s->dml >> 3))
        s->ap += (0x200 - s->ap) >> 4;
    else
        s->ap += (-s->ap) >> 4;
}
/*- End of function --------------------------------------------------------*/

static int16_t tandem_adjust_alaw(int16_t sr,   /* decoder output linear PCM sample */
                                  int se,       /* predictor estimate sample */
                                  int y,        /* quantizer step size */
                                  int i,        /* decoder input code */
                                  int sign,
                                  const int qtab[],
                                  int quantizer_states)
{
    uint8_t sp; /* A-law compressed 8-bit code */
    int16_t dx; /* prediction error */
    int id;     /* quantized prediction error */
    int sd;     /* adjusted A-law decoded sample value */

    if (sr <= -32768)
        sr = -1;
    sp = linear_to_alaw((sr >> 1) << 3);
    /* 16-bit prediction error */
    dx = (int16_t) ((alaw_to_linear(sp) >> 2) - se);
    id = quantize(dx, y, qtab, quantizer_states);
    if (id == i)
    {
        /* No adjustment of sp required */
        return (int16_t) sp;
    }
    /* sp adjustment needed */
    /* ADPCM codes : 8, 9, ... F, 0, 1, ... , 6, 7 */
    /* 2's complement to biased unsigned */
    if ((id ^ sign) > (i ^ sign))
    {
        /* sp adjusted to next lower value */
        if (sp & 0x80)
            sd = (sp == 0xD5)  ?  0x55  :  (((sp ^ 0x55) - 1) ^ 0x55);
        else
            sd = (sp == 0x2A)  ?  0x2A  :  (((sp ^ 0x55) + 1) ^ 0x55);
    }
    else
    {
        /* sp adjusted to next higher value */
        if (sp & 0x80)
            sd = (sp == 0xAA)  ?  0xAA  :  (((sp ^ 0x55) + 1) ^ 0x55);
        else
            sd = (sp == 0x55)  ?  0xD5  :  (((sp ^ 0x55) - 1) ^ 0x55);
    }
    return (int16_t) sd;
}
/*- End of function --------------------------------------------------------*/

static int16_t tandem_adjust_ulaw(int16_t sr,   /* decoder output linear PCM sample */
                                  int se,       /* predictor estimate sample */
                                  int y,        /* quantizer step size */
                                  int i,        /* decoder input code */
                                  int sign,
                                  const int qtab[],
                                  int quantizer_states)
{
    uint8_t sp; /* u-law compressed 8-bit code */
    int16_t dx; /* prediction error */
    int id;     /* quantized prediction error */
    int sd;     /* adjusted u-law decoded sample value */

    if (sr <= -32768)
        sr = 0;
    sp = linear_to_ulaw(sr << 2);
    /* 16-bit prediction error */
    dx = (int16_t) ((ulaw_to_linear(sp) >> 2) - se);
    id = quantize(dx, y, qtab, quantizer_states);
    if (id == i)
    {
        /* No adjustment of sp required. */
        return (int16_t) sp;
    }
    /* ADPCM codes : 8, 9, ... F, 0, 1, ... , 6, 7 */
    /* 2's complement to biased unsigned */
    if ((id ^ sign) > (i ^ sign))
    {
        /* sp adjusted to next lower value */
        if (sp & 0x80)
            sd = (sp == 0xFF)  ?  0x7E  :  (sp + 1);
        else
            sd = (sp == 0x00)  ?  0x00  :  (sp - 1);
    }
    else
    {
        /* sp adjusted to next higher value */
        if (sp & 0x80)
            sd = (sp == 0x80)  ?  0x80  :  (sp - 1);
        else
            sd = (sp == 0x7F)  ?  0xFE  :  (sp + 1);
    }
    return (int16_t) sd;
}
/*- End of function --------------------------------------------------------*/

/*
 * Encodes a linear PCM, A-law or u-law input sample and returns its 3-bit code.
 */
static uint8_t g726_16_encoder(g726_state_t *s, int16_t amp)
{
    int y;
    int16_t sei;
    int16_t sezi;
    int16_t se;
    int16_t d;
    int16_t sr;
    int16_t dqsez;
    int16_t dq;
    int16_t i;

    sezi = predictor_zero(s);
    sei = sezi + predictor_pole(s);
    se = sei >> 1;
    d = amp - se;

    /* Quantize prediction difference */
    y = step_size(s);
    i = quantize(d, y, qtab_726_16, 4);
    dq = reconstruct(i & 2, g726_16_dqlntab[i], y);

    /* Reconstruct the signal */
    sr = (dq < 0)  ?  (se - (dq & 0x3FFF))  :  (se + dq);

    /* Pole prediction difference */
    dqsez = sr + (sezi >> 1) - se;

    update(s, y, g726_16_witab[i], g726_16_fitab[i], dq, sr, dqsez);
    return (uint8_t) i;
}
/*- End of function --------------------------------------------------------*/

/*
 * Decodes a 2-bit CCITT G.726_16 ADPCM code and returns
 * the resulting 16-bit linear PCM, A-law or u-law sample value.
 */
static int16_t g726_16_decoder(g726_state_t *s, uint8_t code)
{
    int16_t sezi;
    int16_t sei;
    int16_t se;
    int16_t sr;
    int16_t dq;
    int16_t dqsez;
    int y;

    /* Mask to get proper bits */
    code &= 0x03;
    sezi = predictor_zero(s);
    sei = sezi + predictor_pole(s);

    y = step_size(s);
    dq = reconstruct(code & 2, g726_16_dqlntab[code], y);

    /* Reconstruct the signal */
    se = sei >> 1;
    sr = (dq < 0)  ?  (se - (dq & 0x3FFF))  :  (se + dq);

    /* Pole prediction difference */
    dqsez = sr + (sezi >> 1) - se;

    update(s, y, g726_16_witab[code], g726_16_fitab[code], dq, sr, dqsez);

    switch (s->ext_coding)
    {
    case G726_ENCODING_ALAW:
        return tandem_adjust_alaw(sr, se, y, code, 2, qtab_726_16, 4);
    case G726_ENCODING_ULAW:
        return tandem_adjust_ulaw(sr, se, y, code, 2, qtab_726_16, 4);
    }
    return (sr << 2);
}
/*- End of function --------------------------------------------------------*/

/*
 * Encodes a linear PCM, A-law or u-law input sample and returns its 3-bit code.
 */
static uint8_t g726_24_encoder(g726_state_t *s, int16_t amp)
{
    int16_t sei;
    int16_t sezi;
    int16_t se;
    int16_t d;
    int16_t sr;
    int16_t dqsez;
    int16_t dq;
    int16_t i;
    int y;

    sezi = predictor_zero(s);
    sei = sezi + predictor_pole(s);
    se = sei >> 1;
    d = amp - se;

    /* Quantize prediction difference */
    y = step_size(s);
    i = quantize(d, y, qtab_726_24, 7);
    dq = reconstruct(i & 4, g726_24_dqlntab[i], y);

    /* Reconstruct the signal */
    sr = (dq < 0)  ?  (se - (dq & 0x3FFF))  :  (se + dq);

    /* Pole prediction difference */
    dqsez = sr + (sezi >> 1) - se;

    update(s, y, g726_24_witab[i], g726_24_fitab[i], dq, sr, dqsez);
    return (uint8_t) i;
}
/*- End of function --------------------------------------------------------*/

/*
 * Decodes a 3-bit CCITT G.726_24 ADPCM code and returns
 * the resulting 16-bit linear PCM, A-law or u-law sample value.
 */
static int16_t g726_24_decoder(g726_state_t *s, uint8_t code)
{
    int16_t sezi;
    int16_t sei;
    int16_t se;
    int16_t sr;
    int16_t dq;
    int16_t dqsez;
    int y;

    /* Mask to get proper bits */
    code &= 0x07;
    sezi = predictor_zero(s);
    sei = sezi + predictor_pole(s);

    y = step_size(s);
    dq = reconstruct(code & 4, g726_24_dqlntab[code], y);

    /* Reconstruct the signal */
    se = sei >> 1;
    sr = (dq < 0)  ?  (se - (dq & 0x3FFF))  :  (se + dq);

    /* Pole prediction difference */
    dqsez = sr + (sezi >> 1) - se;

    update(s, y, g726_24_witab[code], g726_24_fitab[code], dq, sr, dqsez);

    switch (s->ext_coding)
    {
    case G726_ENCODING_ALAW:
        return tandem_adjust_alaw(sr, se, y, code, 4, qtab_726_24, 7);
    case G726_ENCODING_ULAW:
        return tandem_adjust_ulaw(sr, se, y, code, 4, qtab_726_24, 7);
    }
    return (sr << 2);
}
/*- End of function --------------------------------------------------------*/

/*
 * Encodes a linear input sample and returns its 4-bit code.
 */
static uint8_t g726_32_encoder(g726_state_t *s, int16_t amp)
{
    int16_t sei;
    int16_t sezi;
    int16_t se;
    int16_t d;
    int16_t sr;
    int16_t dqsez;
    int16_t dq;
    int16_t i;
    int y;

    sezi = predictor_zero(s);
    sei = sezi + predictor_pole(s);
    se = sei >> 1;
    d = amp - se;

    /* Quantize the prediction difference */
    y = step_size(s);
    i = quantize(d, y, qtab_726_32, 15);
    dq = reconstruct(i & 8, g726_32_dqlntab[i], y);

    /* Reconstruct the signal */
    sr = (dq < 0)  ?  (se - (dq & 0x3FFF))  :  (se + dq);

    /* Pole prediction difference */
    dqsez = sr + (sezi >> 1) - se;

    update(s, y, g726_32_witab[i], g726_32_fitab[i], dq, sr, dqsez);
    return (uint8_t) i;
}
/*- End of function --------------------------------------------------------*/

/*
 * Decodes a 4-bit CCITT G.726_32 ADPCM code and returns
 * the resulting 16-bit linear PCM, A-law or u-law sample value.
 */
static int16_t g726_32_decoder(g726_state_t *s, uint8_t code)
{
    int16_t sezi;
    int16_t sei;
    int16_t se;
    int16_t sr;
    int16_t dq;
    int16_t dqsez;
    int y;

    /* Mask to get proper bits */
    code &= 0x0F;
    sezi = predictor_zero(s);
    sei = sezi + predictor_pole(s);

    y = step_size(s);
    dq = reconstruct(code & 8, g726_32_dqlntab[code], y);

    /* Reconstruct the signal */
    se = sei >> 1;
    sr = (dq < 0)  ?  (se - (dq & 0x3FFF))  :  (se + dq);

    /* Pole prediction difference */
    dqsez = sr + (sezi >> 1) - se;

    update(s, y, g726_32_witab[code], g726_32_fitab[code], dq, sr, dqsez);

    switch (s->ext_coding)
    {
    case G726_ENCODING_ALAW:
        return tandem_adjust_alaw(sr, se, y, code, 8, qtab_726_32, 15);
    case G726_ENCODING_ULAW:
        return tandem_adjust_ulaw(sr, se, y, code, 8, qtab_726_32, 15);
    }
    return (sr << 2);
}
/*- End of function --------------------------------------------------------*/

/*
 * Encodes a 16-bit linear PCM, A-law or u-law input sample and retuens
 * the resulting 5-bit CCITT G.726 40Kbps code.
 */
static uint8_t g726_40_encoder(g726_state_t *s, int16_t amp)
{
    int16_t sei;
    int16_t sezi;
    int16_t se;
    int16_t d;
    int16_t sr;
    int16_t dqsez;
    int16_t dq;
    int16_t i;
    int y;

    sezi = predictor_zero(s);
    sei = sezi + predictor_pole(s);
    se = sei >> 1;
    d = amp - se;

    /* Quantize prediction difference */
    y = step_size(s);
    i = quantize(d, y, qtab_726_40, 31);
    dq = reconstruct(i & 0x10, g726_40_dqlntab[i], y);

    /* Reconstruct the signal */
    sr = (dq < 0)  ?  (se - (dq & 0x7FFF))  :  (se + dq);

    /* Pole prediction difference */
    dqsez = sr + (sezi >> 1) - se;

    update(s, y, g726_40_witab[i], g726_40_fitab[i], dq, sr, dqsez);
    return (uint8_t) i;
}
/*- End of function --------------------------------------------------------*/

/*
 * Decodes a 5-bit CCITT G.726 40Kbps code and returns
 * the resulting 16-bit linear PCM, A-law or u-law sample value.
 */
static int16_t g726_40_decoder(g726_state_t *s, uint8_t code)
{
    int16_t sezi;
    int16_t sei;
    int16_t se;
    int16_t sr;
    int16_t dq;
    int16_t dqsez;
    int y;

    /* Mask to get proper bits */
    code &= 0x1F;
    sezi = predictor_zero(s);
    sei = sezi + predictor_pole(s);

    y = step_size(s);
    dq = reconstruct(code & 0x10, g726_40_dqlntab[code], y);

    /* Reconstruct the signal */
    se = sei >> 1;
    sr = (dq < 0)  ?  (se - (dq & 0x7FFF))  :  (se + dq);

    /* Pole prediction difference */
    dqsez = sr + (sezi >> 1) - se;

    update(s, y, g726_40_witab[code], g726_40_fitab[code], dq, sr, dqsez);

    switch (s->ext_coding)
    {
    case G726_ENCODING_ALAW:
        return tandem_adjust_alaw(sr, se, y, code, 0x10, qtab_726_40, 31);
    case G726_ENCODING_ULAW:
        return tandem_adjust_ulaw(sr, se, y, code, 0x10, qtab_726_40, 31);
    }
    return (sr << 2);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(g726_state_t *) g726_init(g726_state_t *s, int bit_rate, int ext_coding, int packing)
{
    int i;

    if (bit_rate != 16000  &&  bit_rate != 24000  &&  bit_rate != 32000  &&  bit_rate != 40000)
        return NULL;
    if (s == NULL)
    {
        if ((s = (g726_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    s->yl = 34816;
    s->yu = 544;
    s->dms = 0;
    s->dml = 0;
    s->ap = 0;
    s->rate = bit_rate;
    s->ext_coding = ext_coding;
    s->packing = packing;
    for (i = 0; i < 2; i++)
    {
        s->a[i] = 0;
        s->pk[i] = 0;
        s->sr[i] = 32;
    }
    for (i = 0; i < 6; i++)
    {
        s->b[i] = 0;
        s->dq[i] = 32;
    }
    s->td = false;
    switch (bit_rate)
    {
    case 16000:
        s->enc_func = g726_16_encoder;
        s->dec_func = g726_16_decoder;
        s->bits_per_sample = 2;
        break;
    case 24000:
        s->enc_func = g726_24_encoder;
        s->dec_func = g726_24_decoder;
        s->bits_per_sample = 3;
        break;
    case 32000:
    default:
        s->enc_func = g726_32_encoder;
        s->dec_func = g726_32_decoder;
        s->bits_per_sample = 4;
        break;
    case 40000:
        s->enc_func = g726_40_encoder;
        s->dec_func = g726_40_decoder;
        s->bits_per_sample = 5;
        break;
    }
    bitstream_init(&s->bs, (s->packing != G726_PACKING_LEFT));
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g726_release(g726_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g726_free(g726_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g726_decode(g726_state_t *s,
                              int16_t amp[],
                              const uint8_t g726_data[],
                              int g726_bytes)
{
    int i;
    int samples;
    uint8_t code;
    int sl;

    for (samples = i = 0;  ;  )
    {
        if (s->packing != G726_PACKING_NONE)
        {
            /* Unpack the code bits */
            if (s->packing != G726_PACKING_LEFT)
            {
                if (s->bs.residue < s->bits_per_sample)
                {
                    if (i >= g726_bytes)
                        break;
                    s->bs.bitstream |= (g726_data[i++] << s->bs.residue);
                    s->bs.residue += 8;
                }
                code = (uint8_t) (s->bs.bitstream & ((1 << s->bits_per_sample) - 1));
                s->bs.bitstream >>= s->bits_per_sample;
            }
            else
            {
                if (s->bs.residue < s->bits_per_sample)
                {
                    if (i >= g726_bytes)
                        break;
                    s->bs.bitstream = (s->bs.bitstream << 8) | g726_data[i++];
                    s->bs.residue += 8;
                }
                code = (uint8_t) ((s->bs.bitstream >> (s->bs.residue - s->bits_per_sample)) & ((1 << s->bits_per_sample) - 1));
            }
            s->bs.residue -= s->bits_per_sample;
        }
        else
        {
            if (i >= g726_bytes)
                break;
            code = g726_data[i++];
        }
        sl = s->dec_func(s, code);
        if (s->ext_coding != G726_ENCODING_LINEAR)
            ((uint8_t *) amp)[samples++] = (uint8_t) sl;
        else
            amp[samples++] = (int16_t) sl;
    }
    return samples;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g726_encode(g726_state_t *s,
                              uint8_t g726_data[],
                              const int16_t amp[],
                              int len)
{
    int i;
    int g726_bytes;
    int16_t sl;
    uint8_t code;

    for (g726_bytes = i = 0;  i < len;  i++)
    {
        /* Linearize the input sample to 14-bit PCM */
        switch (s->ext_coding)
        {
        case G726_ENCODING_ALAW:
            sl = alaw_to_linear(((const uint8_t *) amp)[i]) >> 2;
            break;
        case G726_ENCODING_ULAW:
            sl = ulaw_to_linear(((const uint8_t *) amp)[i]) >> 2;
            break;
        default:
            sl = amp[i] >> 2;
            break;
        }
        code = s->enc_func(s, sl);
        if (s->packing != G726_PACKING_NONE)
        {
            /* Pack the code bits */
            if (s->packing != G726_PACKING_LEFT)
            {
                s->bs.bitstream |= (code << s->bs.residue);
                s->bs.residue += s->bits_per_sample;
                if (s->bs.residue >= 8)
                {
                    g726_data[g726_bytes++] = (uint8_t) (s->bs.bitstream & 0xFF);
                    s->bs.bitstream >>= 8;
                    s->bs.residue -= 8;
                }
            }
            else
            {
                s->bs.bitstream = (s->bs.bitstream << s->bits_per_sample) | code;
                s->bs.residue += s->bits_per_sample;
                if (s->bs.residue >= 8)
                {
                    g726_data[g726_bytes++] = (uint8_t) ((s->bs.bitstream >> (s->bs.residue - 8)) & 0xFF);
                    s->bs.residue -= 8;
                }
            }
        }
        else
        {
            g726_data[g726_bytes++] = (uint8_t) code;
        }
    }
    return g726_bytes;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
