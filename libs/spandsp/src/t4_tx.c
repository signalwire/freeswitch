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

#include "spandsp/telephony.h"
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
#if defined(SPANDSP_SUPPORT_T43)
#include "spandsp/t43.h"
#endif
#include "spandsp/t4_t6_decode.h"
#include "spandsp/t4_t6_encode.h"

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
    int bit_mask;
} packer_t;

typedef struct
{
    float resolution;
    int code;
} res_table_t;

static void t4_tx_set_image_length(t4_tx_state_t *s, int image_length);

static const res_table_t x_res_table[] =
{
    { 100.0f/CM_PER_INCH, T4_X_RESOLUTION_100},
    { 102.0f/CM_PER_INCH, T4_X_RESOLUTION_R4},
    { 200.0f/CM_PER_INCH, T4_X_RESOLUTION_200},
    { 204.0f/CM_PER_INCH, T4_X_RESOLUTION_R8},
    { 300.0f/CM_PER_INCH, T4_X_RESOLUTION_300},
    { 400.0f/CM_PER_INCH, T4_X_RESOLUTION_400},
    { 408.0f/CM_PER_INCH, T4_X_RESOLUTION_R16},
    { 600.0f/CM_PER_INCH, T4_X_RESOLUTION_600},
    { 800.0f/CM_PER_INCH, T4_X_RESOLUTION_800},
    {1200.0f/CM_PER_INCH, T4_X_RESOLUTION_1200},
    {             -1.00f, -1}
};

static const res_table_t y_res_table[] =
{
    {             38.50f, T4_Y_RESOLUTION_STANDARD},
    { 100.0f/CM_PER_INCH, T4_Y_RESOLUTION_100},
    {             77.00f, T4_Y_RESOLUTION_FINE},
    { 200.0f/CM_PER_INCH, T4_Y_RESOLUTION_200},
    { 300.0f/CM_PER_INCH, T4_Y_RESOLUTION_300},
    {            154.00f, T4_Y_RESOLUTION_SUPERFINE},
    { 400.0f/CM_PER_INCH, T4_Y_RESOLUTION_400},
    { 600.0f/CM_PER_INCH, T4_Y_RESOLUTION_600},
    { 800.0f/CM_PER_INCH, T4_Y_RESOLUTION_800},
    {1200.0f/CM_PER_INCH, T4_Y_RESOLUTION_1200},
    {             -1.00f, -1}
};

static const int resolution_map[10][10] =
{
    {                    0, 0,                     0,  T4_RESOLUTION_R8_STANDARD,                     0,                     0,                           0,                      0, 0,                       0},
    {T4_RESOLUTION_100_100, 0, T4_RESOLUTION_200_100,                          0,                     0,                     0,                           0,                      0, 0,                       0},
    {                    0, 0,                     0,      T4_RESOLUTION_R8_FINE,                     0,                     0,                           0,                      0, 0,                       0},
    {                    0, 0, T4_RESOLUTION_200_200,                          0,                     0,                     0,                           0,                      0, 0,                       0},
    {                    0, 0,                     0,                          0, T4_RESOLUTION_300_300,                     0,                           0,                      0, 0,                       0},
    {                    0, 0,                     0, T4_RESOLUTION_R8_SUPERFINE,                     0,                     0, T4_RESOLUTION_R16_SUPERFINE,                      0, 0,                       0},
    {                    0, 0, T4_RESOLUTION_200_400,                          0,                     0, T4_RESOLUTION_400_400,                           0,                      0, 0,                       0},
    {                    0, 0,                     0,                          0, T4_RESOLUTION_300_600,                     0,                           0,  T4_RESOLUTION_600_600, 0,                       0},
    {                    0, 0,                     0,                          0,                     0, T4_RESOLUTION_400_800,                           0,                      0, 0,                       0},
    {                    0, 0,                     0,                          0,                     0,                     0,                           0, T4_RESOLUTION_600_1200, 0, T4_RESOLUTION_1200_1200},
};

#if defined(SPANDSP_SUPPORT_TIFF_FX)
/* TIFF-FX related extensions to the tag set supported by libtiff */

