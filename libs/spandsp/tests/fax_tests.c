/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
 * $Id: fax_tests.c,v 1.96 2008/09/09 14:05:55 steveu Exp $
 */

/*! \page fax_tests_page FAX tests
\section fax_tests_page_sec_1 What does it do?
\section fax_tests_page_sec_2 How does it work?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <audiofile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define SAMPLES_PER_CHUNK       160

#define INPUT_TIFF_FILE_NAME    "../test-data/itu/fax/itutests.tif"

#define OUTPUT_FILE_NAME_WAVE   "fax_tests.wav"

#define FAX_MACHINES            2

struct machine_s
{
    int chan;
    int16_t amp[SAMPLES_PER_CHUNK];
    int len;
    fax_state_t fax;
    int done;
    int succeeded;
    char tag[50];
    int error_delay;
    int total_audio_time;
} machines[FAX_MACHINES];

int use_receiver_not_ready = FALSE;
int test_local_interrupt = FALSE;
int t30_state_to_wreck = -1;

static int phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    const char *u;
    
    i = (intptr_t) user_data;
    if ((u = t30_get_rx_ident(s)))
        printf("%d: Phase B: remote ident '%s'\n", i, u);
    if ((u = t30_get_rx_sub_address(s)))
        printf("%d: Phase B: remote sub-address '%s'\n", i, u);
    if ((u = t30_get_rx_polled_sub_address(s)))
        printf("%d: Phase B: remote polled sub-address '%s'\n", i, u);
    if ((u = t30_get_rx_selective_polling_address(s)))
        printf("%d: Phase B: remote selective polling address '%s'\n", i, u);
    if ((u = t30_get_rx_sender_ident(s)))
        printf("%d: Phase B: remote sender ident '%s'\n", i, u);
    if ((u = t30_get_rx_password(s)))
        printf("%d: Phase B: remote password '%s'\n", i, u);
    printf("%d: Phase B handler on channel %d - (0x%X) %s\n", i, i, result, t30_frametype(result));
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static int phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    const char *u;

    i = (intptr_t) user_data;
    t30_get_transfer_statistics(s, &t);

    printf("%d: Phase D handler on channel %d - (0x%X) %s\n", i, i, result, t30_frametype(result));
    printf("%d: Phase D: bit rate %d\n", i, t.bit_rate);
    printf("%d: Phase D: ECM %s\n", i, (t.error_correcting_mode)  ?  "on"  :  "off");
    printf("%d: Phase D: pages transferred %d\n", i, t.pages_transferred);
    printf("%d: Phase D: pages in the file %d\n", i, t.pages_in_file);
    printf("%d: Phase D: image size %d x %d\n", i, t.width, t.length);
    printf("%d: Phase D: image resolution %d x %d\n", i, t.x_resolution, t.y_resolution);
    printf("%d: Phase D: bad rows %d\n", i, t.bad_rows);
    printf("%d: Phase D: longest bad row run %d\n", i, t.longest_bad_row_run);
    printf("%d: Phase D: compression type %d\n", i, t.encoding);
    printf("%d: Phase D: image size %d bytes\n", i, t.image_size);
    if ((u = t30_get_tx_ident(s)))
        printf("%d: Phase D: local ident '%s'\n", i, u);
    if ((u = t30_get_rx_ident(s)))
        printf("%d: Phase D: remote ident '%s'\n", i, u);
    printf("%d: Phase D: bits per row - min %d, max %d\n", i, s->t4.min_row_bits, s->t4.max_row_bits);

    if (use_receiver_not_ready)
        t30_set_receiver_not_ready(s, 3);

    if (test_local_interrupt)
    {
        if (i == 0)
        {
            printf("%d: Initiating interrupt request\n", i);
            t30_local_interrupt_request(s, TRUE);
        }
        else
        {
            switch (result)
            {
            case T30_PIP:
            case T30_PRI_MPS:
            case T30_PRI_EOM:
            case T30_PRI_EOP:
                printf("%d: Accepting interrupt request\n", i);
                t30_local_interrupt_request(s, TRUE);
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
    const char *u;
    
    i = (intptr_t) user_data;
    printf("%d: Phase E handler on channel %d - (%d) %s\n", i, i, result, t30_completion_code_to_str(result));    
    t30_get_transfer_statistics(s, &t);
    printf("%d: Phase E: bit rate %d\n", i, t.bit_rate);
    printf("%d: Phase E: ECM %s\n", i, (t.error_correcting_mode)  ?  "on"  :  "off");
    printf("%d: Phase E: pages transferred %d\n", i, t.pages_transferred);
    printf("%d: Phase E: pages in the file %d\n", i, t.pages_in_file);
    printf("%d: Phase E: image size %d x %d\n", i, t.width, t.length);
    printf("%d: Phase E: image resolution %d x %d\n", i, t.x_resolution, t.y_resolution);
    printf("%d: Phase E: bad rows %d\n", i, t.bad_rows);
    printf("%d: Phase E: longest bad row run %d\n", i, t.longest_bad_row_run);
    printf("%d: Phase E: coding method %s\n", i, t4_encoding_to_str(t.encoding));
    printf("%d: Phase E: image size %d bytes\n", i, t.image_size);
    if ((u = t30_get_tx_ident(s)))
        printf("%d: Phase E: local ident '%s'\n", i, u);
    if ((u = t30_get_rx_ident(s)))
        printf("%d: Phase E: remote ident '%s'\n", i, u);
    if ((u = t30_get_rx_country(s)))
        printf("%d: Phase E: Remote was made in '%s'\n", i, u);
    if ((u = t30_get_rx_vendor(s)))
        printf("%d: Phase E: Remote was made by '%s'\n", i, u);
    if ((u = t30_get_rx_model(s)))
        printf("%d: Phase E: Remote is model '%s'\n", i, u);
    machines[i].succeeded = (result == T30_ERR_OK)  &&  (t.pages_transferred == 12);
    machines[i].done = TRUE;
}
/*- End of function --------------------------------------------------------*/

