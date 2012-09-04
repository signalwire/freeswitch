/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t81_t82_arith_coding.c - ITU T.81 and T.82 QM-coder arithmetic encoding
 *                          and decoding
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "spandsp/telephony.h"
#include "spandsp/t81_t82_arith_coding.h"

#include "spandsp/private/t81_t82_arith_coding.h"

/* T.82 defines the QM-coder at a level very close to actual code. Therefore
   this file closely mirrors the routine names, variable names, and flow
   described in T.82. QM-Coder is supposed to be the same in some other image
   compression schemes, such as T.81. However, this code has not been checked
   to see if it follows the letter of any spec other than T.82. */

#define FALSE 0
#define TRUE (!FALSE)

/* Code bytes which must trigger stuffing */
enum
{
    T81_T82_STUFF = 0x00,
    T81_T82_ESC = 0xFF
};

/* This table is from T.82 table 24 - Probability estimation table */
static const struct probability_estimation_s
{
    uint16_t lsz;
    uint8_t nlps;   /* The SWITCH bit is packed into the top of this byte */
    uint8_t nmps;
} prob[113] =
{
    {0x5A1D,   1 + 128,   1},
    {0x2586,  14,         2},
    {0x1114,  16,         3},
    {0x080B,  18,         4},
    {0x03D8,  20,         5},
    {0x01DA,  23,         6},
    {0x00E5,  25,         7},
    {0x006F,  28,         8},
    {0x0036,  30,         9},
    {0x001A,  33,        10},
    {0x000D,  35,        11},
    {0x0006,   9,        12},
    {0x0003,  10,        13},
    {0x0001,  12,        13},
    {0x5A7F,  15 + 128,  15},
    {0x3F25,  36,        16},
    {0x2CF2,  38,        17},
    {0x207C,  39,        18},
    {0x17B9,  40,        19},
    {0x1182,  42,        20},
    {0x0CEF,  43,        21},
    {0x09A1,  45,        22},
    {0x072F,  46,        23},
    {0x055C,  48,        24},
    {0x0406,  49,        25},
    {0x0303,  51,        26},
    {0x0240,  52,        27},
    {0x01B1,  54,        28},
    {0x0144,  56,        29},
    {0x00F5,  57,        30},
    {0x00B7,  59,        31},
    {0x008A,  60,        32},
    {0x0068,  62,        33},
    {0x004E,  63,        34},
    {0x003B,  32,        35},
    {0x002C,  33,         9},
    {0x5AE1,  37 + 128,  37},
    {0x484C,  64,        38},
    {0x3A0D,  65,        39},
    {0x2EF1,  67,        40},
    {0x261F,  68,        41},
    {0x1F33,  69,        42},
    {0x19A8,  70,        43},
    {0x1518,  72,        44},
    {0x1177,  73,        45},
    {0x0E74,  74,        46},
    {0x0BFB,  75,        47},
    {0x09F8,  77,        48},
    {0x0861,  78,        49},
    {0x0706,  79,        50},
    {0x05CD,  48,        51},
    {0x04DE,  50,        52},
    {0x040F,  50,        53},
    {0x0363,  51,        54},
    {0x02D4,  52,        55},
    {0x025C,  53,        56},
    {0x01F8,  54,        57},
    {0x01A4,  55,        58},
    {0x0160,  56,        59},
    {0x0125,  57,        60},
    {0x00F6,  58,        61},
    {0x00CB,  59,        62},
    {0x00AB,  61,        63},
    {0x008F,  61,        32},
    {0x5B12,  65 + 128,  65},
    {0x4D04,  80,        66},
    {0x412C,  81,        67},
    {0x37D8,  82,        68},
    {0x2FE8,  83,        69},
    {0x293C,  84,        70},
    {0x2379,  86,        71},
    {0x1EDF,  87,        72},
    {0x1AA9,  87,        73},
    {0x174E,  72,        74},
    {0x1424,  72,        75},
    {0x119C,  74,        76},
    {0x0F6B,  74,        77},
    {0x0D51,  75,        78},
    {0x0BB6,  77,        79},
    {0x0A40,  77,        48},
    {0x5832,  80 + 128,  81},
    {0x4D1C,  88,        82},
    {0x438E,  89,        83},
    {0x3BDD,  90,        84},
    {0x34EE,  91,        85},
    {0x2EAE,  92,        86},
    {0x299A,  93,        87},
    {0x2516,  86,        71},
    {0x5570,  88 + 128,  89},
    {0x4CA9,  95,        90},
    {0x44D9,  96,        91},
    {0x3E22,  97,        92},
    {0x3824,  99,        93},
    {0x32B4,  99,        94},
    {0x2E17,  93,        86},
    {0x56A8,  95 + 128,  96},
    {0x4F46, 101,        97},
    {0x47E5, 102,        98},
    {0x41CF, 103,        99},
    {0x3C3D, 104,       100},
    {0x375E,  99,        93},
    {0x5231, 105,       102},
    {0x4C0F, 106,       103},
    {0x4639, 107,       104},
    {0x415E, 103,        99},
    {0x5627, 105 + 128, 106},
    {0x50E7, 108,       107},
    {0x4B85, 109,       103},
    {0x5597, 110,       109},
    {0x504F, 111,       107},
    {0x5A10, 110 + 128, 111},
    {0x5522, 112,       109},
    {0x59EB, 112 + 128, 111}
};

