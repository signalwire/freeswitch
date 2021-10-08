/*
 * SpanDSP - a series of DSP components for telephony
 *
 * power_meter_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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

/*! \page power_meter_tests_page Power meter tests
\section power_meter_tests_page_sec_1 What does it do?
These tests assess the accuracy of power meters built from the power meter module.
Both tones and noise are used to check the meter's behaviour.

\section power_meter_tests_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <float.h>
#include <time.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define IN_FILE_NAME        "../test-data/local/short_nb_voice.wav"
#define OUT_FILE_NAME       "power_meter_tests.wav"

static int power_surge_detector_tests(void)
{
    SNDFILE *outhandle;
    power_surge_detector_state_t *sig;
    int i;
    int sample;
    int16_t amp[8000];
    int16_t amp_out[2*8000];
    awgn_state_t *awgnx;
    int32_t phase_rate;
    uint32_t phase_acc;
    int16_t phase_scale;
    float signal_power;
    int32_t signal_level;
    int signal_present;
    int prev_signal_present;
    int ok;
    int extremes[4];

    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 2)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    sig = power_surge_detector_init(NULL, -50.0f, 5.0f);
    prev_signal_present = false;

    phase_rate = dds_phase_rate(450.0f);
    phase_acc = 0;

    phase_scale = dds_scaling_dbm0(-33.0f);
    awgnx = awgn_init_dbm0(NULL, 1234567, -45.0f);

    extremes[0] = 8001;
    extremes[1] = -1;
    extremes[2] = 8001;
    extremes[3] = -1;
    for (sample = 0;  sample < 800000;  sample += 8000)
    {
        ok = 0;
        for (i = 0;  i < 8000;  i++)
        {
            amp[i] = awgn(awgnx);
            if (i < 4000)
                amp[i] += dds_mod(&phase_acc, phase_rate, phase_scale, 0);

            signal_level = power_surge_detector(sig, amp[i]);
            signal_present = (signal_level != 0);
            if (prev_signal_present != signal_present)
            {
                signal_power = power_surge_detector_current_dbm0(sig);
                if (signal_present)
                {
                    if (ok == 0  &&  i >= 0  &&  i < 25)
                        ok = 1;
                    if (extremes[0] > i)
                        extremes[0] = i;
                    if (extremes[1] < i)
                        extremes[1] = i;
                    printf("On at %f (%fdBm0)\n", (sample + i)/8000.0, signal_power);
                }
                else
                {
                    if (ok == 1  &&  i >= 4000 + 0  &&  i < 4000 + 35)
                        ok = 2;
                    if (extremes[2] > i)
                        extremes[2] = i;
                    if (extremes[3] < i)
                        extremes[3] = i;
                    printf("Off at %f (%fdBm0)\n", (sample + i)/8000.0, signal_power);
                }
                prev_signal_present = signal_present;
            }
            amp_out[2*i] = amp[i];
            amp_out[2*i + 1] = signal_present*5000;
        }
        sf_writef_short(outhandle, amp_out, 8000);
        if (ok != 2
            ||
            extremes[0] < 1
            ||
            extremes[1] > 30
            ||
            extremes[2] < 4001
            ||
            extremes[3] > 4030)
        {
            printf("    Surge not detected correctly (%d)\n", ok);
            exit(2);
        }
    }
    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    printf("Min on %d, max on %d, min off %d, max off %d\n", extremes[0], extremes[1], extremes[2], extremes[3]);
    power_surge_detector_free(sig);
    awgn_free(awgnx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int power_surge_detector_file_test(const char *file)
{
    SNDFILE *inhandle;
    SNDFILE *outhandle;
    int inframes;
    power_surge_detector_state_t *sig;
    int i;
    int16_t amp[8000];
    int16_t amp_out[2*8000];
    int sample;
    float signal_power;
    int32_t signal_level;
    int signal_present;
    int prev_signal_present;

    if ((inhandle = sf_open_telephony_read(file, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", file);
        exit(2);
    }

    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    sig = power_surge_detector_init(NULL, -50.0f, 6.0f);
    prev_signal_present = false;

    sample = 0;
    while ((inframes = sf_readf_short(inhandle, amp, 8000)))
    {
        for (i = 0;  i < inframes;  i++)
        {
            signal_level = power_surge_detector(sig, amp[i]);
            signal_present = (signal_level != 0);
            if (prev_signal_present != signal_present)
            {
                signal_power = power_surge_detector_current_dbm0(sig);
                if (signal_present)
                    printf("On at %f (%fdBm0)\n", (sample + i)/8000.0, signal_power);
                else
                    printf("Off at %f (%fdBm0)\n", (sample + i)/8000.0, signal_power);
                prev_signal_present = signal_present;
            }
            amp_out[2*i] = amp[i];
            amp_out[2*i + 1] = signal_present*5000;
        }
        sf_writef_short(outhandle, amp_out, inframes);
        sample += inframes;
    }
    if (sf_close_telephony(inhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", file);
        exit(2);
    }
    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int power_meter_tests(void)
{
    awgn_state_t noise_source;
    power_meter_t meter;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t gen;
    int i;
    int idum = 1234567;
    int16_t amp[1000];
    int len;
    int32_t level;

    power_meter_init(&meter, 7);
    printf("Testing with zero in the power register\n");
    printf("Power: expected %fdBm0, got %fdBm0\n", -90.169f, power_meter_current_dbm0(&meter));
    printf("Power: expected %fdBOv, got %fdBOv\n", -96.329f, power_meter_current_dbov(&meter));

    printf("Testing with a square wave 10dB from maximum\n");
    for (i = 0;  i < 1000;  i++)
    {
        amp[i] = (i & 1)  ?  10362  :  -10362;
        level = power_meter_update(&meter, amp[i]);
        //printf("%12d %fdBm0 %fdBov\n", level, power_meter_current_dbm0(&meter), power_meter_current_dbov(&meter));
    }
    printf("Level: expected %" PRId32 "/%" PRId32 ", got %" PRId32 "\n", power_meter_level_dbov(-10.0f), power_meter_level_dbm0(-10.0f + DBM0_MAX_POWER), level);
    printf("Power: expected %fdBm0, got %fdBm0\n", -10.0f + DBM0_MAX_POWER, power_meter_current_dbm0(&meter));
    printf("Power: expected %fdBOv, got %fdBOv\n", -10.0f, power_meter_current_dbov(&meter));
    if (level < power_meter_level_dbov(-10.0f)*0.99f
        ||
        level > power_meter_level_dbov(-10.0f)*1.01f)
    {
        printf("Test failed (level)\n");
        exit(2);
    }
    if (0.1f < fabsf(power_meter_current_dbm0(&meter) + 10.0f - DBM0_MAX_POWER))
    {
        printf("Test failed (dBm0)\n");
        exit(2);
    }
    if (0.1f < fabsf(power_meter_current_dbov(&meter) + 10.0))
    {
        printf("Test failed (dBOv)\n");
        exit(2);
    }

    printf("Testing with a sine wave tone 10dB from maximum\n");
    tone_gen_descriptor_init(&tone_desc,
                             1000,
                             -4,
                             0,
                             1,
                             1,
                             0,
                             0,
                             0,
                             true);
    tone_gen_init(&gen, &tone_desc);
    len = tone_gen(&gen, amp, 1000);
    for (i = 0;  i < len;  i++)
    {
        level = power_meter_update(&meter, amp[i]);
        //printf("%12d %fdBm0 %fdBov\n", level, power_meter_current_dbm0(&meter), power_meter_current_dbov(&meter));
    }
    printf("Level: expected %" PRId32 "/%" PRId32 ", got %" PRId32 "\n", power_meter_level_dbov(-10.0f), power_meter_level_dbm0(-10.0f + DBM0_MAX_POWER), level);
    printf("Power: expected %fdBm0, got %fdBm0\n", -10.0f + DBM0_MAX_POWER, power_meter_current_dbm0(&meter));
    printf("Power: expected %fdBOv, got %fdBOv\n", -10.0f, power_meter_current_dbov(&meter));
    if (level < power_meter_level_dbov(-10.0f)*0.95f
        ||
        level > power_meter_level_dbov(-10.0f)*1.05f)
    {
        printf("Test failed (level)\n");
        exit(2);
    }
    if (0.2f < fabsf(power_meter_current_dbm0(&meter) + 10.0f - DBM0_MAX_POWER))
    {
        printf("Test failed (dBm0)\n");
        exit(2);
    }
    if (0.2f < fabsf(power_meter_current_dbov(&meter) + 10.0))
    {
        printf("Test failed (dBOv)\n");
        exit(2);
    }

    printf("Testing with AWGN 10dB from maximum\n");
    awgn_init_dbov(&noise_source, idum, -10.0f);
    for (i = 0;  i < 1000;  i++)
        amp[i] = awgn(&noise_source);
    for (i = 0;  i < 1000;  i++)
    {
        level = power_meter_update(&meter, amp[i]);
        //printf("%12d %fdBm0 %fdBov\n", level, power_meter_current_dbm0(&meter), power_meter_current_dbov(&meter));
    }
    printf("Level: expected %" PRId32 "/%" PRId32 ", got %" PRId32 "\n", power_meter_level_dbov(-10.0f), power_meter_level_dbm0(-10.0f + DBM0_MAX_POWER), level);
    printf("Power: expected %fdBm0, got %fdBm0\n", -10.0f + DBM0_MAX_POWER, power_meter_current_dbm0(&meter));
    printf("Power: expected %fdBOv, got %fdBOv\n", -10.0f, power_meter_current_dbov(&meter));
    if (level < power_meter_level_dbov(-10.0f)*0.95f
        ||
        level > power_meter_level_dbov(-10.0f)*1.05f)
    {
        printf("Test failed (level)\n");
        exit(2);
    }
    if (0.2f < fabsf(power_meter_current_dbm0(&meter) + 10.0f - DBM0_MAX_POWER))
    {
        printf("Test failed (dBm0)\n");
        exit(2);
    }
    if (0.2f < fabsf(power_meter_current_dbov(&meter) + 10.0f))
    {
        printf("Test failed (dBOv)\n");
        exit(2);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    bool basic_tests;
    bool decode;
    int opt;
    const char *in_file;

    basic_tests = true;
    decode = false;
    in_file = IN_FILE_NAME;
    while ((opt = getopt(argc, argv, "d:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            in_file = optarg;
            basic_tests = false;
            decode = true;
            break;
        default:
            //usage();
            exit(2);
        }
    }

    if (basic_tests)
    {
        power_meter_tests();
        power_surge_detector_tests();
    }
    if (decode)
    {
        power_surge_detector_file_test(in_file);
    }
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
