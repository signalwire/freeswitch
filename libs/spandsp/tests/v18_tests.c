/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v18_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004-2009 Steve Underwood
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

/*! \page v18_tests_page V.18 tests
\section v18_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define FALSE 0
#define TRUE (!FALSE)

#define OUTPUT_FILE_NAME    "v18.wav"

#define SAMPLES_PER_CHUNK   160

int log_audio = FALSE;
SNDFILE *outhandle = NULL;
char result[2][1024];
int unexpected_echo = FALSE;

char *decode_test_file = NULL;

int good_message_received;

both_ways_line_model_state_t *model;
int rbs_pattern = 0;
float noise_level = -70.0f;
int line_model_no = 0;
int channel_codec = MUNGE_CODEC_NONE;
v18_state_t *v18[2];

const char *qbf_tx = "The quick Brown Fox Jumps Over The Lazy dog 0123456789!@#$%^&*()'";
const char *qbf_rx = "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789!X$$/'+.()'";
const char *full_baudot_rx =
    "\b \n\n\n\r?\n\n\n  !\"$$/+'().+,-./"
    "0123456789:;(=)?"
    "XABCDEFGHIJKLMNOPQRSTUVWXYZ(/)' "
    "'ABCDEFGHIJKLMNOPQRSTUVWXYZ(!) ";

static void put_text_msg(void *user_data, const uint8_t *msg, int len)
{
    if (strcmp((const char *) msg, qbf_rx))
    {
        printf("Result:\n%s\n", msg);
        printf("Reference result:\n%s\n", qbf_rx);
    }
    else
    {
        good_message_received = TRUE;
    }
}
/*- End of function --------------------------------------------------------*/