static const TIFFFieldInfo tiff_fx_tiff_field_info[] =
{
    {TIFFTAG_INDEXED, 1, 1, TIFF_SHORT, FIELD_CUSTOM, FALSE, FALSE, (char *) "Indexed"},
    {TIFFTAG_GLOBALPARAMETERSIFD, 1, 1, TIFF_IFD8, FIELD_CUSTOM, FALSE, FALSE, (char *) "GlobalParametersIFD"},
    {TIFFTAG_PROFILETYPE, 1, 1, TIFF_LONG, FIELD_CUSTOM, FALSE, FALSE, (char *) "ProfileType"},
    {TIFFTAG_FAXPROFILE, 1, 1, TIFF_BYTE, FIELD_CUSTOM, FALSE, FALSE, (char *) "FaxProfile"},
    {TIFFTAG_CODINGMETHODS, 1, 1, TIFF_LONG, FIELD_CUSTOM, FALSE, FALSE, (char *) "CodingMethods"},
    {TIFFTAG_VERSIONYEAR, 4, 4, TIFF_BYTE, FIELD_CUSTOM, FALSE, FALSE, (char *) "VersionYear"},
    {TIFFTAG_MODENUMBER, 1, 1, TIFF_BYTE, FIELD_CUSTOM, FALSE, FALSE, (char *) "ModeNumber"},
    {TIFFTAG_DECODE, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SRATIONAL, FIELD_CUSTOM, FALSE, TRUE, (char *) "Decode"},
    {TIFFTAG_IMAGEBASECOLOR, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SHORT, FIELD_CUSTOM, FALSE, TRUE, (char *) "ImageBaseColor"},
    {TIFFTAG_T82OPTIONS, 1, 1, TIFF_LONG, FIELD_CUSTOM, FALSE, FALSE, (char *) "T82Options"},
    {TIFFTAG_STRIPROWCOUNTS, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_LONG, FIELD_CUSTOM, FALSE, TRUE, (char *) "StripRowCounts"},
    {TIFFTAG_IMAGELAYER, 2, 2, TIFF_LONG, FIELD_CUSTOM, FALSE, FALSE, (char *) "ImageLayer"},
};

#if 1
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
    static int first_time = TRUE;

    if (!first_time)
        return;
    first_time = FALSE;

    /* Grab the inherited method and install */
    _ParentExtender = TIFFSetTagExtender(TIFFFXDefaultDirectory);
}
/*- End of function --------------------------------------------------------*/
#endif

