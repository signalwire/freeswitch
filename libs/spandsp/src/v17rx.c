#define IAXMODEM_STUFF
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
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/fast_convert.h"
#include "spandsp/math_fixed.h"
#include "spandsp/saturated.h"
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/vector_int.h"
#include "spandsp/complex_vector_int.h"
#include "spandsp/async.h"
#include "spandsp/power_meter.h"
#include "spandsp/arctan2.h"
#include "spandsp/dds.h"
#include "spandsp/complex_filters.h"

#include "spandsp/v29rx.h"
#include "spandsp/v17tx.h"
#include "spandsp/v17rx.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/power_meter.h"
#include "spandsp/private/v17rx.h"

#if defined(SPANDSP_USE_FIXED_POINTx)
#define FP_SCALE(x)                     FP_Q6_10(x)
#define FP_FACTOR                       1024
#define FP_SHIFT_FACTOR                 12
#else
#define FP_SCALE(x)                     (x)
#endif

#if defined(SPANDSP_USE_FIXED_POINTx)
#define FP_SYNC_SCALE(x)                FP_Q6_10(x)
#define FP_SYNC_SCALE_32(x)             FP_Q22_10(x)
#define FP_SYNC_SHIFT_FACTOR            10
#else
#define FP_SYNC_SCALE(x)                (x)
#define FP_SYNC_SCALE_32(x)             (x)
#endif

#define FP_CONSTELLATION_SCALE(x)       FP_SCALE(x)
#if defined(SPANDSP_USE_FIXED_POINT)
#define FP_CONSTELLATION_SHIFT_FACTOR   FP_SHIFT_FACTOR
#endif

#include "v17_v32bis_rx_rrc.h"
#include "v17_v32bis_tx_constellation_maps.h"
#include "v17_v32bis_rx_constellation_maps.h"

/*! The nominal frequency of the carrier, in Hertz */
#define CARRIER_NOMINAL_FREQ            1800.0f
/*! The nominal baud or symbol rate */
#define BAUD_RATE                       2400
/*! The adaption rate coefficient for the equalizer during initial training */
#define EQUALIZER_FAST_ADAPTION_DELTA   (0.21f/V17_EQUALIZER_LEN)
/*! The adaption rate coefficient for the equalizer during fine tuning */
#define EQUALIZER_SLOW_ADAPTION_DELTA   (0.1f*EQUALIZER_FAST_ADAPTION_DELTA)

/* Segments of the training sequence */
/*! The length of training segment 1, in symbols */
#define V17_TRAINING_SEG_1_LEN          256
/*! The length of training segment 2 in long training mode, in symbols */
#define V17_TRAINING_SEG_2_LEN          2976
/*! The length of training segment 2 in short training mode, in symbols */
#define V17_TRAINING_SHORT_SEG_2_LEN    38
/*! The length of training segment 3, in symbols */
#define V17_TRAINING_SEG_3_LEN          64
/*! The length of training segment 4A, in symbols */
#define V17_TRAINING_SEG_4A_LEN         15
/*! The length of training segment 4, in symbols */
#define V17_TRAINING_SEG_4_LEN          48

/*! The 16 bit pattern used in the bridge section of the training sequence */
#define V17_BRIDGE_WORD                 0x8880

/* Coefficients for the band edge symbol timing synchroniser (alpha = 0.99) */
/* low_edge = 2.0f*M_PI*(CARRIER_NOMINAL_FREQ - BAUD_RATE/2.0f)/SAMPLE_RATE; */
/* high_edge = 2.0f*M_PI*(CARRIER_NOMINAL_FREQ + BAUD_RATE/2.0f)/SAMPLE_RATE; */
#define SIN_LOW_BAND_EDGE               0.453990499f
#define COS_LOW_BAND_EDGE               0.891006542f
#define SIN_HIGH_BAND_EDGE              0.707106781f
#define COS_HIGH_BAND_EDGE             -0.707106781f
#define ALPHA                           0.99f

#define SYNC_LOW_BAND_EDGE_COEFF_0      FP_SYNC_SCALE(2.0f*ALPHA*COS_LOW_BAND_EDGE)
#define SYNC_LOW_BAND_EDGE_COEFF_1      FP_SYNC_SCALE(-ALPHA*ALPHA)
#define SYNC_LOW_BAND_EDGE_COEFF_2      FP_SYNC_SCALE(-ALPHA*SIN_LOW_BAND_EDGE)
#define SYNC_HIGH_BAND_EDGE_COEFF_0     FP_SYNC_SCALE(2.0f*ALPHA*COS_HIGH_BAND_EDGE)
#define SYNC_HIGH_BAND_EDGE_COEFF_1     FP_SYNC_SCALE(-ALPHA*ALPHA)
#define SYNC_HIGH_BAND_EDGE_COEFF_2     FP_SYNC_SCALE(-ALPHA*SIN_HIGH_BAND_EDGE)
#define SYNC_MIXED_EDGES_COEFF_3        FP_SYNC_SCALE(-ALPHA*ALPHA*(SIN_HIGH_BAND_EDGE*COS_LOW_BAND_EDGE - SIN_LOW_BAND_EDGE*COS_HIGH_BAND_EDGE))

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

#if defined(SPANDSP_USE_FIXED_POINTx)
static const int16_t constellation_spacing[4] =
#else
static const float constellation_spacing[4] =
#endif
{
    FP_SCALE(1.414f),
    FP_SCALE(2.0f),
    FP_SCALE(2.828f),
    FP_SCALE(4.0f)
};

