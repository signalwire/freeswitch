/*
 * SpanDSP - a series of DSP components for telephony
 *
 * image_translate.c - Image translation routines for reworking colour
 *                     and gray scale images to be colour, gray scale or
 *                     bi-level images of an appropriate size to be FAX
 *                     compatible.
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
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"
#include <tiffio.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/fast_convert.h"
#include "spandsp/logging.h"
#include "spandsp/saturated.h"
#include "spandsp/timezone.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/t81_t82_arith_coding.h"
#include "spandsp/t85.h"
#include "spandsp/t42.h"
#if defined(SPANDSP_SUPPORT_T43)
#include "spandsp/t43.h"
#endif
#include "spandsp/t4_t6_decode.h"
#include "spandsp/t4_t6_encode.h"
#include "spandsp/image_translate.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#include "spandsp/private/t42.h"
#if defined(SPANDSP_SUPPORT_T43)
#include "spandsp/private/t43.h"
#endif
#include "spandsp/private/t4_t6_decode.h"
#include "spandsp/private/t4_t6_encode.h"
#include "spandsp/private/image_translate.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"

static int image_colour16_to_colour8_row(uint8_t colour8[], uint16_t colour16[], int pixels)
{
    int i;

    for (i = 0;  i < 3*pixels;  i++)
        colour8[i] = colour16[i] >> 8;
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_colour16_to_gray16_row(uint16_t gray16[], uint16_t colour16[], int pixels)
{
    int i;
    uint32_t gray;

    for (i = 0;  i < pixels;  i++)
    {
        gray = colour16[3*i]*19595 + colour16[3*i + 1]*38469 + colour16[3*i + 2]*7472;
        gray16[i] = saturateu16(gray >> 16);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_colour16_to_gray8_row(uint8_t gray8[], uint16_t colour16[], int pixels)
{
    int i;
    uint32_t gray;

    for (i = 0;  i < pixels;  i++)
    {
        gray = colour16[3*i]*19595 + colour16[3*i + 1]*38469 + colour16[3*i + 2]*7472;
        gray8[i] = saturateu8(gray >> 24);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_colour8_to_gray16_row(uint16_t gray16[], uint8_t colour8[], int pixels)
{
    int i;
    uint32_t gray;

    for (i = 0;  i < pixels;  i++)
    {
        gray = colour8[3*i]*19595 + colour8[3*i + 1]*38469 + colour8[3*i + 2]*7472;
        gray16[i] = saturateu16(gray >> 8);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_colour8_to_gray8_row(uint8_t gray8[], uint8_t colour8[], int pixels)
{
    int i;
    uint32_t gray;

    for (i = 0;  i < pixels;  i++)
    {
        gray = colour8[3*i]*19595 + colour8[3*i + 1]*38469 + colour8[3*i + 2]*7472;
        gray8[i] = saturateu8(gray >> 16);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_colour8_to_colour16_row(uint16_t colour16[], uint8_t colour8[], int pixels)
{
    int i;

    for (i = 3*pixels - 1;  i >= 0;  i--)
        colour16[i] = colour8[i] << 8;
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_gray16_to_colour16_row(uint16_t colour16[], uint16_t gray16[], int pixels)
{
    int i;

    for (i = pixels - 1;  i >= 0;  i--)
    {
        colour16[3*i] = saturateu16((gray16[i]*36532U) >> 15);
        colour16[3*i + 1] = saturateu16((gray16[i]*37216U) >> 16);
        colour16[3*i + 2] = saturateu16((gray16[i]*47900U) >> 14);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_gray16_to_colour8_row(uint8_t colour8[], uint16_t gray16[], int pixels)
{
    int i;

    for (i = pixels - 1;  i >= 0;  i--)
    {
        colour8[3*i] = saturateu8((gray16[i]*36532U) >> 23);
        colour8[3*i + 1] = saturateu8((gray16[i]*37216U) >> 24);
        colour8[3*i + 2] = saturateu8((gray16[i]*47900U) >> 22);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_gray16_to_gray8_row(uint8_t gray8[], uint16_t gray16[], int pixels)
{
    int i;

    for (i = 0;  i < pixels;  i++)
        gray8[i] = gray16[i] >> 8;
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_gray8_to_colour16_row(uint16_t colour16[], uint8_t gray8[], int pixels)
{
    int i;

    for (i = pixels - 1;  i >= 0;  i--)
    {
        colour16[3*i] = saturateu16((gray8[i]*36532U) >> 7);
        colour16[3*i + 1] = saturateu16((gray8[i]*37216U) >> 8);
        colour16[3*i + 2] = saturateu16((gray8[i]*47900U) >> 6);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_gray8_to_colour8_row(uint8_t colour8[], uint8_t gray8[], int pixels)
{
    int i;

    for (i = pixels - 1;  i >= 0;  i--)
    {
        colour8[3*i] = saturateu8((gray8[i]*36532U) >> 15);
        colour8[3*i + 1] = saturateu8((gray8[i]*37216U) >> 16);
        colour8[3*i + 2] = saturateu8((gray8[i]*47900U) >> 14);
    }
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int image_gray8_to_gray16_row(uint16_t gray16[], uint8_t gray8[], int pixels)
{
    int i;

    for (i = pixels - 1;  i >= 0;  i--)
        gray16[i] = gray8[i] << 8;
    return pixels;
}
/*- End of function --------------------------------------------------------*/

