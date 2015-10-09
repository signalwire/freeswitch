/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v18.h - V.18 text telephony for the deaf.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004-2009 Steve Underwood
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

/*! \page v18_page The V.18 text telephony protocols
\section v18_page_sec_1 What does it do?

\section v18_page_sec_2 How does it work?
*/

#if !defined(_SPANDSP_V18_H_)
#define _SPANDSP_V18_H_

typedef struct v18_state_s v18_state_t;

enum
{
    V18_MODE_NONE = 0x0001,
    /* V.18 Annex A - Weitbrecht TDD at 45.45bps (US TTY), half-duplex, 5 bit baudot (USA). */
    V18_MODE_5BIT_4545 = 0x0002,
    /* V.18 Annex A - Weitbrecht TDD at 50bps (International TTY), half-duplex, 5 bit baudot (UK, Australia and others). */
    V18_MODE_5BIT_50 = 0x0004,
    /* V.18 Annex B - DTMF encoding of ASCII (Denmark, Holland and others). */
    V18_MODE_DTMF = 0x0008,
    /* V.18 Annex C - EDT (European Deaf Telephone) 110bps, V.21, half-duplex, ASCII (Germany, Austria, Switzerland and others). */
    V18_MODE_EDT = 0x0010,
    /* V.18 Annex D - 300bps, Bell 103, duplex, ASCII (USA). */
    V18_MODE_BELL103 = 0x0020,
    /* V.18 Annex E - 1200bps Videotex terminals, ASCII (France). */
    V18_MODE_V23VIDEOTEX = 0x0040,
    /* V.18 Annex F - V.21 text telephone, V.21, duplex, ASCII (Sweden, Norway and Finland). */
    V18_MODE_V21TEXTPHONE = 0x0080,
    /* V.18 Annex G - V.18 text telephone mode. */
    V18_MODE_V18TEXTPHONE = 0x0100,
    /* V.18 Annex A - Used during probing. */
    V18_MODE_5BIT_476 = 0x0200,
    /* Use repetitive shift characters where character set shifts are used */ 
    V18_MODE_REPETITIVE_SHIFTS_OPTION = 0x1000
};

/* Automoding sequences for different countries */
enum
{
    V18_AUTOMODING_GLOBAL = 0,

    /* 5-bit, V.21, V.23, EDT, DTMF, Bell 103 */
    V18_AUTOMODING_AUSTRALIA,
    V18_AUTOMODING_IRELAND,

    /* EDT, V.21, V.23, 5-bit, DTMF, Bell 103 */
    V18_AUTOMODING_GERMANY,
    V18_AUTOMODING_SWITZERLAND,
    V18_AUTOMODING_ITALY,
    V18_AUTOMODING_SPAIN,
    V18_AUTOMODING_AUSTRIA,

    /* DTMF, V.21, V.23, 5-bit, EDT, Bell 103 */
    V18_AUTOMODING_NETHERLANDS,

    /* V.21, DTMF, 5-bit, EDT, V.23, Bell 103 */
    V18_AUTOMODING_ICELAND,
    V18_AUTOMODING_NORWAY,
    V18_AUTOMODING_SWEDEN,
    V18_AUTOMODING_FINALND,
    V18_AUTOMODING_DENMARK,

    /* V.21, 5-bit, V.23, EDT, DTMF, Bell 103 */
    V18_AUTOMODING_UK,

    /* 5-bit, Bell 103, V.21, V.23, EDT, DTMF */
    V18_AUTOMODING_USA,

    /* V.23, EDT, DTMF, 5-bit, V.21, Bell 103 */
    V18_AUTOMODING_FRANCE,
    V18_AUTOMODING_BELGIUM,

    V18_AUTOMODING_END
};

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(logging_state_t *) v18_get_logging_state(v18_state_t *s);

/*! Initialise a V.18 context.
    \brief Initialise a V.18 context.
    \param s The V.18 context.
    \param calling_party True if caller mode, else answerer mode.
    \param mode Mode of operation.
    \param nation National variant for automoding.
    \param put_msg A callback routine called to deliver the received text
           to the application.
    \param user_data An opaque pointer for the callback routine.
    \return A pointer to the V.18 context, or NULL if there was a problem. */
SPAN_DECLARE(v18_state_t *) v18_init(v18_state_t *s,
                                     bool calling_party,
                                     int mode,
                                     int nation,
                                     put_msg_func_t put_msg,
                                     void *user_data);

/*! Release a V.18 context.
    \brief Release a V.18 context.
    \param s The V.18 context.
    \return 0 for OK. */
SPAN_DECLARE(int) v18_release(v18_state_t *s);

/*! Free a V.18 context.
    \brief Release a V.18 context.
    \param s The V.18 context.
    \return 0 for OK. */
SPAN_DECLARE(int) v18_free(v18_state_t *s);

/*! Generate a block of V.18 audio samples.
    \brief Generate a block of V.18 audio samples.
    \param s The V.18 context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated.
*/
SPAN_DECLARE(int) v18_tx(v18_state_t *s, int16_t amp[], int max_len);

/*! Process a block of received V.18 audio samples.
    \brief Process a block of received V.18 audio samples.
    \param s The V.18 context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of unprocessed samples.
*/
SPAN_DECLARE(int) v18_rx(v18_state_t *s, const int16_t amp[], int len);

/*! Fake processing of a missing block of received V.18 audio samples.
    (e.g due to packet loss).
    \brief Fake processing of a missing block of received V.18 audio samples.
    \param s The V.18 context.
    \param len The number of samples to fake.
    \return The number of unprocessed samples.
*/
SPAN_DECLARE(int) v18_rx_fillin(v18_state_t *s, int len);

/*! \brief Put a string to a V.18 context's input buffer.
    \param s The V.18 context.
    \param msg The string to be added.
    \param len The length of the string. If negative, the string is
           assumed to be a NULL terminated string.
    \return The number of characters actually added. This may be less than the
            length of the digit string, if the buffer fills up. If the string is
            invalid, this function will return -1. */
SPAN_DECLARE(int) v18_put(v18_state_t *s, const char msg[], int len);

/*! \brief Return a short name for an V.18 mode
    \param mode The code for the V.18 mode.
    \return A pointer to the name.
*/
SPAN_DECLARE(const char *) v18_mode_to_str(int mode);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
