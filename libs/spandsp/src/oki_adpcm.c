/*
 * SpanDSP - a series of DSP components for telephony
 *
 * oki_adpcm.c - Conversion routines between linear 16 bit PCM data and
 *               OKI (Dialogic) ADPCM format. Supports with the 32kbps
 *               and 24kbps variants used by Dialogic.
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
 *
 * The actual OKI ADPCM encode and decode method is derived from freely
 * available code, whose exact origins seem uncertain.
 *
 * $Id: oki_adpcm.c,v 1.32 2009/02/10 13:06:46 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "spandsp/telephony.h"
#include "spandsp/oki_adpcm.h"
#include "spandsp/private/oki_adpcm.h"

/* Routines to convert 12 bit linear samples to the Oki ADPCM coding format,
   widely used in CTI, because Dialogic use it. */

/* OKI ADPCM step variation table */
static const int16_t step_size[49] =
{
       16,    17,    19,    21,    23,    25,    28,    31,
       34,    37,    41,    45,    50,    55,    60,    66,
       73,    80,    88,    97,   107,   118,   130,   143,
      157,   173,   190,   209,   230,   253,   279,   307,
      337,   371,   408,   449,   494,   544,   598,   658,
      724,   796,   876,   963,  1060,  1166,  1282,  1411,
     1552
};

static const int16_t step_adjustment[8] =
{
    -1, -1, -1, -1, 2, 4, 6, 8
};

/* Band limiting filter, to allow sample rate conversion to and
   from 6k samples/second. */
static const float cutoff_coeffs[] =
{
    -3.648392e-4f,
     5.062391e-4f,
     1.206247e-3f,
     1.804452e-3f,
     1.691750e-3f,
     4.083405e-4f,
    -1.931085e-3f,
    -4.452107e-3f,
    -5.794821e-3f,
    -4.778489e-3f,
    -1.161266e-3f,
     3.928504e-3f,
     8.259786e-3f,
     9.500425e-3f,
     6.512800e-3f,
     2.227856e-4f,
    -6.531275e-3f,
    -1.026843e-2f,
    -8.718062e-3f,
    -2.280487e-3f,
     5.817733e-3f,
     1.096777e-2f,
     9.634404e-3f,
     1.569301e-3f,
    -9.522632e-3f,
    -1.748273e-2f,
    -1.684408e-2f,
    -6.100054e-3f,
     1.071206e-2f,
     2.525209e-2f,
     2.871779e-2f,
     1.664411e-2f,
    -7.706268e-3f,
    -3.331083e-2f,
    -4.521249e-2f,
    -3.085962e-2f,
     1.373653e-2f,
     8.089593e-2f,
     1.529060e-1f,
     2.080487e-1f,
     2.286834e-1f,
     2.080487e-1f,
     1.529060e-1f,
     8.089593e-2f,
     1.373653e-2f,
    -3.085962e-2f,
    -4.521249e-2f,
    -3.331083e-2f,
    -7.706268e-3f,
     1.664411e-2f,
     2.871779e-2f,
     2.525209e-2f,
     1.071206e-2f,
    -6.100054e-3f,
    -1.684408e-2f,
    -1.748273e-2f,
    -9.522632e-3f,
     1.569301e-3f,
     9.634404e-3f,
     1.096777e-2f,
     5.817733e-3f,
    -2.280487e-3f,
    -8.718062e-3f,
    -1.026843e-2f,
    -6.531275e-3f,
     2.227856e-4f,
     6.512800e-3f,
     9.500425e-3f,
     8.259786e-3f,
     3.928504e-3f,
    -1.161266e-3f,
    -4.778489e-3f,
    -5.794821e-3f,
    -4.452107e-3f,
    -1.931085e-3f,
     4.083405e-4f,
     1.691750e-3f,
     1.804452e-3f,
     1.206247e-3f,
     5.062391e-4f,
    -3.648392e-4f
};

static int16_t decode(oki_adpcm_state_t *s, uint8_t adpcm)
{
    int16_t e;
    int16_t ss;
    int16_t linear;

    /* Doing the next part as follows:
     *
     * x = adpcm & 0x07;
     * e = (step_size[s->step_index]*(x + x + 1)) >> 3;
     * 
     * Seems an obvious improvement on a modern machine, but remember
     * the truncation errors do not come out the same. It would
     * not, therefore, be an exact match for what this code is doing.
     *
     * Just what a Dialogic card does, I do not know!
     */

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
    linear = s->last + e;

    /* Saturate the values to +/- 2^11 (supposed to be 12 bits) */
    if (linear > 2047)
        linear = 2047;
    else if (linear < -2048)
        linear = -2048;
    /*endif*/

    s->last = linear;
    s->step_index += step_adjustment[adpcm & 0x07];
    if (s->step_index < 0)
        s->step_index = 0;
    else if (s->step_index > 48)
        s->step_index = 48;
    /*endif*/
    /* Note: the result here is a 12 bit value */
    return linear;
}
/*- End of function --------------------------------------------------------*/

