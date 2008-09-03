/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v29rx.c - ITU V.29 modem receive part
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Steve Underwood
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
 * $Id: v29rx.c,v 1.124 2008/07/17 19:12:27 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif

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

#include "v29tx_constellation_maps.h"
#if defined(SPANDSP_USE_FIXED_POINT)
#include "v29rx_fixed_rrc.h"
#else
#include "v29rx_floating_rrc.h"
#endif

#define CARRIER_NOMINAL_FREQ        1700.0f
#define BAUD_RATE                   2400
#define EQUALIZER_DELTA             0.21f

/* Segments of the training sequence */
#define V29_TRAINING_SEG_2_LEN      128
#define V29_TRAINING_SEG_3_LEN      384
#define V29_TRAINING_SEG_4_LEN      48

enum
{
    TRAINING_STAGE_NORMAL_OPERATION = 0,
    TRAINING_STAGE_SYMBOL_ACQUISITION,
    TRAINING_STAGE_LOG_PHASE,
    TRAINING_STAGE_WAIT_FOR_CDCD,
    TRAINING_STAGE_TRAIN_ON_CDCD,
    TRAINING_STAGE_TRAIN_ON_CDCD_AND_TEST,
    TRAINING_STAGE_TEST_ONES,
    TRAINING_STAGE_PARKED
};

static const uint8_t space_map_9600[20][20] =
{
    {13, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11},
    {13, 13, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11},
    {13, 13, 13, 13, 13, 13, 13,  4,  4,  4,  4,  4,  4, 11, 11, 11, 11, 11, 11, 11},
    {13, 13, 13, 13, 13, 13, 13,  4,  4,  4,  4,  4,  4, 11, 11, 11, 11, 11, 11, 11},
    {13, 13, 13, 13, 13, 13, 13,  4,  4,  4,  4,  4,  4, 11, 11, 11, 11, 11, 11, 11},
    {13, 13, 13, 13, 13, 13, 13,  5,  4,  4,  4,  4,  3, 11, 11, 11, 11, 11, 11, 11},
    {14, 13, 13, 13, 13, 13,  5,  5,  5,  5,  3,  3,  3,  3, 11, 11, 11, 11, 11, 10},
    {14, 14,  6,  6,  6,  5,  5,  5,  5,  5,  3,  3,  3,  3,  3,  2,  2,  2, 10, 10},
    {14, 14,  6,  6,  6,  6,  5,  5,  5,  5,  3,  3,  3,  3,  2,  2,  2,  2, 10, 10},
    {14, 14,  6,  6,  6,  6,  5,  5,  5,  5,  3,  3,  3,  3,  2,  2,  2,  2, 10, 10},
    {14, 14,  6,  6,  6,  6,  7,  7,  7,  7,  1,  1,  1,  1,  2,  2,  2,  2, 10, 10},
    {14, 14,  6,  6,  6,  6,  7,  7,  7,  7,  1,  1,  1,  1,  2,  2,  2,  2, 10, 10},
    {14, 14,  6,  6,  6,  7,  7,  7,  7,  7,  1,  1,  1,  1,  1,  2,  2,  2, 10, 10},
    {14, 15, 15, 15, 15, 15,  7,  7,  7,  7,  1,  1,  1,  1,  9,  9,  9,  9,  9, 10},
    {15, 15, 15, 15, 15, 15, 15,  7,  0,  0,  0,  0,  1,  9,  9,  9,  9,  9,  9,  9},
    {15, 15, 15, 15, 15, 15, 15,  0,  0,  0,  0,  0,  0,  9,  9,  9,  9,  9,  9,  9},
    {15, 15, 15, 15, 15, 15, 15,  0,  0,  0,  0,  0,  0,  9,  9,  9,  9,  9,  9,  9},
    {15, 15, 15, 15, 15, 15, 15,  0,  0,  0,  0,  0,  0,  9,  9,  9,  9,  9,  9,  9},
    {15, 15, 15, 15, 15, 15, 15,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9},
    {15, 15, 15, 15, 15, 15,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9}
};

/* Coefficients for the band edge symbol timing synchroniser (alpha = 0.99) */
#define SYNC_LOW_BAND_EDGE_COEFF_0       1.829281f    /* 2*alpha*cos(low_edge) */
#define SYNC_LOW_BAND_EDGE_COEFF_1      -0.980100f    /* -alpha^2 */
#define SYNC_HIGH_BAND_EDGE_COEFF_0     -1.285907f    /* 2*alpha*cos(high_edge) */
#define SYNC_HIGH_BAND_EDGE_COEFF_1     -0.980100f    /* -alpha^2 */
#define SYNC_CROSS_CORR_COEFF_A         -0.932131f    /* -alpha^2*sin(freq_diff) */
#define SYNC_CROSS_CORR_COEFF_B          0.752802f    /* alpha*sin(high_edge) */
#define SYNC_CROSS_CORR_COEFF_C         -0.378857f    /* -alpha*sin(low_edge) */

