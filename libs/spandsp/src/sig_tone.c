/*
 * SpanDSP - a series of DSP components for telephony
 *
 * sig_tone.c - Signalling tone processing for the 2280Hz, 2400Hz, 2600Hz
 *              and similar signalling tones used in older protocols.
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

#include <stdlib.h>
#include <inttypes.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <memory.h>
#include <string.h>
#include <limits.h>

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/saturated.h"
#include "spandsp/vector_int.h"
#include "spandsp/complex.h"
#include "spandsp/power_meter.h"
#include "spandsp/dds.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/sig_tone.h"

#include "spandsp/private/sig_tone.h"

/*! PI */
#define PI 3.14159265358979323

enum
{
    NOTCH_COEFF_SET_2280HZ = 0,
    NOTCH_COEFF_SET_2400HZ,
    NOTCH_COEFF_SET_2600HZ
};

/* The coefficients for the data notch filters. These filters are also the
   guard filters for tone detection. */
static const sig_tone_notch_coeffs_t notch_coeffs[3] =
{
    {                                                   /* 2280 Hz */
#if defined(SPANDSP_USE_FIXED_POINT)
        {  3600,        14397,          32767},
        {     0,        -9425,         -28954},
        {     0,        14196,          32767},
        {     0,       -17393,         -28954},
        12,
#else
        {0.878906f,     0.439362f,      1.0f},
        {0.0f,         -0.287627f,     -0.883605f},
        {0.0f,          0.433228f,      1.0f},
        {0.0f,         -0.530792f,     -0.883605f},
#endif
    },
    {                                                   /* 2400Hz */
#if defined(SPANDSP_USE_FIXED_POINT)
        {  3530,        20055,          32767},
        {     0,       -14950,         -28341},
        {     0,        20349,          32767},
        {     0,       -22633,         -28341},
        12,
#else
        {0.862000f,     0.612055f,      1.0f},
        {0.0f,         -0.456264f,     -0.864899f},
        {0.0f,          0.621021f,      1.0f},
        {0.0f,         -0.690738f,     -0.864899f},
#endif
    },
    {                                                   /* 2600Hz */
#if defined(SPANDSP_USE_FIXED_POINT)
        {  3530,        29569,          32767},
        {     0,       -24010,         -28341},
        {     0,        29844,          32767},
        {     0,       -31208,         -28341},
        12,
#else
        {0.862000f,     0.902374f,      1.0f},
        {0.0f,         -0.732727f,     -0.864899f},
        {0.0f,          0.910766f,      1.0f},
        {0.0f,         -0.952393f,     -0.864899f},
#endif
    }
};

static const sig_tone_flat_coeffs_t flat_coeffs[1] =
{
    {
#if defined(SPANDSP_USE_FIXED_POINT)
        { 12900,       -16384,         -16384}, 
        {     0,        -8578,         -11796},
        15,
#else
        {0.393676f,    -0.5f,          -0.5f}, 
        {0.0f,         -0.261778f,     -0.359985f},
#endif
    }
};

static const sig_tone_descriptor_t sig_tones[3] =
{
    {
        /* 2280Hz (e.g. AC15, and many other European protocols) */
        {2280,  0},
        {{-10, -20}, {0, 0}},       /* -10+-1 dBm0 and -20+-1 dBm0 */
        ms_to_samples(400),         /* High to low timout - 300ms to 550ms */
        ms_to_samples(225),         /* Sharp to flat timeout */
        ms_to_samples(225),         /* Notch insertion timeout */
    
        ms_to_samples(3),           /* Tone on persistence check */
        ms_to_samples(8),           /* Tone off persistence check */

        1,
        {
            &notch_coeffs[NOTCH_COEFF_SET_2280HZ],
            NULL,
        },
        &flat_coeffs[NOTCH_COEFF_SET_2280HZ],

        13.0f,
        -30.0f,
        -30.0f
    },
    {
        /* 2600Hz (e.g. many US protocols) */
        {2600, 0},
        {{-8, -8}, {0, 0}},
        ms_to_samples(0),
        ms_to_samples(0),
        ms_to_samples(225),
    
        ms_to_samples(3),
        ms_to_samples(8),

        1,
        {
            &notch_coeffs[NOTCH_COEFF_SET_2600HZ],
            NULL,
        },
        NULL,
    
        15.6f,
        -30.0f,
        -30.0f
    },
    {
        /* 2400Hz/2600Hz (e.g. SS5 and SS5bis) */
        {2400, 2600},
        {{-8, -8}, {-8, -8}},
        ms_to_samples(0),
        ms_to_samples(0),
        ms_to_samples(225),

        ms_to_samples(3),
        ms_to_samples(8),

        2,
        {
            &notch_coeffs[NOTCH_COEFF_SET_2400HZ],
            &notch_coeffs[NOTCH_COEFF_SET_2600HZ]
        },
        NULL,
    
        15.6f,
        -30.0f,
        -30.0f
    }
};

