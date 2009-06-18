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
 *
 * $Id: v18_tests.c,v 1.8 2009/05/30 15:23:14 steveu Exp $
 */

/*! \page v18_tests_page V.18 tests
\section v18_tests_page_sec_1 What does it do?
*/

/* Enable the following definition to enable direct probing into the spandsp structures */
//#define WITH_SPANDSP_INTERNALS

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#define FALSE 0
#define TRUE (!FALSE)

#define OUTPUT_FILE_NAME    "v18.wav"

#define SAMPLES_PER_CHUNK   160

int log_audio = FALSE;
SNDFILE *outhandle = NULL;

char *decode_test_file = NULL;

int good_message_received;

const char *qbf_tx = "The quick Brown Fox Jumps Over The Lazy dog 0123456789!@#$%^&*()'";
const char *qbf_rx = "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789!X$$/'+.()'";
const char *full_baudot_rx =
    "\b \n\n\n\r?\n\n\n  !\"$$/+'().+,-./"
    "0123456789:;(=)?"
    "XABCDEFGHIJKLMNOPQRSTUVWXYZ(/)' "
    "'ABCDEFGHIJKLMNOPQRSTUVWXYZ(!) ";

#if 1
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
    int16_t amp[SAMPLES_PER_CHUNK];
    int outframes;
    int len;
    int push;
    int i;
    v18_state_t *v18_a;
    v18_state_t *v18_b;

    printf("Testing %s\n", v18_mode_to_str(mode));
    v18_a = v18_init(NULL, TRUE, mode, put_text_msg, NULL);
    v18_b = v18_init(NULL, FALSE, mode, put_text_msg, NULL);

    /* Fake an OK condition for the first message test */
    good_message_received = TRUE;
    push = 0;
    if (v18_put(v18_a, qbf_tx, -1) != strlen(qbf_tx))
    {
        printf("V.18 put failed\n");
        exit(2);
    }
    for (i = 0;  i < 100000;  i++)
    {
        if (push == 0)
        {
            if ((len = v18_tx(v18_a, amp, SAMPLES_PER_CHUNK)) == 0)
                push = 10;
        }
        else
        {
            len = 0;
            /* Push a little silence through, to flush things out */
            if (--push == 0)
            {
                if (!good_message_received)
                {
                    printf("No message received\n");
                    exit(2);
                }
                good_message_received = FALSE;
                if (v18_put(v18_a, qbf_tx, -1) != strlen(qbf_tx))
                {
                    printf("V.18 put failed\n");
                    exit(2);
                }
            }
        }
        if (len < SAMPLES_PER_CHUNK)
        {
            memset(&amp[len], 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - len));
            len = SAMPLES_PER_CHUNK;
        }
        if (log_audio)
        {
            outframes = sf_writef_short(outhandle, amp, len);
            if (outframes != len)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
        }
        v18_rx(v18_b, amp, len);
    }
    v18_free(v18_a);
    v18_free(v18_b);
}
/*- End of function --------------------------------------------------------*/
#endif

