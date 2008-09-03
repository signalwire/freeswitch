/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_terminal.h - T.38 termination, less the packet exchange part
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
 * $Id: t38_terminal.h,v 1.37 2008/08/14 14:06:05 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_T38_TERMINAL_H_)
#define _SPANDSP_T38_TERMINAL_H_

/*! \page t38_terminal_page T.38 real time FAX over IP termination
\section t38_terminal_page_sec_1 What does it do?

\section t38_terminal_page_sec_2 How does it work?
*/

/* Make sure the HDLC frame buffers are big enough for ECM frames. */
#define T38_MAX_HDLC_LEN        260

typedef struct
{
    /*! \brief Internet Aware FAX mode bit mask. */
    int iaf;
    /*! \brief Required time between T.38 transmissions, in ms. */
    int ms_per_tx_chunk;
    /*! \brief Bit fields controlling the way data is packed into chunked for transmission. */
    int chunking_modes;

    /*! \brief Core T.38 IFP support */
    t38_core_state_t t38;

    /*! \brief The current transmit step being timed */
    int timed_step;

    /*! \brief TRUE is there has been some T.38 data missed (i.e. lost packets) in the current
               reception period. */
    int rx_data_missing;

    /*! \brief The number of octets to send in each image packet (non-ECM or ECM) at the current
               rate and the current specified packet interval. */
    int octets_per_data_packet;

    struct
    {
        /*! \brief HDLC receive buffer */
        uint8_t buf[T38_MAX_HDLC_LEN];
        /*! \brief The length of the contents of the HDLC receive buffer */
        int len;
    } hdlc_rx;

    struct
    {
        /*! \brief HDLC transmit buffer */
        uint8_t buf[T38_MAX_HDLC_LEN];
        /*! \brief The length of the contents of the HDLC transmit buffer */
        int len;
        /*! \brief Current pointer within the contents of the HDLC transmit buffer */
        int ptr;
        /*! \brief The number of extra bits in a fully stuffed version of the
                   contents of the HDLC transmit buffer. This is needed to accurately
                   estimate the playout time for this frame, through an analogue modem. */
        int extra_bits;
    } tx;

    /*! \brief Counter for trailing non-ECM bytes, used to flush out the far end's modem. */
    int non_ecm_trailer_bytes;

    /*! \brief The next T.38 indicator queued for transmission. */
    int next_tx_indicator;
    /*! \brief The current T.38 data type being transmitted. */
    int current_tx_data_type;

    /*! \brief TRUE if a carrier is present. Otherwise FALSE. */
    int rx_signal_present;

    /*! \brief The current operating mode of the receiver. */
    int current_rx_type;
    /*! \brief The current operating mode of the transmitter. */
    int current_tx_type;

    /*! \brief Current transmission bit rate. */
    int tx_bit_rate;
    /*! \brief A "sample" count, used to time events. */
    int32_t samples;
    /*! \brief The value for samples at the next transmission point. */
    int32_t next_tx_samples;
    /*! \brief The current receive timeout. */
    int32_t timeout_rx_samples;
} t38_terminal_front_end_state_t;

/*!
    T.38 terminal state.
*/
typedef struct
{
    /*! \brief The T.30 back-end */
    t30_state_t t30;

    /*! \brief The T.38 front-end */
    t38_terminal_front_end_state_t t38_fe;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
} t38_terminal_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

int t38_terminal_send_timeout(t38_terminal_state_t *s, int samples);

void t38_terminal_set_config(t38_terminal_state_t *s, int without_pacing);

/*! Select whether the time for talker echo protection tone will be allowed for when sending.
    \brief Select whether TEP time will be allowed for.
    \param s The T.38 context.
    \param use_tep TRUE if TEP should be allowed for.
*/
void t38_terminal_set_tep_mode(t38_terminal_state_t *s, int use_tep);

/*! Select whether non-ECM fill bits are to be removed during transmission.
    \brief Select whether non-ECM fill bits are to be removed during transmission.
    \param s The T.38 context.
    \param remove TRUE if fill bits are to be removed.
*/
void t38_terminal_set_fill_bit_removal(t38_terminal_state_t *s, int remove);

/*! Get a pointer to the T.30 engine associated with a termination mode T.38 context.
    \brief Get a pointer to the T.30 engine associated with a T.38 context.
    \param s The T.38 context.
    \return A pointer to the T.30 context, or NULL.
*/
t30_state_t *t38_terminal_get_t30_state(t38_terminal_state_t *s);

/*! \brief Initialise a termination mode T.38 context.
    \param s The T.38 context.
    \param calling_party TRUE if the context is for a calling party. FALSE if the
           context is for an answering party.
    \param tx_packet_handler A callback routine to encapsulate and transmit T.38 packets.
    \param tx_packet_user_data An opaque pointer passed to the tx_packet_handler routine.
    \return A pointer to the termination mode T.38 context, or NULL if there was a problem. */
t38_terminal_state_t *t38_terminal_init(t38_terminal_state_t *s,
                                        int calling_party,
                                        t38_tx_packet_handler_t *tx_packet_handler,
                                        void *tx_packet_user_data);

/*! Release a termination mode T.38 context.
    \brief Release a T.38 context.
    \param s The T.38 context.
    \return 0 for OK, else -1. */
int t38_terminal_release(t38_terminal_state_t *s);

/*! Free a a termination mode T.38 context.
    \brief Free a T.38 context.
    \param s The T.38 context.
    \return 0 for OK, else -1. */
int t38_terminal_free(t38_terminal_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
