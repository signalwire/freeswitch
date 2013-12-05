/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t43.c - ITU T.43 JBIG for grey and colour FAX image processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011, 2013 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>
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
#include <time.h>
#include "floating_fudge.h"
#include <setjmp.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/timezone.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/t81_t82_arith_coding.h"
#include "spandsp/t85.h"
#include "spandsp/t42.h"
#include "spandsp/t43.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#include "spandsp/private/t42.h"
#include "spandsp/private/t43.h"

#include "t43_gray_code_tables.h"
#include "t42_t43_local.h"

SPAN_DECLARE(const char *) t43_image_type_to_str(int type)
{
    switch (type)
    {
    case T43_IMAGE_TYPE_RGB_BILEVEL:
        return "1 bit/colour image (RGB primaries)";
    case T43_IMAGE_TYPE_CMY_BILEVEL:
        return "1 bit/colour image (CMY primaries)";
    case T43_IMAGE_TYPE_CMYK_BILEVEL:
        return "1 bit/colour image (CMYK primaries)";
    case T43_IMAGE_TYPE_8BIT_COLOUR_PALETTE:
        return "Palettized colour image (CIELAB 8 bits/component precision table)";
    case T43_IMAGE_TYPE_12BIT_COLOUR_PALETTE:
        return "Palettized colour image (CIELAB 12 bits/component precision table)";
    case T43_IMAGE_TYPE_GRAY:
        return "Gray-scale image (using L*)";
    case T43_IMAGE_TYPE_COLOUR:
        return "Continuous-tone colour image (CIELAB)";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint16_t pack_16(const uint8_t *s)
{
    uint16_t value;

    value = ((uint16_t) s[0] << 8) | (uint16_t) s[1];
    return value;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint32_t pack_32(const uint8_t *s)
{
    uint32_t value;

    value = ((uint32_t) s[0] << 24) | ((uint32_t) s[1] << 16) | ((uint32_t) s[2] << 8) | (uint32_t) s[3];
    return value;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int unpack_16(uint8_t *s, uint16_t value)
{
    s[0] = (value >> 8) & 0xFF;
    s[1] = value & 0xFF;
    return sizeof(uint16_t);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int unpack_32(uint8_t *s, uint32_t value)
{
    s[0] = (value >> 24) & 0xFF;
    s[1] = (value >> 16) & 0xFF;
    s[2] = (value >> 8) & 0xFF;
    s[3] = value & 0xFF;
    return sizeof(uint16_t);
}
/*- End of function --------------------------------------------------------*/

static int t43_create_header(t43_encode_state_t *s, uint8_t data[], size_t len)
{
    int pos;
    int val[6];
#if 0
    int bytes_per_entry;
#endif

    pos = 0;
    unpack_16(data, 0xFFA8);
    pos += 2;

    span_log(&s->logging, SPAN_LOG_FLOW, "Putting G3FAX0\n");
    unpack_16(&data[pos], 0xFFE1);
    pos += 2;
    unpack_16(&data[pos], 2 + 6 + 10);
    pos += 2;
    memcpy(&data[pos], "G3FAX\0", 6);
    pos += 6;
    unpack_16(&data[pos], 1997);
    pos += 2;
    unpack_16(&data[pos], s->spatial_resolution);
    pos += 2;
    /* JBIG coding method (0) is the only possible value here */
    data[pos] = 0;
    pos += 1;
    data[pos] = s->image_type;
    pos += 1;
    data[pos] = s->bit_planes[0];
    pos += 1;
    data[pos] = s->bit_planes[1];
    pos += 1;
    data[pos] = s->bit_planes[2];
    pos += 1;
    data[pos] = s->bit_planes[3];
    pos += 1;

    if (s->lab.offset_L != 0
        ||
        s->lab.range_L != 100
        ||
        s->lab.offset_a != 128
        ||
        s->lab.range_a != 170
        ||
        s->lab.offset_b != 96
        ||
        s->lab.range_b != 200)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Putting G3FAX1\n");
        unpack_16(&data[pos], 0xFFE1);
        pos += 2;
        unpack_16(&data[pos], 2 + 6 + 12);
        pos += 2;
        memcpy(&data[pos], "G3FAX\1", 6);
        pos += 6;
        get_lab_gamut2(&s->lab, &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
        unpack_16(&data[pos + 0], val[0]);
        unpack_16(&data[pos + 2], val[1]);
        unpack_16(&data[pos + 4], val[2]);
        unpack_16(&data[pos + 6], val[3]);
        unpack_16(&data[pos + 8], val[4]);
        unpack_16(&data[pos + 10], val[5]);
        pos += 12;
    }

    if (memcmp(s->illuminant_code, "\0\0\0\0", 4) != 0
        ||
        s->illuminant_colour_temperature > 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Putting G3FAX2\n");
        unpack_16(&data[pos], 0xFFE1);
        pos += 2;
        unpack_16(&data[pos], 2 + 6 + 4);
        pos += 2;
        memcpy(&data[pos], "G3FAX\2", 6);
        pos += 6;
        if (memcmp(s->illuminant_code, "\0\0\0\0", 4) != 0)
        {
            memcpy(&data[pos], s->illuminant_code, 4);
        }
        else
        {
            memcpy(&data[pos], "CT", 2);
            unpack_16(&data[pos + 2], s->illuminant_colour_temperature);
        }
        pos += 4;
    }

#if 0
    if (s->colour_map)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Putting G3FAX3\n");
        bytes_per_entry = (table_id == 0)  ?  1  :  2;
        unpack_16(&data[pos], 0xFFE3);
        pos += 2;
        unpack_32(&data[pos], 2 + 6 + 2 + 4 + 3*s->colour_map_entries*bytes_per_entry);
        pos += 4;
        memcpy(&data[pos], "G3FAX\3", 6);
        pos += 6;
        unpack_16(&data[pos], table_id);
        pos += 2;
        unpack_32(&data[pos], s->colour_map_entries);
        pos += 4;
        srgb_to_lab(&s->lab, &data[pos], s->colour_map, s->colour_map_entries);
        pos += 3*s->colour_map_entries*bytes_per_entry;
    }
#endif

    span_log(&s->logging, SPAN_LOG_FLOW, "Putting G3FAX-FF\n");
    unpack_16(&data[pos], 0xFFE1);
    pos += 2;
    unpack_16(&data[pos], 2 + 6);
    pos += 2;
    memcpy(&data[pos], "G3FAX\xFF", 6);
    pos += 6;
    return pos;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t43_encode_set_options(t43_encode_state_t *s,
                                          uint32_t l0,
                                          int mx,
                                          int options)
{
    t85_encode_set_options(&s->t85, l0, mx, options);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_set_image_width(t43_encode_state_t *s, uint32_t image_width)
{
    return t85_encode_set_image_width(&s->t85, image_width);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_set_image_length(t43_encode_state_t *s, uint32_t image_length)
{
    return t85_encode_set_image_length(&s->t85, image_length);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_set_image_type(t43_encode_state_t *s, int image_type)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t43_encode_abort(t43_encode_state_t *s)
{
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t43_encode_comment(t43_encode_state_t *s, const uint8_t comment[], size_t len)
{
    t85_encode_comment(&s->t85, comment, len);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_image_complete(t43_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_get(t43_encode_state_t *s, uint8_t buf[], size_t max_len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t43_encode_get_image_width(t43_encode_state_t *s)
{
    return t85_encode_get_image_width(&s->t85);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t43_encode_get_image_length(t43_encode_state_t *s)
{
    return t85_encode_get_image_length(&s->t85);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_get_compressed_image_size(t43_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_set_row_read_handler(t43_encode_state_t *s,
                                                  t4_row_read_handler_t handler,
                                                  void *user_data)
{
    s->row_read_handler = handler;
    s->row_read_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t43_encode_get_logging_state(t43_encode_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_restart(t43_encode_state_t *s, uint32_t image_width, uint32_t image_length)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t43_encode_state_t *) t43_encode_init(t43_encode_state_t *s,
                                                   uint32_t image_width,
                                                   uint32_t image_length,
                                                   t4_row_read_handler_t handler,
                                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (t43_encode_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.43");

    s->row_read_handler = handler;
    s->row_read_user_data = user_data;

    t85_encode_init(&s->t85,
                    image_width,
                    image_length,
                    handler,
                    user_data);

    s->image_type = T43_IMAGE_TYPE_8BIT_COLOUR_PALETTE;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_release(t43_encode_state_t *s)
{
    t85_encode_release(&s->t85);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_encode_free(t43_encode_state_t *s)
{
    int ret;

    t85_encode_free(&s->t85);
    ret = t43_encode_release(s);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t43_decode_rx_status(t43_decode_state_t *s, int status)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Signal status is %s (%d)\n", signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_TRAINING_IN_PROGRESS:
    case SIG_STATUS_TRAINING_FAILED:
    case SIG_STATUS_TRAINING_SUCCEEDED:
    case SIG_STATUS_CARRIER_UP:
        /* Ignore these */
        break;
    case SIG_STATUS_CARRIER_DOWN:
    case SIG_STATUS_END_OF_DATA:
        /* Finalise the image */
        t85_decode_put(&s->t85, NULL, 0);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected rx status - %d!\n", status);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void set_simple_colour_map(t43_decode_state_t *s, int code)
{
    int i;

    switch (code)
    {
    case T43_IMAGE_TYPE_RGB_BILEVEL:
        /* Table 3/T.43 1 bit/colour image (using RGB primaries) */
        memset(s->colour_map, 0, sizeof(s->colour_map));
        /* Black */
        /* Blue */
        s->colour_map[3*0x20 + 2] = 0xF0;
        /* Green */
        s->colour_map[3*0x40 + 1] = 0xF0;
        /* Green + Blue */
        s->colour_map[3*0x60 + 1] = 0xF0;
        s->colour_map[3*0x60 + 2] = 0xF0;
        /* Red */
        s->colour_map[3*0x80 + 0] = 0xF0;
        /* Red + Blue */
        s->colour_map[3*0xA0 + 0] = 0xF0;
        s->colour_map[3*0xA0 + 2] = 0xF0;
        /* Red + Green */
        s->colour_map[3*0xC0 + 0] = 0xF0;
        s->colour_map[3*0xC0 + 1] = 0xF0;
        /* White */
        s->colour_map[3*0xE0 + 0] = 0xF0;
        s->colour_map[3*0xE0 + 1] = 0xF0;
        s->colour_map[3*0xE0 + 2] = 0xF0;
        s->colour_map_entries = 256;
        break;
    case T43_IMAGE_TYPE_CMY_BILEVEL:
        /* Table 2/T.43 1 bit/colour image (using CMY primaries) */
        memset(s->colour_map, 0, sizeof(s->colour_map));
        /* White */
        s->colour_map[3*0x00 + 0] = 0xF0;
        s->colour_map[3*0x00 + 1] = 0xF0;
        s->colour_map[3*0x00 + 2] = 0xF0;
        /* Yellow */
        s->colour_map[3*0x20 + 0] = 0xF0;
        s->colour_map[3*0x20 + 1] = 0xF0;
        /* Magenta */
        s->colour_map[3*0x40 + 0] = 0xF0;
        s->colour_map[3*0x40 + 2] = 0xF0;
        /* Magenta + Yellow */
        s->colour_map[3*0x60 + 0] = 0xF0;
        /* Cyan */
        s->colour_map[3*0x80 + 1] = 0xF0;
        /* Cyan + Yellow */
        s->colour_map[3*0xA0 + 1] = 0xF0;
        /* Cyan + Magenta */
        s->colour_map[3*0xC0 + 2] = 0xF0;
        /* Black */
        s->colour_map_entries = 256;
        break;
    case T43_IMAGE_TYPE_CMYK_BILEVEL:
        /* Table 1/T.43 1 bit/colour image (using CMYK primaries) */
        memset(s->colour_map, 0, sizeof(s->colour_map));
        /* White */
        s->colour_map[3*0x00 + 0] = 0xF0;
        s->colour_map[3*0x00 + 1] = 0xF0;
        s->colour_map[3*0x00 + 2] = 0xF0;
        /* Yellow */
        s->colour_map[3*0x20 + 0] = 0xF0;
        s->colour_map[3*0x20 + 1] = 0xF0;
        /* Magenta */
        s->colour_map[3*0x40 + 0] = 0xF0;
        s->colour_map[3*0x40 + 2] = 0xF0;
        /* Magenta + Yellow */
        s->colour_map[3*0x60 + 0] = 0xF0;
        /* Cyan */
        s->colour_map[3*0x80 + 1] = 0xF0;
        /* Cyan + Yellow */
        s->colour_map[3*0xA0 + 1] = 0xF0;
        /* Cyan + Magenta */
        s->colour_map[3*0xC0 + 2] = 0xF0;
        /* Black */
        s->colour_map_entries = 256;
        break;
    case T43_IMAGE_TYPE_8BIT_COLOUR_PALETTE:
        /* Palettized colour image (using CIELAB 8 bits/component precision table) */
        for (i = 0;  i < 3*256;  i += 3)
        {
            s->colour_map[i + 0] = i;
            s->colour_map[i + 1] = i;
            s->colour_map[i + 2] = i;
        }
        s->colour_map_entries = 256;
        break;
    case T43_IMAGE_TYPE_12BIT_COLOUR_PALETTE:
        /* Palettized colour image (using CIELAB 12 bits/component precision table) */
        break;
    case T43_IMAGE_TYPE_GRAY:
        /* Gray-scale image (using L*) */
        for (i = 0;  i < 256;  i++)
            s->colour_map[i] = i;
        s->colour_map_entries = 256;
        break;
    case T43_IMAGE_TYPE_COLOUR:
        /* Continuous-tone colour image (using CIELAB) */
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static int t43_analyse_header(t43_decode_state_t *s, const uint8_t data[], size_t len)
{
    int seg;
    int pos;
    int table_id;
    int val[6];
    uint8_t col[3];
    int i;

    pos = 0;
    if (pack_16(&data[pos]) != 0xFFA8)
        return 0;
    span_log(&s->logging, SPAN_LOG_FLOW, "Got BCIH (bit-plane colour image header)\n");
    pos += 2;
    for (;;)
    {
        if (pack_16(&data[pos]) == 0xFFE1)
        {
            pos += 2;
            seg = pack_16(&data[pos]);
            pos += 2;
            seg -= 2;
            if (seg >= 6  &&  strncmp((char *) &data[pos], "G3FAX", 5) == 0)
            {
                if (data[pos + 5] == 0xFF)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Got ECIH (end of colour image header)\n");
                    if (seg != 6)
                        span_log(&s->logging, SPAN_LOG_FLOW, "Got bad ECIH length - %d\n", seg);
                    pos += seg;
                    break;
                }
                switch (data[pos + 5])
                {
                case 0:
                    span_log(&s->logging, SPAN_LOG_FLOW, "Got G3FAX0\n");
                    if (seg < 6 + 10)
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Got bad G3FAX0 length - %d\n", seg);
                    }
                    else
                    {
                        val[0] = pack_16(&data[pos + 6 + 0]);
                        s->spatial_resolution = pack_16(&data[pos + 6 + 2]);
                        val[2] = data[pos + 6 + 4];
                        s->image_type = data[pos + 6 + 5];
                        s->bit_planes[0] = data[pos + 6 + 6];
                        s->bit_planes[1] = data[pos + 6 + 7];
                        s->bit_planes[2] = data[pos + 6 + 8];
                        s->bit_planes[3] = data[pos + 6 + 9];
                        if (s->image_type == T43_IMAGE_TYPE_GRAY)
                        {
                            s->samples_per_pixel = 1;
                        }
                        else if (s->image_type == T43_IMAGE_TYPE_CMYK_BILEVEL)
                        {
                            s->samples_per_pixel = 4;
                        }
                        else
                        {
                            s->samples_per_pixel = 3;
                        }
                        span_log(&s->logging,
                                 SPAN_LOG_FLOW,
                                 "Version %d, resolution %ddpi, coding method %d, type %s (%d), bit planes %d,%d,%d,%d\n",
                                 val[0],
                                 s->spatial_resolution,
                                 val[2],
                                 t43_image_type_to_str(s->image_type),
                                 s->image_type,
                                 s->bit_planes[0],
                                 s->bit_planes[1],
                                 s->bit_planes[2],
                                 s->bit_planes[3]);
                        set_simple_colour_map(s, s->image_type);
                    }
                    break;
                case 1:
                    span_log(&s->logging, SPAN_LOG_FLOW, "Set gamut\n");
                    if (seg < 6 + 12)
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Got bad G3FAX1 length - %d\n", seg);
                    }
                    else
                    {
                        set_gamut_from_code(&s->logging, &s->lab, &data[pos + 6]);
                    }
                    break;
                case 2:
                    span_log(&s->logging, SPAN_LOG_FLOW, "Set illuminant\n");
                    if (seg < 6 + 4)
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Got bad G3FAX2 length - %d\n", seg);
                    }
                    else
                    {
                        s->illuminant_colour_temperature = set_illuminant_from_code(&s->logging, &s->lab, &data[pos + 6]);
                    }
                    break;
                default:
                    span_log(&s->logging, SPAN_LOG_FLOW, "Got unexpected G3FAX%d length - %d\n", data[pos + 5], seg);
                    break;
                }
            }
            pos += seg;
        }
        else if (pack_16(&data[pos]) == 0xFFE3)
        {
            pos += 2;
            seg = pack_32(&data[pos]);
            pos += 4;
            seg -= 4;
            if (seg >= 6)
            {
                if (strncmp((char *) &data[pos], "G3FAX\3", 6) == 0)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Got G3FAX3\n");
                    table_id = pack_16(&data[pos + 6]);
                    span_log(&s->logging, SPAN_LOG_FLOW, "  Table ID %3d\n", table_id);
                    switch (table_id)
                    {
                    case 0:
                        /* 8 bit CIELAB */
                        s->colour_map_entries = pack_32(&data[pos + 8]);
                        span_log(&s->logging, SPAN_LOG_FLOW, "  Entries %6d (len %d)\n", s->colour_map_entries, seg);
                        if (seg >= 12 + s->colour_map_entries*3)
                        {
                            lab_to_srgb(&s->lab, s->colour_map, &data[pos + 12], s->colour_map_entries);
                        }
                        else
                        {
                            span_log(&s->logging, SPAN_LOG_FLOW, "Got bad G3FAX3 length - %d\n", seg);
                        }
                        break;
                    case 4:
                        /* 12 bit CIELAB */
                        s->colour_map_entries = pack_32(&data[pos + 8]);
                        span_log(&s->logging, SPAN_LOG_FLOW, "  Entries %6d\n", s->colour_map_entries);
                        /* TODO: implement 12bit stuff */
                        if (seg >= 12 + s->colour_map_entries*3*2)
                        {
                            for (i = 0;  i < s->colour_map_entries;  i++)
                            {
                                col[0] = pack_16(&data[pos + 12 + 6*i]) >> 4;
                                col[1] = pack_16(&data[pos + 12 + 6*i + 2]) >> 4;
                                col[2] = pack_16(&data[pos + 12 + 6*i + 4]) >> 4;
                                lab_to_srgb(&s->lab, &s->colour_map[3*i], col, 1);
                            }
                        }
                        else
                        {
                            span_log(&s->logging, SPAN_LOG_FLOW, "Got bad G3FAX3 length - %d\n", seg);
                        }
                        break;
                    default:
                        span_log(&s->logging, SPAN_LOG_FLOW, "Got bad G3FAX3 table ID - %d\n", table_id);
                        break;
                    }
                }
            }
            pos += seg;
        }
        else
        {
            break;
        }
    }
    return pos;
}
/*- End of function --------------------------------------------------------*/

static int t85_row_write_handler(void *user_data, const uint8_t buf[], size_t len)
{
    t43_decode_state_t *s;
    int i;
    int j;
    int image_size;
    uint8_t mask;

    /* Repack the bit packed T.85 image plane to a 3 x 8 bits per pixel colour image.
       We use the red entry now. This will be remapped to RGB later, as we apply the
       colour map. */
    s = (t43_decode_state_t *) user_data;

    if (s->buf == NULL)
    {
        image_size = s->samples_per_pixel*s->t85.xd*s->t85.yd;
        if ((s->buf = span_alloc(image_size)) == NULL)
            return -1;
        memset(s->buf, 0, image_size);
    }

    for (i = 0;  i < len;  i++)
    {
        mask = 0x80;
        if (s->samples_per_pixel == 1)
        {
            for (j = 0;  j < 8;  j += s->samples_per_pixel)
            {
                if ((buf[i] & mask))
                    s->buf[s->ptr + j] |= s->bit_plane_mask;
                mask >>= 1;
            }
        }
        else
        {
            for (j = 0;  j < s->samples_per_pixel*8;  j += s->samples_per_pixel)
            {
                if ((buf[i] & mask))
                    s->buf[s->ptr + j] |= s->bit_plane_mask;
                mask >>= 1;
            }
        }
        s->ptr += s->samples_per_pixel*8;
    }
    s->row++;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_decode_put(t43_decode_state_t *s, const uint8_t data[], size_t len)
{
    int i;
    int j;
    int plane_len;
    int total_len;
    int result;

    /* TODO: this isn't allowing for the header arriving in chunks */
    if (s->current_bit_plane < 0)
    {
        i = t43_analyse_header(s, data, len);
        data += i;
        len -= i;
        s->bit_plane_mask = 0x80;
        s->current_bit_plane++;

        /* There must be at least one bit plane. The real value for this will
           be filled in as the first plane is processed */
        s->t85.bit_planes = 1;
        s->ptr = 0;
        s->row = 0;
        s->buf = NULL;
        s->plane_ptr = 0;
        t85_decode_new_plane(&s->t85);
    }

    /* Now deal the bit-planes, one after another. */
    total_len = 0;
    result = 0;
    while (s->current_bit_plane < s->t85.bit_planes)
    {
        j = s->current_bit_plane;

        result = t85_decode_put(&s->t85, data, len);
        if (result != T4_DECODE_OK)
        {
            s->plane_ptr += len;
            return result;
        }
        plane_len = t85_decode_get_compressed_image_size(&s->t85);
        data += (plane_len/8 - s->plane_ptr);
        len -= (plane_len/8 - s->plane_ptr);
        total_len = s->ptr;

        /* Start the next plane */
        s->bit_plane_mask >>= 1;
        s->current_bit_plane++;
        s->ptr = 0;
        s->row = 0;
        s->plane_ptr = 0;
        t85_decode_new_plane(&s->t85);
    }
    /* Apply the colour map, and produce the RGB data from the collected bit-planes */
    if (s->samples_per_pixel == 1)
    {
        for (j = 0;  j < total_len;  j += s->samples_per_pixel)
            s->buf[j] = s->colour_map[s->buf[j]];
    }
    else
    {
        for (j = 0;  j < total_len;  j += s->samples_per_pixel)
        {
            i = s->buf[j];
            s->buf[j] = s->colour_map[3*i];
            s->buf[j + 1] = s->colour_map[3*i + 1];
            s->buf[j + 2] = s->colour_map[3*i + 2];
        }
    }
    for (j = 0;  j < s->t85.yd;  j++)
        s->row_write_handler(s->row_write_user_data, &s->buf[j*s->samples_per_pixel*s->t85.xd], s->samples_per_pixel*s->t85.xd);
    return result;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_decode_set_row_write_handler(t43_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data)
{
    s->row_write_handler = handler;
    s->row_write_user_data = user_data;
    s->t85.row_write_handler = handler;
    s->t85.row_write_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_decode_set_comment_handler(t43_decode_state_t *s,
                                                 uint32_t max_comment_len,
                                                 t4_row_write_handler_t handler,
                                                 void *user_data)
{
    return t85_decode_set_comment_handler(&s->t85, max_comment_len, handler, user_data);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_decode_set_image_size_constraints(t43_decode_state_t *s,
                                                        uint32_t max_xd,
                                                        uint32_t max_yd)
{
    return t85_decode_set_image_size_constraints(&s->t85, max_xd, max_yd);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t43_decode_get_image_width(t43_decode_state_t *s)
{
    return t85_decode_get_image_width(&s->t85);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t43_decode_get_image_length(t43_decode_state_t *s)
{
    return t85_decode_get_image_length(&s->t85);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_decode_get_compressed_image_size(t43_decode_state_t *s)
{
    return t85_decode_get_compressed_image_size(&s->t85);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t43_decode_get_logging_state(t43_decode_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_decode_restart(t43_decode_state_t *s)
{
    /* ITULAB */
    /* Illuminant D50 */
    //set_lab_illuminant(&s->lab, 96.422f, 100.000f,  82.521f);
    set_lab_illuminant(&s->lab, 100.0f, 100.0f, 100.0f);
    set_lab_gamut(&s->lab, 0, 100, -85, 85, -75, 125, false);

    s->t85.min_bit_planes = 1;
    s->t85.max_bit_planes = 8;
    s->bit_plane_mask = 0x80;
    s->current_bit_plane = -1;
    s->image_type = T43_IMAGE_TYPE_8BIT_COLOUR_PALETTE;

    return t85_decode_restart(&s->t85);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t43_decode_state_t *) t43_decode_init(t43_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (t43_decode_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.43");

    s->row_write_handler = handler;
    s->row_write_user_data = user_data;

    t85_decode_init(&s->t85, t85_row_write_handler, s);

    /* ITULAB */
    /* Illuminant D50 */
    //set_lab_illuminant(&s->lab, 96.422f, 100.000f,  82.521f);
    set_lab_illuminant(&s->lab, 100.0f, 100.0f, 100.0f);
    set_lab_gamut(&s->lab, 0, 100, -85, 85, -75, 125, false);

    s->t85.min_bit_planes = 1;
    s->t85.max_bit_planes = 8;
    s->bit_plane_mask = 0x80;
    s->current_bit_plane = -1;
    s->image_type = T43_IMAGE_TYPE_8BIT_COLOUR_PALETTE;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_decode_release(t43_decode_state_t *s)
{
    t85_decode_release(&s->t85);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t43_decode_free(t43_decode_state_t *s)
{
    int ret;

    ret = t43_decode_release(s);
    t85_decode_free(&s->t85);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
