/*
 * SpanDSP - a series of DSP components for telephony
 *
 * gsm0610_tests.c - Test the GSM 06.10 FR codec.
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
 */

/*! \file */

/*! \page gsm0610_tests_page GSM 06.10 full rate codec tests
\section gsm0610_tests_page_sec_1 What does it do?
Two sets of tests are performed:
    - The tests defined in the GSM 06.10 specification, using the test data files supplied with
      the specification.
    - A generally audio quality test, consisting of compressing and decompressing a speeech
      file for audible comparison.

\section gsm0610_tests_page_sec_2 How is it used?
To perform the tests in the GSM 06.10 specification you need to obtain the test data files from the
specification. These are copyright material, and so cannot be distributed with this test software.
They can, however, be freely downloaded from the ETSI web site.

The files, containing test vectors, which are supplied with the GSM 06.10 specification, should be
copied to etsitests/gsm0610/unpacked so the files are arranged in the following directories.

./fr_A:
    Seq01-A.cod Seq01-A.inp Seq01-A.out
    Seq02-A.cod Seq02-A.inp Seq02-A.out
    Seq03-A.cod Seq03-A.inp Seq03-A.out
    Seq04-A.cod Seq04-A.inp Seq04-A.out
    Seq05-A.out

./fr_L:
    Seq01.cod   Seq01.inp   Seq01.out
    Seq02.cod   Seq02.inp   Seq02.out
    Seq03.cod   Seq03.inp   Seq03.out
    Seq04.cod   Seq04.inp   Seq04.out
    Seq05.cod   Seq05.out

./fr_U:
    Seq01-U.cod Seq01-U.inp Seq01-U.out
    Seq02-U.cod Seq02-U.inp Seq02-U.out
    Seq03-U.cod Seq03-U.inp Seq03-U.out
    Seq04-U.cod Seq04-U.inp Seq04-U.out
    Seq05-U.out

./fr_homing_A:
    Homing01_A.out
    Seq01H_A.cod    Seq01H_A.inp    Seq01H_A.out
    Seq02H_A.cod    Seq02H_A.inp    Seq02H_A.out
    Seq03H_A.cod    Seq03H_A.inp    Seq03H_A.out
    Seq04H_A.cod    Seq04H_A.inp    Seq04H_A.out
    Seq05H_A.out
    Seq06H_A.cod    Seq06H_A.inp

./fr_homing_L:
    Homing01.cod    Homing01.out
    Seq01h.cod      Seq01h.inp      Seq01h.out
    Seq02h.cod      Seq02h.inp      Seq02h.out
    Seq03h.cod      Seq03h.inp      Seq03h.out
    Seq04h.cod      Seq04h.inp      Seq04h.out
    Seq05h.cod      Seq05h.out
    Seq06h.cod      Seq06h.inp

./fr_homing_U:
    Homing01_U.out
    Seq01H_U.cod    Seq01H_U.inp    Seq01H_U.out
    Seq02H_U.cod    Seq02H_U.inp    Seq02H_U.out
    Seq03H_U.cod    Seq03H_U.inp    Seq03H_U.out
    Seq04H_U.cod    Seq04H_U.inp    Seq04H_U.out
    Seq05H_U.out
    Seq06H_U.cod    Seq06H_U.inp

./fr_sync_A:
    Seqsync_A.inp
    Sync000_A.cod   --to--          Sync159_A.cod

./fr_sync_L:
    Bitsync.inp
    Seqsync.inp
    Sync000.cod     --to--          Sync159.cod

./fr_sync_U:
    Seqsync_U.inp
    Sync000_U.cod   --to--          Sync159_U.cod

This is different from the directory structure in which they are supplied. Also, the files names are a little
different. The supplied names are messy, and inconsistent across the sets. The names required by these tests
just clean up these inconsistencies. Note that you will need a Windows machine to unpack some of the supplied
files.

To perform a general audio quality test, gsm0610_tests should be run. The file ../test-data/local/short_nb_voice.wav
will be compressed to GSM 06.10 data, decompressed, and the resulting audio stored in post_gsm0610.wav.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define BLOCK_LEN       160

#define TESTDATA_DIR "../test-data/etsi/gsm0610/unpacked/fr_"

#define IN_FILE_NAME    "../test-data/local/short_nb_voice.wav"
#define OUT_FILE_NAME   "post_gsm0610.wav"

#define HIST_LEN        1000

uint8_t law_in_vector[1000000];
int16_t in_vector[1000000];
uint16_t code_vector_buf[1000000];
uint8_t code_vector[1000000];
uint8_t ref_code_vector[1000000];
uint8_t decoder_code_vector[1000000];
uint8_t law_out_vector[1000000];
int16_t out_vector[1000000];
int16_t ref_out_vector[1000000];
uint8_t ref_law_out_vector[1000000];
int vector_len;

static int get_test_vector(int full, int disk, const char *name)
{
    char buf[500];
    int in;
    int len;
    int i;

    if (full)
    {
        sprintf(buf, "%s%c/%s.inp", TESTDATA_DIR, 'L', name);
        if ((in = open(buf, O_RDONLY)) < 0)
        {
            fprintf(stderr, "Cannot open %s\n", buf);
            exit(2);
        }
        len = read(in, in_vector, 1000000);
        close(in);
        len /= sizeof(int16_t);
        vector_len = len;
    }

    sprintf(buf, "%s%c/%s.out", TESTDATA_DIR, 'L', name);
    if ((in = open(buf, O_RDONLY)) < 0)
    {
        fprintf(stderr, "Cannot open %s\n", buf);
        exit(2);
    }
    len = read(in, ref_out_vector, 1000000);
    close(in);
    len /= sizeof(int16_t);
    if (full)
    {
        if (len != vector_len)
        {
            fprintf(stderr, "Input and reference vector lengths do not match - %d %d\n", vector_len, len);
            exit(2);
        }
    }
    else
    {
        vector_len = len;
    }

    sprintf(buf, "%s%c/%s.cod", TESTDATA_DIR, 'L', name);
    if ((in = open(buf, O_RDONLY)) < 0)
    {
        fprintf(stderr, "Cannot open %s\n", buf);
        exit(2);
    }
    len = read(in, code_vector_buf, 1000000);
    close(in);
    len /= sizeof(int16_t);
    for (i = 0;  i < len;  i++)
    {
        ref_code_vector[i] = code_vector_buf[i];
        decoder_code_vector[i] = code_vector_buf[i];
    }
    if (len*BLOCK_LEN != vector_len*76)
    {
        fprintf(stderr, "Input and code vector lengths do not match - %d %d\n", vector_len, len);
        exit(2);
    }

    return len;
}
/*- End of function --------------------------------------------------------*/

