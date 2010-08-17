/*
 * SpanDSP - a series of DSP components for telephony
 *
 * image_translate.c - Image translation routines for reworking colour
 *                     and gray scale images to be bi-level images of an
 *                     appropriate size to be FAX compatible.
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <memory.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <tiffio.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/logging.h"
#include "spandsp/saturated.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#if defined(SPANDSP_SUPPORT_T85)
#include "spandsp/t81_t82_arith_coding.h"
#include "spandsp/t85.h"
#endif
#include "spandsp/t4_t6_decode.h"
#include "spandsp/t4_t6_encode.h"
#include "spandsp/image_translate.h"

#include "spandsp/private/logging.h"
#if defined(SPANDSP_SUPPORT_T85)
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#endif
#include "spandsp/private/t4_t6_decode.h"
#include "spandsp/private/t4_t6_encode.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"
#include "spandsp/private/image_translate.h"

static int image_colour16_to_gray8_row(uint8_t mono[], uint16_t colour[], int pixels)
{
    int i;
    uint32_t gray;

    for (i = 0;  i < pixels;  i++)
    {
        gray = colour[3*i]*19595 + colour[3*i + 1]*38469 + colour[3*i + 2]*7472;
        mono[i] = saturateu8(gray >> 24);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_colour8_to_gray8_row(uint8_t mono[], uint8_t colour[], int pixels)
{
    int i;
    uint32_t gray;

    for (i = 0;  i < pixels;  i++)
    {
        gray = colour[3*i]*19595 + colour[3*i + 1]*38469 + colour[3*i + 2]*7472;
        mono[i] = saturateu8(gray >> 16);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_gray16_to_gray8_row(uint8_t mono[], uint16_t gray[], int pixels)
{
    int i;

    for (i = 0;  i < pixels;  i++)
        mono[i] = gray[i] >> 8;
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int get_and_scrunch_row(image_translate_state_t *s, uint8_t buf[], size_t len)
{
    int row_len;

    row_len = (*s->row_read_handler)(s->row_read_user_data, buf, s->input_width*s->bytes_per_pixel);
    if (row_len != s->input_width*s->bytes_per_pixel)
        return 0;
    /* Scrunch colour down to gray, and scrunch 16 bit pixels down to 8 bit pixels */
    switch (s->input_format)
    {
    case IMAGE_TRANSLATE_FROM_GRAY_16:
        image_gray16_to_gray8_row(buf, (uint16_t *) buf, s->input_width);
        break;
    case IMAGE_TRANSLATE_FROM_COLOUR_16:
        image_colour16_to_gray8_row(buf, (uint16_t *) buf, s->input_width);
        break;
    case IMAGE_TRANSLATE_FROM_COLOUR_8:
        image_colour8_to_gray8_row(buf, buf, s->input_width);
        break;
    }
    return row_len;
}
/*- End of function --------------------------------------------------------*/

