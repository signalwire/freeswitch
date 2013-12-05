/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_t6_encode.h - definitions for T.4/T.6 fax encoding
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

#if !defined(_SPANDSP_T4_T6_ENCODE_H_)
#define _SPANDSP_T4_T6_ENCODE_H_

typedef struct t4_t6_encode_state_s t4_t6_encode_state_t;

#if defined(__cplusplus)
extern "C" {
#endif

/*! \brief Return the next bit of the current document page, without actually
           moving forward in the buffer. The document will be padded for the
           current minimum scan line time.
    \param s The T.4/T.6 context.
    \return 0 for more data to come. SIG_STATUS_END_OF_DATA for no more data. */
SPAN_DECLARE(int) t4_t6_encode_image_complete(t4_t6_encode_state_t *s);

/*! \brief Get the next bit of the current image. The image will
           be padded for the current minimum scan line time.
    \param s The T.4/T.6 context.
    \return The next bit (i.e. 0 or 1). SIG_STATUS_END_OF_DATA for no more data. */
SPAN_DECLARE(int) t4_t6_encode_get_bit(t4_t6_encode_state_t *s);

/*! \brief Get the next chunk of the current document page. The document will
           be padded for the current minimum scan line time.
    \param s The T.4/T.6 context.
    \param buf The buffer into which the chunk is to written.
    \param max_len The maximum length of the chunk.
    \return The actual length of the chunk. If this is less than max_len it
            indicates that the end of the document has been reached. */
SPAN_DECLARE(int) t4_t6_encode_get(t4_t6_encode_state_t *s, uint8_t buf[], int max_len);

/*! \brief Set the row read handler for a T.4/T.6 encode context.
    \param s The T.4/T.6 context.
    \param handler A pointer to the handler routine.
    \param user_data An opaque pointer passed to the handler routine.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_t6_encode_set_row_read_handler(t4_t6_encode_state_t *s,
                                                    t4_row_read_handler_t handler,
                                                    void *user_data);

/*! \brief Set the encoding for the encoded data.
    \param s The T.4/T.6 context.
    \param encoding The encoding.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_t6_encode_set_encoding(t4_t6_encode_state_t *s, int encoding);

/*! \brief Set the width of the image.
    \param s The T.4/T.6 context.
    \param image_width The image width, in pixels.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_t6_encode_set_image_width(t4_t6_encode_state_t *s, int image_width);

/*! \brief Set the length of the image.
    \param s The T.4/T.6 context.
    \param image_length The image length, in pixels.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_t6_encode_set_image_length(t4_t6_encode_state_t *s, int image_length);

/*! \brief Get the width of the image.
    \param s The T.4/T.6 context.
    \return The width of the image, in pixels. */
SPAN_DECLARE(uint32_t) t4_t6_encode_get_image_width(t4_t6_encode_state_t *s);

/*! \brief Get the length of the image.
    \param s The T.4/T.6 context.
    \return The length of the image, in pixels. */
SPAN_DECLARE(uint32_t) t4_t6_encode_get_image_length(t4_t6_encode_state_t *s);

/*! \brief Get the size of the compressed image, in bits.
    \param s The T.4/T.6 context.
    \return The size of the compressed image, in bits. */
SPAN_DECLARE(int) t4_t6_encode_get_compressed_image_size(t4_t6_encode_state_t *s);

/*! \brief Set the minimum number of encoded bits per row. This allows the
           makes the encoding process to be set to comply with the minimum row
           time specified by a remote receiving machine.
    \param s The T.4/T.6 context.
    \param bits The minimum number of bits per row. */
SPAN_DECLARE(void) t4_t6_encode_set_min_bits_per_row(t4_t6_encode_state_t *s, int bits);

/*! \brief Set the maximum number of 2D encoded rows between 1D encoded rows. This
           is only valid for T.4 2D encoding.
    \param s The T.4/T.6 context.
    \param max The "K" parameter defined in the T.4 specification. This means the value is one
           greater than the maximum number of 2D rows between each 1D row. */
SPAN_DECLARE(void) t4_t6_encode_set_max_2d_rows_per_1d_row(t4_t6_encode_state_t *s, int max);

/*! Get the logging context associated with a T.4 or T.6 encode context.
    \brief Get the logging context associated with a T.4 or T.6 encode context.
    \param s The T.4/T.6 context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t4_t6_encode_get_logging_state(t4_t6_encode_state_t *s);

/*! \brief Restart a T.4 or T.6 encode context.
    \param s The T.4/T.6 context.
    \param image_width The image width, in pixels.
    \param image_length The image length, in pixels. This can be set to -1, if the length is not known.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_t6_encode_restart(t4_t6_encode_state_t *s, int image_width, int image_length);

/*! \brief Prepare to encode an image in T.4 or T.6 format.
    \param s The T.4/T.6 context.
    \param encoding The encoding mode.
    \param image_width The image width, in pixels.
    \param image_length The image length, in pixels. This can be set to -1, if the length is not known.
    \param handler A callback routine to handle decoded image rows.
    \param user_data An opaque pointer passed to handler.
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(t4_t6_encode_state_t *) t4_t6_encode_init(t4_t6_encode_state_t *s,
                                                       int encoding,
                                                       int image_width,
                                                       int image_length,
                                                       t4_row_read_handler_t handler,
                                                       void *user_data);

SPAN_DECLARE(int) t4_t6_encode_release(t4_t6_encode_state_t *s);

SPAN_DECLARE(int) t4_t6_encode_free(t4_t6_encode_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
