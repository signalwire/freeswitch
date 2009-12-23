/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v22bis_rx.c - ITU V.22bis modem receive part
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
 * $Id: v22bis_rx.c,v 1.69 2009/11/04 15:52:06 steveu Exp $
 */

/*! \file */

/* THIS IS A WORK IN PROGRESS - It is basically functional, but it is not feature
   complete, and doesn't reliably sync over the signal and noise level ranges it
   should. There are some nasty inefficiencies too!
   TODO:
        Better noise performance
        Retrain is incomplete
        Rate change is not implemented
        Remote loopback is not implemented */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/async.h"
#include "spandsp/power_meter.h"
#include "spandsp/arctan2.h"
#include "spandsp/dds.h"
#include "spandsp/complex_filters.h"

#include "spandsp/v29rx.h"
#include "spandsp/v22bis.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/v22bis.h"

#if defined(SPANDSP_USE_FIXED_POINTx)
#include "v22bis_rx_1200_floating_rrc.h"
#include "v22bis_rx_2400_floating_rrc.h"
#else
#include "v22bis_rx_1200_floating_rrc.h"
#include "v22bis_rx_2400_floating_rrc.h"
#endif

#define ms_to_symbols(t)        (((t)*600)/1000)

/*! The adaption rate coefficient for the equalizer */
#define EQUALIZER_DELTA         0.25f
/*! The number of phase shifted coefficient set for the pulse shaping/bandpass filter */
#define PULSESHAPER_COEFF_SETS  12

/*
The basic method used by the V.22bis receiver is:

    Put each sample into the pulse-shaping and phase shift filter buffer

    At T/2 rate:
        Filter and demodulate the contents of the input filter buffer, producing a sample
        in the equalizer filter buffer.

        Tune the symbol timing based on the latest 3 samples in the equalizer buffer. This
        updates the decision points for taking the T/2 samples.

        Equalize the contents of the equalizer buffer, producing a demodulated constellation
        point.

        Find the nearest constellation point to the received position. This is our received
        symbol.

        Tune the local carrier, based on the angular mismatch between the actual signal and
        the decision.
        
        Tune the equalizer, based on the mismatch between the actual signal and the decision.

        Descramble and output the bits represented by the decision.
*/

static const uint8_t space_map_v22bis[6][6] =
{
    {11,  9,  9,  6,  6,  7},
    {10,  8,  8,  4,  4,  5},
    {10,  8,  8,  4,  4,  5},
    {13, 12, 12,  0,  0,  2},
    {13, 12, 12,  0,  0,  2},
    {15, 14, 14,  1,  1,  3}
};

static const uint8_t phase_steps[4] =
{
    1, 0, 2, 3
};

static const uint8_t ones[] =
{
    0, 1, 1, 2,
    1, 2, 2, 3,
    1, 2, 2, 3,
    2, 3, 3, 4
};

SPAN_DECLARE(float) v22bis_rx_carrier_frequency(v22bis_state_t *s)
{
    return dds_frequencyf(s->rx.carrier_phase_rate);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) v22bis_rx_symbol_timing_correction(v22bis_state_t *s)
{
    return (float) s->rx.total_baud_timing_correction/((float) PULSESHAPER_COEFF_SETS*40.0f/(3.0f*2.0f));
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) v22bis_rx_signal_power(v22bis_state_t *s)
{
    return power_meter_current_dbm0(&s->rx.rx_power) + 6.34f;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v22bis_rx_signal_cutoff(v22bis_state_t *s, float cutoff)
{
    s->rx.carrier_on_power = (int32_t) (power_meter_level_dbm0(cutoff + 2.5f)*0.232f);
    s->rx.carrier_off_power = (int32_t) (power_meter_level_dbm0(cutoff - 2.5f)*0.232f);
}
/*- End of function --------------------------------------------------------*/

void v22bis_report_status_change(v22bis_state_t *s, int status)
{
    if (s->status_handler)
        s->status_handler(s->status_user_data, status);
    else if (s->put_bit)
        s->put_bit(s->put_bit_user_data, status);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v22bis_rx_equalizer_state(v22bis_state_t *s, complexf_t **coeffs)
{
    *coeffs = s->rx.eq_coeff;
    return 2*V22BIS_EQUALIZER_LEN + 1;
}
/*- End of function --------------------------------------------------------*/

void v22bis_equalizer_coefficient_reset(v22bis_state_t *s)
{
    /* Start with an equalizer based on everything being perfect */
#if defined(SPANDSP_USE_FIXED_POINTx)
    cvec_zeroi16(s->rx.eq_coeff, 2*V22BIS_EQUALIZER_LEN + 1);
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN] = complex_seti16(3*FP_FACTOR, 0*FP_FACTOR);
    s->rx.eq_delta = 32768.0f*EQUALIZER_DELTA/(2*V22BIS_EQUALIZER_LEN + 1);
#else
    cvec_zerof(s->rx.eq_coeff, 2*V22BIS_EQUALIZER_LEN + 1);
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN] = complex_setf(3.0f, 0.0f);
    s->rx.eq_delta = EQUALIZER_DELTA/(2*V22BIS_EQUALIZER_LEN + 1);
#endif
}
/*- End of function --------------------------------------------------------*/

