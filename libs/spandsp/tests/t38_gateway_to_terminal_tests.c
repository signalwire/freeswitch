/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_gateway_to_terminal_tests.c - Joint tests for the T.38 FoIP terminal and gateway modules.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005, 2006 Steve Underwood
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
 * $Id: t38_gateway_to_terminal_tests.c,v 1.66.4.1 2009/12/19 09:47:57 steveu Exp $
 */

/*! \file */

/*! \page t38_gateway_to_terminal_tests_page T.38 mixed gateway and termination tests
\section t38_gateway_to_terminal_tests_page_sec_1 What does it do?
These tests exercise the path

    FAX machine -> T.38 gateway -> T.38 termination
*/

/* Enable the following definition to enable direct probing into the FAX structures */
//#define WITH_SPANDSP_INTERNALS

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)  &&  defined(HAVE_FL_FL_AUDIO_METER_H)
#define ENABLE_GUI
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sndfile.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "media_monitor.h"
#endif
#include "fax_utils.h"

#define SAMPLES_PER_CHUNK       160

#define INPUT_FILE_NAME         "../test-data/itu/fax/itutests.tif"
#define OUTPUT_FILE_NAME        "t38.tif"
#define OUTPUT_FILE_NAME_WAVE   "t38_gateway_to_terminal.wav"

fax_state_t *fax_state_a;
t38_gateway_state_t *t38_state_a;
t38_terminal_state_t *t38_state_b;

g1050_state_t *path_a_to_b;
g1050_state_t *path_b_to_a;

double when = 0.0;

int done[2] = {FALSE, FALSE};
int succeeded[2] = {FALSE, FALSE};

int simulate_incrementing_repeats = FALSE;

