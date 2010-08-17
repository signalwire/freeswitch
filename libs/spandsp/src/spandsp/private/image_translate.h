/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/image_translate.c - Image translation routines for reworking colour
 *                             and gray scale images to be bi-level images of an
 *                             appropriate size to be FAX compatible.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_IMAGE_TRANSLATE_H_)
#define _SPANDSP_PRIVATE_IMAGE_TRANSLATE_H_

struct image_translate_state_s
{
    int input_format;
    int input_width;
    int input_length;
    int output_width;
    int output_length;
    int resize;
    int bytes_per_pixel;
    int raw_input_row;
    int raw_output_row;
    int output_row;

    uint8_t *raw_pixel_row[2];
    uint8_t *pixel_row[2];

    t4_row_read_handler_t row_read_handler;
    void *row_read_user_data;
};

#endif
/*- End of file ------------------------------------------------------------*/
