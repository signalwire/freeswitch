/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v17tx.c - ITU V.17 modem transmit part
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

#if defined(SPANDSP_USE_FIXED_POINT)
#define SPANDSP_USE_FIXED_POINTx
#endif

#include "spandsp/v17tx.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/v17tx.h"

#if defined(SPANDSP_USE_FIXED_POINT)
#define FP_SCALE(x)                     ((int16_t) x)
#else
#define FP_SCALE(x)                     (x)
#endif

#define FP_CONSTELLATION_SCALE(x)       FP_SCALE(x)

#include "v17_v32bis_tx_constellation_maps.h"
#include "v17_v32bis_tx_rrc.h"

/*! The nominal frequency of the carrier, in Hertz */
#define CARRIER_NOMINAL_FREQ            1800.0f

/* Segments of the training sequence */
/*! The start of the optional TEP, that may preceed the actual training, in symbols */
#define V17_TRAINING_SEG_TEP_A          0
/*! The mid point of the optional TEP, that may preceed the actual training, in symbols */
#define V17_TRAINING_SEG_TEP_B          (V17_TRAINING_SEG_TEP_A + 480)
/*! The start of training segment 1, in symbols */
#define V17_TRAINING_SEG_1              (V17_TRAINING_SEG_TEP_B + 48)
/*! The start of training segment 2, in symbols */
#define V17_TRAINING_SEG_2              (V17_TRAINING_SEG_1 + 256)
/*! The start of training segment 3, in symbols */
#define V17_TRAINING_SEG_3              (V17_TRAINING_SEG_2 + 2976)
/*! The start of training segment 4, in symbols */
#define V17_TRAINING_SEG_4              (V17_TRAINING_SEG_3 + 64)
/*! The start of training segment 4 in short training mode, in symbols */
#define V17_TRAINING_SHORT_SEG_4        (V17_TRAINING_SEG_2 + 38)
/*! The end of the training, in symbols */
#define V17_TRAINING_END                (V17_TRAINING_SEG_4 + 48)
#define V17_TRAINING_SHUTDOWN_A         (V17_TRAINING_END + 32)
/*! The end of the shutdown sequence, in symbols */
#define V17_TRAINING_SHUTDOWN_END       (V17_TRAINING_SHUTDOWN_A + 48)

/*! The 16 bit pattern used in the bridge section of the training sequence */
#define V17_BRIDGE_WORD                 0x8880

static __inline__ int scramble(v17_tx_state_t *s, int in_bit)
{
    int out_bit;

    out_bit = (in_bit ^ (s->scramble_reg >> s->scrambler_tap) ^ (s->scramble_reg >> (23 - 1))) & 1;
    s->scramble_reg = (s->scramble_reg << 1) | out_bit;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINT)
static __inline__ complexi16_t training_get(v17_tx_state_t *s)
#else
static __inline__ complexf_t training_get(v17_tx_state_t *s)
#endif
{
    static const int cdba_to_abcd[4] =
    {
        2, 3, 1, 0
    };
    static const int dibit_to_step[4] =
    {
        1, 0, 2, 3
    };
#if defined(SPANDSP_USE_FIXED_POINT)
    static const complexi16_t zero = {0, 0};
#else
    static const complexf_t zero = {0.0f, 0.0f};
#endif
    int bits;
    int shift;

    if (++s->training_step <= V17_TRAINING_SEG_3)
    {
        if (s->training_step <= V17_TRAINING_SEG_2)
        {
            if (s->training_step <= V17_TRAINING_SEG_TEP_B)
            {
                /* Optional segment: Unmodulated carrier (talker echo protection) */
                return v17_v32bis_abcd_constellation[0];
            }
            if (s->training_step <= V17_TRAINING_SEG_1)
            {
                /* Optional segment: silence (talker echo protection) */
                return zero;
            }
            /* Segment 1: ABAB... */
            return v17_v32bis_abcd_constellation[(s->training_step & 1) ^ 1];
        }
        /* Segment 2: CDBA... */
        /* Apply the scrambler */
        bits = scramble(s, 1);
        bits = (bits << 1) | scramble(s, 1);
        s->constellation_state = cdba_to_abcd[bits];
        if (s->short_train  &&  s->training_step == V17_TRAINING_SHORT_SEG_4)
        {
            /* Go straight to the ones test. */
            s->training_step = V17_TRAINING_SEG_4;
        }
        return v17_v32bis_abcd_constellation[s->constellation_state];
    }
    /* Segment 3: Bridge... */
    shift = ((s->training_step - V17_TRAINING_SEG_3 - 1) & 0x7) << 1;
    //span_log(&s->logging, SPAN_LOG_FLOW, "Seg 3 shift %d\n", shift);
    bits = scramble(s, V17_BRIDGE_WORD >> shift);
    bits = (bits << 1) | scramble(s, V17_BRIDGE_WORD >> (shift + 1));
    s->constellation_state = (s->constellation_state + dibit_to_step[bits]) & 3;
    return v17_v32bis_abcd_constellation[s->constellation_state];
}
/*- End of function --------------------------------------------------------*/

