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

TSB85 <-----------+                                     +-----------> TSB85
                   \                                   /
 T.31 <-----------+ \                                 / +-----------> T.31
                   \ \                               / /
          +--Modems-+-+-----------TDM/RTP-----------+-+-Modems--+
          |            \                           /            |
          |             \                         /             |
 T.30 <---+        T.38 gateway            T.38 gateway         +---> T.30
          |               \                     /               |
          |                \                   /                |
          +---T.38---+-+----+----UDPTL/RTP----+----+ +---T.38---+
                    / / \                         / \ \
 T.31 <------------/ /   +----------TCP----------+   \ +------------> T.31
                    /                                 \
TSB85 <------------+                                   +------------> TSB85

T.30<->Modems<-------------------------TDM/RTP------------------------->Modems<->T.30
T.30<->Modems<-TDM/RTP->T.38 gateway<-UDPTL/RTP->T.38 gateway<-TDM/RTP->Modems<->T.30
T.30<->Modems<-TDM/RTP->T.38 gateway<-UDPTL/RTP-------------------------->T.38<->T.30
T.30<->T.38<--------------------------UDPTL/RTP->T.38 gateway<-TDM/RTP->Modems<->T.30
T.30<->T.38<--------------------------UDPTL/RTP-------------------------->T.38<->T.30

The T.31 and TSB85 parts are incomplete right now.
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
#define INPUT_WAVE_FILE_NAME    "fax_cap.wav"
#define OUTPUT_WAVE_FILE_NAME   "fax_tests.wav"

enum
{
    AUDIO_FAX = 1,
    T38_FAX,
    T31_AUDIO_FAX,
    T31_T38_FAX,
    TSB85_AUDIO_FAX,
    TSB85_T38_FAX,
    REPLAY_AUDIO_FAX,
    REPLAY_T38_FAX,
    AUDIO_TO_T38_GATEWAY,
    PASSTHROUGH,
    AUDIO_CHAN,
    T38_CHAN
};

const char *output_tiff_file_name;

struct audio_buf_s
{
    int16_t amp[SAMPLES_PER_CHUNK];
    int len;
};

struct chain_element_s
{
    int node_type;
    int left_chan_type;
    int right_chan_type;
    struct
    {
        fax_state_t *fax_state;
        t38_terminal_state_t *t38_state;
        faxtester_state_t *faxtester_state;
        t38_gateway_state_t *t38_gateway_state;
        SNDFILE *wave_handle;
    } node;
    struct
    {
        g1050_state_t *g1050_path;
        both_ways_line_model_state_t *line_model;
        struct audio_buf_s *audio_in_buf;
        struct audio_buf_s *audio_out_buf;
    } path;
    t30_state_t *t30_state;
    t38_core_state_t *t38_core_state;
    int t38_subst_seq;
    bool phase_e_reached;
    bool completed;
    bool succeeded;
    t30_exchanged_info_t expected_rx_info;

    awgn_state_t *awgn_state;

    struct audio_buf_s audio_buf[2];

    int peer;
    int t38_peer;

    char tag[10];
};

struct chain_element_s chain[7];
int chain_elements = 2;

bool t38_simulate_incrementing_repeats = false;
bool use_receiver_not_ready = false;
bool test_local_interrupt = false;

double when = 0.0;

static int phase_b_handler(void *user_data, int result)
{
    int i;
    int ch;
    int status;
    int len;
    t30_state_t *s;
    char tag[20];
    const char *u;
    const uint8_t *v;
    t30_exchanged_info_t *info;

    i = (int) (intptr_t) user_data;
    s = chain[i].t30_state;
    ch = i + 'A';
    info = &chain[i].expected_rx_info;
    snprintf(tag, sizeof(tag), "%c: Phase B", ch);
    printf("%c: Phase B handler - (0x%X) %s\n", ch, result, t30_frametype(result));
    fax_log_rx_parameters(s, tag);
    status = T30_ERR_OK;

    if ((u = t30_get_rx_ident(s)))
    {
        printf("%c: Phase B remote ident '%s'\n", ch, u);
        if (info->ident[0]  &&  strcmp(info->ident, u))
        {
            printf("%c: Phase B: remote ident incorrect! - expected '%s'\n", ch, info->ident);
            status = T30_ERR_IDENT_UNACCEPTABLE;
        }
    }
    else
    {
        if (info->ident[0])
        {
            printf("%c: Phase B: remote ident missing!\n", ch);
            status = T30_ERR_IDENT_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_sub_address(s)))
    {
        printf("%c: Phase B: remote sub-address '%s'\n", ch, u);
        if (info->sub_address[0]  &&  strcmp(info->sub_address, u))
        {
            printf("%c: Phase B: remote sub-address incorrect! - expected '%s'\n", ch, info->sub_address);
            status = T30_ERR_SUB_UNACCEPTABLE;
        }
    }
    else
    {
        if (info->sub_address[0])
        {
            printf("%c: Phase B: remote sub-address missing!\n", ch);
            status = T30_ERR_SUB_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_polled_sub_address(s)))
    {
        printf("%c: Phase B: remote polled sub-address '%s'\n", ch, u);
        if (info->polled_sub_address[0]  &&  strcmp(info->polled_sub_address, u))
        {
            printf("%c: Phase B: remote polled sub-address incorrect! - expected '%s'\n", ch, info->polled_sub_address);
            status = T30_ERR_PSA_UNACCEPTABLE;
        }
    }
    else
    {
        if (info->polled_sub_address[0])
        {
            printf("%c: Phase B: remote polled sub-address missing!\n", ch);
            status = T30_ERR_PSA_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_selective_polling_address(s)))
    {
        printf("%c: Phase B: remote selective polling address '%s'\n", ch, u);
        if (info->selective_polling_address[0]  &&  strcmp(info->selective_polling_address, u))
        {
            printf("%c: Phase B: remote selective polling address incorrect! - expected '%s'\n", ch, info->selective_polling_address);
            status = T30_ERR_SEP_UNACCEPTABLE;
        }
    }
    else
    {
        if (info->selective_polling_address[0])
        {
            printf("%c: Phase B: remote selective polling address missing!\n", ch);
            status = T30_ERR_SEP_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_sender_ident(s)))
    {
        printf("%c: Phase B: remote sender ident '%s'\n", ch, u);
        if (info->sender_ident[0]  &&  strcmp(info->sender_ident, u))
        {
            printf("%c: Phase B: remote sender ident incorrect! - expected '%s'\n", ch, info->sender_ident);
            status = T30_ERR_SID_UNACCEPTABLE;
        }
    }
    else
    {
        if (info->sender_ident[0])
        {
            printf("%c: Phase B: remote sender ident missing!\n", ch);
            status = T30_ERR_SID_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_password(s)))
    {
        printf("%c: Phase B: remote password '%s'\n", ch, u);
        if (info->password[0]  &&  strcmp(info->password, u))
        {
            printf("%c: Phase B: remote password incorrect! - expected '%s'\n", ch, info->password);
            status = T30_ERR_PWD_UNACCEPTABLE;
        }
    }
    else
    {
        if (info->password[0])
        {
            printf("%c: Phase B: remote password missing!\n", ch);
            status = T30_ERR_PWD_UNACCEPTABLE;
        }
    }
    if ((len = t30_get_rx_nsf(s, &v)))
    {
        printf("%c: Phase B: NSF %d bytes\n", ch, len);
        if (info->nsf_len  &&  (info->nsf_len != len  ||  memcmp(info->nsf, v, len)))
        {
            printf("%c: Phase B: remote NSF incorrect! - expected %u bytes\n", ch, (unsigned int) info->nsf_len);
        }
    }
    else
    {
        if (info->nsf_len)
        {
            printf("%c: Phase B: remote NSF missing! - expected %u bytes\n", ch, (unsigned int) info->nsf_len);
        }
    }
    if ((len = t30_get_rx_nsc(s, &v)))
    {
        printf("%c: Phase B: NSC %d bytes\n", ch, len);
        if (info->nsc_len  &&  (info->nsc_len != len  ||  memcmp(info->nsc, v, len)))
        {
            printf("%c: Phase B: remote NSC incorrect! - expected %u bytes\n", ch, (unsigned int) info->nsc_len);
        }
    }
    else
    {
        if (info->nsc_len)
        {
            printf("%c: Phase B: remote NSC missing! - expected %u bytes\n", ch, (unsigned int) info->nsc_len);
        }
    }
    if ((len = t30_get_rx_nss(s, &v)))
    {
        printf("%c: Phase B: NSS %d bytes\n", ch, len);
        if (info->nss_len  &&  (info->nss_len != len  ||  memcmp(info->nss, v, len)))
        {
            printf("%c: Phase B: remote NSS incorrect! - expected %u bytes\n", ch, (unsigned int) info->nss_len);
        }
    }
    else
    {
        if (info->nss_len)
        {
            printf("%c: Phase B: remote NSS missing! - expected %u bytes\n", ch, (unsigned int) info->nsf_len);
        }
    }

    return status;
}
/*- End of function --------------------------------------------------------*/

