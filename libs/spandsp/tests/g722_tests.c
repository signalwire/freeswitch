/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g722_tests.c - Test G.722 encode and decode.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 * $Id: g722_tests.c,v 1.26 2008/05/13 13:17:25 steveu Exp $
 */

/*! \file */

/*! \page g722_tests_page G.722 tests
\section g722_tests_page_sec_1 What does it do?
This modules implements two sets of tests:
    - The tests defined in the G.722 specification, using the test data files supplied
      with the specification.
    - A generally audio quality test, consisting of compressing and decompressing a speeech
      file for audible comparison.

The speech file should be recorded at 16 bits/sample, 16000 samples/second, and named
"pre_g722.wav".

The ITU tests use the codec in a special mode, in which the QMFs, which split and recombine the
sub-bands, are disabled. This means they do not test 100% of the codec. This is the reason for
including the additional listening test.

\section g722_tests_page_sec_2 How is it used?
To perform the tests in the G.722 specification you need to obtain the test data files from the
specification. These are copyright material, and so cannot be distributed with this test software.

The files, containing test vectors, which are supplied with the G.722 specification, should be
copied to itutests/g722. The ITU tests can then be run by executing g722_tests without
any parameters.

To perform a general audio quality test, g722_tests should be run with a parameter specifying
the required bit rate for compression. The valid parameters are "-48", "-56", and "-64".
The file ../test-data/local/short_wb_voice.wav will be compressed to the specified bit rate, decompressed,
and the resulting audio stored in post_g722.wav.
*/

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <ctype.h>
#include <audiofile.h>

#include "spandsp.h"

#define G722_SAMPLE_RATE    16000

#define BLOCK_LEN           320

#define MAX_TEST_VECTOR_LEN 40000

#define TESTDATA_DIR        "../test-data/itu/g722/"

#define EIGHTK_IN_FILE_NAME "../test-data/local/short_nb_voice.wav"
#define IN_FILE_NAME        "../test-data/local/short_wb_voice.wav"
#define OUT_FILE_NAME       "post_g722.wav"

#if 0
static const char *itu_test_files[] =
{
    TESTDATA_DIR "T1C1.XMT",        /* 69973 bytes */
    TESTDATA_DIR "T1C2.XMT",        /* 3605 bytes */
    TESTDATA_DIR "T1D3.COD",        /* 69973 bytes */

    TESTDATA_DIR "T2R1.COD",        /* 69973 bytes */
    TESTDATA_DIR "T2R2.COD",        /* 3605 bytes */

    TESTDATA_DIR "T3L1.RC1",        /* 69973 bytes */
    TESTDATA_DIR "T3L1.RC2",        /* 69973 bytes */
    TESTDATA_DIR "T3L1.RC3",        /* 69973 bytes */
    TESTDATA_DIR "T3H1.RC0",        /* 69973 bytes */
    TESTDATA_DIR "T3L2.RC1",        /* 3605 bytes */
    TESTDATA_DIR "T3L2.RC2",        /* 3605 bytes */
    TESTDATA_DIR "T3L2.RC3",        /* 3605 bytes */
    TESTDATA_DIR "T3H2.RC0",        /* 3605 bytes */
    TESTDATA_DIR "T3L3.RC1",        /* 69973 bytes */
    TESTDATA_DIR "T3L3.RC2",        /* 69973 bytes */
    TESTDATA_DIR "T3L3.RC3",        /* 69973 bytes */
    TESTDATA_DIR "T3H3.RC0"         /* 69973 bytes */
};
#endif

static const char *encode_test_files[] =
{
    TESTDATA_DIR "T1C1.XMT",    TESTDATA_DIR "T2R1.COD",
    TESTDATA_DIR "T1C2.XMT",    TESTDATA_DIR "T2R2.COD",
    NULL
};