static __inline__ int diff_and_convolutional_encode(v17_tx_state_t *s, int q)
{
    static const uint8_t v32bis_4800_differential_encoder[4][4] =
    {
        {2, 3, 0, 1},
        {0, 2, 1, 3},
        {3, 1, 2, 0},
        {1, 0, 3, 2}
    };
    static const uint8_t v17_differential_encoder[4][4] =
    {
        {0, 1, 2, 3},
        {1, 2, 3, 0},
        {2, 3, 0, 1},
        {3, 0, 1, 2}
    };
    static const uint8_t v17_convolutional_coder[8][4] =
    {
        {0, 2, 3, 1},
        {4, 7, 5, 6},
        {1, 3, 2, 0},
        {7, 4, 6, 5},
        {2, 0, 1, 3},
        {6, 5, 7, 4},
        {3, 1, 0, 2},
        {5, 6, 4, 7}
    };

    if (s->bits_per_symbol == 2)
    {
        /* 4800bps mode for V.32bis */
        /* There is no trellis. We just differentially encode. */
        s->diff = v32bis_4800_differential_encoder[s->diff][q & 0x03];
        return s->diff;
    }
    /* Differentially encode */
    s->diff = v17_differential_encoder[s->diff][q & 0x03];

    /* Convolutionally encode the redundant bit */
    s->convolution = v17_convolutional_coder[s->convolution][s->diff];

    /* The final result is the combination of some uncoded bits, 2 differentially
       encoded bits, and the convolutionally encoded redundant bit. */
    return ((q << 1) & 0x78) | (s->diff << 1) | ((s->convolution >> 2) & 1);
}
/*- End of function --------------------------------------------------------*/

static int fake_get_bit(void *user_data)
{
    return 1;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINT)
