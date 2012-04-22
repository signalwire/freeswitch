/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * g192_bit_stream.c
 *
 * Copyright 2008-2009 Steve Underwood <steveu@coppice.org>
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
#include <stdio.h>
#include <string.h>
#include <audiofile.h>

#include "g192_bit_stream.h"

#if !defined(FALSE)
#define FALSE 0
#endif
#if !defined(TRUE)
#define TRUE (!FALSE)
#endif

enum
{
    G192_FRAME_ERASURE = 0x6B20,
    G192_FRAME_SYNC_1 = 0x6B21,
    G192_FRAME_SYNC_2 = 0x6B22,
    G192_FRAME_SYNC_3 = 0x6B23,
    G192_FRAME_SYNC_4 = 0x6B24,
    G192_FRAME_SYNC_5 = 0x6B25,
    G192_FRAME_SYNC_6 = 0x6B26,
    G192_FRAME_SYNC_7 = 0x6B27,
    G192_FRAME_SYNC_8 = 0x6B28,
    G192_FRAME_SYNC_9 = 0x6B29,
    G192_FRAME_SYNC_10 = 0x6B2A,
    G192_FRAME_SYNC_11 = 0x6B2B,
    G192_FRAME_SYNC_12 = 0x6B2C,
    G192_FRAME_SYNC_13 = 0x6B2D,
    G192_FRAME_SYNC_14 = 0x6B2E,
    G192_FRAME_SYNC_15 = 0x6B2F,
    G192_HARD_ZERO = 0x7F,
    G192_INDETERMINATE = 0x00,
    G192_HARD_ONE = 0x81
};

int itu_codec_bitstream_write(const uint8_t out_data[],
                              int number_of_bits,
                              int mode,
                              FILE *fp_bitstream)
{
    int i;
    int j;
    int bit_count;
    int number_of_bytes;
    uint8_t packed_word;
    int16_t out_array[2 + number_of_bits + 7];

    number_of_bytes = (number_of_bits + 7)/8;
    if (mode == ITU_CODEC_BITSTREAM_PACKED)
    {
        return fwrite(out_data, 1, number_of_bytes, fp_bitstream);
    }
    j = 0;
    out_array[j++] = G192_FRAME_SYNC_1;
    out_array[j++] = number_of_bits;
    for (i = 0;  i < number_of_bytes;  i++)
    {
        packed_word = out_data[i];
        for (bit_count = 7;  bit_count >= 0;  bit_count--)
            out_array[j++] = ((packed_word >> bit_count) & 1)  ?  G192_HARD_ONE  :  G192_HARD_ZERO;
    }   

    return fwrite(out_array, sizeof(int16_t), number_of_bits + 2, fp_bitstream);
}
/*- End of function --------------------------------------------------------*/

int itu_codec_bitstream_read(uint8_t in_data[],
                             int16_t *erasure,
                             int number_of_bits,
                             int mode,
                             FILE *fp_bitstream)
{
    int i;
    int j;
    int bit_pos;
    int nsamp;
    int limit;
    int rem;
    int len;
    int erased_frame;
    int16_t packed_word;
    int16_t bit;
    int16_t in_array[2 + number_of_bits];

    *erasure = FALSE;
    if (mode == ITU_CODEC_BITSTREAM_PACKED)
    {
        nsamp = fread(in_data, 1, number_of_bits/8, fp_bitstream);
        if (nsamp <= 0)
            return -1;
        return nsamp*8;
    }

    nsamp = fread(in_array, sizeof(int16_t), 2, fp_bitstream);
    if (nsamp < 2)
        return -1;
    if (in_array[0] < G192_FRAME_ERASURE  ||  in_array[0] > G192_FRAME_SYNC_15)
    {
        *erasure = TRUE;
        return 0;
    }
    erased_frame = (in_array[0] == G192_FRAME_ERASURE);
    len = in_array[1];
    if (len > number_of_bits)
    {
        *erasure = TRUE;
        return 0;
    }
    nsamp = fread(in_array, sizeof(int16_t), len, fp_bitstream);
    if (nsamp != len)
    {
        *erasure = TRUE;
        return nsamp;
    }

    limit = (nsamp + 7)/8;
    for (i = 0, j = 0;  i < limit;  i++)
    {
        packed_word = 0;
        rem = (i == (limit - 1))  ?  (limit*8 - nsamp)  :  0;
        for (bit_pos = 7;  bit_pos >= rem;  bit_pos--)
        {
            bit = in_array[j++];
            if (bit >= 0x0001  &&  bit <= G192_HARD_ZERO)
            {
                /* Its a zero */
            }
            else if (bit >= G192_HARD_ONE  &&  bit <= 0x00FF)
            {
                /* Its a one */
                packed_word |= (1 << bit_pos);
            }
            else
            {
                /* Bad bit */
                *erasure = 1;
            }
        }
        in_data[i] = packed_word;
    }
    if (erased_frame)
        *erasure = TRUE;
    return nsamp;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
