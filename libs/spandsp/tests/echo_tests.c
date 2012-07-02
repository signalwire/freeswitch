/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
 *
 * Based on a bit from here, a bit from there, eye of toad,
 * ear of bat, etc - plus, of course, my own 2 cents.
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

/*! \page echo_can_tests_page Line echo cancellation for voice tests

\section echo_can_tests_page_sec_1 What does it do?
The echo cancellation tests test the echo cancellor against the G.168 spec. Not
all the tests in G.168 are fully implemented at this time.

\section echo_can_tests_page_sec_2 How does it work?

\section echo_can_tests_page_sec_2 How do I use it?

*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)  &&  defined(HAVE_FL_FL_AUDIO_METER_H)
#define ENABLE_GUI
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <sndfile.h>

#define GEN_CONST
#include <math.h>

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

#define TEST_EC_TAPS            256

#define RESIDUE_FILE_NAME       "residue_sound.wav"

/*
    The key signal names, as defined in G.168

          +--------------+       +------------+
          |              |  Sin  |            |
Sgen -->--|   Echoey     |--->---|  Echo      |-->-- Sout
          |              |       |            |
          |   World      |  Rout |  Canceller |
     --<--|              |---<---|            |--<-- Rin
          |              |       |            |
          +--------------+       +------------+

    Echoey world model. Munge means linear->PCM->linear distortion.
          +-------------------------+
          |                         |  Sin
Sgen -->--|-->munge--->sum-->munge--|--->---
          |        +-->             |
          |       FIR               |  Rout
     --<--|--------+--------munge<--|---<---
          |                         |
          +-------------------------+
*/

typedef struct
{
    const char *name;
    int max;
    int cur;
    float gain;
    SNDFILE *handle;
    int16_t signal[SAMPLE_RATE];
} signal_source_t;

/* Level measurement device, specified in G.168 section 6.4.1.2.1 */
typedef struct
{
    int type;
    fir_float_state_t *fir;
    float history[35*8];
    int pos;
    float factor; 
    float power;
    float peak;
} level_measurement_device_t;

typedef struct
{
    int model_no;
    float erl;
    fir32_state_t impulse;
    float gain;
    int munging_codec;
} channel_model_state_t;

channel_model_state_t chan_model;

signal_source_t local_css;
signal_source_t far_css;
awgn_state_t local_noise_source;
awgn_state_t far_noise_source;

SNDFILE *residue_handle;
int16_t residue_sound[SAMPLE_RATE];
int residue_cur = 0;

level_measurement_device_t *power_meter_1;
level_measurement_device_t *power_meter_2;

int line_model_no;
int supp_line_model_no;
int munger;

level_measurement_device_t *rin_power_meter;    /* Also known as Lrin */
level_measurement_device_t *rout_power_meter;
level_measurement_device_t *sin_power_meter;
level_measurement_device_t *sout_power_meter;   /* Also known as Lret (pre NLP value is known as Lres) */
level_measurement_device_t *sgen_power_meter;

#define RESULT_CHANNELS 7
SNDFILE *result_handle;
int16_t result_sound[SAMPLE_RATE*RESULT_CHANNELS];
int result_cur;

const char *test_name;
int quiet;
int use_gui;

float erl;

/* Dump estimated echo response */
static void dump_ec_state(echo_can_state_t *ctx)
{
    int i;
    FILE *f;

    if ((f = fopen("echo_tests_state.txt", "wt")) == NULL)
        return;
    for (i = 0;  i < TEST_EC_TAPS;  i++)
        fprintf(f, "%f\n", (float) ctx->fir_taps16[0][i]/(1 << 15));
    fclose(f);
}
/*- End of function --------------------------------------------------------*/