static void basic_tests(int mode)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    printf("Testing %s\n", v18_mode_to_str(mode));
    v18[0] = v18_init(NULL, TRUE, mode, V18_AUTOMODING_GLOBAL, put_text_msg, NULL);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, mode, V18_AUTOMODING_GLOBAL, put_text_msg, NULL);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    /* Fake an OK condition for the first message test */
    good_message_received = TRUE;
    push = 0;
    if (v18_put(v18[0], qbf_tx, -1) != strlen(qbf_tx))
    {
        printf("V.18 put failed\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        if (push == 0)
        {
            if ((samples = v18_tx(v18[0], amp[0], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
        }
        else
        {
            samples = 0;
            /* Push a little silence through, to flush things out */
            if (--push == 0)
            {
                if (!good_message_received)
                {
                    printf("No message received\n");
                    exit(2);
                }
                good_message_received = FALSE;
                if (v18_put(v18[0], qbf_tx, -1) != strlen(qbf_tx))
                {
                    printf("V.18 put failed\n");
                    exit(2);
                }
            }
        }
        if (samples < SAMPLES_PER_CHUNK)
        {
            vec_zeroi16(&amp[0][samples], SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        if ((samples = v18_tx(v18[1], amp[1], SAMPLES_PER_CHUNK)) == 0)
            push = 10;
        if (samples < SAMPLES_PER_CHUNK)
        {
            vec_zeroi16(&amp[1][samples], SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }
    v18_free(v18[0]);
    v18_free(v18[1]);
}
/*- End of function --------------------------------------------------------*/

static void misc_01_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_misc_01(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.1.1     No disconnection test
        Purpose:        To verify that the DCE does not initiate a disconnection.
        Preamble:       N/A
        Method:         A call is made to the TUT from the tester which remains off hook for 10 minutes
                        without sending any signal.
        Pass criteria:  The TUT should answer the call and enter the probing state after 3s. The
                        TUT should continue to probe until the test is terminated.
        Comments:       This feature should also be verified by observation during the automoding tests.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_01_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_01_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void misc_02_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_misc_02(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.1.2     Automatic resumption of automoding
        Purpose:        To ensure that the DCE can be configured to automatically re-assume the automode
                        calling state after 10s of no valid signal.
        Preamble:       The TUT should be configured to automatically re-assume the initial automoding
                        state.
        Method:         The tester should set up a call to the TUT in V.21 mode and then drop the carrier.
                        The tester will then transmit silence for 11s followed by a 1300Hz tone for
                        5s (i.e. V.23).
        Pass criteria:  1) 10s after dropping the carrier the TUT should return to state Monitor 1.
                        2) After 2.7+-0.3s the TUT should select V.23 mode and send a 390Hz tone.
        Comments:       The TUT should indicate that carrier has been lost at some time after the 1650Hz
                        signal is lost.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_02_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_02_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void misc_03_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_misc_03(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.1.3     Retention of selected mode on loss of signal
        Purpose:        To ensure that the DCE stays in the selected transmission mode if it is not
                        configured to automatically re-assume the initial automoding state.
        Preamble:       The TUT should be configured to remain in the selected transmission mode when
                        the carrier is lost.
        Method:         The tester should set up a call to the TUT in V.21 mode, for example. It will drop
                        the carrier for 9s and then re-start transmission of the same carrier for
                        1s followed by a short message.
        Pass criteria:  The TUT should resume operation in V.21 mode and capture the entire test
                        message.
        Comments:       The TUT should indicate that carrier has been lost at some time after the carrier
                        signal is removed and not disconnect.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_03_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_03_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void misc_04_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_misc_04(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.1.4     Detection of BUSY tone
        Purpose:        To ensure that the DCE provides the call progress indication "BUSY" in presence of
                        the national busy tone.
        Preamble:       N/A
        Method:         The TUT should be configured to dial out and then be presented with the
                        appropriate national busy tone.
        Pass criteria:  Detection of busy tone should be displayed by the TUT.
        Comments:       ITU-T V.18 specifies that the DCE should not hang up, but that is intended to apply
                        to the case where a connection is established and then lost. A terminal may
                        automatically hang up when busy tone is detected. PABX busy tones may differ in
                        frequency and cadence from national parameters.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_04_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_04_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void misc_05_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_misc_05(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.1.5     Detection of RINGING
        Purpose:        To ensure that the DCE provides the call progress indication "RINGING" in
                        presence of the national ringing tone.
        Preamble:       N/A
        Method:         The tester will make a call to the TUT using the nationally recommended cadence
                        and the minimum recommended ring voltage/current.
        Pass criteria:  The RINGING condition should be visually indicated by the TUT.
        Comments:       This test should be repeated across a range of valid timings and ring voltages.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_05_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_05_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void misc_06_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_misc_06(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.1.6     "LOSS OF CARRIER" indication
        Purpose:        To ensure that the DCE provides the call progress indication "LOSS OF CARRIER"
                        upon a loss of carrier in full duplex modes, i.e. V.21, V.23, Bell 103.
        Preamble:       N/A
        Method:         Set up a call in each of the full duplex modes and force a carrier failure to the TUT.
        Pass criteria:  Loss of carrier should be indicated and disappear when the carrier is restored.
        Comments:       The V.18 modem should not automatically disconnect when used in a manual
                        conversation mode. However, a V.18 equipped terminal may disconnect based on
                        operational decisions, e.g. when it is a terminal in automatic answering machine
                        mode. There may be other cases, e.g. where the V.18 DCE is used in a gateway,
                        when automatic disconnection is required.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_06_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_06_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void misc_07_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_misc_07(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.1.7     Call progress indication
        Purpose:        To ensure that the DCE provides the call progress indication "CONNECT(x)" upon
                        a connection.
        Preamble:       N/A
        Method:         Correct CONNECT messages should be verified during the Automode tests that
                        follow.
        Pass criteria:  The relevant mode should be indicated by the DCE when automoding is complete.
                        However, this may possibly not be indicated by the DTE.
        Comments:       The possible modes are: V.21, V.23, Baudot 45, Baudot 50, EDT, Bell 103, DTMF.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_07_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_07_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void misc_08_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_misc_08(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.1.8     Circuit 135 Test
        Purpose:        To ensure that the DCE implements circuit 135 or an equivalent way of indicating
                        presence of a signal.
        Preamble:       N/A
        Method:         A call from the TUT should be answered in voice mode after 20s. The tester
                        will transmit sampled voice messages. V.24 circuit 135 or its equivalent should be
                        observed.
        Pass criteria:  The ring tone and speech shall be indicated by circuit 135.
        Comment:        The response times and signal level thresholds of Circuit 135 are not specified in
                        ITU-T V.18 or V.24 and therefore the pattern indicated may vary.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_08_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_08_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void misc_09_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_misc_09(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.1.9     Connection procedures
        Purpose:        To ensure that the TUT implements the call connect procedure described in
                        clause 6.
        Preamble:       N/A
        Method:         TBD
        Pass criteria:  TBD
        Comment:        TBD
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_09_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, misc_09_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_01_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_01(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.1     CI and XCI signal coding and cadence
        Purpose:        To verify that TUT correctly emits the CI and XCI signals with the ON/OFF
                        cadence defined in 5.1.1.
        Preamble:       N/A
        Method:         V.21 demodulator is used to decode the CI sequence and a timer to measure the
                        silence intervals between them. The XCI signal is also monitored and decoded to
                        check for correct coding and timing of the signal.
        Pass criteria:  1) No signal should be transmitted for one second after connecting to the line.
                        2) Four CI patterns are transmitted for each repetition.
                        3) No signal is transmitted for 2s after the end of each CI.
                        4) Each CI must have the correct bit pattern.
                        5) The CI patterns followed by 2s of silence must be repeated twice.
                        6) One second after every 3 blocks CI an XCI signal must be transmitted.
                        7) The XCI should have the structure defined in 3.11.
                        8) The whole sequence should be repeated until the call is cleared.
                        9) When V.18 to V.18, the XCI must not force V.23 or Minitel mode.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_01_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_01_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_02_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_02(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.2     ANS signal detection
        Purpose:        To verify that TUT correctly detects the ANS (2100Hz) signal during the
                        two-second interval (Toff) between transmission of CI sequences.
        Preamble:       Make a V.18 call from the TUT.
        Method:         The Test System waits for the TUT to stop transmitting a CI and responds with an
                        ANS signal. The V.21 demodulator is used to decode the TXP sequence and a timer
                        measures the silence intervals between them. ANS should be transmitted for 2s.
        Pass criteria:  1) No signal should be transmitted by TUT for 0.5s from detection of ANS.
                        2) The TUT should reply with transmission of TXP as defined in 5.1.2.
                        3) Verify that TXP sequence has correct bit pattern.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_02_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_02_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_03_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_03(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.3     End of ANS signal detection
        Purpose:        The TUT should stop sending TXP at the end of the current sequence when the ANS
                        tone ceases.
        Preamble:       Test ORG-02 should be successfully completed immediately prior to this test.
        Method:         The tester sends ANS for 2s followed by silence. The tester will then
                        monitor for cessation of TXP at the end of the answer tone.
        Pass criteria:  The TUT should stop sending TXP at the end of the current sequence when ANS
                        tone ceases.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_03_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_03_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_04_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_04(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.4     ANS tone followed by TXP
        Purpose:        To check correct detection of V.18 modem.
        Preamble:       Tests ORG-02 and ORG-03 should be successfully completed prior to this test.
        Method:         Tester transmits ANS for 2.5s followed by 75ms of no tone then transmits
                        3 TXP sequences using V.21 (2) and starts a 1s timer. It will then transmit 1650Hz
                        for 5s.
        Pass criteria:  1) TUT should initially respond with TXP.
                        2) TUT should stop sending TXP within 0.2s of end of ANS.
                        3) TUT should respond with 980Hz carrier within 1s of end of 3 TXP sequences.
                        4) Data should be transmitted and received according to ITU-T T.140 to comply
                           with the V.18 operational requirements.
        Comments:       The TUT should indicate that V.18 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_04_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_04_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_05_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_05(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.5     ANS tone followed by 1650Hz
        Purpose:        To check correct detection of V.21 modem upper channel when preceded by answer
                        tone and to confirm discrimination between V.21 and V.18 modes.
        Preamble:       Tests ORG-02 and ORG-03 should be successfully completed prior to this test.
        Method:         Tester transmits ANS for 2.5s followed by 75ms of no tone then transmits
                        1650Hz and starts a 0.7 second timer.
        Pass criteria:  1) TUT should initially respond with TXP.
                        2) TUT should stop sending TXP within 0.2s of end of ANS.
                        3) TUT should respond with 980Hz at 0.5(+0.2-0.0)s of start of 1650Hz.
                        4) Data should be transmitted and received at 300 bit/s complying with Annex F.
        Comments:       Selection of ITU-T V.21 as opposed to ITU-T V.18 should be confirmed by
                        examination of TUT. If there is no visual indication, verify by use of ITU-T T.50 for
                        ITU-T V.21 as opposed to UTF-8 coded ISO 10646 character set for ITU-T V.18.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_05_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_05_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_06_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_06(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.6     ANS tone followed by 1300Hz
        Purpose:        To check correct detection of V.23 modem upper channel when preceded by answer
                        tone.
        Preamble:       Tests ORG-02 and ORG-03 should be successfully completed prior to this test.
        Method:         Tester transmits ANS for 2.5s followed by 75ms of no tone then transmits
                        1300Hz and starts a 2.7s timer.
        Pass criteria:  1) TUT should initially respond with TXP.
                        2) TUT should stop sending TXP within 0.2s of end of ANS.
                        3) TUT should respond with 390Hz after 1.7(+0.2-0.0)s of start of 1300Hz.
                        4) Data should be transmitted and received at 75 bit/s and 1200 bit/s respectively
                           by the TUT to comply with Annex E.
        Comments:       The TUT should indicate that V.23 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_06_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_06_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_07_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_07(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.7     ANS tone followed by no tone
        Purpose:        To confirm that TUT does not lock up under this condition.
        Preamble:       Tests ORG-02 and ORG-03 should be successfully completed prior to this test.
        Method:         Tester transmits ANS for 2.5s followed by no tone for 10 s. It then transmits
                        DTMF tones for 2s.
        Pass criteria:  1) TUT should initially respond with TXP.
                        2) TUT should stop sending TXP within 0.2s of end of ANS.
                        3) TUT should return to Monitor 1 state and then connect in DTMF mode within
                           12s of the end of ANS tone.
        Comments:       This condition would cause the terminal to lock up if the V.18 standard is followed
                        literally. It may however, occur when connected to certain Swedish textphones if the
                        handset is lifted just after the start of an automatically answered incoming call.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_07_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_07_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_08_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_08(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.8     Bell 103 (2225Hz signal) detection
        Purpose:        To verify that the TUT correctly detects the Bell 103 upper channel signal during
                        the 2-second interval between transmission of CI sequences.
        Preamble:       N/A
        Method:         The tester waits for a CI and then sends a 2225Hz signal for 5s.
        Pass criteria:  1) The TUT should respond with a 1270Hz tone in 0.5+-0.1s.
                        2) Data should be transmitted and received at 300 bit/s to comply with Annex D.
        Comments:       The TUT should indicate that Bell 103 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_08_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_08_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_09_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_09(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.9     V.21 (1650Hz signal) detection
        Purpose:        To verify that the TUT correctly detects the V.21 upper channel signal during the
                        2-second interval between transmission of CI sequences.
        Preamble:       N/A
        Method:         The tester waits for a CI and then sends a 1650Hz signal for 5s.
        Pass criteria:  1) The TUT should respond with a 980Hz tone in 0.5+-0.1s.
                        2) Data should be transmitted and received at 300 bit/s to comply with Annex F.
        Comments:       The TUT should indicate that V.21 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_09_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_09_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_10_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_10(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.10    V.23 (1300Hz signal) detection
        Purpose:        To verify that the TUT correctly detects the V.23 upper channel signal during the
                        2-second interval between transmission of CI sequences.
        Preamble:       N/A
        Method:         The tester waits for a CI and then sends a 1300Hz signal for 5s.
        Pass criteria:  1) The TUT should respond with a 390Hz tone in 1.7+-0.1s.
                        2) Data should be transmitted and received at 75 bit/s and 1200 bit/s respectively
                           by the TUT to comply with Annex E.
        Comments:       The TUT should indicate that V.23 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_10_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_10_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_11_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_11(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.11    V.23 (390Hz signal) detection
        Purpose:        To confirm correct selection of V.23 reverse mode during sending of XCI.
        Preamble:       N/A
        Method:         The tester should wait for the start of the XCI signal and then send 390Hz to TUT
                        for 5s.
        Pass criteria:  1) The TUT should complete the XCI as normal.
                        2) The TUT should then maintain the 1300Hz tone while the 390Hz test tone is
                           present.
                        3) Data should be transmitted and received at 1200 bit/s and 75 bit/s respectively
                           by the TUT to comply with Annex E when connection is indicated.
        Comments:       The TUT should indicate that V.23 mode has been selected at least 3s after
                        the start of the 390Hz tone.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_11_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_11_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_12_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_12(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.12    5 bit mode (Baudot) detection tests
        Purpose:        To confirm detection of Baudot modulation at various bit rates that may be
                        encountered.
        Preamble:       N/A
        Method:         The tester transmits the 5-bit coded characters "0" to "9" followed by "abcdef" at
                        (a) 45.45, (b) 47.6, (c) 50 and (d) 100 bits per second. When TUT indicates a
                        connection, type at least 5 characters back to the tester so that correct selection
                        of bit rate can be confirmed.
        Pass criteria:  1) TUT should select Baudot mode and the appropriate bit rate.
                        2) The tester will analyse the bit rate of received characters, which should be at
                           either 45.45 or 50 bits per second as appropriate.
        Comments:       45.45 and 50 bit/s are the commonly used Baudot bit rates. However, certain
                        textphones can operate at higher rates (e.g. 100 bit/s). Responding at either 45.45 or
                        50 bit/s is acceptable to these devices which normally fall back to the selected rate.
                        47.6 bit/s may possibly be encountered from another V.18 textphone in the
                        automode answer state. The TUT may then select either 45.45 or 50 bit/s for the
                        transmission.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_12_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_12_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_13_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_13(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.13    DTMF signal detection
        Purpose:        To verify whether the TUT correctly recognizes DTMF signals during the 2-second
                        interval between transmission of CI.
        Preamble:       N/A
        Method:         The tester will send a single DTMF tone of 40ms duration to TUT. When TUT
                        indicates a connection, type at least 5 characters back to the tester so that correct
                        selection of mode can be confirmed.
        Pass criteria:  The tester will analyse the received characters to confirm DTMF mode selection.
        Comments:       TUT should indicate that it has selected DTMF mode. The DTMF capabilities of the
                        TUT should comply with ITU-T Q.24 for the Danish Administration while
                        receiving for best possible performance.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_13_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_13_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_14_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_14(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.14    EDT rate detection
        Purpose:        To confirm detection of EDT modems by detecting the transmission rate of received
                        characters.
        Preamble:       N/A
        Method:         The tester transmits EDT characters "abcdef" to TUT at 110 bit/s. When TUT
                        indicates that the connection is established, type characters "abcdef<CR>" back to
                        the tester. The same characters will then be transmitted back to the TUT.
        Pass criteria:  Ensure correct reception of characters by tester and TUT.
        Comments:       The TUT should be able to determine the rate on the six characters given. If it takes
                        more than this then performance is probably inadequate as too many characters
                        would be lost. Some characters may be lost during the detection process. However,
                        the number lost should be minimal. The data bits and parity are specified in
                        Annex C.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_14_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_14_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_15_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_15(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.15    Rate detection test
        Purpose:        To verify the presence of 980/1180Hz at a different signalling rate than 110 bit/s
                        returns the TUT modem to the "monitor A" state.
        Preamble:       N/A
        Method:         The tester transmits 980/1180Hz signals at 300 bit/s for 2s.
        Pass criteria:  The TUT should not select EDT or any other mode and should continue to transmit
                        the CI signal.
        Comments:       Echoes of the CI sequences may be detected at 300 bit/s.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_15_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_15_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_16_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_16(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.16    980Hz detection
        Purpose:        To confirm correct selection of V.21 reverse mode.
        Preamble:       N/A
        Method:         The tester sends 980Hz to TUT for 5s.
        Pass criteria:  1) TUT should respond with 1650Hz tone after 1.5+-0.1s after start of
                           980Hz tone.
                        2) Data should be transmitted and received at 300 bit/s complying with Annex F.
        Comments:       The TUT should indicate that V.21 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_16_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_16_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_17_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_17(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.17    Loss of signal after 980Hz
        Purpose:        To confirm that TUT returns to the Monitor 1 state if 980Hz signal disappears.
        Preamble:       N/A
        Method:         The tester sends 980Hz to TUT for 1.2s followed by silence for 5s.
        Pass criteria:  TUT should not respond to the 980Hz tone and resume sending CI signals after a
                        maximum of 2.4s from the end of the 980Hz tone.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_17_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_17_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_18_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_18(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.18    Tr timer
        Purpose:        To confirm that TUT returns to the Monitor 1 state if Timer Tr expires.
        Preamble:       N/A
        Method:         The tester sends 980Hz to TUT for 1.2s followed by 1650Hz for 5s
                        with no pause.
        Pass criteria:  TUT should respond with 980Hz after 1.3+-0.1s of 1650Hz.
        Comments:       This implies timer Tr has expired 2s after the start of the 980Hz tone and
                        then 1650Hz has been detected for 0.5s.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_18_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_18_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_19_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_19(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.19    Bell 103 (1270Hz signal) detection
        Purpose:        To confirm correct selection of Bell 103 reverse mode.
        Preamble:       N/A
        Method:         The tester sends 1270Hz to TUT for 5s.
        Pass criteria:  1) TUT should respond with 2225Hz tone after 0.7+-0.1 s.
                        2) Data should be transmitted and received at 300 bit/s complying with Annex D.
        Comments:       The TUT should indicate that Bell 103 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_19_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_19_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_20_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_20(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.20    Immunity to network tones
        Purpose:        To ensure that the TUT does not interpret network tones as valid signals.
        Preamble:       N/A
        Method:         The tester will first send a dial tone to the TUT, this will be followed by a ringing
                        tone and a network congestion tone. The frequencies and cadences of the tones will
                        vary according to the country setting. The tester must be configured for the same
                        country as the TUT.
        Pass criteria:  The countries supported by the TUT should be noted along with the response to
                        each tone. The tones should either be ignored or reported as the relevant network
                        tone to the user.
        Comments:       V.18 is required to recognize and report RINGING and BUSY tones. Other network
                        tones may be ignored. Some devices may only provide a visual indication of the
                        presence and cadence of the tones for instance by a flashing light. The TUT may
                        disconnect on reception of tones indicating a failed call attempt.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_20_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_20_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_21_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_21(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.21    Immunity to non-textphone modems
        Purpose:        To ensure that the TUT does not interpret modem tones not supported by V.18 as
                        valid text telephone tones.
        Preamble:       N/A
        Method:         The tester will respond with an ANS tone (2100Hz) followed by simulated (a)
                        V.32 bis and (b) V.34 modem training sequences.
        Pass criteria:  The tones should either be ignored or reported back to the user. No textphone
                        modem should be selected.
        Comments:       Some high speed modems may fall back to a compatibility mode, e.g. V.21 or V.23
                        that should be correctly detected by the TUT.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_21_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_21_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_22_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_22(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.22    Immunity to fax tones
        Purpose:        To ensure that the TUT will not interpret a called fax machine as being a textphone.
        Preamble:       N/A
        Method:         The tester will respond as if it were a typical group 3 fax machine in automatic
                        answer mode. It should send a CED tone (2100Hz) plus Digital Identification
                        Signal (DIS) as defined in ITU-T T.30.
        Pass criteria:  The TUT should ignore the received tones.
        Comments:       Ideally the TUT should detect the presence of a fax machine and report it back to
                        the user.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_22_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_22_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_23_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_23(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.23    Immunity to voice
        Purpose:        To ensure that the TUT does not misinterpret speech as a valid textphone signal.
        Preamble:       N/A
        Method:         The tester will respond with sampled speech. A number of phrases recorded from
                        typical male and female speakers will be transmitted. This will include a typical
                        network announcement.
        Pass criteria:  The TUT should ignore the speech.
        Comments:       Ideally the TUT should report the presence of speech back to the user, e.g. via
                        circuit 135.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_23_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_23_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_24_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_24(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.24    ANSam signal detection
        Purpose:        To verify that TUT correctly detects the ANSam (2100Hz modulated) signal during
                        the two-second interval (Toff) between transmission of CI sequences.
        Preamble:       Make a V.18 call from the TUT.
        Method:         The Test System waits for the TUT to stop transmitting a CI and responds with an
                        ANSam signal. The V.21 demodulator is used to decode the CM sequence. ANSam
                        should be transmitted for 2s.
        Pass criteria:  1) No signal should be transmitted by TUT for 0.5s from detection of
                           ANSam.
                        2) The TUT should reply with transmission of CM as defined in 5.2.13.
                        3) Verify that CM sequence has correct bit pattern.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_24_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_24_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void org_25_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_org_25(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.2.25    V.8 calling procedure
        Purpose:        To verify that TUT correctly performs a V.8 call negotiation.
        Preamble:       Make a V.18 call from the TUT. Answer with ANSam from the Tester and with JM
                        for V.21 on the CM.
        Method:         The Test System waits for the TUT to start transmitting V.21 carrier (1).
        Pass criteria:  The TUT should connect by sending V.21 carrier (1).
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_25_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, org_25_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_01_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_01(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.1     Ta timer
        Purpose:        To ensure that on connecting the call, the DCE starts timer Ta (3s) and on
                        expiry begins probing.
        Preamble:       N/A
        Method:         The tester makes a call to the TUT and attempts to determine when the TUT
                        answers the call. It will then monitor for any signal.
        Pass criteria:  The TUT should start probing 3s after answering the call.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_01_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_01_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_02_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_02(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.2     CI signal detection
        Purpose:        To confirm the correct detection and response to the V.18 CI signal.
        Preamble:       N/A
        Method:         The tester will transmit 2 sequences of 4 CI patterns separated by 2s. It will
                        monitor for ANS and measure duration.
        Pass criteria:  1) The TUT should respond after either the first or second CI with ANSam tone.
                        2) ANSam tone should remain for 3s+-0.5s followed by silence.
        Comments:       The ANSam tone is a modulated 2100Hz tone. It may have phase reversals. The
                        XCI signal is tested in a separate test.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_02_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_02_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_03_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_03(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.3     Early termination of ANSam tone
        Purpose:        To confirm that the TUT will respond correctly to TXP signals, i.e. by stopping
                        ANSam tone on reception of TXP signal.
        Preamble:       N/A
        Method:         The tester will transmit 2 sequences of 4 CI patterns separated by 2s. On
                        reception of the ANSam tone the tester will wait 0.5s and then begin
                        transmitting the TXP signal in V.21 (1) mode.
        Pass criteria:  1) On reception of the TXP signal, the TUT should remain silent for 75+-5ms.
                        2) The TUT should then transmit 3 TXP sequences in V.21(2) mode.
                        3) The 3 TXPs should be followed by continuous 1650Hz.
                        4) Correct transmission and reception of T.140 data should be verified after the
                           V.18 mode connection is completed.
        Comments:       The TUT should indicate V.18 mode.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_03_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_03_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_04_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_04(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.4     Tt timer
        Purpose:        To ensure that after detection of ANSam the TUT will return to Monitor A after
                        timer Tt expires.
        Preamble:       Successful completion of test ANS-03.
        Method:         After completion of test ANS-03 the tester will continue to monitor for signals.
        Pass criteria:  The TUT should start probing 3s after ANSam disappears.
        Comments:       It is assumed that timer Ta is restarted on return to Monitor A.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_04_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_04_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_05_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_05(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.5     ANS tone followed by 980Hz
        Purpose:        To check correct detection of V.21 modem lower channel when preceded by answer
                        tone.
        Preamble:       N/A
        Method:         Tester transmits ANS for 2.5s followed by 75ms of no tone then transmits
                        980Hz and starts a 1s timer.
        Pass criteria:  TUT should respond with 1650Hz within 400+-100ms of start of 980Hz.
        Comments:       The TUT should indicate that V.21 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_05_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_05_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_06_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_06(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.6     ANS tone followed by 1300Hz
        Purpose:        To check correct detection of V.23 modem upper channel when preceded by answer
                        tone.
        Preamble:       N/A
        Method:         Tester transmits ANS for 2.5s followed by 75ms of no tone then transmits
                        1300Hz and starts a 2-s timer.
        Pass criteria:  TUT should respond with 390Hz after 1.7(+0.2-0.0)s of start of 1300Hz.
        Comments:       The TUT should indicate that V.23 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_06_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_06_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_07_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_07(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.7     ANS tone followed by 1650Hz
        Purpose:        To check correct detection of V.21 modem upper channel when preceded by answer
                        tone and to confirm discrimination between V.21 and V.18 modes.
        Preamble:       N/A
        Method:         Tester transmits ANS for 2.5s followed by 75ms of no tone then transmits
                        1650Hz and starts a 1-second timer.
        Pass criteria:  TUT should respond with 980Hz within 400+-100ms of start of 1650Hz.
        Comments:       The TUT should indicate that V.21 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_07_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_07_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_08_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_08(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.8     980Hz followed by 1650Hz
        Purpose:        To ensure the correct selection of V.21 modem channel when certain types of
                        Swedish textphones are encountered.
        Preamble:       N/A
        Method:         The tester will simulate a call from a Diatext2 textphone that alternates between
                        980Hz and 1650Hz until a connection is made.
        Pass criteria:  The TUT should respond with the appropriate carrier depending on when it
                        connects.
        Comments:       The TUT should indicate a V.21 connection. The time for which each frequency is
                        transmitted is random and varies between 0.64 and 2.56s.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_08_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_08_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_09_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_09(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.9     980Hz calling tone detection
        Purpose:        To confirm correct detection of 980Hz calling tones as defined in V.25.
        Preamble:       N/A
        Method:         The tester will send bursts of 980Hz signals (a) 400ms, (b) 500ms, (c) 700ms and
                        (d) 800ms followed by 1s of silence.
        Pass criteria:  1) The TUT should not respond to bursts of 400ms or 800ms.
                        2) The TUT should immediately begin probing after a burst of 980Hz for 500ms or
                           700ms followed by 1s of silence.
        Comments:       The probe sent by the TUT will depend on the country setting.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_09_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_09_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_10_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_10(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.10    V.21 detection by timer
        Purpose:        To confirm correct selection of V.21 calling modem when the received signal is not
                        modulated, i.e. there is no 1180Hz.
        Preamble:       N/A
        Method:         The tester sends 980Hz to TUT for 2s.
        Pass criteria:  The TUT should respond with a 1650Hz tone in 1.5+-0.1s.
        Comments:       The TUT should indicate that V.21 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_10_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_10_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_11_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_11(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.11    EDT detection by rate
        Purpose:        To confirm detection of EDT modems by detecting the transmission rate of received
                        characters.
        Preamble:       N/A
        Method:         The tester transmits EDT characters "abcdef" to TUT at 110 bit/s. When TUT
                        indicates that the connection is established, type characters "abcdef<CR>" back to
                        the tester. The same characters will then be transmitted back to the TUT.
        Pass criteria:  Ensure correct reception of characters by tester and TUT.
        Comments:       The TUT should indicate that EDT mode has been selected. Some characters may
                        be lost during the detection process. However, the number lost should be minimal.
                        The data bits and parity are specified in Annex C.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_11_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_11_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_12_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_12(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.12    V.21 Detection by rate
        Purpose:        To confirm detection of V.21 modem low channel by detecting the transmission rate
                        of received characters and to ensure correct discrimination between V.18 and V.21
                        modes.
        Preamble:       N/A
        Method:         The tester transmits characters "abcdef" to TUT using V.21 (1) at 300 bit/s. When
                        TUT indicates that the connection is established, type characters "abcdef<CR>"
                        back to the tester. The same characters will then be transmitted back to the TUT.
        Pass criteria:  Ensure correct reception of characters by tester and TUT.
        Comments:       This situation is unlikely to occur in practice unless the DCE is sending a V.21
                        (1650Hz) probe. However, it is catered for in V.18. It is more likely that this is
                        where CI or TXP characters would be detected (see test ANS-02).
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_12_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_12_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_13_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_13(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.13    Tr timer
        Purpose:        To ensure that the TUT returns to the Monitor A state on expiry of timer Tr
                        (2s). Timer Tr is started when a modulated V.21 (1) signal is detected.
        Preamble:       N/A
        Method:         The tester will transmit 980Hz for 200ms followed by alternating 980Hz/1180Hz
                        at 110 bit/s for 100ms followed by 980Hz for 1s.
        Pass criteria:  The TUT should begin probing 4+-0.5s after the 980Hz signal is removed.
        Comments:       It is not possible to be precise on timings for this test since the definition of a
                        "modulated signal" as in 5.2.4.4 is not specified. Therefore it is not known exactly
                        when timer Tr will start. It is assumed that timer Ta is restarted on re-entering the
                        Monitor A state.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_13_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_13_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_14_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_14(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.14    Te timer
        Purpose:        To ensure that the TUT returns to the Monitor A on expiry of timer Te
                        (2.7s). Timer Te is started when a 980Hz signal is detected.
        Preamble:       N/A
        Method:         The tester will transmit 980Hz for 200ms followed silence for 7 s.
        Pass criteria:  The TUT should begin probing 5.5+-0.5s after the 980Hz signal is removed.
        Comments:       It is assumed that timer Ta (3s) is restarted on re-entering the Monitor A
                        state.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_14_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_14_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_15_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_15(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.15 5  Bit mode (Baudot) detection tests
        Purpose:        To confirm detection of Baudot modulation at various bit rates that may be
                        encountered.
        Preamble:       N/A
        Method:         The tester transmits the 5-bit coded characters "0" to "9" followed by "abcdef" at
                        (a) 45.45, (b) 47.6, (c) 50 and (d) 100 bits per second. When TUT indicates a
                        connection, type at least 5 characters back to the tester so that correct selection of bit
                        rate can be confirmed.
        Pass criteria:  1) The TUT should select Baudot mode and the appropriate bit rate.
                        2) The tester will analyse the bit rate of received characters, which should be at an
                           appropriate rate, and confirm the carrier on/off times before and after the
                           characters.
        Comments:       45.45 and 50 bit/s are the commonly used Baudot bit rates. However, some
                        textphones can transmit at higher rates, e.g. 100 bit/s. Responding at either 45.45 or
                        50 bit/s is acceptable to these devices which then fall back to the selected rate.
                        A rate of 47.6 bit/s may be encountered from another V.18 textphone in the
                        automode answer state. The TUT may then select either 45.45 or 50 bit/s for the
                        transmission.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_15_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_15_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_16_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_16(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.16    DTMF signal detection
        Purpose:        To verify whether the TUT correctly recognizes DTMF signals.
        Preamble:       N/A
        Method:         The tester will send a single DTMF tone of 40ms duration to TUT. When TUT
                        indicates a connection, type at least 5 characters back to the tester so that correct
                        selection of mode can be confirmed.
        Pass criteria:  Tester will analyse the received characters to confirm DTMF mode selection.
        Comments:       The TUT should indicate that it has selected DTMF mode. The DTMF capabilities
                        of the TUT should comply with ITU-T Q.24 for the Danish Administration.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_16_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_16_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_17_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_17(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.17    Bell 103 (1270Hz signal) detection
        Purpose:        To ensure correct detection and selection of Bell 103 modems.
        Preamble:       N/A
        Method:         The tester sends 1270Hz to TUT for 5s.
        Pass criteria:  TUT should respond with 2225Hz tone after 0.7+-0.1s.
        Comments:       The TUT should indicate that Bell 103 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_17_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_17_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_18_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_18(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.18    Bell 103 (2225Hz signal) detection
        Purpose:        To ensure correct detection and selection of Bell 103 modems in reverse mode.
        Preamble:       N/A
        Method:         The tester sends 2225Hz to TUT for 5s.
        Pass criteria:  The TUT should respond with 1270Hz after 1+-0.2s.
        Comments:       The TUT should indicate that Bell 103 mode has been selected. Bell 103 modems
                        use 2225Hz as both answer tone and higher frequency of the upper channel.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_18_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_18_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_19_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_19(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.19    V.21 Reverse mode (1650Hz) detection
        Purpose:        To ensure correct detection and selection of V.21 reverse mode.
        Preamble:       N/A
        Method:         The tester sends 1650Hz to TUT for 5s.
        Pass criteria:  The TUT should respond with 980Hz after 0.4+-0.2s.
        Comments:       The TUT should indicate that V.21 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_19_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_19_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_20_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_20(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.20    1300Hz calling tone discrimination
        Purpose:        To confirm correct detection of 1300Hz calling tones as defined in ITU-T V.25.
        Preamble:       N/A
        Method:         The tester will send 1300Hz bursts of (a) 400ms, (b) 500ms, (c) 700ms and
                        (d) 800ms followed by 1s of silence.
        Pass criteria:  1) The TUT should not respond to bursts of 400ms or 800ms.
                        2) The TUT should immediately begin probing after a burst of 1300Hz for 500ms or
                           700ms followed by 1s of silence.
        Comments:       The probe sent by the TUT will depend on the country setting.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_20_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_20_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_21_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_21(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.21    V.23 Reverse mode (1300Hz) detection
        Purpose:        To ensure correct detection and selection of V.23 reverse mode.
        Preamble:       N/A
        Method:         The tester sends 1300Hz only, with no XCI signals, to TUT for 5s.
                        Pass criteria: The TUT should respond with 390Hz after 1.7+-0.1s.
        Comments:       The TUT should indicate that V.23 mode has been selected.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_21_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_21_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_22_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_22(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.22    1300Hz with XCI test
        Purpose:        To ensure correct detection of the XCI signal and selection of V.18 mode.
        Preamble:       N/A
        Method:         The tester sends XCI signal as defined in 3.11. On reception of ANS it will become
                        silent for 500ms then transmit the TXP signal in V.21 (1) mode.
        Pass criteria:  The TUT should respond with TXP using V.21 (2) and select V.18 mode.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_22_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_22_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_23_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_23(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.23    Stimulate mode country settings
        Purpose:        To ensure that the TUT steps through the probes in the specified order for the
                        country selected.
        Preamble:       The TUT should be configured for each of the possible probe orders specified in
                        Appendix I in turn.
        Method:         The tester will call the TUT, wait for Ta to expire and then monitor the probes sent
                        by the TUT.
        Pass criteria:  The TUT should use the orders described in Appendix I.
        Comments:       The order of the probes is not mandatory.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_23_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_23_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_24_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_24(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.24    Stimulate carrierless mode probe message
        Purpose:        To ensure that the TUT sends the correct probe message for each of the carrierless
                        modes.
        Preamble:       N/A
        Method:         The tester will call the TUT, wait for Ta to expire and then monitor the probes sent
                        by the TUT.
        Pass criteria:  The TUT should send the user defined probe message for Annexes A, B, and C
                        modes followed by a pause of Tm (default 3)s.
        Comments:       The carrierless modes are those described in Annexes A, B and C.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_24_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_24_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_25_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_25(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.25    Interrupted carrierless mode probe
        Purpose:        To ensure that the TUT continues probing from the point of interruption a maximum
                        of 20s after a failed connect attempt.
        Preamble:       The TUT should be configured for the UK country setting.
        Method:         The tester will call the TUT, wait for Ta to expire and then during the pause after
                        the first Baudot probe it will send a 200ms burst of 1270Hz followed by silence
                        for 30s.
        Pass criteria:  The TUT should transmit silence on detecting the 1270Hz tone and then continue
                        probing starting with the V.23 probe 20s after the end of the 1270Hz signal.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_25_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_25_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_26_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_26(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.26    Stimulate carrier mode probe time
        Purpose:        To ensure that the TUT sends each carrier mode for time Tc (default 6s)
                        preceded by the correct answer tone.
        Preamble:       None.
        Method:         The tester will call the TUT, wait for Ta to expire and then monitor the probes sent
                        by the TUT.
        Pass criteria:  The TUT should send the ANS tone (2100Hz) for 1s followed by silence for
                        75+-5ms and then the 1650Hz, 1300Hz and 2225Hz probes for time Tc.
        Comments:       The carrier modes are those described in Annexes D, E, and F.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_26_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_26_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_27_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_27(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.27    V.23 mode (390Hz) detection
        Purpose:        To confirm correct selection of V.23 mode.
        Preamble:       N/A
        Method:         The tester waits until the 1300Hz probe is detected from the TUT and then
                        transmits 390Hz for 11s.
        Pass criteria:  1) After 3s of the 390Hz signal the TUT should indicate that V.23 has
                           been selected.
                        2) The tester will confirm that the 1300Hz carrier is maintained for at least
                           4s beyond the normal probe duration, i.e. Tc (= 6s default) + 4s =
                           10s total.
        Comments:       All known V.23 devices need to receive 1300Hz tone before they will respond with
                        390Hz. When the 1300Hz probe is not being transmitted, a 390Hz tone may be
                        interpreted as a 400Hz network tone.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_27_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_27_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_28_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_28(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.28    Interrupted carrier mode probe
        Purpose:        To ensure that the TUT continues probing from the point of interruption a maximum
                        of 4s after a failed connect attempt.
        Preamble:       The TUT should be configured for the UK country setting.
        Method:         The tester will call the TUT, wait for Ta to expire and then during the first V.21
                        probe it will send a 200ms burst of 1270Hz followed by silence for 30s.
        Pass criteria:  The TUT should transmit silence on detecting the 1270Hz tone and then continue
                        probing with the Baudot stored message 4s after the end of the 1270Hz
                        burst.
        Comments:       It is most likely that the TUT will return to probing time Ta (3s) after the
                        1270Hz tone ceases. This condition needs further clarification.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_28_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_28_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_29_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_29(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.29    Stimulate mode response during probe
        Purpose:        To ensure that the TUT is able to detect an incoming signal while transmitting a
                        carrier mode probe.
        Preamble:       N/A
        Method:         The tester will step through each possible response as defined in tests ANS-08 to
                        ANS-23 for each of the carrier mode probes and for each pause after a carrierless
                        mode probe message.
        Pass criteria:  The TUT should respond as described in the appropriate test above.
        Comments:       The TUT may not respond to any signals while a carrierless mode probe is being
                        sent since these modes are half duplex.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_29_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_29_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_30_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_30(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.30    Immunity to network tones
        Purpose:        To ensure that the TUT does not interpret network tones as valid signals.
        Preamble:       N/A
        Method:         The tester will first send a busy tone to the TUT this will be followed by a number
                        unobtainable tone. The frequencies and cadences of the tones will vary according to
                        the country setting. The tester must be configured for the same country as the TUT.
        Pass criteria:  The countries supported by the TUT should be noted along with the response to
                        each tone. The tones should either be ignored or reported as the relevant network
                        tone to the user.
        Comments:       V.18 is required to recognize and report RINGING and BUSY tones. Other network
                        tones may be ignored. Some devices may only provide a visual indication of the
                        presence and cadence of the tones for instance by a flashing light.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_30_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_30_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_31_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_31(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.31    Immunity to fax calling tones
        Purpose:        To determine whether the TUT can discriminate fax calling tones.
        Preamble:       N/A
        Method:         The tester will call the TUT and send the fax calling tone, CNG. This is an 1100Hz
                        tone with cadence of 0.5s ON and 3s OFF as defined in ITU-T T.30.
        Pass criteria:  The TUT should not respond to this signal and may report it as being a calling fax
                        machine.
        Comments:       This is an optional test as detection of the fax calling tone is not required by
                        ITU-T V.18.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_31_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_31_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_32_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_32(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.32    Immunity to voice
        Purpose:        To ensure that the TUT does not misinterpret speech as a valid textphone signal.
        Preamble:       N/A
        Method:         The tester will respond with sampled speech. A number of phrases recorded from
                        typical male and female speakers will be transmitted. This will include a typical
                        network announcement.
        Pass criteria:  The TUT should ignore the speech.
        Comments:       Ideally the TUT should report the presence of speech back to the user. This is an
                        optional test.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_32_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_32_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void ans_33_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_ans_33(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.3.33    CM detection and V.8 answering
        Purpose:        To confirm that the TUT will respond correctly to CM signals and connect
                        according to V.8 procedures.
        Preamble:       N/A
        Method:         The tester will transmit 2 sequences of 4 CI patterns separated by 2s. On
                        reception of the ANSam tone the tester will wait 0.5s and then begin
                        transmitting the CM signal with textphone and V.21 specified.
        Pass criteria:  1) On reception of the CM signal, the TUT should transmit JM with textphone
                           and V.21.
                        2) The TUT should then transmit in V.21 (2) mode.
                        3) The JM should be followed by continuous 1650Hz.
                        4) Correct transmission and reception of T.140 data should be verified after the
                           V.18 mode connection is completed.
        Comments:       The TUT should indicate V.18 mode.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_33_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, ans_33_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void mon_01_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_mon_01(void)
{
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void mon_21_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_mon_21(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.4.1     Automode monitor Ta timer test
        Purpose:        To ensure that on entering monitor mode, timer Ta (3s) is not active and that
                        the TUT does not enter the probing state.
        Preamble:       N/A
        Method:         The TUT should be put into monitor state. The tester will then monitor for signals
                        for 1 minute.
        Pass criteria:  The TUT should not start probing.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, mon_21_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, mon_21_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void mon_22_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_mon_22(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.4.2     Automode monitor 1300Hz calling tone discrimination
        Purpose:        To confirm correct detection and reporting of 1300Hz calling tones as defined in
                        ITU-T V.25.
        Preamble:       N/A
        Method:         The tester will send 1300Hz bursts of (a) 400ms, (b) 500ms, (c) 700ms and
                        (d) 800ms followed by 1s of silence.
        Pass criteria:  1) The TUT should not respond to bursts of 400ms or 800ms.
                        2) The TUT should report detection of calling tones to the DTE after a burst of
                           1300Hz for 500ms or 700ms followed by 1s of silence.
        Comments:       In automode answer, the 1300Hz calling causes the DCE to start probing. In
                        monitor mode it should only report detection to the DTE.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, mon_22_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, mon_22_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void mon_23_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_mon_23(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.4.3     Automode monitor 980Hz calling tone discrimination
        Purpose:        To confirm correct detection and reporting of 980Hz calling tones as defined in
                        ITU-T V.25.
        Preamble:       N/A
        Method:         The tester will send 980Hz bursts of (a) 400ms, (b) 500ms, (c) 700ms and
                        (d) 800ms followed by 1s of silence.
        Pass criteria:  1) The TUT should not respond to bursts of 400ms or 800ms.
                        2) The TUT should report detection of calling tones to the DTE after a burst of
                           980Hz for 500ms or 700ms followed by 1s of silence.
        Comments:       In automode answer, the 980Hz calling causes the DCE to start probing. In monitor
                        mode it should only report detection to the DTE.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, mon_23_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, mon_23_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void x_01_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
printf("1-1 %d '%s'\n", len, msg);
    if (user_data == NULL)
        strcat(result[1], (const char *) msg);
    //else
    //    v18_put(v18[1], "abcdefghij", 10);
}
/*- End of function --------------------------------------------------------*/

static int test_x_01(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;
    const char *ref;

    /*
        III.5.4.5.1     Baudot carrier timing and receiver disabling
        Purpose:        To verify that the TUT sends unmodulated carrier for 150ms before a new character
                        and disables its receiver for 300ms after a character is transmitted.
        Preamble:       Establish a call between the tester and TUT in Baudot mode.
        Method:         The operator should send a single character from the TUT. The tester will
                        immediately start sending a unique character sequence. Examination of the TUT
                        display will show when its receiver is re-enabled.
        Pass criteria:  1) The TUT should send unmodulated carrier for 150ms before the beginning of
                           the start bit.
                        2) The receiver should be re-enabled after 300ms.
                        3) The tester will confirm that 1 start bit and at least 1.5 stop bits are used.
        Comments:       The carrier should be maintained during the 300ms after a character.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_5BIT_4545, V18_AUTOMODING_GLOBAL, x_01_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_5BIT_4545, V18_AUTOMODING_GLOBAL, x_01_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    v18_put(v18[0], "z", 1);
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    ref = "cdefghij";
    printf("Result:\n%s\n", result[0]);
    printf("Reference result:\n%s\n", ref);
    if (unexpected_echo  ||  strcmp(result[1], ref) != 0)
        return -1;
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void x_02_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_x_02(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.5.2     Baudot bit rate confirmation
        Purpose:        To verify that the TUT uses the correct bit rates in the Baudot mode.
        Preamble:       Establish a call between the tester and TUT in Baudot mode for each of the two
                        tests.
        Method:         The operator should select Baudot (a) 45 bit/s followed by (b) 50 bit/s modes and
                        transmit the string "abcdef" at each rate.
        Pass criteria:  The tester will measure the bit timings and confirm the rates.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_5BIT_4545, V18_AUTOMODING_GLOBAL, x_02_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_5BIT_4545, V18_AUTOMODING_GLOBAL, x_02_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void x_03_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_x_03(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.5.3     Baudot probe bit rate confirmation
        Purpose:        To verify that the TUT uses the correct bit rates in the Baudot mode probe during
                        automoding.
        Preamble:       Set the user defined carrierless mode probe message to the string "abcdef" if
                        possible. Set the TUT country setting to "United States". A call should be initiated
                        from the tester to the TUT.
        Method:         The tester will wait for the Baudot mode probe and measure the bit rate.
        Pass criteria:  The tester will measure the bit timings and confirm the rate of 47.6 bit/s.
        Comments:       The probe message must be long enough for the tester to establish the bit rate. "GA"
                        may not be sufficient.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_5BIT_4545, V18_AUTOMODING_USA, x_03_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_5BIT_4545, V18_AUTOMODING_USA, x_03_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void x_04_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
    if (user_data == NULL)
    {
        strcat(result[0], (const char *) msg);
printf("Unexpected ECHO received (%d) '%s'\n", len, msg);
        unexpected_echo = TRUE;
    }
    else
    {
printf("1-1 %d '%s'\n", len, msg);
        strcat(result[1], (const char *) msg);
        /* Echo each received character */
        //v18_put(v18[1], msg, len);
    }
}
/*- End of function --------------------------------------------------------*/

static int test_x_04(void)
{
    char msg[1024];
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int i;
    int j;

    /*
        III.5.4.5.4     5 Bit to T.50 character conversion
        Purpose:        To check that the character conversion tables in Annex A have been correctly
                        implemented.
        Preamble:       Establish a call between the tester and TUT in Baudot mode at 45 bit/s.
        Method:         The tester will send all possible characters preceded by the relevant case shift
                        command one at a time and wait for a response from the TUT operator. Each
                        character should be responded to at the TUT by typing the received character or
                        <CR> if the character is not available.
        Pass criteria:  1) The tester will verify that each character is correctly echoed back by the TUT.
                           The operator should verify that each character is correctly displayed on the TUT.
                        2) The TUT will send the LTRS symbol before its first character and the
                           appropriate mode character (either LTRS or FIGS) after every 72 subsequent
                           characters.
        Comments:       The tester should indicate which character has been sent in each case. Some of the
                        characters may not be available from the TUT keyboard and can be ignored. It is
                        assumed that the character conversion is the same for Baudot at 50 bit/s and any
                        other supported speed.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_5BIT_4545 | V18_MODE_REPETITIVE_SHIFTS_OPTION, V18_AUTOMODING_GLOBAL, x_04_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_5BIT_4545 | V18_MODE_REPETITIVE_SHIFTS_OPTION, V18_AUTOMODING_GLOBAL, x_04_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    unexpected_echo = FALSE;
    for (i = 0;  i < 127;  i++)
        msg[i] = i + 1;
    msg[127] = '\0';
    v18_put(v18[0], msg, 127);

    for (i = 0;  i < 2000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK);
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Result:\n%s\n", result[0]);
    printf("Result:\n%s\n", result[1]);
    printf("Reference result:\n%s\n", full_baudot_rx);
    if (unexpected_echo  ||  strcmp(result[1], full_baudot_rx) != 0)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void x_05_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
    if (user_data == NULL)
        strcat(result[0], (const char *) msg);
    else
        v18_put(v18[1], "behknqtwz", 9);
}
/*- End of function --------------------------------------------------------*/

static int test_x_05(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;
    const char *ref;

    /*
        III.5.4.5.5     DTMF receiver disabling
        Purpose:        To verify that the TUT disables its DTMF receiver for 300ms when a character is
                        transmitted.
        Preamble:       Establish a call between the tester and TUT in DTMF mode.
        Method:         The operator should send a single "e" character from the TUT which will result in
                        sending a single DTMF tone to the tester. The tester will immediately start sending a
                        unique character sequence using single DTMF tones. Examination of the TUT
                        display will show when its receiver is re-enabled.
        Pass criteria:  The receiver should be re-enabled after 300ms.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_05_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_05_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    v18_put(v18[0], "e", 1);

    for (i = 0;  i < 1000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    ref = "knqtwz";
    printf("Result:\n%s\n", result[0]);
    printf("Reference result:\n%s\n", ref);
    if (strcmp(result[0], ref) != 0)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void x_06_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
    if (user_data == NULL)
        ;
    else
        strcat(result[1], (const char *) msg);
}
/*- End of function --------------------------------------------------------*/

static int test_x_06(void)
{
    char msg[128];
    const char *ref;
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.5.6     DTMF character conversion
        Purpose:        To check that the character conversion tables in Annex B have been correctly
                        implemented.
        Preamble:       Establish a call between the tester and TUT in DTMF mode.
        Method:         The tester will send each character from the set in Annex B, waiting for a response
                        after each one. Each character should be responded to at the TUT by typing the
                        same character.
        Pass criteria:  The tester will verify that each character is correctly echoed back by the TUT.
        Comments:       The conversion table is specified in Annex B. The receiver at the tester may be
                        re-enabled 100ms after transmission of each character to maximize likelihood of
                        receiving character from the TUT. It is assumed that the echo delay in the test
                        system is negligible.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_06_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_06_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 127;  i++)
        msg[i] = i + 1;
    msg[127] = '\0';
    v18_put(v18[0], msg, 127);

    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    ref = "\b \n\n\n?\n\n\n  !%+().+,-.0123456789:;(=)?"
          "XABCDEFGHIJKLMNOPQRSTUVWXYZ\xC6\xD8\xC5"
          " abcdefghijklmnopqrstuvwxyz\xE6\xF8\xE5 \b";

    printf("Result:\n%s\n", result[0]);
    printf("Reference result:\n%s\n", ref);
    v18_free(v18[0]);
    v18_free(v18[1]);
    if (strcmp(result[1], ref) != 0)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void x_07_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_x_07(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.5.7     EDT carrier timing and receiver disabling
        Purpose:        To verify that the TUT sends unmodulated carrier for 300ms before a character and
                        disables its receiver for 300ms after a character is transmitted.
        Preamble:       Establish a call between the tester and TUT in EDT mode.
        Method:         The operator should send a single character from the TUT. The tester will
                        immediately start sending a unique character sequence. Examination of the TUT
                        display will show when its receiver is re-enabled.
        Pass criteria:  1) The TUT should send unmodulated carrier for 300ms before the beginning of
                           the start bit.
                        2) The receiver should be re-enabled after 300ms.
                        3) The tester will confirm that 1 start bit and at least 1.5 stop bits are used.
        Comments:       The carrier should be maintained during the 300ms after a character.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_07_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_07_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void x_08_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_x_08(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.5.8     EDT bit rate and character structure
        Purpose:        To verify that the TUT uses the correct bit rate and character structure in the EDT
                        mode.
        Preamble:       Establish a call between the tester and TUT in EDT mode.
        Method:         The operator should transmit the string "abcdef" from the TUT.
        Pass criteria:  1) The tester should measure the bit timings and confirm that the rate is 110 bit/s.
                        2) The tester should confirm that 1 start bit, 7 data bits, 1 even parity bit and 2 stop
                           bits are used.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_08_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_08_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void x_09_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_x_09(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.5.9     V.23 calling mode character format
        Purpose:        To verify that the TUT uses the correct character format in the V.23 calling mode.
        Preamble:       Establish a call from the TUT to the tester in V.23 mode.
        Method:         The operator should transmit the string "abcdef" from the TUT. The tester will echo
                        characters back to the TUT as they are received. The tester will then transmit the
                        string "abcdef" with ODD parity to the TUT.
        Pass criteria:  1) Confirm that 1 start bit, 7 data bits, 1 even parity bit and 2 stop bits are
                           transmitted.
                        2) The operator should confirm that there is no local echo at the TUT by checking
                           that there are no duplicate characters on the TUT display.
                        3) The received string should be correctly displayed despite the incorrect parity.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_09_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_09_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void x_10_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_x_10(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.5.10    V.23 answer mode character format
        Purpose:        To verify that the TUT uses the correct character format in the V.23 answer mode.
        Preamble:       Establish a call from the tester to the TUT in V.23 mode.
        Method:         The tester will transmit the string "abcdef" with ODD parity. The TUT should echo
                        characters back to the tester as they are received. The operator should then transmit
                        the string "abcdef" from the TUT.
        Pass criteria:  1) The received string should be correctly displayed at the TUT despite the
                           incorrect parity.
                        2) Confirm that 1 start bit, 7 data bits, 1 even parity bit and 2 stop bits are
                           transmitted by the TUT.
                        3) The tester should confirm that there is remote echo from TUT.
                        4) The operator should confirm that there is local echo on the TUT.
        Comments:       This test is only applicable to Minitel Dialogue terminals. Prestel and Minitel
                        Normal terminals cannot operate in this mode.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_10_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_10_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void x_11_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_x_11(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.5.11    V.21 character structure
        Purpose:        To verify that the TUT uses the character structure in the V.21 mode.
        Preamble:       Establish a call from the TUT to the tester in V.21 mode.
        Method:         The operator should transmit a string from the TUT that is long enough to cause the
                        display to word wrap followed by "abcdef", new line (CR+LF). The tester will then
                        transmit the string "123456", BACKSPACE (0/8) with ODD parity to the TUT.
        Pass criteria:  1) The tester should confirm that 1 start bit, 7 data bits, 1 even parity bit and 1 stop
                           bits are transmitted.
                        2) The word wrap should not result in CR+LF.
                        3) The forced new line should be indicated by CR+LF.
                        4) The last five characters on the TUT display should be "12345" (no "6")
                           correctly displayed despite the incorrect parity.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_11_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_11_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void x_12_put_text_msg(void *user_data, const uint8_t *msg, int len)
{
}
/*- End of function --------------------------------------------------------*/

static int test_x_12(void)
{
    logging_state_t *logging;
    int16_t amp[2][SAMPLES_PER_CHUNK];
    int16_t model_amp[2][SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int outframes;
    int samples;
    int push;
    int i;
    int j;

    /*
        III.5.4.5.12    V.18 mode
        Purpose:        To verify that the TUT uses the protocol defined in ITU-T T.140.
        Preamble:       Establish a call from the TUT to the tester in V.18 mode.
        Method:         The operator should transmit a string from the TUT that is long enough to cause the
                        display to word wrap followed by "abcdef", new line (CR+LF), new line
                        (UNICODE preferred). The tester will then transmit the string "123456",
                        BACKSPACE.
        Pass criteria:  The tester should confirm UTF8 encoded UNICODE characters are used with the
                        controls specified in ITU-T T.140.
     */
    v18[0] = v18_init(NULL, TRUE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_12_put_text_msg, (void *) (intptr_t) 0);
    logging = v18_get_logging_state(v18[0]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "Tester");
    v18[1] = v18_init(NULL, FALSE, V18_MODE_DTMF, V18_AUTOMODING_GLOBAL, x_12_put_text_msg, (void *) (intptr_t) 1);
    logging = v18_get_logging_state(v18[1]);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "TUT");

    if ((model = both_ways_line_model_init(line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no,
                                           (float) noise_level,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }

    result[0][0] =
    result[1][0] = '\0';
    for (i = 0;  i < 10000;  i++)
    {
        for (j = 0;  j < 2;  j++)
        {
            if ((samples = v18_tx(v18[j], amp[j], SAMPLES_PER_CHUNK)) == 0)
                push = 10;
            if (samples < SAMPLES_PER_CHUNK)
            {
                vec_zeroi16(&amp[j][samples], SAMPLES_PER_CHUNK - samples);
                samples = SAMPLES_PER_CHUNK;
            }
        }
        if (log_audio)
        {
            for (j = 0;  j < samples;  j++)
            {
                out_amp[2*j + 0] = amp[0][j];
                out_amp[2*j + 1] = amp[1][j];
            }
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
#if 1
        both_ways_line_model(model,
                             model_amp[0],
                             amp[0],
                             model_amp[1],
                             amp[1],
                             samples);
#else
        vec_copyi16(model_amp[0], amp[0], samples);
        vec_copyi16(model_amp[1], amp[1], samples);
#endif
        v18_rx(v18[0], model_amp[1], samples);
        v18_rx(v18[1], model_amp[0], samples);
    }

    v18_free(v18[0]);
    v18_free(v18[1]);
    printf("Test not yet implemented\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void put_v18_msg(void *user_data, const uint8_t *msg, int len)
{
    char buf[1024];

    memcpy(buf, msg, len);
    buf[len] = '\0';
    printf("Received (%d bytes) '%s'\n", len, buf);
}
/*- End of function --------------------------------------------------------*/

static int decode_test_data_file(int mode, const char *filename)
{
    int16_t amp[SAMPLES_PER_CHUNK];
    SNDFILE *inhandle;
    int len;
    v18_state_t *v18_state;
    logging_state_t *logging;

    printf("Decoding as '%s'\n", v18_mode_to_str(mode));
    /* We will decode the audio from a file. */
    if ((inhandle = sf_open_telephony_read(decode_test_file, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", decode_test_file);
        exit(2);
    }
    v18_state = v18_init(NULL, FALSE, mode, V18_AUTOMODING_GLOBAL, put_v18_msg, NULL);
    logging = v18_get_logging_state(v18_state);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "");
    for (;;)
    {
        if ((len = sf_readf_short(inhandle, amp, SAMPLES_PER_CHUNK)) <= 0)
            break;
        v18_rx(v18_state, amp, len);
    }
    if (sf_close_telephony(inhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", decode_test_file);
        exit(2);
    }
    v18_free(v18_state);
    return 0;
}
/*- End of function --------------------------------------------------------*/

const struct
{
    const char *title;
    int (*func)(void);
} test_list[] =
{
    {"III.3.2.1 Operational requirements tests", NULL},
    {"MISC-01         4 (1)       No disconnection test", test_misc_01},
    {"MISC-02         4 (2)       Automatic resumption of automoding", test_misc_02},
    {"MISC-03         4 (2)       Retention of selected mode on loss of signal", test_misc_03},
    {"MISC-04         4 (4)       Detection of BUSY tone", test_misc_04},
    {"MISC-05         4 (4)       Detection of RINGING", test_misc_05},
    {"MISC-06         4 (4)       LOSS OF CARRIER indication", test_misc_06},
    {"MISC-07         4 (4)       Call progress indication", test_misc_07},
    {"MISC-08         4 (5)       Circuit 135 test", test_misc_08},
    {"MISC-09         4 (6)       Connection procedures", test_misc_09},

    {"III.3.2.2 Automode originate tests", NULL},
    {"ORG-01          5.1.1       CI & XCI signal coding and cadence", test_org_01},
    {"ORG-02          5.1.3       ANS signal detection", test_org_02},
    {"ORG-03          5.2.3.1     End of ANS signal detection", test_org_03},
    {"ORG-04          5.1.3.2     ANS tone followed by TXP", test_org_04},
    {"ORG-05          5.1.3.3     ANS tone followed by 1650Hz", test_org_05},
    {"ORG-06          5.1.3.4     ANS tone followed by 1300Hz", test_org_06},
    {"ORG-07          5.1.3       ANS tone followed by no tone", test_org_07},
    {"ORG-08          5.1.4       Bell 103 (2225Hz signal) detection", test_org_08},
    {"ORG-09          5.1.5       V.21 (1650Hz signal) detection", test_org_09},
    {"ORG-10          5.1.6       V.23 (1300Hz signal) detection", test_org_10},
    {"ORG-11          5.1.7       V.23 (390Hz signal) detection", test_org_11},
    {"ORG-12a to d    5.1.8       5 Bit Mode (baudot) detection Tests", test_org_12},
    {"ORG-13          5.1.9       DTMF signal detection", test_org_13},
    {"ORG-14          5.1.10      EDT rate detection", test_org_14},
    {"ORG-15          5.1.10.1    Rate detection test", test_org_15},
    {"ORG-16          5.1.10.2    980Hz detection", test_org_16},
    {"ORG-17          5.1.10.3    Loss of signal after 980Hz", test_org_17},
    {"ORG-18          5.1.10.3    Tr Timer", test_org_18},
    {"ORG-19          5.1.11      Bell 103 (1270Hz signal) detection", test_org_19},
    {"ORG-20                      Immunity to network tones", test_org_20},
    {"ORG-21a to b                Immunity to other non-textphone modems", test_org_21},
    {"ORG-22                      Immunity to Fax tones", test_org_22},
    {"ORG-23                      Immunity to voice", test_org_23},
    {"ORG-24          5.1.2       ANSam detection", test_org_24},
    {"ORG-25          6.1         V.8 originate call", test_org_25},

    {"III.3.2.3 Automode answer tests", NULL},
    {"ANS-01          5.2.1       Ta timer", test_ans_01},
    {"ANS-02          5.2.2       CI signal detection", test_ans_02},
    {"ANS-03          5.2.2.1     Early termination of ANS tone", test_ans_03},
    {"ANS-04          5.2.2.2     Tt Timer", test_ans_04},
    {"ANS-05          5.2.3.2     ANS tone followed by 980Hz", test_ans_05},
    {"ANS-06          5.2.3.2     ANS tone followed by 1300Hz", test_ans_06},
    {"ANS-07          5.2.3.3     ANS tone followed by 1650Hz", test_ans_07},
    {"ANS-08          5.2.4.1     980Hz followed by 1650Hz", test_ans_08},
    {"ANS-09a to d    5.2.4.2     980Hz calling tone detection", test_ans_09},
    {"ANS-10          5.2.4.3     V.21 detection by timer", test_ans_10},
    {"ANS-11          5.2.4.4.1   EDT detection by rate", test_ans_11},
    {"ANS-12          5.2.4.4.2   V.21 detection by rate", test_ans_12},
    {"ANS-13          5.2.4.4.3   Tr Timer", test_ans_13},
    {"ANS-14          5.2.4.5     Te Timer", test_ans_14},
    {"ANS-15a to d    5.2.5       5 bit mode (Baudot) detection tests", test_ans_15},
    {"ANS-16          5.2.6       DTMF signal detection", test_ans_16},
    {"ANS-17          5.2.7       Bell 103 (1270Hz signal) detection", test_ans_17},
    {"ANS-18          5.2.8       Bell 103 (2225Hz signal) detection", test_ans_18},
    {"ANS-19          5.2.9       V.21 reverse mode (1650Hz) detection", test_ans_19},
    {"ANS-20a to d    5.2.10      1300Hz calling tone discrimination", test_ans_20},
    {"ANS-21          5.2.11      V.23 reverse mode (1300Hz) detection", test_ans_21},
    {"ANS-22                      1300Hz with XCI Test", test_ans_22},
    {"ANS-23          5.2.12      Stimulate mode country settings", test_ans_23},
    {"ANS-24          5.2.12.1    Stimulate carrierless mode probe message", test_ans_24},
    {"ANS-25          5.2.12.1.1  Interrupted carrierless mode probe", test_ans_25},
    {"ANS-26          5.2.12.2    Stimulate carrier mode probe time", test_ans_26},
    {"ANS-27          5.2.12.2.1  V.23 mode (390Hz) detection", test_ans_27},
    {"ANS-28          5.2.12.2.2  Interrupted carrier mode probe", test_ans_28},
    {"ANS-29          5.2.12.2.2  Stimulate mode response during probe", test_ans_29},
    {"ANS-30                      Immunity to network tones", test_ans_30},
    {"ANS-31                      Immunity to Fax calling tones", test_ans_31},
    {"ANS-32                      Immunity to voice", test_ans_32},
    {"ANS-33          5.2.2.1     V.8 CM detection and V.8 answering", test_ans_33},

    {"III.3.2.4 Automode monitor tests", NULL},
    {"MON-01 to -20   5.3         Repeat all answer mode tests excluding tests ANS-01, ANS-20 and ANS-23 to ANS-29", test_mon_01},
    {"MON-21          5.3         Automode monitor Ta timer", test_mon_21},
    {"MON-22a to d    5.3         Automode monitor 1300Hz calling tone discrimination", test_mon_22},
    {"MON-23a to d    5.3         Automode monitor 980Hz calling tone discrimination", test_mon_23},

    {"III.3.2.5 ITU-T V.18 annexes tests", NULL},
    {"X-01            A.1         Baudot carrier timing and receiver disabling", test_x_01},
    {"X-02            A.2         Baudot bit rate confirmation", test_x_02},
    {"X-03            A.3         Baudot probe bit rate confirmation", test_x_03},
    {"X-04            A.4         5 Bit to T.50 character conversion", test_x_04},
    {"X-05            B.1         DTMF receiver disabling", test_x_05},
    {"X-06            B.2         DTMF character conversion", test_x_06},
    {"X-07            C.1         EDT carrier timing and receiver disabling", test_x_07},
    {"X-08            C.2-3       EDT bit rate and character structure", test_x_08},
    {"X-09            E           V.23 calling mode character format", test_x_09},
    {"X-10            E           V.23 answer mode character format", test_x_10},
    {"X-11            F.4-5       V.21 character structure", test_x_11},
    {"X-12            G.1-3       V.18 mode", test_x_12},

    {"", NULL}
};

int main(int argc, char *argv[])
{
    int i;
    int res;
    int hit;
    const char *match;
    int test_standard;
    int opt;

    match = NULL;
    test_standard = -1;
    while ((opt = getopt(argc, argv, "d:ls:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            decode_test_file = optarg;
            break;
        case 'l':
            log_audio = TRUE;
            break;
        case 's':
            test_standard = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    if (decode_test_file)
    {
        decode_test_data_file(test_standard, decode_test_file);
        exit(0);
    }
    argc -= optind;
    argv += optind;
    if (argc > 0)
        match = argv[0];

    outhandle = NULL;
    if (log_audio)
    {
        if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_FILE_NAME);
            exit(2);
        }
    }

    hit = FALSE;
    for (i = 0;  test_list[i].title[0];  i++)
    {
        if (test_list[i].func
            &&
               (match == NULL
                ||
                   (strncmp(match, test_list[i].title, strlen(match)) == 0
                    &&
                    test_list[i].title[strlen(match)] == ' ')))
        {
            hit = TRUE;
            printf("%s\n", test_list[i].title);
            res = test_list[i].func();
            if (res < 0)
            {
                printf("    Test failed\n");
                exit(2);
            }
            if (res == 0)
            {
                printf("    Test passed\n");
            }
        }
        else
        {
            if (match == NULL)
                printf("%s\n", test_list[i].title);
        }
    }
    if (!hit)
    {
        printf("Test not found\n");
        exit(2);
    }
    basic_tests(V18_MODE_5BIT_4545);
    basic_tests(V18_MODE_5BIT_4545 | V18_MODE_REPETITIVE_SHIFTS_OPTION);
    if (log_audio)
    {
        if (sf_close_telephony(outhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
            exit(2);
        }
    }
    printf("Tests passed\n");
    return 0;

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