static int phase_d_handler(void *user_data, int result)
{
    int i;
    int ch;
    t30_state_t *s;
    char tag[20];

    i = (int) (intptr_t) user_data;
    s = chain[i].t30_state;
    ch = i + 'A';
    snprintf(tag, sizeof(tag), "%c: Phase D", ch);
    printf("%c: Phase D handler - (0x%X) %s\n", ch, result, t30_frametype(result));
    fax_log_page_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);

    if (use_receiver_not_ready)
        t30_set_receiver_not_ready(s, 3);

    if (test_local_interrupt)
    {
        if (i == 0)
        {
            printf("%c: Initiating interrupt request\n", ch);
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
                printf("%c: Accepting interrupt request\n", ch);
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

static void phase_e_handler(void *user_data, int result)
{
    int i;
    int ch;
    t30_stats_t t;
    t30_state_t *s;
    char tag[20];

    i = (int) (intptr_t) user_data;
    s = chain[i].t30_state;
    ch = i + 'A';
    snprintf(tag, sizeof(tag), "%c: Phase E", ch);
    printf("%c: Phase E handler - (%d) %s\n", ch, result, t30_completion_code_to_str(result));
    fax_log_final_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);
    t30_get_transfer_statistics(s, &t);
    chain[i].succeeded = (result == T30_ERR_OK);
    chain[i].phase_e_reached = true;
}
/*- End of function --------------------------------------------------------*/

static void real_time_t30_frame_handler(void *user_data,
                                        bool incoming,
                                        const uint8_t *msg,
                                        int len)
{
    int i;
    int ch;

    i = (intptr_t) user_data;
    ch = i + 'A';
    printf("%c: Real time frame handler - %s, %s, length = %d\n",
           ch,
           (incoming)  ?  "line->T.30"  : "T.30->line",
           t30_frametype(msg[2]),
           len);
}
/*- End of function --------------------------------------------------------*/

static int document_handler(void *user_data, int event)
{
    int i;
    int ch;

    i = (intptr_t) user_data;
    ch = i + 'A';
    printf("%c: Document handler - event %d\n", ch, event);
    return false;
}
/*- End of function --------------------------------------------------------*/

static void set_t30_callbacks(t30_state_t *t30, int chan)
{
    t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) chan);
    t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) chan);
    t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) chan);
    t30_set_real_time_frame_handler(t30, real_time_t30_frame_handler, (void *) (intptr_t) chan);
    t30_set_document_handler(t30, document_handler, (void *) (intptr_t) chan);
}
/*- End of function --------------------------------------------------------*/

