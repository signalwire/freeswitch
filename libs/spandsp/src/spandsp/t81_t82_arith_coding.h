/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t81_t82_arith_coding.h - ITU T.81 and T.82 QM-coder arithmetic encoding
 *                          and decoding
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

#if !defined(_SPANDSP_T81_T82_ARITH_CODING_H_)
#define _SPANDSP_T81_T82_ARITH_CODING_H_

/*! \page t81_t82_arith_coding_page T.81 and T.82 QM-coder arithmetic encoding and decoding

\section t81_t82_arith_coding_page_sec_1 What does it do?
A similar arithmetic coder, called the QM-coder, is used by several image compression
schemes. These routines implement this coder in a (hopefully) reusable way.

\section t81_t82_arith_coding_page_sec_1 How does it work?
*/

/* State of a working instance of the arithmetic encoder */
typedef struct t81_t82_arith_encode_state_s  t81_t82_arith_encode_state_t;

/* State of a working instance of the arithmetic decoder */
typedef struct t81_t82_arith_decode_state_s  t81_t82_arith_decode_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(t81_t82_arith_encode_state_t *) t81_t82_arith_encode_init(t81_t82_arith_encode_state_t *s,
                                                                       void (*output_byte_handler)(void *, int),
                                                                       void *user_data);

SPAN_DECLARE(int) t81_t82_arith_encode_restart(t81_t82_arith_encode_state_t *s, int reuse_st);

SPAN_DECLARE(int) t81_t82_arith_encode_release(t81_t82_arith_encode_state_t *s);

SPAN_DECLARE(int) t81_t82_arith_encode_free(t81_t82_arith_encode_state_t *s);

SPAN_DECLARE(void) t81_t82_arith_encode(t81_t82_arith_encode_state_t *s, int cx, int pix);

SPAN_DECLARE(void) t81_t82_arith_encode_flush(t81_t82_arith_encode_state_t *s);

SPAN_DECLARE(t81_t82_arith_decode_state_t *) t81_t82_arith_decode_init(t81_t82_arith_decode_state_t *s);

SPAN_DECLARE(int) t81_t82_arith_decode_restart(t81_t82_arith_decode_state_t *s, int reuse_st);

SPAN_DECLARE(int) t81_t82_arith_decode_release(t81_t82_arith_decode_state_t *s);

SPAN_DECLARE(int) t81_t82_arith_decode_free(t81_t82_arith_decode_state_t *s);

SPAN_DECLARE(int) t81_t82_arith_decode(t81_t82_arith_decode_state_t *s, int cx);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