static int phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    char tag[20];

    i = (int) (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase B", i);
    printf("%c: Phase B handler on channel %c - (0x%X) %s\n", i, i, result, t30_frametype(result));
    log_rx_parameters(s, tag);
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static int phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    char tag[20];

    i = (int) (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase D", i);
    printf("%c: Phase D handler on channel %c - (0x%X) %s\n", i, i, result, t30_frametype(result));
    log_transfer_statistics(s, tag);
    log_tx_parameters(s, tag);
    log_rx_parameters(s, tag);
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    char tag[20];
    
    i = (int) (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase E", i);
    printf("%c: Phase E handler on channel %c - (%d) %s\n", i, i, result, t30_completion_code_to_str(result));
    log_transfer_statistics(s, tag);
    log_tx_parameters(s, tag);
    log_rx_parameters(s, tag);
    t30_get_transfer_statistics(s, &t);
    succeeded[i - 'A'] = (result == T30_ERR_OK)  &&  (t.pages_tx == 12  ||  t.pages_rx == 12);
    done[i - 'A'] = TRUE;
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler_a(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    t38_terminal_state_t *t;
    int i;
    static int subst_seq = 0;

    /* This routine queues messages between two instances of T.38 processing */
    t = (t38_terminal_state_t *) user_data;
    if (simulate_incrementing_repeats)
    {
        for (i = 0;  i < count;  i++)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d\n", subst_seq, len);

            g1050_put(path_a_to_b, buf, len, subst_seq, when);
            subst_seq = (subst_seq + 1) & 0xFFFF;
        }
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);

        for (i = 0;  i < count;  i++)
            g1050_put(path_a_to_b, buf, len, s->tx_seq_no, when);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler_b(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    t38_terminal_state_t *t;
    int i;
    static int subst_seq = 0;

    /* This routine queues messages between two instances of T.38 processing */
    t = (t38_terminal_state_t *) user_data;
    if (simulate_incrementing_repeats)
    {
        for (i = 0;  i < count;  i++)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d\n", subst_seq, len);

            g1050_put(path_b_to_a, buf, len, subst_seq, when);
            subst_seq = (subst_seq + 1) & 0xFFFF;
        }
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);

        for (i = 0;  i < count;  i++)
            g1050_put(path_b_to_a, buf, len, s->tx_seq_no, when);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int16_t t38_amp_a[SAMPLES_PER_CHUNK];
    int16_t t30_amp_a[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int t38_len_a;
    int t30_len_a;
    int msg_len;
    uint8_t msg[1024];
    int log_audio;
    int outframes;
    int t38_version;
    int use_ecm;
    int use_tep;
    int feedback_audio;
    int use_transmit_on_idle;
    SNDFILE *wave_handle;
    const char *input_file_name;
    int i;
    int seq_no;
    int model_no;
    int speed_pattern_no;
    double tx_when;
    double rx_when;
    int supported_modems;
    int use_gui;
    int opt;
    t30_state_t *t30;
    t38_core_state_t *t38_core;
    logging_state_t *logging;

    log_audio = FALSE;
    t38_version = 1;
    use_ecm = FALSE;
    input_file_name = INPUT_FILE_NAME;
    simulate_incrementing_repeats = FALSE;
    model_no = 0;
    speed_pattern_no = 1;
    use_gui = FALSE;
    use_tep = FALSE;
    feedback_audio = FALSE;
    use_transmit_on_idle = TRUE;
    supported_modems = T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17;
    while ((opt = getopt(argc, argv, "efgi:Ilm:M:s:tv:")) != -1)
    {
        switch (opt)
        {
        case 'e':
            use_ecm = TRUE;
            break;
        case 'f':
            feedback_audio = TRUE;
            break;
        case 'g':
#if defined(ENABLE_GUI)
            use_gui = TRUE;
#else
            fprintf(stderr, "Graphical monitoring not available\n");
            exit(2);
#endif
            break;
        case 'i':
            input_file_name = optarg;
            break;
        case 'I':
            simulate_incrementing_repeats = TRUE;
            break;
        case 'l':
            log_audio = TRUE;
            break;
        case 'm':
            supported_modems = atoi(optarg);
            break;
        case 'M':
            model_no = optarg[0] - 'A' + 1;
            break;
        case 's':
            speed_pattern_no = atoi(optarg);
            break;
        case 't':
            use_tep = TRUE;
            break;
        case 'v':
            t38_version = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    printf("Using T.38 version %d\n", t38_version);
    if (use_ecm)
        printf("Using ECM\n");

    wave_handle = NULL;
    if (log_audio)
    {
        if ((wave_handle = sf_open_telephony_write(OUTPUT_FILE_NAME_WAVE, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_FILE_NAME_WAVE);
            exit(2);
        }
    }

    srand48(0x1234567);
    if ((path_a_to_b = g1050_init(model_no, speed_pattern_no, 100, 33)) == NULL)
    {
        fprintf(stderr, "Failed to start IP network path model\n");
        exit(2);
    }
    if ((path_b_to_a = g1050_init(model_no, speed_pattern_no, 100, 33)) == NULL)
    {
        fprintf(stderr, "Failed to start IP network path model\n");
        exit(2);
    }

    if ((fax_state_a = fax_init(NULL, TRUE)) == NULL)
    {
        fprintf(stderr, "Cannot start FAX\n");
        exit(2);
    }
    t30 = fax_get_t30_state(fax_state_a);
    fax_set_transmit_on_idle(fax_state_a, use_transmit_on_idle);
    fax_set_tep_mode(fax_state_a, use_tep);
    t30_set_supported_modems(t30, supported_modems);
    t30_set_tx_ident(t30, "11111111");
    t30_set_tx_nsf(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
    t30_set_tx_file(t30, input_file_name, -1, -1);
    t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) 'A');
    t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) 'A');
    t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) 'A');
    t30_set_ecm_capability(t30, use_ecm);
    if (use_ecm)
        t30_set_supported_compressions(t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);

    logging = fax_get_logging_state(fax_state_a);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "FAX-A ");

    logging = t30_get_logging_state(t30);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "FAX-A ");

    memset(t30_amp_a, 0, sizeof(t30_amp_a));

    if ((t38_state_a = t38_gateway_init(NULL, tx_packet_handler_a, t38_state_b)) == NULL)
    {
        fprintf(stderr, "Cannot start the T.38 channel\n");
        exit(2);
    }
    t38_core = t38_gateway_get_t38_core_state(t38_state_a);
    t38_gateway_set_transmit_on_idle(t38_state_a, use_transmit_on_idle);
    t38_set_t38_version(t38_core, t38_version);
    t38_gateway_set_ecm_capability(t38_state_a, use_ecm);

    logging = t38_gateway_get_logging_state(t38_state_a);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.38-A");

    logging = t38_core_get_logging_state(t38_core);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.38-A");

    memset(t38_amp_a, 0, sizeof(t38_amp_a));

    if ((t38_state_b = t38_terminal_init(NULL, FALSE, tx_packet_handler_b, t38_state_a)) == NULL)
    {
        fprintf(stderr, "Cannot start the T.38 channel\n");
        exit(2);
    }
    t30 = t38_terminal_get_t30_state(t38_state_b);
    t38_core = t38_terminal_get_t38_core_state(t38_state_b);
    t38_set_t38_version(t38_core, t38_version);

    logging = t38_terminal_get_logging_state(t38_state_b);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.38-B");

    logging = t38_core_get_logging_state(t38_core);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.38-B");

    logging = t30_get_logging_state(t30);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.38-B");

    t30_set_supported_modems(t30, supported_modems);
    t30_set_tx_ident(t30, "22222222");
    t30_set_rx_file(t30, OUTPUT_FILE_NAME, -1);
    t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) 'B');
    t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) 'B');
    t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) 'B');
    t30_set_ecm_capability(t30, use_ecm);
    if (use_ecm)
        t30_set_supported_compressions(t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);

