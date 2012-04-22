/*
 * SpanDSP - a series of DSP components for telephony
 *
 * gsm0610_decode.c - GSM 06.10 full rate speech codec.
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
#include "spandsp/saturated.h"
#include "spandsp/gsm0610.h"

#include "gsm0610_local.h"

/* 4.3 FIXED POINT IMPLEMENTATION OF THE RPE-LTP DECODER */

static void postprocessing(gsm0610_state_t *s, int16_t amp[])
{
    int k;
    int16_t msr;
    int16_t tmp;

    msr = s->msr;
    for (k = 0;  k < GSM0610_FRAME_LEN;  k++)
    {
        tmp = gsm_mult_r(msr, 28180);
        /* De-emphasis */
        msr = saturated_add16(amp[k], tmp);
        /* Truncation & upscaling */
        amp[k] = (int16_t) (saturated_add16(msr, msr) & 0xFFF8);
    }
    /*endfor*/
    s->msr = msr;
}
/*- End of function --------------------------------------------------------*/

static void decode_a_frame(gsm0610_state_t *s,
                           int16_t amp[GSM0610_FRAME_LEN],
                           gsm0610_frame_t *f)
{
    int j;
    int k;
    int16_t erp[40];
    int16_t wt[GSM0610_FRAME_LEN];
    int16_t *drp;

    drp = s->dp0 + 120;
    for (j = 0;  j < 4;  j++)
    {
        gsm0610_rpe_decoding(s, f->xmaxc[j], f->Mc[j], f->xMc[j], erp);
        gsm0610_long_term_synthesis_filtering(s, f->Nc[j], f->bc[j], erp, drp);
        for (k = 0;  k < 40;  k++)
            wt[j*40 + k] = drp[k];
        /*endfor*/
    }
    /*endfor*/

    gsm0610_short_term_synthesis_filter(s, f->LARc, wt, amp);
    postprocessing(s, amp);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_unpack_none(gsm0610_frame_t *s, const uint8_t c[])
{
    int i;
    int j;
    int k;
    
    i = 0;
    for (j = 0;  j < 8;  j++)
        s->LARc[j] = c[i++];
    for (j = 0;  j < 4;  j++)
    {
        s->Nc[j] = c[i++];
        s->bc[j] = c[i++];
        s->Mc[j] = c[i++];
        s->xmaxc[j] = c[i++];
        for (k = 0;  k < 13;  k++)
            s->xMc[j][k] = c[i++];
    }
    return 76;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_unpack_wav49(gsm0610_frame_t *s, const uint8_t c[])
{
    uint16_t sr;
    int i;

    sr = *c++;
    s->LARc[0] = sr & 0x3F;
    sr >>= 6;
    sr |= (uint16_t) *c++ << 2;
    s->LARc[1] = sr & 0x3F;
    sr >>= 6;
    sr |= (uint16_t) *c++ << 4;
    s->LARc[2] = sr & 0x1F;
    sr >>= 5;
    s->LARc[3] = sr & 0x1F;
    sr >>= 5;
    sr |= (uint16_t) *c++ << 2;
    s->LARc[4] = sr & 0xF;
    sr >>= 4;
    s->LARc[5] = sr & 0xF;
    sr >>= 4;
    sr |= (uint16_t) *c++ << 2;
    s->LARc[6] = sr & 0x7;
    sr >>= 3;
    s->LARc[7] = sr & 0x7;
    sr >>= 3;

    for (i = 0;  i < 4;  i++)
    {
        sr |= (uint16_t) *c++ << 4;
        s->Nc[i] = sr & 0x7F;
        sr >>= 7;
        s->bc[i] = sr & 0x3;
        sr >>= 2;
        s->Mc[i] = sr & 0x3;
        sr >>= 2;
        sr |= (uint16_t) *c++ << 1;
        s->xmaxc[i] = sr & 0x3F;
        sr >>= 6;
        s->xMc[i][0] = sr & 0x7;
        sr >>= 3;
        sr = *c++;
        s->xMc[i][1] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][2] = sr & 0x7;
        sr >>= 3;
        sr |= (uint16_t) *c++ << 2;
        s->xMc[i][3] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][4] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][5] = sr & 0x7;
        sr >>= 3;
        sr |= (uint16_t) *c++ << 1;
        s->xMc[i][6] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][7] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][8] = sr & 0x7;
        sr >>= 3;
        sr = *c++;
        s->xMc[i][9] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][10] = sr & 0x7;
        sr >>= 3;
        sr |= (uint16_t) *c++ << 2;
        s->xMc[i][11] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][12] = sr & 0x7;
        sr >>= 3;
    }

    s++;
    sr |= (uint16_t) *c++ << 4;
    s->LARc[0] = sr & 0x3F;
    sr >>= 6;
    s->LARc[1] = sr & 0x3F;
    sr >>= 6;
    sr = *c++;
    s->LARc[2] = sr & 0x1F;
    sr >>= 5;
    sr |= (uint16_t) *c++ << 3;
    s->LARc[3] = sr & 0x1F;
    sr >>= 5;
    s->LARc[4] = sr & 0xF;
    sr >>= 4;
    sr |= (uint16_t) *c++ << 2;
    s->LARc[5] = sr & 0xF;
    sr >>= 4;
    s->LARc[6] = sr & 0x7;
    sr >>= 3;
    s->LARc[7] = sr & 0x7;
    sr >>= 3;

    for (i = 0;  i < 4;  i++)
    {
        sr = *c++;
        s->Nc[i] = sr & 0x7F;
        sr >>= 7;
        sr |= (uint16_t) *c++ << 1;
        s->bc[i] = sr & 0x3;
        sr >>= 2;
        s->Mc[i] = sr & 0x3;
        sr >>= 2;
        sr |= (uint16_t) *c++ << 5;
        s->xmaxc[i] = sr & 0x3F;
        sr >>= 6;
        s->xMc[i][0] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][1] = sr & 0x7;
        sr >>= 3;
        sr |= (uint16_t) *c++ << 1;
        s->xMc[i][2] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][3] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][4] = sr & 0x7;
        sr >>= 3;
        sr = *c++;
        s->xMc[i][5] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][6] = sr & 0x7;
        sr >>= 3;
        sr |= (uint16_t) *c++ << 2;
        s->xMc[i][7] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][8] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][9] = sr & 0x7;
        sr >>= 3;
        sr |= (uint16_t) *c++ << 1;
        s->xMc[i][10] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][11] = sr & 0x7;
        sr >>= 3;
        s->xMc[i][12] = sr & 0x7;
        sr >>= 3;
    }
    return 65;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_unpack_voip(gsm0610_frame_t *s, const uint8_t c[33])
{
    int i;

    s->LARc[0]  = (*c++ & 0xF) << 2;
    s->LARc[0] |= (*c >> 6) & 0x3;
    s->LARc[1]  = *c++ & 0x3F;
    s->LARc[2]  = (*c >> 3) & 0x1F;
    s->LARc[3]  = (*c++ & 0x7) << 2;
    s->LARc[3] |= (*c >> 6) & 0x3;
    s->LARc[4]  = (*c >> 2) & 0xF;
    s->LARc[5]  = (*c++ & 0x3) << 2;
    s->LARc[5] |= (*c >> 6) & 0x3;
    s->LARc[6]  = (*c >> 3) & 0x7;
    s->LARc[7]  = *c++ & 0x7;

    for (i = 0;  i < 4;  i++)
    {
        s->Nc[i]       = (*c >> 1) & 0x7F;
        s->bc[i]       = (*c++ & 0x1) << 1;
        s->bc[i]      |= (*c >> 7) & 0x1;
        s->Mc[i]       = (*c >> 5) & 0x3;
        s->xmaxc[i]    = (*c++ & 0x1F) << 1;
        s->xmaxc[i]   |= (*c >> 7) & 0x1;
        s->xMc[i][0]   = (*c >> 4) & 0x7;
        s->xMc[i][1]   = (*c >> 1) & 0x7;
        s->xMc[i][2]   = (*c++ & 0x1) << 2;
        s->xMc[i][2]  |= (*c >> 6) & 0x3;
        s->xMc[i][3]   = (*c >> 3) & 0x7;
        s->xMc[i][4]   = *c++ & 0x7;
        s->xMc[i][5]   = (*c >> 5) & 0x7;
        s->xMc[i][6]   = (*c >> 2) & 0x7;
        s->xMc[i][7]   = (*c++ & 0x3) << 1;
        s->xMc[i][7]  |= (*c >> 7) & 0x1;
        s->xMc[i][8]   = (*c >> 4) & 0x7;
        s->xMc[i][9]   = (*c >> 1) & 0x7;
        s->xMc[i][10]  = (*c++ & 0x1) << 2;
        s->xMc[i][10] |= (*c >> 6) & 0x3;
        s->xMc[i][11]  = (*c >> 3) & 0x7;
        s->xMc[i][12]  = *c++ & 0x7;
    }
    return 33;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_decode(gsm0610_state_t *s, int16_t amp[], const uint8_t code[], int len)
{
    gsm0610_frame_t frame[2];
    int bytes;
    int samples;
    int i;

    samples = 0;
    for (i = 0;  i < len;  i += bytes)
    {
        switch (s->packing)
        {
        default:
        case GSM0610_PACKING_NONE:
            if ((bytes = gsm0610_unpack_none(frame, &code[i])) < 0)
                return 0;
            decode_a_frame(s, &amp[samples], frame);
            samples += GSM0610_FRAME_LEN;
            break;
        case GSM0610_PACKING_WAV49:
            if ((bytes = gsm0610_unpack_wav49(frame, &code[i])) < 0)
                return 0;
            decode_a_frame(s, &amp[samples], frame);
            samples += GSM0610_FRAME_LEN;
            decode_a_frame(s, &amp[samples], frame + 1);
            samples += GSM0610_FRAME_LEN;
            break;
        case GSM0610_PACKING_VOIP:
            if ((bytes = gsm0610_unpack_voip(frame, &code[i])) < 0)
                return 0;
            decode_a_frame(s, &amp[samples], frame);
            samples += GSM0610_FRAME_LEN;
            break;
        }
        /*endswitch*/
    }
    /*endfor*/
    return samples;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
