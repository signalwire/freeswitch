/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_t6_tests.c - ITU T.4 and T.6 FAX image compression and decompression tests
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2009 Steve Underwood
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

/*! \page t4_t6_tests_page T.4 and T.6 image compress and decompression tests
\section t4_t6_tests_page_sec_1 What does it do
These tests exercise the image compression and decompression methods defined
in ITU specifications T.4 and T.6.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>

//#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#define XSIZE           1728

t4_t6_encode_state_t *send_state;
t4_t6_decode_state_t *receive_state;

/* The following are some test cases from T.4 */
#define FILL_70      "                                                                      "
#define FILL_80      "                                                                                "
#define FILL_100     "                                                                                                    "
#define FILL_670     FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_70
#define FILL_980     FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_100 FILL_80

#define TEST_ROWS   16
static const char t4_t6_test_patterns[TEST_ROWS][XSIZE + 1] =
{
    "XXXXXX              " FILL_980 "  XXX  XXX X                  " FILL_670 "                        XXXX",
    "XXXXXX              " FILL_980 "     XXX   X                  " FILL_670 "                        XXXX",
    /* Line start should code to V(0). Line middle codes to VR(3) VL(2) V(0). Line end should code to V(0) V(0). */

    " XXXX               " FILL_980 "    XXXXXXX                   " FILL_670 "                        XX  ",
    "XXXXX               " FILL_980 "XX       XX                   " FILL_670 "                        XXXX",
    /* Line start should code to VL(1). Line middle codes to H(7,2). Line end should code to V(0) VR(2) */

    "XXX                 " FILL_980 " XX  XX   XX        XXX       " FILL_670 "                      X     ",
    "                    " FILL_980 " X       XXX   XXXX           " FILL_670 "                      X   XX",
    /* Line start should code to P. Line middle codes to P VL(1) V(0) H(3,4) P. Line end codes to V(0) VL(2) V(0). */

    "XXXXX               " FILL_980 "                              " FILL_670 "                        XXXX",
    "  XXX               " FILL_980 "                              " FILL_670 "                        XX  ",
    /* Line start should code to VR(2). Line end codes to V(0) VL(2) V(0). */

    "         XX         " FILL_980 "                              " FILL_670 "                      X  XXX",
    "XXX       X         " FILL_980 "                              " FILL_670 "                      X     ",
    /* Line start should code to H(0,3) VR(1). Line end codes to V(0) VR(3). */

    "                    " FILL_980 "                              " FILL_670 "                         XX ",
    "                    " FILL_980 "                              " FILL_670 "                            ",
    /* Line end codes to P V(0) a'0. */

    "                    " FILL_980 "                              " FILL_670 "                  XXXXXXXXXX",
    "                    " FILL_980 "                              " FILL_670 "              XXXXXX  XXXXXX",
    /* Line end codes to H(2,6). */

    "                    " FILL_980 "                              " FILL_670 "                   XX  XXXXX",
    "                    " FILL_980 "                              " FILL_670 "                   XX       ",
    /* Line end codes to V(0) H(7,0). */
};

#if 0
static void dump_image_as_xxx(const uint8_t buf[], int bytes_per_row, int len)
{
    const uint8_t *s;
    int i;
    int j;
    int k;

    /* Dump the entire image as text 'X's and spaces */
    printf("Image (%d pixels x %d pixels):\n", bytes_per_row*8, len/bytes_per_row);
    s = buf;
    for (i = 0;  i < len;  i++)
    {
        for (j = 0;  j < bytes_per_row;  j++)
        {
            for (k = 0;  k < 8;  k++)
                printf((buf[i*bytes_per_row + j] & (0x80 >> k))  ?  "X"  :  " ");
        }
        printf("\n");
    }
}
/*- End of function --------------------------------------------------------*/
#endif

