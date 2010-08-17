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

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

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

static int test_dithered_50_by_50(int row, int width, uint8_t buf[])
{
    static const char *image[50] =
    {
        "  0:                  @  @  @ @ @ @ @ @@ @@@@@@@@@@@@@@",
        "  1:             @ @ @  @  @ @ @ @ @ @@ @@ @ @ @ @ @@@@",
        "  2:        @ @       @  @  @  @ @ @@ @ @ @@@ @@@@@@ @@",
        "  3:              @ @  @  @ @ @ @ @ @ @ @@@ @@@@ @@@@@@",
        "  4:      @    @     @  @  @ @ @ @ @ @@@ @ @@ @@@@ @@@@",
        "  5:         @   @ @  @  @ @  @ @ @ @ @ @ @@ @@@ @@@@@@",
        "  6:                 @  @  @ @ @ @ @@ @@@@ @@@ @@@ @@ @",
        "  7:           @ @ @   @  @  @ @ @ @ @@  @@ @ @@ @@@@@@",
        "  8:      @         @   @  @ @  @ @ @ @@@ @@@@@@@@@ @@@",
        "  9:        @  @  @  @ @ @ @ @ @ @ @ @ @ @@ @ @ @ @@@@@",
        " 10:                @    @  @ @ @ @@ @ @@ @@@@@@@@@@@@@",
        " 11:            @ @   @ @  @ @ @ @ @ @@ @@ @ @ @@ @@ @@",
        " 12:      @  @      @   @ @   @ @ @ @ @@ @@@@@@ @@@@@@@",
        " 13:           @  @  @ @   @ @ @ @ @@ @ @ @ @ @@@@@ @@@",
        " 14:                @   @ @ @ @ @ @ @@ @@@ @@@ @ @@@@@@",
        " 15:         @  @ @   @ @  @  @ @ @ @ @ @ @@ @@@@@@@ @@",
        " 16:     @          @    @  @ @ @ @ @ @@ @@ @@ @@ @@@@@",
        " 17:           @  @   @ @ @ @ @ @ @ @@ @@ @@ @@ @@@@@@@",
        " 18:       @     @  @  @  @  @ @ @ @ @ @ @@ @@@@@ @ @@@",
        " 19:           @     @   @  @ @ @ @ @ @@@ @@@ @ @@@@@@@",
        " 20:         @   @ @  @ @ @ @  @ @ @@ @ @@ @@@@@@ @@@ @",
        " 21:      @         @      @  @ @ @ @ @@ @@ @ @ @@@@@@@",
        " 22:          @  @   @ @ @  @@ @ @ @ @ @@ @@@@@@@ @@ @@",
        " 23:        @      @  @  @ @   @ @ @@ @@ @@ @ @ @@@@@@@",
        " 24:           @     @  @  @ @@ @ @ @@ @ @ @@@@@@ @@@@@",
        " 25:             @ @  @  @ @ @  @ @ @ @ @@@@@ @ @@@@ @@",
        " 26:     @  @        @  @   @ @ @ @ @ @@ @ @ @@@@ @@@@@",
        " 27:           @ @ @  @  @ @  @ @ @ @@ @ @@ @@@ @@@@@@@",
        " 28:                 @  @ @ @ @ @ @ @ @@@ @@@ @@@ @@ @@",
        " 29:         @  @  @  @  @   @ @ @ @@ @ @@ @ @@ @@@@@@@",
        " 30:      @       @    @  @ @ @ @ @ @ @ @ @@@@@@@ @@@@@",
        " 31:            @    @  @ @ @  @ @ @ @ @@@@ @ @ @@@@ @@",
        " 32:        @  @  @ @  @   @ @ @ @ @@ @ @ @@@@@@@ @@@@@",
        " 33:                 @  @ @  @ @ @ @ @@@ @@ @ @ @@@@@@@",
        " 34:     @   @  @  @   @  @ @ @ @ @ @ @ @@ @@@@@@ @ @@@",
        " 35:             @    @  @  @  @ @ @ @ @ @@ @ @ @@@@@@@",
        " 36:          @    @ @  @  @ @ @ @ @@ @@@ @@@@@@@ @@@ @",
        " 37:        @    @     @  @  @ @ @ @ @ @ @@ @ @ @@@@@@@",
        " 38:               @ @  @ @ @ @ @ @ @@ @@ @@@@@@@ @@ @@",
        " 39:      @   @ @ @    @   @  @ @ @ @ @@ @@ @ @ @@@@@@@",
        " 40:                @ @  @ @ @ @ @ @ @ @@ @@ @@@@ @@@@@",
        " 41:        @   @  @   @ @  @ @ @ @ @@ @ @@ @@@ @@@@ @@",
        " 42:              @  @   @ @  @ @ @@ @@ @@ @@ @@@ @@@@@",
        " 43:         @  @      @  @  @ @ @ @ @ @ @@ @@@ @@@@@@@",
        " 44:     @        @ @ @  @ @ @  @ @ @ @@@ @@@ @@@ @@ @@",
        " 45:            @       @   @ @ @ @ @@ @ @ @ @@ @@@@@@@",
        " 46:       @  @   @ @ @  @ @ @ @ @ @ @@ @@@@@@@@@ @@@@@",
        " 47:            @    @  @ @ @  @ @ @ @ @@ @ @ @ @@@@ @@",
        " 48:              @   @    @ @ @ @ @@ @ @@ @@@@@@ @@@@@",
        " 49:     @   @  @   @  @ @ @  @ @ @ @ @@ @@ @ @ @@@@@@@"
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

static void get_flattened_image(image_translate_state_t *s, int compare)
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
                printf("Test failed\n");
                exit(2);
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

static void dither_tests_gray16(void)
{
    int i;
    int j;
    image_translate_state_t bw;
    image_translate_state_t *s = &bw;
    uint16_t image[50*50];
    image_descriptor_t im;

    printf("Dithering from a 16 bit per sample gray scale to bi-level\n");
    im.image = (const uint8_t *) image;
    im.width = 50;
    im.length = 50;
    im.bytes_per_pixel = 2;
    im.current_row = 0;

    for (i = 0;  i < im.length;  i++)
    {
        for (j = 0;  j < im.width;  j++)
            image[i*im.width + j] = j*1200;
    }

    s = image_translate_init(s, IMAGE_TRANSLATE_FROM_GRAY_16, im.width, im.length, -1, row_read, &im);
    get_flattened_image(s, TRUE);
}
/*- End of function --------------------------------------------------------*/

static void dither_tests_gray8(void)
{
    int i;
    int j;
    image_translate_state_t bw;
    image_translate_state_t *s = &bw;
    uint8_t image[50*50];
    image_descriptor_t im;

    printf("Dithering from a 8 bit per sample gray scale to bi-level\n");
    im.image = image;
    im.width = 50;
    im.length = 50;
    im.bytes_per_pixel = 1;
    im.current_row = 0;

    for (i = 0;  i < im.length;  i++)
    {
        for (j = 0;  j < im.width;  j++)
            image[i*im.width + j] = j*1200/256;
    }
    s = image_translate_init(s, IMAGE_TRANSLATE_FROM_GRAY_8, im.width, im.length, -1, row_read, &im);
    get_flattened_image(s, TRUE);
}
/*- End of function --------------------------------------------------------*/

static void dither_tests_colour16(void)
{
    int i;
    int j;
    image_translate_state_t bw;
    image_translate_state_t *s = &bw;
    uint16_t image[50*50*3];
    image_descriptor_t im;

    printf("Dithering from a 3x16 bit per sample colour to bi-level\n");
    im.image = (const uint8_t *) image;
    im.width = 50;
    im.length = 50;
    im.bytes_per_pixel = 6;
    im.current_row = 0;

    for (i = 0;  i < im.length;  i++)
    {
        for (j = 0;  j < im.width;  j++)
        {
            image[i*3*im.width + 3*j + 0] = j*1200;
            image[i*3*im.width + 3*j + 1] = j*1200;
            image[i*3*im.width + 3*j + 2] = j*1200;
        }
    }
    s = image_translate_init(s, IMAGE_TRANSLATE_FROM_COLOUR_16, im.width, im.length, -1, row_read, &im);
    get_flattened_image(s, TRUE);
}
/*- End of function --------------------------------------------------------*/

static void dither_tests_colour8(void)
{
    int i;
    int j;
    image_translate_state_t bw;
    image_translate_state_t *s = &bw;
    uint8_t image[50*50*3];
    image_descriptor_t im;

    printf("Dithering from a 3x8 bit per sample colour to bi-level\n");
    im.image = image;
    im.width = 50;
    im.length = 50;
    im.bytes_per_pixel = 3;
    im.current_row = 0;

    for (i = 0;  i < im.length;  i++)
    {
        for (j = 0;  j < im.width;  j++)
        {
            image[i*3*im.width + 3*j + 0] = j*1200/256;
            image[i*3*im.width + 3*j + 1] = j*1200/256;
            image[i*3*im.width + 3*j + 2] = j*1200/256;
        }
    }

    s = image_translate_init(s, IMAGE_TRANSLATE_FROM_COLOUR_8, im.width, im.length, -1, row_read, &im);
    get_flattened_image(s, TRUE);
}
/*- End of function --------------------------------------------------------*/

static void grow_tests_colour8(void)
{
    int i;
    int j;
    image_translate_state_t resize;
    image_translate_state_t *s1 = &resize;
    uint8_t image[50*50*3];
    image_descriptor_t im;

    printf("Image growth tests\n");
    im.image = image;
    im.width = 50;
    im.length = 50;
    im.bytes_per_pixel = 3;
    im.current_row = 0;

    for (i = 0;  i < im.length;  i++)
    {
        for (j = 0;  j < im.width;  j++)
        {
            image[i*3*im.width + 3*j + 0] = j*1200/256;
            image[i*3*im.width + 3*j + 1] = j*1200/256;
            image[i*3*im.width + 3*j + 2] = j*1200/256;
        }
    }

    s1 = image_translate_init(s1, IMAGE_TRANSLATE_FROM_COLOUR_8, im.width, im.length, 200, row_read, &im);

    get_flattened_image(s1, FALSE);
}
/*- End of function --------------------------------------------------------*/

static void lenna_tests(int output_width, const char *file)
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
    image_translate_state_t bw;
    image_translate_state_t *s = &bw;
    image_descriptor_t im;

    printf("Dithering Lenna from colour to bi-level test\n");
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
    bits_per_sample = 0;
    TIFFGetField(in_file, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    samples_per_pixel = 0;
    TIFFGetField(in_file, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    printf("Original image is %d x %d, %d bits per sample, %d samples per pixel\n", image_width, image_length, bits_per_sample, samples_per_pixel);
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

    im.image = image;
    im.width = image_width;
    im.length = image_length;
    im.current_row = 0;
    im.bytes_per_pixel = samples_per_pixel;

    s = image_translate_init(s, IMAGE_TRANSLATE_FROM_COLOUR_8, image_width, image_length, output_width, row_read, &im);
    output_width = image_translate_get_output_width(s);
    output_length = image_translate_get_output_length(s);

    if ((out_file = TIFFOpen(file, "w")) == NULL)
        return;
    TIFFSetField(out_file, TIFFTAG_IMAGEWIDTH, output_width);
    TIFFSetField(out_file, TIFFTAG_IMAGELENGTH, output_length);
    TIFFSetField(out_file, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(out_file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out_file, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(out_file, TIFFTAG_ROWSPERSTRIP, -1);
    TIFFSetField(out_file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out_file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(out_file, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);
    TIFFSetField(out_file, TIFFTAG_PAGENUMBER, 0, 1);
    TIFFSetField(out_file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(out_file, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);

    printf("Input %d x %d, output %d x %d\n", image_width, image_length, output_width, output_length);

    if ((image2 = malloc(output_width*output_length/8)) == NULL)
        return;
    memset(image2, 0, output_width*output_length/8);
    n = 0;
    for (i = 0;  i < output_length;  i++)
        n += image_translate_row(s, &image2[n], output_width/8);

    TIFFWriteEncodedStrip(out_file, 0, image2, output_width*output_length/8);
    TIFFWriteDirectory(out_file);
    TIFFClose(out_file);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char **argv)
{
#if 1
    dither_tests_gray16();
    dither_tests_gray8();
    dither_tests_colour16();
    dither_tests_colour8();
#endif
#if 1
    grow_tests_colour8();
#endif
#if 1
    lenna_tests(0, "lenna-bw.tif");
    lenna_tests(1728, "lenna-bw-1728.tif");
    lenna_tests(200, "lenna-bw-200.tif");
#endif
    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
