/*
 * SpanDSP - a series of DSP components for telephony
 *
 * oki_adpcm_tests.c - Test the Oki (Dialogic) ADPCM encode and decode
 *                     software at 24kbps and 32kbps.
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

/*! \file */

/*! \page oki_adpcm_tests_page OKI (Dialogic) ADPCM tests
\section oki_adpcm_tests_page_sec_1 What does it do?
To perform a general audio quality test, oki_adpcm_tests should be run. The test file
../test-data/local/short_nb_voice.wav will be compressed to the specified bit rate,
decompressed, and the resulting audio stored in post_oki_adpcm.wav. A simple SNR test
is automatically performed. Listening tests may be used for a more detailed evaluation
of the degradation in quality caused by the compression. Both 32k bps and 24k bps
compression may be tested.

\section oki_adpcm_tests_page_sec_2 How is it used?
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

#define IN_FILE_NAME    "../test-data/local/short_nb_voice.wav"
#define OUT_FILE_NAME   "post_oki_adpcm.wav"

#define HIST_LEN        1000

int main(int argc, char *argv[])
{
    int i;
    SNDFILE *inhandle;
    SNDFILE *outhandle;
    int frames;
    int dec_frames;
    int oki_bytes;
    int bit_rate;
    double pre_energy;
    double post_energy;
    double diff_energy;
    int16_t pre_amp[HIST_LEN];
    int16_t post_amp[HIST_LEN];
    uint8_t oki_data[HIST_LEN];
    int16_t history[HIST_LEN];
    int hist_in;
    int hist_out;
    oki_adpcm_state_t *oki_enc_state;
    oki_adpcm_state_t *oki_dec_state;
    oki_adpcm_state_t *oki_dec_state2;
    int xx;
    int total_pre_samples;
    int total_compressed_bytes;
    int total_post_samples;
    int successive_08_bytes;
    int successive_80_bytes;
    int encoded_fd;
    const char *encoded_file_name;
    const char *in_file_name;
    int log_encoded_data;
    int opt;

    bit_rate = 32000;
    encoded_file_name = NULL;
    in_file_name = IN_FILE_NAME;
    log_encoded_data = FALSE;
    while ((opt = getopt(argc, argv, "2d:i:l")) != -1)
    {
        switch (opt)
        {
        case '2':
            bit_rate = 24000;
            break;
        case 'd':
            encoded_file_name = optarg;
            break;
        case 'i':
            in_file_name = optarg;
            break;
        case 'l':
            log_encoded_data = TRUE;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    encoded_fd = -1;
    inhandle = NULL;
    oki_enc_state = NULL;
    if (encoded_file_name)
    {
        if ((encoded_fd = open(encoded_file_name, O_RDONLY)) < 0)
        {
            fprintf(stderr, "    Cannot open encoded file '%s'\n", encoded_file_name);
            exit(2);
        }
    }
    else
    {
        if ((inhandle = sf_open_telephony_read(in_file_name, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open audio file '%s'\n", in_file_name);
            exit(2);
        }
        if ((oki_enc_state = oki_adpcm_init(NULL, bit_rate)) == NULL)
        {
            fprintf(stderr, "    Cannot create encoder\n");
            exit(2);
        }
    }

    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    if ((oki_dec_state = oki_adpcm_init(NULL, bit_rate)) == NULL)
    {
        fprintf(stderr, "    Cannot create decoder\n");
        exit(2);
    }
    if ((oki_dec_state2 = oki_adpcm_init(NULL, bit_rate)) == NULL)
    {
        fprintf(stderr, "    Cannot create decoder\n");
        exit(2);
    }

    hist_in = 0;
    if (bit_rate == 32000)
        hist_out = 0;
    else
        hist_out = HIST_LEN - 27;
    memset(history, 0, sizeof(history));
    pre_energy = 0.0;
    post_energy = 0.0;
    diff_energy = 0.0;
    total_pre_samples = 0;
    total_compressed_bytes = 0;
    total_post_samples = 0;
    if (encoded_file_name)
    {
        /* Decode a file of OKI ADPCM code to a linear wave file */
        while ((oki_bytes = read(encoded_fd, oki_data, 80)) > 0)
        {
            total_compressed_bytes += oki_bytes;
            dec_frames = oki_adpcm_decode(oki_dec_state, post_amp, oki_data, oki_bytes);
            total_post_samples += dec_frames;
            for (i = 0;  i < dec_frames;  i++)
            {
                post_energy += (double) post_amp[i] * (double) post_amp[i];
                xx = post_amp[i] - history[hist_out++];
                if (hist_out >= HIST_LEN)
                    hist_out = 0;
                diff_energy += (double) xx * (double) xx;
            }
            sf_writef_short(outhandle, post_amp, dec_frames);
        }
        close(encoded_fd);
    }
    else
    {
        /* Perform a linear wave file -> OKI ADPCM -> linear wave file cycle. Along the way
           check the decoder resets on the sequence specified for this codec, and the gain
           and worst case sample distortion. */
        successive_08_bytes = 0;
        successive_80_bytes = 0;
        while ((frames = sf_readf_short(inhandle, pre_amp, 159)))
        {
            total_pre_samples += frames;
            oki_bytes = oki_adpcm_encode(oki_enc_state, oki_data, pre_amp, frames);
            if (log_encoded_data)
                write(1, oki_data, oki_bytes);
            total_compressed_bytes += oki_bytes;
            /* Look for a condition which is defined as something which should cause a reset of
               the decoder (48 samples of 0, 8, 0, 8, etc.), and verify that it really does. Use
               a second decode, which we feed byte by byte, for this. */
            for (i = 0;  i < oki_bytes;  i++)
            {
                oki_adpcm_decode(oki_dec_state2, post_amp, &oki_data[i], 1);
                if (oki_data[i] == 0x08)
                    successive_08_bytes++;
                else
                    successive_08_bytes = 0;
                if (oki_data[i] == 0x80)
                    successive_80_bytes++;
                else
                    successive_80_bytes = 0;
                if (successive_08_bytes == 24  ||  successive_80_bytes == 24)
                {
                    if (oki_dec_state2->step_index != 0)
                    {
                        fprintf(stderr, "Decoder reset failure\n");
                        exit(2);
                    }
                }
            }
            dec_frames = oki_adpcm_decode(oki_dec_state, post_amp, oki_data, oki_bytes);
            total_post_samples += dec_frames;
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
                //post_amp[i] = xx;
            }
            sf_writef_short(outhandle, post_amp, dec_frames);
        }
        printf("Pre samples: %d\n", total_pre_samples);
        printf("Compressed bytes: %d\n", total_compressed_bytes);
        printf("Post samples: %d\n", total_post_samples);

        printf("Output energy is %f%% of input energy.\n", 100.0*post_energy/pre_energy);
        printf("Residual energy is %f%% of the total.\n", 100.0*diff_energy/post_energy);
        if (bit_rate == 32000)
        {
            if (fabs(1.0 - post_energy/pre_energy) > 0.01
                ||
                fabs(diff_energy/post_energy) > 0.01)
            {
                printf("Tests failed.\n");
                exit(2);
            }
        }
        else
        {
            if (fabs(1.0 - post_energy/pre_energy) > 0.11
                ||
                fabs(diff_energy/post_energy) > 0.05)
            {
                printf("Tests failed.\n");
                exit(2);
            }
        }


        oki_adpcm_release(oki_enc_state);
        if (sf_close_telephony(inhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", in_file_name);
            exit(2);
        }
    }
    oki_adpcm_release(oki_dec_state);
    oki_adpcm_release(oki_dec_state2);
    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