SPAN_DECLARE(float) v17_rx_carrier_frequency(v17_rx_state_t *s)
{
    return dds_frequencyf(s->carrier_phase_rate);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) v17_rx_symbol_timing_correction(v17_rx_state_t *s)
{
    return (float) s->total_baud_timing_correction/((float) RX_PULSESHAPER_COEFF_SETS*10.0f/3.0f);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) v17_rx_signal_power(v17_rx_state_t *s)
{
    return power_meter_current_dbm0(&s->power) + 3.98f;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v17_rx_signal_cutoff(v17_rx_state_t *s, float cutoff)
{
    /* The 0.4 factor allows for the gain of the DC blocker */
    s->carrier_on_power = (int32_t) (power_meter_level_dbm0(cutoff + 2.5f)*0.4f);
    s->carrier_off_power = (int32_t) (power_meter_level_dbm0(cutoff - 2.5f)*0.4f);
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

#if defined(SPANDSP_USE_FIXED_POINT)
SPAN_DECLARE(int) v17_rx_equalizer_state(v17_rx_state_t *s, complexi16_t **coeffs)
#else
SPAN_DECLARE(int) v17_rx_equalizer_state(v17_rx_state_t *s, complexf_t **coeffs)
#endif
{
#if defined(SPANDSP_USE_FIXED_POINT)
    *coeffs = NULL;
    return 0;
#else
    *coeffs = s->eq_coeff;
    return V17_EQUALIZER_LEN;
#endif
}
/*- End of function --------------------------------------------------------*/

static void equalizer_save(v17_rx_state_t *s)
{
#if defined(SPANDSP_USE_FIXED_POINTx)
    cvec_copyi16(s->eq_coeff_save, s->eq_coeff, V17_EQUALIZER_LEN);
#else
    cvec_copyf(s->eq_coeff_save, s->eq_coeff, V17_EQUALIZER_LEN);
#endif
}
/*- End of function --------------------------------------------------------*/

static void equalizer_restore(v17_rx_state_t *s)
{
#if defined(SPANDSP_USE_FIXED_POINTx)
    cvec_copyi16(s->eq_coeff, s->eq_coeff_save, V17_EQUALIZER_LEN);
    cvec_zeroi16(s->eq_buf, V17_EQUALIZER_LEN);
    s->eq_delta = 32768.0f*EQUALIZER_SLOW_ADAPTION_DELTA;
#else
    cvec_copyf(s->eq_coeff, s->eq_coeff_save, V17_EQUALIZER_LEN);
    cvec_zerof(s->eq_buf, V17_EQUALIZER_LEN);
    s->eq_delta = EQUALIZER_SLOW_ADAPTION_DELTA;
#endif

    s->eq_put_step = RX_PULSESHAPER_COEFF_SETS*10/(3*2) - 1;
    s->eq_step = 0;
    s->eq_skip = 0;
}
/*- End of function --------------------------------------------------------*/

static void equalizer_reset(v17_rx_state_t *s)
{
    /* Start with an equalizer based on everything being perfect */
#if defined(SPANDSP_USE_FIXED_POINTx)
    static const complexi16_t x = {FP_CONSTELLATION_SCALE(3.0f), FP_CONSTELLATION_SCALE(0.0f)};

    cvec_zeroi16(s->eq_coeff, V17_EQUALIZER_LEN);
    s->eq_coeff[V17_EQUALIZER_PRE_LEN] = x;
    cvec_zeroi16(s->eq_buf, V17_EQUALIZER_LEN);
    s->eq_delta = 32768.0f*EQUALIZER_FAST_ADAPTION_DELTA;
#else
    static const complexf_t x = {FP_CONSTELLATION_SCALE(3.0f), FP_CONSTELLATION_SCALE(0.0f)};

    cvec_zerof(s->eq_coeff, V17_EQUALIZER_LEN);
    s->eq_coeff[V17_EQUALIZER_PRE_LEN] = x;
    cvec_zerof(s->eq_buf, V17_EQUALIZER_LEN);
    s->eq_delta = EQUALIZER_FAST_ADAPTION_DELTA;
#endif

    s->eq_put_step = RX_PULSESHAPER_COEFF_SETS*10/(3*2) - 1;
    s->eq_step = 0;
    s->eq_skip = 0;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTx)
static __inline__ complexi16_t equalizer_get(v17_rx_state_t *s)
{
    complexi32_t zz;
    complexi16_t z;

    /* Get the next equalized value. */
    zz = cvec_circular_dot_prodi16(s->eq_buf, s->eq_coeff, V17_EQUALIZER_LEN, s->eq_step);
    z.re = zz.re >> FP_SHIFT_FACTOR;
    z.im = zz.im >> FP_SHIFT_FACTOR;
    return z;
}
#else
static __inline__ complexf_t equalizer_get(v17_rx_state_t *s)
{
    /* Get the next equalized value. */
    return cvec_circular_dot_prodf(s->eq_buf, s->eq_coeff, V17_EQUALIZER_LEN, s->eq_step);
}
#endif
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTx)
static void tune_equalizer(v17_rx_state_t *s, const complexi16_t *z, const complexi16_t *target)
{
    complexi16_t err;

    /* Find the x and y mismatch from the exact constellation position. */
    err = complex_subi16(target, z);
    err.re = ((int32_t) err.re*s->eq_delta) >> 15;
    err.im = ((int32_t) err.im*s->eq_delta) >> 15;
    cvec_circular_lmsi16(s->eq_buf, s->eq_coeff, V17_EQUALIZER_LEN, s->eq_step, &err);
}
#else
static void tune_equalizer(v17_rx_state_t *s, const complexf_t *z, const complexf_t *target)
{
    complexf_t err;

    /* Find the x and y mismatch from the exact constellation position. */
    err = complex_subf(target, z);
    //span_log(&s->logging, SPAN_LOG_FLOW, "Equalizer error %f\n", sqrt(err.re*err.re + err.im*err.im));
    err.re *= s->eq_delta;
    err.im *= s->eq_delta;
    cvec_circular_lmsf(s->eq_buf, s->eq_coeff, V17_EQUALIZER_LEN, s->eq_step, &err);
}
#endif
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTx)
static __inline__ void track_carrier(v17_rx_state_t *s, const complexi16_t *z, const complexi16_t *target)
#else
static __inline__ void track_carrier(v17_rx_state_t *s, const complexf_t *z, const complexf_t *target)
#endif
{
#if defined(SPANDSP_USE_FIXED_POINTx)
    int32_t error;
#else
    float error;
#endif

    /* For small errors the imaginary part of the difference between the actual and the target
       positions is proportional to the phase error, for any particular target. However, the
       different amplitudes of the various target positions scale things. */
#if defined(SPANDSP_USE_FIXED_POINTx)
    error = z->im*(((int32_t) target->re) >> FP_SHIFT_FACTOR) - z->re*(((int32_t) target->im) >> FP_SHIFT_FACTOR);
    s->carrier_phase_rate += ((s->carrier_track_i*error) >> FP_SHIFT_FACTOR);
    s->carrier_phase += ((s->carrier_track_p*error) >> FP_SHIFT_FACTOR);
#else
    error = z->im*target->re - z->re*target->im;
    s->carrier_phase_rate += (int32_t) (s->carrier_track_i*error);
    s->carrier_phase += (int32_t) (s->carrier_track_p*error);
    //span_log(&s->logging, SPAN_LOG_FLOW, "Im = %15.5f   f = %15.5f\n", error, dds_frequencyf(s->carrier_phase_rate));
#endif
}
/*- End of function --------------------------------------------------------*/

