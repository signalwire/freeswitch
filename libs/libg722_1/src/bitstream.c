/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * bitstream.c
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: bitstream.c,v 1.2 2008/10/17 13:18:21 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "g722_1/g722_1.h"
#include "bitstream.h"

void g722_1_bitstream_put(g722_1_bitstream_state_t *s, uint8_t **c, uint32_t value, int bits)
{
    if (bits < 32)
        value &= ((1 << bits) - 1);
    if (bits > 24)
    {
        /* We can't deal with this many bits in one go. Split up the operation */
        bits -= 8;
        s->bitstream = (s->bitstream << bits) | (value >> 8);
        s->residue += bits;
        while (s->residue >= 8)
        {
            s->residue -= 8;
            *(*c)++ = (uint8_t) ((s->bitstream >> s->residue) & 0xFF);
        }
        bits = 8;
        value &= 0xFF;
    }
    s->bitstream = (s->bitstream << bits) | value;
    s->residue += bits;
    while (s->residue >= 8)
    {
        s->residue -= 8;
        *(*c)++ = (uint8_t) ((s->bitstream >> s->residue) & 0xFF);
    }
}
/*- End of function --------------------------------------------------------*/

uint32_t g722_1_bitstream_get(g722_1_bitstream_state_t *s, const uint8_t **c, int bits)
{
    uint32_t x;

    if (bits > 24)
    {
        /* We can't deal with this many bits in one go. Split up the operation */
        while (s->residue < 24)
        {
            s->bitstream = (s->bitstream << 8) | ((uint32_t) *(*c)++);
            s->residue += 8;
        }
        s->residue -= 24;
        bits -= 24;
        x = ((s->bitstream >> s->residue) & 0xFFFFFF) << bits;
        while (s->residue < bits)
        {
            s->bitstream = (s->bitstream << 8) | ((uint32_t) *(*c)++);
            s->residue += 8;
        }
        s->residue -= bits;
        x |= (s->bitstream >> s->residue) & ((1 << bits) - 1);
    }
    else
    {
        while (s->residue < bits)
        {
            s->bitstream = (s->bitstream << 8) | ((uint32_t) *(*c)++);
            s->residue += 8;
        }
        s->residue -= bits;
        x = (s->bitstream >> s->residue) & ((1 << bits) - 1);
    }
    return x;
}
/*- End of function --------------------------------------------------------*/

void g722_1_bitstream_flush(g722_1_bitstream_state_t *s, uint8_t **c)
{
    if (s->residue > 0)
    {
        *(*c)++ = (uint8_t) ((s->bitstream << (8 - s->residue)) & 0xFF);
        s->residue = 0;
    }
}
/*- End of function --------------------------------------------------------*/

g722_1_bitstream_state_t *g722_1_bitstream_init(g722_1_bitstream_state_t *s)
{
    if (s == NULL)
        return NULL;
    s->bitstream = 0;
    s->residue = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