static void equalizer_reset(v22bis_state_t *s)
{
    v22bis_equalizer_coefficient_reset(s);
#if defined(SPANDSP_USE_FIXED_POINTx)
    cvec_zeroi16(s->rx.eq_buf, V22BIS_EQUALIZER_MASK + 1);
#else
    cvec_zerof(s->rx.eq_buf, V22BIS_EQUALIZER_MASK + 1);
#endif
    s->rx.eq_put_step = 20 - 1;
    s->rx.eq_step = 0;
}
/*- End of function --------------------------------------------------------*/

static complexf_t equalizer_get(v22bis_state_t *s)
{
    int i;
    int p;
    complexf_t z;
    complexf_t z1;

    /* Get the next equalized value. */
    z = complex_setf(0.0f, 0.0f);
    p = s->rx.eq_step - 1;
    for (i = 0;  i < 2*V22BIS_EQUALIZER_LEN + 1;  i++)
    {
        p = (p - 1) & V22BIS_EQUALIZER_MASK;
        z1 = complex_mulf(&s->rx.eq_coeff[i], &s->rx.eq_buf[p]);
        z = complex_addf(&z, &z1);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

static void tune_equalizer(v22bis_state_t *s, const complexf_t *z, const complexf_t *target)
{
    int i;
    int p;
    complexf_t ez;
    complexf_t z1;

    /* Find the x and y mismatch from the exact constellation position. */
    ez = complex_subf(target, z);
    ez.re *= s->rx.eq_delta;
    ez.im *= s->rx.eq_delta;

    p = s->rx.eq_step - 1;
    for (i = 0;  i < 2*V22BIS_EQUALIZER_LEN + 1;  i++)
    {
        p = (p - 1) & V22BIS_EQUALIZER_MASK;
        z1 = complex_conjf(&s->rx.eq_buf[p]);
        z1 = complex_mulf(&ez, &z1);
        s->rx.eq_coeff[i] = complex_addf(&s->rx.eq_coeff[i], &z1);
        /* If we don't leak a little bit we seem to get some wandering adaption */
        s->rx.eq_coeff[i].re *= 0.9999f;
        s->rx.eq_coeff[i].im *= 0.9999f;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void track_carrier(v22bis_state_t *s, const complexf_t *z, const complexf_t *target)
{
    float error;

    /* For small errors the imaginary part of the difference between the actual and the target
       positions is proportional to the phase error, for any particular target. However, the
       different amplitudes of the various target positions scale things. */
    error = z->im*target->re - z->re*target->im;
    
    s->rx.carrier_phase_rate += (int32_t) (s->rx.carrier_track_i*error);
    s->rx.carrier_phase += (int32_t) (s->rx.carrier_track_p*error);
    //span_log(&s->logging, SPAN_LOG_FLOW, "Im = %15.5f   f = %15.5f\n", error, dds_frequencyf(s->rx.carrier_phase_rate));
}
/*- End of function --------------------------------------------------------*/

static __inline__ int descramble(v22bis_state_t *s, int bit)
{
    int out_bit;

    bit &= 1;

    /* Descramble the bit */
    out_bit = (bit ^ (s->rx.scramble_reg >> 13) ^ (s->rx.scramble_reg >> 16)) & 1;
    s->rx.scramble_reg = (s->rx.scramble_reg << 1) | bit;

    if (s->rx.scrambler_pattern_count >= 64)
    {
        out_bit ^= 1;
        s->rx.scrambler_pattern_count = 0;
    }
    if (bit)
        s->rx.scrambler_pattern_count++;
    else
        s->rx.scrambler_pattern_count = 0;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_bit(v22bis_state_t *s, int bit)
{
    int out_bit;

    /* Descramble the bit */
    out_bit = descramble(s, bit);
    s->put_bit(s->put_bit_user_data, out_bit);
}
/*- End of function --------------------------------------------------------*/

static void decode_baud(v22bis_state_t *s, int nearest)
{
    int raw_bits;

    raw_bits = phase_steps[((nearest >> 2) - (s->rx.constellation_state >> 2)) & 3];
    s->rx.constellation_state = nearest;
    /* The first two bits are the quadrant */
    put_bit(s, raw_bits >> 1);
    put_bit(s, raw_bits);
    if (s->rx.sixteen_way_decisions)
    {
        /* The other two bits are the position within the quadrant */
        put_bit(s, nearest >> 1);
        put_bit(s, nearest);
    }
}
/*- End of function --------------------------------------------------------*/

static int decode_baudx(v22bis_state_t *s, int nearest)
{
    int raw_bits;
    int out_bits;

    raw_bits = phase_steps[((nearest >> 2) - (s->rx.constellation_state >> 2)) & 3];
    s->rx.constellation_state = nearest;
    /* The first two bits are the quadrant */
    out_bits = descramble(s, raw_bits >> 1);
    out_bits = (out_bits << 1) | descramble(s, raw_bits);
    if (s->rx.sixteen_way_decisions)
    {
        /* The other two bits are the position within the quadrant */
        out_bits = (out_bits << 1) | descramble(s, nearest >> 1);
        out_bits = (out_bits << 1) | descramble(s, nearest);
    }
    return out_bits;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void symbol_sync(v22bis_state_t *s)
{
    float p;
    float q;
    complexf_t zz;
    complexf_t a;
    complexf_t b;
    complexf_t c;

    /* This routine adapts the position of the half baud samples entering the equalizer. */

    /* Perform a Gardner test for baud alignment on the three most recent samples. */
    if (s->rx.sixteen_way_decisions)
    {
        p = s->rx.eq_buf[(s->rx.eq_step - 3) & V22BIS_EQUALIZER_MASK].re
          - s->rx.eq_buf[(s->rx.eq_step - 1) & V22BIS_EQUALIZER_MASK].re;
        p *= s->rx.eq_buf[(s->rx.eq_step - 2) & V22BIS_EQUALIZER_MASK].re;

        q = s->rx.eq_buf[(s->rx.eq_step - 3) & V22BIS_EQUALIZER_MASK].im
        - s->rx.eq_buf[(s->rx.eq_step - 1) & V22BIS_EQUALIZER_MASK].im;
        q *= s->rx.eq_buf[(s->rx.eq_step - 2) & V22BIS_EQUALIZER_MASK].im;
    }
    else
    {
        /* Rotate the points to the 45 degree positions, to maximise the effectiveness of
           the Gardner algorithm. This is particularly significant at the start of operation
           to pull things in quickly. */
        zz = complex_setf(0.894427, 0.44721f);
        a = complex_mulf(&s->rx.eq_buf[(s->rx.eq_step - 3) & V22BIS_EQUALIZER_MASK], &zz);
        b = complex_mulf(&s->rx.eq_buf[(s->rx.eq_step - 2) & V22BIS_EQUALIZER_MASK], &zz);
        c = complex_mulf(&s->rx.eq_buf[(s->rx.eq_step - 1) & V22BIS_EQUALIZER_MASK], &zz);
        p = (a.re - c.re)*b.re;
        q = (a.im - c.im)*b.im;
    }

    s->rx.gardner_integrate += (p + q > 0.0f)  ?  s->rx.gardner_step  :  -s->rx.gardner_step;

    if (abs(s->rx.gardner_integrate) >= 16)
    {
        /* This integrate and dump approach avoids rapid changes of the equalizer put step.
           Rapid changes, without hysteresis, are bad. They degrade the equalizer performance
           when the true symbol boundary is close to a sample boundary. */
        s->rx.eq_put_step += (s->rx.gardner_integrate/16);
        s->rx.total_baud_timing_correction += (s->rx.gardner_integrate/16);
        //span_log(&s->logging, SPAN_LOG_FLOW, "Gardner kick %d [total %d]\n", s->rx.gardner_integrate, s->rx.total_baud_timing_correction);
        if (s->rx.qam_report)
            s->rx.qam_report(s->rx.qam_user_data, NULL, NULL, s->rx.gardner_integrate);
        s->rx.gardner_integrate = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static void process_half_baud(v22bis_state_t *s, const complexf_t *sample)
{
    complexf_t z;
    complexf_t zz;
    const complexf_t *target;
    int re;
    int im;
    int nearest;
    int bitstream;
    int raw_bits;

    z.re = sample->re;
    z.im = sample->im;

    /* Add a sample to the equalizer's circular buffer, but don't calculate anything
       at this time. */
    s->rx.eq_buf[s->rx.eq_step] = z;
    s->rx.eq_step = (s->rx.eq_step + 1) & V22BIS_EQUALIZER_MASK;

    /* On alternate insertions we have a whole baud and must process it. */
    if ((s->rx.baud_phase ^= 1))
        return;

    symbol_sync(s);

    z = equalizer_get(s);

    /* Find the constellation point */
    if (s->rx.sixteen_way_decisions)
    {
        re = (int) (z.re + 3.0f);
        if (re > 5)
            re = 5;
        else if (re < 0)
            re = 0;
        im = (int) (z.im + 3.0f);
        if (im > 5)
            im = 5;
        else if (im < 0)
            im = 0;
        nearest = space_map_v22bis[re][im];
    }
    else
    {
        /* Rotate to 45 degrees, to make the slicing trivial */
        zz = complex_setf(0.894427, 0.44721f);
        zz = complex_mulf(&z, &zz);
        nearest = 0x01;
        if (zz.re < 0.0f)
            nearest |= 0x04;
        if (zz.im < 0.0f)
        {
            nearest ^= 0x04;
            nearest |= 0x08;
        }
    }
    raw_bits = 0;

    switch (s->rx.training)
    {
    case V22BIS_RX_TRAINING_STAGE_NORMAL_OPERATION:
        /* Normal operation. */
        target = &v22bis_constellation[nearest];
        track_carrier(s, &z, target);
        tune_equalizer(s, &z, target);
        raw_bits = phase_steps[((nearest >> 2) - (s->rx.constellation_state >> 2)) & 3];
        /* TODO: detect unscrambled ones indicating a loopback request */

        /* Search for the S1 signal that might be requesting a retrain */
        if ((s->rx.last_raw_bits ^ raw_bits) == 0x3)
        {
            s->rx.pattern_repeats++;
        }
        else
        {
            if (s->rx.pattern_repeats >= 50  &&  (s->rx.last_raw_bits == 0x3  ||  s->rx.last_raw_bits == 0x0))
            {
                /* We should get a full run of 00 11 (about 60 bauds) at either modem. */
                span_log(&s->logging, SPAN_LOG_FLOW, "+++ S1 detected (%d long)\n", s->rx.pattern_repeats);
                span_log(&s->logging, SPAN_LOG_FLOW, "+++ Accepting a retrain request\n");
                s->rx.pattern_repeats = 0;
                s->rx.training_count = 0;
                s->rx.training = V22BIS_RX_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
                s->tx.training_count = 0;
                s->tx.training = V22BIS_TX_TRAINING_STAGE_U0011;
                v22bis_equalizer_coefficient_reset(s);
                v22bis_report_status_change(s, SIG_STATUS_MODEM_RETRAIN_OCCURRED);
            }
            s->rx.pattern_repeats = 0;
        }
        decode_baud(s, nearest);
        break;
    case V22BIS_RX_TRAINING_STAGE_SYMBOL_ACQUISITION:
        /* Allow time for the Gardner algorithm to settle the symbol timing. */
        target = &z;
        if (++s->rx.training_count >= 40)
        {
            /* QAM and Gardner only play nicely with heavy damping, so we need to change to
               a slow rate of symbol timing adaption. However, it must not be so slow that it
               cannot track the worst case timing error specified in V.22bis. This should be 0.01%,
               but since we might be off in the opposite direction from the source, the total
               error could be higher. */
            s->rx.gardner_step = 4;
            s->rx.pattern_repeats = 0;
            if (s->calling_party)
                s->rx.training = V22BIS_RX_TRAINING_STAGE_UNSCRAMBLED_ONES;
            else
                s->rx.training = V22BIS_RX_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
            /* Be pessimistic and see what the handshake brings */
            s->negotiated_bit_rate = 1200;
            break;
        }
        /* Once we have pulled in the symbol timing in a coarse way, use finer
           steps to fine tune the timing. */
        if (s->rx.training_count == 30)
            s->rx.gardner_step = 32;
        break;
    case V22BIS_RX_TRAINING_STAGE_UNSCRAMBLED_ONES:
        /* Calling modem only */
        /* The calling modem should initially receive unscrambled ones at 1200bps */
        target = &v22bis_constellation[nearest];
        track_carrier(s, &z, target);
        raw_bits = phase_steps[((nearest >> 2) - (s->rx.constellation_state >> 2)) & 3];
        s->rx.constellation_state = nearest;
        if (raw_bits != s->rx.last_raw_bits)
            s->rx.pattern_repeats = 0;
        else
            s->rx.pattern_repeats++;
        if (++s->rx.training_count == ms_to_symbols(155 + 456))
        {
            /* After the first 155ms things should have been steady, so check if the last 456ms was
               steady at 11 or 00. */
            if (raw_bits == s->rx.last_raw_bits
                &&
                (raw_bits == 0x3  ||  raw_bits == 0x0)
                &&
                s->rx.pattern_repeats >= ms_to_symbols(456))
            {
                /* It looks like the answering machine is sending us a clean unscrambled 11 or 00 */
                if (s->bit_rate == 2400)
                {
                    /* Try to establish at 2400bps */
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting U0011 (S1) (Caller)\n");
                    s->tx.training = V22BIS_TX_TRAINING_STAGE_U0011;
                    s->tx.training_count = 0;
                }
                else
                {
                    /* Only try to establish at 1200bps */
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting S11 (1200) (Caller)\n");
                    s->tx.training = V22BIS_TX_TRAINING_STAGE_S11;
                    s->tx.training_count = 0;
                }
            }
            s->rx.pattern_repeats = 0;
            s->rx.training_count = 0;
            s->rx.training = V22BIS_RX_TRAINING_STAGE_UNSCRAMBLED_ONES_SUSTAINING;
        }
        break;
    case V22BIS_RX_TRAINING_STAGE_UNSCRAMBLED_ONES_SUSTAINING:
        /* Calling modem only */
        /* Wait for the end of the unscrambled ones at 1200bps */
        target = &v22bis_constellation[nearest];
        track_carrier(s, &z, target);
        raw_bits = phase_steps[((nearest >> 2) - (s->rx.constellation_state >> 2)) & 3];
        s->rx.constellation_state = nearest;
        if (raw_bits != s->rx.last_raw_bits)
        {
            /* This looks like the end of the sustained initial unscrambled 11 or 00 */
            s->tx.training_count = 0;
            s->tx.training = V22BIS_TX_TRAINING_STAGE_TIMED_S11;
            s->rx.training_count = 0;
            s->rx.training = V22BIS_RX_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
            s->rx.pattern_repeats = 0;
        }
        break;
    case V22BIS_RX_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200:
        target = &v22bis_constellation[nearest];
        track_carrier(s, &z, target);
        tune_equalizer(s, &z, target);
        raw_bits = phase_steps[((nearest >> 2) - (s->rx.constellation_state >> 2)) & 3];
        bitstream = decode_baudx(s, nearest);
        s->rx.training_count++;
//span_log(&s->logging, SPAN_LOG_FLOW, "S11 0x%02x 0x%02x 0x%X %d %d %d %d %d\n", raw_bits, nearest, bitstream, s->rx.scrambled_ones_to_date, 0, 0, 0, s->rx.training_count);
        if (s->negotiated_bit_rate == 1200)
        {
            /* Search for the S1 signal */
            if ((s->rx.last_raw_bits ^ raw_bits) == 0x3)
            {
                s->rx.pattern_repeats++;
            }
            else
            {
                if (s->rx.pattern_repeats >= 15  &&  (s->rx.last_raw_bits == 0x3  ||  s->rx.last_raw_bits == 0x0))
                {
                    /* We should get a full run of 00 11 (about 60 bauds) at the calling modem, but only about 20
                       at the answering modem, as the first 40 are TED settling time. */
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ S1 detected (%d long)\n", s->rx.pattern_repeats);
                    if (s->bit_rate == 2400)
                    {
                        if (!s->calling_party)
                        {
                            /* Accept establishment at 2400bps */
                            span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting U0011 (S1) (Answerer)\n");
                            s->tx.training = V22BIS_TX_TRAINING_STAGE_U0011;
                            s->tx.training_count = 0;
                        }
                        s->negotiated_bit_rate = 2400;
                    }
                }
                s->rx.pattern_repeats = 0;
            }
            if (s->rx.training_count >= ms_to_symbols(270))
            {
                /* If we haven't seen the S1 signal by now, we are committed to be in 1200bps mode */
                if (s->calling_party)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ Rx normal operation (1200)\n");
                    /* The transmit side needs to sustain the scrambled ones for a timed period */
                    s->tx.training_count = 0;
                    s->tx.training = V22BIS_TX_TRAINING_STAGE_TIMED_S11;
                    /* Normal reception starts immediately */
                    s->rx.training = V22BIS_RX_TRAINING_STAGE_NORMAL_OPERATION;
                    s->rx.carrier_track_i = 8000.0f;
                }
                else
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting S11 (1200) (Answerer)\n");
                    /* The transmit side needs to sustain the scrambled ones for a timed period */
                    s->tx.training_count = 0;
                    s->tx.training = V22BIS_TX_TRAINING_STAGE_TIMED_S11;
                    /* The receive side needs to wait a timed period, receiving scrambled ones,
                       before entering normal operation. */
                    s->rx.training = V22BIS_RX_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200_SUSTAINING;
                }
            }
        }
        else
        {
            if (s->calling_party)
            {
                if (s->rx.training_count >= ms_to_symbols(100 + 450))
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting 16 way decisions (caller)\n");
                    s->rx.sixteen_way_decisions = TRUE;
                    s->rx.training = V22BIS_RX_TRAINING_STAGE_WAIT_FOR_SCRAMBLED_ONES_AT_2400;
                    s->rx.pattern_repeats = 0;
                    s->rx.carrier_track_i = 8000.0f;
                }
            }
            else
            {
                if (s->rx.training_count >= ms_to_symbols(450))
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting 16 way decisions (answerer)\n");
                    s->rx.sixteen_way_decisions = TRUE;
                    s->rx.training = V22BIS_RX_TRAINING_STAGE_WAIT_FOR_SCRAMBLED_ONES_AT_2400;
                    s->rx.pattern_repeats = 0;
                }
            }
        }
        break;
    case V22BIS_RX_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200_SUSTAINING:
        target = &v22bis_constellation[nearest];
        track_carrier(s, &z, target);
        tune_equalizer(s, &z, target);
        bitstream = decode_baudx(s, nearest);
        if (++s->rx.training_count > ms_to_symbols(270 + 765))
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "+++ Rx normal operation (1200)\n");
            s->rx.training = V22BIS_RX_TRAINING_STAGE_NORMAL_OPERATION;
        }
        break;
    case V22BIS_RX_TRAINING_STAGE_WAIT_FOR_SCRAMBLED_ONES_AT_2400:
        target = &v22bis_constellation[nearest];
        track_carrier(s, &z, target);
        tune_equalizer(s, &z, target);
        bitstream = decode_baudx(s, nearest);
        /* We need 32 sustained 1's to switch into normal operation. */
        if (bitstream == 0xF)
        {
            if (++s->rx.pattern_repeats >= 9)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "+++ Rx normal operation (2400)\n");
                s->rx.training = V22BIS_RX_TRAINING_STAGE_NORMAL_OPERATION;
            }
        }
        else
        {
            s->rx.pattern_repeats = 0;
        }
        break;
    case V22BIS_RX_TRAINING_STAGE_PARKED:
    default:
        /* We failed to train! */
        /* Park here until the carrier drops. */
        target = &z;
        break;
    }
    s->rx.last_raw_bits = raw_bits;
    if (s->rx.qam_report)
        s->rx.qam_report(s->rx.qam_user_data, &z, target, s->rx.constellation_state);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) v22bis_rx(v22bis_state_t *s, const int16_t amp[], int len)
{
    int i;
    int j;
    int step;
    complexf_t z;
    complexf_t zz;
    int32_t power;
    complexf_t sample;
    float ii;
    float qq;

    for (i = 0;  i < len;  i++)
    {
        /* Complex bandpass filter the signal, using a pair of FIRs, and RRC coeffs shifted
           to centre at 1200Hz or 2400Hz. The filters support 12 fractional phase shifts, to 
           permit signal extraction very close to the middle of a symbol. */
        s->rx.rrc_filter[s->rx.rrc_filter_step] =
        s->rx.rrc_filter[s->rx.rrc_filter_step + V22BIS_RX_FILTER_STEPS] = amp[i];
        if (++s->rx.rrc_filter_step >= V22BIS_RX_FILTER_STEPS)
            s->rx.rrc_filter_step = 0;

        /* Calculate the I filter, with an arbitrary phase step, just so we can calculate
           the signal power of the required carrier, with any guard tone or spillback of our
           own transmitted signal suppressed. */
        if (s->calling_party)
        {
            ii = rx_pulseshaper_2400_re[6][0]*s->rx.rrc_filter[s->rx.rrc_filter_step];
            for (j = 1;  j < V22BIS_RX_FILTER_STEPS;  j++)
                ii += rx_pulseshaper_2400_re[6][j]*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
        }
        else
        {
            ii = rx_pulseshaper_1200_re[6][0]*s->rx.rrc_filter[s->rx.rrc_filter_step];
            for (j = 1;  j < V22BIS_RX_FILTER_STEPS;  j++)
                ii += rx_pulseshaper_1200_re[6][j]*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
        }
        power = power_meter_update(&(s->rx.rx_power), (int16_t) ii);
        if (s->rx.signal_present)
        {
            /* Look for power below the carrier off point */
            if (power < s->rx.carrier_off_power)
            {
                v22bis_restart(s, s->bit_rate);
                v22bis_report_status_change(s, SIG_STATUS_CARRIER_DOWN);
                continue;
            }
        }
        else
        {
            /* Look for power exceeding the carrier on point */
            if (power < s->rx.carrier_on_power)
                continue;
            s->rx.signal_present = TRUE;
            v22bis_report_status_change(s, SIG_STATUS_CARRIER_UP);
        }
        if (s->rx.training != V22BIS_RX_TRAINING_STAGE_PARKED)
        {
            /* Only spend effort processing this data if the modem is not
               parked, after a training failure. */
            z = dds_complexf(&s->rx.carrier_phase, s->rx.carrier_phase_rate);
            if (s->rx.training == V22BIS_RX_TRAINING_STAGE_SYMBOL_ACQUISITION)
            {
                /* Only AGC during the initial symbol acquisition, and then lock the gain. */
                s->rx.agc_scaling = 0.18f*3.60f/sqrtf(power);
            }
            /* Put things into the equalization buffer at T/2 rate. The Gardner algorithm
               will fiddle the step to align this with the symbols. */
            if ((s->rx.eq_put_step -= PULSESHAPER_COEFF_SETS) <= 0)
            {
                /* Pulse shape while still at the carrier frequency, using a quadrature
                   pair of filters. This results in a properly bandpass filtered complex
                   signal, which can be brought directly to bandband by complex mixing.
                   No further filtering, to remove mixer harmonics, is needed. */
                step = -s->rx.eq_put_step;
                if (step > PULSESHAPER_COEFF_SETS - 1)
                    step = PULSESHAPER_COEFF_SETS - 1;
                s->rx.eq_put_step += PULSESHAPER_COEFF_SETS*40/(3*2);
                if (s->calling_party)
                {
                    ii = rx_pulseshaper_2400_re[step][0]*s->rx.rrc_filter[s->rx.rrc_filter_step];
                    qq = rx_pulseshaper_2400_im[step][0]*s->rx.rrc_filter[s->rx.rrc_filter_step];
                    for (j = 1;  j < V22BIS_RX_FILTER_STEPS;  j++)
                    {
                        ii += rx_pulseshaper_2400_re[step][j]*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
                        qq += rx_pulseshaper_2400_im[step][j]*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
                    }
                }
                else
                {
                    ii = rx_pulseshaper_1200_re[step][0]*s->rx.rrc_filter[s->rx.rrc_filter_step];
                    qq = rx_pulseshaper_1200_im[step][0]*s->rx.rrc_filter[s->rx.rrc_filter_step];
                    for (j = 1;  j < V22BIS_RX_FILTER_STEPS;  j++)
                    {
                        ii += rx_pulseshaper_1200_re[step][j]*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
                        qq += rx_pulseshaper_1200_im[step][j]*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
                    }
                }
                sample.re = ii*s->rx.agc_scaling;
                sample.im = qq*s->rx.agc_scaling;
                /* Shift to baseband - since this is done in a full complex form, the
                   result is clean, and requires no further filtering apart from the
                   equalizer. */
                zz.re = sample.re*z.re - sample.im*z.im;
                zz.im = -sample.re*z.im - sample.im*z.re;
                process_half_baud(s, &zz);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v22bis_rx_fillin(v22bis_state_t *s, int len)
{
    int i;

    /* We want to sustain the current state (i.e carrier on<->carrier off), and
       try to sustain the carrier phase. We should probably push the filters, as well */
    span_log(&s->logging, SPAN_LOG_FLOW, "Fill-in %d samples\n", len);
    if (!s->rx.signal_present)
        return 0;
    for (i = 0;  i < len;  i++)
    {
#if defined(SPANDSP_USE_FIXED_POINTx)
        dds_advance(&s->rx.carrier_phase, s->rx.carrier_phase_rate);
#else
        dds_advancef(&s->rx.carrier_phase, s->rx.carrier_phase_rate);
#endif
    }
    /* TODO: Advance the symbol phase the appropriate amount */
    return 0;
}
/*- End of function --------------------------------------------------------*/

int v22bis_rx_restart(v22bis_state_t *s)
{
    vec_zerof(s->rx.rrc_filter, sizeof(s->rx.rrc_filter)/sizeof(s->rx.rrc_filter[0]));
    s->rx.rrc_filter_step = 0;
    s->rx.scramble_reg = 0;
    s->rx.scrambler_pattern_count = 0;
    s->rx.training = V22BIS_RX_TRAINING_STAGE_SYMBOL_ACQUISITION;
    s->rx.training_count = 0;
    s->rx.signal_present = FALSE;

    s->rx.carrier_phase_rate = dds_phase_ratef((s->calling_party)  ?  2400.0f  :  1200.0f);
    s->rx.carrier_phase = 0;
    power_meter_init(&(s->rx.rx_power), 5);
    v22bis_rx_signal_cutoff(s, -45.5f);
    s->rx.agc_scaling = 0.0005f*0.025f;

    s->rx.constellation_state = 0;
    s->rx.sixteen_way_decisions = FALSE;

    equalizer_reset(s);

    s->rx.pattern_repeats = 0;
    s->rx.last_raw_bits = 0;
    s->rx.gardner_integrate = 0;
    s->rx.gardner_step = 256;
    s->rx.baud_phase = 0;
    s->rx.training_error = 0.0f;
    s->rx.total_baud_timing_correction = 0;
    /* We want the carrier to pull in faster on the answerer side, as it has very little time to adapt. */
    s->rx.carrier_track_i = (s->calling_party)  ?  8000.0f  :  40000.0f;
    s->rx.carrier_track_p = 8000000.0f;

    s->negotiated_bit_rate = 1200;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v22bis_rx_set_qam_report_handler(v22bis_state_t *s, qam_report_handler_t handler, void *user_data)
{
    s->rx.qam_report = handler;
    s->rx.qam_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
