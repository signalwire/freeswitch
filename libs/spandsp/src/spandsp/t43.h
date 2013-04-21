/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t43.h - ITU T.43 JBIG for grey and colour FAX image processing
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

/*! \file */

#if !defined(_SPANDSP_T43_H_)
#define _SPANDSP_T43_H_

/*! \page t43_page T.43 (JBIG for gray and colour FAX) image compression and decompression

\section t43_page_sec_1 What does it do?

\section t43_page_sec_1 How does it work?
*/

/*! State of a working instance of the T.43 encoder */
typedef struct t43_encode_state_s t43_encode_state_t;

/*! State of a working instance of the T.43 decoder */
typedef struct t43_decode_state_s t43_decode_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(const char *) t43_image_type_to_str(int type);

SPAN_DECLARE(void) t43_encode_set_options(t43_encode_state_t *s,
                                          uint32_t l0,
                                          int mx,
                                          int options);

SPAN_DECLARE(int) t43_encode_set_image_width(t43_encode_state_t *s, uint32_t image_width);

SPAN_DECLARE(int) t43_encode_set_image_length(t43_encode_state_t *s, uint32_t length);

SPAN_DECLARE(void) t43_encode_abort(t43_encode_state_t *s);

SPAN_DECLARE(void) t43_encode_comment(t43_encode_state_t *s, const uint8_t comment[], size_t len);

/*! \brief Check if we are at the end of the current document page.
    \param s The T.43 context.
    \return 0 for more data to come. SIG_STATUS_END_OF_DATA for no more data. */
SPAN_DECLARE(int) t43_encode_image_complete(t43_encode_state_t *s);

SPAN_DECLARE(int) t43_encode_get(t43_encode_state_t *s, uint8_t buf[], size_t max_len);

SPAN_DECLARE(uint32_t) t43_encode_get_image_width(t43_encode_state_t *s);

SPAN_DECLARE(uint32_t) t43_encode_get_image_length(t43_encode_state_t *s);

SPAN_DECLARE(int) t43_encode_get_compressed_image_size(t43_encode_state_t *s);

SPAN_DECLARE(int) t43_encode_set_row_read_handler(t43_encode_state_t *s,
                                                  t4_row_read_handler_t handler,
                                                  void *user_data);

/*! Get the logging context associated with a T.43 encode context.
    \brief Get the logging context associated with a T.43 encode context.
    \param s The T.43 encode context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t43_encode_get_logging_state(t43_encode_state_t *s);

/*! \brief Restart a T.43 encode context.
    \param s The T.43 context.
    \param image image_width The image width, in pixels.
    \param image image_width The image length, in pixels.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t43_encode_restart(t43_encode_state_t *s, uint32_t image_width, uint32_t image_length);

/*! \brief Prepare to encode an image in T.43 format.
    \param s The T.43 context.
    \param image_width Image width, in pixels.
    \param image_length Image length, in pixels.
    \param handler A callback routine to handle encoded image rows.
    \param user_data An opaque pointer passed to handler.
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(t43_encode_state_t *) t43_encode_init(t43_encode_state_t *s,
                                                   uint32_t image_width,
                                                   uint32_t image_length,
                                                   t4_row_read_handler_t handler,
                                                   void *user_data);

/*! \brief Release a T.43 encode context.
    \param s The T.43 encode context.
    \return 0 for OK, else -1. */
SPAN_DECLARE(int) t43_encode_release(t43_encode_state_t *s);

/*! \brief Free a T.43 encode context.
    \param s The T.43 encode context.
    \return 0 for OK, else -1. */
SPAN_DECLARE(int) t43_encode_free(t43_encode_state_t *s);

SPAN_DECLARE(int) t43_create_header(t43_decode_state_t *s, uint8_t data[], size_t len);

SPAN_DECLARE(void) t43_decode_rx_status(t43_decode_state_t *s, int status);

/*! \brief Decode a chunk of T.43 data.
    \param s The T.43 context.
    \param data The data to be decoded.
    \param len The length of the data to be decoded.
    \return 0 for OK. */
SPAN_DECLARE(int) t43_decode_put(t43_decode_state_t *s, const uint8_t data[], size_t len);

/*! \brief Set the row handler routine.
    \param s The T.43 context.
    \param handler A callback routine to handle decoded image rows.
    \param user_data An opaque pointer passed to handler.
    \return 0 for OK. */
SPAN_DECLARE(int) t43_decode_set_row_write_handler(t43_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data);

/*! \brief Set the comment handler routine.
    \param s The T.43 context.
    \param max_comment_len The maximum length of comment to be passed to the handler.
    \param handler A callback routine to handle decoded comment.
    \param user_data An opaque pointer passed to handler.
    \return 0 for OK. */
SPAN_DECLARE(int) t43_decode_set_comment_handler(t43_decode_state_t *s,
                                                 uint32_t max_comment_len,
                                                 t4_row_write_handler_t handler,
                                                 void *user_data);

SPAN_DECLARE(int) t43_decode_set_image_size_constraints(t43_decode_state_t *s,
                                                        uint32_t max_xd,
                                                        uint32_t max_yd);

/*! \brief Get the width of the image.
    \param s The T.43 context.
    \return The width of the image, in pixels. */
SPAN_DECLARE(uint32_t) t43_decode_get_image_width(t43_decode_state_t *s);

/*! \brief Get the length of the image.
    \param s The T.43 context.
    \return The length of the image, in pixels. */
SPAN_DECLARE(uint32_t) t43_decode_get_image_length(t43_decode_state_t *s);

SPAN_DECLARE(int) t43_decode_get_compressed_image_size(t43_decode_state_t *s);

/*! Get the logging context associated with a T.43 decode context.
    \brief Get the logging context associated with a T.43 decode context.
    \param s The T.43 decode context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t43_decode_get_logging_state(t43_decode_state_t *s);

SPAN_DECLARE(int) t43_decode_restart(t43_decode_state_t *s);

/*! \brief Prepare to decode an image in T.43 format.
    \param s The T.43 context.
    \param handler A callback routine to handle decoded image rows.
    \param user_data An opaque pointer passed to handler.
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(t43_decode_state_t *) t43_decode_init(t43_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data);

/*! \brief Release a T.43 decode context.
    \param s The T.43 decode context.
    \return 0 for OK, else -1. */
SPAN_DECLARE(int) t43_decode_release(t43_decode_state_t *s);

/*! \brief Free a T.43 decode context.
    \param s The T.43 decode context.
    \return 0 for OK, else -1. */
SPAN_DECLARE(int) t43_decode_free(t43_decode_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