static int match_resolution(int res_unit, float actual, const res_table_t table[])
{
    int i;
    int best_entry;
    float best_ratio;
    float ratio;

    if (actual == 0.0f)
        return -1;

    if (res_unit == RESUNIT_INCH)
        actual /= CM_PER_INCH;
    best_ratio = 0.0f;
    best_entry = -1;
    for (i = 0;  table[i].code > 0;  i++)
    {
        if (actual > table[i].resolution)
            ratio = table[i].resolution/actual;
        else
            ratio = actual/table[i].resolution;
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

#if 0 //defined(SPANDSP_SUPPORT_TIFF_FX)
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
    if ((s->colour_map = realloc(s->colour_map, 3*s->colour_map_entries)) == NULL)
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
    lab_to_srgb(&s->lab_params, s->colour_map, s->colour_map, 256);
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
    uint64_t diroff;
    uint8_t parm8;
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

    t = &s->tiff;
    bits_per_sample = 1;
    TIFFGetField(t->tiff_file, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    samples_per_pixel = 1;
    TIFFGetField(t->tiff_file, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    if (samples_per_pixel == 1  &&  bits_per_sample == 1)
        t->image_type = T4_IMAGE_TYPE_BILEVEL;
    else if (samples_per_pixel == 3  &&  bits_per_sample == 1)
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
#if 0
    /* Limit ourselves to plain black and white pages */
    if (t->image_type != T4_IMAGE_TYPE_BILEVEL)
        return -1;
#endif
    parm32 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_IMAGEWIDTH, &parm32);
    t->image_width =
    s->image_width = parm32;
    parm32 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_IMAGELENGTH, &parm32);
    t->image_length =
    s->image_length = parm32;
    x_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_XRESOLUTION, &x_resolution);
    y_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_YRESOLUTION, &y_resolution);
    res_unit = RESUNIT_INCH;
    TIFFGetField(t->tiff_file, TIFFTAG_RESOLUTIONUNIT, &res_unit);
    t->photo_metric = PHOTOMETRIC_MINISWHITE;
    TIFFGetField(t->tiff_file, TIFFTAG_PHOTOMETRIC, &t->photo_metric);

    set_lab_illuminant(&s->lab_params, 0.9638f, 1.0f, 0.8245f);
    set_lab_gamut(&s->lab_params, 0, 100, -85, 85, -75, 125, FALSE);

    t->compression = -1;
    TIFFGetField(t->tiff_file, TIFFTAG_COMPRESSION, &t->compression);
    t->fill_order = FILLORDER_LSB2MSB;

    if (res_unit == RESUNIT_INCH)
        t->image_x_resolution = x_resolution*100.0f/CM_PER_INCH;
    else
        t->image_x_resolution = x_resolution*100.0f;
    /* Treat everything we can't match as R8. Most FAXes are this resolution anyway. */
    if ((best_x_entry = match_resolution(res_unit, x_resolution, x_res_table)) < 0)
        best_x_entry = 3;
    s->metadata.x_resolution = x_res_table[best_x_entry].code;

    if (res_unit == RESUNIT_INCH)
        t->image_y_resolution = y_resolution*100.0f/CM_PER_INCH;
    else
        t->image_y_resolution = y_resolution*100.0f;
    if ((best_y_entry = match_resolution(res_unit, y_resolution, y_res_table)) < 0)
        best_y_entry = 0;
    s->metadata.y_resolution = y_res_table[best_y_entry].code;

    s->metadata.resolution_code = resolution_map[best_y_entry][best_x_entry];

    t4_tx_set_image_width(s, s->image_width);
    t4_tx_set_image_length(s, s->image_length);
    t4_tx_set_max_2d_rows_per_1d_row(s, -s->metadata.y_resolution);
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

    /* If global parameters are present they should only be on the first page of the file.
       However, as we scan the file we might as well look for them on any page. */
    if (TIFFGetField(t->tiff_file, TIFFTAG_GLOBALPARAMETERSIFD, &diroff))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Global parameters IFD at %" PRIu64 "\n", diroff);
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
    int best_x_entry;
    int best_y_entry;
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
#if 0
    /* Limit ourselves to plain black and white pages */
    if (t->image_type != T4_IMAGE_TYPE_BILEVEL)
        return -1;
#endif
    if (t->image_type != image_type)
        return 1;

    parm32 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_IMAGEWIDTH, &parm32);
    if (s->image_width != (int) parm32)
        return 1;
    x_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_XRESOLUTION, &x_resolution);
    y_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_YRESOLUTION, &y_resolution);
    res_unit = RESUNIT_INCH;
    TIFFGetField(t->tiff_file, TIFFTAG_RESOLUTIONUNIT, &res_unit);

    /* Treat everything we can't match as R8. Most FAXes are this resolution anyway. */
    if ((best_x_entry = match_resolution(res_unit, x_resolution, x_res_table)) < 0)
        return 1;
    if (s->metadata.x_resolution != x_res_table[best_x_entry].code)
        return 1;

    if ((best_y_entry = match_resolution(res_unit, y_resolution, y_res_table)) < 0)
        return 1;
    if (s->metadata.y_resolution != y_res_table[best_y_entry].code)
        return 1;

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