static int descramble(v17_rx_state_t *s, int in_bit)
{
    int out_bit;

    in_bit &= 1;
    out_bit = (in_bit ^ (s->scramble_reg >> s->scrambler_tap) ^ (s->scramble_reg >> (23 - 1))) & 1;
    s->scramble_reg <<= 1;
    if (s->training_stage > TRAINING_STAGE_NORMAL_OPERATION  &&  s->training_stage < TRAINING_STAGE_TCM_WINDUP)
        s->scramble_reg |= out_bit;
    else
        s->scramble_reg |= (in_bit & 1);
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void put_bit(v17_rx_state_t *s, int bit)
{
    int out_bit;

    /* We need to strip the last part of the training - the test period of all 1s -
       before we let data go to the application. */
    out_bit = descramble(s, bit);
    if (s->training_stage == TRAINING_STAGE_NORMAL_OPERATION)
    {
        s->put_bit(s->put_bit_user_data, out_bit);
    }
    else if (s->training_stage == TRAINING_STAGE_TEST_ONES)
    {
        /* The bits during the final stage of training should be all ones. However,
           buggy modems mean you cannot rely on this. Therefore we don't bother
           testing for ones, but just rely on a constellation mismatch measurement. */
        //span_log(&s->logging, SPAN_LOG_FLOW, "A 1 is really %d\n", out_bit);
    }
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTx)
static __inline__ uint32_t dist_sq(const complexi16_t *x, const complexi16_t *y)
{
    return (int32_t) (x->re - y->re)*(x->re - y->re) + (int32_t) (x->im - y->im)*(x->im - y->im);
}
/*- End of function --------------------------------------------------------*/
#else
static __inline__ float dist_sq(const complexf_t *x, const complexf_t *y)
{
    return (x->re - y->re)*(x->re - y->re) + (x->im - y->im)*(x->im - y->im);
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(SPANDSP_USE_FIXED_POINTx)
static int decode_baud(v17_rx_state_t *s, complexi16_t *z)
#else
static int decode_baud(v17_rx_state_t *s, complexf_t *z)
#endif
{
    static const uint8_t v32bis_4800_differential_decoder[4][4] =
    {
        {2, 3, 0, 1},
        {0, 2, 1, 3},
        {3, 1, 2, 0},
        {1, 0, 3, 2}
    };
    static const uint8_t v17_differential_decoder[4][4] =
    {
        {0, 1, 2, 3},
        {3, 0, 1, 2},
        {2, 3, 0, 1},
        {1, 2, 3, 0}
    };
    static const uint8_t tcm_paths[8][4] =
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
#define DIST_FACTOR 1024    /* Something less than sqrt(0xFFFFFFFF/10)/10 */
    complexi32_t zi;
    uint32_t distances[8];
    uint32_t new_distances[8];
    uint32_t min;
    complexi32_t ci;
#else
    float distances[8];
    float new_distances[8];
    float min;
#endif

#if defined(SPANDSP_USE_FIXED_POINTx)
    re = (z->re + FP_CONSTELLATION_SCALE(9.0f)) >> (FP_CONSTELLATION_SHIFT_FACTOR - 1);
    im = (z->im + FP_CONSTELLATION_SCALE(9.0f)) >> (FP_CONSTELLATION_SHIFT_FACTOR - 1);
#else
    re = (int) ((z->re + FP_CONSTELLATION_SCALE(9.0f))*2.0f);
    im = (int) ((z->im + FP_CONSTELLATION_SCALE(9.0f))*2.0f);
#endif
    if (re > 35)
        re = 35;
    else if (re < 0)
        re = 0;
    if (im > 35)
        im = 35;
    else if (im < 0)
        im = 0;
    if (s->bits_per_symbol == 2)
    {
        /* 4800bps V.32bis mode, without trellis coding */
        nearest = constel_map_4800[re][im];
        raw = v32bis_4800_differential_decoder[s->diff][nearest];
        s->diff = nearest;
        put_bit(s, raw);
        put_bit(s, raw >> 1);
        return nearest;
    }

    /* Find a set of 8 candidate constellation positions, that are the closest
       to the target, with different patterns in the last 3 bits. */
#if defined(SPANDSP_USE_FIXED_POINTx)
    min = 0xFFFFFFFF;
    zi = complex_seti32(z->re*DIST_FACTOR, z->im*DIST_FACTOR);
#else
    min = 9999999.0f;
#endif
    j = 0;
    for (i = 0;  i < 8;  i++)
    {
        nearest = constel_maps[s->space_map][re][im][i];
#if defined(SPANDSP_USE_FIXED_POINTx)
        ci = complex_seti32(s->constellation[nearest].re*DIST_FACTOR,
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
    raw = (nearest & 0x3C) | v17_differential_decoder[s->diff][nearest & 0x03];
    s->diff = nearest & 0x03;
    for (i = 0;  i < s->bits_per_symbol;  i++)
    {
        put_bit(s, raw);
        raw >>= 1;
    }
    return constellation_state;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void symbol_sync(v17_rx_state_t *s)
{
    int i;
#if defined(SPANDSP_USE_FIXED_POINTx)
    int32_t v;
    int32_t p;
#else
    float v;
    float p;
#endif

    /* This routine adapts the position of the half baud samples entering the equalizer. */

    /* This symbol sync scheme is based on the technique first described by Dominique Godard in
        Passband Timing Recovery in an All-Digital Modem Receiver
        IEEE TRANSACTIONS ON COMMUNICATIONS, VOL. COM-26, NO. 5, MAY 1978 */

    /* This is slightly rearranged from figure 3b of the Godard paper, as this saves a couple of
       maths operations */
#if defined(SPANDSP_USE_FIXED_POINTx)
    /* TODO: The scalings used here need more thorough evaluation, to see if overflows are possible. */
    /* Cross correlate */
    v = (((s->symbol_sync_low[1] >> (FP_SYNC_SHIFT_FACTOR/2))*(s->symbol_sync_high[0] >> (FP_SYNC_SHIFT_FACTOR/2))) >> 14)*SYNC_LOW_BAND_EDGE_COEFF_2
      - (((s->symbol_sync_low[0] >> (FP_SYNC_SHIFT_FACTOR/2))*(s->symbol_sync_high[1] >> (FP_SYNC_SHIFT_FACTOR/2))) >> 14)*SYNC_HIGH_BAND_EDGE_COEFF_2
      + (((s->symbol_sync_low[1] >> (FP_SYNC_SHIFT_FACTOR/2))*(s->symbol_sync_high[1] >> (FP_SYNC_SHIFT_FACTOR/2))) >> 14)*SYNC_MIXED_EDGES_COEFF_3;
    /* Filter away any DC component */
    p = v - s->symbol_sync_dc_filter[1];
    s->symbol_sync_dc_filter[1] = s->symbol_sync_dc_filter[0];
    s->symbol_sync_dc_filter[0] = v;
    /* A little integration will now filter away much of the HF noise */
    s->baud_phase -= p;
    v = labs(s->baud_phase);
#else
    /* Cross correlate */
    v = s->symbol_sync_low[1]*s->symbol_sync_high[0]*SYNC_LOW_BAND_EDGE_COEFF_2
      - s->symbol_sync_low[0]*s->symbol_sync_high[1]*SYNC_HIGH_BAND_EDGE_COEFF_2
      + s->symbol_sync_low[1]*s->symbol_sync_high[1]*SYNC_MIXED_EDGES_COEFF_3;
    /* Filter away any DC component */
    p = v - s->symbol_sync_dc_filter[1];
    s->symbol_sync_dc_filter[1] = s->symbol_sync_dc_filter[0];
    s->symbol_sync_dc_filter[0] = v;
    /* A little integration will now filter away much of the HF noise */
    s->baud_phase -= p;
    v = fabsf(s->baud_phase);
#endif
    if (v > FP_SYNC_SCALE_32(100.0f))
    {
        i = (v > FP_SYNC_SCALE_32(1000.0f))  ?  15  :  1;
        if (s->baud_phase < FP_SYNC_SCALE_32(0.0f))
            i = -i;
        //printf("v = %10.5f %5d - %f %f %d\n", v, i, p, s->baud_phase, s->total_baud_timing_correction);
        s->eq_put_step += i;
        s->total_baud_timing_correction += i;
    }
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINTx)
static void process_half_baud(v17_rx_state_t *s, const complexi16_t *sample)
#else
static void process_half_baud(v17_rx_state_t *s, const complexf_t *sample)
#endif
{
#if defined(SPANDSP_USE_FIXED_POINTx)
    static const complexi16_t cdba[4] =
#else
    static const complexf_t cdba[4] =
#endif
    {
        {FP_SCALE( 6.0f), FP_SCALE( 2.0f)},
        {FP_SCALE(-2.0f), FP_SCALE( 6.0f)},
        {FP_SCALE( 2.0f), FP_SCALE(-6.0f)},
        {FP_SCALE(-6.0f), FP_SCALE(-2.0f)}
    };
#if defined(SPANDSP_USE_FIXED_POINTx)
    uint16_t ip;
    complexi16_t z;
    complexi16_t z16;
    const complexi16_t *target;
    static const complexi16_t zero = {FP_SCALE(0.0f), FP_SCALE(0.0f)};
#else
    float p;
    complexf_t z;
    complexf_t zz;
    const complexf_t *target;
    static const complexf_t zero = {FP_SCALE(0.0f), FP_SCALE(0.0f)};
#endif
    int bit;
    int i;
    int j;
    uint32_t phase_step;
    int32_t angle;
    int32_t ang;
    int constellation_state;

    /* This routine processes every half a baud, as we put things into the equalizer at the T/2 rate. */

    /* Add a sample to the equalizer's circular buffer, but don't calculate anything at this time. */
    s->eq_buf[s->eq_step] = *sample;
    if (++s->eq_step >= V17_EQUALIZER_LEN)
        s->eq_step = 0;

    /* On alternate insertions we have a whole baud and must process it. */
    if ((s->baud_half ^= 1))
        return;

    /* Symbol timing synchronisation */
    symbol_sync(s);

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
        target = &zero;
        if (++s->training_count >= 100)
        {
            /* Record the current phase angle */
            s->training_stage = TRAINING_STAGE_LOG_PHASE;
            vec_zeroi32(s->diff_angles, 16);
            s->last_angles[0] = arctan2(z.im, z.re);
            if (s->agc_scaling_save == FP_SCALE(0.0f))
                s->agc_scaling_save = s->agc_scaling;
        }
        break;
    case TRAINING_STAGE_LOG_PHASE:
        /* Record the current alternate phase angle */
        target = &zero;
        angle = arctan2(z.im, z.re);
        s->training_count = 1;
        if (s->short_train)
        {
            /* We should already know the accurate carrier frequency. All we need to sort
               out is the phase. */
            /* Check if we just saw A or B */
            /* atan(1/3) = 18.433 degrees */
            if ((uint32_t) (angle - s->last_angles[0]) < (uint32_t) DDS_PHASE(180.0f))
            {
                angle = s->last_angles[0];
                s->last_angles[0] = DDS_PHASE(270.0f + 18.433f);
                s->last_angles[1] = DDS_PHASE(180.0f + 18.433f);
            }
            else
            {
                s->last_angles[0] = DDS_PHASE(180.0f + 18.433f);
                s->last_angles[1] = DDS_PHASE(270.0f + 18.433f);
            }
            /* Make a step shift in the phase, to pull it into line. We need to rotate the equalizer
               buffer, as well as the carrier phase, for this to play out nicely. */
            /* angle is now the difference between where A is, and where it should be */
            phase_step = angle - DDS_PHASE(180.0f + 18.433f);
#if defined(SPANDSP_USE_FIXED_POINTx)
            ip = phase_step >> 16;
            span_log(&s->logging, SPAN_LOG_FLOW, "Spin (short) by %d\n", ip);
            z16 = complex_seti16(fixed_cos(ip), -fixed_sin(ip));
            for (i = 0;  i < V17_EQUALIZER_LEN;  i++)
                s->eq_buf[i] = complex_mul_q1_15(&s->eq_buf[i], &z16);
            s->carrier_track_p = 500000;
#else
            p = dds_phase_to_radians(phase_step);
            span_log(&s->logging, SPAN_LOG_FLOW, "Spin (short) by %.5f rads\n", p);
            zz = complex_setf(cosf(p), -sinf(p));
            for (i = 0;  i < V17_EQUALIZER_LEN;  i++)
                s->eq_buf[i] = complex_mulf(&s->eq_buf[i], &zz);
            s->carrier_track_p = 500000.0f;
#endif
            s->carrier_phase += phase_step;
            s->training_stage = TRAINING_STAGE_SHORT_WAIT_FOR_CDBA;
        }
        else
        {
            s->last_angles[1] = angle;
            s->training_stage = TRAINING_STAGE_WAIT_FOR_CDBA;
        }
        break;
    case TRAINING_STAGE_WAIT_FOR_CDBA:
        target = &zero;
        angle = arctan2(z.im, z.re);
        /* Look for the initial ABAB sequence to display a phase reversal, which will
           signal the start of the scrambled CDBA segment */
        i = s->training_count + 1;
        ang = angle - s->last_angles[i & 1];
        s->last_angles[i & 1] = angle;
        s->diff_angles[i & 0xF] = s->diff_angles[(i - 2) & 0xF] + (ang >> 4);
        if ((ang > DDS_PHASE(90.0f)  ||  ang < DDS_PHASE(-90.0f))  &&  s->training_count >= 13)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "We seem to have a reversal at symbol %d\n", s->training_count);
            /* We seem to have a phase reversal */
            /* Slam the carrier frequency into line, based on the total phase drift over the last
               section. Use the shift from the odd bits and the shift from the even bits to get
               better jitter suppression. */
            /* Step back a few symbols so we don't get ISI distorting things. */
            i = (s->training_count - 8) & ~1;
            /* Avoid the possibility of a divide by zero */
            if (i > 1)
            {
                j = i & 0xF;
                ang = (s->diff_angles[j] + s->diff_angles[j | 0x1])/(i - 1);
                s->carrier_phase_rate += 3*16*(ang/20);
                span_log(&s->logging, SPAN_LOG_FLOW, "Angles %x, %x, dist %d\n", s->last_angles[0], s->last_angles[1], i);
            }
            span_log(&s->logging, SPAN_LOG_FLOW, "Coarse carrier frequency %7.2f (%d)\n", dds_frequencyf(s->carrier_phase_rate), s->training_count);
            /* Check if the carrier frequency is plausible */
            if (s->carrier_phase_rate < DDS_PHASE_RATE(CARRIER_NOMINAL_FREQ - 20.0f)
                ||
                s->carrier_phase_rate > DDS_PHASE_RATE(CARRIER_NOMINAL_FREQ + 20.0f))
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (sequence failed)\n");
                /* Park this modem */
                s->agc_scaling_save = FP_SCALE(0.0f);
                s->training_stage = TRAINING_STAGE_PARKED;
                report_status_change(s, SIG_STATUS_TRAINING_FAILED);
                break;
            }

            /* Make a step shift in the phase, to pull it into line. We need to rotate the equalizer buffer,
               as well as the carrier phase, for this to play out nicely. */
            /* angle is now the difference between where C is, and where it should be */
            phase_step = angle - DDS_PHASE(18.433f);
#if defined(SPANDSP_USE_FIXED_POINTx)
            ip = phase_step >> 16;
            span_log(&s->logging, SPAN_LOG_FLOW, "Spin (long) by %d\n", ip);
            z16 = complex_seti16(fixed_cos(ip), -fixed_sin(ip));
            for (i = 0;  i < V17_EQUALIZER_LEN;  i++)
                s->eq_buf[i] = complex_mul_q1_15(&s->eq_buf[i], &z16);
#else
            p = dds_phase_to_radians(phase_step);
            span_log(&s->logging, SPAN_LOG_FLOW, "Spin (long) by %.5f rads\n", p);
            zz = complex_setf(cosf(p), -sinf(p));
            for (i = 0;  i < V17_EQUALIZER_LEN;  i++)
                s->eq_buf[i] = complex_mulf(&s->eq_buf[i], &zz);
#endif
            s->carrier_phase += phase_step;

            /* We have just seen the first symbol of the scrambled sequence, so skip it. */
            bit = descramble(s, 1);
            bit = (bit << 1) | descramble(s, 1);
            target = &cdba[bit];
            s->training_count = 1;
            s->training_stage = TRAINING_STAGE_COARSE_TRAIN_ON_CDBA;
            report_status_change(s, SIG_STATUS_TRAINING_IN_PROGRESS);
            break;
        }
        if (++s->training_count > V17_TRAINING_SEG_1_LEN)
        {
            /* This is bogus. There are not this many bits in this section
               of a real training sequence. Note that this might be TEP. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (sequence failed)\n");
            /* Park this modem */
            s->agc_scaling_save = FP_SCALE(0.0f);
            s->training_stage = TRAINING_STAGE_PARKED;
            report_status_change(s, SIG_STATUS_TRAINING_FAILED);
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
#if defined(SPANDSP_USE_FIXED_POINTx)
        z16 = complex_subi16(&z, target);
        s->training_error += poweri16(&z16);
#else
        zz = complex_subf(&z, target);
        s->training_error = powerf(&zz);
#endif
        if (++s->training_count == V17_TRAINING_SEG_2_LEN - 2000  ||  s->training_error < FP_SCALE(1.0f)*FP_SCALE(1.0f)  ||  s->training_error > FP_SCALE(200.0f)*FP_SCALE(1.0f))
#else
        if (++s->training_count == V17_TRAINING_SEG_2_LEN - 2000)
#endif
        {
            /* Now the equaliser adaption should be getting somewhere, slow it down, or it will never
               tune very well on a noisy signal. */
            s->eq_delta = EQUALIZER_SLOW_ADAPTION_DELTA;
#if defined(SPANDSP_USE_FIXED_POINTx)
            s->carrier_track_i = 1000;
#else
            s->carrier_track_i = 1000.0f;
#endif
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
            s->training_error = FP_SCALE(0.0f);
#if defined(SPANDSP_USE_FIXED_POINTx)
            s->carrier_track_i = 100;
            s->carrier_track_p = 500000;
#else
            s->carrier_track_i = 100.0f;
            s->carrier_track_p = 500000.0f;
#endif
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
#if defined(SPANDSP_USE_FIXED_POINTx)
            z16 = complex_subi16(&z, target);
            s->training_error += poweri16(&z16);
#else
            zz = complex_subf(&z, target);
            s->training_error += powerf(&zz);
#endif
        }
        else if (s->training_count >= V17_TRAINING_SEG_2_LEN)
        {
#if defined(SPANDSP_USE_FIXED_POINTx)
            span_log(&s->logging, SPAN_LOG_FLOW, "Long training error %d\n", s->training_error);
#else
            span_log(&s->logging, SPAN_LOG_FLOW, "Long training error %f\n", s->training_error);
#endif
            if (s->training_error < FP_SCALE(20.0f)*FP_SCALE(1.414f)*constellation_spacing[s->space_map])
            {
                s->training_error = FP_SCALE(0.0f);
                s->training_count = 0;
                s->training_stage = TRAINING_STAGE_BRIDGE;
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (convergence failed)\n");
                /* Park this modem */
                s->agc_scaling_save = FP_SCALE(0.0f);
                s->training_stage = TRAINING_STAGE_PARKED;
                report_status_change(s, SIG_STATUS_TRAINING_FAILED);
            }
        }
        break;
    case TRAINING_STAGE_BRIDGE:
        descramble(s, V17_BRIDGE_WORD >> ((s->training_count & 0x7) << 1));
        descramble(s, V17_BRIDGE_WORD >> (((s->training_count & 0x7) << 1) + 1));
        target = &z;
        if (++s->training_count >= V17_TRAINING_SEG_3_LEN)
        {
            s->training_error = FP_SCALE(0.0f);
            s->training_count = 0;
            if (s->bits_per_symbol == 2)
            {
                /* Restart the differential decoder */
                /* There is no trellis, so go straight to processing decoded data */
                s->diff = (s->short_train)  ?  0  :  1;
                s->training_stage = TRAINING_STAGE_TEST_ONES;
            }
            else
            {
                /* Wait for the trellis to wind up */
                s->training_stage = TRAINING_STAGE_TCM_WINDUP;
            }
        }
        break;
    case TRAINING_STAGE_SHORT_WAIT_FOR_CDBA:
        /* Look for the initial ABAB sequence to display a phase reversal, which will
           signal the start of the scrambled CDBA segment */
        angle = arctan2(z.im, z.re);
        ang = angle - s->last_angles[s->training_count & 1];
        if (ang > DDS_PHASE(90.0f)  ||  ang < DDS_PHASE(-90.0f))
        {
            /* We seem to have a phase reversal */
            /* We have just seen the first symbol of the scrambled sequence, so skip it. */
            bit = descramble(s, 1);
            bit = (bit << 1) | descramble(s, 1);
            target = &cdba[bit];
            s->training_error = FP_SCALE(0.0f);
            s->training_count = 1;
            s->training_stage = TRAINING_STAGE_SHORT_TRAIN_ON_CDBA_AND_TEST;
            break;
        }
        target = &cdba[(s->training_count & 1) + 2];
        track_carrier(s, &z, target);
        if (++s->training_count > V17_TRAINING_SEG_1_LEN)
        {
            /* This is bogus. There are not this many bits in this section
               of a real training sequence. Note that this might be TEP. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (sequence failed)\n");
            /* Park this modem */
            s->training_stage = TRAINING_STAGE_PARKED;
            report_status_change(s, SIG_STATUS_TRAINING_FAILED);
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
#if defined(SPANDSP_USE_FIXED_POINTx)
            z16 = complex_subi16(&z, &cdba[bit]);
            s->training_error += poweri16(&z16);
#else
            zz = complex_subf(&z, &cdba[bit]);
            s->training_error += powerf(&zz);
#endif
        }
        if (++s->training_count >= V17_TRAINING_SHORT_SEG_2_LEN)
        {
#if defined(SPANDSP_USE_FIXED_POINTx)
            span_log(&s->logging, SPAN_LOG_FLOW, "Short training error %d\n", s->training_error);
            s->carrier_track_i = 100;
            s->carrier_track_p = 500000;
#else
            span_log(&s->logging, SPAN_LOG_FLOW, "Short training error %f\n", s->training_error);
            s->carrier_track_i = 100.0f;
            s->carrier_track_p = 500000.0f;
#endif
            /* TODO: This was increased by a factor of 10 after studying real world failures.
                     However, it is not clear why this is an improvement, If something gives
                     a huge training error, surely it shouldn't decode too well? */
            if (s->training_error < (V17_TRAINING_SHORT_SEG_2_LEN - 8)*FP_SCALE(4.0f)*FP_SCALE(1.0f)*constellation_spacing[s->space_map])
            {
                s->training_count = 0;
                if (s->bits_per_symbol == 2)
                {
                    /* There is no trellis, so go straight to processing decoded data */
                    /* Restart the differential decoder */
                    s->diff = (s->short_train)  ?  0  :  1;
                    s->training_error = FP_SCALE(0.0f);
                    s->training_stage = TRAINING_STAGE_TEST_ONES;
                }
                else
                {
                    /* Wait for the trellis to wind up */
                    s->training_stage = TRAINING_STAGE_TCM_WINDUP;
                }
                report_status_change(s, SIG_STATUS_TRAINING_IN_PROGRESS);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Short training failed (convergence failed)\n");
                /* Park this modem */
                s->training_stage = TRAINING_STAGE_PARKED;
                report_status_change(s, SIG_STATUS_TRAINING_FAILED);
            }
        }
        break;
    case TRAINING_STAGE_TCM_WINDUP:
        /* We need to wait 15 bauds while the trellis fills up. */
        //span_log(&s->logging, SPAN_LOG_FLOW, "%5d %15.5f, %15.5f\n", s->training_count, z.re, z.im);
        constellation_state = decode_baud(s, &z);
        target = &s->constellation[constellation_state];
        /* Measure the training error */
#if defined(SPANDSP_USE_FIXED_POINTx)
        z16 = complex_subi16(&z, target);
        s->training_error += poweri16(&z16);
#else
        zz = complex_subf(&z, target);
        s->training_error += powerf(&zz);
#endif
        if (++s->training_count >= V17_TRAINING_SEG_4A_LEN)
        {
            s->training_error = FP_SCALE(0.0f);
            s->training_count = 0;
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
#if defined(SPANDSP_USE_FIXED_POINTx)
        z16 = complex_subi16(&z, target);
        s->training_error += poweri16(&z16);
#else
        zz = complex_subf(&z, target);
        s->training_error += powerf(&zz);
#endif
        if (++s->training_count >= V17_TRAINING_SEG_4_LEN)
        {
            if (s->training_error < V17_TRAINING_SEG_4_LEN*FP_SCALE(1.0f)*FP_SCALE(1.0f)*constellation_spacing[s->space_map])
            {
#if defined(SPANDSP_USE_FIXED_POINTx)
                span_log(&s->logging, SPAN_LOG_FLOW, "Training succeeded at %dbps (constellation mismatch %d)\n", s->bit_rate, s->training_error);
#else
                span_log(&s->logging, SPAN_LOG_FLOW, "Training succeeded at %dbps (constellation mismatch %f)\n", s->bit_rate, s->training_error);
#endif
                /* We are up and running */
                report_status_change(s, SIG_STATUS_TRAINING_SUCCEEDED);
                /* Apply some lag to the carrier off condition, to ensure the last few bits get pushed through
                   the processing. */
                s->signal_present = 60;
                equalizer_save(s);
                s->carrier_phase_rate_save = s->carrier_phase_rate;
                s->short_train = true;
                s->training_stage = TRAINING_STAGE_NORMAL_OPERATION;
            }
            else
            {
                /* Training has failed. Park this modem. */
#if defined(SPANDSP_USE_FIXED_POINTx)
                span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (constellation mismatch %d)\n", s->training_error);
#else
                span_log(&s->logging, SPAN_LOG_FLOW, "Training failed (constellation mismatch %f)\n", s->training_error);
#endif
                if (!s->short_train)
                    s->agc_scaling_save = FP_SCALE(0.0f);
                s->training_stage = TRAINING_STAGE_PARKED;
                report_status_change(s, SIG_STATUS_TRAINING_FAILED);
            }
        }
        break;
    case TRAINING_STAGE_PARKED:
    default:
        /* We failed to train! */
        /* Park here until the carrier drops. */
        target = &zero;
        break;
    }
    if (s->qam_report)
    {
#if defined(SPANDSP_USE_FIXED_POINT)
        complexi16_t zi;
        complexi16_t targeti;

        zi.re = z.re*1024.0f;
        zi.im = z.im*1024.0f;
        targeti.re = target->re*1024.0f;
        targeti.im = target->im*1024.0f;
        s->qam_report(s->qam_user_data, &zi, &targeti, constellation_state);
#else
        s->qam_report(s->qam_user_data, &z, target, constellation_state);
#endif
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ int signal_detect(v17_rx_state_t *s, int16_t amp)
{
    int16_t diff;
    int16_t x;
    int32_t power;

    /* There should be no DC in the signal, but sometimes there is.
       We need to measure the power with the DC blocked, but not using
       a slow to respond DC blocker. Use the most elementary HPF. */
    x = amp >> 1;
    /* There could be overflow here, but it isn't a problem in practice */
    diff = x - s->last_sample;
    s->last_sample = x;
    power = power_meter_update(&s->power, diff);
#if defined(IAXMODEM_STUFF)
    /* Quick power drop fudge */
    diff = abs(diff);
    if (10*diff < s->high_sample)
    {
        if (++s->low_samples > 120)
        {
            power_meter_init(&s->power, 4);
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
    if (s->signal_present > 0)
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
                v17_rx_restart(s, s->bit_rate, s->short_train);
                report_status_change(s, SIG_STATUS_CARRIER_DOWN);
                return 0;
            }
#if defined(IAXMODEM_STUFF)
            /* Carrier has dropped, but the put_bit is pending the signal_present delay. */
            s->carrier_drop_pending = true;
#endif
        }
    }
    else
    {
        /* Look for power exceeding turn-on threshold to turn the carrier on */
        if (power < s->carrier_on_power)
            return 0;
        s->signal_present = 1;
#if defined(IAXMODEM_STUFF)
        s->carrier_drop_pending = false;
#endif
        report_status_change(s, SIG_STATUS_CARRIER_UP);
    }
    return power;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) v17_rx(v17_rx_state_t *s, const int16_t amp[], int len)
{
    int i;
    int step;
#if defined(SPANDSP_USE_FIXED_POINTx)
    complexi16_t z;
    complexi16_t zz;
    complexi16_t sample;
    int32_t v;
#else
    complexf_t z;
    complexf_t zz;
    complexf_t sample;
    float v;
#endif
    int32_t root_power;
    int32_t power;

    for (i = 0;  i < len;  i++)
    {
        s->rrc_filter[s->rrc_filter_step] = amp[i];
        if (++s->rrc_filter_step >= V17_RX_FILTER_STEPS)
            s->rrc_filter_step = 0;

        if ((power = signal_detect(s, amp[i])) == 0)
            continue;
        if (s->training_stage == TRAINING_STAGE_PARKED)
            continue;
        /* Only spend effort processing this data if the modem is not
           parked, after training failure. */
        s->eq_put_step -= RX_PULSESHAPER_COEFF_SETS;
        step = -s->eq_put_step;
        if (step < 0)
            step += RX_PULSESHAPER_COEFF_SETS;
        if (step < 0)
            step = 0;
        else if (step > RX_PULSESHAPER_COEFF_SETS - 1)
            step = RX_PULSESHAPER_COEFF_SETS - 1;
#if defined(SPANDSP_USE_FIXED_POINTx)
        v = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_re[step], V17_RX_FILTER_STEPS, s->rrc_filter_step) >> 15;
        sample.re = (v*s->agc_scaling) >> 10;
        /* Symbol timing synchronisation band edge filters */
        /* Low Nyquist band edge filter */
        v = ((s->symbol_sync_low[0]*SYNC_LOW_BAND_EDGE_COEFF_0) >> FP_SYNC_SHIFT_FACTOR)
          + ((s->symbol_sync_low[1]*SYNC_LOW_BAND_EDGE_COEFF_1) >> FP_SYNC_SHIFT_FACTOR)
          + sample.re;
        s->symbol_sync_low[1] = s->symbol_sync_low[0];
        s->symbol_sync_low[0] = v;
        /* High Nyquist band edge filter */
        v = ((s->symbol_sync_high[0]*SYNC_HIGH_BAND_EDGE_COEFF_0) >> FP_SYNC_SHIFT_FACTOR)
          + ((s->symbol_sync_high[1]*SYNC_HIGH_BAND_EDGE_COEFF_1) >> FP_SYNC_SHIFT_FACTOR)
          + sample.re;
        s->symbol_sync_high[1] = s->symbol_sync_high[0];
        s->symbol_sync_high[0] = v;
#else
        v = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_re[step], V17_RX_FILTER_STEPS, s->rrc_filter_step);
        sample.re = v*s->agc_scaling;
        /* Symbol timing synchronisation band edge filters */
        /* Low Nyquist band edge filter */
        v = s->symbol_sync_low[0]*SYNC_LOW_BAND_EDGE_COEFF_0 + s->symbol_sync_low[1]*SYNC_LOW_BAND_EDGE_COEFF_1 + sample.re;
        s->symbol_sync_low[1] = s->symbol_sync_low[0];
        s->symbol_sync_low[0] = v;
        /* High Nyquist band edge filter */
        v = s->symbol_sync_high[0]*SYNC_HIGH_BAND_EDGE_COEFF_0 + s->symbol_sync_high[1]*SYNC_HIGH_BAND_EDGE_COEFF_1 + sample.re;
        s->symbol_sync_high[1] = s->symbol_sync_high[0];
        s->symbol_sync_high[0] = v;
#endif
        /* Put things into the equalization buffer at T/2 rate. The symbol synchronisation
           will fiddle the step to align this with the symbols. */
        if (s->eq_put_step <= 0)
        {
            /* Only AGC until we have locked down the setting. */
            if (s->agc_scaling_save == FP_SCALE(0.0f))
            {
                if ((root_power = fixed_sqrt32(power)) == 0)
                    root_power = 1;
#if defined(SPANDSP_USE_FIXED_POINTx)
                s->agc_scaling = saturate16(((int32_t) (FP_SCALE(2.17f)*1024.0f))/root_power);
#else
                s->agc_scaling = (FP_SCALE(2.17f)/RX_PULSESHAPER_GAIN)/root_power;
#endif
            }
            /* Pulse shape while still at the carrier frequency, using a quadrature
               pair of filters. This results in a properly bandpass filtered complex
               signal, which can be brought directly to baseband by complex mixing.
               No further filtering, to remove mixer harmonics, is needed. */
#if defined(SPANDSP_USE_FIXED_POINTx)
            v = vec_circular_dot_prodi16(s->rrc_filter, rx_pulseshaper_im[step], V17_RX_FILTER_STEPS, s->rrc_filter_step) >> 15;
            sample.im = (v*s->agc_scaling) >> 10;
            z = dds_lookup_complexi16(s->carrier_phase);
            zz.re = ((int32_t) sample.re*z.re - (int32_t) sample.im*z.im) >> 15;
            zz.im = ((int32_t) -sample.re*z.im - (int32_t) sample.im*z.re) >> 15;
#else
            v = vec_circular_dot_prodf(s->rrc_filter, rx_pulseshaper_im[step], V17_RX_FILTER_STEPS, s->rrc_filter_step);
            sample.im = v*s->agc_scaling;
            z = dds_lookup_complexf(s->carrier_phase);
            zz.re = sample.re*z.re - sample.im*z.im;
            zz.im = -sample.re*z.im - sample.im*z.re;
#endif
            s->eq_put_step += RX_PULSESHAPER_COEFF_SETS*10/(3*2);
            process_half_baud(s, &zz);
        }
#if defined(SPANDSP_USE_FIXED_POINT)
        dds_advance(&s->carrier_phase, s->carrier_phase_rate);
#else
        dds_advancef(&s->carrier_phase, s->carrier_phase_rate);
#endif
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) v17_rx_fillin(v17_rx_state_t *s, int len)
{
    int i;

    /* We want to sustain the current state (i.e carrier on<->carrier off), and
       try to sustain the carrier phase. We should probably push the filters, as well */
    span_log(&s->logging, SPAN_LOG_FLOW, "Fill-in %d samples\n", len);
    if (s->signal_present <= 0)
        return 0;
    if (s->training_stage == TRAINING_STAGE_PARKED)
        return 0;
    for (i = 0;  i < len;  i++)
    {
#if defined(SPANDSP_USE_FIXED_POINT)
        dds_advance(&s->carrier_phase, s->carrier_phase_rate);
#else
        dds_advancef(&s->carrier_phase, s->carrier_phase_rate);
#endif
        /* Advance the symbol phase the appropriate amount */
        s->eq_put_step -= RX_PULSESHAPER_COEFF_SETS;
        if (s->eq_put_step <= 0)
            s->eq_put_step += RX_PULSESHAPER_COEFF_SETS*10/(3*2);
        /* TODO: Should we rotate any buffers */
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v17_rx_set_put_bit(v17_rx_state_t *s, put_bit_func_t put_bit, void *user_data)
{
    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v17_rx_set_modem_status_handler(v17_rx_state_t *s, modem_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v17_rx_get_logging_state(v17_rx_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v17_rx_restart(v17_rx_state_t *s, int bit_rate, int short_train)
{
    int i;

    span_log(&s->logging, SPAN_LOG_FLOW, "Restarting V.17, %dbps, %s training\n", bit_rate, (short_train)  ?  "short"  :  "long");
    switch (bit_rate)
    {
    case 14400:
        s->constellation = v17_v32bis_14400_constellation;
        s->space_map = 0;
        s->bits_per_symbol = 6;
        break;
    case 12000:
        s->constellation = v17_v32bis_12000_constellation;
        s->space_map = 1;
        s->bits_per_symbol = 5;
        break;
    case 9600:
        s->constellation = v17_v32bis_9600_constellation;
        s->space_map = 2;
        s->bits_per_symbol = 4;
        break;
    case 7200:
        s->constellation = v17_v32bis_7200_constellation;
        s->space_map = 3;
        s->bits_per_symbol = 3;
        break;
    case 4800:
        /* This does not exist in the V.17 spec as a valid mode of operation.
           However, it does exist in V.32bis, so it is here for completeness. */
        s->constellation = v17_v32bis_4800_constellation;
        s->space_map = 0;
        s->bits_per_symbol = 2;
        break;
    default:
        return -1;
    }
    s->bit_rate = bit_rate;
#if defined(SPANDSP_USE_FIXED_POINTx)
    vec_zeroi16(s->rrc_filter, sizeof(s->rrc_filter)/sizeof(s->rrc_filter[0]));
#else
    vec_zerof(s->rrc_filter, sizeof(s->rrc_filter)/sizeof(s->rrc_filter[0]));
#endif
    s->training_error = FP_SCALE(0.0f);
    s->rrc_filter_step = 0;

    s->diff = 1;
    s->scramble_reg = 0x2ECDD5;
    s->training_stage = TRAINING_STAGE_SYMBOL_ACQUISITION;
    s->training_count = 0;
    s->signal_present = 0;
#if defined(IAXMODEM_STUFF)
    s->high_sample = 0;
    s->low_samples = 0;
    s->carrier_drop_pending = false;
#endif
    if (short_train != 2)
        s->short_train = short_train;
    vec_zeroi32(s->last_angles, 2);
    vec_zeroi32(s->diff_angles, 16);

    /* Initialise the TCM decoder parameters. */
    /* The accumulated distance vectors are set so state zero starts
       at a value of zero, and all others start larger. This forces the
       initial paths to merge at the zero states. */
    for (i = 0;  i < 8;  i++)
        s->distances[i] = FP_CONSTELLATION_SCALE(99.0f)*FP_CONSTELLATION_SCALE(1.0f);
    memset(s->full_path_to_past_state_locations, 0, sizeof(s->full_path_to_past_state_locations));
    memset(s->past_state_locations, 0, sizeof(s->past_state_locations));
    s->distances[0] = 0;
    s->trellis_ptr = 14;

    s->carrier_phase = 0;
    power_meter_init(&s->power, 4);

    if (s->short_train)
    {
        s->carrier_phase_rate = s->carrier_phase_rate_save;
        equalizer_restore(s);
        s->agc_scaling = s->agc_scaling_save;
        /* Don't allow any frequency correction at all, until we start to pull the phase in. */
#if defined(SPANDSP_USE_FIXED_POINTx)
        s->carrier_track_i = 0;
        s->carrier_track_p = 40000;
#else
        s->carrier_track_i = 0.0f;
        s->carrier_track_p = 40000.0f;
#endif
    }
    else
    {
        s->carrier_phase_rate = DDS_PHASE_RATE(CARRIER_NOMINAL_FREQ);
        equalizer_reset(s);
        s->agc_scaling_save = FP_SCALE(0.0f);
#if defined(SPANDSP_USE_FIXED_POINTx)
        s->agc_scaling = (float) (FP_SCALE(2.17f)*1024.0f)/735.0f;
        s->carrier_track_i = 5000;
        s->carrier_track_p = 40000;
#else
        s->agc_scaling = (FP_SCALE(2.17f)/RX_PULSESHAPER_GAIN)/735.0f;
        s->carrier_track_i = 5000.0f;
        s->carrier_track_p = 40000.0f;
#endif
    }
    s->last_sample = 0;
    span_log(&s->logging, SPAN_LOG_FLOW, "Gains %f %f\n", (float) s->agc_scaling_save, (float) s->agc_scaling);
    span_log(&s->logging, SPAN_LOG_FLOW, "Phase rates %f %f\n", dds_frequencyf(s->carrier_phase_rate), dds_frequencyf(s->carrier_phase_rate_save));

    /* Initialise the working data for symbol timing synchronisation */
    for (i = 0;  i < 2;  i++)
    {
        s->symbol_sync_low[i] = FP_SCALE(0.0f);
        s->symbol_sync_high[i] = FP_SCALE(0.0f);
        s->symbol_sync_dc_filter[i] = FP_SCALE(0.0f);
    }
    s->baud_phase = FP_SCALE(0.0f);
    s->baud_half = 0;

    s->total_baud_timing_correction = 0;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v17_rx_state_t *) v17_rx_init(v17_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data)
{
    switch (bit_rate)
    {
    case 14400:
    case 12000:
    case 9600:
    case 7200:
    case 4800:
        /* 4800 is an extension of V.17, to provide full coverage of the V.32bis modes */
        break;
    default:
        return NULL;
    }
    if (s == NULL)
    {
        if ((s = (v17_rx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.17 RX");
    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
    s->short_train = false;
    s->scrambler_tap = 18 - 1;
    v17_rx_signal_cutoff(s, -45.5f);
    s->carrier_phase_rate_save = DDS_PHASE_RATE(CARRIER_NOMINAL_FREQ);
    v17_rx_restart(s, bit_rate, s->short_train);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v17_rx_release(v17_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v17_rx_free(v17_rx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v17_rx_set_qam_report_handler(v17_rx_state_t *s, qam_report_handler_t handler, void *user_data)
{
    s->qam_report = handler;
    s->qam_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