static void real_time_frame_handler(t30_state_t *s,
                                    void *user_data,
                                    int direction,
                                    const uint8_t *msg,
                                    int len)
{
    int i;
    
    i = (intptr_t) user_data;
    printf("%d: Real time frame handler on channel %d - %s, %s, length = %d\n",
           i,
           i,
           (direction)  ?  "line->T.30"  : "T.30->line",
           t30_frametype(msg[2]),
           len);
}
/*- End of function --------------------------------------------------------*/

static int document_handler(t30_state_t *s, void *user_data, int event)
{
    int i;
    
    i = (intptr_t) user_data;
    printf("%d: Document handler on channel %d - event %d\n", i, i, event);
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    AFfilehandle wave_handle;
    AFfilehandle input_wave_handle;
    int i;
    int j;
    int k;
    struct machine_s *mc;
    int outframes;
    char buf[128 + 1];
    int16_t silence[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int alldone;
    const char *input_tiff_file_name;
    const char *input_audio_file_name;
    int log_audio;
    int use_ecm;
    int use_tep;
    int use_transmit_on_idle;
    int use_line_hits;
    int polled_mode;
    int reverse_flow;
    int use_page_limits;
    int supported_modems;
    time_t start_time;
    time_t end_time;
    char *page_header_info;
    int opt;
    t30_state_t *t30;

    log_audio = FALSE;
    input_tiff_file_name = INPUT_TIFF_FILE_NAME;
    input_audio_file_name = NULL;
    use_ecm = FALSE;
    use_line_hits = FALSE;
    use_tep = FALSE;
    polled_mode = FALSE;
    page_header_info = NULL;
    reverse_flow = FALSE;
    use_transmit_on_idle = TRUE;
    use_receiver_not_ready = FALSE;
    use_page_limits = FALSE;
    supported_modems = T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17;
    while ((opt = getopt(argc, argv, "ehH:i:I:lm:prRtTw:")) != -1)
    {
        switch (opt)
        {
        case 'e':
            use_ecm = TRUE;
            break;
        case 'h':
            use_line_hits = TRUE;
            break;
        case 'H':
            page_header_info = optarg;
            break;
        case 'i':
            input_tiff_file_name = optarg;
            break;
        case 'I':
            input_audio_file_name = optarg;
            break;
        case 'l':
            log_audio = TRUE;
            break;
        case 'm':
            supported_modems = atoi(optarg);
            break;
        case 'p':
            polled_mode = TRUE;
            break;
        case 'r':
            reverse_flow = TRUE;
            break;
        case 'R':
            use_receiver_not_ready = TRUE;
            break;
        case 't':
            use_tep = TRUE;
            break;
        case 'T':
            use_page_limits = TRUE;
            break;
        case 'w':
            t30_state_to_wreck = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    input_wave_handle = AF_NULL_FILEHANDLE;
    if (input_audio_file_name)
    {
        if ((input_wave_handle = afOpenFile_telephony_read(input_audio_file_name, 1)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open wave file '%s'\n", input_audio_file_name);
            exit(2);
        }
    }

    wave_handle = AF_NULL_FILEHANDLE;
    if (log_audio)
    {
        if ((wave_handle = afOpenFile_telephony_write(OUTPUT_FILE_NAME_WAVE, 2)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME_WAVE);
            exit(2);
        }
    }

    memset(silence, 0, sizeof(silence));
    for (j = 0;  j < FAX_MACHINES;  j++)
    {
        machines[j].chan = j;
        mc = &machines[j];

        i = mc->chan + 1;
        sprintf(buf, "%d%d%d%d%d%d%d%d", i, i, i, i, i, i, i, i);
        if (reverse_flow)
            fax_init(&mc->fax, (mc->chan & 1)  ?  TRUE  :  FALSE);
        else
            fax_init(&mc->fax, (mc->chan & 1)  ?  FALSE  :  TRUE);
        fax_set_transmit_on_idle(&mc->fax, use_transmit_on_idle);
        fax_set_tep_mode(&mc->fax, use_tep);
        t30 = fax_get_t30_state(&mc->fax);
        t30_set_tx_ident(t30, buf);
        t30_set_tx_sub_address(t30, "Sub-address");
        t30_set_tx_sender_ident(t30, "Sender ID");
        t30_set_tx_password(t30, "Password");
        t30_set_tx_polled_sub_address(t30, "Polled sub-address");
        t30_set_tx_selective_polling_address(t30, "Selective polling address");
        t30_set_tx_page_header_info(t30, page_header_info);
        t30_set_tx_nsf(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
        t30_set_ecm_capability(t30, use_ecm);
        t30_set_supported_t30_features(t30,
                                       T30_SUPPORT_IDENTIFICATION
                                     | T30_SUPPORT_SELECTIVE_POLLING
                                     | T30_SUPPORT_SUB_ADDRESSING);

        if ((mc->chan & 1))
            t30_set_minimum_scan_line_time(t30, 40);
        t30_set_supported_image_sizes(t30,
                                      T30_SUPPORT_US_LETTER_LENGTH
                                    | T30_SUPPORT_US_LEGAL_LENGTH
                                    | T30_SUPPORT_UNLIMITED_LENGTH
                                    | T30_SUPPORT_215MM_WIDTH
                                    | T30_SUPPORT_255MM_WIDTH
                                    | T30_SUPPORT_303MM_WIDTH);
        t30_set_supported_resolutions(t30,
                                      T30_SUPPORT_STANDARD_RESOLUTION
                                    | T30_SUPPORT_FINE_RESOLUTION
                                    | T30_SUPPORT_SUPERFINE_RESOLUTION
                                    | T30_SUPPORT_R8_RESOLUTION
                                    | T30_SUPPORT_R16_RESOLUTION
                                    | T30_SUPPORT_300_300_RESOLUTION
                                    | T30_SUPPORT_400_400_RESOLUTION
                                    | T30_SUPPORT_600_600_RESOLUTION
                                    | T30_SUPPORT_1200_1200_RESOLUTION
                                    | T30_SUPPORT_300_600_RESOLUTION
                                    | T30_SUPPORT_400_800_RESOLUTION
                                    | T30_SUPPORT_600_1200_RESOLUTION);
        t30_set_supported_modems(t30, supported_modems);
        if (use_ecm)
            t30_set_supported_compressions(t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);
        if ((mc->chan & 1))
        {
            if (polled_mode)
            {
                if (use_page_limits)
                    t30_set_tx_file(t30, input_tiff_file_name, 3, 6);
                else
                    t30_set_tx_file(t30, input_tiff_file_name, -1, -1);
            }
            else
            {
                sprintf(buf, "fax_tests_%d.tif", (mc->chan + 1)/2);
                t30_set_rx_file(t30, buf, -1);
            }
        }
        else
        {
            if (polled_mode)
            {
                sprintf(buf, "fax_tests_%d.tif", (mc->chan + 1)/2);
                t30_set_rx_file(t30, buf, -1);
            }
            else
            {
                if (use_page_limits)
                    t30_set_tx_file(t30, input_tiff_file_name, 3, 6);
                else
                    t30_set_tx_file(t30, input_tiff_file_name, -1, -1);
            }
        }
        t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) mc->chan);
        t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) mc->chan);
        t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) mc->chan);
        t30_set_real_time_frame_handler(t30, real_time_frame_handler, (void *) (intptr_t) mc->chan);
        t30_set_document_handler(t30, document_handler, (void *) (intptr_t) mc->chan);
        sprintf(mc->tag, "FAX-%d", j + 1);
        span_log_set_level(&t30->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        span_log_set_tag(&t30->logging, mc->tag);
        span_log_set_level(&mc->fax.modems.v29_rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        span_log_set_tag(&mc->fax.modems.v29_rx.logging, mc->tag);
        span_log_set_level(&mc->fax.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        span_log_set_tag(&mc->fax.logging, mc->tag);
        memset(mc->amp, 0, sizeof(mc->amp));
        mc->total_audio_time = 0;
        mc->done = FALSE;
    }
    time(&start_time);
    for (;;)
    {
        alldone = TRUE;
        for (j = 0;  j < FAX_MACHINES;  j++)
        {
            mc = &machines[j];

            if ((j & 1) == 0  &&  input_audio_file_name)
            {
                mc->len = afReadFrames(input_wave_handle, AF_DEFAULT_TRACK, mc->amp, SAMPLES_PER_CHUNK);
                if (mc->len == 0)
                    break;
            }
            else
            {
                mc->len = fax_tx(&mc->fax, mc->amp, SAMPLES_PER_CHUNK);
            }
            mc->total_audio_time += SAMPLES_PER_CHUNK;
            if (!use_transmit_on_idle)
            {
                /* The receive side always expects a full block of samples, but the
                   transmit side may not be sending any when it doesn't need to. We
                   may need to pad with some silence. */
                if (mc->len < SAMPLES_PER_CHUNK)
                {
                    memset(mc->amp + mc->len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - mc->len));
                    mc->len = SAMPLES_PER_CHUNK;
                }
            }
            span_log_bump_samples(&mc->fax.t30.logging, mc->len);
            span_log_bump_samples(&mc->fax.modems.v29_rx.logging, mc->len);
            span_log_bump_samples(&mc->fax.logging, mc->len);

            if (log_audio)
            {
                for (k = 0;  k < mc->len;  k++)
                    out_amp[2*k + j] = mc->amp[k];
            }
            if (machines[j ^ 1].len < SAMPLES_PER_CHUNK)
                memset(machines[j ^ 1].amp + machines[j ^ 1].len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - machines[j ^ 1].len));
            if (use_line_hits)
            {
                /* TODO: This applies very crude line hits. improve it */
                if (mc->fax.t30.state == 22)
                {
                    if (++mc->error_delay == 100)
                    {
                        fprintf(stderr, "HIT %d!\n", j);
                        mc->error_delay = 0;
                        for (k = 0;  k < 5;  k++)
                            mc->amp[k] = 0;
                    }
                }    
            }
            if (mc->fax.t30.state == t30_state_to_wreck)
                memset(machines[j ^ 1].amp, 0, sizeof(int16_t)*SAMPLES_PER_CHUNK);
            if (fax_rx(&mc->fax, machines[j ^ 1].amp, SAMPLES_PER_CHUNK))
                break;
            if (!mc->done)
                alldone = FALSE;
        }

        if (log_audio)
        {
            outframes = afWriteFrames(wave_handle, AF_DEFAULT_TRACK, out_amp, SAMPLES_PER_CHUNK);
            if (outframes != SAMPLES_PER_CHUNK)
                break;
        }

        if (alldone  ||  j < FAX_MACHINES)
            break;
    }
    time(&end_time);
    for (j = 0;  j < FAX_MACHINES;  j++)
    {
        mc = &machines[j];
        fax_release(&mc->fax);
    }
    if (log_audio)
    {
        if (afCloseFile(wave_handle))
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME_WAVE);
            exit(2);
        }
    }
    if (input_audio_file_name)
    {
        if (afCloseFile(input_wave_handle))
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", input_audio_file_name);
            exit(2);
        }
    }
    printf("Total audio time = %ds (wall time %ds)\n", machines[0].total_audio_time/8000, (int) (end_time - start_time));
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
