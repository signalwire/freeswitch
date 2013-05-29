/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t4_rx.h - definitions for T.4 FAX receive processing
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

#if !defined(_SPANDSP_PRIVATE_T4_RX_H_)
#define _SPANDSP_PRIVATE_T4_RX_H_

/*!
    TIFF specific state information to go with T.4 compression or decompression handling.
*/
typedef struct
{
    /*! \brief The current file name. */
    const char *file;
    /*! \brief The libtiff context for the current TIFF file */
    TIFF *tiff_file;

    /*! Image type - bilevel, gray, colour */
    int image_type;
    /*! \brief The compression type for output to the TIFF file. */
    int compression;
    /*! \brief The TIFF photometric setting for the current page. */
    uint16_t photo_metric;
    /*! \brief The TIFF fill order setting for the current page. */
    uint16_t fill_order;

    /*! \brief The number of pages in the current image file. */
    int pages_in_file;

    /*! \brief The time at which handling of the current page began. */
    time_t page_start_time;

    /*! \brief A point to the image buffer. */
    uint8_t *image_buffer;
    /*! \brief The size of the image in the image buffer, in bytes. */
    int image_size;
    /*! \brief The current size of the image buffer. */
    int image_buffer_size;
} t4_rx_tiff_state_t;

/*!
    T.4 FAX decompression metadata descriptor. This contains information about the image
    which may be relevant to the backend, but is not relevant to the image decoding process.
*/
typedef struct
{
    /*! \brief The type of compression used on the wire. */
    int compression;
    /*! \brief The width of the current page, in pixels. */
    uint32_t image_width;
    /*! \brief The length of the current page, in pixels. */
    uint32_t image_length;
    /*! \brief Column-to-column (X) resolution in pixels per metre. */
    int x_resolution;
    /*! \brief Row-to-row (Y) resolution in pixels per metre. */
    int y_resolution;

    /* "Background" information about the FAX, which can be stored in the image file. */
    /*! \brief The vendor of the machine which produced the file. */
    const char *vendor;
    /*! \brief The model of machine which produced the file. */
    const char *model;
    /*! \brief The remote end's ident string. */
    const char *far_ident;
    /*! \brief The FAX sub-address. */
    const char *sub_address;
    /*! \brief The FAX DCS information, as an ASCII hex string. */
    const char *dcs;
} t4_rx_metadata_t;

/*!
    T.4 FAX decompression descriptor. This defines the working state
    for a single instance of a T.4 FAX decompression channel.
*/
struct t4_rx_state_s
{
    /*! \brief Callback function to write a row of pixels to the image destination. */
    t4_row_write_handler_t row_handler;
    /*! \brief Opaque pointer passed to row_write_handler. */
    void *row_handler_user_data;

    /*! \brief A bit mask of the currently supported image compression modes for writing
               to the TIFF file. */
    int supported_tiff_compressions;

    /*! \brief The number of pages transferred to date. */
    int current_page;

    /*! \brief The size of the compressed image on the line side, in bits. */
    int line_image_size;

    union
    {
        t4_t6_decode_state_t t4_t6;
        t85_decode_state_t t85;
#if defined(SPANDSP_SUPPORT_T88)
        t88_decode_state_t t88;
#endif
        t42_decode_state_t t42;
#if defined(SPANDSP_SUPPORT_T43)
        t43_decode_state_t t43;
#endif
#if defined(SPANDSP_SUPPORT_T45)
        t45_decode_state_t t45;
#endif
    } decoder;
    int current_decoder;

    uint8_t *pre_encoded_buf;
    int pre_encoded_len;
    int pre_encoded_ptr;
    int pre_encoded_bit;

    /* Supporting information, like resolutions, which the backend may want. */
    t4_rx_metadata_t metadata;

    /*! \brief All TIFF file specific state information for the T.4 context. */
    t4_rx_tiff_state_t tiff;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
