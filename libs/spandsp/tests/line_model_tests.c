/*
 * SpanDSP - a series of DSP components for telephony
 *
 * line_model_tests.c - Tests for the telephone line model.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: line_model_tests.c,v 1.24 2008/05/13 13:17:26 steveu Exp $
 */

/*! \page line_model_tests_page Telephony line model tests
\section line_model_tests_page_sec_1 What does it do?
???.

\section line_model_tests_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <audiofile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#if !defined(NULL)
#define NULL (void *) 0
#endif

#define BLOCK_LEN           160

#define OUT_FILE_COMPLEXIFY "complexify.wav"
#define IN_FILE_NAME1       "line_model_test_in1.wav"
#define IN_FILE_NAME2       "line_model_test_in2.wav"
#define OUT_FILE_NAME1      "line_model_one_way_test_out.wav"
#define OUT_FILE_NAME       "line_model_test_out.wav"

int channel_codec;
int rbs_pattern;

static void complexify_tests(void)
{
    complexify_state_t *s;
    complexf_t cc;
    int16_t amp;
    int i;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;
    int16_t out[40000];
    awgn_state_t noise1;
    
    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 2);

    if ((outhandle = afOpenFile(OUT_FILE_COMPLEXIFY, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_COMPLEXIFY);
        exit(2);
    }
    awgn_init_dbm0(&noise1, 1234567, -10.0f);
    s = complexify_init();
    for (i = 0;  i < 20000;  i++)
    {
        amp = awgn(&noise1);
        cc = complexify(s, amp);
        out[2*i] = cc.re;
        out[2*i + 1] = cc.im;
    }
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              out,
                              20000);
    if (outframes != 20000)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }
    if (afCloseFile(outhandle))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_COMPLEXIFY);
        exit(2);
    }
    afFreeFileSetup(filesetup);
}
/*- End of function --------------------------------------------------------*/

static void test_one_way_model(int line_model_no, int speech_test)
{
    one_way_line_model_state_t *model;
    int16_t input1[BLOCK_LEN];
    int16_t output1[BLOCK_LEN];
    int16_t amp[2*BLOCK_LEN];
    AFfilehandle inhandle1;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;
    int samples;
    int i;
    int j;
    awgn_state_t noise1;
    
    if ((model = one_way_line_model_init(line_model_no, -50, channel_codec, rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    
    awgn_init_dbm0(&noise1, 1234567, -10.0f);

    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    if ((inhandle1 = afOpenFile(IN_FILE_NAME1, "r", NULL)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", IN_FILE_NAME1);
        exit(2);
    }
    if ((outhandle = afOpenFile(OUT_FILE_NAME1, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME1);
        exit(2);
    }
    for (i = 0;  i < 10000;  i++)
    {
        if (speech_test)
        {
            samples = afReadFrames(inhandle1,
                                   AF_DEFAULT_TRACK,
                                   input1,
                                   BLOCK_LEN);
            if (samples == 0)
                break;
        }
        else
        {
            for (j = 0;  j < BLOCK_LEN;  j++)
                input1[j] = awgn(&noise1);
            samples = BLOCK_LEN;
        }
        for (j = 0;  j < samples;  j++)
        {
            one_way_line_model(model, 
                               &output1[j],
                               &input1[j],
                               1);
            amp[j] = output1[j];
        }
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  samples);
        if (outframes != samples)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
    }
    if (afCloseFile(inhandle1))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", IN_FILE_NAME1);
        exit(2);
    }
    if (afCloseFile(outhandle))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME1);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    one_way_line_model_release(model);
}
/*- End of function --------------------------------------------------------*/

static void test_both_ways_model(int line_model_no, int speech_test)
{
    both_ways_line_model_state_t *model;
    int16_t input1[BLOCK_LEN];
    int16_t input2[BLOCK_LEN];
    int16_t output1[BLOCK_LEN];
    int16_t output2[BLOCK_LEN];
    int16_t amp[2*BLOCK_LEN];
    AFfilehandle inhandle1;
    AFfilehandle inhandle2;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;
    int samples;
    int i;
    int j;
    awgn_state_t noise1;
    awgn_state_t noise2;
    
    if ((model = both_ways_line_model_init(line_model_no, -50, line_model_no + 1, -35, channel_codec, rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    
    awgn_init_dbm0(&noise1, 1234567, -10.0f);
    awgn_init_dbm0(&noise2, 1234567, -10.0f);

    filesetup = afNewFileSetup();
    if (filesetup == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 2);

    inhandle1 = afOpenFile(IN_FILE_NAME1, "r", NULL);
    if (inhandle1 == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", IN_FILE_NAME1);
        exit(2);
    }
    inhandle2 = afOpenFile(IN_FILE_NAME2, "r", NULL);
    if (inhandle2 == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", IN_FILE_NAME2);
        exit(2);
    }
    outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    for (i = 0;  i < 10000;  i++)
    {
        if (speech_test)
        {
            samples = afReadFrames(inhandle1,
                                   AF_DEFAULT_TRACK,
                                   input1,
                                   BLOCK_LEN);
            if (samples == 0)
                break;
            samples = afReadFrames(inhandle2,
                                   AF_DEFAULT_TRACK,
                                   input2,
                                   samples);
            if (samples == 0)
                break;
        }
        else
        {
            for (j = 0;  j < BLOCK_LEN;  j++)
            {
                input1[j] = awgn(&noise1);
                input2[j] = awgn(&noise2);
            }
            samples = BLOCK_LEN;
        }
        for (j = 0;  j < samples;  j++)
        {
            both_ways_line_model(model, 
                                 &output1[j],
                                 &input1[j],
                                 &output2[j],
                                 &input2[j],
                                 1);
            amp[2*j] = output1[j];
            amp[2*j + 1] = output2[j];
        }
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  samples);
        if (outframes != samples)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
    }
    if (afCloseFile(inhandle1))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", IN_FILE_NAME1);
        exit(2);
    }
    if (afCloseFile(inhandle2))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", IN_FILE_NAME2);
        exit(2);
    }
    if (afCloseFile(outhandle))
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    both_ways_line_model_release(model);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int line_model_no;
    int speech_test;
    int log_audio;
    int i;

    line_model_no = 0;
    speech_test = FALSE;
    log_audio = FALSE;
    channel_codec = MUNGE_CODEC_NONE;
    for (i = 1;  i < argc;  i++)
    {
        if (strcmp(argv[i], "-c") == 0)
        {
            channel_codec = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-l") == 0)
        {
            log_audio = TRUE;
            continue;
        }
        if (strcmp(argv[i], "-m") == 0)
        {
            line_model_no = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-r") == 0)
        {
            rbs_pattern = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-s") == 0)
        {
            speech_test = TRUE;
            continue;
        }
    }
    complexify_tests();
    test_one_way_model(line_model_no, speech_test);
    test_both_ways_model(line_model_no, speech_test);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
