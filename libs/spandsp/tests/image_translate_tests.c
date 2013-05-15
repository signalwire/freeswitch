/*
 * SpanDSP - a series of DSP components for telephony
 *
 * image_translate_tests.c - Tests for the image translation routines.
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

/*! \file */

/*! \page image_translate_tests_page Image translation tests
\section image_translate_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <errno.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#define INPUT_TIFF_FILE_NAME    "../test-data/local/lenna-colour.tif"

typedef struct
{
    const uint8_t *image;
    int width;
    int length;
    int current_row;
    int bytes_per_pixel;
} image_descriptor_t;

static void display_row(int row, int width, uint8_t buf[])
{
    int i;
    int test_pixel;

    printf("%3d: ", row);
    for (i = 0;  i < width;  i++)
    {
        test_pixel = (buf[i >> 3] >> (7 - (i & 7))) & 0x01;
        printf("%c", (test_pixel)  ?  ' '  :  '@');
    }
    printf("\n");
}
/*- End of function --------------------------------------------------------*/

static void create_undithered_50_by_50(image_descriptor_t *im, uint8_t buf[], int bytes_per_pixel)
{
    unsigned int i;
    unsigned int j;
    uint8_t *image8;
    uint16_t *image16;

    im->image = (const uint8_t *) buf;
    im->width = 50;
    im->length = 50;
    im->bytes_per_pixel = bytes_per_pixel;
    im->current_row = 0;

    switch (bytes_per_pixel)
    {
    case 1:
        image8 = buf;
        for (i = 0;  i < 50;  i++)
        {
            for (j = 0;  j < 50;  j++)
                image8[50*i + j] = ((i + j)*655) >> 8;
        }
        break;
    case 2:
        image16 = (uint16_t *) buf;
        for (i = 0;  i < 50;  i++)
        {
            for (j = 0;  j < 50;  j++)
                image16[50*i + j] = (i + j)*655;
        }
        break;
    case 3:
        image8 = buf;
        for (i = 0;  i < 50;  i++)
        {
            for (j = 0;  j < 50;  j++)
            {
#if 0
                image8[50*3*i + 3*j + 0] = ((i + j)*655) >> 8;
                image8[50*3*i + 3*j + 1] = ((i + j)*655) >> 8;
                image8[50*3*i + 3*j + 2] = ((i + j)*655) >> 8;
#else
                image8[50*3*i + 3*j + 0] = saturateu8((((i + j)*655U)*36532U) >> 23);
                image8[50*3*i + 3*j + 1] = saturateu8((((i + j)*655U)*37216U) >> 24);
                image8[50*3*i + 3*j + 2] = saturateu8((((i + j)*655U)*47900U) >> 22);
#endif
            }
        }
        break;
    case 6:
        image16 = (uint16_t *) buf;
        for (i = 0;  i < 50;  i++)
        {
            for (j = 0;  j < 50;  j++)
            {
#if 0
                image16[50*3*i + 3*j + 0] = (i + j)*655;
                image16[50*3*i + 3*j + 1] = (i + j)*655;
                image16[50*3*i + 3*j + 2] = (i + j)*655;
#else
                image16[50*3*i + 3*j + 0] = saturateu16((((i + j)*655U)*36532U) >> 15);
                image16[50*3*i + 3*j + 1] = saturateu16((((i + j)*655U)*37216U) >> 16);
                image16[50*3*i + 3*j + 2] = saturateu16((((i + j)*655U)*47900U) >> 14);
#endif
            }
        }
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static int test_dithered_50_by_50(int row, int width, uint8_t buf[])
{
    static const char *image[50] =
    {
        "  0:                               @   @  @  @ @ @ @ @ ",
        "  1:                @   @  @ @ @ @  @ @  @  @  @  @ @ @",
        "  2:           @      @   @     @  @   @  @ @ @ @ @ @ @",
        "  3:               @    @    @ @  @  @ @ @ @ @ @ @ @ @ ",
        "  4:       @  @  @   @    @      @  @   @  @  @ @ @ @ @",
        "  5:                    @  @ @ @  @  @ @  @ @ @  @ @ @ ",
        "  6:    @      @  @  @       @  @  @  @ @ @  @ @ @ @ @@",
        "  7:         @     @   @ @ @  @  @  @ @  @ @ @ @ @ @ @ ",
        "  8:      @     @    @    @  @  @ @  @ @ @ @ @ @ @ @ @ ",
        "  9:   @      @    @    @   @  @   @ @  @ @ @ @ @ @ @ @",
        " 10:      @     @    @ @  @  @  @ @  @ @  @ @ @ @ @@ @@",
        " 11:          @   @ @    @  @  @  @ @ @ @ @ @ @ @ @ @@ ",
        " 12:   @  @ @        @ @  @  @ @ @  @  @ @ @ @ @@ @ @ @",
        " 13:           @ @ @    @  @  @  @ @ @ @ @ @ @ @ @ @@ @",
        " 14:   @   @        @ @  @  @  @  @ @ @ @ @ @ @ @@ @ @@",
        " 15:     @   @ @ @ @    @  @ @ @ @ @ @ @ @ @ @ @@ @ @@ ",
        " 16:                @ @  @  @  @ @  @ @ @ @ @@ @ @@@ @@",
        " 17:   @  @ @ @ @ @  @  @  @  @ @ @ @ @ @ @ @ @ @ @ @ @",
        " 18:     @       @  @  @ @ @ @  @ @ @ @ @ @@ @@ @@ @@@ ",
        " 19: @ @     @ @  @  @ @  @  @ @ @ @ @ @ @ @ @ @ @@ @ @",
        " 20:     @ @   @   @   @ @ @ @ @  @ @ @ @@ @@ @@@ @@@@@",
        " 21:   @     @  @ @ @ @  @  @ @ @@ @ @ @ @ @ @ @ @@ @ @",
        " 22:    @ @ @  @    @  @  @ @  @  @ @ @@ @@ @@@ @ @@ @@",
        " 23:  @       @  @ @  @ @ @ @ @ @@ @ @ @ @ @ @ @@@ @@ @",
        " 24:   @ @ @ @  @ @ @ @ @ @ @ @ @ @ @@ @@ @@@ @ @ @@ @@",
        " 25: @    @   @  @   @  @  @ @ @ @ @ @ @ @ @ @@@@@ @@@@",
        " 26:   @   @ @  @ @ @ @ @ @ @ @ @ @@ @@ @@@ @ @ @@@ @ @",
        " 27: @  @ @   @  @  @  @ @ @ @ @ @ @ @ @ @ @@@ @@ @@@@@",
        " 28:   @   @ @  @ @ @ @ @ @ @ @ @ @ @@ @@ @ @ @@ @@ @ @",
        " 29: @  @ @   @ @  @  @ @ @ @ @ @@ @@ @@ @@@@@ @@@ @@@@",
        " 30:   @   @ @  @ @ @ @ @ @ @ @@ @@ @ @ @@ @ @@@ @@@ @@",
        " 31: @  @ @ @  @ @ @ @ @ @ @ @ @ @ @@ @@@ @ @@ @@@ @@@@",
        " 32:   @   @ @  @  @ @ @ @ @ @ @ @ @ @@ @@@@@ @@ @@@ @@",
        " 33: @  @ @  @ @ @ @ @ @ @ @ @@ @@@ @@ @ @ @ @@@@@ @@@@",
        " 34:  @  @ @  @ @ @ @ @ @ @@ @ @ @ @@ @@@@@@@ @ @@@@ @@",
        " 35:   @  @  @ @  @ @ @ @ @ @@ @@ @@ @@ @ @ @@@@@@ @@@@",
        " 36:  @ @  @ @ @ @ @ @ @ @ @ @@ @@ @@ @@@ @@ @ @@@@@@ @",
        " 37:  @  @ @ @ @ @ @ @ @@ @@ @ @ @@ @@ @ @@@@@@@ @@ @@@",
        " 38:  @ @ @ @ @ @ @ @ @ @ @ @@@ @@ @@ @@@@ @ @@@@@@@@@@",
        " 39:   @  @  @  @ @ @ @ @@ @@ @@ @@ @@@@ @@@@ @@ @@ @@@",
        " 40: @  @ @ @ @@ @ @ @@ @ @ @ @ @@ @@ @ @@ @@@@@@@@@@@@",
        " 41: @ @ @ @ @  @ @ @@ @ @@@ @@@ @@@ @@@@@@@@@ @@ @@ @@",
        " 42:  @ @ @ @ @@ @ @ @ @@ @ @@ @@ @@@@ @ @ @@@@@@@@@@@@",
        " 43: @ @  @ @ @ @ @@ @@ @ @@ @@ @@@ @ @@@@@@@ @@ @@@@@@",
        " 44:  @ @ @ @ @ @ @ @ @@ @@ @@ @@ @@@@@ @@ @@@@@@@@ @@@",
        " 45: @ @ @ @ @ @ @@ @@ @@ @@ @@ @@@ @ @@@@@@@@ @@@@@@@@",
        " 46:  @ @ @ @ @ @ @@ @ @ @@ @@ @@ @@@@@ @ @@ @@@@@@@@@@",
        " 47: @ @ @ @ @ @@ @ @ @@@@ @@@@ @@@@@ @@@@@@@@@@@ @@@@@",
        " 48:  @ @ @ @@ @ @@ @@ @ @@ @ @@@ @ @@@@@ @@@@@@@@@@@@@",
        " 49: @ @ @ @ @ @@ @@ @@ @@ @@@@ @@@@@@@ @@@@@@ @@@@@@@@"
    };
    int i;
    int match;
    int ref_pixel;
    int test_pixel;

    match = 0;
    for (i = 0;  i < width;  i++)
    {
        ref_pixel = (image[row][i + 5] == ' ');
        test_pixel = (buf[i >> 3] >> (7 - (i & 7))) & 0x01;
        if (ref_pixel != test_pixel)
            match = -1;
    }
    return match;
}
/*- End of function --------------------------------------------------------*/

static int row_read(void *user_data, uint8_t buf[], size_t len)
{
    image_descriptor_t *im;

    im = (image_descriptor_t *) user_data;
    if (im->current_row >= im->length)
        return 0;
    memcpy(buf, &im->image[im->current_row*im->width*im->bytes_per_pixel], len);
    im->current_row++;
    return len;
}
/*- End of function --------------------------------------------------------*/

static void get_bilevel_image(image_translate_state_t *s, int compare)
{
    int i;
    int len;
    uint8_t row_buf[5000];

    for (i = 0;  i < s->output_length;  i++)
    {
        if ((len = image_translate_row(s, row_buf, (s->output_width + 7)/8)) != (s->output_width + 7)/8)
        {
            printf("Image finished early - %d %d\n", len, (s->output_width + 7)/8);
            exit(2);
        }
        display_row(i, s->output_width, row_buf);
        if (compare)
        {
            if (test_dithered_50_by_50(i, s->output_width, row_buf))
            {
                printf("Dithered image mismatch at row %d\n", i);
                //exit(2);
            }
        }
    }
    if ((len = image_translate_row(s, row_buf, (s->output_width + 7)/8)) != 0)
    {
        printf("Image finished late - %d %d\n", len, (s->output_width + 7)/8);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void get_gray8_image(image_translate_state_t *s, int compare)
{
    unsigned int i;
    unsigned int j;
    int len;
    uint8_t row_buf[5000];

    for (i = 0;  i < s->output_length;  i++)
    {
        if ((len = image_translate_row(s, row_buf, s->output_width)) != s->output_width)
        {
            printf("Image finished early - %d %d\n", len, s->output_width);
            exit(2);
        }
        if (compare)
        {
            for (j = 0;  j < 50;  j++)
            {
                if (row_buf[j] != (((i + j)*655) >> 8))
                {
                    printf("Image mismatch - %dx%d - %d %d\n", j, i, ((i + j)*655) >> 8, row_buf[j]);
                    //exit(2);
                }
            }
        }
    }
    if ((len = image_translate_row(s, row_buf, s->output_width)) != 0)
    {
        printf("Image finished late - %d %d\n", len, s->output_width);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void get_gray16_image(image_translate_state_t *s, int compare)
{
    unsigned int i;
    unsigned int j;
    int len;
    uint16_t row_buf[5000];

    for (i = 0;  i < s->output_length;  i++)
    {
        if ((len = image_translate_row(s, (uint8_t *) row_buf, 2*s->output_width)) != 2*s->output_width)
        {
            printf("Image finished early - %d %d\n", len, 2*s->output_width);
            exit(2);
        }
        if (compare)
        {
            for (j = 0;  j < 50;  j++)
            {
                if (row_buf[j] != (i + j)*655)
                {
                    printf("Image mismatch - %dx%d - %d %d\n", j, i, (i + j)*655, row_buf[j]);
                    //exit(2);
                }
            }
        }
    }
    if ((len = image_translate_row(s, (uint8_t *) row_buf, 2*s->output_width)) != 0)
    {
        printf("Image finished late - %d %d\n", len, 2*s->output_width);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void get_colour8_image(image_translate_state_t *s, int compare)
{
    unsigned int i;
    unsigned int j;
    int len;
    int r;
    int g;
    int b;
    uint8_t row_buf[5000];

    for (i = 0;  i < s->output_length;  i++)
    {
        if ((len = image_translate_row(s, row_buf, 3*s->output_width)) != 3*s->output_width)
        {
            printf("Image finished early - %d %d\n", len, 3*s->output_width);
            exit(2);
        }
        if (compare)
        {
            for (j = 0;  j < 50;  j++)
            {
#if 0
                r = ((i + j)*655) >> 8;
                g = ((i + j)*655) >> 8;
                b = ((i + j)*655) >> 8;
#else
                r = saturateu8((((i + j)*655U)*36532U) >> 23);
                g = saturateu8((((i + j)*655U)*37216U) >> 24);
                b = saturateu8((((i + j)*655U)*47900U) >> 22);
#endif
                if (row_buf[3*j + 0] != r  ||  row_buf[3*j + 1] != g  ||  row_buf[3*j + 2] != b)
                {
                    printf("Image mismatch - %dx%d - (%d %d %d) (%d %d %d)\n",
                           j, i,
                           r, g, b,
                           row_buf[3*j + 0], row_buf[3*j + 1], row_buf[3*j + 2]);
                    //exit(2);
                }
            }
        }
    }
    if ((len = image_translate_row(s, row_buf, 2*s->output_width)) != 0)
    {
        printf("Image finished late - %d %d\n", len, 3*s->output_width);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void get_colour16_image(image_translate_state_t *s, int compare)
{
    unsigned int i;
    unsigned int j;
    int len;
    int r;
    int g;
    int b;
    uint16_t row_buf[5000];

    for (i = 0;  i < s->output_length;  i++)
    {
        if ((len = image_translate_row(s, (uint8_t *) row_buf, 6*s->output_width)) != 6*s->output_width)
        {
            printf("Image finished early - %d %d\n", len, 6*s->output_width);
            exit(2);
        }
        if (compare)
        {
            for (j = 0;  j < 50;  j++)
            {
#if 0
                r = (i + j)*655;
                g = (i + j)*655;
                b = (i + j)*655;
#else
                r = saturateu16((((i + j)*655U)*36532U) >> 15);
                g = saturateu16((((i + j)*655U)*37216U) >> 16);
                b = saturateu16((((i + j)*655U)*47900U) >> 14);
#endif
                if (row_buf[3*j + 0] != r  ||  row_buf[3*j + 1] != g  ||  row_buf[3*j + 2] != b)
                {
                    printf("Image mismatch - %dx%d - (%d %d %d) (%d %d %d)\n",
                           j, i,
                           r, g, b,
                           row_buf[3*j + 0], row_buf[3*j + 1], row_buf[3*j + 2]);
                    //exit(2);
                }
            }
        }
    }
    if ((len = image_translate_row(s, (uint8_t *) row_buf, 6*s->output_width)) != 0)
    {
        printf("Image finished late - %d %d\n", len, 6*s->output_width);
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void translate_tests_gray16(void)
{
    image_translate_state_t *s;
    uint16_t image[50*50];
    image_descriptor_t im;

    printf("Dithering from a 16 bit per sample gray scale to bi-level\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 2);
    s = image_translate_init(NULL, T4_IMAGE_TYPE_BILEVEL, -1, -1, T4_IMAGE_TYPE_GRAY_12BIT, im.width, im.length, row_read, &im);
    get_bilevel_image(s, TRUE);

    printf("Scrunching from a 16 bit per sample gray scale to 8 bit per sample gray scale\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 2);
    s = image_translate_init(s, T4_IMAGE_TYPE_GRAY_8BIT, -1, -1, T4_IMAGE_TYPE_GRAY_12BIT, im.width, im.length, row_read, &im);
    get_gray8_image(s, TRUE);

    printf("Scrunching from a 16 bit per sample gray scale to 16 bit per sample gray scale\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 2);
    s = image_translate_init(s, T4_IMAGE_TYPE_GRAY_12BIT, -1, -1, T4_IMAGE_TYPE_GRAY_12BIT, im.width, im.length, row_read, &im);
    get_gray16_image(s, TRUE);

    printf("Scrunching from a 16 bit per sample gray scale to 3x8 bit per sample colour\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 2);
    s = image_translate_init(s, T4_IMAGE_TYPE_COLOUR_8BIT, -1, -1, T4_IMAGE_TYPE_GRAY_12BIT, im.width, im.length, row_read, &im);
    get_colour8_image(s, TRUE);

    printf("Scrunching from a 16 bit per sample gray scale to 3x16 bit per sample colour\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 2);
    s = image_translate_init(s, T4_IMAGE_TYPE_COLOUR_12BIT, -1, -1, T4_IMAGE_TYPE_GRAY_12BIT, im.width, im.length, row_read, &im);
    get_colour16_image(s, TRUE);

    image_translate_free(s);
}
/*- End of function --------------------------------------------------------*/

static void translate_tests_gray8(void)
{
    image_translate_state_t *s;
    uint8_t image[50*50];
    image_descriptor_t im;

    printf("Dithering from a 8 bit per sample gray scale to bi-level\n");
    create_undithered_50_by_50(&im, image, 1);
    s = image_translate_init(NULL, T4_IMAGE_TYPE_BILEVEL, -1, -1, T4_IMAGE_TYPE_GRAY_8BIT, im.width, im.length, row_read, &im);
    get_bilevel_image(s, TRUE);

    printf("Scrunching from a 8 bit per sample gray scale to 8 bit per sample gray scale\n");
    create_undithered_50_by_50(&im, image, 1);
    s = image_translate_init(s, T4_IMAGE_TYPE_GRAY_8BIT, -1, -1, T4_IMAGE_TYPE_GRAY_8BIT, im.width, im.length, row_read, &im);
    get_gray8_image(s, TRUE);

    printf("Scrunching from a 8 bit per sample gray scale to 16 bit per sample gray scale\n");
    create_undithered_50_by_50(&im, image, 1);
    s = image_translate_init(s, T4_IMAGE_TYPE_GRAY_12BIT, -1, -1, T4_IMAGE_TYPE_GRAY_8BIT, im.width, im.length, row_read, &im);
    get_gray16_image(s, TRUE);

    printf("Scrunching from a 8 bit per sample gray scale to 3x8 bit per sample colour\n");
    create_undithered_50_by_50(&im, image, 1);
    s = image_translate_init(s, T4_IMAGE_TYPE_COLOUR_8BIT, -1, -1, T4_IMAGE_TYPE_GRAY_8BIT, im.width, im.length, row_read, &im);
    get_colour8_image(s, TRUE);

    printf("Scrunching from a 8 bit per sample gray scale to 3x16 bit per sample colour\n");
    create_undithered_50_by_50(&im, image, 1);
    s = image_translate_init(s, T4_IMAGE_TYPE_COLOUR_12BIT, -1, -1, T4_IMAGE_TYPE_GRAY_8BIT, im.width, im.length, row_read, &im);
    get_colour16_image(s, TRUE);

    image_translate_free(s);
}
/*- End of function --------------------------------------------------------*/

static void translate_tests_colour16(void)
{
    image_translate_state_t *s;
    uint16_t image[50*50*3];
    image_descriptor_t im;

    printf("Dithering from a 3x16 bit per sample colour to bi-level\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 6);
    s = image_translate_init(NULL, T4_IMAGE_TYPE_BILEVEL, -1, -1, T4_IMAGE_TYPE_COLOUR_12BIT, im.width, im.length, row_read, &im);
    get_bilevel_image(s, TRUE);

    printf("Scrunching from a 3x16 bit per sample colour to 8 bit per sample gray scale\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 6);
    s = image_translate_init(s, T4_IMAGE_TYPE_GRAY_8BIT, -1, -1, T4_IMAGE_TYPE_COLOUR_12BIT, im.width, im.length, row_read, &im);
    get_gray8_image(s, TRUE);

    printf("Scrunching from a 3x16 bit per sample colour to 16 bit per sample gray scale\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 6);
    s = image_translate_init(s, T4_IMAGE_TYPE_GRAY_12BIT, -1, -1, T4_IMAGE_TYPE_COLOUR_12BIT, im.width, im.length, row_read, &im);
    get_gray16_image(s, TRUE);

    printf("Scrunching from a 3x16 bit per sample colour to 3x8 bit per sample colour\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 6);
    s = image_translate_init(s, T4_IMAGE_TYPE_COLOUR_8BIT, -1, -1, T4_IMAGE_TYPE_COLOUR_12BIT, im.width, im.length, row_read, &im);
    get_colour8_image(s, TRUE);

    printf("Scrunching from a 3x16 bit per sample colour to 3x16 bit per sample colour\n");
    create_undithered_50_by_50(&im, (uint8_t *) image, 6);
    s = image_translate_init(s, T4_IMAGE_TYPE_COLOUR_12BIT, -1, -1, T4_IMAGE_TYPE_COLOUR_12BIT, im.width, im.length, row_read, &im);
    get_colour16_image(s, TRUE);

    image_translate_free(s);
}
/*- End of function --------------------------------------------------------*/

static void translate_tests_colour8(void)
{
    image_translate_state_t *s;
    uint8_t image[50*50*3];
    image_descriptor_t im;

    printf("Dithering from a 3x8 bit per sample colour to bi-level\n");
    create_undithered_50_by_50(&im, image, 3);
    s = image_translate_init(NULL, T4_IMAGE_TYPE_BILEVEL, -1, -1, T4_IMAGE_TYPE_COLOUR_8BIT, im.width, im.length, row_read, &im);
    get_bilevel_image(s, TRUE);

    printf("Scrunching from a 3x8 bit per sample colour to 8 bit per sample gray scale\n");
    create_undithered_50_by_50(&im, image, 3);
    s = image_translate_init(s, T4_IMAGE_TYPE_GRAY_8BIT, -1, -1, T4_IMAGE_TYPE_COLOUR_8BIT, im.width, im.length, row_read, &im);
    get_gray8_image(s, TRUE);

    printf("Scrunching from a 3x8 bit per sample colour to 16 bit per sample gray scale\n");
    create_undithered_50_by_50(&im, image, 3);
    s = image_translate_init(s, T4_IMAGE_TYPE_GRAY_12BIT, -1, -1, T4_IMAGE_TYPE_COLOUR_8BIT, im.width, im.length, row_read, &im);
    get_gray16_image(s, TRUE);

    printf("Scrunching from a 3x8 bit per sample colour to 3x8 bit per sample colour\n");
    create_undithered_50_by_50(&im, image, 3);
    s = image_translate_init(s, T4_IMAGE_TYPE_COLOUR_8BIT, -1, -1, T4_IMAGE_TYPE_COLOUR_8BIT, im.width, im.length, row_read, &im);
    get_colour8_image(s, TRUE);

    printf("Scrunching from a 3x8 bit per sample colour to 3x16 bit per sample colour\n");
    create_undithered_50_by_50(&im, image, 3);
    s = image_translate_init(s, T4_IMAGE_TYPE_COLOUR_12BIT, -1, -1, T4_IMAGE_TYPE_COLOUR_8BIT, im.width, im.length, row_read, &im);
    get_colour16_image(s, TRUE);

    image_translate_free(s);
}
/*- End of function --------------------------------------------------------*/

static void grow_tests_colour8(void)
{
    image_translate_state_t *s;
    uint8_t image[50*50*3];
    image_descriptor_t im;

    printf("Image growth tests\n");
    create_undithered_50_by_50(&im, image, 3);

    s = image_translate_init(NULL, T4_IMAGE_TYPE_BILEVEL, 200, -1, T4_IMAGE_TYPE_COLOUR_8BIT, im.width, im.length, row_read, &im);
    get_bilevel_image(s, FALSE);
    image_translate_free(s);
}
/*- End of function --------------------------------------------------------*/

static int row_read2(void *user_data, uint8_t buf[], size_t len)
{
    image_translate_state_t *s;

    s = (image_translate_state_t *) user_data;
    image_translate_row(s, buf, len);
    return len;
}
/*- End of function --------------------------------------------------------*/

static void lenna_tests(int output_width, int output_length_scaling, const char *file)
{
    TIFF *in_file;
    TIFF *out_file;
    int image_width;
    int image_length;
    int output_length;
    uint8_t *image;
    uint8_t *image2;
    int len;
    int total;
    int16_t bits_per_sample;
    int16_t samples_per_pixel;
    int i;
    int n;
    image_translate_state_t *s;
    image_translate_state_t *s2;
    image_descriptor_t im;
    float x_resolution;
    float y_resolution;
    uint16_t res_unit;

    if (output_length_scaling >= 0)
        printf("Dithering Lenna from colour to bi-level test\n");
    else
        printf("Processing Lenna test\n");
    if ((in_file = TIFFOpen(INPUT_TIFF_FILE_NAME, "r")) == NULL)
        return;
    image_width = 0;
    TIFFGetField(in_file, TIFFTAG_IMAGEWIDTH, &image_width);
    if (image_width <= 0)
        return;
    image_length = 0;
    TIFFGetField(in_file, TIFFTAG_IMAGELENGTH, &image_length);
    if (image_length <= 0)
        return;
    x_resolution = 200.0;
    TIFFGetField(in_file, TIFFTAG_XRESOLUTION, &x_resolution);
    y_resolution = 200.0;
    TIFFGetField(in_file, TIFFTAG_YRESOLUTION, &y_resolution);
    res_unit = RESUNIT_INCH;
    TIFFGetField(in_file, TIFFTAG_RESOLUTIONUNIT, &res_unit);
    bits_per_sample = 0;
    TIFFGetField(in_file, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    samples_per_pixel = 0;
    TIFFGetField(in_file, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    printf("Original image is %d x %d, %.2f x %.2f resolution, %d bits per sample, %d samples per pixel\n", image_width, image_length, x_resolution, y_resolution, bits_per_sample, samples_per_pixel);
    if ((image = malloc(image_width*image_length*samples_per_pixel)) == NULL)
        return;
    for (total = 0, i = 0;  i < 1000;  i++)
    {
        len = TIFFReadEncodedStrip(in_file, i, &image[total], image_width*image_length*samples_per_pixel - total);
        if (len <= 0)
            break;
        total += len;
        if (total == image_width*image_length*samples_per_pixel)
        {
            printf("Done\n");
            break;
        }
    }
    printf("Image size %d %d\n", total, image_width*image_length*samples_per_pixel);
    TIFFClose(in_file);

    if (output_length_scaling > 0)
        output_length = (double) image_length*output_length_scaling*output_width/image_width;
    else
        output_length = -1;

    im.image = image;
    im.width = image_width;
    im.length = image_length;
    im.current_row = 0;
    im.bytes_per_pixel = samples_per_pixel;

    s2 = NULL;
    switch (output_length_scaling)
    {
    case -2:
        s = image_translate_init(NULL, T4_IMAGE_TYPE_GRAY_8BIT, output_width, output_length, T4_IMAGE_TYPE_COLOUR_8BIT, image_width, image_length, row_read, &im);
        output_width = image_translate_get_output_width(s);
        output_length = image_translate_get_output_length(s);
        s2 = image_translate_init(NULL, T4_IMAGE_TYPE_COLOUR_8BIT, -1, -1, T4_IMAGE_TYPE_GRAY_8BIT, output_width, output_length, row_read2, s);
        output_width = image_translate_get_output_width(s2);
        output_length = image_translate_get_output_length(s2);
        break;
    case -1:
        s = image_translate_init(NULL, T4_IMAGE_TYPE_COLOUR_8BIT, output_width, output_length, T4_IMAGE_TYPE_COLOUR_8BIT, image_width, image_length, row_read, &im);
        output_width = image_translate_get_output_width(s);
        output_length = image_translate_get_output_length(s);
        break;
    default:
        s = image_translate_init(NULL, T4_IMAGE_TYPE_BILEVEL, output_width, output_length, T4_IMAGE_TYPE_COLOUR_8BIT, image_width, image_length, row_read, &im);
        output_width = image_translate_get_output_width(s);
        output_length = image_translate_get_output_length(s);
        break;
    }

    if ((out_file = TIFFOpen(file, "w")) == NULL)
        return;
    TIFFSetField(out_file, TIFFTAG_IMAGEWIDTH, output_width);
    TIFFSetField(out_file, TIFFTAG_IMAGELENGTH, output_length);
    TIFFSetField(out_file, TIFFTAG_RESOLUTIONUNIT, res_unit);

    switch (output_length_scaling)
    {
    case -2:
    case -1:
        TIFFSetField(out_file, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(out_file, TIFFTAG_SAMPLESPERPIXEL, 3);
        TIFFSetField(out_file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        break;
    default:
        TIFFSetField(out_file, TIFFTAG_BITSPERSAMPLE, 1);
        TIFFSetField(out_file, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(out_file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
        break;
    }
    if (output_length_scaling > 0)
        y_resolution *= output_length_scaling;
    TIFFSetField(out_file, TIFFTAG_XRESOLUTION, x_resolution);
    TIFFSetField(out_file, TIFFTAG_YRESOLUTION, y_resolution);
    TIFFSetField(out_file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out_file, TIFFTAG_ROWSPERSTRIP, -1);
    TIFFSetField(out_file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out_file, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);
    TIFFSetField(out_file, TIFFTAG_PAGENUMBER, 0, 1);

    printf("Input %d x %d, output %d x %d\n", image_width, image_length, output_width, output_length);
    switch (output_length_scaling)
    {
    case -2:
        if ((image2 = malloc(output_width*output_length*3)) == NULL)
            return;
        memset(image2, 0, output_width*output_length*3);
        n = 0;
        for (i = 0;  i < output_length;  i++)
            n += image_translate_row(s2, &image2[n], output_width*3);
        TIFFWriteEncodedStrip(out_file, 0, image2, n);
        break;
    case -1:
        if ((image2 = malloc(output_width*output_length*3)) == NULL)
            return;
        memset(image2, 0, output_width*output_length*3);
        n = 0;
        for (i = 0;  i < output_length;  i++)
            n += image_translate_row(s, &image2[n], output_width*3);
        TIFFWriteEncodedStrip(out_file, 0, image2, n);
        break;
    default:
        if ((image2 = malloc(output_width*output_length/8)) == NULL)
            return;
        memset(image2, 0, output_width*output_length/8);
        n = 0;
        for (i = 0;  i < output_length;  i++)
            n += image_translate_row(s, &image2[n], output_width/8);
        TIFFWriteEncodedStrip(out_file, 0, image2, n);
        break;
    }
    TIFFWriteDirectory(out_file);
    TIFFClose(out_file);
    image_translate_free(s);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char **argv)
{
#if 1
    translate_tests_gray16();
    translate_tests_gray8();
    translate_tests_colour16();
    translate_tests_colour8();
#endif
#if 1
    grow_tests_colour8();
#endif
#if 1
    lenna_tests(0, 0, "lenna-bw.tif");
    lenna_tests(200, 0, "lenna-bw-200.tif");
    lenna_tests(1728, 0, "lenna-bw-1728.tif");
    lenna_tests(1728, 2, "lenna-bw-1728-superfine.tif");
    lenna_tests(1728, -1, "lenna-colour-1728.tif");
    lenna_tests(1728, -2, "lenna-gray-1728.tif");
#endif
    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
