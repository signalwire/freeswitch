/*
 * SpanDSP - a series of DSP components for telephony
 *
 * lpc10_encode.c - LPC10 low bit rate speech codec.
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
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/dc_restore.h"
#include "spandsp/lpc10.h"
#include "spandsp/private/lpc10.h"

#include "lpc10_encdecs.h"

static void lpc10_pack(lpc10_encode_state_t *s, uint8_t ibits[], lpc10_frame_t *t)
{
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

    itab[0] = t->ipitch;
    itab[1] = t->irms;
    itab[2] = 0;
    for (i = 0;  i < LPC10_ORDER;  i++)
        itab[i + 3] = t->irc[LPC10_ORDER - 1 - i] & 0x7FFF;
    /* Put 54 bits into the output buffer */
    x = 0;
    for (i = 0;  i < 53;  i++)
    {
        x = (x << 1) | (itab[iblist[i] - 1] & 1);
        if ((i & 7) == 7)
            ibits[i >> 3] = (uint8_t) (x & 0xFF);
        itab[iblist[i] - 1] >>= 1;
    }
    x = (x << 1) | (s->isync & 1);
    s->isync ^= 1;
    x <<= 2;
    ibits[6] = (uint8_t) (x & 0xFF);
}
/*- End of function --------------------------------------------------------*/

