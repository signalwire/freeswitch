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
 * $Id: sig_tone_tests.c,v 1.32 2010/03/11 14:22:30 steveu Exp $
 */

/*! \file */

/*! \page sig_tone_tests_page The 2280/2400/2600Hz signalling tone Rx/Tx tests
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

#define MITEL_DIR                   "../test-data/mitel/"
#define BELLCORE_DIR                "../test-data/bellcore/"

const char *bellcore_files[] =
{
    MITEL_DIR    "mitel-cm7291-talkoff.wav",
    BELLCORE_DIR "tr-tsy-00763-1.wav",
    BELLCORE_DIR "tr-tsy-00763-2.wav",
    BELLCORE_DIR "tr-tsy-00763-3.wav",
    BELLCORE_DIR "tr-tsy-00763-4.wav",
    BELLCORE_DIR "tr-tsy-00763-5.wav",
    BELLCORE_DIR "tr-tsy-00763-6.wav",
    ""
};

static int number_of_tones = 1;

static int sampleno = 0;
static int tone_1_present = 0;
static int tone_2_present = 0;
static int tx_section = 0;
static int dial_pulses = 0;

static int rx_handler_callbacks = 0;
static int tx_handler_callbacks = 0;

static void tx_handler(void *user_data, int what, int level, int duration)
{
    sig_tone_tx_state_t *s;
    
    s = (sig_tone_tx_state_t *) user_data;
    tx_handler_callbacks++;
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
            /*endif*/
            sig_tone_tx_set_mode(s, 0, ms_to_samples(67));
            break;
        case 2:
            tx_section++;
            printf("600ms on - %d samples\n", ms_to_samples(600));
            if (number_of_tones == 2)
                sig_tone_tx_set_mode(s, SIG_TONE_2_PRESENT, ms_to_samples(600));
            else
                sig_tone_tx_set_mode(s, SIG_TONE_1_PRESENT, ms_to_samples(600));
            break;
        case 3:
            printf("End of sequence\n");
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

    rx_handler_callbacks++;
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

static void map_frequency_response(sig_tone_rx_state_t *s,
                                   double f1,
                                   double f2,
                                   double f3,
                                   double f4)
{
    int16_t buf[SAMPLES_PER_CHUNK];
    int i;
    int len;
    double sumin;
    double sumout;
    swept_tone_state_t *swept;
    double freq;
    double gain;
    
    /* Things like noise don't highlight the frequency response of the high Q notch
       very well. We use a slowly swept frequency to check it. */
    printf("Frequency response test\n");
    sig_tone_rx_set_mode(s, SIG_TONE_RX_PASSTHROUGH | SIG_TONE_RX_FILTER_TONE, 0);
    swept = swept_tone_init(NULL, 200.0f, 3900.0f, -10.0f, 120*SAMPLE_RATE, 0);
    for (;;)
    {
        if ((len = swept_tone(swept, buf, SAMPLES_PER_CHUNK)) <= 0)
            break;
        /*endif*/
        sumin = 0.0;
        for (i = 0;  i < len;  i++)
            sumin += (double) buf[i]*(double) buf[i];
        /*endfor*/
        sig_tone_rx(s, buf, len);
        sumout = 0.0;
        for (i = 0;  i < len;  i++)
            sumout += (double) buf[i]*(double) buf[i];
        /*endfor*/
        freq = swept_tone_current_frequency(swept);
        if (sumin)
            gain = 10.0*log10(sumout/sumin);
        else
            gain = 0.0;
        printf("%7.1f Hz %f dBm0\n", freq, gain);
        if (gain > 0.0
            ||
            (freq < f1  &&  gain < -1.0)
            ||
            (freq > f2  &&  freq < f3  &&  gain > -30.0)
            ||
            (freq > f4  &&  gain < -1.0))
        {
            printf("    Failed\n");
            exit(2);
        }
        /*endif*/
    }
    /*endfor*/
    swept_tone_free(swept);
    printf("    Passed\n");
}
/*- End of function --------------------------------------------------------*/

