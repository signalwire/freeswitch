/*
 * SpanDSP - a series of DSP components for telephony
 *
 * modem_echo_tests.c
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

/*! \page modem_echo_can_tests_page Line echo cancellation for modems tests
\section modem_echo_can_tests_page_sec_1 What does it do?
Currently the echo cancellation tests only provide simple exercising of the
cancellor in the way it might be used for line echo cancellation. The test code
is in echotests.c. 

The goal is to test the echo cancellor again the G.16X specs. Clearly, that also
means the goal for the cancellor itself is to comply with those specs. Right
now, the only aspect of these tests implemented is the line impulse response
models in g168tests.c. 

\section modem_echo_can_tests_page_sec_2 How does it work?
The current test consists of feeding an audio file of real speech to the echo
cancellor as the transmit signal. A very simple model of a telephone line is
used to simulate a simple echo from the transmit signal. A second audio file of
real speech is also used to simulate a signal received form the far end of the
line. This is gated so it is only placed for one second every 10 seconds,
simulating the double talk condition. The resulting echo cancelled signal can
either be store in a file for further analysis, or played back as the data is
processed. 

A number of modified versions of this test have been performed. The signal level
of the two speech sources has been varied. Several simple models of the
telephone line have been used. Although the current cancellor design has known
limitations, it seems stable for all these test conditions. No instability has
been observed in the current version due to arithmetic overflow when the speech
is very loud (with earlier versions, well, ....:) ). The lack of saturating
arithmetic in general purpose CPUs is a huge disadvantage here, as software
saturation logic would cause a major slow down. Floating point would be good,
but is not usable in the Linux kernel. Anyway, the bottom line seems to be the
current design is genuinely useful, if imperfect. 

\section modem_echo_can_tests_page_sec_2 How do I use it?

Build the tests with the command "./build". Currently there is no proper make
setup, or way to build individual tests. "./build" will built all the tests
which currently exist for the DSP functions. The echo cancellation test assumes
there are two audio files containing mono, 16 bit signed PCM speech data, sampled
at 8kHz. These should be called local_sound.wav and far_sound.wav. A third wave
file will be produced. This very crudely starts with the first 256 bytes from
the local_sound.wav file, followed by the results of the echo cancellation. The
resulting audio is also played to the /dev/dsp device. A printf near the end of
echo_tests.c is commented out with a \#if. If this is enabled, detailed
information about the results of the echo cancellation will be written to
stdout. By saving this into a file, Grace (recommended), GnuPlot, or some other
plotting package may be used to graphically display the functioning of the
cancellor.  
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)  &&  defined(HAVE_FL_FL_AUDIO_METER_H)
#define ENABLE_GUI
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sndfile.h>
#if defined(HAVE_MATH_H)
#define GEN_CONST
#endif

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp/g168models.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "echo_monitor.h"
#endif

#if !defined(NULL)
#define NULL (void *) 0
#endif

typedef struct
{
    const char *name;
    int max;
    int cur;
    SNDFILE *handle;
    int16_t signal[8000];
} signal_source_t;

signal_source_t local_css;

fir32_state_t line_model;

SNDFILE *resulthandle;
int16_t residue_sound[8000];
int residue_cur = 0;
int do_codec_munge = TRUE;
int use_gui = FALSE;

static const int16_t tone_1khz[] = {10362, 7327, 0, -7327, -10362, -7327, 0, 7327};

static inline void put_residue(int16_t tx, int16_t residue)
{
    int outframes;

    residue_sound[residue_cur++] = tx;
    residue_sound[residue_cur++] = residue;
    if (residue_cur >= 8000)
    {
        residue_cur >>= 1;
        outframes = sf_writef_short(resulthandle, residue_sound, residue_cur);
        if (outframes != residue_cur)
        {
            fprintf(stderr, "    Error writing residue sound\n");
            exit(2);
        }
        residue_cur = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static void signal_load(signal_source_t *sig, const char *name)
{
    sig->name = name;
    if ((sig->handle = sf_open_telephony_read(sig->name, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open sound file '%s'\n", sig->name);
        exit(2);
    }
    sig->max = sf_readf_short(sig->handle, sig->signal, 8000);
    if (sig->max < 0)
    {
        fprintf(stderr, "    Error reading sound file '%s'\n", sig->name);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void signal_free(signal_source_t *sig)
{
    if (sf_close_telephony(sig->handle))
    {
        fprintf(stderr, "    Cannot close sound file '%s'\n", sig->name);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void signal_restart(signal_source_t *sig)
{
    sig->cur = 0;
}
/*- End of function --------------------------------------------------------*/

static int16_t signal_amp(signal_source_t *sig)
{
    int16_t tx;

    tx = sig->signal[sig->cur++];
    if (sig->cur >= sig->max)
        sig->cur = 0;
    return tx;
}
/*- End of function --------------------------------------------------------*/

static inline int16_t codec_munger(int16_t amp)
{
    if (do_codec_munge)
        return alaw_to_linear(linear_to_alaw(amp));
    return amp;
}
/*- End of function --------------------------------------------------------*/

static void channel_model_create(int model)
{
    static const int32_t *line_models[] =
    {
        line_model_d2_coeffs,
        line_model_d3_coeffs,
        line_model_d4_coeffs,
        line_model_d5_coeffs,
        line_model_d6_coeffs,
        line_model_d7_coeffs,
        line_model_d8_coeffs,
        line_model_d9_coeffs
    };

    static int line_model_sizes[] =
    {
        sizeof(line_model_d2_coeffs)/sizeof(int32_t),
        sizeof(line_model_d3_coeffs)/sizeof(int32_t),
        sizeof(line_model_d4_coeffs)/sizeof(int32_t),
        sizeof(line_model_d5_coeffs)/sizeof(int32_t),
        sizeof(line_model_d6_coeffs)/sizeof(int32_t),
        sizeof(line_model_d7_coeffs)/sizeof(int32_t),
        sizeof(line_model_d8_coeffs)/sizeof(int32_t),
        sizeof(line_model_d9_coeffs)/sizeof(int32_t)
    };

    fir32_create(&line_model, line_models[model], line_model_sizes[model]);
}
/*- End of function --------------------------------------------------------*/

