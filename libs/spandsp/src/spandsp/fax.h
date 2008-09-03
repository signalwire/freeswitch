/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax.h - definitions for analogue line ITU T.30 fax processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 * $Id: fax.h,v 1.35 2008/08/13 00:11:30 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_FAX_H_)
#define _SPANDSP_FAX_H_

/*! \page fax_page FAX over analogue modem handling

\section fax_page_sec_1 What does it do?

\section fax_page_sec_2 How does it work?
*/

typedef struct fax_state_s fax_state_t;

/*!
    Analogue line T.30 FAX channel descriptor. This defines the state of a single working
    instance of an analogue line soft-FAX machine.
*/
struct fax_state_s
{
    /*! \brief The T.30 back-end */
    t30_state_t t30;
    
    /*! \brief The analogue modem front-end */
    fax_modems_state_t modems;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Apply T.30 receive processing to a block of audio samples.
    \brief Apply T.30 receive processing to a block of audio samples.
    \param s The FAX context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. This should only be non-zero if
            the software has reached the end of the FAX call.
*/
int fax_rx(fax_state_t *s, int16_t *amp, int len);

/*! Apply T.30 transmit processing to generate a block of audio samples.
    \brief Apply T.30 transmit processing to generate a block of audio samples.
    \param s The FAX context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated. This will be zero when
            there is nothing to send.
*/
int fax_tx(fax_state_t *s, int16_t *amp, int max_len);

/*! Select whether silent audio will be sent when FAX transmit is idle.
    \brief Select whether silent audio will be sent when FAX transmit is idle.
    \param s The FAX context.
    \param transmit_on_idle TRUE if silent audio should be output when the FAX transmitter is
           idle. FALSE to transmit zero length audio when the FAX transmitter is idle. The default
           behaviour is FALSE.
*/
void fax_set_transmit_on_idle(fax_state_t *s, int transmit_on_idle);

/*! Select whether talker echo protection tone will be sent for the image modems.
    \brief Select whether TEP will be sent for the image modems.
    \param s The FAX context.
    \param use_tep TRUE if TEP should be sent.
*/
void fax_set_tep_mode(fax_state_t *s, int use_tep);

/*! Get a pointer to the T.30 engine associated with a FAX context.
    \brief Get a pointer to the T.30 engine associated with a FAX context.
    \param s The FAX context.
    \return A pointer to the T.30 context, or NULL.
*/
t30_state_t *fax_get_t30_state(fax_state_t *s);

/*! Initialise a FAX context.
    \brief Initialise a FAX context.
    \param s The FAX context.
    \param calling_party TRUE if the context is for a calling party. FALSE if the
           context is for an answering party.
    \return A pointer to the FAX context, or NULL if there was a problem.
*/
fax_state_t *fax_init(fax_state_t *s, int calling_party);

/*! Release a FAX context.
    \brief Release a FAX context.
    \param s The FAX context.
    \return 0 for OK, else -1. */
int fax_release(fax_state_t *s);

/*! Free a FAX context.
    \brief Free a FAX context.
    \param s The FAX context.
    \return 0 for OK, else -1. */
int fax_free(fax_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
