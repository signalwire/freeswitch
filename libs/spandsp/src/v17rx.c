/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v17rx.c - ITU V.17 modem receive part
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004, 2005, 2006, 2007 Steve Underwood
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
 * $Id: v17rx.c,v 1.112 2008/07/17 19:12:27 steveu Exp $
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
#include "spandsp/v17tx.h"
#include "spandsp/v17rx.h"

#include "v17tx_constellation_maps.h"
#include "v17rx_constellation_maps.h"
#if defined(SPANDSP_USE_FIXED_POINT)
#include "v17rx_fixed_rrc.h"
#else
#include "v17rx_floating_rrc.h"
#endif

#define CARRIER_NOMINAL_FREQ            1800.0f
#define BAUD_RATE                       2400
#define EQUALIZER_DELTA                 0.21f
#define EQUALIZER_SLOW_ADAPT_RATIO      0.1f

/* Segments of the training sequence */
#define V17_TRAINING_SEG_1_LEN          256
#define V17_TRAINING_SEG_2_LEN          2976
#define V17_TRAINING_SHORT_SEG_2_LEN    38
#define V17_TRAINING_SEG_3_LEN          64
#define V17_TRAINING_SEG_4A_LEN         15
#define V17_TRAINING_SEG_4_LEN          48

#define V17_BRIDGE_WORD                 0x8880

enum
{
    TRAINING_STAGE_NORMAL_OPERATION = 0,
    TRAINING_STAGE_SYMBOL_ACQUISITION,
    TRAINING_STAGE_LOG_PHASE,
    TRAINING_STAGE_SHORT_WAIT_FOR_CDBA,
    TRAINING_STAGE_WAIT_FOR_CDBA,
    TRAINING_STAGE_COARSE_TRAIN_ON_CDBA,
    TRAINING_STAGE_FINE_TRAIN_ON_CDBA,
    TRAINING_STAGE_SHORT_TRAIN_ON_CDBA_AND_TEST,
    TRAINING_STAGE_TRAIN_ON_CDBA_AND_TEST,
    TRAINING_STAGE_BRIDGE,
    TRAINING_STAGE_TCM_WINDUP,
    TRAINING_STAGE_TEST_ONES,
    TRAINING_STAGE_PARKED
};

/* Coefficients for the band edge symbol timing synchroniser (alpha = 0.99) */
#define SYNC_LOW_BAND_EDGE_COEFF_0       1.764193f    /* 2*alpha*cos(low_edge) */
#define SYNC_LOW_BAND_EDGE_COEFF_1      -0.980100f    /* -alpha^2 */
#define SYNC_HIGH_BAND_EDGE_COEFF_0     -1.400072f    /* 2*alpha*cos(high_edge) */
#define SYNC_HIGH_BAND_EDGE_COEFF_1     -0.980100f    /* -alpha^2 */
#define SYNC_CROSS_CORR_COEFF_A         -0.932131f    /* -alpha^2*sin(freq_diff) */
#define SYNC_CROSS_CORR_COEFF_B          0.700036f    /* alpha*sin(high_edge) */
#define SYNC_CROSS_CORR_COEFF_C         -0.449451f    /* -alpha*sin(low_edge) */

float v17_rx_carrier_frequency(v17_rx_state_t *s)
{
    return dds_frequencyf(s->carrier_phase_rate);
}
/*- End of function --------------------------------------------------------*/

float v17_rx_symbol_timing_correction(v17_rx_state_t *s)
{
    return (float) s->total_baud_timing_correction/((float) RX_PULSESHAPER_COEFF_SETS*10.0f/3.0f);
}
/*- End of function --------------------------------------------------------*/

float v17_rx_signal_power(v17_rx_state_t *s)
{
    return power_meter_current_dbm0(&s->power);
}
/*- End of function --------------------------------------------------------*/

void v17_rx_signal_cutoff(v17_rx_state_t *s, float cutoff)
{
    /* The 0.4 factor allows for the gain of the DC blocker */
    s->carrier_on_power = (int32_t) (power_meter_level_dbm0(cutoff + 2.5f)*0.4f);
    s->carrier_off_power = (int32_t) (power_meter_level_dbm0(cutoff - 2.5f)*0.4f);
}
/*- End of function --------------------------------------------------------*/

int v17_rx_equalizer_state(v17_rx_state_t *s, complexf_t **coeffs)
{
    *coeffs = s->eq_coeff;
    return V17_EQUALIZER_PRE_LEN + 1 + V17_EQUALIZER_POST_LEN;
}
/*- End of function --------------------------------------------------------*/

static void report_status_change(v17_rx_state_t *s, int status)
{
    if (s->status_handler)
        s->status_handler(s->status_user_data, status);
    else if (s->put_bit)
        s->put_bit(s->put_bit_user_data, status);
}
/*- End of function --------------------------------------------------------*/

static void equalizer_save(v17_rx_state_t *s)
{
    cvec_copyf(s->eq_coeff_save, s->eq_coeff, V17_EQUALIZER_PRE_LEN + 1 + V17_EQUALIZER_POST_LEN);
}
/*- End of function --------------------------------------------------------*/

