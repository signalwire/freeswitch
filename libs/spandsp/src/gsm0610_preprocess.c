/*
 * SpanDSP - a series of DSP components for telephony
 *
 * gsm0610_preprocess.c - GSM 06.10 full rate speech codec.
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

/*
    4.2.0 .. 4.2.3  PREPROCESSING SECTION

    After A-law to linear conversion (or directly from the
    A to D converter) the following scaling is assumed for
    input to the RPE-LTP algorithm:

    in:  0.1.....................12
         S.v.v.v.v.v.v.v.v.v.v.v.v.*.*.*

    Where S is the sign bit, v a valid bit, and * a "don't care" bit.
    The original signal is called sop[..]

    out:   0.1................... 12
         S.S.v.v.v.v.v.v.v.v.v.v.v.v.0.0
*/

void gsm0610_preprocess(gsm0610_state_t *s, const int16_t amp[GSM0610_FRAME_LEN], int16_t so[GSM0610_FRAME_LEN])
{
    int16_t z1;
    int16_t mp;
    int16_t s1;
    int16_t msp;
    int16_t SO;
    int32_t L_z2;
    int32_t L_s2;
    int32_t L_temp;
#if !defined(__GNUC__)
    int16_t lsp;
#endif
    int k;

    z1 = s->z1;
    L_z2 = s->L_z2;
    mp = s->mp;
    for (k = 0;  k < GSM0610_FRAME_LEN;  k++)
    {
        /* 4.2.1   Downscaling of the input signal */
        SO = (amp[k] >> 1) & ~3;

        /* This is supposed to have been downscaled by previous routine. */
        assert(SO >= -0x4000);
        assert(SO <=  0x3FFC);

        /* 4.2.2   Offset compensation */

        /*  This part implements a high-pass filter and requires extended
            arithmetic precision for the recursive part of this filter.
            The input of this procedure is the array so[0...159] and the
            output the array sof[0...159].
        */
        /* Compute the non-recursive part */
        s1 = SO - z1;
        z1 = SO;

        assert(s1 != INT16_MIN);

        /* Compute the recursive part */
        L_s2 = s1;
        L_s2 <<= 15;

        /* Perform a 31 by 16 bits multiplication */
#if defined(__GNUC__)
        L_z2 = ((int64_t) L_z2*32735 + 0x4000) >> 15;
        /* Alternate (ANSI) version of below line does slightly different rounding:
         * L_temp = L_z2 >> 9;
         * L_temp += L_temp >> 5;
         * L_temp = (++L_temp) >> 1;
         * L_z2 = L_z2 - L_temp;
         */
        L_z2 = sat_add32(L_z2, L_s2);
#else
        /* This does L_z2  = L_z2 * 0x7FD5/0x8000 + L_s2 */
        msp = (int16_t) (L_z2 >> 15);
        lsp = (int16_t) (L_z2 - ((int32_t) msp << 15));

        L_s2 += gsm_mult_r(lsp, 32735);
        L_temp = (int32_t) msp*32735;
        L_z2 = sat_add32(L_temp, L_s2);
#endif

        /* Compute sof[k] with rounding */
        L_temp = sat_add32(L_z2, 16384);

        /* 4.2.3  Preemphasis */
        msp = gsm_mult_r(mp, -28180);
        mp = (int16_t) (L_temp >> 15);
        so[k] = sat_add16(mp, msp);
    }
    /*endfor*/

    s->z1 = z1;
    s->L_z2 = L_z2;
    s->mp = mp;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
