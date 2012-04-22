/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * g192_bit_stream.h
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
 *
 * $Id: g192_bit_stream.h,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if !defined(_G192_BIT_STREAM_H_)
#define _G192_BIT_STREAM_H_

/*! \page g192_bit_stream_page ITU G.192 codec bit stream handling
\section g192_bit_stream_page_sec_1 What does it do?

\section g192_bit_stream_page_sec_2 How does it work?
*/

enum
{
    ITU_CODEC_BITSTREAM_PACKED = 0,
    ITU_CODEC_BITSTREAM_G192 = 1
};

#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Write a frame of data to an output file.
    \param out_data The buffer for the data to be written.
    \param number_of_bits The number of bits to be written.
    \param mode 0 = continuous, 1 = ITU G.192 codec bitstream format.
    \param fp_bitstream The file context to be written to.*/
void itu_codec_bitstream_write(const uint8_t out_data[],
                               int number_of_bits,
                               int mode,
                               FILE *fp_bitstream);

/*! \brief Read a frame of data from an input file.
    \param in_data The buffer for the data to be read.
    \param p_frame_error_flags ???.
    \param number_of_bits The number of bits to be read.
    \param mode 0 = continuous, 1 = ITU G.192 codec bitstream format.
    \param fp_bitstream The file context to be read from.
    \return The number of words read. */
int itu_codec_bitstream_read(uint8_t in_data[],
                             int16_t *p_frame_error_flag,
                             int number_of_bits,
                             int mode,
                             FILE *fp_bitstream);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
