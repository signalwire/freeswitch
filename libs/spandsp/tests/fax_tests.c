/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_tests.c - Tests for the audio and T.38 FAX modules.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005, 2006, 2009, 2010 Steve Underwood
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

/*! \page fax_tests_page FAX tests
\section fax_tests_page_sec_1 What does it do?
These tests exercise the following FAX to FAX paths:

         +--Modems-+-----------TDM/RTP-----------+-Modems--+
         |          \                           /          |
         |           \                         /           |
T.30 <---+      T.38 gateway            T.38 gateway       +--->T.30
         |             \                     /             |
         |              \                   /              |
         +---T.38---+----+----UDPTL/RTP----+----+---T.38---+
                     \                         /
                      +----------TCP----------+

T.30<->Modems<-------------------------TDM/RTP------------------------->Modems<->T.30
T.30<->Modems<-TDM/RTP->T.38 gateway<-UDPTL/RTP->T.38 gateway<-TDM/RTP->Modems<->T.30
T.30<->Modems<-TDM/RTP->T.38 gateway<-UDPTL/RTP-------------------------->T.38<->T.30
T.30<->T.38<--------------------------UDPTL/RTP->T.38 gateway<-TDM/RTP->Modems<->T.30
T.30<->T.38<--------------------------UDPTL/RTP-------------------------->T.38<->T.30

*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)  &&  defined(HAVE_FL_FL_AUDIO_METER_H)
#define ENABLE_GUI
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sndfile.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#if defined(HAVE_LIBXML_XMLMEMORY_H)
#include <libxml/xmlmemory.h>
#endif
#if defined(HAVE_LIBXML_PARSER_H)
#include <libxml/parser.h>
#endif
#if defined(HAVE_LIBXML_XINCLUDE_H)
#include <libxml/xinclude.h>
#endif

#include "udptl.h"
#include "spandsp.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "media_monitor.h"
#endif
#include "fax_tester.h"
#include "fax_utils.h"
#include "pcap_parse.h"

#define SAMPLES_PER_CHUNK       160

#define INPUT_TIFF_FILE_NAME    "../test-data/itu/fax/itutests.tif"
#define OUTPUT_TIFF_FILE_NAME   "fax_tests.tif"
#define OUTPUT_WAVE_FILE_NAME   "fax_tests.wav"

enum
{
    AUDIO_FAX,
    T38_TERMINAL_FAX,
    T38_GATEWAY_FAX
};

int mode[2] = {AUDIO_FAX, AUDIO_FAX};

t30_state_t *t30_state[2];
fax_state_t *fax_state[2];
t38_gateway_state_t *t38_gateway_state[2];
t38_terminal_state_t *t38_state[2];
t38_core_state_t *t38_core_state[2];
g1050_state_t *g1050_path[2];
awgn_state_t *awgn_state[2];
int16_t audio_buffer[2*2][SAMPLES_PER_CHUNK];

int t38_subst_seq[2] = {0, 0};

t30_exchanged_info_t expected_rx_info[2];

bool use_receiver_not_ready = false;
bool test_local_interrupt = false;

double when = 0.0;

bool phase_e_reached[2] = {false, false};
bool completed[2] = {false, false};
bool succeeded[2] = {false, false};

bool t38_simulate_incrementing_repeats = false;

static int phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    int ch;
    int status;
    int len;
    char tag[20];
    const char *u;
    const uint8_t *v;

    i = (int) (intptr_t) user_data;
    ch = i + 'A';
    snprintf(tag, sizeof(tag), "%c: Phase B", ch);
    printf("%c: Phase B handler - (0x%X) %s\n", ch, result, t30_frametype(result));
    fax_log_rx_parameters(s, tag);
    status = T30_ERR_OK;

    if ((u = t30_get_rx_ident(s)))
    {
        printf("%c: Phase B remote ident '%s'\n", ch, u);
        if (expected_rx_info[i].ident[0]  &&  strcmp(expected_rx_info[i].ident, u))
        {
            printf("%c: Phase B: remote ident incorrect! - expected '%s'\n", ch, expected_rx_info[i].ident);
            status = T30_ERR_IDENT_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info[i].ident[0])
        {
            printf("%c: Phase B: remote ident missing!\n", ch);
            status = T30_ERR_IDENT_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_sub_address(s)))
    {
        printf("%c: Phase B: remote sub-address '%s'\n", ch, u);
        if (expected_rx_info[i].sub_address[0]  &&  strcmp(expected_rx_info[i].sub_address, u))
        {
            printf("%c: Phase B: remote sub-address incorrect! - expected '%s'\n", ch, expected_rx_info[i].sub_address);
            status = T30_ERR_SUB_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info[i].sub_address[0])
        {
            printf("%c: Phase B: remote sub-address missing!\n", ch);
            status = T30_ERR_SUB_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_polled_sub_address(s)))
    {
        printf("%c: Phase B: remote polled sub-address '%s'\n", ch, u);
        if (expected_rx_info[i].polled_sub_address[0]  &&  strcmp(expected_rx_info[i].polled_sub_address, u))
        {
            printf("%c: Phase B: remote polled sub-address incorrect! - expected '%s'\n", ch, expected_rx_info[i].polled_sub_address);
            status = T30_ERR_PSA_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info[i].polled_sub_address[0])
        {
            printf("%c: Phase B: remote polled sub-address missing!\n", ch);
            status = T30_ERR_PSA_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_selective_polling_address(s)))
    {
        printf("%c: Phase B: remote selective polling address '%s'\n", ch, u);
        if (expected_rx_info[i].selective_polling_address[0]  &&  strcmp(expected_rx_info[i].selective_polling_address, u))
        {
            printf("%c: Phase B: remote selective polling address incorrect! - expected '%s'\n", ch, expected_rx_info[i].selective_polling_address);
            status = T30_ERR_SEP_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info[i].selective_polling_address[0])
        {
            printf("%c: Phase B: remote selective polling address missing!\n", ch);
            status = T30_ERR_SEP_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_sender_ident(s)))
    {
        printf("%c: Phase B: remote sender ident '%s'\n", ch, u);
        if (expected_rx_info[i].sender_ident[0]  &&  strcmp(expected_rx_info[i].sender_ident, u))
        {
            printf("%c: Phase B: remote sender ident incorrect! - expected '%s'\n", ch, expected_rx_info[i].sender_ident);
            status = T30_ERR_SID_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info[i].sender_ident[0])
        {
            printf("%c: Phase B: remote sender ident missing!\n", ch);
            status = T30_ERR_SID_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_password(s)))
    {
        printf("%c: Phase B: remote password '%s'\n", ch, u);
        if (expected_rx_info[i].password[0]  &&  strcmp(expected_rx_info[i].password, u))
        {
            printf("%c: Phase B: remote password incorrect! - expected '%s'\n", ch, expected_rx_info[i].password);
            status = T30_ERR_PWD_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info[i].password[0])
        {
            printf("%c: Phase B: remote password missing!\n", ch);
            status = T30_ERR_PWD_UNACCEPTABLE;
        }
    }
    if ((len = t30_get_rx_nsf(s, &v)))
    {
        printf("%c: Phase B: NSF %d bytes\n", ch, len);
        if (expected_rx_info[i].nsf_len  &&  (expected_rx_info[i].nsf_len != len  ||  memcmp(expected_rx_info[i].nsf, v, len)))
        {
            printf("%c: Phase B: remote NSF incorrect! - expected %u bytes\n", ch, (unsigned int) expected_rx_info[i].nsf_len);
        }
    }
    else
    {
        if (expected_rx_info[i].nsf_len)
        {
            printf("%c: Phase B: remote NSF missing! - expected %u bytes\n", ch, (unsigned int) expected_rx_info[i].nsf_len);
        }
    }
    if ((len = t30_get_rx_nsc(s, &v)))
    {
        printf("%c: Phase B: NSC %d bytes\n", ch, len);
        if (expected_rx_info[i].nsc_len  &&  (expected_rx_info[i].nsc_len != len  ||  memcmp(expected_rx_info[i].nsc, v, len)))
        {
            printf("%c: Phase B: remote NSC incorrect! - expected %u bytes\n", ch, (unsigned int) expected_rx_info[i].nsc_len);
        }
    }
    else
    {
        if (expected_rx_info[i].nsc_len)
        {
            printf("%c: Phase B: remote NSC missing! - expected %u bytes\n", ch, (unsigned int) expected_rx_info[i].nsc_len);
        }
    }
    if ((len = t30_get_rx_nss(s, &v)))
    {
        printf("%c: Phase B: NSS %d bytes\n", ch, len);
        if (expected_rx_info[i].nss_len  &&  (expected_rx_info[i].nss_len != len  ||  memcmp(expected_rx_info[i].nss, v, len)))
        {
            printf("%c: Phase B: remote NSS incorrect! - expected %u bytes\n", ch, (unsigned int) expected_rx_info[i].nss_len);
        }
    }
    else
    {
        if (expected_rx_info[i].nss_len)
        {
            printf("%c: Phase B: remote NSS missing! - expected %u bytes\n", ch, (unsigned int) expected_rx_info[i].nsf_len);
        }
    }

    return status;
}
/*- End of function --------------------------------------------------------*/