static int tiff_row_read_handler(void *user_data, uint8_t buf[], size_t len)
{
    t4_tx_state_t *s;
    int i;
    int j;

    s = (t4_tx_state_t *) user_data;
    if (s->tiff.row >= s->image_length)
        return 0;
    memcpy(buf, &s->tiff.image_buffer[s->tiff.row*len], len);
    s->tiff.row++;

    /* If this is a bi-level image which has more vertical resolution than the
       far end will accept, we need to squash it down to size. */
    for (i = 1;  i < s->row_squashing_ratio  &&  s->tiff.row < s->image_length;  i++)
    {
        for (j = 0;  j < s->image_width/8;  j++)
            buf[j] |= s->tiff.image_buffer[s->tiff.row*len + j];
        s->tiff.row++;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static int row_read(void *user_data, uint8_t buf[], size_t len)
{
    t4_tx_state_t *s;

    s = (t4_tx_state_t *) user_data;

    if (s->tiff.raw_row >= s->tiff.image_length)
        return 0;
    if (TIFFReadScanline(s->tiff.tiff_file, buf, s->tiff.raw_row, 0) < 0)
        return 0;
    if (s->apply_lab)
        lab_to_srgb(&s->lab_params, buf, buf, len/3);
    s->tiff.raw_row++;
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

static int read_tiff_t85_image(t4_tx_state_t *s)
{
    int biggest;
    int num_strips;
    int total_len;
    int len;
    int i;
    int result;
    uint8_t *t;
    uint8_t *raw_data;
    t85_decode_state_t t85;
    packer_t pack;

    /* Size up and allocate the buffer for the raw data */
    num_strips = TIFFNumberOfStrips(s->tiff.tiff_file);
    biggest = TIFFRawStripSize(s->tiff.tiff_file, 0);
    for (i = 1;  i < num_strips;  i++)
    {
        len = TIFFRawStripSize(s->tiff.tiff_file, i);
        if (len > biggest)
            biggest = len;
    }
    if ((raw_data = malloc(biggest)) == NULL)
        return -1;

    s->tiff.image_size = s->image_length*((s->image_width + 7)/8);
    if (s->tiff.image_size >= s->tiff.image_buffer_size)
    {
        if ((t = realloc(s->tiff.image_buffer, s->tiff.image_size)) == NULL)
            return -1;
        s->tiff.image_buffer_size = s->tiff.image_size;
        s->tiff.image_buffer = t;
    }

    pack.buf = s->tiff.image_buffer;
    pack.ptr = 0;
    pack.row = 0;
    t85_decode_init(&t85, packing_row_write_handler, &pack);
    t85_decode_set_comment_handler(&t85, 1000, embedded_comment_handler, s);

    total_len = 0;
    result = -1;
    for (i = 0;  i < num_strips;  i++, total_len += len)
    {
        len = TIFFRawStripSize(s->tiff.tiff_file, i);
        if ((len = TIFFReadRawStrip(s->tiff.tiff_file, i, raw_data, len)) < 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: ReadRaw error.\n", s->tiff.file);
            return -1;
        }
        result = t85_decode_put(&t85, raw_data, len);
        if (result != T4_DECODE_MORE_DATA)
            break;
    }
    if (result == T4_DECODE_MORE_DATA)
        result = t85_decode_put(&t85, NULL, 0);

    len = t85_decode_get_compressed_image_size(&t85);
    span_log(&s->logging, SPAN_LOG_WARNING, "Compressed image is %d bytes, %d rows\n", len/8, s->image_length);
    t85_decode_release(&t85);
    free(raw_data);
    return 0;
}
/*- End of function --------------------------------------------------------*/

#if defined(SPANDSP_SUPPORT_T43)
static int read_tiff_t43_image(t4_tx_state_t *s, uint8_t **buf)
{
    int num_strips;
    int total_len;
    int len;
    int i;
    int total_image_len;
    int image_size;
    logging_state_t *logging;
    uint8_t *raw_data;
    t43_decode_state_t t43;
    packer_t pack;

    num_strips = TIFFNumberOfStrips(s->tiff.tiff_file);
    total_image_len = 0;
    for (i = 0;  i < num_strips;  i++)
        total_image_len += TIFFRawStripSize(s->tiff.tiff_file, i);
    if ((raw_data = malloc(total_image_len)) == NULL)
        return -1;

    total_len = 0;
    for (i = 0;  i < num_strips;  i++, total_len += len)
    {
        if ((len = TIFFReadRawStrip(s->tiff.tiff_file, i, &raw_data[total_len], total_image_len - total_len)) < 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "TIFF read error.\n");
            return -1;
        }
    }

    t43_decode_init(&t43, packing_row_write_handler, &pack);
    t43_decode_set_comment_handler(&t43, 1000, embedded_comment_handler, NULL);
    logging = t43_decode_get_logging_state(&t43);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

    image_size = 3*s->image_length*s->image_width;
    if ((*buf = malloc(image_size)) == NULL)
        return -1;

    pack.buf = *buf;
    pack.ptr = 0;
    pack.row = 0;
    t43_decode_put(&t43, raw_data, total_len);
    t43_decode_release(&t43);
    free(raw_data);
    return image_size;
}
/*- End of function --------------------------------------------------------*/
#endif

#if 0
static int read_tiff_t42_t81_image(t4_tx_state_t *s, uint8_t **buf)
{
    int total_len;
    int len;
    int i;
    int num_strips;
    int total_image_len;
    int image_size;
    uint8_t *raw_data;
    uint8_t *jpeg_table;
    uint32_t jpeg_table_len;
    uint32_t w;
    uint32_t h;
    tsize_t off;

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
    if ((raw_data = malloc(total_image_len)) == NULL)
        return -1;

    total_len = 0;
    if (jpeg_table_len > 0)
        total_len += jpeg_table_len - 4;
    for (i = 0;  i < num_strips;  i++, total_len += len)
    {
        if ((len = TIFFReadRawStrip(s->tiff.tiff_file, i, &raw_data[total_len], total_image_len - total_len)) < 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "TIFF read error.\n");
            return -1;
        }
    }
    if (jpeg_table_len > 0)
        memcpy(raw_data, jpeg_table, jpeg_table_len - 2);

    if (total_len != total_image_len)
        span_log(&s->logging, SPAN_LOG_FLOW, "Size mismatch %d %d\n", (int) total_len, (int) total_image_len);

    image_size = 3*s->image_length*s->image_width;
    if ((*buf = malloc(image_size)) == NULL)
        return -1;

    off = 0;
    if (!t42_itulab_to_srgb(&s->logging, &s->lab_params, *buf, &off, raw_data, total_image_len, &w, &h))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Failed to convert from ITULAB.\n");
        return -1;
    }
    free(raw_data);
    return image_size;
}
/*- End of function --------------------------------------------------------*/
#endif

