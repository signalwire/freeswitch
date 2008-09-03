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
 * $Id: v22bis_rx.c,v 1.40 2008/07/25 13:56:54 steveu Exp $
 */

/*! \file */

/* THIS IS A WORK IN PROGRESS - NOT YET FUNCTIONAL! */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/vector_float.h"
#include "spandsp/async.h"
#include "spandsp/power_meter.h"
#include "spandsp/arctan2.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/complex_filters.h"

#include "spandsp/v29rx.h"
#include "spandsp/v22bis.h"

#if defined(SPANDSP_USE_FIXED_POINT)
#include "v22bis_rx_1200_floating_rrc.h"
#include "v22bis_rx_2400_floating_rrc.h"
#else
#include "v22bis_rx_1200_floating_rrc.h"
#include "v22bis_rx_2400_floating_rrc.h"
#endif

#define ms_to_symbols(t)        (((t)*600)/1000)

#define EQUALIZER_DELTA         0.25f
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

enum
{
    V22BIS_TRAINING_STAGE_NORMAL_OPERATION,
    V22BIS_TRAINING_STAGE_SYMBOL_ACQUISITION,
    V22BIS_TRAINING_STAGE_LOG_PHASE,
    V22BIS_TRAINING_STAGE_UNSCRAMBLED_ONES,
    V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011,
    V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200,
    V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_2400,
    V22BIS_TRAINING_STAGE_WAIT_FOR_START_1,
    V22BIS_TRAINING_STAGE_WAIT_FOR_START_2,
    V22BIS_TRAINING_STAGE_PARKED
};

static const uint8_t space_map_v22bis[6][6] =
{
    {11,  9,  9,  6,  6,  7},
    {10,  8,  8,  4,  4,  5},
    {10,  8,  8,  4,  4,  5},
    {13, 12, 12,  0,  0,  2},
    {13, 12, 12,  0,  0,  2},
    {15, 14, 14,  1,  1,  3}
};

float v22bis_rx_carrier_frequency(v22bis_state_t *s)
{
    return dds_frequencyf(s->rx.carrier_phase_rate);
}
/*- End of function --------------------------------------------------------*/

float v22bis_symbol_timing_correction(v22bis_state_t *s)
{
    return (float) s->rx.total_baud_timing_correction/((float) PULSESHAPER_COEFF_SETS*40.0f/(3.0f*2.0f));
}
/*- End of function --------------------------------------------------------*/

float v22bis_rx_signal_power(v22bis_state_t *s)
{
    return power_meter_current_dbm0(&s->rx.rx_power);
}
/*- End of function --------------------------------------------------------*/

int v22bis_equalizer_state(v22bis_state_t *s, complexf_t **coeffs)
{
    *coeffs = s->rx.eq_coeff;
    return 2*V22BIS_EQUALIZER_LEN + 1;
}
/*- End of function --------------------------------------------------------*/

static void equalizer_reset(v22bis_state_t *s)
{
    int i;

    /* Start with an equalizer based on everything being perfect */
    for (i = 0;  i < 2*V22BIS_EQUALIZER_LEN + 1;  i++)
        s->rx.eq_coeff[i] = complex_setf(0.0f, 0.0f);
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN] = complex_setf(3.0f, 0.0f);
    for (i = 0;  i <= V22BIS_EQUALIZER_MASK;  i++)
        s->rx.eq_buf[i] = complex_setf(0.0f, 0.0f);

    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN - 6].re = -0.02f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN - 5].re =  0.035f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN - 4].re =  0.08f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN - 3].re = -0.30f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN - 2].re = -0.37f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN - 1].re =  0.09f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN].re     =  3.19f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN + 1].re =  0.09f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN + 2].re = -0.37f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN + 3].re = -0.30f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN + 5].re =  0.035f;
    s->rx.eq_coeff[V22BIS_EQUALIZER_LEN + 6].re = -0.02f;

    s->rx.eq_put_step = 20 - 1;
    s->rx.eq_step = 0;
    s->rx.eq_delta = EQUALIZER_DELTA/(2*V22BIS_EQUALIZER_LEN + 1);
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

