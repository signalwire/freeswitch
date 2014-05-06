/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/data_modems.h - definitions for the analogue modem set for data processing
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

#if !defined(_SPANDSP_PRIVATE_DATA_MODEMS_H_)
#define _SPANDSP_PRIVATE_DATA_MODEMS_H_

/*!
    The set of modems needed for data, plus the auxilliary stuff, like tone generation.
*/
struct data_modems_state_s
{
    bool calling_party;
    /*! True is talker echo protection should be sent for the modems which support this */
    bool use_tep;

    /*! If true, transmit silence when there is nothing else to transmit. If false return only
        the actual generated audio. Note that this only affects untimed silences. Timed silences
        (e.g. the 75ms silence between V.21 and a high speed modem) will alway be transmitted as
        silent audio. */
    bool transmit_on_idle;

    get_bit_func_t get_bit;
    void *get_user_data;
    put_bit_func_t put_bit;
    void *put_user_data;

    void *user_data;

    put_msg_func_t put_msg;
    get_msg_func_t get_msg;

    v42_state_t v42;
    v42bis_state_t v42bis;

    int use_v14;
    async_tx_state_t async_tx;
    async_rx_state_t async_rx;

    union
    {
        v8_state_t v8;
        struct
        {
            /*! \brief Tone generator */
            modem_connect_tones_tx_state_t tx;
            /*! \brief Tone detector */
            modem_connect_tones_rx_state_t rx;
        } tones;
        struct
        {
            /*! \brief FSK transmit modem context used for 103, V.21 and V.23. */
            fsk_tx_state_t tx;
            /*! \brief FSK receive modem context used for 103, V.21 and V.23. */
            fsk_rx_state_t rx;
        } fsk;
        /*! \brief V.22bis modem context */
        v22bis_state_t v22bis;
#if defined(SPANDSP_SUPPORT_V32BIS)
        /*! \brief V.32bis modem context */
        v32bis_state_t v32bis;
#endif
#if defined(SPANDSP_SUPPORT_V34)
        /*! \brief V.22bis modem context */
        v34_state_t v34;
#endif
        /*! \brief Used to insert timed silences. */
        silence_gen_state_t silence_gen;
    } modems;
    /*! \brief */
    dc_restore_state_t dc_restore;

    int current_modem;
    int queued_modem;
    int queued_baud_rate;
    int queued_bit_rate;

    /*! \brief The currently select receiver type */
    int current_rx_type;
    /*! \brief The currently select transmitter type */
    int current_tx_type;

    /*! \brief True if a carrier is present. Otherwise false. */
    bool rx_signal_present;
    /*! \brief True if a modem has trained correctly. */
    bool rx_trained;
    /*! \brief True if an HDLC frame has been received correctly. */
    bool rx_frame_received;

    /*! The current receive signal handler */
    span_rx_handler_t rx_handler;
    /*! The current receive missing signal fill-in handler */
    span_rx_fillin_handler_t rx_fillin_handler;
    void *rx_user_data;

    /*! The current transmit signal handler */
    span_tx_handler_t tx_handler;
    void *tx_user_data;

    /*! \brief Audio logging file handle for received audio. */
    int audio_rx_log;
    /*! \brief Audio logging file handle for transmitted audio. */
    int audio_tx_log;
    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
