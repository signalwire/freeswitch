/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v29rx.h - ITU V.29 modem receive part
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
 * $Id: v29rx.h,v 1.55 2008/07/16 14:23:48 steveu Exp $
 */

/*! \file */

#if !defined(_V29RX_H_)
#define _V29RX_H_

/*! \page v29rx_page The V.29 receiver
\section v29rx_page_sec_1 What does it do?
The V.29 receiver implements the receive side of a V.29 modem. This can operate
at data rates of 9600, 7200 and 4800 bits/s. The audio input is a stream of 16
bit samples, at 8000 samples/second. The transmit and receive side of V.29
modems operate independantly. V.29 is mostly used for FAX transmission, where it
provides the standard 9600 and 7200 bits/s rates (the 4800 bits/s mode is not
used for FAX). 

\section v29rx_page_sec_2 How does it work?
V.29 operates at 2400 baud for all three bit rates. It uses 16-QAM modulation for
9600bps, 8-QAM for 7200bps, and 4-PSK for 4800bps. A training sequence is specified
at the start of transmission, which makes the design of a V.29 receiver relatively
straightforward.

The first stage of the training sequence consists of 128
symbols, alternating between two constellation positions. The receiver monitors
the signal power, to sense the possible presence of a valid carrier. When the
alternating signal begins, the power rising above a minimum threshold (-26dBm0)
causes the main receiver computation to begin. The initial measured power is
used to quickly set the gain of the receiver. After this initial settling, the
front end gain is locked, and the adaptive equalizer tracks any subsequent
signal level variation. The signal is oversampled to 24000 samples/second (i.e.
signal, zero, zero, signal, zero, zero, ...) and fed to a complex root raised
cosine pulse shaping filter. This filter has been modified from the conventional
root raised cosine filter, by shifting it up the band, to be centred at the nominal
carrier frequency. This filter interpolates the samples, pulse shapes, and performs
a fractional sample delay at the same time. 48 sets of filter coefficients are used to
achieve a set of finely spaces fractional sample delays, between zero and
one sample. By choosing every fifth sample, and the appropriate set of filter
coefficients, the properly tuned symbol tracker can select data samples at 4800
samples/second from points within 1.125 degrees of the centre and mid-points of
each symbol. The output of the filter is multiplied by a complex carrier, generated
by a DDS. The result is a baseband signal, requiring no further filtering, apart from
an adaptive equalizer. The baseband signal is fed to a T/2 adaptive equalizer.
A band edge component maximisation algorithm is used to tune the sampling, so the samples
fed to the equalizer are close to the mid point and edges of each symbol. Initially
the algorithm is very lightly damped, to ensure the symbol alignment pulls in
quickly. Because the sampling rate will not be precisely the same as the
transmitter's (the spec. says the symbol timing should be within 0.01%), the
receiver constantly evaluates and corrects this sampling throughout its
operation. During the symbol timing maintainence phase, the algorithm uses
a heavier damping.

The carrier is specified as 1700Hz +-1Hz at the transmitter, and 1700 +-7Hz at
the receiver. The receive carrier would only be this inaccurate if the link
includes FDM sections. These are being phased out, but the design must still
allow for the worst case. Using an initial 1700Hz signal for demodulation gives
a worst case rotation rate for the constellation of about one degree per symbol.
Once the symbol timing synchronisation algorithm has been given time to lock to
the symbol timing of the initial alternating pattern, the phase of the demodulated
signal is recorded on two successive symbols - once for each of the constellation
positions. The receiver then tracks the symbol alternations, until a large phase jump
occurs. This signifies the start of the next phase of the training sequence. At this
point the total phase shift between the original recorded symbol phase, and the
symbol phase just before the phase jump occurred is used to provide a coarse
estimation of the rotation rate of the constellation, and it current absolute
angle of rotation. These are used to update the current carrier phase and phase
update rate in the carrier DDS. The working data already in the pulse shaping
filter and equalizer buffers is given a similar step rotation to pull it all
into line. From this point on, a heavily damped integrate and dump approach,
based on the angular difference between each received constellation position and
its expected position, is sufficient to track the carrier, and maintain phase
alignment. A fast rough approximator for the arc-tangent function is adequate
for the estimation of the angular error. 

The next phase of the training sequence is a scrambled sequence of two
particular symbols. We train the T/2 adaptive equalizer using this sequence. The
scrambling makes the signal sufficiently diverse to ensure the equalizer
converges to the proper generalised solution. At the end of this sequence, the
equalizer should be sufficiently well adapted that is can correctly resolve the
full QAM constellation. However, the equalizer continues to adapt throughout
operation of the modem, fine tuning on the more complex data patterns of the
full QAM constellation. 

In the last phase of the training sequence, the modem enters normal data
operation, with a short defined period of all ones as data. As in most high
speed modems, data in a V.29 modem passes through a scrambler, to whiten the
spectrum of the signal. The transmitter should initialise its data scrambler,
and pass the ones through it. At the end of the ones, real data begins to pass
through the scrambler, and the transmit modem is in normal operation. The
receiver tests that ones are really received, in order to verify the modem
trained correctly. If all is well, the data following the ones is fed to the
application, and the receive modem is up and running. Unfortunately, some
transmit side of some real V.29 modems fail to initialise their scrambler before
sending the ones. This means the first 23 received bits (the length of the
scrambler register) cannot be trusted for the test. The receive modem,
therefore, only tests that bits starting at bit 24 are really ones. 
*/

