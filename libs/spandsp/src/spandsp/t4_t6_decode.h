/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_t6_decode.h - definitions for T.4/T.6 fax decoding
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2009 Steve Underwood
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

#if !defined(_SPANDSP_T4_T6_DECODE_H_)
#define _SPANDSP_T4_T6_DECODE_H_

/*! \page t4_t6_decode_page T.4 and T.6 FAX image decompression

\section t4_t6_decode_page_sec_1 What does it do?
The T.4 image compression and decompression routines implement the 1D and 2D
encoding methods defined in ITU specification T.4. They also implement the pure
2D encoding method defined in T.6. These are image compression algorithms used
for FAX transmission.

\section t4_t6_decode_page_sec_1 How does it work?
*/

typedef struct t4_t6_decode_state_s t4_t6_decode_state_t;

#if defined(__cplusplus)
extern "C" {
#endif

/*! \brief Put a bit of the current document page.
    \param s The T.4/T.6 context.
    \param bit The data bit.
    \return Decode status. */
SPAN_DECLARE(int) t4_t6_decode_put_bit(t4_t6_decode_state_t *s, int bit);

/*! \brief Put a byte of the current document page.
    \param s The T.4/T.6 context.
    \param buf The buffer containing the chunk.
    \param len The length of the chunk.
    \return T4_DECODE_MORE_DATA when the image is still in progress. T4_DECODE_OK when the image is complete. */
SPAN_DECLARE(int) t4_t6_decode_put(t4_t6_decode_state_t *s, const uint8_t buf[], size_t len);

/*! \brief Set the row write handler for a T.4/T.6 decode context.
    \param s The T.4/T.6 context.
    \param handler A pointer to the handler routine.
    \param user_data An opaque pointer passed to the handler routine.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_t6_decode_set_row_write_handler(t4_t6_decode_state_t *s, t4_row_write_handler_t handler, void *user_data);

/*! \brief Set the encoding for the encoded data.
    \param s The T.4/T.6 context.
    \param encoding The encoding.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_t6_decode_set_encoding(t4_t6_decode_state_t *s, int encoding);

/*! \brief Get the width of the image.
    \param s The T.4/T.6 context.
    \return The width of the image, in pixels. */
SPAN_DECLARE(uint32_t) t4_t6_decode_get_image_width(t4_t6_decode_state_t *s);

/*! \brief Get the length of the image.
    \param s The T.4/T.6 context.
    \return The length of the image, in pixels. */
SPAN_DECLARE(uint32_t) t4_t6_decode_get_image_length(t4_t6_decode_state_t *s);

/*! \brief Get the size of the compressed image, in bits.
    \param s The T.4/T.6 context.
    \return The size of the compressed image, in bits. */
SPAN_DECLARE(int) t4_t6_decode_get_compressed_image_size(t4_t6_decode_state_t *s);

/*! Get the logging context associated with a T.4 or T.6 decode context.
    \brief Get the logging context associated with a T.4 or T.6 decode context.
    \param s The T.4/T.6 context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t4_t6_decode_get_logging_state(t4_t6_decode_state_t *s);

SPAN_DECLARE(int) t4_t6_decode_restart(t4_t6_decode_state_t *s, int image_width);

/*! \brief Prepare to decode an image in T.4 or T.6 format.
    \param s The T.4/T.6 context.
    \param encoding The encoding mode.
    \param image width The image width, in pixels.
    \param handler A callback routine to handle decoded image rows.
    \param user_data An opaque pointer passed to handler.
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(t4_t6_decode_state_t *) t4_t6_decode_init(t4_t6_decode_state_t *s,
                                                       int encoding,
                                                       int image_width,
                                                       t4_row_write_handler_t handler,
                                                       void *user_data);

SPAN_DECLARE(int) t4_t6_decode_release(t4_t6_decode_state_t *s);

SPAN_DECLARE(int) t4_t6_decode_free(t4_t6_decode_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
