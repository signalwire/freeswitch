/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v22bis.h - ITU V.22bis modem
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: v22bis.h,v 1.30 2008/07/25 13:56:54 steveu Exp $
 */

/*! \file */

/*! \page v22bis_page The V.22bis modem
\section v22bis_page_sec_1 What does it do?
The V.22bis modem is a duplex modem for general data use on the PSTN, at rates
of 1200 and 2400 bits/second. It is a compatible extension of the V.22 modem,
which is a 1200 bits/second only design. It is a band-split design, using carriers
of 1200Hz and 2400Hz. It is the fastest PSTN modem in general use which does not
use echo-cancellation.

\section v22bis__page_sec_2 How does it work?
V.22bis uses 4PSK modulation at 1200 bits/second or 16QAM modulation at 2400
bits/second. At 1200bps the symbols are so long that a fixed compromise equaliser
is sufficient to recover the 4PSK signal reliably. At 2400bps an adaptive
equaliser is necessary.

The V.22bis training sequence includes sections which allow the modems to determine
if the far modem can support (or is willing to support) 2400bps operation. The
modems will automatically use 2400bps if both ends are willing to use that speed,
or 1200bps if one or both ends to not acknowledge that 2400bps is OK.
*/

#if !defined(_V22BIS_H_)
#define _V22BIS_H_

#define V22BIS_EQUALIZER_LEN    7  /* this much to the left and this much to the right */
#define V22BIS_EQUALIZER_MASK   15 /* one less than a power of 2 >= (2*V22BIS_EQUALIZER_LEN + 1) */

#define V22BIS_TX_FILTER_STEPS  9

#define V22BIS_RX_FILTER_STEPS  37

/*!
    V.22bis modem descriptor. This defines the working state for a single instance
    of a V.22bis modem.
*/
typedef struct
{
    /*! \brief The bit rate of the modem. Valid values are 1200 and 2400. */
    int bit_rate;
    /*! \brief TRUE is this is the calling side modem. */
    int caller;
    /*! \brief The callback function used to put each bit received. */
    put_bit_func_t put_bit;
    /*! \brief The callback function used to get the next bit to be transmitted. */
    get_bit_func_t get_bit;
    /*! \brief A user specified opaque pointer passed to the callback routines. */
    void *user_data;

    /* RECEIVE SECTION */
    struct
    {
        /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
        float rrc_filter[2*V22BIS_RX_FILTER_STEPS];
        /*! \brief Current offset into the RRC pulse shaping filter buffer. */
        int rrc_filter_step;

        /*! \brief The register for the data scrambler. */
        unsigned int scramble_reg;
        /*! \brief A counter for the number of consecutive bits of repeating pattern through
                   the scrambler. */
        int scrambler_pattern_count;

        /*! \brief 0 if receiving user data. A training stage value during training */
        int training;
        /*! \brief A count of how far through the current training step we are. */
        int training_count;

        /*! \brief >0 if a signal above the minimum is present. It may or may not be a V.22bis signal. */
        int signal_present;

        /*! \brief A measure of how much mismatch there is between the real constellation,
            and the decoded symbol positions. */
        float training_error;

        /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
        uint32_t carrier_phase;
        /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
        int32_t carrier_phase_rate;
        /*! \brief The proportional part of the carrier tracking filter. */
        float carrier_track_p;
        /*! \brief The integral part of the carrier tracking filter. */
        float carrier_track_i;

        /*! \brief A callback function which may be enabled to report every symbol's
                   constellation position. */
        qam_report_handler_t qam_report;
        /*! \brief A user specified opaque pointer passed to the qam_report callback
                   routine. */
        void *qam_user_data;

        /*! \brief A power meter, to measure the HPF'ed signal power in the channel. */    
        power_meter_t rx_power;
        /*! \brief The power meter level at which carrier on is declared. */
        int32_t carrier_on_power;
        /*! \brief The power meter level at which carrier off is declared. */
        int32_t carrier_off_power;
        /*! \brief The scaling factor accessed by the AGC algorithm. */
        float agc_scaling;
    
        int constellation_state;

        /*! \brief The current delta factor for updating the equalizer coefficients. */
        float eq_delta;
#if defined(SPANDSP_USE_FIXED_POINTx)
        /*! \brief The adaptive equalizer coefficients. */
        complexi_t eq_coeff[2*V22BIS_EQUALIZER_LEN + 1];
        /*! \brief The equalizer signal buffer. */
        complexi_t eq_buf[V22BIS_EQUALIZER_MASK + 1];
#else
        complexf_t eq_coeff[2*V22BIS_EQUALIZER_LEN + 1];
        complexf_t eq_buf[V22BIS_EQUALIZER_MASK + 1];
#endif
        /*! \brief Current offset into the equalizer buffer. */
        int eq_step;
        /*! \brief Current write offset into the equalizer buffer. */
        int eq_put_step;

        /*! \brief Integration variable for damping the Gardner algorithm tests. */
        int gardner_integrate;
        /*! \brief Current step size of Gardner algorithm integration. */
        int gardner_step;
        /*! \brief The total symbol timing correction since the carrier came up.
                   This is only for performance analysis purposes. */
        int total_baud_timing_correction;
        /*! \brief The current fractional phase of the baud timing. */
        int baud_phase;
    
        int sixteen_way_decisions;
    } rx;

    /* TRANSMIT SECTION */
    struct
    {
        /*! \brief The gain factor needed to achieve the specified output power. */
        float gain;

        /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
        complexf_t rrc_filter[2*V22BIS_TX_FILTER_STEPS];
        /*! \brief Current offset into the RRC pulse shaping filter buffer. */
        int rrc_filter_step;

        /*! \brief The register for the data scrambler. */
        unsigned int scramble_reg;
        /*! \brief A counter for the number of consecutive bits of repeating pattern through
                   the scrambler. */
        int scrambler_pattern_count;

        /*! \brief 0 if transmitting user data. A training stage value during training */
        int training;
        /*! \brief A counter used to track progress through sending the training sequence. */
        int training_count;
        /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
        uint32_t carrier_phase;
        /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
        int32_t carrier_phase_rate;
        /*! \brief The current phase of the guard tone (i.e. the DDS parameter). */
        uint32_t guard_phase;
        /*! \brief The update rate for the phase of the guard tone (i.e. the DDS increment). */
        int32_t guard_phase_rate;
        float guard_level;
        /*! \brief The current fractional phase of the baud timing. */
        int baud_phase;
        /*! \brief The code number for the current position in the constellation. */
        int constellation_state;
        /*! \brief An indicator to mark that we are tidying up to stop transmission. */
        int shutdown;
        /*! \brief The get_bit function in use at any instant. */
        get_bit_func_t current_get_bit;
    } tx;

    int detected_unscrambled_ones;
    int detected_unscrambled_zeros;

    int detected_unscrambled_ones_or_zeros;
    int detected_unscrambled_0011_ending;
    int detected_scrambled_ones_or_zeros_at_1200bps;
    int detected_scrambled_ones_at_2400bps;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
} v22bis_state_t;

