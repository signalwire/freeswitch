/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t85.h - ITU T.85 JBIG for FAX image processing
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

/*! \file */

#if !defined(_SPANDSP_T85_H_)
#define _SPANDSP_T85_H_

/*! \page t85_page T.85 (JBIG for FAX) image compression and decompression

\section t85_page_sec_1 What does it do?
The T.85 image compression and decompression routines implement the variant of the
JBIG encoding method defined in ITU specification T.85. This is an image compression
algorithm used for black and white FAX transmission. T.85 defines a subset of the
full JBIG spec (T.82), which only handled a single progressively scanned bit plane.
This results in a great deal of simplification, and results in the ability to
compress or decompress progressively, while only buffering the latest 3 pixel rows
of the image.

\section t85_page_sec_1 How does it work?
*/

/*! Bits in the option byte of the T.82 BIH which are valid for T.85 */
enum
{
    /*! Enable typical prediction (bottom) */
    T85_TPBON = 0x08,
    /*! Variable length image */
    T85_VLENGTH = 0x20,
    /*! Lowest-resolution-layer is a two-line template */
    T85_LRLTWO = 0x40
};

/*! State of a working instance of the T.85 encoder */
typedef struct t85_encode_state_s t85_encode_state_t;

/*! State of a working instance of the T.85 decoder */
typedef struct t85_decode_state_s t85_decode_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(bool) t85_analyse_header(uint32_t *width, uint32_t *length, const uint8_t data[], size_t len);

/*! \brief Check if we are at the end of the current document page.
    \param s The T.85 context.
    \return 0 for more data to come. SIG_STATUS_END_OF_DATA for no more data. */
SPAN_DECLARE(int) t85_encode_image_complete(t85_encode_state_t *s);

/*! \brief Get the next chunk of the current document page. The document will
           be padded for the current minimum scan line time.
    \param s The T.85 context.
    \param buf The buffer into which the chunk is to written.
    \param max_len The maximum length of the chunk.
    \return The actual length of the chunk. If this is less than max_len it
            indicates that the end of the document has been reached. */
SPAN_DECLARE(int) t85_encode_get(t85_encode_state_t *s, uint8_t buf[], size_t max_len);

/*! \brief Set the row read handler for a T.85 encode context.
    \param s The T.85 context.
    \param handler A pointer to the handler routine.
    \param user_data An opaque pointer passed to the handler routine.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t85_encode_set_row_read_handler(t85_encode_state_t *s,
                                                  t4_row_read_handler_t handler,
                                                  void *user_data);

/*! Get the logging context associated with a T.85 encode context.
    \brief Get the logging context associated with a T.85 encode context.
    \param s The T.85 encode context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t85_encode_get_logging_state(t85_encode_state_t *s);

/*! \brief Set the T.85 options
    \param s The T.85 context.
    \brief l0 ???
    \brief mx ???
    \brief options ???. */
SPAN_DECLARE(void) t85_encode_set_options(t85_encode_state_t *s,
                                          uint32_t l0,
                                          int mx,
                                          int options);

/*! \brief Insert a comment in the encoded file.
    \param s The T.85 context.
    \param comment The comment. Note that this is not a C string, and may contain any bytes.
    \param len The length of the comment. */
SPAN_DECLARE(void) t85_encode_comment(t85_encode_state_t *s,
                                      const uint8_t comment[],
                                      size_t len);

/*! \brief Set the image width.
    \param s The T.85 context.
    \param image_width The width of the image.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t85_encode_set_image_width(t85_encode_state_t *s, uint32_t image_width);

/*! \brief Alter the length of a T.85 encoded image. The new length cannot be greater than the
           originally specified length. If the new length is less than the current length it
           will be silently adjusted to the current length. Therefore, adjust the length to 1
           will make the currently encoded length the final length.
    \param s The T.85 context.
    \param image_length The new image length, in pixels.
    \return 0 if OK, or -1 if the request was not valid. */
SPAN_DECLARE(int) t85_encode_set_image_length(t85_encode_state_t *s, uint32_t image_length);

/*! \brief Get the width of the image.
    \param s The T.85 context.
    \return The width of the image, in pixels. */
SPAN_DECLARE(uint32_t) t85_encode_get_image_width(t85_encode_state_t *s);

/*! \brief Get the length of the image.
    \param s The T.85 context.
    \return The length of the image, in pixels. */
SPAN_DECLARE(uint32_t) t85_encode_get_image_length(t85_encode_state_t *s);

/*! \brief Get the size of the compressed image, in bits.
    \param s The T.85 context.
    \return The size of the compressed image, in bits. */
SPAN_DECLARE(int) t85_encode_get_compressed_image_size(t85_encode_state_t *s);

/*! \brief Stop image encoding prematurely.
    \param s The T.85 context. */
SPAN_DECLARE(void) t85_encode_abort(t85_encode_state_t *s);

