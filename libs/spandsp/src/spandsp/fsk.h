/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fsk.h - FSK modem transmit and receive parts
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
 * $Id: fsk.h,v 1.30 2008/07/16 14:23:48 steveu Exp $
 */

/*! \file */

/*! \page fsk_page FSK modems
\section fsk_page_sec_1 What does it do?
Most of the oldest telephony modems use incoherent FSK modulation. This module can
be used to implement both the transmit and receive sides of a number of these
modems. There are integrated definitions for: 

 - V.21
 - V.23
 - Bell 103
 - Bell 202
 - Weitbrecht (Used for TDD - Telecoms Device for the Deaf)

The audio output or input is a stream of 16 bit samples, at 8000 samples/second.
The transmit and receive sides can be used independantly. 

\section fsk_page_sec_2 The transmitter

The FSK transmitter uses a DDS generator to synthesise the waveform. This
naturally produces phase coherent transitions, as the phase update rate is
switched, producing a clean spectrum. The symbols are not generally an integer
number of samples long. However, the symbol time for the fastest data rate
generally used (1200bps) is more than 7 samples long. The jitter resulting from
switching at the nearest sample is, therefore, acceptable. No interpolation is
used. 

\section fsk_page_sec_3 The receiver

The FSK receiver uses a quadrature correlation technique to demodulate the
signal. Two DDS quadrature oscillators are used. The incoming signal is
correlated with the oscillator signals over a period of one symbol. The
oscillator giving the highest net correlation from its I and Q outputs is the
one that matches the frequency being transmitted during the correlation
interval. Because the transmission is totally asynchronous, the demodulation
process must run sample by sample to find the symbol transitions. The
correlation is performed on a sliding window basis, so the computational load of
demodulating sample by sample is not great. 

Two modes of symbol synchronisation are provided:

    - In synchronous mode, symbol transitions are smoothed, to track their true
      position in the prescence of high timing jitter. This provides the most
      reliable symbol recovery in poor signal to noise conditions. However, it
      takes a little time to settle, so it not really suitable for data streams
      which must start up instantaneously (e.g. the TDD systems used by hearing
      impaired people).

    - In asynchronous mode each transition is taken at face value, with no temporal
      smoothing. There is no settling time for this mode, but when the signal to
      noise ratio is very poor it does not perform as well as the synchronous mode.
*/

#if !defined(_SPANDSP_FSK_H_)
#define _SPANDSP_FSK_H_

/*!
    FSK modem specification. This defines the frequencies, signal levels and
    baud rate (== bit rate for simple FSK) for a single channel of an FSK modem.
*/
typedef struct
{
    /*! Short text name for the modem. */
    const char *name;
    /*! The frequency of the zero bit state, in Hz */
    int freq_zero;
    /*! The frequency of the one bit state, in Hz */
    int freq_one;
    /*! The transmit power level, in dBm0 */
    int tx_level;
    /*! The minimum acceptable receive power level, in dBm0 */
    int min_level;
    /*! The bit rate of the modem, in units of 1/100th bps */
    int baud_rate;
} fsk_spec_t;

/* Predefined FSK modem channels */
enum
{
    FSK_V21CH1 = 0,
    FSK_V21CH2,
    FSK_V23CH1,
    FSK_V23CH2,
    FSK_BELL103CH1,
    FSK_BELL103CH2,
    FSK_BELL202,
    FSK_WEITBRECHT,     /* Used for TDD (Telecom Device for the Deaf) */
};

extern const fsk_spec_t preset_fsk_specs[];

/*!
    FSK modem transmit descriptor. This defines the state of a single working
    instance of an FSK modem transmitter.
*/
typedef struct
{
    int baud_rate;
    /*! \brief The callback function used to get the next bit to be transmitted. */
    get_bit_func_t get_bit;
    /*! \brief A user specified opaque pointer passed to the get_bit function. */
    void *get_bit_user_data;

    /*! \brief The callback function used to report modem status changes. */
    modem_tx_status_func_t status_handler;
    /*! \brief A user specified opaque pointer passed to the status function. */
    void *status_user_data;

    int32_t phase_rates[2];
    int scaling;
    int32_t current_phase_rate;
    uint32_t phase_acc;
    int baud_frac;
    int baud_inc;
    int shutdown;
} fsk_tx_state_t;

/* The longest window will probably be 106 for 75 baud */
#define FSK_MAX_WINDOW_LEN 128

