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
#include <unistd.h>
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

typedef struct
{
    double freq;
    double min_level;
    double max_level;
} template_t;

static int number_of_tones = 1;

static int sampleno = 0;
static int tone_1_present = 0;
static int tone_2_present = 0;
static int tx_section = 0;
static int dial_pulses = 0;

static int rx_handler_callbacks = 0;
static int tx_handler_callbacks = 0;

static int use_gui = FALSE;

static void plot_frequency_response(void)
{
    FILE *gnucmd;
    
    if ((gnucmd = popen("gnuplot", "w")) == NULL)
    {
        exit(2);
    }
    
    fprintf(gnucmd, "set autoscale\n");
    fprintf(gnucmd, "unset log\n");
    fprintf(gnucmd, "unset label\n");
    fprintf(gnucmd, "set xtic auto\n");
    fprintf(gnucmd, "set ytic auto\n");
    fprintf(gnucmd, "set title 'Notch filter frequency response'\n");
    fprintf(gnucmd, "set xlabel 'Frequency (Hz)'\n");
    fprintf(gnucmd, "set ylabel 'Gain (dB)'\n");
    fprintf(gnucmd, "plot 'sig_tone_notch' using 1:3 title 'min' with lines,"
                    "'sig_tone_notch' using 1:6 title 'actual' with lines,"
                    "'sig_tone_notch' using 1:9 title 'max' with lines\n");
    fflush(gnucmd);
    getchar();
    if (pclose(gnucmd) == -1)
    {
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void tx_handler(void *user_data, int what, int level, int duration)
{
    sig_tone_tx_state_t *s;
    int tone;
    int time;
    static const int pattern_1_tone[][2] =
    {
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {600, SIG_TONE_1_PRESENT},
        {0, 0}
    };
    static const int pattern_2_tones[][2] =
    {
#if 0
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
        {33, SIG_TONE_1_PRESENT},
        {67, 0},
#endif
        {100, SIG_TONE_1_PRESENT},
        {100, SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT},
        {100, SIG_TONE_2_PRESENT},
#if 0
        {100, 0},
        {100, SIG_TONE_2_PRESENT},
        {100, SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT},
        {100, SIG_TONE_1_PRESENT},
#endif
        {0, 0}
    };
    
    s = (sig_tone_tx_state_t *) user_data;
    tx_handler_callbacks++;
    //printf("What - %d, duration - %d\n", what, duration);
    if ((what & SIG_TONE_TX_UPDATE_REQUEST))
    {
        /* The sig tone transmit side wants to know what to do next */
        printf("Tx: update request\n");

        if (number_of_tones == 1)
        {
            time = pattern_1_tone[tx_section][0];
            tone = pattern_1_tone[tx_section][1];
        }
        else
        {
            time = pattern_2_tones[tx_section][0];
            tone = pattern_2_tones[tx_section][1];
        }
        if (time)
        {
            printf("Tx: [%04x] %s %s for %d samples (%dms)\n",
                   tone,
                   (tone & SIG_TONE_1_PRESENT)  ?  "on "  :  "off",
                   (tone & SIG_TONE_2_PRESENT)  ?  "on "  :  "off",
                   ms_to_samples(time),
                   time);
            sig_tone_tx_set_mode(s, tone, ms_to_samples(time));
            tx_section++;
        }
        else
        {
            printf("End of sequence\n");
        }
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void rx_handler(void *user_data, int what, int level, int duration)
{
    float ms;
    int x;

    rx_handler_callbacks++;
    ms = 1000.0f*(float) duration/(float) SAMPLE_RATE;
    printf("Rx: [%04x]", what);
    x = what & SIG_TONE_1_PRESENT;
    if ((what & SIG_TONE_1_CHANGE))
    {
        printf(" %s", (x)  ?  "on "  : "off");
        if (x == tone_1_present)
            exit(2);
        tone_1_present = x;
    }
    else
    {
        printf(" ---");
        if (x != tone_1_present)
            exit(2);
    }
    /*endif*/
    x = what & SIG_TONE_2_PRESENT;
    if ((what & SIG_TONE_2_CHANGE))
    {
        printf(" %s", (x)  ?  "on "  : "off");
        if (x == tone_2_present)
            exit(2);
        tone_2_present = x;
    }
    else
    {
        if (x != tone_2_present)
            exit(2);
        printf(" ---");
    }
    /*endif*/
    printf(" after %d samples (%.3fms)\n", duration, ms);
}
/*- End of function --------------------------------------------------------*/

static void map_frequency_response(sig_tone_rx_state_t *s, template_t template[])
{
    int16_t buf[SAMPLES_PER_CHUNK];
    int i;
    int len;
    double sumin;
    double sumout;
    swept_tone_state_t *swept;
    double freq;
    double gain;
    int template_entry;
    FILE *file;
    
    /* Things like noise don't highlight the frequency response of the high Q notch
       very well. We use a slowly swept frequency to check it. */
    printf("Frequency response test\n");
    sig_tone_rx_set_mode(s, SIG_TONE_RX_PASSTHROUGH | SIG_TONE_RX_FILTER_TONE, 0);
    swept = swept_tone_init(NULL, 200.0f, 3900.0f, -10.0f, 120*SAMPLE_RATE, 0);
    template_entry = 0;
    file = fopen("sig_tone_notch", "wb");
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
        gain = (sumin != 0.0)  ?  10.0*log10(sumout/sumin + 1.0e-10)  :  0.0;
        printf("%7.1f Hz %.3f dBm0 < %.3f dBm0 < %.3f dBm0\n",
               freq,
               template[template_entry].min_level,
               gain,
               template[template_entry].max_level);
        if (file)
        {
            fprintf(file,
                    "%7.1f Hz %.3f dBm0 < %.3f dBm0 < %.3f dBm0\n",
                    freq,
                    template[template_entry].min_level,
                    gain,
                    template[template_entry].max_level);
        }
        /*endif*/
        if (gain < template[template_entry].min_level  ||  gain > template[template_entry].max_level)
        {
            printf("Expected: %.3f dBm0 to  %.3f dBm0\n",
                   template[template_entry].min_level,
                   template[template_entry].max_level);
            printf("    Failed\n");
            exit(2);
        }
        /*endif*/
        if (freq > template[template_entry].freq)
            template_entry++;
    }
    /*endfor*/
    swept_tone_free(swept);
    if (file)
    {
        fclose(file);
        if (use_gui)
            plot_frequency_response();
        /*endif*/
    }
    /*endif*/
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
        if (sf_close_telephony(inhandle))
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

static void level_and_ratio_tests(sig_tone_rx_state_t *s, double pitch[2])
{
    awgn_state_t noise_source;
    int32_t phase_rate[2];
    uint32_t phase[2];
    int16_t gain;
    int16_t amp[SAMPLE_RATE];
    int i;
    int j;
    int k;
    int l;
    float noise_level;
    float tone_level;
    power_meter_t noise_meter;
    power_meter_t tone_meter;
    int16_t noise;
    int16_t tone;

    printf("Acceptable level and ratio test - %.2f Hz + %.2f Hz\n", pitch[0], pitch[1]);
    for (l = 0;  l < 2;  l++)
    {
        phase[l] = 0;
        phase_rate[l] = (pitch[l] != 0.0)  ?  dds_phase_rate(pitch[l])  :  0;
    }
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
                tone = dds_mod(&phase[0], phase_rate[0], gain, 0);
                if (phase_rate[1])
                    tone += dds_mod(&phase[1], phase_rate[1], gain, 0);
                power_meter_update(&noise_meter, noise);
                power_meter_update(&tone_meter, tone);
                amp[i] = noise + tone;
            }
            /*endfor*/
            sig_tone_rx(s, amp, SAMPLES_PER_CHUNK);
            if (rx_handler_callbacks)
            {
                printf("Hit at   tone = %.2fdBm0, noise = %.2fdBm0\n", tone_level, noise_level);
                printf("Measured tone = %.2fdBm0, noise = %.2fdBm0\n", power_meter_current_dbm0(&tone_meter), power_meter_current_dbm0(&noise_meter));
                if (rx_handler_callbacks != 1)
                    printf("Callbacks = %d\n", rx_handler_callbacks);
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
    tx_section = 0;
    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 2)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    /*endif*/

    awgn_init_dbm0(&noise_source, 1234567, -20.0f);
    sig_tone_tx_set_mode(tx_state, SIG_TONE_1_PRESENT | SIG_TONE_2_PRESENT | SIG_TONE_TX_PASSTHROUGH, 0);
    sig_tone_rx_set_mode(rx_state, SIG_TONE_RX_PASSTHROUGH, 0);
    for (sampleno = 0;  sampleno < 4000;  sampleno += SAMPLES_PER_CHUNK)
    {
        if (sampleno == 800)
        {
            /* 100ms seize */
            printf("Tx: [0000] off off for %d samples (%dms)\n", ms_to_samples(100), 100);
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
    if (sf_close_telephony(outhandle))
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
    double fc[2];
    int i;
    template_t template[10];
    int opt;

    use_gui = FALSE;
    while ((opt = getopt(argc, argv, "g")) != -1)
    {
        switch (opt)
        {
        case 'g':
            use_gui = TRUE;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    for (type = 1;  type <= 3;  type++)
    {
        sampleno = 0;
        tone_1_present = 0;
        tone_2_present = 0;
        munge = NULL;
        for (i = 0;  i < 10;  i++)
        {
            template[i].freq = 0.0;
            template[i].min_level = 0.0;
            template[i].max_level = 0.0;
        }
        fc[0] =
        fc[1] = 0.0;
        switch (type)
        {
        case 1:
            printf("2280Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ALAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2280HZ, tx_handler, &tx_state);
            sig_tone_rx_init(&rx_state, SIG_TONE_2280HZ, rx_handler, &rx_state);
            number_of_tones = 1;
            fc[0] = 2280.0;

            /* From BTNR 181 2.3.3.1 */
            template[0].freq = 1150.0;
            template[0].min_level = -0.2;
            template[0].max_level = 0.0;
            template[1].freq = 1880.0;
            template[1].min_level = -0.5;
            template[1].max_level = 0.0;
            template[2].freq = 2080.0;
            template[2].min_level = -5.0;
            template[2].max_level = 0.0;
            template[3].freq = 2280.0 - 20.0;
            template[3].min_level = -99.0;
            template[3].max_level = 0.0;
            template[4].freq = 2280.0 + 20.0;
            template[4].min_level = -99.0;
            template[4].max_level = -30.0;
            template[5].freq = 2480.0;
            template[5].min_level = -99.0;
            template[5].max_level = 0.0;
            template[6].freq = 2680.0;
            template[6].min_level = -5.0;
            template[6].max_level = 0.0;
            template[7].freq = 4000.0;
            template[7].min_level = -0.5;
            template[7].max_level = 0.0;
            break;
        case 2:
            printf("2600Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ULAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2600HZ, tx_handler, &tx_state);
            sig_tone_rx_init(&rx_state, SIG_TONE_2600HZ, rx_handler, &rx_state);
            number_of_tones = 1;
            fc[0] = 2600.0;

            template[0].freq = 2600.0 - 200.0;
            template[0].min_level = -1.0;
            template[0].max_level = 0.0;
            template[1].freq = 2600.0 - 20.0;
            template[1].min_level = -99.0;
            template[1].max_level = 0.0;
            template[2].freq = 2600.0 + 20.0;
            template[2].min_level = -99.0;
            template[2].max_level = -30.0;
            template[3].freq = 2600.0 + 200.0;
            template[3].min_level = -99.0;
            template[3].max_level = 0.0;
            template[4].freq = 4000.0;
            template[4].min_level = -1.0;
            template[4].max_level = 0.0;
            break;
        case 3:
            printf("2400Hz/2600Hz tests.\n");
            munge = codec_munge_init(MUNGE_CODEC_ULAW, 0);
            sig_tone_tx_init(&tx_state, SIG_TONE_2400HZ_2600HZ, tx_handler, &tx_state);
            sig_tone_rx_init(&rx_state, SIG_TONE_2400HZ_2600HZ, rx_handler, &rx_state);
            number_of_tones = 2;
            fc[0] = 2400.0;
            fc[1] = 2600.0;

            template[0].freq = 2400.0 - 200.0;
            template[0].min_level = -1.0;
            template[0].max_level = 0.0;
            template[1].freq = 2400.0 - 20.0;
            template[1].min_level = -99.0;
            template[1].max_level = 0.0;
            template[2].freq = 2400.0 + 20.0;
            template[2].min_level = -99.0;
            template[2].max_level = -30.0;
            template[3].freq = 2600.0 - 20.0;
            template[3].min_level = -99.0;
            template[3].max_level = 0.0;
            template[4].freq = 2600.0 + 20.0;
            template[4].min_level = -99.0;
            template[4].max_level = -30.0;
            template[5].freq = 2600.0 + 200.0;
            template[5].min_level = -99.0;
            template[5].max_level = 0.0;
            template[6].freq = 4000.0;
            template[6].min_level = -1.0;
            template[6].max_level = 0.0;
            break;
        }
        /*endswitch*/
        map_frequency_response(&rx_state, template);
        speech_immunity_tests(&rx_state);
        level_and_ratio_tests(&rx_state, fc);
        sequence_tests(&tx_state, &rx_state, munge);
    }
    /*endfor*/
    
    printf("Tests completed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
