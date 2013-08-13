/*
 * SpanDSP - a series of DSP components for telephony
 *
 * ima_adpcm.c - Conversion routines between linear 16 bit PCM data and
 *               IMA/DVI/Intel ADPCM format.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2004 Steve Underwood
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
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/fast_convert.h"
#include "spandsp/saturated.h"
#include "spandsp/ima_adpcm.h"
#include "spandsp/private/ima_adpcm.h"

/*
 * Intel/DVI ADPCM coder/decoder.
 *
 * The algorithm for this coder was taken from the IMA Compatability Project
 * proceedings, Vol 2, Number 2; May 1992.
 *
 * The RTP payload specs. reference a variant of DVI, called VDVI. This attempts to
 * further compress, in a variable bit rate manner, by expressing the 4 bit codes
 * from the DVI codec as:
 *
 *  0 00
 *  1 010
 *  2 1100
 *  3 11100
 *  4 111100
 *  5 1111100
 *  6 11111100
 *  7 11111110
 *  8 10
 *  9 011
 * 10 1101
 * 11 11101
 * 12 111101
 * 13 1111101
 * 14 11111101
 * 15 11111111
 *
 * Any left over bits in the last octet of an encoded burst are set to one.
 */

/*
   DVI4 uses an adaptive delta pulse code modulation (ADPCM) encoding
   scheme that was specified by the Interactive Multimedia Association
   (IMA) as the "IMA ADPCM wave type".  However, the encoding defined
   here as DVI4 differs in three respects from the IMA specification:

   o  The RTP DVI4 header contains the predicted value rather than the
      first sample value contained the IMA ADPCM block header.

   o  IMA ADPCM blocks contain an odd number of samples, since the first
      sample of a block is contained just in the header (uncompressed),
      followed by an even number of compressed samples.  DVI4 has an
      even number of compressed samples only, using the `predict' word
      from the header to decode the first sample.

   o  For DVI4, the 4-bit samples are packed with the first sample in
      the four most significant bits and the second sample in the four
      least significant bits.  In the IMA ADPCM codec, the samples are
      packed in the opposite order.

   Each packet contains a single DVI block.  This profile only defines
   the 4-bit-per-sample version, while IMA also specified a 3-bit-per-
   sample encoding.

   The "header" word for each channel has the following structure:

      int16  predict;  // predicted value of first sample
                       // from the previous block (L16 format)
      u_int8 index;    // current index into stepsize table
      u_int8 reserved; // set to zero by sender, ignored by receiver

   Each octet following the header contains two 4-bit samples, thus the
   number of samples per packet MUST be even because there is no means
   to indicate a partially filled last octet.
*/

/*! The number of ADPCM step sizes */
#define STEP_MAX 88