static int read_tiff_decompressed_image(t4_tx_state_t *s)
{
    int total_len;
    int len;
    int num_strips;
    int i;
    uint8_t *t;

    /* Decode the whole image into a buffer */
    /* Let libtiff handle the decompression */
    s->tiff.image_size = s->image_length*TIFFScanlineSize(s->tiff.tiff_file);
    if (s->tiff.image_size >= s->tiff.image_buffer_size)
    {
        if ((t = realloc(s->tiff.image_buffer, s->tiff.image_size)) == NULL)
            return -1;
        s->tiff.image_buffer_size = s->tiff.image_size;
        s->tiff.image_buffer = t;
    }

    /* Allow for the image being stored in multiple strips, although it is rare to find
       a stripped image in a T.4 or T.6 encoded file. */
    num_strips = TIFFNumberOfStrips(s->tiff.tiff_file);
    for (i = 0, total_len = 0;  i < num_strips;  i++, total_len += len)
    {
        if ((len = TIFFReadEncodedStrip(s->tiff.tiff_file, i, &s->tiff.image_buffer[total_len], s->tiff.image_size - total_len)) < 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: Read error.\n", s->tiff.file);
            return -1;
        }
    }
    /* We might need to flip all the bits, so 1 = black and 0 = white. */
    if (s->tiff.photo_metric != PHOTOMETRIC_MINISWHITE)
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
    uint8_t *t;
    image_translate_state_t *translator;

    if (s->tiff.image_type != T4_IMAGE_TYPE_BILEVEL)
    {
        /* We need to dither this image down to pure black and white, possibly resizing it
           along the way. */
        if ((translator = image_translate_init(NULL, s->tiff.image_type, s->image_width, s->image_length, T4_IMAGE_TYPE_BILEVEL, 1728, -1, row_read, s)) == NULL)
            return -1;
        s->image_width = image_translate_get_output_width(translator);
        s->image_length = image_translate_get_output_length(translator);
        s->metadata.x_resolution = T4_X_RESOLUTION_R8;
        s->metadata.y_resolution = T4_Y_RESOLUTION_FINE;
        s->metadata.resolution_code = T4_RESOLUTION_R8_FINE;
        s->tiff.image_size = (s->image_width*s->image_length + 7)/8;
        if (s->tiff.image_size >= s->tiff.image_buffer_size)
        {
            if ((t = realloc(s->tiff.image_buffer, s->tiff.image_size)) == NULL)
                return -1;
            s->tiff.image_buffer_size = s->tiff.image_size;
            s->tiff.image_buffer = t;
        }
        s->tiff.raw_row = 0;
        switch (s->tiff.photo_metric)
        {
        case PHOTOMETRIC_CIELAB:
            /* The default luminant is D50 */
            set_lab_illuminant(&s->lab_params, 0.96422f, 1.0f,  0.82521f);
            set_lab_gamut(&s->lab_params, 0, 100, -128, 127, -128, 127, TRUE);
            s->apply_lab = TRUE;
            break;
        case PHOTOMETRIC_ITULAB:
            set_lab_illuminant(&s->lab_params, 0.9638f, 1.0f, 0.8245f);
            set_lab_gamut(&s->lab_params, 0, 100, -85, 85, -75, 125, FALSE);
            s->apply_lab = TRUE;
            break;
        default:
            s->apply_lab = FALSE;
            break;
        }
        total_len = 0;
        for (i = 0;  i < s->image_length;  i++)
            total_len += image_translate_row(translator, &s->tiff.image_buffer[total_len], s->image_width/8);
        image_translate_free(translator);
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
        case COMPRESSION_T85:
            /* Decode the whole image into a buffer */
            /* libtiff probably cannot decompress T.85, so we must handle it ourselves */
            /* Decode the whole image into a buffer */
            if (read_tiff_t85_image(s) < 0)
                return -1;
            break;
        default:
            /* Decode the whole image into a buffer */
            /* Let libtiff handle the decompression */
            if (read_tiff_decompressed_image(s) < 0)
                return -1;
            break;
        }
    }
    s->tiff.row = 0;
    return s->image_length;
}
/*- End of function --------------------------------------------------------*/

