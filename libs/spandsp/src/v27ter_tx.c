/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v27ter_tx.c - ITU V.27ter modem transmit part
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
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
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
#include "spandsp/fast_convert.h"
#include "spandsp/logging.h"
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/vector_int.h"
#include "spandsp/complex_vector_int.h"
#include "spandsp/async.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"

#include "spandsp/v27ter_tx.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/v27ter_tx.h"

#if defined(SPANDSP_USE_FIXED_POINT)
#define FP_SCALE(x)                     FP_Q6_10(x)
#else
#define FP_SCALE(x)                     (x)
#endif

#include "v27ter_tx_4800_rrc.h"
#include "v27ter_tx_2400_rrc.h"

/*! The nominal frequency of the carrier, in Hertz */
#define CARRIER_NOMINAL_FREQ            1800.0f

/* Segments of the training sequence */
/* V.27ter defines a long and a short sequence. FAX doesn't use the
   short sequence, so it is not implemented here. */
/*! The start of training segment 1, in symbols */
#define V27TER_TRAINING_SEG_1           0
/*! The start of training segment 2, in symbols */
#define V27TER_TRAINING_SEG_2           (V27TER_TRAINING_SEG_1 + 320)
/*! The start of training segment 3, in symbols */
#define V27TER_TRAINING_SEG_3           (V27TER_TRAINING_SEG_2 + 32)
/*! The start of training segment 4, in symbols */
#define V27TER_TRAINING_SEG_4           (V27TER_TRAINING_SEG_3 + 50)
/*! The start of training segment 5, in symbols */
#define V27TER_TRAINING_SEG_5           (V27TER_TRAINING_SEG_4 + 1074)
/*! The end of the training, in symbols */
#define V27TER_TRAINING_END             (V27TER_TRAINING_SEG_5 + 8)
/*! The end of the shutdown sequence, in symbols */
#define V27TER_TRAINING_SHUTDOWN_END    (V27TER_TRAINING_END + 32)

