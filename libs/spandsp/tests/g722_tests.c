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
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <ctype.h>
#include <sndfile.h>

#include "spandsp.h"

#include "spandsp/private/g722.h"

#define G722_SAMPLE_RATE    16000

#define BLOCK_LEN           320

#define MAX_TEST_VECTOR_LEN 40000

#define TESTDATA_DIR        "../test-data/itu/g722/"

#define EIGHTK_IN_FILE_NAME "../test-data/local/short_nb_voice.wav"
#define IN_FILE_NAME        "../test-data/local/short_wb_voice.wav"
#define ENCODED_FILE_NAME   "g722.g722"
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
    TESTDATA_DIR "T1C1.XMT",
    TESTDATA_DIR "T2R1.COD",
    TESTDATA_DIR "T1C2.XMT",
    TESTDATA_DIR "T2R2.COD",
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

static void itu_compliance_tests(void)
{
    g722_encode_state_t *enc_state;
    g722_decode_state_t *dec_state;
    int i;
    int j;
    int k;
    int len_comp;
    int len_comp_lower;
    int len_comp_upper;
    int len_data;
    int len;
    int len2;
    int mode;
    int file;

#if 1
    /* ITU G.722 encode tests, using configuration 1. The QMF is bypassed */
    for (file = 0;  encode_test_files[file];  file += 2)
    {
        printf("Testing %s -> %s\n", encode_test_files[file], encode_test_files[file + 1]);

        /* Get the input data */
        len_data = get_test_vector(encode_test_files[file], (uint16_t *) itu_data, MAX_TEST_VECTOR_LEN);

        /* Get the reference output data */
        len_comp = get_test_vector(encode_test_files[file + 1], itu_ref, MAX_TEST_VECTOR_LEN);

        if (len_data != len_comp)
        {
            printf("Test data length mismatch\n");
            exit(2);
        }
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
        enc_state = g722_encode_init(NULL, 64000, 0);
        enc_state->itu_test_mode = true;
        len2 = g722_encode(enc_state, compressed, itu_data + i, len);

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
#endif
#if 1
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
            len_comp_lower = get_test_vector(decode_test_files[file + mode], itu_ref, MAX_TEST_VECTOR_LEN);

            /* Get the upper reference output data */
            len_comp_upper = get_test_vector(decode_test_files[file + 4], itu_ref_upper, MAX_TEST_VECTOR_LEN);

            if (len_data != len_comp_lower  ||  len_data != len_comp_upper)
            {
                printf("Test data length mismatch\n");
                exit(2);
            }
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

            dec_state = g722_decode_init(NULL, (mode == 3)  ?  48000  :  (mode == 2)  ?  56000  :  64000, 0);
            dec_state->itu_test_mode = true;
            len2 = g722_decode(dec_state, decompressed, compressed, len);

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
#endif
    printf("Tests passed.\n");
}
/*- End of function --------------------------------------------------------*/

static void signal_to_distortion_tests(void)
{
    g722_encode_state_t *enc_state;
    g722_decode_state_t *dec_state;
    swept_tone_state_t *swept;
    power_meter_t *in_meter;
    power_meter_t *out_meter;
    int16_t original[1024];
    uint8_t compressed[1024];
    int16_t decompressed[1024];
    int len;
    int len2;
    int len3;
    int i;
    int32_t in_level;
    int32_t out_level;

    /* Test a back to back encoder/decoder pair to ensure we comply with Figure 11/G.722 to
       Figure 16/G.722, Figure A.1/G.722, and Figure A.2/G.722 */
    enc_state = g722_encode_init(NULL, 64000, 0);
    dec_state = g722_decode_init(NULL, 64000, 0);
    in_meter = power_meter_init(NULL, 7);
    out_meter = power_meter_init(NULL, 7);

    /* First some silence */
    len = 1024;
    memset(original, 0, len*sizeof(original[0]));
    for (i = 0;  i < len;  i++)
        in_level = power_meter_update(in_meter, original[i]);
    len2 = g722_encode(enc_state, compressed, original, len);
    len3 = g722_decode(dec_state, decompressed, compressed, len2);
    out_level = 0;
    for (i = 0;  i < len3;  i++)
        out_level = power_meter_update(out_meter, decompressed[i]);
    printf("Silence produces %d at the output\n", out_level);

    /* Now a swept tone test */
    swept = swept_tone_init(NULL, 25.0f, 3500.0f, -10.0f, 60*16000, false);
    do
    {
        len = swept_tone(swept, original, 1024);
        for (i = 0;  i < len;  i++)
            in_level = power_meter_update(in_meter, original[i]);
        len2 = g722_encode(enc_state, compressed, original, len);
        len3 = g722_decode(dec_state, decompressed, compressed, len2);
        for (i = 0;  i < len3;  i++)
            out_level = power_meter_update(out_meter, decompressed[i]);
        printf("%10d, %10d, %f\n", in_level, out_level, (float) out_level/in_level);
    }
    while (len > 0);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    g722_encode_state_t *enc_state;
    g722_decode_state_t *dec_state;
    int len2;
    int len3;
    int i;
    int file;
    SNDFILE *inhandle;
    SNDFILE *outhandle;
    SF_INFO info;
    int outframes;
    int samples;
    int opt;
    int itutests;
    int bit_rate;
    int eight_k_in;
    int eight_k_out;
    int encode;
    int decode;
    int tone_test;
    const char *in_file;
    const char *out_file;
    int16_t indata[BLOCK_LEN];
    int16_t outdata[BLOCK_LEN];
    uint8_t adpcmdata[BLOCK_LEN];
    float tone_level;
    uint32_t tone_phase;
    int32_t tone_phase_rate;

    bit_rate = 64000;
    eight_k_in = false;
    eight_k_out = false;
    itutests = true;
    encode = false;
    decode = false;
    tone_test = false;
    in_file = NULL;
    out_file = NULL;
    while ((opt = getopt(argc, argv, "b:d:e:i:l:o:t")) != -1)
    {
        switch (opt)
        {
        case 'b':
            bit_rate = atoi(optarg);
            if (bit_rate != 48000  &&  bit_rate != 56000  &&  bit_rate != 64000)
            {
                fprintf(stderr, "Invalid bit rate selected. Only 48000, 56000 and 64000 are valid.\n");
                exit(2);
            }
            itutests = false;
            break;
        case 'd':
            in_file = optarg;
            decode = true;
            itutests = false;
            break;
        case 'e':
            in_file = optarg;
            encode = true;
            itutests = false;
            break;
        case 'i':
            i = atoi(optarg);
            if (i != 8000  &&  i != 16000)
            {
                fprintf(stderr, "Invalid incoming sample rate. Only 8000 and 16000 are valid.\n");
                exit(2);
            }
            eight_k_in = (i == 8000);
            if (eight_k_in)
                in_file = EIGHTK_IN_FILE_NAME;
            break;
        case 'l':
            out_file = optarg;
            break;
        case 'o':
            i = atoi(optarg);
            if (i != 8000  &&  i != 16000)
            {
                fprintf(stderr, "Invalid outgoing sample rate. Only 8000 and 16000 are valid.\n");
                exit(2);
            }
            eight_k_out = (i == 8000);
            break;
        case 't':
            tone_test = true;
            itutests = false;
            break;
        default:
            //usage();
            exit(2);
        }
    }

    if (itutests)
    {
        itu_compliance_tests();
        signal_to_distortion_tests();
    }
    else
    {
        tone_level = dds_scaling_dbm0f(2.5f);
        tone_phase = 0;
        tone_phase_rate = dds_phase_ratef(1500.0f/2.0f);
        if (!decode  &&  !encode)
        {
            decode =
            encode = true;
        }
        if (in_file == NULL)
        {
            if (encode)
            {
                if (eight_k_in)
                    in_file = EIGHTK_IN_FILE_NAME;
                else
                    in_file = IN_FILE_NAME;
            }
            else
            {
                in_file = ENCODED_FILE_NAME;
            }
        }
        if (out_file == NULL)
        {
            out_file = (decode)  ?  OUT_FILE_NAME  :  ENCODED_FILE_NAME;
        }
        inhandle = NULL;
        outhandle = NULL;
        file = -1;
        if (encode)
        {
            if (eight_k_in)
            {
                if ((inhandle = sf_open(in_file, SFM_READ, &info)) == NULL)
                {
                    fprintf(stderr, "    Cannot open audio file '%s'\n", in_file);
                    exit(2);
                }
                if (info.samplerate != SAMPLE_RATE)
                {
                    fprintf(stderr, "    Unexpected sample rate %d in audio file '%s'\n", info.samplerate, in_file);
                    exit(2);
                }
                if (info.channels != 1)
                {
                    fprintf(stderr, "    Unexpected number of channels in audio file '%s'\n", in_file);
                    exit(2);
                }
            }
            else
            {
                if ((inhandle = sf_open(in_file, SFM_READ, &info)) == NULL)
                {
                    fprintf(stderr, "    Cannot open audio file '%s'\n", in_file);
                    exit(2);
                }
                if (info.samplerate != G722_SAMPLE_RATE)
                {
                    fprintf(stderr, "    Unexpected sample rate %d in audio file '%s'\n", info.samplerate, in_file);
                    exit(2);
                }
                if (info.channels != 1)
                {
                    fprintf(stderr, "    Unexpected number of channels in audio file '%s'\n", in_file);
                    exit(2);
                }
            }
            if (eight_k_in)
                enc_state = g722_encode_init(NULL, bit_rate, G722_PACKED | G722_SAMPLE_RATE_8000);
            else
                enc_state = g722_encode_init(NULL, bit_rate, G722_PACKED);
        }
        else
        {
            if ((file = open(in_file, O_RDONLY)) < 0)
            {
                fprintf(stderr, "    Failed to open '%s'\n", in_file);
                exit(2);
            }
        }
        if (decode)
        {
            memset(&info, 0, sizeof(info));
            info.frames = 0;
            info.samplerate = (eight_k_out)  ?  SAMPLE_RATE  :  G722_SAMPLE_RATE;
            info.channels = 1;
            info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
            info.sections = 1;
            info.seekable = 1;
            if ((outhandle = sf_open(out_file, SFM_WRITE, &info)) == NULL)
            {
                fprintf(stderr, "    Cannot create audio file '%s'\n", out_file);
                exit(2);
            }
            if (eight_k_out)
                dec_state = g722_decode_init(NULL, bit_rate, G722_PACKED | G722_SAMPLE_RATE_8000);
            else
                dec_state = g722_decode_init(NULL, bit_rate, G722_PACKED);
        }
        else
        {
            if ((file = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
            {
                fprintf(stderr, "    Failed to open '%s'\n", out_file);
                exit(2);
            }
        }
        for (;;)
        {
            if (encode)
            {
                samples = sf_readf_short(inhandle, indata, BLOCK_LEN);
                if (samples <= 0)
                    break;
                if (tone_test)
                {
                    for (i = 0;  i < samples;  i++)
                        indata[i] = dds_modf(&tone_phase, tone_phase_rate, tone_level, 0);
                }
                len2 = g722_encode(enc_state, adpcmdata, indata, samples);
            }
            else
            {
                len2 = read(file, adpcmdata, BLOCK_LEN);
                if (len2 <= 0)
                    break;
            }
            if (decode)
            {
                len3 = g722_decode(dec_state, outdata, adpcmdata, len2);
                outframes = sf_writef_short(outhandle, outdata, len3);
                if (outframes != len3)
                {
                    fprintf(stderr, "    Error writing audio file\n");
                    exit(2);
                }
            }
            else
            {
                len3 = write(file, adpcmdata, len2);
                if (len3 <= 0)
                    break;
            }
        }
        if (encode)
        {
            if (sf_close(inhandle))
            {
                fprintf(stderr, "    Cannot close audio file '%s'\n", IN_FILE_NAME);
                exit(2);
            }
        }
        else
        {
            close(file);
        }
        if (decode)
        {
            if (sf_close(outhandle))
            {
                fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
                exit(2);
            }
        }
        else
        {
            close(file);
        }
        printf("'%s' translated to '%s' at %dbps.\n", in_file, out_file, bit_rate);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