static int image_resize_row(image_translate_state_t *s, uint8_t buf[], size_t len)
{
    int i;
    int output_width;
    int output_length;
    int input_width;
    int input_length;
    double c1;
    double c2;
    double int_part;
    int x;
#if defined(SPANDSP_USE_FIXED_POINT)
    int frac_row;
    int frac_col;
#else
    double frac_row;
    double frac_col;
#endif
    int row_len;
    int skip;
    uint8_t *p;

    if (s->raw_output_row < 0)
        return 0;
    output_width = s->output_width - 1;
    output_length = s->output_length - 1;
    input_width = s->input_width - 1;
    input_length = s->input_length - 1;

    skip = s->raw_output_row*input_length/output_length;
    if (skip >= s->raw_input_row)
    {
        skip++;
        while (skip >= s->raw_input_row)
        {
            if (s->raw_input_row >= s->input_length)
            {
                s->raw_output_row = -1;
                break;
            }
            row_len = get_and_scrunch_row(s, s->raw_pixel_row[0], s->input_width*s->bytes_per_pixel);
            if (row_len != s->input_width*s->bytes_per_pixel)
            {
                s->raw_output_row = -1;
                return 0;
            }
            s->raw_input_row++;
            p = s->raw_pixel_row[0];
            s->raw_pixel_row[0] = s->raw_pixel_row[1];
            s->raw_pixel_row[1] = p;
        }
    }

#if defined(SPANDSP_USE_FIXED_POINT)
    frac_row = s->raw_output_row*input_length/output_length;
    frac_row = s->raw_output_row*input_length - frac_row*output_length;
    for (i = 0;  i < output_width;  i++)
    {
        x = i*input_width/output_width;
        frac_col = x - x*output_width;
        c1 = s->raw_pixel_row[0][x] + (s->raw_pixel_row[0][x + 1] - s->raw_pixel_row[0][x])*frac_col;
        c1 = s->raw_pixel_row[1][x] + (s->raw_pixel_row[1][x + 1] - s->raw_pixel_row[1][x])*frac_col;
        buf[i] = saturateu8(c1 + (c2 - c1)*frac_row);
    }
#else
    frac_row = modf((double) s->raw_output_row*input_length/output_length, &int_part);
    for (i = 0;  i < output_width;  i++)
    {
        frac_col = modf((double) i*input_width/output_width, &int_part);
        x = int_part;
        c1 = s->raw_pixel_row[0][x] + (s->raw_pixel_row[0][x + 1] - s->raw_pixel_row[0][x])*frac_col;
        c2 = s->raw_pixel_row[1][x] + (s->raw_pixel_row[1][x + 1] - s->raw_pixel_row[1][x])*frac_col;
        buf[i] = saturateu8(c1 + (c2 - c1)*frac_row);
    }
#endif
    if (++s->raw_output_row >= s->output_length)
        s->raw_output_row = -1;
    return len;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint8_t find_closest_palette_color(int in)
{
    return (in >= 128)  ?  255  :  0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) image_translate_row(image_translate_state_t *s, uint8_t buf[], size_t len)
{
    int x;
    int y;
    int i;
    int j;
    int limit;
    int old_pixel;
    int new_pixel;
    int quant_error;
    uint8_t *p;
    uint8_t xx;

    if (s->output_row < 0)
        return 0;
    y = s->output_row++;
    /* This algorithm works over two rows, and outputs the earlier of the two. To
       make this work:
           - At row 0 we grab and scrunch two rows.
           - From row 1 up to the last row we grab one new additional row each time.
           - At the last row we dither and output, without getting an extra row in. */
    for (i = (y == 0)  ?  0  :  1;  i < 2;  i++)
    {
        p = s->pixel_row[0];
        s->pixel_row[0] = s->pixel_row[1];
        s->pixel_row[1] = p;

        /* If this is the end of the image just ignore that there is now rubbish in pixel_row[1].
           Mark that the end has occurred. This row will be properly output, and the next one
           will fail, with the end of image condition (i.e. returning zero length) */
        if (s->resize)
        {
            if (image_resize_row(s, s->pixel_row[1], s->output_width*s->bytes_per_pixel) != s->output_width*s->bytes_per_pixel)
                s->output_row = -1;
        }
        else
        {
            if (get_and_scrunch_row(s, s->pixel_row[1], s->output_width*s->bytes_per_pixel) != s->output_width*s->bytes_per_pixel)
                s->output_row = -1;
        }
    }
    /* Apply Floyd-Steinberg dithering to the 8 bit pixels, using a bustrophodontic
       scan, to reduce the grayscale image to pure black and white */
    /* The first and last pixels in each row need special treatment, so we do not
       step outside the row. */
    if ((y & 1))
    {
        x = s->output_width - 1;
        old_pixel = s->pixel_row[0][x];
        new_pixel = find_closest_palette_color(old_pixel);
        quant_error = old_pixel - new_pixel;
        s->pixel_row[0][x + 0] = new_pixel;
        s->pixel_row[0][x - 1] = saturateu8(s->pixel_row[0][x - 1] + (7*quant_error)/16);
        s->pixel_row[1][x + 0] = saturateu8(s->pixel_row[1][x + 0] + (5*quant_error)/16);
        s->pixel_row[1][x - 1] = saturateu8(s->pixel_row[1][x - 1] + (1*quant_error)/16);
        for (  ;  x > 0;  x--)
        {
            old_pixel = s->pixel_row[0][x];
            new_pixel = find_closest_palette_color(old_pixel);
            quant_error = old_pixel - new_pixel;
            s->pixel_row[0][x + 0] = new_pixel;
            s->pixel_row[0][x - 1] = saturateu8(s->pixel_row[0][x - 1] + (7*quant_error)/16);
            s->pixel_row[1][x + 1] = saturateu8(s->pixel_row[1][x + 1] + (3*quant_error)/16);
            s->pixel_row[1][x + 0] = saturateu8(s->pixel_row[1][x + 0] + (5*quant_error)/16);
            s->pixel_row[1][x - 1] = saturateu8(s->pixel_row[1][x - 1] + (1*quant_error)/16);
        }
        old_pixel = s->pixel_row[0][x];
        new_pixel = find_closest_palette_color(old_pixel);
        quant_error = old_pixel - new_pixel;
        s->pixel_row[0][x + 0] = new_pixel;
        s->pixel_row[1][x + 1] = saturateu8(s->pixel_row[1][x + 1] + (3*quant_error)/16);
        s->pixel_row[1][x + 0] = saturateu8(s->pixel_row[1][x + 0] + (5*quant_error)/16);
    }
    else
    {
        x = 0;
        old_pixel = s->pixel_row[0][x];
        new_pixel = find_closest_palette_color(old_pixel);
        quant_error = old_pixel - new_pixel;
        s->pixel_row[0][x + 0] = new_pixel;
        s->pixel_row[0][x + 1] = saturateu8(s->pixel_row[0][x + 1] + (7*quant_error)/16);
        s->pixel_row[1][x + 0] = saturateu8(s->pixel_row[1][x + 0] + (5*quant_error)/16);
        s->pixel_row[1][x + 1] = saturateu8(s->pixel_row[1][x + 1] + (1*quant_error)/16);
        for (  ;  x < s->output_width - 1;  x++)
        {
            old_pixel = s->pixel_row[0][x];
            new_pixel = find_closest_palette_color(old_pixel);
            quant_error = old_pixel - new_pixel;
            s->pixel_row[0][x + 0] = new_pixel;
            s->pixel_row[0][x + 1] = saturateu8(s->pixel_row[0][x + 1] + (7*quant_error)/16);
            s->pixel_row[1][x - 1] = saturateu8(s->pixel_row[1][x - 1] + (3*quant_error)/16);
            s->pixel_row[1][x + 0] = saturateu8(s->pixel_row[1][x + 0] + (5*quant_error)/16);
            s->pixel_row[1][x + 1] = saturateu8(s->pixel_row[1][x + 1] + (1*quant_error)/16);
        }
        old_pixel = s->pixel_row[0][x];
        new_pixel = find_closest_palette_color(old_pixel);
        quant_error = old_pixel - new_pixel;
        s->pixel_row[0][x + 0] = new_pixel;
        s->pixel_row[1][x - 1] = saturateu8(s->pixel_row[1][x - 1] + (3*quant_error)/16);
        s->pixel_row[1][x + 0] = saturateu8(s->pixel_row[1][x + 0] + (5*quant_error)/16);
    }
    /* Now bit pack the pixel per byte row into a pixel per bit row. */
    for (i = 0, x = 0;  x < s->output_width;  i++, x += 8)
    {
        xx = 0;
        /* Allow for the possibility that the width is not a multiple of 8 */
        limit = (8 <= s->output_width - x)  ?  8  :  (s->output_width - x);
        for (j = 0;  j < limit;  j++)
        {
            if (s->pixel_row[0][x + j] <= 128)
                xx |= (1 << (7 - j));
        }
        buf[i] = xx;
    }
    return i;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) image_translate_get_output_width(image_translate_state_t *s)
{
    return s->output_width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) image_translate_get_output_length(image_translate_state_t *s)
{
    return s->output_length;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(image_translate_state_t *) image_translate_init(image_translate_state_t *s,
                                                             int input_format,
                                                             int input_width,
                                                             int input_length,
                                                             int output_width,
                                                             t4_row_read_handler_t row_read_handler,
                                                             void *row_read_user_data)
{
    int i;

    if (s == NULL)
    {
        if ((s = (image_translate_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->input_format = input_format;

    s->input_width = input_width;
    s->input_length = input_length;

    s->resize = (output_width > 0);
    s->output_width = (s->resize)  ?  output_width  :  s->input_width;
    s->output_length = (s->resize)  ?  s->input_length*s->output_width/s->input_width  :  s->input_length;

    switch (s->input_format)
    {
    case IMAGE_TRANSLATE_FROM_GRAY_8:
        s->bytes_per_pixel = 1;
        break;
    case IMAGE_TRANSLATE_FROM_GRAY_16:
        s->bytes_per_pixel = 2;
        break;
    case IMAGE_TRANSLATE_FROM_COLOUR_8:
        s->bytes_per_pixel = 3;
        break;
    case IMAGE_TRANSLATE_FROM_COLOUR_16:
        s->bytes_per_pixel = 6;
        break;
    default:
        s->bytes_per_pixel = 1;
        break;
    }

    /* Allocate the two row buffers we need, using the space requirements we now have */
    if (s->resize)
    {
        for (i = 0;  i < 2;  i++)
        {
            if ((s->raw_pixel_row[i] = (uint8_t *) malloc(s->input_width*s->bytes_per_pixel)) == NULL)
                return NULL;
            memset(s->raw_pixel_row[i], 0, s->input_width*s->bytes_per_pixel);
            if ((s->pixel_row[i] = (uint8_t *) malloc(s->output_width*sizeof(uint8_t))) == NULL)
                return NULL;
            memset(s->pixel_row[i], 0, s->output_width*sizeof(uint8_t));
        }
    }
    else
    {
        for (i = 0;  i < 2;  i++)
        {
            if ((s->pixel_row[i] = (uint8_t *) malloc(s->output_width*s->bytes_per_pixel)) == NULL)
                return NULL;
            memset(s->pixel_row[i], 0, s->output_width*s->bytes_per_pixel);
        }
    }

    s->row_read_handler = row_read_handler;
    s->row_read_user_data = row_read_user_data;

    s->raw_input_row = 0;
    s->raw_output_row = 0;
    s->output_row = 0;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) image_translate_release(image_translate_state_t *s)
{
    int i;

    for (i = 0;  i < 2;  i++)
    {
        if (s->raw_pixel_row[i])
        {
            free(s->raw_pixel_row[i]);
            s->raw_pixel_row[i] = NULL;
        }
        if (s->pixel_row[i])
        {
            free(s->pixel_row[i]);
            s->pixel_row[i] = NULL;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) image_translate_free(image_translate_state_t *s)
{
    int res;

    res = image_translate_release(s);
    free(s);
    return res;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
