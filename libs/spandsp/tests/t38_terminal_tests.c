/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_terminal_tests.c - Tests for the T.38 FoIP terminal module.
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
 * $Id: t38_terminal_tests.c,v 1.61 2008/08/16 14:59:50 steveu Exp $
 */

/*! \file */

/*! \page t38_terminal_tests_page T.38 termination tests
\section t38_terminal_tests_page_sec_1 What does it do?
These tests exercise the path

    T.38 termination <-> T.38 termination
*/

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)  &&  defined(HAVE_FL_FL_AUDIO_METER_H)
#define ENABLE_GUI
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#if !defined(__USE_MISC)
#define __USE_MISC
#endif
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <audiofile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "media_monitor.h"
#endif

#define SAMPLES_PER_CHUNK 160

#define INPUT_FILE_NAME         "../test-data/itu/fax/itutests.tif"
#define OUTPUT_FILE_NAME        "t38.tif"

t38_terminal_state_t t38_state_a;
t38_terminal_state_t t38_state_b;

g1050_state_t *path_a_to_b;
g1050_state_t *path_b_to_a;

double when = 0.0;

int done[2] = {FALSE, FALSE};
int succeeded[2] = {FALSE, FALSE};

int simulate_incrementing_repeats = FALSE;

static int phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    
    i = (int) (intptr_t) user_data;
    printf("%c: Phase B handler on channel %c - (0x%X) %s\n", i, i, result, t30_frametype(result));
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static int phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    const char *u;

    i = (int) (intptr_t) user_data;
    printf("%c: Phase D handler on channel %c - (0x%X) %s\n", i, i, result, t30_frametype(result));
    t30_get_transfer_statistics(s, &t);
    printf("%c: Phase D: bit rate %d\n", i, t.bit_rate);
    printf("%c: Phase D: ECM %s\n", i, (t.error_correcting_mode)  ?  "on"  :  "off");
    printf("%c: Phase D: pages transferred %d\n", i, t.pages_transferred);
    printf("%c: Phase D: image size %d x %d\n", i, t.width, t.length);
    printf("%c: Phase D: image resolution %d x %d\n", i, t.x_resolution, t.y_resolution);
    printf("%c: Phase D: bad rows %d\n", i, t.bad_rows);
    printf("%c: Phase D: longest bad row run %d\n", i, t.longest_bad_row_run);
    printf("%c: Phase D: coding method %s\n", i, t4_encoding_to_str(t.encoding));
    printf("%c: Phase D: image size %d bytes\n", i, t.image_size);
    if ((u = t30_get_tx_ident(s)))
        printf("%c: Phase D: local ident '%s'\n", i, u);
    if ((u = t30_get_rx_ident(s)))
        printf("%c: Phase D: remote ident '%s'\n", i, u);
    printf("%c: Phase D: bits per row - min %d, max %d\n", i, s->t4.min_row_bits, s->t4.max_row_bits);
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    const char *u;
    
    i = (int) (intptr_t) user_data;
    printf("%c: Phase E handler on channel %c - (%d) %s\n", i, i, result, t30_completion_code_to_str(result));
    t30_get_transfer_statistics(s, &t);
    printf("%c: Phase E: bit rate %d\n", i, t.bit_rate);
    printf("%c: Phase E: ECM %s\n", i, (t.error_correcting_mode)  ?  "on"  :  "off");
    printf("%c: Phase E: pages transferred %d\n", i, t.pages_transferred);
    printf("%c: Phase E: image size %d x %d\n", i, t.width, t.length);
    printf("%c: Phase E: image resolution %d x %d\n", i, t.x_resolution, t.y_resolution);
    printf("%c: Phase E: bad rows %d\n", i, t.bad_rows);
    printf("%c: Phase E: longest bad row run %d\n", i, t.longest_bad_row_run);
    printf("%c: Phase E: coding method %s\n", i, t4_encoding_to_str(t.encoding));
    printf("%c: Phase E: image size %d bytes\n", i, t.image_size);
    if ((u = t30_get_tx_ident(s)))
        printf("%c: Phase E: local ident '%s'\n", i, u);
    if ((u = t30_get_rx_ident(s)))
        printf("%c: Phase E: remote ident '%s'\n", i, u);
    succeeded[i - 'A'] = (result == T30_ERR_OK)  &&  (t.pages_transferred == 12);
    //done[i - 'A'] = TRUE;
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

            if (g1050_put(path_a_to_b, buf, len, subst_seq, when) < 0)
                printf("Lost packet %d\n", subst_seq);
            subst_seq = (subst_seq + 1) & 0xFFFF;
        }
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);

        for (i = 0;  i < count;  i++)
        {
            if (g1050_put(path_a_to_b, buf, len, s->tx_seq_no, when) < 0)
                printf("Lost packet %d\n", s->tx_seq_no);
        }
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
    int msg_len;
    uint8_t msg[1024];
    int t38_version;
    int seq_no;
    int use_ecm;
    int without_pacing;
    int use_tep;
    int model_no;
    int speed_pattern_no;
    const char *input_file_name;
    double tx_when;
    double rx_when;
    int use_gui;
    int supported_modems;
    int opt;
    t30_state_t *t30;

    t38_version = 1;
    without_pacing = FALSE;
    use_tep = FALSE;
    input_file_name = INPUT_FILE_NAME;
    use_ecm = FALSE;
    simulate_incrementing_repeats = FALSE;
    model_no = 0;
    speed_pattern_no = 1;
    use_gui = FALSE;
    supported_modems = T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17;
    while ((opt = getopt(argc, argv, "efgi:Im:M:ps:tv:")) != -1)
    {
        switch (opt)
        {
        case 'e':
            use_ecm = TRUE;
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
        case 'm':
            supported_modems = atoi(optarg);
            break;
        case 'M':
            model_no = optarg[0] - 'A' + 1;
            break;
        case 'p':
            without_pacing = TRUE;
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

    if (t38_terminal_init(&t38_state_a, TRUE, tx_packet_handler_a, &t38_state_b) == NULL)
    {
        fprintf(stderr, "Cannot start the T.38 channel\n");
        exit(2);
    }
    t30 = t38_terminal_get_t30_state(&t38_state_a);
    t38_set_t38_version(&t38_state_a.t38_fe.t38, t38_version);
    t38_terminal_set_config(&t38_state_a, without_pacing);
    t38_terminal_set_tep_mode(&t38_state_a, use_tep);
    span_log_set_level(&t38_state_a.logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(&t38_state_a.logging, "T.38-A");
    span_log_set_level(&t38_state_a.t38_fe.t38.logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(&t38_state_a.t38_fe.t38.logging, "T.38-A");
    span_log_set_level(&t30->logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(&t30->logging, "T.38-A");

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

    if (t38_terminal_init(&t38_state_b, FALSE, tx_packet_handler_b, &t38_state_a) == NULL)
    {
        fprintf(stderr, "Cannot start the T.38 channel\n");
        exit(2);
    }
    t30 = t38_terminal_get_t30_state(&t38_state_b);
    t38_set_t38_version(&t38_state_b.t38_fe.t38, t38_version);
    t38_terminal_set_config(&t38_state_b, without_pacing);
    t38_terminal_set_tep_mode(&t38_state_b, use_tep);
    span_log_set_level(&t38_state_b.logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(&t38_state_b.logging, "T.38-B");
    span_log_set_level(&t38_state_b.t38_fe.t38.logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(&t38_state_b.t38_fe.t38.logging, "T.38-B");
    span_log_set_level(&t30->logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(&t30->logging, "T.38-B");

    t30_set_supported_modems(t30, supported_modems);
    t30_set_tx_ident(t30, "22222222");
    t30_set_tx_nsf(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
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
        span_log_bump_samples(&t38_state_a.logging, SAMPLES_PER_CHUNK);
        span_log_bump_samples(&t38_state_a.t38_fe.t38.logging, SAMPLES_PER_CHUNK);
        span_log_bump_samples(&t38_state_a.t30.logging, SAMPLES_PER_CHUNK);
        span_log_bump_samples(&t38_state_b.logging, SAMPLES_PER_CHUNK);
        span_log_bump_samples(&t38_state_b.t38_fe.t38.logging, SAMPLES_PER_CHUNK);
        span_log_bump_samples(&t38_state_b.t30.logging, SAMPLES_PER_CHUNK);

        done[0] = t38_terminal_send_timeout(&t38_state_a, SAMPLES_PER_CHUNK);
        done[1] = t38_terminal_send_timeout(&t38_state_b, SAMPLES_PER_CHUNK);

        when += 0.02;

        while ((msg_len = g1050_get(path_a_to_b, msg, 1024, when, &seq_no, &tx_when, &rx_when)) >= 0)
        {
#if defined(ENABLE_GUI)
            if (use_gui)
                media_monitor_rx(seq_no, tx_when, rx_when);
#endif
            t38_core_rx_ifp_packet(&t38_state_b.t38_fe.t38, msg, msg_len, seq_no);
        }
        while ((msg_len = g1050_get(path_b_to_a, msg, 1024, when, &seq_no, &tx_when, &rx_when)) >= 0)
        {
#if defined(ENABLE_GUI)
            if (use_gui)
                media_monitor_rx(seq_no, tx_when, rx_when);
#endif
            t38_core_rx_ifp_packet(&t38_state_a.t38_fe.t38, msg, msg_len, seq_no);
        }
        if (done[0]  &&  done[1])
            break;
#if defined(ENABLE_GUI)
        if (use_gui)
            media_monitor_update_display();
#endif
    }
    t38_terminal_release(&t38_state_a);
    t38_terminal_release(&t38_state_b);
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