static int get_and_scrunch_row(image_translate_state_t *s, uint8_t buf[])
{
    int input_row_len;

    input_row_len = (*s->row_read_handler)(s->row_read_user_data, buf, s->input_width*s->input_bytes_per_pixel);
    if (input_row_len != s->input_width*s->input_bytes_per_pixel)
        return 0;
    /* Scrunch colour down to gray, vice versa. Scrunch 16 bit pixels down to 8 bit pixels, or vice versa. */
    switch (s->input_format)
    {
    case T4_IMAGE_TYPE_GRAY_12BIT:
        switch (s->output_format)
        {
        case T4_IMAGE_TYPE_BILEVEL:
        case T4_IMAGE_TYPE_GRAY_8BIT:
            image_gray16_to_gray8_row(buf, (uint16_t *) buf, s->input_width);
            break;
        case T4_IMAGE_TYPE_COLOUR_12BIT:
            image_gray16_to_colour16_row((uint16_t *) buf, (uint16_t *) buf, s->input_width);
            break;
        case T4_IMAGE_TYPE_COLOUR_BILEVEL:
        case T4_IMAGE_TYPE_COLOUR_8BIT:
            image_gray16_to_colour8_row(buf, (uint16_t *) buf, s->input_width);
            break;
        }
        break;
    case T4_IMAGE_TYPE_GRAY_8BIT:
        switch (s->output_format)
        {
        case T4_IMAGE_TYPE_GRAY_12BIT:
            image_gray8_to_gray16_row((uint16_t *) buf, buf, s->input_width);
            break;
        case T4_IMAGE_TYPE_COLOUR_12BIT:
            image_gray8_to_colour16_row((uint16_t *) buf, buf, s->input_width);
            break;
        case T4_IMAGE_TYPE_COLOUR_BILEVEL:
        case T4_IMAGE_TYPE_COLOUR_8BIT:
            image_gray8_to_colour8_row(buf, buf, s->input_width);
            break;
        }
        break;
    case T4_IMAGE_TYPE_COLOUR_12BIT:
        switch (s->output_format)
        {
        case T4_IMAGE_TYPE_GRAY_12BIT:
            image_colour16_to_gray16_row((uint16_t *) buf, (uint16_t *) buf, s->input_width);
            break;
        case T4_IMAGE_TYPE_BILEVEL:
        case T4_IMAGE_TYPE_GRAY_8BIT:
            image_colour16_to_gray8_row(buf, (uint16_t *) buf, s->input_width);
            break;
        case T4_IMAGE_TYPE_COLOUR_BILEVEL:
        case T4_IMAGE_TYPE_COLOUR_8BIT:
            image_colour16_to_colour8_row(buf, (uint16_t *) buf, s->input_width);
            break;
        }
        break;
    case T4_IMAGE_TYPE_COLOUR_BILEVEL:
    case T4_IMAGE_TYPE_COLOUR_8BIT:
        switch (s->output_format)
        {
        case T4_IMAGE_TYPE_GRAY_12BIT:
            image_colour8_to_gray16_row((uint16_t *) buf, buf, s->input_width);
            break;
        case T4_IMAGE_TYPE_BILEVEL:
        case T4_IMAGE_TYPE_GRAY_8BIT:
            image_colour8_to_gray8_row(buf, buf, s->input_width);
            break;
        case T4_IMAGE_TYPE_COLOUR_12BIT:
            image_colour8_to_colour16_row((uint16_t *) buf, buf, s->input_width);
            break;
        }
        break;
    }
    return s->output_width;
}
/*- End of function --------------------------------------------------------*/