/* Intel ADPCM step variation table */
static const int step_size[STEP_MAX + 1] =
{
        7,     8,     9,    10,    11,    12,    13,    14,
       16,    17,    19,    21,    23,    25,    28,    31,
       34,    37,    41,    45,    50,    55,    60,    66,
       73,    80,    88,    97,   107,   118,   130,   143,
      157,   173,   190,   209,   230,   253,   279,   307,
      337,   371,   408,   449,   494,   544,   598,   658,
      724,   796,   876,   963,  1060,  1166,  1282,  1411,
     1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
     3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
     7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

static const int step_adjustment[8] =
{
    -1, -1, -1, -1, 2, 4, 6, 8
};

static const struct
{
    uint8_t code;
    uint8_t bits;
} vdvi_encode[] =
{
    {0x00,       2},
    {0x02,       3},
    {0x0C,       4},
    {0x1C,       5},
    {0x3C,       6},
    {0x7C,       7},
    {0xFC,       8},
    {0xFE,       8},
    {0x02,       2},
    {0x03,       3},
    {0x0D,       4},
    {0x1D,       5},
    {0x3D,       6},
    {0x7D,       7},
    {0xFD,       8},
    {0xFF,       8}
};

static const struct
{
    uint16_t code;
    uint16_t mask;
    uint8_t bits;
} vdvi_decode[] =
{
    {0x0000,    0xC000,     2},
    {0x4000,    0xE000,     3},
    {0xC000,    0xF000,     4},
    {0xE000,    0xF800,     5},
    {0xF000,    0xFC00,     6},
    {0xF800,    0xFE00,     7},
    {0xFC00,    0xFF00,     8},
    {0xFE00,    0xFF00,     8},
    {0x8000,    0xC000,     2},
    {0x6000,    0xE000,     3},
    {0xD000,    0xF000,     4},
    {0xE800,    0xF800,     5},
    {0xF400,    0xFC00,     6},
    {0xFA00,    0xFE00,     7},
    {0xFD00,    0xFF00,     8},
    {0xFF00,    0xFF00,     8}
};

static int16_t decode(ima_adpcm_state_t *s, uint8_t adpcm)
{
    int e;
    int ss;
    int16_t linear;

    /* e = (adpcm+0.5)*step/4 */
    ss = step_size[s->step_index];
    e = ss >> 3;
    if (adpcm & 0x01)
        e += (ss >> 2);
    /*endif*/
    if (adpcm & 0x02)
        e += (ss >> 1);
    /*endif*/
    if (adpcm & 0x04)
        e += ss;
    /*endif*/
    if (adpcm & 0x08)
        e = -e;
    /*endif*/
    linear = saturate(s->last + e);
    s->last = linear;
    s->step_index += step_adjustment[adpcm & 0x07];
    if (s->step_index < 0)
        s->step_index = 0;
    else if (s->step_index > STEP_MAX)
        s->step_index = STEP_MAX;
    /*endif*/
    return linear;
}
/*- End of function --------------------------------------------------------*/

static uint8_t encode(ima_adpcm_state_t *s, int16_t linear)
{
    int e;
    int ss;
    int adpcm;
    int diff;
    int initial_e;

    ss = step_size[s->step_index];
    initial_e =
    e = linear - s->last;
    diff = ss >> 3;
    adpcm = (uint8_t) 0x00;
    if (e < 0)
    {
        adpcm = (uint8_t) 0x08;
        e = -e;
    }
    /*endif*/
    if (e >= ss)
    {
        adpcm |= (uint8_t) 0x04;
        e -= ss;
    }
    /*endif*/
    ss >>= 1;
    if (e >= ss)
    {
        adpcm |= (uint8_t) 0x02;
        e -= ss;
    }
    /*endif*/
    ss >>= 1;
    if (e >= ss)
    {
        adpcm |= (uint8_t) 0x01;
        e -= ss;
    }
    /*endif*/

    if (initial_e < 0)
        diff = -(diff - initial_e - e);
    else
        diff = diff + initial_e - e;
    /*endif*/
    s->last = saturate(diff + s->last);
    s->step_index += step_adjustment[adpcm & 0x07];
    if (s->step_index < 0)
        s->step_index = 0;
    else if (s->step_index > STEP_MAX)
        s->step_index = STEP_MAX;
    /*endif*/
    return (uint8_t) adpcm;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(ima_adpcm_state_t *) ima_adpcm_init(ima_adpcm_state_t *s,
                                                 int variant,
                                                 int chunk_size)
{
    if (s == NULL)
    {
        if ((s = (ima_adpcm_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    s->variant = variant;
    s->chunk_size = chunk_size;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ima_adpcm_release(ima_adpcm_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ima_adpcm_free(ima_adpcm_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ima_adpcm_decode(ima_adpcm_state_t *s,
                                   int16_t amp[],
                                   const uint8_t ima_data[],
                                   int ima_bytes)
{
    int i;
    int j;
    int samples;
    uint16_t code;

    samples = 0;
    switch (s->variant)
    {
    case IMA_ADPCM_IMA4:
        i = 0;
        if (s->chunk_size == 0)
        {
            amp[samples++] = (ima_data[1] << 8) | ima_data[0];
            s->step_index = ima_data[2];
            s->last = amp[0];
            i = 4;
        }
        /*endif*/
        for (  ;  i < ima_bytes;  i++)
        {
            amp[samples++] = decode(s, ima_data[i] & 0xF);
            amp[samples++] = decode(s, (ima_data[i] >> 4) & 0xF);
        }
        /*endfor*/
        break;
    case IMA_ADPCM_DVI4:
        i = 0;
        if (s->chunk_size == 0)
        {
            s->last = (int16_t) ((ima_data[0] << 8) | ima_data[1]);
            s->step_index = ima_data[2];
            i = 4;
        }
        /*endif*/
        for (  ;  i < ima_bytes;  i++)
        {
            amp[samples++] = decode(s, (ima_data[i] >> 4) & 0xF);
            amp[samples++] = decode(s, ima_data[i] & 0xF);
        }
        /*endfor*/
        break;
    case IMA_ADPCM_VDVI:
        i = 0;
        if (s->chunk_size == 0)
        {
            s->last = (int16_t) ((ima_data[0] << 8) | ima_data[1]);
            s->step_index = ima_data[2];
            i = 4;
        }
        /*endif*/
        code = 0;
        s->bits = 0;
        for (;;)
        {
            if (s->bits <= 8)
            {
                if (i >= ima_bytes)
                    break;
                /*endif*/
                code |= ((uint16_t) ima_data[i++] << (8 - s->bits));
                s->bits += 8;
            }
            /*endif*/
            for (j = 0;  j < 8;  j++)
            {
                if ((vdvi_decode[j].mask & code) == vdvi_decode[j].code)
                    break;
                if ((vdvi_decode[j + 8].mask & code) == vdvi_decode[j + 8].code)
                {
                    j += 8;
                    break;
                }
                /*endif*/
            }
            /*endfor*/
            amp[samples++] = decode(s, (uint8_t) j);
            code <<= vdvi_decode[j].bits;
            s->bits -= vdvi_decode[j].bits;
        }
        /*endfor*/
        /* Use up the remanents of the last octet */
        while (s->bits > 0)
        {
            for (j = 0;  j < 8;  j++)
            {
                if ((vdvi_decode[j].mask & code) == vdvi_decode[j].code)
                    break;
                /*endif*/
                if ((vdvi_decode[j + 8].mask & code) == vdvi_decode[j + 8].code)
                {
                    j += 8;
                    break;
                }
                /*endif*/
            }
            /*endfor*/
            if (vdvi_decode[j].bits > s->bits)
                break;
            /*endif*/
            amp[samples++] = decode(s, (uint8_t) j);
            code <<= vdvi_decode[j].bits;
            s->bits -= vdvi_decode[j].bits;
        }
        /*endwhile*/
        break;
    }
    /*endswitch*/
    return samples;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ima_adpcm_encode(ima_adpcm_state_t *s,
                                   uint8_t ima_data[],
                                   const int16_t amp[],
                                   int len)
{
    int i;
    int bytes;
    uint8_t code;

    bytes = 0;
    switch (s->variant)
    {
    case IMA_ADPCM_IMA4:
        i = 0;
        if (s->chunk_size == 0)
        {
            ima_data[bytes++] = (uint8_t) amp[0];
            ima_data[bytes++] = (uint8_t) (amp[0] >> 8);
            ima_data[bytes++] = (uint8_t) s->step_index;
            ima_data[bytes++] = 0;
            s->last = amp[0];
            s->bits = 0;
            i = 1;
        }
        /*endif*/
        for (  ;  i < len;  i++)
        {
            s->ima_byte = (uint8_t) ((s->ima_byte >> 4) | (encode(s, amp[i]) << 4));
            if ((s->bits++ & 1))
                ima_data[bytes++] = (uint8_t) s->ima_byte;
            /*endif*/
        }
        /*endfor*/
        break;
    case IMA_ADPCM_DVI4:
        if (s->chunk_size == 0)
        {
            ima_data[bytes++] = (uint8_t) (s->last >> 8);
            ima_data[bytes++] = (uint8_t) s->last;
            ima_data[bytes++] = (uint8_t) s->step_index;
            ima_data[bytes++] = 0;
        }
        /*endif*/
        for (i = 0;  i < len;  i++)
        {
            s->ima_byte = (uint8_t) ((s->ima_byte << 4) | encode(s, amp[i]));
            if ((s->bits++ & 1))
                ima_data[bytes++] = (uint8_t) s->ima_byte;
            /*endif*/
        }
        /*endfor*/
        break;
    case IMA_ADPCM_VDVI:
        if (s->chunk_size == 0)
        {
            ima_data[bytes++] = (uint8_t) (s->last >> 8);
            ima_data[bytes++] = (uint8_t) s->last;
            ima_data[bytes++] = (uint8_t) s->step_index;
            ima_data[bytes++] = 0;
        }
        /*endif*/
        s->bits = 0;
        for (i = 0;  i < len;  i++)
        {
            code = encode(s, amp[i]);
            s->ima_byte = (s->ima_byte << vdvi_encode[code].bits) | vdvi_encode[code].code;
            s->bits += vdvi_encode[code].bits;
            if (s->bits >= 8)
            {
                s->bits -= 8;
                ima_data[bytes++] = (uint8_t) (s->ima_byte >> s->bits);
            }
            /*endif*/
        }
        /*endfor*/
        if (s->bits)
            ima_data[bytes++] = (uint8_t) (((s->ima_byte << 8) | 0xFF) >> s->bits);
        /*endif*/
        break;
    }
    /*endswitch*/
    return bytes;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