static void equalizer_restore(v17_rx_state_t *s)
{
    cvec_copyf(s->eq_coeff, s->eq_coeff_save, V17_EQUALIZER_PRE_LEN + 1 + V17_EQUALIZER_POST_LEN);
    cvec_zerof(s->eq_buf, V17_EQUALIZER_MASK);

    s->eq_put_step = RX_PULSESHAPER_COEFF_SETS*10/(3*2) - 1;
    s->eq_step = 0;
    s->eq_delta = EQUALIZER_SLOW_ADAPT_RATIO*EQUALIZER_DELTA/(V17_EQUALIZER_PRE_LEN + 1 + V17_EQUALIZER_POST_LEN);
}
/*- End of function --------------------------------------------------------*/

static void equalizer_reset(v17_rx_state_t *s)
{
    /* Start with an equalizer based on everything being perfect */
    cvec_zerof(s->eq_coeff, V17_EQUALIZER_PRE_LEN + 1 + V17_EQUALIZER_POST_LEN);
    s->eq_coeff[V17_EQUALIZER_PRE_LEN] = complex_setf(3.0f, 0.0f);
    cvec_zerof(s->eq_buf, V17_EQUALIZER_MASK);

    s->eq_put_step = RX_PULSESHAPER_COEFF_SETS*10/(3*2) - 1;
    s->eq_step = 0;
    s->eq_delta = EQUALIZER_DELTA/(V17_EQUALIZER_PRE_LEN + 1 + V17_EQUALIZER_POST_LEN);
}
/*- End of function --------------------------------------------------------*/