static const int tone_present_bits[3] =
{
    SIG_TONE_1_PRESENT,
    SIG_TONE_2_PRESENT,
    SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT
};

static const int tone_change_bits[3] =
{
    SIG_TONE_1_CHANGE,
    SIG_TONE_2_CHANGE,
    SIG_TONE_1_CHANGE | SIG_TONE_2_CHANGE
};

static const int coeff_sets[3] =
{
    0,
    1,
    0
};

SPAN_DECLARE(int) sig_tone_tx(sig_tone_tx_state_t *s, int16_t amp[], int len)
{
    int i;
    int j;
    int k;
    int n;
    int16_t tone;
    int need_update;
    int high_low;

    for (i = 0;  i < len;  i += n)
    {
        if (s->current_tx_timeout)
        {
            if (s->current_tx_timeout <= len - i)
            {
                n = s->current_tx_timeout;
                need_update = TRUE;
            }
            else
            {
                n = len - i;
                need_update = FALSE;
            }
            s->current_tx_timeout -= n;
        }
        else
        {
            n = len - i;
            need_update = FALSE;
        }
        if (!(s->current_tx_tone & SIG_TONE_TX_PASSTHROUGH))
            vec_zeroi16(&amp[i], n);
        /*endif*/
        if ((s->current_tx_tone & (SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT)))
        {
            /* Are we in the early phase (high tone energy level), or the sustaining
               phase (low tone energy level) of tone generation? */
            /* This doesn't try to get the high/low timing precise, as there is no
               value in doing so. It works block by block, and the blocks are normally
               quite short. */
            if (s->high_low_timer > 0)
            {
                if (n > s->high_low_timer)
                    n = s->high_low_timer;
                s->high_low_timer -= n;
                high_low = 0;
            }
            else
            {
                high_low = 1;
            }
            /*endif*/
            for (k = 0;  k < s->desc->tones;  k++)
            {
                if ((s->current_tx_tone & tone_present_bits[k])  &&  s->phase_rate[k])
                {
                    for (j = i;  j < i + n;  j++)
                    {
                        tone = dds_mod(&(s->phase_acc[k]), s->phase_rate[k], s->tone_scaling[k][high_low], 0);
                        amp[j] = saturated_add16(amp[j], tone);
                    }
                    /*endfor*/
                }
                /*endif*/
            }
        }
        /*endif*/
        if (need_update  &&  s->sig_update)
            s->sig_update(s->user_data, SIG_TONE_TX_UPDATE_REQUEST, 0, 0);
        /*endif*/
    }
    /*endfor*/
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) sig_tone_tx_set_mode(sig_tone_tx_state_t *s, int mode, int duration)
{
    int old_tones;
    int new_tones;
    
    old_tones = s->current_tx_tone & (SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT);
    new_tones = mode & (SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT);
    if (new_tones  &&  old_tones != new_tones)
        s->high_low_timer = s->desc->high_low_timeout;
    /*endif*/
    /* If a tone is being turned on, let's start the phase from zero */
    if ((mode & SIG_TONE_1_PRESENT)  &&  !(s->current_tx_tone & SIG_TONE_1_PRESENT))
        s->phase_acc[0] = 0;
    if ((mode & SIG_TONE_2_PRESENT)  &&  !(s->current_tx_tone & SIG_TONE_2_PRESENT))
        s->phase_acc[1] = 0;
    s->current_tx_tone = mode;
    s->current_tx_timeout = duration;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(sig_tone_tx_state_t *) sig_tone_tx_init(sig_tone_tx_state_t *s, int tone_type, tone_report_func_t sig_update, void *user_data)
{
    int i;

    if (sig_update == NULL  ||  tone_type < 1  ||  tone_type > 3)
        return NULL;
    /*endif*/

    if (s == NULL)
    {
        if ((s = (sig_tone_tx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->sig_update = sig_update;
    s->user_data = user_data;

    s->desc = &sig_tones[tone_type - 1];

    for (i = 0;  i < 2;  i++)
    {
        if (s->desc->tone_freq[i])
            s->phase_rate[i] = dds_phase_rate((float) s->desc->tone_freq[i]);
        else
            s->phase_rate[i] = 0;
        s->tone_scaling[i][0] = dds_scaling_dbm0((float) s->desc->tone_amp[i][0]);
        s->tone_scaling[i][1] = dds_scaling_dbm0((float) s->desc->tone_amp[i][1]);
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sig_tone_tx_release(sig_tone_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sig_tone_tx_free(sig_tone_tx_state_t *s)
{
    if (s)
        free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int nnn = 0;

SPAN_DECLARE(int) sig_tone_rx(sig_tone_rx_state_t *s, int16_t amp[], int len)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    int16_t x;
    int32_t v;
    int16_t notched_signal[3];
    int16_t bandpass_signal;
    int16_t signal;
#else
    float x;
    float v;
    float notched_signal[3];
    float bandpass_signal;
    float signal;
#endif
    int i;
    int j;
    int k;
    int l;
    int m;
    int32_t notch_power[3];
    int32_t flat_power;
    int immediate;

    l = s->desc->tones;
    if (l == 2)
        l = 3;
    notch_power[1] =
    notch_power[2] = INT32_MAX;
    for (i = 0;  i < len;  i++)
    {
        if (s->signalling_state_duration < INT_MAX)
            s->signalling_state_duration++;
        /*endif*/
        signal = amp[i];
        for (j = 0;  j < l;  j++)
        {
            k = coeff_sets[j];
            /* The notch filter is two cascaded biquads. */
#if defined(SPANDSP_USE_FIXED_POINT)
            v = ((int32_t) signal*s->desc->notch[k]->a1[0])
              + ((int32_t) s->tone[j].notch_z1[0]*s->desc->notch[k]->b1[1])
              + ((int32_t) s->tone[j].notch_z1[1]*s->desc->notch[k]->b1[2]);
            x = v >> 15;
            v +=   ((int32_t) s->tone[j].notch_z1[0]*s->desc->notch[k]->a1[1])
                 + ((int32_t) s->tone[j].notch_z1[1]*s->desc->notch[k]->a1[2]);
            s->tone[j].notch_z1[1] = s->tone[j].notch_z1[0];
            s->tone[j].notch_z1[0] = x;
            v +=   ((int32_t) s->tone[j].notch_z2[0]*s->desc->notch[k]->b2[1])
                 + ((int32_t) s->tone[j].notch_z2[1]*s->desc->notch[k]->b2[2]);
            x = v >> 15;
            v +=   ((int32_t) s->tone[j].notch_z2[0]*s->desc->notch[k]->a2[1])
                 + ((int32_t) s->tone[j].notch_z2[1]*s->desc->notch[k]->a2[2]);
            s->tone[j].notch_z2[1] = s->tone[j].notch_z2[0];
            s->tone[j].notch_z2[0] = x;
            notched_signal[j] = v >> s->desc->notch[k]->postscale;
#else
            v = signal*s->desc->notch[k]->a1[0]
              + s->tone[j].notch_z1[0]*s->desc->notch[k]->b1[1]
              + s->tone[j].notch_z1[1]*s->desc->notch[k]->b1[2];
            x = v;
            v +=   s->tone[j].notch_z1[0]*s->desc->notch[k]->a1[1]
                 + s->tone[j].notch_z1[1]*s->desc->notch[k]->a1[2];
            s->tone[j].notch_z1[1] = s->tone[j].notch_z1[0];
            s->tone[j].notch_z1[0] = x;
            v +=   s->tone[j].notch_z2[0]*s->desc->notch[k]->b2[1]
                 + s->tone[j].notch_z2[1]*s->desc->notch[k]->b2[2];
            x = v;
            v +=   s->tone[j].notch_z2[0]*s->desc->notch[k]->a2[1]
                 + s->tone[j].notch_z2[1]*s->desc->notch[k]->a2[2];
            s->tone[j].notch_z2[1] = s->tone[j].notch_z2[0];
            s->tone[j].notch_z2[0] = x;
            notched_signal[j] = v;
#endif
            /* Modulus and leaky integrate the notched data. The result of
               this isn't used in low tone detect mode, but we must keep the
               power measurement rolling along. */
            notch_power[j] = power_meter_update(&s->tone[j].power, notched_signal[j]);
            if (j == 1)
                signal = notched_signal[j];
        }
        if ((s->signalling_state & (SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT)))
        {
            if (s->flat_mode_timeout  &&  --s->flat_mode_timeout == 0)
                s->flat_mode = TRUE;
            /*endif*/
        }
        else
        {
            s->flat_mode_timeout = s->desc->sharp_flat_timeout;
            s->flat_mode = FALSE;
        }
        /*endif*/

        immediate = -1;
        if (s->flat_mode)
        {
            //printf("Flat mode %d %d\n", s->flat_mode_timeout, s->desc->sharp_flat_timeout);
            /* Flat mode */
            bandpass_signal = amp[i];
            if (s->desc->flat)
            {
                /* The bandpass filter is a single bi-quad stage */
#if defined(SPANDSP_USE_FIXED_POINT)
                v = ((int32_t) amp[i]*s->desc->flat->a[0])
                  + ((int32_t) s->flat_z[0]*s->desc->flat->b[1])
                  + ((int32_t) s->flat_z[1]*s->desc->flat->b[2]);
                x = v >> 15;
                v +=   ((int32_t) s->flat_z[0]*s->desc->flat->a[1])
                     + ((int32_t) s->flat_z[1]*s->desc->flat->a[2]);
                s->flat_z[1] = s->flat_z[0];
                s->flat_z[0] = x;
                bandpass_signal = v >> s->desc->flat->postscale;
#else
                v = amp[i]*s->desc->flat->a[0]
                  + s->flat_z[0]*s->desc->flat->b[1]
                  + s->flat_z[1]*s->desc->flat->b[2];
                x = v;
                v +=   s->flat_z[0]*s->desc->flat->a[1]
                     + s->flat_z[1]*s->desc->flat->a[2];
                s->flat_z[1] = s->flat_z[0];
                s->flat_z[0] = x;
                bandpass_signal = v;
#endif
            }
            flat_power = power_meter_update(&s->flat_power, bandpass_signal);
    
            /* For the flat receiver we use a simple power threshold! */
            if ((s->signalling_state & (SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT)))
            {
                if (flat_power < s->flat_detection_threshold)
                {
                    s->signalling_state &= ~tone_present_bits[0];
                    s->signalling_state |= tone_change_bits[0];
                }
                /*endif*/
            }
            else
            {
                if (flat_power > s->flat_detection_threshold)
                    s->signalling_state |= (tone_present_bits[0] | tone_change_bits[0]);
                /*endif*/
            }
            /*endif*/

            /* Notch insertion logic */
            /* tone_present and tone_on are equivalent in flat mode */
            if ((s->signalling_state & (SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT)))
            {
                s->notch_insertion_timeout = s->desc->notch_lag_time;
            }
            else
            {
                if (s->notch_insertion_timeout)
                    s->notch_insertion_timeout--;
                /*endif*/
            }
            /*endif*/
        }
        else
        {
            /* Sharp mode */
            flat_power = power_meter_update(&s->flat_power, amp[i]);

            /* Persistence checking and notch insertion logic */
            if (flat_power >= s->sharp_detection_threshold)
            {
                /* Which is the better of the single tone responses? */
                m = (notch_power[0] < notch_power[1])  ?  0  :  1;
                /* Single tone has precedence. If the better one fails to detect, try
                   for a dual tone signal. */
                if ((notch_power[m] >> 6)*s->detection_ratio < (flat_power >> 6))
                    immediate = m;
                else if ((notch_power[2] >> 6)*s->detection_ratio < (flat_power >> 7))
                    immediate = 2;
            }
            //printf("Immediate = %d  %d   %d\n", immediate, s->signalling_state, s->tone_persistence_timeout);
            if ((s->signalling_state & (SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT)))
            {
                if (immediate != s->current_notch_filter)
                {
                    /* No tone is detected this sample */
                    if (--s->tone_persistence_timeout == 0)
                    {
                        /* Tone off is confirmed */
                        s->tone_persistence_timeout = s->desc->tone_on_check_time;
                        s->signalling_state |= ((s->signalling_state & (SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT)) << 1);
                        s->signalling_state &= ~(SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT);
                    }
                    /*endif*/
                }
                else
                {
                    s->tone_persistence_timeout = s->desc->tone_off_check_time;
                }
                /*endif*/
            }
            else
            {
                if (s->notch_insertion_timeout)
                    s->notch_insertion_timeout--;
                /*endif*/
                if (immediate >= 0  &&  immediate == s->last_sample_tone_present)
                {
                    /* Consistent tone detected this sample */
                    if (--s->tone_persistence_timeout == 0)
                    {
                        /* Tone on is confirmed */
                        s->tone_persistence_timeout = s->desc->tone_off_check_time;
                        s->notch_insertion_timeout = s->desc->notch_lag_time;
                        s->signalling_state |= (tone_present_bits[immediate] | tone_change_bits[immediate]);
                        s->current_notch_filter = immediate;
                    }
                    /*endif*/
                }
                else
                {
                    s->tone_persistence_timeout = s->desc->tone_on_check_time;
                }
                /*endif*/
            }
            /*endif*/
            //printf("XXX %d %d %d %d %d %d\n", nnn++, notch_power[0], notch_power[1], notch_power[2], flat_power, immediate*10000000);
        }
        /*endif*/
        if ((s->signalling_state & (SIG_TONE_1_CHANGE | SIG_TONE_2_CHANGE)))
        {
            if (s->sig_update)
                s->sig_update(s->user_data, s->signalling_state, 0, s->signalling_state_duration);
            /*endif*/
            s->signalling_state &= ~(SIG_TONE_1_CHANGE | SIG_TONE_2_CHANGE);
            s->signalling_state_duration = 0;
        }
        /*endif*/

        if ((s->current_rx_tone & SIG_TONE_RX_PASSTHROUGH))
        {
            if ((s->current_rx_tone & SIG_TONE_RX_FILTER_TONE)  ||  s->notch_insertion_timeout)
#if defined(SPANDSP_USE_FIXED_POINT)
                amp[i] = saturate16(notched_signal[s->current_notch_filter]);
#else
                amp[i] = fsaturatef(notched_signal[s->current_notch_filter]);
#endif
            /*endif*/
        }
        else
        {
            /* Simply mute the media path */
            amp[i] = 0;
        }
        /*endif*/
        s->last_sample_tone_present = immediate;
    }
    /*endfor*/
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) sig_tone_rx_set_mode(sig_tone_rx_state_t *s, int mode, int duration)
{
    s->current_rx_tone = mode;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(sig_tone_rx_state_t *) sig_tone_rx_init(sig_tone_rx_state_t *s, int tone_type, tone_report_func_t sig_update, void *user_data)
{
    int i;
#if !defined(SPANDSP_USE_FIXED_POINT)
    int j;
#endif
    
    if (sig_update == NULL  ||  tone_type < 1  ||  tone_type > 3)
        return NULL;
    /*endif*/

    if (s == NULL)
    {
        if ((s = (sig_tone_rx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
#if !defined(SPANDSP_USE_FIXED_POINT)
    for (j = 0;  j < 3;  j++)
    {
        for (i = 0;  i < 2;  i++)
        {
            s->tone[j].notch_z1[i] = 0.0f;
            s->tone[j].notch_z2[i] = 0.0f;
        }
    }
    for (i = 0;  i < 2;  i++)
        s->flat_z[i] = 0.0f;
#endif
    s->last_sample_tone_present = -1;

    s->sig_update = sig_update;
    s->user_data = user_data;

    s->desc = &sig_tones[tone_type - 1];

    for (i = 0;  i < 3;  i++)
        power_meter_init(&s->tone[i].power, 5);
    power_meter_init(&s->flat_power, 5);

    s->flat_detection_threshold = power_meter_level_dbm0(s->desc->flat_detection_threshold);
    s->sharp_detection_threshold = power_meter_level_dbm0(s->desc->sharp_detection_threshold);
    s->detection_ratio = powf(10.0f, s->desc->detection_ratio/10.0f) + 1.0f;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sig_tone_rx_release(sig_tone_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) sig_tone_rx_free(sig_tone_rx_state_t *s)
{
    if (s)
        free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
