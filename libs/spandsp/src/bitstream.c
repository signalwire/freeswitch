/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bitstream.c - Bitstream composition and decomposition routines.
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
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/bitstream.h"

#include "spandsp/private/bitstream.h"

SPAN_DECLARE(void) bitstream_put(bitstream_state_t *s, uint8_t **c, uint32_t value, int bits)
{
    value &= ((1 << bits) - 1);
    if (s->lsb_first)
    {
        if (s->residue + bits <= 32)
        {
            s->bitstream |= (value << s->residue);
            s->residue += bits;
        }
        while (s->residue >= 8)
        {
            s->residue -= 8;
            *(*c)++ = (uint8_t) (s->bitstream & 0xFF);
            s->bitstream >>= 8;
        }
    }
    else
    {
        if (s->residue + bits <= 32)
        {
            s->bitstream = (s->bitstream << bits) | value;
            s->residue += bits;
        }
        while (s->residue >= 8)
        {
            s->residue -= 8;
            *(*c)++ = (uint8_t) ((s->bitstream >> s->residue) & 0xFF);
        }
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) bitstream_emit(bitstream_state_t *s, uint8_t **c)
{
    uint32_t bitstream;

    if (s->residue > 0)
    {
        bitstream = s->bitstream & ((1 << s->residue) - 1);
        if (s->lsb_first)
            *(*c) = (uint8_t) bitstream;
        else
            *(*c) = (uint8_t) (bitstream << (8 - s->residue));
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) bitstream_flush(bitstream_state_t *s, uint8_t **c)
{
    if (s->residue > 0)
    {
        bitstream_emit(s, c);
        (*c)++;
        s->residue = 0;
    }
    s->bitstream = 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) bitstream_get(bitstream_state_t *s, const uint8_t **c, int bits)
{
    uint32_t x;

    if (s->lsb_first)
    {
        while (s->residue < bits)
        {
            s->bitstream |= (((uint32_t) *(*c)++) << s->residue);
            s->residue += 8;
        }
        s->residue -= bits;
        x = s->bitstream & ((1 << bits) - 1);
        s->bitstream >>= bits;
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

SPAN_DECLARE(bitstream_state_t *) bitstream_init(bitstream_state_t *s, int lsb_first)
{
    if (s == NULL)
    {
        if ((s = (bitstream_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    s->bitstream = 0;
    s->residue = 0;
    s->lsb_first = lsb_first;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bitstream_release(bitstream_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bitstream_free(bitstream_state_t *s)
{
    if (s)
        span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
