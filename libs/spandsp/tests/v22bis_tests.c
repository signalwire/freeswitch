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
 *
 * $Id: v22bis_tests.c,v 1.51 2008/08/16 14:59:50 steveu Exp $
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
#include <audiofile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "modem_monitor.h"
#endif

#define BLOCK_LEN       160

#define IN_FILE_NAME    "v22bis_samp.wav"
#define OUT_FILE_NAME   "v22bis.wav"

int in_bit = 0;
int out_bit = 0;

int in_bit_no = 0;
int out_bit_no = 0;

uint8_t tx_buf[1000];
int rx_ptr = 0;
int tx_ptr = 0;

int rx_bits = 0;
int rx_bad_bits = 0;

int use_gui = FALSE;

both_ways_line_model_state_t *model;

v22bis_state_t caller;
v22bis_state_t answerer;

struct qam_report_control_s
{
    v22bis_state_t *s;
#if defined(ENABLE_GUI)
    qam_monitor_t *qam_monitor;
#endif
    float smooth_power;
    int symbol_no;
};

struct qam_report_control_s qam_caller;
struct qam_report_control_s qam_answerer;

static void v22bis_putbit(void *user_data, int bit)
{
    v22bis_state_t *s;
    int i;
    int len;
    complexf_t *coeffs;
    
    s = (v22bis_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_TRAINING_FAILED:
            printf("Training failed\n");
            break;
        case PUTBIT_TRAINING_IN_PROGRESS:
            printf("Training in progress\n");
            break;
        case PUTBIT_TRAINING_SUCCEEDED:
            printf("Training succeeded\n");
            len = v22bis_equalizer_state(s, &coeffs);
            printf("Equalizer:\n");
            for (i = 0;  i < len;  i++)
                printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
            break;
        case PUTBIT_CARRIER_UP:
            printf("Carrier up\n");
            break;
        case PUTBIT_CARRIER_DOWN:
            printf("Carrier down\n");
            break;
        default:
            printf("Eh! - %d\n", bit);
            break;
        }
        return;
    }

    if (bit != tx_buf[rx_ptr])
    {
        printf("Rx bit %d - %d\n", rx_bits, bit);
        rx_bad_bits++;
    }
    rx_ptr++;
    if (rx_ptr > 1000)
        rx_ptr = 0;
    rx_bits++;
    if ((rx_bits % 100000) == 0)
    {
        printf("%d bits received, %d bad bits\r", rx_bits, rx_bad_bits);
        fflush(stdout);
    }
}
/*- End of function --------------------------------------------------------*/

