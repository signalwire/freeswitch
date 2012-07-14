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

#if !defined(_SPANDSP_PRIVATE_T81_T82_ARITH_CODING_H_)
#define _SPANDSP_PRIVATE_T81_T82_ARITH_CODING_H_

/* State of a working instance of the QM-coder arithmetic encoder */
struct t81_t82_arith_encode_state_s
{
    /*! A register - see T.82 Table 23 */
    uint32_t a;
    /*! C register - see T.82 Table 23 */
    uint32_t c;
    /*! Probability status for contexts. MSB = MPS */
    uint8_t st[4096];
    /*! Number of buffered 0xFF values that might still overflow */
    int32_t sc;
    /*! Bit shift counter. This determines when the next byte will be written */
    int ct;
    /*! Buffer for the most recent output byte which is not 0xFF */
    int buffer;
    /*! Callback function to deliver the encoded data, byte by byte */
    void (*output_byte_handler)(void *, int);
    /*! Opaque pointer passed to byte_out */
    void *user_data;
};

/* State of a working instance of the QM-coder arithmetic decoder */
struct t81_t82_arith_decode_state_s
{
    /*! A register - see T.82 Table 25 */
    uint32_t a;
    /*! C register - see T.82 Table 25 */
    uint32_t c;
    /*! Probability status for contexts. MSB = MPS */
    uint8_t st[4096];
    /*! Bit-shift counter. Determines when next byte will be read.
        Special value -1 signals that zero-padding has started */
    int ct;
    /*! Pointer to next PSCD data byte */
    const uint8_t *pscd_ptr;
    /*! Pointer to byte after PSCD */
    const uint8_t *pscd_end;
    /*! Boolean flag that controls initial fill of s->c */
    int startup;
    /*! Boolean flag that triggers return -2 between reaching PSCD end
        and decoding the first symbol that might never have been encoded
        in the first place */
    int nopadding;
};

#endif

/*- End of file ------------------------------------------------------------*/
