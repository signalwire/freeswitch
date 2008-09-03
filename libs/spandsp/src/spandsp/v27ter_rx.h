/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v27ter_rx.h - ITU V.27ter modem receive part
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
 *
 * $Id: v27ter_rx.h,v 1.47 2008/07/16 14:23:48 steveu Exp $
 */

/*! \file */

#if !defined(_V27TER_RX_H_)
#define _V27TER_RX_H_

/*! \page v27ter_rx_page The V.27ter receiver

\section v27ter_rx_page_sec_1 What does it do?
The V.27ter receiver implements the receive side of a V.27ter modem. This can operate
at data rates of 4800 and 2400 bits/s. The audio input is a stream of 16 bit samples,
at 8000 samples/second. The transmit and receive side of V.27ter modems operate
independantly. V.27ter is mostly used for FAX transmission, where it provides the
standard 4800 bits/s rate (the 2400 bits/s mode is not used for FAX). 

\section v27ter_rx_page_sec_2 How does it work?
V.27ter defines two modes of operation. One uses 8-PSK at 1600 baud, giving 4800bps.
The other uses 4-PSK at 1200 baud, giving 2400bps. A training sequence is specified
at the start of transmission, which makes the design of a V.27ter receiver relatively
straightforward.
*/

/* Target length for the equalizer is about 43 taps for 4800bps and 32 taps for 2400bps
   to deal with the worst stuff in V.56bis. */
#define V27TER_EQUALIZER_PRE_LEN        15  /* this much before the real event */
#define V27TER_EQUALIZER_POST_LEN       15  /* this much after the real event */
#define V27TER_EQUALIZER_MASK           63  /* one less than a power of 2 >= (V27TER_EQUALIZER_PRE_LEN + 1 + V27TER_EQUALIZER_POST_LEN) */

#define V27TER_RX_4800_FILTER_STEPS     27
#define V27TER_RX_2400_FILTER_STEPS     27

#if V27TER_RX_4800_FILTER_STEPS > V27TER_RX_2400_FILTER_STEPS
#define V27TER_RX_FILTER_STEPS V27TER_RX_4800_FILTER_STEPS
#else
#define V27TER_RX_FILTER_STEPS V27TER_RX_2400_FILTER_STEPS
#endif

/*!
    V.27ter modem receive side descriptor. This defines the working state for a
    single instance of a V.27ter modem receiver.
*/
typedef struct
{
    /*! \brief The bit rate of the modem. Valid values are 2400 and 4800. */
    int bit_rate;
    /*! \brief The callback function used to put each bit received. */
    put_bit_func_t put_bit;
    /*! \brief A user specified opaque pointer passed to the put_bit routine. */
    void *put_bit_user_data;

    /*! \brief The callback function used to report modem status changes. */
    modem_rx_status_func_t status_handler;
    /*! \brief A user specified opaque pointer passed to the status function. */
    void *status_user_data;

    /*! \brief A callback function which may be enabled to report every symbol's
               constellation position. */
    qam_report_handler_t qam_report;
    /*! \brief A user specified opaque pointer passed to the qam_report callback
               routine. */
    void *qam_user_data;

    /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t rrc_filter[2*V27TER_RX_FILTER_STEPS];
#else
    float rrc_filter[2*V27TER_RX_FILTER_STEPS];
#endif
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;

    /*! \brief The register for the training and data scrambler. */
    unsigned int scramble_reg;
    /*! \brief A counter for the number of consecutive bits of repeating pattern through
               the scrambler. */
    int scrambler_pattern_count;
    /*! \brief The section of the training data we are currently in. */
    int training_stage;
    /*! \brief The current step in the table of BC constellation positions. */
    int training_bc;
    /*! \brief A count of how far through the current training step we are. */
    int training_count;
    /*! \brief A measure of how much mismatch there is between the real constellation,
        and the decoded symbol positions. */
    float training_error;
    /*! \brief The value of the last signal sample, using the a simple HPF for signal power estimation. */
    int16_t last_sample;
    /*! \brief >0 if a signal above the minimum is present. It may or may not be a V.27ter signal. */
    int signal_present;
    /*! \brief Whether or not a carrier drop was detected and the signal delivery is pending. */
    int carrier_drop_pending;
    /*! \brief A count of the current consecutive samples below the carrier off threshold. */
    int low_samples;
    /*! \brief A highest magnitude sample seen. */
    int16_t high_sample;
    /*! \brief TRUE if the previous trained values are to be reused. */
    int old_train;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t carrier_phase_rate;
    /*! \brief The carrier update rate saved for reuse when using short training. */
    int32_t carrier_phase_rate_save;
    /*! \brief The proportional part of the carrier tracking filter. */
    float carrier_track_p;
    /*! \brief The integral part of the carrier tracking filter. */
    float carrier_track_i;

    /*! \brief A power meter, to measure the HPF'ed signal power in the channel. */    
    power_meter_t power;
    /*! \brief The power meter level at which carrier on is declared. */
    int32_t carrier_on_power;
    /*! \brief The power meter level at which carrier off is declared. */
    int32_t carrier_off_power;
    /*! \brief The scaling factor accessed by the AGC algorithm. */
    float agc_scaling;
    /*! \brief The previous value of agc_scaling, needed to reuse old training. */
    float agc_scaling_save;

    int constellation_state;

    /*! \brief The current delta factor for updating the equalizer coefficients. */
    float eq_delta;
#if defined(SPANDSP_USE_FIXED_POINTx)
    /*! \brief The adaptive equalizer coefficients. */
    complexi_t eq_coeff[V27TER_EQUALIZER_PRE_LEN + 1 + V27TER_EQUALIZER_POST_LEN];
    /*! \brief A saved set of adaptive equalizer coefficients for use after restarts. */
    complexi_t eq_coeff_save[V27TER_EQUALIZER_PRE_LEN + 1 + V27TER_EQUALIZER_POST_LEN];
    /*! \brief The equalizer signal buffer. */
    complexi_t eq_buf[V27TER_EQUALIZER_MASK + 1];
#else
    complexf_t eq_coeff[V27TER_EQUALIZER_PRE_LEN + 1 + V27TER_EQUALIZER_POST_LEN];
    complexf_t eq_coeff_save[V27TER_EQUALIZER_PRE_LEN + 1 + V27TER_EQUALIZER_POST_LEN];
    complexf_t eq_buf[V27TER_EQUALIZER_MASK + 1];
#endif
    /*! \brief Current offset into the equalizer buffer. */
    int eq_step;
    /*! \brief Current write offset into the equalizer buffer. */
    int eq_put_step;
    /*! \brief Symbol counter to the next equalizer update. */
    int eq_skip;

    /*! \brief Integration variable for damping the Gardner algorithm tests. */
    int gardner_integrate;
    /*! \brief Current step size of Gardner algorithm integration. */
    int gardner_step;
    /*! \brief The total symbol timing correction since the carrier came up.
               This is only for performance analysis purposes. */
    int total_baud_timing_correction;
    /*! \brief The current fractional phase of the baud timing. */
    int baud_phase;

    /*! \brief Starting phase angles for the coarse carrier aquisition step. */
    int32_t start_angles[2];
    /*! \brief History list of phase angles for the coarse carrier aquisition step. */
    int32_t angles[16];
    /*! \brief Error and flow logging control */
    logging_state_t logging;
} v27ter_rx_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Initialise a V.27ter modem receive context.
    \brief Initialise a V.27ter modem receive context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 2400 and 4800.
    \param put_bit The callback routine used to put the received data.
    \param user_data An opaque pointer passed to the put_bit routine.
    \return A pointer to the modem context, or NULL if there was a problem. */
