/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * itu_bit_stream.c
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * code supplied with ITU G.722.1, which is:
 *
 *   © 2004 Polycom, Inc.
 *   All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: itu_bit_stream.c,v 1.6 2008/11/21 15:30:22 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <audiofile.h>

#include "itu_bit_stream.h"

static const int16_t frame_start = 0x6B21;
static const int16_t erased_frame_start = 0x6B20;
static const int16_t one = 0x0081;
static const int16_t zero = 0x007F;

void itu_codec_bitstream_write(const uint8_t out_data[],
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
        fwrite(out_data, 1, number_of_bytes, fp_bitstream);
        return;
    }
    j = 0;
    out_array[j++] = frame_start;
    out_array[j++] = number_of_bits;
    for (i = 0;  i < number_of_bytes;  i++)
    {
        packed_word = out_data[i];
        for (bit_count = 7;  bit_count >= 0;  bit_count--)
            out_array[j++] = ((packed_word >> bit_count) & 1)  ?  one  :  zero;
    }   

    fwrite(out_array, sizeof(int16_t), number_of_bits + 2, fp_bitstream);
}
/*- End of function --------------------------------------------------------*/

int itu_codec_bitstream_read(uint8_t in_data[],
                             int16_t *p_frame_error_flag,
                             int number_of_bits,
                             int mode,
                             FILE *fp_bitstream)
{
    int i;
    int j;
    int bit_count;
    int nsamp;
    int len;
    int erased_frame;
    int16_t packed_word;
    int16_t bit;
    int16_t in_array[2 + number_of_bits];

    if (mode == ITU_CODEC_BITSTREAM_PACKED)
        return fread(in_data, 1, number_of_bits/8, fp_bitstream)*8;

    nsamp = fread(in_array, sizeof(int16_t), 2, fp_bitstream);
    if (nsamp < 2)
        return -1;
    if (in_array[0] != frame_start  &&  in_array[0] != erased_frame_start)
    {
        *p_frame_error_flag = 1;
        return 0;
    }
    erased_frame = (in_array[0] == erased_frame_start);
    len = in_array[1];
    if (len > number_of_bits)
    {
        *p_frame_error_flag = 1;
        return 0;
    }
    nsamp = fread(in_array, sizeof(int16_t), len, fp_bitstream);
    if (nsamp != len)
    {
        *p_frame_error_flag = 1;
        return nsamp;
    }
    *p_frame_error_flag = 0;

    for (i = 0, j = 0;  i < nsamp/8;  i++)
    {
        packed_word = 0;
        bit_count = 7;
        while (bit_count >= 0)
        {
            bit = in_array[j++];
            if (bit == zero)
                bit = 0;
            else if (bit == one)
                bit = 1;
            else
            {
                /* Bad bit */
                bit = 1;
                *p_frame_error_flag = 1;
                /* printf("read_ITU_format: bit not zero or one: %4x\n", bit); */
            }
            packed_word = (packed_word << 1) | bit;
            bit_count--;
        }
        in_data[i] = packed_word;
    }
    if (erased_frame)
        *p_frame_error_flag = 1;
    return nsamp;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