float v29_rx_carrier_frequency(v29_rx_state_t *s)
{
    return dds_frequencyf(s->carrier_phase_rate);
}
/*- End of function --------------------------------------------------------*/

float v29_rx_symbol_timing_correction(v29_rx_state_t *s)
{
    return (float) s->total_baud_timing_correction/((float) RX_PULSESHAPER_COEFF_SETS*10.0f/3.0f);
}
/*- End of function --------------------------------------------------------*/

void v29_rx_signal_cutoff(v29_rx_state_t *s, float cutoff)
{
    /* The 0.4 factor allows for the gain of the DC blocker */
    s->carrier_on_power = (int32_t) (power_meter_level_dbm0(cutoff + 2.5f)*0.4f);
    s->carrier_off_power = (int32_t) (power_meter_level_dbm0(cutoff - 2.5f)*0.4f);
}
/*- End of function --------------------------------------------------------*/

float v29_rx_signal_power(v29_rx_state_t *s)
{
    return power_meter_current_dbm0(&s->power);
}
/*- End of function --------------------------------------------------------*/

int v29_rx_equalizer_state(v29_rx_state_t *s, complexf_t **coeffs)
{
    *coeffs = s->eq_coeff;
    return V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN;
}
/*- End of function --------------------------------------------------------*/

static void equalizer_save(v29_rx_state_t *s)
{
    cvec_copyf(s->eq_coeff_save, s->eq_coeff, V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN);
}
/*- End of function --------------------------------------------------------*/

static void report_status_change(v29_rx_state_t *s, int status)
{
    if (s->status_handler)
        s->status_handler(s->status_user_data, status);
    else if (s->put_bit)
        s->put_bit(s->put_bit_user_data, status);
}
/*- End of function --------------------------------------------------------*/

static void equalizer_restore(v29_rx_state_t *s)
{
    cvec_copyf(s->eq_coeff, s->eq_coeff_save, V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN);
    cvec_zerof(s->eq_buf, V29_EQUALIZER_MASK);

    s->eq_put_step = RX_PULSESHAPER_COEFF_SETS*10/(3*2) - 1;
    s->eq_step = 0;
    s->eq_delta = EQUALIZER_DELTA/(V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN);
}
/*- End of function --------------------------------------------------------*/

static void equalizer_reset(v29_rx_state_t *s)
{
    /* Start with an equalizer based on everything being perfect */
    cvec_zerof(s->eq_coeff, V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN);
    s->eq_coeff[V29_EQUALIZER_PRE_LEN] = complex_setf(3.0f, 0.0f);
    cvec_zerof(s->eq_buf, V29_EQUALIZER_MASK);

    s->eq_put_step = RX_PULSESHAPER_COEFF_SETS*10/(3*2) - 1;
    s->eq_step = 0;
    s->eq_delta = EQUALIZER_DELTA/(V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN);
}
/*- End of function --------------------------------------------------------*/

