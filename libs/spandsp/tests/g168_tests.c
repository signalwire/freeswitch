/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g168tests.c - Tests of the "test equipment" (filters, etc.) specified
 *               in G.168. This code is only for checking out the tools,
 *               not for testing an echo cancellor.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: g168_tests.c,v 1.19 2008/11/30 10:17:31 steveu Exp $
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <audiofile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp/g168models.h"

#define FALSE 0
#define TRUE (!FALSE)

typedef struct
{
    const char *name;
    int max;
    int cur;
    float gain;
    AFfilehandle handle;
    int16_t signal[SAMPLE_RATE];
} signal_source_t;

signal_source_t local_css;
signal_source_t far_css;

static AFfilehandle afOpenFile_telephony_read(const char *name, int channels)
{
    float x;
    AFfilehandle handle;

    if ((handle = afOpenFile(name, "r", 0)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", name);
        exit(2);
    }
    if ((x = afGetFrameSize(handle, AF_DEFAULT_TRACK, 1)) != 2.0)
    {
        fprintf(stderr, "    Unexpected frame size in wave file '%s'\n", name);
        exit(2);
    }
    if ((x = afGetRate(handle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
    {
        printf("    Unexpected sample rate in wave file '%s'\n", name);
        exit(2);
    }
    if ((x = afGetChannels(handle, AF_DEFAULT_TRACK)) != (float) channels)
    {
        printf("    Unexpected number of channels in wave file '%s'\n", name);
        exit(2);
    }

    return handle;
}
/*- End of function --------------------------------------------------------*/

#if 0
static AFfilehandle afOpenFile_telephony_write(const char *name, int channels)
{
    AFfilesetup setup;
    AFfilehandle handle;

    if ((setup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    %s: Failed to create file setup\n", name);
        exit(2);
    }
    afInitSampleFormat(setup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(setup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(setup, AF_FILE_WAVE);
    afInitChannels(setup, AF_DEFAULT_TRACK, channels);

    if ((handle = afOpenFile(name, "w", setup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open result file\n");
        exit(2);
    }
    afFreeFileSetup(setup);

    return handle;
}
/*- End of function --------------------------------------------------------*/
#endif

static void signal_load(signal_source_t *sig, const char *name)
{
    sig->handle = afOpenFile_telephony_read(name, 1);
    sig->name = name;
    sig->max = afReadFrames(sig->handle, AF_DEFAULT_TRACK, sig->signal, SAMPLE_RATE);
    if (sig->max < 0)
    {
        fprintf(stderr, "    Error reading sound file '%s'\n", sig->name);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void signal_free(signal_source_t *sig)
{
    if (afCloseFile(sig->handle) != 0)
    {
        fprintf(stderr, "    Cannot close sound file '%s'\n", sig->name);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void signal_restart(signal_source_t *sig, float gain)
{
    sig->cur = 0;
    sig->gain = powf(10.0f, gain/20.0f);
}
/*- End of function --------------------------------------------------------*/

static int16_t signal_amp(signal_source_t *sig)
{
    int16_t tx;

    tx = sig->signal[sig->cur++]*sig->gain;
    if (sig->cur >= sig->max)
        sig->cur = 0;
    return tx;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int f;
    int i;
    int16_t amp[8000];
    int16_t value;
    int signal;
    float power[10];
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_state;
    awgn_state_t noise_source;
    fir32_state_t line_model_d2;
    fir32_state_t line_model_d3;
    fir32_state_t line_model_d4;
    fir32_state_t line_model_d5;
    fir32_state_t line_model_d6;
    fir32_state_t line_model_d7;
    fir32_state_t line_model_d8;
    fir32_state_t line_model_d9;
    fir_float_state_t level_measurement_bp;

    signal_load(&local_css, "sound_c1_8k.wav");
    signal_load(&far_css, "sound_c3_8k.wav");

    fir32_create(&line_model_d2,
                 line_model_d2_coeffs,
                 sizeof(line_model_d2_coeffs)/sizeof(int32_t));
    fir32_create(&line_model_d3,
                 line_model_d3_coeffs,
                 sizeof(line_model_d3_coeffs)/sizeof(int32_t));
    fir32_create(&line_model_d4,
                 line_model_d4_coeffs,
                 sizeof(line_model_d4_coeffs)/sizeof(int32_t));
    fir32_create(&line_model_d5,
                 line_model_d5_coeffs,
                 sizeof(line_model_d5_coeffs)/sizeof(int32_t));
    fir32_create(&line_model_d6,
                 line_model_d6_coeffs,
                 sizeof(line_model_d6_coeffs)/sizeof(int32_t));
    fir32_create(&line_model_d7,
                 line_model_d7_coeffs,
                 sizeof(line_model_d7_coeffs)/sizeof(int32_t));
    fir32_create(&line_model_d8,
                 line_model_d8_coeffs,
                 sizeof(line_model_d8_coeffs)/sizeof(int32_t));
    fir32_create(&line_model_d9,
                 line_model_d9_coeffs,
                 sizeof(line_model_d9_coeffs)/sizeof(int32_t));
    fir_float_create(&level_measurement_bp,
                     level_measurement_bp_coeffs,
                     sizeof(level_measurement_bp_coeffs)/sizeof(float));

    for (f = 10;  f < 4000;  f++)
    {
         make_tone_gen_descriptor(&tone_desc,
                                  f,
                                  -10,
                                  0,
                                  0,
                                  1,
                                  0,
                                  0,
                                  0,
                                  TRUE);
        tone_gen_init(&tone_state, &tone_desc);
        tone_gen(&tone_state, amp, 8000);
        for (i = 0;  i < 10;  i++)
            power[i] = 0.0f;
        for (i = 0;  i < 800;  i++)
        {
            signal = fir32(&line_model_d2, amp[i]);
            power[0] += ((signal*signal - power[0])/32.0f);
            signal = fir32(&line_model_d3, amp[i]);
            power[1] += ((signal*signal - power[1])/32.0f);
            signal = fir32(&line_model_d4, amp[i]);
            power[2] += ((signal*signal - power[2])/32.0f);
            signal = fir32(&line_model_d5, amp[i]);
            power[3] += ((signal*signal - power[3])/32.0f);
            signal = fir32(&line_model_d6, amp[i]);
            power[4] += ((signal*signal - power[4])/32.0f);
            signal = fir32(&line_model_d7, amp[i]);
            power[5] += ((signal*signal - power[5])/32.0f);
            signal = fir32(&line_model_d8, amp[i]);
            power[6] += ((signal*signal - power[6])/32.0f);
            signal = fir32(&line_model_d9, amp[i]);
            power[7] += ((signal*signal - power[7])/32.0f);
            signal = fir_float(&level_measurement_bp, amp[i]);
            power[8] += ((signal*signal - power[8])/32.0f);
            signal = amp[i];
            power[9] += ((signal*signal - power[9])/32.0f);
        }
        printf("%d %f %f %f %f %f %f %f %f %f %f\n",
               f,
               sqrt(power[0])*LINE_MODEL_D2_GAIN,
               sqrt(power[1])*LINE_MODEL_D3_GAIN,
               sqrt(power[2])*LINE_MODEL_D4_GAIN,
               sqrt(power[3])*LINE_MODEL_D5_GAIN,
               sqrt(power[4])*LINE_MODEL_D6_GAIN,
               sqrt(power[5])*LINE_MODEL_D7_GAIN,
               sqrt(power[6])*LINE_MODEL_D8_GAIN,
               sqrt(power[7])*LINE_MODEL_D9_GAIN,
               sqrt(power[8]),
               sqrt(power[9]));
    }
    awgn_init_dbm0(&noise_source, 1234567, -20.0f);
    for (i = 0;  i < 10;  i++)
        power[i] = 0.0f;
    signal_restart(&local_css, 0.0f);
    signal_restart(&far_css, 0.0f);
    for (i = 0;  i < SAMPLE_RATE;  i++)
    {
        value = signal_amp(&local_css);
        //value = awgn(&noise_source);
        signal = fir32(&line_model_d2, value);
        power[0] += ((signal*signal - power[0])/32.0f);
        signal = fir32(&line_model_d3, value);
        power[1] += ((signal*signal - power[1])/32.0f);
        signal = fir32(&line_model_d4, value);
        power[2] += ((signal*signal - power[2])/32.0f);
        signal = fir32(&line_model_d5, value);
        power[3] += ((signal*signal - power[3])/32.0f);
        signal = fir32(&line_model_d6, value);
        power[4] += ((signal*signal - power[4])/32.0f);
        signal = fir32(&line_model_d7, value);
        power[5] += ((signal*signal - power[5])/32.0f);
        signal = fir32(&line_model_d8, value);
        power[6] += ((signal*signal - power[6])/32.0f);
        signal = fir32(&line_model_d9, value);
        power[7] += ((signal*signal - power[7])/32.0f);
        signal = fir_float(&level_measurement_bp, value);
        power[8] += ((signal*signal - power[8])/32.0f);
        signal = value;
        power[9] += ((signal*signal - power[9])/32.0f);
    }
    for (i = 0;  i < 10;  i++)
        power[i] = 0.0f;
    for (i = 0;  i < SAMPLE_RATE;  i++)
    {
        value = signal_amp(&local_css);
        //value = awgn(&noise_source);
        signal = fir32(&line_model_d2, value);
        power[0] += ((signal*signal - power[0])/32.0f);
        signal = fir32(&line_model_d3, value);
        power[1] += ((signal*signal - power[1])/32.0f);
        signal = fir32(&line_model_d4, value);
        power[2] += ((signal*signal - power[2])/32.0f);
        signal = fir32(&line_model_d5, value);
        power[3] += ((signal*signal - power[3])/32.0f);
        signal = fir32(&line_model_d6, value);
        power[4] += ((signal*signal - power[4])/32.0f);
        signal = fir32(&line_model_d7, value);
        power[5] += ((signal*signal - power[5])/32.0f);
        signal = fir32(&line_model_d8, value);
        power[6] += ((signal*signal - power[6])/32.0f);
        signal = fir32(&line_model_d9, value);
        power[7] += ((signal*signal - power[7])/32.0f);
        signal = fir_float(&level_measurement_bp, value);
        power[8] += ((signal*signal - power[8])/32.0f);
        signal = value;
        power[9] += ((signal*signal - power[9])/32.0f);
    }
    printf("%d %f %f %f %f %f %f %f %f %f %f\n",
           0,
           sqrt(power[0])*LINE_MODEL_D2_GAIN,
           sqrt(power[1])*LINE_MODEL_D3_GAIN,
           sqrt(power[2])*LINE_MODEL_D4_GAIN,
           sqrt(power[3])*LINE_MODEL_D5_GAIN,
           sqrt(power[4])*LINE_MODEL_D6_GAIN,
           sqrt(power[5])*LINE_MODEL_D7_GAIN,
           sqrt(power[6])*LINE_MODEL_D8_GAIN,
           sqrt(power[7])*LINE_MODEL_D9_GAIN,
           sqrt(power[8]),
           sqrt(power[9]));
    printf("%d %f %f %f %f %f %f %f %f %f %f\n",
           0,
           sqrt(power[0]),
           sqrt(power[1]),
           sqrt(power[2]),
           sqrt(power[3]),
           sqrt(power[4]),
           sqrt(power[5]),
           sqrt(power[6]),
           sqrt(power[7]),
           sqrt(power[8]),
           sqrt(power[9]));
    printf("%d %f %f %f %f %f %f %f %f %f %f\n",
           0,
           sqrt(power[0])/sqrt(power[9]),
           sqrt(power[1])/sqrt(power[9]),
           sqrt(power[2])/sqrt(power[9]),
           sqrt(power[3])/sqrt(power[9]),
           sqrt(power[4])/sqrt(power[9]),
           sqrt(power[5])/sqrt(power[9]),
           sqrt(power[6])/sqrt(power[9]),
           sqrt(power[7])/sqrt(power[9]),
           sqrt(power[8]),
           sqrt(power[9]));
    printf("%d %f %f %f %f %f %f %f %f %f %f\n",
           0,
           sqrt(power[0])*LINE_MODEL_D2_GAIN/sqrt(power[9]),
           sqrt(power[1])*LINE_MODEL_D3_GAIN/sqrt(power[9]),
           sqrt(power[2])*LINE_MODEL_D4_GAIN/sqrt(power[9]),
           sqrt(power[3])*LINE_MODEL_D5_GAIN/sqrt(power[9]),
           sqrt(power[4])*LINE_MODEL_D6_GAIN/sqrt(power[9]),
           sqrt(power[5])*LINE_MODEL_D7_GAIN/sqrt(power[9]),
           sqrt(power[6])*LINE_MODEL_D8_GAIN/sqrt(power[9]),
           sqrt(power[7])*LINE_MODEL_D9_GAIN/sqrt(power[9]),
           sqrt(power[8]),
           sqrt(power[9]));

    for (i = 0;  i < (int) (sizeof(css_c1)/sizeof(css_c1[0]));  i++)
        printf("%d\n", css_c1[i]);
    printf("\n");
    for (i = 0;  i < (int) (sizeof(css_c1)/sizeof(css_c3[0]));  i++)
        printf("%d\n", css_c3[i]);
    signal_free(&local_css);
    signal_free(&far_css);
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