static __inline__ complexi16_t getbaud(v17_tx_state_t *s)
#else
static __inline__ complexf_t getbaud(v17_tx_state_t *s)
#endif
{
    int i;
    int bit;
    int bits;
#if defined(SPANDSP_USE_FIXED_POINT)
    static const complexi16_t zero = {0, 0};
#else
    static const complexf_t zero = {0.0f, 0.0f};
#endif

    if (s->in_training)
    {
        if (s->training_step <= V17_TRAINING_END)
        {
            /* Send the training sequence */
            if (s->training_step < V17_TRAINING_SEG_4)
                return training_get(s);
            /* The last step in training is to send some 1's */
            if (++s->training_step > V17_TRAINING_END)
            {
                /* Training finished - commence normal operation. */
                s->current_get_bit = s->get_bit;
                s->in_training = false;
            }
        }
        else
        {
            if (++s->training_step > V17_TRAINING_SHUTDOWN_A)
            {
                /* The shutdown sequence is 32 bauds of all 1's, then 48 bauds
                   of silence */
                return zero;
            }
            if (s->training_step == V17_TRAINING_SHUTDOWN_END)
            {
                if (s->status_handler)
                    s->status_handler(s->status_user_data, SIG_STATUS_SHUTDOWN_COMPLETE);
            }
        }
    }
    bits = 0;
    for (i = 0;  i < s->bits_per_symbol;  i++)
    {
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
        bits |= (scramble(s, bit) << i);
    }
    return s->constellation[diff_and_convolutional_encode(s, bits)];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) v17_tx(v17_tx_state_t *s, int16_t amp[], int len)
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

    if (s->training_step >= V17_TRAINING_SHUTDOWN_END)
    {
        /* Once we have sent the shutdown sequence, we stop sending completely. */
        return 0;
    }
    for (sample = 0;  sample < len;  sample++)
    {
        if ((s->baud_phase += 3) >= 10)
        {
            s->baud_phase -= 10;
            v = getbaud(s);
            s->rrc_filter_re[s->rrc_filter_step] = v.re;
            s->rrc_filter_im[s->rrc_filter_step] = v.im;
            if (++s->rrc_filter_step >= V17_TX_FILTER_STEPS)
                s->rrc_filter_step = 0;
        }
#if defined(SPANDSP_USE_FIXED_POINT)
        /* Root raised cosine pulse shaping at baseband */
        x.re = vec_circular_dot_prodi16(s->rrc_filter_re, tx_pulseshaper[TX_PULSESHAPER_COEFF_SETS - 1 - s->baud_phase], V17_TX_FILTER_STEPS, s->rrc_filter_step) >> 4;
        x.im = vec_circular_dot_prodi16(s->rrc_filter_im, tx_pulseshaper[TX_PULSESHAPER_COEFF_SETS - 1 - s->baud_phase], V17_TX_FILTER_STEPS, s->rrc_filter_step) >> 4;
        /* Now create and modulate the carrier */
        z = dds_complexi32(&s->carrier_phase, s->carrier_phase_rate);
        iamp = ((int32_t) x.re*z.re - x.im*z.im) >> 15;
        /* Don't bother saturating. We should never clip. */
        amp[sample] = (int16_t) (((int32_t) iamp*s->gain) >> 11);
#else
        /* Root raised cosine pulse shaping at baseband */
        x.re = vec_circular_dot_prodf(s->rrc_filter_re, tx_pulseshaper[TX_PULSESHAPER_COEFF_SETS - 1 - s->baud_phase], V17_TX_FILTER_STEPS, s->rrc_filter_step);
        x.im = vec_circular_dot_prodf(s->rrc_filter_im, tx_pulseshaper[TX_PULSESHAPER_COEFF_SETS - 1 - s->baud_phase], V17_TX_FILTER_STEPS, s->rrc_filter_step);
        /* Now create and modulate the carrier */
        z = dds_complexf(&s->carrier_phase, s->carrier_phase_rate);
        famp = x.re*z.re - x.im*z.im;
        /* Don't bother saturating. We should never clip. */
        amp[sample] = (int16_t) lfastrintf(famp*s->gain);
#endif
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v17_tx_power(v17_tx_state_t *s, float power)
{
    float gain;

    /* The constellation design seems to keep the average power the same, regardless
       of which bit rate is in use. */
    gain = 0.223f*powf(10.0f, (power - DBM0_MAX_POWER)/20.0f)*32768.0f/TX_PULSESHAPER_GAIN;
#if defined(SPANDSP_USE_FIXED_POINT)
    s->gain = (int16_t) gain;
#else
    s->gain = gain;
#endif
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v17_tx_set_get_bit(v17_tx_state_t *s, get_bit_func_t get_bit, void *user_data)
{
    if (s->get_bit == s->current_get_bit)
        s->current_get_bit = get_bit;
    s->get_bit = get_bit;
    s->get_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v17_tx_set_modem_status_handler(v17_tx_state_t *s, modem_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v17_tx_get_logging_state(v17_tx_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v17_tx_restart(v17_tx_state_t *s, int bit_rate, bool tep, bool short_train)
{
    switch (bit_rate)
    {
    case 14400:
        s->bits_per_symbol = 6;
        s->constellation = v17_v32bis_14400_constellation;
        break;
    case 12000:
        s->bits_per_symbol = 5;
        s->constellation = v17_v32bis_12000_constellation;
        break;
    case 9600:
        s->bits_per_symbol = 4;
        s->constellation = v17_v32bis_9600_constellation;
        break;
    case 7200:
        s->bits_per_symbol = 3;
        s->constellation = v17_v32bis_7200_constellation;
        break;
    case 4800:
        /* This does not exist in the V.17 spec as a valid mode of operation.
           However, it does exist in V.32bis, so it is here for completeness. */
        s->bits_per_symbol = 2;
        s->constellation = v17_v32bis_4800_constellation;
        break;
    default:
        return -1;
    }
    s->bit_rate = bit_rate;
    /* NB: some modems seem to use 3 instead of 1 for long training */
    s->diff = (short_train)  ?  0  :  1;
#if defined(SPANDSP_USE_FIXED_POINT)
    vec_zeroi16(s->rrc_filter_re, sizeof(s->rrc_filter_re)/sizeof(s->rrc_filter_re[0]));
    vec_zeroi16(s->rrc_filter_im, sizeof(s->rrc_filter_im)/sizeof(s->rrc_filter_im[0]));
#else
    vec_zerof(s->rrc_filter_re, sizeof(s->rrc_filter_re)/sizeof(s->rrc_filter_re[0]));
    vec_zerof(s->rrc_filter_im, sizeof(s->rrc_filter_im)/sizeof(s->rrc_filter_im[0]));
#endif
    s->rrc_filter_step = 0;
    s->convolution = 0;
    s->scramble_reg = 0x2ECDD5;
    s->in_training = true;
    s->short_train = short_train;
    s->training_step = (tep)  ?  V17_TRAINING_SEG_TEP_A  :  V17_TRAINING_SEG_1;
    s->carrier_phase = 0;
    s->baud_phase = 0;
    s->constellation_state = 0;
    s->current_get_bit = fake_get_bit;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v17_tx_state_t *) v17_tx_init(v17_tx_state_t *s, int bit_rate, bool tep, get_bit_func_t get_bit, void *user_data)
{
    switch (bit_rate)
    {
    case 14400:
    case 12000:
    case 9600:
    case 7200:
    case 4800:
        /* 4800 is an extension of V.17, to provide full converage of the V.32bis modes */
        break;
    default:
        return NULL;
    }
    if (s == NULL)
    {
        if ((s = (v17_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.17 TX");
    s->get_bit = get_bit;
    s->get_bit_user_data = user_data;
    s->scrambler_tap = 18 - 1;
    s->carrier_phase_rate = dds_phase_ratef(CARRIER_NOMINAL_FREQ);
    v17_tx_power(s, -14.0f);
    v17_tx_restart(s, bit_rate, tep, false);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v17_tx_release(v17_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v17_tx_free(v17_tx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