static void tiff_tx_release(t4_tx_state_t *s)
{
    if (s->tiff.tiff_file)
    {
        TIFFClose(s->tiff.tiff_file);
        s->tiff.tiff_file = NULL;
        if (s->tiff.file)
            free((char *) s->tiff.file);
        s->tiff.file = NULL;
    }
    if (s->tiff.image_buffer)
    {
        free(s->tiff.image_buffer);
        s->tiff.image_buffer = NULL;
        s->tiff.image_size = 0;
        s->tiff.image_buffer_size = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static int set_row_read_handler(t4_tx_state_t *s, t4_row_read_handler_t handler, void *user_data)
{
    switch (s->line_encoding)
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
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        return t42_encode_set_row_read_handler(&s->encoder.t42, handler, user_data);
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_T43:
        return t43_encode_set_row_read_handler(&s->encoder.t43, handler, user_data);
#endif
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        break;
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
        if ((s->header_text = malloc(132 + 1)) == NULL)
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
    int repeats;
    int pattern;
    int pos;
    int row;
    char *t;
    t4_tx_state_t *s;

    s = (t4_tx_state_t *) user_data;
    switch (s->metadata.y_resolution)
    {
    case T4_Y_RESOLUTION_1200:
        repeats = 12;
        break;
    case T4_Y_RESOLUTION_800:
        repeats = 8;
        break;
    case T4_Y_RESOLUTION_600:
        repeats = 6;
        break;
    case T4_Y_RESOLUTION_SUPERFINE:
        repeats = 4;
        break;
    case T4_Y_RESOLUTION_300:
        repeats = 3;
        break;
    case T4_Y_RESOLUTION_FINE:
        repeats = 2;
        break;
    default:
        repeats = 1;
        break;
    }
    repeats /= s->row_squashing_ratio;
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
    row = s->header_row/repeats;
    pos = 0;
    for (t = s->header_text;  *t  &&  pos <= len - 2;  t++)
    {
        pattern = header_font[(uint8_t) *t][row];
        buf[pos++] = (uint8_t) (pattern >> 8);
        buf[pos++] = (uint8_t) (pattern & 0xFF);
    }
    while (pos < len)
        buf[pos++] = 0;
    s->header_row++;
    if (s->header_row >= 16*repeats)
    {
        /* End of header. Change to normal image row data. */
        set_row_read_handler(s, s->row_handler, s->row_handler_user_data);
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_row_squashing_ratio(t4_tx_state_t *s, int row_squashing_ratio)
{
    s->row_squashing_ratio = row_squashing_ratio;
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

SPAN_DECLARE(int) t4_tx_set_tx_encoding(t4_tx_state_t *s, int encoding)
{
    {
        switch (encoding)
        {
        case T4_COMPRESSION_T4_1D:
        case T4_COMPRESSION_T4_2D:
        case T4_COMPRESSION_T6:
            switch (s->line_encoding)
            {
            case T4_COMPRESSION_T4_1D:
            case T4_COMPRESSION_T4_2D:
            case T4_COMPRESSION_T6:
                break;
            default:
                t4_t6_encode_init(&s->encoder.t4_t6, encoding, s->image_width, s->row_handler, s->row_handler_user_data);
                t4_t6_encode_set_max_2d_rows_per_1d_row(&s->encoder.t4_t6, -s->metadata.y_resolution);
                break;
            }
            s->line_encoding = encoding;
            if (t4_t6_encode_set_encoding(&s->encoder.t4_t6, encoding))
                return -1;
            return s->line_encoding;
        case T4_COMPRESSION_T85:
        case T4_COMPRESSION_T85_L0:
            switch (s->line_encoding)
            {
            case T4_COMPRESSION_T85:
            case T4_COMPRESSION_T85_L0:
                break;
            default:
                t85_encode_init(&s->encoder.t85, s->image_width, s->image_length, s->row_handler, s->row_handler_user_data);
                break;
            }
            s->line_encoding = encoding;
            return s->line_encoding;
#if defined(SPANDSP_SUPPORT_T88)
        case T4_COMPRESSION_T88:
            switch (s->line_encoding)
            {
            case T4_COMPRESSION_T88:
                break;
            default:
                break;
            }
            s->line_encoding = encoding;
            return s->line_encoding;
#endif
        case T4_COMPRESSION_T42_T81:
        case T4_COMPRESSION_SYCC_T81:
            switch (s->line_encoding)
            {
            case T4_COMPRESSION_T42_T81:
            case T4_COMPRESSION_SYCC_T81:
                break;
            default:
                t42_encode_init(&s->encoder.t42, s->image_width, s->image_length, s->row_handler, s->row_handler_user_data);
                break;
            }
            s->line_encoding = encoding;
            return s->line_encoding;
#if defined(SPANDSP_SUPPORT_T43)
        case T4_COMPRESSION_T43:
            switch (s->line_encoding)
            {
            case T4_COMPRESSION_T43:
                break;
            default:
                t43_encode_init(&s->encoder.t43, s->image_width, s->image_length, s->row_handler, s->row_handler_user_data);
                break;
            }
            s->line_encoding = encoding;
            return s->line_encoding;
#endif
#if defined(SPANDSP_SUPPORT_T45)
        case T4_COMPRESSION_T45:
            switch (s->line_encoding)
            {
            case T4_COMPRESSION_T45:
                break;
            default:
                break;
            }
            s->line_encoding = encoding;
            return s->line_encoding;
#endif
        }
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_min_bits_per_row(t4_tx_state_t *s, int bits)
{
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t4_t6_encode_set_min_bits_per_row(&s->encoder.t4_t6, bits);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_image_width(t4_tx_state_t *s, int image_width)
{
    s->image_width = image_width;
    switch (s->line_encoding)
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
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        t42_encode_set_image_width(&s->encoder.t42, image_width);
        break;
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_T43:
        t43_encode_set_image_width(&s->encoder.t43, image_width);
        break;
#endif
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        break;
#endif
    }
}
/*- End of function --------------------------------------------------------*/

static void t4_tx_set_image_length(t4_tx_state_t *s, int image_length)
{
    s->image_length = image_length;
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        t85_encode_set_image_length(&s->encoder.t85, image_length);
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        t42_encode_set_image_length(&s->encoder.t42, image_length);
        break;
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_T43:
        t43_encode_set_image_length(&s->encoder.t43, image_length);
        break;
#endif
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        break;
#endif
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_max_2d_rows_per_1d_row(t4_tx_state_t *s, int max)
{
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t4_t6_encode_set_max_2d_rows_per_1d_row(&s->encoder.t4_t6, max);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_image_width(t4_tx_state_t *s)
{
    return s->image_width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_header_overlays_image(t4_tx_state_t *s, int header_overlays_image)
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

SPAN_DECLARE(int) t4_tx_get_y_resolution(t4_tx_state_t *s)
{
    return s->metadata.y_resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_x_resolution(t4_tx_state_t *s)
{
    return s->metadata.x_resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_resolution(t4_tx_state_t *s)
{
    return s->metadata.resolution_code;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_image_type(t4_tx_state_t *s)
{
    return s->metadata.image_type;
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
    t->image_x_resolution = s->tiff.image_x_resolution;
    t->image_y_resolution = s->tiff.image_y_resolution;

    t->x_resolution = s->metadata.x_resolution;
    t->y_resolution = s->metadata.y_resolution/s->row_squashing_ratio;
    t->encoding = s->line_encoding;
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t->type = T4_IMAGE_TYPE_BILEVEL;
        t->width = t4_t6_encode_get_image_width(&s->encoder.t4_t6);
        t->length = t4_t6_encode_get_image_length(&s->encoder.t4_t6)/s->row_squashing_ratio;
        t->line_image_size = t4_t6_encode_get_compressed_image_size(&s->encoder.t4_t6)/8;
        break;
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        t->type = T4_IMAGE_TYPE_BILEVEL;
        t->width = t85_encode_get_image_width(&s->encoder.t85);
        t->length = t85_encode_get_image_length(&s->encoder.t85)/s->row_squashing_ratio;
        t->line_image_size = t85_encode_get_compressed_image_size(&s->encoder.t85)/8;
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        t->type = 0;
        t->width = t42_encode_get_image_width(&s->encoder.t42);
        t->length = t42_encode_get_image_length(&s->encoder.t42)/s->row_squashing_ratio;
        t->line_image_size = t42_encode_get_compressed_image_size(&s->encoder.t42)/8;
        break;
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_T43:
        t->type = 0;
        t->width = t43_encode_get_image_width(&s->encoder.t43);
        t->length = t43_encode_get_image_length(&s->encoder.t43)/s->row_squashing_ratio;
        t->line_image_size = t43_encode_get_compressed_image_size(&s->encoder.t43)/8;
        break;
#endif
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        break;
#endif
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_image_complete(t4_tx_state_t *s)
{
    switch (s->line_encoding)
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
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        return t42_encode_image_complete(&s->encoder.t42);
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_T43:
        return t43_encode_image_complete(&s->encoder.t43);
#endif
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        break;
#endif
    }
    return SIG_STATUS_END_OF_DATA;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_bit(t4_tx_state_t *s)
{
    /* We only get bit by bit for T.4 1D and T.4 2-D. */
    return t4_t6_encode_get_bit(&s->encoder.t4_t6);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get(t4_tx_state_t *s, uint8_t buf[], size_t max_len)
{
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        return t4_t6_encode_get(&s->encoder.t4_t6, buf, max_len);
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        return t85_encode_get(&s->encoder.t85, buf, max_len);
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        return t42_encode_get(&s->encoder.t42, buf, max_len);
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_T43:
        return t43_encode_get(&s->encoder.t43, buf, max_len);
#endif
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        break;
#endif
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_start_page(t4_tx_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Start tx page %d - compression %s\n", s->current_page, t4_encoding_to_str(s->line_encoding));
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
        s->image_length = UINT32_MAX;
    }

    switch (s->line_encoding)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        t4_t6_encode_restart(&s->encoder.t4_t6, s->image_width);
        break;
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        t85_encode_restart(&s->encoder.t85, s->image_width, s->image_length);
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        t42_encode_restart(&s->encoder.t42, s->image_width, s->image_length);
        break;
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_T43:
        t43_encode_restart(&s->encoder.t43, s->image_width, s->image_length);
        break;
#endif
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        break;
#endif
    }
    /* If there is a page header, create that first */
    if (s->tiff.image_type == T4_IMAGE_TYPE_BILEVEL  &&  s->header_info  &&  s->header_info[0]  &&  make_header(s) == 0)
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

    allocated = FALSE;
    if (s == NULL)
    {
        if ((s = (t4_tx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
        allocated = TRUE;
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
    s->line_encoding = T4_COMPRESSION_NONE;

    s->row_handler = tiff_row_read_handler;
    s->row_handler_user_data = (void *) s;

    s->row_squashing_ratio = 1;

    if (file)
    {
        if (open_tiff_input_file(s, file) < 0)
        {
            if (allocated)
                free(s);
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
                free(s);
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
        free(s->header_text);
        s->header_text = NULL;
    }
    if (s->colour_map)
    {
        free(s->colour_map);
        s->colour_map = NULL;
    }
    switch (s->line_encoding)
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
        break;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        return t42_encode_release(&s->encoder.t42);
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_T43:
        return t43_encode_release(&s->encoder.t43);
#endif
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        break;
#endif
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_free(t4_tx_state_t *s)
{
    int ret;

    ret = t4_tx_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
