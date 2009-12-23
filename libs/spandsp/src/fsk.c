/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fsk.c - FSK modem transmit and receive parts
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
 * $Id: fsk.c,v 1.60 2009/11/02 13:25:20 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"
#include "spandsp/async.h"
#include "spandsp/fsk.h"

#include "spandsp/private/fsk.h"

const fsk_spec_t preset_fsk_specs[] =
{
    {
        "V21 ch 1",
        1080 + 100,
        1080 - 100,
        -14,
        -30,
        300*100
    },
    {
        "V21 ch 2",
        1750 + 100,
        1750 - 100,
        -14,
        -30,
        300*100
    },
    {
        "V23 ch 1",
        2100,
        1300,
        -14,
        -30,
        1200*100
    },
    {
        "V23 ch 2",
        450,
        390,
        -14,
        -30,
        75*100
    },
    {
        "Bell103 ch 1",
        2125 - 100,
        2125 + 100,
        -14,
        -30,
        300*100
    },
    {
        "Bell103 ch 2",
        1170 - 100,
        1170 + 100,
        -14,
        -30,
        300*100
    },
    {
        "Bell202",
        2200,
        1200,
        -14,
        -30,
        1200*100
    },
    {
        "Weitbrecht 45.45", /* Used for TDD (Telecoms Device for the Deaf) */
        1800,
        1400,
        -14,
        -30,
         4545
    },
    {
        "Weitbrecht 50",    /* Used for TDD (Telecoms Device for the Deaf) */
        1800,
        1400,
        -14,
        -30,
         5000
    }
};

