/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v22bis_tx.c - ITU V.22bis modem transmit part
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
 * $Id: v22bis_tx.c,v 1.64 2009/11/04 15:52:06 steveu Exp $
 */

/*! \file */

/* THIS IS A WORK IN PROGRESS - It is basically functional, but it is not feature
   complete, and doesn't reliably sync over the signal and noise level ranges it should! */

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
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/logging.h"
#include "spandsp/complex.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/async.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"

#include "spandsp/v29rx.h"
#include "spandsp/v22bis.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/v22bis.h"

#if defined(SPANDSP_USE_FIXED_POINTx)
#include "v22bis_tx_fixed_rrc.h"
#else
#include "v22bis_tx_floating_rrc.h"
#endif

/* Quoting from the V.22bis spec.

6.3.1.1 Interworking at 2400 bit/s

6.3.1.1.1   Calling modem

a)  On connection to line the calling modem shall be conditioned to receive signals
    in the high channel at 1200 bit/s and transmit signals in the low channel at 1200 bit/s
    in accordance with section 2.5.2.2. It shall apply an ON condition to circuit 107 in accordance
    with Recommendation V.25. The modem shall initially remain silent.

b)  After 155 +-10 ms of unscrambled binary 1 has been detected, the modem shall remain silent
    for a further 456 +-10 ms then transmit an unscrambled repetitive double dibit pattern of 00
    and 11 at 1200 bit/s for 100 +-3 ms. Following this signal the modem shall transmit scrambled
    binary 1 at 1200 bit/s.

c)  If the modem detects scrambled binary 1 in the high channel at 1200 bit/s for 270 +-40 ms,
    the handshake shall continue in accordance with section 6.3.1.2.1 c) and d). However, if unscrambled
    repetitive double dibit 00 and 11 at 1200 bit/s is detected in the high channel, then at the
    end of receipt of this signal the modem shall apply an ON condition to circuit 112.

d)  600 +-10 ms after circuit 112 has been turned ON the modem shall begin transmitting scrambled
    binary 1 at 2400 bit/s, and 450 +-10 ms after circuit 112 has been turned ON the receiver may
    begin making 16-way decisions.

e)  Following transmission of scrambled binary 1 at 2400 bit/s for 200 +-10 ms, circuit 106 shall
    be conditioned to respond to circuit 105 and the modem shall be ready to transmit data at
    2400 bit/s.

f)  When 32 consecutive bits of scrambled binary 1 at 2400 bit/s have been detected in the high
    channel the modem shall be ready to receive data at 2400 bit/s and shall apply an ON condition
    to circuit 109.

6.3.1.1.2   Answering modem

a)  On connection to line the answering modem shall be conditioned to transmit signals in the high
    channel at 1200 bit/s in accordance with  section 2.5.2.2 and receive signals in the low channel at
    1200 bit/s. Following transmission of the answer sequence in accordance with Recommendation
    V.25, the modem shall apply an ON condition to circuit 107 and then transmit unscrambled
    binary 1 at 1200 bit/s.

b)  If the modem detects scrambled binary 1 or 0 in the low channel at 1200 bit/s for 270 +-40 ms,
    the handshake shall continue in accordance with section 6.3.1.2.2 b) and c). However, if unscrambled
    repetitive double dibit 00 and 11 at 1200 bit/s is detected in the low channel, at the end of
    receipt of this signal the modem shall apply an ON condition to circuit 112 and then transmit
    an unscrambled repetitive double dibit pattern of 00 and 11 at 1200 bit/s for 100 +-3 ms.
    Following these signals the modem shall transmit scrambled binary 1 at 1200 bit/s.

c)  600 +-10 ms after circuit 112 has been turned ON the modem shall begin transmitting scrambled
    binary 1 at 2400 bit/s, and 450 +-10 ms after circuit 112 has been turned ON the receiver may
    begin making 16-way decisions.

d)  Following transmission of scrambled binary 1 at 2400 bit/s for 200 +-10 ms, circuit 106 shall
    be conditioned to respond to circuit 105 and the modem shall be ready to transmit data at
    2400 bit/s.

e)  When 32 consecutive bits of scrambled binary 1 at 2400 bit/s have been detected in the low
    channel the modem shall be ready to receive data at 2400 bit/s and shall apply an ON
    condition to circuit 109.

6.3.1.2 Interworking at 1200 bit/s

The following handshake is identical to the Recommendation V.22 alternative A and B handshake.

6.3.1.2.1   Calling modem

a)  On connection to line the calling modem shall be conditioned to receive signals in the high
    channel at 1200 bit/s and transmit signals in the low channel at 1200 bit/s in accordance
    with section 2.5.2.2. It shall apply an ON condition to circuit 107 in accordance with
    Recommendation V.25. The modem shall initially remain silent.

b)  After 155 +-10 ms of unscrambled binary 1 has been detected, the modem shall remain silent
    for a further 456 +-10 ms then transmit scrambled binary 1 at 1200 bit/s (a preceding V.22 bis
    signal, as shown in Figure 7/V.22 bis, would not affect the operation of a V.22 answer modem).

c)  On detection of scrambled binary 1 in the high channel at 1200 bit/s for 270 +-40 ms the modem
    shall be ready to receive data at 1200 bit/s and shall apply an ON condition to circuit 109 and
    an OFF condition to circuit 112.

d)  765 +-10 ms after circuit 109 has been turned ON, circuit 106 shall be conditioned to respond
    to circuit 105 and the modem shall be ready to transmit data at 1200 bit/s.
 
6.3.1.2.2   Answering modem

a)  On connection to line the answering modem shall be conditioned to transmit signals in the high
    channel at 1200 bit/s in accordance with section 2.5.2.2 and receive signals in the low channel at
    1200 bit/s.

    Following transmission of the answer sequence in accordance with V.25 the modem shall apply
    an ON condition to circuit 107 and then transmit unscrambled binary 1 at 1200 bit/s.

b)  On detection of scrambled binary 1 or 0 in the low channel at 1200 bit/s for 270 +-40 ms the
    modem shall apply an OFF condition to circuit 112 and shall then transmit scrambled binary 1
    at 1200 bit/s.

c)  After scrambled binary 1 has been transmitted at 1200 bit/s for 765 +-10 ms the modem shall be
    ready to transmit and receive data at 1200 bit/s, shall condition circuit 106 to respond to
    circuit 105 and shall apply an ON condition to circuit 109.

Note - Manufacturers may wish to note that in certain countries, for national purposes, modems are
       in service which emit an answering tone of 2225 Hz instead of unscrambled binary 1.


V.22bis to V.22bis
------------------
Calling party
                                                           S1       scrambled 1's                  scrambled 1's  data
                                                                    at 1200bps                     at 2400bps
|---------------------------------------------------------|XXXXXXXX|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXXX|XXXXXXXXXXXXX
                                      |<155+-10>|<456+-10>|<100+-3>|        |<------600+-10------>|<---200+-10-->|
                                      ^                            |        ^<----450+-100---->|[16 way decisions begin]
                                      |                            |        |
                                      |                            v        |
                                      |                            |<------450+-100----->|[16 way decisions begin]
                                      |                            |<----------600+-10-------->|
  |<2150+-350>|<--3300+-700->|<75+-20>|                            |<100+-3>|                  |<---200+-10-->
  |-----------|XXXXXXXXXXXXXX|--------|XXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXX|XXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXXX|XXXXXXXXXXXXX
   silence    2100Hz                   unscrambled 1's              S1       scrambled 1's      scrambled 1's  data
                                       at 1200bps                            at 1200bps         at 2400bps
Answering party

S1 = Unscrambled double dibit 00 and 11 at 1200bps
When the 2400bps section starts, both sides should look for 32 bits of continuous ones, as a test of integrity.




V.22 to V.22bis
---------------
Calling party
                                                           scrambled 1's                                 data
                                                           at 1200bps
|---------------------------------------------------------|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX
                                      |<155+-10>|<456+-10>|         |<270+-40>|<--------765+-10-------->|
                                      ^                   |         ^
                                      |                   |         |
                                      |                   |         |
                                      |                   |         |
                                      |                   v         |
  |<2150+-350>|<--3300+-700->|<75+-20>|                   |<270+-40>|<---------765+-10-------->|
  |-----------|XXXXXXXXXXXXXX|--------|XXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX
   silence    2100Hz                   unscrambled 1's                scrambled 1's             data
                                       at 1200bps                     at 1200bps
Answering party

Both ends should accept unscrambled binary 1 or binary 0 as the preamble.




V.22bis to V.22
---------------
Calling party
                                                           S1      scrambled 1's                                 data
                                                                   at 1200bps
|---------------------------------------------------------|XXXXXXXX|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX
                                      |<155+-10>|<456+-10>|<100+-3>|           |<-270+-40-><------765+-10------>|
                                      ^                            |           ^
                                      |                            |           |
                                      |                            v           |
                                      |                            |
                                      |                            |
  |<2150+-350>|<--3300+-700->|<75+-20>|                            |<-270+-40->|<------765+-10----->|
  |-----------|XXXXXXXXXXXXXX|--------|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXXXXXXXXX|XXXXXXXXXXXXX
   silence    2100Hz                   unscrambled 1's                          scrambled 1's        data
                                       at 1200bps                               at 1200bps
Answering party

Both ends should accept unscrambled binary 1 or binary 0 as the preamble.
*/