static int get_law_test_vector(int full, int law, const char *name)
{
    char buf[500];
    int in;
    int len;
    int i;
    int law_uc;

    law_uc = toupper(law);

    if (full)
    {
        sprintf(buf, "%s%c/%s-%c.inp", TESTDATA_DIR, law_uc, name, law_uc);
        if ((in = open(buf, O_RDONLY)) < 0)
        {
            fprintf(stderr, "Cannot open %s\n", buf);
            exit(2);
        }
        len = read(in, law_in_vector, 1000000);
        close(in);
        vector_len = len;

        sprintf(buf, "%s%c/%s-%c.cod", TESTDATA_DIR, law_uc, name, law_uc);
        if ((in = open(buf, O_RDONLY)) < 0)
        {
            fprintf(stderr, "Cannot open %s\n", buf);
            exit(2);
        }
        len = read(in, code_vector_buf, 1000000);
        close(in);
        len /= sizeof(int16_t);
        for (i = 0;  i < len;  i++)
            ref_code_vector[i] = code_vector_buf[i];
        if (len*BLOCK_LEN != vector_len*76)
        {
            fprintf(stderr, "Input and code vector lengths do not match - %d %d\n", vector_len, len);
            exit(2);
        }
    }

    sprintf(buf, "%s%c/%s-%c.out", TESTDATA_DIR, law_uc, name, law_uc);
    if ((in = open(buf, O_RDONLY)) < 0)
    {
        fprintf(stderr, "Cannot open %s\n", buf);
        exit(2);
    }
    len = read(in, ref_law_out_vector, 1000000);
    close(in);
    if (full)
    {
        if (len != vector_len)
        {
            fprintf(stderr, "Input and reference vector lengths do not match - %d %d\n", vector_len, len);
            exit(2);
        }
    }
    else
    {
        vector_len = len;
    }

    sprintf(buf, "%s%c/%s.cod", TESTDATA_DIR, 'L', name);
    if ((in = open(buf, O_RDONLY)) < 0)
    {
        fprintf(stderr, "Cannot open %s\n", buf);
        exit(2);
    }
    len = read(in, code_vector_buf, 1000000);
    close(in);
    len /= sizeof(int16_t);
    for (i = 0;  i < len;  i++)
        decoder_code_vector[i] = code_vector_buf[i];

    return len;
}
/*- End of function --------------------------------------------------------*/