static int fake_get_bit(void *user_data)
{
    return 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int scramble(v27ter_tx_state_t *s, int in_bit)
{
    int out_bit;

    /* This scrambler is really quite messy to implement. There seems to be no efficient shortcut */
    out_bit = (in_bit ^ (s->scramble_reg >> 5) ^ (s->scramble_reg >> 6)) & 1;
    if (s->scrambler_pattern_count >= 33)
    {
        out_bit ^= 1;
        s->scrambler_pattern_count = 0;
    }
    else
    {
        if ((((s->scramble_reg >> 7) ^ out_bit) & ((s->scramble_reg >> 8) ^ out_bit) & ((s->scramble_reg >> 11) ^ out_bit) & 1))
            s->scrambler_pattern_count = 0;
        else
            s->scrambler_pattern_count++;
    }
    s->scramble_reg = (s->scramble_reg << 1) | out_bit;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int get_scrambled_bit(v27ter_tx_state_t *s)
{
    int bit;

    if ((bit = s->current_get_bit(s->get_bit_user_data)) == SIG_STATUS_END_OF_DATA)
    {
        /* End of real data. Switch to the fake get_bit routine, until we
           have shut down completely. */
        if (s->status_handler)
            s->status_handler(s->status_user_data, SIG_STATUS_END_OF_DATA);
        s->current_get_bit = fake_get_bit;
        s->in_training = true;
        bit = 1;
    }
    return scramble(s, bit);
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINT)
static complexi16_t getbaud(v27ter_tx_state_t *s)
#else
static complexf_t getbaud(v27ter_tx_state_t *s)
#endif
{
    static const int phase_steps_4800[8] =
    {
        1, 0, 2, 3, 6, 7, 5, 4
    };
    static const int phase_steps_2400[4] =
    {
        0, 2, 6, 4
    };
#if defined(SPANDSP_USE_FIXED_POINT)
    static const complexi16_t zero = {0, 0};
    static const complexi16_t constellation[8] =
#else
    static const complexf_t zero = {0.0f, 0.0f};
    static const complexf_t constellation[8] =
#endif
    {
        {FP_SCALE( 1.414f), FP_SCALE( 0.0f)},       /*   0deg */
        {FP_SCALE( 1.0f),   FP_SCALE( 1.0f)},       /*  45deg */
        {FP_SCALE( 0.0f),   FP_SCALE( 1.414f)},     /*  90deg */
        {FP_SCALE(-1.0f),   FP_SCALE( 1.0f)},       /* 135deg */
        {FP_SCALE(-1.414f), FP_SCALE( 0.0f)},       /* 180deg */
        {FP_SCALE(-1.0f),   FP_SCALE(-1.0f)},       /* 225deg */
        {FP_SCALE( 0.0f),   FP_SCALE(-1.414f)},     /* 270deg */
        {FP_SCALE( 1.0f),   FP_SCALE(-1.0f)}        /* 315deg */
    };
    int bits;

    if (s->in_training)
    {
        /* Send the training sequence */
        if (++s->training_step <= V27TER_TRAINING_SEG_5)
        {
            if (s->training_step <= V27TER_TRAINING_SEG_4)
            {
                if (s->training_step <= V27TER_TRAINING_SEG_2)
                {
                    /* Segment 1: Unmodulated carrier (talker echo protection) */
                    return constellation[0];
                }
                if (s->training_step <= V27TER_TRAINING_SEG_3)
                {
                    /* Segment 2: Silence */
                    return zero;
                }
                /* Segment 3: Regular reversals... */
                s->constellation_state = (s->constellation_state + 4) & 7;
                return constellation[s->constellation_state];
            }
            /* Segment 4: Scrambled reversals... */
            /* Apply the 1 + x^-6 + x^-7 scrambler. We want every third
               bit from the scrambler. */
            bits = get_scrambled_bit(s) << 2;
            get_scrambled_bit(s);
            get_scrambled_bit(s);
            s->constellation_state = (s->constellation_state + bits) & 7;
            return constellation[s->constellation_state];
        }
        /* We should be in the block of test ones, or shutdown ones, if we get here. */
        /* There is no graceful shutdown procedure defined for V.27ter. Just
           send some ones, to ensure we get the real data bits through, even
           with bad ISI. */
        if (s->training_step == V27TER_TRAINING_END + 1)
        {
            /* End of the last segment - segment 5: All ones */
            /* Switch from the fake get_bit routine, to the user supplied real
               one, and we are up and running. */
            s->current_get_bit = s->get_bit;
            s->in_training = false;
        }
        if (s->training_step == V27TER_TRAINING_SHUTDOWN_END)
        {
            if (s->status_handler)
                s->status_handler(s->status_user_data, SIG_STATUS_SHUTDOWN_COMPLETE);
        }
    }
    /* 4800bps uses 8 phases. 2400bps uses 4 phases. */
    if (s->bit_rate == 4800)
    {
        bits = get_scrambled_bit(s);
        bits = (bits << 1) | get_scrambled_bit(s);
        bits = (bits << 1) | get_scrambled_bit(s);
        bits = phase_steps_4800[bits];
    }
    else
    {
        bits = get_scrambled_bit(s);
        bits = (bits << 1) | get_scrambled_bit(s);
        bits = phase_steps_2400[bits];
    }
    s->constellation_state = (s->constellation_state + bits) & 7;
    return constellation[s->constellation_state];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v27ter_tx(v27ter_tx_state_t *s, int16_t amp[], int len)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t v;
    complexi32_t x;
    complexi32_t z;
    int16_t iamp;
#else
    complexf_t v;
    complexf_t x;
    complexf_t z;
    float famp;
#endif
    int sample;

    if (s->training_step >= V27TER_TRAINING_SHUTDOWN_END)
    {
        /* Once we have sent the shutdown symbols, we stop sending completely. */
        return 0;
    }
    /* The symbol rates for the two bit rates are different. This makes it difficult to
       merge both generation procedures into a single efficient loop. We do not bother
       trying. We use two independent loops, filter coefficients, etc. */
    if (s->bit_rate == 4800)
    {
        for (sample = 0;  sample < len;  sample++)
        {
            if (++s->baud_phase >= 5)
            {
                s->baud_phase -= 5;
                v = getbaud(s);;
                s->rrc_filter_re[s->rrc_filter_step] = v.re;
                s->rrc_filter_im[s->rrc_filter_step] = v.im;
                if (++s->rrc_filter_step >= V27TER_TX_FILTER_STEPS)
                    s->rrc_filter_step = 0;
            }
#if defined(SPANDSP_USE_FIXED_POINT)
            /* Root raised cosine pulse shaping at baseband */
            x.re = vec_circular_dot_prodi16(s->rrc_filter_re, tx_pulseshaper_4800[TX_PULSESHAPER_4800_COEFF_SETS - 1 - s->baud_phase], V27TER_TX_FILTER_STEPS, s->rrc_filter_step) >> (10 + 4);
            x.im = vec_circular_dot_prodi16(s->rrc_filter_im, tx_pulseshaper_4800[TX_PULSESHAPER_4800_COEFF_SETS - 1 - s->baud_phase], V27TER_TX_FILTER_STEPS, s->rrc_filter_step) >> (10 + 4);
            /* Now create and modulate the carrier */
            z = dds_complexi32(&s->carrier_phase, s->carrier_phase_rate);
            iamp = ((int32_t) x.re*z.re - x.im*z.im) >> 15;
            /* Don't bother saturating. We should never clip. */
            amp[sample] = (int16_t) (((int32_t) iamp*s->gain_4800) >> 11);
#else
            /* Root raised cosine pulse shaping at baseband */
            x.re = vec_circular_dot_prodf(s->rrc_filter_re, tx_pulseshaper_4800[TX_PULSESHAPER_4800_COEFF_SETS - 1 - s->baud_phase], V27TER_TX_FILTER_STEPS, s->rrc_filter_step);
            x.im = vec_circular_dot_prodf(s->rrc_filter_im, tx_pulseshaper_4800[TX_PULSESHAPER_4800_COEFF_SETS - 1 - s->baud_phase], V27TER_TX_FILTER_STEPS, s->rrc_filter_step);
            /* Now create and modulate the carrier */
            z = dds_complexf(&s->carrier_phase, s->carrier_phase_rate);
            famp = x.re*z.re - x.im*z.im;
            /* Don't bother saturating. We should never clip. */
            amp[sample] = (int16_t) lfastrintf(famp*s->gain_4800);
#endif
        }
    }
    else
    {
        for (sample = 0;  sample < len;  sample++)
        {
            if ((s->baud_phase += 3) >= 20)
            {
                s->baud_phase -= 20;
                v = getbaud(s);
                s->rrc_filter_re[s->rrc_filter_step] = v.re;
                s->rrc_filter_im[s->rrc_filter_step] = v.im;
                if (++s->rrc_filter_step >= V27TER_TX_FILTER_STEPS)
                    s->rrc_filter_step = 0;
            }
#if defined(SPANDSP_USE_FIXED_POINT)
            /* Root raised cosine pulse shaping at baseband */
            x.re = vec_circular_dot_prodi16(s->rrc_filter_re, tx_pulseshaper_2400[TX_PULSESHAPER_2400_COEFF_SETS - 1 - s->baud_phase], V27TER_TX_FILTER_STEPS, s->rrc_filter_step) >> (10 + 4);
            x.im = vec_circular_dot_prodi16(s->rrc_filter_im, tx_pulseshaper_2400[TX_PULSESHAPER_2400_COEFF_SETS - 1 - s->baud_phase], V27TER_TX_FILTER_STEPS, s->rrc_filter_step) >> (10 + 4);
            /* Now create and modulate the carrier */
            z = dds_complexi32(&s->carrier_phase, s->carrier_phase_rate);
            iamp = ((int32_t) x.re*z.re - x.im*z.im) >> 15;
            /* Don't bother saturating. We should never clip. */
            amp[sample] = (int16_t) (((int32_t) iamp*s->gain_2400) >> 11);
#else
            /* Root raised cosine pulse shaping at baseband */
            x.re = vec_circular_dot_prodf(s->rrc_filter_re, tx_pulseshaper_2400[TX_PULSESHAPER_2400_COEFF_SETS - 1 - s->baud_phase], V27TER_TX_FILTER_STEPS, s->rrc_filter_step);
            x.im = vec_circular_dot_prodf(s->rrc_filter_im, tx_pulseshaper_2400[TX_PULSESHAPER_2400_COEFF_SETS - 1 - s->baud_phase], V27TER_TX_FILTER_STEPS, s->rrc_filter_step);
            /* Now create and modulate the carrier */
            z = dds_complexf(&s->carrier_phase, s->carrier_phase_rate);
            famp = x.re*z.re - x.im*z.im;
            /* Don't bother saturating. We should never clip. */
            amp[sample] = (int16_t) lfastrintf(famp*s->gain_2400);
#endif
        }
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v27ter_tx_power(v27ter_tx_state_t *s, float power)
{
    float gain;

    gain = powf(10.0f, (power - DBM0_MAX_POWER)/20.0f)*32768.0f;
#if defined(SPANDSP_USE_FIXED_POINT)
    s->gain_2400 = (int16_t) (gain/TX_PULSESHAPER_2400_GAIN);
    s->gain_4800 = (int16_t) (gain/TX_PULSESHAPER_4800_GAIN);
#else
    s->gain_2400 = gain/TX_PULSESHAPER_2400_GAIN;
    s->gain_4800 = gain/TX_PULSESHAPER_4800_GAIN;
#endif
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v27ter_tx_set_get_bit(v27ter_tx_state_t *s, get_bit_func_t get_bit, void *user_data)
{
    if (s->get_bit == s->current_get_bit)
        s->current_get_bit = get_bit;
    s->get_bit = get_bit;
    s->get_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v27ter_tx_set_modem_status_handler(v27ter_tx_state_t *s, modem_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v27ter_tx_get_logging_state(v27ter_tx_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v27ter_tx_restart(v27ter_tx_state_t *s, int bit_rate, bool tep)
{
    if (bit_rate != 4800  &&  bit_rate != 2400)
        return -1;
    s->bit_rate = bit_rate;
#if defined(SPANDSP_USE_FIXED_POINT)
    vec_zeroi16(s->rrc_filter_re, sizeof(s->rrc_filter_re)/sizeof(s->rrc_filter_re[0]));
    vec_zeroi16(s->rrc_filter_im, sizeof(s->rrc_filter_im)/sizeof(s->rrc_filter_im[0]));
#else
    vec_zerof(s->rrc_filter_re, sizeof(s->rrc_filter_re)/sizeof(s->rrc_filter_re[0]));
    vec_zerof(s->rrc_filter_im, sizeof(s->rrc_filter_im)/sizeof(s->rrc_filter_im[0]));
#endif
    s->rrc_filter_step = 0;
    s->scramble_reg = 0x3C;
    s->scrambler_pattern_count = 0;
    s->in_training = true;
    s->training_step = (tep)  ?  V27TER_TRAINING_SEG_1  :  V27TER_TRAINING_SEG_2;
    s->carrier_phase = 0;
    s->baud_phase = 0;
    s->constellation_state = 0;
    s->current_get_bit = fake_get_bit;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v27ter_tx_state_t *) v27ter_tx_init(v27ter_tx_state_t *s, int bit_rate, bool tep, get_bit_func_t get_bit, void *user_data)
{
    switch (bit_rate)
    {
    case 4800:
    case 2400:
        break;
    default:
        return NULL;
    }
    if (s == NULL)
    {
        if ((s = (v27ter_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.27ter TX");
    s->get_bit = get_bit;
    s->get_bit_user_data = user_data;
    s->carrier_phase_rate = dds_phase_ratef(CARRIER_NOMINAL_FREQ);
    v27ter_tx_power(s, -14.0f);
    v27ter_tx_restart(s, bit_rate, tep);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v27ter_tx_release(v27ter_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v27ter_tx_free(v27ter_tx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
