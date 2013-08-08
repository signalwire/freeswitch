/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t85_tests.c - ITU T.85 FAX image compression and decompression tests
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

/*
 * These tests are based on code from Markus Kuhn's jbigkit. See
 * http://www.cl.cam.ac.uk/~mgk25/
 *
 * jbigkit is GPL2 licenced. This file is also GPL2 licenced, and our
 * T.85 code is LGPL2.1 licenced. There are no licence incompatibilities
 * in this reuse of Markus's work.
 */

/*! \file */

/*! \page t85_tests_page T.85 image compress and decompression tests
\section t85_tests_page_sec_1 What does it do
These tests exercise the image compression and decompression methods defined
in ITU specifications T.85.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#define TESTBUF_SIZE        400000
#define TEST_IMAGE_SIZE     (1951*1960/8)

uint8_t testbuf[TESTBUF_SIZE];
uint8_t test_image[TEST_IMAGE_SIZE];

size_t testbuf_len;

int read_row = 0;
int write_row = 0;

int clip_to_row = 0;

static int row_read_handler(void *user_data, uint8_t buf[], size_t len)
{
    memcpy(buf, &test_image[len*read_row], len);
    if (clip_to_row  &&  read_row == clip_to_row)
    {
        clip_to_row = 0;
        return 0;
    }
    read_row++;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int row_write_handler(void *user_data, const uint8_t buf[], size_t len)
{
    uint8_t *bitmap;

    bitmap = (uint8_t *) user_data;
    memcpy(&bitmap[len*write_row], buf, len);
    //printf("Write row %d\n", write_row);
    write_row++;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int comment_handler(void *user_data, const uint8_t buf[], size_t len)
{
    if (buf)
        printf("Comment (%lu): %s\n", (unsigned long int) len, buf);
    else
        printf("Comment (%lu): ---\n", (unsigned long int) len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void create_test_image(uint8_t *pic)
{
    int i;
    int j;
    uint32_t sum;
    uint32_t prsg;
    uint32_t repeat[8];
    uint8_t *p;

    /* Cook up the test image defined in T.82/7.2.1. This image is 1960 x 1951
       pixels, and occupies a single plane (which it has to for T.85). */
    memset(pic, 0, TEST_IMAGE_SIZE);
    p = pic;
    prsg = 1;
    for (i = 0;  i < 1951;  i++)
    {
        for (j = 0;  j < 1960;  j++)
        {
            if (i >= 192)
            {
                if (i < 1023  ||  (j & (3 << 3)) == 0)
                {
                    sum = (prsg & 1)
                        + ((prsg >> 2) & 1)
                        + ((prsg >> 11) & 1)
                        + ((prsg >> 15) & 1);
                    prsg = (prsg << 1) + (sum & 1);
                    if ((prsg & 3) == 0)
                    {
                        *p |= (1 << (7 - (j & 7)));
                        repeat[j & 7] = 1;
                    }
                    else
                    {
                        repeat[j & 7] = 0;
                    }
                }
                else
                {
                    if (repeat[j & 7])
                        *p |= 1 << (7 - (j & 7));
                }
            }
            if ((j & 7) == 7)
                ++p;
        }
    }

    /* Verify the test image has been generated OK, by checking the number of set pixels */
    sum = 0;
    for (i = 0;  i < TEST_IMAGE_SIZE;  i++)
    {
        for (j = 0;  j < 8;  j++)
            sum += ((pic[i] >> j) & 1);
    }
    if (sum != 861965)
    {
        printf("WARNING: Test image has %" PRIu32 " foreground pixels. There should be 861965.\n",
               sum);
    }
}
/*- End of function --------------------------------------------------------*/