static int phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    char tag[20];

    i = (int) (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase D", i + 'A');
    printf("%c: Phase D handler - (0x%X) %s\n", i + 'A', result, t30_frametype(result));
    fax_log_page_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);

    if (use_receiver_not_ready)
        t30_set_receiver_not_ready(s, 3);

    if (test_local_interrupt)
    {
        if (i == 'A')
        {
            printf("%c: Initiating interrupt request\n", i);
            t30_local_interrupt_request(s, true);
        }
        else
        {
            switch (result)
            {
            case T30_PIP:
            case T30_PRI_MPS:
            case T30_PRI_EOM:
            case T30_PRI_EOP:
                printf("%c: Accepting interrupt request\n", i);
                t30_local_interrupt_request(s, true);
                break;
            case T30_PIN:
                break;
            }
        }
    }
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    char tag[20];

    i = (int) (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase E", i + 'A');
    printf("%c: Phase E handler - (%d) %s\n", i + 'A', result, t30_completion_code_to_str(result));
    fax_log_final_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);
    t30_get_transfer_statistics(s, &t);
    succeeded[i] = (result == T30_ERR_OK);
    phase_e_reached[i] = true;
}
/*- End of function --------------------------------------------------------*/

static void real_time_frame_handler(t30_state_t *s,
                                    void *user_data,
                                    bool incoming,
                                    const uint8_t *msg,
                                    int len)
{
    int i;

    i = (intptr_t) user_data;
    printf("%c: Real time frame handler - %s, %s, length = %d\n",
           i + 'A',
           (incoming)  ?  "line->T.30"  : "T.30->line",
           t30_frametype(msg[2]),
           len);
}
/*- End of function --------------------------------------------------------*/

