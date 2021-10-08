/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2011 Steve Underwood
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

/*! \page v42_page V.42 modem error correction
\section v42_page_sec_1 What does it do?
The V.42 specification defines an error correcting protocol for PSTN modems, based on
HDLC and LAP. This makes it similar to an X.25 link. A special variant of LAP, known
as LAP-M, is defined in the V.42 specification. A means for modems to determine if the
far modem supports V.42 is also defined.

\section v42_page_sec_2 How does it work?
*/

#if !defined(_SPANDSP_V42_H_)
#define _SPANDSP_V42_H_

typedef struct v42_state_s v42_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

SPAN_DECLARE(const char *) lapm_status_to_str(int status);

SPAN_DECLARE(void) lapm_receive(void *user_data, const uint8_t *frame, int len, int ok);

SPAN_DECLARE(void) v42_start(v42_state_t *s);

SPAN_DECLARE(void) v42_stop(v42_state_t *s);

/*! Set the busy status of the local end of a V.42 context.
    \param s The V.42 context.
    \param busy The new local end busy status.
    \return The previous local end busy status.
*/
SPAN_DECLARE(int) v42_set_local_busy_status(v42_state_t *s, int busy);

/*! Get the busy status of the far end of a V.42 context.
    \param s The V.42 context.
    \return The far end busy status.
*/
SPAN_DECLARE(int) v42_get_far_busy_status(v42_state_t *s);

SPAN_DECLARE(void) v42_rx_bit(void *user_data, int bit);

SPAN_DECLARE(int) v42_tx_bit(void *user_data);

SPAN_DECLARE(void) v42_set_status_callback(v42_state_t *s, modem_status_func_t callback, void *user_data);

/*! Get the logging context associated with a V.42 context.
    \brief Get the logging context associated with a V.42 context.
    \param s The V.42 context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) v42_get_logging_state(v42_state_t *s);

/*! Initialise a V.42 context.
    \param s The V.42 context.
    \param calling_party True if caller mode, else answerer mode.
    \param detect True to perform the V.42 detection, else go straight into LAP.M
    \param iframe_get A callback function to handle received frames of data.
    \param iframe_put A callback function to get frames of data for transmission.
    \param user_data An opaque pointer passed to the frame handler routines.
    \return ???
*/
SPAN_DECLARE(v42_state_t *) v42_init(v42_state_t *s,
                                     bool calling_party,
                                     bool detect,
                                     get_msg_func_t iframe_get,
                                     put_msg_func_t iframe_put,
                                     void *user_data);

/*! Restart a V.42 context.
    \param s The V.42 context.
*/
SPAN_DECLARE(void) v42_restart(v42_state_t *s);

/*! Release a V.42 context.
    \param s The V.42 context.
    \return 0 if OK */
SPAN_DECLARE(int) v42_release(v42_state_t *s);

/*! Free a V.42 context.
    \param s The V.42 context.
    \return 0 if OK */
SPAN_DECLARE(int) v42_free(v42_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