static const char *decode_test_files[] =
{
    TESTDATA_DIR "T2R1.COD",
    TESTDATA_DIR "T3L1.RC1",
    TESTDATA_DIR "T3L1.RC2",
    TESTDATA_DIR "T3L1.RC3",
    TESTDATA_DIR "T3H1.RC0",

    TESTDATA_DIR "T2R2.COD",
    TESTDATA_DIR "T3L2.RC1",
    TESTDATA_DIR "T3L2.RC2",
    TESTDATA_DIR "T3L2.RC3",
    TESTDATA_DIR "T3H2.RC0",

    TESTDATA_DIR "T1D3.COD",
    TESTDATA_DIR "T3L3.RC1",
    TESTDATA_DIR "T3L3.RC2",
    TESTDATA_DIR "T3L3.RC3",
    TESTDATA_DIR "T3H3.RC0",
    
    NULL
};

int16_t itu_data[MAX_TEST_VECTOR_LEN];
uint16_t itu_ref[MAX_TEST_VECTOR_LEN];
uint16_t itu_ref_upper[MAX_TEST_VECTOR_LEN];
uint8_t compressed[MAX_TEST_VECTOR_LEN];
int16_t decompressed[MAX_TEST_VECTOR_LEN];

static int hex_get(char *s)
{
    int i;
    int value;
    int x;

    for (value = i = 0;  i < 4;  i++)
    {
        x = *s++ - 0x30;
        if (x > 9)
            x -= 0x07;
        if (x > 15)
            x -= 0x20;
        if (x < 0  ||  x > 15)
            return -1;
        value <<= 4;
        value |= x;
    }
    return value;
}
/*- End of function --------------------------------------------------------*/

