/*
 * SpanDSP - a series of DSP components for telephony
 *
 * modem_connect_tones_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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

/*! \page modem_connect_tones_tests_page Modem connect tones tests
\section modem_connect_tones_rx_tests_page_sec_1 What does it do?
These tests...
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define SAMPLES_PER_CHUNK               160

#define OUTPUT_FILE_NAME                "modem_connect_tones.wav"

#define MITEL_DIR                       "../test-data/mitel/"
#define BELLCORE_DIR                    "../test-data/bellcore/"

#define LEVEL_MAX                       -5
#define LEVEL_MIN                       -48
#define LEVEL_MIN_ACCEPT                -43
#define LEVEL_MIN_REJECT                -44

/* The 1100Hz tone is supposed to be within 38Hz, according to T.30. Allow another 8Hz for FDM, even though
   you rarely see that used today. */
#define CED_FREQ_TOLERANCE              (38 + 8)
#define CED_FREQ_BLACKOUT               (80)
/* The 2100Hz tone is supposed to be within 15Hz, according to T.30. Allow another 8Hz for FDM, even though
   you rarely see that used today. */
#define CNG_FREQ_TOLERANCE              (15 + 8)
#define CNG_FREQ_BLACKOUT               (80)
#define AM_FREQ_TOLERANCE               (1)
/* The 2225Hz tone is supposed to be within 15Hz. Allow another 8Hz for FDM, even though
   you rarely see that used today. */
#define BELL_ANS_FREQ_TOLERANCE         (15 + 8)
#define BELL_ANS_FREQ_BLACKOUT          (80)
/* The 1300Hz tone is supposed to be within 15Hz, according to V.25. Allow another 8Hz for FDM, even though
   you rarely see that used today. */
#define CALLING_TONE_FREQ_TOLERANCE     (15 + 8)
#define CALLING_TONE_FREQ_BLACKOUT      (80)

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

enum
{
    PERFORM_TEST_1A = (1 << 1),
    PERFORM_TEST_1B = (1 << 2),
    PERFORM_TEST_1C = (1 << 3),
    PERFORM_TEST_1D = (1 << 4),
    PERFORM_TEST_1E = (1 << 5),
    PERFORM_TEST_1F = (1 << 6),
    PERFORM_TEST_1G = (1 << 7),
    PERFORM_TEST_2A = (1 << 8),
    PERFORM_TEST_2B = (1 << 9),
    PERFORM_TEST_2C = (1 << 10),
    PERFORM_TEST_2D = (1 << 11),
    PERFORM_TEST_2E = (1 << 12),
    PERFORM_TEST_2F = (1 << 13),
    PERFORM_TEST_2G = (1 << 14),
    PERFORM_TEST_3A = (1 << 15),
    PERFORM_TEST_3B = (1 << 16),
    PERFORM_TEST_3C = (1 << 17),
    PERFORM_TEST_3D = (1 << 18),
    PERFORM_TEST_3E = (1 << 19),
    PERFORM_TEST_3F = (1 << 20),
    PERFORM_TEST_3G = (1 << 21),
    PERFORM_TEST_4 = (1 << 22),
    PERFORM_TEST_5A = (1 << 23),
    PERFORM_TEST_5B = (1 << 24),
    PERFORM_TEST_6A = (1 << 25),
    PERFORM_TEST_6B = (1 << 26),
    PERFORM_TEST_7A = (1 << 27),
    PERFORM_TEST_7B = (1 << 28),
    PERFORM_TEST_8 = (1 << 29)
};

int preamble_count = 0;
int preamble_on_at = -1;
int preamble_off_at = -1;
int hits = 0;
int when = 0;