static __inline__ complexf_t equalizer_get(v29_rx_state_t *s)
{
    int i;
    int p;
    complexf_t z;
    complexf_t z1;

    /* Get the next equalized value. */
    z = complex_setf(0.0f, 0.0f);
    p = s->eq_step - 1;
    for (i = 0;  i < V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN;  i++)
    {
        p = (p - 1) & V29_EQUALIZER_MASK;
        z1 = complex_mulf(&s->eq_coeff[i], &s->eq_buf[p]);
        z = complex_addf(&z, &z1);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTy)
static void tune_equalizer(v29_rx_state_t *s, const complexi16_t *z, const complexi16_t *target)
#else
static void tune_equalizer(v29_rx_state_t *s, const complexf_t *z, const complexf_t *target)
#endif
{
    int i;
    int p;
    complexf_t ez;
    complexf_t z1;

    /* Find the x and y mismatch from the exact constellation position. */
    ez = complex_subf(target, z);
    ez.re *= s->eq_delta;
    ez.im *= s->eq_delta;

    p = s->eq_step - 1;
    for (i = 0;  i < V29_EQUALIZER_PRE_LEN + 1 + V29_EQUALIZER_POST_LEN;  i++)
    {
        p = (p - 1) & V29_EQUALIZER_MASK;
        z1 = complex_conjf(&s->eq_buf[p]);
        z1 = complex_mulf(&ez, &z1);
        s->eq_coeff[i] = complex_addf(&s->eq_coeff[i], &z1);
        /* Leak a little to tame uncontrolled wandering */
        s->eq_coeff[i].re *= 0.9999f;
        s->eq_coeff[i].im *= 0.9999f;
    }
}
/*- End of function --------------------------------------------------------*/

static int scrambled_training_bit(v29_rx_state_t *s)
{
    int bit;

    /* Segment 3 of the training sequence - the scrambled CDCD part. */
    /* Apply the 1 + x^-6 + x^-7 scrambler */
    bit = s->training_scramble_reg & 1;
    s->training_scramble_reg >>= 1;
    if (bit ^ (s->training_scramble_reg & 1))
        s->training_scramble_reg |= 0x40;
    return bit;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTy)
static __inline__ int find_quadrant(const complexi16_t *z)
#else
static __inline__ int find_quadrant(const complexf_t *z)
#endif
{
    int b1;
    int b2;

    /* Split the space along the two diagonals. */
    b1 = (z->im > z->re);
    b2 = (z->im < -z->re);
    return (b2 << 1) | (b1 ^ b2);
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTy)
static __inline__ void track_carrier(v29_rx_state_t *s, const complexi16_t *z, const complexi16_t *target)
#else
static __inline__ void track_carrier(v29_rx_state_t *s, const complexf_t *z, const complexf_t *target)
#endif
{
    float error;

    /* The initial coarse carrier frequency and phase estimation should have
       got us in the right ballpark. Now we need to fine tune fairly quickly,
       to get the receovered carrier more precisely on target. Then we need to
       fine tune in a more damped way to keep us on target. The goal is to have
       things running really well by the time the training is complete. 
       We assume the frequency of the oscillators at the two ends drift only
       very slowly. The PSTN has rather limited doppler problems. :-) Any
       remaining FDM in the network should also drift slowly. */
    /* For small errors the imaginary part of the difference between the actual and the target
       positions is proportional to the phase error, for any particular target. However, the
       different amplitudes of the various target positions scale things. This isn't all bad,
       as the angular error for the larger amplitude constellation points is probably
       a more reliable indicator, and we are weighting it as such. */
    error = z->im*target->re - z->re*target->im;

    /* Use a proportional-integral approach to tracking the carrier. The PI
       parameters are coarser at first, until we get precisely on target. Then,
       the filter will be damped more to keep us on target. */
    s->carrier_phase_rate += (int32_t) (s->carrier_track_i*error);
    s->carrier_phase += (int32_t) (s->carrier_track_p*error);
    //span_log(&s->logging, SPAN_LOG_FLOW, "Im = %15.5f   f = %15.5f\n", error, dds_frequencyf(s->carrier_phase_rate));
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_bit(v29_rx_state_t *s, int bit)
{
    int out_bit;

    bit &= 1;

    /* Descramble the bit */
    out_bit = (bit ^ (s->scramble_reg >> 17) ^ (s->scramble_reg >> 22)) & 1;
    s->scramble_reg = (s->scramble_reg << 1) | bit;

    /* We need to strip the last part of the training - the test period of all 1s -
       before we let data go to the application. */
    if (s->training_stage == TRAINING_STAGE_NORMAL_OPERATION)
    {
        s->put_bit(s->put_bit_user_data, out_bit);
    }
    else
    {
        /* The bits during the final stage of training should be all ones. However,
           buggy modems mean you cannot rely on this. Therefore we don't bother
           testing for ones, but just rely on a constellation mismatch measurement. */
    }
}
/*- End of function --------------------------------------------------------*/

static void decode_baud(v29_rx_state_t *s, complexf_t *z)
{
    static const uint8_t phase_steps_9600[8] =
    {
        4, 0, 2, 6, 7, 3, 1, 5
    };
    static const uint8_t phase_steps_4800[4] =
    {
        0, 2, 3, 1
    };
    int nearest;
    int raw_bits;
    int i;
    int re;
    int im;

    switch (s->bit_rate)
    {
    case 9600:
    default:
        re = (int) ((z->re + 5.0f)*2.0f);
        if (re > 19)
            re = 19;
        else if (re < 0)
            re = 0;
        im = (int) ((z->im + 5.0f)*2.0f);
        if (im > 19)
            im = 19;
        else if (im < 0)
            im = 0;
        nearest = space_map_9600[re][im];
        /* Deal with the amplitude bit */
        put_bit(s, nearest >> 3);
        raw_bits = phase_steps_9600[(nearest - s->constellation_state) & 7];
        for (i = 0;  i < 3;  i++)
        {
            put_bit(s, raw_bits);
            raw_bits >>= 1;
        }
        break;
    case 7200:
        /* We can reuse the space map for 9600, but drop the top bit */
        re = (int) ((z->re + 5.0f)*2.0f);
        if (re > 19)
            re = 19;
        else if (re < 0)
            re = 0;
        im = (int) ((z->im + 5.0f)*2.0f);
        if (im > 19)
            im = 19;
        else if (im < 0)
            im = 0;
        nearest = space_map_9600[re][im] & 7;
        raw_bits = phase_steps_9600[(nearest - s->constellation_state) & 7];
        for (i = 0;  i < 3;  i++)
        {
            put_bit(s, raw_bits);
            raw_bits >>= 1;
        }
        break;
    case 4800:
        nearest = find_quadrant(z) << 1;
        raw_bits = phase_steps_4800[((nearest - s->constellation_state) >> 1) & 3];
        put_bit(s, raw_bits);
        put_bit(s, raw_bits >> 1);
        break;
    }
    track_carrier(s, z, &v29_9600_constellation[nearest]);
    if (--s->eq_skip <= 0)
    {
        /* Once we are in the data the equalization should not need updating.
           However, the line characteristics may slowly drift. We, therefore,
           tune up on the occassional sample, keeping the compute down. */
        s->eq_skip = 10;
        tune_equalizer(s, z, &v29_9600_constellation[nearest]);
    }
    s->constellation_state = nearest;
}
/*- End of function --------------------------------------------------------*/

static void process_half_baud(v29_rx_state_t *s, complexf_t *sample)
{
    static const int cdcd_pos[6] =
    {
        0, 11,
        0,  3,
        0,  2
    };
    complexf_t z;
    complexf_t zz;
#if defined(SPANDSP_USE_FIXED_POINTy)
    const complexi16_t *target;
#else
    const complexf_t *target;
#endif
    float v;
    float p;
    int bit;
    int i;
    int j;
    int32_t angle;
    int32_t ang;

    /* This routine processes every half a baud, as we put things into the equalizer at the T/2 rate.
       This routine adapts the position of the half baud samples, which the caller takes. */

    /* Add a sample to the equalizer's circular buffer, but don't calculate anything
       at this time. */
    s->eq_buf[s->eq_step] = *sample;
    s->eq_step = (s->eq_step + 1) & V29_EQUALIZER_MASK;

    /* On alternate insertions we have a whole baud, and must process it. */
    if ((s->baud_half ^= 1))
        return;

    /* Symbol timing synchronisation */
    /* Cross correlate */
    v = s->symbol_sync_low[1]*s->symbol_sync_high[1]*SYNC_CROSS_CORR_COEFF_A
      + s->symbol_sync_low[0]*s->symbol_sync_high[1]*SYNC_CROSS_CORR_COEFF_B
      + s->symbol_sync_low[1]*s->symbol_sync_high[0]*SYNC_CROSS_CORR_COEFF_C;

    /* Filter away any DC component  */
    p = v - s->symbol_sync_dc_filter[1];
    s->symbol_sync_dc_filter[1] = s->symbol_sync_dc_filter[0];
    s->symbol_sync_dc_filter[0] = v;
    /* A little integration will now filter away much of the noise */
    s->baud_phase -= p;

    if (fabsf(s->baud_phase) > 30.0f)
    {
        if (s->baud_phase > 0.0f)
            i = (s->baud_phase > 1000.0f)  ?  5  :  1;
        else
            i = (s->baud_phase < -1000.0f)  ?  -5  :  -1;

        //printf("v = %10.5f %5d - %f %f %d %d\n", v, i, p, s->baud_phase, s->total_baud_timing_correction);
        s->eq_put_step += i;
        s->total_baud_timing_correction += i;
    }

    z = equalizer_get(s);

    switch (s->training_stage)
    {
    case TRAINING_STAGE_NORMAL_OPERATION:
        /* Normal operation. */
        decode_baud(s, &z);
        target = &v29_9600_constellation[s->constellation_state];
        break;
    case TRAINING_STAGE_SYMBOL_ACQUISITION:
        /* Allow time for symbol synchronisation to settle the symbol timing. */
        target = &z;
        if (++s->training_count >= 60)
        {
            /* Record the current phase angle */
            s->training_stage = TRAINING_STAGE_LOG_PHASE;
            s->angles[0] =
            s->start_angles[0] = arctan2(z.im, z.re);
        }
        break;
    case TRAINING_STAGE_LOG_PHASE:
        /* Record the current alternate phase angle */
        target = &z;
        s->angles[1] =
        s->start_angles[1] = arctan2(z.im, z.re);
        s->training_count = 1;
        s->training_stage = TRAINING_STAGE_WAIT_FOR_CDCD;
        break;
    case TRAINING_STAGE_WAIT_FOR_CDCD:
        target = &z;
        angle = arctan2(z.im, z.re);
        /* Look for the initial ABAB sequence to display a phase reversal, which will
           signal the start of the scrambled CDCD segment */
        ang = angle - s->angles[(s->training_count - 1) & 0xF];
        s->angles[(s->training_count + 1) & 0xF] = angle;
        if ((ang > 0x20000000  ||  ang < -0x20000000)  &&  s->training_count >= 13)
        {
            /* We seem to have a phase reversal */
            /* Slam the carrier frequency into line, based on the total phase drift over the last
               section. Use the shift from the odd bits and the shift from the even bits to get
               better jitter suppression. We need to scale here, or at the maximum specified
               frequency deviation we could overflow, and get a silly answer. */
            /* Step back a few symbols so we don't get ISI distorting things. */
            i = (s->training_count - 8) & ~1;
            /* Avoid the possibility of a divide by zero */
            if (i)
            {
                j = i & 0xF;
                ang = (s->angles[j] - s->start_angles[0])/i
                    + (s->angles[j | 0x1] - s->start_angles[1])/i;
                s->carrier_phase_rate += 3*(ang/20);
            }
            span_log(&s->logging, SPAN_LOG_FLOW, "Coarse carrier frequency %7.2f\n", dds_frequencyf(s->carrier_phase_rate));
            /* Check if the carrier frequency is plausible */
            if (s->carrier_phase_rate < dds_phase_ratef(CARRIER_NOMINAL_FREQ - 20.0f)
                ||
                s->carrier_phase_rate > dds_phase_ratef(CARRIER_NOMINAL_FREQ + 20.0f))
            {
               span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (sequence failed)\n");
               /* Park this modem */
               s->training_stage = TRAINING_STAGE_PARKED;
               report_status_change(s, PUTBIT_TRAINING_FAILED);
               break;
            }
            /* Make a step shift in the phase, to pull it into line. We need to rotate the equalizer
               buffer, as well as the carrier phase, for this to play out nicely. */
            p = angle*2.0f*3.14159f/(65536.0f*65536.0f);
            zz = complex_setf(cosf(p), -sinf(p));
            for (i = 0;  i <= V29_EQUALIZER_MASK;  i++)
                s->eq_buf[i] = complex_mulf(&s->eq_buf[i], &zz);
            s->carrier_phase += angle;
            /* We have just seen the first bit of the scrambled sequence, so skip it. */
            bit = scrambled_training_bit(s);
            s->training_count = 1;
            s->training_stage = TRAINING_STAGE_TRAIN_ON_CDCD;
            report_status_change(s, PUTBIT_TRAINING_IN_PROGRESS);
            break;
        }
        if (++s->training_count > V29_TRAINING_SEG_2_LEN)
        {
            /* This is bogus. There are not this many bauds in this section
               of a real training sequence. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (sequence failed)\n");
            /* Park this modem */
            s->training_stage = TRAINING_STAGE_PARKED;
            report_status_change(s, PUTBIT_TRAINING_FAILED);
        }
        break;
    case TRAINING_STAGE_TRAIN_ON_CDCD:
        /* Train on the scrambled CDCD section. */
        bit = scrambled_training_bit(s);
        //span_log(&s->logging, SPAN_LOG_FLOW, "%5d %15.5f, %15.5f     %15.5f, %15.5f\n", s->training_count, z.re, z.im, v29_9600_constellation[cdcd_pos[s->training_cd + bit]].re, v29_9600_constellation[cdcd_pos[s->training_cd + bit]].im);
        s->constellation_state = cdcd_pos[s->training_cd + bit];
        target = &v29_9600_constellation[s->constellation_state];
        track_carrier(s, &z, target);
        tune_equalizer(s, &z, target);
        if (++s->training_count >= V29_TRAINING_SEG_3_LEN - 48)
        {
            s->training_stage = TRAINING_STAGE_TRAIN_ON_CDCD_AND_TEST;
            s->training_error = 0.0f;
            s->carrier_track_i = 200.0f;
            s->carrier_track_p = 1000000.0f;
        }
        break;
    case TRAINING_STAGE_TRAIN_ON_CDCD_AND_TEST:
        /* Continue training on the scrambled CDCD section, but measure the quality of training too. */
        bit = scrambled_training_bit(s);
        //span_log(&s->logging, SPAN_LOG_FLOW, "%5d %15.5f, %15.5f     %15.5f, %15.5f\n", s->training_count, z.re, z.im, v29_9600_constellation[cdcd_pos[s->training_cd + bit]].re, v29_9600_constellation[cdcd_pos[s->training_cd + bit]].im);
        s->constellation_state = cdcd_pos[s->training_cd + bit];
        target = &v29_9600_constellation[s->constellation_state];
        track_carrier(s, &z, target);
        tune_equalizer(s, &z, target);
        /* Measure the training error */
        zz = complex_subf(&z, target);
        s->training_error += powerf(&zz);
        if (++s->training_count >= V29_TRAINING_SEG_3_LEN)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Constellation mismatch %f\n", s->training_error);
            if (s->training_error < 100.0f)
            {
                s->training_count = 0;
                s->training_error = 0.0f;
                s->constellation_state = 0;
                s->training_stage = TRAINING_STAGE_TEST_ONES;
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (convergence failed)\n");
                /* Park this modem */
                s->training_stage = TRAINING_STAGE_PARKED;
                report_status_change(s, PUTBIT_TRAINING_FAILED);
            }
        }
        break;
    case TRAINING_STAGE_TEST_ONES:
        /* We are in the test phase, where we check that we can receive reliably.
           We should get a run of 1's, 48 symbols (192 bits at 9600bps) long. */
        //span_log(&s->logging, SPAN_LOG_FLOW, "%5d %15.5f, %15.5f\n", s->training_count, z.re, z.im);
        decode_baud(s, &z);
        target = &v29_9600_constellation[s->constellation_state];
        /* Measure the training error */
        zz = complex_subf(&z, target);
        s->training_error += powerf(&zz);
        if (++s->training_count >= V29_TRAINING_SEG_4_LEN)
        {
            if (s->training_error < 50.0f)
            {
                /* We are up and running */
                span_log(&s->logging, SPAN_LOG_FLOW, "Training succeeded (constellation mismatch %f)\n", s->training_error);
                report_status_change(s, PUTBIT_TRAINING_SUCCEEDED);
                /* Apply some lag to the carrier off condition, to ensure the last few bits get pushed through
                   the processing. */
                s->signal_present = 60;
                s->training_stage = TRAINING_STAGE_NORMAL_OPERATION;
                equalizer_save(s);
                s->carrier_phase_rate_save = s->carrier_phase_rate;
                s->agc_scaling_save = s->agc_scaling;
            }
            else
            {
                /* Training has failed */
                span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (constellation mismatch %f)\n", s->training_error);
                /* Park this modem */
                report_status_change(s, PUTBIT_TRAINING_FAILED);
                s->training_stage = TRAINING_STAGE_PARKED;
            }
        }
        break;
    case TRAINING_STAGE_PARKED:
    default:
        /* We failed to train! */
        /* Park here until the carrier drops. */
        target = &z;
        break;
    }
    if (s->qam_report)
        s->qam_report(s->qam_user_data, &z, target, s->constellation_state);
}
/*- End of function --------------------------------------------------------*/

int v29_rx(v29_rx_state_t *s, const int16_t amp[], int len)
{
    int i;
    int j;
    int step;
    int16_t x;
    int32_t diff;
    complexf_t z;
    complexf_t zz;
    complexf_t sample;
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi_t zi;
#endif
    int32_t power;
    float v;

    for (i = 0;  i < len;  i++)
    {
        s->rrc_filter[s->rrc_filter_step] =
        s->rrc_filter[s->rrc_filter_step + V29_RX_FILTER_STEPS] = amp[i];
        if (++s->rrc_filter_step >= V29_RX_FILTER_STEPS)
            s->rrc_filter_step = 0;

        /* There should be no DC in the signal, but sometimes there is.
           We need to measure the power with the DC blocked, but not using
           a slow to respond DC blocker. Use the most elementary HPF. */
        x = amp[i] >> 1;
        diff = x - s->last_sample;
        power = power_meter_update(&(s->power), diff);
#if defined(IAXMODEM_STUFF)
        /* Quick power drop fudge */
        diff = abs(diff);
        if (10*diff < s->high_sample)
        {
            if (++s->low_samples > 120)
            {
                power_meter_init(&(s->power), 4);
                s->high_sample = 0;
                s->low_samples = 0;
            }
        }
        else
        { 
            s->low_samples = 0;
            if (diff > s->high_sample)
               s->high_sample = diff;
        }
#endif
        s->last_sample = x;
        if (s->signal_present)
        {
            /* Look for power below turn-off threshold to turn the carrier off */
#if defined(IAXMODEM_STUFF)
            if (s->carrier_drop_pending  ||  power < s->carrier_off_power)
#else
            if (power < s->carrier_off_power)
#endif
            {
                if (--s->signal_present <= 0)
                {
                    /* Count down a short delay, to ensure we push the last
                       few bits through the filters before stopping. */
                    v29_rx_restart(s, s->bit_rate, FALSE);
                    report_status_change(s, PUTBIT_CARRIER_DOWN);
                    continue;
                }
#if defined(IAXMODEM_STUFF)
                /* Carrier has dropped, but the put_bit is
                   pending the signal_present delay. */
                s->carrier_drop_pending = TRUE;
#endif
            }
        }
        else
        {
            /* Look for power exceeding turn-on threshold to turn the carrier on */
            if (power < s->carrier_on_power)
                continue;
            s->signal_present = 1;
#if defined(IAXMODEM_STUFF)
            s->carrier_drop_pending = FALSE;
#endif
            report_status_change(s, PUTBIT_CARRIER_UP);
        }
        if (s->training_stage == TRAINING_STAGE_PARKED)
            continue;
        /* Only spend effort processing this data if the modem is not
           parked, after training failure. */
        s->eq_put_step -= RX_PULSESHAPER_COEFF_SETS;
        step = -s->eq_put_step;
        if (step > RX_PULSESHAPER_COEFF_SETS - 1)
            step = RX_PULSESHAPER_COEFF_SETS - 1;
        if (step < 0)
            step += RX_PULSESHAPER_COEFF_SETS;
#if defined(SPANDSP_USE_FIXED_POINT)
        zi.re = (int32_t) rx_pulseshaper[step][0].re*(int32_t) s->rrc_filter[s->rrc_filter_step];
        for (j = 1;  j < V29_RX_FILTER_STEPS;  j++)
            zi.re += (int32_t) rx_pulseshaper[step][j].re*(int32_t) s->rrc_filter[j + s->rrc_filter_step];
        sample.re = zi.re*s->agc_scaling;
#else
        zz.re = rx_pulseshaper[step][0].re*s->rrc_filter[s->rrc_filter_step];
        for (j = 1;  j < V29_RX_FILTER_STEPS;  j++)
            zz.re += rx_pulseshaper[step][j].re*s->rrc_filter[j + s->rrc_filter_step];
        sample.re = zz.re*s->agc_scaling;
#endif

        /* Symbol timing synchronisation band edge filters */
        /* Low Nyquist band edge filter */
        v = s->symbol_sync_low[0]*SYNC_LOW_BAND_EDGE_COEFF_0 + s->symbol_sync_low[1]*SYNC_LOW_BAND_EDGE_COEFF_1 + sample.re;
        s->symbol_sync_low[1] = s->symbol_sync_low[0];
        s->symbol_sync_low[0] = v;
        /* High Nyquist band edge filter */
        v = s->symbol_sync_high[0]*SYNC_HIGH_BAND_EDGE_COEFF_0 + s->symbol_sync_high[1]*SYNC_HIGH_BAND_EDGE_COEFF_1 + sample.re;
        s->symbol_sync_high[1] = s->symbol_sync_high[0];
        s->symbol_sync_high[0] = v;

        /* Put things into the equalization buffer at T/2 rate. The symbol synchronisation
           will fiddle the step to align this with the symbols. */
        if (s->eq_put_step <= 0)
        {
            if (s->training_stage == TRAINING_STAGE_SYMBOL_ACQUISITION)
            {
                /* Only AGC during the initial training */
                s->agc_scaling = (1.0f/RX_PULSESHAPER_GAIN)*5.0f*0.25f/sqrtf(power);
            }
            /* Pulse shape while still at the carrier frequency, using a quadrature
               pair of filters. This results in a properly bandpass filtered complex
               signal, which can be brought directly to baseband by complex mixing.
               No further filtering, to remove mixer harmonics, is needed. */
            step = -s->eq_put_step;
            if (step > RX_PULSESHAPER_COEFF_SETS - 1)
                step = RX_PULSESHAPER_COEFF_SETS - 1;
#if defined(SPANDSP_USE_FIXED_POINT)
            zi.im = (int32_t) rx_pulseshaper[step][0].im*(int32_t) s->rrc_filter[s->rrc_filter_step];
            for (j = 1;  j < V29_RX_FILTER_STEPS;  j++)
                zi.im += (int32_t) rx_pulseshaper[step][j].im*(int32_t) s->rrc_filter[j + s->rrc_filter_step];
            sample.im = zi.im*s->agc_scaling;
#else
            zz.im = rx_pulseshaper[step][0].im*s->rrc_filter[s->rrc_filter_step];
            for (j = 1;  j < V29_RX_FILTER_STEPS;  j++)
                zz.im += rx_pulseshaper[step][j].im*s->rrc_filter[j + s->rrc_filter_step];
            sample.im = zz.im*s->agc_scaling;
#endif
            s->eq_put_step += RX_PULSESHAPER_COEFF_SETS*10/(3*2);
            /* Shift to baseband - since this is done in a full complex form, the
               result is clean, and requires no further filtering, apart from the
               equalizer. */
            z = dds_lookup_complexf(s->carrier_phase);
            zz.re = sample.re*z.re - sample.im*z.im;
            zz.im = -sample.re*z.im - sample.im*z.re;
            process_half_baud(s, &zz);
        }
        dds_advancef(&(s->carrier_phase), s->carrier_phase_rate);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

void v29_rx_set_put_bit(v29_rx_state_t *s, put_bit_func_t put_bit, void *user_data)
{
    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void v29_rx_set_modem_status_handler(v29_rx_state_t *s, modem_tx_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

int v29_rx_restart(v29_rx_state_t *s, int bit_rate, int old_train)
{
    switch (bit_rate)
    {
    case 9600:
        s->training_cd = 0;
        break;
    case 7200:
        s->training_cd = 2;
        break;
    case 4800:
        s->training_cd = 4;
        break;
    default:
        return -1;
    }
    s->bit_rate = bit_rate;

#if defined(SPANDSP_USE_FIXED_POINT)
    memset(s->rrc_filter, 0, sizeof(s->rrc_filter));
#else
    vec_zerof(s->rrc_filter, sizeof(s->rrc_filter)/sizeof(s->rrc_filter[0]));
#endif
    s->rrc_filter_step = 0;

    s->scramble_reg = 0;
    s->training_scramble_reg = 0x2A;
    s->training_stage = TRAINING_STAGE_SYMBOL_ACQUISITION;
    s->training_count = 0;
    s->signal_present = 0;
#if defined(IAXMODEM_STUFF)
    s->high_sample = 0;
    s->low_samples = 0;
    s->carrier_drop_pending = FALSE;
#endif
    s->old_train = old_train;

    s->carrier_phase = 0;
    s->carrier_track_i = 8000.0f;
    s->carrier_track_p = 8000000.0f;

    power_meter_init(&(s->power), 4);

    s->constellation_state = 0;

    if (s->old_train)
    {
        s->carrier_phase_rate = s->carrier_phase_rate_save;
        s->agc_scaling = s->agc_scaling_save;
        equalizer_restore(s);
    }
    else
    {
        s->carrier_phase_rate = dds_phase_ratef(CARRIER_NOMINAL_FREQ);
        s->agc_scaling = 0.0017f/RX_PULSESHAPER_GAIN;
        equalizer_reset(s);
    }
    s->eq_skip = 0;
    s->last_sample = 0;

    /* Initialise the working data for symbol timing synchronisation */
    s->symbol_sync_low[0] = 0.0f;
    s->symbol_sync_low[1] = 0.0f;
    s->symbol_sync_high[0] = 0.0f;
    s->symbol_sync_high[1] = 0.0f;
    s->symbol_sync_dc_filter[0] = 0.0f;
    s->symbol_sync_dc_filter[1] = 0.0f;
    s->baud_phase = 0.0f;
    s->baud_half = 0;

    s->total_baud_timing_correction = 0;

    return 0;
}
/*- End of function --------------------------------------------------------*/

v29_rx_state_t *v29_rx_init(v29_rx_state_t *s, int rate, put_bit_func_t put_bit, void *user_data)
{
    if (s == NULL)
    {
        if ((s = (v29_rx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }

    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.29 RX");
    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
    /* The V.29 spec says the thresholds should be -31dBm and -26dBm, but that makes little
       sense. V.17 uses -48dBm and -43dBm, and there seems no good reason to cut off at a
       higher level (though at 9600bps and 7200bps, TCM should put V.17 sensitivity several
       dB ahead of V.29). */
    /* The thresholds should be on at -26dBm0 and off at -31dBm0 */
    v29_rx_signal_cutoff(s, -28.5f);

    v29_rx_restart(s, rate, FALSE);
    return s;
}
/*- End of function --------------------------------------------------------*/

int v29_rx_free(v29_rx_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

void v29_rx_set_qam_report_handler(v29_rx_state_t *s, qam_report_handler_t handler, void *user_data)
{
    s->qam_report = handler;
    s->qam_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
