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

typedef int (*t4_image_get_handler_t)(void *user_data, uint8_t buf[], size_t len);

/*!
    TIFF specific state information to go with T.4 compression or decompression handling.
*/
typedef struct
{
    /*! \brief The current file name. */
    const char *file;
    /*! \brief The libtiff context for the current TIFF file */
    TIFF *tiff_file;

    /*! \brief The compression type used in the TIFF file */
    uint16_t compression;
    /*! \brief Image type - bi-level, gray, colour, etc. */
    int image_type;
    /*! \brief The TIFF photometric setting for the current page. */
    uint16_t photo_metric;
    /*! \brief The TIFF fill order setting for the current page. */
    uint16_t fill_order;

    /*! \brief Width of the image in the file. */
    uint32_t image_width;
    /*! \brief Length of the image in the file. */
    uint32_t image_length;
    /*! \brief Column-to-column (X) resolution in pixels per metre of the image in the file. */
    int x_resolution;
    /*! \brief Row-to-row (Y) resolution in pixels per metre of the image in the file. */
    int y_resolution;
    /*! \brief Code for the combined X and Y resolution of the image in the file. */
    int resolution_code;

    /*! \brief The number of pages in the current image file. */
    int pages_in_file;

    /*! \brief A pointer to the image buffer. */
    uint8_t *image_buffer;
    /*! \brief The size of the image in the image buffer, in bytes. */
    int image_size;
    /*! \brief The current size of the image buffer. */
    int image_buffer_size;
    /*! \brief Row counter for playing out the rows of the image. */
    int row;
    /*! \brief Row counter used when the image is resized or dithered flat. */
    int raw_row;
} t4_tx_tiff_state_t;

/*!
    T.4 FAX compression metadata descriptor. This contains information about the image
    which may be relevant to the backend, but is not generally relevant to the image
    encoding process. The exception here is the y-resolution, which is used in T.4 2-D
    encoding for non-ECM applications, to optimise the balance of density and robustness.
*/
typedef struct
{
    /*! \brief The type of compression used on the wire. */
    int compression;
    /*! \brief Image type - bi-level, gray, colour, etc. */
    int image_type;
    /*! \brief The width code for the image on the line side. */
    int width_code;

    /*! \brief The width of the current page on the wire, in pixels. */
    uint32_t image_width;
    /*! \brief The length of the current page on the wire, in pixels. */
    uint32_t image_length;
    /*! \brief Column-to-column (X) resolution in pixels per metre on the wire. */
    int x_resolution;
    /*! \brief Row-to-row (Y) resolution in pixels per metre on the wire. */
    int y_resolution;
    /*! \brief Code for the combined X and Y resolution on the wire. */
    int resolution_code;
} t4_tx_metadata_t;

/*!
    T.4 FAX compression descriptor. This defines the working state
    for a single instance of a T.4 FAX compression channel.
*/
struct t4_tx_state_s
{
    /*! \brief Callback function to read a row of pixels from the image source. */
    t4_row_read_handler_t row_handler;
    /*! \brief Opaque pointer passed to row_read_handler. */
    void *row_handler_user_data;

    /*! \brief When superfine and fine resolution images need to be squahed vertically
               to a lower resolution, this value sets the number of source rows which
               must be squashed to form each row on the wire. */
    int row_squashing_ratio;

    /*! \brief The size of the compressed image on the line side, in bits. */
    int line_image_size;

    /*! \brief The first page to transfer. -1 to start at the beginning of the file. */
    int start_page;
    /*! \brief The last page to transfer. -1 to continue to the end of the file. */
    int stop_page;

    /*! \brief TRUE for FAX page headers to overlay (i.e. replace) the beginning of the
               page image. FALSE for FAX page headers to add to the overall length of
               the page. */
    int header_overlays_image;
    /*! \brief The text which will be used in FAX page header. No text results
               in no header line. */
    const char *header_info;
    /*! \brief The local ident string. This is used with header_info to form a
               page header line. */
    const char *local_ident;
    /*! \brief The page number of current page. The first page is zero. If FAX page
               headers are used, the page number in the header will be one more than
               this value (i.e. they start from 1). */
    int current_page;

    /*! \brief The composed text of the FAX page header, if there is one. */
    char *header_text;
    /*! \brief Optional per instance time zone for the FAX page header timestamp. */
    tz_t *tz;

    /*! \brief Row counter for playing out the rows of the header line. */
    int header_row;

    union
    {
        t4_t6_encode_state_t t4_t6;
        t85_encode_state_t t85;
#if defined(SPANDSP_SUPPORT_T88)
        t88_encode_state_t t88;
#endif
        t42_encode_state_t t42;
#if defined(SPANDSP_SUPPORT_T43)
        t43_encode_state_t t43;
#endif
#if defined(SPANDSP_SUPPORT_T45)
        t45_encode_state_t t45;
#endif
    } encoder;

    t4_image_get_handler_t image_get_handler;

    int apply_lab;
    lab_params_t lab_params;
    uint8_t *colour_map;
    int colour_map_entries;

    image_translate_state_t translator;
    uint8_t *pack_buf;
    int pack_ptr;
    int pack_row;
    int pack_bit_mask;

    uint8_t *pre_encoded_buf;
    int pre_encoded_len;
    int pre_encoded_ptr;
    int pre_encoded_bit;

    /*! \brief Supporting information, like resolutions, which the backend may want. */
    t4_tx_metadata_t metadata;

    /*! \brief All TIFF file specific state information for the T.4 context. */
    t4_tx_tiff_state_t tiff;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
