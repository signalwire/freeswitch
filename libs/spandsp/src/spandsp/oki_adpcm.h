/*
 * SpanDSP - a series of DSP components for telephony
 *
 * oki_adpcm.h - Conversion routines between linear 16 bit PCM data and
 *		         OKI (Dialogic) ADPCM format.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 *
 * $Id: oki_adpcm.h,v 1.20 2008/04/17 14:27:00 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_OKI_ADPCM_H_)
#define _SPANDSP_OKI_ADPCM_H_

/*! \page okiadpcm_page OKI (Dialogic) ADPCM encoding and decoding
\section okiadpcm_page_sec_1 What does it do?
OKI ADPCM is widely used in the CTI industry because it is the principal format
supported by Dialogic. As the market leader, they tend to define "common
practice". It offers a good balance of simplicity and quality at rates of
24kbps or 32kbps. 32kbps is obtained by ADPCM compressing 8k samples/second linear
PCM. 24kbps is obtained by resampling to 6k samples/second and using the same ADPCM
compression algorithm on the slower samples.

The algorithms for this ADPCM codec can be found in "PC Telephony - The complete guide
to designing, building and programming systems using Dialogic and Related Hardware"
by Bob Edgar. pg 272-276. */

/*!
    Oki (Dialogic) ADPCM conversion state descriptor. This defines the state of
    a single working instance of the Oki ADPCM converter. This is used for
    either linear to ADPCM or ADPCM to linear conversion.
*/
typedef struct
{
    /*! \brief The bit rate - 24000 or 32000. */
    int bit_rate;
    /*! \brief The last state of the ADPCM algorithm. */
    int16_t last;
    /*! \brief Current index into the step size table. */
    int16_t step_index;
    /*! \brief The compressed data byte in progress. */
    uint8_t oki_byte;
    /*! \brief The signal history for the sample rate converter. */
    int16_t history[32];
    /*! \brief Pointer into the history buffer. */
    int ptr;
    /*! \brief Odd/even sample counter. */
    int mark;
    /*! \brief Phase accumulator for the sample rate converter. */
    int phase;
} oki_adpcm_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Initialise an Oki ADPCM encode or decode context.
    \param s The Oki ADPCM context.
    \param bit_rate The required bit rate for the ADPCM data.
           The valid rates are 24000 and 32000.
    \return A pointer to the Oki ADPCM context, or NULL for error. */
oki_adpcm_state_t *oki_adpcm_init(oki_adpcm_state_t *s, int bit_rate);

/*! Free an Oki ADPCM encode or decode context.
    \param s The Oki ADPCM context.
    \return 0 for OK. */
int oki_adpcm_release(oki_adpcm_state_t *s);

/*! Decode a buffer of Oki ADPCM data to linear PCM.
    \param s The Oki ADPCM context.
    \param amp The audio sample buffer.
    \param oki_data
    \param oki_bytes
    \return The number of samples returned. */
int oki_adpcm_decode(oki_adpcm_state_t *s,
                     int16_t amp[],
                     const uint8_t oki_data[],
                     int oki_bytes);

/*! Encode a buffer of linear PCM data to Oki ADPCM.
    \param s The Oki ADPCM context.
    \param oki_data The Oki ADPCM data produced
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of bytes of Oki ADPCM data produced. */
int oki_adpcm_encode(oki_adpcm_state_t *s,
                     uint8_t oki_data[],
                     const int16_t amp[],
                     int len);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