static int row_read_handler(void *user_data, uint8_t buf[], size_t len)
{
    int i;
    int j;
    const char *s;
    static int row = 0;

    /* Send the test pattern. */
    if (row >= TEST_ROWS)
    {
        row = 0;
        return 0;
    }
    s = t4_t6_test_patterns[row++];
    memset(buf, 0, len);
    for (i = 0;  i < len;  i++)
    {
        for (j = 0;  j < 8;  j++)
        {
            if (*s++ != ' ')
                buf[i] |= (0x80 >> j);
        }
    }
    if (*s)
        printf("Oops - '%c' at end of row %d\n", *s, row);
    return len;
}
/*- End of function --------------------------------------------------------*/

static int row_write_handler(void *user_data, const uint8_t buf[], size_t len)
{
    int i;
    int j;
    const char *s;
    static int row = 0;
    uint8_t ref[8192];

    /* Verify that what is received matches the test pattern. */
    //printf("Row %d\n", row);
    if (len == 0)
        return 0;
    s = t4_t6_test_patterns[row++];
    if (row >= TEST_ROWS)
        row = 0;
    memset(ref, 0, len);
    for (i = 0;  i < len;  i++)
    {
        for (j = 0;  j < 8;  j++)
        {
            if (*s++ != ' ')
                ref[i] |= (0x80 >> j);
        }
    }
    if (*s)
        printf("Oops - '%c' at end of row %d\n", *s, row);
    if (memcmp(buf, ref, len))
    {
        printf("Test failed at row %d\n", row);
        exit(2);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int detect_page_end(int bit, int page_ended)
{
    static int consecutive_eols;
    static int max_consecutive_eols;
    static int consecutive_zeros;
    static int consecutive_ones;
    static int eol_zeros;
    static int eol_ones;
    static int expected_eols;
    static int end_marks;

    /* Check the EOLs are added properly to the end of an image. We can't rely on the
       decoder giving the right answer, as a full set of EOLs is not needed for the
       decoder to work. */
    if (bit == -1000000)
    {
        /* Reset */
        consecutive_eols = 0;
        max_consecutive_eols = 0;
        consecutive_zeros = 0;
        consecutive_ones = 0;
        end_marks = 0;

        eol_zeros = 11;
        eol_ones = (page_ended == T4_COMPRESSION_T4_2D)  ?  2  :  1;
        expected_eols = (page_ended == T4_COMPRESSION_T6)  ?  2  :  6;
        return 0;
    }

    /* Monitor whether the EOLs are there in the correct amount */
    if (bit == 0)
    {
        consecutive_zeros++;
        consecutive_ones = 0;
    }
    else if (bit == 1)
    {
        if (++consecutive_ones == eol_ones)
        {
            if (consecutive_eols == 0  &&  consecutive_zeros >= eol_zeros)
                consecutive_eols++;
            else if (consecutive_zeros == eol_zeros)
                consecutive_eols++;
            else
                consecutive_eols = 0;
            consecutive_zeros = 0;
            consecutive_ones = 0;
        }
        if (max_consecutive_eols < consecutive_eols)
            max_consecutive_eols = consecutive_eols;
    }
    else if (bit == SIG_STATUS_END_OF_DATA)
    {
        if (end_marks == 0)
        {
            if (max_consecutive_eols != expected_eols)
            {
                printf("Only %d EOLs (should be %d)\n", max_consecutive_eols, expected_eols);
                return 2;
            }
            consecutive_zeros = 0;
            consecutive_eols = 0;
            max_consecutive_eols = 0;
        }
        if (!page_ended)
        {
            /* We might need to push a few bits to get the receiver to report the
                end of page condition (at least with T.6). */
            if (++end_marks > 50)
            {
                printf("Receiver missed the end of page mark\n");
                return 2;
            }
            return 0;
        }
        return 1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    static const int compression_sequence[] =
    {
        T4_COMPRESSION_T4_1D,
        T4_COMPRESSION_T4_2D,
        T4_COMPRESSION_T6,
        -1
    };
    int bit;
    int end_of_page;
    int end_marks;
    int compression;
    int compression_step;
    int min_row_bits;
    int opt;
    int tests_failed;
    int block_size;
    int len;
    int res;
    uint8_t chunk_buf[1024];

    tests_failed = 0;
    compression = -1;
    compression_step = 0;
    /* Use a non-zero default minimum row length to ensure we test the consecutive EOLs part
       properly. */
    min_row_bits = 50;
    block_size = 0;
    while ((opt = getopt(argc, argv, "b:c:m:")) != -1)
    {
        switch (opt)
        {
        case 'b':
            block_size = atoi(optarg);
            if (block_size > 1024)
                block_size = 1024;
            break;
        case 'c':
            if (strcmp(optarg, "T41D") == 0)
            {
                compression = T4_COMPRESSION_T4_1D;
                compression_step = -1;
            }
            else if (strcmp(optarg, "T42D") == 0)
            {
                compression = T4_COMPRESSION_T4_2D;
                compression_step = -1;
            }
            else if (strcmp(optarg, "T6") == 0)
            {
                compression = T4_COMPRESSION_T6;
                compression_step = -1;
            }
            break;
        case 'm':
            min_row_bits = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    end_of_page = false;
#if 1
    printf("Testing image_function->compress->decompress->image_function\n");
    /* Send end gets image from a function */
    if ((send_state = t4_t6_encode_init(NULL, compression, 1728, -1, row_read_handler, NULL)) == NULL)
    {
        printf("Failed to init T.4/T.6 encoder\n");
        exit(2);
    }
    span_log_set_level(t4_t6_encode_get_logging_state(send_state), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    t4_t6_encode_set_min_bits_per_row(send_state, min_row_bits);
    t4_t6_encode_set_max_2d_rows_per_1d_row(send_state, 2);

    /* Receive end puts TIFF to a function. */
    if ((receive_state = t4_t6_decode_init(NULL, compression, 1728, row_write_handler, NULL)) == NULL)
    {
        printf("Failed to init T.4/T.6 decoder\n");
        exit(2);
    }
    span_log_set_level(t4_t6_decode_get_logging_state(receive_state), SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);

    /* Now send and receive the test data with all compression modes. */
    /* If we are stepping around the compression schemes, reset to the start of the sequence. */
    if (compression_step > 0)
        compression_step = 0;
    for (;;)
    {
        end_marks = 0;
        if (compression_step >= 0)
        {
            compression = compression_sequence[compression_step++];
            if (compression < 0)
                break;
        }
        t4_t6_encode_set_encoding(send_state, compression);
        t4_t6_decode_set_encoding(receive_state, compression);

        if (t4_t6_encode_restart(send_state, 1728, -1))
            break;
        if (t4_t6_decode_restart(receive_state, 1728))
            break;
        detect_page_end(-1000000, compression);
        switch (block_size)
        {
        case 0:
            end_of_page = false;
            for (;;)
            {
                bit = t4_t6_encode_get_bit(send_state);
                if ((res = detect_page_end(bit, end_of_page)))
                {
                    if (res != 1)
                    {
                        printf("Test failed\n");
                        exit(2);
                    }
                    break;
                }
                if (!end_of_page)
                    end_of_page = t4_t6_decode_put_bit(receive_state, bit & 1);
            }
            break;
        default:
            do
            {
                len = t4_t6_encode_get(send_state, chunk_buf, block_size);
                if (len == 0)
                {
                    if (++end_marks > 50)
                    {
                        printf("Receiver missed the end of page mark\n");
                        tests_failed++;
                        break;
                    }
                    chunk_buf[0] = 0xFF;
                    len = 1;
                }
                end_of_page = t4_t6_decode_put(receive_state, chunk_buf, len);
            }
            while (!end_of_page);
            break;
        }
        if (compression_step < 0)
            break;
    }
    t4_t6_encode_release(send_state);
    t4_t6_decode_release(receive_state);
#endif
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
