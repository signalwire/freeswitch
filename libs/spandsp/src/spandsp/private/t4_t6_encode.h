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
    /*! \brief The minimum number of encoded bits per row. This is a timing thing
               for hardware FAX machines. */
    int min_bits_per_row;
    /*! \brief The current maximum contiguous rows that may be 2D encoded. */
    int max_rows_to_next_1d_row;

    /*! \brief Number of rows left that can be 2D encoded, before a 1D encoded row
               must be used. */
    int rows_to_next_1d_row;

    /*! \brief The number of runs currently in the reference row. */
    int ref_steps;

    /*! \brief Pointer to the byte containing the next image bit to transmit. */
    int bit_pos;
    /*! \brief Pointer to the bit within the byte containing the next image bit to transmit. */
    int bit_ptr;

    /*! \brief Callback function to read a row of pixels from the image source. */
    t4_row_read_handler_t row_read_handler;
    /*! \brief Opaque pointer passed to row_read_handler. */
    void *row_read_user_data;
};

#endif
/*- End of file ------------------------------------------------------------*/
