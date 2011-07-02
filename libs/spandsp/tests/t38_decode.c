/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_decode.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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
 * Some code from SIPP (http://sf.net/projects/sipp) was used as a model
 * for how to work with PCAP files. That code was authored by Guillaume
 * TEISSIER from FTR&D 02/02/2006, and released under the GPL2 licence.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "udptl.h"
#include "spandsp.h"

#include "fax_utils.h"
#include "pcap_parse.h"

#define INPUT_FILE_NAME         "t38.pcap"
#define OUTPUT_FILE_NAME        "t38pcap.tif"

t38_terminal_state_t *t38_state;
struct timeval now;

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
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int timing_update(void *user_data, struct timeval *ts)
{
    t30_state_t *t30;
    t38_core_state_t *t38_core;
    logging_state_t *logging;
    int samples;
    int partial;
    static int64_t current = 0;
    int64_t when;
    int64_t diff;

    memcpy(&now, ts, sizeof(now));

    when = ts->tv_sec*1000000LL + ts->tv_usec;
    if (current == 0)
        current = when;

    diff = when - current;
    samples = diff/125LL;
    while (samples > 0)
    {
        partial = (samples > 160)  ?  160  :  samples;
        //fprintf(stderr, "Update time by %d samples\n", partial);
        logging = t38_terminal_get_logging_state(t38_state);
        span_log_bump_samples(logging, partial);
        t38_core = t38_terminal_get_t38_core_state(t38_state);
        logging = t38_core_get_logging_state(t38_core);
        span_log_bump_samples(logging, partial);
        t30 = t38_terminal_get_t30_state(t38_state);
        logging = t30_get_logging_state(t30);
        span_log_bump_samples(logging, partial);
    
        t38_terminal_send_timeout(t38_state, partial);
        current = when;
        samples -= partial;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int ifp_handler(void *user_data, const uint8_t msg[], int len, int seq_no)
{
    t38_core_state_t *t38_core;
    int i;
    
    printf("%5d >>> ", seq_no);
    for (i = 0;  i < len;  i++)
        printf("%02X ", msg[i]);
    printf("\n");

    t38_core = t38_terminal_get_t38_core_state(t38_state);
    t38_core_rx_ifp_packet(t38_core, msg, len, seq_no);

    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_packet(void *user_data, const uint8_t *pkt, int len)
{
    static udptl_state_t *state = NULL;

    if (state == NULL)
        state = udptl_init(NULL, UDPTL_ERROR_CORRECTION_REDUNDANCY, 3, 3, ifp_handler, NULL);

    udptl_rx_packet(state, pkt, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    t30_state_t *t30;
    t38_core_state_t *t38_core;
    logging_state_t *logging;
    const char *input_file_name;
    int t38_version;
    int use_ecm;
    int use_tep;
    int options;
    int supported_modems;
    int fill_removal;
    int opt;
    uint32_t src_addr;
    uint16_t src_port;
    uint32_t dest_addr;
    uint16_t dest_port;


    use_ecm = FALSE;
    t38_version = 1;
    options = 0;
    input_file_name = INPUT_FILE_NAME;
    fill_removal = FALSE;
    use_tep = FALSE;
    supported_modems = T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17;
    src_addr = 0;
    src_port = 0;
    dest_addr = 0;
    dest_port = 0;
    while ((opt = getopt(argc, argv, "D:d:eFi:m:oS:s:tv:")) != -1)
    {
        switch (opt)
        {
        case 'D':
            dest_addr = atoi(optarg);
            break;
        case 'd':
            dest_port = atoi(optarg);
            break;
        case 'e':
            use_ecm = TRUE;
            break;
        case 'F':
            fill_removal = TRUE;
            break;
        case 'i':
            input_file_name = optarg;
            break;
        case 'm':
            supported_modems = atoi(optarg);
            break;
        case 'o':
            options = atoi(optarg);
            break;
        case 'S':
            src_addr = atoi(optarg);
            break;
        case 's':
            src_port = atoi(optarg);
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

    if ((t38_state = t38_terminal_init(NULL, FALSE, tx_packet_handler, NULL)) == NULL)
    {
        fprintf(stderr, "Cannot start the T.38 channel\n");
        exit(2);
    }
    t30 = t38_terminal_get_t30_state(t38_state);
    t38_core = t38_terminal_get_t38_core_state(t38_state);
    t38_set_t38_version(t38_core, t38_version);
    t38_terminal_set_config(t38_state, options);
    t38_terminal_set_tep_mode(t38_state, use_tep);
    
    logging = t38_terminal_get_logging_state(t38_state);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.38");

    logging = t38_core_get_logging_state(t38_core);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.38");

    logging = t30_get_logging_state(t30);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(logging, "T.38");

    t30_set_supported_modems(t30, supported_modems);
    t30_set_tx_ident(t30, "11111111");
    t30_set_tx_nsf(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
    t30_set_rx_file(t30, OUTPUT_FILE_NAME, -1);
    t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) 'A');
    t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) 'A');
    t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) 'A');
    t30_set_ecm_capability(t30, TRUE);
    t30_set_supported_compressions(t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION | T30_SUPPORT_T85_COMPRESSION);

    if (pcap_scan_pkts(input_file_name, src_addr, src_port, dest_addr, dest_port, timing_update, process_packet, NULL))
        exit(2);
    /* Push the time along, to flush out any remaining activity from the application. */
    now.tv_sec += 60;
    timing_update(NULL, &now);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
