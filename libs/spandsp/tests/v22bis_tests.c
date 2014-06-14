/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v22bis_tests.c
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

/*! \page v22bis_tests_page V.22bis modem tests
\section v22bis_tests_page_sec_1 What does it do?
These tests connect two V.22bis modems back to back, through a telephone line
model. BER testing is then used to evaluate performance under various line
conditions.

If the appropriate GUI environment exists, the tests are built such that a visual
display of modem status is maintained.

\section v22bis_tests_page_sec_2 How is it used?
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
#include <unistd.h>
#include <string.h>
#include <sndfile.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include "spandsp.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "modem_monitor.h"
#endif

#define BLOCK_LEN       160

#define OUT_FILE_NAME   "v22bis.wav"

char *decode_test_file = NULL;
bool use_gui = false;

int rx_bits = 0;

both_ways_line_model_state_t *model;

typedef struct
{
    v22bis_state_t *v22bis;
    bert_state_t bert_tx;
    bert_state_t bert_rx;
    bert_results_t latest_results;
#if defined(ENABLE_GUI)
    qam_monitor_t *qam_monitor;
#endif
    float smooth_power;
    int symbol_no;
} endpoint_t;

endpoint_t endpoint[2];

static void reporter(void *user_data, int reason, bert_results_t *results)
{
    endpoint_t *s;

    s = (endpoint_t *) user_data;
    switch (reason)
    {
    case BERT_REPORT_REGULAR:
        fprintf(stderr, "V.22bis rx %p BERT report regular - %d bits, %d bad bits, %d resyncs\n",
                user_data,
                results->total_bits,
                results->bad_bits,
                results->resyncs);
        memcpy(&s->latest_results, results, sizeof(s->latest_results));
        break;
    default:
        fprintf(stderr,
                "V.22bis rx %p BERT report %s\n",
                user_data,
                bert_event_to_str(reason));
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void v22bis_rx_status(void *user_data, int status)
{
    endpoint_t *s;
    int bit_rate;
    int i;
    int len;
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t *coeffs;
#else
    complexf_t *coeffs;
#endif

    /* Special conditions */
    s = (endpoint_t *) user_data;
    printf("V.22bis rx %p status is %s (%d)\n", user_data, signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_TRAINING_SUCCEEDED:
        bit_rate = v22bis_get_current_bit_rate(s->v22bis);
        printf("Negotiated bit rate: %d\n", bit_rate);
        if ((len = v22bis_rx_equalizer_state(s->v22bis, &coeffs)))
        {
            printf("Equalizer:\n");
            for (i = 0;  i < len;  i++)
#if defined(SPANDSP_USE_FIXED_POINT)
                printf("%3d (%15.5f, %15.5f)\n", i, coeffs[i].re/V22BIS_CONSTELLATION_SCALING_FACTOR, coeffs[i].im/V22BIS_CONSTELLATION_SCALING_FACTOR);
#else
                printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
#endif
        }
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void v22bis_putbit(void *user_data, int bit)
{
    endpoint_t *s;

    if (bit < 0)
    {
        v22bis_rx_status(user_data, bit);
        return;
    }

    s = (endpoint_t *) user_data;
    if (decode_test_file)
        printf("Rx bit %p-%d - %d\n", user_data, rx_bits++, bit);
    else
        bert_put_bit(&s->bert_rx, bit);
}
/*- End of function --------------------------------------------------------*/

static int v22bis_getbit(void *user_data)
{
    endpoint_t *s;
    int bit;

    s = (endpoint_t *) user_data;
    bit = bert_get_bit(&s->bert_tx);
    return bit;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_USE_FIXED_POINT)
static void qam_report(void *user_data, const complexi16_t *constel, const complexi16_t *target, int symbol)
#else
static void qam_report(void *user_data, const complexf_t *constel, const complexf_t *target, int symbol)
#endif
{
    int i;
    int len;
#if defined(SPANDSP_USE_FIXED_POINT)
    complexi16_t *coeffs;
#else
    complexf_t *coeffs;
#endif
    complexf_t constel_point;
    complexf_t target_point;
    float fpower;
    endpoint_t *s;

    s = (endpoint_t *) user_data;
    if (constel)
    {
        constel_point.re = constel->re/V22BIS_CONSTELLATION_SCALING_FACTOR;
        constel_point.im = constel->im/V22BIS_CONSTELLATION_SCALING_FACTOR;
        target_point.re = target->re/V22BIS_CONSTELLATION_SCALING_FACTOR;
        target_point.im = target->im/V22BIS_CONSTELLATION_SCALING_FACTOR;
#if defined(ENABLE_GUI)
        if (use_gui)
        {
            qam_monitor_update_constel(s->qam_monitor, &constel_point);
            qam_monitor_update_carrier_tracking(s->qam_monitor, v22bis_rx_carrier_frequency(s->v22bis));
            qam_monitor_update_symbol_tracking(s->qam_monitor, v22bis_rx_symbol_timing_correction(s->v22bis));
        }
#endif
        fpower = (constel->re - target->re)*(constel->re - target->re)
               + (constel->im - target->im)*(constel->im - target->im);
        s->smooth_power = 0.95f*s->smooth_power + 0.05f*fpower;

        printf("%8d [%8.4f, %8.4f] [%8.4f, %8.4f] %2x %8.4f %8.4f %8.4f\n",
               s->symbol_no,
               constel_point.re,
               constel_point.im,
               target_point.re,
               target_point.im,
               symbol,
               fpower,
               s->smooth_power,
               v22bis_rx_signal_power(s->v22bis));
        s->symbol_no++;
    }
    else
    {
        printf("Gardner step %d\n", symbol);
        if ((len = v22bis_rx_equalizer_state(s->v22bis, &coeffs)))
        {
            printf("Equalizer A:\n");
            for (i = 0;  i < len;  i++)
#if defined(SPANDSP_USE_FIXED_POINT)
                printf("%3d (%15.5f, %15.5f)\n", i, coeffs[i].re/V22BIS_CONSTELLATION_SCALING_FACTOR, coeffs[i].im/V22BIS_CONSTELLATION_SCALING_FACTOR);
#else
                printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
#endif
#if defined(ENABLE_GUI)
            if (use_gui)
            {
#if defined(SPANDSP_USE_FIXED_POINT)
                qam_monitor_update_int_equalizer(s->qam_monitor, coeffs, len);
#else
                qam_monitor_update_equalizer(s->qam_monitor, coeffs, len);
#endif
            }
#endif
        }
    }
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int16_t amp[2][BLOCK_LEN];
    int16_t model_amp[2][BLOCK_LEN];
    int16_t out_amp[2*BLOCK_LEN];
    SNDFILE *inhandle;
    SNDFILE *outhandle;
    int outframes;
    int samples;
    int samples2;
    int i;
    int j;
    int test_bps;
    int line_model_no;
    int bits_per_test;
    int noise_level;
    int signal_level;
    int channel_codec;
    int rbs_pattern;
    int guard_tone_option;
    int opt;
    bool log_audio;

    channel_codec = MUNGE_CODEC_NONE;
    rbs_pattern = 0;
    test_bps = 2400;
    line_model_no = 0;
    decode_test_file = NULL;
    noise_level = -70;
    signal_level = -13;
    bits_per_test = 50000;
    guard_tone_option = V22BIS_GUARD_TONE_1800HZ;
    log_audio = false;
    while ((opt = getopt(argc, argv, "b:B:c:d:gG:lm:n:r:s:")) != -1)
    {
        switch (opt)
        {
        case 'b':
            test_bps = atoi(optarg);
            if (test_bps != 2400  &&  test_bps != 1200)
            {
                fprintf(stderr, "Invalid bit rate specified\n");
                exit(2);
            }
            break;
        case 'B':
            bits_per_test = atoi(optarg);
            break;
        case 'c':
            channel_codec = atoi(optarg);
            break;
        case 'd':
            decode_test_file = optarg;
            break;
        case 'g':
#if defined(ENABLE_GUI)
            use_gui = true;
#else
            fprintf(stderr, "Graphical monitoring not available\n");
            exit(2);
#endif
            break;
        case 'G':
            guard_tone_option = atoi(optarg);
            break;
        case 'l':
            log_audio = true;
            break;
        case 'm':
            line_model_no = atoi(optarg);
            break;
        case 'n':
            noise_level = atoi(optarg);
            break;
        case 'r':
            rbs_pattern = atoi(optarg);
            break;
        case 's':
            signal_level = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    inhandle = NULL;
    if (decode_test_file)
    {
        /* We will decode the audio from a file. */
        if ((inhandle = sf_open_telephony_read(decode_test_file, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open audio file '%s'\n", decode_test_file);
            exit(2);
        }
    }

    outhandle = NULL;
    if (log_audio)
    {
        if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
    }
    memset(endpoint, 0, sizeof(endpoint));

    for (i = 0;  i < 2;  i++)
    {
        endpoint[i].v22bis = v22bis_init(NULL, test_bps, guard_tone_option, (i == 0), v22bis_getbit, &endpoint[i], v22bis_putbit, &endpoint[i]);
        v22bis_tx_power(endpoint[i].v22bis, signal_level);
        /* Move the carrier off a bit */
        endpoint[i].v22bis->tx.carrier_phase_rate = dds_phase_ratef((i == 0)  ?  1207.0f  :  2407.0f);
        v22bis_rx_set_qam_report_handler(endpoint[i].v22bis, qam_report, (void *) &endpoint[i]);
        span_log_set_level(&endpoint[i].v22bis->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        span_log_set_tag(&endpoint[i].v22bis->logging, (i == 0)  ?  "caller"  :  "answerer");
        endpoint[i].smooth_power = 0.0f;
        endpoint[i].symbol_no = 0;
        bert_init(&endpoint[i].bert_tx, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
        bert_init(&endpoint[i].bert_rx, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
        bert_set_report(&endpoint[i].bert_rx, 10000, reporter, &endpoint[i]);
    }

#if defined(ENABLE_GUI)
    if (use_gui)
    {
        endpoint[0].qam_monitor = qam_monitor_init(6.0f, V22BIS_CONSTELLATION_SCALING_FACTOR, "Calling modem");
        endpoint[1].qam_monitor = qam_monitor_init(6.0f, V22BIS_CONSTELLATION_SCALING_FACTOR, "Answering modem");
    }
#endif
    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    samples = 0;
    for (;;)
    {
        for (i = 0;  i < 2;  i++)
        {
            samples = v22bis_tx(endpoint[i].v22bis, amp[i], BLOCK_LEN);
#if defined(ENABLE_GUI)
            if (use_gui)
                qam_monitor_update_audio_level(endpoint[i].qam_monitor, amp[i], samples);
#endif
            if (samples == 0)
            {
                /* Note that we might get a few bad bits as the carrier shuts down. */
                bert_result(&endpoint[i].bert_rx, &endpoint[i].latest_results);

                bert_init(&endpoint[i].bert_tx, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
                bert_init(&endpoint[i].bert_rx, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
                bert_set_report(&endpoint[i].bert_rx, 10000, reporter, &endpoint[i]);

                printf("Restarting on zero output\n");
                v22bis_restart(endpoint[i].v22bis, test_bps);
            }
        }

#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        if (decode_test_file)
        {
            samples2 = sf_readf_short(inhandle, model_amp[0], samples);
            if (samples2 != samples)
                break;
        }
        for (i = 0;  i < 2;  i++)
        {
            span_log_bump_samples(&endpoint[i].v22bis->logging, samples);
            v22bis_rx(endpoint[i ^ 1].v22bis, model_amp[i], samples);
            for (j = 0;  j < samples;  j++)
                out_amp[2*j + i] = model_amp[i][j];
            for (  ;  j < BLOCK_LEN;  j++)
                out_amp[2*j + i] = 0;
        }

        if (log_audio)
        {
            outframes = sf_writef_short(outhandle, out_amp, BLOCK_LEN);
            if (outframes != BLOCK_LEN)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
    }
    both_ways_line_model_free(model);
#if defined(ENABLE_GUI)
    if (use_gui)
        qam_wait_to_end(endpoint[0].qam_monitor);
#endif
    if (decode_test_file)
    {
        if (sf_close_telephony(inhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", decode_test_file);
            exit(2);
        }
    }
    if (log_audio)
    {
        if (sf_close_telephony(outhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