static int get_vector(FILE *file, uint16_t vec[])
{
    char buf[132 + 1];
    char *s;
    int i;
    int value;

    while (fgets(buf, 133, file))
    {
        if (buf[0] == '/'  &&  buf[1] == '*')
            continue;
        s = buf;
        i = 0;
        while ((value = hex_get(s)) >= 0)
        {
            vec[i++] = value;
            s += 4;
        }
        return i;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int get_test_vector(const char *file, uint16_t buf[], int max_len)
{
    int octets;
    int i;
    FILE *infile;
    
    if ((infile = fopen(file, "r")) == NULL)
    {
        fprintf(stderr, "    Failed to open '%s'\n", file);
        exit(2);
    }
    octets = 0;  
    while ((i = get_vector(infile, buf + octets)) > 0)
        octets += i;
    fclose(infile);
    return octets;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    g722_encode_state_t enc_state;
    g722_decode_state_t dec_state;
    int len;
    int len_comp;
    int len_comp_upper;
    int len_data;
    int len2;
    int len3;
    int i;
    int j;
    int k;
    int file;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;
    int samples;
    int mode;
    int itutests;
    int bit_rate;
    int eight_k_in;
    int eight_k_out;
    float x;
    int16_t indata[BLOCK_LEN];
    int16_t outdata[BLOCK_LEN];
    uint8_t adpcmdata[BLOCK_LEN];

    i = 1;
    bit_rate = 64000;
    eight_k_in = FALSE;
    eight_k_out = FALSE;
    itutests = TRUE;
    while (argc > i)
    {
        if (strcmp(argv[i], "-48") == 0)
        {
            bit_rate = 48000;
            itutests = FALSE;
            i++;
        }
        else if (strcmp(argv[i], "-56") == 0)
        {
            bit_rate = 56000;
            itutests = FALSE;
            i++;
        }
        else if (strcmp(argv[i], "-64") == 0)
        {
            bit_rate = 64000;
            itutests = FALSE;
            i++;
        }
        else if (strcmp(argv[i], "-8k8k") == 0)
        {
            eight_k_in = TRUE;
            eight_k_out = TRUE;
            i++;
        }
        else if (strcmp(argv[i], "-8k16k") == 0)
        {
            eight_k_in = TRUE;
            eight_k_out = FALSE;
            i++;
        }
        else if (strcmp(argv[i], "-16k8k") == 0)
        {
            eight_k_in = FALSE;
            eight_k_out = TRUE;
            i++;
        }
        else if (strcmp(argv[i], "-16k16k") == 0)
        {
            eight_k_in = FALSE;
            eight_k_out = FALSE;
            i++;
        }
        else
        {
            fprintf(stderr, "Unknown parameter %s specified.\n", argv[i]);
            exit(2);
        }
    }

    if (itutests)
    {
        /* ITU G.722 encode tests, using configuration 1. The QMF is bypassed */
        for (file = 0;  encode_test_files[file];  file += 2)
        {
            printf("Testing %s -> %s\n", encode_test_files[file], encode_test_files[file + 1]);
    
            /* Get the input data */
            len_data = get_test_vector(encode_test_files[file], (uint16_t *) itu_data, MAX_TEST_VECTOR_LEN);

            /* Get the reference output data */
            len_comp = get_test_vector(encode_test_files[file + 1], itu_ref, MAX_TEST_VECTOR_LEN);

            /* Process the input data */
            /* Skip the reset stuff at each end of the data */
            for (i = 0;  i < len_data;  i++)
            {
                if ((itu_data[i] & 1) == 0)
                    break;
            }
            for (j = i;  j < len_data;  j++)
            {
                if ((itu_data[j] & 1))
                    break;
            }
            len = j - i;
            g722_encode_init(&enc_state, 64000, 0);
            enc_state.itu_test_mode = TRUE;
            len2 = g722_encode(&enc_state, compressed, itu_data + i, len);

            /* Check the result against the ITU's reference output data */
            j = 0;
            for (k = 0;  k < len2;  k++)
            {
                if ((compressed[k] & 0xFF) != ((itu_ref[k + i] >> 8) & 0xFF))
                {
                    printf(">>> %6d %4x %4x\n", k, compressed[k] & 0xFF, itu_ref[k + i] & 0xFFFF);
                    j++;
                }
            }
            printf("%d bad samples, out of %d/%d samples\n", j, len, len_data);
            if (j)
            {
                printf("Test failed\n");
                exit(2);
            }
            printf("Test passed\n");
        }

        /* ITU G.722 decode tests, using configuration 2. The QMF is bypassed */
        /* Run each of the tests for each of the modes - 48kbps, 56kbps and 64kbps. */
        for (mode = 1;  mode <= 3;  mode++)
        {
            for (file = 0;  decode_test_files[file];  file += 5)
            {
                printf("Testing mode %d, %s -> %s + %s\n",
                       mode,
                       decode_test_files[file],
                       decode_test_files[file + mode],
                       decode_test_files[file + 4]);

                /* Get the input data */
                len_data = get_test_vector(decode_test_files[file], (uint16_t *) itu_data, MAX_TEST_VECTOR_LEN);
        
                /* Get the lower reference output data */
                len_comp = get_test_vector(decode_test_files[file + mode], itu_ref, MAX_TEST_VECTOR_LEN);
        
                /* Get the upper reference output data */
                len_comp_upper = get_test_vector(decode_test_files[file + 4], itu_ref_upper, MAX_TEST_VECTOR_LEN);
    
                /* Process the input data */
                /* Skip the reset stuff at each end of the data */
                for (i = 0;  i < len_data;  i++)
                {
                    if ((itu_data[i] & 1) == 0)
                        break;
                }
                for (j = i;  j < len_data;  j++)
                {
                    if ((itu_data[j] & 1))
                        break;
                }
                len = j - i;
                for (k = 0;  k < len;  k++)
                    compressed[k] = itu_data[k + i] >> ((mode == 3)  ?  10  :  (mode == 2)  ?  9  :  8);
        
                g722_decode_init(&dec_state, (mode == 3)  ?  48000  :  (mode == 2)  ?  56000  :  64000, 0);
                dec_state.itu_test_mode = TRUE;
                len2 = g722_decode(&dec_state, decompressed, compressed, len);
        
                /* Check the result against the ITU's reference output data */
                j = 0;
                for (k = 0;  k < len2;  k += 2)
                {
                    if ((decompressed[k] & 0xFFFF) != (itu_ref[(k >> 1) + i] & 0xFFFF)
                        ||
                        (decompressed[k + 1] & 0xFFFF) != (itu_ref_upper[(k >> 1) + i] & 0xFFFF))
                    {
                        printf(">>> %6d %4x %4x %4x %4x\n", k >> 1, decompressed[k] & 0xFFFF, decompressed[k + 1] & 0xFFFF, itu_ref[(k >> 1) + i] & 0xFFFF, itu_ref_upper[(k >> 1) + i] & 0xFFFF);
                        j++;
                    }
                }
                printf("%d bad samples, out of %d/%d samples\n", j, len, len_data);
                if (j)
                {
                    printf("Test failed\n");
                    exit(2);
                }
                printf("Test passed\n");
            }
        }

        printf("Tests passed.\n");
    }
    else
    {
        if (eight_k_in)
        {
            if ((inhandle = afOpenFile(EIGHTK_IN_FILE_NAME, "r", NULL)) == AF_NULL_FILEHANDLE)
            {
                fprintf(stderr, "    Cannot open wave file '%s'\n", EIGHTK_IN_FILE_NAME);
                exit(2);
            }
            if ((x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
            {
                fprintf(stderr, "    Unexpected frame size in wave file '%s'\n", EIGHTK_IN_FILE_NAME);
                exit(2);
            }
            if ((x = afGetRate(inhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
            {
                fprintf(stderr, "    Unexpected sample rate %f in wave file '%s'\n", x, EIGHTK_IN_FILE_NAME);
                exit(2);
            }
            if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
            {
                fprintf(stderr, "    Unexpected number of channels in wave file '%s'\n", EIGHTK_IN_FILE_NAME);
                exit(2);
            }
        }
        else
        {
            if ((inhandle = afOpenFile(IN_FILE_NAME, "r", NULL)) == AF_NULL_FILEHANDLE)
            {
                fprintf(stderr, "    Cannot open wave file '%s'\n", IN_FILE_NAME);
                exit(2);
            }
            if ((x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
            {
                fprintf(stderr, "    Unexpected frame size in wave file '%s'\n", IN_FILE_NAME);
                exit(2);
            }
            if ((x = afGetRate(inhandle, AF_DEFAULT_TRACK)) != (float) G722_SAMPLE_RATE)
            {
                fprintf(stderr, "    Unexpected sample rate %f in wave file '%s'\n", x, IN_FILE_NAME);
                exit(2);
            }
            if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
            {
                fprintf(stderr, "    Unexpected number of channels in wave file '%s'\n", IN_FILE_NAME);
                exit(2);
            }
        }
        
        if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
        {
            fprintf(stderr, "    Failed to create file setup\n");
            exit(2);
        }
        afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
        if (eight_k_out)
            afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
        else
            afInitRate(filesetup, AF_DEFAULT_TRACK, (float) G722_SAMPLE_RATE);
        afInitFileFormat(filesetup, AF_FILE_WAVE);
        afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);
        if ((outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
        if (eight_k_in)
            g722_encode_init(&enc_state, bit_rate, G722_PACKED | G722_SAMPLE_RATE_8000);
        else
            g722_encode_init(&enc_state, bit_rate, G722_PACKED);
        if (eight_k_out)
            g722_decode_init(&dec_state, bit_rate, G722_PACKED | G722_SAMPLE_RATE_8000);
        else
            g722_decode_init(&dec_state, bit_rate, G722_PACKED);
        for (;;)
        {
            samples = afReadFrames(inhandle,
                                   AF_DEFAULT_TRACK,
                                   indata,
                                   BLOCK_LEN);
            if (samples <= 0)
                break;
            len2 = g722_encode(&enc_state, adpcmdata, indata, samples);
            len3 = g722_decode(&dec_state, outdata, adpcmdata, len2);
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      outdata,
                                      len3);
            if (outframes != len3)
            {
                fprintf(stderr, "    Error writing wave file\n");
                exit(2);
            }
        }
        if (afCloseFile(inhandle))
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", IN_FILE_NAME);
            exit(2);
        }
        if (afCloseFile(outhandle))
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
        afFreeFileSetup(filesetup);

        printf("'%s' transcoded to '%s' at %dbps.\n", IN_FILE_NAME, OUT_FILE_NAME, bit_rate);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