#define ms_to_symbols(t)    (((t)*600)/1000)

static const int phase_steps[4] =
{
    1, 0, 2, 3
};

const complexf_t v22bis_constellation[16] =
{
    { 1.0f,  1.0f},
    { 3.0f,  1.0f},     /* 1200bps 00 */
    { 1.0f,  3.0f},
    { 3.0f,  3.0f},
    {-1.0f,  1.0f},
    {-1.0f,  3.0f},     /* 1200bps 01 */
    {-3.0f,  1.0f},
    {-3.0f,  3.0f},
    {-1.0f, -1.0f},
    {-3.0f, -1.0f},     /* 1200bps 10 */
    {-1.0f, -3.0f},
    {-3.0f, -3.0f},
    { 1.0f, -1.0f},
    { 1.0f, -3.0f},     /* 1200bps 11 */
    { 3.0f, -1.0f},
    { 3.0f, -3.0f}
};

static int fake_get_bit(void *user_data)
{
    return 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int scramble(v22bis_state_t *s, int bit)
{
    int out_bit;

    if (s->tx.scrambler_pattern_count >= 64)
    {
        bit ^= 1;
        s->tx.scrambler_pattern_count = 0;
    }
    out_bit = (bit ^ (s->tx.scramble_reg >> 13) ^ (s->tx.scramble_reg >> 16)) & 1;
    s->tx.scramble_reg = (s->tx.scramble_reg << 1) | out_bit;
    
    if (out_bit == 1)
        s->tx.scrambler_pattern_count++;
    else
        s->tx.scrambler_pattern_count = 0;
    return out_bit;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int get_scrambled_bit(v22bis_state_t *s)
{
    int bit;

    if ((bit = s->tx.current_get_bit(s->get_bit_user_data)) == SIG_STATUS_END_OF_DATA)
    {
        /* Fill out this symbol with ones, and prepare to send
           the rest of the shutdown sequence. */
        s->tx.current_get_bit = fake_get_bit;
        s->tx.shutdown = 1;
        bit = 1;
    }
    return scramble(s, bit);
}
/*- End of function --------------------------------------------------------*/

static complexf_t training_get(v22bis_state_t *s)
{
    complexf_t z;
    int bits;

    /* V.22bis training sequence */
    switch (s->tx.training)
    {
    case V22BIS_TX_TRAINING_STAGE_INITIAL_TIMED_SILENCE:
        /* The answerer waits 75ms, then sends unscrambled ones */
        if (++s->tx.training_count >= ms_to_symbols(75))
        {
            /* Initial 75ms of silence is over */
            span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting U11 1200\n");
            s->tx.training_count = 0;
            s->tx.training = V22BIS_TX_TRAINING_STAGE_U11;
        }
        /* Fall through */
    case V22BIS_TX_TRAINING_STAGE_INITIAL_SILENCE:
        /* Silence */
        s->tx.constellation_state = 0;
        z = complex_setf(0.0f, 0.0f);
        break;
    case V22BIS_TX_TRAINING_STAGE_U11:
        /* Send continuous unscrambled ones at 1200bps (i.e. 270 degree phase steps). */
        /* Only the answering modem sends unscrambled ones. It is the first thing exchanged between the modems. */
        s->tx.constellation_state = (s->tx.constellation_state + phase_steps[3]) & 3;
        z = v22bis_constellation[(s->tx.constellation_state << 2) | 0x01];
        break;
    case V22BIS_TX_TRAINING_STAGE_U0011:
        /* Continuous unscrambled double dibit 00 11 at 1200bps. This is termed the S1 segment in
           the V.22bis spec. It is only sent to request or accept 2400bps mode, and lasts 100+-3ms. After this
           timed burst, we unconditionally change to sending scrambled ones at 1200bps. */
        s->tx.constellation_state = (s->tx.constellation_state + phase_steps[3*(s->tx.training_count & 1)]) & 3;
        z = v22bis_constellation[(s->tx.constellation_state << 2) | 0x01];
        if (++s->tx.training_count >= ms_to_symbols(100))
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting S11 after U0011\n");
            if (s->calling_party)
            {
                s->tx.training_count = 0;
                s->tx.training = V22BIS_TX_TRAINING_STAGE_S11;
            }
            else
            {
                s->tx.training_count = ms_to_symbols(756 - (600 - 100));
                s->tx.training = V22BIS_TX_TRAINING_STAGE_TIMED_S11;
            }
        }
        break;
    case V22BIS_TX_TRAINING_STAGE_TIMED_S11:
        /* A timed period of scrambled ones at 1200bps. */
        if (++s->tx.training_count >= ms_to_symbols(756))
        {
            if (s->negotiated_bit_rate == 2400)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "+++ starting S1111 (C)\n");
                s->tx.training_count = 0;
                s->tx.training = V22BIS_TX_TRAINING_STAGE_S1111;
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "+++ Tx normal operation (1200)\n");
                s->tx.training_count = 0;
                s->tx.training = V22BIS_TX_TRAINING_STAGE_NORMAL_OPERATION;
                v22bis_report_status_change(s, SIG_STATUS_TRAINING_SUCCEEDED);
                s->tx.current_get_bit = s->get_bit;
            }
        }
        /* Fall through */
    case V22BIS_TX_TRAINING_STAGE_S11:
        /* Scrambled ones at 1200bps. */
        bits = scramble(s, 1);
        bits = (bits << 1) | scramble(s, 1);
        s->tx.constellation_state = (s->tx.constellation_state + phase_steps[bits]) & 3;
        z = v22bis_constellation[(s->tx.constellation_state << 2) | 0x01];
        break;
    case V22BIS_TX_TRAINING_STAGE_S1111:
        /* Scrambled ones at 2400bps. We send a timed 200ms burst, and switch to normal operation at 2400bps */
        bits = scramble(s, 1);
        bits = (bits << 1) | scramble(s, 1);
        s->tx.constellation_state = (s->tx.constellation_state + phase_steps[bits]) & 3;
        bits = scramble(s, 1);
        bits = (bits << 1) | scramble(s, 1);
        z = v22bis_constellation[(s->tx.constellation_state << 2) | bits];
        if (++s->tx.training_count >= ms_to_symbols(200))
        {
            /* We have completed training. Now handle some real work. */
            span_log(&s->logging, SPAN_LOG_FLOW, "+++ Tx normal operation (2400)\n");
            s->tx.training_count = 0;
            s->tx.training = V22BIS_TX_TRAINING_STAGE_NORMAL_OPERATION;
            v22bis_report_status_change(s, SIG_STATUS_TRAINING_SUCCEEDED);
            s->tx.current_get_bit = s->get_bit;
        }
        break;
    case V22BIS_TX_TRAINING_STAGE_PARKED:
    default:
        z = complex_setf(0.0f, 0.0f);
        break;
    }
    return z;
}
/*- End of function --------------------------------------------------------*/

