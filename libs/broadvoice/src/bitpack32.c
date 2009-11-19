/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bitpack32.c - 
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from code which is
 * Copyright 2000-2009 Broadcom Corporation
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
 * $Id: bitpack32.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>

#include "broadvoice.h"
#include "bv32strct.h"
#include "bitstream.h"
#include "bitpack32.h"

/*
 *  The following is the bit table within the bit structure for
 *  BroadVoice32
 *
 *  int16_t  bit_table[] =
 *  {
 *      7, 5, 5,                      // LSP
 *      8,                            // Pitch Lag
 *      5,                            // Pitch Gain
 *      5, 5,                         // Excitation Vector Log-Gain
 *      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, // Excitation Vector 1st subframe
 *      6, 6, 6, 6, 6, 6, 6, 6, 6, 6  // Excitation Vector 2nd subframe
 *  };
 */

int bv32_bitpack(uint8_t *PackedStream, struct BV32_Bit_Stream *BitStruct)
{
    bitstream_state_t bs;
    uint8_t *stream;
    int i;

    stream = PackedStream;
    bitstream_init(&bs);
    bitstream_put(&bs, &stream, BitStruct->lspidx[0], 7);
    bitstream_put(&bs, &stream, BitStruct->lspidx[1], 5);
    bitstream_put(&bs, &stream, BitStruct->lspidx[2], 5);
    bitstream_put(&bs, &stream, BitStruct->ppidx, 8);
    bitstream_put(&bs, &stream, BitStruct->bqidx, 5);
    bitstream_put(&bs, &stream, BitStruct->gidx[0], 5);
    bitstream_put(&bs, &stream, BitStruct->gidx[1], 5);
    for (i = 0;  i < 20;  i++)
        bitstream_put(&bs, &stream, BitStruct->qvidx[i], 6);
    bitstream_flush(&bs, &stream);
    return stream - PackedStream;
}

void bv32_bitunpack(const uint8_t *PackedStream, struct BV32_Bit_Stream *BitStruct)
{
    bitstream_state_t bs;
    const uint8_t *stream;
    int i;

    stream = PackedStream;
    bitstream_init(&bs);

    BitStruct->lspidx[0] = bitstream_get(&bs, &stream, 7);
    BitStruct->lspidx[1] = bitstream_get(&bs, &stream, 5);
    BitStruct->lspidx[2] = bitstream_get(&bs, &stream, 5);

    BitStruct->ppidx = bitstream_get(&bs, &stream, 8);
    BitStruct->bqidx = bitstream_get(&bs, &stream, 5);
    BitStruct->gidx[0] = bitstream_get(&bs, &stream, 5);
    BitStruct->gidx[1] = bitstream_get(&bs, &stream, 5);

    for (i = 0;  i < 20;  i++)
        BitStruct->qvidx[i] = bitstream_get(&bs, &stream, 6);
}