static int perform_linear_test(int full, int disk, const char *name)
{
    gsm0610_state_t *gsm0610_enc_state;
    gsm0610_state_t *gsm0610_dec_state;
    int i;
    int xxx;
    int mismatches;

    printf("Performing linear test '%s' from disk %d\n", name, disk);

    get_test_vector(full, disk, name);

    if (full)
    {
        if ((gsm0610_enc_state = gsm0610_init(NULL, GSM0610_PACKING_NONE)) == NULL)
        {
            fprintf(stderr, "    Cannot create encoder\n");
            exit(2);
        }
        xxx = gsm0610_encode(gsm0610_enc_state, code_vector, in_vector, vector_len);

        printf("Check code vector of length %d\n", xxx);
        for (i = 0, mismatches = 0;  i < xxx;  i++)
        {
            if (code_vector[i] != ref_code_vector[i])
            {
                printf("%8d/%3d: %6d %6d\n", i/76, i%76, code_vector[i], ref_code_vector[i]);
                mismatches++;
            }
        }
        gsm0610_release(gsm0610_enc_state);
        if (mismatches)
        {
            printf("Test failed: %d of %d samples mismatch\n", mismatches, xxx);
            exit(2);
        }
        printf("Test passed\n");
    }

    if ((gsm0610_dec_state = gsm0610_init(NULL, GSM0610_PACKING_NONE)) == NULL)
    {
        fprintf(stderr, "    Cannot create decoder\n");
        exit(2);
    }
    xxx = gsm0610_decode(gsm0610_dec_state, out_vector, decoder_code_vector, vector_len);
    printf("Check output vector of length %d\n", vector_len);
    for (i = 0, mismatches = 0;  i < vector_len;  i++)
    {
        if (out_vector[i] != ref_out_vector[i])
        {
            printf("%8d: %6d %6d\n", i, out_vector[i], ref_out_vector[i]);
            mismatches++;
        }
    }
    if (mismatches)
    {
        printf("Test failed: %d of %d samples mismatch\n", mismatches, vector_len);
        exit(2);
    }
    gsm0610_release(gsm0610_dec_state);
    printf("Test passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int perform_law_test(int full, int law, const char *name)
{
    gsm0610_state_t *gsm0610_enc_state;
    gsm0610_state_t *gsm0610_dec_state;
    int i;
    int xxx;
    int mismatches;

    if (law == 'a')
        printf("Performing A-law test '%s'\n", name);
    else
        printf("Performing u-law test '%s'\n", name);

    get_law_test_vector(full, law, name);

    if (full)
    {
        if ((gsm0610_enc_state = gsm0610_init(NULL, GSM0610_PACKING_NONE)) == NULL)
        {
            fprintf(stderr, "    Cannot create encoder\n");
            exit(2);
        }
        if (law == 'a')
        {
            for (i = 0;  i < vector_len;  i++)
                in_vector[i] = alaw_to_linear(law_in_vector[i]);
        }
        else
        {
            for (i = 0;  i < vector_len;  i++)
                in_vector[i] = ulaw_to_linear(law_in_vector[i]);
        }
        xxx = gsm0610_encode(gsm0610_enc_state, code_vector, in_vector, vector_len);

        printf("Check code vector of length %d\n", xxx);
        for (i = 0, mismatches = 0;  i < xxx;  i++)
        {
            if (code_vector[i] != ref_code_vector[i])
            {
                printf("%8d/%3d: %6d %6d %6d\n", i/76, i%76, code_vector[i], ref_code_vector[i], decoder_code_vector[i]);
                mismatches++;
            }
        }
        if (mismatches)
        {
            printf("Test failed: %d of %d samples mismatch\n", mismatches, xxx);
            exit(2);
        }
        printf("Test passed\n");
        gsm0610_release(gsm0610_enc_state);
    }

    if ((gsm0610_dec_state = gsm0610_init(NULL, GSM0610_PACKING_NONE)) == NULL)
    {
        fprintf(stderr, "    Cannot create decoder\n");
        exit(2);
    }
    xxx = gsm0610_decode(gsm0610_dec_state, out_vector, decoder_code_vector, vector_len);
    if (law == 'a')
    {
        for (i = 0;  i < vector_len;  i++)
            law_out_vector[i] = linear_to_alaw(out_vector[i]);
    }
    else
    {
        for (i = 0;  i < vector_len;  i++)
            law_out_vector[i] = linear_to_ulaw(out_vector[i]);
    }
    printf("Check output vector of length %d\n", vector_len);
    for (i = 0, mismatches = 0;  i < vector_len;  i++)
    {
        if (law_out_vector[i] != ref_law_out_vector[i])
        {
            printf("%8d: %6d %6d\n", i, law_out_vector[i], ref_law_out_vector[i]);
            mismatches++;
        }
    }
    if (mismatches)
    {
        printf("Test failed: %d of %d samples mismatch\n", mismatches, vector_len);
        exit(2);
    }
    gsm0610_release(gsm0610_dec_state);
    printf("Test passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int repack_gsm0610_voip_to_wav49(uint8_t c[], const uint8_t d[])
{
    gsm0610_frame_t frame[2];
    int n;

    n = gsm0610_unpack_voip(&frame[0], d);
    gsm0610_unpack_voip(&frame[1], d + n);
    n = gsm0610_pack_wav49(c, frame);
    return n;
}
/*- End of function --------------------------------------------------------*/

static int repack_gsm0610_wav49_to_voip(uint8_t d[], const uint8_t c[])
{
    gsm0610_frame_t frame[2];
    int n[2];

    gsm0610_unpack_wav49(frame, c);
    n[0] = gsm0610_pack_voip(d, &frame[0]);
    n[1] = gsm0610_pack_voip(d + n[0], &frame[1]);
    return n[0] + n[1];
}
/*- End of function --------------------------------------------------------*/

static int perform_pack_unpack_test(void)
{
    uint8_t a[66];
    uint8_t b[66];
    uint8_t c[66];
    int i;
    int j;

    printf("Performing packing/unpacking tests (not part of the ETSI conformance tests).\n");
    /* Try trans-packing a lot of random data looking for before/after mismatch. */
    for (j = 0;  j < 1000;  j++)
    {
        for (i = 0;  i < 65;  i++)
            a[i] = rand();
        repack_gsm0610_wav49_to_voip(b, a);
        repack_gsm0610_voip_to_wav49(c, b);
        if (memcmp(a, c, 65))
        {
            printf("Test failed: data mismatch\n");
            exit(2);
        }

        for (i = 0;  i < 66;  i++)
            a[i] = rand();
        /* Insert the magic code */
        a[0] = (a[0] & 0xF) | 0xD0;
        a[33] = (a[33] & 0xF) | 0xD0;
        repack_gsm0610_voip_to_wav49(b, a);
        repack_gsm0610_wav49_to_voip(c, b);
        //for (i = 0;  i < 66;  i++)
        //    printf("%2d: 0x%02X 0x%02X\n", i, a[i], c[i]);
        if (memcmp(a, c, 66))
        {
            printf("Test failed: data mismatch\n");
            exit(2);
        }
    }
    printf("Test passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void etsi_compliance_tests(void)
{
    perform_linear_test(true, 1, "Seq01");
    perform_linear_test(true, 1, "Seq02");
    perform_linear_test(true, 1, "Seq03");
    perform_linear_test(true, 1, "Seq04");
    perform_linear_test(false, 1, "Seq05");
    perform_law_test(true, 'a', "Seq01");
    perform_law_test(true, 'a', "Seq02");
    perform_law_test(true, 'a', "Seq03");
    perform_law_test(true, 'a', "Seq04");
    perform_law_test(false, 'a', "Seq05");
    perform_law_test(true, 'u', "Seq01");
    perform_law_test(true, 'u', "Seq02");
    perform_law_test(true, 'u', "Seq03");
    perform_law_test(true, 'u', "Seq04");
    perform_law_test(false, 'u', "Seq05");
    /* This is not actually an ETSI test */
    perform_pack_unpack_test();

    printf("Tests passed.\n");
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    SNDFILE *inhandle;
    SNDFILE *outhandle;
    int frames;
    int bytes;
    int16_t pre_amp[HIST_LEN];
    int16_t post_amp[HIST_LEN];
    uint8_t gsm0610_data[HIST_LEN];
    gsm0610_state_t *gsm0610_enc_state;
    gsm0610_state_t *gsm0610_dec_state;
    int opt;
    int etsitests;
    int packing;

    etsitests = true;
    packing = GSM0610_PACKING_NONE;
    while ((opt = getopt(argc, argv, "lp:")) != -1)
    {
        switch (opt)
        {
        case 'l':
            etsitests = false;
            break;
        case 'p':
            packing = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
        }
    }

    if (etsitests)
    {
        etsi_compliance_tests();
    }
    else
    {
        if ((inhandle = sf_open_telephony_read(IN_FILE_NAME, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open audio file '%s'\n", IN_FILE_NAME);
            exit(2);
        }
        if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }

        if ((gsm0610_enc_state = gsm0610_init(NULL, packing)) == NULL)
        {
            fprintf(stderr, "    Cannot create encoder\n");
            exit(2);
        }

        if ((gsm0610_dec_state = gsm0610_init(NULL, packing)) == NULL)
        {
            fprintf(stderr, "    Cannot create decoder\n");
            exit(2);
        }

        while ((frames = sf_readf_short(inhandle, pre_amp, 2*BLOCK_LEN)))
        {
            bytes = gsm0610_encode(gsm0610_enc_state, gsm0610_data, pre_amp, frames);
            gsm0610_decode(gsm0610_dec_state, post_amp, gsm0610_data, bytes);
            sf_writef_short(outhandle, post_amp, frames);
        }

        if (sf_close_telephony(inhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", IN_FILE_NAME);
            exit(2);
        }
        if (sf_close_telephony(outhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
            exit(2);
        }
        gsm0610_release(gsm0610_enc_state);
        gsm0610_release(gsm0610_dec_state);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
