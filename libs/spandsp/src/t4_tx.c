/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_tx.c - ITU T.4 FAX image transmit processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2007, 2010 Steve Underwood
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

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/async.h"
#include "spandsp/timezone.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/image_translate.h"
#include "spandsp/t81_t82_arith_coding.h"
#include "spandsp/t85.h"
#include "spandsp/t42.h"
#include "spandsp/t43.h"
#include "spandsp/t4_t6_decode.h"
#include "spandsp/t4_t6_encode.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#include "spandsp/private/t42.h"
#include "spandsp/private/t43.h"
#include "spandsp/private/t4_t6_decode.h"
#include "spandsp/private/t4_t6_encode.h"
#include "spandsp/private/image_translate.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"

#include "faxfont.h"

#if defined(SPANDSP_SUPPORT_TIFF_FX)
#include <tif_dir.h>
#endif

/*! The number of centimetres in one inch */
#define CM_PER_INCH                 2.54f

typedef struct
{
    uint8_t *buf;
    int ptr;
    int row;
    int size;
    int bit_mask;
} packer_t;

static void t4_tx_set_image_type(t4_tx_state_t *s, int image_type);
static void set_image_width(t4_tx_state_t *s, uint32_t image_width);
static void set_image_length(t4_tx_state_t *s, uint32_t image_length);

static const float x_res_table[] =
{
     100.0f*100.0f/CM_PER_INCH,
     102.0f*100.0f/CM_PER_INCH,
     200.0f*100.0f/CM_PER_INCH,
     204.0f*100.0f/CM_PER_INCH,
     300.0f*100.0f/CM_PER_INCH,
     400.0f*100.0f/CM_PER_INCH,
     408.0f*100.0f/CM_PER_INCH,
     600.0f*100.0f/CM_PER_INCH,
    1200.0f*100.0f/CM_PER_INCH,
                        -1.00f
};

static const float y_res_table[] =
{
                 38.50f*100.0f,
     100.0f*100.0f/CM_PER_INCH,
                 77.00f*100.0f,
     200.0f*100.0f/CM_PER_INCH,
     300.0f*100.0f/CM_PER_INCH,
                154.00f*100.0f,
     400.0f*100.0f/CM_PER_INCH,
     600.0f*100.0f/CM_PER_INCH,
     800.0f*100.0f/CM_PER_INCH,
    1200.0f*100.0f/CM_PER_INCH,
                        -1.00f
};

static const int resolution_map[10][9] =
{
    /*  x =           100 102                    200                         204                    300                    400                          408                     600                     1200 */
    {                    0, 0,                     0,  T4_RESOLUTION_R8_STANDARD,                     0,                     0,                           0,                      0,                       0}, /* y = 3.85/mm */
    {T4_RESOLUTION_100_100, 0, T4_RESOLUTION_200_100,                          0,                     0,                     0,                           0,                      0,                       0}, /* y = 100 */
    {                    0, 0,                     0,      T4_RESOLUTION_R8_FINE,                     0,                     0,                           0,                      0,                       0}, /* y = 7.7/mm */
    {                    0, 0, T4_RESOLUTION_200_200,                          0,                     0,                     0,                           0,                      0,                       0}, /* y = 200 */
    {                    0, 0,                     0,                          0, T4_RESOLUTION_300_300,                     0,                           0,                      0,                       0}, /* y = 300 */
    {                    0, 0,                     0, T4_RESOLUTION_R8_SUPERFINE,                     0,                     0, T4_RESOLUTION_R16_SUPERFINE,                      0,                       0}, /* y = 154/mm */
    {                    0, 0, T4_RESOLUTION_200_400,                          0,                     0, T4_RESOLUTION_400_400,                           0,                      0,                       0}, /* y = 400 */
    {                    0, 0,                     0,                          0, T4_RESOLUTION_300_600,                     0,                           0,  T4_RESOLUTION_600_600,                       0}, /* y = 600 */
    {                    0, 0,                     0,                          0,                     0, T4_RESOLUTION_400_800,                           0,                      0,                       0}, /* y = 800 */
    {                    0, 0,                     0,                          0,                     0,                     0,                           0, T4_RESOLUTION_600_1200, T4_RESOLUTION_1200_1200}  /* y = 1200 */
};

#if defined(SPANDSP_SUPPORT_TIFF_FX)
/* TIFF-FX related extensions to the tag set supported by libtiff */

static const TIFFFieldInfo tiff_fx_tiff_field_info[] =
{
    {TIFFTAG_INDEXED, 1, 1, TIFF_SHORT, FIELD_CUSTOM, false, false, (char *) "Indexed"},
    {TIFFTAG_GLOBALPARAMETERSIFD, 1, 1, TIFF_IFD8, FIELD_CUSTOM, false, false, (char *) "GlobalParametersIFD"},
    {TIFFTAG_PROFILETYPE, 1, 1, TIFF_LONG, FIELD_CUSTOM, false, false, (char *) "ProfileType"},
    {TIFFTAG_FAXPROFILE, 1, 1, TIFF_BYTE, FIELD_CUSTOM, false, false, (char *) "FaxProfile"},
    {TIFFTAG_CODINGMETHODS, 1, 1, TIFF_LONG, FIELD_CUSTOM, false, false, (char *) "CodingMethods"},
    {TIFFTAG_VERSIONYEAR, 4, 4, TIFF_BYTE, FIELD_CUSTOM, false, false, (char *) "VersionYear"},
    {TIFFTAG_MODENUMBER, 1, 1, TIFF_BYTE, FIELD_CUSTOM, false, false, (char *) "ModeNumber"},
    {TIFFTAG_DECODE, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SRATIONAL, FIELD_CUSTOM, false, true, (char *) "Decode"},
    {TIFFTAG_IMAGEBASECOLOR, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SHORT, FIELD_CUSTOM, false, true, (char *) "ImageBaseColor"},
    {TIFFTAG_T82OPTIONS, 1, 1, TIFF_LONG, FIELD_CUSTOM, false, false, (char *) "T82Options"},
    {TIFFTAG_STRIPROWCOUNTS, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_LONG, FIELD_CUSTOM, false, true, (char *) "StripRowCounts"},
    {TIFFTAG_IMAGELAYER, 2, 2, TIFF_LONG, FIELD_CUSTOM, false, false, (char *) "ImageLayer"},
};

#if TIFFLIB_VERSION >= 20120615
static TIFFField tiff_fx_tiff_fields[] =
{
    { TIFFTAG_INDEXED, 1, 1, TIFF_SHORT, 0, TIFF_SETGET_UINT16, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "Indexed" },
    { TIFFTAG_GLOBALPARAMETERSIFD, 1, 1, TIFF_IFD8, 0, TIFF_SETGET_IFD8, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 0, 0, (char *) "GlobalParametersIFD", NULL },
    { TIFFTAG_PROFILETYPE, 1, 1, TIFF_LONG, 0, TIFF_SETGET_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "ProfileType", NULL },
    { TIFFTAG_FAXPROFILE, 1, 1, TIFF_BYTE, 0, TIFF_SETGET_UINT8, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "FaxProfile", NULL },
    { TIFFTAG_CODINGMETHODS, 1, 1, TIFF_LONG, 0, TIFF_SETGET_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "CodingMethods", NULL },
    { TIFFTAG_VERSIONYEAR, 4, 4, TIFF_BYTE, 0, TIFF_SETGET_C0_UINT8, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "VersionYear", NULL },
    { TIFFTAG_MODENUMBER, 1, 1, TIFF_BYTE, 0, TIFF_SETGET_UINT8, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "ModeNumber", NULL },
    { TIFFTAG_DECODE, -1, -1, TIFF_SRATIONAL, 0, TIFF_SETGET_C16_FLOAT, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 1, (char *) "Decode", NULL },
    { TIFFTAG_IMAGEBASECOLOR, -1, -1, TIFF_SHORT, 0, TIFF_SETGET_C16_UINT16, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 1, (char *) "ImageBaseColor", NULL },
    { TIFFTAG_T82OPTIONS, 1, 1, TIFF_LONG, 0, TIFF_SETGET_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "T82Options", NULL },
    { TIFFTAG_STRIPROWCOUNTS, -1, -1, TIFF_LONG, 0, TIFF_SETGET_C16_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 1, (char *) "StripRowCounts", NULL },
    { TIFFTAG_IMAGELAYER, 2, 2, TIFF_LONG, 0, TIFF_SETGET_C0_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "ImageLayer", NULL },
};

TIFFFieldArray tiff_fx_field_array = { tfiatOther, 0, 12, tiff_fx_tiff_fields };
#endif

static TIFFExtendProc _ParentExtender = NULL;

static void TIFFFXDefaultDirectory(TIFF *tif)
{
    /* Install the extended tag field info */
    TIFFMergeFieldInfo(tif, tiff_fx_tiff_field_info, 12);

    /* Since we may have overriddden another directory method, we call it now to
       allow it to set up the rest of its own methods. */
    if (_ParentExtender)
        (*_ParentExtender)(tif);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) TIFF_FX_init(void)
{
    static int first_time = true;

    if (!first_time)
        return;
    first_time = false;

    /* Grab the inherited method and install */
    _ParentExtender = TIFFSetTagExtender(TIFFFXDefaultDirectory);
}
/*- End of function --------------------------------------------------------*/
#endif

static int code_to_x_resolution(int code)
{
    static const int xxx[] =
    {
        T4_X_RESOLUTION_R8,         /* R8 x standard */
        T4_X_RESOLUTION_R8,         /* R8 x fine */
        T4_X_RESOLUTION_R8,         /* R8 x superfine */
        T4_X_RESOLUTION_R16,        /* R16 x superfine */
        T4_X_RESOLUTION_100,        /* 100x100 */
        T4_X_RESOLUTION_200,        /* 200x100 */
        T4_X_RESOLUTION_200,        /* 200x200 */
        T4_X_RESOLUTION_200,        /* 200x400 */
        T4_X_RESOLUTION_300,        /* 300x300 */
        T4_X_RESOLUTION_300,        /* 300x600 */
        T4_X_RESOLUTION_400,        /* 400x400 */
        T4_X_RESOLUTION_400,        /* 400x800 */
        T4_X_RESOLUTION_600,        /* 600x600 */
        T4_X_RESOLUTION_600,        /* 600x1200 */
        T4_X_RESOLUTION_1200        /* 1200x1200 */
    };
    int entry;

    entry = top_bit(code);
    if (entry < 0  ||  entry > 14)
        return 0;
    return xxx[entry];
}
/*- End of function --------------------------------------------------------*/

static int code_to_y_resolution(int code)
{
    static const int yyy[] =
    {
        T4_Y_RESOLUTION_STANDARD,   /* R8 x standard */
        T4_Y_RESOLUTION_FINE,       /* R8 x fine */
        T4_Y_RESOLUTION_SUPERFINE,  /* R8 x superfine */
        T4_Y_RESOLUTION_SUPERFINE,  /* R16 x superfine */
        T4_Y_RESOLUTION_100,        /* 100x100 */
        T4_Y_RESOLUTION_100,        /* 200x100 */
        T4_Y_RESOLUTION_200,        /* 200x200 */
        T4_Y_RESOLUTION_400,        /* 200x400 */
        T4_Y_RESOLUTION_300,        /* 300x300 */
        T4_Y_RESOLUTION_600,        /* 300x600 */
        T4_Y_RESOLUTION_400,        /* 400x400 */
        T4_Y_RESOLUTION_800,        /* 400x800 */
        T4_Y_RESOLUTION_600,        /* 600x600 */
        T4_Y_RESOLUTION_1200,       /* 600x1200 */
        T4_Y_RESOLUTION_1200        /* 1200x1200 */
    };
    int entry;

    entry = top_bit(code);
    if (entry < 0  ||  entry > 14)
        return 0;
    return yyy[entry];
}
/*- End of function --------------------------------------------------------*/

