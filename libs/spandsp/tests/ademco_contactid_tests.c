/*
 * SpanDSP - a series of DSP components for telephony
 *
 * ademco_contactid.c - Ademco ContactID alarm protocol
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2012 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \page ademco_contactid_tests_page Ademco ContactID tests
\section ademco_contactid_tests_page_sec_1 What does it do?

\section ademco_contactid_tests_page_sec_2 How does it work?
*/

/* Enable the following definition to enable direct probing into the FAX structures */
//#define WITH_SPANDSP_INTERNALS

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
//#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#define SAMPLES_PER_CHUNK           160

#define OUTPUT_FILE_NAME            "ademco_contactid.wav"

#define MITEL_DIR                   "../test-data/mitel/"
#define BELLCORE_DIR                "../test-data/bellcore/"

const char *bellcore_files[] =
{
    MITEL_DIR    "mitel-cm7291-talkoff.wav",
    BELLCORE_DIR "tr-tsy-00763-1.wav",
    BELLCORE_DIR "tr-tsy-00763-2.wav",
    BELLCORE_DIR "tr-tsy-00763-3.wav",
    BELLCORE_DIR "tr-tsy-00763-4.wav",
    BELLCORE_DIR "tr-tsy-00763-5.wav",
    BELLCORE_DIR "tr-tsy-00763-6.wav",
    ""
};

static const ademco_contactid_report_t reports[] =
{
    {0x1234, 0x18, 0x1, 0x131, 0x1, 0x15},
    {0x1234, 0x18, 0x3, 0x131, 0x1, 0x15},
    {0x1234, 0x18, 0x1, 0x401, 0x2, 0x3},
    {0x1234, 0x18, 0x3, 0x401, 0x3, 0x5},
    {0x1234, 0x56, 0x7, 0x890, 0xBC, 0xDEF},
    {0x1234, 0x56, 0x7, 0x89A, 0xBC, 0xDEF}     /* This one is bad, as it contains a hex 'A' */
};
static int reports_entry = 0;

static int16_t amp[1000000];

int tx_callback_reported = FALSE;
int rx_callback_reported = FALSE;

int sending_complete = FALSE;

SNDFILE *outhandle;

static void talkoff_tx_callback(void *user_data, int tone, int level, int duration)
{
    printf("Ademco sender report %d\n", tone);
    tx_callback_reported = TRUE;
}