/* Target length for the equalizer is about 63 taps, to deal with the worst stuff
   in V.56bis. */
#define V29_EQUALIZER_PRE_LEN   15  /* this much before the real event */
#define V29_EQUALIZER_POST_LEN  15  /* this much after the real event */
#define V29_EQUALIZER_MASK      63  /* one less than a power of 2 >= (2*V29_EQUALIZER_LEN + 1) */

#define V29_RX_FILTER_STEPS     27

typedef void (*qam_report_handler_t)(void *user_data, const complexf_t *constel, const complexf_t *target, int symbol);

/*!
    V.29 modem receive side descriptor. This defines the working state for a
    single instance of a V.29 modem receiver.
*/
typedef struct
{
    /*! \brief The bit rate of the modem. Valid values are 4800, 7200 and 9600. */
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
    int16_t rrc_filter[2*V29_RX_FILTER_STEPS];
#else
    float rrc_filter[2*V29_RX_FILTER_STEPS];
#endif
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;

    /*! \brief The register for the data scrambler. */
    unsigned int scramble_reg;
    /*! \brief The register for the training scrambler. */
    uint8_t training_scramble_reg;
    /*! \brief The section of the training data we are currently in. */
    int training_stage;
    /*! \brief The current step in the table of CD constellation positions. */
    int training_cd;
    /*! \brief A count of how far through the current training step we are. */
    int training_count;
    /*! \brief A measure of how much mismatch there is between the real constellation,
        and the decoded symbol positions. */
    float training_error;
    /*! \brief The value of the last signal sample, using the a simple HPF for signal power estimation. */
    int16_t last_sample;
    /*! \brief >0 if a signal above the minimum is present. It may or may not be a V.29 signal. */
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
    complexi_t eq_coeff[V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN];
    /*! \brief A saved set of adaptive equalizer coefficients for use after restarts. */
    complexi_t eq_coeff_save[V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN];
    /*! \brief The equalizer signal buffer. */
    complexi_t eq_buf[V29_EQUALIZER_MASK + 1];
#else
    complexf_t eq_coeff[V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN];
    complexf_t eq_coeff_save[V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN];
    complexf_t eq_buf[V29_EQUALIZER_MASK + 1];
#endif
    /*! \brief Current offset into the equalizer buffer. */
    int eq_step;
    /*! \brief Current write offset into the equalizer buffer. */
    int eq_put_step;
    /*! \brief Symbol counter to the next equalizer update. */
    int eq_skip;

    /*! \brief The current half of the baud. */
    int baud_half;
#if defined(SPANDSP_USE_FIXED_POINTx)
    /*! Low band edge filter for symbol sync. */
    int32_t symbol_sync_low[2];
    /*! High band edge filter for symbol sync. */
    int32_t symbol_sync_high[2];
    /*! DC filter for symbol sync. */
    int32_t symbol_sync_dc_filter[2];
    /*! Baud phase for symbol sync. */
    int32_t baud_phase;
#else
    /*! Low band edge filter for symbol sync. */
    float symbol_sync_low[2];
    /*! High band edge filter for symbol sync. */
    float symbol_sync_high[2];
    /*! DC filter for symbol sync. */
    float symbol_sync_dc_filter[2];
    /*! Baud phase for symbol sync. */
    float baud_phase;
#endif

    /*! \brief The total symbol timing correction since the carrier came up.
               This is only for performance analysis purposes. */
    int total_baud_timing_correction;

    /*! \brief Starting phase angles for the coarse carrier aquisition step. */
    int32_t start_angles[2];
    /*! \brief History list of phase angles for the coarse carrier aquisition step. */
    int32_t angles[16];
    /*! \brief Error and flow logging control */
    logging_state_t logging;
} v29_rx_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Initialise a V.29 modem receive context.
    \brief Initialise a V.29 modem receive context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 4800, 7200 and 9600.
    \param put_bit The callback routine used to put the received data.
    \param user_data An opaque pointer passed to the put_bit routine.
    \return A pointer to the modem context, or NULL if there was a problem. */
