/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t4_t6_encode.h - definitions for T.4/T.6 fax compression
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_T4_T6_ENCODE_H_)
#define _SPANDSP_PRIVATE_T4_T6_ENCODE_H_

/*!
    T.4 1D, T4 2D and T6 compressor state.
*/
struct t4_t6_encode_state_s
{
    /*! \brief Callback function to read a row of pixels from the image source. */
    t4_row_read_handler_t row_read_handler;
    /*! \brief Opaque pointer passed to row_read_handler. */
    void *row_read_user_data;

    /*! \brief The type of compression used. */
    int encoding;
    /*! \brief Width of the current page, in pixels. */
    int image_width;
    /*! \brief The minimum number of encoded bits per row. This is a timing thing
               for hardware FAX machines. */
    int min_bits_per_row;
    /*! \brief The current maximum contiguous rows that may be 2D encoded. */
    int max_rows_to_next_1d_row;

    /*! \brief Length of the current page, in pixels. */
    int image_length;
    /*! \brief The current number of bytes per row of uncompressed image data. */
    int bytes_per_row;

    /*! \brief Number of rows left that can be 2D encoded, before a 1D encoded row
               must be used. */
    int rows_to_next_1d_row;
    /*! \brief The current number of bits in the current encoded row. */
    int row_bits;

    /*! \brief This variable is set if we are treating the current row as a 2D encoded
               one. */
    int row_is_2d;

    /*! \brief Encoded data bits buffer. */
    uint32_t tx_bitstream;
    /*! \brief The number of bits currently in tx_bitstream. */
    int tx_bits;
    /*! \brief The working chunk of the output bit stream */
    uint8_t *bitstream;
    /*! \brief Input pointer to the output bit stream buffer. */
    int bitstream_iptr;
    /*! \brief Output pointer to the output bit stream buffer. */
    int bitstream_optr;
    /*! \brief Pointer to the bit within the byte containing the next image bit to transmit. */
    int bit_pos;

    /*! \brief Black and white run-lengths for the current row. */
    uint32_t *cur_runs;
    /*! \brief Black and white run-lengths for the reference row. */
    uint32_t *ref_runs;
    /*! \brief The number of runs currently in the reference row. */
    int ref_steps;

    /*! \brief The minimum bits in any row of the current page. For monitoring only. */
    int min_row_bits;
    /*! \brief The maximum bits in any row of the current page. For monitoring only. */
    int max_row_bits;

    /*! \brief The size of the compressed image, in bits. */
    int compressed_image_size;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