static inline void put_residue(int16_t amp)
{
    int outframes;

    residue_sound[residue_cur++] = amp;
    if (residue_cur >= SAMPLE_RATE)
    {
        outframes = sf_writef_short(residue_handle, residue_sound, residue_cur);
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
    sig->handle = sf_open_telephony_read(name, 1);
    sig->name = name;
    sig->max = sf_readf_short(sig->handle, sig->signal, SAMPLE_RATE);
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

static level_measurement_device_t *level_measurement_device_create(int type)
{
    level_measurement_device_t *dev;
    int i;

    dev = (level_measurement_device_t *) malloc(sizeof(level_measurement_device_t));
    dev->fir = (fir_float_state_t *) malloc(sizeof(fir_float_state_t));
    fir_float_create(dev->fir,
                     level_measurement_bp_coeffs,
                     sizeof(level_measurement_bp_coeffs)/sizeof(float));
    for (i = 0;  i < 35*8;  i++)
        dev->history[i] = 0.0f;
    dev->pos = 0;
    dev->factor = expf(-1.0f/((float) SAMPLE_RATE*0.035f));
    dev->power = 0;
    dev->type = type;
    return  dev;
}
/*- End of function --------------------------------------------------------*/

#if 0
static void level_measurement_device_reset(level_measurement_device_t *dev)
{
    int i;

    for (i = 0;  i < 35*8;  i++)
        dev->history[i] = 0.0f;
    dev->pos = 0;
    dev->power = 0;
    dev->peak = 0.0f;
}
/*- End of function --------------------------------------------------------*/

static int level_measurement_device_release(level_measurement_device_t *s)
{
    fir_float_free(s->fir);
    free(s->fir);
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

static float level_measurement_device_get_peak(level_measurement_device_t *dev)
{
    return dev->peak;
}
/*- End of function --------------------------------------------------------*/

static float level_measurement_device_reset_peak(level_measurement_device_t *dev)
{
    float power;

    power = dev->peak;
    dev->peak = -99.0f;
    return power;
}
/*- End of function --------------------------------------------------------*/

static float level_measurement_device(level_measurement_device_t *dev, int16_t amp)
{
    float signal;
    float power;

    /* Level measurement device(s), specified in G.168 section 6.4.1.2.1 and 6.4.1.2.2 */
    signal = fir_float(dev->fir, amp);
    signal *= signal;
    if (dev->type == 0)
    {
        /* Level measurement device, specified in G.168 section 6.4.1.2.1 -
           level measurement device. This version uses a single pole
           estimator.*/
        dev->power = dev->power*dev->factor + signal*(1.0f - dev->factor);
        signal = sqrtf(dev->power);
    }
    else
    {
        /* Level measurement device, specified in G.168 section 6.4.1.2.2 -
           level measurement device for peaks. This version uses a sliding
           window estimator. */
        dev->power += (signal - dev->history[dev->pos]);
        dev->history[dev->pos++] = signal;
        signal = sqrtf(dev->power/(35.8f*8.0f));
    }
    if (signal <= 0.0f)
        return -99.0f;
    power = DBM0_MAX_POWER + 20.0f*log10f(signal/32767.0f + 1.0e-10f);
    if (power > dev->peak)
        dev->peak = power;
    return power;
}
/*- End of function --------------------------------------------------------*/

static void level_measurements_create(int type)
{
    rin_power_meter = level_measurement_device_create(type);
    rout_power_meter = level_measurement_device_create(type);
    sin_power_meter = level_measurement_device_create(type);
    sout_power_meter = level_measurement_device_create(type);
    sgen_power_meter = level_measurement_device_create(type);
}
/*- End of function --------------------------------------------------------*/

static void level_measurements_update(int16_t rin, int16_t sin, int16_t rout, int16_t sout, int16_t sgen)
{
    level_measurement_device(rin_power_meter, rin);
    level_measurement_device(rout_power_meter, rout);
    level_measurement_device(sin_power_meter, sin);
    level_measurement_device(sout_power_meter, sout);
    level_measurement_device(sgen_power_meter, sgen);
}
/*- End of function --------------------------------------------------------*/

static void level_measurements_reset_peaks(void)
{
    level_measurement_device_reset_peak(rin_power_meter);
    level_measurement_device_reset_peak(rout_power_meter);
    level_measurement_device_reset_peak(sin_power_meter);
    level_measurement_device_reset_peak(sout_power_meter);
    level_measurement_device_reset_peak(sgen_power_meter);
}
/*- End of function --------------------------------------------------------*/

static void print_results(void)
{
    if (!quiet)
        printf("test  model  ERL   time     Max Rin  Max Rout Max Sgen Max Sin  Max Sout\n");
    printf("%-4s  %-1d      %-5.1f%6.2fs%9.2f%9.2f%9.2f%9.2f%9.2f\n", 
           test_name,
           chan_model.model_no,
           20.0f*log10f(-chan_model.erl + 1.0e-10f),
           0.0f, //test_clock,
           level_measurement_device_get_peak(rin_power_meter),
           level_measurement_device_get_peak(rout_power_meter),
           level_measurement_device_get_peak(sgen_power_meter),
           level_measurement_device_get_peak(sin_power_meter),
           level_measurement_device_get_peak(sout_power_meter));
}
/*- End of function --------------------------------------------------------*/

static int channel_model_create(channel_model_state_t *chan, int model, float erl, int codec)
{
    static const int32_t line_model_clear_coeffs[] =
    {
        32768
    };
    static const int32_t *line_models[] =
    {
        line_model_clear_coeffs,
        line_model_d2_coeffs,
        line_model_d3_coeffs,
        line_model_d4_coeffs,
        line_model_d5_coeffs,
        line_model_d6_coeffs,
        line_model_d7_coeffs,
        line_model_d8_coeffs,
        line_model_d9_coeffs
    };
    static const int line_model_sizes[] =
    {
        sizeof(line_model_clear_coeffs)/sizeof(line_model_clear_coeffs[0]),
        sizeof(line_model_d2_coeffs)/sizeof(line_model_d2_coeffs[0]),
        sizeof(line_model_d3_coeffs)/sizeof(line_model_d3_coeffs[0]),
        sizeof(line_model_d4_coeffs)/sizeof(line_model_d4_coeffs[0]),
        sizeof(line_model_d5_coeffs)/sizeof(line_model_d5_coeffs[0]),
        sizeof(line_model_d6_coeffs)/sizeof(line_model_d6_coeffs[0]),
        sizeof(line_model_d7_coeffs)/sizeof(line_model_d7_coeffs[0]),
        sizeof(line_model_d8_coeffs)/sizeof(line_model_d8_coeffs[0]),
        sizeof(line_model_d9_coeffs)/sizeof(line_model_d9_coeffs[0])
    };
    static const float ki[] = 
    {
        3.05e-5f,
        LINE_MODEL_D2_GAIN,
        LINE_MODEL_D3_GAIN,
        LINE_MODEL_D4_GAIN,
        LINE_MODEL_D5_GAIN,
        LINE_MODEL_D6_GAIN,
        LINE_MODEL_D7_GAIN,
        LINE_MODEL_D8_GAIN,
        LINE_MODEL_D9_GAIN
    };

    if (model < 0  ||  model >= (int) (sizeof(line_model_sizes)/sizeof(line_model_sizes[0])))
        return -1;
    fir32_create(&chan->impulse, line_models[model], line_model_sizes[model]);
    chan->gain = 32768.0f*powf(10.0f, erl/20.0f)*ki[model];
    chan->munging_codec = codec;
    chan->model_no = model;
    chan->erl = erl;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int16_t channel_model(channel_model_state_t *chan, int16_t rout, int16_t sgen)
{
    int16_t echo;
    int16_t sin;

    /* Channel modelling is merely simulating the effects of A-law or u-law distortion
       and using one of the echo models from G.168. Simulating the codec is very important,
       as this is usually the limiting factor in how much echo reduction is achieved. */

    /* The far end signal will have been through codec munging. */
    switch (chan->munging_codec)
    {
    case G711_ALAW:
        sgen = alaw_to_linear(linear_to_alaw(sgen));
        break;
    case G711_ULAW:
        sgen = ulaw_to_linear(linear_to_ulaw(sgen));
        break;
    }

    /* The local tx signal will usually have gone through codec munging before
       it reached the line's analogue area, where the echo occurs. */
    switch (chan->munging_codec)
    {
    case G711_ALAW:
        rout = alaw_to_linear(linear_to_alaw(rout));
        break;
    case G711_ULAW:
        rout = ulaw_to_linear(linear_to_ulaw(rout));
        break;
    }
    /* Now we need to model the echo. We only model a single analogue segment, as per
       the G.168 spec. However, there will generally be near end and far end analogue/echoey
       segments in the real world, unless an end is purely digital. */
    echo = fir32(&chan->impulse, rout*chan->gain);
    sin = saturate(echo + sgen);

    /* This mixed echo and far end signal will have been through codec munging
       when it came back into the digital network. */
    switch (chan->munging_codec)
    {
    case G711_ALAW:
        sin = alaw_to_linear(linear_to_alaw(sin));
        break;
    case G711_ULAW:
        sin = ulaw_to_linear(linear_to_ulaw(sin));
        break;
    }
    return sin;
}
/*- End of function --------------------------------------------------------*/

static void write_log_files(int16_t rout, int16_t sin)
{
#if 0
    fprintf(flevel, "%f\t%f\t%f\t%f\n", LRin, LSin, LSout, LSgen);
    fprintf(fdump, "%d %d %d", ctx->tx, ctx->rx, ctx->clean);
    fprintf(fdump,
            " %d %d %d %d %d %d %d %d %d %d\n",
            ctx->clean_nlp,
            ctx->Ltx, 
            ctx->Lrx,
            ctx->Lclean, 
            (ctx->nonupdate_dwell > 0),
            ctx->adapt,
            ctx->Lclean_bg,
            ctx->Pstates, 
            ctx->Lbgn_upper,
            ctx->Lbgn);
#endif
}
/*- End of function --------------------------------------------------------*/

static int16_t silence(void)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int16_t local_css_signal(void)
{
    return signal_amp(&local_css);
}
/*- End of function --------------------------------------------------------*/

static int16_t far_css_signal(void)
{
    return signal_amp(&far_css);
}
/*- End of function --------------------------------------------------------*/

static int16_t local_noise_signal(void)
{
    return awgn(&local_noise_source);
}
/*- End of function --------------------------------------------------------*/

static int16_t far_noise_signal(void)
{
    return awgn(&far_noise_source);
}
/*- End of function --------------------------------------------------------*/

#if 0
static int16_t local_hoth_noise_signal(void)
{
    static float hoth_noise = 0.0;

    hoth_noise = hoth_noise*0.625 + awgn(&local_noise_source)*0.375;
    return (int16_t) hoth_noise;
}
/*- End of function --------------------------------------------------------*/
#endif

static int16_t far_hoth_noise_signal(void)
{
    static float hoth_noise = 0.0;

    hoth_noise = hoth_noise*0.625 + awgn(&far_noise_source)*0.375;
    return (int16_t) hoth_noise;
}
/*- End of function --------------------------------------------------------*/

static void run_test(echo_can_state_t *ctx, int16_t (*tx_source)(void), int16_t (*rx_source)(void), int period)
{
    int i;
    int16_t rin;
    int16_t rout;
    int16_t sin;
    int16_t sout;
    int16_t sgen;
    int outframes;

    for (i = 0;  i < period*SAMPLE_RATE/1000;  i++)
    {
        rin = tx_source();
        sgen = rx_source();

        rout = echo_can_hpf_tx(ctx, rin);
        sin = channel_model(&chan_model, rout, sgen);
        sout = echo_can_update(ctx, rout, sin);

        level_measurements_update(rin, sin, rout, sout, sgen);
        //residue = 100.0f*pp1/pp2;
        //put_residue(residue);

        //put_residue(clean - rx);
#if defined(ENABLE_GUI)
        if (use_gui)
            echo_can_monitor_can_update(ctx->fir_taps16[0], TEST_EC_TAPS);
#endif
        result_sound[result_cur++] = rin;
        result_sound[result_cur++] = sgen;
        result_sound[result_cur++] = sin;
        result_sound[result_cur++] = rout;
        result_sound[result_cur++] = sout;
        result_sound[result_cur++] = sout - sgen;
        result_sound[result_cur++] = 0; // TODO: insert the EC's internal status here
        if (result_cur >= RESULT_CHANNELS*SAMPLE_RATE)
        {
            outframes = sf_writef_short(result_handle, result_sound, result_cur/RESULT_CHANNELS);
            if (outframes != result_cur/RESULT_CHANNELS)
            {
                fprintf(stderr, "    Error writing result sound\n");
                exit(2);
            }
            result_cur = 0;
        }
    }
#if defined(ENABLE_GUI)
    if (use_gui)
        echo_can_monitor_can_update(ctx->fir_taps16[0], TEST_EC_TAPS);
#endif
    if (result_cur >= 0)
    {
        outframes = sf_writef_short(result_handle, result_sound, result_cur/RESULT_CHANNELS);
        if (outframes != result_cur/RESULT_CHANNELS)
        {
            fprintf(stderr, "    Error writing result sound\n");
            exit(2);
        }
        result_cur = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static void print_test_title(const char *title)
{
    if (quiet == FALSE) 
        printf(title);
}
/*- End of function --------------------------------------------------------*/

static int perform_test_sanity(void)
{
    echo_can_state_t *ctx;
    int i;
    int16_t rx;
    int16_t tx;
    int16_t clean;
    int far_tx;
    //int16_t far_sound[SAMPLE_RATE];
    int16_t result_sound[64000];
    int result_cur;
    int outframes;
    //int local_cur;
    //int far_cur;
    //int32_t coeffs[200][128];
    //int coeff_index;

    print_test_title("Performing basic sanity test\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    //local_cur = 0;
    //far_cur = 0;
    result_cur = 0;

    echo_can_flush(ctx);
    /* Converge the canceller */
    signal_restart(&local_css, 0.0f);
    run_test(ctx, silence, silence, 200);
    run_test(ctx, local_css_signal, silence, 5000);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION | ECHO_CAN_USE_NLP | ECHO_CAN_USE_CNG);
    run_test(ctx, local_css_signal, silence, 5000);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    
    for (i = 0;  i < SAMPLE_RATE*10;  i++)
    {
        tx = local_css_signal();
#if 0
        if ((i/10000)%10 == 9)
        {
            /* Inject a burst of far sound */
            if (far_cur >= far_max)
            {
                far_max = sf_readf_short(farhandle, far_sound, SAMPLE_RATE);
                if (far_max < 0)
                {
                    fprintf(stderr, "    Error reading far sound\n");
                    exit(2);
                }
                if (far_max == 0)
                    break;
                far_cur = 0;
            }
            far_tx = far_sound[far_cur++];
        }
        else
        {
            far_tx = 0;
        }
#else
        //far_sound[0] = 0;
        far_tx = 0;
#endif
        rx = channel_model(&chan_model, tx, far_tx);
        //rx += awgn(&far_noise_source);
        //tx += awgn(&far_noise_source);
        clean = echo_can_update(ctx, tx, rx);

#if defined(XYZZY)
        if (i%SAMPLE_RATE == 0)
        {
            if (coeff_index < 200)
            {
                for (j = 0;  j < ctx->taps;  j++)
                    coeffs[coeff_index][j] = ctx->fir_taps32[j];
                coeff_index++;
            }
        }
#endif
        result_sound[result_cur++] = tx;
        result_sound[result_cur++] = rx;
        result_sound[result_cur++] = clean - far_tx;
        //result_sound[result_cur++] = ctx->tx_power[2];
        //result_sound[result_cur++] = ctx->tx_power[1];
        ////result_sound[result_cur++] = (ctx->tx_power[1] > 64)  ?  SAMPLE_RATE  :  -SAMPLE_RATE;
        //result_sound[result_cur++] = ctx->tap_set*SAMPLE_RATE;
        //result_sound[result_cur++] = (ctx->nonupdate_dwell > 0)  ?  SAMPLE_RATE  :  -SAMPLE_RATE;
        //result_sound[result_cur++] = ctx->latest_correction >> 8;
        //result_sound[result_cur++] = level_measurement_device(tx)/(16.0*65536.0);
        //result_sound[result_cur++] = level_measurement_device(tx)/4096.0;
        ////result_sound[result_cur++] = (ctx->tx_power[1] > ctx->rx_power[0])  ?  SAMPLE_RATE  :  -SAMPLE_RATE;
        //result_sound[result_cur++] = (ctx->tx_power[1] > ctx->rx_power[0])  ?  SAMPLE_RATE  :  -SAMPLE_RATE;
        //result_sound[result_cur++] = (ctx->narrowband_score)*5; //  ?  SAMPLE_RATE  :  -SAMPLE_RATE;
        //result_sound[result_cur++] = ctx->tap_rotate_counter*10;
        ////result_sound[result_cur++] = ctx->vad;
        
        put_residue(clean - far_tx);
        if (result_cur >= RESULT_CHANNELS*SAMPLE_RATE)
        {
            outframes = sf_writef_short(result_handle, result_sound, result_cur/RESULT_CHANNELS);
            if (outframes != result_cur/RESULT_CHANNELS)
            {
                fprintf(stderr, "    Error writing result sound\n");
                exit(2);
            }
            result_cur = 0;
        }
    }
    if (result_cur > 0)
    {
        outframes = sf_writef_short(result_handle, result_sound, result_cur/RESULT_CHANNELS);
        if (outframes != result_cur/RESULT_CHANNELS)
        {
            fprintf(stderr, "    Error writing result sound\n");
            exit(2);
        }
    }
#if defined(XYZZY)
    for (j = 0;  j < ctx->taps;  j++)
    {
        for (i = 0;  i < coeff_index;  i++)
            fprintf(stderr, "%d ", coeffs[i][j]);
        fprintf(stderr, "\n");
    }
#endif

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_2a(void)
{
    echo_can_state_t *ctx;

    /* Test 2 - Convergence and steady state residual and returned echo level test */
    /* Test 2A - Convergence and reconvergence test with NLP enabled */
    print_test_title("Performing test 2A - Convergence and reconvergence test with NLP enabled\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION | ECHO_CAN_USE_NLP);

    /* Test 2A (a) - Convergence test with NLP enabled */

    /* Converge the canceller. */
    run_test(ctx, silence, silence, 200);
    signal_restart(&local_css, 0.0f);
    run_test(ctx, local_css_signal, silence, 1000);
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 9000);
    print_results();
    if (level_measurement_device_get_peak(sout_power_meter) > -65.0f)
        printf("Test failed\n");
    else
        printf("Test passed\n");

    /* Test 2A (b) - Reconvergence test with NLP enabled */

    /* Make an abrupt change of channel characteristic, to another of the channel echo models. */
    if (channel_model_create(&chan_model, supp_line_model_no, erl, munger))
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    signal_restart(&local_css, 0.0f);
    run_test(ctx, local_css_signal, silence, 1000);
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 9000);
    print_results();
    if (level_measurement_device_get_peak(sout_power_meter) > -65.0f)
        printf("Test failed\n");
    else
        printf("Test passed\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_2b(void)
{
    echo_can_state_t *ctx;

    /* Test 2 - Convergence and steady state residual and returned echo level test */
    /* Test 2B - Convergence and reconverge with NLP disabled */
    print_test_title("Performing test 2B - Convergence and reconverge with NLP disabled\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    signal_restart(&local_css, 0.0f);
    
    /* Test 2B (a) - Convergence test with NLP disabled */

    /* Converge the canceller */
    run_test(ctx, silence, silence, 200);
    run_test(ctx, local_css_signal, silence, 1000);
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 9000);
    print_results();
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 170000);
    print_results();
    if (level_measurement_device_get_peak(sout_power_meter) > -65.0)
        printf("Test failed\n");
    else
        printf("Test passed\n");

    /* Test 2B (b) - Reconvergence test with NLP disabled */

    /* Make an abrupt change of channel characteristic, to another of the channel echo models. */
    if (channel_model_create(&chan_model, supp_line_model_no, erl, munger))
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    run_test(ctx, local_css_signal, silence, 1000);
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 9000);
    print_results();
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 170000);
    print_results();
    if (level_measurement_device_get_peak(sout_power_meter) > -65.0)
        printf("Test failed\n");
    else
        printf("Test passed\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_2ca(void)
{
    echo_can_state_t *ctx;

    /* Test 2 - Convergence and steady state residual and returned echo level test */
    /* Test 2C(a) - Convergence with background noise present */
    print_test_title("Performing test 2C(a) - Convergence with background noise present\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);
    awgn_init_dbm0(&far_noise_source, 7162534, -50.0f);
    
    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    
    /* Converge a canceller */
    signal_restart(&local_css, 0.0f);
    run_test(ctx, silence, silence, 200);
    
    awgn_init_dbm0(&far_noise_source, 7162534, -40.0f);
    run_test(ctx, local_css_signal, far_hoth_noise_signal, 5000);
    
    /* Now freeze adaption, and measure the echo. */
    echo_can_adaption_mode(ctx, 0);
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 5000);
    print_results();
    if (level_measurement_device_get_peak(sout_power_meter) > level_measurement_device_get_peak(sgen_power_meter))
        printf("Test failed\n");
    else
        printf("Test passed\n");

    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_3a(void)
{
    echo_can_state_t *ctx;

    /* Test 3 - Performance under double talk conditions */
    /* Test 3A - Double talk test with low cancelled-end levels */
    print_test_title("Performing test 3A - Double talk test with low cancelled-end levels\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    
    run_test(ctx, silence, silence, 200);
    signal_restart(&local_css, 0.0f);
    signal_restart(&far_css, -20.0f);

    /* Apply double talk, with a weak far end signal */
    run_test(ctx, local_css_signal, far_css_signal, 5000);
    
    /* Now freeze adaption. */
    echo_can_adaption_mode(ctx, 0);
    run_test(ctx, local_css_signal, silence, 500);
    
    /* Now measure the echo */
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 5000);
    print_results();
    if (level_measurement_device_get_peak(sout_power_meter) > level_measurement_device_get_peak(sgen_power_meter))
        printf("Test failed\n");
    else
        printf("Test passed\n");

    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_3ba(void)
{
    echo_can_state_t *ctx;

    /* Test 3 - Performance under double talk conditions */
    /* Test 3B(a) - Double talk stability test with high cancelled-end levels */
    print_test_title("Performing test 3B(b) - Double talk stability test with high cancelled-end levels\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);

    run_test(ctx, silence, silence, 200);
    signal_restart(&local_css, 0.0f);
    signal_restart(&far_css, 0.0f);
    
    /* Converge the canceller */
    run_test(ctx, local_css_signal, silence, 5000);
    
    /* Apply double talk */
    run_test(ctx, local_css_signal, far_css_signal, 5000);
    
    /* Now freeze adaption. */
    echo_can_adaption_mode(ctx, 0);
    run_test(ctx, local_css_signal, far_css_signal, 1000);
    
    /* Turn off the double talk. */
    run_test(ctx, local_css_signal, silence, 500);
    
    /* Now measure the echo */
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 5000);
    print_results();

    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_3bb(void)
{
    echo_can_state_t *ctx;

    /* Test 3 - Performance under double talk conditions */
    /* Test 3B(b) - Double talk stability test with low cancelled-end levels */
    print_test_title("Performing test 3B(b) - Double talk stability test with low cancelled-end levels\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);

    run_test(ctx, silence, silence, 200);
    signal_restart(&local_css, 0.0f);
    signal_restart(&far_css, -15.0f);
    
    /* Converge the canceller */
    run_test(ctx, local_css_signal, silence, 5000);
    
    /* Apply double talk */
    run_test(ctx, local_css_signal, far_css_signal, 5000);
    
    /* Now freeze adaption. */
    echo_can_adaption_mode(ctx, 0);
    run_test(ctx, local_css_signal, silence, 1000);
    
    /* Turn off the double talk. */
    run_test(ctx, local_css_signal, silence, 500);
    
    /* Now measure the echo */
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 5000);
    print_results();

    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_3c(void)
{
    echo_can_state_t *ctx;

    /* Test 3 - Performance under double talk conditions */
    /* Test 3C - Double talk test with simulated conversation */
    print_test_title("Performing test 3C - Double talk test with simulated conversation\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    run_test(ctx, silence, silence, 200);

    signal_restart(&local_css, 0.0f);
    signal_restart(&far_css, -15.0f);

    /* Apply double talk */
    run_test(ctx, local_css_signal, far_css_signal, 5600);

    /* Stop the far signal, and measure the echo. */
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 1400);
    print_results();

    /* Continue measuring the resulting echo */
    run_test(ctx, local_css_signal, silence, 5000);

    /* Reapply double talk */
    signal_restart(&far_css, 0.0f);
    run_test(ctx, local_css_signal, far_css_signal, 5600);

    /* Now the far signal only */
    run_test(ctx, silence, far_css_signal, 5600);

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_4(void)
{
    echo_can_state_t *ctx;

    /* Test 4 - Leak rate test */
    print_test_title("Performing test 4 - Leak rate test\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);

    run_test(ctx, silence, silence, 200);

    /* Converge the canceller */
    signal_restart(&local_css, 0.0f);
    run_test(ctx, local_css_signal, silence, 5000);

    /* Put 2 minutes of silence through it */
    run_test(ctx, silence, silence, 120000);

    /* Now freeze it, and check if it is still well adapted. */
    echo_can_adaption_mode(ctx, 0);
    level_measurements_reset_peaks();
    run_test(ctx, local_css_signal, silence, 5000);
    print_results();

    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_5(void)
{
    echo_can_state_t *ctx;
    
    /* Test 5 - Infinite return loss convergence test */
    print_test_title("Performing test 5 - Infinite return loss convergence test\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);

    /* Converge the canceller */
    signal_restart(&local_css, 0.0f);
    run_test(ctx, local_css_signal, silence, 5000);

    /* Now stop echoing, and see we don't do anything unpleasant as the
       echo path is open looped. */
    run_test(ctx, local_css_signal, silence, 5000);
    print_results();

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_6(void)
{
    echo_can_state_t *ctx;
    int i;
    int j;
    int k;
    int16_t rx;
    int16_t tx;
    int16_t clean;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_state;
    int16_t local_sound[40000];

    /* Test 6 - Non-divergence on narrow-band signals */
    print_test_title("Performing test 6 - Non-divergence on narrow-band signals\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);

    /* Converge the canceller */
    signal_restart(&local_css, 0.0f);
    run_test(ctx, local_css_signal, silence, 5000);

    /* Now put 5s bursts of a list of tones through the converged canceller, and check
       that nothing unpleasant happens. */
    for (k = 0;  tones_6_4_2_7[k][0];  k++)
    {
        tone_gen_descriptor_init(&tone_desc,
                                 tones_6_4_2_7[k][0],
                                 -11,
                                 tones_6_4_2_7[k][1],
                                 -9,
                                 1,
                                 0,
                                 0,
                                 0,
                                 1);
        tone_gen_init(&tone_state, &tone_desc);
        j = 0;
        for (i = 0;  i < 5;  i++)
        {
            tone_gen(&tone_state, local_sound, SAMPLE_RATE);
            for (j = 0;  j < SAMPLE_RATE;  j++)
            {
                tx = local_sound[j];
                rx = channel_model(&chan_model, tx, 0);
                clean = echo_can_update(ctx, tx, rx);
                put_residue(clean);
            }
#if defined(ENABLE_GUI)
            if (use_gui)
            {
                echo_can_monitor_can_update(ctx->fir_taps16[0], TEST_EC_TAPS);
                echo_can_monitor_update_display();
                usleep(100000);
            }
#endif
        }
    }
#if defined(ENABLE_GUI)
    if (use_gui)
        echo_can_monitor_can_update(ctx->fir_taps16[0], TEST_EC_TAPS);
#endif
    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_7(void)
{
    echo_can_state_t *ctx;
    int i;
    int j;
    int16_t rx;
    int16_t tx;
    int16_t clean;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_state;
    int16_t local_sound[40000];

    /* Test 7 - Stability */
    print_test_title("Performing test 7 - Stability\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    /* Put tones through an unconverged canceller, and check nothing unpleasant
       happens. */
    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION);
    tone_gen_descriptor_init(&tone_desc,
                             tones_6_4_2_7[0][0],
                             -11,
                             tones_6_4_2_7[0][1],
                             -9,
                             1,
                             0,
                             0,
                             0,
                             1);
    tone_gen_init(&tone_state, &tone_desc);
    j = 0;
    for (i = 0;  i < 120;  i++)
    {
        tone_gen(&tone_state, local_sound, SAMPLE_RATE);
        for (j = 0;  j < SAMPLE_RATE;  j++)
        {
            tx = local_sound[j];
            rx = channel_model(&chan_model, tx, 0);
            clean = echo_can_update(ctx, tx, rx);
            put_residue(clean);
        }
#if defined(ENABLE_GUI)
        if (use_gui)
        {
            echo_can_monitor_can_update(ctx->fir_taps16[0], TEST_EC_TAPS);
            echo_can_monitor_update_display();
            usleep(100000);
        }
#endif
    }
#if defined(ENABLE_GUI)
    if (use_gui)
        echo_can_monitor_can_update(ctx->fir_taps16[0], TEST_EC_TAPS);
#endif
    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_8(void)
{
    echo_can_state_t *ctx;

    /* Test 8 - Non-convergence on No 5, 6, and 7 in-band signalling */
    print_test_title("Performing test 8 - Non-convergence on No 5, 6, and 7 in-band signalling\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    fprintf(stderr, "Test 8 not yet implemented\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_9(void)
{
    echo_can_state_t *ctx;
    awgn_state_t local_noise_source;
    awgn_state_t far_noise_source;

    /* Test 9 - Comfort noise test */
    print_test_title("Performing test 9 - Comfort noise test\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);
    awgn_init_dbm0(&far_noise_source, 7162534, -50.0f);

    echo_can_flush(ctx);
    echo_can_adaption_mode(ctx,
                           ECHO_CAN_USE_ADAPTION 
                         | ECHO_CAN_USE_NLP 
                         | ECHO_CAN_USE_CNG);

    /* Test 9 part 1 - matching */
    /* Converge the canceller */
    signal_restart(&local_css, 0.0f);
    run_test(ctx, local_css_signal, silence, 5000);

    echo_can_adaption_mode(ctx, ECHO_CAN_USE_ADAPTION | ECHO_CAN_USE_NLP | ECHO_CAN_USE_CNG);
    awgn_init_dbm0(&far_noise_source, 7162534, -45.0f);
    run_test(ctx, silence, far_noise_signal, 30000);

    awgn_init_dbm0(&local_noise_source, 1234567, -10.0f);
    run_test(ctx, local_noise_signal, far_noise_signal, 2000);

    /* Test 9 part 2 - adjust down */
    awgn_init_dbm0(&far_noise_source, 7162534, -55.0f);
    run_test(ctx, silence, far_noise_signal, 10000);
    run_test(ctx, local_noise_signal, far_noise_signal, 2000);

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_10a(void)
{
    echo_can_state_t *ctx;

    /* Test 10 - FAX test during call establishment phase */
    /* Test 10A - Canceller operation on the calling station side */
    print_test_title("Performing test 10A - Canceller operation on the calling station side\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    fprintf(stderr, "Test 10A not yet implemented\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_10b(void)
{
    echo_can_state_t *ctx;

    /* Test 10 - FAX test during call establishment phase */
    /* Test 10B - Canceller operation on the called station side */
    print_test_title("Performing test 10B - Canceller operation on the called station side\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    fprintf(stderr, "Test 10B not yet implemented\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_10c(void)
{
    echo_can_state_t *ctx;

    /* Test 10 - FAX test during call establishment phase */
    /* Test 10C - Canceller operation on the calling station side during page
                  transmission and page breaks (for further study) */
    print_test_title("Performing test 10C - Canceller operation on the calling station side during page\n"
                     "transmission and page breaks (for further study)\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    fprintf(stderr, "Test 10C not yet implemented\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_11(void)
{
    echo_can_state_t *ctx;

    /* Test 11 - Tandem echo canceller test (for further study) */
    print_test_title("Performing test 11 - Tandem echo canceller test (for further study)\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    fprintf(stderr, "Test 11 not yet implemented\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_12(void)
{
    echo_can_state_t *ctx;

    /* Test 12 - Residual acoustic echo test (for further study) */
    print_test_title("Performing test 12 - Residual acoustic echo test (for further study)\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    fprintf(stderr, "Test 12 not yet implemented\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_13(void)
{
    echo_can_state_t *ctx;

    /* Test 13 - Performance with ITU-T low-bit rate coders in echo path
                 (Optional, under study) */
    print_test_title("Performing test 13 - Performance with ITU-T low-bit rate coders in echo path (Optional, under study)\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    fprintf(stderr, "Test 13 not yet implemented\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_14(void)
{
    echo_can_state_t *ctx;

    /* Test 14 - Performance with V-series low-speed data modems */
    print_test_title("Performing test 14 - Performance with V-series low-speed data modems\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    fprintf(stderr, "Test 14 not yet implemented\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_test_15(void)
{
    echo_can_state_t *ctx;

    /* Test 15 - PCM offset test (Optional) */
    print_test_title("Performing test 15 - PCM offset test (Optional)\n");
    ctx = echo_can_init(TEST_EC_TAPS, 0);

    fprintf(stderr, "Test 15 not yet implemented\n");

    echo_can_free(ctx);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int match_test_name(const char *name)
{
    const struct
    {
        const char *name;
        int (*func)(void);
    } tests[] =
    {
        {"sanity", perform_test_sanity},
        {"2a", perform_test_2a},
        {"2b", perform_test_2b},
        {"2ca", perform_test_2ca},
        {"3a", perform_test_3a},
        {"3ba", perform_test_3ba},
        {"3bb", perform_test_3bb},
        {"3c", perform_test_3c},
        {"4", perform_test_4},
        {"5", perform_test_5},
        {"6", perform_test_6},
        {"7", perform_test_7},
        {"8", perform_test_8},
        {"9", perform_test_9},
        {"10a", perform_test_10a},
        {"10b", perform_test_10b},
        {"10c", perform_test_10c},
        {"11", perform_test_11},
        {"12", perform_test_12},
        {"13", perform_test_13},
        {"14", perform_test_14},
        {"15", perform_test_15},
        {NULL, NULL}
    };
    int i;

    for (i = 0;  tests[i].name;  i++)
    {
        if (strcasecmp(name, tests[i].name) == 0)
        {
            test_name = name;
            tests[i].func();
            return 0;
        }
    }
    printf("Unknown test name '%s' specified. The known test names are ", name);
    for (i = 0;  tests[i].name;  i++)
    {
        printf("%s", tests[i].name);
        if (tests[i + 1].name)
            printf(", ");
    }
    printf("\n");
    return -1;
}
/*- End of function --------------------------------------------------------*/

static void simulate_ec(char *argv[], int two_channel_file, int mode)
{
    echo_can_state_t *ctx;
    SNDFILE *txfile;
    SNDFILE *rxfile;
    SNDFILE *rxtxfile;
    SNDFILE *ecfile;
    int ntx;
    int nrx;
    int nec;
    int16_t buf[2];
    int16_t rin;
    int16_t rout;
    int16_t sin;
    int16_t sout;

    mode |= ECHO_CAN_USE_ADAPTION;
    txfile = NULL;
    rxfile = NULL;
    rxtxfile = NULL;
    ecfile = NULL;
    if (two_channel_file)
    {
        txfile = sf_open_telephony_read(argv[0], 1);
        rxfile = sf_open_telephony_read(argv[1], 1);      
        ecfile = sf_open_telephony_write(argv[2], 1);
    }
    else
    {
        rxtxfile = sf_open_telephony_read(argv[0], 2);
        ecfile = sf_open_telephony_write(argv[1], 1);
    }

    ctx = echo_can_init(TEST_EC_TAPS, 0);
    echo_can_adaption_mode(ctx, mode);
    do
    {
        if (two_channel_file)
        {
            if ((ntx = sf_readf_short(rxtxfile, buf, 1)) < 0)
            {
                fprintf(stderr, "    Error reading tx sound file\n");
                exit(2);
            }
            rin = buf[0];
            sin = buf[1];
            nrx = ntx;
        }
        else
        {
            if ((ntx = sf_readf_short(txfile, &rin, 1)) < 0)
            {
                fprintf(stderr, "    Error reading tx sound file\n");
                exit(2);
            }
            if ((nrx = sf_readf_short(rxfile, &sin, 1)) < 0)
            {           
                fprintf(stderr, "    Error reading rx sound file\n");
                exit(2);
            }
        }
        rout = echo_can_hpf_tx(ctx, rin);
        sout = echo_can_update(ctx, rout, sin);

        if ((nec = sf_writef_short(ecfile, &sout, 1)) != 1)
        {
            fprintf(stderr, "    Error writing ec sound file\n");
            exit(2);
        }
        level_measurements_update(rin, sin, rout, sout, 0);
        write_log_files(rin, sin);
#if defined(ENABLE_GUI)
        if (use_gui)
            echo_can_monitor_can_update(ctx->fir_taps16[0], TEST_EC_TAPS);
#endif
    }
    while (ntx  &&  nrx);
    dump_ec_state(ctx);

    echo_can_free(ctx);

    if (two_channel_file)
    {
        sf_close_telephony(rxtxfile);
    }
    else
    {
        sf_close_telephony(txfile);
        sf_close_telephony(rxfile);
    }
    sf_close_telephony(ecfile);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int i;
    time_t now;
    int simulate;
    int cng;
    int hpf;
    int two_channel_file;
    int opt;
    int mode;

    /* Check which tests we should run */
    if (argc < 2)
        fprintf(stderr, "Usage: echo tests [-g] [-m <model number>] [-s] <list of test numbers>\n");
    line_model_no = 0;
    supp_line_model_no = 0;
    cng = FALSE;
    hpf = FALSE;
    use_gui = FALSE;
    simulate = FALSE;
    munger = -1;
    two_channel_file = FALSE;
    erl = -12.0f;

    while ((opt = getopt(argc, argv, "2ace:ghm:M:su")) != -1)
    {
        switch (opt)
        {
        case '2':
            two_channel_file = TRUE;
            break;
        case 'a':
            munger = G711_ALAW;
            break;
        case 'c':
            cng = TRUE;
            break;
        case 'e':
            /* Allow for ERL being entered as x or -x */
            erl = -fabs(atof(optarg));
            break;
        case 'g':
#if defined(ENABLE_GUI)
            use_gui = TRUE;
#else
            fprintf(stderr, "Graphical monitoring not available\n");
            exit(2);
#endif
            break;
        case 'h':
            hpf = TRUE;
            break;
        case 'm':
            line_model_no = atoi(optarg);
            break;
        case 'M':
            supp_line_model_no = atoi(optarg);
            break;
        case 's':
            simulate = TRUE;
            break;
        case 'u':
            munger = G711_ULAW;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    argc -= optind;
    argv += optind;

#if defined(ENABLE_GUI)
    if (use_gui)
        start_echo_can_monitor(TEST_EC_TAPS);
#endif
    if (simulate)
    {
        /* Process a pair of transmitted and received audio files, and produce
           an echo cancelled audio file. */
        if (argc < ((two_channel_file)  ?  2  :  3))
        {
            printf("not enough arguments for a simulation\n");
            exit(2);
        }
        mode = ECHO_CAN_USE_NLP;
        mode |= ((cng)  ?  ECHO_CAN_USE_CNG  :  ECHO_CAN_USE_CLIP);
        if (hpf)
        {
            mode |= ECHO_CAN_USE_TX_HPF;
            mode |= ECHO_CAN_USE_RX_HPF;
        }
        simulate_ec(argv, two_channel_file, mode);
    }
    else
    {
        /* Run some G.168 tests */
#if defined(ENABLE_GUI)
        if (use_gui)
            echo_can_monitor_line_model_update(chan_model.impulse.coeffs, chan_model.impulse.taps);
#endif
        signal_load(&local_css, "sound_c1_8k.wav");
        signal_load(&far_css, "sound_c3_8k.wav");

        if ((residue_handle = sf_open_telephony_write(RESIDUE_FILE_NAME, 1)) == NULL)
        {
            fprintf(stderr, "    Failed to open '%s'\n", RESIDUE_FILE_NAME);
            exit(2);
        }
        if (argc <= 0)
        {
            printf("No tests specified\n");
        }
        else
        {
            time(&now);
            if ((result_handle = sf_open_telephony_write("echo_tests_result.wav", RESULT_CHANNELS)) == NULL)
            {
                fprintf(stderr, "    Failed to open result file\n");
                exit(2);
            }
            result_cur = 0;
            level_measurements_create(0);
            for (i = 0;  i < argc;  i++)
            {
                if (channel_model_create(&chan_model, line_model_no, erl, munger))
                {
                    fprintf(stderr, "    Failed to create line model\n");
                    exit(2);
                }
                match_test_name(argv[i]);
            }
            if (sf_close_telephony(result_handle))
            {
                fprintf(stderr, "    Cannot close speech file '%s'\n", "result_sound.wav");
                exit(2);
            }
            printf("Run time %lds\n", time(NULL) - now);
        }
        signal_free(&local_css);
        signal_free(&far_css);
        if (sf_close_telephony(residue_handle))
        {
            fprintf(stderr, "    Cannot close speech file '%s'\n", RESIDUE_FILE_NAME);
            exit(2);
        }
    }
#if defined(ENABLE_GUI)
    if (use_gui)
        echo_can_monitor_wait_to_end();
#endif

    printf("Tests passed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
