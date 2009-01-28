/*
 * SpanDSP - a series of DSP components for telephony
 *
 * time_scale_tests.c
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
 * $Id: time_scale_tests.c,v 1.23 2008/11/15 14:27:29 steveu Exp $
 */

/*! \page time_scale_tests_page Time scaling tests
\section time_scale_tests_page_sec_1 What does it do?
These tests run a speech file through the time scaling routines.

\section time_scale_tests_page_sec_2 How are the tests run?
These tests process a speech file called pre_time_scale.wav. This file should contain
8000 sample/second 16 bits/sample linear audio. The tests read this file, change the
time scale of its contents, and write the resulting audio to post_time_scale.wav.
This file also contains 8000 sample/second 16 bits/sample linear audio.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <audiofile.h>

#include "spandsp.h"

#include "spandsp/private/time_scale.h"

#define BLOCK_LEN       160

#define IN_FILE_NAME    "../test-data/local/short_nb_voice.wav"
#define OUT_FILE_NAME   "time_scale_result.wav"

int main(int argc, char *argv[])
{
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int16_t in[BLOCK_LEN];
    int16_t out[5*(BLOCK_LEN + TIME_SCALE_MAX_SAMPLE_RATE/TIME_SCALE_MIN_PITCH)];
    int frames;
    int new_frames;
    int out_frames;
    int count;
    int max;
    time_scale_state_t state;
    float x;
    float rate;
    float sample_rate;
    const char *in_file_name;
    int sweep_rate;
    int opt;

    rate = 1.8f;
    sweep_rate = FALSE;
    in_file_name = IN_FILE_NAME;
    while ((opt = getopt(argc, argv, "i:r:s")) != -1)
    {
        switch (opt)
        {
        case 'i':
            in_file_name = optarg;
            break;
        case 'r':
            rate = atof(optarg);
            break;
        case 's':
            sweep_rate = TRUE;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    if ((inhandle = afOpenFile(in_file_name, "r", 0)) == AF_NULL_FILEHANDLE)
    {
        printf("    Cannot open wave file '%s'\n", in_file_name);
        exit(2);
    }
    if ((x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
    {
        printf("    Unexpected frame size in wave file '%s'\n", in_file_name);
        exit(2);
    }
    if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
    {
        printf("    Unexpected number of channels in wave file '%s'\n", in_file_name);
        exit(2);
    }
    sample_rate = afGetRate(inhandle, AF_DEFAULT_TRACK);

    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, sample_rate);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);
    if ((outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    if ((time_scale_init(&state, (int) sample_rate, rate)) == NULL)
    {
        fprintf(stderr, "    Cannot start the time scaler\n");
        exit(2);
    }
    max = time_scale_max_output_len(&state, BLOCK_LEN);
    printf("Rate is %f, longest output block is %d\n", rate, max);
    count = 0;
    while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, in, BLOCK_LEN)))
    {
        new_frames = time_scale(&state, out, in, frames);
        out_frames = afWriteFrames(outhandle, AF_DEFAULT_TRACK, out, new_frames);
        if (out_frames != new_frames)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        if (sweep_rate  &&  ++count > 100)
        {
            if (rate > 0.5f)
            {
                rate -= 0.1f;
                if (rate >= 0.99f  &&  rate <= 1.01f)
                    rate -= 0.1f;
                time_scale_init(&state, SAMPLE_RATE, rate);
                max = time_scale_max_output_len(&state, BLOCK_LEN);
                printf("Rate is %f, longest output block is %d\n", rate, max);
            }
            count = 0;
        }
    }
    if (afCloseFile(inhandle) != 0)
    {
        printf("    Cannot close wave file '%s'\n", in_file_name);
        exit(2);
    }
    if (afCloseFile(outhandle) != 0)
    {
        printf("    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