#if defined(ENABLE_GUI)
    if (use_gui)
        start_media_monitor();
#endif
    for (;;)
    {
        logging = fax_get_logging_state(fax_state_a);
        span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
        t30 = fax_get_t30_state(fax_state_a);
        logging = t30_get_logging_state(t30);
        span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
        logging = t38_gateway_get_logging_state(t38_state_a);
        span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
        t38_core = t38_gateway_get_t38_core_state(t38_state_a);
        logging = t38_core_get_logging_state(t38_core);
        span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
        logging = t38_terminal_get_logging_state(t38_state_b);
        span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
        t38_core = t38_terminal_get_t38_core_state(t38_state_b);
        logging = t38_core_get_logging_state(t38_core);
        span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
        t30 = t38_terminal_get_t30_state(t38_state_b);
        logging = t30_get_logging_state(t30);
        span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
        memset(out_amp, 0, sizeof(out_amp));

        t38_terminal_send_timeout(t38_state_b, SAMPLES_PER_CHUNK);

        t30_len_a = fax_tx(fax_state_a, t30_amp_a, SAMPLES_PER_CHUNK);
        if (!use_transmit_on_idle)
        {
            /* The receive side always expects a full block of samples, but the
               transmit side may not be sending any when it doesn't need to. We
               may need to pad with some silence. */
            if (t30_len_a < SAMPLES_PER_CHUNK)
            {
                memset(t30_amp_a + t30_len_a, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - t30_len_a));
                t30_len_a = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (i = 0;  i < t30_len_a;  i++)
                out_amp[2*i] = t30_amp_a[i];
        }
        if (feedback_audio)
        {
            for (i = 0;  i < t30_len_a;  i++)
                t30_amp_a[i] += t38_amp_a[i] >> 1;
        }
        if (t38_gateway_rx(t38_state_a, t30_amp_a, t30_len_a))
            break;
    
        t38_len_a = t38_gateway_tx(t38_state_a, t38_amp_a, SAMPLES_PER_CHUNK);
        if (!use_transmit_on_idle)
        {
            if (t38_len_a < SAMPLES_PER_CHUNK)
            {
                memset(t38_amp_a + t38_len_a, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - t38_len_a));
                t38_len_a = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (i = 0;  i < t38_len_a;  i++)
                out_amp[2*i + 1] = t38_amp_a[i];
        }
        if (fax_rx(fax_state_a, t38_amp_a, SAMPLES_PER_CHUNK))
            break;

        when += (float) SAMPLES_PER_CHUNK/(float) SAMPLE_RATE;

        while ((msg_len = g1050_get(path_a_to_b, msg, 1024, when, &seq_no, &tx_when, &rx_when)) >= 0)
        {
#if defined(ENABLE_GUI)
            if (use_gui)
                media_monitor_rx(seq_no, tx_when, rx_when);
#endif
            t38_core = t38_terminal_get_t38_core_state(t38_state_b);
            t38_core_rx_ifp_packet(t38_core, msg, msg_len, seq_no);
        }
        while ((msg_len = g1050_get(path_b_to_a, msg, 1024, when, &seq_no, &tx_when, &rx_when)) >= 0)
        {
#if defined(ENABLE_GUI)
            if (use_gui)
                media_monitor_rx(seq_no, tx_when, rx_when);
#endif
            t38_core = t38_gateway_get_t38_core_state(t38_state_a);
            t38_core_rx_ifp_packet(t38_core, msg, msg_len, seq_no);
        }
        if (log_audio)
        {
            outframes = sf_writef_short(wave_handle, out_amp, SAMPLES_PER_CHUNK);
            if (outframes != SAMPLES_PER_CHUNK)
                break;
        }

        if (done[0]  &&  done[1])
            break;
#if defined(ENABLE_GUI)
        if (use_gui)
            media_monitor_update_display();
#endif
    }
    fax_release(fax_state_a);
    t38_terminal_release(t38_state_b);
    if (log_audio)
    {
        if (sf_close(wave_handle) != 0)
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME_WAVE);
            exit(2);
        }
    }
    if (!succeeded[0]  ||  !succeeded[1])
    {
        printf("Tests failed\n");
        exit(2);
    }
    printf("Tests passed\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