v27ter_rx_state_t *v27ter_rx_init(v27ter_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data);

/*! Reinitialise an existing V.27ter modem receive context.
    \brief Reinitialise an existing V.27ter modem receive context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 2400 and 4800.
    \param old_train TRUE if a previous trained values are to be reused.
    \return 0 for OK, -1 for bad parameter */
int v27ter_rx_restart(v27ter_rx_state_t *s, int bit_rate, int old_train);

/*! Free a V.27ter modem receive context.
    \brief Free a V.27ter modem receive context.
    \param s The modem context.
    \return 0 for OK */
int v27ter_rx_free(v27ter_rx_state_t *s);

/*! Change the put_bit function associated with a V.27ter modem receive context.
    \brief Change the put_bit function associated with a V.27ter modem receive context.
    \param s The modem context.
    \param put_bit The callback routine used to handle received bits.
    \param user_data An opaque pointer. */
void v27ter_rx_set_put_bit(v27ter_rx_state_t *s, put_bit_func_t put_bit, void *user_data);

/*! Change the modem status report function associated with a V.27ter modem receive context.
    \brief Change the modem status report function associated with a V.27ter modem receive context.
    \param s The modem context.
    \param handler The callback routine used to report modem status changes.
    \param user_data An opaque pointer. */
void v27ter_rx_set_modem_status_handler(v27ter_rx_state_t *s, modem_rx_status_func_t handler, void *user_data);

/*! Process a block of received V.27ter modem audio samples.
    \brief Process a block of received V.27ter modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed.
*/
int v27ter_rx(v27ter_rx_state_t *s, const int16_t amp[], int len);

/*! Get a snapshot of the current equalizer coefficients.
    \brief Get a snapshot of the current equalizer coefficients.
    \param coeffs The vector of complex coefficients.
    \return The number of coefficients in the vector. */
int v27ter_rx_equalizer_state(v27ter_rx_state_t *s, complexf_t **coeffs);

/*! Get the current received carrier frequency.
    \param s The modem context.
    \return The frequency, in Hertz. */
float v27ter_rx_carrier_frequency(v27ter_rx_state_t *s);

/*! Get the current symbol timing correction since startup.
    \param s The modem context.
    \return The correction. */
float v27ter_rx_symbol_timing_correction(v27ter_rx_state_t *s);

/*! Get a current received signal power.
    \param s The modem context.
    \return The signal power, in dBm0. */
float v27ter_rx_signal_power(v27ter_rx_state_t *s);

/*! Set the power level at which the carrier detection will cut in
    \param s The modem context.
    \param cutoff The signal cutoff power, in dBm0. */
void v27ter_rx_signal_cutoff(v27ter_rx_state_t *s, float cutoff);

/*! Set a handler routine to process QAM status reports
    \param s The modem context.
    \param handler The handler routine.
    \param user_data An opaque pointer passed to the handler routine. */
void v27ter_rx_set_qam_report_handler(v27ter_rx_state_t *s, qam_report_handler_t handler, void *user_data);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
