/*
 * SpanDSP - a series of DSP components for telephony
 *
 * image_translate.h - Image translation routines for reworking colour
 *                     and gray scale images to be colour, gray scale or
 *                     bi-level images of an appropriate size to be FAX
 *                     compatible.
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

The image translate functions allow an image to be translated and resized between
various colour an monochrome formats. It also allows a colour or gray-scale image
to be reduced to a bi-level monochrome image. This is useful for preparing images
to be sent as traditional bi-level FAX pages.

\section image_translate_page_sec_2 How does it work?

\section image_translate_page_sec_3 How do I use it?
*/

typedef struct image_translate_state_s image_translate_state_t;

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

/*! \brief Set the row read callback routine for an image translation context.
    \param s The image translation context.
    \param row_read_handler A callback routine used to pull rows of pixels from the source image
           into the translation process.
    \param row_read_user_data An opaque pointer passed to read_row_handler
    \return 0 for success, else -1. */
SPAN_DECLARE(int) image_translate_set_row_read_handler(image_translate_state_t *s, t4_row_read_handler_t row_read_handler, void *row_read_user_data);

/*! \brief Initialise an image translation context for rescaling and squashing a gray scale
           or colour image to a bi-level FAX type image.
    \param s The image translation context.
    \param output_format The type of output image
    \param output_width The width of the output image, in pixels. If this is set <= 0 the image
           will not be resized.
    \param output_length The length of the output image, in pixels. If this is set to <= 0 the
           output length will be derived automatically from the width, to maintain the geometry
           of the original image.
    \param input_format The type of source image
    \param input_width The width of the source image, in pixels.
    \param input_length The length of the source image, in pixels.
    \param row_read_handler A callback routine used to pull rows of pixels from the source image
           into the translation process.
    \param row_read_user_data An opaque pointer passed to read_row_handler
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(image_translate_state_t *) image_translate_init(image_translate_state_t *s,
                                                             int output_format,
                                                             int output_width,
                                                             int output_length,
                                                             int input_format,
                                                             int input_width,
                                                             int input_length,
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