/* Perform a test cycle, as defined in T.82/7, with one set of parameters. */
static int test_cycle(const char *test_id,
                      const uint8_t *image,
                      uint32_t width,
                      uint32_t height,
                      uint32_t l0,
                      int mx,
                      int options,
                      int optionsx,
                      const uint8_t *comment,
                      size_t correct_length)
{
    t85_encode_state_t *t85_enc;
    t85_decode_state_t *t85_dec;
    long int l;
    size_t image_size;
    int result;
    int len;
    int max_len;
    size_t bytes_per_row;
    size_t cnt_a;
    size_t cnt_b;
    uint8_t *decoded_image;

    printf("%s: TPBON=%d, LRLTWO=%d, Mx=%d, L0=%" PRIu32 "\n",
           test_id,
           (options & T85_TPBON)  ?  1  :  0,
           (options & T85_LRLTWO)  ?  1  :  0,
           mx,
           l0);

    printf("%s.1: Encode\n", test_id);
    bytes_per_row = (width + 7)/8;
    image_size = bytes_per_row*height;

    if ((optionsx & T85_VLENGTH))
    {
        t85_enc = t85_encode_init(NULL, width, height + 10, row_read_handler, NULL);
        clip_to_row = height;
    }
    else
    {
        t85_enc = t85_encode_init(NULL, width, height, row_read_handler, NULL);
        clip_to_row = 0;
    }
    read_row = 0;
    t85_encode_set_options(t85_enc, l0, mx, options);
    /* A comment inserted here should always succeed. The later one, inserted some way
       down the image, will only succeed if a new chunk is started afterwards. */
    if (comment)
        t85_encode_comment(t85_enc, comment, strlen((const char *) comment) + 1);

    testbuf_len = 0;
    max_len = 100;
    while ((len = t85_encode_get(t85_enc, &testbuf[testbuf_len], max_len)) > 0)
    {
        testbuf_len += len;
        max_len = 100;
        if (testbuf_len + 100 > TESTBUF_SIZE)
            max_len = TESTBUF_SIZE - testbuf_len;
        if (comment  &&  testbuf_len == 1000)
            t85_encode_comment(t85_enc, comment, strlen((const char *) comment) + 1);
    }
    t85_encode_release(t85_enc);
    printf("Encoded BIE has %lu bytes\n", (unsigned long int) testbuf_len);
    if (correct_length > 0)
    {
        if (testbuf_len != correct_length)
        {
            printf("Incorrect encoded length. Should have been %lu\n", (unsigned long int) correct_length);
            printf("Test failed\n");
            exit(2);
        }
        printf("Test passed\n");
    }

    printf("%s.2: Decode in one big chunk\n", test_id);
    if ((decoded_image = (uint8_t *) malloc(image_size)) == NULL)
    {
        fprintf(stderr, "Out of memory!\n");
        exit(2);
    }
    t85_dec = t85_decode_init(NULL, row_write_handler, decoded_image);
    if (comment  &&  comment[0] != 'X')
        t85_decode_set_comment_handler(t85_dec, 1000, comment_handler, NULL);
    write_row = 0;
    result = t85_decode_put(t85_dec, testbuf, testbuf_len);
    if (result == T4_DECODE_MORE_DATA)
        result = t85_decode_put(t85_dec, NULL, 0);
    cnt_a = t85_encode_get_compressed_image_size(t85_enc);
    cnt_b = t85_decode_get_compressed_image_size(t85_dec);
    if (cnt_a != cnt_b  ||  cnt_a != testbuf_len*8  ||  result != T4_DECODE_OK)
    {
        printf("Decode result %d\n", result);
        printf("%ld/%ld bits of %ld bits of BIE read. %lu lines decoded.\n",
               (long int) cnt_a,
               (long int) cnt_b,
               (unsigned long int) testbuf_len*8,
               (unsigned long int) t85_dec->y);
        printf("Test failed\n");
        exit(2);
    }
    if (memcmp(decoded_image, image, image_size))
    {
        printf("Image mismatch\n");
        printf("Test failed\n");
        exit(2);
    }
    free(decoded_image);
    t85_decode_release(t85_dec);
    printf("Test passed\n");

    printf("%s.3: Decode byte by byte\n", test_id);
    if ((decoded_image = (uint8_t *) malloc(image_size)) == NULL)
    {
        fprintf(stderr, "Out of memory!\n");
        exit(2);
    }
    t85_dec = t85_decode_init(NULL, row_write_handler, decoded_image);
    if (comment  &&  comment[0] != 'X')
        t85_decode_set_comment_handler(t85_dec, 1000, comment_handler, NULL);
    write_row = 0;
    result = T4_DECODE_MORE_DATA;
    for (l = 0;  l < testbuf_len;  l++)
    {
        result = t85_decode_put(t85_dec, &testbuf[l], 1);
        if (result != T4_DECODE_MORE_DATA)
        {
            l++;
            break;
        }
    }
    if (result == T4_DECODE_MORE_DATA)
        result = t85_decode_put(t85_dec, NULL, 0);
    if (l != testbuf_len  ||  result != T4_DECODE_OK)
    {
        printf("Decode result %d\n", result);
        printf("%ld bytes of %ld bytes of BIE read. %lu lines decoded.\n",
               (long int) l,
               (unsigned long int) testbuf_len,
               (unsigned long int) t85_dec->y);
        printf("Test failed\n");
        exit(2);
    }
    if (memcmp(decoded_image, image, image_size))
    {
        printf("Image mismatch\n");
        printf("Test failed\n");
        exit(2);
    }
    free(decoded_image);
    t85_decode_release(t85_dec);
    printf("Test passed\n");

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char **argv)
{
    printf("T.85 JBIG for FAX encoder and decoder tests, from ITU-T T.82\n\n");

    printf("Generating the test image from T.82...\n");
    create_test_image(test_image);

    /* Run through the tests in T.82/7.2, which are applicable to T.85 */
    test_cycle("1", test_image, 1960, 1951, 1951, 0, 0,          0, NULL, 317384);
    test_cycle("2", test_image, 1960, 1951, 1951, 0, T85_LRLTWO, 0, NULL, 317132);
    test_cycle("3", test_image, 1960, 1951,  128, 8, T85_TPBON,  0, NULL, 253653);
    /* Again with a comment added and handled */
    test_cycle("4", test_image, 1960, 1951, 1951, 0, 0,          0, (const uint8_t *) "Comment 4", 317384 + 16);
    test_cycle("5", test_image, 1960, 1951, 1951, 0, T85_LRLTWO, 0, (const uint8_t *) "Comment 5", 317132 + 16);
    test_cycle("6", test_image, 1960, 1951,  128, 8, T85_TPBON,  0, (const uint8_t *) "Comment 6", 253653 + 2*16);
    /* Again with a comment added, but not handled */
    test_cycle("7", test_image, 1960, 1951, 1951, 0, 0,          0, (const uint8_t *) "Xomment 7", 317384 + 16);
    test_cycle("8", test_image, 1960, 1951, 1951, 0, T85_LRLTWO, 0, (const uint8_t *) "Xomment 8", 317132 + 16);
    test_cycle("9", test_image, 1960, 1951,  128, 8, T85_TPBON,  0, (const uint8_t *) "Xomment 9", 253653 + 2*16);
    /* Again with the image variable length and prematurely terminated */
    test_cycle("10", test_image, 1960, 1951, 1951, 0, T85_VLENGTH,              T85_VLENGTH, NULL, 317384 + 8);
    test_cycle("11", test_image, 1960, 1951, 1951, 0, T85_VLENGTH | T85_LRLTWO, T85_VLENGTH, NULL, 317132 + 8);
    test_cycle("12", test_image, 1960, 1951,  128, 8, T85_VLENGTH | T85_TPBON,  T85_VLENGTH, NULL, 253653 + 8);
    /* Again with the image variable length but not prematurely terminated */
    test_cycle("13", test_image, 1960, 1951, 1951, 0, T85_VLENGTH,              0, NULL, 317384);
    test_cycle("14", test_image, 1960, 1951, 1951, 0, T85_VLENGTH | T85_LRLTWO, 0, NULL, 317132);
    test_cycle("15", test_image, 1960, 1951,  128, 8, T85_VLENGTH | T85_TPBON,  0, NULL, 253653);

    printf("Tests passed\n");

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