static complexf_t getbaud(v22bis_state_t *s)
{
    int bits;

    if (s->tx.training)
    {
        /* Send the training sequence */
        return training_get(s);
    }

    /* There is no graceful shutdown procedure defined for V.22bis. Just
       send some ones, to ensure we get the real data bits through, even
       with bad ISI. */
    if (s->tx.shutdown)
    {
        if (++s->tx.shutdown > 10)
            return complex_setf(0.0f, 0.0f);
    }
    /* The first two bits define the quadrant */
    bits = get_scrambled_bit(s);
    bits = (bits << 1) | get_scrambled_bit(s);
    s->tx.constellation_state = (s->tx.constellation_state + phase_steps[bits]) & 3;
    if (s->negotiated_bit_rate == 1200)
    {
        bits = 0x01;
    }
    else
    {
        /* The other two bits define the position within the quadrant */
        bits = get_scrambled_bit(s);
        bits = (bits << 1) | get_scrambled_bit(s);
    }
    return v22bis_constellation[(s->tx.constellation_state << 2) | bits];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) v22bis_tx(v22bis_state_t *s, int16_t amp[], int len)
{
    complexf_t x;
    complexf_t z;
    int i;
    int sample;
    float famp;

    if (s->tx.shutdown > 10)
        return 0;
    for (sample = 0;  sample < len;  sample++)
    {
        if ((s->tx.baud_phase += 3) >= 40)
        {
            s->tx.baud_phase -= 40;
            s->tx.rrc_filter[s->tx.rrc_filter_step] =
            s->tx.rrc_filter[s->tx.rrc_filter_step + V22BIS_TX_FILTER_STEPS] = getbaud(s);
            if (++s->tx.rrc_filter_step >= V22BIS_TX_FILTER_STEPS)
                s->tx.rrc_filter_step = 0;
        }
        /* Root raised cosine pulse shaping at baseband */
        x = complex_setf(0.0f, 0.0f);
        for (i = 0;  i < V22BIS_TX_FILTER_STEPS;  i++)
        {
            x.re += tx_pulseshaper[39 - s->tx.baud_phase][i]*s->tx.rrc_filter[i + s->tx.rrc_filter_step].re;
            x.im += tx_pulseshaper[39 - s->tx.baud_phase][i]*s->tx.rrc_filter[i + s->tx.rrc_filter_step].im;
        }
        /* Now create and modulate the carrier */
        z = dds_complexf(&(s->tx.carrier_phase), s->tx.carrier_phase_rate);
        famp = (x.re*z.re - x.im*z.im)*s->tx.gain;
        if (s->tx.guard_phase_rate  &&  (s->tx.rrc_filter[s->tx.rrc_filter_step].re != 0.0f  ||  s->tx.rrc_filter[i + s->tx.rrc_filter_step].im != 0.0f))
        {
            /* Add the guard tone */
            famp += dds_modf(&(s->tx.guard_phase), s->tx.guard_phase_rate, s->tx.guard_level, 0);
        }
        /* Don't bother saturating. We should never clip. */
        amp[sample] = (int16_t) lfastrintf(famp);
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v22bis_tx_power(v22bis_state_t *s, float power)
{
    float l;

    if (s->tx.guard_phase_rate == dds_phase_ratef(550.0f))
    {
        l = 1.6f*powf(10.0f, (power - 1.0f - DBM0_MAX_POWER)/20.0f);
        s->tx.gain = l*32768.0f/(TX_PULSESHAPER_GAIN*3.0f);
        l = powf(10.0f, (power - 1.0f - 3.0f - DBM0_MAX_POWER)/20.0f);
        s->tx.guard_level = l*32768.0f;
    }
    else if(s->tx.guard_phase_rate == dds_phase_ratef(1800.0f))
    {
        l = 1.6f*powf(10.0f, (power - 1.0f - 1.0f - DBM0_MAX_POWER)/20.0f);
        s->tx.gain = l*32768.0f/(TX_PULSESHAPER_GAIN*3.0f);
        l = powf(10.0f, (power - 1.0f - 6.0f - DBM0_MAX_POWER)/20.0f);
        s->tx.guard_level = l*32768.0f;
    }
    else
    {
        l = 1.6f*powf(10.0f, (power - DBM0_MAX_POWER)/20.0f);
        s->tx.gain = l*32768.0f/(TX_PULSESHAPER_GAIN*3.0f);
        s->tx.guard_level = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static int v22bis_tx_restart(v22bis_state_t *s)
{
    cvec_zerof(s->tx.rrc_filter, sizeof(s->tx.rrc_filter)/sizeof(s->tx.rrc_filter[0]));
    s->tx.rrc_filter_step = 0;
    s->tx.scramble_reg = 0;
    s->tx.scrambler_pattern_count = 0;
    if (s->calling_party)
        s->tx.training = V22BIS_TX_TRAINING_STAGE_INITIAL_SILENCE;
    else
        s->tx.training = V22BIS_TX_TRAINING_STAGE_INITIAL_TIMED_SILENCE;
    s->tx.training_count = 0;
    s->tx.carrier_phase = 0;
    s->tx.guard_phase = 0;
    s->tx.baud_phase = 0;
    s->tx.constellation_state = 0;
    s->tx.current_get_bit = fake_get_bit;
    s->tx.shutdown = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v22bis_set_get_bit(v22bis_state_t *s, get_bit_func_t get_bit, void *user_data)
{
    s->get_bit = get_bit;
    s->get_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v22bis_set_put_bit(v22bis_state_t *s, put_bit_func_t put_bit, void *user_data)
{
    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v22bis_set_modem_status_handler(v22bis_state_t *s, modem_tx_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v22bis_get_logging_state(v22bis_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v22bis_restart(v22bis_state_t *s, int bit_rate)
{
    switch (bit_rate)
    {
    case 2400:
    case 1200:
        break;
    default:
        return -1;
    }
    s->bit_rate = bit_rate;
    s->negotiated_bit_rate = 1200;
    if (v22bis_tx_restart(s))
        return -1;
    return v22bis_rx_restart(s);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v22bis_request_retrain(v22bis_state_t *s, int bit_rate)
{
    /* TODO: support bit rate switching */
    switch (bit_rate)
    {
    case 2400:
    case 1200:
        break;
    default:
        return -1;
    }
    /* TODO: support bit rate changes */
    /* Retrain is only valid when we are normal operation at 2400bps */
    if (s->rx.training != V22BIS_RX_TRAINING_STAGE_NORMAL_OPERATION
        ||
        s->tx.training != V22BIS_TX_TRAINING_STAGE_NORMAL_OPERATION
        ||
        s->negotiated_bit_rate != 2400)
    {
        return -1;
    }
    /* Send things back into the training process at the appropriate point.
       The far end should detect the S1 signal, and reciprocate. */
    span_log(&s->logging, SPAN_LOG_FLOW, "+++ Initiating a retrain\n");
    s->rx.pattern_repeats = 0;
    s->rx.training_count = 0;
    s->rx.training = V22BIS_RX_TRAINING_STAGE_SCRAMBLED_ONES_AT_1200;
    s->tx.training_count = 0;
    s->tx.training = V22BIS_TX_TRAINING_STAGE_U0011;
    v22bis_equalizer_coefficient_reset(s);
    v22bis_report_status_change(s, SIG_STATUS_MODEM_RETRAIN_OCCURRED);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v22bis_remote_loopback(v22bis_state_t *s, int enable)
{
    /* TODO: */
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v22bis_current_bit_rate(v22bis_state_t *s)
{
    return s->negotiated_bit_rate;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v22bis_state_t *) v22bis_init(v22bis_state_t *s,
                                           int bit_rate,
                                           int guard,
                                           int calling_party,
                                           get_bit_func_t get_bit,
                                           void *get_bit_user_data,
                                           put_bit_func_t put_bit,
                                           void *put_bit_user_data)
{
    switch (bit_rate)
    {
    case 2400:
    case 1200:
        break;
    default:
        return NULL;
    }
    if (s == NULL)
    {
        if ((s = (v22bis_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.22bis");
    s->bit_rate = bit_rate;
    s->calling_party = calling_party;

    s->get_bit = get_bit;
    s->get_bit_user_data = get_bit_user_data;
    s->put_bit = put_bit;
    s->put_bit_user_data = put_bit_user_data;

    if (s->calling_party)
    {
        s->tx.carrier_phase_rate = dds_phase_ratef(1200.0f);
    }
    else
    {
        s->tx.carrier_phase_rate = dds_phase_ratef(2400.0f);
        switch (guard)
        {
        case V22BIS_GUARD_TONE_550HZ:
            s->tx.guard_phase_rate = dds_phase_ratef(550.0f);
            break;
        case V22BIS_GUARD_TONE_1800HZ:
            s->tx.guard_phase_rate = dds_phase_ratef(1800.0f);
            break;
        default:
            s->tx.guard_phase_rate = 0;
            break;
        }
    }
    v22bis_tx_power(s, -14.0f);
    v22bis_restart(s, s->bit_rate);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v22bis_release(v22bis_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v22bis_free(v22bis_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