static __inline__ complexf_t equalizer_get(v17_rx_state_t *s)
{
    int i;
    int p;
    complexf_t z;
    complexf_t z1;

    /* Get the next equalized value. */
    z = complex_setf(0.0f, 0.0f);
    p = s->eq_step - 1;
    for (i = 0;  i < V17_EQUALIZER_PRE_LEN + 1 + V17_EQUALIZER_POST_LEN;  i++)
    {
        p = (p - 1) & V17_EQUALIZER_MASK;
        z1 = complex_mulf(&s->eq_coeff[i], &s->eq_buf[p]);
        z = complex_addf(&z, &z1);
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

static void tune_equalizer(v17_rx_state_t *s, const complexf_t *z, const complexf_t *target)
{
    int i;
    int p;
    complexf_t ez;
    complexf_t z1;

    /* Find the x and y mismatch from the exact constellation position. */
    ez = complex_subf(target, z);
    //span_log(&s->logging, SPAN_LOG_FLOW, "Equalizer error %f\n", sqrt(ez.re*ez.re + ez.im*ez.im));
    ez.re *= s->eq_delta;
    ez.im *= s->eq_delta;

    p = s->eq_step - 1;
    for (i = 0;  i < V17_EQUALIZER_PRE_LEN + 1 + V17_EQUALIZER_POST_LEN;  i++)
    {
        p = (p - 1) & V17_EQUALIZER_MASK;
        z1 = complex_conjf(&s->eq_buf[p]);
        z1 = complex_mulf(&ez, &z1);
        s->eq_coeff[i] = complex_addf(&s->eq_coeff[i], &z1);
        /* Leak a little to tame uncontrolled wandering */
        s->eq_coeff[i].re *= 0.9999f;
        s->eq_coeff[i].im *= 0.9999f;
    }
}
/*- End of function --------------------------------------------------------*/

static int descramble(v17_rx_state_t *s, int in_bit)
{
    int out_bit;

    out_bit = (in_bit ^ (s->scramble_reg >> 17) ^ (s->scramble_reg >> 22)) & 1;
    s->scramble_reg <<= 1;
    if (s->training_stage > TRAINING_STAGE_NORMAL_OPERATION  &&  s->training_stage < TRAINING_STAGE_TCM_WINDUP)
        s->scramble_reg |= out_bit;
    else
        s->scramble_reg |= (in_bit & 1);
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int find_quadrant(complexf_t *z)
{
    int b1;
    int b2;

    /* Split the space along the two diagonals. */
    b1 = (z->im > z->re);
    b2 = (z->im < -z->re);
    return (b2 << 1) | (b1 ^ b2);
}
/*- End of function --------------------------------------------------------*/

static void track_carrier(v17_rx_state_t *s, const complexf_t *z, const complexf_t *target)
{
    float error;

    /* For small errors the imaginary part of the difference between the actual and the target
       positions is proportional to the phase error, for any particular target. However, the
       different amplitudes of the various target positions scale things. */
    error = z->im*target->re - z->re*target->im;
    
    s->carrier_phase_rate += (int32_t) (s->carrier_track_i*error);
    s->carrier_phase += (int32_t) (s->carrier_track_p*error);
    //span_log(&s->logging, SPAN_LOG_FLOW, "Im = %15.5f   f = %15.5f\n", error, dds_frequencyf(s->carrier_phase_rate));
    //printf("XXX Im = %15.5f   f = %15.5f   %f %f %f %f (%f %f)\n", error, dds_frequencyf(s->carrier_phase_rate), target->re, target->im, z->re, z->im, s->carrier_track_i, s->carrier_track_p);
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_bit(v17_rx_state_t *s, int bit)
{
    int out_bit;

    /* We need to strip the last part of the training - the test period of all 1s -
       before we let data go to the application. */
    if (s->training_stage == TRAINING_STAGE_NORMAL_OPERATION)
    {
        out_bit = descramble(s, bit);
        s->put_bit(s->put_bit_user_data, out_bit);
    }
    else if (s->training_stage == TRAINING_STAGE_TEST_ONES)
    {
        /* The bits during the final stage of training should be all ones. However,
           buggy modems mean you cannot rely on this. Therefore we don't bother
           testing for ones, but just rely on a constellation mismatch measurement. */
        out_bit = descramble(s, bit);
        //span_log(&s->logging, SPAN_LOG_FLOW, "A 1 is really %d\n", out_bit);
    }
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTx)
static __inline__ uint32_t dist_sq(const complexi_t *x, const complexi_t *y)
{
    return (x->re - y->re)*(x->re - y->re) + (x->im - y->im)*(x->im - y->im);
}
/*- End of function --------------------------------------------------------*/
#else
static __inline__ float dist_sq(const complexf_t *x, const complexf_t *y)
{
    return (x->re - y->re)*(x->re - y->re) + (x->im - y->im)*(x->im - y->im);
}
/*- End of function --------------------------------------------------------*/
#endif

static int decode_baud(v17_rx_state_t *s, complexf_t *z)
{
    static const int diff_code[16] =
    {
        0, 3, 2, 1, 1, 0, 3, 2, 2, 1, 0, 3, 3, 2, 1, 0                         
    };
    static const int tcm_paths[8][4] =
    {
        {0, 6, 2, 4},
        {6, 0, 4, 2},
        {2, 4, 0, 6},
        {4, 2, 6, 0},
        {1, 3, 7, 5},
        {5, 7, 3, 1},
        {7, 5, 1, 3},
        {3, 1, 5, 7}
    };
    int nearest;
    int i;
    int j;
    int k;
    int re;
    int im;
    int raw;
    int constellation_state;
#if defined(SPANDSP_USE_FIXED_POINTx)
#define DIST_FACTOR 2048       /* Something less than sqrt(0xFFFFFFFF/10)/10 */
    complexi_t zi;
    uint32_t distances[8];
    uint32_t new_distances[8];
    uint32_t min;
    complexi_t ci;
#else
    float distances[8];
    float new_distances[8];
    float min;
#endif

    re = (int) ((z->re + 9.0f)*2.0f);
    if (re > 35)
        re = 35;
    else if (re < 0)
        re = 0;
    im = (int) ((z->im + 9.0f)*2.0f);
    if (im > 35)
        im = 35;
    else if (im < 0)
        im = 0;

    /* Find a set of 8 candidate constellation positions, that are the closest
       to the target, with different patterns in the last 3 bits. */
#if defined(SPANDSP_USE_FIXED_POINTx)
    min = 0xFFFFFFFF;
    zi = complex_seti(z->re*DIST_FACTOR, z->im*DIST_FACTOR);
#else
    min = 9999999.0f;
#endif
    j = 0;
    for (i = 0;  i < 8;  i++)
    {
        nearest = constel_maps[s->space_map][re][im][i];
#if defined(SPANDSP_USE_FIXED_POINTx)
        ci = complex_seti(s->constellation[nearest].re*DIST_FACTOR,
                          s->constellation[nearest].im*DIST_FACTOR);
        distances[i] = dist_sq(&ci, &zi);
#else
        distances[i] = dist_sq(&s->constellation[nearest], z);
#endif
        if (min > distances[i])
        {
            min = distances[i];
            j = i;
        }
    }
    /* Use the nearest of these soft-decisions as the basis for DFE */
    constellation_state = constel_maps[s->space_map][re][im][j];
    /* Control the equalizer, carrier tracking, etc. based on the non-trellis
       corrected information. The trellis correct stuff comes out a bit late. */
    track_carrier(s, z, &s->constellation[constellation_state]);
    //tune_equalizer(s, z, &s->constellation[constellation_state]);

    /* Now do the trellis decoding */

    /* TODO: change to processing blocks of stored symbols here, instead of processing
             one symbol at a time, to speed up the processing. */

    /* Update the minimum accumulated distance to each of the 8 states */
    if (++s->trellis_ptr >= V17_TRELLIS_STORAGE_DEPTH)
        s->trellis_ptr = 0;
    for (i = 0;  i < 4;  i++)
    {
        min = distances[tcm_paths[i][0]] + s->distances[0];
        k = 0;
        for (j = 1;  j < 4;  j++)
        {
            if (min > distances[tcm_paths[i][j]] + s->distances[j << 1])
            {
                min = distances[tcm_paths[i][j]] + s->distances[j << 1];
                k = j;
            }
        }
        /* Use an elementary IIR filter to track the distance to date. */
#if defined(SPANDSP_USE_FIXED_POINTx)
        new_distances[i] = s->distances[k << 1]*9/10 + distances[tcm_paths[i][k]]*1/10;
#else
        new_distances[i] = s->distances[k << 1]*0.9f + distances[tcm_paths[i][k]]*0.1f;
#endif
        s->full_path_to_past_state_locations[s->trellis_ptr][i] = constel_maps[s->space_map][re][im][tcm_paths[i][k]];
        s->past_state_locations[s->trellis_ptr][i] = k << 1;
    }
    for (i = 4;  i < 8;  i++)
    {
        min = distances[tcm_paths[i][0]] + s->distances[1];
        k = 0;
        for (j = 1;  j < 4;  j++)
        {
            if (min > distances[tcm_paths[i][j]] + s->distances[(j << 1) + 1])
            {
                min = distances[tcm_paths[i][j]] + s->distances[(j << 1) + 1];
                k = j;
            }
        }
#if defined(SPANDSP_USE_FIXED_POINTx)
        new_distances[i] = s->distances[(k << 1) + 1]*9/10 + distances[tcm_paths[i][k]]*1/10;
#else
        new_distances[i] = s->distances[(k << 1) + 1]*0.9f + distances[tcm_paths[i][k]]*0.1f;
#endif
        s->full_path_to_past_state_locations[s->trellis_ptr][i] = constel_maps[s->space_map][re][im][tcm_paths[i][k]];
        s->past_state_locations[s->trellis_ptr][i] = (k << 1) + 1;
    }
    memcpy(s->distances, new_distances, sizeof(s->distances));

    /* Find the minimum distance to date. This is the start of the path back to the result. */
    min = s->distances[0];
    k = 0;
    for (i = 1;  i < 8;  i++)
    {
        if (min > s->distances[i])
        {
            min = s->distances[i];
            k = i;
        }
    }
    /* Trace back through every time step, starting with the current one, and find the
       state from which the path came one step before. At the end of this search, the
       last state found also points to the constellation point at that state. This is the
       output of the trellis. */
    for (i = 0, j = s->trellis_ptr;  i < V17_TRELLIS_LOOKBACK_DEPTH - 1;  i++)
    {
        k = s->past_state_locations[j][k];
        if (--j < 0)
            j = V17_TRELLIS_STORAGE_DEPTH - 1;
    }
    nearest = s->full_path_to_past_state_locations[j][k] >> 1;

    /* Differentially decode */
    raw = (nearest & 0x3C) | diff_code[((nearest & 0x03) << 2) | s->diff];
    s->diff = nearest & 0x03;
    for (i = 0;  i < s->bits_per_symbol;  i++)
    {
        put_bit(s, raw);
        raw >>= 1;
    }
    return constellation_state;
}
/*- End of function --------------------------------------------------------*/

static void process_half_baud(v17_rx_state_t *s, const complexf_t *sample)
{
    static const complexf_t cdba[4] =
    {
        { 6.0f,  2.0f},
        {-2.0f,  6.0f},
        { 2.0f, -6.0f},
        {-6.0f, -2.0f}
    };
    complexf_t z;
    complexf_t zz;
#if defined(SPANDSP_USE_FIXED_POINTx)
    const complexi_t *target;
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
    int constellation_state;

    /* This routine processes every half a baud, as we put things into the equalizer at the T/2 rate.
       This routine adapts the position of the half baud samples, which the caller takes. */

    /* Add a sample to the equalizer's circular buffer, but don't calculate anything
       at this time. */
    s->eq_buf[s->eq_step] = *sample;
    s->eq_step = (s->eq_step + 1) & V17_EQUALIZER_MASK;

    /* On alternate insertions we have a whole baud and must process it. */
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
    /* A little integration will now filter away much of the HF noise */
    s->baud_phase -= p;

    if (fabsf(s->baud_phase) > 100.0f)
    {
        if (s->baud_phase > 0.0f)
            i = (s->baud_phase > 1000.0f)  ?  15  :  1;
        else
            i = (s->baud_phase < -1000.0f)  ?  -15  :  -1;
        //printf("v = %10.5f %5d - %f %f %d\n", v, i, p, s->baud_phase, s->total_baud_timing_correction);

        s->eq_put_step += i;
        s->total_baud_timing_correction += i;
    }

    z = equalizer_get(s);

    constellation_state = 0;
    switch (s->training_stage)
    {
    case TRAINING_STAGE_NORMAL_OPERATION:
        /* Normal operation. */
        constellation_state = decode_baud(s, &z);
        target = &s->constellation[constellation_state];
        break;
    case TRAINING_STAGE_SYMBOL_ACQUISITION:
        /* Allow time for the symbol synchronisation to settle the symbol timing. */
        target = &z;
#if defined(IAXMODEM_STUFF)
        if (++s->training_count >= 100)
#else
        if (++s->training_count >= 50)
#endif
        {
            /* Record the current phase angle */
            s->angles[0] =
            s->start_angles[0] = arctan2(z.im, z.re);
            s->training_stage = TRAINING_STAGE_LOG_PHASE;
            if (s->agc_scaling_save == 0.0f)
                s->agc_scaling_save = s->agc_scaling;
        }
        break;
    case TRAINING_STAGE_LOG_PHASE:
        /* Record the current alternate phase angle */
        target = &z;
        angle = arctan2(z.im, z.re);
        s->training_count = 1;
        if (s->short_train)
        {
            /* We should already know the accurate carrier frequency. All we need to sort
               out is the phase. */
            /* Check if we just saw A or B */
            if ((uint32_t) (angle - s->start_angles[0]) < 0x80000000U)
            {
                angle = s->start_angles[0];
                s->angles[0] = 0xC0000000 + 219937506;
                s->angles[1] = 0x80000000 + 219937506;
            }
            else
            {
                s->angles[0] = 0x80000000 + 219937506;
                s->angles[1] = 0xC0000000 + 219937506;
            }
            /* Make a step shift in the phase, to pull it into line. We need to rotate the equalizer
               buffer, as well as the carrier phase, for this to play out nicely. */
            /* angle is now the difference between where A is, and where it should be */
            p = 3.14159f + angle*2.0f*3.14159f/(65536.0f*65536.0f) - 0.321751f;
            span_log(&s->logging, SPAN_LOG_FLOW, "Spin (short) by %.5f rads\n", p);
            zz = complex_setf(cosf(p), -sinf(p));
            for (i = 0;  i <= V17_EQUALIZER_MASK;  i++)
                s->eq_buf[i] = complex_mulf(&s->eq_buf[i], &zz);
            s->carrier_phase += (0x80000000 + angle - 219937506);

            s->carrier_track_p = 500000.0f;

            s->training_stage = TRAINING_STAGE_SHORT_WAIT_FOR_CDBA;
        }
        else
        {
            s->angles[1] =
            s->start_angles[1] = angle;
            s->training_stage = TRAINING_STAGE_WAIT_FOR_CDBA;
        }
        break;
    case TRAINING_STAGE_WAIT_FOR_CDBA:
        target = &z;
        angle = arctan2(z.im, z.re);
        /* Look for the initial ABAB sequence to display a phase reversal, which will
           signal the start of the scrambled CDBA segment */
        ang = angle - s->angles[(s->training_count - 1) & 0xF];
        s->angles[(s->training_count + 1) & 0xF] = angle;

        /* Do a coarse frequency adjustment about half way through the reversals, as if we wait until
           the end, we might have rotated too far to correct properly. */
        if (s->training_count == 100)
        {
            i = s->training_count;
            /* Avoid the possibility of a divide by zero */
            if (i)
            {
                j = i & 0xF;
                ang = (s->angles[j] - s->start_angles[0])/i
                    + (s->angles[j | 0x1] - s->start_angles[1])/i;
                s->carrier_phase_rate += 3*(ang/20);
                //span_log(&s->logging, SPAN_LOG_FLOW, "Angles %x, %x, %x, %x, dist %d\n", s->angles[j], s->start_angles[0], s->angles[j | 0x1], s->start_angles[1], i);

                s->start_angles[0] = s->angles[j];
                s->start_angles[1] = s->angles[j | 0x1];
            }
            //span_log(&s->logging, SPAN_LOG_FLOW, "%d %d %d %d %d\n", s->angles[s->training_count & 0xF], s->start_angles[0], s->angles[(s->training_count | 0x1) & 0xF], s->start_angles[1], s->training_count);
            span_log(&s->logging, SPAN_LOG_FLOW, "First coarse carrier frequency %7.2f (%d)\n", dds_frequencyf(s->carrier_phase_rate), s->training_count);

        }
        if ((ang > 0x40000000  ||  ang < -0x40000000)  &&  s->training_count >= 13)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "We seem to have a reversal at symbol %d\n", s->training_count);
            /* We seem to have a phase reversal */
            /* Slam the carrier frequency into line, based on the total phase drift over the last
               section. Use the shift from the odd bits and the shift from the even bits to get
               better jitter suppression. */
            /* TODO: We are supposed to deal with frequancy errors up to +-8Hz. Over 200+
                     symbols that is more than half a cycle. We get confused an do crazy things.
                     We can only cope with errors up to 5Hz right now. We need to implement
                     greater tolerance to be compliant, although it doesn't really matter much
                     these days. */
            /* Step back a few symbols so we don't get ISI distorting things. */
            i = (s->training_count - 8) & ~1;
            /* Avoid the possibility of a divide by zero */
            if (i - 100 + 8)
            {
                j = i & 0xF;
                ang = (s->angles[j] - s->start_angles[0])/(i - 100 + 8)
                    + (s->angles[j | 0x1] - s->start_angles[1])/(i - 100 + 8);
                s->carrier_phase_rate += 3*(ang/20);
                span_log(&s->logging, SPAN_LOG_FLOW, "Angles %x, %x, %x, %x, dist %d\n", s->angles[j], s->start_angles[0], s->angles[j | 0x1], s->start_angles[1], i);
            }
            //span_log(&s->logging, SPAN_LOG_FLOW, "%d %d %d %d %d\n", s->angles[s->training_count & 0xF], s->start_angles[0], s->angles[(s->training_count | 0x1) & 0xF], s->start_angles[1], s->training_count);
            span_log(&s->logging, SPAN_LOG_FLOW, "Second coarse carrier frequency %7.2f (%d)\n", dds_frequencyf(s->carrier_phase_rate), s->training_count);
            /* Check if the carrier frequency is plausible */
            if (s->carrier_phase_rate < dds_phase_ratef(CARRIER_NOMINAL_FREQ - 20.0f)
                ||
                s->carrier_phase_rate > dds_phase_ratef(CARRIER_NOMINAL_FREQ + 20.0f))
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (sequence failed)\n");
                /* Park this modem */
                s->agc_scaling_save = 0.0f;
                s->training_stage = TRAINING_STAGE_PARKED;
                report_status_change(s, PUTBIT_TRAINING_FAILED);
                break;
            }

            /* Make a step shift in the phase, to pull it into line. We need to rotate the equalizer buffer,
               as well as the carrier phase, for this to play out nicely. */
            /* angle is now the difference between where C is, and where it should be */
            p = angle*2.0f*3.14159f/(65536.0f*65536.0f) - 0.321751f;
            span_log(&s->logging, SPAN_LOG_FLOW, "Spin (long) by %.5f rads\n", p);
            zz = complex_setf(cosf(p), -sinf(p));
            for (i = 0;  i <= V17_EQUALIZER_MASK;  i++)
                s->eq_buf[i] = complex_mulf(&s->eq_buf[i], &zz);
            s->carrier_phase += (angle - 219937506);

            /* We have just seen the first symbol of the scrambled sequence, so skip it. */
            descramble(s, 1);
            descramble(s, 1);
            s->training_count = 1;
            s->training_stage = TRAINING_STAGE_COARSE_TRAIN_ON_CDBA;
            report_status_change(s, PUTBIT_TRAINING_IN_PROGRESS);
            break;
        }
        if (++s->training_count > V17_TRAINING_SEG_1_LEN)
        {
            /* This is bogus. There are not this many bits in this section
               of a real training sequence. Note that this might be TEP. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (sequence failed)\n");
            /* Park this modem */
            s->agc_scaling_save = 0.0f;
            s->training_stage = TRAINING_STAGE_PARKED;
            report_status_change(s, PUTBIT_TRAINING_FAILED);
        }
        break;
    case TRAINING_STAGE_COARSE_TRAIN_ON_CDBA:
        /* Train on the scrambled CDBA section. */
        bit = descramble(s, 1);
        bit = (bit << 1) | descramble(s, 1);
        target = &cdba[bit];
        track_carrier(s, &z, target);
        tune_equalizer(s, &z, target);
#if defined(IAXMODEM_STUFF)
        zz = complex_subf(&z, target);
        s->training_error = powerf(&zz);
        if (++s->training_count == V17_TRAINING_SEG_2_LEN - 2000  ||  s->training_error < 1.0f  ||  s->training_error > 200.0f)
#else
        if (++s->training_count == V17_TRAINING_SEG_2_LEN - 2000)
#endif
        {
            /* Now the equaliser adaption should be getting somewhere, slow it down, or it will never
               tune very well on a noisy signal. */
            s->eq_delta *= EQUALIZER_SLOW_ADAPT_RATIO;
            s->carrier_track_i = 1000.0f;
            s->training_stage = TRAINING_STAGE_FINE_TRAIN_ON_CDBA;
        }
        break;
    case TRAINING_STAGE_FINE_TRAIN_ON_CDBA:
        /* Train on the scrambled CDBA section. */
        bit = descramble(s, 1);
        bit = (bit << 1) | descramble(s, 1);
        target = &cdba[bit];
        /* By this point the training should be comming into focus. */
        track_carrier(s, &z, target);
        tune_equalizer(s, &z, target);
        if (++s->training_count >= V17_TRAINING_SEG_2_LEN - 48)
        {
            s->training_error = 0.0f;
            s->carrier_track_i = 100.0f;
            s->carrier_track_p = 500000.0f;
            s->training_stage = TRAINING_STAGE_TRAIN_ON_CDBA_AND_TEST;
        }
        break;
    case TRAINING_STAGE_TRAIN_ON_CDBA_AND_TEST:
        /* Continue training on the scrambled CDBA section, but measure the quality of training too. */
        bit = descramble(s, 1);
        bit = (bit << 1) | descramble(s, 1);
        target = &cdba[bit];
        //span_log(&s->logging, SPAN_LOG_FLOW, "%5d [%15.5f, %15.5f]     [%15.5f, %15.5f]\n", s->training_count, z.re, z.im, cdba[bit].re, cdba[bit].im);
        /* We ignore the last few symbols because it seems some modems do not end this
           part properly, and it throws things off. */
        if (++s->training_count < V17_TRAINING_SEG_2_LEN - 20)
        {
            track_carrier(s, &z, target);
            tune_equalizer(s, &z, target);
            /* Measure the training error */
            zz = complex_subf(&z, &cdba[bit]);
            s->training_error += powerf(&zz);
        }
        else if (s->training_count >= V17_TRAINING_SEG_2_LEN)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Long training error %f\n", s->training_error);
            if (s->training_error < 40.0f)
            {
                s->training_count = 0;
                s->training_error = 0.0f;
                s->training_stage = TRAINING_STAGE_BRIDGE;
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (convergence failed)\n");
                /* Park this modem */
                s->agc_scaling_save = 0.0f;
                s->training_stage = TRAINING_STAGE_PARKED;
                report_status_change(s, PUTBIT_TRAINING_FAILED);
            }
        }
        break;
    case TRAINING_STAGE_BRIDGE:
        descramble(s, V17_BRIDGE_WORD >> ((s->training_count & 0x7) << 1));
        descramble(s, V17_BRIDGE_WORD >> (((s->training_count & 0x7) << 1) + 1));
        target = &z;
        if (++s->training_count >= V17_TRAINING_SEG_3_LEN)
        {
            s->training_count = 0;
            s->training_error = 0.0f;
            s->training_stage = TRAINING_STAGE_TCM_WINDUP;
        }
        break;
    case TRAINING_STAGE_SHORT_WAIT_FOR_CDBA:
        target = &cdba[(s->training_count & 1) + 2];
        /* Look for the initial ABAB sequence to display a phase reversal, which will
           signal the start of the scrambled CDBA segment */
        angle = arctan2(z.im, z.re);
        ang = angle - s->angles[s->training_count & 1];
        if (ang > 0x40000000  ||  ang < -0x40000000)
        {
            /* We seem to have a phase reversal */
            /* We have just seen the first symbol of the scrambled sequence, so skip it. */
            descramble(s, 1);
            descramble(s, 1);
            s->training_count = 1;
            s->training_error = 0.0f;
            s->training_stage = TRAINING_STAGE_SHORT_TRAIN_ON_CDBA_AND_TEST;
            break;
        }
        track_carrier(s, &z, target);
        if (++s->training_count > V17_TRAINING_SEG_1_LEN)
        {
            /* This is bogus. There are not this many bits in this section
               of a real training sequence. Note that this might be TEP. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (sequence failed)\n");
            /* Park this modem */
            report_status_change(s, PUTBIT_TRAINING_FAILED);
            s->training_stage = TRAINING_STAGE_PARKED;
        }
        break;
    case TRAINING_STAGE_SHORT_TRAIN_ON_CDBA_AND_TEST:
        /* Short retrain on the scrambled CDBA section, but measure the quality of training too. */
        bit = descramble(s, 1);
        bit = (bit << 1) | descramble(s, 1);
        //span_log(&s->logging, SPAN_LOG_FLOW, "%5d [%15.5f, %15.5f]     [%15.5f, %15.5f] %d\n", s->training_count, z.re, z.im, cdba[bit].re, cdba[bit].im, arctan2(z.im, z.re));
        target = &cdba[bit];
        track_carrier(s, &z, target);
        //tune_equalizer(s, &z, target);
        /* Measure the training error */
        if (s->training_count > 8)
        {
            zz = complex_subf(&z, &cdba[bit]);
            s->training_error += powerf(&zz);
        }
        if (++s->training_count >= V17_TRAINING_SHORT_SEG_2_LEN)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Short training error %f\n", s->training_error);
            s->carrier_track_i = 100.0f;
            s->carrier_track_p = 500000.0f;
            /* TODO: This was changed from 20.0 to 200.0 after studying real world failures.
                     However, it is not clear why this is an improvement, If something gives
                     a training error way over 20, surely it shouldn't decode too well? */
            if (s->training_error < 200.0f)
            {
                s->training_count = 0;
                s->training_stage = TRAINING_STAGE_TCM_WINDUP;
                report_status_change(s, PUTBIT_TRAINING_IN_PROGRESS);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Short training failed (convergence failed)\n");
                /* Park this modem */
                report_status_change(s, PUTBIT_TRAINING_FAILED);
                s->training_stage = TRAINING_STAGE_PARKED;
            }
        }
        break;
    case TRAINING_STAGE_TCM_WINDUP:
        /* We need to wait 15 bauds while the trellis fills up. */
        //span_log(&s->logging, SPAN_LOG_FLOW, "%5d %15.5f, %15.5f\n", s->training_count, z.re, z.im);
        constellation_state = decode_baud(s, &z);
        target = &s->constellation[constellation_state];
        /* Measure the training error */
        zz = complex_subf(&z, target);
        s->training_error += powerf(&zz);
        if (++s->training_count >= V17_TRAINING_SEG_4A_LEN)
        {
            s->training_count = 0;
            s->training_error = 0.0f;
            /* Restart the differential decoder */
            s->diff = (s->short_train)  ?  0  :  1;
            s->training_stage = TRAINING_STAGE_TEST_ONES;
        }
        break;
    case TRAINING_STAGE_TEST_ONES:
        /* We are in the test phase, where we check that we can receive reliably.
           We should get a run of 1's, 48 symbols long. */
        //span_log(&s->logging, SPAN_LOG_FLOW, "%5d %15.5f, %15.5f\n", s->training_count, z.re, z.im);
        constellation_state = decode_baud(s, &z);
        target = &s->constellation[constellation_state];
        /* Measure the training error */
        zz = complex_subf(&z, target);
        s->training_error += powerf(&zz);
        if (++s->training_count >= V17_TRAINING_SEG_4_LEN)
        {
#if defined(IAXMODEM_STUFF)
            if (s->training_error < 80.0f)
#else
            if (s->training_error < 30.0f)
#endif
            {
                /* We are up and running */
                span_log(&s->logging, SPAN_LOG_FLOW, "Training succeeded (constellation mismatch %f)\n", s->training_error);
                report_status_change(s, PUTBIT_TRAINING_SUCCEEDED);
                /* Apply some lag to the carrier off condition, to ensure the last few bits get pushed through
                   the processing. */
                s->signal_present = 60;
                equalizer_save(s);
                s->carrier_phase_rate_save = s->carrier_phase_rate;
                s->short_train = TRUE;
                s->training_stage = TRAINING_STAGE_NORMAL_OPERATION;
            }
            else
            {
                /* Training has failed */
                span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (constellation mismatch %f)\n", s->training_error);
                /* Park this modem */
                if (!s->short_train)
                    s->agc_scaling_save = 0.0f;
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
        s->qam_report(s->qam_user_data, &z, target, constellation_state);
}
/*- End of function --------------------------------------------------------*/

int v17_rx(v17_rx_state_t *s, const int16_t amp[], int len)
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
        s->rrc_filter[s->rrc_filter_step + V17_RX_FILTER_STEPS] = amp[i];
        if (++s->rrc_filter_step >= V17_RX_FILTER_STEPS)
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
            /* Look for power below turnoff threshold to turn the carrier off */
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
                    v17_rx_restart(s, s->bit_rate, s->short_train);
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
            /* Look for power exceeding turnon threshold to turn the carrier on */
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
        for (j = 1;  j < V17_RX_FILTER_STEPS;  j++)
            zi.re += (int32_t) rx_pulseshaper[step][j].re*(int32_t) s->rrc_filter[j + s->rrc_filter_step];
        sample.re = zi.re*s->agc_scaling;
#else
        zz.re = rx_pulseshaper[step][0].re*s->rrc_filter[s->rrc_filter_step];
        for (j = 1;  j < V17_RX_FILTER_STEPS;  j++)
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

        /* Put things into the equalization buffer at T/2 rate. The symbol sync.
           will fiddle the step to align this with the symbols. */
        if (s->eq_put_step <= 0)
        {
            /* Only AGC until we have locked down the setting. */
            if (s->agc_scaling_save == 0.0f)
                s->agc_scaling = (1.0f/RX_PULSESHAPER_GAIN)*2.17f/sqrtf(power);
            /* Pulse shape while still at the carrier frequency, using a quadrature
               pair of filters. This results in a properly bandpass filtered complex
               signal, which can be brought directly to baseband by complex mixing.
               No further filtering, to remove mixer harmonics, is needed. */
            step = -s->eq_put_step;
            if (step > RX_PULSESHAPER_COEFF_SETS - 1)
                step = RX_PULSESHAPER_COEFF_SETS - 1;
#if defined(SPANDSP_USE_FIXED_POINT)
            zi.im = (int32_t) rx_pulseshaper[step][0].im*(int32_t) s->rrc_filter[s->rrc_filter_step];
            for (j = 1;  j < V17_RX_FILTER_STEPS;  j++)
                zi.im += (int32_t) rx_pulseshaper[step][j].im*(int32_t) s->rrc_filter[j + s->rrc_filter_step];
            sample.im = zi.im*s->agc_scaling;
#else
            zz.im = rx_pulseshaper[step][0].im*s->rrc_filter[s->rrc_filter_step];
            for (j = 1;  j < V17_RX_FILTER_STEPS;  j++)
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

void v17_rx_set_put_bit(v17_rx_state_t *s, put_bit_func_t put_bit, void *user_data)
{
    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void v17_rx_set_modem_status_handler(v17_rx_state_t *s, modem_tx_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

int v17_rx_restart(v17_rx_state_t *s, int bit_rate, int short_train)
{
    int i;

    span_log(&s->logging, SPAN_LOG_FLOW, "Restarting V.17, %dbps, %s training\n", bit_rate, (short_train)  ?  "short"  :  "long");
    switch (bit_rate)
    {
    case 14400:
        s->constellation = v17_14400_constellation;
        s->space_map = 0;
        s->bits_per_symbol = 6;
        break;
    case 12000:
        s->constellation = v17_12000_constellation;
        s->space_map = 1;
        s->bits_per_symbol = 5;
        break;
    case 9600:
        s->constellation = v17_9600_constellation;
        s->space_map = 2;
        s->bits_per_symbol = 4;
        break;
    case 7200:
        s->constellation = v17_7200_constellation;
        s->space_map = 3;
        s->bits_per_symbol = 3;
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

    s->diff = 1;
    s->scramble_reg = 0x2ECDD5;
    s->training_stage = TRAINING_STAGE_SYMBOL_ACQUISITION;
    s->training_count = 0;
    s->training_error = 0.0f;
    s->signal_present = 0;
#if defined(IAXMODEM_STUFF)
    s->high_sample = 0;
    s->low_samples = 0;
    s->carrier_drop_pending = FALSE;
#endif
    if (short_train != 2)
        s->short_train = short_train;
    memset(s->start_angles, 0, sizeof(s->start_angles));
    memset(s->angles, 0, sizeof(s->angles));

    /* Initialise the TCM decoder parameters. */
    /* The accumulated distance vectors are set so state zero starts
       at a value of zero, and all others start larger. This forces the
       initial paths to merge at the zero states. */
    for (i = 0;  i < 8;  i++)
#if defined(SPANDSP_USE_FIXED_POINTx)
        s->distances[i] = 99*DIST_FACTOR*DIST_FACTOR;
#else
        s->distances[i] = 99.0f;
#endif
    memset(s->full_path_to_past_state_locations, 0, sizeof(s->full_path_to_past_state_locations));
    memset(s->past_state_locations, 0, sizeof(s->past_state_locations));
    s->distances[0] = 0;
    s->trellis_ptr = 14;

    span_log(&s->logging, SPAN_LOG_FLOW, "Phase rates %f %f\n", dds_frequencyf(s->carrier_phase_rate), dds_frequencyf(s->carrier_phase_rate_save));
    s->carrier_phase = 0;
    power_meter_init(&(s->power), 4);

    if (s->short_train)
    {
        s->carrier_phase_rate = s->carrier_phase_rate_save;
        s->agc_scaling = s->agc_scaling_save;
        equalizer_restore(s);
        /* Don't allow any frequency correction at all, until we start to pull the phase in. */
        s->carrier_track_i = 0.0f;
        s->carrier_track_p = 40000.0f;
    }
    else
    {
        s->carrier_phase_rate = dds_phase_ratef(CARRIER_NOMINAL_FREQ);
        s->agc_scaling_save = 0.0f;
        s->agc_scaling = 0.0017f/RX_PULSESHAPER_GAIN;
        equalizer_reset(s);
        s->carrier_track_i = 5000.0f;
        s->carrier_track_p = 40000.0f;
    }
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

v17_rx_state_t *v17_rx_init(v17_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data)
{
    if (s == NULL)
    {
        if ((s = (v17_rx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.17 RX");
    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
    s->short_train = FALSE;
    v17_rx_signal_cutoff(s, -45.5f);
    s->agc_scaling = 0.0017f/RX_PULSESHAPER_GAIN;
    s->agc_scaling_save = 0.0f;
    s->carrier_phase_rate_save = dds_phase_ratef(CARRIER_NOMINAL_FREQ);

    v17_rx_restart(s, bit_rate, s->short_train);
    return s;
}
/*- End of function --------------------------------------------------------*/

int v17_rx_free(v17_rx_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

void v17_rx_set_qam_report_handler(v17_rx_state_t *s, qam_report_handler_t handler, void *user_data)
{
    s->qam_report = handler;
    s->qam_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
