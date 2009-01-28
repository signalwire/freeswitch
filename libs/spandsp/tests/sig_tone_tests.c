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
 * $Id: sig_tone_tests.c,v 1.24 2008/11/30 10:17:31 steveu Exp $
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
#include <audiofile.h>

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
static int ping = 0;

void map_frequency_response(sig_tone_rx_state_t *s);

static int tx_handler(void *user_data, int what)
{
    //printf("What - %d\n", what);
    if ((what & SIG_TONE_UPDATE_REQUEST))
    {
        printf("Tx: update request\n");
        /* The signaling processor wants to know what to do next */
        if (sampleno < ms_to_samples(100))
        {
            /* 100ms off-hook */
            printf("100ms off-hook - %d samples\n", 800 - sampleno);
            return 0x02 | ((ms_to_samples(100) - sampleno) << 16);
        }
        else if (sampleno < ms_to_samples(600))
        {
            /* 500ms idle */
            printf("500ms idle - %d samples\n", ms_to_samples(600) - sampleno);
            return 0x02 | SIG_TONE_1_PRESENT | ((ms_to_samples(600) - sampleno) << 16);
        }
        else if (sampleno < ms_to_samples(700))
        {
            /* 100ms seize */
            printf("100ms seize - %d samples\n", ms_to_samples(700) - sampleno);
            return 0x02 | ((ms_to_samples(700) - sampleno) << 16);
        }
        else if (sampleno < ms_to_samples(1700))
        {
            if (ping)
            {
                printf("33ms break - %d samples\n", ms_to_samples(33));
                ping = !ping;
                return 0x02 | (ms_to_samples(33) << 16);
            }
            else
            {
                printf("67ms make - %d samples\n", ms_to_samples(67));
                ping = !ping;
                return 0x02 | SIG_TONE_1_PRESENT | (ms_to_samples(67) << 16);
            }
            /*endif*/
        }
        else
        {
            return 0x02 | SIG_TONE_1_PRESENT | ((ms_to_samples(700) - sampleno) << 16) | SIG_TONE_TX_PASSTHROUGH;
        }
        /*endif*/
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_handler(void *user_data, int what)
{
    //printf("What - %d\n", what);
    if ((what & SIG_TONE_1_CHANGE))
    {
        tone_1_present = what & SIG_TONE_1_PRESENT;
        printf("Rx: tone 1 is %s after %d samples\n", (tone_1_present)  ?  "on"  : "off", (what >> 16) & 0xFFFF);
    }
    /*endif*/
    if ((what & SIG_TONE_2_CHANGE))
    {
        tone_2_present = what & SIG_TONE_2_PRESENT;
        printf("Rx: tone 2 is %s after %d samples\n", (tone_2_present)  ?  "on"  : "off", (what >> 16) & 0xFFFF);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

void map_frequency_response(sig_tone_rx_state_t *s)
{
    int16_t buf[8192];
    awgn_state_t noise_source;
    int i;
    int f;
    uint32_t phase_acc;
    int32_t phase_rate;
    int32_t scaling;
    double sum;
    
    /* Things like noise don't highlight the frequency response of the high Q notch
       very well. We use a slowly swept frequency to check it. */
    awgn_init_dbm0(&noise_source, 1234567, -10.0f);
    for (f = 1;  f < 4000;  f++)
    {
        phase_rate = dds_phase_rate(f);
        scaling = dds_scaling_dbm0(-10);
        phase_acc = 0;
        for (i = 0;  i < 8192;  i++)
            buf[i] = dds_mod(&phase_acc, phase_rate, scaling, 0);
        /*endfor*/
        sig_tone_rx(s, buf, 8192);
        sum = 0.0;
        for (i = 1000;  i < 8192;  i++)
            sum += (double) buf[i]*(double) buf[i];
        /*endfor*/
        sum = sqrt(sum);
        printf("%7d %f\n", f, sum);
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    AFfilehandle outhandle;
    int outframes;
    int i;
    int type;
    int rx_samples;
    int tx_samples;
    sig_tone_tx_state_t tx_state;
    sig_tone_rx_state_t rx_state;
    awgn_state_t noise_source;
    codec_munge_state_t *munge;

    if ((outhandle = afOpenFile_telephony_write(OUT_FILE_NAME, 2)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    /*endif*/

    awgn_init_dbm0(&noise_source, 1234567, -10.0f);

    for (type = 1;  type <= 3;  type++)
    {
        sampleno = 0;
        tone_1_present = 0;
        tone_2_present = 0;
        ping = 0;
        munge = NULL;
        switch (type)
        {
        case 1:
            printf("2280Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ALAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2280HZ, tx_handler, NULL);
            sig_tone_rx_init(&rx_state, SIG_TONE_2280HZ, rx_handler, NULL);
            rx_state.current_rx_tone |= SIG_TONE_RX_PASSTHROUGH;
            break;
        case 2:
            printf("26000Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ULAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2600HZ, tx_handler, NULL);
            sig_tone_rx_init(&rx_state, SIG_TONE_2600HZ, rx_handler, NULL);
            rx_state.current_rx_tone |= SIG_TONE_RX_PASSTHROUGH;
            break;
        case 3:
            printf("2400Hz/26000Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ULAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2400HZ_2600HZ, tx_handler, NULL);
            sig_tone_rx_init(&rx_state, SIG_TONE_2400HZ_2600HZ, rx_handler, NULL);
            rx_state.current_rx_tone |= SIG_TONE_RX_PASSTHROUGH;
            break;
        }
        /*endswitch*/
    
        //map_frequency_response(&rx_state);

        for (sampleno = 0;  sampleno < 20000;  sampleno += SAMPLES_PER_CHUNK)
        {
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
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      out_amp,
                                      rx_samples);
            if (outframes != rx_samples)
            {
                fprintf(stderr, "    Error writing wave file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endfor*/
    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    /*endif*/
    
    printf("Tests completed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