static int test_x_01(void)
{
    /* III.5.4.5.1 Baudot carrier timing and receiver disabling */
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static int test_x_02(void)
{
    /* III.5.4.5.2 Baudot bit rate confirmation */
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static int test_x_03(void)
{
    /* III.5.4.5.3 Baudot probe bit rate confirmation */
    printf("Test not yet implemented\n");
    return 1;
}
/*- End of function --------------------------------------------------------*/

static int test_x_04(void)
{
    char result[1024];
    char *t;
    int ch;
    int xx;
    int yy;
    int i;
    v18_state_t *v18_state;

    /* III.5.4.5.4 5 Bit to T.50 character conversion */
    v18_state = v18_init(NULL, TRUE, V18_MODE_5BIT_45, NULL, NULL);
    printf("Original:\n");
    t = result;
    for (i = 0;  i < 127;  i++)
    {
        ch = i;
        printf("%c", ch);
        xx = v18_encode_baudot(v18_state, ch);
        if (xx)
        {
            if ((xx & 0x3E0))
            {
                yy = v18_decode_baudot(v18_state, (xx >> 5) & 0x1F);
                if (yy)
                    *t++ = yy;
            }
            yy = v18_decode_baudot(v18_state, xx & 0x1F);
            if (yy)
                *t++ = yy;
        }
    }
    printf("\n");
    *t = '\0';
    v18_free(v18_state);
    printf("Result:\n%s\n", result);
    printf("Reference result:\n%s\n", full_baudot_rx);
    if (strcmp(result, full_baudot_rx) != 0)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int test_x_06(void)
{
    char msg[128];
    char dtmf[1024];
    char result[1024];
    const char *ref;
    int len;
    int i;

    /* III.5.4.5.6 DTMF character conversion */
    for (i = 0;  i < 127;  i++)
        msg[i] = i + 1;
    msg[127] = '\0';
    printf("%s\n", msg);
    
    len = v18_encode_dtmf(NULL, dtmf, msg);
    printf("%s\n", dtmf);

    len = v18_decode_dtmf(NULL, result, dtmf);

    ref = "\b \n\n\n?\n\n\n  %+().+,-.0123456789:;(=)"
          "?XABCDEFGHIJKLMNOPQRSTUVWXYZÆØÅ"
          " abcdefghijklmnopqrstuvwxyzæøå \b";

    printf("Result:\n%s\n", result);
    printf("Reference result:\n%s\n", ref);
    if (strcmp(result, ref) != 0)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int test_unimplemented(void)
{
    printf("Test not yet implemented\n");
    return 1;
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
    v18_state_t *v18_state;
    int16_t amp[SAMPLES_PER_CHUNK];
    SNDFILE *inhandle;
    int len;

    printf("Decoding as '%s'\n", v18_mode_to_str(mode));
    /* We will decode the audio from a file. */
    if ((inhandle = sf_open_telephony_read(decode_test_file, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", decode_test_file);
        exit(2);
    }
    v18_state = v18_init(NULL, FALSE, mode, put_v18_msg, NULL);
    for (;;)
    {
        len = sf_readf_short(inhandle, amp, SAMPLES_PER_CHUNK);
        if (len == 0)
            break;
        v18_rx(v18_state, amp, len);
    }
    if (sf_close(inhandle) != 0)
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
    {"MISC-01         4 (1)       No Disconnection Test", test_unimplemented},
    {"MISC-02         4 (2)       Automatic resumption of automoding", test_unimplemented},
    {"MISC-03         4 (2)       Retention of selected mode on loss of signal", test_unimplemented},
    {"MISC-04         4 (4)       Detection of BUSY tone", test_unimplemented},
    {"MISC-05         4 (4)       Detection of RINGING", test_unimplemented},
    {"MISC-06         4 (4)       LOSS OF CARRIER indication", test_unimplemented},
    {"MISC-07         4 (4)       Call progress indication", test_unimplemented},
    {"MISC-08         4 (5)       Circuit 135 test", test_unimplemented},
    {"MISC-09                     Connection Procedures", test_unimplemented},

    {"III.3.2.2 Automode originate tests", NULL},
    {"ORG-01          5.1.1       CI & XCI Signal coding and cadence", test_unimplemented},
    {"ORG-02          5.1.3       ANS Signal Detection", test_unimplemented},
    {"ORG-03          5.2.3.1     End of ANS signal detection", test_unimplemented},
    {"ORG-04          5.1.3.2     ANS tone followed by TXP", test_unimplemented},
    {"ORG-05          5.1.3.3     ANS tone followed by 1650Hz", test_unimplemented},
    {"ORG-06          5.1.3.4     ANS tone followed by 1300Hz", test_unimplemented},
    {"ORG-07          5.1.3       ANS tone followed by no tone", test_unimplemented},
    {"ORG-08          5.1.4       Bell 103 (2225Hz Signal) Detection", test_unimplemented},
    {"ORG-09          5.1.5       V.21 (1650Hz Signal) Detection", test_unimplemented},
    {"ORG-10          5.1.6       V.23 (1300Hz Signal) Detection", test_unimplemented},
    {"ORG-11          5.1.7       V.23 (390Hz Signal) Detection", test_unimplemented},
    {"ORG-12a to d    5.1.8       5 Bit Mode (Baudot) Detection Tests", test_unimplemented},
    {"ORG-13          5.1.9       DTMF signal detection", test_unimplemented},
    {"ORG-14          5.1.10      EDT Rate Detection", test_unimplemented},
    {"ORG-15          5.1.10.1    Rate Detection Test", test_unimplemented},
    {"ORG-16          5.1.10.2    980Hz Detection", test_unimplemented},
    {"ORG-17          5.1.10.3    Loss of signal after 980Hz", test_unimplemented},
    {"ORG-18          5.1.10.3    Tr Timer", test_unimplemented},
    {"ORG-19          5.1.11      Bell 103 (1270Hz Signal) Detection", test_unimplemented},
    {"ORG-20                      Immunity to Network Tones", test_unimplemented},
    {"ORG-21a to b                Immunity to other non-textphone modems", test_unimplemented},
    {"ORG-22                      Immunity to Fax Tones", test_unimplemented},
    {"ORG-23                      Immunity to Voice", test_unimplemented},
    {"ORG-24          5.1.2       ANSam detection", test_unimplemented},
    {"ORG-25          6.1         V.8 originate call", test_unimplemented},

    {"III.3.2.3 Automode answer tests", NULL},
    {"ANS-01          5.2.1       Ta timer", test_unimplemented},
    {"ANS-02          5.2.2       CI Signal Detection", test_unimplemented},
    {"ANS-03          5.2.2.1     Early Termination of ANS tone", test_unimplemented},
    {"ANS-04          5.2.2.2     Tt Timer", test_unimplemented},
    {"ANS-05          5.2.3.2     ANS tone followed by 980Hz", test_unimplemented},
    {"ANS-06          5.2.3.2     ANS tone followed by 1300Hz", test_unimplemented},
    {"ANS-07          5.2.3.3     ANS tone followed by 1650Hz", test_unimplemented},
    {"ANS-08          5.2.4.1     980Hz followed by 1650Hz", test_unimplemented},
    {"ANS-09a to d    5.2.4.2     980Hz calling tone detection", test_unimplemented},
    {"ANS-10          5.2.4.3     V.21 Detection by Timer", test_unimplemented},
    {"ANS-11          5.2.4.4.1   EDT Detection by Rate", test_unimplemented},
    {"ANS-12          5.2.4.4.2   V.21 Detection by Rate", test_unimplemented},
    {"ANS-13          5.2.4.4.3   Tr Timer", test_unimplemented},
    {"ANS-14          5.2.4.5     Te Timer", test_unimplemented},
    {"ANS-15a to d    5.2.5       5 Bit Mode (Baudot) Detection Tests", test_unimplemented},
    {"ANS-16          5.2.6       DTMF Signal Detection", test_unimplemented},
    {"ANS-17          5.2.7       Bell 103 (1270Hz signal) detection", test_unimplemented},
    {"ANS-18          5.2.8       Bell 103 (2225Hz signal) detection", test_unimplemented},
    {"ANS-19          5.2.9       V.21 Reverse Mode (1650Hz) Detection", test_unimplemented},
    {"ANS-20a to d    5.2.10      1300Hz Calling Tone Discrimination", test_unimplemented},
    {"ANS-21          5.2.11      V.23 Reverse Mode (1300Hz) Detection", test_unimplemented},
    {"ANS-22                      1300Hz with XCI Test", test_unimplemented},
    {"ANS-23          5.2.12      Stimulate Mode Country Settings", test_unimplemented},
    {"ANS-24          5.2.12.1    Stimulate Carrierless Mode Probe Message", test_unimplemented},
    {"ANS-25          5.2.12.1.1  Interrupted Carrierless Mode Probe", test_unimplemented},
    {"ANS-26          5.2.12.2    Stimulate Carrier Mode Probe Time", test_unimplemented},
    {"ANS-27          5.2.12.2.1  V.23 Mode (390Hz) Detection", test_unimplemented},
    {"ANS-28          5.2.12.2.2  Interrupted Carrier Mode Probe", test_unimplemented},
    {"ANS-29          5.2.12.2.2  Stimulate Mode Response During Probe", test_unimplemented},
    {"ANS-30                      Immunity to Network Tones", test_unimplemented},
    {"ANS-31                      Immunity to Fax Calling Tones", test_unimplemented},
    {"ANS-32                      Immunity to Voice", test_unimplemented},
    {"ANS-33          5.2.2.1     V.8 CM detection and V.8 Answering", test_unimplemented},

    {"III.3.2.4 Automode monitor tests", NULL},
    {"MON-01 to -20   5.3         Repeat all answer mode tests excluding tests ANS-01, ANS-20 and ANS-23 to ANS-29", test_unimplemented},
    {"MON-21          5.3         Automode Monitor Ta timer", test_unimplemented},
    {"MON-22a to d    5.3         Automode Monitor 1300Hz Calling Tone Discrimination", test_unimplemented},
    {"MON-23a to d    5.3         Automode Monitor 980Hz Calling Tone Discrimination", test_unimplemented},

    {"III.3.2.5 ITU-T V.18 annexes tests", NULL},
    {"X-01            A.1         Baudot carrier timing and receiver disabling", test_x_01},
    {"X-02            A.2         Baudot bit rate confirmation", test_x_02},
    {"X-03            A.3         Baudot probe bit rate confirmation", test_x_03},
    {"X-04            A.4         5 Bit to T.50 Character Conversion", test_x_04},
    {"X-05            B.1         DTMF receiver disabling", test_unimplemented},
    {"X-06            B.2         DTMF character conversion", test_x_06},
    {"X-07            C.1         EDT carrier timing and receiver disabling", test_unimplemented},
    {"X-08            C.2-3       EDT bit rate and character structure", test_unimplemented},
    {"X-09            E           V.23 calling mode character format", test_unimplemented},
    {"X-10            E           V.23 answer mode character format", test_unimplemented},
    {"X-11            F.4-5       V.21 character structure", test_unimplemented},
    {"X-12            G.1-3       V.18 mode", test_unimplemented},

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
        if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 1)) == NULL)
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
    basic_tests(V18_MODE_5BIT_45);
    if (log_audio)
    {
        if (sf_close(outhandle) != 0)
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
