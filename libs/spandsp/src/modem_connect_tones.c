/*
 * SpanDSP - a series of DSP components for telephony
 *
 * modem_connect_tones.c - Generation and detection of tones
 *                         associated with modems calling and answering calls.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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

/* CNG is 0.5s+-15% of 1100+-38Hz, 3s+-15% off, repeating.

   CED is 0.2s silence, 3.3+-0.7s of 2100+-15Hz, and 75+-20ms of silence.

   Calling tone is 0.5s-0.7s of 1300Hz+-15Hz, 1.5s-2.0s off, repeating.

   ANS is 3.3+-0.7s of 2100+-15Hz.

   ANS/ is 3.3+-0.7s of 2100+-15Hz, with phase reversals (180+-10 degrees, hopping in <1ms) every 450+-25ms.

   ANSam/ is 2100+-1Hz, with phase reversals (180+-10 degrees, hopping in <1ms) every 450+-25ms, and AM with a sinewave of 15+-0.1Hz. 
   The modulated envelope ranges in amplitude between (0.8+-0.01) and (1.2+-0.01) times its average
   amplitude. It lasts up to 5s, but will be stopped early if the V.8 protocol proceeds. */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <memory.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <stdio.h>

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/logging.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/power_meter.h"
#include "spandsp/async.h"
#include "spandsp/fsk.h"
#include "spandsp/modem_connect_tones.h"

#include "spandsp/private/fsk.h"
#include "spandsp/private/modem_connect_tones.h"

#define HDLC_FRAMING_OK_THRESHOLD       5

