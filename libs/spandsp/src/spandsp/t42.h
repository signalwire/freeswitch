/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t42.h - ITU T.42 JPEG for FAX image processing
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

#if !defined(_SPANDSP_T42_H_)
#define _SPANDSP_T42_H_

/*! \page t42_page T.42 (JPEG for FAX) image compression and decompression

\section t42_page_sec_1 What does it do?

\section t42_page_sec_1 How does it work?
*/

/*! State of a working instance of the T.42 encoder */
typedef struct t42_encode_state_s t42_encode_state_t;

/*! State of a working instance of the T.42 decoder */
typedef struct t42_decode_state_s t42_decode_state_t;

typedef struct lab_params_s lab_params_t;

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(void) srgb_to_lab(lab_params_t *s, uint8_t lab[], const uint8_t srgb[], int pixels);

SPAN_DECLARE(void) lab_to_srgb(lab_params_t *s, uint8_t srgb[], const uint8_t lab[], int pixels);
    
SPAN_DECLARE(void) set_lab_illuminant(lab_params_t *s, float new_xn, float new_yn, float new_zn);

SPAN_DECLARE(void) set_lab_gamut(lab_params_t *s, int L_min, int L_max, int a_min, int a_max, int b_min, int b_max, int ab_are_signed);

SPAN_DECLARE(void) set_lab_gamut2(lab_params_t *s, int L_P, int L_Q, int a_P, int a_Q, int b_P, int b_Q);
    
SPAN_DECLARE(int) t42_itulab_to_itulab(logging_state_t *logging, tdata_t *dst, tsize_t *dstlen, tdata_t src, tsize_t srclen, uint32_t width, uint32_t height);

SPAN_DECLARE(int) t42_itulab_to_jpeg(logging_state_t *logging, lab_params_t *s, tdata_t *dst, tsize_t *dstlen, tdata_t src, tsize_t srclen);

SPAN_DECLARE(int) t42_jpeg_to_itulab(logging_state_t *logging, lab_params_t *s, tdata_t *dst, tsize_t *dstlen, tdata_t src, tsize_t srclen);

SPAN_DECLARE(int) t42_srgb_to_itulab(logging_state_t *logging, lab_params_t *s, tdata_t *dst, tsize_t *dstlen, tdata_t src, tsize_t srclen, uint32_t width, uint32_t height);

SPAN_DECLARE(int) t42_itulab_to_srgb(logging_state_t *logging, lab_params_t *s, tdata_t dst, tsize_t *dstlen, tdata_t src, tsize_t srclen, uint32_t *width, uint32_t *height);

SPAN_DECLARE(void) t42_encode_set_options(t42_encode_state_t *s,
                                          uint32_t l0,
                                          int mx,
                                          int options);

SPAN_DECLARE(int) t42_encode_set_image_width(t42_encode_state_t *s, uint32_t image_width);

SPAN_DECLARE(int) t42_encode_set_image_length(t42_encode_state_t *s, uint32_t length);

SPAN_DECLARE(void) t42_encode_abort(t42_encode_state_t *s);

SPAN_DECLARE(void) t42_encode_comment(t42_encode_state_t *s, const uint8_t comment[], size_t len);

/*! \brief Check if we are at the end of the current document page.
    \param s The T.42 context.
    \return 0 for more data to come. SIG_STATUS_END_OF_DATA for no more data. */
SPAN_DECLARE(int) t42_encode_image_complete(t42_encode_state_t *s);

SPAN_DECLARE(int) t42_encode_get(t42_encode_state_t *s, uint8_t buf[], size_t max_len);

SPAN_DECLARE(uint32_t) t42_encode_get_image_width(t42_encode_state_t *s);

SPAN_DECLARE(uint32_t) t42_encode_get_image_length(t42_encode_state_t *s);

SPAN_DECLARE(int) t42_encode_get_compressed_image_size(t42_encode_state_t *s);

SPAN_DECLARE(int) t42_encode_set_row_read_handler(t42_encode_state_t *s,
                                                  t4_row_read_handler_t handler,
                                                  void *user_data);

/*! Get the logging context associated with a T.42 encode context.
    \brief Get the logging context associated with a T.42 encode context.
    \param s The T.42 encode context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t42_encode_get_logging_state(t42_encode_state_t *s);

SPAN_DECLARE(int) t42_encode_restart(t42_encode_state_t *s, uint32_t image_width, uint32_t image_length);

SPAN_DECLARE(t42_encode_state_t *) t42_encode_init(t42_encode_state_t *s,
                                                   uint32_t image_width,
                                                   uint32_t image_length,
                                                   t4_row_read_handler_t handler,
                                                   void *user_data);

SPAN_DECLARE(int) t42_encode_release(t42_encode_state_t *s);

SPAN_DECLARE(int) t42_encode_free(t42_encode_state_t *s);

SPAN_DECLARE(void) t42_decode_rx_status(t42_decode_state_t *s, int status);

SPAN_DECLARE(int) t42_decode_put(t42_decode_state_t *s, const uint8_t data[], size_t len);

SPAN_DECLARE(int) t42_decode_set_row_write_handler(t42_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data);

SPAN_DECLARE(int) t42_decode_set_comment_handler(t42_decode_state_t *s,
                                                 uint32_t max_comment_len,
                                                 t4_row_write_handler_t handler,
                                                 void *user_data);

SPAN_DECLARE(int) t42_decode_set_image_size_constraints(t42_decode_state_t *s,
                                                        uint32_t max_xd,
                                                        uint32_t max_yd);

SPAN_DECLARE(uint32_t) t42_decode_get_image_width(t42_decode_state_t *s);

SPAN_DECLARE(uint32_t) t42_decode_get_image_length(t42_decode_state_t *s);

SPAN_DECLARE(int) t42_decode_get_compressed_image_size(t42_decode_state_t *s);

SPAN_DECLARE(int) t42_decode_new_plane(t42_decode_state_t *s);

/*! Get the logging context associated with a T.42 decode context.
    \brief Get the logging context associated with a T.42 decode context.
    \param s The T.42 decode context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t42_decode_get_logging_state(t42_decode_state_t *s);

SPAN_DECLARE(int) t42_decode_restart(t42_decode_state_t *s);

SPAN_DECLARE(t42_decode_state_t *) t42_decode_init(t42_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data);

SPAN_DECLARE(int) t42_decode_release(t42_decode_state_t *s);

SPAN_DECLARE(int) t42_decode_free(t42_decode_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
