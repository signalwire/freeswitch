/*
 * VoIPcodecs - a series of DSP components for telephony
 *
 * ima_adpcm_tests.c - Test the IMA/DVI/Intel ADPCM encode and decode
 *                     software.
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
 * $Id$
 */

/*! \file */

/*! \page ima_adpcm_tests_page IMA ADPCM tests
\section ima_adpcm_tests_page_sec_1 What does it do?
To perform a general audio quality test, ima_adpcm_tests should be run. The test file
../localtests/short_nb_voice.wav will be compressed to the specified bit rate,
decompressed, and the resulting audio stored in post_ima_adpcm.wav. A simple SNR test
is automatically performed. Listening tests may be used for a more detailed evaluation
of the degradation in quality caused by the compression.

\section ima_adpcm_tests_page_sec_2 How is it used?
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <audiofile.h>

#include "voipcodecs.h"

#define IN_FILE_NAME    "../localtests/short_nb_voice.wav"
#define OUT_FILE_NAME   "post_ima_adpcm.wav"

#define HIST_LEN        1000

int main(int argc, char *argv[])
{
    int i;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int frames;
    int dec_frames;
    int outframes;
    int ima_bytes;
    float x;
    double pre_energy;
    double post_energy;
    double diff_energy;
    int16_t pre_amp[HIST_LEN];
    int16_t post_amp[HIST_LEN];
    uint8_t ima_data[HIST_LEN];
    int16_t history[HIST_LEN];
    int hist_in;
    int hist_out;
    ima_adpcm_state_t *ima_enc_state;
    ima_adpcm_state_t *ima_dec_state;
    int xx;
    int vbr;
    const char *in_file_name;

    vbr = FALSE;
    in_file_name = IN_FILE_NAME;
    for (i = 1;  i < argc;  i++)
    {
        if (strcmp(argv[i], "-v") == 0)
        {
            vbr = TRUE;
            continue;
        }
        if (strcmp(argv[i], "-i") == 0)
        {
            in_file_name = argv[++i];
            continue;
        }
        fprintf(stderr, "Unknown parameter %s specified.\n", argv[i]);
        exit(2);
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
    if ((x = afGetRate(inhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
    {
        printf("    Unexpected sample rate in wave file '%s'\n", in_file_name);
        exit(2);
    }
    if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
    {
        printf("    Unexpected number of channels in wave file '%s'\n", in_file_name);
        exit(2);
    }

    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);
    if ((outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    if ((ima_enc_state = ima_adpcm_init(NULL, (vbr)  ?  IMA_ADPCM_VDVI  :  IMA_ADPCM_DVI4)) == NULL)
    {
        fprintf(stderr, "    Cannot create encoder\n");
        exit(2);
    }
        
    if ((ima_dec_state = ima_adpcm_init(NULL, (vbr)  ?  IMA_ADPCM_VDVI  :  IMA_ADPCM_DVI4)) == NULL)
    {
        fprintf(stderr, "    Cannot create decoder\n");
        exit(2);
    }

    hist_in = 0;
    hist_out = 0;
    pre_energy = 0.0;
    post_energy = 0.0;
    diff_energy = 0.0;
    while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, pre_amp, 159)))
    {
        ima_bytes = ima_adpcm_encode(ima_enc_state, ima_data, pre_amp, frames);
        dec_frames = ima_adpcm_decode(ima_dec_state, post_amp, ima_data, ima_bytes);
        for (i = 0;  i < frames;  i++)
        {
            history[hist_in++] = pre_amp[i];
            if (hist_in >= HIST_LEN)
                hist_in = 0;
            pre_energy += (double) pre_amp[i] * (double) pre_amp[i];
        }
        for (i = 0;  i < dec_frames;  i++)
        {
            post_energy += (double) post_amp[i] * (double) post_amp[i];
            xx = post_amp[i] - history[hist_out++];
            if (hist_out >= HIST_LEN)
                hist_out = 0;
            diff_energy += (double) xx * (double) xx;
        }
        outframes = afWriteFrames(outhandle, AF_DEFAULT_TRACK, post_amp, dec_frames);
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
    ima_adpcm_release(ima_enc_state);
    ima_adpcm_release(ima_dec_state);

    printf("Output energy is %f%% of input energy.\n", 100.0*post_energy/pre_energy);
    printf("Residual energy is %f%% of the total.\n", 100.0*diff_energy/post_energy);
    if (fabs(1.0 - post_energy/pre_energy) > 0.05
        ||
        fabs(diff_energy/post_energy) > 0.03)
    {
        printf("Tests failed.\n");
        exit(2);
    }
    
    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