static int mitel_cm7291_side_2_and_bellcore_tests(void)
{
    int j;
    SNDFILE *inhandle;
    int frames;
    ademco_contactid_sender_state_t *sender;
    logging_state_t *logging;

    if ((sender = ademco_contactid_sender_init(NULL, talkoff_tx_callback, NULL)) == NULL)
        return -1;
    logging = ademco_contactid_sender_get_logging_state(sender);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Ademco-tx");

    tx_callback_reported = FALSE;

    /* The remainder of the Mitel tape is the talk-off test */
    /* Here we use the Bellcore test tapes (much tougher), in six
      files - 1 from each side of the original 3 cassette tapes */
    /* Bellcore say you should get no more than 470 false detections with
       a good receiver. Dialogic claim 20. Of course, we can do better than
       that, eh? */
    printf("Talk-off test\n");
    for (j = 0;  bellcore_files[j][0];  j++)
    {
        if ((inhandle = sf_open_telephony_read(bellcore_files[j], 1)) == NULL)
        {
            printf("    Cannot open speech file '%s'\n", bellcore_files[j]);
            return -1;
        }
        while ((frames = sf_readf_short(inhandle, amp, SAMPLE_RATE)))
        {
            ademco_contactid_sender_rx(sender, amp, frames);
        }
        if (sf_close_telephony(inhandle))
        {
            printf("    Cannot close speech file '%s'\n", bellcore_files[j]);
            return -1;
        }
        printf("    File %d gave %d false hits.\n", j + 1, 0);
    }
    if (tx_callback_reported)
    {
        printf("    Failed\n");
        return -1;
    }
    printf("    Passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void rx_callback(void *user_data, const ademco_contactid_report_t *report)
{
    printf("Ademco Contact ID message:\n");
    printf("    Account %X\n", report->acct);
    printf("    Message type %X\n", report->mt);
    printf("    Qualifier %X\n", report->q);
    printf("    Event %X\n", report->xyz);
    printf("    Group/partition %X\n", report->gg);
    printf("    User/Zone information %X\n", report->ccc);
    if (memcmp(&reports[reports_entry], report, sizeof (*report)))
    {
        printf("Report mismatch\n");
        exit(2);
    }
    rx_callback_reported = TRUE;
}
/*- End of function --------------------------------------------------------*/

static void tx_callback(void *user_data, int tone, int level, int duration)
{
    ademco_contactid_sender_state_t *sender;

    sender = (ademco_contactid_sender_state_t *) user_data;
    printf("Ademco sender report %d\n", tone);
    switch (tone)
    {
    case -1:
        /* We are connected and ready to send */
        ademco_contactid_sender_put(sender, &reports[reports_entry]);
        break;
    case 1:
        /* We have succeeded in sending, and are ready to send another message. */
        if (++reports_entry < 5)
            ademco_contactid_sender_put(sender, &reports[reports_entry]);
        else
            sending_complete = TRUE;
        break;
    case 0:
        /* Sending failed after retries */
        sending_complete = TRUE;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static int end_to_end_tests(void)
{
    ademco_contactid_receiver_state_t *receiver;
    ademco_contactid_sender_state_t *sender;
    logging_state_t *logging;
    codec_munge_state_t *munge;
    awgn_state_t noise_source;
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t sndfile_buf[2*SAMPLES_PER_CHUNK];
    int samples;
    int i;
    int j;

    printf("End to end tests\n");

    if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 2)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    if ((receiver = ademco_contactid_receiver_init(NULL, rx_callback, NULL)) == NULL)
        return -1;
    ademco_contactid_receiver_set_realtime_callback(receiver, rx_callback, receiver);

    logging = ademco_contactid_receiver_get_logging_state(receiver);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Ademco-rx");

    if ((sender = ademco_contactid_sender_init(NULL, tx_callback, NULL)) == NULL)
        return -1;
    ademco_contactid_sender_set_realtime_callback(sender, tx_callback, sender);
    logging = ademco_contactid_sender_get_logging_state(sender);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Ademco-tx");

    awgn_init_dbm0(&noise_source, 1234567, -50);
    munge = codec_munge_init(MUNGE_CODEC_ALAW, 0);

    sending_complete = FALSE;
    rx_callback_reported = FALSE;

    for (i = 0;  i < 1000;  i++)
    {
        samples = ademco_contactid_sender_tx(sender, amp, SAMPLES_PER_CHUNK);
        for (j = samples;  j < SAMPLES_PER_CHUNK;  j++)
            amp[j] = 0;
        for (j = 0;  j < SAMPLES_PER_CHUNK;  j++)
            sndfile_buf[2*j] = amp[j];
        /* There is no point in impairing this signal. It is just DTMF tones, which
           will work as wel as the DTMF detector beign used. */
        ademco_contactid_receiver_rx(receiver, amp, SAMPLES_PER_CHUNK);

        samples = ademco_contactid_receiver_tx(receiver, amp, SAMPLES_PER_CHUNK);
        for (j = samples;  j < SAMPLES_PER_CHUNK;  j++)
            amp[j] = 0;
        
        /* We add AWGN and codec impairments to the signal, to stress the tone detector. */
        codec_munge(munge, amp, SAMPLES_PER_CHUNK);
        for (j = 0;  j < SAMPLES_PER_CHUNK;  j++)
        {
            sndfile_buf[2*j + 1] = amp[j];
            /* Add noise to the tones */
            amp[j] += awgn(&noise_source);
        }
        codec_munge(munge, amp, SAMPLES_PER_CHUNK);
        ademco_contactid_sender_rx(sender, amp, SAMPLES_PER_CHUNK);

        sf_writef_short(outhandle, sndfile_buf, SAMPLES_PER_CHUNK);
    }
    if (!rx_callback_reported)
    {
        fprintf(stderr, "    Report not received\n");
        return -1;
    }

    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
        return -1;
    }
    printf("    Passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int encode_decode_tests(void)
{
    char buf[100];
    ademco_contactid_receiver_state_t *receiver;
    ademco_contactid_sender_state_t *sender;
    logging_state_t *logging;
    ademco_contactid_report_t result;
    int i;

    printf("Encode and decode tests\n");

    if ((receiver = ademco_contactid_receiver_init(NULL, NULL, NULL)) == NULL)
        return 2;
    logging = ademco_contactid_receiver_get_logging_state(receiver);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Ademco-rx");

    if ((sender = ademco_contactid_sender_init(NULL, NULL, NULL)) == NULL)
        return 2;
    logging = ademco_contactid_sender_get_logging_state(sender);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Ademco-tx");

    for (i = 0;  i < 5;  i++)
    {
        if (encode_msg(buf, &reports[i]) < 0)
        {
            printf("Bad encode message\n");
            return -1;
        }
        printf("'%s'\n", buf);
        if (decode_msg(&result, buf))
        {
            printf("Bad decode message\n");
            return -1;
        }
        ademco_contactid_receiver_log_msg(receiver, &result);
        printf("\n");
        if (memcmp(&reports[i], &result, sizeof(result)))
        {
            printf("Received message does not match the one sent\n");
            return -1;
        }
    }

    if (encode_msg(buf, &reports[5]) >= 0)
    {
        printf("Incorrectly good message\n");
        return -1;
    }
    printf("'%s'\n", buf);
    printf("\n");
    printf("    Passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void decode_file(const char *file)
{
    //SPAN_DECLARE(int) decode_msg(ademco_contactid_report_t *report, const char buf[])
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int opt;
    const char *decode_test_file;

    decode_test_file = NULL;
    while ((opt = getopt(argc, argv, "d:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            decode_test_file = optarg;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    if (decode_test_file)
    {
        decode_file(decode_test_file);
        return 0;
    }

    if (encode_decode_tests())
    {
        printf("Tests failed\n");
        return 2;
    }

    if (mitel_cm7291_side_2_and_bellcore_tests())
    {
        printf("Tests failed\n");
        return 2;
    }

    if (end_to_end_tests())
    {
        printf("Tests failed\n");
        return 2;
    }

    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