static int match_resolution(float actual, const float table[])
{
    int i;
    int best_entry;
    float best_ratio;
    float ratio;

    if (actual == 0.0f)
        return -1;

    best_ratio = 0.0f;
    best_entry = -1;
    for (i = 0;  table[i] > 0.0f;  i++)
    {
        if (actual > table[i])
            ratio = table[i]/actual;
        else
            ratio = actual/table[i];
        if (ratio > best_ratio)
        {
            best_entry = i;
            best_ratio = ratio;
        }
    }
    if (best_ratio < 0.95f)
        return -1;
    return best_entry;
}
/*- End of function --------------------------------------------------------*/

#if 0
static int best_colour_resolution(float actual, int allowed_resolutions)
{
    static const struct
    {
        float resolution;
        int resolution_code;
    } x_res_table[] =
    {
        { 100.0f*100.0f/CM_PER_INCH, T4_RESOLUTION_100_100},
        { 200.0f*100.0f/CM_PER_INCH, T4_RESOLUTION_200_200},
        { 300.0f*100.0f/CM_PER_INCH, T4_RESOLUTION_300_300},
        { 400.0f*100.0f/CM_PER_INCH, T4_RESOLUTION_400_400},
        { 600.0f*100.0f/CM_PER_INCH, T4_RESOLUTION_600_600},
        {1200.0f*100.0f/CM_PER_INCH, T4_RESOLUTION_1200_1200},
        {                    -1.00f, -1}
    };
    int i;
    int best_entry;
    float best_ratio;
    float ratio;

    if (actual == 0.0f)
        return -1;

    best_ratio = 0.0f;
    best_entry = 0;
    for (i = 0;  x_res_table[i].resolution > 0.0f;  i++)
    {
        if (!(allowed_resolutions & x_res_table[i].resolution_code))
            continue;
        if (actual > x_res_table[i].resolution)
            ratio = x_res_table[i].resolution/actual;
        else
            ratio = actual/x_res_table[i].resolution;
        if (ratio > best_ratio)
        {
            best_entry = i;
            best_ratio = ratio;
        }
    }
    return x_res_table[best_entry].resolution_code;
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(SPANDSP_SUPPORT_TIFF_FX)
static int read_colour_map(t4_tx_state_t *s, int bits_per_sample)
{
    int i;
    uint16_t *map_L;
    uint16_t *map_a;
    uint16_t *map_b;
    uint16_t *map_z;

    map_L = NULL;
    map_a = NULL;
    map_b = NULL;
    map_z = NULL;
    if (!TIFFGetField(s->tiff.tiff_file, TIFFTAG_COLORMAP, &map_L, &map_a, &map_b, &map_z))
        return -1;

    /* TODO: This only allows for 8 bit deep maps */
    span_log(&s->logging, SPAN_LOG_FLOW, "Got a colour map\n");
    s->colour_map_entries = 1 << bits_per_sample;
    if ((s->colour_map = span_realloc(s->colour_map, 3*s->colour_map_entries)) == NULL)
        return -1;
#if 0
    /* Sweep the colormap in the proper order */
    for (i = 0;  i < s->colour_map_entries;  i++)
    {
        s->colour_map[3*i + 0] = (map_L[i] >> 8) & 0xFF;
        s->colour_map[3*i + 1] = (map_a[i] >> 8) & 0xFF;
        s->colour_map[3*i + 2] = (map_b[i] >> 8) & 0xFF;
        span_log(&s->logging, SPAN_LOG_FLOW, "Map %3d - %5d %5d %5d\n", i, s->colour_map[3*i], s->colour_map[3*i + 1], s->colour_map[3*i + 2]);
    }
#else
    /* Sweep the colormap in the order that seems to work for l04x_02x.tif */
    for (i = 0;  i < s->colour_map_entries;  i++)
    {
        s->colour_map[0*s->colour_map_entries + i] = (map_L[i] >> 8) & 0xFF;
        s->colour_map[1*s->colour_map_entries + i] = (map_a[i] >> 8) & 0xFF;
        s->colour_map[2*s->colour_map_entries + i] = (map_b[i] >> 8) & 0xFF;
    }
#endif
    lab_to_srgb(&s->lab_params, s->colour_map, s->colour_map, s->colour_map_entries);
    for (i = 0;  i < s->colour_map_entries;  i++)
        span_log(&s->logging, SPAN_LOG_FLOW, "Map %3d - %5d %5d %5d\n", i, s->colour_map[3*i], s->colour_map[3*i + 1], s->colour_map[3*i + 2]);
    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

static int get_tiff_directory_info(t4_tx_state_t *s)
{
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    static const char *tiff_fx_fax_profiles[] =
    {
        "???",
        "profile S",
        "profile F",
        "profile J",
        "profile C",
        "profile L",
        "profile M"
    };
    char *u;
    char uu[10];
    float *fl_parms;
    uint64_t diroff;
    float lmin;
    float lmax;
    float amin;
    float amax;
    float bmin;
    float bmax;
    uint8_t parm8;
#endif
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    uint16_t parm16;
#endif
    uint32_t parm32;
    int best_x_entry;
    int best_y_entry;
    float x_resolution;
    float y_resolution;
    t4_tx_tiff_state_t *t;
    uint16_t bits_per_sample;
    uint16_t samples_per_pixel;
    uint16_t res_unit;
    uint16_t YCbCrSubsample_horiz;
    uint16_t YCbCrSubsample_vert;

    t = &s->tiff;
    bits_per_sample = 1;
    TIFFGetField(t->tiff_file, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    samples_per_pixel = 1;
    TIFFGetField(t->tiff_file, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    if (samples_per_pixel == 1  &&  bits_per_sample == 1)
        t->image_type = T4_IMAGE_TYPE_BILEVEL;
    else if (samples_per_pixel == 3  &&  bits_per_sample == 1)
        t->image_type = T4_IMAGE_TYPE_COLOUR_BILEVEL;
    else if (samples_per_pixel == 4  &&  bits_per_sample == 1)
        t->image_type = T4_IMAGE_TYPE_COLOUR_BILEVEL;
    else if (samples_per_pixel == 1  &&  bits_per_sample == 8)
        t->image_type = T4_IMAGE_TYPE_GRAY_8BIT;
    else if (samples_per_pixel == 1  &&  bits_per_sample > 8)
        t->image_type = T4_IMAGE_TYPE_GRAY_12BIT;
    else if (samples_per_pixel == 3  &&  bits_per_sample == 8)
        t->image_type = T4_IMAGE_TYPE_COLOUR_8BIT;
    else if (samples_per_pixel == 3  &&  bits_per_sample > 8)
        t->image_type = T4_IMAGE_TYPE_COLOUR_12BIT;
    else
        return -1;

#if defined(SPANDSP_SUPPORT_TIFF_FX)
    parm16 = 0;
    if (TIFFGetField(t->tiff_file, TIFFTAG_INDEXED, &parm16))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Indexed %s (%u)\n", (parm16)  ?  "palette image"  :  "non-palette image", parm16);
        if (parm16 == 1)
        {
            /* Its an indexed image, so its really a colour image, even though it may have only one sample per pixel */
            if (samples_per_pixel == 1  &&  bits_per_sample == 8)
                t->image_type = T4_IMAGE_TYPE_COLOUR_8BIT;
            else if (samples_per_pixel == 1  &&  bits_per_sample > 8)
                t->image_type = T4_IMAGE_TYPE_COLOUR_12BIT;
        }
    }
#endif

    parm32 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_IMAGEWIDTH, &parm32);
    t->image_width = parm32;
    parm32 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_IMAGELENGTH, &parm32);
    t->image_length = parm32;

    x_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_XRESOLUTION, &x_resolution);
    y_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_YRESOLUTION, &y_resolution);
    res_unit = RESUNIT_INCH;
    TIFFGetField(t->tiff_file, TIFFTAG_RESOLUTIONUNIT, &res_unit);

    t->x_resolution = x_resolution*100.0f;
    t->y_resolution = y_resolution*100.0f;
    if (res_unit == RESUNIT_INCH)
    {
        t->x_resolution /= CM_PER_INCH;
        t->y_resolution /= CM_PER_INCH;
    }

    if (((best_x_entry = match_resolution(t->x_resolution, x_res_table)) >= 0)
        &&
        ((best_y_entry = match_resolution(t->y_resolution, y_res_table)) >= 0))
    {
        t->resolution_code = resolution_map[best_y_entry][best_x_entry];
    }
    else
    {
        t->resolution_code = 0;
    }

    t->photo_metric = PHOTOMETRIC_MINISWHITE;
    TIFFGetField(t->tiff_file, TIFFTAG_PHOTOMETRIC, &t->photo_metric);

    /* The default luminant is D50 */
    set_lab_illuminant(&s->lab_params, 96.422f, 100.000f,  82.521f);
    set_lab_gamut(&s->lab_params, 0, 100, -85, 85, -75, 125, false);

    t->compression = -1;
    TIFFGetField(t->tiff_file, TIFFTAG_COMPRESSION, &t->compression);
    switch (t->compression)
    {
    case COMPRESSION_CCITT_T4:
        span_log(&s->logging, SPAN_LOG_FLOW, "T.4\n");
        break;
    case COMPRESSION_CCITT_T6:
        span_log(&s->logging, SPAN_LOG_FLOW, "T.6\n");
        break;
    case COMPRESSION_T85:
        span_log(&s->logging, SPAN_LOG_FLOW, "T.85\n");
        break;
    case COMPRESSION_T43:
        span_log(&s->logging, SPAN_LOG_FLOW, "T.43\n");
        break;
    case COMPRESSION_JPEG:
        span_log(&s->logging, SPAN_LOG_FLOW, "JPEG\n");
        if (t->photo_metric == PHOTOMETRIC_ITULAB)
            span_log(&s->logging, SPAN_LOG_FLOW, "ITULAB\n");
        break;
    case COMPRESSION_NONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "No compression\n");
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected compression %d\n", t->compression);
        break;
    }

#if defined(SPANDSP_SUPPORT_TIFF_FX)
    read_colour_map(s, bits_per_sample);
#endif

    YCbCrSubsample_horiz = 0;
    YCbCrSubsample_vert = 0;
    if (TIFFGetField(t->tiff_file, TIFFTAG_YCBCRSUBSAMPLING, &YCbCrSubsample_horiz, &YCbCrSubsample_vert))
        span_log(&s->logging, SPAN_LOG_FLOW, "Subsampling %d %d\n", YCbCrSubsample_horiz, YCbCrSubsample_vert);

    t->fill_order = FILLORDER_LSB2MSB;

