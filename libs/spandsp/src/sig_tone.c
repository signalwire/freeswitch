/*
 * SpanDSP - a series of DSP components for telephony
 *
 * sig_tone.c - Signalling tone processing for the 2280Hz, 2600Hz and similar
 *              signalling tone used in older protocols.
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
 * $Id: sig_tone.c,v 1.33 2009/09/04 14:38:46 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
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

#undef SPANDSP_USE_FIXED_POINT
#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/dc_restore.h"
#include "spandsp/saturated.h"
#include "spandsp/vector_int.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/sig_tone.h"

#include "spandsp/private/sig_tone.h"

/*! PI */
#define PI 3.14159265358979323

/* The coefficients for the data notch filter. This filter is also the
   guard filter for tone detection. */

sig_tone_descriptor_t sig_tones[4] =
{
    {
        /* 2280Hz (e.g. AC15, and many other European protocols) */
        {2280,  0},
        {{-10, -20}, {0, 0}},       /* -10+-1 dBmO and -20+-1 dBm0 */
        ms_to_samples(400),         /* 300ms to 550ms */
    
        ms_to_samples(225),
    
        ms_to_samples(225),
        TRUE,
    
        24,
        64,

        1,
        {
            {
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
            {
#if defined(SPANDSP_USE_FIXED_POINT)
                {     0,            0,              0},
                {     0,            0,              0},
                {     0,            0,              0},
                {     0,            0,              0},
                0,
#else
                {0.0f,          0.0f,           0.0f},
                {0.0f,          0.0f,           0.0f},
                {0.0f,          0.0f,           0.0f},
                {0.0f,          0.0f,           0.0f},
#endif
            }
        },
#if defined(SPANDSP_USE_FIXED_POINT)
        { 12900,       -16384,         -16384}, 
        {     0,        -8578,         -11796},
        15,
#else
        {0.393676f,    -0.5f,          -0.5f}, 
        {0.0f,         -0.261778f,     -0.359985f},
#endif

        31744,
        1024,
    
        31744,
        187,
    
        31744,
        187,
    
        -1,
        -32,
    
        57
    },
    {
        /* 2600Hz (e.g. many US protocols) */
        {2600, 0},
        {{-8, -8}, {0, 0}},
        ms_to_samples(400),
    
        ms_to_samples(225),
    
        ms_to_samples(225),
        FALSE,
    
        24,
        64,

        1,
        {
            {            
#if defined(SPANDSP_USE_FIXED_POINT)
                {  3539,        29569,          32767},
                {     0,       -24010,         -28341},
                {     0,        29844,          32767},
                {     0,       -31208,         -28341},
                12,
#else
                {0.864014f,     0.902374f,      1.0f},
                {0.0f,         -0.732727f,     -0.864899f},
                {0.0f,          0.910766f,      1.0f},
                {0.0f,         -0.952393f,     -0.864899f},
#endif
            },
            {            
#if defined(SPANDSP_USE_FIXED_POINT)
                {     0,            0,              0},
                {     0,            0,              0},
                {     0,            0,              0},
                {     0,            0,              0},
                0,
#else
                {0.0f,          0.0f,           0.0f},
                {0.0f,          0.0f,           0.0f},
                {0.0f,          0.0f,           0.0f},
                {0.0f,          0.0f,           0.0f},
#endif
            }
        },
#if defined(SPANDSP_USE_FIXED_POINT)
        { 32768,            0,              0},
        {     0,            0,              0},
        15,
#else
        {1.0f,          0.0f,           0.0f},
        {0.0f,          0.0f,           0.0f},
#endif
    
        31744,
        1024,
    
        31744,
        170,
    
        31744,
        170,
    
        -1,
        -32,
    
        52
    },
    {
        /* 2400Hz/2600Hz (e.g. SS5 and SS5bis) */
        {2600, 2400},
        {{-8, -8}, {-8, -8}},
        ms_to_samples(400),

        ms_to_samples(225),

        ms_to_samples(225),
        FALSE,

        24,
        64,

        2,
        {
            {
#if defined(SPANDSP_USE_FIXED_POINT)
                {  3539,        29569,          32767},
                {     0,       -24010,         -28341},
                {     0,        29844,          32767},
                {     0,       -31208,         -28341},
                12,
#else
                {0.864014f,     0.902374f,      1.0f},
                {0.0f,         -0.732727f,     -0.864899f},
                {0.0f,          0.910766f,      1.0f},
                {0.0f,         -0.952393f,     -0.864899f},
#endif
            },
            {
#if defined(SPANDSP_USE_FIXED_POINT)
                {  3539,        20349,          32767},
                {     0,       -22075,         -31856},
                {     0,        20174,          32767},
                {     0,       -17832,         -31836},
                12,
#else
                {0.864014f,     0.621007f,      1.0f},
                {0.0f,         -0.673667f,     -0.972167f},
                {0.0f,          0.615669f,      1.0f},
                {0.0f,         -0.544180f,     -0.971546f},
#endif
            }
        },
#if defined(SPANDSP_USE_FIXED_POINT)
        { 32768,            0,              0},
        {     0,            0,              0},
        15,
#else
        {1.0f,          0.0f,           0.0f},
        {0.0f,          0.0f,           0.0f},
#endif
    
        31744,
        1024,
    
        31744,
        170,
    
        31744,
        170,
    
        -1,
        -32,
    
        52
    }
};

SPAN_DECLARE(int) sig_tone_tx(sig_tone_tx_state_t *s, int16_t amp[], int len)
{
    int i;
    int j;
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
        if ((s->current_tx_tone & (SIG_TONE_1_PRESENT  ||  SIG_TONE_2_PRESENT)))
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
            if ((s->current_tx_tone & SIG_TONE_1_PRESENT)  &&  s->phase_rate[0])
            {
                for (j = i;  j < i + n;  j++)
                {
                    tone = dds_mod(&(s->phase_acc[0]), s->phase_rate[0], s->tone_scaling[0][high_low], 0);
                    amp[j] = saturate(amp[j] + tone);
                }
                /*endfor*/
            }
            /*endif*/
            if ((s->current_tx_tone & SIG_TONE_2_PRESENT)  &&  s->phase_rate[1])
            {
                for (j = i;  j < i + n;  j++)
                {
                    tone = dds_mod(&(s->phase_acc[1]), s->phase_rate[1], s->tone_scaling[1][high_low], 0);
                    amp[j] = saturate(amp[j] + tone);
                }
                /*endfor*/
            }
            /*endif*/
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

SPAN_DECLARE(int) sig_tone_rx(sig_tone_rx_state_t *s, int16_t amp[], int len)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t x;
    int32_t notched_signal;
    int32_t bandpass_signal;
#else
    float x;
    float notched_signal;
    float bandpass_signal;
#endif
    int i;
    int j;
    int32_t mown_notch[2];
    int32_t mown_bandpass;

    for (i = 0;  i < len;  i++)
    {
        if (s->signaling_state_duration < INT_MAX)
            s->signaling_state_duration++;
        /*endif*/
        notched_signal = 0;
        for (j = 0;  j < s->desc->tones;  j++)
        {
            /* The notch filter is two cascaded biquads. */
            notched_signal = amp[i];

#if defined(SPANDSP_USE_FIXED_POINT)
            notched_signal *= s->desc->tone[j].notch_a1[0];
            notched_signal += s->tone[j].notch_z1[1]*s->desc->tone[j].notch_b1[1];
            notched_signal += s->tone[j].notch_z1[2]*s->desc->tone[j].notch_b1[2];
            x = notched_signal;
            notched_signal += s->tone[j].notch_z1[1]*s->desc->tone[j].notch_a1[1];
            notched_signal += s->tone[j].notch_z1[2]*s->desc->tone[j].notch_a1[2];
            s->tone[j].notch_z1[2] = s->tone[j].notch_z1[1];
            s->tone[j].notch_z1[1] = x >> 15;

            notched_signal += s->tone[j].notch_z2[1]*s->desc->tone[j].notch_b2[1];
            notched_signal += s->tone[j].notch_z2[2]*s->desc->tone[j].notch_b2[2];
            x = notched_signal;
            notched_signal += s->tone[j].notch_z2[1]*s->desc->tone[j].notch_a2[1];
            notched_signal += s->tone[j].notch_z2[2]*s->desc->tone[j].notch_a2[2];
            s->tone[j].notch_z2[2] = s->tone[j].notch_z2[1];
            s->tone[j].notch_z2[1] = x >> 15;

            notched_signal >>= s->desc->notch_postscale;
#else
            notched_signal *= s->desc->tone[j].notch_a1[0];
            notched_signal += s->tone[j].notch_z1[1]*s->desc->tone[j].notch_b1[1];
            notched_signal += s->tone[j].notch_z1[2]*s->desc->tone[j].notch_b1[2];
            x = notched_signal;
            notched_signal += s->tone[j].notch_z1[1]*s->desc->tone[j].notch_a1[1];
            notched_signal += s->tone[j].notch_z1[2]*s->desc->tone[j].notch_a1[2];
            s->tone[j].notch_z1[2] = s->tone[j].notch_z1[1];
            s->tone[j].notch_z1[1] = x;

            notched_signal += s->tone[j].notch_z2[1]*s->desc->tone[j].notch_b2[1];
            notched_signal += s->tone[j].notch_z2[2]*s->desc->tone[j].notch_b2[2];
            x = notched_signal;
            notched_signal += s->tone[j].notch_z2[1]*s->desc->tone[j].notch_a2[1];
            notched_signal += s->tone[j].notch_z2[2]*s->desc->tone[j].notch_a2[2];
            s->tone[j].notch_z2[2] = s->tone[j].notch_z2[1];
            s->tone[j].notch_z2[1] = x;
#endif
            /* Modulus and leaky integrate the notched data. The result of
               this isn't used in low tone detect mode, but we must keep notch_zl
               rolling along. */
            s->tone[j].notch_zl = ((s->tone[j].notch_zl*s->desc->notch_slugi) >> 15)
                                + ((abs((int) notched_signal)*s->desc->notch_slugp) >> 15);
            /* Mow the grass to weed out the noise! */
            mown_notch[j] = s->tone[0].notch_zl & s->desc->notch_threshold;
        }

        if (s->tone_present)
        {
            if (s->flat_mode_timeout <= 0)
                s->flat_mode = TRUE;
            else
                s->flat_mode_timeout--;
            /*endif*/
        }
        else
        {
            s->flat_mode_timeout = s->desc->sharp_flat_timeout;
            s->flat_mode = FALSE;
        }
        /*endif*/

        if (s->flat_mode)
        {
            /* Flat mode */
    
            /* The bandpass filter is a single bi-quad stage */
            bandpass_signal = amp[i];
#if defined(SPANDSP_USE_FIXED_POINT)
            bandpass_signal *= s->desc->broad_a[0];
            bandpass_signal += s->broad_z[1]*s->desc->broad_b[1];
            bandpass_signal += s->broad_z[2]*s->desc->broad_b[2];
            x = bandpass_signal;
            bandpass_signal += s->broad_z[1]*s->desc->broad_a[1];
            bandpass_signal += s->broad_z[2]*s->desc->broad_a[2];
            s->broad_z[2] = s->broad_z[1];
            s->broad_z[1] = x >> 15;
            bandpass_signal >>= s->desc->broad_postscale;
#else
            bandpass_signal *= s->desc->broad_a[0];
            bandpass_signal += s->broad_z[1]*s->desc->broad_b[1];
            bandpass_signal += s->broad_z[2]*s->desc->broad_b[2];
            x = bandpass_signal;
            bandpass_signal += s->broad_z[1]*s->desc->broad_a[1];
            bandpass_signal += s->broad_z[2]*s->desc->broad_a[2];
            s->broad_z[2] = s->broad_z[1];
            s->broad_z[1] = x;
#endif            
            /* Leaky integrate the bandpassed data */
            s->broad_zl = ((s->broad_zl*s->desc->broad_slugi) >> 15)
                        + ((abs((int) bandpass_signal)*s->desc->broad_slugp) >> 15);
    
            /* For the broad band receiver we use a simple linear threshold! */
            if (s->tone_present)
            {
                s->tone_present = (s->broad_zl > s->desc->broad_threshold);
                if (!s->tone_present)
                {
                    if (s->sig_update)
                        s->sig_update(s->user_data, SIG_TONE_1_CHANGE, 0, s->signaling_state_duration);
                    /*endif*/
                    s->signaling_state_duration = 0;
                }
                /*endif*/
            }
            else
            {
                s->tone_present = (s->broad_zl > s->desc->broad_threshold);
                if (s->tone_present)
                {
                    if (s->sig_update)
                        s->sig_update(s->user_data, SIG_TONE_1_CHANGE | SIG_TONE_1_PRESENT, 0, s->signaling_state_duration);
                    /*endif*/
                    s->signaling_state_duration = 0;
                }
                /*endif*/
            }
            /*endif*/

            /* Notch insertion logic */    
            /* tone_present and tone_on are equivalent in flat mode */
            if (s->tone_present)
            {
                s->notch_enabled = s->desc->notch_allowed;
                s->notch_insertion_timeout = s->desc->notch_lag_time;
            }
            else
            {
                if (s->notch_insertion_timeout > 0)
                    s->notch_insertion_timeout--;
                else
                    s->notch_enabled = FALSE;
                /*endif*/
            }
            /*endif*/
        }
        else
        {
            /* Sharp mode */

            /* Modulus and leaky integrate the data */
            s->broad_zl = ((s->broad_zl*s->desc->unfiltered_slugi) >> 15)
                        + ((abs((int) amp[i])*s->desc->unfiltered_slugp) >> 15);
     
            /* Mow the grass to weed out the noise! */
            mown_bandpass = s->broad_zl & s->desc->unfiltered_threshold;
    
            /* Persistence checking and notch insertion logic */
            if (!s->tone_present)
            {
                if (mown_notch[0] < mown_bandpass)
                {
                    /* Tone is detected this sample */
                    if (s->tone_persistence_timeout <= 0)
                    {
                        s->tone_present = TRUE;
                        s->notch_enabled = s->desc->notch_allowed;
                        s->tone_persistence_timeout = s->desc->tone_off_check_time;
                        s->notch_insertion_timeout = s->desc->notch_lag_time;
                        if (s->sig_update)
                            s->sig_update(s->user_data, SIG_TONE_1_CHANGE | SIG_TONE_1_PRESENT, 0, s->signaling_state_duration);
                        /*endif*/
                        s->signaling_state_duration = 0;
                    }
                    else
                    {
                        s->tone_persistence_timeout--;
                        if (s->notch_insertion_timeout > 0)
                            s->notch_insertion_timeout--;
                        else
                            s->notch_enabled = FALSE;
                        /*endif*/
                    }
                    /*endif*/
                }
                else
                {
                    s->tone_persistence_timeout = s->desc->tone_on_check_time;
                    if (s->notch_insertion_timeout > 0)
                        s->notch_insertion_timeout--;
                    else
                        s->notch_enabled = FALSE;
                    /*endif*/
                }
                /*endif*/
            }
            else
            {
                if (mown_notch[0] > mown_bandpass)
                {
                    /* Tone is not detected this sample */
                    if (s->tone_persistence_timeout <= 0)
                    {
                        s->tone_present = FALSE;
                        s->tone_persistence_timeout = s->desc->tone_on_check_time;
                        if (s->sig_update)
                            s->sig_update(s->user_data, SIG_TONE_1_CHANGE, 0, s->signaling_state_duration);
                        /*endif*/
                        s->signaling_state_duration = 0;
                    }
                    else
                    {
                        s->tone_persistence_timeout--;
                    }
                    /*endif*/
                }
                else
                {
                    s->tone_persistence_timeout = s->desc->tone_off_check_time;
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/

        if ((s->current_rx_tone & SIG_TONE_RX_PASSTHROUGH))
        {
            if ((s->current_rx_tone & SIG_TONE_RX_FILTER_TONE)  ||  s->notch_enabled)
                amp[i] = (int16_t) notched_signal;
            /*endif*/
        }
        else
        {
            amp[i] = 0;
        }
        /*endif*/
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
    if (sig_update == NULL  ||  tone_type < 1  ||  tone_type > 3)
        return NULL;
    /*endif*/

    if (s == NULL)
    {
        if ((s = (sig_tone_rx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->sig_update = sig_update;
    s->user_data = user_data;

    s->desc = &sig_tones[tone_type - 1];

    s->flat_mode_timeout = 0;
    s->notch_insertion_timeout = 0;
    s->tone_persistence_timeout = 0;
    s->signaling_state_duration = 0;
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