static void real_time_gateway_frame_handler(void *user_data,
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
            span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d\n", chain[chan].t38_subst_seq, len);

            if (g1050_put(chain[chan].path.g1050_path, buf, len, chain[chan].t38_subst_seq, when) < 0)
                printf("Lost packet %d\n", chain[chan].t38_subst_seq);
            chain[chan].t38_subst_seq = (chain[chan].t38_subst_seq + 1) & 0xFFFF;
        }
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);

        for (i = 0;  i < count;  i++)
        {
            if (g1050_put(chain[chan].path.g1050_path, buf, len, s->tx_seq_no, when) < 0)
                printf("Lost packet %d\n", s->tx_seq_no);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void t33_tests(void)
{
    int n;
    int item_no;
    int type;
    uint8_t num[21];
    uint8_t new_t33[133];
    /* These patterns are from the T.33 spec */
    static const uint8_t *pkts[] =
    {
        (const uint8_t *) "#1234567890#1234",
        (const uint8_t *) "1234#5678#8910",
        (const uint8_t *) "#6174444100#1234#567",
        (const uint8_t *) "1234#5678##2032223",
        (const uint8_t *) "#2037445555##6446666",
        (const uint8_t *) "#2037445555#1234##6446666#5678",
        //(const uint8_t *) "#123456789012345678901#1234##6446666#5678",
        (const uint8_t *) ""
    };

    printf("T.33 sub-address packing/unpacking tests\n");
    for (n = 0;  pkts[n][0];  n++)
    {
        new_t33[0] = '\0';
        printf("'%s'\n", pkts[n]);
        for (item_no = 0;  item_no < 100;  item_no++)
        {
            if ((type = t33_sub_address_extract_field(num, pkts[n], item_no)) <= 0)
            {
                if (type == T33_NONE)
                    break;
                printf("Bad sub-address field\n");
                exit(2);
            }
            switch (type)
            {
            case T33_SST:
                printf("SST '%s'\n", num);
                t33_sub_address_add_field(new_t33, num, type);
                break;
            case T33_EXT:
                printf("    EXT '%s'\n", num);
                t33_sub_address_add_field(new_t33, num, type);
                break;
            }
        }
        if (strcmp((const char *) pkts[n], (const char *) new_t33))
        {
            printf("Re-encode mismatch '%s' '%s'\n", pkts[n], new_t33);
            exit(2);
        }
    }
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int16_t silence[SAMPLES_PER_CHUNK];
    int16_t t38_amp_hist_a[8][SAMPLES_PER_CHUNK];
    int16_t t38_amp_hist_b[8][SAMPLES_PER_CHUNK];
    int16_t audio_log[SAMPLES_PER_CHUNK*4];
    int hist_ptr;
    int log_audio;
    int msg_len;
    uint8_t msg[1024];
    int outframes;
    SNDFILE *wave_handle;
    bool use_ecm;
    bool use_tep;
    bool use_polled_mode;
    bool use_transmit_on_idle;
    bool feedback_audio;
    int t38_version;
    const char *input_tiff_file_name;
    const char *replay_file_name;
    int i;
    int j;
    int k;
    int seq_no;
    int g1050_model_no;
    int g1050_speed_pattern_no;
    int t38_transport;
    double tx_when;
    double rx_when;
    int supported_modems;
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
    int allowed_bilevel_resolutions[2];
    int allowed;
    bool remove_fill_bits;
    bool colour_enabled;
    bool t37_like_output;
    t38_stats_t t38_stats;
    t30_stats_t t30_stats;
    logging_state_t *logging;
    int expected_pages;
    char *page_header_info;
    char *page_header_tz;
    const char *xml_file_name;
    const char *xml_test_name[2];
    int xml_step;
    char buf[132 + 1];
    int line_model_no;
    int channel_codec;
    int rbs_pattern;
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
    output_tiff_file_name = OUTPUT_TIFF_FILE_NAME;
    t38_simulate_incrementing_repeats = false;
    g1050_model_no = 0;
    g1050_speed_pattern_no = 1;
    remove_fill_bits = false;
    use_tep = false;
    feedback_audio = false;
    use_transmit_on_idle = true;
    use_polled_mode = false;
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
    replay_file_name = INPUT_WAVE_FILE_NAME;
    code_to_look_up = -1;
    allowed_bilevel_resolutions[0] = 0;
    allowed_bilevel_resolutions[1] = 0;
    allowed = 0;
    line_model_no = 0;
    channel_codec = MUNGE_CODEC_NONE;
    rbs_pattern = 0;
    colour_enabled = false;
    t37_like_output = false;
    t38_transport = T38_TRANSPORT_UDPTL;
    xml_file_name = "../spandsp/tsb85.xml";
    xml_test_name[0] = "MRGN01";
    xml_test_name[1] = "MRGN01";
    xml_step = 0;
    while ((opt = getopt(argc, argv, "7b:c:Cd:D:efFgH:i:Ilm:M:n:p:Ps:S:tT:u:v:x:X:z:")) != -1)
    {
        switch (opt)
        {
        case '7':
            t37_like_output = true;
            break;
        case 'b':
            allowed_bilevel_resolutions[allowed] = atoi(optarg);
            allowed ^= 1;
            break;
        case 'c':
            code_to_look_up = atoi(optarg);
            break;
        case 'C':
            colour_enabled = true;
            break;
        case 'd':
            replay_file_name = optarg;
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
            /*
               -p FAX-audio-FAX
               -p FAX-T38-FAX
               -p FAX-audio-T38gateway-T38-T38gateway-audio-FAX
               -p FAX-T38-T38gateway-audio-T38gateway-T38-FAX
               -p FAX-T38-T38gateway-audio-FAX
               -p FAX-audio-T38gateway-T38-FAX
               -p tester-audio-FAX
               -p tester-T38-FAX
               -p tester-audio-T38gateway-T38-T38gateway-audio-FAX
               -p tester-T38-T38gateway-audio-T38gateway-T38-FAX
               -p tester-T38-T38gateway-audio-FAX
               -p tester-audio-T38gateway-T38-FAX
             */
            for (i = 0, chain_elements = 0, k = 0;  chain_elements < 7;  i++)
            {
                if (optarg[i] != '-'  &&  optarg[i] != '\0')
                    continue;
                j = optarg[i];
                optarg[i] = '\0';
                if (strcmp(&optarg[k], "FAX") == 0)
                {
                    chain[chain_elements++].node_type = AUDIO_FAX;
                }
                else if (strcmp(&optarg[k], "T38") == 0)
                {
                    chain[chain_elements++].node_type = T38_FAX;
                }
                else if (strcmp(&optarg[k], "T31") == 0)
                {
                    chain[chain_elements++].node_type = T31_AUDIO_FAX;
                }
                else if (strcmp(&optarg[k], "tester") == 0)
                {
                    chain[chain_elements++].node_type = TSB85_AUDIO_FAX;
                }
                else if (strcmp(&optarg[k], "replay") == 0)
                {
                    chain[chain_elements++].node_type = REPLAY_AUDIO_FAX;
                }
                else if (strcmp(&optarg[k], "T38gateway") == 0)
                {
                    chain[chain_elements++].node_type = AUDIO_TO_T38_GATEWAY;
                }
                else if (strcmp(&optarg[k], "passthrough") == 0)
                {
                    chain[chain_elements++].node_type = PASSTHROUGH;
                }
                else
                {
                    fprintf(stderr, "Unknown FAX path element %s\n", &optarg[k]);
                    exit(2);
                }
                k = i + 1;
                if (j == '\0')
                    break;
            }
#if 0
            if ((chain[0].node_type == AUDIO_FAX  &&  chain[chain_elements - 1].node_type != AUDIO_FAX)
                ||
                (chain[0].node_type != AUDIO_FAX  &&  chain[chain_elements - 1].node_type == AUDIO_FAX))
            {
                fprintf(stderr, "Invalid FAX path\n");
                exit(2);
            }
#endif
            break;
        case 'P':
            use_polled_mode = true;
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
        case 'x':
            xml_test_name[xml_step] = optarg;
            xml_step ^= 1;
            break;
        case 'X':
            xml_file_name = optarg;
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

    memset(t38_amp_hist_a, 0, sizeof(t38_amp_hist_a));
    memset(t38_amp_hist_b, 0, sizeof(t38_amp_hist_b));

    /* Set up the nodes */
    chain[0].peer = chain_elements - 1;
    chain[chain_elements - 1].peer = 0;

    for (i = 0;  i < chain_elements;  i++)
    {
        chain[i].tag[0] = i + 'A';
        chain[i].tag[1] = '\0';

        memset(&chain[i].audio_buf[0], 0, sizeof(chain[i].audio_buf[0]));
        memset(&chain[i].audio_buf[1], 0, sizeof(chain[i].audio_buf[1]));
        memset(&chain[i].expected_rx_info, 0, sizeof(chain[i].expected_rx_info));

        switch (chain[i].node_type)
        {
        case AUDIO_FAX:
            if ((chain[i].node.fax_state = fax_init(NULL, (i == 0))) == NULL)
            {
                fprintf(stderr, "    Cannot start FAX instance\n");
                exit(2);
            }
            chain[i].t30_state = fax_get_t30_state(chain[i].node.fax_state);

            logging = fax_get_logging_state(chain[i].node.fax_state);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            logging = fax_modems_get_logging_state(&chain[i].node.fax_state->modems);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            logging = t30_get_logging_state(chain[i].t30_state);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            set_t30_callbacks(chain[i].t30_state, i);

            chain[i].path.audio_in_buf = &chain[i + ((i == 0)  ?  1  :  -1)].audio_buf[0];
            chain[i].path.audio_out_buf = &chain[i].audio_buf[0];

            chain[i].awgn_state = NULL;
            signal_scaling = 1.0f;
            if (noise_level > -99)
            {
                chain[i].awgn_state = awgn_init_dbm0(NULL, 1234567, noise_level);
                signal_scaling = powf(10.0f, signal_level/20.0f);
                printf("Signal scaling %f\n", signal_scaling);
            }
            break;
        case T38_FAX:
            if ((chain[i].node.t38_state = t38_terminal_init(NULL, (i == 0), tx_packet_handler, (void *) (intptr_t) i)) == NULL)
            {
                fprintf(stderr, "    Cannot start the T.38 terminal instance\n");
                exit(2);
            }
            chain[i].t30_state = t38_terminal_get_t30_state(chain[i].node.t38_state);
            chain[i].t38_core_state = t38_terminal_get_t38_core_state(chain[i].node.t38_state);

            logging = t38_terminal_get_logging_state(chain[i].node.t38_state);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            logging = t38_core_get_logging_state(chain[i].t38_core_state);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            logging = t30_get_logging_state(chain[i].t30_state);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            set_t30_callbacks(chain[i].t30_state, i);

            if (i == 0)
            {
                chain[i].t38_peer = i + 1;
            }
            else
            {
                switch (chain[i - 1].node_type)
                {
                case T38_FAX:
                case AUDIO_TO_T38_GATEWAY:
                    chain[i].t38_peer = i - 1;
                    break;
                default:
                    chain[i].t38_peer = i + 1;
                    break;
                }
            }
            break;
        case T31_AUDIO_FAX:
            break;
        case T31_T38_FAX:
            break;
        case TSB85_AUDIO_FAX:
        case TSB85_T38_FAX:
            if ((chain[i].node.faxtester_state = faxtester_init(NULL, xml_file_name, xml_test_name[(i == 0)  ?  0  :  1])) == NULL)
            {
                fprintf(stderr, "    Cannot start FAX tester instance\n");
                exit(2);
            }
            logging = faxtester_get_logging_state(chain[i].node.faxtester_state);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            faxtester_set_transmit_on_idle(chain[i].node.faxtester_state, true);

            chain[i].path.audio_in_buf = &chain[i + ((i == 0)  ?  1  :  -1)].audio_buf[0];
            chain[i].path.audio_out_buf = &chain[i].audio_buf[0];

            if (i == 0)
            {
                chain[i].t38_peer = i + 1;
            }
            else
            {
                switch (chain[i - 1].node_type)
                {
                case T38_FAX:
                case AUDIO_TO_T38_GATEWAY:
                    chain[i].t38_peer = i - 1;
                    break;
                default:
                    chain[i].t38_peer = i + 1;
                    break;
                }
            }

            chain[i].awgn_state = NULL;
            signal_scaling = 1.0f;
            if (noise_level > -99)
            {
                chain[i].awgn_state = awgn_init_dbm0(NULL, 1234567, noise_level);
                signal_scaling = powf(10.0f, signal_level/20.0f);
                printf("Signal scaling %f\n", signal_scaling);
            }
            break;
        case REPLAY_AUDIO_FAX:
            if ((chain[i].node.wave_handle = sf_open_telephony_read(replay_file_name, 1)) == NULL)
            {
                fprintf(stderr, "    Cannot open audio file '%s'\n", replay_file_name);
                exit(2);
            }

            chain[i].path.audio_in_buf = &chain[i + ((i == 0)  ?  1  :  -1)].audio_buf[0];
            chain[i].path.audio_out_buf = &chain[i].audio_buf[0];
            break;
        case AUDIO_TO_T38_GATEWAY:
            if ((chain[i].node.t38_gateway_state = t38_gateway_init(NULL, tx_packet_handler, (void *) (intptr_t) i)) == NULL)
            {
                fprintf(stderr, "    Cannot start T.38 gateway instance\n");
                exit(2);
            }
            chain[i].t38_core_state = t38_gateway_get_t38_core_state(chain[i].node.t38_gateway_state);

            logging = t38_gateway_get_logging_state(chain[i].node.t38_gateway_state);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            logging = fax_modems_get_logging_state(&chain[i].node.t38_gateway_state->audio.modems);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            logging = t38_core_get_logging_state(chain[i].t38_core_state);
            span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
            span_log_set_tag(logging, chain[i].tag);

            t38_gateway_set_transmit_on_idle(chain[i].node.t38_gateway_state, use_transmit_on_idle);
            t38_gateway_set_supported_modems(chain[i].node.t38_gateway_state, supported_modems);
            //t38_gateway_set_nsx_suppression(chain[i].node.t38_state, NULL, 0, NULL, 0);
            t38_gateway_set_fill_bit_removal(chain[i].node.t38_gateway_state, remove_fill_bits);
            t38_gateway_set_real_time_frame_handler(chain[i].node.t38_gateway_state, real_time_gateway_frame_handler, (void *) (intptr_t) i);
            t38_gateway_set_ecm_capability(chain[i].node.t38_gateway_state, use_ecm);
            t38_set_t38_version(chain[i].t38_core_state, t38_version);

            if (i == 0)
            {
                chain[i].t38_peer = i + 1;
                chain[i].path.audio_in_buf = NULL;
            }
            else
            {
                switch (chain[i - 1].node_type)
                {
                case T38_FAX:
                case AUDIO_TO_T38_GATEWAY:
                    chain[i].t38_peer = i - 1;
                    chain[i].path.audio_in_buf = &chain[i + 1].audio_buf[0];
                    break;
                default:
                    chain[i].t38_peer = i + 1;
                    chain[i].path.audio_in_buf = &chain[i - 1].audio_buf[0];
                    break;
                }
            }

            chain[i].path.audio_out_buf = &chain[i].audio_buf[0];

            chain[i].awgn_state = NULL;
            signal_scaling = 1.0f;
            if (noise_level > -99)
            {
                chain[i].awgn_state = awgn_init_dbm0(NULL, 1234567, noise_level);
                signal_scaling = powf(10.0f, signal_level/20.0f);
                printf("Signal scaling %f\n", signal_scaling);
            }
        }
        if ((chain[i].path.g1050_path = g1050_init(g1050_model_no, g1050_speed_pattern_no, 100, 33)) == NULL)
        {
            fprintf(stderr, "    Failed to start IP network path model\n");
            exit(2);
        }
    }

    for (i = 0;  i < chain_elements;  i++)
    {
        j = i + 1;
        if (chain[i].t30_state)
        {
            sprintf(buf, "%d%d%d%d%d%d%d%d", j, j, j, j, j, j, j, j);
            t30_set_tx_ident(chain[i].t30_state, buf);
            strcpy(chain[chain[i].peer].expected_rx_info.ident, buf);
            sprintf(buf, "Sub-address %d", j);
            t30_set_tx_sub_address(chain[i].t30_state, buf);
            //strcpy(chain[chain[i].peer].expected_rx_info.sub_address, buf);
            sprintf(buf, "Sender ID %d", j);
            t30_set_tx_sender_ident(chain[i].t30_state, buf);
            //strcpy(chain[chain[i].peer].expected_rx_info.sender_ident, buf);
            sprintf(buf, "Password %d", j);
            t30_set_tx_password(chain[i].t30_state, buf);
            //strcpy(chain[chain[i].peer].expected_rx_info.password, buf);
            sprintf(buf, "Polled sub-add %d", j);
            t30_set_tx_polled_sub_address(chain[i].t30_state, buf);
            //strcpy(chain[chain[i].peer].expected_rx_info.polled_sub_address, buf);
            sprintf(buf, "Select poll add %d", j);
            t30_set_tx_selective_polling_address(chain[i].t30_state, buf);
            //strcpy(chain[chain[i].peer].expected_rx_info.selective_polling_address, buf);
            t30_set_tx_page_header_info(chain[i].t30_state, page_header_info);
            if (page_header_tz)
                t30_set_tx_page_header_tz(chain[i].t30_state, page_header_tz);

            if (i != 0)
            {
                t30_set_tx_nsf(chain[i].t30_state, (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
                chain[chain[i].peer].expected_rx_info.nsf = (uint8_t *) "\x50\x00\x00\x00Spandsp\x00";
                chain[chain[i].peer].expected_rx_info.nsf_len = 12;
            }

            t30_set_supported_modems(chain[i].t30_state, supported_modems);
            t30_set_supported_t30_features(chain[i].t30_state,
                                           T30_SUPPORT_IDENTIFICATION
                                         | T30_SUPPORT_SELECTIVE_POLLING
                                         | T30_SUPPORT_SUB_ADDRESSING);
            t30_set_supported_image_sizes(chain[i].t30_state,
                                          T4_SUPPORT_WIDTH_215MM
                                        | T4_SUPPORT_WIDTH_255MM
                                        | T4_SUPPORT_WIDTH_303MM
                                        | T4_SUPPORT_LENGTH_US_LETTER
                                        | T4_SUPPORT_LENGTH_US_LEGAL
                                        | T4_SUPPORT_LENGTH_UNLIMITED);
            switch (allowed_bilevel_resolutions[(i == 0)  ?  0  :  1])
            {
            case 0:
                /* Allow anything */
                t30_set_supported_bilevel_resolutions(chain[i].t30_state,
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
                t30_set_supported_bilevel_resolutions(chain[i].t30_state,
                                                      T4_RESOLUTION_R8_STANDARD
                                                    | T4_RESOLUTION_R8_FINE
                                                    | T4_RESOLUTION_R8_SUPERFINE
                                                    | T4_RESOLUTION_R16_SUPERFINE);
                break;
            case 2:
                /* Allow anything inch based */
                t30_set_supported_bilevel_resolutions(chain[i].t30_state,
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
                t30_set_supported_bilevel_resolutions(chain[i].t30_state,
                                                      T4_RESOLUTION_R8_STANDARD
                                                    | T4_RESOLUTION_R8_FINE
                                                    | T4_RESOLUTION_200_100
                                                    | T4_RESOLUTION_200_200);
                break;
            case 4:
                /* Allow only more restricted length resolution */
                t30_set_supported_bilevel_resolutions(chain[i].t30_state,
                                                      T4_RESOLUTION_R8_STANDARD
                                                    | T4_RESOLUTION_200_100);
                break;
            }
            if (colour_enabled)
            {
                t30_set_supported_colour_resolutions(chain[i].t30_state,
                                                     T4_RESOLUTION_100_100
                                                   | T4_RESOLUTION_200_200
                                                   | T4_RESOLUTION_300_300
                                                   | T4_RESOLUTION_400_400
                                                   | T4_RESOLUTION_600_600
                                                   | T4_RESOLUTION_1200_1200);
            }
            else
            {
                t30_set_supported_colour_resolutions(chain[i].t30_state, 0);
            }
            if (t37_like_output)
            {
                t30_set_supported_output_compressions(chain[i].t30_state,
                                                      T4_COMPRESSION_T85
                                                    | T4_COMPRESSION_T85_L0
                                                    | T4_COMPRESSION_T6
                                                    | T4_COMPRESSION_T42_T81);
            }
            else
            {
                t30_set_supported_output_compressions(chain[i].t30_state,
                                                      T4_COMPRESSION_T6
                                                    | T4_COMPRESSION_JPEG);
            }

            t30_set_ecm_capability(chain[i].t30_state, use_ecm);
            t30_set_supported_compressions(chain[i].t30_state,
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
            t30_set_minimum_scan_line_time(chain[i].t30_state, scan_line_time);
        }

        switch (chain[i].node_type)
        {
        case AUDIO_FAX:
            fax_set_transmit_on_idle(chain[i].node.fax_state, use_transmit_on_idle);
            fax_set_tep_mode(chain[i].node.fax_state, use_tep);
            break;
        case T38_FAX:
            t38_set_t38_version(chain[i].t38_core_state, t38_version);
            //t30_set_iaf_mode(chain[i].t30_state, T30_IAF_MODE_NO_FILL_BITS);
            switch (t38_transport)
            {
            case T38_TRANSPORT_UDPTL:
            case T38_TRANSPORT_RTP:
                t38_terminal_set_fill_bit_removal(chain[i].node.t38_state, remove_fill_bits);
                t38_terminal_set_tep_mode(chain[i].node.t38_state, use_tep);
                break;
            case T38_TRANSPORT_TCP:
            case T38_TRANSPORT_TCP_TPKT:
                t38_terminal_set_fill_bit_removal(chain[i].node.t38_state, true);
                t38_terminal_set_config(chain[i].node.t38_state, T38_TERMINAL_OPTION_NO_PACING | T38_TERMINAL_OPTION_NO_INDICATORS);
                t38_terminal_set_tep_mode(chain[i].node.t38_state, false);
                break;
            }
            break;
        }
    }

    for (i = 0;  i < chain_elements;  i++)
    {
        switch (chain[i].node_type)
        {
        case TSB85_AUDIO_FAX:
        case TSB85_T38_FAX:
            if (chain[chain[i].peer].node_type == AUDIO_FAX)
                chain[i].node.faxtester_state->far_fax = chain[chain[i].peer].node.fax_state;
            else
                chain[i].node.faxtester_state->far_t38 = chain[chain[i].peer].node.t38_state;
            chain[i].node.faxtester_state->far_t30 = chain[chain[i].peer].t30_state;
            chain[i].node.faxtester_state->far_tag = chain[i].peer + 'A';

            while (faxtester_next_step(chain[i].node.faxtester_state) == 0)
                /*dummy loop*/;
            /*endwhile*/
            break;
        case REPLAY_AUDIO_FAX:
            break;
        case PASSTHROUGH:
            if (chain[i - 1].path.audio_in_buf == &chain[i].audio_buf[0])
                chain[i - 1].path.audio_in_buf = &chain[i + 1].audio_buf[0];
            if (chain[i + 1].path.audio_in_buf == &chain[i].audio_buf[0])
                chain[i + 1].path.audio_in_buf = &chain[i - 1].audio_buf[0];
            break;
        }
    }

    switch (chain[chain_elements - 1].node_type)
    {
    case AUDIO_FAX:
    case T38_FAX:
        k = (use_polled_mode)  ?  (chain_elements - 1)  :  0;
        if (chain[k].t30_state)
            t30_set_tx_file(chain[k].t30_state, input_tiff_file_name, start_page, end_page);
        break;
    }
    switch (chain[0].node_type)
    {
    case AUDIO_FAX:
    case T38_FAX:
        k = (use_polled_mode)  ?  0  :  (chain_elements - 1);
        if (chain[k].t30_state)
            t30_set_rx_file(chain[k].t30_state, output_tiff_file_name, -1);
        break;
    }

#if defined(ENABLE_GUI)
    if (use_gui)
        start_media_monitor();
#endif
    hist_ptr = 0;
    for (;;)
    {
        memset(audio_log, 0, sizeof(audio_log));

        for (i = 0;  i < chain_elements;  i++)
        {
            /* Update T.30 timing */
            switch (chain[i].node_type)
            {
            case AUDIO_FAX:
                /* Update timing */
                logging = t30_get_logging_state(chain[i].t30_state);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
                logging = fax_get_logging_state(chain[i].node.fax_state);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
                logging = fax_modems_get_logging_state(&chain[i].node.fax_state->modems);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
#if 0
                /* Probe inside the modems to update their logs */
                span_log_bump_samples(chain[i].node.fax_state->modems.v27ter_rx.logging, len);
                span_log_bump_samples(chain[i].node.fax_state->modems.v29_rx.logging, len);
                span_log_bump_samples(chain[i].node.fax_state->modems.v17_rx.logging, len);
#endif

#if 0
                /* Mute the signal */
                vec_zeroi16(chain[i].path.audio_in_buf->amp, SAMPLES_PER_CHUNK);
                chain[i].path.audio_in_buf->len = SAMPLES_PER_CHUNK;
#endif
                if (log_audio)
                {
                    k = (i == 0)  ?  0  :  2;
                    for (j = 0;  j < chain[i].path.audio_in_buf->len;  j++)
                        audio_log[4*j + k] = chain[i].path.audio_in_buf->amp[j];
                }
                fax_rx(chain[i].node.fax_state, chain[i].path.audio_in_buf->amp, chain[i].path.audio_in_buf->len);
                if (!t30_call_active(chain[i].t30_state))
                {
                    chain[i].completed = true;
                    continue;
                }

                chain[i].path.audio_out_buf->len = fax_tx(chain[i].node.fax_state, chain[i].path.audio_out_buf->amp, SAMPLES_PER_CHUNK);
                if (!use_transmit_on_idle)
                {
                    /* The receive side always expects a full block of samples, but the
                       transmit side may not be sending any when it doesn't need to. We
                       may need to pad with some silence. */
                    if (chain[i].path.audio_out_buf->len < SAMPLES_PER_CHUNK)
                    {
                        vec_zeroi16(&chain[i].path.audio_out_buf->amp[chain[i].path.audio_out_buf->len], SAMPLES_PER_CHUNK - chain[i].path.audio_out_buf->len);
                        chain[i].path.audio_out_buf->len = SAMPLES_PER_CHUNK;
                    }
                }
                if (chain[i].awgn_state)
                {
                    for (j = 0;  j < chain[i].path.audio_out_buf->len;  j++)
                        chain[i].path.audio_out_buf->amp[j] = ((int16_t) (chain[i].path.audio_out_buf->amp[j]*signal_scaling)) + awgn(chain[i].awgn_state);
                }
                if (log_audio)
                {
                    k = (i == 0)  ?  1  :  3;
                    for (j = 0;  j < chain[i].path.audio_out_buf->len;  j++)
                        audio_log[4*j + k] = chain[i].path.audio_out_buf->amp[j];
                }
                if (feedback_audio)
                {
                    for (j = 0;  j < chain[i].path.audio_out_buf->len;  j++)
                        chain[i].path.audio_out_buf->amp[j] += t38_amp_hist_a[hist_ptr][j] >> 1;
                    memcpy(t38_amp_hist_a[hist_ptr], chain[i].path.audio_out_buf->amp, sizeof(int16_t)*SAMPLES_PER_CHUNK);
                }
                break;
            case T38_FAX:
                /* Update timing */
                logging = t30_get_logging_state(chain[i].t30_state);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
                logging = t38_terminal_get_logging_state(chain[i].node.t38_state);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
                logging = t38_core_get_logging_state(chain[i].t38_core_state);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);

                chain[i].completed = t38_terminal_send_timeout(chain[i].node.t38_state, SAMPLES_PER_CHUNK);

                while ((msg_len = g1050_get(chain[i].path.g1050_path, msg, 1024, when, &seq_no, &tx_when, &rx_when)) >= 0)
                {
#if defined(ENABLE_GUI)
                    if (use_gui)
                        media_monitor_rx(seq_no, tx_when, rx_when);
#endif
                    t38_core_rx_ifp_packet(chain[chain[i].t38_peer].t38_core_state, msg, msg_len, seq_no);
                }
                break;
            case TSB85_AUDIO_FAX:
                /* Update timing */
                logging = faxtester_get_logging_state(chain[i].node.faxtester_state);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
#if 0
                /* Probe inside the modems to update their logs */
                span_log_bump_samples(&chain[i].node.faxtester_state->modems.v27ter_rx.logging, len);
                span_log_bump_samples(&chain[i].node.faxtester_state->modems.v29_rx.logging, len);
                span_log_bump_samples(&chain[i].node.faxtester_state->modems.v17_rx.logging, len);
#endif

                if (log_audio)
                {
                    k = (i == 0)  ?  0  :  2;
                    for (j = 0;  j < chain[i].path.audio_in_buf->len;  j++)
                        audio_log[4*j + k] = chain[i].path.audio_in_buf->amp[j];
                }
                faxtester_rx(chain[i].node.faxtester_state, chain[i].path.audio_in_buf->amp, chain[i].path.audio_in_buf->len);
                chain[i].path.audio_out_buf->len = faxtester_tx(chain[i].node.faxtester_state, chain[i].path.audio_out_buf->amp, SAMPLES_PER_CHUNK);
                if (chain[i].path.audio_out_buf->len == 0)
                    break;
                if (log_audio)
                {
                    k = (i == 0)  ?  1  :  3;
                    for (j = 0;  j < chain[i].path.audio_out_buf->len;  j++)
                        audio_log[4*j + k] = chain[i].path.audio_out_buf->amp[j];
                }
                if (chain[i].node.faxtester_state->test_for_call_clear  &&  !chain[i].node.faxtester_state->far_end_cleared_call)
                {
                    chain[i].node.faxtester_state->call_clear_timer += chain[i].path.audio_out_buf->len;
                    if (!t30_call_active(chain[i].node.faxtester_state->far_t30))
                    {
                        span_log(faxtester_get_logging_state(chain[i].node.faxtester_state),
                                 SPAN_LOG_FLOW,
                                 "Far end cleared after %dms (limits %dms to %dms)\n",
                                 chain[i].node.faxtester_state->call_clear_timer/8,
                                 chain[i].node.faxtester_state->timein_x,
                                 chain[i].node.faxtester_state->timeout);
                        if (chain[i].node.faxtester_state->call_clear_timer/8 < chain[i].node.faxtester_state->timein_x  ||  chain[i].node.faxtester_state->call_clear_timer/8 > chain[i].node.faxtester_state->timeout_x)
                        {
                            printf("Test failed\n");
                            exit(2);
                        }
                        span_log(faxtester_get_logging_state(chain[i].node.faxtester_state), SPAN_LOG_FLOW, "Clear time OK\n");
                        chain[i].node.faxtester_state->far_end_cleared_call = true;
                        chain[i].node.faxtester_state->test_for_call_clear = false;
                        while (faxtester_next_step(chain[i].node.faxtester_state) == 0)
                            /*dummy loop*/;
                        /*endwhile*/
                    }
                    /*endif*/
                }
                /*endif*/
                break;
            case REPLAY_AUDIO_FAX:
                chain[i].path.audio_out_buf->len = sf_readf_short(chain[i].node.wave_handle, chain[i].path.audio_out_buf->amp, SAMPLES_PER_CHUNK);
                if (chain[i].path.audio_out_buf->len == 0)
                    break;
                break;
            case AUDIO_TO_T38_GATEWAY:
                /* Update timing */
                logging = t38_gateway_get_logging_state(chain[i].node.t38_gateway_state);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
                logging = t38_core_get_logging_state(chain[i].t38_core_state);
                span_log_bump_samples(logging, SAMPLES_PER_CHUNK);
#if 0
                /* Probe inside the modems to update their logs */
                span_log_bump_samples(&chain[i].node.t38_gateway_state->modems.v27ter_rx.logging, len);
                span_log_bump_samples(&chain[i].node.t38_gateway_state->modems.v29_rx.logging, len);
                span_log_bump_samples(&chain[i].node.t38_gateway_state->modems.v17_rx.logging, len);
#endif

                if (drop_frame_rate  &&  --drop_frame == 0)
                {
                    drop_frame = drop_frame_rate;
                    if (t38_gateway_rx_fillin(chain[i].node.t38_gateway_state, SAMPLES_PER_CHUNK))
                        break;
                }
                else
                {
                    if (t38_gateway_rx(chain[i].node.t38_gateway_state, chain[i].path.audio_in_buf->amp, chain[i].path.audio_in_buf->len))
                        break;
                }

                chain[i].path.audio_out_buf->len = t38_gateway_tx(chain[i].node.t38_gateway_state, chain[i].path.audio_out_buf->amp, SAMPLES_PER_CHUNK);
                if (!use_transmit_on_idle)
                {
                    if (chain[i].path.audio_out_buf->len < SAMPLES_PER_CHUNK)
                    {
                        vec_zeroi16(&chain[i].path.audio_out_buf->amp[chain[i].path.audio_out_buf->len], SAMPLES_PER_CHUNK - chain[i].path.audio_out_buf->len);
                        chain[i].path.audio_out_buf->len = SAMPLES_PER_CHUNK;
                    }
                }
                if (feedback_audio)
                {
                    for (j = 0;  j < chain[i].path.audio_out_buf->len;  j++)
                        chain[i].path.audio_out_buf->amp[j] += t38_amp_hist_a[hist_ptr][j] >> 1;
                    vec_movei16(t38_amp_hist_a[hist_ptr], chain[i].path.audio_out_buf->amp, SAMPLES_PER_CHUNK);
                }

#if 0
                if (log_audio)
                {
                    k = (i == 0)  ?  1  :  3;
                    for (j = 0;  j < chain[i].path.audio_out_buf->len;  j++)
                        audio_log[4*j + k] = chain[i].path.audio_out_buf->amp[j];
                }
#endif
                while ((msg_len = g1050_get(chain[i].path.g1050_path, msg, 1024, when, &seq_no, &tx_when, &rx_when)) >= 0)
                {
#if defined(ENABLE_GUI)
                    if (use_gui)
                        media_monitor_rx(seq_no, tx_when, rx_when);
#endif
                    t38_core_rx_ifp_packet(chain[chain[i].t38_peer].t38_core_state, msg, msg_len, seq_no);
                }
                break;
            }
        }
        if (log_audio)
        {
            outframes = sf_writef_short(wave_handle, audio_log, SAMPLES_PER_CHUNK);
            if (outframes != SAMPLES_PER_CHUNK)
                break;
        }

        when += (float) SAMPLES_PER_CHUNK/(float) SAMPLE_RATE;

        if (chain[0].completed  &&  chain[chain_elements - 1].completed)
            break;
#if defined(ENABLE_GUI)
        if (use_gui)
            media_monitor_update_display();
#endif
        if (++hist_ptr > 3)
            hist_ptr = 0;
    }

    for (i = 0;  i < chain_elements;  i++)
    {
        switch (chain[i].node_type)
        {
        case AUDIO_TO_T38_GATEWAY:
            t38_gateway_get_transfer_statistics(chain[i].node.t38_gateway_state, &t38_stats);
            printf("%c side exchanged %d pages at %dbps, in %s mode\n",
                   i + 'A',
                   t38_stats.pages_transferred,
                   t38_stats.bit_rate,
                   (t38_stats.error_correcting_mode)  ?  "ECM"  :  "non-ECM");
            break;
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
    for (j = 0;  j < 2;  j++)
    {
        i = (j == 0)  ?  0  :  (chain_elements - 1);
        if (!chain[i].phase_e_reached)
            break;
        if (!chain[i].succeeded)
            break;

        t30_get_transfer_statistics(chain[i].t30_state, &t30_stats);
        if ((!use_polled_mode  &&  i != 0)  ||  (use_polled_mode  &&  i == 0))
        {
            if (t30_stats.pages_tx != 0  ||  t30_stats.pages_rx != expected_pages)
                break;
        }
        else
        {
            if (t30_stats.pages_tx != expected_pages  ||  t30_stats.pages_rx != 0)
                break;
        }
    }
    for (i = 0;  i < chain_elements;  i++)
    {
        switch (chain[i].node_type)
        {
        case AUDIO_FAX:
            fax_free(chain[i].node.fax_state);
            break;
        case T38_FAX:
            t38_terminal_free(chain[i].node.t38_state);
            break;
        case TSB85_AUDIO_FAX:
        case TSB85_T38_FAX:
            faxtester_free(chain[i].node.faxtester_state);
            break;
        case REPLAY_AUDIO_FAX:
            if (sf_close_telephony(chain[i].node.wave_handle))
            {
                fprintf(stderr, "    Cannot close audio file '%s'\n", replay_file_name);
                exit(2);
            }
            chain[i].node.wave_handle = NULL;
            break;
        case AUDIO_TO_T38_GATEWAY:
            t38_gateway_free(chain[i].node.t38_gateway_state);
            break;
        }
        if (chain[i].path.g1050_path)
        {
            g1050_free(chain[i].path.g1050_path);
            chain[i].path.g1050_path = NULL;
        }
    }
    if (j < 2)
    {
        printf("Tests failed\n");
        exit(2);
    }
    t33_tests();
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