extern const complexf_t v22bis_constellation[16];

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Reinitialise an existing V.22bis modem receive context.
    \brief Reinitialise an existing V.22bis modem receive context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 1200 and 2400.
    \return 0 for OK, -1 for bad parameter */
int v22bis_rx_restart(v22bis_state_t *s, int bit_rate);

/*! Process a block of received V.22bis modem audio samples.
    \brief Process a block of received V.22bis modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. */
int v22bis_rx(v22bis_state_t *s, const int16_t amp[], int len);

/*! Get a snapshot of the current equalizer coefficients.
    \brief Get a snapshot of the current equalizer coefficients.
    \param coeffs The vector of complex coefficients.
    \return The number of coefficients in the vector. */
int v22bis_equalizer_state(v22bis_state_t *s, complexf_t **coeffs);

/*! Get the current received carrier frequency.
    \param s The modem context.
    \return The frequency, in Hertz. */
float v22bis_rx_carrier_frequency(v22bis_state_t *s);

/*! Get the current symbol timing correction since startup.
    \param s The modem context.
    \return The correction. */
float v22bis_symbol_timing_correction(v22bis_state_t *s);

/*! Get a current received signal power.
    \param s The modem context.
    \return The signal power, in dBm0. */
float v22bis_rx_signal_power(v22bis_state_t *s);

/*! Set a handler routine to process QAM status reports
    \param s The modem context.
    \param handler The handler routine.
    \param user_data An opaque pointer passed to the handler routine. */
void v22bis_set_qam_report_handler(v22bis_state_t *s, qam_report_handler_t handler, void *user_data);

/*! Generate a block of V.22bis modem audio samples.
    \brief Generate a block of V.22bis modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples to be generated.
    \return The number of samples actually generated. */
int v22bis_tx(v22bis_state_t *s, int16_t amp[], int len);

/*! Adjust a V.22bis modem transmit context's power output.
    \brief Adjust a V.22bis modem transmit context's output power.
    \param s The modem context.
    \param power The power level, in dBm0 */
void v22bis_tx_power(v22bis_state_t *s, float power);

/*! Reinitialise an existing V.22bis modem context, so it may be reused.
    \brief Reinitialise an existing V.22bis modem context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 1200 and 2400.
    \return 0 for OK, -1 for bad parameter. */
int v22bis_restart(v22bis_state_t *s, int bit_rate);

/*! Initialise a V.22bis modem context. This must be called before the first
    use of the context, to initialise its contents.
    \brief Initialise a V.22bis modem context.
    \param s The modem context.
    \param bit_rate The bit rate of the modem. Valid values are 1200 and 2400.
    \param guard The guard tone option. 0 = none, 1 = 550Hz, 2 = 1800Hz.
    \param caller TRUE if this is the calling modem.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param put_bit The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer, passed in calls to the get and put routines.
    \return A pointer to the modem context, or NULL if there was a problem. */
v22bis_state_t *v22bis_init(v22bis_state_t *s,
                            int bit_rate,
                            int guard,
                            int caller,
                            get_bit_func_t get_bit,
                            put_bit_func_t put_bit,
                            void *user_data);

/*! Free a V.22bis modem receive context.
    \brief Free a V.22bis modem receive context.
    \param s The modem context.
    \return 0 for OK */
int v22bis_free(v22bis_state_t *s);

/*! Change the get_bit function associated with a V.22bis modem context.
    \brief Change the get_bit function associated with a V.22bis modem context.
    \param s The modem context.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer. */
void v22bis_set_get_bit(v22bis_state_t *s, get_bit_func_t get_bit, void *user_data);

/*! Change the get_bit function associated with a V.22bis modem context.
    \brief Change the put_bit function associated with a V.22bis modem context.
    \param s The modem context.
    \param put_bit The callback routine used to process the data received.
    \param user_data An opaque pointer. */
void v22bis_set_put_bit(v22bis_state_t *s, put_bit_func_t put_bit, void *user_data);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