static int document_handler(t30_state_t *s, void *user_data, int event)
{
    int i;

    i = (intptr_t) user_data;
    printf("%c: Document handler - event %d\n", i + 'A', event);
    return false;
}
/*- End of function --------------------------------------------------------*/

static void set_t30_callbacks(t30_state_t *t30, int chan)
{
    t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) chan);
    t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) chan);
    t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) chan);
    t30_set_real_time_frame_handler(t30, real_time_frame_handler, (void *) (intptr_t) chan);
    t30_set_document_handler(t30, document_handler, (void *) (intptr_t) chan);
}
/*- End of function --------------------------------------------------------*/

static void real_time_gateway_frame_handler(t38_gateway_state_t *s,
                                            void *user_data,
                                            bool incoming,
                                            const uint8_t *msg,
                                            int len)
{
    int i;

    i = (intptr_t) user_data;
    printf("%c: Real time gateway frame handler - %s, %s, length = %d\n",
           i + 'A',
           (incoming)  ?  "PSTN->T.38"  : "T.38->PSTN",
           t30_frametype(msg[2]),
           len);
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    int i;
    int chan;

    /* This routine queues messages between two instances of T.38 processing */
    chan = (intptr_t) user_data;
    if (t38_simulate_incrementing_repeats)
    {
        for (i = 0;  i < count;  i++)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d\n", t38_subst_seq[chan], len);

            if (g1050_put(g1050_path[chan], buf, len, t38_subst_seq[chan], when) < 0)
                printf("Lost packet %d\n", t38_subst_seq[chan]);
            t38_subst_seq[chan] = (t38_subst_seq[chan] + 1) & 0xFFFF;
        }
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);

        for (i = 0;  i < count;  i++)
        {
            if (g1050_put(g1050_path[chan], buf, len, s->tx_seq_no, when) < 0)
                printf("Lost packet %d\n", s->tx_seq_no);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int16_t silence[SAMPLES_PER_CHUNK];
    int16_t t30_amp[2][SAMPLES_PER_CHUNK];
    int16_t t38_amp[2][SAMPLES_PER_CHUNK];
    int16_t t38_amp_hist_a[8][SAMPLES_PER_CHUNK];
    int16_t t38_amp_hist_b[8][SAMPLES_PER_CHUNK];
    int16_t out_amp[SAMPLES_PER_CHUNK*4];
    int16_t *fax_rx_buf[2];
    int16_t *fax_tx_buf[2];
    int16_t *t38_gateway_rx_buf[2];
    int16_t *t38_gateway_tx_buf[2];
    int t30_len[2];
    int t38_len[2];
    int hist_ptr;
    int log_audio;
    int msg_len;
    uint8_t msg[1024];
    int outframes;
    SNDFILE *wave_handle;
    SNDFILE *input_wave_handle;
    int use_ecm;
    int use_tep;
    int feedback_audio;
    int use_transmit_on_idle;
    int t38_version;
    const char *input_tiff_file_name;
    const char *decode_file_name;
    int i;
    int j;
    int seq_no;
    int g1050_model_no;
    int g1050_speed_pattern_no;
    int t38_transport;
    double tx_when;
    double rx_when;
    int supported_modems;
    int remove_fill_bits;
    int opt;
    int start_page;
    int end_page;
    int drop_frame;
    int drop_frame_rate;
    float signal_scaling;
    int signal_level;
    int noise_level;
    int code_to_look_up;
    int scan_line_time;
    int allowed_bilevel_resolutions; 
    int colour_enabled;
    t38_stats_t t38_stats;
    t30_stats_t t30_stats;
    logging_state_t *logging;
    int expected_pages;
    char *page_header_info;
    char *page_header_tz;
    const char *tag;
    char buf[132 + 1];
#if defined(ENABLE_GUI)
    int use_gui;
#endif

#if defined(ENABLE_GUI)
    use_gui = false;
#endif
    log_audio = false;
    use_ecm = false;
    t38_version = 1;
    input_tiff_file_name = INPUT_TIFF_FILE_NAME;
    t38_simulate_incrementing_repeats = false;
    g1050_model_no = 0;
    g1050_speed_pattern_no = 1;
    remove_fill_bits = false;
    use_tep = false;
    feedback_audio = false;
    use_transmit_on_idle = true;
    supported_modems = T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17;
    page_header_info = NULL;
    page_header_tz = NULL;
    drop_frame = 0;
    drop_frame_rate = 0;
    start_page = -1;
    end_page = -1;
    signal_level = 0;
    noise_level = -99;
    scan_line_time = 0;
    decode_file_name = NULL;
    code_to_look_up = -1;
    allowed_bilevel_resolutions = 0;
    colour_enabled = false;
    t38_transport = T38_TRANSPORT_UDPTL;
    while ((opt = getopt(argc, argv, "b:c:Cd:D:efFgH:i:Ilm:M:n:p:s:S:tT:u:v:z:")) != -1)
    {
        switch (opt)
        {
        case 'b':
            allowed_bilevel_resolutions = atoi(optarg);
            break;
        case 'c':
            code_to_look_up = atoi(optarg);
            break;
        case 'C':
            colour_enabled = true;
            break;
        case 'd':
            decode_file_name = optarg;
            break;
        case 'D':
            drop_frame_rate =
            drop_frame = atoi(optarg);
            break;
        case 'e':
            use_ecm = true;
            break;
        case 'f':
            feedback_audio = true;
            break;
        case 'F':
            remove_fill_bits = true;
            break;
        case 'g':
#if defined(ENABLE_GUI)
            use_gui = true;
#else
            fprintf(stderr, "Graphical monitoring not available\n");
            exit(2);
#endif
            break;
        case 'H':
            page_header_info = optarg;
            break;
        case 'i':
            input_tiff_file_name = optarg;
            break;
        case 'I':
            t38_simulate_incrementing_repeats = true;
            break;
        case 'l':
            log_audio = true;
            break;
        case 'm':
            supported_modems = atoi(optarg);
            break;
        case 'M':
            g1050_model_no = optarg[0] - 'A' + 1;
            break;
        case 'n':
            noise_level = atoi(optarg);
            break;
        case 'p':
            for (i = 0;  i < 2;  i++)
            {
                switch (optarg[i])
                {
                case 'A':
                    mode[i] = AUDIO_FAX;
                    break;
                case 'G':
                    mode[i] = T38_GATEWAY_FAX;
                    break;
                case 'T':
                    mode[i] = T38_TERMINAL_FAX;
                    break;
                default:
                    fprintf(stderr, "Unknown FAX path element %c\n", optarg[i]);
                    exit(2);
                }
            }
            if ((mode[0] == AUDIO_FAX  &&  mode[1] != AUDIO_FAX)
                ||
                (mode[0] != AUDIO_FAX  &&  mode[1] == AUDIO_FAX))
            {
                fprintf(stderr, "Invalid FAX path %s\n", optarg);
                exit(2);
            }
            break;
        case 's':
            g1050_speed_pattern_no = atoi(optarg);
            break;
#if 0
        case 's':
            signal_level = atoi(optarg);
            break;
#endif
        case 'S':
            scan_line_time = atoi(optarg);
            break;
        case 't':
            use_tep = true;
            break;
        case 'T':
            start_page = 0;
            end_page = atoi(optarg);
            break;
        case 'u':
            if (strcasecmp(optarg, "udptl") == 0)
                t38_transport = T38_TRANSPORT_UDPTL;
            else if (strcasecmp(optarg, "rtp") == 0)
                t38_transport = T38_TRANSPORT_RTP;
            else if (strcasecmp(optarg, "tcp") == 0)
                t38_transport = T38_TRANSPORT_TCP;
            else if (strcasecmp(optarg, "tcp-tpkt") == 0)
                t38_transport = T38_TRANSPORT_TCP_TPKT;
            else
            {
                fprintf(stderr, "Unknown T.38 transport mode\n");
                exit(2);
            }
            break;
        case 'v':
            t38_version = atoi(optarg);
            break;
        case 'z':
            page_header_tz = optarg;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    if (code_to_look_up >= 0)
    {
        printf("Result code %d is %s\n", code_to_look_up, t30_completion_code_to_str(code_to_look_up));
        exit(0);
    }

    printf("Using T.38 version %d\n", t38_version);
    if (use_ecm)
        printf("Using ECM\n");

    wave_handle = NULL;
    if (log_audio)
    {
        if ((wave_handle = sf_open_telephony_write(OUTPUT_WAVE_FILE_NAME, 4)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            exit(2);
        }
    }
    memset(silence, 0, sizeof(silence));

    srand48(0x1234567);
    /* Set up the nodes */
    input_wave_handle = NULL;
    if (mode[0] == T38_TERMINAL_FAX)
    {
    }
    else
    {
        if (decode_file_name)
        {
            if ((input_wave_handle = sf_open_telephony_read(decode_file_name, 1)) == NULL)
            {
                fprintf(stderr, "    Cannot open audio file '%s'\n", decode_file_name);
                exit(2);
            }
        }
    }

    for (i = 0;  i < 2;  i++)
    {
        tag = (i == 0)  ?  "A"  :  "B";

        memset(&expected_rx_info[i], 0, sizeof(expected_rx_info[i]));
        if (mode[i] == T38_TERMINAL_FAX)
        {
            if ((t38_state[i] = t38_terminal_init(NULL, (i == 0), tx_packet_handler, (void *) (intptr_t) i)) == NULL)
            {
                fprintf(stderr, "Cannot start the T.38 terminal instance\n");
                exit(2);
            }
            t30_state[i] = t38_terminal_get_t30_state(t38_state[i]);
            t38_core_state[i] = t38_terminal_get_t38_core_state(t38_state[i]);

            logging = t38_terminal_get_logging_state(t38_state[i]);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, tag);

            logging = t38_core_get_logging_state(t38_core_state[i]);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, tag);

            logging = t30_get_logging_state(t30_state[i]);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, tag);
        }
        else
        {
            if ((fax_state[i] = fax_init(NULL, (i == 0))) == NULL)
            {
                fprintf(stderr, "Cannot start FAX instance\n");
                exit(2);
            }
            t30_state[i] = fax_get_t30_state(fax_state[i]);

            logging = fax_get_logging_state(fax_state[i]);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, tag);

            logging = fax_modems_get_logging_state(&fax_state[i]->modems);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, tag);

            logging = t30_get_logging_state(t30_state[i]);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, tag);

            if (mode[i] == T38_GATEWAY_FAX)
            {
                if ((t38_gateway_state[i] = t38_gateway_init(NULL, tx_packet_handler, (void *) (intptr_t) i)) == NULL)
                {
                    fprintf(stderr, "Cannot start the T.38 gateway instancel\n");
                    exit(2);
                }
                t38_core_state[i] = t38_gateway_get_t38_core_state(t38_gateway_state[i]);

                logging = t38_gateway_get_logging_state(t38_gateway_state[i]);
                span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
                span_log_set_tag(logging, tag);

                logging = fax_modems_get_logging_state(&t38_gateway_state[i]->audio.modems);
                span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
                span_log_set_tag(logging, tag);

                logging = t38_core_get_logging_state(t38_core_state[i]);
                span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
                span_log_set_tag(logging, tag);

                fax_rx_buf[i] = t38_amp[i];
                fax_tx_buf[i] = t30_amp[i];
                t38_gateway_rx_buf[i] = t30_amp[i];
                t38_gateway_tx_buf[i] = t38_amp[i];
            }
            else
            {
                fax_rx_buf[i] = t30_amp[i];
                fax_tx_buf[i] = t30_amp[i ^ 1];
                t38_gateway_rx_buf[i] = NULL;
                t38_gateway_tx_buf[i] = NULL;
            }
            awgn_state[i] = NULL;
            signal_scaling = 1.0f;
            if (noise_level > -99)
            {
                awgn_state[i] = awgn_init_dbm0(NULL, 1234567, noise_level);
                signal_scaling = powf(10.0f, signal_level/20.0f);
                printf("Signal scaling %f\n", signal_scaling);
            }
        }
        set_t30_callbacks(t30_state[i], i);
    }
    /* Set up the channels */
    for (i = 0;  i < 2;  i++)
    {
        if ((g1050_path[i] = g1050_init(g1050_model_no, g1050_speed_pattern_no, 100, 33)) == NULL)
        {
            fprintf(stderr, "Failed to start IP network path model\n");
            exit(2);
        }
        memset(audio_buffer[2*i], 0, SAMPLES_PER_CHUNK*sizeof(int16_t));
        memset(audio_buffer[2*i + 1], 0, SAMPLES_PER_CHUNK*sizeof(int16_t));
        memset(t30_amp[i], 0, sizeof(t30_amp[i]));
        memset(t38_amp[i], 0, sizeof(t38_amp[i]));
    }
    memset(t38_amp_hist_a, 0, sizeof(t38_amp_hist_a));
    memset(t38_amp_hist_b, 0, sizeof(t38_amp_hist_b));

    for (i = 0;  i < 2;  i++)
    {
        j = i + 1;
        sprintf(buf, "%d%d%d%d%d%d%d%d", j, j, j, j, j, j, j, j);
        t30_set_tx_ident(t30_state[i], buf);
        strcpy(expected_rx_info[i ^ 1].ident, buf);
        sprintf(buf, "Sub-address %d", j);
        t30_set_tx_sub_address(t30_state[i], buf);
        //strcpy(expected_rx_info[i ^ 1].sub_address, buf);
        sprintf(buf, "Sender ID %d", j);
        t30_set_tx_sender_ident(t30_state[i], buf);
        //strcpy(expected_rx_info[i ^ 1].sender_ident, buf);
        sprintf(buf, "Password %d", j);
        t30_set_tx_password(t30_state[i], buf);
        //strcpy(expected_rx_info[i ^ 1].password, buf);
        sprintf(buf, "Polled sub-add %d", j);
        t30_set_tx_polled_sub_address(t30_state[i], buf);
        //strcpy(expected_rx_info[i ^ 1].polled_sub_address, buf);
        sprintf(buf, "Select poll add %d", j);
        t30_set_tx_selective_polling_address(t30_state[i], buf);
        //strcpy(expected_rx_info[i ^ 1].selective_polling_address, buf);
        t30_set_tx_page_header_info(t30_state[i], page_header_info);
        if (page_header_tz)
            t30_set_tx_page_header_tz(t30_state[i], page_header_tz);

        if ((i & 1) == 1)
        {
            t30_set_tx_nsf(t30_state[i], (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
            expected_rx_info[i ^ 1].nsf = (uint8_t *) "\x50\x00\x00\x00Spandsp\x00";
            expected_rx_info[i ^ 1].nsf_len = 12;
        }

        t30_set_supported_modems(t30_state[i], supported_modems);
        t30_set_supported_t30_features(t30_state[i],
                                       T30_SUPPORT_IDENTIFICATION
                                     | T30_SUPPORT_SELECTIVE_POLLING
                                     | T30_SUPPORT_SUB_ADDRESSING);
        t30_set_supported_image_sizes(t30_state[i],
                                      T4_SUPPORT_WIDTH_215MM
                                    | T4_SUPPORT_WIDTH_255MM
                                    | T4_SUPPORT_WIDTH_303MM
                                    | T4_SUPPORT_LENGTH_US_LETTER
                                    | T4_SUPPORT_LENGTH_US_LEGAL
                                    | T4_SUPPORT_LENGTH_UNLIMITED);
        switch (allowed_bilevel_resolutions)
        {
        case 0:
            /* Allow anything */
            t30_set_supported_bilevel_resolutions(t30_state[i],
                                                  T4_RESOLUTION_R8_STANDARD
                                                | T4_RESOLUTION_R8_FINE
                                                | T4_RESOLUTION_R8_SUPERFINE
                                                | T4_RESOLUTION_R16_SUPERFINE
                                                | T4_RESOLUTION_200_100
                                                | T4_RESOLUTION_200_200
                                                | T4_RESOLUTION_200_400
                                                | T4_RESOLUTION_300_300
                                                | T4_RESOLUTION_300_600
                                                | T4_RESOLUTION_400_400
                                                | T4_RESOLUTION_400_800
                                                | T4_RESOLUTION_600_600
                                                | T4_RESOLUTION_600_1200
                                                | T4_RESOLUTION_1200_1200);
            break;
        case 1:
            /* Allow anything metric */
            t30_set_supported_bilevel_resolutions(t30_state[i],
                                                  T4_RESOLUTION_R8_STANDARD
                                                | T4_RESOLUTION_R8_FINE
                                                | T4_RESOLUTION_R8_SUPERFINE
                                                | T4_RESOLUTION_R16_SUPERFINE);
            break;
        case 2:
            /* Allow anything inch based */
            t30_set_supported_bilevel_resolutions(t30_state[i],
                                                  T4_RESOLUTION_200_100
                                                | T4_RESOLUTION_200_200
                                                | T4_RESOLUTION_200_400
                                                | T4_RESOLUTION_300_300
                                                | T4_RESOLUTION_300_600
                                                | T4_RESOLUTION_400_400
                                                | T4_RESOLUTION_400_800
                                                | T4_RESOLUTION_600_600
                                                | T4_RESOLUTION_600_1200
                                                | T4_RESOLUTION_1200_1200);
            break;
        case 3:
            /* Allow only restricted length resolution */
            t30_set_supported_bilevel_resolutions(t30_state[i],
                                                  T4_RESOLUTION_R8_STANDARD
                                                | T4_RESOLUTION_R8_FINE
                                                | T4_RESOLUTION_200_100
                                                | T4_RESOLUTION_200_200);
            break;
        case 4:
            /* Allow only more restricted length resolution */
            t30_set_supported_bilevel_resolutions(t30_state[i],
                                                  T4_RESOLUTION_R8_STANDARD
                                                | T4_RESOLUTION_200_100);
            break;
        }
        if (colour_enabled)
        {
            t30_set_supported_colour_resolutions(t30_state[i],
                                                 T4_RESOLUTION_100_100
                                               | T4_RESOLUTION_200_200
                                               | T4_RESOLUTION_300_300
                                               | T4_RESOLUTION_400_400
                                               | T4_RESOLUTION_600_600
                                               | T4_RESOLUTION_1200_1200);
        }
        else
        {
            t30_set_supported_colour_resolutions(t30_state[i], 0);
        }
        t30_set_supported_output_compressions(t30_state[i], T4_COMPRESSION_T6 | T4_COMPRESSION_JPEG);
        t30_set_ecm_capability(t30_state[i], use_ecm);
        t30_set_supported_compressions(t30_state[i],
                                       T4_COMPRESSION_T4_1D
                                     | T4_COMPRESSION_T4_2D
                                     | T4_COMPRESSION_T6
                                     | T4_COMPRESSION_T85
                                     | T4_COMPRESSION_T85_L0
                                     //| T4_COMPRESSION_T88
                                     | T4_COMPRESSION_T43
                                     | T4_COMPRESSION_T45
                                     | T4_COMPRESSION_T42_T81
                                     | T4_COMPRESSION_SYCC_T81
                                     | T4_COMPRESSION_GRAYSCALE
                                     | T4_COMPRESSION_COLOUR
                                     | T4_COMPRESSION_12BIT
                                     | T4_COMPRESSION_COLOUR_TO_GRAY
                                     | T4_COMPRESSION_GRAY_TO_BILEVEL
                                     | T4_COMPRESSION_COLOUR_TO_BILEVEL
                                     | T4_COMPRESSION_RESCALING
                                     | 0);
        t30_set_minimum_scan_line_time(t30_state[i], scan_line_time);

        if (mode[i] == T38_GATEWAY_FAX)
        {
            t38_gateway_set_transmit_on_idle(t38_gateway_state[i], use_transmit_on_idle);
            t38_gateway_set_supported_modems(t38_gateway_state[i], supported_modems);
            //t38_gateway_set_nsx_suppression(t38_state[i], NULL, 0, NULL, 0);
            t38_gateway_set_fill_bit_removal(t38_gateway_state[i], remove_fill_bits);
            t38_gateway_set_real_time_frame_handler(t38_gateway_state[i], real_time_gateway_frame_handler, (void *) (intptr_t) i);
            t38_gateway_set_ecm_capability(t38_gateway_state[i], use_ecm);
        }
        if (mode[i] != AUDIO_FAX)
        {
            t38_set_t38_version(t38_core_state[i], t38_version);
        }

        if (mode[i] == T38_TERMINAL_FAX)
        {
            //t30_set_iaf_mode(t30_state[i], T30_IAF_MODE_NO_FILL_BITS);
            switch (t38_transport)
            {
            case T38_TRANSPORT_UDPTL:
            case T38_TRANSPORT_RTP:
                t38_terminal_set_fill_bit_removal(t38_state[i], remove_fill_bits);
                t38_terminal_set_tep_mode(t38_state[i], use_tep);
                break;
            case T38_TRANSPORT_TCP:
            case T38_TRANSPORT_TCP_TPKT:
                t38_terminal_set_fill_bit_removal(t38_state[i], true);
                t38_terminal_set_config(t38_state[i], T38_TERMINAL_OPTION_NO_PACING | T38_TERMINAL_OPTION_NO_INDICATORS);
                t38_terminal_set_tep_mode(t38_state[i], false);
                break;
            }
        }
        else
        {
            fax_set_transmit_on_idle(fax_state[i], use_transmit_on_idle);
            fax_set_tep_mode(fax_state[i], use_tep);
        }
    }

    t30_set_tx_file(t30_state[0], input_tiff_file_name, start_page, end_page);
    t30_set_rx_file(t30_state[1], OUTPUT_TIFF_FILE_NAME, -1);

#if defined(ENABLE_GUI)
    if (use_gui)
        start_media_monitor();
#endif
    hist_ptr = 0;
    for (;;)
    {
        memset(out_amp, 0, sizeof(out_amp));

        for (i = 0;  i < 2;  i++)
        {
            /* Update T.30 timing */
            logging = t30_get_logging_state(t30_state[i]);
            span_log_bump_samples(logging, SAMPLES_PER_CHUNK);

            if (mode[i] == T38_TERMINAL_FAX)
            {
                /* Update T.38 termination timing */
                logging = t38_terminal_get_logging_state(t38_state[i]);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
                logging = t38_core_get_logging_state(t38_core_state[i]);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);

                completed[i] = t38_terminal_send_timeout(t38_state[i], SAMPLES_PER_CHUNK);
            }
            else
            {
                /* Update audio FAX timing */
                logging = fax_get_logging_state(fax_state[i]);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);

#if 0
                /* Mute the signal */
                vec_zeroi16(fax_rx_buf[i], SAMPLES_PER_CHUNK);
#endif
                fax_rx(fax_state[i], fax_rx_buf[i], SAMPLES_PER_CHUNK);
                if (!t30_call_active(t30_state[i]))
                {
                    completed[i] = true;
                    continue;
                }

                if (i == 0  &&  input_wave_handle)
                {
                    t30_len[i] = sf_readf_short(input_wave_handle, fax_tx_buf[i], SAMPLES_PER_CHUNK);
                    if (t30_len[i] == 0)
                        break;
                }
                else
                {
                    t30_len[i] = fax_tx(fax_state[i], fax_tx_buf[i], SAMPLES_PER_CHUNK);
                    if (!use_transmit_on_idle)
                    {
                        /* The receive side always expects a full block of samples, but the
                           transmit side may not be sending any when it doesn't need to. We
                           may need to pad with some silence. */
                        if (t30_len[i] < SAMPLES_PER_CHUNK)
                        {
                            memset(t30_amp[i] + t30_len[i], 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - t30_len[i]));
                            t30_len[i] = SAMPLES_PER_CHUNK;
                        }
                    }
                    if (awgn_state[i])
                    {
                        for (j = 0;  j < t30_len[i];  j++)
                            fax_tx_buf[i][j] = ((int16_t) (fax_tx_buf[i][j]*signal_scaling)) + awgn(awgn_state[i]);
                    }
                }
                if (log_audio)
                {
                    for (j = 0;  j < t30_len[i];  j++)
                        out_amp[4*j + 2*i] = t30_amp[i][j];
                }
                if (feedback_audio)
                {
                    for (j = 0;  j < t30_len[i];  j++)
                        t30_amp[i][j] += t38_amp_hist_a[hist_ptr][j] >> 1;
                    memcpy(t38_amp_hist_a[hist_ptr], t38_amp[i], sizeof(int16_t)*SAMPLES_PER_CHUNK);
                }

                if (mode[i] == T38_GATEWAY_FAX)
                {
                    /* Update T.38 gateway timing */
                    logging = t38_gateway_get_logging_state(t38_gateway_state[i]);
                    span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
                    logging = t38_core_get_logging_state(t38_core_state[i]);
                    span_log_bump_samples(logging, SAMPLES_PER_CHUNK);

                    if (drop_frame_rate  &&  --drop_frame == 0)
                    {
                        drop_frame = drop_frame_rate;
                        if (t38_gateway_rx_fillin(t38_gateway_state[i], SAMPLES_PER_CHUNK))
                            break;
                    }
                    else
                    {
                        if (t38_gateway_rx(t38_gateway_state[i], t38_gateway_rx_buf[i], SAMPLES_PER_CHUNK))
                            break;
                    }

                    t38_len[i] = t38_gateway_tx(t38_gateway_state[i], t38_gateway_tx_buf[i], SAMPLES_PER_CHUNK);
                    if (!use_transmit_on_idle)
                    {
                        if (t38_len[i] < SAMPLES_PER_CHUNK)
                        {
                            memset(t38_amp[i] + t38_len[i], 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - t38_len[i]));
                            t38_len[i] = SAMPLES_PER_CHUNK;
                        }
                    }
                    if (feedback_audio)
                    {
                        for (j = 0;  j < t30_len[i];  j++)
                            t30_amp[i][j] += t38_amp_hist_a[hist_ptr][j] >> 1;
                        memcpy(t38_amp_hist_a[hist_ptr], t38_amp[i], sizeof(int16_t)*SAMPLES_PER_CHUNK);
                    }

                    if (log_audio)
                    {
                        for (j = 0;  j < t38_len[i];  j++)
                            out_amp[4*j + 2*i + 1] = t38_amp[i][j];
                    }
                }
            }
            if (mode[i] != AUDIO_FAX)
            {
                while ((msg_len = g1050_get(g1050_path[i], msg, 1024, when, &seq_no, &tx_when, &rx_when)) >= 0)
                {
#if defined(ENABLE_GUI)
                    if (use_gui)
                        media_monitor_rx(seq_no, tx_when, rx_when);
#endif
                   t38_core_rx_ifp_packet(t38_core_state[i ^ 1], msg, msg_len, seq_no);
                }
            }
        }
        if (log_audio)
        {
            outframes = sf_writef_short(wave_handle, out_amp, SAMPLES_PER_CHUNK);
            if (outframes != SAMPLES_PER_CHUNK)
                break;
        }

        when += (float) SAMPLES_PER_CHUNK/(float) SAMPLE_RATE;

        if (completed[0]  &&  completed[1])
            break;
