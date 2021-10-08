/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/sig_tone.h - Signalling tone processing for the 2280Hz, 2400Hz, 2600Hz
 *                      and similar signalling tones used in older protocols.
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
 */

#if !defined(_SPANDSP_PRIVATE_SIG_TONE_H_)
#define _SPANDSP_PRIVATE_SIG_TONE_H_

/*! \brief The coefficient set for a pair of cascaded bi-quads that make a signalling notch filter. */
typedef struct
{
#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t a1[3];
    int16_t b1[3];
    int16_t a2[3];
    int16_t b2[3];
    int postscale;
#else
    float a1[3];
    float b1[3];
    float a2[3];
    float b2[3];
#endif
} sig_tone_notch_coeffs_t;

/*! \brief The coefficient set for a bi-quad that makes a signalling flat filter.
           Some signalling tone schemes require such a filter, and some don't.
           It is termed a flat filter, to distinguish it from the sharp filter,
           but obviously it is not actually flat. It is a broad band weighting
           filter. */
typedef struct
{
#if defined(SPANDSP_USE_FIXED_POINT)
    /*! \brief Flat mode bandpass bi-quad parameters */
    int16_t a[3];
    /*! \brief Flat mode bandpass bi-quad parameters */
    int16_t b[3];
    /*! \brief Post filter scaling */
    int postscale;
#else
    /*! \brief Flat mode bandpass bi-quad parameters */
    float a[3];
    /*! \brief Flat mode bandpass bi-quad parameters */
    float b[3];
#endif
} sig_tone_flat_coeffs_t;

/*!
    signalling tone descriptor. This defines the working state for a
    single instance of the transmit and receive sides of a signalling
    tone processor.
*/
typedef struct
{
    /*! \brief The tones used. */
    int tone_freq[2];
    /*! \brief The high and low tone amplitudes for each of the tones, in dBm0. */
    int tone_amp[2][2];

    /*! \brief The delay, in audio samples, before the high level tone drops
               to a low level tone. Some signalling protocols require the
               signalling tone be started at a high level, to ensure crisp
               initial detection at the receiver, but require the tone
               amplitude to drop by a number of dBs if it is sustained,
               to reduce crosstalk levels. */
    int high_low_timeout;

    /*! \brief Some signalling tone detectors use a sharp initial filter,
               changing to a broader, flatter, filter after some delay. This
               parameter defines the delay. 0 means it never changes. */
    int sharp_flat_timeout;

    /*! \brief Parameters to control the behaviour of the notch filter, used
               to remove the tone from the voice path in some protocols. The
               notch is applied as fast as possible, when the signalling tone
               is detected. Its removal is delayed by this timeout, to avoid
               clicky noises from repeated switching of the filter on rapid
               pulses of signalling tone. */
    int notch_lag_time;

    /*! \brief The tone on persistence check, in audio samples. */
    int tone_on_check_time;
    /*! \brief The tone off persistence check, in audio samples. */
    int tone_off_check_time;

    /*! \brief The number of tones used. */
    int tones;
    /*! \brief The coefficients for the cascaded bi-quads notch filter. */
    const sig_tone_notch_coeffs_t *notch[2];
    /*! \brief The coefficients for the single bi-quad flat mode filter. */
    const sig_tone_flat_coeffs_t *flat;

#if defined(SPANDSP_USE_FIXED_POINT)
    /*! \brief Minimum signalling tone to total power ratio, in dB */
    int16_t detection_ratio;
    /*! \brief Minimum total power for detection in sharp mode, in dB */
    int16_t sharp_detection_threshold;
    /*! \brief Minimum total power for detection in flat mode, in dB */
    int16_t flat_detection_threshold;
#else
    /*! \brief Minimum signalling tone to total power ratio, in dB */
    float detection_ratio;
    /*! \brief Minimum total power for detection in sharp mode, in dB */
    float sharp_detection_threshold;
    /*! \brief Minimum total power for detection in flat mode, in dB */
    float flat_detection_threshold;
#endif
} sig_tone_descriptor_t;

/*!
    Signalling tone transmit state
 */
struct sig_tone_tx_state_s
{
    /*! \brief The callback function used to handle signalling changes. */
    tone_report_func_t sig_update;
    /*! \brief A user specified opaque pointer passed to the callback function. */
    void *user_data;

    /*! \brief Tone descriptor */
    const sig_tone_descriptor_t *desc;

    /*! The phase rates for the one or two tones */
    int32_t phase_rate[2];
    /*! The phase accumulators for the one or two tones */
    uint32_t phase_acc[2];

    /*! The scaling values for the one or two tones, and the high and low level of each tone */
    int16_t tone_scaling[2][2];
    /*! The sample timer, used to switch between the high and low level tones. */
    int high_low_timer;

    /*! \brief Current transmit tone */
    int current_tx_tone;
    /*! \brief Current transmit timeout */
    int current_tx_timeout;
    /*! \brief Time in current signalling state, in samples. */
    int signalling_state_duration;
};

/*!
    Signalling tone receive state
 */
struct sig_tone_rx_state_s
{
    /*! \brief The callback function used to handle signalling changes. */
    tone_report_func_t sig_update;
    /*! \brief A user specified opaque pointer passed to the callback function. */
    void *user_data;

    /*! \brief Tone descriptor */
    const sig_tone_descriptor_t *desc;

    /*! \brief The current receive tone */
    int current_rx_tone;
    /*! \brief The timeout for switching from the high level to low level tone detector. */
    int high_low_timer;
    /*! \brief ??? */
    int current_notch_filter;

    struct
    {
#if defined(SPANDSP_USE_FIXED_POINT)
        /*! \brief The z's for the notch filter */
        int16_t notch_z1[2];
        /*! \brief The z's for the notch filter */
        int16_t notch_z2[2];
#else
        /*! \brief The z's for the notch filter */
        float notch_z1[2];
        /*! \brief The z's for the notch filter */
        float notch_z2[2];
#endif

        /*! \brief The power output of the notch. */
        power_meter_t power;
    } tone[3];

#if defined(SPANDSP_USE_FIXED_POINT)
    /*! \brief The z's for the weighting/bandpass filter. */
    int16_t flat_z[2];
#else
    /*! \brief The z's for the weighting/bandpass filter. */
    float flat_z[2];
#endif
    /*! \brief The output power of the flat (unfiltered or flat filtered) path. */
    power_meter_t flat_power;

    /*! \brief Persistence check for tone present */
    int tone_persistence_timeout;
    /*! \brief The tone pattern on the last audio sample */
    int last_sample_tone_present;

    /*! \brief The minimum reading from the power meter for detection in flat mode */
    int32_t flat_detection_threshold;
    /*! \brief The minimum reading from the power meter for detection in sharp mode */
    int32_t sharp_detection_threshold;
    /*! \brief The minimum ratio between notched power and total power for detection */
    int32_t detection_ratio;

    /*! \brief True if in flat mode. False if in sharp mode. */
    bool flat_mode;
    /*! \brief True if the notch filter is enabled in the media path */
    bool notch_enabled;
    /*! \brief ??? */
    int flat_mode_timeout;
    /*! \brief ??? */
    int notch_insertion_timeout;

    /*! \brief ??? */
    int signalling_state;
    /*! \brief ??? */
    int signalling_state_duration;
};

#endif
/*- End of file ------------------------------------------------------------*/