static int16_t channel_model(int16_t local, int16_t far)
{
    int16_t echo;
    int16_t rx;

    /* Channel modelling is merely simulating the effects of A-law distortion
       and using one of the echo models from G.168 */

    /* The local tx signal will have gone through an A-law munging before
       it reached the line's analogue area where the echo occurs. */
    echo = fir32(&line_model, codec_munger(local/8));
    /* The far end signal will have been through an A-law munging, although
       this should not affect things. */
    rx = echo + codec_munger(far);
    /* This mixed echo and far end signal will have been through an A-law munging when it came back into
       the digital network. */
    rx = codec_munger(rx);
    return rx;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    modem_echo_can_state_t *ctx;
    //awgn_state_t local_noise_source;
    awgn_state_t far_noise_source;
    int i;
    int clean;
    int16_t rx;
    int16_t tx;
    int line_model_no;
    time_t now;
    power_meter_t power_before;
    power_meter_t power_after;
    float unadapted_output_power;
    float unadapted_echo_power;
    float adapted_output_power;
    float adapted_echo_power;
#if defined(ENABLE_GUI)
    int16_t amp[2];
#endif
    
    line_model_no = 0;
    use_gui = FALSE;
    for (i = 1;  i < argc;  i++)
    {
        if (strcmp(argv[i], "-g") == 0)
        {
            use_gui = TRUE;
            continue;
        }
        line_model_no = atoi(argv[1]);
    }
    time(&now);
    ctx = modem_echo_can_init(256);
    awgn_init_dbm0(&far_noise_source, 7162534, -50.0f);

    signal_load(&local_css, "sound_c1_8k.wav");

    if ((resulthandle = sf_open_telephony_write("modem_echo.wav", 2)) == NULL)
    {
        fprintf(stderr, "    Failed to open result file\n");
        exit(2);
    }

#if defined(ENABLE_GUI)
    if (use_gui)
        start_echo_can_monitor(256);
#endif

    channel_model_create(line_model_no);
#if defined(ENABLE_GUI)
    if (use_gui)
        echo_can_monitor_line_model_update(line_model.coeffs, line_model.taps);
#endif

    modem_echo_can_flush(ctx);

    power_meter_init(&power_before, 5);
    power_meter_init(&power_after, 5);
    
    /* Measure the echo power before adaption */
    modem_echo_can_adaption_mode(ctx, FALSE);
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = tone_1khz[i & 7];
        rx = channel_model(tx, 0);
        clean = modem_echo_can_update(ctx, tx, rx);
        power_meter_update(&power_before, rx);
        power_meter_update(&power_after, clean);
    }
    unadapted_output_power = power_meter_current_dbm0(&power_before);
    unadapted_echo_power = power_meter_current_dbm0(&power_after);
    printf("Pre-adaption: output power %10.5fdBm0, echo power %10.5fdBm0\n", unadapted_output_power, unadapted_echo_power);
    
    /* Converge the canceller */
    signal_restart(&local_css);
    modem_echo_can_adaption_mode(ctx, TRUE);
    for (i = 0;  i < 800*2;  i++)
    {
        clean = modem_echo_can_update(ctx, 0, 0);
        put_residue(0, clean);
    }

    for (i = 0;  i < 8000*50;  i++)
    {
        tx = signal_amp(&local_css);
        rx = channel_model(tx, 0);
        clean = modem_echo_can_update(ctx, tx, rx);
        power_meter_update(&power_before, rx);
        power_meter_update(&power_after, clean);
#if 0
        if (i%800 == 0)
            printf("Powers %10.5fdBm0 %10.5fdBm0\n", power_meter_current_dbm0(&power_before), power_meter_current_dbm0(&power_after));
#endif
        put_residue(tx, clean);
#if defined(ENABLE_GUI)
        if (use_gui)
        {
            echo_can_monitor_can_update(ctx->fir_taps16, 256);
            amp[0] = tx;
            echo_can_monitor_line_spectrum_update(amp, 1);
        }
#endif
    }

    /* Now lets see how well adapted we are */
    modem_echo_can_adaption_mode(ctx, FALSE);
    for (i = 0;  i < 8000*5;  i++)
    {
        tx = tone_1khz[i & 7];
        rx = channel_model(tx, 0);
        clean = modem_echo_can_update(ctx, tx, rx);
        power_meter_update(&power_before, rx);
        power_meter_update(&power_after, clean);
    }
    adapted_output_power = power_meter_current_dbm0(&power_before);
    adapted_echo_power = power_meter_current_dbm0(&power_after);
    printf("Post-adaption: output power %10.5fdBm0, echo power %10.5fdBm0\n", adapted_output_power, adapted_echo_power);
    
    if (fabsf(adapted_output_power - unadapted_output_power) > 0.1f
        ||
        adapted_echo_power > unadapted_echo_power - 30.0f)
    {
        printf("Tests failed.\n");
        exit(2);
    }

    modem_echo_can_free(ctx);
    signal_free(&local_css);

    if (sf_close_telephony(resulthandle))
    {
        fprintf(stderr, "    Cannot close speech file '%s'\n", "result_sound.wav");
        exit(2);
    }

#if defined(ENABLE_GUI)
    if (use_gui)
        echo_can_monitor_wait_to_end();
#endif

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
