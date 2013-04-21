/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t43.h - ITU T.43 JBIG for gray and colour FAX image processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_T43_H_)
#define _SPANDSP_PRIVATE_T43_H_

/* State of a working instance of the T.43 JBIG for gray and colour FAX encoder */
struct t43_encode_state_s
{
    /*! \brief Callback function to read a row of pixels from the image source. */
    t4_row_read_handler_t row_read_handler;
    /*! \brief Opaque pointer passed to row_read_handler. */
    void *row_read_user_data;

    struct lab_params_s lab;
    struct t85_encode_state_s t85;

    int bit_planes[4];

    int colour_map_entries;
    uint8_t colour_map[3*256];

    /*! The width of the full image, in pixels */
    uint32_t xd;
    /*! The height of the full image, in pixels */
    uint32_t yd;
    int x_resolution;
    int y_resolution;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

/* State of a working instance of the T.43 JBIG for gray and colour FAX decoder */
struct t43_decode_state_s
{
    /*! A callback routine to handle decoded pixel rows */
    t4_row_write_handler_t row_write_handler;
    /*! An opaque pointer passed to row_write_handler() */
    void *row_write_user_data;

    struct lab_params_s lab;
    struct t85_decode_state_s t85;

    int bit_planes[4];
    uint8_t bit_plane_mask;
    int current_bit_plane;
    int plane_ptr;

    int colour_map_entries;
    uint8_t colour_map[3*256];

    int x_resolution;
    int y_resolution;

    uint8_t *buf;
    int ptr;
    int row;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
