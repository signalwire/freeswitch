/*
 * SpanDSP - a series of DSP components for telephony
 *
 * data_modems_tests.c - Tests for data_modems.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011 Steve Underwood
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

/*! \page data_modems_tests_page Data modems tests
\section data_modems_tests_page_sec_1 What does it do?
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
#include <assert.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#if defined(ENABLE_GUI)
#include "media_monitor.h"
#endif

#define INPUT_FILE_NAME         "../test-data/itu/fax/itu1.tif"
#define OUTPUT_FILE_NAME        "t31.tif"
#define OUTPUT_WAVE_FILE_NAME   "data_modems.wav"

#define SAMPLES_PER_CHUNK 160

struct command_response_s
{
    const char *command;
    int len_command;
    const char *response;
    int len_response;
};

char *decode_test_file = NULL;
int countdown = 0;
int command_response_test_step = -1;
char response_buf[1000];
int response_buf_ptr = 0;
bool answered = false;
bool done = false;
bool sequence_terminated = false;

data_modems_state_t *data_modems_state[2];

static void reporter(void *user_data, int reason, bert_results_t *results)
{
    int channel;

    channel = (int) (intptr_t) user_data;
    switch (reason)
    {
    case BERT_REPORT_SYNCED:
        fprintf(stderr, "%d: BERT report synced\n", channel);
        break;
    case BERT_REPORT_UNSYNCED:
        fprintf(stderr, "%d: BERT report unsync'ed\n", channel);
        break;
    case BERT_REPORT_REGULAR:
        fprintf(stderr, "%d: BERT report regular - %d bits, %d bad bits, %d resyncs\n", channel, results->total_bits, results->bad_bits, results->resyncs);
        break;
    case BERT_REPORT_GT_10_2:
        fprintf(stderr, "%d: BERT report > 1 in 10^2\n", channel);
        break;
    case BERT_REPORT_LT_10_2:
        fprintf(stderr, "%d: BERT report < 1 in 10^2\n", channel);
        break;
    case BERT_REPORT_LT_10_3:
        fprintf(stderr, "%d: BERT report < 1 in 10^3\n", channel);
        break;
    case BERT_REPORT_LT_10_4:
        fprintf(stderr, "%d: BERT report < 1 in 10^4\n", channel);
        break;
    case BERT_REPORT_LT_10_5:
        fprintf(stderr, "%d: BERT report < 1 in 10^5\n", channel);
        break;
    case BERT_REPORT_LT_10_6:
        fprintf(stderr, "%d: BERT report < 1 in 10^6\n", channel);
        break;
    case BERT_REPORT_LT_10_7:
        fprintf(stderr, "%d: BERT report < 1 in 10^7\n", channel);
        break;
    default:
        fprintf(stderr, "%d: BERT report reason %d\n", channel, reason);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static int get_msg(void *user_data, uint8_t msg[], int len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void put_msg(void *user_data, const uint8_t msg[], int len)
{
    if (len < 0)
        printf("Status %s\n", signal_status_to_str(len));
}
/*- End of function --------------------------------------------------------*/

static int modem_tests(int use_gui, int log_audio, int test_sending)
{
    int mdm_len;
    int16_t mdm_amp[SAMPLES_PER_CHUNK];
    //int use_tep;
    //logging_state_t *logging;
    int outframes;
    int16_t silence[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    SNDFILE *wave_handle;
    SNDFILE *in_handle;
    int i;
    int k;
    int calling_party;
    logging_state_t *logging;
    bert_state_t *bert[2];

    /* Test a pair of modems against each other */

    /* Set up the test environment */
    //use_tep = false;

    wave_handle = NULL;
    if (log_audio)
    {
        if ((wave_handle = sf_open_telephony_write(OUTPUT_WAVE_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            exit(2);
        }
    }

    in_handle = NULL;
    if (decode_test_file)
    {
        if ((in_handle = sf_open_telephony_read(decode_test_file, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", decode_test_file);
            exit(2);
        }
    }

    memset(silence, 0, sizeof(silence));
    memset(mdm_amp, 0, sizeof(mdm_amp));
    mdm_len = 0;

    /* Now set up and run the modems */
    calling_party = true;
    for (i = 0;  i < 2;  i++)
    {
        bert[i] = bert_init(NULL, 1000000, BERT_PATTERN_ITU_O152_11, 2400, 20);
        bert_set_report(bert[i], 100000, reporter, (void *) (intptr_t) i);
        if ((data_modems_state[i] = data_modems_init(NULL,
                                                     calling_party,
                                                     put_msg,
                                                     get_msg,
                                                     NULL)) == NULL)
        {
            fprintf(stderr, "    Cannot start the data modem\n");
            exit(2);
        }
        logging = data_modems_get_logging_state(data_modems_state[i]);
        span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
        span_log_set_tag(logging, "Modem");
        calling_party = false;
    }

#if defined(ENABLE_GUI)
    if (use_gui)
        start_media_monitor();
#endif
    while (!done)
    {
        for (i = 0;  i < 2;  i++)
        {
            /* The receive side always expects a full block of samples, but the
               transmit side may not be sending any when it doesn't need to. We
               may need to pad with some silence. */
            mdm_len = data_modems_tx(data_modems_state[i], mdm_amp, SAMPLES_PER_CHUNK);
            if (mdm_len < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(mdm_amp + mdm_len, SAMPLES_PER_CHUNK - mdm_len);
                mdm_len = SAMPLES_PER_CHUNK;
            }
            if (log_audio)
            {
                for (k = 0;  k < mdm_len;  k++)
                    out_amp[2*k + i] = mdm_amp[k];
            }
            if (data_modems_rx(data_modems_state[i ^ 1], mdm_amp, mdm_len))
                break;
        }

        if (log_audio)
        {
            outframes = sf_writef_short(wave_handle, out_amp, SAMPLES_PER_CHUNK);
            if (outframes != SAMPLES_PER_CHUNK)
                break;
        }
    }

    if (decode_test_file)
    {
        if (sf_close_telephony(in_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", decode_test_file);
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

    if (!done  ||  !sequence_terminated)
    {
        printf("Tests failed\n");
        return 2;
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int log_audio;
    int test_sending;
    int use_gui;
    int opt;

    decode_test_file = NULL;
    log_audio = false;
    test_sending = false;
    use_gui = false;
    while ((opt = getopt(argc, argv, "d:glrs")) != -1)
    {
        switch (opt)
        {
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
        case 'l':
            log_audio = true;
            break;
        case 'r':
            test_sending = false;
            break;
        case 's':
            test_sending = true;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    modem_tests(use_gui, log_audio, test_sending);
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
