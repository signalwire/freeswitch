/*
 * SpanDSP - a series of DSP components for telephony
 *
 * logging_tests.c - Tests for the logging functions.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: logging_tests.c,v 1.16 2009/02/12 12:38:39 steveu Exp $
 */

/*! \page logging_tests_page Logging tests
\section logging_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#define _POSIX_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <time.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

static int tests_failed = FALSE;

static int msg_step = 0;
static int msg2_step = 0;
static int error_step = 0;
static int msg_done = FALSE;
static int msg2_done = FALSE;
static int error_done = FALSE;

static void message_handler(int level, const char *text)
{
    const char *ref[] =
    {
        "TAG Log with tag 1 2 3\n",
        "Log with protocol 1 2 3\n",
        "FLOW NewTag Log with new tag 1 2 3\n",
        "FLOW Protocol NewTag Log with protocol 1 2 3\n",
        "FLOW Protocol NewTag Buf 00 01 02 03 04 05 06 07 08 09\n",
        "FLOW Protocol NewTag Buf 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f 30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f 50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f 60 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f 70 71 72 73 74 75 76 77 78 79 7a 7b 7c 7d 7e 7f 80 81 82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f 90 91 92 93 94 95 96 97 98 99 9a 9b 9c 9d 9e 9f a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 ba bb bc bd be bf c0 c1 c2 c3 c4 c5 c6 c7 c8 c9 ca cb cc cd ce cf d0 d1 d2 d3 d4 d5 d6 d7 d8 d9 da db dc dd de df e0 e1 e2 e3 e4 e5 e6 e7 e8 e9 ea eb ec ed ee ef f0 f1 f2 f3 f4 f5 f6 f7 f8 f9 fa fb fc fd fe ff 00 01 02 03 04 05 06 07 08 09\n",
        "00:00:00.000 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        "00:00:00.020 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        "00:00:00.040 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        "00:00:00.060 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        "00:00:00.080 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        "00:00:00.100 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        "00:00:00.120 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        "00:00:00.140 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        "00:00:00.160 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        "00:00:00.180 FLOW Protocol NewTag Time tagged log 1 2 3\n",
        ""
    };

    if (strcmp(ref[msg_step], text))
    {
        printf(">>>: %s", ref[msg_step]);
        tests_failed = TRUE;
    }
    if (ref[++msg_step][0] == '\0')
        msg_done = TRUE;
    printf("MSG: %s", text);
}
/*- End of function --------------------------------------------------------*/

static void message_handler2(int level, const char *text)
{
    /* TODO: This doesn't check if the date/time field makes sense */
    if (strcmp(" FLOW Protocol NewTag Date/time tagged log 1 2 3\n", text + 23))
    {
        printf(">>>: %s", text + 23);
        tests_failed = TRUE;
    }
    if (++msg2_step == 10)
        msg2_done = TRUE;
    printf("MSG: %s", text);
}
/*- End of function --------------------------------------------------------*/

static void error_handler(const char *text)
{
    const char *ref[] =
    {
        "ERROR Log with severity log 1 2 3\n",
        ""
    };
    
    if (strcmp(ref[error_step], text))
    {
        printf(">>>: %s", ref[error_step]);
        tests_failed = TRUE;
    }
    if (ref[++error_step][0] == '\0')
        error_done = TRUE;
    printf("ERR: %s", text);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    logging_state_t log;
    int i;
    uint8_t buf[1000];
    struct timespec delay;

    /* Set up a logger */
    if (span_log_init(&log, 123, "TAG") == NULL)
    {
        fprintf(stderr, "Failed to initialise log.\n");
        exit(2);
    }
    /* Try it */
    span_log_set_level(&log, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    if (span_log(&log, SPAN_LOG_FLOW, "Logging to fprintf, as simple as %d %d %d\n", 1, 2, 3))
        fprintf(stderr, "Logged.\n");
    else
        fprintf(stderr, "Not logged.\n");

    /* Now set a custom log handler */
    span_log_set_message_handler(&log, &message_handler);
    span_log_set_error_handler(&log, &error_handler);
    span_log_set_sample_rate(&log, 44100);

    /* Try the different logging elements */
    span_log_set_level(&log, SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    if (span_log(&log, SPAN_LOG_FLOW, "Log with tag %d %d %d\n", 1, 2, 3))
        fprintf(stderr, "Logged.\n");
    else
        fprintf(stderr, "Not logged.\n");
    span_log_set_level(&log, SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    if (span_log(&log, SPAN_LOG_FLOW, "Log with protocol %d %d %d\n", 1, 2, 3))
        fprintf(stderr, "Logged.\n");
    else
        fprintf(stderr, "Not logged.\n");
    span_log_set_level(&log, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_FLOW);
    if (span_log(&log, SPAN_LOG_ERROR, "Log with severity log %d %d %d\n", 1, 2, 3))
        fprintf(stderr, "Logged.\n");
    else
        fprintf(stderr, "Not logged.\n");

    span_log_set_level(&log, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW);
    span_log_set_tag(&log, "NewTag");
    if (span_log(&log, SPAN_LOG_FLOW, "Log with new tag %d %d %d\n", 1, 2, 3))
        fprintf(stderr, "Logged.\n");
    else
        fprintf(stderr, "Not logged.\n");

    span_log_set_protocol(&log, "Protocol");
    if (span_log(&log, SPAN_LOG_FLOW, "Log with protocol %d %d %d\n", 1, 2, 3))
        fprintf(stderr, "Logged.\n");
    else
        fprintf(stderr, "Not logged.\n");
    
    /* Test logging of buffer contents */
    for (i = 0;  i < 1000;  i++)
        buf[i] = i;
    if (span_log_buf(&log, SPAN_LOG_FLOW, "Buf", buf, 10))
        fprintf(stderr, "Logged.\n");
    else
        fprintf(stderr, "Not logged.\n");
    if (span_log_buf(&log, SPAN_LOG_FLOW, "Buf", buf, 1000))
        fprintf(stderr, "Logged.\n");
    else
        fprintf(stderr, "Not logged.\n");

    /* Test the correct severities will be logged */
    for (i = 0;  i < 10;  i++)
    {
        if (!span_log_test(&log, i))
        {
            if (i != 6)
                tests_failed = TRUE;
            break;
        }
    }
    
    /* Check timestamping by samples */
    span_log_set_level(&log, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW | SPAN_LOG_SHOW_SAMPLE_TIME);
    for (i = 0;  i < 10;  i++)
    {
        span_log(&log, SPAN_LOG_FLOW, "Time tagged log %d %d %d\n", 1, 2, 3);
        span_log_bump_samples(&log, 441*2);
    }

    /* Check timestamping by current date and time */
    span_log_set_message_handler(&log, &message_handler2);
    span_log_set_level(&log, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_FLOW | SPAN_LOG_SHOW_DATE);
    for (i = 0;  i < 10;  i++)
    {
        span_log(&log, SPAN_LOG_FLOW, "Date/time tagged log %d %d %d\n", 1, 2, 3);
        delay.tv_sec = 0;
        delay.tv_nsec = 20000000;
        nanosleep(&delay, NULL);
    }
    if (tests_failed  ||  !msg_done  ||  !error_done)
    {
        printf("Tests failed - %d %d %d.\n", tests_failed, msg_done, error_done);
        return 2;
    }
    
    span_log_set_message_handler(&log, &message_handler);

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