static int image_resize_row(image_translate_state_t *s, uint8_t buf[])
{
    int i;
    int j;
    int output_width;
    int output_length;
    int input_width;
    int input_length;
    int x;
#if defined(SPANDSP_USE_FIXED_POINT)
    int c1;
    int c2;
    int frac_row;
    int frac_col;
#else
    double c1;
    double c2;
    double int_part;
    double frac_row;
    double frac_col;
#endif
    uint8_t *row8[2];
    uint16_t *row16[2];
    uint16_t *buf16;
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
            row_len = get_and_scrunch_row(s, s->raw_pixel_row[0]);
            if (row_len != s->output_width)
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
    frac_row = ((s->raw_output_row*256*input_length)/output_length) & 0xFF;
#else
    frac_row = modf((double) s->raw_output_row*input_length/output_length, &int_part);
#endif

    switch (s->output_format)
    {
    case T4_IMAGE_TYPE_COLOUR_BILEVEL:
    case T4_IMAGE_TYPE_COLOUR_8BIT:
        row8[0] = s->raw_pixel_row[0];
        row8[1] = s->raw_pixel_row[1];
        for (i = 0;  i < output_width;  i++)
        {
#if defined(SPANDSP_USE_FIXED_POINT)
            x = i*256*input_width/output_width;
            frac_col = x & 0xFF;
            x >>= 8;
            x = 3*x;
            for (j = 0;  j < 3;  j++)
            {
                c1 = row8[0][x + j] + (((row8[0][x + j + 3] - row8[0][x + j])*frac_col) >> 8);
                c2 = row8[1][x + j] + (((row8[1][x + j + 3] - row8[1][x + j])*frac_col) >> 8);
                buf[3*i + j] = saturateu8(c1 + (((c2 - c1)*frac_row) >> 8));
            }
#else
            frac_col = modf((double) i*input_width/output_width, &int_part);
            x = 3*int_part;
            for (j = 0;  j < 3;  j++)
            {
                c1 = row8[0][x + j] + (row8[0][x + j + 3] - row8[0][x + j])*frac_col;
                c2 = row8[1][x + j] + (row8[1][x + j + 3] - row8[1][x + j])*frac_col;
                buf[3*i + j] = saturateu8(c1 + (c2 - c1)*frac_row);
            }
#endif
        }
        break;
    case T4_IMAGE_TYPE_COLOUR_12BIT:
        row16[0] = (uint16_t *) s->raw_pixel_row[0];
        row16[1] = (uint16_t *) s->raw_pixel_row[1];
        buf16 = (uint16_t *) buf;
        for (i = 0;  i < output_width;  i++)
        {
#if defined(SPANDSP_USE_FIXED_POINT)
            x = i*256*input_width/output_width;
            frac_col = x & 0xFF;
            x >>= 8;
            x = 3*x;
            for (j = 0;  j < 3;  j++)
            {
                c1 = row16[0][x + j] + (((row16[0][x + j + 3] - row16[0][x + j])*frac_col) >> 8);
                c2 = row16[1][x + j] + (((row16[1][x + j + 3] - row16[1][x + j])*frac_col) >> 8);
                buf16[3*i + j] = saturateu16(c1 + (((c2 - c1)*frac_row) >> 8));
            }
#else
            frac_col = modf((double) i*input_width/output_width, &int_part);
            x = 3*int_part;
            for (j = 0;  j < 3;  j++)
            {
                c1 = row16[0][x + j] + (row16[0][x + j + 3] - row16[0][x + j])*frac_col;
                c2 = row16[1][x + j] + (row16[1][x + j + 3] - row16[1][x + j])*frac_col;
                buf16[3*i + j] = saturateu16(c1 + (c2 - c1)*frac_row);
            }
#endif
        }
        break;
    case T4_IMAGE_TYPE_BILEVEL:
    case T4_IMAGE_TYPE_GRAY_8BIT:
        row8[0] = s->raw_pixel_row[0];
        row8[1] = s->raw_pixel_row[1];
        for (i = 0;  i < output_width;  i++)
        {
#if defined(SPANDSP_USE_FIXED_POINT)
            x = i*256*input_width/output_width;
            frac_col = x & 0xFF;
            x >>= 8;
            c1 = row8[0][x] + (((row8[0][x + 1] - row8[0][x])*frac_col) >> 8);
            c2 = row8[1][x] + (((row8[1][x + 1] - row8[1][x])*frac_col) >> 8);
            buf[i] = saturateu8(c1 + (((c2 - c1)*frac_row) >> 8));
#else
            frac_col = modf((double) i*input_width/output_width, &int_part);
            x = int_part;
            c1 = row8[0][x] + (row8[0][x + 1] - row8[0][x])*frac_col;
            c2 = row8[1][x] + (row8[1][x + 1] - row8[1][x])*frac_col;
            buf[i] = saturateu8(c1 + (c2 - c1)*frac_row);
#endif
        }
        break;
    case T4_IMAGE_TYPE_GRAY_12BIT:
        row16[0] = (uint16_t *) s->raw_pixel_row[0];
        row16[1] = (uint16_t *) s->raw_pixel_row[1];
        buf16 = (uint16_t *) buf;
        for (i = 0;  i < output_width;  i++)
        {
#if defined(SPANDSP_USE_FIXED_POINT)
            x = i*256*input_width/output_width;
            frac_col = x & 0xFF;
            x >>= 8;
            c1 = row16[0][x] + (((row16[0][x + 1] - row16[0][x])*frac_col) >> 8);
            c2 = row16[1][x] + (((row16[1][x + 1] - row16[1][x])*frac_col) >> 8);
            buf[i] = saturateu8(c1 + (((c2 - c1)*frac_row) >> 8));
#else
            frac_col = modf((double) i*input_width/output_width, &int_part);
            x = int_part;
            c1 = row16[0][x] + (row16[0][x + 1] - row16[0][x])*frac_col;
            c2 = row16[1][x] + (row16[1][x + 1] - row16[1][x])*frac_col;
            buf[i] = saturateu8(c1 + (c2 - c1)*frac_row);
#endif
        }
        break;
    }
    if (++s->raw_output_row >= s->output_length)
        s->raw_output_row = -1;
    return s->output_width;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint8_t find_closest_palette_color(int in)
{
    return (in >= 128)  ?  255  :  0;
}
/*- End of function --------------------------------------------------------*/

static int floyd_steinberg_dither_row(image_translate_state_t *s, uint8_t buf[])
{
    int x;
    int y;
    int i;
    int j;
    int limit;
    int old_pixel;
    int new_pixel;
    int quant_error;
    uint8_t xx;
    uint8_t *p;

    y = s->output_row++;
    /* This algorithm works over two rows, and outputs the earlier of the two. To
       make this work:
           - At row 0 we grab and scrunch two rows.
           - From row 1 up to the last row we grab one new additional row each time.
           - At the last row we dither and output, without getting an extra row in. */
    for (i = (y == 0)  ?  0  :  1;  i < 2;  i++)
    {
        /* Swap the row buffers */
        p = s->pixel_row[0];
        s->pixel_row[0] = s->pixel_row[1];
        s->pixel_row[1] = p;

        /* If this is the end of the image just ignore that there is now rubbish in pixel_row[1].
           Mark that the end has occurred. This row will be properly output, and the next one
           will fail, with the end of image condition (i.e. returning zero length) */
        if (s->resize)
        {
            if (image_resize_row(s, s->pixel_row[1]) != s->output_width)
                s->output_row = -1;
        }
        else
        {
            if (get_and_scrunch_row(s, s->pixel_row[1]) != s->output_width)
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

SPAN_DECLARE(int) image_translate_row(image_translate_state_t *s, uint8_t buf[], size_t len)
{
    int i;

    if (s->output_row < 0)
        return 0;
    switch (s->output_format)
    {
    case T4_IMAGE_TYPE_BILEVEL:
    case T4_IMAGE_TYPE_COLOUR_BILEVEL:
    case T4_IMAGE_TYPE_4COLOUR_BILEVEL:
        i = floyd_steinberg_dither_row(s, buf);
        break;
    default:
        s->output_row++;
        if (s->resize)
        {
            if (image_resize_row(s, buf) != s->output_width)
                s->output_row = -1;
        }
        else
        {
            if (get_and_scrunch_row(s, buf) != s->output_width)
                s->output_row = -1;
        }
        if (s->output_row < 0)
            return 0;
        i = s->output_width*s->output_bytes_per_pixel;
        break;
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

SPAN_DECLARE(int) image_translate_set_row_read_handler(image_translate_state_t *s, t4_row_read_handler_t row_read_handler, void *row_read_user_data)
{
    s->row_read_handler = row_read_handler;
    s->row_read_user_data = row_read_user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int image_format_to_bytes_per_pixel(int image_format)
{
    switch (image_format)
    {
    default:
    case T4_IMAGE_TYPE_BILEVEL:
    case T4_IMAGE_TYPE_GRAY_8BIT:
        return 1;
    case T4_IMAGE_TYPE_GRAY_12BIT:
        return 2;
    case T4_IMAGE_TYPE_COLOUR_BILEVEL:
    case T4_IMAGE_TYPE_COLOUR_8BIT:
        return 3;
    case T4_IMAGE_TYPE_4COLOUR_BILEVEL:
    case T4_IMAGE_TYPE_4COLOUR_8BIT:
        return 4;
    case T4_IMAGE_TYPE_COLOUR_12BIT:
        return 6;
    case T4_IMAGE_TYPE_4COLOUR_12BIT:
        return 8;
    }
    return 1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) image_translate_restart(image_translate_state_t *s, int input_length)
{
    int i;
    int raw_row_size;
    int row_size;

    s->input_length = input_length;
    if (s->resize)
        s->output_length = (s->input_length*s->output_width)/s->input_width;
    else
        s->output_length = s->input_length;

    /* Allocate the two row buffers we need, using the space requirements we now have */
    raw_row_size = s->input_width*s->input_bytes_per_pixel;
    row_size = s->output_width*s->output_bytes_per_pixel;
    if (raw_row_size < row_size)
        raw_row_size = row_size;
    if (s->resize)
    {
        for (i = 0;  i < 2;  i++)
        {
            if (s->raw_pixel_row[i] == NULL)
            {
                if ((s->raw_pixel_row[i] = (uint8_t *) span_alloc(raw_row_size)) == NULL)
                    return -1;
            }
            memset(s->raw_pixel_row[i], 0, raw_row_size);
        }
    }
    switch (s->output_format)
    {
    case T4_IMAGE_TYPE_BILEVEL:
    case T4_IMAGE_TYPE_COLOUR_BILEVEL:
    case T4_IMAGE_TYPE_4COLOUR_BILEVEL:
        if (s->resize)
            raw_row_size = row_size;
        for (i = 0;  i < 2;  i++)
        {
            if (s->pixel_row[i] == NULL)
            {
                if ((s->pixel_row[i] = (uint8_t *) span_alloc(raw_row_size)) == NULL)
                    return -1;
            }
            memset(s->pixel_row[i], 0, raw_row_size);
        }
        break;
    }

    s->raw_input_row = 0;
    s->raw_output_row = 0;
    s->output_row = 0;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(image_translate_state_t *) image_translate_init(image_translate_state_t *s,
                                                             int output_format,
                                                             int output_width,
                                                             int output_length,
                                                             int input_format,
                                                             int input_width,
                                                             int input_length,
                                                             t4_row_read_handler_t row_read_handler,
                                                             void *row_read_user_data)
{
    if (s == NULL)
    {
        if ((s = (image_translate_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->row_read_handler = row_read_handler;
    s->row_read_user_data = row_read_user_data;

    s->input_format = input_format;
    s->input_width = input_width;
    s->input_length = input_length;
    s->input_bytes_per_pixel = image_format_to_bytes_per_pixel(s->input_format);

    s->output_format = output_format;
    s->output_bytes_per_pixel = image_format_to_bytes_per_pixel(s->output_format);

    s->resize = (output_width > 0);
    if (s->resize)
        s->output_width = output_width;
    else
        s->output_width = s->input_width;

    if (image_translate_restart(s, input_length))
        return NULL;

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
            span_free(s->raw_pixel_row[i]);
            s->raw_pixel_row[i] = NULL;
        }
        if (s->pixel_row[i])
        {
            span_free(s->pixel_row[i]);
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
    span_free(s);
    return res;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
