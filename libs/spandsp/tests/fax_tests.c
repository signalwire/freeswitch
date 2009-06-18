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
 * $Id: fax_tests.c,v 1.102 2009/05/30 15:23:13 steveu Exp $
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
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#include "fax_utils.h"

#define SAMPLES_PER_CHUNK       160

#define INPUT_TIFF_FILE_NAME    "../test-data/itu/fax/itutests.tif"

#define OUTPUT_FILE_NAME_WAVE   "fax_tests.wav"

#define FAX_MACHINES            2

struct machine_s
{
    int chan;
    int16_t amp[SAMPLES_PER_CHUNK];
    int len;
    fax_state_t *fax;
    awgn_state_t *awgn;
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
    char tag[20];

    i = (int) (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase B:", i);
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
    snprintf(tag, sizeof(tag), "%c: Phase D:", i);
    printf("%c: Phase D handler on channel %c - (0x%X) %s\n", i, i, result, t30_frametype(result));
    log_transfer_statistics(s, tag);
    log_tx_parameters(s, tag);
    log_rx_parameters(s, tag);

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
    char tag[20];

    i = (intptr_t) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase E:", i);
    printf("%c: Phase E handler on channel %c - (%d) %s\n", i, i, result, t30_completion_code_to_str(result));    
    log_transfer_statistics(s, tag);
    log_tx_parameters(s, tag);
    log_rx_parameters(s, tag);
    t30_get_transfer_statistics(s, &t);
    machines[i - 'A'].succeeded = (result == T30_ERR_OK)  &&  (t.pages_tx == 12  ||  t.pages_rx == 12);
    machines[i - 'A'].done = TRUE;
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
    printf("%c: Real time frame handler on channel %c - %s, %s, length = %d\n",
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
    printf("%c: Document handler on channel %c - event %d\n", i, i, event);
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    SNDFILE *wave_handle;
    SNDFILE *input_wave_handle;
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
    int signal_level;
    int noise_level;
    float signal_scaling;
    time_t start_time;
    time_t end_time;
    char *page_header_info;
    int opt;
    t30_state_t *t30;
    logging_state_t *logging;

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
    signal_level = 0;
    noise_level = -99;
    supported_modems = T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17;
    while ((opt = getopt(argc, argv, "ehH:i:I:lm:n:prRs:tTw:")) != -1)
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
        case 'n':
            noise_level = atoi(optarg);
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
        case 's':
            signal_level = atoi(optarg);
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

    input_wave_handle = NULL;
    if (input_audio_file_name)
    {
        if ((input_wave_handle = sf_open_telephony_read(input_audio_file_name, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open audio file '%s'\n", input_audio_file_name);
            exit(2);
        }
    }

    wave_handle = NULL;
    if (log_audio)
    {
        if ((wave_handle = sf_open_telephony_write(OUTPUT_FILE_NAME_WAVE, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_FILE_NAME_WAVE);
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
            mc->fax = fax_init(NULL, (mc->chan & 1)  ?  TRUE  :  FALSE);
        else
            mc->fax = fax_init(NULL, (mc->chan & 1)  ?  FALSE  :  TRUE);
        mc->awgn = NULL;
        signal_scaling = 1.0f;
        if (noise_level > -99)
        {
            mc->awgn = awgn_init_dbm0(NULL, 1234567, noise_level);
            signal_scaling = powf(10.0f, signal_level/20.0f);
            printf("Signal scaling %f\n", signal_scaling);
        }
        fax_set_transmit_on_idle(mc->fax, use_transmit_on_idle);
        fax_set_tep_mode(mc->fax, use_tep);
        t30 = fax_get_t30_state(mc->fax);
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
                t30_set_rx_encoding(t30, T4_COMPRESSION_ITU_T6);
            }
        }
        else
        {
            if (polled_mode)
            {
                sprintf(buf, "fax_tests_%d.tif", (mc->chan + 1)/2);
                t30_set_rx_file(t30, buf, -1);
                t30_set_rx_encoding(t30, T4_COMPRESSION_ITU_T6);
            }
            else
            {
                if (use_page_limits)
                    t30_set_tx_file(t30, input_tiff_file_name, 3, 6);
                else
                    t30_set_tx_file(t30, input_tiff_file_name, -1, -1);
            }
        }
        t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) mc->chan + 'A');
        t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) mc->chan + 'A');
        t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) mc->chan + 'A');
        t30_set_real_time_frame_handler(t30, real_time_frame_handler, (void *) (intptr_t) mc->chan + 'A');
        t30_set_document_handler(t30, document_handler, (void *) (intptr_t) mc->chan + 'A');
        sprintf(mc->tag, "FAX-%d", j + 1);

        logging = t30_get_logging_state(t30);
        span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        span_log_set_tag(logging, mc->tag);
        span_log_set_level(&t30->t4.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        span_log_set_tag(&t30->t4.logging, mc->tag);

        logging = fax_get_logging_state(mc->fax);
        span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
        span_log_set_tag(logging, mc->tag);

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
                mc->len = sf_readf_short(input_wave_handle, mc->amp, SAMPLES_PER_CHUNK);
                if (mc->len == 0)
                    break;
            }
            else
            {
                mc->len = fax_tx(mc->fax, mc->amp, SAMPLES_PER_CHUNK);
                if (mc->awgn)
                {
                    for (k = 0;  k < mc->len;  k++)
                        mc->amp[k] = ((int16_t) (mc->amp[k]*signal_scaling)) + awgn(mc->awgn);
                }
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
            t30 = fax_get_t30_state(mc->fax);
            logging = t30_get_logging_state(t30);
            span_log_bump_samples(logging, mc->len);
            logging = fax_get_logging_state(mc->fax);
            span_log_bump_samples(logging, mc->len);

            if (log_audio)
            {
                for (k = 0;  k < mc->len;  k++)
                    out_amp[2*k + j] = mc->amp[k];
            }
            if (machines[j ^ 1].len < SAMPLES_PER_CHUNK)
                memset(machines[j ^ 1].amp + machines[j ^ 1].len, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - machines[j ^ 1].len));
            t30 = fax_get_t30_state(mc->fax);
#if defined(WITH_SPANDSP_INTERNALS)
            if (use_line_hits)
            {
                /* TODO: This applies very crude line hits. improve it */
                if (t30->state == 22)
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
            if (t30->state == t30_state_to_wreck)
                memset(machines[j ^ 1].amp, 0, sizeof(int16_t)*SAMPLES_PER_CHUNK);
#endif
            if (fax_rx(mc->fax, machines[j ^ 1].amp, SAMPLES_PER_CHUNK))
                break;
            if (!mc->done)
                alldone = FALSE;
        }

        if (log_audio)
        {
            outframes = sf_writef_short(wave_handle, out_amp, SAMPLES_PER_CHUNK);
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
        fax_release(mc->fax);
    }
    if (log_audio)
    {
        if (sf_close(wave_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME_WAVE);
            exit(2);
        }
    }
    if (input_audio_file_name)
    {
        if (sf_close(input_wave_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", input_audio_file_name);
            exit(2);
        }
    }
    printf("Total audio time = %ds (wall time %ds)\n", machines[0].total_audio_time/8000, (int) (end_time - start_time));
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