SPAN_DECLARE(int) fsk_tx_restart(fsk_tx_state_t *s, const fsk_spec_t *spec)
{
    s->baud_rate = spec->baud_rate;
    s->phase_rates[0] = dds_phase_rate((float) spec->freq_zero);
    s->phase_rates[1] = dds_phase_rate((float) spec->freq_one);
    s->scaling = dds_scaling_dbm0((float) spec->tx_level);
    /* Initialise fractional sample baud generation. */
    s->phase_acc = 0;
    s->baud_frac = 0;
    s->current_phase_rate = s->phase_rates[1];
    
    s->shutdown = FALSE;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(fsk_tx_state_t *) fsk_tx_init(fsk_tx_state_t *s,
                                           const fsk_spec_t *spec,
                                           get_bit_func_t get_bit,
                                           void *user_data)
{
    if (s == NULL)
    {
        if ((s = (fsk_tx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->get_bit = get_bit;
    s->get_bit_user_data = user_data;
    fsk_tx_restart(s, spec);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fsk_tx_release(fsk_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fsk_tx_free(fsk_tx_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) fsk_tx(fsk_tx_state_t *s, int16_t amp[], int len)
{
    int sample;
    int bit;

    if (s->shutdown)
        return 0;
    /* Make the transitions between 0 and 1 phase coherent, but instantaneous
       jumps. There is currently no interpolation for bauds that end mid-sample.
       Mainstream users will not care. Some specialist users might have a problem
       with them, if they care about accurate transition timing. */
    for (sample = 0;  sample < len;  sample++)
    {
        if ((s->baud_frac += s->baud_rate) >= SAMPLE_RATE*100)
        {
            s->baud_frac -= SAMPLE_RATE*100;
            if ((bit = s->get_bit(s->get_bit_user_data)) == SIG_STATUS_END_OF_DATA)
            {
                if (s->status_handler)
                    s->status_handler(s->status_user_data, SIG_STATUS_END_OF_DATA);
                if (s->status_handler)
                    s->status_handler(s->status_user_data, SIG_STATUS_SHUTDOWN_COMPLETE);
                s->shutdown = TRUE;
                break;
            }
            s->current_phase_rate = s->phase_rates[bit & 1];
        }
        amp[sample] = dds_mod(&(s->phase_acc), s->current_phase_rate, s->scaling, 0);
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fsk_tx_power(fsk_tx_state_t *s, float power)
{
    s->scaling = dds_scaling_dbm0(power);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fsk_tx_set_get_bit(fsk_tx_state_t *s, get_bit_func_t get_bit, void *user_data)
{
    s->get_bit = get_bit;
    s->get_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fsk_tx_set_modem_status_handler(fsk_tx_state_t *s, modem_tx_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fsk_rx_signal_cutoff(fsk_rx_state_t *s, float cutoff)
{
    /* The 6.04 allows for the gain of the DC blocker */
    s->carrier_on_power = (int32_t) (power_meter_level_dbm0(cutoff + 2.5f - 6.04f));
    s->carrier_off_power = (int32_t) (power_meter_level_dbm0(cutoff - 2.5f - 6.04f));
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(float) fsk_rx_signal_power(fsk_rx_state_t *s)
{
    return power_meter_current_dbm0(&s->power);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fsk_rx_set_put_bit(fsk_rx_state_t *s, put_bit_func_t put_bit, void *user_data)
{
    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fsk_rx_set_modem_status_handler(fsk_rx_state_t *s, modem_tx_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fsk_rx_restart(fsk_rx_state_t *s, const fsk_spec_t *spec, int framing_mode)
{
    int chop;

    s->baud_rate = spec->baud_rate;
    s->framing_mode = framing_mode;
    fsk_rx_signal_cutoff(s, (float) spec->min_level);

    /* Detect by correlating against the tones we want, over a period
       of one baud. The correlation must be quadrature. */
    
    /* First we need the quadrature tone generators to correlate
       against. */
    s->phase_rate[0] = dds_phase_rate((float) spec->freq_zero);
    s->phase_rate[1] = dds_phase_rate((float) spec->freq_one);
    s->phase_acc[0] = 0;
    s->phase_acc[1] = 0;
    s->last_sample = 0;

    /* The correlation should be over one baud. */
    s->correlation_span = SAMPLE_RATE*100/spec->baud_rate;
    /* But limit it for very slow baud rates, so we do not overflow our
       buffer. */
    if (s->correlation_span > FSK_MAX_WINDOW_LEN)
        s->correlation_span = FSK_MAX_WINDOW_LEN;

    /* We need to scale, to avoid overflow in the correlation. */
    s->scaling_shift = 0;
    chop = s->correlation_span;
    while (chop != 0)
    {
        s->scaling_shift++;
        chop >>= 1;
    }

    /* Initialise the baud/bit rate tracking. */
    s->baud_phase = 0;
    s->frame_state = 0;
    s->frame_bits = 0;
    s->last_bit = 0;
    
    /* Initialise a power detector, so sense when a signal is present. */
    power_meter_init(&(s->power), 4);
    s->signal_present = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(fsk_rx_state_t *) fsk_rx_init(fsk_rx_state_t *s,
                                           const fsk_spec_t *spec,
                                           int framing_mode,
                                           put_bit_func_t put_bit,
                                           void *user_data)
{
    if (s == NULL)
    {
        if ((s = (fsk_rx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
    fsk_rx_restart(s, spec, framing_mode);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fsk_rx_release(fsk_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fsk_rx_free(fsk_rx_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void report_status_change(fsk_rx_state_t *s, int status)
{
    if (s->status_handler)
        s->status_handler(s->status_user_data, status);
    else if (s->put_bit)
        s->put_bit(s->put_bit_user_data, status);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) fsk_rx(fsk_rx_state_t *s, const int16_t *amp, int len)
{
    int buf_ptr;
    int baudstate;
    int i;
    int j;
    int16_t x;
    int32_t dot;
    int32_t sum[2];
    int32_t power;
    complexi_t ph;

    buf_ptr = s->buf_ptr;

    for (i = 0;  i < len;  i++)
    {
        /* The *totally* asynchronous character to character behaviour of these
           modems, when carrying async. data, seems to force a sample by sample
           approach. */
        for (j = 0;  j < 2;  j++)
        {
            s->dot[j].re -= s->window[j][buf_ptr].re;
            s->dot[j].im -= s->window[j][buf_ptr].im;

            ph = dds_complexi(&(s->phase_acc[j]), s->phase_rate[j]);
            s->window[j][buf_ptr].re = (ph.re*amp[i]) >> s->scaling_shift;
            s->window[j][buf_ptr].im = (ph.im*amp[i]) >> s->scaling_shift;

            s->dot[j].re += s->window[j][buf_ptr].re;
            s->dot[j].im += s->window[j][buf_ptr].im;

            dot = s->dot[j].re >> 15;
            sum[j] = dot*dot;
            dot = s->dot[j].im >> 15;
            sum[j] += dot*dot;
        }
        /* If there isn't much signal, don't demodulate - it will only produce
           useless junk results. */
        /* There should be no DC in the signal, but sometimes there is.
           We need to measure the power with the DC blocked, but not using
           a slow to respond DC blocker. Use the most elementary HPF. */
        x = amp[i] >> 1;
        power = power_meter_update(&(s->power), x - s->last_sample);
        s->last_sample = x;
        if (s->signal_present)
        {
            /* Look for power below turn-off threshold to turn the carrier off */
            if (power < s->carrier_off_power)
            {
                if (--s->signal_present <= 0)
                {
                    /* Count down a short delay, to ensure we push the last
                       few bits through the filters before stopping. */
                    report_status_change(s, SIG_STATUS_CARRIER_DOWN);
                    s->baud_phase = 0;
                    continue;
                }
            }
        }
        else
        {
            /* Look for power exceeding turn-on threshold to turn the carrier on */
            if (power < s->carrier_on_power)
            {
                s->baud_phase = 0;
                continue;
            }
            if (s->baud_phase < (s->correlation_span >> 1) - 30)
            {
                s->baud_phase++;
                continue;
            }
            s->signal_present = 1;
            /* Initialise the baud/bit rate tracking. */
            s->baud_phase = 0;
            s->frame_state = 0;
            s->frame_bits = 0;
            s->last_bit = 0;
            report_status_change(s, SIG_STATUS_CARRIER_UP);
        }
        /* Non-coherent FSK demodulation by correlation with the target tones
           over a one baud interval. The slow V.xx specs. are too open ended
           to allow anything fancier to be used. The dot products are calculated
           using a sliding window approach, so the compute load is not that great. */

        baudstate = (sum[0] < sum[1]);
        switch (s->framing_mode)
        {
        case FSK_FRAME_MODE_SYNC:
            /* Synchronous serial operation - e.g. for HDLC */
            if (s->last_bit != baudstate)
            {
                /* On a transition we check our timing */
                s->last_bit = baudstate;
                /* For synchronous use (e.g. HDLC channels in FAX modems), nudge
                   the baud phase gently, trying to keep it centred on the bauds. */
                if (s->baud_phase < (SAMPLE_RATE*50))
                    s->baud_phase += (s->baud_rate >> 3);
                else
                    s->baud_phase -= (s->baud_rate >> 3);
            }
            if ((s->baud_phase += s->baud_rate) >= (SAMPLE_RATE*100))
            {
                /* We should be in the middle of a baud now, so report the current
                   state as the next bit */
                s->baud_phase -= (SAMPLE_RATE*100);
                s->put_bit(s->put_bit_user_data, baudstate);
            }
            break;
        case FSK_FRAME_MODE_ASYNC:
            /* Fully asynchronous mode */
            if (s->last_bit != baudstate)
            {
                /* On a transition we check our timing */
                s->last_bit = baudstate;
                /* For async. operation, believe transitions completely, and
                   sample appropriately. This allows instant start on the first
                   transition. */
                /* We must now be about half way to a sampling point. We do not do
                   any fractional sample estimation of the transitions, so this is
                   the most accurate baud alignment we can do. */
                s->baud_phase = SAMPLE_RATE*50;
            }
            if ((s->baud_phase += s->baud_rate) >= (SAMPLE_RATE*100))
            {
                /* We should be in the middle of a baud now, so report the current
                   state as the next bit */
                s->baud_phase -= (SAMPLE_RATE*100);
                s->put_bit(s->put_bit_user_data, baudstate);
            }
            break;
        default:
            /* Gather the specified number of bits, with robust checking to ensure reasonable voice immunity.
               The first bit should be a start bit (0), and the last bit should be a stop bit (1) */
            if (s->frame_state == 0)
            {
                /* Looking for the start of a zero bit, which hopefully the start of a start bit */
                if (baudstate == 0)
                {
                    s->baud_phase = SAMPLE_RATE*(100 - 40)/2;
                    s->frame_state = -1;
                    s->frame_bits = 0;
                    s->last_bit = -1;
                }
            }
            else if (s->frame_state == -1)
            {
                /* Look for a continuous zero from the start of the start bit until
                   beyond the middle */
                if (baudstate != 0)
                {
                    /* If we aren't looking at a stable start bit, restart */
                    s->frame_state = 0;
                }
                else
                {
                    s->baud_phase += s->baud_rate;
                    if (s->baud_phase >= SAMPLE_RATE*100)
                    {
                        s->frame_state = 1;
                        s->last_bit = baudstate;
                    }
                }
            }
            else
            {
                s->baud_phase += s->baud_rate;
                if (s->baud_phase >= SAMPLE_RATE*(100 - 40))
                {
                    if (s->last_bit < 0)
                        s->last_bit = baudstate;
                    /* Look for the bit being consistent over the central 20% of the bit time. */
                    if (s->last_bit != baudstate)
                    {
                        s->frame_state = 0;
                    }
                    else if (s->baud_phase >= SAMPLE_RATE*100)
                    {
                        /* We should be in the middle of a baud now, so report the current
                           state as the next bit */
                        if (s->last_bit == baudstate)
                        {
                            s->frame_bits |= (baudstate << s->framing_mode);
                            s->frame_bits >>= 1;
                            s->baud_phase -= (SAMPLE_RATE*100);
                            if (++s->frame_state > s->framing_mode)
                            {
                                /* Check we have a stop bit */
                                if (baudstate == 1)
                                {
                                    /* Check we have a start bit */
                                    if ((s->frame_bits & 1) == 0)
                                    {
                                        /* Drop the start bit, and pass the rest back */
                                        s->frame_bits >>= 1;
                                        s->put_bit(s->put_bit_user_data, s->frame_bits);
                                    }
                                }
                                s->frame_state = 0;
                            }
                        }
                        else
                        {
                            s->frame_state = 0;
                        }
                        s->last_bit = -1;
                    }
                }
            }
            break;
        }
        if (++buf_ptr >= s->correlation_span)
            buf_ptr = 0;
    }
    s->buf_ptr = buf_ptr;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fsk_rx_fillin(fsk_rx_state_t *s, int len)
{
    /* The valid choice here is probably to do nothing. We don't change state
      (i.e carrier on<->carrier off), and we'll just output less bits than we
      should. */
    /* TODO: Advance the symbol phase the appropriate amount */
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