static __inline__ void output_stuffed_byte(t81_t82_arith_encode_state_t *s, int byte)
{
    s->output_byte_handler(s->user_data, byte);
    if (byte == T81_T82_ESC)
        s->output_byte_handler(s->user_data, T81_T82_STUFF);
}
/*- End of function --------------------------------------------------------*/

static __inline__ void byteout(t81_t82_arith_encode_state_t *s)
{
    uint32_t temp;

    /* T.30 figure 26 - BYTEOUT */
    temp = s->c >> 19;
    if (temp > 0xFF)
    {
        if (s->buffer >= 0)
            output_stuffed_byte(s, s->buffer + 1);
        while (s->sc)
        {
            s->output_byte_handler(s->user_data, 0x00);
            s->sc--;
        }
        s->buffer = temp & 0xFF;
    }
    else if (temp == 0xFF)
    {
        s->sc++;
    }
    else
    {
        if (s->buffer >= 0)
            output_stuffed_byte(s, s->buffer);
        while (s->sc)
        {
            output_stuffed_byte(s, T81_T82_ESC);
            s->sc--;
        }
        s->buffer = temp;
    }
    s->c &= 0x7FFFF;
    s->ct = 8;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void renorme(t81_t82_arith_encode_state_t *s)
{
    /* T.82 figure 25 - RENORME */
    do
    {
        s->a <<= 1;
        s->c <<= 1;
        s->ct--;
        if (s->ct == 0)
            byteout(s);
    }
    while (s->a < 0x8000);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t81_t82_arith_encode(t81_t82_arith_encode_state_t *s, int cx, int pix)
{
    uint32_t ss;

    /* T.82 figure 22 - ENCODE */
    ss = s->st[cx] & 0x7F;
    if (((pix << 7) ^ s->st[cx]) & 0x80)
    {
        /* T.82 figure 23 - CODELPS */
        s->a -= prob[ss].lsz;
        if (s->a >= prob[ss].lsz)
        {
            s->c += s->a;
            s->a = prob[ss].lsz;
        }
        s->st[cx] = (s->st[cx] & 0x80) ^ prob[ss].nlps;
        renorme(s);
    }
    else
    {
        /* T.82 figure 24 - CODEMPS */
        s->a -= prob[ss].lsz;
        if (s->a < 0x8000)
        {
            if (s->a < prob[ss].lsz)
            {
                s->c += s->a;
                s->a = prob[ss].lsz;
            }
            s->st[cx] = (s->st[cx] & 0x80) | prob[ss].nmps;
            renorme(s);
        }
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t81_t82_arith_encode_flush(t81_t82_arith_encode_state_t *s)
{
    uint32_t temp;

    /* T.82 figure 28 - FLUSH */
    /* T.82 figure 29 - CLEARBITS */
    temp = (s->c + s->a - 1) & 0xFFFF0000;
    s->c = (temp < s->c)  ?  (temp + 0x8000)  :  temp;
    /* T.82 figure 30 - FINALWRITES */
    s->c <<= s->ct;
    if ((s->c > 0x7FFFFFF))
    {
        if (s->buffer >= 0)
            output_stuffed_byte(s, s->buffer + 1);
        /* Only output 0x00 bytes if something non-0x00 will follow */
        if ((s->c & 0x7FFF800))
        {
            while (s->sc)
            {
                output_stuffed_byte(s, 0x00);
                s->sc--;
            }
        }
    }
    else
    {
        /* The next bit says s->buffer + 1 in T.82, but that makes no sense. It doesn't
           agree with how we code things away from the flush condition, and it gives
           answers which don't seem to match other JBIG coders. */
        if (s->buffer >= 0)
            output_stuffed_byte(s, s->buffer);
        while (s->sc)
        {
            output_stuffed_byte(s, 0xFF);
            s->sc--;
        }
    }
    /* Only output final bytes if they are not 0x00 */
    if ((s->c & 0x7FFF800))
    {
        output_stuffed_byte(s, (s->c >> 19) & 0xFF);
        if ((s->c & 0x7F800))
            output_stuffed_byte(s, (s->c >> 11) & 0xFF);
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t81_t82_arith_encode_restart(t81_t82_arith_encode_state_t *s, int reuse_st)
{
    /* T.82 figure 27 - INITENC */
    if (!reuse_st)
        memset(s->st, 0, sizeof(s->st));
    s->c = 0;
    s->a = 0x10000;
    s->sc = 0;
    s->ct = 11;
    s->buffer = -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t81_t82_arith_encode_state_t *) t81_t82_arith_encode_init(t81_t82_arith_encode_state_t *s,
                                                                       void (*output_byte_handler)(void *, int),
                                                                       void *user_data)
{
    if (s == NULL)
    {
        if ((s = (t81_t82_arith_encode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->output_byte_handler = output_byte_handler;
    s->user_data = user_data;

    t81_t82_arith_encode_restart(s, FALSE);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t81_t82_arith_encode_release(t81_t82_arith_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t81_t82_arith_encode_free(t81_t82_arith_encode_state_t *s)
{
    int ret;

    ret = t81_t82_arith_encode_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t81_t82_arith_decode(t81_t82_arith_decode_state_t *s, int cx)
{
    uint32_t ss;
    int pix;

    /* T.82 figure 35 - RENORMD */
    while (s->a < 0x8000  ||  s->startup)
    {
        while (s->ct <= 8  &&  s->ct >= 0)
        {
            /* First we can move a new byte into s->c */
            if (s->pscd_ptr >= s->pscd_end)
                return -1;
            if (s->pscd_ptr[0] == T81_T82_ESC)
            {
                if (s->pscd_ptr + 1 >= s->pscd_end)
                    return -1;
                if (s->pscd_ptr[1] == T81_T82_STUFF)
                {
                    s->c |= (0xFF << (8 - s->ct));
                    s->ct += 8;
                    s->pscd_ptr += 2;
                }
                else
                {
                    /* Start padding with zero bytes */
                    s->ct = -1;
                    if (s->nopadding)
                    {
                        /* Subsequent symbols might depend on zero padding */
                        s->nopadding = FALSE;
                        return -2;
                    }
                }
            }
            else
            {
                s->c |= (int32_t) *(s->pscd_ptr++) << (8 - s->ct);
                s->ct += 8;
            }
        }
        s->a <<= 1;
        s->c <<= 1;
        if (s->ct >= 0)
            s->ct--;
        if (s->a == 0x10000)
            s->startup = FALSE;
    }

    /* T.82 figure 32 - DECODE */
    ss = s->st[cx] & 0x7F;
    if ((s->c >> 16) >= (s->a -= prob[ss].lsz))
    {
        /* T.82 figure 33 - LPS_EXCHANGE */
        if (s->a < prob[ss].lsz)
        {
            s->c -= (s->a << 16);
            s->a = prob[ss].lsz;
            pix = s->st[cx] >> 7;
            s->st[cx] = (s->st[cx] & 0x80) | prob[ss].nmps;
        }
        else
        {
            s->c -= (s->a << 16);
            s->a = prob[ss].lsz;
            pix = 1 - (s->st[cx] >> 7);
            s->st[cx] = (s->st[cx]& 0x80) ^ prob[ss].nlps;
        }
    }
    else
    {
        if (s->a < 0x8000)
        {
            /* T.82 figure 34 - MPS_EXCHANGE */
            if (s->a < prob[ss].lsz)
            {
                pix = 1 - (s->st[cx] >> 7);
                s->st[cx] = (s->st[cx] & 0x80) ^ prob[ss].nlps;
            }
            else
            {
                pix = s->st[cx] >> 7;
                s->st[cx] = (s->st[cx] & 0x80) | prob[ss].nmps;
            }
        }
        else
        {
            pix = s->st[cx] >> 7;
        }
    }

    return pix;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t81_t82_arith_decode_restart(t81_t82_arith_decode_state_t *s, int reuse_st)
{
    if (!reuse_st)
        memset(s->st, 0, sizeof(s->st));
    s->c = 0;
    s->a = 1;
    s->ct = 0;
    s->startup = TRUE;
    s->nopadding = FALSE;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t81_t82_arith_decode_state_t *) t81_t82_arith_decode_init(t81_t82_arith_decode_state_t *s)
{
    if (s == NULL)
    {
        if ((s = (t81_t82_arith_decode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    t81_t82_arith_decode_restart(s, FALSE);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t81_t82_arith_decode_release(t81_t82_arith_decode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t81_t82_arith_decode_free(t81_t82_arith_decode_state_t *s)
{
    int ret;

    ret = t81_t82_arith_decode_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