static void speech_immunity_tests(sig_tone_rx_state_t *s)
{
    int j;
    int total_hits;
    SNDFILE *inhandle;
    int16_t amp[SAMPLE_RATE];
    int frames;

    printf("Speech immunity test\n");
    total_hits = 0;
    for (j = 0;  bellcore_files[j][0];  j++)
    {
        /* Push some silence through, so we should be in the tone off state */
        vec_zeroi16(amp, SAMPLE_RATE);
        sig_tone_rx(s, amp, SAMPLE_RATE);
        rx_handler_callbacks = 0;
        if ((inhandle = sf_open_telephony_read(bellcore_files[j], 1)) == NULL)
        {
            printf("    Cannot open speech file '%s'\n", bellcore_files[j]);
            exit(2);
        }
        /*endif*/
        while ((frames = sf_readf_short(inhandle, amp, SAMPLE_RATE)))
        {
            sig_tone_rx(s, amp, frames);
        }
        /*endwhile*/
        if (sf_close(inhandle) != 0)
        {
            printf("    Cannot close speech file '%s'\n", bellcore_files[j]);
            exit(2);
        }
        /*endif*/
        printf("    File %d gave %d false hits.\n", j + 1, rx_handler_callbacks);
        total_hits += rx_handler_callbacks;
    }
    /*endfor*/
    printf("    %d hits in total\n", total_hits);
    if (total_hits > 0)
    {
        printf("    Failed\n");
        exit(2);
    }
    /*endif*/
    printf("    Passed\n");
}
/*- End of function --------------------------------------------------------*/

static void level_and_ratio_tests(sig_tone_rx_state_t *s, double pitch)
{
    awgn_state_t noise_source;
    int32_t phase_rate;
    uint32_t phase;
    int16_t gain;
    int16_t amp[SAMPLE_RATE];
    int i;
    int j;
    int k;
    float noise_level;
    float tone_level;
    power_meter_t noise_meter;
    power_meter_t tone_meter;
    int16_t noise;
    int16_t tone;

    printf("Acceptable level and ratio test\n");
    phase = 0;
    phase_rate = dds_phase_rate(pitch);
    for (k = -25;  k > -60;  k--)
    {
        noise_level = k;
        awgn_init_dbm0(&noise_source, 1234567, noise_level);
        tone_level = noise_level;
        /* Push some silence through, so we should be in the tone off state */
        vec_zeroi16(amp, SAMPLE_RATE);
        sig_tone_rx(s, amp, SAMPLE_RATE);
        power_meter_init(&noise_meter, 6);
        power_meter_init(&tone_meter, 6);
        for (j = 0;  j < 20;  j++)
        {
            rx_handler_callbacks = 0;
            gain = dds_scaling_dbm0(tone_level);
            for (i = 0;  i < SAMPLES_PER_CHUNK;  i++)
            {
                noise = awgn(&noise_source);
                tone = dds_mod(&phase, phase_rate, gain, 0);
                power_meter_update(&noise_meter, noise);
                power_meter_update(&tone_meter, tone);
                amp[i] = noise + tone;
            }
            /*endfor*/
            sig_tone_rx(s, amp, SAMPLES_PER_CHUNK);
            if (rx_handler_callbacks)
            {
                printf("Hit at tone = %fdBm0, noise = %fdBm0\n", tone_level, noise_level);
                printf("Noise = %fdBm0, tone = %fdBm0\n", power_meter_current_dbm0(&noise_meter), power_meter_current_dbm0(&tone_meter));
            }
            /*endif*/
            tone_level += 1.0f;
        }
        /*endfor*/
    }
    /*endfor*/
    printf("    Passed\n");
}
/*- End of function --------------------------------------------------------*/

