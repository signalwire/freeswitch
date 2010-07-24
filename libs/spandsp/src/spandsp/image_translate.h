/*
 * SpanDSP - a series of DSP components for telephony
 *
 * image_translate.h - Image translation routines for reworking colour
 *                     and gray scale images to be bi-level images of an
 *                     appropriate size to be FAX compatible.
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

/*! \file */

#if !defined(_SPANDSP_IMAGE_TRANSLATE_H_)
#define _SPANDSP_IMAGE_TRANSLATE_H_

/*! \page image_translate_page Image translation
\section image_translate_page_sec_1 What does it do?

\section image_translate_page_sec_2 How does it work?

\section image_translate_page_sec_3 How do I use it?
*/

typedef struct image_translate_state_s image_translate_state_t;

enum
{
    IMAGE_TRANSLATE_FROM_MONO = 1,
    IMAGE_TRANSLATE_FROM_GRAY_8 = 2,
    IMAGE_TRANSLATE_FROM_GRAY_16 = 3,
    IMAGE_TRANSLATE_FROM_COLOUR_8 = 4,
    IMAGE_TRANSLATE_FROM_COLOUR_16 = 5
};

#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Get the next row of a translated image.
    \param s The image translation context.
    \return the length of the row buffer, in bytes */
SPAN_DECLARE(int) image_translate_row(image_translate_state_t *s, uint8_t buf[], size_t len);

/*! \brief Get the width of the image being produced by an image translation context.
    \param s The image translation context.
    \return The width of the output image, in pixel. */
SPAN_DECLARE(int) image_translate_get_output_width(image_translate_state_t *s);

/*! \brief Get the length of the image being produced by an image translation context.
    \param s The image translation context.
    \return The length of the output image, in pixel. */
SPAN_DECLARE(int) image_translate_get_output_length(image_translate_state_t *s);

/*! \brief Initialise an image translation context for rescaling and squashing a gray scale
           or colour image to a bi-level FAX type image.
    \param s The image translation context.
    \param input_format x
    \param input_width The width of the source image, in pixels.
    \param input_length The length of the source image, in pixels.
    \param output_width The width of the output image, in pixels. The length of the output image
           will be derived automatically from this and the source image dimension, to main the
           geometry of the original image.
    \param row_read_handler A callback routine used to pull rows of pixels from the source image
           into the translation process.
    \param row_read_user_data An opaque point passed to read_row_handler
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(image_translate_state_t *) image_translate_init(image_translate_state_t *s,
                                                             int input_format,
                                                             int input_width,
                                                             int input_length,
                                                             int output_width,
                                                             t4_row_read_handler_t row_read_handler,
                                                             void *row_read_user_data);

/*! \brief Release the resources associated with an image translation context.
    \param s The image translation context.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) image_translate_release(image_translate_state_t *s);

/*! \brief Free the resources associated with an image translation context.
    \param s The image translation context.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) image_translate_free(image_translate_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
