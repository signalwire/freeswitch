/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_decode.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2010 Steve Underwood
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
#include "spandsp-sim.h"

#include "fax_utils.h"
#include "pcap_parse.h"

#define INPUT_FILE_NAME         "t38.pcap"
#define INPUT_TIFF_FILE_NAME    "../test-data/itu/fax/itutests.tif"
#define OUTPUT_TIFF_FILE_NAME   "t38pcap.tif"

#define OUTPUT_WAVE_FILE_NAME   "t38_decode2.wav"

#define SAMPLES_PER_CHUNK       160

static t38_core_state_t *t38_core;
static t38_terminal_state_t *t38_terminal_state;
static t38_gateway_state_t *t38_gateway_state;
static fax_state_t *fax_state;
static struct timeval now;
static SNDFILE *wave_handle;

static int log_audio;
static int use_transmit_on_idle;
static int done = false;

static int started = false;
static int64_t current = 0;

static int phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    char tag[20];

    i = (int) (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase B", i);
    printf("%c: Phase B handler on channel %c - (0x%X) %s\n", i, i, result, t30_frametype(result));
    fax_log_rx_parameters(s, tag);
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
    fax_log_page_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);
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
    fax_log_final_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);
    t30_get_transfer_statistics(s, &t);
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int t38_terminal_timing_update(void *user_data, struct timeval *ts)
{
    t30_state_t *t30;
    logging_state_t *logging;
    int samples;
    int partial;
    int64_t when;
    int64_t diff;

    memcpy(&now, ts, sizeof(now));

    when = ts->tv_sec*1000000LL + ts->tv_usec;
    if (current == 0)
    {
        if (started)
            current = when;
        return 0;
    }

    diff = when - current;
    samples = diff/125LL;
    while (samples > 0)
    {
        partial = (samples > SAMPLES_PER_CHUNK)  ?  SAMPLES_PER_CHUNK  :  samples;
        //fprintf(stderr, "Update time by %d samples\n", partial);
        logging = t38_terminal_get_logging_state(t38_terminal_state);
        span_log_bump_samples(logging, partial);
        logging = t38_core_get_logging_state(t38_core);
        span_log_bump_samples(logging, partial);
        t30 = t38_terminal_get_t30_state(t38_terminal_state);
        logging = t30_get_logging_state(t30);
        span_log_bump_samples(logging, partial);

        t38_terminal_send_timeout(t38_terminal_state, partial);
        current = when;
        samples -= partial;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int t38_gateway_timing_update(void *user_data, struct timeval *ts)
{
    t30_state_t *t30;
    logging_state_t *logging;
    int samples;
    int partial;
    int64_t when;
    int64_t diff;
    int16_t t38_amp[SAMPLES_PER_CHUNK];
    int16_t t30_amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int t38_len;
    int t30_len;
    int outframes;
    int i;

    memcpy(&now, ts, sizeof(now));

    when = ts->tv_sec*1000000LL + ts->tv_usec;
    if (current == 0)
    {
        if (started)
            current = when;
        return 0;
    }

    diff = when - current;
    samples = diff/125LL;
    while (samples > 0)
    {
        partial = (samples > SAMPLES_PER_CHUNK)  ?  SAMPLES_PER_CHUNK  :  samples;
        //fprintf(stderr, "Update time by %d samples\n", partial);
        logging = t38_gateway_get_logging_state(t38_gateway_state);
        span_log_bump_samples(logging, partial);
        logging = t38_core_get_logging_state(t38_core);
        span_log_bump_samples(logging, partial);
        logging = fax_get_logging_state(fax_state);
        span_log_bump_samples(logging, partial);
        t30 = fax_get_t30_state(fax_state);
        logging = t30_get_logging_state(t30);
        span_log_bump_samples(logging, partial);

        memset(out_amp, 0, sizeof(out_amp));

        t30_len = fax_tx(fax_state, t30_amp, partial);
        if (!use_transmit_on_idle)
        {
            /* The receive side always expects a full block of samples, but the
               transmit side may not be sending any when it doesn't need to. We
               may need to pad with some silence. */
            if (t30_len < partial)
            {
                memset(t30_amp + t30_len, 0, sizeof(int16_t)*(partial - t30_len));
                t30_len = partial;
            }
        }
        if (log_audio)
        {
            for (i = 0;  i < t30_len;  i++)
                out_amp[2*i + 1] = t30_amp[i];
        }
        if (t38_gateway_rx(t38_gateway_state, t30_amp, t30_len))
            break;

        t38_len = t38_gateway_tx(t38_gateway_state, t38_amp, partial);
        if (!use_transmit_on_idle)
        {
            if (t38_len < partial)
            {
                memset(t38_amp + t38_len, 0, sizeof(int16_t)*(partial - t38_len));
                t38_len = partial;
            }
        }
        if (log_audio)
        {
            for (i = 0;  i < t38_len;  i++)
                out_amp[2*i] = t38_amp[i];
        }
        if (fax_rx(fax_state, t38_amp, partial))
            break;

        if (log_audio)
        {
            outframes = sf_writef_short(wave_handle, out_amp, partial);
            if (outframes != partial)
                break;
        }

        if (done)
            break;

        current = when;
        samples -= partial;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int ifp_handler(void *user_data, const uint8_t msg[], int len, int seq_no)
{
    int i;

    started = true;

    printf("%5d >>> ", seq_no);
    for (i = 0;  i < len;  i++)
        printf("%02X ", msg[i]);
    printf("\n");

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

static uint32_t parse_inet_addr(const char *s)
{
    int i;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;

    a = 0;
    b = 0;
    c = 0;
    d = 0;
    i = sscanf(s, "%" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32, &a, &b, &c, &d);
    switch (i)
    {
    case 4:
        c = (c << 8) | d;
    case 3:
        b = (b << 16) | c;
    case 2:
        a = (a << 24) | b;
    }
    return a;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    t30_state_t *t30;
    logging_state_t *logging;
    const char *input_file_name;
    int t38_version;
    int caller;
    int use_ecm;
    int use_tep;
    int options;
    int supported_modems;
    int fill_removal;
    int opt;
    int t38_terminal_operation;
    uint32_t src_addr;
    uint16_t src_port;
    uint32_t dest_addr;
    uint16_t dest_port;

    caller = false;
    use_ecm = false;
    t38_version = 0;
    options = 0;
    input_file_name = INPUT_FILE_NAME;
    fill_removal = false;
    use_tep = false;
    use_transmit_on_idle = true;
    supported_modems = T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17;
    t38_terminal_operation = true;
    log_audio = false;
    src_addr = 0;
    src_port = 0;
    dest_addr = 0;
    dest_port = 0;
    while ((opt = getopt(argc, argv, "cD:d:eFGi:lm:oS:s:tv:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            caller = true;
            break;
        case 'D':
            dest_addr = parse_inet_addr(optarg);
            break;
        case 'd':
            dest_port = atoi(optarg);
            break;
        case 'e':
            use_ecm = true;
            break;
        case 'F':
            fill_removal = true;
            break;
        case 'G':
            t38_terminal_operation = false;
            break;
        case 'i':
            input_file_name = optarg;
            break;
        case 'l':
            log_audio = true;
            break;
        case 'm':
            supported_modems = atoi(optarg);
            break;
        case 'o':
            options = atoi(optarg);
            break;
        case 'S':
            src_addr = parse_inet_addr(optarg);
            break;
        case 's':
            src_port = atoi(optarg);
            break;
        case 't':
            use_tep = true;
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

    if (t38_terminal_operation)
    {
        if ((t38_terminal_state = t38_terminal_init(NULL, caller, tx_packet_handler, NULL)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 channel\n");
            exit(2);
        }
        t30 = t38_terminal_get_t30_state(t38_terminal_state);
        t38_core = t38_terminal_get_t38_core_state(t38_terminal_state);
        t38_set_t38_version(t38_core, t38_version);
        t38_terminal_set_config(t38_terminal_state, options);
        t38_terminal_set_tep_mode(t38_terminal_state, use_tep);
        t38_terminal_set_fill_bit_removal(t38_terminal_state, fill_removal);

        logging = t38_terminal_get_logging_state(t38_terminal_state);
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
        if (caller)
            t30_set_tx_file(t30, INPUT_TIFF_FILE_NAME, -1, -1);
        else
            t30_set_rx_file(t30, OUTPUT_TIFF_FILE_NAME, -1);
        t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) 'A');
        t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) 'A');
        t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) 'A');
        t30_set_ecm_capability(t30, use_ecm);
        t30_set_supported_compressions(t30, T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6 | T4_COMPRESSION_T85);

        if (pcap_scan_pkts(input_file_name, src_addr, src_port, dest_addr, dest_port, t38_terminal_timing_update, process_packet, NULL))
            exit(2);
        /* Push the time along, to flush out any remaining activity from the application. */
        now.tv_sec += 60;
        t38_terminal_timing_update(NULL, &now);
    }
    else
    {
        wave_handle = NULL;
        if (log_audio)
        {
            if ((wave_handle = sf_open_telephony_write(OUTPUT_WAVE_FILE_NAME, 2)) == NULL)
            {
                fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
                exit(2);
            }
        }

        if ((t38_gateway_state = t38_gateway_init(NULL, tx_packet_handler, NULL)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 channel\n");
            exit(2);
        }
        t38_core = t38_gateway_get_t38_core_state(t38_gateway_state);
        t38_gateway_set_transmit_on_idle(t38_gateway_state, use_transmit_on_idle);
        t38_set_t38_version(t38_core, t38_version);
        t38_gateway_set_ecm_capability(t38_gateway_state, use_ecm);

        logging = t38_gateway_get_logging_state(t38_gateway_state);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
        span_log_set_tag(logging, "T.38");

        logging = t38_core_get_logging_state(t38_core);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
        span_log_set_tag(logging, "T.38");

        if ((fax_state = fax_init(NULL, caller)) == NULL)
        {
            fprintf(stderr, "Cannot start FAX\n");
            exit(2);
        }
        t30 = fax_get_t30_state(fax_state);
        fax_set_transmit_on_idle(fax_state, use_transmit_on_idle);
        fax_set_tep_mode(fax_state, use_tep);
        t30_set_supported_modems(t30, supported_modems);
        t30_set_tx_ident(t30, "22222222");
        t30_set_tx_nsf(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
        if (caller)
            t30_set_tx_file(t30, INPUT_TIFF_FILE_NAME, -1, -1);
        else
            t30_set_rx_file(t30, OUTPUT_TIFF_FILE_NAME, -1);
        t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) 'B');
        t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) 'B');
        t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) 'B');
        t30_set_ecm_capability(t30, use_ecm);
        t30_set_supported_compressions(t30, T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6);

        logging = fax_get_logging_state(fax_state);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
        span_log_set_tag(logging, "FAX ");

        logging = t30_get_logging_state(t30);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
        span_log_set_tag(logging, "FAX ");

        if (pcap_scan_pkts(input_file_name, src_addr, src_port, dest_addr, dest_port, t38_gateway_timing_update, process_packet, NULL))
            exit(2);
        /* Push the time along, to flush out any remaining activity from the application. */
        now.tv_sec += 60;
        t38_gateway_timing_update(NULL, &now);

        fax_release(fax_state);
        t38_gateway_release(t38_gateway_state);
        if (log_audio)
        {
            if (sf_close_telephony(wave_handle))
            {
                fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
                exit(2);
            }
        }
    }
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