static uint8_t encode(oki_adpcm_state_t *s, int16_t linear)
{
    int16_t e;
    int16_t ss;
    uint8_t adpcm;

    ss = step_size[s->step_index];
    e = (linear >> 4) - s->last;
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
    if (e >= (ss >> 1))
    {
        adpcm |= (uint8_t) 0x02;
        e -= ss;
    }
    /*endif*/
    if (e >= (ss >> 2))
        adpcm |= (uint8_t) 0x01;
    /*endif*/

    /* Use the decoder to set the estimate of the last sample. */
    /* It also will adjust the step_index for us. */
    s->last = decode(s, adpcm);
    return adpcm;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(oki_adpcm_state_t *) oki_adpcm_init(oki_adpcm_state_t *s, int bit_rate)
{
    if (bit_rate != 32000  &&  bit_rate != 24000)
        return NULL;
    if (s == NULL)
    {
        if ((s = (oki_adpcm_state_t *) malloc(sizeof(*s))) == NULL)
            return  NULL;
    }
    memset(s, 0, sizeof(*s));
    s->bit_rate = bit_rate;
    
    return  s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) oki_adpcm_release(oki_adpcm_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) oki_adpcm_free(oki_adpcm_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) oki_adpcm_decode(oki_adpcm_state_t *s,
                                   int16_t amp[],
                                   const uint8_t oki_data[],
                                   int oki_bytes)
{
    int i;
    int x;
    int l;
    int n;
    int samples;
    float z;

#if (_MSC_VER >= 1400) 
    __analysis_assume(s->phase >= 0  &&  s->phase <= 4);
#endif
    samples = 0;
    if (s->bit_rate == 32000)
    {
        for (i = 0;  i < oki_bytes;  i++)
        {
            amp[samples++] = decode(s, (oki_data[i] >> 4) & 0xF) << 4;
            amp[samples++] = decode(s, oki_data[i] & 0xF) << 4;
        }
        /*endwhile*/
    }
    else
    {
        n = 0;
        for (i = 0;  i < oki_bytes;  )
        {
            /* 6k to 8k sample/second conversion */
            if (s->phase)
            {
                s->history[s->ptr++] =
                    decode(s, (n++ & 1)  ?  (oki_data[i++] & 0xF)  :  ((oki_data[i] >> 4) & 0xF)) << 4;
                s->ptr &= (32 - 1);
            }
            /*endif*/
            z = 0.0f;
            for (l = 80 - 3 + s->phase, x = s->ptr - 1;  l >= 0;  l -= 4, x--)
                z += cutoff_coeffs[l]*s->history[x & (32 - 1)];
            amp[samples++] = (int16_t) (z*4.0f);
            if (++s->phase > 3)
                s->phase = 0;
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    return  samples;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) oki_adpcm_encode(oki_adpcm_state_t *s,
                                   uint8_t oki_data[],
                                   const int16_t amp[],
                                   int len)
{
    int x;
    int l;
    int n;
    int bytes;
    float z;

    bytes = 0;
    if (s->bit_rate == 32000)
    {
        for (n = 0;  n < len;  n++)
        {
            s->oki_byte = (s->oki_byte << 4) | encode(s, amp[n]);
            if ((s->mark++ & 1))
                oki_data[bytes++] = s->oki_byte;
            /*endif*/
        }
        /*endfor*/
    }
    else
    {
        n = 0;
        for (;;)
        {
            /* 8k to 6k sample/second conversion */
            if (s->phase > 2)
            {
                s->history[s->ptr++] = amp[n];
                s->ptr &= (32 - 1);
                s->phase = 0;
                if (++n >= len)
                    break;
                /*endif*/
            }
            /*endif*/
            s->history[s->ptr++] = amp[n];
            s->ptr &= (32 - 1);
            z = 0.0f;
            for (l = 80 - s->phase, x = s->ptr - 1;  l >= 0;  l -= 3, x--)
                z += cutoff_coeffs[l]*s->history[x & (32 - 1)];
            /*endfor*/
            s->oki_byte = (s->oki_byte << 4) | encode(s, (int16_t) (z*3.0f));
            if ((s->mark++ & 1))
                oki_data[bytes++] = s->oki_byte;
            /*endif*/
            s->phase++;
            if (++n >= len)
                break;
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    return  bytes;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
