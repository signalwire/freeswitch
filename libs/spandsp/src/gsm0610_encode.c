/*
 * SpanDSP - a series of DSP components for telephony
 *
 * gsm0610_encode.c - GSM 06.10 full rate speech codec.
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
#include "spandsp/alloc.h"
#include "spandsp/fast_convert.h"
#include "spandsp/bitstream.h"
#include "spandsp/saturated.h"
#include "spandsp/gsm0610.h"

#include "gsm0610_local.h"

/* 4.2 FIXED POINT IMPLEMENTATION OF THE RPE-LTP CODER */

/* The RPE-LTD coder works on a frame by frame basis.  The length of
   the frame is equal to 160 samples.  Some computations are done
   once per frame to produce at the output of the coder the
   LARc[1..8] parameters which are the coded LAR coefficients and
   also to realize the inverse filtering operation for the entire
   frame (160 samples of signal d[0..159]).  These parts produce at
   the output of the coder:

   Procedure 4.2.11 to 4.2.18 are to be executed four times per
   frame.  That means once for each sub-segment RPE-LTP analysis of
   40 samples.  These parts produce at the output of the coder.
*/
static void encode_a_frame(gsm0610_state_t *s, gsm0610_frame_t *f, const int16_t amp[])
{
    int k;
    int16_t *dp;
    int16_t *dpp;
    int16_t so[GSM0610_FRAME_LEN];
    int i;

    dp = s->dp0 + 120;
    dpp = dp;
    gsm0610_preprocess(s, amp, so);
    gsm0610_lpc_analysis(s, so, f->LARc);
    gsm0610_short_term_analysis_filter(s, f->LARc, so);

    for (k = 0;  k < 4;  k++)
    {
        gsm0610_long_term_predictor(s,
                                    so + k*40,
                                    dp,
                                    s->e + 5,
                                    dpp,
                                    &f->Nc[k],
                                    &f->bc[k]);
        gsm0610_rpe_encoding(s, s->e + 5, &f->xmaxc[k], &f->Mc[k], f->xMc[k]);

        for (i = 0;  i < 40;  i++)
            dp[i] = sat_add16(s->e[5 + i], dpp[i]);
        /*endfor*/
        dp += 40;
        dpp += 40;
    }
    /*endfor*/
    memcpy((char *) s->dp0,
           (char *) (s->dp0 + GSM0610_FRAME_LEN),
           120*sizeof(*s->dp0));
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_set_packing(gsm0610_state_t *s, int packing)
{
    s->packing = packing;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(gsm0610_state_t *) gsm0610_init(gsm0610_state_t *s, int packing)
{
    if (s == NULL)
    {
        if ((s = (gsm0610_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset((char *) s, '\0', sizeof(gsm0610_state_t));
    s->nrp = 40;
    s->packing = packing;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_release(gsm0610_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_free(gsm0610_state_t *s)
{
    if (s)
        span_free(s);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_pack_none(uint8_t c[], const gsm0610_frame_t *s)
{
    int i;
    int j;
    int k;

    i = 0;
    for (j = 0;  j < 8;  j++)
        c[i++] = (uint8_t) s->LARc[j];
    for (j = 0;  j < 4;  j++)
    {
        c[i++] = (uint8_t) s->Nc[j];
        c[i++] = (uint8_t) s->bc[j];
        c[i++] = (uint8_t) s->Mc[j];
        c[i++] = (uint8_t) s->xmaxc[j];
        for (k = 0;  k < 13;  k++)
            c[i++] = (uint8_t) s->xMc[j][k];
    }
    /*endfor*/
    return 76;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_pack_wav49(uint8_t c[], const gsm0610_frame_t *s)
{
    uint16_t sr;
    int i;

    sr = 0;
    sr = (sr >> 6) | (s->LARc[0] << 10);
    sr = (sr >> 6) | (s->LARc[1] << 10);
    *c++ = (uint8_t) (sr >> 4);
    sr = (sr >> 5) | (s->LARc[2] << 11);
    *c++ = (uint8_t) (sr >> 7);
    sr = (sr >> 5) | (s->LARc[3] << 11);
    sr = (sr >> 4) | (s->LARc[4] << 12);
    *c++ = (uint8_t) (sr >> 6);
    sr = (sr >> 4) | (s->LARc[5] << 12);
    sr = (sr >> 3) | (s->LARc[6] << 13);
    *c++ = (uint8_t) (sr >> 7);
    sr = (sr >> 3) | (s->LARc[7] << 13);

    for (i = 0;  i < 4;  i++)
    {
        sr = (sr >> 7) | (s->Nc[i] << 9);
        *c++ = (uint8_t) (sr >> 5);
        sr = (sr >> 2) | (s->bc[i] << 14);
        sr = (sr >> 2) | (s->Mc[i] << 14);
        sr = (sr >> 6) | (s->xmaxc[i] << 10);
        *c++ = (uint8_t) (sr >> 3);
        sr = (sr >> 3) | (s->xMc[i][0] << 13);
        *c++ = (uint8_t) (sr >> 8);
        sr = (sr >> 3) | (s->xMc[i][1] << 13);
        sr = (sr >> 3) | (s->xMc[i][2] << 13);
        sr = (sr >> 3) | (s->xMc[i][3] << 13);
        *c++ = (uint8_t) (sr >> 7);
        sr = (sr >> 3) | (s->xMc[i][4] << 13);
        sr = (sr >> 3) | (s->xMc[i][5] << 13);
        sr = (sr >> 3) | (s->xMc[i][6] << 13);
        *c++ = (uint8_t) (sr >> 6);
        sr = (sr >> 3) | (s->xMc[i][7] << 13);
        sr = (sr >> 3) | (s->xMc[i][8] << 13);
        *c++ = (uint8_t) (sr >> 8);
        sr = (sr >> 3) | (s->xMc[i][9] << 13);
        sr = (sr >> 3) | (s->xMc[i][10] << 13);
        sr = (sr >> 3) | (s->xMc[i][11] << 13);
        *c++ = (uint8_t) (sr >> 7);
        sr = (sr >> 3) | (s->xMc[i][12] << 13);
    }
    /*endfor*/

    s++;
    sr = (sr >> 6) | (s->LARc[0] << 10);
    *c++ = (uint8_t) (sr >> 6);
    sr = (sr >> 6) | (s->LARc[1] << 10);
    *c++ = (uint8_t) (sr >> 8);
    sr = (sr >> 5) | (s->LARc[2] << 11);
    sr = (sr >> 5) | (s->LARc[3] << 11);
    *c++ = (uint8_t) (sr >> 6);
    sr = (sr >> 4) | (s->LARc[4] << 12);
    sr = (sr >> 4) | (s->LARc[5] << 12);
    *c++ = (uint8_t) (sr >> 6);
    sr = (sr >> 3) | (s->LARc[6] << 13);
    sr = (sr >> 3) | (s->LARc[7] << 13);
    *c++ = (uint8_t) (sr >> 8);

    for (i = 0;  i < 4;  i++)
    {
        sr = (sr >> 7) | (s->Nc[i] << 9);
        sr = (sr >> 2) | (s->bc[i] << 14);
        *c++ = (uint8_t) (sr >> 7);
        sr = (sr >> 2) | (s->Mc[i] << 14);
        sr = (sr >> 6) | (s->xmaxc[i] << 10);
        *c++ = (uint8_t) (sr >> 7);
        sr = (sr >> 3) | (s->xMc[i][0] << 13);
        sr = (sr >> 3) | (s->xMc[i][1] << 13);
        sr = (sr >> 3) | (s->xMc[i][2] << 13);
        *c++ = (uint8_t) (sr >> 6);
        sr = (sr >> 3) | (s->xMc[i][3] << 13);
        sr = (sr >> 3) | (s->xMc[i][4] << 13);
        *c++ = (uint8_t) (sr >> 8);
        sr = (sr >> 3) | (s->xMc[i][5] << 13);
        sr = (sr >> 3) | (s->xMc[i][6] << 13);
        sr = (sr >> 3) | (s->xMc[i][7] << 13);
        *c++ = (uint8_t) (sr >> 7);
        sr = (sr >> 3) | (s->xMc[i][8] << 13);
        sr = (sr >> 3) | (s->xMc[i][9] << 13);
        sr = (sr >> 3) | (s->xMc[i][10] << 13);
        *c++ = (uint8_t) (sr >> 6);
        sr = (sr >> 3) | (s->xMc[i][11] << 13);
        sr = (sr >> 3) | (s->xMc[i][12] << 13);
        *c++ = (uint8_t) (sr >> 8);
    }
    /*endfor*/
    return 65;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_pack_voip(uint8_t c[33], const gsm0610_frame_t *s)
{
    int i;

    *c++ = (uint8_t) (((GSM0610_MAGIC & 0xF) << 4)
                    | ((s->LARc[0] >> 2) & 0xF));
    *c++ = (uint8_t) (((s->LARc[0] & 0x3) << 6)
                    |  (s->LARc[1] & 0x3F));
    *c++ = (uint8_t) (((s->LARc[2] & 0x1F) << 3)
                    | ((s->LARc[3] >> 2) & 0x7));
    *c++ = (uint8_t) (((s->LARc[3] & 0x3) << 6)
                    | ((s->LARc[4] & 0xF) << 2)
                    | ((s->LARc[5] >> 2) & 0x3));
    *c++ = (uint8_t) (((s->LARc[5] & 0x3) << 6)
                    | ((s->LARc[6] & 0x7) << 3)
                    |  (s->LARc[7] & 0x7));

    for (i = 0;  i < 4;  i++)
    {
        *c++ = (uint8_t) (((s->Nc[i] & 0x7F) << 1)
                        | ((s->bc[i] >> 1) & 0x1));
        *c++ = (uint8_t) (((s->bc[i] & 0x1) << 7)
                        | ((s->Mc[i] & 0x3) << 5)
                        | ((s->xmaxc[i] >> 1) & 0x1F));
        *c++ = (uint8_t) (((s->xmaxc[i] & 0x1) << 7)
                        | ((s->xMc[i][0] & 0x7) << 4)
                        | ((s->xMc[i][1] & 0x7) << 1)
                        | ((s->xMc[i][2] >> 2) & 0x1));
        *c++ = (uint8_t) (((s->xMc[i][2] & 0x3) << 6)
                        | ((s->xMc[i][3] & 0x7) << 3)
                        |  (s->xMc[i][4] & 0x7));
        *c++ = (uint8_t) (((s->xMc[i][5] & 0x7) << 5)
                        | ((s->xMc[i][6] & 0x7) << 2)
                        | ((s->xMc[i][7] >> 1) & 0x3));
        *c++ = (uint8_t) (((s->xMc[i][7] & 0x1) << 7)
                        | ((s->xMc[i][8] & 0x7) << 4)
                        | ((s->xMc[i][9] & 0x7) << 1)
                        | ((s->xMc[i][10] >> 2) & 0x1));
        *c++ = (uint8_t) (((s->xMc[i][10] & 0x3) << 6)
                        | ((s->xMc[i][11] & 0x7) << 3)
                        |  (s->xMc[i][12] & 0x7));
    }
    /*endfor*/
    return 33;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) gsm0610_encode(gsm0610_state_t *s, uint8_t code[], const int16_t amp[], int len)
{
    gsm0610_frame_t frame[2];
    int bytes;
    int i;

    bytes = 0;
    for (i = 0;  i < len;  i += GSM0610_FRAME_LEN)
    {
        encode_a_frame(s, frame, &amp[i]);
        switch (s->packing)
        {
        case GSM0610_PACKING_WAV49:
            i += GSM0610_FRAME_LEN;
            encode_a_frame(s, frame + 1, &amp[i]);
            bytes += gsm0610_pack_wav49(&code[bytes], frame);
            break;
        case GSM0610_PACKING_VOIP:
            bytes += gsm0610_pack_voip(&code[bytes], frame);
            break;
        default:
            bytes += gsm0610_pack_none(&code[bytes], frame);
            break;
        }
        /*endswitch*/
    }
    /*endfor*/
    return bytes;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
