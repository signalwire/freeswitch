/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/ademco_contactid.h - Ademco ContactID alarm protocol
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2012 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_ADEMCO_CONTACTID_H_)
#define _SPANDSP_PRIVATE_ADEMCO_CONTACTID_H_

struct ademco_contactid_receiver_state_s
{
    ademco_contactid_report_func_t callback;
    void *callback_user_data;

    int step;
    int remaining_samples;
    uint32_t tone_phase;
    int32_t tone_phase_rate;
    int16_t tone_level;
    dtmf_rx_state_t dtmf;

    char rx_digits[16 + 1];
    int rx_digits_len;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

struct ademco_contactid_sender_state_s
{
    tone_report_func_t callback;
    void *callback_user_data;

    int step;
    int remaining_samples;

    dtmf_tx_state_t dtmf;
#if defined(SPANDSP_USE_FIXED_POINT)
    /*! Minimum acceptable tone level for detection. */
    int32_t threshold;
    /*! The accumlating total energy on the same period over which the Goertzels work. */
    int32_t energy;
#else
    /*! Minimum acceptable tone level for detection. */
    float threshold;
    /*! The accumlating total energy on the same period over which the Goertzels work. */
    float energy;
#endif
    goertzel_state_t tone_1400;
    goertzel_state_t tone_2300;
    /*! The current sample number within a processing block. */
    int current_sample;

    /*! \brief A buffer to save the sent message, in case we need to retry. */
    char tx_digits[16 + 1];
    int tx_digits_len;
    /*! \brief The number of consecutive retries. */
    int tries;

    int tone_state;
    int duration;
    int last_hit;
    int in_tone;
    int clear_to_send;
    int timer;

    int busy;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
