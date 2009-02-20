/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g711.c - A-law and u-law transcoding routines
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
 * $Id: g711.c,v 1.16 2009/02/10 13:06:46 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/bit_operations.h"
#include "spandsp/g711.h"
#include "spandsp/private/g711.h"

/* Copied from the CCITT G.711 specification */
static const uint8_t ulaw_to_alaw_table[256] =
{
     42,  43,  40,  41,  46,  47,  44,  45,  34,  35,  32,  33,  38,  39,  36,  37,
     58,  59,  56,  57,  62,  63,  60,  61,  50,  51,  48,  49,  54,  55,  52,  53,
     10,  11,   8,   9,  14,  15,  12,  13,   2,   3,   0,   1,   6,   7,   4,  26,
     27,  24,  25,  30,  31,  28,  29,  18,  19,  16,  17,  22,  23,  20,  21, 106,
    104, 105, 110, 111, 108, 109,  98,  99,  96,  97, 102, 103, 100, 101, 122, 120,
    126, 127, 124, 125, 114, 115, 112, 113, 118, 119, 116, 117,  75,  73,  79,  77,
     66,  67,  64,  65,  70,  71,  68,  69,  90,  91,  88,  89,  94,  95,  92,  93,
     82,  82,  83,  83,  80,  80,  81,  81,  86,  86,  87,  87,  84,  84,  85,  85,
    170, 171, 168, 169, 174, 175, 172, 173, 162, 163, 160, 161, 166, 167, 164, 165,
    186, 187, 184, 185, 190, 191, 188, 189, 178, 179, 176, 177, 182, 183, 180, 181,
    138, 139, 136, 137, 142, 143, 140, 141, 130, 131, 128, 129, 134, 135, 132, 154,
    155, 152, 153, 158, 159, 156, 157, 146, 147, 144, 145, 150, 151, 148, 149, 234,
    232, 233, 238, 239, 236, 237, 226, 227, 224, 225, 230, 231, 228, 229, 250, 248,
    254, 255, 252, 253, 242, 243, 240, 241, 246, 247, 244, 245, 203, 201, 207, 205,
    194, 195, 192, 193, 198, 199, 196, 197, 218, 219, 216, 217, 222, 223, 220, 221,
    210, 210, 211, 211, 208, 208, 209, 209, 214, 214, 215, 215, 212, 212, 213, 213
};

/* These transcoding tables are copied from the CCITT G.711 specification. To achieve
   optimal results, do not change them. */

static const uint8_t alaw_to_ulaw_table[256] =
{
     42,  43,  40,  41,  46,  47,  44,  45,  34,  35,  32,  33,  38,  39,  36,  37,
     57,  58,  55,  56,  61,  62,  59,  60,  49,  50,  47,  48,  53,  54,  51,  52,
     10,  11,   8,   9,  14,  15,  12,  13,   2,   3,   0,   1,   6,   7,   4,   5,
     26,  27,  24,  25,  30,  31,  28,  29,  18,  19,  16,  17,  22,  23,  20,  21,
     98,  99,  96,  97, 102, 103, 100, 101,  93,  93,  92,  92,  95,  95,  94,  94,
    116, 118, 112, 114, 124, 126, 120, 122, 106, 107, 104, 105, 110, 111, 108, 109,
     72,  73,  70,  71,  76,  77,  74,  75,  64,  65,  63,  63,  68,  69,  66,  67,
     86,  87,  84,  85,  90,  91,  88,  89,  79,  79,  78,  78,  82,  83,  80,  81,
    170, 171, 168, 169, 174, 175, 172, 173, 162, 163, 160, 161, 166, 167, 164, 165,
    185, 186, 183, 184, 189, 190, 187, 188, 177, 178, 175, 176, 181, 182, 179, 180,
    138, 139, 136, 137, 142, 143, 140, 141, 130, 131, 128, 129, 134, 135, 132, 133,
    154, 155, 152, 153, 158, 159, 156, 157, 146, 147, 144, 145, 150, 151, 148, 149,
    226, 227, 224, 225, 230, 231, 228, 229, 221, 221, 220, 220, 223, 223, 222, 222,
    244, 246, 240, 242, 252, 254, 248, 250, 234, 235, 232, 233, 238, 239, 236, 237,
    200, 201, 198, 199, 204, 205, 202, 203, 192, 193, 191, 191, 196, 197, 194, 195,
    214, 215, 212, 213, 218, 219, 216, 217, 207, 207, 206, 206, 210, 211, 208, 209
};

SPAN_DECLARE(uint8_t) alaw_to_ulaw(uint8_t alaw)
{
    return alaw_to_ulaw_table[alaw];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint8_t) ulaw_to_alaw(uint8_t ulaw)
{
    return ulaw_to_alaw_table[ulaw];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g711_decode(g711_state_t *s,
                              int16_t amp[],
                              const uint8_t g711_data[],
                              int g711_bytes)
{
    int i;

    if (s->mode == G711_ALAW)
    {
        for (i = 0;  i < g711_bytes;  i++)
            amp[i] = alaw_to_linear(g711_data[i]);
        /*endfor*/
    }
    else
    {
        for (i = 0;  i < g711_bytes;  i++)
            amp[i] = ulaw_to_linear(g711_data[i]);
        /*endfor*/
    }
    /*endif*/
    return g711_bytes;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g711_encode(g711_state_t *s,
                              uint8_t g711_data[],
                              const int16_t amp[],
                              int len)
{
    int i;

    if (s->mode == G711_ALAW)
    {
        for (i = 0;  i < len;  i++)
            g711_data[i] = linear_to_alaw(amp[i]);
        /*endfor*/
    }
    else
    {
        for (i = 0;  i < len;  i++)
            g711_data[i] = linear_to_ulaw(amp[i]);
        /*endfor*/
    }
    /*endif*/
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g711_transcode(g711_state_t *s,
                                 uint8_t g711_out[],
                                 const uint8_t g711_in[],
                                 int g711_bytes)
{
    int i;

    if (s->mode == G711_ALAW)
    {
        for (i = 0;  i < g711_bytes;  i++)
            g711_out[i] = alaw_to_ulaw_table[g711_in[i]];
        /*endfor*/
    }
    else
    {
        for (i = 0;  i < g711_bytes;  i++)
            g711_out[i] = ulaw_to_alaw_table[g711_in[i]];
        /*endfor*/
    }
    /*endif*/
    return g711_bytes;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(g711_state_t *) g711_init(g711_state_t *s, int mode)
{
    if (s == NULL)
    {
        if ((s = (g711_state_t *) malloc(sizeof(*s))) == NULL)
            return  NULL;
    }
    s->mode = mode;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g711_release(g711_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g711_free(g711_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