#if defined(SPANDSP_SUPPORT_TIFF_FX)
    if (TIFFGetField(t->tiff_file, TIFFTAG_PROFILETYPE, &parm32))
        span_log(&s->logging, SPAN_LOG_FLOW, "Profile type %u\n", parm32);
    if (TIFFGetField(t->tiff_file, TIFFTAG_FAXPROFILE, &parm8))
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX profile %s (%u)\n", tiff_fx_fax_profiles[parm8], parm8);

    if (TIFFGetField(t->tiff_file, TIFFTAG_CODINGMETHODS, &parm32))
        span_log(&s->logging, SPAN_LOG_FLOW, "Coding methods 0x%x\n", parm32);
    if (TIFFGetField(t->tiff_file, TIFFTAG_VERSIONYEAR, &u))
    {
        memcpy(uu, u, 4);
        uu[4] = '\0';
        span_log(&s->logging, SPAN_LOG_FLOW, "Version year \"%s\"\n", uu);
    }
    if (TIFFGetField(t->tiff_file, TIFFTAG_MODENUMBER, &parm8))
        span_log(&s->logging, SPAN_LOG_FLOW, "Mode number %u\n", parm8);

    switch (t->photo_metric)
    {
    case PHOTOMETRIC_ITULAB:
#if 1
        /* 8 bit version */
        lmin = 0.0f;
        lmax = 100.0f;
        amin = -21760.0f/255.0f;
        amax = 21590.0f/255.0f;
        bmin = -19200.0f/255.0f;
        bmax = 31800.0f/255.0f;
#else
        /* 12 bit version */
        lmin = 0.0f;
        lmax = 100.0f;
        amin = -348160.0f/4095.0f
        amax = 347990.0f/4095.0f
        bmin = -307200.0f/4095.0f
        bmax = 511800.0f/4095.0f
#endif
        break;
    default:
        lmin = 0.0f;
        lmax = 0.0f;
        amin = 0.0f;
        amax = 0.0f;
        bmin = 0.0f;
        bmax = 0.0f;
        break;
    }

    if (TIFFGetField(t->tiff_file, TIFFTAG_DECODE, &parm16, &fl_parms))
    {
        lmin = fl_parms[0];
        lmax = fl_parms[1];
        amin = fl_parms[2];
        amax = fl_parms[3];
        bmin = fl_parms[4];
        bmax = fl_parms[5];
        span_log(&s->logging, SPAN_LOG_FLOW, "Got decode tag %f %f %f %f %f %f\n", lmin, lmax, amin, amax, bmin, bmax);
    }

    /* TIFFTAG_IMAGEBASECOLOR */

    if (TIFFGetField(t->tiff_file, TIFFTAG_T82OPTIONS, &parm32))
        span_log(&s->logging, SPAN_LOG_FLOW, "T.82 options 0x%x\n", parm32);

    /* TIFFTAG_STRIPROWCOUNTS */
    /* TIFFTAG_IMAGELAYER */

    /* If global parameters are present they should only be on the first page of the file.
       However, as we scan the file we might as well look for them on any page. */
    if (TIFFGetField(t->tiff_file, TIFFTAG_GLOBALPARAMETERSIFD, &diroff))
    {
        if (!TIFFReadCustomDirectory(t->tiff_file, diroff, &tiff_fx_field_array))
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Global parameter read failed\n");
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Global parameters\n");
            if (TIFFGetField(t->tiff_file, TIFFTAG_PROFILETYPE, &parm32))
                span_log(&s->logging, SPAN_LOG_FLOW, "  Profile type %u\n", parm32);
            if (TIFFGetField(t->tiff_file, TIFFTAG_FAXPROFILE, &parm8))
                span_log(&s->logging, SPAN_LOG_FLOW, "  FAX profile %s (%u)\n", tiff_fx_fax_profiles[parm8], parm8);
            if (TIFFGetField(t->tiff_file, TIFFTAG_CODINGMETHODS, &parm32))
                span_log(&s->logging, SPAN_LOG_FLOW, "  Coding methods 0x%x\n", parm32);
            if (TIFFGetField(t->tiff_file, TIFFTAG_VERSIONYEAR, &u))
            {
                memcpy(uu, u, 4);
                uu[4] = '\0';
                span_log(&s->logging, SPAN_LOG_FLOW, "  Version year \"%s\"\n", uu);
            }
            if (TIFFGetField(t->tiff_file, TIFFTAG_MODENUMBER, &parm8))
                span_log(&s->logging, SPAN_LOG_FLOW, "  Mode number %u\n", parm8);

            if (!TIFFSetDirectory(t->tiff_file, (tdir_t) s->current_page))
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to set directory to page %d\n", s->current_page);
        }
    }
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int test_tiff_directory_info(t4_tx_state_t *s)
{
    uint16_t res_unit;
    uint32_t parm32;
    uint16_t bits_per_sample;
    uint16_t samples_per_pixel;
    int image_type;
    float x_resolution;
    float y_resolution;
    t4_tx_tiff_state_t *t;

    t = &s->tiff;
    bits_per_sample = 1;
    TIFFGetField(t->tiff_file, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    samples_per_pixel = 1;
    TIFFGetField(t->tiff_file, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    if (samples_per_pixel == 1  &&  bits_per_sample == 1)
        image_type = T4_IMAGE_TYPE_BILEVEL;
    else if (samples_per_pixel == 3  &&  bits_per_sample == 1)
        image_type = T4_IMAGE_TYPE_COLOUR_BILEVEL;
    else if (samples_per_pixel == 4  &&  bits_per_sample == 1)
        image_type = T4_IMAGE_TYPE_COLOUR_BILEVEL;
    else if (samples_per_pixel == 1  &&  bits_per_sample == 8)
        image_type = T4_IMAGE_TYPE_GRAY_8BIT;
    else if (samples_per_pixel == 1  &&  bits_per_sample > 8)
        image_type = T4_IMAGE_TYPE_GRAY_12BIT;
    else if (samples_per_pixel == 3  &&  bits_per_sample == 8)
        image_type = T4_IMAGE_TYPE_COLOUR_8BIT;
    else if (samples_per_pixel == 3  &&  bits_per_sample > 8)
        image_type = T4_IMAGE_TYPE_COLOUR_12BIT;
    else
        image_type = -1;
    if (t->image_type != image_type)
        return 1;

    parm32 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_IMAGEWIDTH, &parm32);
    if (s->tiff.image_width != (int) parm32)
        return 2;

    x_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_XRESOLUTION, &x_resolution);
    y_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_YRESOLUTION, &y_resolution);
    res_unit = RESUNIT_INCH;
    TIFFGetField(t->tiff_file, TIFFTAG_RESOLUTIONUNIT, &res_unit);

    x_resolution *= 100.0f;
    y_resolution *= 100.0f;
    if (res_unit == RESUNIT_INCH)
    {
        x_resolution /= CM_PER_INCH;
        y_resolution /= CM_PER_INCH;
    }
    if (s->tiff.x_resolution != (int) x_resolution)
        return 3;
    if (s->tiff.y_resolution != (int) y_resolution)
        return 4;

    return 0;
}
/*- End of function --------------------------------------------------------*/

static int get_tiff_total_pages(t4_tx_state_t *s)
{
    int max;

    /* Each page *should* contain the total number of pages, but can this be
       trusted? Some files say 0. Actually searching for the last page is
       more reliable. */
    max = 0;
    while (TIFFSetDirectory(s->tiff.tiff_file, (tdir_t) max))
        max++;
    /* Back to the previous page */
    if (!TIFFSetDirectory(s->tiff.tiff_file, (tdir_t) s->current_page))
        return -1;
    return max;
}
/*- End of function --------------------------------------------------------*/