static void sequence_tests(sig_tone_tx_state_t *tx_state, sig_tone_rx_state_t *rx_state, codec_munge_state_t *munge)
{
    int i;
    awgn_state_t noise_source;
    SNDFILE *outhandle;
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int rx_samples;
    int tx_samples;

    printf("Signalling sequence test\n");
    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 2)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    /*endif*/

    awgn_init_dbm0(&noise_source, 1234567, -20.0f);
    for (sampleno = 0;  sampleno < 60000;  sampleno += SAMPLES_PER_CHUNK)
    {
        if (sampleno == 8000)
        {
            /* 100ms seize */
            printf("100ms seize - %d samples\n", ms_to_samples(100));
            dial_pulses = 0;
            sig_tone_tx_set_mode(tx_state, 0, ms_to_samples(100));
        }
        /*endif*/
        for (i = 0;  i < SAMPLES_PER_CHUNK;  i++)
            amp[i] = awgn(&noise_source);
        /*endfor*/
        tx_samples = sig_tone_tx(tx_state, amp, SAMPLES_PER_CHUNK);
        for (i = 0;  i < tx_samples;  i++)
            out_amp[2*i] = amp[i];
        /*endfor*/
        codec_munge(munge, amp, tx_samples);
        rx_samples = sig_tone_rx(rx_state, amp, tx_samples);
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
    if (sf_close(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int type;
    sig_tone_tx_state_t tx_state;
    sig_tone_rx_state_t rx_state;
    codec_munge_state_t *munge;
    double f1;
    double f2;
    double fc;
    double f3;
    double f4;

    for (type = 1;  type <= 3;  type++)
    {
        sampleno = 0;
        tone_1_present = 0;
        tone_2_present = 0;
        tx_section = 0;
        munge = NULL;
        f1 =
        f2 =
        fc =
        f3 =
        f4 = 0.0;
        switch (type)
        {
        case 1:
            printf("2280Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ALAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2280HZ, tx_handler, &tx_state);
            sig_tone_rx_init(&rx_state, SIG_TONE_2280HZ, rx_handler, &rx_state);
            number_of_tones = 1;
            f1 = 2280.0 - 200.0;
            f2 = 2280.0 - 20.0;
            fc = 2280.0;
            f3 = 2280.0 + 20.0;
            f4 = 2280.0 + 200.0;
            break;
        case 2:
            printf("2600Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ULAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2600HZ, tx_handler, &tx_state);
            sig_tone_rx_init(&rx_state, SIG_TONE_2600HZ, rx_handler, &rx_state);
            number_of_tones = 1;
            f1 = 2600.0 - 200.0;
            f2 = 2600.0 - 20.0;
            fc = 2600.0;
            f3 = 2600.0 + 20.0;
            f4 = 2600.0 + 200.0;
            break;
        case 3:
            printf("2400Hz/2600Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ULAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2400HZ_2600HZ, tx_handler, &tx_state);
            sig_tone_rx_init(&rx_state, SIG_TONE_2400HZ_2600HZ, rx_handler, &rx_state);
            number_of_tones = 2;
            f1 = 2400.0 - 200.0;
            f2 = 2400.0 - 20.0;
            fc = 2400.0;
            f3 = 2400.0 + 20.0;
            f4 = 2400.0 + 200.0;
            break;
        }
        /*endswitch*/
        /* Set to the default on hook condition */
        map_frequency_response(&rx_state, f1, f2, f3, f4);
        speech_immunity_tests(&rx_state);
        level_and_ratio_tests(&rx_state, fc);

        sig_tone_tx_set_mode(&tx_state, SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT | SIG_TONE_TX_PASSTHROUGH, 0);
        sig_tone_rx_set_mode(&rx_state, SIG_TONE_RX_PASSTHROUGH, 0);
        sequence_tests(&tx_state, &rx_state, munge);
    }
    /*endfor*/
    
    printf("Tests completed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
