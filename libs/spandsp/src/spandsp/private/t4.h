/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t4.h - definitions for T.4 fax processing
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
 *
 * $Id: t4.h,v 1.4 2009/02/20 12:34:20 steveu Exp $
 */

#if !defined(_SPANDSP_PRIVATE_T4_H_)
#define _SPANDSP_PRIVATE_T4_H_

/*!
    TIFF specific state information to go with T.4 compression or decompression handling.
*/
typedef struct
{
    /*! \brief The libtiff context for the current TIFF file */
    TIFF *tiff_file;

    /*! \brief The compression type for output to the TIFF file. */
    int output_compression;
    /*! \brief The TIFF G3 FAX options. */
    int output_t4_options;
    /*! \brief The TIFF photometric setting for the current page. */
    uint16_t photo_metric;
    /*! \brief The TIFF fill order setting for the current page. */
    uint16_t fill_order;

    /* "Background" information about the FAX, which can be stored in the image file. */
    /*! \brief The vendor of the machine which produced the file. */ 
    const char *vendor;
    /*! \brief The model of machine which produced the file. */ 
    const char *model;
    /*! \brief The local ident string. */ 
    const char *local_ident;
    /*! \brief The remote end's ident string. */ 
    const char *far_ident;
    /*! \brief The FAX sub-address. */ 
    const char *sub_address;
    /*! \brief The FAX DCS information, as an ASCII string. */ 
    const char *dcs;
} t4_tiff_state_t;

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
    /*! \brief The minimum number of encoded bits per row. This is a timing thing
               for hardware FAX machines. */
    int min_bits_per_row;
    
    /*! \brief Callback function to read a row of pixels from the image source. */
    t4_row_read_handler_t row_read_handler;
    /*! \brief Opaque pointer passed to row_read_handler. */
    void *row_read_user_data;
    /*! \brief Callback function to write a row of pixels to the image destination. */
    t4_row_write_handler_t row_write_handler;
    /*! \brief Opaque pointer passed to row_write_handler. */
    void *row_write_user_data;

    /*! \brief The time at which handling of the current page began. */
    time_t page_start_time;

    /*! \brief The current number of bytes per row of uncompressed image data. */
    int bytes_per_row;
    /*! \brief The size of the image in the image buffer, in bytes. */
    int image_size;
    /*! \brief The size of the compressed image on the line side, in bits. */
    int line_image_size;
    /*! \brief The current size of the image buffer. */
    int image_buffer_size;
    /*! \brief A point to the image buffer. */
    uint8_t *image_buffer;

    /*! \brief The current file name. */
    const char *file;
    /*! \brief The first page to transfer. -1 to start at the beginning of the file. */
    int start_page;
    /*! \brief The last page to transfer. -1 to continue to the end of the file. */
    int stop_page;

    /*! \brief The number of pages transferred to date. */
    int current_page;
    /*! \brief The number of pages in the current image file. */
    int pages_in_file;
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
    /*! \brief The current number of consecutive bad rows. */
    int curr_bad_row_run;
    /*! \brief The longest run of consecutive bad rows seen in the current page. */
    int longest_bad_row_run;
    /*! \brief The total number of bad rows in the current page. */
    int bad_rows;

    /*! \brief Incoming bit buffer for decompression. */
    uint32_t rx_bitstream;
    /*! \brief The number of bits currently in rx_bitstream. */
    int rx_bits;
    /*! \brief The number of bits to be skipped before trying to match the next code word. */
    int rx_skip_bits;

    /*! \brief This variable is set if we are treating the current row as a 2D encoded
               one. */
    int row_is_2d;
    /*! \brief TRUE if the current run is black */
    int its_black;
    /*! \brief The current length of the current row. */
    int row_len;
    /*! \brief This variable is used to count the consecutive EOLS we have seen. If it
               reaches six, this is the end of the image. It is initially set to -1 for
               1D and 2D decoding, as an indicator that we must wait for the first EOL,
               before decodin any image data. */
    int consecutive_eols;

    /*! \brief Black and white run-lengths for the current row. */
    uint32_t *cur_runs;
    /*! \brief Black and white run-lengths for the reference row. */
    uint32_t *ref_runs;
    /*! \brief The number of runs currently in the reference row. */
    int ref_steps;
    /*! \brief The current step into the reference row run-lengths buffer. */
    int b_cursor;
    /*! \brief The current step into the current row run-lengths buffer. */
    int a_cursor;

    /*! \brief The reference or starting changing element on the coding line. At the
               start of the coding line, a0 is set on an imaginary white changing element
               situated just before the first element on the line. During the coding of
               the coding line, the position of a0 is defined by the previous coding mode.
               (See 4.2.1.3.2.). */
    int a0;
    /*! \brief The first changing element on the reference line to the right of a0 and of
               opposite colour to a0. */
    int b1;
    /*! \brief The length of the in-progress run of black or white. */
    int run_length;
    /*! \brief 2D horizontal mode control. */
    int black_white;

    /*! \brief Encoded data bits buffer. */
    uint32_t tx_bitstream;
    /*! \brief The number of bits currently in tx_bitstream. */
    int tx_bits;

    /*! \brief A pointer into the image buffer indicating where the last row begins */
    int last_row_starts_at;
    /*! \brief A pointer into the image buffer indicating where the current row begins */
    int row_starts_at;
    
    /*! \brief Pointer to the buffer for the current pixel row. */
    uint8_t *row_buf;
    
    /*! \brief Pointer to the byte containing the next image bit to transmit. */
    int bit_pos;
    /*! \brief Pointer to the bit within the byte containing the next image bit to transmit. */
    int bit_ptr;

    /*! \brief The current maximum contiguous rows that may be 2D encoded. */
    int max_rows_to_next_1d_row;
    /*! \brief Number of rows left that can be 2D encoded, before a 1D encoded row
               must be used. */
    int rows_to_next_1d_row;
    /*! \brief The current number of bits in the current encoded row. */
    int row_bits;
    /*! \brief The minimum bits in any row of the current page. For monitoring only. */
    int min_row_bits;
    /*! \brief The maximum bits in any row of the current page. For monitoring only. */
    int max_row_bits;

    /*! \brief The text which will be used in FAX page header. No text results
               in no header line. */
    const char *header_info;

    /*! \brief Error and flow logging control */
    logging_state_t logging;

    /*! \brief All TIFF file specific state information for the T.4 context. */
    t4_tiff_state_t tiff;
};

#endif
/*- End of file ------------------------------------------------------------*/
