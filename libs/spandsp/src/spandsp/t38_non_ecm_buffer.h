/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_non_ecm_buffer.h - A rate adapting buffer for T.38 non-ECM image
 *                        and TCF data
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005, 2006, 2007, 2008 Steve Underwood
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
 * $Id: t38_non_ecm_buffer.h,v 1.7 2009/02/10 13:06:47 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_T38_NON_ECM_BUFFER_H_)
#define _SPANDSP_T38_NON_ECM_BUFFER_H_

/*! \page t38_non_ecm_buffer_page T.38 rate adapting non-ECM image data buffer
\section t38_non_ecm_buffer_page_sec_1 What does it do?

The T.38 rate adapting non-ECM image data buffer is used to buffer TCF and non-ECM
FAX image data being gatewayed from a T.38 linke to an analogue FAX modem link.

\section t38_non_ecm_buffer_page_sec_2 How does it work?
*/

/*! The buffer length much be a power of two. The chosen length is big enough for
    over 9s of data at the V.17 14,400bps rate. */    
#define T38_NON_ECM_TX_BUF_LEN  16384

/*! \brief A flow controlled non-ECM image data buffer, for buffering T.38 to analogue
           modem data.
*/
typedef struct t38_non_ecm_buffer_state_s t38_non_ecm_buffer_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Initialise a T.38 rate adapting non-ECM buffer context.
    \param s The buffer context.
    \param mode TRUE for image data mode, or FALSE for TCF mode.
    \param bits The minimum number of bits per FAX image row.
    \return A pointer to the buffer context, or NULL if there was a problem. */
SPAN_DECLARE(t38_non_ecm_buffer_state_t *) t38_non_ecm_buffer_init(t38_non_ecm_buffer_state_t *s, int mode, int min_row_bits);

SPAN_DECLARE(int) t38_non_ecm_buffer_release(t38_non_ecm_buffer_state_t *s);

SPAN_DECLARE(int) t38_non_ecm_buffer_free(t38_non_ecm_buffer_state_t *s);

/*! \brief Set the mode of a T.38 rate adapting non-ECM buffer context.
    \param s The buffer context.
    \param mode TRUE for image data mode, or FALSE for TCF mode.
    \param bits The minimum number of bits per FAX image row. */
SPAN_DECLARE(void) t38_non_ecm_buffer_set_mode(t38_non_ecm_buffer_state_t *s, int mode, int min_row_bits);

/*! \brief Inject data to T.38 rate adapting non-ECM buffer context.
    \param s The buffer context.
    \param buf The data buffer to be injected.
    \param len The length of the data to be injected. */
SPAN_DECLARE(void) t38_non_ecm_buffer_inject(t38_non_ecm_buffer_state_t *s, const uint8_t *buf, int len);

/*! \brief Inform a T.38 rate adapting non-ECM buffer context that the incoming data has finished,
           and the contents of the buffer should be played out as quickly as possible.
    \param s The buffer context. */
SPAN_DECLARE(void) t38_non_ecm_buffer_push(t38_non_ecm_buffer_state_t *s);

/*! \brief Report the input status of a T.38 rate adapting non-ECM buffer context to the specified
           logging context.
    \param s The buffer context.
    \param logging The logging context. */
SPAN_DECLARE(void) t38_non_ecm_buffer_report_input_status(t38_non_ecm_buffer_state_t *s, logging_state_t *logging);

/*! \brief Report the output status of a T.38 rate adapting non-ECM buffer context to the specified
           logging context.
    \param s The buffer context.
    \param logging The logging context. */
SPAN_DECLARE(void) t38_non_ecm_buffer_report_output_status(t38_non_ecm_buffer_state_t *s, logging_state_t *logging);

/*! \brief Get the next bit of data from a T.38 rate adapting non-ECM buffer context.
    \param user_data The buffer context, cast to a void pointer.
    \return The next bit, or one of the values indicating a change of modem status. */
SPAN_DECLARE_NONSTD(int) t38_non_ecm_buffer_get_bit(void *user_data);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
