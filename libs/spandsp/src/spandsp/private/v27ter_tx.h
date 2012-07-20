/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v27ter_tx.h - ITU V.27ter modem transmit part
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_V27TER_TX_H_)
#define _SPANDSP_PRIVATE_V27TER_TX_H_

/*! The number of taps in the pulse shaping/bandpass filter */
#define V27TER_TX_FILTER_STEPS      9

/*!
    V.27ter modem transmit side descriptor. This defines the working state for a
    single instance of a V.27ter modem transmitter.
*/
struct v27ter_tx_state_s
{
    /*! \brief The bit rate of the modem. Valid values are 2400 and 4800. */
    int bit_rate;
    /*! \brief The callback function used to get the next bit to be transmitted. */
    get_bit_func_t get_bit;
    /*! \brief A user specified opaque pointer passed to the get_bit function. */
    void *get_bit_user_data;

    /*! \brief The callback function used to report modem status changes. */
    modem_status_func_t status_handler;
    /*! \brief A user specified opaque pointer passed to the status function. */
    void *status_user_data;

#if defined(SPANDSP_USE_FIXED_POINT)
    /*! \brief The gain factor needed to achieve the specified output power at 2400bps. */
    int16_t gain_2400;
    /*! \brief The gain factor needed to achieve the specified output power at 4800bps. */
    int16_t gain_4800;
    /*! \brief The root raised cosine (RRC) pulse shaping filter buffer. */
    int16_t rrc_filter_re[V27TER_TX_FILTER_STEPS];
    int16_t rrc_filter_im[V27TER_TX_FILTER_STEPS];
#else
    /*! \brief The gain factor needed to achieve the specified output power at 2400bps. */
    float gain_2400;
    /*! \brief The gain factor needed to achieve the specified output power at 4800bps. */
    float gain_4800;
    /*! \brief The root raised cosine (RRC) pulse shaping filter buffer. */
    float rrc_filter_re[V27TER_TX_FILTER_STEPS];
    float rrc_filter_im[V27TER_TX_FILTER_STEPS];
#endif

    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;
    
    /*! \brief The register for the training and data scrambler. */
    uint32_t scramble_reg;
    /*! \brief A counter for the number of consecutive bits of repeating pattern through
               the scrambler. */
    int scrambler_pattern_count;
    /*! \brief TRUE if transmitting the training sequence, or shutting down transmission.
               FALSE if transmitting user data. */
    int in_training;
    /*! \brief A counter used to track progress through sending the training sequence. */
    int training_step;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t carrier_phase_rate;
    /*! \brief The current fractional phase of the baud timing. */
    int baud_phase;
    /*! \brief The code number for the current position in the constellation. */
    int constellation_state;
    /*! \brief The get_bit function in use at any instant. */
    get_bit_func_t current_get_bit;
    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