static __inline__ void put_bit(v22bis_state_t *s, int bit)
{
    int out_bit;

    bit &= 1;

    /* Descramble the bit */
    s->rx.scramble_reg = (s->rx.scramble_reg << 1) | bit;
    out_bit = (bit ^ (s->rx.scramble_reg >> 15) ^ (s->rx.scramble_reg >> 18)) & 1;
    if (s->rx.scrambler_pattern_count >= 64)
    {
        out_bit ^= 1;
        s->rx.scrambler_pattern_count = 0;
    }
    if (bit)
        s->rx.scrambler_pattern_count++;
    else
        s->rx.scrambler_pattern_count = 0;

    s->put_bit(s->user_data, out_bit);
}
/*- End of function --------------------------------------------------------*/

static void decode_baud(v22bis_state_t *s, int nearest)
{
    static const uint8_t phase_steps[4] =
    {
        1, 0, 2, 3
    };
    int raw_bits;

    raw_bits = phase_steps[((nearest - s->rx.constellation_state) >> 2) & 3];
    /* The first two bits are the quadrant */
    put_bit(s, raw_bits);
    put_bit(s, raw_bits >> 1);
    if (s->bit_rate == 2400)
    {
        /* The other two bits are the position within the quadrant */
        put_bit(s, nearest >> 1);
        put_bit(s, nearest);
    }
    s->rx.constellation_state = nearest;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int find_quadrant(const complexf_t *z)
{
    int b1;
    int b2;

#if 0
    /* Split along the axes, as follows:
        1  0
        2  3
     */
    b1 = (z->re <= 0.0f);
    b2 = (z->im <= 0.0f);
    return (b2 << 1) | (b1 ^ b2);
#endif    
    /* Split the space along the two diagonals, as follows:
         \ 1 /
        2     0
         / 3 \
     */
    b1 = (z->im > z->re);
    b2 = (z->im < -z->re);
    return (b2 << 1) | (b1 ^ b2);
}
/*- End of function --------------------------------------------------------*/

static void process_half_baud(v22bis_state_t *s, const complexf_t *sample)
{
    complexf_t a;
    complexf_t b;
    complexf_t c;

    complexf_t z;
    complexf_t zz;
    const complexf_t *target;
    float p;
    float q;
    int re;
    int im;
    int nearest;

    z.re = sample->re;
    z.im = sample->im;

    /* Add a sample to the equalizer's circular buffer, but don't calculate anything
       at this time. */
    s->rx.eq_buf[s->rx.eq_step] = z;
    s->rx.eq_step = (s->rx.eq_step + 1) & V22BIS_EQUALIZER_MASK;

    /* On alternate insertions we have a whole baud and must process it. */
    if ((s->rx.baud_phase ^= 1))
        return;

    /* Perform a Gardner test for baud alignment on the three most recent samples. */
#if 0
    p = s->rx.eq_buf[(s->rx.eq_step - 3) & V22BIS_EQUALIZER_MASK].re
      - s->rx.eq_buf[(s->rx.eq_step - 1) & V22BIS_EQUALIZER_MASK].re;
    p *= s->rx.eq_buf[(s->rx.eq_step - 2) & V22BIS_EQUALIZER_MASK].re;

    q = s->rx.eq_buf[(s->rx.eq_step - 3) & V22BIS_EQUALIZER_MASK].im
      - s->rx.eq_buf[(s->rx.eq_step - 1) & V22BIS_EQUALIZER_MASK].im;
    q *= s->rx.eq_buf[(s->rx.eq_step - 2) & V22BIS_EQUALIZER_MASK].im;
#else
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
        /* Rotate the points to the 45 degree positions, to maximise the effectiveness of the Gardner algorithm */
        zz = complex_setf(cosf(26.57f*3.14159f/180.0f), sinf(26.57f*3.14159f/180.0f));
        a = complex_mulf(&s->rx.eq_buf[(s->rx.eq_step - 3) & V22BIS_EQUALIZER_MASK], &zz);
        b = complex_mulf(&s->rx.eq_buf[(s->rx.eq_step - 2) & V22BIS_EQUALIZER_MASK], &zz);
        c = complex_mulf(&s->rx.eq_buf[(s->rx.eq_step - 1) & V22BIS_EQUALIZER_MASK], &zz);
        p = (a.re - c.re)*b.re;
        q = (a.im - c.im)*b.im;
    }
#endif

    p += q;
    s->rx.gardner_integrate += ((p + q) > 0.0f)  ?  s->rx.gardner_step  :  -s->rx.gardner_step;

    if (abs(s->rx.gardner_integrate) >= 16)
    {
        /* This integrate and dump approach avoids rapid changes of the equalizer put step.
           Rapid changes, without hysteresis, are bad. They degrade the equalizer performance
           when the true symbol boundary is close to a sample boundary. */
        s->rx.eq_put_step += (s->rx.gardner_integrate/16);
        s->rx.total_baud_timing_correction += (s->rx.gardner_integrate/16);
span_log(&s->logging, SPAN_LOG_FLOW, "Gardner kick %d [total %d]\n", s->rx.gardner_integrate, s->rx.total_baud_timing_correction);
        if (s->rx.qam_report)
            s->rx.qam_report(s->rx.qam_user_data, NULL, NULL, s->rx.gardner_integrate);
        s->rx.gardner_integrate = 0;
    }

    z = equalizer_get(s);
printf("VVV %15.5f %15.5f\n", z.re, z.im);

span_log(&s->logging, SPAN_LOG_FLOW, "VVV %p %d\n", s->user_data, s->rx.training);
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
        zz = complex_setf(3.0f/sqrtf(10.0f), -1.0f/sqrtf(10.0f));
        zz = complex_mulf(&z, &zz);
        nearest = (find_quadrant(&zz) << 2) | 0x01;
        printf("Trackit %15.5f %15.5f     %15.5f %15.5f   %d\n", z.re, z.im, zz.re, zz.im, nearest);
    }

    switch (s->rx.training)
    {
    case V22BIS_TRAINING_STAGE_NORMAL_OPERATION:
        /* Normal operation. */
        track_carrier(s, &z, &v22bis_constellation[nearest]);
        tune_equalizer(s, &z, &v22bis_constellation[nearest]);
        decode_baud(s, nearest);
        target = &v22bis_constellation[s->rx.constellation_state];
        break;
    case V22BIS_TRAINING_STAGE_SYMBOL_ACQUISITION:
        /* Allow time for the Gardner algorithm to settle the symbol timing. */
        target = &z;
        if (++s->rx.training_count >= 40)
        {
            s->rx.gardner_step = 4;
            if (s->caller)
            {
                s->rx.training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_ONES;
            }
            else
            {
                if (s->bit_rate == 2400)
                    s->rx.training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011;
                else
                    s->rx.training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
            }
            break;
        }

        /* QAM and Gardner only play nicely with heavy damping, so we need to change to
           a slow rate of symbol timing adaption. However, it must not be so slow that it
           cannot track the worst case timing error specified in V.22bis. This should be 0.01%,
           but since we might be off in the opposite direction from the source, the total
           error could be higher. */
        if (s->rx.training_count == 30)
            s->rx.gardner_step = 32;
        break;
    case V22BIS_TRAINING_STAGE_UNSCRAMBLED_ONES:
        /* The answering modem should initially receive unscrambled ones at 1200bps */
        track_carrier(s, &z, &v22bis_constellation[nearest]);
        target = &z;
        if (nearest == ((s->rx.constellation_state - 4) & 0x0F))
            s->detected_unscrambled_ones++;
        if (nearest == ((s->rx.constellation_state + 4) & 0x0F))
            s->detected_unscrambled_zeros++;
        s->rx.constellation_state = nearest;
span_log(&s->logging, SPAN_LOG_FLOW, "TWIDDLING THUMBS - %d\n", s->rx.training_count);
        if (++s->rx.training_count == ms_to_symbols(155 + 456))
        {
            if (s->detected_unscrambled_ones >= 250  ||  s->detected_unscrambled_zeros >= 250)
                s->detected_unscrambled_ones_or_zeros = TRUE;
        }
        if (s->rx.training_count == ms_to_symbols(155 + 457))
        {
            /* We should only bother looking for the 2400bps marker if we are allowed to use
               2400bps */
            s->rx.training_count = 0;
            if (s->bit_rate == 2400)
                s->rx.training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011;
            else
                s->rx.training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
        }
        break;
    case V22BIS_TRAINING_STAGE_UNSCRAMBLED_0011:
s->rx.sixteen_way_decisions = TRUE;
        /* If we can actually find this it means we can use 2400bps. If we find unscrambled ones, it means we
           we must use 1200bps. */
        track_carrier(s, &z, &v22bis_constellation[nearest]);
        tune_equalizer(s, &z, &v22bis_constellation[nearest]);
        target = &z;
        //s->rx.carrier_track_i = 1000.0f;
        //s->rx.carrier_track_p = 1000000.0f;
#if 0
        if (++s->rx.training_count > ms_to_symbols(800))
        {
            s->detected_unscrambled_0011_ending = TRUE;
            s->rx.training = V22BIS_TRAINING_STAGE_UNSCRAMBLED_ONES;
        }
#else
        if (++s->rx.training_count == 1)
        {
            s->detected_unscrambled_zeros = nearest;
            s->detected_unscrambled_ones = 0;
        }
        else
        {
span_log(&s->logging, SPAN_LOG_FLOW, "0x%X 0x%X 0x%X\n", s->detected_unscrambled_zeros, nearest, (s->detected_unscrambled_zeros + nearest) & 0x0F);
            if ((s->rx.training_count & 1) == 0)
            {
span_log(&s->logging, SPAN_LOG_FLOW, "AAA\n");
                if (((s->detected_unscrambled_zeros + nearest) & 0x0F) == 0x06)
                    s->detected_unscrambled_ones = 1;
                else if (((s->detected_unscrambled_zeros + nearest) & 0x0F) == 0x02)
                    s->detected_unscrambled_ones = -1;
                else
                {
span_log(&s->logging, SPAN_LOG_FLOW, "AAA 1\n");
                    if (s->detected_unscrambled_ones > 5  ||  s->detected_unscrambled_ones < -5)
                        s->detected_unscrambled_0011_ending = TRUE;
                    else
                        s->bit_rate = 1200;
                    s->rx.training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
                }
            }
            else
            {
span_log(&s->logging, SPAN_LOG_FLOW, "BBB\n");
                if (((s->detected_unscrambled_zeros + nearest) & 0x0F) == 0x06)
                    s->detected_unscrambled_ones = 1;
                else if (((s->detected_unscrambled_zeros + nearest) & 0x0F) == 0x02)
                    s->detected_unscrambled_ones = -1;
                else
                {
span_log(&s->logging, SPAN_LOG_FLOW, "BBB 1\n");
                    if (s->detected_unscrambled_ones > 5  ||  s->detected_unscrambled_ones < -5)
                        s->detected_unscrambled_0011_ending = TRUE;
                    else
                        s->bit_rate = 1200;
                    s->rx.training = V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
                }
            }
        }
#endif
        break;
    case V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200:
        track_carrier(s, &z, &v22bis_constellation[nearest]);
        tune_equalizer(s, &z, &v22bis_constellation[nearest]);
        target = &z;
span_log(&s->logging, SPAN_LOG_FLOW, "S11 0x%02x\n", nearest);
        if (++s->rx.training_count > ms_to_symbols(900))
        {
            s->detected_scrambled_ones_or_zeros_at_1200bps = TRUE;
            s->rx.training = V22BIS_TRAINING_STAGE_NORMAL_OPERATION;
        }
        if (s->bit_rate == 2400  &&  s->rx.training_count == ms_to_symbols(450))
            s->rx.sixteen_way_decisions = TRUE;
        break;
    case V22BIS_TRAINING_STAGE_SCRAMBLED_ONES_AT_2400:
        track_carrier(s, &z, &v22bis_constellation[nearest]);
        tune_equalizer(s, &z, &v22bis_constellation[nearest]);
        s->rx.sixteen_way_decisions = TRUE;
        target = &z;
        break;
    case V22BIS_TRAINING_STAGE_PARKED:
    default:
        /* We failed to train! */
        /* Park here until the carrier drops. */
        target = &z;
        break;
    }
    if (s->rx.qam_report)
        s->rx.qam_report(s->rx.qam_user_data, &z, target, s->rx.constellation_state);
}
/*- End of function --------------------------------------------------------*/