/*!
    FSK modem receive descriptor. This defines the state of a single working
    instance of an FSK modem receiver.
*/
typedef struct
{
    int baud_rate;
    int sync_mode;
    /*! \brief The callback function used to put each bit received. */
    put_bit_func_t put_bit;
    /*! \brief A user specified opaque pointer passed to the put_bit routine. */
    void *put_bit_user_data;

    /*! \brief The callback function used to report modem status changes. */
    modem_tx_status_func_t status_handler;
    /*! \brief A user specified opaque pointer passed to the status function. */
    void *status_user_data;

    int32_t carrier_on_power;
    int32_t carrier_off_power;
    power_meter_t power;
    /*! \brief The value of the last signal sample, using the a simple HPF for signal power estimation. */
    int16_t last_sample;
    /*! \brief >0 if a signal above the minimum is present. It may or may not be a V.29 signal. */
    int signal_present;

    int32_t phase_rate[2];
    uint32_t phase_acc[2];

    int correlation_span;

    complexi32_t window[2][FSK_MAX_WINDOW_LEN];
    complexi32_t dot[2];
    int buf_ptr;

    int baud_inc;
    int baud_pll;
    int lastbit;
    int scaling_shift;
} fsk_rx_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Initialise an FSK modem transmit context.
    \brief Initialise an FSK modem transmit context.
    \param s The modem context.
    \param spec The specification of the modem tones and rate.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer.
    \return A pointer to the modem context, or NULL if there was a problem. */
fsk_tx_state_t *fsk_tx_init(fsk_tx_state_t *s,
                            const fsk_spec_t *spec,
                            get_bit_func_t get_bit,
                            void *user_data);

/*! Adjust an FSK modem transmit context's power output.
    \brief Adjust an FSK modem transmit context's power output.
    \param s The modem context.
    \param power The power level, in dBm0 */
void fsk_tx_power(fsk_tx_state_t *s, float power);

void fsk_tx_set_get_bit(fsk_tx_state_t *s, get_bit_func_t get_bit, void *user_data);

/*! Change the modem status report function associated with an FSK modem transmit context.
    \brief Change the modem status report function associated with an FSK modem transmit context.
    \param s The modem context.
    \param handler The callback routine used to report modem status changes.
    \param user_data An opaque pointer. */
void fsk_tx_set_modem_status_handler(fsk_tx_state_t *s, modem_tx_status_func_t handler, void *user_data);

/*! Generate a block of FSK modem audio samples.
    \brief Generate a block of FSK modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int fsk_tx(fsk_tx_state_t *s, int16_t *amp, int len);

/*! Get the current received signal power.
    \param s The modem context.
    \return The signal power, in dBm0. */
float fsk_rx_signal_power(fsk_rx_state_t *s);

/*! Adjust an FSK modem receive context's carrier detect power threshold.
    \brief Adjust an FSK modem receive context's carrier detect power threshold.
    \param s The modem context.
    \param cutoff The power level, in dBm0 */
void fsk_rx_signal_cutoff(fsk_rx_state_t *s, float cutoff);

/*! Initialise an FSK modem receive context.
    \brief Initialise an FSK modem receive context.
    \param s The modem context.
    \param spec The specification of the modem tones and rate.
    \param sync_mode TRUE for synchronous modem. FALSE for asynchronous mode.
    \param put_bit The callback routine used to put the received data.
    \param user_data An opaque pointer.
    \return A pointer to the modem context, or NULL if there was a problem. */
fsk_rx_state_t *fsk_rx_init(fsk_rx_state_t *s,
                            const fsk_spec_t *spec,
                            int sync_mode,
                            put_bit_func_t put_bit,
                            void *user_data);

/*! Process a block of received FSK modem audio samples.
    \brief Process a block of received FSK modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed.
*/
int fsk_rx(fsk_rx_state_t *s, const int16_t *amp, int len);

void fsk_rx_set_put_bit(fsk_rx_state_t *s, put_bit_func_t put_bit, void *user_data);

/*! Change the modem status report function associated with an FSK modem receive context.
    \brief Change the modem status report function associated with an FSK modem receive context.
    \param s The modem context.
    \param handler The callback routine used to report modem status changes.
    \param user_data An opaque pointer. */
void fsk_rx_set_modem_status_handler(fsk_rx_state_t *s, modem_rx_status_func_t handler, void *user_data);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
