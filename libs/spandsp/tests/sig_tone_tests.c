/*
 * SpanDSP - a series of DSP components for telephony
 *
 * sig_tone_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: sig_tone_tests.c,v 1.27 2009/09/23 16:02:59 steveu Exp $
 */

/*! \file */

/*! \page sig_tone_tests_page The signaling tone processor tests
\section sig_tone_tests_sec_1 What does it do?
???.

\section sig_tone_tests_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#define OUT_FILE_NAME               "sig_tone.wav"

#define SAMPLES_PER_CHUNK           160

static int sampleno = 0;
static int tone_1_present = 0;
static int tone_2_present = 0;
static int tx_section = 0;
static int dial_pulses = 0;

static void tx_handler(void *user_data, int what, int level, int duration)
{
    sig_tone_tx_state_t *s;
    
    s = (sig_tone_tx_state_t *) user_data;
    //printf("What - %d, duration - %d\n", what, duration);
    if ((what & SIG_TONE_TX_UPDATE_REQUEST))
    {
        printf("Tx: update request\n");
        /* The sig tone transmit side wants to know what to do next */
        switch (tx_section)
        {
        case 0:
            printf("33ms break - %d samples\n", ms_to_samples(33));
            tx_section++;
            sig_tone_tx_set_mode(s, SIG_TONE_1_PRESENT, ms_to_samples(33));
            break;
        case 1:
            printf("67ms make - %d samples\n", ms_to_samples(67));
            if (++dial_pulses == 9)
                tx_section++;
            else
                tx_section--;
            sig_tone_tx_set_mode(s, 0, ms_to_samples(67));
            break;
        case 2:
            tx_section++;
            sig_tone_tx_set_mode(s, SIG_TONE_1_PRESENT, ms_to_samples(600));
            break;
        case 3:
            sig_tone_tx_set_mode(s, SIG_TONE_1_PRESENT | SIG_TONE_TX_PASSTHROUGH, 0);
            break;
        }
        /*endswitch*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void rx_handler(void *user_data, int what, int level, int duration)
{
    float ms;

    ms = 1000.0f*(float) duration/(float) SAMPLE_RATE;
    printf("What - %d, duration - %d\n", what, duration);
    if ((what & SIG_TONE_1_CHANGE))
    {
        tone_1_present = what & SIG_TONE_1_PRESENT;
        printf("Rx: tone 1 is %s after %d samples (%fms)\n", (tone_1_present)  ?  "on"  : "off", duration, ms);
    }
    /*endif*/
    if ((what & SIG_TONE_2_CHANGE))
    {
        tone_2_present = what & SIG_TONE_2_PRESENT;
        printf("Rx: tone 2 is %s after %d samples (%fms)\n", (tone_2_present)  ?  "on"  : "off", duration, ms);
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void map_frequency_response(sig_tone_rx_state_t *s)
{
    int16_t buf[8192];
    int i;
    int len;
    double sumin;
    double sumout;
    swept_tone_state_t *swept;
    
    /* Things like noise don't highlight the frequency response of the high Q notch
       very well. We use a slowly swept frequency to check it. */
    swept = swept_tone_init(NULL, 200.0f, 3900.0f, -10.0f, 120*SAMPLE_RATE, 0);
    for (;;)
    {
        if ((len = swept_tone(swept, buf, SAMPLES_PER_CHUNK)) <= 0)
            break;
        sumin = 0.0;
        for (i = 0;  i < len;  i++)
            sumin += (double) buf[i]*(double) buf[i];
        sig_tone_rx(s, buf, len);
        sumout = 0.0;
        for (i = 0;  i < len;  i++)
            sumout += (double) buf[i]*(double) buf[i];
        /*endfor*/
        printf("%7.1f %f\n", swept_tone_current_frequency(swept), 10.0*log10(sumout/sumin));
    }
    /*endfor*/
    swept_tone_free(swept);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    SNDFILE *outhandle;
    int outframes;
    int i;
    int type;
    int rx_samples;
    int tx_samples;
    sig_tone_tx_state_t tx_state;
    sig_tone_rx_state_t rx_state;
    awgn_state_t noise_source;
    codec_munge_state_t *munge;

    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 2)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    /*endif*/

    awgn_init_dbm0(&noise_source, 1234567, -20.0f);

    for (type = 1;  type <= 3;  type++)
    {
        sampleno = 0;
        tone_1_present = 0;
        tone_2_present = 0;
        tx_section = 0;
        munge = NULL;
        switch (type)
        {
        case 1:
            printf("2280Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ALAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2280HZ, tx_handler, &tx_state);
            sig_tone_rx_init(&rx_state, SIG_TONE_2280HZ, rx_handler, &rx_state);
            rx_state.current_rx_tone |= SIG_TONE_RX_PASSTHROUGH;
            break;
        case 2:
            printf("2600Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ULAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2600HZ, tx_handler, &tx_state);
            sig_tone_rx_init(&rx_state, SIG_TONE_2600HZ, rx_handler, &rx_state);
            rx_state.current_rx_tone |= SIG_TONE_RX_PASSTHROUGH;
            break;
        case 3:
            printf("2400Hz/2600Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ULAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2400HZ_2600HZ, tx_handler, &tx_state);
            sig_tone_rx_init(&rx_state, SIG_TONE_2400HZ_2600HZ, rx_handler, &rx_state);
            rx_state.current_rx_tone |= SIG_TONE_RX_PASSTHROUGH;
            break;
        }
        /*endswitch*/
        /* Set to the default of hook condition */
        sig_tone_rx_set_mode(&rx_state, SIG_TONE_RX_PASSTHROUGH | SIG_TONE_RX_FILTER_TONE, 0);
        sig_tone_tx_set_mode(&tx_state, SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT | SIG_TONE_TX_PASSTHROUGH, 0);

        map_frequency_response(&rx_state);

        sig_tone_rx_set_mode(&rx_state, SIG_TONE_RX_PASSTHROUGH, 0);
        for (sampleno = 0;  sampleno < 30000;  sampleno += SAMPLES_PER_CHUNK)
        {
            if (sampleno == 8000)
            {
                /* 100ms seize */
                printf("100ms seize - %d samples\n", ms_to_samples(100));
                dial_pulses = 0;
                sig_tone_tx_set_mode(&tx_state, 0, ms_to_samples(100));
            }
            for (i = 0;  i < SAMPLES_PER_CHUNK;  i++)
                amp[i] = awgn(&noise_source);
            /*endfor*/
            tx_samples = sig_tone_tx(&tx_state, amp, SAMPLES_PER_CHUNK);
            for (i = 0;  i < tx_samples;  i++)
                out_amp[2*i] = amp[i];
            /*endfor*/
            codec_munge(munge, amp, tx_samples);
            rx_samples = sig_tone_rx(&rx_state, amp, tx_samples);
            for (i = 0;  i < rx_samples;  i++)
                out_amp[2*i + 1] = amp[i];
            /*endfor*/
            outframes = sf_writef_short(outhandle, out_amp, rx_samples);
            if (outframes != rx_samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endfor*/
    if (sf_close(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    /*endif*/
    
    printf("Tests completed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