static int preamble_get_bit(void *user_data)
{
    static int bit_no = 0;
    int bit;

    /* Generate a section of HDLC flag octet preamble. Then generate some random
       bits, which should not look like preamble. */
    if (++preamble_count < 255)
    {
        bit = (bit_no < 2)  ?  0  :  1;
        if (++bit_no >= 8)
            bit_no = 0;
#if 0
        /* Inject some bad bits */
        if (rand()%15 == 0)
            return bit ^ 1;
#endif
    }
    else
    {
        bit = rand() & 1;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void cng_detected(void *user_data, int tone, int level, int delay)
{
    printf("%s (%d) declared at %fs, delay %d (%ddBm0)\n", modem_connect_tone_to_str(tone), tone, (float) when/SAMPLE_RATE, delay, level);
    if (tone == MODEM_CONNECT_TONES_FAX_CNG)
        hits++;
}
/*- End of function --------------------------------------------------------*/

static void preamble_detected(void *user_data, int tone, int level, int delay)
{
    printf("%s (%d) declared at bit %d (%ddBm0)\n", modem_connect_tone_to_str(tone), tone, preamble_count, level);
    if (tone == MODEM_CONNECT_TONES_FAX_PREAMBLE)
        preamble_on_at = preamble_count;
    else
        preamble_off_at = preamble_count;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void ced_detected(void *user_data, int tone, int level, int delay)
{
    printf("%s (%d) declared at %fs, delay %d (%ddBm0)\n", modem_connect_tone_to_str(tone), tone, (float) when/SAMPLE_RATE, delay, level);
    if (tone == MODEM_CONNECT_TONES_FAX_PREAMBLE
        ||
        tone == MODEM_CONNECT_TONES_ANS)
    {
        hits++;
    }
}
/*- End of function --------------------------------------------------------*/

static void ans_pr_detected(void *user_data, int tone, int level, int delay)
{
    printf("%s (%d) declared at %fs, delay %d (%ddBm0)\n", modem_connect_tone_to_str(tone), tone, (float) when/SAMPLE_RATE, delay, level);
    if (tone == MODEM_CONNECT_TONES_ANS_PR)
        hits++;
}
/*- End of function --------------------------------------------------------*/

static void bell_ans_detected(void *user_data, int tone, int level, int delay)
{
    printf("%s (%d) declared at %fs, delay %d (%ddBm0)\n", modem_connect_tone_to_str(tone), tone, (float) when/SAMPLE_RATE, delay, level);
    if (tone == MODEM_CONNECT_TONES_BELL_ANS)
        hits++;
}
/*- End of function --------------------------------------------------------*/

static void calling_tone_detected(void *user_data, int tone, int level, int delay)
{
    printf("%s (%d) declared at %fs, delay %d (%ddBm0)\n", modem_connect_tone_to_str(tone), tone, (float) when/SAMPLE_RATE, delay, level);
    if (tone == MODEM_CONNECT_TONES_CALLING_TONE)
        hits++;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int i;
    int j;
    int pitch;
    int depth;
    int level;
    int interval;
    int cycle;
    int16_t amp[8000];
    modem_connect_tones_rx_state_t cng_rx;
    modem_connect_tones_rx_state_t ced_rx;
    modem_connect_tones_rx_state_t ans_pr_rx;
    modem_connect_tones_tx_state_t modem_tone_tx;
    modem_connect_tones_rx_state_t calling_tone_rx;
    modem_connect_tones_rx_state_t bell_ans_rx;
    awgn_state_t chan_noise_source;
    SNDFILE *inhandle;
    SNDFILE *outhandle;
    int outframes;
    int frames;
    int samples;
    int hit;
    int level2;
    int max_level2;
    int tone_type;
    int test_list;
    int opt;
    bool false_hit;
    bool false_miss;
    char *decode_test_file;
    fsk_tx_state_t preamble_tx;

    test_list = 0;
    decode_test_file = NULL;
    while ((opt = getopt(argc, argv, "d:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            decode_test_file = optarg;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    argc -= optind;
    argv += optind;
    for (i = 0;  i < argc;  i++)
    {
        if (strcasecmp(argv[i], "1a") == 0)
            test_list |= PERFORM_TEST_1A;
        else if (strcasecmp(argv[i], "1b") == 0)
            test_list |= PERFORM_TEST_1B;
        else if (strcasecmp(argv[i], "1c") == 0)
            test_list |= PERFORM_TEST_1C;
        else if (strcasecmp(argv[i], "1d") == 0)
            test_list |= PERFORM_TEST_1D;
        else if (strcasecmp(argv[i], "1e") == 0)
            test_list |= PERFORM_TEST_1E;
        else if (strcasecmp(argv[i], "1f") == 0)
            test_list |= PERFORM_TEST_1F;
        else if (strcasecmp(argv[i], "1g") == 0)
            test_list |= PERFORM_TEST_1G;
        else if (strcasecmp(argv[i], "2a") == 0)
            test_list |= PERFORM_TEST_2A;
        else if (strcasecmp(argv[i], "2b") == 0)
            test_list |= PERFORM_TEST_2B;
        else if (strcasecmp(argv[i], "2c") == 0)
            test_list |= PERFORM_TEST_2C;
        else if (strcasecmp(argv[i], "2d") == 0)
            test_list |= PERFORM_TEST_2D;
        else if (strcasecmp(argv[i], "2e") == 0)
            test_list |= PERFORM_TEST_2E;
        else if (strcasecmp(argv[i], "2f") == 0)
            test_list |= PERFORM_TEST_2F;
        else if (strcasecmp(argv[i], "2g") == 0)
            test_list |= PERFORM_TEST_2G;
        else if (strcasecmp(argv[i], "3a") == 0)
            test_list |= PERFORM_TEST_3A;
        else if (strcasecmp(argv[i], "3b") == 0)
            test_list |= PERFORM_TEST_3B;
        else if (strcasecmp(argv[i], "3c") == 0)
            test_list |= PERFORM_TEST_3C;
        else if (strcasecmp(argv[i], "3d") == 0)
            test_list |= PERFORM_TEST_3D;
        else if (strcasecmp(argv[i], "3e") == 0)
            test_list |= PERFORM_TEST_3E;
        else if (strcasecmp(argv[i], "3f") == 0)
            test_list |= PERFORM_TEST_3F;
        else if (strcasecmp(argv[i], "3g") == 0)
            test_list |= PERFORM_TEST_3G;
        else if (strcasecmp(argv[i], "4") == 0)
            test_list |= PERFORM_TEST_4;
        else if (strcasecmp(argv[i], "5a") == 0)
            test_list |= PERFORM_TEST_5A;
        else if (strcasecmp(argv[i], "5b") == 0)
            test_list |= PERFORM_TEST_5B;
        else if (strcasecmp(argv[i], "6a") == 0)
            test_list |= PERFORM_TEST_6A;
        else if (strcasecmp(argv[i], "6b") == 0)
            test_list |= PERFORM_TEST_6B;
        else if (strcasecmp(argv[i], "7a") == 0)
            test_list |= PERFORM_TEST_7A;
        else if (strcasecmp(argv[i], "7b") == 0)
            test_list |= PERFORM_TEST_7B;
        else if (strcasecmp(argv[i], "8") == 0)
            test_list |= PERFORM_TEST_8;
        else
        {
            fprintf(stderr, "Unknown test '%s' specified\n", argv[i]);
            exit(2);
        }
    }
    if (decode_test_file == NULL  &&  test_list == 0)
        test_list = 0xFFFFFFFF;

    if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    if ((test_list & PERFORM_TEST_1A))
    {
        printf("Test 1a: CNG generation to a file\n");
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_FAX_CNG);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = sf_writef_short(outhandle, amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_1B))
    {
        printf("Test 1b: CED/ANS generation to a file\n");
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_FAX_CED);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = sf_writef_short(outhandle, amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_1C))
    {
        printf("Test 1c: ANSam (Modulated ANS) generation to a file\n");
        /* Some with modulation */
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_ANSAM);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = sf_writef_short(outhandle, amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_1D))
    {
        printf("Test 1d: ANS/ (EC-disable) generation to a file\n");
        /* Some without modulation, but with phase reversals */
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_ANS_PR);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = sf_writef_short(outhandle, amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_1E))
    {
        printf("Test 1e: ANSam/ (Modulated EC-disable) generation to a file\n");
        /* Some with modulation and phase reversals */
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_ANSAM_PR);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = sf_writef_short(outhandle, amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_1F))
    {
        printf("Test 1f: Bell answer tone generation to a file\n");
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_BELL_ANS);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = sf_writef_short(outhandle, amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_1G))
    {
        printf("Test 1g: Calling tone generation to a file\n");
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_CALLING_TONE);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = sf_writef_short(outhandle, amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

    if (sf_close_telephony(outhandle))
    {
        printf("    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_2A))
    {
        printf("Test 2a: CNG detection with frequency\n");
        tone_type = MODEM_CONNECT_TONES_FAX_CNG;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 1100 - 500;  pitch <= 1100 + 500;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
            modem_connect_tones_rx_init(&cng_rx, tone_type, NULL, NULL);
            level2 = 0;
            max_level2 = 0;
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                {
                    amp[j] += awgn(&chan_noise_source);
                    level2 += ((abs(amp[j]) - level2) >> 5);
                    if (level2 > max_level2)
                        max_level2 = level2;
                }
                /*endfor*/
                modem_connect_tones_rx(&cng_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&cng_rx);
            if (pitch < (1100 - CED_FREQ_BLACKOUT)  ||  pitch > (1100 + CED_FREQ_BLACKOUT))
            {
                if (hit != MODEM_CONNECT_TONES_NONE)
                    false_hit = true;
                /*endif*/
            }
            else if (pitch > (1100 - CED_FREQ_TOLERANCE)  &&  pitch < (1100 + CED_FREQ_TOLERANCE))
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, cng_rx.channel_level, cng_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_2B))
    {
        printf("Test 2b: CED/ANS detection with frequency\n");
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2100 - 500;  pitch < 2100 + 500;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_ANS);
            /* Fudge things for the test */
            modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
            modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED_OR_PREAMBLE, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ced_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ced_rx);
            if (pitch < (2100 - CNG_FREQ_BLACKOUT)  ||  pitch > (2100 + CNG_FREQ_BLACKOUT))
            {
                if (hit != MODEM_CONNECT_TONES_NONE)
                    false_hit = true;
                /*endif*/
            }
            else if (pitch > (2100 - CNG_FREQ_TOLERANCE)  &&  pitch < (2100 + CNG_FREQ_TOLERANCE))
            {
                if (hit != MODEM_CONNECT_TONES_FAX_CED)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, ced_rx.channel_level, ced_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_2C))
    {
        printf("Test 2c: ANSam detection with frequency\n");
        tone_type = MODEM_CONNECT_TONES_ANSAM;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2100 - 100;  pitch <= 2100 + 100;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
            modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ans_pr_rx);
            if (pitch < (2100 - CNG_FREQ_BLACKOUT)  ||  pitch > (2100 + CNG_FREQ_BLACKOUT))
            {
                if (hit != MODEM_CONNECT_TONES_NONE)
                    false_hit = true;
                /*endif*/
            }
            else if (pitch > (2100 - CNG_FREQ_TOLERANCE)  &&  pitch < (2100 + CNG_FREQ_TOLERANCE))
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_2D))
    {
        printf("Test 2d: ANS/ (EC-disable) detection with frequency\n");
        tone_type = MODEM_CONNECT_TONES_ANS_PR;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2100 - 100;  pitch <= 2100 + 100;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
            modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ans_pr_rx);
            if (pitch < (2100 - CNG_FREQ_BLACKOUT)  ||  pitch > (2100 + CNG_FREQ_BLACKOUT))
            {
                if (hit != MODEM_CONNECT_TONES_NONE)
                    false_hit = true;
                /*endif*/
            }
            else if (pitch > (2100 - CNG_FREQ_TOLERANCE)  &&  pitch < (2100 + CNG_FREQ_TOLERANCE))
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_2E))
    {
        printf("Test 2e: ANSam/ (Modulated EC-disable) detection with frequency\n");
        tone_type = MODEM_CONNECT_TONES_ANSAM_PR;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2100 - 100;  pitch <= 2100 + 100;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
            modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ans_pr_rx);
            if (pitch < (2100 - CNG_FREQ_BLACKOUT)  ||  pitch > (2100 + CNG_FREQ_BLACKOUT))
            {
                if (hit != MODEM_CONNECT_TONES_NONE)
                    false_hit = true;
                /*endif*/
            }
            else if (pitch > (2100 - CNG_FREQ_TOLERANCE)  &&  pitch < (2100 + CNG_FREQ_TOLERANCE))
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_2F))
    {
        printf("Test 2f: Bell answer tone detection with frequency\n");
        tone_type = MODEM_CONNECT_TONES_BELL_ANS;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2225 - 500;  pitch <= 2225 + 500;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
            modem_connect_tones_rx_init(&bell_ans_rx, tone_type, NULL, NULL);
            level2 = 0;
            max_level2 = 0;
            for (i = 0;  i < 8000;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                {
                    amp[j] += awgn(&chan_noise_source);
                    level2 += ((abs(amp[j]) - level2) >> 5);
                    if (level2 > max_level2)
                        max_level2 = level2;
                }
                /*endfor*/
                modem_connect_tones_rx(&bell_ans_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&bell_ans_rx);
            if (pitch < (2225 - BELL_ANS_FREQ_BLACKOUT)  ||  pitch > (2225 + BELL_ANS_FREQ_BLACKOUT))
            {
                if (hit != MODEM_CONNECT_TONES_NONE)
                    false_hit = true;
                /*endif*/
            }
            else if (pitch > (2225 - BELL_ANS_FREQ_TOLERANCE)  &&  pitch < (2225 + BELL_ANS_FREQ_TOLERANCE))
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, bell_ans_rx.channel_level, bell_ans_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_2G))
    {
        printf("Test 2g: Calling tone detection with frequency\n");
        tone_type = MODEM_CONNECT_TONES_CALLING_TONE;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 1300 - 500;  pitch <= 1300 + 500;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
            modem_connect_tones_rx_init(&calling_tone_rx, tone_type, NULL, NULL);
            level2 = 0;
            max_level2 = 0;
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                {
                    amp[j] += awgn(&chan_noise_source);
                    level2 += ((abs(amp[j]) - level2) >> 5);
                    if (level2 > max_level2)
                        max_level2 = level2;
                }
                /*endfor*/
                modem_connect_tones_rx(&calling_tone_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&calling_tone_rx);
            if (pitch < (1300 - CALLING_TONE_FREQ_BLACKOUT)  ||  pitch > (1300 + CALLING_TONE_FREQ_BLACKOUT))
            {
                if (hit != MODEM_CONNECT_TONES_NONE)
                    false_hit = true;
                /*endif*/
            }
            else if (pitch > (1300 - CALLING_TONE_FREQ_TOLERANCE)  &&  pitch < (1300 + CALLING_TONE_FREQ_TOLERANCE))
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, calling_tone_rx.channel_level, calling_tone_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3A))
    {
        printf("Test 3a: CNG detection with level\n");
        tone_type = MODEM_CONNECT_TONES_FAX_CNG;
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 1100 - CED_FREQ_TOLERANCE;  pitch <= 1100 + CED_FREQ_TOLERANCE;  pitch += 2*CED_FREQ_TOLERANCE)
        {
            for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            {
                /* Use the transmitter to test the receiver */
                modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
                /* Fudge things for the test */
                modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
                modem_tone_tx.level = dds_scaling_dbm0(level);
                modem_connect_tones_rx_init(&cng_rx, tone_type, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&cng_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&cng_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit != MODEM_CONNECT_TONES_NONE)
                        false_hit = true;
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != tone_type)
                        false_miss = true;
                    /*endif*/
                }
                /*endif*/
                if (hit != MODEM_CONNECT_TONES_NONE)
                    printf("Detected at %5dHz %4ddB %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, level, cng_rx.channel_level, cng_rx.notch_level, modem_connect_tone_to_str(hit), hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3B))
    {
        printf("Test 3b: CED/ANS detection with level\n");
        tone_type = MODEM_CONNECT_TONES_ANS;
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2100 - CNG_FREQ_TOLERANCE;  pitch <= 2100 + CNG_FREQ_TOLERANCE;  pitch += 2*CNG_FREQ_TOLERANCE)
        {
            for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            {
                /* Use the transmitter to test the receiver */
                modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
                /* Fudge things for the test */
                modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
                modem_tone_tx.level = dds_scaling_dbm0(level);
                modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED_OR_PREAMBLE, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&ced_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&ced_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit != MODEM_CONNECT_TONES_NONE)
                        false_hit = true;
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != tone_type)
                        false_miss = true;
                    /*endif*/
                }
                /*endif*/
                if (hit != MODEM_CONNECT_TONES_NONE)
                    printf("Detected at %5dHz %4ddB %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, level, ced_rx.channel_level, ced_rx.notch_level, modem_connect_tone_to_str(hit), hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3C))
    {
        printf("Test 3c: ANSam detection with level\n");
        tone_type = MODEM_CONNECT_TONES_ANSAM;
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2100 - CNG_FREQ_TOLERANCE;  pitch <= 2100 + CNG_FREQ_TOLERANCE;  pitch += 2*CNG_FREQ_TOLERANCE)
        {
            //for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            for (level = -26;  level >= -26;  level--)
            {
                /* Use the transmitter to test the receiver */
                modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
                /* Fudge things for the test */
                modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
                modem_tone_tx.level = dds_scaling_dbm0(level);
                modem_tone_tx.mod_level = modem_tone_tx.level*20/100;
                modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&ans_pr_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&ans_pr_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit != MODEM_CONNECT_TONES_NONE)
                        false_hit = true;
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != tone_type)
                        false_miss = true;
                    /*endif*/
                }
                /*endif*/
                if (hit != MODEM_CONNECT_TONES_NONE)
                    printf("Detected at %5dHz %4ddB %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, level, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3D))
    {
        printf("Test 3d: ANS/ (EC-disable) detection with level\n");
        tone_type = MODEM_CONNECT_TONES_ANS_PR;
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2100 - CNG_FREQ_TOLERANCE;  pitch <= 2100 + CNG_FREQ_TOLERANCE;  pitch += 2*CNG_FREQ_TOLERANCE)
        {
            for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            {
                /* Use the transmitter to test the receiver */
                modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
                /* Fudge things for the test */
                modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
                modem_tone_tx.level = dds_scaling_dbm0(level);
                modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&ans_pr_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&ans_pr_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit != MODEM_CONNECT_TONES_NONE)
                        false_hit = true;
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != tone_type)
                        false_miss = true;
                    /*endif*/
                }
                /*endif*/
                if (hit != MODEM_CONNECT_TONES_NONE)
                    printf("Detected at %5dHz %4ddB %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, level, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3E))
    {
        printf("Test 3e: ANSam/ (Modulated EC-disable) detection with level\n");
        tone_type = MODEM_CONNECT_TONES_ANSAM_PR;
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2100 - CNG_FREQ_TOLERANCE;  pitch <= 2100 + CNG_FREQ_TOLERANCE;  pitch += 2*CNG_FREQ_TOLERANCE)
        {
            for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            {
                /* Use the transmitter to test the receiver */
                modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
                /* Fudge things for the test */
                modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
                modem_tone_tx.level = dds_scaling_dbm0(level);
                modem_tone_tx.mod_level = modem_tone_tx.level*20/100;
                modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&ans_pr_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&ans_pr_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit != MODEM_CONNECT_TONES_NONE)
                        false_hit = true;
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != tone_type)
                        false_miss = true;
                    /*endif*/
                }
                /*endif*/
                if (hit != MODEM_CONNECT_TONES_NONE)
                    printf("Detected at %5dHz %4ddB %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, level, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3F))
    {
        printf("Test 3f: Bell answer tone detection with level\n");
        tone_type = MODEM_CONNECT_TONES_BELL_ANS;
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 2225 - BELL_ANS_FREQ_TOLERANCE;  pitch <= 2225 + BELL_ANS_FREQ_TOLERANCE;  pitch += 2*BELL_ANS_FREQ_TOLERANCE)
        {
            for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            {
                /* Use the transmitter to test the receiver */
                modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
                /* Fudge things for the test */
                modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
                modem_tone_tx.level = dds_scaling_dbm0(level);
                modem_connect_tones_rx_init(&calling_tone_rx, tone_type, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&calling_tone_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&calling_tone_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit != MODEM_CONNECT_TONES_NONE)
                        false_hit = true;
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != tone_type)
                        false_miss = true;
                    /*endif*/
                }
                /*endif*/
                if (hit != MODEM_CONNECT_TONES_NONE)
                    printf("Detected at %5dHz %4ddB %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, level, calling_tone_rx.channel_level, calling_tone_rx.notch_level, modem_connect_tone_to_str(hit), hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3G))
    {
        printf("Test 3g: Calling tone detection with level\n");
        tone_type = MODEM_CONNECT_TONES_CALLING_TONE;
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 1300 - CALLING_TONE_FREQ_TOLERANCE;  pitch <= 1300 + CALLING_TONE_FREQ_TOLERANCE;  pitch += 2*CALLING_TONE_FREQ_TOLERANCE)
        {
            for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            {
                /* Use the transmitter to test the receiver */
                modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
                /* Fudge things for the test */
                modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
                modem_tone_tx.level = dds_scaling_dbm0(level);
                modem_connect_tones_rx_init(&calling_tone_rx, tone_type, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&calling_tone_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&calling_tone_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit != MODEM_CONNECT_TONES_NONE)
                        false_hit = true;
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != tone_type)
                        false_miss = true;
                    /*endif*/
                }
                /*endif*/
                if (hit != MODEM_CONNECT_TONES_NONE)
                    printf("Detected at %5dHz %4ddB %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, level, calling_tone_rx.channel_level, calling_tone_rx.notch_level, modem_connect_tone_to_str(hit), hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_4))
    {
        printf("Test 4: CED detection, when stimulated with V.21 preamble\n");
        false_hit = false;
        false_miss = false;

        /* Send 255 bits of preamble (0.85s, the minimum specified preamble for T.30), and then
           some random bits. Check the preamble detector comes on, and goes off at reasonable times. */
        fsk_tx_init(&preamble_tx, &preset_fsk_specs[FSK_V21CH2], preamble_get_bit, NULL);
        modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED_OR_PREAMBLE, preamble_detected, NULL);
        for (i = 0;  i < 2*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = fsk_tx(&preamble_tx, amp, SAMPLES_PER_CHUNK);
            modem_connect_tones_rx(&ced_rx, amp, samples);
        }
        /*endfor*/
        for (i = 0;  i < SAMPLE_RATE/10;  i += SAMPLES_PER_CHUNK)
        {
            memset(amp, 0, sizeof(int16_t)*SAMPLES_PER_CHUNK);
            modem_connect_tones_rx(&ced_rx, amp, SAMPLES_PER_CHUNK);
        }
        /*endfor*/
        if (preamble_on_at < 40  ||  preamble_on_at > 50
            ||
            preamble_off_at < 580  ||  preamble_off_at > 620)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_5A))
    {
        printf("Test 5A: ANS and ANS/ detection with reversal interval\n");
        tone_type = MODEM_CONNECT_TONES_ANS_PR;
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = false;
        false_miss = false;
        for (interval = 400;  interval < 800;  interval++)
        {
            printf("Reversal interval = %d\n", interval);
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            modem_connect_tones_rx_init(&ans_pr_rx, tone_type, ans_pr_detected, NULL);
            hits = 0;
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                when = i;
                samples = SAMPLES_PER_CHUNK;
                for (j = 0;  j < samples;  j++)
                {
                    if (--modem_tone_tx.hop_timer <= 0)
                    {
                        modem_tone_tx.hop_timer = ms_to_samples(interval);
                        modem_tone_tx.tone_phase += 0x80000000;
                    }
                    /*endif*/
                    amp[j] = dds_mod(&modem_tone_tx.tone_phase, modem_tone_tx.tone_phase_rate, modem_tone_tx.level, 0);
                }
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            if (interval < (450 - 25)  ||  interval > (450 + 25))
            {
                if (hits != 0)
                    false_hit = true;
                /*endif*/
            }
            else if (interval > (450 - 25)  &&  interval < (450 + 25))
            {
                if (hits == 0)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hits)
                printf("Detected at %5dHz %4ddB %dms %12" PRId32 " %12" PRId32 " %d\n", 2100, -11, interval, ans_pr_rx.channel_level, ans_pr_rx.notch_level, hits);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_5B))
    {
        printf("Test 5B: ANS and ANS/ detection with mixed reversal intervals\n");
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        tone_type = MODEM_CONNECT_TONES_ANS_PR;
        false_hit = false;
        false_miss = false;
        interval = 450;
        printf("Reversal interval = %d\n", interval);
        /* Use the transmitter to test the receiver */
        modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
        modem_connect_tones_rx_init(&ans_pr_rx, tone_type, ans_pr_detected, NULL);
        cycle = 0;
        hits = 0;
        for (i = 0;  i < 60*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            when = i;
            samples = SAMPLES_PER_CHUNK;
            for (j = 0;  j < samples;  j++)
            {
                if (--modem_tone_tx.hop_timer <= 0)
                {
                    if (++cycle == 10)
                        interval = 1000;
                    if (cycle == 20)
                        interval = 450;
                    modem_tone_tx.hop_timer = ms_to_samples(interval);
                    modem_tone_tx.tone_phase += 0x80000000;
                }
                amp[j] = dds_mod(&modem_tone_tx.tone_phase, modem_tone_tx.tone_phase_rate, modem_tone_tx.level, 0);
            }
            /*endfor*/
            for (j = 0;  j < samples;  j++)
                amp[j] += awgn(&chan_noise_source);
            /*endfor*/
            modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            /* TODO: Add test result detection logic. */
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_6A))
    {
        printf("Test 6a: ANSam detection with AM pitch\n");
        tone_type = MODEM_CONNECT_TONES_ANSAM;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 5;  pitch < 25;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.mod_phase_rate = dds_phase_rate(pitch);
            modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ans_pr_rx);
            if (pitch < (15 - 10)  ||  pitch > (15 + 10))
            {
                if (hit == tone_type)
                    false_hit = true;
                /*endif*/
            }
            else if (pitch > (15 - AM_FREQ_TOLERANCE)  &&  pitch < (15 + AM_FREQ_TOLERANCE))
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_6B))
    {
        printf("Test 6b: ANSam/ (Modulated EC-disable) detection with AM pitch\n");
        tone_type = MODEM_CONNECT_TONES_ANSAM_PR;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (pitch = 5;  pitch < 25;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.mod_phase_rate = dds_phase_rate(pitch);
            modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ans_pr_rx);
            if (pitch < (15 - 10)  ||  pitch > (15 + 10))
            {
                if (hit == tone_type)
                    false_hit = true;
                /*endif*/
            }
            else if (pitch > (15 - AM_FREQ_TOLERANCE)  &&  pitch < (15 + AM_FREQ_TOLERANCE))
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_7A))
    {
        printf("Test 7a: ANSam detection with AM depth\n");
        tone_type = MODEM_CONNECT_TONES_ANSAM;
        pitch = 2100;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (depth = 0;  depth < 40;  depth++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.mod_level = modem_tone_tx.level*depth/100;
            modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ans_pr_rx);
            if (depth < 10)
            {
                if (hit == tone_type)
                    false_hit = true;
                /*endif*/
            }
            else if (depth > 15)
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_7B))
    {
        printf("Test 7b: ANSam/ (Modulated EC-disable) detection with AM depth\n");
        tone_type = MODEM_CONNECT_TONES_ANSAM_PR;
        pitch = 2100;
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = false;
        false_miss = false;
        for (depth = 0;  depth < 40;  depth++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, tone_type);
            /* Fudge things for the test */
            modem_tone_tx.mod_level = modem_tone_tx.level*depth/100;
            modem_connect_tones_rx_init(&ans_pr_rx, tone_type, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ans_pr_rx);
            if (depth < 10)
            {
                if (hit == tone_type)
                    false_hit = true;
                /*endif*/
            }
            else if (depth > 15)
            {
                if (hit != tone_type)
                    false_miss = true;
                /*endif*/
            }
            /*endif*/
            if (hit != MODEM_CONNECT_TONES_NONE)
                printf("Detected at %5dHz %12" PRId32 " %12" PRId32 " %s (%d)\n", pitch, ans_pr_rx.channel_level, ans_pr_rx.notch_level, modem_connect_tone_to_str(hit), hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_8))
    {
        /* Talk-off test */
        /* Here we use the BellCore and Mitel talk off test tapes, intended for DTMF
           detector testing. Presumably they should also have value here, but I am not
           sure. If those voice snippets were chosen to be tough on DTMF detectors, they
           might go easy on detectors looking for different pitches. However, the
           Mitel DTMF test tape is known (the hard way) to exercise 2280Hz tone
           detectors quite well. */
        printf("Test 8: Talk-off test\n");
        modem_connect_tones_rx_init(&cng_rx, MODEM_CONNECT_TONES_FAX_CNG, NULL, NULL);
        modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED_OR_PREAMBLE, NULL, NULL);
        modem_connect_tones_rx_init(&ans_pr_rx, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);
        modem_connect_tones_rx_init(&bell_ans_rx, MODEM_CONNECT_TONES_BELL_ANS, NULL, NULL);
        modem_connect_tones_rx_init(&calling_tone_rx, MODEM_CONNECT_TONES_CALLING_TONE, NULL, NULL);
        for (j = 0;  bellcore_files[j][0];  j++)
        {
            if ((inhandle = sf_open_telephony_read(bellcore_files[j], 1)) == NULL)
            {
                fprintf(stderr, "    Cannot open speech file '%s'\n", bellcore_files[j]);
                exit (2);
            }
            /*endif*/

            when = 0;
            hits = 0;
            while ((frames = sf_readf_short(inhandle, amp, 8000)))
            {
                when++;
                modem_connect_tones_rx(&cng_rx, amp, frames);
                modem_connect_tones_rx(&ced_rx, amp, frames);
                modem_connect_tones_rx(&ans_pr_rx, amp, frames);
                modem_connect_tones_rx(&bell_ans_rx, amp, frames);
                modem_connect_tones_rx(&calling_tone_rx, amp, frames);
                if (modem_connect_tones_rx_get(&cng_rx) != MODEM_CONNECT_TONES_NONE)
                {
                    /* This is not a true measure of hits, as there might be more
                       than one in a block of data. However, since the only good
                       result is no hits, this approximation is OK. */
                    printf("Hit CNG at %ds\n", when);
                    hits++;
                    modem_connect_tones_rx_init(&cng_rx, MODEM_CONNECT_TONES_FAX_CNG, NULL, NULL);
                }
                /*endif*/
                if (modem_connect_tones_rx_get(&ced_rx) != MODEM_CONNECT_TONES_NONE)
                {
                    printf("Hit CED at %ds\n", when);
                    hits++;
                    modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED_OR_PREAMBLE, NULL, NULL);
                }
                /*endif*/
                if (modem_connect_tones_rx_get(&ans_pr_rx) != MODEM_CONNECT_TONES_NONE)
                {
                    printf("Hit EC disable at %ds\n", when);
                    hits++;
                    modem_connect_tones_rx_init(&ans_pr_rx, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);
                }
                /*endif*/
                if (modem_connect_tones_rx_get(&bell_ans_rx) != MODEM_CONNECT_TONES_NONE)
                {
                    printf("Hit calling tone at %ds\n", when);
                    hits++;
                    modem_connect_tones_rx_init(&bell_ans_rx, MODEM_CONNECT_TONES_BELL_ANS, NULL, NULL);
                }
                /*endif*/
                if (modem_connect_tones_rx_get(&calling_tone_rx) != MODEM_CONNECT_TONES_NONE)
                {
                    printf("Hit calling tone at %ds\n", when);
                    hits++;
                    modem_connect_tones_rx_init(&calling_tone_rx, MODEM_CONNECT_TONES_CALLING_TONE, NULL, NULL);
                }
                /*endif*/
            }
            /*endwhile*/
            if (sf_close_telephony(inhandle))
            {
                fprintf(stderr, "    Cannot close speech file '%s'\n", bellcore_files[j]);
                exit(2);
            }
            /*endif*/
            printf("    File %d gave %d false hits.\n", j + 1, hits);
        }
        /*endfor*/
        if (hits > 0)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if (decode_test_file)
    {
        printf("Decode file '%s'\n", decode_test_file);
        modem_connect_tones_rx_init(&cng_rx, MODEM_CONNECT_TONES_FAX_CNG, cng_detected, NULL);
        modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED_OR_PREAMBLE, ced_detected, NULL);
        modem_connect_tones_rx_init(&ans_pr_rx, MODEM_CONNECT_TONES_ANS_PR, ans_pr_detected, NULL);
        modem_connect_tones_rx_init(&bell_ans_rx, MODEM_CONNECT_TONES_BELL_ANS, bell_ans_detected, NULL);
        modem_connect_tones_rx_init(&calling_tone_rx, MODEM_CONNECT_TONES_CALLING_TONE, calling_tone_detected, NULL);
        hits = 0;
        if ((inhandle = sf_open_telephony_read(decode_test_file, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open speech file '%s'\n", decode_test_file);
            exit (2);
        }
        /*endif*/

        when = 0;
        hits = 0;
        while ((frames = sf_readf_short(inhandle, amp, 8000)))
        {
            when++;
            modem_connect_tones_rx(&cng_rx, amp, frames);
            modem_connect_tones_rx(&ced_rx, amp, frames);
            modem_connect_tones_rx(&ans_pr_rx, amp, frames);
            modem_connect_tones_rx(&bell_ans_rx, amp, frames);
            modem_connect_tones_rx(&calling_tone_rx, amp, frames);
        }
        /*endwhile*/
        if (sf_close_telephony(inhandle))
        {
            fprintf(stderr, "    Cannot close speech file '%s'\n", decode_test_file);
            exit(2);
        }
        /*endif*/
        printf("    File gave %d hits.\n", hits);
    }
    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
