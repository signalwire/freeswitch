/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/sig_tone.h - Signalling tone processing for the 2280Hz, 2400Hz, 2600Hz
 *                      and similar signalling tone used in older protocols.
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
 * $Id: sig_tone.h,v 1.1 2008/11/30 13:08:42 steveu Exp $
 */

#if !defined(_SPANDSP_PRIVATE_SIG_TONE_H_)
#define _SPANDSP_PRIVATE_SIG_TONE_H_

/*!
    Signaling tone descriptor. This defines the working state for a
    single instance of the transmit and receive sides of a signaling
    tone processor.
*/
struct sig_tone_descriptor_s
{
    /*! \brief The tones used. */
    int tone_freq[2];
    /*! \brief The high and low tone amplitudes. */
    int tone_amp[2];

    /*! \brief The delay, in audio samples, before the high level tone drops
               to a low level tone. */
    int high_low_timeout;

    /*! \brief Some signaling tone detectors use a sharp initial filter,
               changing to a broader band filter after some delay. This
               parameter defines the delay. 0 means it never changes. */
    int sharp_flat_timeout;

    /*! \brief Parameters to control the behaviour of the notch filter, used
               to remove the tone from the voice path in some protocols. */
    int notch_lag_time;
    int notch_allowed;

    /*! \brief The tone on persistence check, in audio samples. */
    int tone_on_check_time;
    /*! \brief The tone off persistence check, in audio samples. */
    int tone_off_check_time;

    int tones;
    /*! \brief The coefficients for the cascaded bi-quads notch filter. */
    struct
    {
#if defined(SPANDSP_USE_FIXED_POINT)
        int32_t notch_a1[3];
        int32_t notch_b1[3];
        int32_t notch_a2[3];
        int32_t notch_b2[3];
        int notch_postscale;
#else
        float notch_a1[3];
        float notch_b1[3];
        float notch_a2[3];
        float notch_b2[3];
#endif
    } tone[2];


    /*! \brief Flat mode bandpass bi-quad parameters */
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t broad_a[3];
    int32_t broad_b[3];
    int broad_postscale;
#else
    float broad_a[3];
    float broad_b[3];
#endif
    /*! \brief The coefficients for the post notch leaky integrator. */
    int32_t notch_slugi;
    int32_t notch_slugp;

    /*! \brief The coefficients for the post modulus leaky integrator in the
               unfiltered data path.  The prescale value incorporates the
               detection ratio. This is called the guard ratio in some
               protocols. */
    int32_t unfiltered_slugi;
    int32_t unfiltered_slugp;

    /*! \brief The coefficients for the post modulus leaky integrator in the
               bandpass filter data path. */
    int32_t broad_slugi;
    int32_t broad_slugp;

    /*! \brief Masks which effectively threshold the notched, weighted and
               bandpassed data. */
    int32_t notch_threshold;
    int32_t unfiltered_threshold;
    int32_t broad_threshold;
};

struct sig_tone_tx_state_s
{
    /*! \brief The callback function used to handle signaling changes. */
    sig_tone_func_t sig_update;
    /*! \brief A user specified opaque pointer passed to the callback function. */
    void *user_data;

    sig_tone_descriptor_t *desc;

    /*! The scaling values for the high and low level tones */
    int32_t tone_scaling[2];
    /*! The sample timer, used to switch between the high and low level tones. */
    int high_low_timer;

    /*! The phase rates for the one or two tones */
    int32_t phase_rate[2];
    /*! The phase accumulators for the one or two tones */
    uint32_t phase_acc[2];

    int current_tx_tone;
    int current_tx_timeout;
    int signaling_state_duration;
};

struct sig_tone_rx_state_s
{
    /*! \brief The callback function used to handle signaling changes. */
    sig_tone_func_t sig_update;
    /*! \brief A user specified opaque pointer passed to the callback function. */
    void *user_data;

    sig_tone_descriptor_t *desc;

    int current_rx_tone;
    int high_low_timer;

    struct
    {
        /*! \brief The z's for the notch filter */
#if defined(SPANDSP_USE_FIXED_POINT)
        int32_t notch_z1[3];
        int32_t notch_z2[3];
#else
        float notch_z1[3];
        float notch_z2[3];
#endif

        /*! \brief The z's for the notch integrators. */
        int32_t notch_zl;
    } tone[2];

    /*! \brief The z's for the weighting/bandpass filter. */
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t broad_z[3];
#else
    float broad_z[3];
#endif
    /*! \brief The z for the broadband integrator. */
    int32_t broad_zl;

    int flat_mode;
    int tone_present;
    int notch_enabled;
    int flat_mode_timeout;
    int notch_insertion_timeout;
    int tone_persistence_timeout;
    
    int signaling_state_duration;
};

#endif
/*- End of file ------------------------------------------------------------*/