/* Quantize LPC parameters for transmission */
static int encode(lpc10_encode_state_t *s,
                  lpc10_frame_t *t,
                  int32_t *voice,
                  int32_t pitch,
                  float rms,
                  float *rc)
{
    static const int32_t enctab[16] =
    {
        0, 7, 11, 12, 13, 10, 6, 1, 14, 9, 5, 2, 3, 4, 8, 15
    };
    static const int32_t entau[60] =
    {
        19, 11, 27, 25, 29, 21, 23, 22, 30, 14, 15,  7, 39, 38, 46, 
        42, 43, 41, 45, 37, 53, 49, 51, 50, 54, 52, 60, 56, 58, 26,
        90, 88, 92, 84, 86, 82, 83, 81, 85, 69, 77, 73, 75, 74, 78,
        70, 71, 67, 99, 97, 113, 112, 114, 98, 106, 104, 108, 100,
        101, 76
    };
    static const int32_t enadd[8] =
    {
        1920, -768, 2432, 1280, 3584, 1536, 2816, -1152
    };
    static const float enscl[8] =
    {
        0.0204f, 0.0167f, 0.0145f, 0.0147f, 0.0143f, 0.0135f, 0.0125f, 0.0112f
    };
    static const int32_t enbits[8] =
    {
        6, 5, 4, 4, 4, 4, 3, 3
    };
    static const int32_t entab6[64] =
    {
        0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3,
        3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 12, 13, 14, 15
    };
    static const int32_t rmst[64] =
    {
        1024, 936, 856, 784, 718, 656, 600, 550, 502,
        460, 420, 384, 352, 328, 294, 270, 246, 226,
        206, 188, 172, 158, 144, 132, 120, 110, 102, 
        92, 84, 78, 70, 64, 60, 54, 50,
        46, 42, 38, 34, 32, 30, 26, 24,
        22, 20, 18, 17, 16, 15, 14, 13,
        12, 11, 10, 9, 8, 7, 6, 5, 4,
        3, 2, 1, 0
    };

    int32_t idel;
    int32_t nbit;
    int32_t i;
    int32_t j;
    int32_t i2;
    int32_t i3;
    int32_t mrk;

    /* Scale RMS and RC's to int32_ts */
    t->irms = (int32_t) rms;
    for (i = 0;  i < LPC10_ORDER;  i++)
        t->irc[i] = (int32_t) (rc[i]*32768.0f);
    if (voice[0] != 0  &&  voice[1] != 0)
    {
        t->ipitch = entau[pitch - 1];
    }
    else
    {
        if (s->error_correction)
        {
            t->ipitch = 0;
            if (voice[0] != voice[1])
                t->ipitch = 127;
        }
        else
        {
            t->ipitch = (voice[0] << 1) + voice[1];
        }
    }
    /* Encode RMS by binary table search */
    j = 32;
    idel = 16;
    t->irms = min(t->irms, 1023);
    while (idel > 0)
    {
        if (t->irms > rmst[j - 1])
            j -= idel;
        if (t->irms < rmst[j - 1])
            j += idel;
        idel /= 2;
    }
    if (t->irms > rmst[j - 1])
        --j;
    t->irms = 31 - j/2;
    /* Encode RC(1) and (2) as log-area-ratios */
    for (i = 0;  i < 2;  i++)
    {
        i2 = t->irc[i];
        mrk = 0;
        if (i2 < 0)
        {
            i2 = -i2;
            mrk = 1;
        }
        i2 = min(i2/512, 63);
        i2 = entab6[i2];
        if (mrk != 0)
            i2 = -i2;
        t->irc[i] = i2;
    }
    /* Encode RC(3) - (10) linearly, remove bias then scale */
    for (i = 2;  i < LPC10_ORDER;  i++)
    {
        i2 = (int32_t) ((t->irc[i]/2 + enadd[LPC10_ORDER - 1 - i])*enscl[LPC10_ORDER - 1 - i]);
        i2 = max(i2, -127);
        i2 = min(i2, 127);
        nbit = enbits[LPC10_ORDER - 1 - i];
        i3 = (i2 < 0);
        i2 /= pow_ii(2, nbit);
        if (i3)
            i2--;
        t->irc[i] = i2;
    }
    /* Protect the most significant bits of the most
       important parameters during non-voiced frames.
       RC(1) - RC(4) are protected using 20 parity bits
       replacing RC(5) - RC(10). */
    if (s->error_correction)
    {
        if (t->ipitch == 0  ||  t->ipitch == 127)
        {
            t->irc[4] = enctab[(t->irc[0] & 0x1E) >> 1];
            t->irc[5] = enctab[(t->irc[1] & 0x1E) >> 1];
            t->irc[6] = enctab[(t->irc[2] & 0x1E) >> 1];
            t->irc[7] = enctab[(t->irms & 0x1E) >> 1];
            t->irc[8] = enctab[(t->irc[3] & 0x1E) >> 1] >> 1;
            t->irc[9] = enctab[(t->irc[3] & 0x1E) >> 1] & 1;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void high_pass_100hz(lpc10_encode_state_t *s, float speech[], int start, int len)
{
    float si;
    float err;
    int i;

    /* 100 Hz high pass filter */
    for (i = start;  i < len;  i++)
    {
        si = speech[i];
        err = si + s->z11*1.859076f - s->z21*0.8648249f;
        si = err - s->z11*2.0f + s->z21;
        s->z21 = s->z11;
        s->z11 = err;
        err = si + s->z12*1.935715f - s->z22*0.9417004f;
        si = err - s->z12*2.0f + s->z22;
        s->z22 = s->z12;
        s->z12 = err;
        speech[i] = si*0.902428f;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(lpc10_encode_state_t *) lpc10_encode_init(lpc10_encode_state_t *s, int error_correction)
{
    int i;
    int j;

    if (s == NULL)
    {
        if ((s = (lpc10_encode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }

    s->error_correction = error_correction;

    /* State used only by function high_pass_100hz */
    s->z11 = 0.0f;
    s->z21 = 0.0f;
    s->z12 = 0.0f;
    s->z22 = 0.0f;
    
    /* State used by function lpc10_analyse */
    for (i = 0;  i < 540;  i++)
    {
        s->inbuf[i] = 0.0f;
        s->pebuf[i] = 0.0f;
    }
    for (i = 0;  i < 696;  i++)
        s->lpbuf[i] = 0.0f;
    for (i = 0;  i < 312;  i++)
        s->ivbuf[i] = 0.0f;
    s->bias = 0.0f;
    s->osptr = 1;
    for (i = 0;  i < 3;  i++)
        s->obound[i] = 0;
    s->vwin[2][0] = 307;
    s->vwin[2][1] = 462;
    s->awin[2][0] = 307;
    s->awin[2][1] = 462;
    for (i = 0;  i < 4;  i++)
    {
        s->voibuf[i][0] = 0;
        s->voibuf[i][1] = 0;
    }
    for (i = 0;  i < 3;  i++)
        s->rmsbuf[i] = 0.0f;
    for (i = 0;  i < 3;  i++)
    {
        for (j = 0;  j < 10;  j++)
            s->rcbuf[i][j] = 0.0f;
    }
    s->zpre = 0.0f;

    /* State used by function onset */
    s->n = 0.0f;
    s->d__ = 1.0f;
    for (i = 0;  i < 16;  i++)
        s->l2buf[i] = 0.0f;
    s->l2sum1 = 0.0f;
    s->l2ptr1 = 1;
    s->l2ptr2 = 9;
    s->hyst = FALSE;

    /* State used by function lpc10_voicing */
    s->dither = 20.0f;
    s->maxmin = 0.0f;
    for (i = 0;  i < 3;  i++)
    {
        s->voice[i][0] = 0.0f;
        s->voice[i][1] = 0.0f;
    }
    s->lbve = 3000;
    s->fbve = 3000;
    s->fbue = 187;
    s->ofbue = 187;
    s->sfbue = 187;
    s->lbue = 93;
    s->olbue = 93;
    s->slbue = 93;
    s->snr = (float) (s->fbve / s->fbue << 6);

    /* State used by function dynamic_pitch_tracking */
    for (i = 0;  i < 60;  i++)
        s->s[i] = 0.0f;
    for (i = 0;  i < 2;  i++)
    {
        for (j = 0;  j < 60;  j++)
            s->p[i][j] = 0;
    }
    s->ipoint = 0;
    s->alphax = 0.0f;

    /* State used by function lpc10_pack */
    s->isync = 0;
    
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lpc10_encode_release(lpc10_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lpc10_encode_free(lpc10_encode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lpc10_encode(lpc10_encode_state_t *s, uint8_t code[], const int16_t amp[], int len)
{
    int32_t voice[2];
    int32_t pitch;
    float speech[LPC10_SAMPLES_PER_FRAME];
    float rc[LPC10_ORDER];
    float rms;
    lpc10_frame_t frame;
    int i;
    int j;

    len /= LPC10_SAMPLES_PER_FRAME;
    for (i = 0;  i < len;  i++)
    {
        for (j = 0;  j < LPC10_SAMPLES_PER_FRAME;  j++)
            speech[j] = (float) amp[i*LPC10_SAMPLES_PER_FRAME + j]/32768.0f;
        high_pass_100hz(s, speech, 0, LPC10_SAMPLES_PER_FRAME);
        lpc10_analyse(s, speech, voice, &pitch, &rms, rc);
        encode(s, &frame, voice, pitch, rms, rc);
        lpc10_pack(s, &code[7*i], &frame);
    }
    return len*7;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