#if defined(ENABLE_GUI)
        if (use_gui)
            media_monitor_update_display();
#endif
        if (++hist_ptr > 3)
            hist_ptr = 0;
    }
    for (i = 0;  i < 2;  i++)
    {
        if (mode[i] == T38_GATEWAY_FAX)
        {
            t38_gateway_get_transfer_statistics(t38_gateway_state[i], &t38_stats);
            printf("%c side exchanged %d pages at %dbps, in %s mode\n",
                   i + 'A',
                   t38_stats.pages_transferred,
                   t38_stats.bit_rate,
                   (t38_stats.error_correcting_mode)  ?  "ECM"  :  "non-ECM");
        }
    }
    if (input_wave_handle)
    {
        if (sf_close_telephony(input_wave_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", decode_file_name);
            exit(2);
        }
    }
    if (log_audio)
    {
        if (sf_close_telephony(wave_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            exit(2);
        }
    }

    /* Check how many pages should have been transferred */
    expected_pages = get_tiff_total_pages(input_tiff_file_name);
    if (end_page >= 0  &&  expected_pages > end_page + 1)
        expected_pages = end_page + 1;
    if (start_page >= 0)
        expected_pages -= start_page;
    /* Check how many pages were transferred */
    for (i = 0;  i < 2;  i++)
    {
        if (!phase_e_reached[i])
            break;
        if (!succeeded[i])
            break;
        t30_get_transfer_statistics(t30_state[i], &t30_stats);
        if (i & 1)
        {
            if (t30_stats.pages_tx != 0  ||  t30_stats.pages_rx != expected_pages)
                break;
        }
        else
        {
            if (t30_stats.pages_tx != expected_pages  ||  t30_stats.pages_rx != 0)
                break;
        }
        if (mode[i] == T38_TERMINAL_FAX)
            t38_terminal_release(t38_state[i]);
        else
            fax_release(fax_state[i]);
        if (mode[i] == T38_GATEWAY_FAX)
            t38_gateway_release(t38_gateway_state[i]);
    }
    if (i < 2)
    {
        printf("Tests failed\n");
        exit(2);
    }
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
