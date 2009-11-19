/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bitstream.c - 
 *
 * Copyright 2009 by Steve Underwood <steveu@coppice.org>
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
 * $Id: bitstream.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bitstream.h"

void bitstream_put(bitstream_state_t *s, uint8_t **c, uint32_t value, int bits)
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

uint32_t bitstream_get(bitstream_state_t *s, const uint8_t **c, int bits)
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

void bitstream_flush(bitstream_state_t *s, uint8_t **c)
{
    if (s->residue > 0)
    {
        *(*c)++ = (uint8_t) ((s->bitstream << (8 - s->residue)) & 0xFF);
        s->residue = 0;
    }
}
/*- End of function --------------------------------------------------------*/

bitstream_state_t *bitstream_init(bitstream_state_t *s)
{
    if (s == NULL)
        return NULL;
    s->bitstream = 0;
    s->residue = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