v29_rx_state_t *v29_rx_init(v29_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data);

/*! Reinitialise an existing V.29 modem receive context.
    \brief Reinitialise an existing V.29 modem receive context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 4800, 7200 and 9600.
    \param old_train TRUE if a previous trained values are to be reused.
    \return 0 for OK, -1 for bad parameter */
int v29_rx_restart(v29_rx_state_t *s, int bit_rate, int old_train);

/*! Free a V.29 modem receive context.
    \brief Free a V.29 modem receive context.
    \param s The modem context.
    \return 0 for OK */
int v29_rx_free(v29_rx_state_t *s);

/*! Change the put_bit function associated with a V.29 modem receive context.
    \brief Change the put_bit function associated with a V.29 modem receive context.
    \param s The modem context.
    \param put_bit The callback routine used to handle received bits.
    \param user_data An opaque pointer. */
void v29_rx_set_put_bit(v29_rx_state_t *s, put_bit_func_t put_bit, void *user_data);

/*! Change the modem status report function associated with a V.29 modem receive context.
    \brief Change the modem status report function associated with a V.29 modem receive context.
    \param s The modem context.
    \param handler The callback routine used to report modem status changes.
    \param user_data An opaque pointer. */
void v29_rx_set_modem_status_handler(v29_rx_state_t *s, modem_rx_status_func_t handler, void *user_data);

/*! Process a block of received V.29 modem audio samples.
    \brief Process a block of received V.29 modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. */
int v29_rx(v29_rx_state_t *s, const int16_t amp[], int len);

/*! Get a snapshot of the current equalizer coefficients.
    \brief Get a snapshot of the current equalizer coefficients.
    \param s The modem context.
    \param coeffs The vector of complex coefficients.
    \return The number of coefficients in the vector. */
int v29_rx_equalizer_state(v29_rx_state_t *s, complexf_t **coeffs);

/*! Get the current received carrier frequency.
    \param s The modem context.
    \return The frequency, in Hertz. */
float v29_rx_carrier_frequency(v29_rx_state_t *s);

/*! Get the current symbol timing correction since startup.
    \param s The modem context.
    \return The correction. */
float v29_rx_symbol_timing_correction(v29_rx_state_t *s);

/*! Get the current received signal power.
    \param s The modem context.
    \return The signal power, in dBm0. */
float v29_rx_signal_power(v29_rx_state_t *s);

/*! Set the power level at which the carrier detection will cut in
    \param s The modem context.
    \param cutoff The signal cutoff power, in dBm0. */
void v29_rx_signal_cutoff(v29_rx_state_t *s, float cutoff);

/*! Set a handler routine to process QAM status reports
    \param s The modem context.
    \param handler The handler routine.
    \param user_data An opaque pointer passed to the handler routine. */
void v29_rx_set_qam_report_handler(v29_rx_state_t *s, qam_report_handler_t handler, void *user_data);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
