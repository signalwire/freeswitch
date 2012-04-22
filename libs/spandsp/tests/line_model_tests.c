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
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

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
#define OUT_FILE_NAME2      "line_model_two_way_test_out.wav"

int channel_codec;
int rbs_pattern;

static void complexify_tests(void)
{
    complexify_state_t *s;
    complexf_t cc;
    int16_t amp;
    int i;
    SNDFILE *outhandle;
    int outframes;
    int16_t out[40000];
    awgn_state_t noise1;

    if ((outhandle = sf_open_telephony_write(OUT_FILE_COMPLEXIFY, 2)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_COMPLEXIFY);
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
    outframes = sf_writef_short(outhandle, out, 20000);
    if (outframes != 20000)
    {
        fprintf(stderr, "    Error writing audio file\n");
        exit(2);
    }
    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_COMPLEXIFY);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void test_one_way_model(int line_model_no, int speech_test)
{
    one_way_line_model_state_t *model;
    int16_t input1[BLOCK_LEN];
    int16_t output1[BLOCK_LEN];
    int16_t amp[2*BLOCK_LEN];
    SNDFILE *inhandle1;
    SNDFILE *outhandle;
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

    if (speech_test)
    {
        if ((inhandle1 = sf_open_telephony_read(IN_FILE_NAME1, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open audio file '%s'\n", IN_FILE_NAME1);
            exit(2);
        }
    }
    else
    {
        inhandle1 = NULL;
    }
    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME1, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME1);
        exit(2);
    }
    for (i = 0;  i < 10000;  i++)
    {
        if (speech_test)
        {
            samples = sf_readf_short(inhandle1, input1, BLOCK_LEN);
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
        outframes = sf_writef_short(outhandle, amp, samples);
        if (outframes != samples)
        {
            fprintf(stderr, "    Error writing audio file\n");
            exit(2);
        }
    }
    if (speech_test)
    {
        if (sf_close_telephony(inhandle1))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", IN_FILE_NAME1);
            exit(2);
        }
    }
    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME1);
        exit(2);
    }
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
    SNDFILE *inhandle1;
    SNDFILE *inhandle2;
    SNDFILE *outhandle;
    int outframes;
    int samples;
    int i;
    int j;
    awgn_state_t noise1;
    awgn_state_t noise2;
    
    if ((model = both_ways_line_model_init(line_model_no,
                                           -50,
                                           -15.0f,
                                           -15.0f,
                                           line_model_no + 1,
                                           -35,
                                           -15.0f,
                                           -15.0f,
                                           channel_codec,
                                           rbs_pattern)) == NULL)
    {
        fprintf(stderr, "    Failed to create line model\n");
        exit(2);
    }
    
    awgn_init_dbm0(&noise1, 1234567, -10.0f);
    awgn_init_dbm0(&noise2, 1234567, -10.0f);

    if (speech_test)
    {
        if ((inhandle1 = sf_open_telephony_read(IN_FILE_NAME1, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open audio file '%s'\n", IN_FILE_NAME1);
            exit(2);
        }
        if ((inhandle2 = sf_open_telephony_read(IN_FILE_NAME2, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open audio file '%s'\n", IN_FILE_NAME2);
            exit(2);
        }
    }
    else
    {
        inhandle1 =
        inhandle2 = NULL;
    }
    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME2, 2)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME2);
        exit(2);
    }
    for (i = 0;  i < 10000;  i++)
    {
        if (speech_test)
        {
            samples = sf_readf_short(inhandle1, input1, BLOCK_LEN);
            if (samples == 0)
                break;
            samples = sf_readf_short(inhandle2, input2, samples);
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
        outframes = sf_writef_short(outhandle, amp, samples);
        if (outframes != samples)
        {
            fprintf(stderr, "    Error writing audio file\n");
            exit(2);
        }
    }
    if (speech_test)
    {
        if (sf_close_telephony(inhandle1))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", IN_FILE_NAME1);
            exit(2);
        }
        if (sf_close_telephony(inhandle2))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", IN_FILE_NAME2);
            exit(2);
        }
    }
    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME2);
        exit(2);
    }
    both_ways_line_model_release(model);
}
/*- End of function --------------------------------------------------------*/

static void test_line_filter(int line_model_no)
{
    float out;
    double sumin;
    double sumout;
    double gain;
    int i;
    int j;
    int p;
    int ptr;
    int len;
    swept_tone_state_t *s;
    float filter[129];
    int16_t buf[BLOCK_LEN];

    s = swept_tone_init(NULL, 200.0f, 3900.0f, -10.0f, 120*SAMPLE_RATE, 0);
    for (j = 0;  j < 129;  j++)
        filter[j] = 0.0f;
    ptr = 0;
    for (;;)
    {
        if ((len = swept_tone(s, buf, BLOCK_LEN)) <= 0)
            break;
        sumin = 0.0;
        sumout = 0.0;
        for (i = 0;  i < len;  i++)
        {
            /* Add the sample in the filter buffer */
            p = ptr;
            filter[p] = buf[i];
            if (++p == 129)
                p = 0;
            ptr = p;
    
            /* Apply the filter */
            out = 0.0f;
            for (j = 0;  j < 129;  j++)
            {
                out += line_models[line_model_no][128 - j]*filter[p];
                if (++p >= 129)
                    p = 0;
            }
            sumin += buf[i]*buf[i];
            sumout += out*out;
        }
        /*endfor*/
        gain = (sumin != 0.0)  ?  10.0*log10(sumout/sumin + 1.0e-10)  :  0.0;
        printf("%7.1f %f\n", swept_tone_current_frequency(s), gain);
    }
    /*endfor*/
    swept_tone_free(s);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int line_model_no;
    int speech_test;
    int opt;

    channel_codec = MUNGE_CODEC_NONE;
    line_model_no = 0;
    rbs_pattern = 0;
    speech_test = FALSE;
    while ((opt = getopt(argc, argv, "c:m:r:s:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            channel_codec = atoi(optarg);
            break;
        case 'm':
            line_model_no = atoi(optarg);
            break;
        case 'r':
            rbs_pattern = atoi(optarg);
            break;
        case 's':
            speech_test = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
        }
    }
    complexify_tests();
    test_one_way_model(line_model_no, speech_test);
    test_both_ways_model(line_model_no, speech_test);
    test_line_filter(line_model_no);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
