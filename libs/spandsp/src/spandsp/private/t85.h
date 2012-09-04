/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t85.h - ITU T.85 JBIG for FAX image processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008, 2009 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_T85_H_)
#define _SPANDSP_PRIVATE_T85_H_

/* Maximum number of ATMOVEs per stripe that the decoder can handle */
#define T85_ATMOVES_MAX  1

/* TP special pixels */
#define TPB2CX 0x195
#define TPB3CX 0x0E5

/* T.82 table 2 - symbolic constants */
enum
{
    T82_STUFF = 0x00,
    T82_RESERVE = 0x01,
    T82_SDNORM = 0x02,
    T82_SDRST = 0x03,
    T82_ABORT = 0x04,
    T82_NEWLEN = 0x05,
    T82_ATMOVE = 0x06,
    T82_COMMENT = 0x07,
    T82_ESC = 0xFF
};

/* State of a working instance of the T.85 JBIG FAX encoder */
struct t85_encode_state_s
{
    /*! \brief Callback function to read a row of pixels from the image source. */
    t4_row_read_handler_t row_read_handler;
    /*! \brief Opaque pointer passed to row_read_handler. */
    void *row_read_user_data;

    /*! The number of bit planes. Always 1 for true T.85 */
    uint8_t bit_planes;
    uint8_t current_bit_plane;
    /*! The width of the full image, in pixels */
    uint32_t xd;
    /*! The height of the full image, in pixels */
    uint32_t yd;
    /*! The number of rows per stripe */
    uint32_t l0;
    /*! Maximum ATMOVE window size (0 - 127) */
    int mx;
    /*! Encoding parameters */
    int options;
    /*! The contents for a COMMENT marker segment, to be added to the
        image at the next opportunity. This is set to NULL when nothing is
        pending. */
    const uint8_t *comment;
    /*! Length of data pointed to by comment */
    size_t comment_len;

    /*! Next row number to be encoded */
    uint32_t y;
    /*! Next row within current stripe */
    uint32_t i;
    /*! Flag for handling NEWLEN processing. */
    int newlen;
    /*! X-offset of adaptive template pixel */
    int32_t tx;
    /*! Adaptive template algorithm variables */
    uint32_t c_all;
    /*! Adaptive template algorithm variables */
    uint32_t c[128];
    /*! New TX value, or <0 for analysis in progress */
    int32_t new_tx;
    /*! TRUE if previous row was typical */
    int prev_ltp;
    /*! Pointers to the 3 row buffers */
    uint8_t *prev_row[3];
    /*! Pointer to a block of allocated memory 3 rows long, which
        we divide up for the 3 row buffers. */
    uint8_t *row_buf;
    uint8_t *bitstream;
    int bitstream_len;
    int bitstream_iptr;
    int bitstream_optr;
    int fill_with_white;

    /*! \brief The size of the compressed image, in bytes. */
    int compressed_image_size;

    /*! Arithmetic encoder state */
    t81_t82_arith_encode_state_t s;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

/* State of a working instance of the T.85 JBIG FAX decoder */
struct t85_decode_state_s
{
    /*! A callback routine to handle decoded pixel rows */
    t4_row_write_handler_t row_write_handler;
    /*! An opaque pointer passed to row_write_handler() */
    void *row_write_user_data;
    /*! A callback routine to handle decoded comments */
    t4_row_write_handler_t comment_handler;
    /*! An opaque pointer passed to comment_handler() */
    void *comment_user_data;
    /*! The maximum length of comment to be passed to the comment handler */
    uint32_t max_comment_len;

    uint8_t min_bit_planes;
    uint8_t max_bit_planes;
    /*! The maximum permitted width of the full image, in pixels */
    uint32_t max_xd;
    /*! The maximum permitted height of the full image, in pixels */
    uint32_t max_yd;

    /*! The number of bit planes expected, according to the header. Always 1 for true T.85 */
    uint8_t bit_planes;
    uint8_t current_bit_plane;
    
    /*! The width of the full image, in pixels */
    uint32_t xd;
    /*! The height of the full image, in pixels */
    uint32_t yd;
    /*! The number of rows per stripe */
    uint32_t l0;
    /*! Maximum ATMOVE window size */
    int mx;
    /*! Encoding parameters */
    int options;

    /*! The current row and the previous 2 rows of image data */
    int p[3];
    /*! Pointers to the 3 row buffers */
    uint8_t *prev_row[3];
    /*! Pointer to a block of allocated memory 3 rows long, which
        we divide up for the 3 row buffers. */
    uint8_t *row_buf;
    /*! The length of the row buffer */
    int row_buf_len;
    /*! Bytes per pixel row */
    size_t bytes_per_row;
    /*! X-offset of AT pixel */
    int32_t tx;
    /*! Number of bytes read so far */
    uint32_t bie_len;
    /*! Buffer space for the BIH or marker segments fragments */
    uint8_t buffer[20];
    /*! Number of bytes in buffer. */
    int buf_len;
    /*! Required number of bytes in buffer to proceed with processing
        its contents. */
    int buf_needed;
    /*! The content of a decoded COMMENT marker segment. */
    uint8_t *comment;
    /*! The expected length of a decoded COMMENT segment */
    uint32_t comment_len;
    /*! The length of COMMENT decoded to date */
    uint32_t comment_progress;
    /*! Current column */
    uint32_t x;
    /*! Current row */
    uint32_t y;
    /*! Current row within the current stripe */
    uint32_t i;
    /*! Number of AT moves in the current stripe */
    int at_moves;
    /*! Rows at which an AT move will happen */
    uint32_t at_row[T85_ATMOVES_MAX];
    /*! ATMOVE x-offsets in current stripe */
    int at_tx[T85_ATMOVES_MAX];
    /*! Working data for decode_pscd() */
    uint32_t row_h[3];
    /*! Flag for TPBON/TPDON: next pixel is a pseudo pixel */
    int pseudo;
    /*! Line is not typical flag. */
    int lntp;
    /*! Flag that row_write_handler() requested an interrupt. */
    int interrupt;
    /*! Flag that the data to be decoded has run out. */
    int end_of_data;
    /*! Arithmetic decoder state */
    t81_t82_arith_decode_state_t s;

    /*! \brief The size of the compressed image, in bytes. */
    int compressed_image_size;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