static int v22bis_getbit(void *user_data)
{
    int bit;
    static int tx_bits = 0;

    bit = rand() & 1;
    tx_buf[tx_ptr++] = bit;
    if (tx_ptr > 1000)
        tx_ptr = 0;
    //printf("Tx bit %d\n", bit);
    if (++tx_bits > 100000)
    {
        tx_bits = 0;
        bit = 2;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void qam_report(void *user_data, const complexf_t *constel, const complexf_t *target, int symbol)
{
    int i;
    int len;
    complexf_t *coeffs;
    float fpower;
    struct qam_report_control_s *s;

    s = (struct qam_report_control_s *) user_data;
    if (constel)
    {
#if defined(ENABLE_GUI)
        if (use_gui)
        {
            qam_monitor_update_constel(s->qam_monitor, constel);
            qam_monitor_update_carrier_tracking(s->qam_monitor, v22bis_rx_carrier_frequency(s->s));
            qam_monitor_update_symbol_tracking(s->qam_monitor, v22bis_symbol_timing_correction(s->s));
        }
#endif
        fpower = (constel->re - target->re)*(constel->re - target->re)
               + (constel->im - target->im)*(constel->im - target->im);
        s->smooth_power = 0.95f*s->smooth_power + 0.05f*fpower;
        printf("%8d [%8.4f, %8.4f] [%8.4f, %8.4f] %2x %8.4f %8.4f\n",
               s->symbol_no,
               constel->re,
               constel->im,
               target->re,
               target->im,
               symbol,
               fpower,
               s->smooth_power);
        s->symbol_no++;
    }
    else
    {
        printf("Gardner step %d\n", symbol);
        len = v22bis_equalizer_state(s->s, &coeffs);
        printf("Equalizer A:\n");
        for (i = 0;  i < len;  i++)
            printf("%3d (%15.5f, %15.5f) -> %15.5f\n", i, coeffs[i].re, coeffs[i].im, powerf(&coeffs[i]));
#if defined(ENABLE_GUI)
        if (use_gui)
            qam_monitor_update_equalizer(s->qam_monitor, coeffs, len);
#endif
    }
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int16_t caller_amp[BLOCK_LEN];
    int16_t answerer_amp[BLOCK_LEN];
    int16_t caller_model_amp[BLOCK_LEN];
    int16_t answerer_model_amp[BLOCK_LEN];
    int16_t out_amp[2*BLOCK_LEN];
    AFfilehandle outhandle;
    int outframes;
    int samples;
    int i;
    int test_bps;
    int line_model_no;
    int bits_per_test;
    int noise_level;
    int signal_level;
    int log_audio;
    int channel_codec;
    int opt;
    
    channel_codec = MUNGE_CODEC_NONE;
    test_bps = 2400;
    line_model_no = 0;
    noise_level = -70;
    signal_level = -13;
    bits_per_test = 50000;
    log_audio = FALSE;
    while ((opt = getopt(argc, argv, "b:c:glm:n:s:")) != -1)
    {
        switch (opt)
        {
        case 'b':
            bits_per_test = atoi(optarg);
            break;
        case 'c':
            channel_codec = atoi(optarg);
            break;
        case 'g':
#if defined(ENABLE_GUI)
            use_gui = TRUE;
#else
            fprintf(stderr, "Graphical monitoring not available\n");
            exit(2);
#endif
            break;
        case 'l':
            log_audio = TRUE;
            break;
        case 'm':
            line_model_no = atoi(optarg);
            break;
        case 'n':
            noise_level = atoi(optarg);
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
    argc -= optind;
    argv += optind;
    if (argc > 0)
    {
        if (strcmp(argv[0], "2400") == 0)
            test_bps = 2400;
        else if (strcmp(argv[0], "1200") == 0)
            test_bps = 1200;
        else
        {
            fprintf(stderr, "Invalid bit rate\n");
            exit(2);
        }
    }
    outhandle = AF_NULL_FILEHANDLE;
    if (log_audio)
    {
        if ((outhandle = afOpenFile_telephony_write(OUT_FILE_NAME, 2)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
    }
    v22bis_init(&caller, test_bps, 2, TRUE, v22bis_getbit, v22bis_putbit, &caller);
    v22bis_tx_power(&caller, signal_level);
    /* Move the carrier off a bit */
    caller.tx.carrier_phase_rate = dds_phase_ratef(1207.0f);
    v22bis_init(&answerer, test_bps, 2, FALSE, v22bis_getbit, v22bis_putbit, &answerer);
    v22bis_tx_power(&answerer, signal_level);
    answerer.tx.carrier_phase_rate = dds_phase_ratef(2407.0f);
    v22bis_set_qam_report_handler(&caller, qam_report, (void *) &qam_caller);
    v22bis_set_qam_report_handler(&answerer, qam_report, (void *) &qam_answerer);
    span_log_set_level(&caller.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(&caller.logging, "caller");
    span_log_set_level(&answerer.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(&answerer.logging, "answerer");

    qam_caller.s = &caller;
    qam_caller.smooth_power = 0.0f;
    qam_caller.symbol_no = 0;

    qam_answerer.s = &answerer;
    qam_answerer.smooth_power = 0.0f;
    qam_answerer.symbol_no = 0;

#if defined(ENABLE_GUI)
    if (use_gui)
    {
        qam_caller.qam_monitor = qam_monitor_init(6.0f, "Calling modem");
        qam_answerer.qam_monitor = qam_monitor_init(6.0f, "Answering modem");
    }
#endif

    if ((model = both_ways_line_model_init(line_model_no, (float) noise_level, line_model_no, (float) noise_level, channel_codec, 0)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    for (;;)
    {
        samples = v22bis_tx(&caller, caller_amp, BLOCK_LEN);
#if defined(ENABLE_GUI)
        if (use_gui)
            qam_monitor_update_audio_level(qam_caller.qam_monitor, caller_amp, samples);
#endif
        if (samples == 0)
        {
            printf("Restarting on zero output\n");
            v22bis_restart(&caller, test_bps);
            rx_ptr = 0;
            tx_ptr = 0;
        }

        samples = v22bis_tx(&answerer, answerer_amp, BLOCK_LEN);
#if defined(ENABLE_GUI)
        if (use_gui)
            qam_monitor_update_audio_level(qam_answerer.qam_monitor, answerer_amp, samples);
#endif
        if (samples == 0)
        {
            printf("Restarting on zero output\n");
            v22bis_restart(&answerer, test_bps);
            rx_ptr = 0;
            tx_ptr = 0;
        }

        both_ways_line_model(model, 
                             caller_model_amp,
                             caller_amp,
                             answerer_model_amp,
                             answerer_amp,
                             samples);

        v22bis_rx(&answerer, caller_model_amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = caller_model_amp[i];
        for (  ;  i < BLOCK_LEN;  i++)
            out_amp[2*i] = 0;

        v22bis_rx(&caller, answerer_model_amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = answerer_model_amp[i];
        for (  ;  i < BLOCK_LEN;  i++)
            out_amp[2*i + 1] = 0;

        if (log_audio)
        {
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      out_amp,
                                      BLOCK_LEN);
            if (outframes != BLOCK_LEN)
            {
                fprintf(stderr, "    Error writing wave file\n");
                exit(2);
            }
        }
    }
    if (log_audio)
    {
        if (afCloseFile(outhandle) != 0)
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