static int open_tiff_input_file(t4_tx_state_t *s, const char *file)
{
    if ((s->tiff.tiff_file = TIFFOpen(file, "r")) == NULL)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int metadata_row_read_handler(void *user_data, uint8_t buf[], size_t len)
{
    t4_tx_state_t *s;

    s = (t4_tx_state_t *) user_data;
    if (s->tiff.row >= s->metadata.image_length)
        return 0;
    memcpy(buf, &s->tiff.image_buffer[s->tiff.row*len], len);
    s->tiff.row++;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int tiff_row_read_handler(void *user_data, uint8_t buf[], size_t len)
{
    t4_tx_state_t *s;
    int i;
    int j;

    s = (t4_tx_state_t *) user_data;
    if (s->tiff.row >= s->tiff.image_length)
        return 0;
    if (s->tiff.image_buffer == NULL)
        return 0;
    memcpy(buf, &s->tiff.image_buffer[s->tiff.row*len], len);
    s->tiff.row++;

    /* If this is a bi-level image which has more vertical resolution than the
       far end will accept, we need to squash it down to size. */
    for (i = 1;  i < s->row_squashing_ratio  &&  s->tiff.row < s->tiff.image_length;  i++)
    {
        for (j = 0;  j < len;  j++)
            buf[j] |= s->tiff.image_buffer[s->tiff.row*len + j];
        s->tiff.row++;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static int translate_row_read2(void *user_data, uint8_t buf[], size_t len)
{
    t4_tx_state_t *s;

    s = (t4_tx_state_t *) user_data;
    memcpy(buf, &s->pack_buf[s->pack_ptr], len);
    s->pack_ptr += len;
    s->pack_row++;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int translate_row_read(void *user_data, uint8_t buf[], size_t len)
{
    t4_tx_state_t *s;
    int i;
    int j;

    s = (t4_tx_state_t *) user_data;

    if (s->tiff.raw_row >= s->tiff.image_length)
        return 0;

    if (TIFFReadScanline(s->tiff.tiff_file, buf, s->tiff.raw_row, 0) < 0)
        return 0;
    s->tiff.raw_row++;

    /* If this is a bi-level image which is stretched more vertically than we are able
       to send we need to squash it down to size. */
    for (i = 1;  i < s->row_squashing_ratio;  i++)
    {
#if defined(_MSC_VER)
        uint8_t *extra_buf = (uint8_t *) _alloca(len);
#else
        uint8_t extra_buf[len];
#endif

        if (TIFFReadScanline(s->tiff.tiff_file, extra_buf, s->tiff.raw_row, 0) < 0)
            return 0;
        s->tiff.raw_row++;
        /* We know this is a bi-level image if we are squashing */
        for (j = 0;  j < s->tiff.image_width/8;  j++)
            buf[j] |= extra_buf[s->tiff.image_width/8 + j];
    }
    if (s->apply_lab)
        lab_to_srgb(&s->lab_params, buf, buf, len/3);
    return len;
}
/*- End of function --------------------------------------------------------*/

static int packing_row_write_handler(void *user_data, const uint8_t buf[], size_t len)
{
    packer_t *s;

    s = (packer_t *) user_data;
    memcpy(&s->buf[s->ptr], buf, len);
    s->ptr += len;
    s->row++;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int embedded_comment_handler(void *user_data, const uint8_t buf[], size_t len)
{
    t4_tx_state_t *s;

    s = (t4_tx_state_t *) user_data;
    if (buf)
        span_log(&s->logging, SPAN_LOG_WARNING, "T.85 comment (%d): %s\n", (int) len, buf);
    else
        span_log(&s->logging, SPAN_LOG_WARNING, "T.85 comment (%d): ---\n", (int) len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int read_tiff_raw_image(t4_tx_state_t *s)
{
    int num_strips;
    int total_len;
    int len;
    int i;

    num_strips = TIFFNumberOfStrips(s->tiff.tiff_file);
    total_len = 0;
    for (i = 0;  i < num_strips;  i++)
        total_len += TIFFRawStripSize(s->tiff.tiff_file, i);
    if ((s->no_encoder.buf = span_realloc(s->no_encoder.buf, total_len)) == NULL)
        return -1;
    total_len = 0;
    for (i = 0;  i < num_strips;  i++, total_len += len)
    {
        len = TIFFRawStripSize(s->tiff.tiff_file, i);
        if ((len = TIFFReadRawStrip(s->tiff.tiff_file, i, &s->no_encoder.buf[total_len], len)) < 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: TIFFReadRawStrip error.\n", s->tiff.file);
            return -1;
        }
    }
    s->no_encoder.buf_len = total_len;
    s->no_encoder.buf_ptr = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int read_tiff_t85_image(t4_tx_state_t *s)
{
    int biggest;
    int num_strips;
    int len;
    int i;
    int result;
    uint8_t *t;
    uint8_t *raw_data;
    t85_decode_state_t t85;
    packer_t pack;

    /* Size up and allocate the buffer for the raw data */
    num_strips = TIFFNumberOfStrips(s->tiff.tiff_file);
    biggest = 0;
    for (i = 0;  i < num_strips;  i++)
    {
        len = TIFFRawStripSize(s->tiff.tiff_file, i);
        if (len > biggest)
            biggest = len;
    }
    if ((raw_data = span_alloc(biggest)) == NULL)
        return -1;

    s->tiff.image_size = s->tiff.image_length*((s->tiff.image_width + 7)/8);
    if (s->tiff.image_size >= s->tiff.image_buffer_size)
    {
        if ((t = span_realloc(s->tiff.image_buffer, s->tiff.image_size)) == NULL)
        {
            span_free(raw_data);
            return -1;
        }
        s->tiff.image_buffer_size = s->tiff.image_size;
        s->tiff.image_buffer = t;
    }

    pack.buf = s->tiff.image_buffer;
    pack.ptr = 0;
    pack.size = s->tiff.image_size;
    pack.row = 0;
    t85_decode_init(&t85, packing_row_write_handler, &pack);
    t85_decode_set_comment_handler(&t85, 1000, embedded_comment_handler, s);
    t85_decode_set_image_size_constraints(&t85, s->tiff.image_width, s->tiff.image_length);
    result = -1;
    for (i = 0;  i < num_strips;  i++)
    {
        len = TIFFRawStripSize(s->tiff.tiff_file, i);
        if ((len = TIFFReadRawStrip(s->tiff.tiff_file, i, raw_data, len)) < 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: TIFFReadRawStrip error.\n", s->tiff.file);
            span_free(raw_data);
            return -1;
        }
        if ((result = t85_decode_put(&t85, raw_data, len)) != T4_DECODE_MORE_DATA)
            break;
    }
    if (result == T4_DECODE_MORE_DATA)
        result = t85_decode_put(&t85, NULL, 0);

    len = t85_decode_get_compressed_image_size(&t85);
    span_log(&s->logging, SPAN_LOG_WARNING, "Compressed image is %d bytes, %d rows\n", len/8, s->tiff.image_length);
    t85_decode_release(&t85);
    span_free(raw_data);
    return 0;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_SUPPORT_T43)
static int read_tiff_t43_image(t4_tx_state_t *s)
{
    int biggest;
    int num_strips;
    int len;
    int i;
    int result;
    uint8_t *t;
    uint8_t *raw_data;
    logging_state_t *logging;
    t43_decode_state_t t43;
    packer_t pack;
    uint16_t bits_per_sample;
    uint16_t samples_per_pixel;

    bits_per_sample = 1;
    TIFFGetField(s->tiff.tiff_file, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    samples_per_pixel = 3;
    TIFFGetField(s->tiff.tiff_file, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);

    samples_per_pixel = 3;

    num_strips = TIFFNumberOfStrips(s->tiff.tiff_file);
    biggest = 0;
    for (i = 0;  i < num_strips;  i++)
    {
        len = TIFFRawStripSize(s->tiff.tiff_file, i);
        if (len > biggest)
            biggest = len;
    }
    if ((raw_data = span_alloc(biggest)) == NULL)
        return -1;

    s->tiff.image_size = samples_per_pixel*s->tiff.image_width*s->tiff.image_length;
    if (s->tiff.image_size >= s->tiff.image_buffer_size)
    {
        if ((t = span_realloc(s->tiff.image_buffer, s->tiff.image_size)) == NULL)
        {
            span_free(raw_data);
            return -1;
        }
        s->tiff.image_buffer_size = s->tiff.image_size;
        s->tiff.image_buffer = t;
    }

    t43_decode_init(&t43, packing_row_write_handler, &pack);
    t43_decode_set_comment_handler(&t43, 1000, embedded_comment_handler, NULL);
    t43_decode_set_image_size_constraints(&t43, s->tiff.image_width, s->tiff.image_length);
    logging = t43_decode_get_logging_state(&t43);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

    pack.buf = s->tiff.image_buffer;
    pack.ptr = 0;
    pack.size = s->tiff.image_size;
    pack.row = 0;

    result = -1;
    for (i = 0;  i < num_strips;  i++)
    {
        len = TIFFRawStripSize(s->tiff.tiff_file, i);
        if ((len = TIFFReadRawStrip(s->tiff.tiff_file, i, raw_data, len)) < 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: TIFFReadRawStrip error.\n", s->tiff.file);
            span_free(raw_data);
            return -1;
        }
        if ((result = t43_decode_put(&t43, raw_data, len)) != T4_DECODE_MORE_DATA)
            break;
    }
    if (result == T4_DECODE_MORE_DATA)
        result = t43_decode_put(&t43, NULL, 0);

    t43_decode_release(&t43);
    span_free(raw_data);
    return s->tiff.image_size;
}
/*- End of function --------------------------------------------------------*/
#endif

static int read_tiff_t42_t81_image(t4_tx_state_t *s)
{
    int total_len;
    int len;
    int i;
    int num_strips;
    int total_image_len;
    uint8_t *t;
    uint8_t *raw_data;
    uint8_t *jpeg_table;
    uint32_t jpeg_table_len;
    packer_t pack;
    uint16_t bits_per_sample;
    uint16_t samples_per_pixel;
    t42_decode_state_t t42;

    bits_per_sample = 1;
    TIFFGetField(s->tiff.tiff_file, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    samples_per_pixel = 1;
    TIFFGetField(s->tiff.tiff_file, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);

    num_strips = TIFFNumberOfStrips(s->tiff.tiff_file);
    total_image_len = 0;
    jpeg_table_len = 0;
    if (TIFFGetField(s->tiff.tiff_file, TIFFTAG_JPEGTABLES, &jpeg_table_len, &jpeg_table))
    {
        total_image_len += (jpeg_table_len - 4);
        span_log(&s->logging, SPAN_LOG_FLOW, "JPEG tables %u\n", jpeg_table_len);
    }

    for (i = 0;  i < num_strips;  i++)
        total_image_len += TIFFRawStripSize(s->tiff.tiff_file, i);
    if ((raw_data = span_alloc(total_image_len)) == NULL)
        return -1;

    total_len = 0;
    if (jpeg_table_len > 0)
        total_len += jpeg_table_len - 4;
    for (i = 0;  i < num_strips;  i++, total_len += len)
    {
        if ((len = TIFFReadRawStrip(s->tiff.tiff_file, i, &raw_data[total_len], total_image_len - total_len)) < 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: TIFFReadRawStrip error.\n", s->tiff.file);
            span_free(raw_data);
            return -1;
        }
    }
    if (jpeg_table_len > 0)
        memcpy(raw_data, jpeg_table, jpeg_table_len - 2);

    if (total_len != total_image_len)
        span_log(&s->logging, SPAN_LOG_FLOW, "Size mismatch %d %d\n", (int) total_len, (int) total_image_len);

    s->tiff.image_size = samples_per_pixel*s->tiff.image_width*s->tiff.image_length;
    if (s->tiff.image_size >= s->tiff.image_buffer_size)
    {
        if ((t = span_realloc(s->tiff.image_buffer, s->tiff.image_size)) == NULL)
        {
            span_free(raw_data);
            return -1;
        }
        s->tiff.image_buffer_size = s->tiff.image_size;
        s->tiff.image_buffer = t;
    }

    t42_decode_init(&t42, packing_row_write_handler, &pack);

    pack.buf = s->tiff.image_buffer;
    pack.ptr = 0;
    pack.row = 0;

    t42_decode_put(&t42, raw_data, total_image_len);
    t42_decode_put(&t42, NULL, 0);

    t42_decode_release(&t42);
    span_free(raw_data);
    return s->tiff.image_size;
}
/*- End of function --------------------------------------------------------*/

static int read_tiff_decompressed_image(t4_tx_state_t *s)
{
    int total_len;
    int len;
    int num_strips;
    int i;
    uint8_t *t;

    /* Decode the whole image into a buffer */
    /* Let libtiff handle the decompression */
    s->tiff.image_size = s->tiff.image_length*TIFFScanlineSize(s->tiff.tiff_file);
    if (s->tiff.image_size >= s->tiff.image_buffer_size)
    {
        if ((t = span_realloc(s->tiff.image_buffer, s->tiff.image_size)) == NULL)
            return -1;
        s->tiff.image_buffer_size = s->tiff.image_size;
        s->tiff.image_buffer = t;
    }

    /* Allow for the image being stored in multiple strips. */
    num_strips = TIFFNumberOfStrips(s->tiff.tiff_file);
    for (i = 0, total_len = 0;  i < num_strips;  i++, total_len += len)
    {
        if ((len = TIFFReadEncodedStrip(s->tiff.tiff_file, i, &s->tiff.image_buffer[total_len], s->tiff.image_size - total_len)) < 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: TIFFReadEncodedStrip error.\n", s->tiff.file);
            return -1;
        }
    }
    /* We might need to flip all the bits, so 1 = black and 0 = white. */
    if (s->tiff.image_type == T4_IMAGE_TYPE_BILEVEL  &&  s->tiff.photo_metric != PHOTOMETRIC_MINISWHITE)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "%s: Photometric needs swapping.\n", s->tiff.file);
        for (i = 0;  i < s->tiff.image_size;  i++)
            s->tiff.image_buffer[i] = ~s->tiff.image_buffer[i];
        s->tiff.photo_metric = PHOTOMETRIC_MINISWHITE;
    }
    /* We might need to bit reverse each of the bytes of the image. */
    if (s->tiff.fill_order != FILLORDER_LSB2MSB)
        bit_reverse(s->tiff.image_buffer, s->tiff.image_buffer, s->tiff.image_size);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int read_tiff_image(t4_tx_state_t *s)
{
    int total_len;
    int i;
    int alter_image;
    uint8_t *t;

    if (s->metadata.image_type != s->tiff.image_type  ||  s->metadata.image_width != s->tiff.image_width)
    {
        /* We need to rework the image, so it can't pass directly through */
        alter_image = true;
        image_translate_restart(&s->translator, s->tiff.image_length);
        s->metadata.image_length = image_translate_get_output_length(&s->translator);
        image_translate_set_row_read_handler(&s->translator, translate_row_read2, s);
    }
    else
    {
        alter_image = false;
        s->metadata.image_length = s->tiff.image_length;
    }
    s->pack_buf = NULL;
    s->pack_ptr = 0;
    s->pack_row = 0;

    s->apply_lab = false;
    if (s->tiff.image_type != T4_IMAGE_TYPE_BILEVEL)
    {
        /* If colour/gray scale is supported we may be able to send the image as it is, perhaps after
           a resizing. Otherwise we need to resize it, and squash it to a bilevel image. */
        if (s->tiff.compression == COMPRESSION_JPEG  &&  s->tiff.photo_metric == PHOTOMETRIC_ITULAB)
        {
            if (alter_image)
            {
                if (read_tiff_t42_t81_image(s) < 0)
                    return -1;
                s->pack_buf = s->tiff.image_buffer;
            }
            else
            {
                /* Read the raw image, and send it as is */
                if (read_tiff_raw_image(s) < 0)
                    return -1;
            }
        }
#if defined(SPANDSP_SUPPORT_T43)
        else if (s->tiff.compression == COMPRESSION_T43)
        {
            if (alter_image)
            {
                if ( read_tiff_t43_image(s) < 0)
                    return -1;
                s->pack_buf = s->tiff.image_buffer;
            }
            else
            {
                /* Read the raw image, and send it as is */
                if (read_tiff_raw_image(s) < 0)
                    return -1;
            }
        }
#endif
#if defined(SPANDSP_SUPPORT_T45)
        else if (s->tiff.compression == COMPRESSION_T45)
        {
            if (alter_image)
            {
                if (read_tiff_t45_image(s) < 0)
                    return -1;
                s->pack_buf = s->tiff.image_buffer;
            }
            else
            {
                /* Read the raw image, and send it as is */
                if (read_tiff_raw_image(s) < 0)
                    return -1;
            }
        }
#endif
        else
        {
            /* Let libtiff handle the decompression */
            TIFFSetField(s->tiff.tiff_file, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
            if (alter_image)
            {
                image_translate_set_row_read_handler(&s->translator, translate_row_read, s);
            }
            else
            {
                if (read_tiff_decompressed_image(s) < 0)
                    return -1;
            }
        }

        set_image_width(s, s->metadata.image_width);
        set_image_length(s, s->metadata.image_length);
        t4_tx_set_image_type(s, s->metadata.image_type);
        if (s->metadata.image_type == T4_IMAGE_TYPE_BILEVEL)
        {
            /* We need to dither this image down to pure black and white, possibly resizing it
               along the way. */
            s->tiff.image_size = (s->metadata.image_width*s->metadata.image_length + 7)/8;
            if (s->tiff.image_size >= s->tiff.image_buffer_size)
            {
                if ((t = span_realloc(s->tiff.image_buffer, s->tiff.image_size)) == NULL)
                    return -1;
                s->tiff.image_buffer_size = s->tiff.image_size;
                s->tiff.image_buffer = t;
            }
            s->tiff.raw_row = 0;
            switch (s->tiff.photo_metric)
            {
            case PHOTOMETRIC_CIELAB:
                /* The default luminant is D50 */
                set_lab_illuminant(&s->lab_params, 96.422f, 100.000f,  82.521f);
                set_lab_gamut(&s->lab_params, 0, 100, -128, 127, -128, 127, true);
                s->apply_lab = true;
                break;
            case PHOTOMETRIC_ITULAB:
                /* The default luminant is D50 */
                set_lab_illuminant(&s->lab_params, 96.422f, 100.000f,  82.521f);
                set_lab_gamut(&s->lab_params, 0, 100, -85, 85, -75, 125, false);
                s->apply_lab = true;
                break;
            }
            total_len = 0;
            for (i = 0;  i < s->metadata.image_length;  i++)
                total_len += image_translate_row(&s->translator, &s->tiff.image_buffer[total_len], s->metadata.image_width/8);
            image_translate_release(&s->translator);
            s->row_handler = metadata_row_read_handler;
            s->row_handler_user_data = (void *) s;
        }
        else
        {
            if (alter_image)
            {
                total_len = 0;
                s->tiff.image_buffer = span_realloc(s->tiff.image_buffer, s->metadata.image_width*s->metadata.image_length*3);
                for (i = 0;  i < s->metadata.image_length;  i++)
                    total_len += image_translate_row(&s->translator, &s->tiff.image_buffer[total_len], s->metadata.image_width);
                image_translate_release(&s->translator);
                s->row_handler = metadata_row_read_handler;
                s->row_handler_user_data = (void *) s;
            }
            else
            {
                s->row_handler = tiff_row_read_handler;
                s->row_handler_user_data = (void *) s;
            }
        }
    }
    else
    {
        /* The original image is a bi-level one. We can't really rescale it, as that works out
           really poorly for a bi-level image. It has to be used in its original form. The only
           practical exception is to conver a superfine resolution image to a fine resolution one,
           or a fine image to a standard resolution one. We could pad slightly short rows or crop
           slightly long one, but lets not bother. */
        switch (s->tiff.compression)
        {
#if defined(SPANDSP_SUPPORT_T88)
        case COMPRESSION_T88:
            switch (s->metadata.compression)
            {
            case T4_COMPRESSION_T88:
                /* Read the raw image, and send it as is */
                if (read_tiff_raw_image(s) < 0)
                    return -1;
                break;
            default:
                /* libtiff probably cannot decompress T.88, so we must handle it ourselves */
                /* Decode the whole image into a buffer */
                if (read_tiff_t88_image(s) < 0)
                    return -1;
                break;
            }
            break;
#endif
        case COMPRESSION_T85:
            switch (s->metadata.compression)
            {
            case T4_COMPRESSION_T85:
            case T4_COMPRESSION_T85_L0:
                /* Read the raw image, and send it as is */
                if (read_tiff_raw_image(s) < 0)
                    return -1;
                break;
            default:
                /* libtiff probably cannot decompress T.85, so we must handle it ourselves */
                /* Decode the whole image into a buffer */
                if (read_tiff_t85_image(s) < 0)
                    return -1;
                break;
            }
            break;
#if 0
        case COMPRESSION_CCITT_T6:
            switch (s->metadata.compression)
            {
            case T4_COMPRESSION_T6:
                /* Read the raw image, and send it as is */
                if (read_tiff_raw_image(s) < 0)
                    return -1;
                break;
            default:
                /* Decode the whole image into a buffer */
                /* Let libtiff handle the decompression */
                if (read_tiff_decompressed_image(s) < 0)
                    return -1;
                break;
            }
            break;
#endif
        default:
            /* Decode the whole image into a buffer */
            /* Let libtiff handle the decompression */
            if (read_tiff_decompressed_image(s) < 0)
                return -1;
            break;
        }
    }
    s->tiff.row = 0;
    return s->metadata.image_length;
}
/*- End of function --------------------------------------------------------*/

static void tiff_tx_release(t4_tx_state_t *s)
{
    if (s->tiff.tiff_file)
    {
        TIFFClose(s->tiff.tiff_file);
        s->tiff.tiff_file = NULL;
        if (s->tiff.file)
            span_free((char *) s->tiff.file);
        s->tiff.file = NULL;
    }
    if (s->tiff.image_buffer)
    {
        span_free(s->tiff.image_buffer);
        s->tiff.image_buffer = NULL;
        s->tiff.image_size = 0;
        s->tiff.image_buffer_size = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static int set_row_read_handler(t4_tx_state_t *s, t4_row_read_handler_t handler, void *user_data)
{
    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        return t4_t6_encode_set_row_read_handler(&s->encoder.t4_t6, handler, user_data);
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        return t85_encode_set_row_read_handler(&s->encoder.t85, handler, user_data);
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        return t88_encode_set_row_read_handler(&s->encoder.t88, handler, user_data);
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        return t42_encode_set_row_read_handler(&s->encoder.t42, handler, user_data);
    case T4_COMPRESSION_T43:
        return t43_encode_set_row_read_handler(&s->encoder.t43, handler, user_data);
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        return t45_encode_set_row_read_handler(&s->encoder.t45, handler, user_data);
#endif
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int make_header(t4_tx_state_t *s)
{
    time_t now;
    struct tm tm;
    static const char *months[] =
    {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    if (s->header_text == NULL)
    {
        if ((s->header_text = span_alloc(132 + 1)) == NULL)
            return -1;
    }
    /* This is very English oriented, but then most FAX machines are, too. Some
       measure of i18n in the time and date, and even the header_info string, is
       entirely possible, although the font area would need some serious work to
       properly deal with East Asian script. There is no spec for what the header
       should contain, or how much of the page it might occupy. The present format
       follows the common practice of a few FAX machines. Nothing more. */
    time(&now);
    if (s->tz)
        tz_localtime(s->tz, &tm, now);
    else
        tm = *localtime(&now);

    snprintf(s->header_text,
             132,
             "  %2d-%s-%d  %02d:%02d    %-50s %-21s   p.%d",
             tm.tm_mday,
             months[tm.tm_mon],
             tm.tm_year + 1900,
             tm.tm_hour,
             tm.tm_min,
             (s->header_info)  ?  s->header_info  :  "",
             (s->local_ident)  ?  s->local_ident  :  "",
             s->current_page + 1);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int header_row_read_handler(void *user_data, uint8_t buf[], size_t len)
{
    int x_repeats;
    int y_repeats;
    int pattern;
    int pos;
    int row;
    int i;
    char *t;
    t4_tx_state_t *s;

    s = (t4_tx_state_t *) user_data;
    switch (s->metadata.resolution_code)
    {
    default:
    case T4_RESOLUTION_100_100:
        x_repeats = 1;
        y_repeats = 1;
        break;
    case T4_RESOLUTION_R8_STANDARD:
    case T4_RESOLUTION_200_100:
        x_repeats = 2;
        y_repeats = 1;
        break;
    case T4_RESOLUTION_R8_FINE:
    case T4_RESOLUTION_200_200:
        x_repeats = 2;
        y_repeats = 2;
        break;
    case T4_RESOLUTION_300_300:
        x_repeats = 3;
        y_repeats = 3;
        break;
    case T4_RESOLUTION_R8_SUPERFINE:
    case T4_RESOLUTION_200_400:
        x_repeats = 2;
        y_repeats = 4;
        break;
    case T4_RESOLUTION_R16_SUPERFINE:
    case T4_RESOLUTION_400_400:
        x_repeats = 4;
        y_repeats = 4;
        break;
    case T4_RESOLUTION_400_800:
        x_repeats = 4;
        y_repeats = 8;
        break;
    case T4_RESOLUTION_300_600:
        x_repeats = 3;
        y_repeats = 6;
        break;
    case T4_RESOLUTION_600_600:
        x_repeats = 6;
        y_repeats = 6;
        break;
    case T4_RESOLUTION_600_1200:
        x_repeats = 6;
        y_repeats = 12;
        break;
    case T4_RESOLUTION_1200_1200:
        x_repeats = 12;
        y_repeats = 12;
        break;
    }
    switch (s->metadata.width_code)
    {
    case T4_SUPPORT_WIDTH_215MM:
        break;
    case T4_SUPPORT_WIDTH_255MM:
        x_repeats *= 2;
        break;
    case T4_SUPPORT_WIDTH_303MM:
        x_repeats *= 3;
        break;
    }
    if (s->header_overlays_image)
    {
        /* Read and dump a row of the real image, allowing for the possibility
           that the real image might end within the header itself */
        if (len != s->row_handler(s->row_handler_user_data, buf, len))
        {
            set_row_read_handler(s, s->row_handler, s->row_handler_user_data);
            return len;
        }
    }
    t = s->header_text;
    row = s->header_row/y_repeats;
    pos = 0;
    switch (s->metadata.image_type)
    {
    case T4_IMAGE_TYPE_BILEVEL:
        for (  ;  *t  &&  pos <= len - 2;  t++)
        {
            pattern = header_font[(uint8_t) *t][row];
            buf[pos++] = (uint8_t) (pattern >> 8);
            buf[pos++] = (uint8_t) (pattern & 0xFF);
        }
        if (pos < len)
            memset(&buf[pos], 0, len - pos);
        break;
    case T4_IMAGE_TYPE_GRAY_8BIT:
        for (  ;  *t  &&  pos <= len - 2;  t++)
        {
            pattern = header_font[(uint8_t) *t][row];
            for (i = 0;  i < 16;  i++)
            {
                buf[pos + i] = (pattern & 0x8000)  ?  0  :  0xFF;
                pattern <<= 1;
            }
            pos += 16;
        }
        if (pos < len)
            memset(&buf[pos], 0xFF, len - pos);
        break;
    case T4_IMAGE_TYPE_COLOUR_8BIT:
        for (  ;  *t  &&  pos <= len - 2;  t++)
        {
            pattern = header_font[(uint8_t) *t][row];
            for (i = 0;  i < 16;  i++)
            {
                buf[pos + 3*i + 0] =
                buf[pos + 3*i + 1] =
                buf[pos + 3*i + 2] = (pattern & 0x8000)  ?  0  :  0xFF;
                pattern <<= 1;
            }
            pos += 3*16;
        }
        if (pos < len)
            memset(&buf[pos], 0xFF, len - pos);
        break;
    case T4_IMAGE_TYPE_COLOUR_BILEVEL:
    case T4_IMAGE_TYPE_4COLOUR_BILEVEL:
    case T4_IMAGE_TYPE_GRAY_12BIT:
    case T4_IMAGE_TYPE_4COLOUR_8BIT:
    case T4_IMAGE_TYPE_COLOUR_12BIT:
    case T4_IMAGE_TYPE_4COLOUR_12BIT:
    default:
        memset(buf, 0xFF, len);
    }
    s->header_row++;
    if (s->header_row >= 16*y_repeats)
    {
        /* End of header. Change to normal image row data. */
        set_row_read_handler(s, s->row_handler, s->row_handler_user_data);
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_next_page_has_different_format(t4_tx_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Checking for the existence of page %d\n", s->current_page + 1);
    if (s->current_page >= s->stop_page)
        return -1;
    if (s->tiff.file)
    {
        if (!TIFFSetDirectory(s->tiff.tiff_file, (tdir_t) s->current_page + 1))
            return -1;
        return test_tiff_directory_info(s);
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_set_row_read_handler(t4_tx_state_t *s, t4_row_read_handler_t handler, void *user_data)
{
    s->row_handler = handler;
    s->row_handler_user_data = user_data;
    return set_row_read_handler(s, handler, user_data);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_set_tx_image_format(t4_tx_state_t *s,
                                            int supported_compressions,
                                            int supported_image_sizes,
                                            int supported_bilevel_resolutions,
                                            int supported_colour_resolutions)
{
    static const struct
    {
        int width;
        int width_code;
        int res_code;           /* Correct resolution code */
        int alt_res_code;       /* Fallback resolution code, where a metric/inch swap is possible */
    } width_and_res_info[] =
    {
        { T4_WIDTH_100_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_100_100,                           0},
        { T4_WIDTH_100_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_100_100,                           0},
        { T4_WIDTH_100_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_100_100,                           0},
        { T4_WIDTH_200_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_200_100,   T4_RESOLUTION_R8_STANDARD},
        { T4_WIDTH_200_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_200_200,       T4_RESOLUTION_R8_FINE},
        { T4_WIDTH_200_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_200_400,  T4_RESOLUTION_R8_SUPERFINE},
        { T4_WIDTH_200_A4, T4_SUPPORT_WIDTH_215MM,   T4_RESOLUTION_R8_STANDARD,       T4_RESOLUTION_200_100},
        { T4_WIDTH_200_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_R8_FINE,       T4_RESOLUTION_200_200},
        { T4_WIDTH_200_A4, T4_SUPPORT_WIDTH_215MM,  T4_RESOLUTION_R8_SUPERFINE,       T4_RESOLUTION_200_400},
        { T4_WIDTH_200_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_200_100,   T4_RESOLUTION_R8_STANDARD},
        { T4_WIDTH_200_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_200_200,       T4_RESOLUTION_R8_FINE},
        { T4_WIDTH_200_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_200_400,  T4_RESOLUTION_R8_SUPERFINE},
        { T4_WIDTH_200_B4, T4_SUPPORT_WIDTH_255MM,   T4_RESOLUTION_R8_STANDARD,       T4_RESOLUTION_200_100},
        { T4_WIDTH_200_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_R8_FINE,       T4_RESOLUTION_200_200},
        { T4_WIDTH_200_B4, T4_SUPPORT_WIDTH_255MM,  T4_RESOLUTION_R8_SUPERFINE,       T4_RESOLUTION_200_400},
        { T4_WIDTH_200_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_200_100,   T4_RESOLUTION_R8_STANDARD},
        { T4_WIDTH_200_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_200_200,       T4_RESOLUTION_R8_FINE},
        { T4_WIDTH_200_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_200_400,  T4_RESOLUTION_R8_SUPERFINE},
        { T4_WIDTH_200_A3, T4_SUPPORT_WIDTH_303MM,   T4_RESOLUTION_R8_STANDARD,       T4_RESOLUTION_200_100},
        { T4_WIDTH_200_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_R8_FINE,       T4_RESOLUTION_200_200},
        { T4_WIDTH_200_A3, T4_SUPPORT_WIDTH_303MM,  T4_RESOLUTION_R8_SUPERFINE,       T4_RESOLUTION_200_400},
        { T4_WIDTH_300_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_300_300,                           0},
        { T4_WIDTH_300_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_300_600,                           0},
        { T4_WIDTH_300_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_300_300,                           0},
        { T4_WIDTH_300_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_300_600,                           0},
        { T4_WIDTH_400_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_400_400, T4_RESOLUTION_R16_SUPERFINE},
        { T4_WIDTH_400_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_400_800,                           0},
        { T4_WIDTH_400_A4, T4_SUPPORT_WIDTH_215MM, T4_RESOLUTION_R16_SUPERFINE,       T4_RESOLUTION_400_400},
        { T4_WIDTH_300_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_300_300,                           0},
        { T4_WIDTH_300_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_300_600,                           0},
        { T4_WIDTH_400_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_400_400, T4_RESOLUTION_R16_SUPERFINE},
        { T4_WIDTH_400_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_400_800,                           0},
        { T4_WIDTH_400_B4, T4_SUPPORT_WIDTH_255MM, T4_RESOLUTION_R16_SUPERFINE,       T4_RESOLUTION_400_400},
        { T4_WIDTH_400_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_400_400, T4_RESOLUTION_R16_SUPERFINE},
        { T4_WIDTH_400_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_400_800,                           0},
        { T4_WIDTH_400_A3, T4_SUPPORT_WIDTH_303MM, T4_RESOLUTION_R16_SUPERFINE,       T4_RESOLUTION_400_400},
        { T4_WIDTH_600_A4, T4_SUPPORT_WIDTH_215MM,       T4_RESOLUTION_600_600,                           0},
        { T4_WIDTH_600_A4, T4_SUPPORT_WIDTH_215MM,      T4_RESOLUTION_600_1200,                           0},
        { T4_WIDTH_600_B4, T4_SUPPORT_WIDTH_255MM,       T4_RESOLUTION_600_600,                           0},
        { T4_WIDTH_600_B4, T4_SUPPORT_WIDTH_255MM,      T4_RESOLUTION_600_1200,                           0},
        { T4_WIDTH_600_A3, T4_SUPPORT_WIDTH_303MM,       T4_RESOLUTION_600_600,                           0},
        { T4_WIDTH_600_A3, T4_SUPPORT_WIDTH_303MM,      T4_RESOLUTION_600_1200,                           0},
        {T4_WIDTH_1200_A4, T4_SUPPORT_WIDTH_215MM,     T4_RESOLUTION_1200_1200,                           0},
        {T4_WIDTH_1200_B4, T4_SUPPORT_WIDTH_255MM,     T4_RESOLUTION_1200_1200,                           0},
        {T4_WIDTH_1200_A3, T4_SUPPORT_WIDTH_303MM,     T4_RESOLUTION_1200_1200,                           0},
        {0x7FFFFFFF, -1, -1, -1}
    };

    static const struct
    {
        int resolution;
        struct
        {
            int resolution;
            int squashing_factor;
        } fallback[4];
    } squashable[4] =
    {
        {
            T4_RESOLUTION_200_400,
            {
                {T4_RESOLUTION_200_200,     2},
                {T4_RESOLUTION_R8_FINE,     2},
                {T4_RESOLUTION_200_100,     4},
                {T4_RESOLUTION_R8_STANDARD, 4}
            }
        },
        {
            T4_RESOLUTION_200_200,
            {
                {T4_RESOLUTION_200_100,     2},
                {T4_RESOLUTION_R8_STANDARD, 2},
                {0,                         0},
                {0,                         0}
            }
        },
        {
            T4_RESOLUTION_R8_SUPERFINE,
            {
                {T4_RESOLUTION_R8_FINE,     2},
                {T4_RESOLUTION_200_200,     2},
                {T4_RESOLUTION_R8_STANDARD, 4},
                {T4_RESOLUTION_200_100,     4}
            }
        },
        {
            T4_RESOLUTION_R8_FINE,
            {
                {T4_RESOLUTION_R8_STANDARD, 2},
                {T4_RESOLUTION_200_100,     2},
                {0,                         0},
                {0,                         0}
            }
        }
    };
    
    int i;
    int j;
    int entry;
    int compression;
    int res;
    int supported_colour_compressions;

    supported_colour_compressions = supported_compressions & (T4_COMPRESSION_T42_T81 | T4_COMPRESSION_T43 | T4_COMPRESSION_T45 | T4_COMPRESSION_SYCC_T81);
    compression = -1;
    s->metadata.image_type = s->tiff.image_type;
    if (s->tiff.image_type != T4_IMAGE_TYPE_BILEVEL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Non-bi-level image\n");
        /* Can we send this page as it is? */
        if (supported_colour_resolutions
            &&
            supported_colour_compressions
            &&
                  (((s->tiff.image_type == T4_IMAGE_TYPE_COLOUR_BILEVEL  ||  s->tiff.image_type == T4_IMAGE_TYPE_COLOUR_8BIT  ||  s->tiff.image_type == T4_IMAGE_TYPE_COLOUR_12BIT)
                    &&
                    (supported_compressions & T4_COMPRESSION_COLOUR))
                ||
                   ((s->tiff.image_type == T4_IMAGE_TYPE_GRAY_8BIT  ||  s->tiff.image_type == T4_IMAGE_TYPE_GRAY_12BIT)
                    &&
                    (supported_compressions & T4_COMPRESSION_GRAYSCALE))))
        {
            /* Gray-scale/colour is possible */
            span_log(&s->logging, SPAN_LOG_FLOW, "Gray-scale/colour is allowed\n");
            /* Choose the best gray-scale/colour encoding available to us */
            if (s->tiff.image_type == T4_IMAGE_TYPE_COLOUR_BILEVEL  &&  (supported_compressions & T4_COMPRESSION_T43))
                compression = T4_COMPRESSION_T43;
            else if ((supported_compressions & T4_COMPRESSION_T42_T81))
                compression = T4_COMPRESSION_T42_T81;
            else if ((supported_compressions & T4_COMPRESSION_T43))
                compression = T4_COMPRESSION_T43;
            else if ((supported_compressions & T4_COMPRESSION_T45))
                compression = T4_COMPRESSION_T45;
            else if ((supported_compressions & T4_COMPRESSION_SYCC_T81))
                compression = T4_COMPRESSION_SYCC_T81;
 
            //best_colour_resolution(s->tiff.x_resolution, supported_colour_resolutions);
        }
        else 
        {
            /* Gray-scale/colour is not possible. Can we flatten the image to send it? */
            span_log(&s->logging, SPAN_LOG_FLOW, "Gray-scale/colour is not allowed\n");
            switch (s->tiff.image_type)
            {
            case T4_IMAGE_TYPE_COLOUR_BILEVEL:
            case T4_IMAGE_TYPE_COLOUR_8BIT:
            case T4_IMAGE_TYPE_COLOUR_12BIT:
                if (!(supported_compressions & T4_COMPRESSION_COLOUR_TO_BILEVEL))
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Flattening is not allowed\n");
                    return T4_IMAGE_FORMAT_INCOMPATIBLE;
                }
                break;
            case T4_IMAGE_TYPE_GRAY_8BIT:
            case T4_IMAGE_TYPE_GRAY_12BIT:
                if (!(supported_compressions & T4_COMPRESSION_GRAY_TO_BILEVEL))
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Flattening is not allowed\n");
                    return T4_IMAGE_FORMAT_INCOMPATIBLE;
                }
                break;
            }
            /* Squashing to a bi-level image is possible */
            s->metadata.image_type = T4_IMAGE_TYPE_BILEVEL;
            span_log(&s->logging, SPAN_LOG_FLOW, "The image will be flattened to %s\n", t4_image_type_to_str(s->metadata.image_type));
        }
    }

    if (s->metadata.image_type == T4_IMAGE_TYPE_BILEVEL)
    {
        /* Choose the best bi-level encoding available to us */
        if ((supported_compressions & T4_COMPRESSION_T85_L0))
            compression = T4_COMPRESSION_T85_L0;
        else if ((supported_compressions & T4_COMPRESSION_T85))
            compression = T4_COMPRESSION_T85;
        else if ((supported_compressions & T4_COMPRESSION_T6))
            compression = T4_COMPRESSION_T6;
        else if ((supported_compressions & T4_COMPRESSION_T4_2D))
            compression = T4_COMPRESSION_T4_2D;
        else
            compression = T4_COMPRESSION_T4_1D;
    }

    /* Deal with the image width/resolution combination. */
    /* Look for a pattern that matches the image */
    s->metadata.width_code = -1;
    for (entry = 0;  s->tiff.image_width >= width_and_res_info[entry].width;  entry++)
    {
        if (s->tiff.image_width == width_and_res_info[entry].width  &&  s->tiff.resolution_code == width_and_res_info[entry].res_code)
        {
            s->metadata.width_code = width_and_res_info[entry].width_code;
            break;
        }
    }
    res = T4_IMAGE_FORMAT_NOSIZESUPPORT;
    s->row_squashing_ratio = 1;
    if (s->metadata.width_code >= 0  &&  (supported_image_sizes & s->metadata.width_code))
    {
        /* We have a valid and supported width/resolution combination */

        /* No resize necessary */
        s->metadata.image_width = s->tiff.image_width;
        s->metadata.image_length = s->tiff.image_length;

        res = T4_IMAGE_FORMAT_NORESSUPPORT;
        if (s->metadata.image_type == T4_IMAGE_TYPE_BILEVEL)
        {
            if ((width_and_res_info[entry].res_code & supported_bilevel_resolutions))
            {
                /* We can use the resolution of the original image */
                s->metadata.resolution_code = s->tiff.resolution_code;
                s->metadata.x_resolution = code_to_x_resolution(s->metadata.resolution_code);
                s->metadata.y_resolution = code_to_y_resolution(s->metadata.resolution_code);
                res = T4_IMAGE_FORMAT_OK;
            }
            else if ((width_and_res_info[entry].alt_res_code & supported_bilevel_resolutions))
            {
                /* We can do a metric/imperial swap, and have a usable resolution */
                span_log(&s->logging,
                         SPAN_LOG_FLOW,
                         "Image resolution %s falls back to %s\n",
                         t4_image_resolution_to_str(s->tiff.resolution_code),
                         t4_image_resolution_to_str(width_and_res_info[entry].alt_res_code));
                s->metadata.resolution_code = width_and_res_info[entry].alt_res_code;
                s->metadata.x_resolution = code_to_x_resolution(s->metadata.resolution_code);
                s->metadata.y_resolution = code_to_y_resolution(s->metadata.resolution_code);
                res = T4_IMAGE_FORMAT_OK;
            }
            else
            {
                if (s->tiff.image_type == T4_IMAGE_TYPE_BILEVEL)
                {
                    if ((s->tiff.resolution_code & (T4_RESOLUTION_200_400 | T4_RESOLUTION_200_200 | T4_RESOLUTION_R8_SUPERFINE | T4_RESOLUTION_R8_FINE)))
                    {
                        /* This might be a resolution we can squash down to something which is supported */
                        for (i = 0;  i < 4;  i++)
                        {
                            if ((s->tiff.resolution_code & squashable[i].resolution))
                                break;
                        }
                        if (i < 4)
                        {
                            /* This is a squashable resolution, so let's see if there is a valid
                               fallback we can squash the image to, scanning through the entries
                               in their order of preference. */
                            for (j = 0;  j < 4;  j++)
                            {
                                if ((supported_bilevel_resolutions & squashable[i].fallback[j].resolution))
                                {
                                    span_log(&s->logging,
                                             SPAN_LOG_FLOW,
                                             "Image resolution %s falls back to %s\n",
                                             t4_image_resolution_to_str(s->tiff.resolution_code),
                                             t4_image_resolution_to_str(squashable[i].fallback[j].resolution));
                                    s->row_squashing_ratio = squashable[i].fallback[j].squashing_factor;
                                    s->metadata.resolution_code = squashable[i].fallback[j].resolution;
                                    s->metadata.x_resolution = code_to_x_resolution(s->metadata.resolution_code);
                                    s->metadata.y_resolution = code_to_y_resolution(s->metadata.resolution_code);
                                    res = T4_IMAGE_FORMAT_OK;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            /* If we have not succeeded in matching up the size and resolution, the next step will
               depend on whether the original was a bi-level image. If it was, we are stuck, as you can't
               really resize those. If it was not, a resize might be possible */
            if (res != T4_IMAGE_FORMAT_OK)
            {
                if (s->tiff.image_type == T4_IMAGE_TYPE_BILEVEL)
                    return T4_IMAGE_FORMAT_NORESSUPPORT;
                if (!(supported_compressions & T4_COMPRESSION_RESCALING))
                    return T4_IMAGE_FORMAT_NOSIZESUPPORT;
            }
            /* TODO */
        }
        else
        {
            if ((width_and_res_info[entry].res_code & supported_bilevel_resolutions))
            {
                if ((s->tiff.resolution_code & supported_colour_resolutions))
                {
                    /* We can use the resolution of the original image */
                    s->metadata.resolution_code = width_and_res_info[entry].res_code;
                    s->metadata.x_resolution = code_to_x_resolution(s->metadata.resolution_code);
                    s->metadata.y_resolution = code_to_y_resolution(s->metadata.resolution_code);
                    res = T4_IMAGE_FORMAT_OK;
                }
            }
        }
    }
    else
    {
        /* Can we rework the image to fit? */
        /* We can't rework a bilevel image that fits none of the patterns */
        if (s->tiff.image_type == T4_IMAGE_TYPE_BILEVEL  ||  s->tiff.image_type == T4_IMAGE_TYPE_COLOUR_BILEVEL)
            return T4_IMAGE_FORMAT_NORESSUPPORT;
        if (!(supported_compressions & T4_COMPRESSION_RESCALING))
            return T4_IMAGE_FORMAT_NOSIZESUPPORT;
        /* Any other kind of image might be resizable */
        res = T4_IMAGE_FORMAT_OK;
        /* TODO: use more sophisticated resizing */
        s->metadata.image_width = T4_WIDTH_200_A4;
        s->metadata.resolution_code = T4_RESOLUTION_200_200;
        s->metadata.x_resolution = code_to_x_resolution(s->metadata.resolution_code);
        s->metadata.y_resolution = code_to_y_resolution(s->metadata.resolution_code);
    }

    if (res != T4_IMAGE_FORMAT_OK)
        return res;

    if (s->metadata.image_type != s->tiff.image_type  ||  s->metadata.image_width != s->tiff.image_width)
    {
        if (image_translate_init(&s->translator,
                                 s->metadata.image_type,
                                 s->metadata.image_width,
                                 -1,
                                 s->tiff.image_type,
                                 s->tiff.image_width,
                                 s->tiff.image_length,
                                 translate_row_read2,
                                 s) == NULL)
        {
            return T4_IMAGE_FORMAT_INCOMPATIBLE;
        }
        s->metadata.image_length = image_translate_get_output_length(&s->translator);
    }

    if (compression != s->metadata.compression)
    {
        switch (compression)
        {
        case T4_COMPRESSION_T4_1D:
        case T4_COMPRESSION_T4_2D:
        case T4_COMPRESSION_T6:
            switch (s->metadata.compression)
            {
            case T4_COMPRESSION_T4_1D:
            case T4_COMPRESSION_T4_2D:
            case T4_COMPRESSION_T6:
                break;
            default:
                t4_t6_encode_init(&s->encoder.t4_t6, compression, s->metadata.image_width, s->metadata.image_length, s->row_handler, s->row_handler_user_data);
                break;
            }
            s->metadata.compression = compression;
            res = T4_IMAGE_FORMAT_OK;
            if (t4_t6_encode_set_encoding(&s->encoder.t4_t6, compression))
                res = -1;
            break;
        case T4_COMPRESSION_T85:
        case T4_COMPRESSION_T85_L0:
            switch (s->metadata.compression)
            {
            case T4_COMPRESSION_T85:
            case T4_COMPRESSION_T85_L0:
                break;
            default:
                t85_encode_init(&s->encoder.t85, s->metadata.image_width, s->metadata.image_length, s->row_handler, s->row_handler_user_data);
                break;
            }
            s->metadata.compression = compression;
            res = T4_IMAGE_FORMAT_OK;
            break;
#if defined(SPANDSP_SUPPORT_T88)
        case T4_COMPRESSION_T88:
            switch (s->metadata.compression)
            {
            case T4_COMPRESSION_T88:
                break;
            default:
                t88_encode_init(&s->encoder.t88, s->metadata.image_width, s->metadata.image_length, s->row_handler, s->row_handler_user_data);
                break;
            }
            s->metadata.compression = compression;
            res = T4_IMAGE_FORMAT_OK;
            break;
#endif
        case T4_COMPRESSION_T42_T81:
        case T4_COMPRESSION_SYCC_T81:
            switch (s->metadata.compression)
            {
            case T4_COMPRESSION_T42_T81:
            case T4_COMPRESSION_SYCC_T81:
                break;
            default:
                t42_encode_init(&s->encoder.t42, s->metadata.image_width, s->metadata.image_length, s->row_handler, s->row_handler_user_data);
                break;
            }
            s->metadata.compression = compression;
            res = T4_IMAGE_FORMAT_OK;
            break;
        case T4_COMPRESSION_T43:
            switch (s->metadata.compression)
            {
            case T4_COMPRESSION_T43:
                break;
            default:
                t43_encode_init(&s->encoder.t43, s->metadata.image_width, s->metadata.image_length, s->row_handler, s->row_handler_user_data);
                break;
            }
            s->metadata.compression = compression;
            res = T4_IMAGE_FORMAT_OK;
            break;
#if defined(SPANDSP_SUPPORT_T45)
        case T4_COMPRESSION_T45:
            switch (s->metadata.compression)
            {
            case T4_COMPRESSION_T45:
                break;
            default:
                t45_encode_init(&s->encoder.t45, s->metadata.image_width, s->metadata.image_length, s->row_handler, s->row_handler_user_data);
                break;
            }
            s->metadata.compression = compression;
            res = T4_IMAGE_FORMAT_OK;
            break;
#endif
        }
    }

    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t4_t6_encode_set_max_2d_rows_per_1d_row(&s->encoder.t4_t6, -s->metadata.y_resolution);
        break;
    }

    set_image_width(s, s->metadata.image_width);
    set_image_length(s, s->metadata.image_length);
    t4_tx_set_image_type(s, s->metadata.image_type);
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_tx_compression(t4_tx_state_t *s)
{
    return s->metadata.compression;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_tx_image_type(t4_tx_state_t *s)
{
    return s->metadata.image_type;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_tx_resolution(t4_tx_state_t *s)
{
    return s->metadata.resolution_code;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_tx_x_resolution(t4_tx_state_t *s)
{
    return s->metadata.x_resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_tx_y_resolution(t4_tx_state_t *s)
{
    return s->metadata.y_resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_tx_image_width(t4_tx_state_t *s)
{
    return s->metadata.image_width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_tx_image_width_code(t4_tx_state_t *s)
{
    return s->metadata.width_code;
}
/*- End of function --------------------------------------------------------*/

static void set_image_width(t4_tx_state_t *s, uint32_t image_width)
{
    s->metadata.image_width = image_width;
    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t4_t6_encode_set_image_width(&s->encoder.t4_t6, image_width);
        break;
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        t85_encode_set_image_width(&s->encoder.t85, image_width);
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        t88_encode_set_image_width(&s->encoder.t88, image_width);
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        t42_encode_set_image_width(&s->encoder.t42, image_width);
        break;
    case T4_COMPRESSION_T43:
        t43_encode_set_image_width(&s->encoder.t43, image_width);
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        t45_encode_set_image_width(&s->encoder.t45, image_width);
        break;
#endif
    }
}
/*- End of function --------------------------------------------------------*/

static void set_image_length(t4_tx_state_t *s, uint32_t image_length)
{
    s->metadata.image_length = image_length;
    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t4_t6_encode_set_image_length(&s->encoder.t4_t6, image_length);
        break;
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        t85_encode_set_image_length(&s->encoder.t85, image_length);
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        t88_encode_set_image_length(&s->encoder.t88, image_length);
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        t42_encode_set_image_length(&s->encoder.t42, image_length);
        break;
    case T4_COMPRESSION_T43:
        t43_encode_set_image_length(&s->encoder.t43, image_length);
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        t45_encode_set_image_length(&s->encoder.t45, image_length);
        break;
#endif
    }
}
/*- End of function --------------------------------------------------------*/

static void t4_tx_set_image_type(t4_tx_state_t *s, int image_type)
{
    s->metadata.image_type = image_type;
    switch (s->metadata.compression)
    {
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        t88_encode_set_image_type(&s->encoder.t88, image_type);
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        t42_encode_set_image_type(&s->encoder.t42, image_type);
        break;
    case T4_COMPRESSION_T43:
        t43_encode_set_image_type(&s->encoder.t43, image_type);
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        t45_encode_set_image_type(&s->encoder.t45, image_type);
        break;
#endif
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_min_bits_per_row(t4_tx_state_t *s, int bits)
{
    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t4_t6_encode_set_min_bits_per_row(&s->encoder.t4_t6, bits);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_max_2d_rows_per_1d_row(t4_tx_state_t *s, int max)
{
    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t4_t6_encode_set_max_2d_rows_per_1d_row(&s->encoder.t4_t6, max);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_header_overlays_image(t4_tx_state_t *s, bool header_overlays_image)
{
    s->header_overlays_image = header_overlays_image;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_local_ident(t4_tx_state_t *s, const char *ident)
{
    s->local_ident = (ident  &&  ident[0])  ?  ident  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_header_info(t4_tx_state_t *s, const char *info)
{
    s->header_info = (info  &&  info[0])  ?  info  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_header_tz(t4_tx_state_t *s, struct tz_s *tz)
{
    s->tz = tz;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_pages_in_file(t4_tx_state_t *s)
{
    int max;

    if (s->tiff.file)
        max = get_tiff_total_pages(s);
    else
        max = 1;
    if (max >= 0)
        s->tiff.pages_in_file = max;
    return max;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_current_page_in_file(t4_tx_state_t *s)
{
    return s->current_page;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_get_transfer_statistics(t4_tx_state_t *s, t4_stats_t *t)
{
    memset(t, 0, sizeof(*t));
    t->pages_transferred = s->current_page - s->start_page;
    t->pages_in_file = s->tiff.pages_in_file;

    t->image_type = s->tiff.image_type;
    t->image_width = s->tiff.image_width;
    t->image_length = s->tiff.image_length;

    t->image_x_resolution = s->tiff.x_resolution;
    t->image_y_resolution = s->tiff.y_resolution;
    t->x_resolution = s->metadata.x_resolution;
    t->y_resolution = s->metadata.y_resolution;

    t->type = s->metadata.image_type;
    t->compression = s->metadata.compression;

    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t->width = t4_t6_encode_get_image_width(&s->encoder.t4_t6);
        t->length = t4_t6_encode_get_image_length(&s->encoder.t4_t6);
        t->line_image_size = t4_t6_encode_get_compressed_image_size(&s->encoder.t4_t6)/8;
        break;
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        t->width = t85_encode_get_image_width(&s->encoder.t85);
        t->length = t85_encode_get_image_length(&s->encoder.t85);
        t->line_image_size = t85_encode_get_compressed_image_size(&s->encoder.t85)/8;
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        t->width = t88_encode_get_image_width(&s->encoder.t88);
        t->length = t88_encode_get_image_length(&s->encoder.t88);
        t->line_image_size = t88_encode_get_compressed_image_size(&s->encoder.t88)/8;
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        t->width = t42_encode_get_image_width(&s->encoder.t42);
        t->length = t42_encode_get_image_length(&s->encoder.t42);
        t->line_image_size = t42_encode_get_compressed_image_size(&s->encoder.t42)/8;
        break;
    case T4_COMPRESSION_T43:
        t->width = t43_encode_get_image_width(&s->encoder.t43);
        t->length = t43_encode_get_image_length(&s->encoder.t43);
        t->line_image_size = t43_encode_get_compressed_image_size(&s->encoder.t43)/8;
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        t->width = t45_encode_get_image_width(&s->encoder.t45);
        t->length = t45_encode_get_image_length(&s->encoder.t45);
        t->line_image_size = t45_encode_get_compressed_image_size(&s->encoder.t45)/8;
        break;
#endif
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_image_complete(t4_tx_state_t *s)
{
    if (s->no_encoder.buf_len > 0)
    {
        if (s->no_encoder.buf_ptr >= s->no_encoder.buf_len)
            return SIG_STATUS_END_OF_DATA;
        return 0;
    }

    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        return t4_t6_encode_image_complete(&s->encoder.t4_t6);
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        return t85_encode_image_complete(&s->encoder.t85);
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        return t88_encode_image_complete(&s->encoder.t88);
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        return t42_encode_image_complete(&s->encoder.t42);
    case T4_COMPRESSION_T43:
        return t43_encode_image_complete(&s->encoder.t43);
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        return t45_encode_image_complete(&s->encoder.t45);
#endif
    }
    return SIG_STATUS_END_OF_DATA;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_bit(t4_tx_state_t *s)
{
    int bit;

    /* We only get bit by bit for T.4 1D and T.4 2-D. */
    if (s->no_encoder.buf_len > 0)
    {
        if (s->no_encoder.buf_ptr >= s->no_encoder.buf_len)
            return SIG_STATUS_END_OF_DATA;
        bit = (s->no_encoder.buf[s->no_encoder.buf_ptr] >> s->no_encoder.bit) & 1;
        if (++s->no_encoder.bit >= 8)
        {
            s->no_encoder.bit = 0;
            s->no_encoder.buf_ptr++;
        }
        return bit;
    }
    return t4_t6_encode_get_bit(&s->encoder.t4_t6);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get(t4_tx_state_t *s, uint8_t buf[], size_t max_len)
{
    if (s->no_encoder.buf_len > 0)
    {
        if (max_len > (s->no_encoder.buf_len - s->no_encoder.buf_ptr))
            max_len = s->no_encoder.buf_len - s->no_encoder.buf_ptr;
        memcpy(buf, &s->no_encoder.buf[s->no_encoder.buf_ptr], max_len);
        s->no_encoder.buf_ptr += max_len;
        return max_len;
    }

    if (s->image_get_handler)
        return s->image_get_handler((void *) &s->encoder, buf, max_len);

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_start_page(t4_tx_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Start tx page %d - compression %s\n", s->current_page, t4_compression_to_str(s->metadata.compression));
    if (s->current_page > s->stop_page)
        return -1;
    if (s->tiff.file)
    {
        if (!TIFFSetDirectory(s->tiff.tiff_file, (tdir_t) s->current_page))
            return -1;
        get_tiff_directory_info(s);
        if (read_tiff_image(s) < 0)
            return -1;
    }
    else
    {
        s->metadata.image_length = UINT32_MAX;
    }

    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t4_t6_encode_restart(&s->encoder.t4_t6, s->metadata.image_width, s->metadata.image_length);
        s->image_get_handler = (t4_image_get_handler_t) t4_t6_encode_get;
        break;
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        t85_encode_restart(&s->encoder.t85, s->metadata.image_width, s->metadata.image_length);
        s->image_get_handler = (t4_image_get_handler_t) t85_encode_get;
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        t88_encode_restart(&s->encoder.t88, s->metadata.image_width, s->metadata.image_length);
        s->image_get_handler = (t4_image_get_handler_t) t88_encode_get;
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        t42_encode_restart(&s->encoder.t42, s->metadata.image_width, s->metadata.image_length);
        s->image_get_handler = (t4_image_get_handler_t) t42_encode_get;
        break;
    case T4_COMPRESSION_T43:
        t43_encode_restart(&s->encoder.t43, s->metadata.image_width, s->metadata.image_length);
        s->image_get_handler = (t4_image_get_handler_t) t43_encode_get;
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        t45_encode_restart(&s->encoder.t45, s->metadata.image_width, s->metadata.image_length);
        s->image_get_handler = (t4_image_get_handler_t) t45_encode_get;
        break;
#endif
    default:
        s->image_get_handler = NULL;
        break;
    }

    /* If there is a page header, create that first */
    //if (s->metadata.image_type == T4_IMAGE_TYPE_BILEVEL  &&  s->header_info  &&  s->header_info[0]  &&  make_header(s) == 0)
    if (s->header_info  &&  s->header_info[0]  &&  make_header(s) == 0)
    {
        s->header_row = 0;
        set_row_read_handler(s, header_row_read_handler, (void *) s);
    }
    else
    {
        set_row_read_handler(s, s->row_handler, s->row_handler_user_data);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_restart_page(t4_tx_state_t *s)
{
    /* This is currently the same as starting a page, but keep it a separate call,
       as the two things might diverge a little in the future. */
    return t4_tx_start_page(s);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_end_page(t4_tx_state_t *s)
{
    s->current_page++;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t4_tx_get_logging_state(t4_tx_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t4_tx_state_t *) t4_tx_init(t4_tx_state_t *s, const char *file, int start_page, int stop_page)
{
    int allocated;

    allocated = false;
    if (s == NULL)
    {
        if ((s = (t4_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        allocated = true;
    }
    memset(s, 0, sizeof(*s));
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    TIFF_FX_init();
#endif
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.4");

    span_log(&s->logging, SPAN_LOG_FLOW, "Start tx document\n");

    s->current_page =
    s->start_page = (start_page >= 0)  ?  start_page  :  0;
    s->stop_page = (stop_page >= 0)  ?  stop_page  :  INT_MAX;
    s->metadata.compression = T4_COMPRESSION_NONE;

    s->row_handler = tiff_row_read_handler;
    s->row_handler_user_data = (void *) s;

    s->row_squashing_ratio = 1;

    if (file)
    {
        if (open_tiff_input_file(s, file) < 0)
        {
            if (allocated)
                span_free(s);
            return NULL;
        }
        s->tiff.file = strdup(file);
        s->tiff.pages_in_file = -1;
        if (!TIFFSetDirectory(s->tiff.tiff_file, (tdir_t) s->current_page)
            ||
            get_tiff_directory_info(s))
        {
            tiff_tx_release(s);
            if (allocated)
                span_free(s);
            return NULL;
        }
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_release(t4_tx_state_t *s)
{
    if (s->tiff.file)
        tiff_tx_release(s);
    if (s->header_text)
    {
        span_free(s->header_text);
        s->header_text = NULL;
    }
    if (s->colour_map)
    {
        span_free(s->colour_map);
        s->colour_map = NULL;
    }
    switch (s->metadata.compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        return t4_t6_encode_release(&s->encoder.t4_t6);
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        return t85_encode_release(&s->encoder.t85);
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        return t88_encode_release(&s->encoder.t88);
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        return t42_encode_release(&s->encoder.t42);
    case T4_COMPRESSION_T43:
        return t43_encode_release(&s->encoder.t43);
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        return t45_encode_release(&s->encoder.t45);
#endif
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_free(t4_tx_state_t *s)
{
    int ret;

    ret = t4_tx_release(s);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
