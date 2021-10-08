/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v18.h - V.18 text telephony for the deaf.
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

#if !defined(_SPANDSP_PRIVATE_V18_H_)
#define _SPANDSP_PRIVATE_V18_H_

struct v18_state_s
{
    /*! \brief True if we are the calling modem */
    bool calling_party;
    int mode;
    int initial_mode;
    int current_mode;
    int nation;
    put_msg_func_t put_msg;
    void *user_data;
    bool repeat_shifts;
    bool autobauding;

    union
    {
        queue_state_t queue;
        uint8_t buf[QUEUE_STATE_T_SIZE(128)];
    } queue;
    tone_gen_descriptor_t alert_tone_desc;
    tone_gen_state_t alert_tone_gen;
    fsk_tx_state_t fsk_tx;
    dtmf_tx_state_t dtmf_tx;
    async_tx_state_t async_tx;
    int baudot_tx_shift;
    int tx_signal_on;
    bool tx_draining;
    uint8_t next_byte;

    fsk_rx_state_t fsk_rx;
    dtmf_rx_state_t dtmf_rx;

#if defined(SPANDSP_USE_FIXED_POINTx)
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
    goertzel_state_t tone_390;
    goertzel_state_t tone_980;
    goertzel_state_t tone_1180;
    goertzel_state_t tone_1270;
    goertzel_state_t tone_1300;
    goertzel_state_t tone_1400;
    goertzel_state_t tone_1650;
    goertzel_state_t tone_1800;
    /*! The current sample number within a processing block. */
    int current_sample;
    /*! Tone state duration */
    int duration;
    int target_duration;
    int in_tone;

    int baudot_rx_shift;
    int consecutive_ones;
    uint8_t rx_msg[256 + 1];
    int rx_msg_len;
    int bit_pos;
    int in_progress;
    int rx_suppression;
    int tx_suppression;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
