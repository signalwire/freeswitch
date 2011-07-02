/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t4_tx.h - definitions for T.4 FAX transmit processing
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

#if !defined(_SPANDSP_PRIVATE_T4_TX_H_)
#define _SPANDSP_PRIVATE_T4_TX_H_

/*!
    T.4 FAX compression/decompression descriptor. This defines the working state
    for a single instance of a T.4 FAX compression or decompression channel.
*/
struct t4_state_s
{
    /*! \brief The same structure is used for T.4 transmit and receive. This variable
               records which mode is in progress. */
    int rx;

    /*! \brief The type of compression used between the FAX machines. */
    int line_encoding;

    /*! \brief The time at which handling of the current page began. */
    time_t page_start_time;

    /*! \brief TRUE for FAX page headers to overlay (i.e. replace) the beginning of the
               page image. FALSE for FAX page headers to add to the overall length of
               the page. */
    int header_overlays_image;
    /*! \brief The text which will be used in FAX page header. No text results
               in no header line. */
    const char *header_info;
    /*! \brief Optional per instance time zone for the FAX pager header timestamp. */
    struct tz_s *tz;

    /*! \brief The size of the compressed image on the line side, in bits. */
    int line_image_size;

    /*! \brief The current number of bytes per row of uncompressed image data. */
    int bytes_per_row;
    /*! \brief The size of the image in the image buffer, in bytes. */
    int image_size;
    /*! \brief The current size of the image buffer. */
    int image_buffer_size;
    /*! \brief A point to the image buffer. */
    uint8_t *image_buffer;

    /*! \brief The number of pages transferred to date. */
    int current_page;
    /*! \brief Column-to-column (X) resolution in pixels per metre. */
    int x_resolution;
    /*! \brief Row-to-row (Y) resolution in pixels per metre. */
    int y_resolution;
    /*! \brief Width of the current page, in pixels. */
    int image_width;
    /*! \brief Length of the current page, in pixels. */
    int image_length;
    /*! \brief Current pixel row number. */
    int row;

    /*! \brief This variable is set if we are treating the current row as a 2D encoded
               one. */
    int row_is_2d;
    /*! \brief The current length of the current row. */
    int row_len;

    /*! \brief Black and white run-lengths for the current row. */
    uint32_t *cur_runs;
    /*! \brief Black and white run-lengths for the reference row. */
    uint32_t *ref_runs;
    /*! \brief Pointer to the buffer for the current pixel row. */
    uint8_t *row_buf;

    /*! \brief Encoded data bits buffer. */
    uint32_t tx_bitstream;
    /*! \brief The number of bits currently in tx_bitstream. */
    int tx_bits;

    /*! \brief The current number of bits in the current encoded row. */
    int row_bits;
    /*! \brief The minimum bits in any row of the current page. For monitoring only. */
    int min_row_bits;
    /*! \brief The maximum bits in any row of the current page. For monitoring only. */
    int max_row_bits;

    /*! \brief Error and flow logging control */
    logging_state_t logging;

    /*! \brief All TIFF file specific state information for the T.4 context. */
    t4_tiff_state_t tiff;
    t4_t6_decode_state_t t4_t6_rx;
    t4_t6_encode_state_t t4_t6_tx;
};

#endif
/*- End of file ------------------------------------------------------------*/