SPAN_DECLARE(const char *) modem_connect_tone_to_str(int tone)
{
    switch (tone)
    {
    case MODEM_CONNECT_TONES_NONE:
        return "No tone";
    case MODEM_CONNECT_TONES_FAX_CNG:
        return "FAX CNG";
    case MODEM_CONNECT_TONES_ANS:
        return "ANS or FAX CED";
    case MODEM_CONNECT_TONES_ANS_PR:
        return "ANS/";
    case MODEM_CONNECT_TONES_ANSAM:
        return "ANSam";
    case MODEM_CONNECT_TONES_ANSAM_PR:
        return "ANSam/";
    case MODEM_CONNECT_TONES_FAX_PREAMBLE:
        return "FAX preamble";
    case MODEM_CONNECT_TONES_FAX_CED_OR_PREAMBLE:
        return "FAX CED or preamble";
    case MODEM_CONNECT_TONES_BELL_ANS:
        return "Bell ANS";
    case MODEM_CONNECT_TONES_CALLING_TONE:
        return "Calling tone";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) modem_connect_tones_tx(modem_connect_tones_tx_state_t *s,
                                                int16_t amp[],
                                                int len)
{
    int16_t mod;
    int i;
    int xlen;

    i = 0;
    switch (s->tone_type)
    {
    case MODEM_CONNECT_TONES_FAX_CNG:
        for (  ;  i < len;  i++)
        {
            if (s->duration_timer > ms_to_samples(3000))
            {
                if ((xlen = i + s->duration_timer - ms_to_samples(3000)) > len)
                    xlen = len;
                s->duration_timer -= (xlen - i);
                for (  ;  i < xlen;  i++)
                    amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, s->level, 0);
            }
            if (s->duration_timer > 0)
            {
                if ((xlen = i + s->duration_timer) > len)
                    xlen = len;
                s->duration_timer -= (xlen - i);
                memset(amp + i, 0, sizeof(int16_t)*(xlen - i));
                i = xlen;
            }
            if (s->duration_timer == 0)
                s->duration_timer = ms_to_samples(500 + 3000);
        }
        break;
    case MODEM_CONNECT_TONES_ANS:
        if (s->duration_timer < len)
            len = s->duration_timer;
        if (s->duration_timer > ms_to_samples(2600))
        {
            /* There is some initial silence to be generated. */
            if ((i = s->duration_timer - ms_to_samples(2600)) > len)
                i = len;
            memset(amp, 0, sizeof(int16_t)*i);
        }
        for (  ;  i < len;  i++)
            amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, s->level, 0);
        s->duration_timer -= len;
        break;
    case MODEM_CONNECT_TONES_ANS_PR:
        if (s->duration_timer < len)
            len = s->duration_timer;
        if (s->duration_timer > ms_to_samples(3300))
        {
            if ((i = s->duration_timer - ms_to_samples(3300)) > len)
                i = len;
            memset(amp, 0, sizeof(int16_t)*i);
        }
        for (  ;  i < len;  i++)
        {
            if (--s->hop_timer <= 0)
            {
                s->hop_timer = ms_to_samples(450);
                s->tone_phase += 0x80000000;
            }
            amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, s->level, 0);
        }
        s->duration_timer -= len;
        break;
    case MODEM_CONNECT_TONES_ANSAM:
        if (s->duration_timer < len)
            len = s->duration_timer;
        if (s->duration_timer > ms_to_samples(5000))
        {
            if ((i = s->duration_timer - ms_to_samples(5000)) > len)
                i = len;
            memset(amp, 0, sizeof(int16_t)*i);
        }
        for (  ;  i < len;  i++)
        {
            mod = (int16_t) (s->level + dds_mod(&s->mod_phase, s->mod_phase_rate, s->mod_level, 0));
            amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, mod, 0);
        }
        s->duration_timer -= len;
        break;
    case MODEM_CONNECT_TONES_ANSAM_PR:
        if (s->duration_timer < len)
            len = s->duration_timer;
        if (s->duration_timer > ms_to_samples(5000))
        {
            if ((i = s->duration_timer - ms_to_samples(5000)) > len)
                i = len;
            memset(amp, 0, sizeof(int16_t)*i);
        }
        for (  ;  i < len;  i++)
        {
            if (--s->hop_timer <= 0)
            {
                s->hop_timer = ms_to_samples(450);
                s->tone_phase += 0x80000000;
            }
            mod = (int16_t) (s->level + dds_mod(&s->mod_phase, s->mod_phase_rate, s->mod_level, 0));
            amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, mod, 0);
        }
        s->duration_timer -= len;
        break;
    case MODEM_CONNECT_TONES_BELL_ANS:
        if (s->duration_timer < len)
            len = s->duration_timer;
        if (s->duration_timer > ms_to_samples(2600))
        {
            /* There is some initial silence to be generated. */
            if ((i = s->duration_timer - ms_to_samples(2600)) > len)
                i = len;
            memset(amp, 0, sizeof(int16_t)*i);
        }
        for (  ;  i < len;  i++)
            amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, s->level, 0);
        s->duration_timer -= len;
        break;
    case MODEM_CONNECT_TONES_CALLING_TONE:
        for (  ;  i < len;  i++)
        {
            if (s->duration_timer > ms_to_samples(2000))
            {
                if ((xlen = i + s->duration_timer - ms_to_samples(2000)) > len)
                    xlen = len;
                s->duration_timer -= (xlen - i);
                for (  ;  i < xlen;  i++)
                    amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, s->level, 0);
            }
            if (s->duration_timer > 0)
            {
                if ((xlen = i + s->duration_timer) > len)
                    xlen = len;
                s->duration_timer -= (xlen - i);
                memset(amp + i, 0, sizeof(int16_t)*(xlen - i));
                i = xlen;
            }
            if (s->duration_timer == 0)
                s->duration_timer = ms_to_samples(600 + 2000);
        }
        break;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(modem_connect_tones_tx_state_t *) modem_connect_tones_tx_init(modem_connect_tones_tx_state_t *s,
                                                                           int tone_type)
{
    int alloced;

    alloced = FALSE;
    if (s == NULL)
    {
        if ((s = (modem_connect_tones_tx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
        alloced = TRUE;
    }
    s->tone_type = tone_type;
    switch (s->tone_type)
    {
    case MODEM_CONNECT_TONES_FAX_CNG:
        /* 0.5s of 1100Hz+-38Hz + 3.0s of silence repeating. Timing +-15% */
        s->tone_phase_rate = dds_phase_rate(1100.0);
        s->level = dds_scaling_dbm0(-11);
        s->duration_timer = ms_to_samples(500 + 3000);
        s->mod_phase_rate = 0;
        s->tone_phase = 0;
        s->mod_phase = 0;
        s->mod_level = 0;
        s->hop_timer = 0;
        break;
    case MODEM_CONNECT_TONES_ANS:
    case MODEM_CONNECT_TONES_ANSAM:
        /* 0.2s of silence, then 2.6s to 4s of 2100Hz+-15Hz tone, then 75ms of silence. */
        s->tone_phase_rate = dds_phase_rate(2100.0);
        s->level = dds_scaling_dbm0(-11);
        if (s->tone_type == MODEM_CONNECT_TONES_ANSAM)
        {
            s->mod_phase_rate = dds_phase_rate(15.0);
            s->mod_level = s->level/5;
            s->duration_timer = ms_to_samples(200 + 5000);
        }
        else
        {
            s->mod_phase_rate = 0;
            s->mod_level = 0;
            s->duration_timer = ms_to_samples(200 + 2600);
        }
        s->tone_phase = 0;
        s->mod_phase = 0;
        s->hop_timer = 0;
        break;
    case MODEM_CONNECT_TONES_ANS_PR:
    case MODEM_CONNECT_TONES_ANSAM_PR:
        s->tone_phase_rate = dds_phase_rate(2100.0);
        s->level = dds_scaling_dbm0(-12);
        if (s->tone_type == MODEM_CONNECT_TONES_ANSAM_PR)
        {
            s->mod_phase_rate = dds_phase_rate(15.0);
            s->mod_level = s->level/5;
            s->duration_timer = ms_to_samples(200 + 5000);
        }
        else
        {
            s->mod_phase_rate = 0;
            s->mod_level = 0;
            s->duration_timer = ms_to_samples(200 + 3300);
        }
        s->tone_phase = 0;
        s->mod_phase = 0;
        s->hop_timer = ms_to_samples(450);
        break;
    case MODEM_CONNECT_TONES_BELL_ANS:
        /* 0.2s of silence, then 2.6s to 4s of 2225Hz+-15Hz tone, then 75ms of silence. */
        s->tone_phase_rate = dds_phase_rate(2225.0);
        s->level = dds_scaling_dbm0(-11);
        s->mod_phase_rate = 0;
        s->mod_level = 0;
        s->duration_timer = ms_to_samples(200 + 2600);
        s->tone_phase = 0;
        s->mod_phase = 0;
        s->hop_timer = 0;
        break;
    case MODEM_CONNECT_TONES_CALLING_TONE:
        /* 0.6s of 1300Hz+-15Hz + 2.0s of silence repeating. */
        s->tone_phase_rate = dds_phase_rate(1300.0);
        s->level = dds_scaling_dbm0(-11);
        s->duration_timer = ms_to_samples(600 + 2000);
        s->mod_phase_rate = 0;
        s->tone_phase = 0;
        s->mod_phase = 0;
        s->mod_level = 0;
        s->hop_timer = 0;
        break;
    default:
        if (alloced)
            free(s);
        return NULL;
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) modem_connect_tones_tx_release(modem_connect_tones_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) modem_connect_tones_tx_free(modem_connect_tones_tx_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void report_tone_state(modem_connect_tones_rx_state_t *s, int tone, int level)
{
    if (tone != s->tone_present)
    {
        if (s->tone_callback)
        {
            s->tone_callback(s->callback_data, tone, level, 0);
        }
        else
        {
            if (tone != MODEM_CONNECT_TONES_NONE)
                s->hit = tone;
        }
        s->tone_present = tone;
    }
}
/*- End of function --------------------------------------------------------*/

static void v21_put_bit(void *user_data, int bit)
{
    modem_connect_tones_rx_state_t *s;

    s = (modem_connect_tones_rx_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions. */
        switch (bit)
        {
        case SIG_STATUS_CARRIER_DOWN:
            /* Only declare tone off, if we were the one to declare tone on. */
            if (s->tone_present == MODEM_CONNECT_TONES_FAX_PREAMBLE)
                report_tone_state(s, MODEM_CONNECT_TONES_NONE, -99);
            /* Fall through */
        case SIG_STATUS_CARRIER_UP:
            s->raw_bit_stream = 0;
            s->num_bits = 0;
            s->flags_seen = 0;
            s->framing_ok_announced = FALSE;
            break;
        }
        return;
    }
    /* Look for enough FAX V.21 message preamble (back to back HDLC flag octets) to be sure
       we are really seeing preamble, and declare the signal to be present. Any change from
       preamble declares the signal to not be present, though it will probably be the body
       of the messages following the preamble. */
    s->raw_bit_stream = (s->raw_bit_stream << 1) | ((bit << 8) & 0x100);
    s->num_bits++;
    if ((s->raw_bit_stream & 0x7F00) == 0x7E00)
    {
        if ((s->raw_bit_stream & 0x8000))
        {
            /* Hit HDLC abort */
            s->flags_seen = 0;
        }
        else
        {
            /* Hit HDLC flag */
            if (s->flags_seen < HDLC_FRAMING_OK_THRESHOLD)
            {
                /* Check the flags are back-to-back when testing for valid preamble. This
                   greatly reduces the chances of false preamble detection, and anything
                   which doesn't send them back-to-back is badly broken. */
                if (s->num_bits != 8)
                    s->flags_seen = 0;
                if (++s->flags_seen >= HDLC_FRAMING_OK_THRESHOLD  &&  !s->framing_ok_announced)
                {
                    report_tone_state(s, MODEM_CONNECT_TONES_FAX_PREAMBLE, lfastrintf(fsk_rx_signal_power(&(s->v21rx))));
                    s->framing_ok_announced = TRUE;
                }
            }
        }
        s->num_bits = 0;
    }
    else
    {
        if (s->flags_seen >= HDLC_FRAMING_OK_THRESHOLD)
        {
            if (s->num_bits == 8)
            {
                s->framing_ok_announced = FALSE;
                s->flags_seen = 0;
            }
        }
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) modem_connect_tones_rx(modem_connect_tones_rx_state_t *s,
                                                const int16_t amp[],
                                                int len)
{
    int i;
    int16_t notched;
    float v1;
    float famp;
    float filtered;

    switch (s->tone_type)
    {
    case MODEM_CONNECT_TONES_FAX_CNG:
        for (i = 0;  i < len;  i++)
        {
            famp = amp[i];
            /* A Cauer notch at 1100Hz, spread just wide enough to meet our detection bandwidth
               criteria. */
            /* Poles 0.736618498*exp(+-1047/4000 * PI * j)
               Zeroes exp(+-1099.5/4000 * PI * j) */
            v1 = 0.792928f*famp + 1.0018744927985f*s->znotch_1 - 0.54196833412465f*s->znotch_2;
            famp = v1 - 1.2994747954630f*s->znotch_1 + s->znotch_2;
            s->znotch_2 = s->znotch_1;
            s->znotch_1 = v1;
            notched = (int16_t) lfastrintf(famp);

            /* Estimate the overall energy in the channel, and the energy in
               the notch (i.e. overall channel energy - tone energy => noise).
               Use abs instead of multiply for speed (is it really faster?). */
            s->channel_level += ((abs(amp[i]) - s->channel_level) >> 5);
            s->notch_level += ((abs(notched) - s->notch_level) >> 5);
            if (s->channel_level > 70  &&  s->notch_level*6 < s->channel_level)
            {
                /* There is adequate energy in the channel, and it is mostly at 1100Hz. */
                if (s->tone_present != MODEM_CONNECT_TONES_FAX_CNG)
                {
                    if (++s->tone_cycle_duration >= ms_to_samples(415))
                        report_tone_state(s, MODEM_CONNECT_TONES_FAX_CNG, lfastrintf(log10f(s->channel_level/32768.0f)*20.0f + DBM0_MAX_POWER + 0.8f));
                }
            }
            else
            {
                /* If the signal looks wrong, even for a moment, we consider this the
                   end of the tone. */
                if (s->tone_present == MODEM_CONNECT_TONES_FAX_CNG)
                    report_tone_state(s, MODEM_CONNECT_TONES_NONE, -99);
                s->tone_cycle_duration = 0;
            }
        }
        break;
    case MODEM_CONNECT_TONES_FAX_PREAMBLE:
        /* Ignore any CED tone, and just look for V.21 preamble. */
        fsk_rx(&(s->v21rx), amp, len);
        break;
    case MODEM_CONNECT_TONES_FAX_CED_OR_PREAMBLE:
        /* Also look for V.21 preamble. A lot of machines don't send the 2100Hz burst. It
           might also not be seen all the way through the channel, due to switching delays. */
        fsk_rx(&(s->v21rx), amp, len);
        /* Now fall through and look for a 2100Hz tone */
    case MODEM_CONNECT_TONES_ANS:
        for (i = 0;  i < len;  i++)
        {
            famp = amp[i];
            /* A Cauer bandpass at 15Hz, with which we demodulate the AM signal. */
            /* Poles 0.9983989*exp(+-15/4000 * PI * j)
               Zeroes exp(0/4000 * PI * j) */
            v1 = fabs(famp) + 1.996667f*s->z15hz_1 - 0.9968004f*s->z15hz_2;
            filtered = 0.001599787f*(v1 - s->z15hz_2);
            s->z15hz_2 = s->z15hz_1;
            s->z15hz_1 = v1;
            s->am_level += abs(lfastrintf(filtered)) - (s->am_level >> 8);
            //printf("%9.1f %10.4f %9d %9d\n", famp, filtered, s->am_level, s->channel_level);
            /* A Cauer notch at 2100Hz, spread just wide enough to meet our detection bandwidth
               criteria. */
            /* Poles 0.7144255*exp(+-2105.612/4000 * PI * j)
               Zeroes exp(+-2099.9/4000 * PI * j) */
            v1 = 0.7552f*famp - 0.1183852f*s->znotch_1 - 0.5104039f*s->znotch_2;
            famp = v1 + 0.1567596f*s->znotch_1 + s->znotch_2;
            s->znotch_2 = s->znotch_1;
            s->znotch_1 = v1;
            notched = (int16_t) lfastrintf(famp);
            /* Estimate the overall energy in the channel, and the energy in
               the notch (i.e. overall channel energy - tone energy => noise).
               Use abs instead of multiply for speed (is it really faster?).
               Damp the overall energy a little more for a stable result.
               Damp the notch energy a little less, so we don't damp out the
               blip every time the phase reverses. */
            s->channel_level += ((abs(amp[i]) - s->channel_level) >> 5);
            s->notch_level += ((abs(notched) - s->notch_level) >> 4);
            /* This should cut off at about -43dBm0 */
            if (s->channel_level <= 70)
            {
                /* If the energy level is low, even for a moment, we consider this the
                   end of the tone. */
                if (s->tone_present != MODEM_CONNECT_TONES_NONE)
                    report_tone_state(s, MODEM_CONNECT_TONES_NONE, -99);
                s->tone_cycle_duration = 0;
                s->good_cycles = 0;
                s->tone_on = FALSE;
                continue;
            }
            /* There is adequate energy in the channel. Is it mostly at 2100Hz? */
            s->tone_cycle_duration++;
            if (s->notch_level*6 < s->channel_level)
            {
                /* The notch test says yes, so we have the tone. */
                /* We should get a kick from the notch filter every 450+-25ms, as the phase reverses, for an
                   EC disable tone. For a simple answer tone, the tone should persist unbroken for longer. */
                if (!s->tone_on)
                {
                    if (s->tone_cycle_duration >= ms_to_samples(450 - 25))
                    {
                        if (++s->good_cycles == 3)
                        {
                            report_tone_state(s,
                                              (s->am_level*15/256 > s->channel_level)  ?  MODEM_CONNECT_TONES_ANSAM_PR  :  MODEM_CONNECT_TONES_ANS_PR,
                                              lfastrintf(log10f(s->channel_level/32768.0f)*20.0f + DBM0_MAX_POWER + 0.8f));
                        }
                    }
                    else
                    {
                        s->good_cycles = 0;
                    }
                    /* Cycles are timed from rising edge to rising edge */
                    s->tone_cycle_duration = 0;
                }
                else
                {
                    if (s->tone_cycle_duration >= ms_to_samples(450 + 100))
                    {
                        if (s->tone_present == MODEM_CONNECT_TONES_NONE)
                        {
                            report_tone_state(s,
                                              (s->am_level*15/256 > s->channel_level)  ?  MODEM_CONNECT_TONES_ANSAM  :  MODEM_CONNECT_TONES_ANS,
                                              lfastrintf(log10f(s->channel_level/32768.0f)*20.0f + DBM0_MAX_POWER + 0.8f));
                        }
                        s->good_cycles = 0;
                        s->tone_cycle_duration = ms_to_samples(450 + 100);
                    }
                }
                s->tone_on = TRUE;
            }
            else if (s->notch_level*5 > s->channel_level)
            {
                if (s->tone_present == MODEM_CONNECT_TONES_ANS)
                {
                    report_tone_state(s, MODEM_CONNECT_TONES_NONE, -99);
                    s->good_cycles = 0;
                }
                else
                {
                    if (s->tone_cycle_duration >= ms_to_samples(450 + 25))
                    {
                        /* The change came too late for a cycle of ANS_PR tone */
                        if (s->tone_present == MODEM_CONNECT_TONES_ANS_PR  ||  s->tone_present == MODEM_CONNECT_TONES_ANSAM_PR)
                            report_tone_state(s, MODEM_CONNECT_TONES_NONE, -99);
                        s->good_cycles = 0;
                    }
                }
                s->tone_on = FALSE;
            }
        }
        break;
    case MODEM_CONNECT_TONES_BELL_ANS:
        for (i = 0;  i < len;  i++)
        {
            famp = amp[i];
            /* A Cauer notch at 2225Hz, spread just wide enough to meet our detection bandwidth
               criteria. */
            /* Poles 0.7144255*exp(+-2230.612/4000 * PI * j)
               Zeroes exp(+-2224.9/4000 * PI * j) */
            v1 = 0.739651f*famp - 0.257384f*s->znotch_1 - 0.510404f*s->znotch_2;
            famp = v1 + 0.351437f*s->znotch_1 + s->znotch_2;
            s->znotch_2 = s->znotch_1;
            s->znotch_1 = v1;
            notched = (int16_t) lfastrintf(famp);

            /* Estimate the overall energy in the channel, and the energy in
               the notch (i.e. overall channel energy - tone energy => noise).
               Use abs instead of multiply for speed (is it really faster?). */
            s->channel_level += ((abs(amp[i]) - s->channel_level) >> 5);
            s->notch_level += ((abs(notched) - s->notch_level) >> 5);
            if (s->channel_level > 70  &&  s->notch_level*6 < s->channel_level)
            {
                /* There is adequate energy in the channel, and it is mostly at 2225Hz. */
                if (s->tone_present != MODEM_CONNECT_TONES_BELL_ANS)
                {
                    if (++s->tone_cycle_duration >= ms_to_samples(415))
                        report_tone_state(s, MODEM_CONNECT_TONES_BELL_ANS, lfastrintf(log10f(s->channel_level/32768.0f)*20.0f + DBM0_MAX_POWER + 0.8f));
                }
            }
            else
            {
                /* If the signal looks wrong, even for a moment, we consider this the
                   end of the tone. */
                if (s->tone_present == MODEM_CONNECT_TONES_BELL_ANS)
                    report_tone_state(s, MODEM_CONNECT_TONES_NONE, -99);
                s->tone_cycle_duration = 0;
            }
        }
        break;
    case MODEM_CONNECT_TONES_CALLING_TONE:
        for (i = 0;  i < len;  i++)
        {
            famp = amp[i];
            /* A Cauer notch at 1300Hz, spread just wide enough to meet our detection bandwidth
               criteria. */
            /* Poles 0.736618498*exp(+-1247/4000 * PI * j)
               Zeroes exp(+-1299.5/4000 * PI * j) */
            v1 = 0.755582f*famp + 0.820887174515f*s->znotch_1 - 0.541968324778f*s->znotch_2;
            famp = v1 - 1.0456667108f*s->znotch_1 + s->znotch_2;
            s->znotch_2 = s->znotch_1;
            s->znotch_1 = v1;
            notched = (int16_t) lfastrintf(famp);

            /* Estimate the overall energy in the channel, and the energy in
               the notch (i.e. overall channel energy - tone energy => noise).
               Use abs instead of multiply for speed (is it really faster?). */
            s->channel_level += ((abs(amp[i]) - s->channel_level) >> 5);
            s->notch_level += ((abs(notched) - s->notch_level) >> 5);
            if (s->channel_level > 70  &&  s->notch_level*6 < s->channel_level)
            {
                /* There is adequate energy in the channel, and it is mostly at 1300Hz. */
                if (s->tone_present != MODEM_CONNECT_TONES_CALLING_TONE)
                {
                    if (++s->tone_cycle_duration >= ms_to_samples(415))
                        report_tone_state(s, MODEM_CONNECT_TONES_CALLING_TONE, lfastrintf(log10f(s->channel_level/32768.0f)*20.0f + DBM0_MAX_POWER + 0.8f));
                }
            }
            else
            {
                /* If the signal looks wrong, even for a moment, we consider this the
                   end of the tone. */
                if (s->tone_present == MODEM_CONNECT_TONES_CALLING_TONE)
                    report_tone_state(s, MODEM_CONNECT_TONES_NONE, -99);
                s->tone_cycle_duration = 0;
            }
        }
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) modem_connect_tones_rx_get(modem_connect_tones_rx_state_t *s)
{
    int x;
    
    x = s->hit;
    s->hit = MODEM_CONNECT_TONES_NONE;
    return x;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(modem_connect_tones_rx_state_t *) modem_connect_tones_rx_init(modem_connect_tones_rx_state_t *s,
                                                                           int tone_type,
                                                                           tone_report_func_t tone_callback,
                                                                           void *user_data)
{
    if (s == NULL)
    {
        if ((s = (modem_connect_tones_rx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }

    s->tone_type = tone_type;
    switch (s->tone_type)
    {
    case MODEM_CONNECT_TONES_FAX_PREAMBLE:
    case MODEM_CONNECT_TONES_FAX_CED_OR_PREAMBLE:
        fsk_rx_init(&(s->v21rx), &preset_fsk_specs[FSK_V21CH2], FSK_FRAME_MODE_SYNC, v21_put_bit, s);
        fsk_rx_signal_cutoff(&(s->v21rx), -45.5f);
        break;
    case MODEM_CONNECT_TONES_ANS_PR:
    case MODEM_CONNECT_TONES_ANSAM:
    case MODEM_CONNECT_TONES_ANSAM_PR:
        /* Treat these all the same for receive purposes */
        s->tone_type = MODEM_CONNECT_TONES_ANS;
        break;
    }
    s->channel_level = 0;
    s->notch_level = 0;
    s->am_level = 0;
    s->tone_present = MODEM_CONNECT_TONES_NONE;
    s->tone_cycle_duration = 0;
    s->good_cycles = 0;
    s->hit = MODEM_CONNECT_TONES_NONE;
    s->tone_on = FALSE;
    s->tone_callback = tone_callback;
    s->callback_data = user_data;
    s->znotch_1 = 0.0f;
    s->znotch_2 = 0.0f;
    s->z15hz_1 = 0.0f;
    s->z15hz_2 = 0.0f;
    s->num_bits = 0;
    s->flags_seen = 0;
    s->framing_ok_announced = FALSE;
    s->raw_bit_stream = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) modem_connect_tones_rx_release(modem_connect_tones_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) modem_connect_tones_rx_free(modem_connect_tones_rx_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