int v22bis_rx(v22bis_state_t *s, const int16_t amp[], int len)
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
           the signal power. */
        /* TODO: get rid of this */
        if (s->caller)
        {
            ii = rx_pulseshaper_2400[6][0].re*s->rx.rrc_filter[s->rx.rrc_filter_step];
            for (j = 1;  j < V22BIS_RX_FILTER_STEPS;  j++)
                ii += rx_pulseshaper_2400[6][j].re*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
        }
        else
        {
            ii = rx_pulseshaper_1200[6][0].re*s->rx.rrc_filter[s->rx.rrc_filter_step];
            for (j = 1;  j < V22BIS_RX_FILTER_STEPS;  j++)
                ii += rx_pulseshaper_1200[6][j].re*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
        }
        power = power_meter_update(&(s->rx.rx_power), (int16_t) (ii/10.0f));
        if (s->rx.signal_present)
        {
            /* Look for power below -48dBm0 to turn the carrier off */
            if (power < s->rx.carrier_off_power)
            {
                v22bis_rx_restart(s, s->bit_rate);
                s->put_bit(s->user_data, PUTBIT_CARRIER_DOWN);
                continue;
            }
        }
        else
        {
            /* Look for power exceeding -43dBm0 to turn the carrier on */
            if (power < s->rx.carrier_on_power)
                continue;
            s->rx.signal_present = TRUE;
            s->put_bit(s->user_data, PUTBIT_CARRIER_UP);
        }
        if (s->rx.training != V22BIS_TRAINING_STAGE_PARKED)
        {
            /* Only spend effort processing this data if the modem is not
               parked, after training failure. */
            z = dds_complexf(&(s->rx.carrier_phase), s->rx.carrier_phase_rate);
            if (s->rx.training == V22BIS_TRAINING_STAGE_SYMBOL_ACQUISITION)
            {
                /* Only AGC during the initial symbol acquisition, and then lock the gain. */
                s->rx.agc_scaling = 0.018f*3.60f/sqrtf(power);
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
                if (s->caller)
                {
                    ii = rx_pulseshaper_2400[step][0].re*s->rx.rrc_filter[s->rx.rrc_filter_step];
                    qq = rx_pulseshaper_2400[step][0].im*s->rx.rrc_filter[s->rx.rrc_filter_step];
                    for (j = 1;  j < V22BIS_RX_FILTER_STEPS;  j++)
                    {
                        ii += rx_pulseshaper_2400[step][j].re*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
                        qq += rx_pulseshaper_2400[step][j].im*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
                    }
                }
                else
                {
                    ii = rx_pulseshaper_1200[step][0].re*s->rx.rrc_filter[s->rx.rrc_filter_step];
                    qq = rx_pulseshaper_1200[step][0].im*s->rx.rrc_filter[s->rx.rrc_filter_step];
                    for (j = 1;  j < V22BIS_RX_FILTER_STEPS;  j++)
                    {
                        ii += rx_pulseshaper_1200[step][j].re*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
                        qq += rx_pulseshaper_1200[step][j].im*s->rx.rrc_filter[j + s->rx.rrc_filter_step];
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

int v22bis_rx_restart(v22bis_state_t *s, int bit_rate)
{
    /* If bit_rate is 2400, the real bit rate is negotiated. If bit_rate
       is 1200, the real bit rate is forced to 1200. */
    s->bit_rate = bit_rate;
    vec_zerof(s->rx.rrc_filter, sizeof(s->rx.rrc_filter)/sizeof(s->rx.rrc_filter[0]));
    s->rx.rrc_filter_step = 0;
    s->rx.scramble_reg = 0;
    s->rx.scrambler_pattern_count = 0;
    s->rx.training = V22BIS_TRAINING_STAGE_SYMBOL_ACQUISITION;
    s->rx.training_count = 0;
    s->rx.signal_present = FALSE;

    s->rx.carrier_phase_rate = dds_phase_ratef((s->caller)  ?  2400.0f  :  1200.0f);
    s->rx.carrier_phase = 0;
    power_meter_init(&(s->rx.rx_power), 5);
    s->rx.carrier_on_power = power_meter_level_dbm0(-43);
    s->rx.carrier_off_power = power_meter_level_dbm0(-48);
    s->rx.agc_scaling = 0.0005f*0.025f;

    s->rx.constellation_state = 0;
    s->rx.sixteen_way_decisions = FALSE;

    equalizer_reset(s);

    s->detected_unscrambled_ones = 0;
    s->detected_unscrambled_zeros = 0;
    s->rx.gardner_integrate = 0;
    s->rx.gardner_step = 256;
    s->rx.baud_phase = 0;
    s->rx.carrier_track_i = 8000.0f;
    s->rx.carrier_track_p = 8000000.0f;
    return 0;
}
/*- End of function --------------------------------------------------------*/

void v22bis_set_qam_report_handler(v22bis_state_t *s, qam_report_handler_t handler, void *user_data)
{
    s->rx.qam_report = handler;
    s->rx.qam_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