/*! \brief Restart a T.85 encode context.
    \param s The T.85 context.
    \param image_width The image width, in pixels.
    \param image_length The image length, in pixels.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t85_encode_restart(t85_encode_state_t *s,
                                     uint32_t image_width,
                                     uint32_t image_length);

/*! \brief Prepare to encode an image in T.85 format.
    \param s The T.85 context.
    \param image_width The image width, in pixels.
    \param image_length The image length, in pixels.
    \param handler A callback routine to handle encoded image rows.
    \param user_data An opaque pointer passed to handler.
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(t85_encode_state_t *) t85_encode_init(t85_encode_state_t *s,
                                                   uint32_t image_width,
                                                   uint32_t image_length,
                                                   t4_row_read_handler_t handler,
                                                   void *user_data);

/*! \brief Release a T.85 encode context.
    \param s The T.85 encode context.
    \return 0 for OK, else -1. */
SPAN_DECLARE(int) t85_encode_release(t85_encode_state_t *s);

/*! \brief Free a T.85 encode context.
    \param s The T.85 encode context.
    \return 0 for OK, else -1. */
SPAN_DECLARE(int) t85_encode_free(t85_encode_state_t *s);

/*! Get the logging context associated with a T.85 decode context.
    \brief Get the logging context associated with a T.85 decode context.
    \param s The T.85 decode context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t85_decode_get_logging_state(t85_decode_state_t *s);

/*! \brief Get the width of the image.
    \param s The T.85 context.
    \return The width of the image, in pixels. */
SPAN_DECLARE(uint32_t) t85_decode_get_image_width(t85_decode_state_t *s);

/*! \brief Get the length of the image.
    \param s The T.85 context.
    \return The length of the image, in pixels. */
SPAN_DECLARE(uint32_t) t85_decode_get_image_length(t85_decode_state_t *s);

/*! \brief Get the size of the compressed image, in bits.
    \param s The T.85 context.
    \return The size of the compressed image, in bits. */
SPAN_DECLARE(int) t85_decode_get_compressed_image_size(t85_decode_state_t *s);

SPAN_DECLARE(int) t85_decode_new_plane(t85_decode_state_t *s);

/*! \brief Set the row handler routine.
    \param s The T.85 context.
    \param handler A callback routine to handle decoded image rows.
    \param user_data An opaque pointer passed to handler.
    \return 0 for OK. */
SPAN_DECLARE(int) t85_decode_set_row_write_handler(t85_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data);

/*! \brief Set the comment handler routine.
    \param s The T.85 context.
    \param max_comment_len The maximum length of comment to be passed to the handler.
    \param handler A callback routine to handle decoded comment.
    \param user_data An opaque pointer passed to handler.
    \return 0 for OK. */
SPAN_DECLARE(int) t85_decode_set_comment_handler(t85_decode_state_t *s,
                                                 uint32_t max_comment_len,
                                                 t4_row_write_handler_t handler,
                                                 void *user_data);

/*! A maliciously constructed T.85 image could consume too much memory, and constitute
    a denial of service attack on the system. This function allows constraints to be
    applied.
    \brief Set constraints on the received image size.
    \param s The T.85 context.
    \param max_xd The maximum permitted width of the full image, in pixels
    \param max_yd The maximum permitted height of the full image, in pixels
    \return 0 for OK */
SPAN_DECLARE(int) t85_decode_set_image_size_constraints(t85_decode_state_t *s,
                                                        uint32_t max_xd,
                                                        uint32_t max_yd);

/*! After the final BIE byte has been delivered to t85_decode_put_xx(), it may still
    return T85_MORE_DATA when the T85_VLENGTH option was used, and no NEWLEN
    marker section has appeared yet. This is because such a BIE is not
    self-terminating (i.e. there could still be a NEWLEN followed by an SDNORM
    or SDRST at the very end of the final stripe, which needs to be processed
    before the final row is output. See ITU-T Recommendation T.85, Appendix I).
    Therefore, after the last byte has been delivered, call this routine to
    signal the end of the BIE. This is necessary to allow the routine to finish
    processing BIEs with option T85_VLENGTH that do not actually contain any
    NEWLEN marker section.
    \brief Inform the T.85 decode engine of a status change in the signal source (end
           of tx, rx signal change, etc.).
    \param s The T.85 context.
    \param status The type of status change which occured. */
SPAN_DECLARE(void) t85_decode_rx_status(t85_decode_state_t *s, int status);

/*! \brief Decode a chunk of T.85 data.
    \param s The T.85 context.
    \param data The data to be decoded.
    \param len The length of the data to be decoded.
    \return 0 for OK. */
SPAN_DECLARE(int) t85_decode_put(t85_decode_state_t *s, const uint8_t data[], size_t len);

SPAN_DECLARE(int) t85_decode_restart(t85_decode_state_t *s);

/*! \brief Prepare to decode an image in T.85 format.
    \param s The T.85 context.
    \param handler A callback routine to handle decoded image rows.
    \param user_data An opaque pointer passed to handler.
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(t85_decode_state_t *) t85_decode_init(t85_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data);

/*! \brief Release a T.85 decode context.
    \param s The T.85 decode context.
    \return 0 for OK, else -1. */
SPAN_DECLARE(int) t85_decode_release(t85_decode_state_t *s);

/*! \brief Free a T.85 decode context.
    \param s The T.85 decode context.
    \return 0 for OK, else -1. */
SPAN_DECLARE(int) t85_decode_free(t85_decode_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
