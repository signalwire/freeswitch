//#define T4_STATE_DEBUGGING
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4.c - ITU T.4 FAX image processing
 * This depends on libtiff (see <http://www.libtiff.org>)
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2007 Steve Underwood
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
 *
 * $Id: t4.c,v 1.131 2009/05/16 03:34:45 steveu Exp $
 */

/*
 * Much of this file is based on the T.4 and T.6 support in libtiff, which requires
 * the following notice in any derived source code:
 *
 * Copyright (c) 1990-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 *
 * Decoder support is derived from code in Frank Cringle's viewfax program;
 *      Copyright (C) 1990, 1995  Frank D. Cringle.
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
#include "spandsp/t4.h"
#include "spandsp/version.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t4.h"

/*! The number of centimetres in one inch */
#define CM_PER_INCH                 2.54f

/*! The number of EOLs to be sent at the end of a T.4 page */
#define EOLS_TO_END_T4_TX_PAGE      6
/*! The number of EOLs to be sent at the end of a T.6 page */
#define EOLS_TO_END_T6_TX_PAGE      2

/*! The number of EOLs to expect at the end of a T.4 page */
#define EOLS_TO_END_ANY_RX_PAGE     6
/*! The number of EOLs to check at the end of a T.4 page */
#define EOLS_TO_END_T4_RX_PAGE      5
/*! The number of EOLs to check at the end of a T.6 page */
#define EOLS_TO_END_T6_RX_PAGE      2

/* Finite state machine state codes */
enum
{
    S_Null      = 0,
    S_Pass      = 1,
    S_Horiz     = 2,
    S_Vert      = 3,
    S_Ext       = 4,
    S_TermW     = 5,
    S_TermB     = 6,
    S_MakeUpW   = 7,
    S_MakeUpB   = 8,
    S_MakeUp    = 9,
    S_EOL       = 10
};

#include "faxfont.h"

static int encode_row(t4_state_t *s);

#if defined(T4_STATE_DEBUGGING)
static void STATE_TRACE(const char *format, ...)
{
    va_list arg_ptr;

    va_start(arg_ptr, format);
    vprintf(format, arg_ptr);
    va_end(arg_ptr);
}
/*- End of function --------------------------------------------------------*/
#else
#define STATE_TRACE(...) /**/
#endif

/*! T.4 finite state machine state table entry */
typedef struct
{
    /*! State */
    uint8_t state;
    /*! Width of code in bits */
    uint8_t width;
    /*! Run length in bits */
    int16_t param;
} t4_table_entry_t;

/*! T.4 run length table entry */
typedef struct
{
    /*! Length of T.4 code, in bits */
    uint16_t length;
    /*! T.4 code */
    uint16_t code;
    /*! Run length, in bits */
    int16_t run_length;
} t4_run_table_entry_t;

#include "t4_states.h"

#if defined(HAVE_LIBTIFF)
static int set_tiff_directory_info(t4_state_t *s)
{
    time_t now;
    struct tm *tm;
    char buf[256 + 1];
    uint16_t resunit;
    float x_resolution;
    float y_resolution;
    t4_tiff_state_t *t;

    t = &s->tiff;
    /* Prepare the directory entry fully before writing the image, or libtiff complains */
    TIFFSetField(t->tiff_file, TIFFTAG_COMPRESSION, t->output_compression);
    if (t->output_compression == COMPRESSION_CCITT_T4)
    {
        TIFFSetField(t->tiff_file, TIFFTAG_T4OPTIONS, t->output_t4_options);
        TIFFSetField(t->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
    }
    TIFFSetField(t->tiff_file, TIFFTAG_IMAGEWIDTH, s->image_width);
    TIFFSetField(t->tiff_file, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(t->tiff_file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(t->tiff_file, TIFFTAG_SAMPLESPERPIXEL, 1);
    if (t->output_compression == COMPRESSION_CCITT_T4
        ||
        t->output_compression == COMPRESSION_CCITT_T6)
    {
        TIFFSetField(t->tiff_file, TIFFTAG_ROWSPERSTRIP, -1L);
    }
    else
    {
        TIFFSetField(t->tiff_file,
                     TIFFTAG_ROWSPERSTRIP,
                     TIFFDefaultStripSize(t->tiff_file, 0));
    }
    TIFFSetField(t->tiff_file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(t->tiff_file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(t->tiff_file, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);

    x_resolution = s->x_resolution/100.0f;
    y_resolution = s->y_resolution/100.0f;
    /* Metric seems the sane thing to use in the 21st century, but a lot of lousy software
       gets FAX resolutions wrong, and more get it wrong using metric than using inches. */
#if 0
    TIFFSetField(t->tiff_file, TIFFTAG_XRESOLUTION, x_resolution);
    TIFFSetField(t->tiff_file, TIFFTAG_YRESOLUTION, y_resolution);
    resunit = RESUNIT_CENTIMETER;
    TIFFSetField(t->tiff_file, TIFFTAG_RESOLUTIONUNIT, resunit);
#else
    TIFFSetField(t->tiff_file, TIFFTAG_XRESOLUTION, floorf(x_resolution*CM_PER_INCH + 0.5f));
    TIFFSetField(t->tiff_file, TIFFTAG_YRESOLUTION, floorf(y_resolution*CM_PER_INCH + 0.5f));
    resunit = RESUNIT_INCH;
    TIFFSetField(t->tiff_file, TIFFTAG_RESOLUTIONUNIT, resunit);
#endif
    /* TODO: add the version of spandsp */
    TIFFSetField(t->tiff_file, TIFFTAG_SOFTWARE, "Spandsp " SPANDSP_RELEASE_DATETIME_STRING);
    if (gethostname(buf, sizeof(buf)) == 0)
        TIFFSetField(t->tiff_file, TIFFTAG_HOSTCOMPUTER, buf);

#if defined(TIFFTAG_FAXDCS)
    if (t->dcs)
        TIFFSetField(t->tiff_file, TIFFTAG_FAXDCS, t->dcs);
#endif
    if (t->sub_address)
        TIFFSetField(t->tiff_file, TIFFTAG_FAXSUBADDRESS, t->sub_address);
    if (t->far_ident)
        TIFFSetField(t->tiff_file, TIFFTAG_IMAGEDESCRIPTION, t->far_ident);
    if (t->vendor)
        TIFFSetField(t->tiff_file, TIFFTAG_MAKE, t->vendor);
    if (t->model)
        TIFFSetField(t->tiff_file, TIFFTAG_MODEL, t->model);

    time(&now);
    tm = localtime(&now);
    sprintf(buf,
            "%4d/%02d/%02d %02d:%02d:%02d",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec);
    TIFFSetField(t->tiff_file, TIFFTAG_DATETIME, buf);
    TIFFSetField(t->tiff_file, TIFFTAG_FAXRECVTIME, now - s->page_start_time);

    TIFFSetField(t->tiff_file, TIFFTAG_IMAGELENGTH, s->image_length);
    /* Set the total pages to 1. For any one page document we will get this
       right. For multi-page documents we will need to come back and fill in
       the right answer when we know it. */
    TIFFSetField(t->tiff_file, TIFFTAG_PAGENUMBER, s->current_page++, 1);
    s->pages_in_file = s->current_page;
    if (t->output_compression == COMPRESSION_CCITT_T4)
    {
        if (s->bad_rows)
        {
            TIFFSetField(t->tiff_file, TIFFTAG_BADFAXLINES, s->bad_rows);
            TIFFSetField(t->tiff_file, TIFFTAG_CLEANFAXDATA, CLEANFAXDATA_REGENERATED);
            TIFFSetField(t->tiff_file, TIFFTAG_CONSECUTIVEBADFAXLINES, s->longest_bad_row_run);
        }
        else
        {
            TIFFSetField(t->tiff_file, TIFFTAG_CLEANFAXDATA, CLEANFAXDATA_CLEAN);
        }
    }
    TIFFSetField(t->tiff_file, TIFFTAG_IMAGEWIDTH, s->image_width);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int test_resolution(int res_unit, float actual, float expected)
{
    if (res_unit == RESUNIT_INCH)
        actual *= 1.0f/CM_PER_INCH;
    return (expected*0.95f <= actual  &&  actual <= expected*1.05f);
}
/*- End of function --------------------------------------------------------*/

static int get_tiff_directory_info(t4_state_t *s)
{
    static const struct
    {
        float resolution;
        int code;
    } x_res_table[] =
    {
        { 102.0f/CM_PER_INCH, T4_X_RESOLUTION_R4},
        { 204.0f/CM_PER_INCH, T4_X_RESOLUTION_R8},
        { 300.0f/CM_PER_INCH, T4_X_RESOLUTION_300},
        { 408.0f/CM_PER_INCH, T4_X_RESOLUTION_R16},
        { 600.0f/CM_PER_INCH, T4_X_RESOLUTION_600},
        { 800.0f/CM_PER_INCH, T4_X_RESOLUTION_800},
        {1200.0f/CM_PER_INCH, T4_X_RESOLUTION_1200},
        {             -1.00f, -1}
    };
    static const struct
    {
        float resolution;
        int code;
        int max_rows_to_next_1d_row;
    } y_res_table[] =
    {
        {             38.50f, T4_Y_RESOLUTION_STANDARD, 2},
        {             77.00f, T4_Y_RESOLUTION_FINE, 4},
        { 300.0f/CM_PER_INCH, T4_Y_RESOLUTION_300, 6},
        {            154.00f, T4_Y_RESOLUTION_SUPERFINE, 8},
        { 600.0f/CM_PER_INCH, T4_Y_RESOLUTION_600, 12},
        { 800.0f/CM_PER_INCH, T4_Y_RESOLUTION_800, 16},
        {1200.0f/CM_PER_INCH, T4_Y_RESOLUTION_1200, 24},
        {             -1.00f, -1, -1}
    };
    uint16_t res_unit;
    uint16_t parm16;
    uint32_t parm32;
    float x_resolution;
    float y_resolution;
    int i;
    t4_tiff_state_t *t;

    t = &s->tiff;
    parm16 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_BITSPERSAMPLE, &parm16);
    if (parm16 != 1)
        return -1;
    parm32 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_IMAGEWIDTH, &parm32);
    s->image_width = parm32;
    s->bytes_per_row = (s->image_width + 7)/8;
    parm32 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_IMAGELENGTH, &parm32);
    s->image_length = parm32;
    x_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_XRESOLUTION, &x_resolution);
    y_resolution = 0.0f;
    TIFFGetField(t->tiff_file, TIFFTAG_YRESOLUTION, &y_resolution);
    res_unit = RESUNIT_INCH;
    TIFFGetField(t->tiff_file, TIFFTAG_RESOLUTIONUNIT, &res_unit);
    t->photo_metric = PHOTOMETRIC_MINISWHITE;
    TIFFGetField(t->tiff_file, TIFFTAG_PHOTOMETRIC, &t->photo_metric);
    if (t->photo_metric != PHOTOMETRIC_MINISWHITE)
        span_log(&s->logging, SPAN_LOG_FLOW, "%s: Photometric needs swapping.\n", s->file);
    t->fill_order = FILLORDER_LSB2MSB;
#if 0
    TIFFGetField(t->tiff_file, TIFFTAG_FILLORDER, &t->fill_order);
    if (t->fill_order != FILLORDER_LSB2MSB)
        span_log(&s->logging, SPAN_LOG_FLOW, "%s: Fill order needs swapping.\n", s->file);
#endif

    /* Allow a little range for the X resolution in centimeters. The spec doesn't pin down the
       precise value. The other value should be exact. */
    /* Treat everything we can't match as R8. Most FAXes are this resolution anyway. */
    s->x_resolution = T4_X_RESOLUTION_R8;
    for (i = 0;  x_res_table[i].code > 0;  i++)
    {
        if (test_resolution(res_unit, x_resolution, x_res_table[i].resolution))
        {
            s->x_resolution = x_res_table[i].code;
            break;
        }
    }

    s->y_resolution = T4_Y_RESOLUTION_STANDARD;
    s->max_rows_to_next_1d_row = 2;
    for (i = 0;  y_res_table[i].code > 0;  i++)
    {
        if (test_resolution(res_unit, y_resolution, y_res_table[i].resolution))
        {
            s->y_resolution = y_res_table[i].code;
            s->max_rows_to_next_1d_row = y_res_table[i].max_rows_to_next_1d_row;
            break;
        }
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/

static int test_tiff_directory_info(t4_state_t *s)
{
    static const struct
    {
        float resolution;
        int code;
    } x_res_table[] =
    {
        { 102.0f/CM_PER_INCH, T4_X_RESOLUTION_R4},
        { 204.0f/CM_PER_INCH, T4_X_RESOLUTION_R8},
        { 300.0f/CM_PER_INCH, T4_X_RESOLUTION_300},
        { 408.0f/CM_PER_INCH, T4_X_RESOLUTION_R16},
        { 600.0f/CM_PER_INCH, T4_X_RESOLUTION_600},
        { 800.0f/CM_PER_INCH, T4_X_RESOLUTION_800},
        {1200.0f/CM_PER_INCH, T4_X_RESOLUTION_1200},
        {             -1.00f, -1}
    };
    static const struct
    {
        float resolution;
        int code;
        int max_rows_to_next_1d_row;
    } y_res_table[] =
    {
        {             38.50f, T4_Y_RESOLUTION_STANDARD, 2},
        {             77.00f, T4_Y_RESOLUTION_FINE, 4},
        { 300.0f/CM_PER_INCH, T4_Y_RESOLUTION_300, 6},
        {            154.00f, T4_Y_RESOLUTION_SUPERFINE, 8},
        { 600.0f/CM_PER_INCH, T4_Y_RESOLUTION_600, 12},
        { 800.0f/CM_PER_INCH, T4_Y_RESOLUTION_800, 16},
        {1200.0f/CM_PER_INCH, T4_Y_RESOLUTION_1200, 24},
        {             -1.00f, -1, -1}
    };
    uint16_t res_unit;
    uint16_t parm16;
    uint32_t parm32;
    float x_resolution;
    float y_resolution;
    int i;
    t4_tiff_state_t *t;

    t = &s->tiff;
    parm16 = 0;
    TIFFGetField(t->tiff_file, TIFFTAG_BITSPERSAMPLE, &parm16);
    if (parm16 != 1)
        return -1;
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

    /* Allow a little range for the X resolution in centimeters. The spec doesn't pin down the
       precise value. The other value should be exact. */
    /* Treat everything we can't match as R8. Most FAXes are this resolution anyway. */
    for (i = 0;  x_res_table[i].code > 0;  i++)
    {
        if (test_resolution(res_unit, x_resolution, x_res_table[i].resolution))
            break;
    }
    if (s->x_resolution != x_res_table[i].code)
        return 1;
    for (i = 0;  y_res_table[i].code > 0;  i++)
    {
        if (test_resolution(res_unit, y_resolution, y_res_table[i].resolution))
            break;
    }
    if (s->y_resolution != y_res_table[i].code)
        return 1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int get_tiff_total_pages(t4_state_t *s)
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

static int open_tiff_input_file(t4_state_t *s, const char *file)
{
    if ((s->tiff.tiff_file = TIFFOpen(file, "r")) == NULL)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int read_tiff_image(t4_state_t *s)
{
    int row;
    int image_length;
    int i;

    image_length = 0;
    TIFFGetField(s->tiff.tiff_file, TIFFTAG_IMAGELENGTH, &image_length);
    for (row = 0;  row < image_length;  row++)
    {
        if (TIFFReadScanline(s->tiff.tiff_file, s->row_buf, row, 0) <= 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: Read error at row %d.\n", s->file, row);
            break;
        }
        if (s->tiff.photo_metric != PHOTOMETRIC_MINISWHITE)
        {
            for (i = 0;  i < s->bytes_per_row;  i++)
                s->row_buf[i] = ~s->row_buf[i];
        }
        if (s->tiff.fill_order != FILLORDER_LSB2MSB)
            bit_reverse(s->row_buf, s->row_buf, s->bytes_per_row);
        if (encode_row(s))
            return -1;
    }
    return image_length;
}
/*- End of function --------------------------------------------------------*/

static int close_tiff_input_file(t4_state_t *s)
{
    TIFFClose(s->tiff.tiff_file);
    s->tiff.tiff_file = NULL;
    if (s->file)
        free((char *) s->file);
    s->file = NULL;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int open_tiff_output_file(t4_state_t *s, const char *file)
{
    if ((s->tiff.tiff_file = TIFFOpen(file, "w")) == NULL)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void write_tiff_image(t4_state_t *s)
{
    /* Set up the TIFF directory info... */
    set_tiff_directory_info(s);
    /* ..and then write the image... */
    if (TIFFWriteEncodedStrip(s->tiff.tiff_file, 0, s->image_buffer, s->image_length*s->bytes_per_row) < 0)
        span_log(&s->logging, SPAN_LOG_WARNING, "%s: Error writing TIFF strip.\n", s->file);
    /* ...then the directory entry, and libtiff is happy. */
    TIFFWriteDirectory(s->tiff.tiff_file);
}
/*- End of function --------------------------------------------------------*/

static int close_tiff_output_file(t4_state_t *s)
{
    int i;
    t4_tiff_state_t *t;

    t = &s->tiff;
    /* Perform any operations needed to tidy up a written TIFF file before
       closure. */
    if (s->current_page > 1)
    {
        /* We need to edit the TIFF directories. Until now we did not know
           the total page count, so the TIFF file currently says one. Now we
           need to set the correct total page count associated with each page. */
        for (i = 0;  i < s->current_page;  i++)
        {
            TIFFSetDirectory(t->tiff_file, (tdir_t) i);
            TIFFSetField(t->tiff_file, TIFFTAG_PAGENUMBER, i, s->current_page);
            TIFFWriteDirectory(t->tiff_file);
        }
    }
    TIFFClose(t->tiff_file);
    t->tiff_file = NULL;
    if (s->file)
    {
        /* Try not to leave a file behind, if we didn't receive any pages to
           put in it. */
        if (s->current_page == 0)
            remove(s->file);
        free((char *) s->file);
    }
    s->file = NULL;
    return 0;
}
/*- End of function --------------------------------------------------------*/

#else

static int set_tiff_directory_info(t4_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int get_tiff_directory_info(t4_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int test_tiff_directory_info(t4_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int open_tiff_input_file(t4_state_t *s, const char *file)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int read_tiff_image(t4_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int close_tiff_input_file(t4_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int open_tiff_output_file(t4_state_t *s, const char *file)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void write_tiff_image(t4_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int close_tiff_output_file(t4_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

static void update_row_bit_info(t4_state_t *s)
{
    if (s->row_bits > s->max_row_bits)
        s->max_row_bits = s->row_bits;
    if (s->row_bits < s->min_row_bits)
        s->min_row_bits = s->row_bits;
    s->row_bits = 0;
}
/*- End of function --------------------------------------------------------*/

#if defined(__i386__)  ||  defined(__x86_64__)  ||  defined(__ppc__)  ||   defined(__powerpc__)
static __inline__ int run_length(unsigned int bits)
{
    return 7 - top_bit(bits);
}
/*- End of function --------------------------------------------------------*/
#else
static __inline__ int run_length(unsigned int bits)
{
    static const uint8_t run_len[256] =
    {
        8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, /* 0x00 - 0x0F */
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 0x10 - 0x1F */
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0x20 - 0x2F */
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0x30 - 0x3F */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x40 - 0x4F */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x50 - 0x5F */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x60 - 0x6F */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x70 - 0x7F */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x80 - 0x8F */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x90 - 0x9F */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xA0 - 0xAF */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xB0 - 0xBF */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xC0 - 0xCF */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xD0 - 0xDF */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xE0 - 0xEF */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xF0 - 0xFF */
    };

    return run_len[bits];
}
/*- End of function --------------------------------------------------------*/
#endif

static int row_to_run_lengths(uint32_t list[], const uint8_t row[], int width)
{
    uint32_t flip;
    uint32_t x;
    int span;
    int entry;
    int frag;
    int rem;
    int limit;
    int i;
    int pos;

    /* Deal with whole words first. We know we are starting on a word boundary. */
    entry = 0;
    flip = 0;
    limit = (width >> 3) & ~3;
    span = 0;
    pos = 0;
    for (i = 0;  i < limit;  i += sizeof(uint32_t))
    {
        x = *((uint32_t *) &row[i]);
        if (x != flip)
        {
            x = ((uint32_t) row[i] << 24) | ((uint32_t) row[i + 1] << 16) | ((uint32_t) row[i + 2] << 8) | ((uint32_t) row[i + 3]);
            /* We know we are going to find at least one transition. */
            frag = 31 - top_bit(x ^ flip);
            pos += ((i << 3) - span + frag);
            list[entry++] = pos;
            x <<= frag;
            flip ^= 0xFFFFFFFF;
            rem = 32 - frag;
            /* Now see if there are any more */
            while ((frag = 31 - top_bit(x ^ flip)) < rem)
            {
                pos += frag;
                list[entry++] = pos;
                x <<= frag;
                flip ^= 0xFFFFFFFF;
                rem -= frag;
            }
            /* Save the remainder of the word */
            span = (i << 3) + 32 - rem;
        }
    }
    /* Now deal with some whole bytes, if there are any left. */
    limit = width >> 3;
    flip &= 0xFF000000;
    if (i < limit)
    {
        for (  ;  i < limit;  i++)
        {
            x = (uint32_t) row[i] << 24;
            if (x != flip)
            {
                /* We know we are going to find at least one transition. */
                frag = 31 - top_bit(x ^ flip);
                pos += ((i << 3) - span + frag);
                list[entry++] = pos;
                x <<= frag;
                flip ^= 0xFF000000;
                rem = 8 - frag;
                /* Now see if there are any more */
                while ((frag = 31 - top_bit(x ^ flip)) < rem)
                {
                    pos += frag;
                    list[entry++] = pos;
                    x <<= frag;
                    flip ^= 0xFF000000;
                    rem -= frag;
                }   
                /* Save the remainder of the word */
                span = (i << 3) + 8 - rem;
            }
        }
    }
    /* Deal with any left over fractional byte. */
    span = (i << 3) - span;
    if ((rem = width & 7))
    {
        x = row[i];
        x <<= 24;
        do
        {
            frag = 31 - top_bit(x ^ flip);
            if (frag > rem)
                frag = rem;
            pos += (span + frag);
            list[entry++] = pos;
            x <<= frag;
            span = 0;
            flip ^= 0xFF000000;
            rem -= frag;
        }
        while (rem > 0);
    }
    else
    {
        if (span)
        {
            pos += span;
            list[entry++] = pos;
        }
    }
    return entry;
}
/*- End of function --------------------------------------------------------*/

static int free_buffers(t4_state_t *s)
{
    if (s->image_buffer)
    {
        free(s->image_buffer);
        s->image_buffer = NULL;
        s->image_buffer_size = 0;
    }
    if (s->cur_runs)
    {
        free(s->cur_runs);
        s->cur_runs = NULL;
    }
    if (s->ref_runs)
    {
        free(s->ref_runs);
        s->ref_runs = NULL;
    }
    if (s->row_buf)
    {
        free(s->row_buf);
        s->row_buf = NULL;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void add_run_to_row(t4_state_t *s)
{
    if (s->run_length >= 0)
    {
        s->row_len += s->run_length;
        /* Don't allow rows to grow too long, and overflow the buffers */
        if (s->row_len <= s->image_width)
            s->cur_runs[s->a_cursor++] = s->run_length;
    }
    s->run_length = 0;
}
/*- End of function --------------------------------------------------------*/

static int put_decoded_row(t4_state_t *s)
{
    static const int msbmask[9] =
    {
        0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
    };
    uint8_t *t;
    uint32_t i;
    uint32_t *p;
    int fudge;
    int row_starts_at;
    int x;
    int j;

    if (s->run_length)
        add_run_to_row(s);
#if defined(T4_STATE_DEBUGGING)
    /* Dump the runs of black and white for analysis */
    {
        int total;

        total = 0;
        for (x = 0;  x < s->b_cursor;  x++)
            total += s->ref_runs[x];
        printf("Ref (%d)", total);
        for (x = 0;  x < s->b_cursor;  x++)
            printf(" %" PRIu32, s->ref_runs[x]);
        printf("\n");
        total = 0;
        for (x = 0;  x < s->a_cursor;  x++)
            total += s->cur_runs[x];
        printf("Cur (%d)", total);
        for (x = 0;  x < s->a_cursor;  x++)
            printf(" %" PRIu32, s->cur_runs[x]);
        printf("\n");
    }
#endif
    row_starts_at = s->image_size;
    /* Make sure there is enough room for another row */
    if (s->image_size + s->bytes_per_row >= s->image_buffer_size)
    {
        if ((t = realloc(s->image_buffer, s->image_buffer_size + 100*s->bytes_per_row)) == NULL)
            return -1;
        s->image_buffer_size += 100*s->bytes_per_row;
        s->image_buffer = t;
    }
    if (s->row_len == s->image_width)
    {
        STATE_TRACE("%d Good row - %d %s\n", s->image_length, s->row_len, (s->row_is_2d)  ?  "2D"  :  "1D");
        if (s->curr_bad_row_run)
        {
            if (s->curr_bad_row_run > s->longest_bad_row_run)
                s->longest_bad_row_run = s->curr_bad_row_run;
            s->curr_bad_row_run = 0;
        }
        /* Convert the runs to a bit image of the row */
        /* White/black/white... runs, always starting with white. That means the first run could be
           zero length. */
        for (x = 0, fudge = 0;  x < s->a_cursor;  x++, fudge ^= 0xFF)
        {
            i = s->cur_runs[x];
            if ((int) i >= s->tx_bits)
            {
                s->tx_bitstream = (s->tx_bitstream << s->tx_bits) | (msbmask[s->tx_bits] & fudge);
                for (i += (8 - s->tx_bits);  i >= 8;  i -= 8)
                {
                    s->tx_bits = 8;
                    s->image_buffer[s->image_size++] = (uint8_t) s->tx_bitstream;
                    s->tx_bitstream = fudge;
                }
            }
            s->tx_bitstream = (s->tx_bitstream << i) | (msbmask[i] & fudge);
            s->tx_bits -= i;
        }
        s->image_length++;
    }
    else
    {
        STATE_TRACE("%d Bad row - %d %s\n", s->image_length, s->row_len, (s->row_is_2d)  ?  "2D"  :  "1D");
        /* Try to clean up the bad runs, and produce something reasonable as the reference
           row for the next row. Use a copy of the previous good row as the actual current
           row. If the row only fell apart near the end, reusing it might be the best
           solution. */
        for (j = 0, fudge = 0;  j < s->a_cursor  &&  fudge < s->image_width;  j++)
            fudge += s->cur_runs[j];
        if (fudge < s->image_width)
        {
            /* Try to pad with white, and avoid black, to minimise mess on the image. */
            if ((s->a_cursor & 1))
            {
                /* We currently finish in white. We could extend that, but it is probably of
                   the right length. Changing it would only further mess up what happens in the
                   next row. It seems better to add a black spot, and an extra white run. */
                s->cur_runs[s->a_cursor++] = 1;
                fudge++;
                if (fudge < s->image_width)
                    s->cur_runs[s->a_cursor++] = s->image_width - fudge;
            }
            else
            {
                /* We currently finish on black, so we add an extra white run to fill out the line. */
                s->cur_runs[s->a_cursor++] = s->image_width - fudge;
            }
        }
        else
        {
            /* Trim the last element to align with the proper image width */
            s->cur_runs[s->a_cursor] += (s->image_width - fudge);
        }
        /* Ensure there is a previous line to copy from. */
        if (s->image_size != s->last_row_starts_at)
        {
            /* Copy the previous row over this one */
            memcpy(s->image_buffer + s->image_size, s->image_buffer + s->last_row_starts_at, s->bytes_per_row);
            s->image_size += s->bytes_per_row;
            s->image_length++;
        }
        s->bad_rows++;
        s->curr_bad_row_run++;
    }

    /* Pad the row as it becomes the reference row, so there are no odd runs to pick up if we
       step off the end of the list. */
    s->cur_runs[s->a_cursor] = 0;
    s->cur_runs[s->a_cursor + 1] = 0;

    /* Prepare the buffers for the next row. */
    s->last_row_starts_at = row_starts_at;
    /* Swap the buffers */
    p = s->cur_runs;
    s->cur_runs = s->ref_runs;
    s->ref_runs = p;

    s->b_cursor = 1;
    s->a_cursor = 0;
    s->b1 = s->ref_runs[0];
    s->a0 = 0;
    
    s->run_length = 0;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_end_page(t4_state_t *s)
{
    int row;
    int i;

    if (s->line_encoding == T4_COMPRESSION_ITU_T6)
    {
        /* Push enough zeros through the decoder to flush out any remaining codes */
        for (i = 0;  i < 13;  i++)
            t4_rx_put_bit(s, 0);
    }
    if (s->curr_bad_row_run)
    {
        if (s->curr_bad_row_run > s->longest_bad_row_run)
            s->longest_bad_row_run = s->curr_bad_row_run;
        s->curr_bad_row_run = 0;
    }

    if (s->image_size == 0)
        return -1;

    if (s->row_write_handler)
    {
        for (row = 0;  row < s->image_length;  row++)
        {
            if (s->row_write_handler(s->row_write_user_data, s->image_buffer + row*s->bytes_per_row, s->bytes_per_row) < 0)
            {
                span_log(&s->logging, SPAN_LOG_WARNING, "Write error at row %d.\n", row);
                break;
            }
        }
        /* Write a blank row to indicate the end of the image. */
        if (s->row_write_handler(s->row_write_user_data, NULL, 0) < 0)
            span_log(&s->logging, SPAN_LOG_WARNING, "Write error at row %d.\n", row);
    }
    else
    {
        write_tiff_image(s);
    }
    s->rx_bits = 0;
    s->rx_skip_bits = 0;
    s->rx_bitstream = 0;
    s->consecutive_eols = EOLS_TO_END_ANY_RX_PAGE;

    s->image_size = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void drop_rx_bits(t4_state_t *s, int bits)
{
    /* Only remove one bit right now. The rest need to be removed step by step,
       checking for a misaligned EOL along the way. This is time consuming, but
       if we don't do it a single bit error can severely damage an image. */
    s->row_bits += bits;
    s->rx_skip_bits += (bits - 1);
    s->rx_bits--;
    s->rx_bitstream >>= 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void force_drop_rx_bits(t4_state_t *s, int bits)
{
    /* This should only be called to drop the bits of an EOL, as that is the
       only place where it is safe to drop them all at once. */
    s->row_bits += bits;
    s->rx_skip_bits = 0;
    s->rx_bits -= bits;
    s->rx_bitstream >>= bits;
}
/*- End of function --------------------------------------------------------*/

static int rx_put_bits(t4_state_t *s, uint32_t bit_string, int quantity)
{
    int bits;

    /* We decompress bit by bit, as the data stream is received. We need to
       scan continuously for EOLs, so we might as well work this way. */
    s->line_image_size += quantity;
    s->rx_bitstream |= (bit_string << s->rx_bits);
    /* The longest item we need to scan for is 13 bits long (a 2D EOL), so we
       need a minimum of 13 bits in the buffer to proceed with any bit stream
       analysis. */
    if ((s->rx_bits += quantity) < 13)
        return FALSE;
    if (s->consecutive_eols)
    {
        /* Check if the image has already terminated. */
        if (s->consecutive_eols >= EOLS_TO_END_ANY_RX_PAGE)
            return TRUE;
        /* Check if the image hasn't even started. */
        if (s->consecutive_eols < 0)
        {
            /* We are waiting for the very first EOL (1D or 2D only). */
            /* We need to take this bit by bit, as the EOL could be anywhere,
               and any junk could preceed it. */
            while ((s->rx_bitstream & 0xFFF) != 0x800)
            {
                s->rx_bitstream >>= 1;
                if (--s->rx_bits < 13)
                    return FALSE;
            }
            /* We have an EOL, so now the page begins and we can proceed to
               process the bit stream as image data. */
            s->consecutive_eols = 0;
            if (s->line_encoding == T4_COMPRESSION_ITU_T4_1D)
            {
                s->row_is_2d = FALSE;
                force_drop_rx_bits(s, 12);
            }
            else
            {
                s->row_is_2d = !(s->rx_bitstream & 0x1000);
                force_drop_rx_bits(s, 13);
            }
        }
    }

    while (s->rx_bits >= 13)
    {
        /* We need to check for EOLs bit by bit through the whole stream. If
           we just try looking between code words, we will miss an EOL when a bit
           error has throw the code words completely out of step. The can mean
           recovery takes many lines, and the image gets really messed up. */
        /* Although EOLs are not inserted at the end of each row of a T.6 image,
           they are still perfectly valid, and can terminate an image. */
        if ((s->rx_bitstream & 0x0FFF) == 0x0800)
        {
            STATE_TRACE("EOL\n");
            if (s->row_len == 0)
            {
                /* A zero length row - i.e. 2 consecutive EOLs - is distinctly
                   the end of page condition. That's all we actually get on a
                   T.6 page. However, there are a minimum of 6 EOLs at the end of
                   any T.4 page. We can look for more than 2 EOLs in case bit
                   errors simulate the end of page condition at the wrong point.
                   Such robust checking is irrelevant for a T.6 page, as it should
                   be error free. */
                /* Note that for a T.6 page we should get here on the very first
                   EOL, as the row length should be zero at that point. Therefore
                   we should count up both EOLs, unless there is some bogus partial
                   row ahead of them. */
                s->consecutive_eols++;
                if (s->line_encoding == T4_COMPRESSION_ITU_T6)
                {
                    if (s->consecutive_eols >= EOLS_TO_END_T6_RX_PAGE)
                    {
                        s->consecutive_eols = EOLS_TO_END_ANY_RX_PAGE;
                        return TRUE;
                    }
                }
                else
                {
                    if (s->consecutive_eols >= EOLS_TO_END_T4_RX_PAGE)
                    {
                        s->consecutive_eols = EOLS_TO_END_ANY_RX_PAGE;
                        return TRUE;
                    }
                }
            }
            else
            {
                /* The EOLs are not back-to-back, so they are not part of the
                   end of page condition. */
                if (s->run_length > 0)
                    add_run_to_row(s);
                s->consecutive_eols = 0;
                if (put_decoded_row(s))
                    return TRUE;
                update_row_bit_info(s);
            }
            if (s->line_encoding == T4_COMPRESSION_ITU_T4_2D)
            {
                s->row_is_2d = !(s->rx_bitstream & 0x1000);
                force_drop_rx_bits(s, 13);
            }
            else
            {
                force_drop_rx_bits(s, 12);
            }
            s->its_black = FALSE;
            s->black_white = 0;
            s->run_length = 0;
            s->row_len = 0;
            continue;
        }
        if (s->rx_skip_bits)
        {
            /* We are clearing out the remaining bits of the last code word we
               absorbed. */
            s->rx_skip_bits--;
            s->rx_bits--;
            s->rx_bitstream >>= 1;
            continue;
        }
        if (s->row_is_2d  &&  s->black_white == 0)
        {
            bits = s->rx_bitstream & 0x7F;
            STATE_TRACE("State %d, %d - ",
                        t4_2d_table[bits].state,
                        t4_2d_table[bits].width);
            if (s->row_len >= s->image_width)
            {
                drop_rx_bits(s, t4_2d_table[bits].width);
                continue;
            }
            if (s->a_cursor)
            {
                /* Move past a0, always staying on the current colour */
                for (  ;  s->b1 <= s->a0;  s->b_cursor += 2)
                    s->b1 += (s->ref_runs[s->b_cursor] + s->ref_runs[s->b_cursor + 1]);
            }
            switch (t4_2d_table[bits].state)
            {
            case S_Horiz:
                STATE_TRACE("Horiz %d %d %d\n",
                            s->image_width,
                            s->a0,
                            s->a_cursor);
                /* We now need to extract a white/black or black/white pair of runs, using the 1D
                   method. If the first of the pair takes us exactly to the end of the row, there
                   should still be a zero length element for the second of the pair. */
                s->its_black = s->a_cursor & 1;
                s->black_white = 2;
                break;
            case S_Vert:
                STATE_TRACE("Vert[%d] %d %d %d %d\n",
                            t4_2d_table[bits].param,
                            s->image_width,
                            s->a0,
                            s->b1,
                            s->run_length);
                s->run_length += (s->b1 - s->a0 + t4_2d_table[bits].param);
                s->a0 = s->b1 + t4_2d_table[bits].param;
                add_run_to_row(s);
                /* We need to move one step in one direction or the other, to change to the
                   opposite colour */
                if (t4_2d_table[bits].param >= 0)
                {
                    s->b1 += s->ref_runs[s->b_cursor++];
                }
                else
                {
                    if (s->b_cursor)
                        s->b1 -= s->ref_runs[--s->b_cursor];
                }
                break;
            case S_Pass:
                STATE_TRACE("Pass %d %d %d %d %d\n",
                            s->image_width,
                            s->a0,
                            s->b1,
                            s->ref_runs[s->b_cursor],
                            s->ref_runs[s->b_cursor + 1]);
                s->b1 += s->ref_runs[s->b_cursor++];
                s->run_length += (s->b1 - s->a0);
                s->a0 = s->b1;
                s->b1 += s->ref_runs[s->b_cursor++];
                break;
            case S_Ext:
                /* We do not currently handle any kind of extension */
                STATE_TRACE("Ext %d %d %d 0x%x\n",
                            s->image_width,
                            s->a0,
                            ((s->rx_bitstream >> t4_2d_table[bits].width) & 0x7),
                            s->rx_bitstream);
                /* TODO: The uncompressed option should be implemented. */
                break;
            case S_Null:
                STATE_TRACE("Null\n");
                break;
            default:
                STATE_TRACE("Unexpected T.4 state\n");
                span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected T.4 state %d\n", t4_2d_table[bits].state);
                break;
            }
            drop_rx_bits(s, t4_2d_table[bits].width);
        }
        else
        {
            if (s->its_black)
            {
                bits = s->rx_bitstream & 0x1FFF;
                STATE_TRACE("State %d, %d - Black %d %d %d\n",
                            t4_1d_black_table[bits].state,
                            t4_1d_black_table[bits].width,
                            s->image_width,
                            s->a0,
                            t4_1d_black_table[bits].param);
                switch (t4_1d_black_table[bits].state)
                {
                case S_MakeUpB:
                case S_MakeUp:
                    s->run_length += t4_1d_black_table[bits].param;
                    s->a0 += t4_1d_black_table[bits].param;
                    break;
                case S_TermB:
                    s->its_black = FALSE;
                    if (s->row_len < s->image_width)
                    {
                        s->run_length += t4_1d_black_table[bits].param;
                        s->a0 += t4_1d_black_table[bits].param;
                        add_run_to_row(s);
                    }
                    if (s->black_white)
                        s->black_white--;
                    break;
                default:
                    /* Bad black */
                    s->black_white = 0;
                    break;
                }
                drop_rx_bits(s, t4_1d_black_table[bits].width);
            }
            else
            {
                bits = s->rx_bitstream & 0xFFF;
                STATE_TRACE("State %d, %d - White %d %d %d\n",
                            t4_1d_white_table[bits].state,
                            t4_1d_white_table[bits].width,
                            s->image_width,
                            s->a0,
                            t4_1d_white_table[bits].param);
                switch (t4_1d_white_table[bits].state)
                {
                case S_MakeUpW:
                case S_MakeUp:
                    s->run_length += t4_1d_white_table[bits].param;
                    s->a0 += t4_1d_white_table[bits].param;
                    break;
                case S_TermW:
                    s->its_black = TRUE;
                    if (s->row_len < s->image_width)
                    {
                        s->run_length += t4_1d_white_table[bits].param;
                        s->a0 += t4_1d_white_table[bits].param;
                        add_run_to_row(s);
                    }
                    if (s->black_white)
                        s->black_white--;
                    break;
                default:
                    /* Bad white */
                    s->black_white = 0;
                    break;
                }
                drop_rx_bits(s, t4_1d_white_table[bits].width);
            }
        }
        if (s->a0 >= s->image_width)
            s->a0 = s->image_width - 1;

        if (s->line_encoding == T4_COMPRESSION_ITU_T6)
        {
            /* T.6 has no EOL markers. We sense the end of a line by its length alone. */
            /* The last test here is a backstop protection, so a corrupt image cannot
               cause us to do bad things. Bad encoders have actually been seen, which
               demand such protection. */
            if (s->black_white == 0  &&  s->row_len >= s->image_width)
            {
                STATE_TRACE("EOL T.6\n");
                if (s->run_length > 0)
                    add_run_to_row(s);
                update_row_bit_info(s);
                if (put_decoded_row(s))
                    return TRUE;
                s->its_black = FALSE;
                s->black_white = 0;
                s->run_length = 0;
                s->row_len = 0;
            }
        }
    }
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_put_bit(t4_state_t *s, int bit)
{
    return rx_put_bits(s, bit & 1, 1);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_put_byte(t4_state_t *s, uint8_t byte)
{
    return rx_put_bits(s, byte & 0xFF, 8);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_put_chunk(t4_state_t *s, const uint8_t buf[], int len)
{
    int i;
    uint8_t byte;

    for (i = 0;  i < len;  i++)
    {
        byte = buf[i];
        if (rx_put_bits(s, byte & 0xFF, 8))
            return TRUE;
    }
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_set_row_write_handler(t4_state_t *s, t4_row_write_handler_t handler, void *user_data)
{
    s->row_write_handler = handler;
    s->row_write_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t4_state_t *) t4_rx_init(t4_state_t *s, const char *file, int output_encoding)
{
    if (s == NULL)
    {
        if ((s = (t4_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.4");
    s->rx = TRUE;
    
    span_log(&s->logging, SPAN_LOG_FLOW, "Start rx document\n");

    if (open_tiff_output_file(s, file) < 0)
        return NULL;

    /* Save the file name for logging reports. */
    s->file = strdup(file);
    /* Only provide for one form of coding throughout the file, even though the
       coding on the wire could change between pages. */
    switch (output_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
        s->tiff.output_compression = COMPRESSION_CCITT_T4;
        s->tiff.output_t4_options = GROUP3OPT_FILLBITS;
        break;
    case T4_COMPRESSION_ITU_T4_2D:
        s->tiff.output_compression = COMPRESSION_CCITT_T4;
        s->tiff.output_t4_options = GROUP3OPT_FILLBITS | GROUP3OPT_2DENCODING;
        break;
    case T4_COMPRESSION_ITU_T6:
        s->tiff.output_compression = COMPRESSION_CCITT_T6;
        s->tiff.output_t4_options = 0;
        break;
    }

    /* Until we have a valid figure for the bytes per row, we need it to be set to a suitable
       value to ensure it will be seen as changing when the real value is used. */
    s->bytes_per_row = 0;

    s->current_page = 0;
    s->pages_in_file = 0;
    s->start_page = 0;
    s->stop_page = INT_MAX;

    s->image_buffer = NULL;
    s->image_buffer_size = 0;

    /* Set some default values */
    s->x_resolution = T4_X_RESOLUTION_R8;
    s->y_resolution = T4_Y_RESOLUTION_FINE;
    s->image_width = T4_WIDTH_R8_A4;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_start_page(t4_state_t *s)
{
    int bytes_per_row;
    int run_space;
    uint32_t *bufptr;

    span_log(&s->logging, SPAN_LOG_FLOW, "Start rx page - compression %d\n", s->line_encoding);
    if (s->tiff.tiff_file == NULL)
        return -1;

    /* Calculate the scanline/tile width. */
    bytes_per_row = (s->image_width + 7)/8;
    run_space = (s->image_width + 4)*sizeof(uint32_t);
    if (bytes_per_row != s->bytes_per_row)
    {
        /* Allocate the space required for decoding the new row length. */
        s->bytes_per_row = bytes_per_row;
        if ((bufptr = (uint32_t *) realloc(s->cur_runs, run_space)) == NULL)
            return -1;
        s->cur_runs = bufptr;
        if ((bufptr = (uint32_t *) realloc(s->ref_runs, run_space)) == NULL)
            return -1;
        s->ref_runs = bufptr;
    }
    memset(s->cur_runs, 0, run_space);
    memset(s->ref_runs, 0, run_space);

    s->rx_bits = 0;
    s->rx_skip_bits = 0;
    s->rx_bitstream = 0;
    s->row_bits = 0;
    s->min_row_bits = INT_MAX;
    s->max_row_bits = 0;

    s->row_is_2d = (s->line_encoding == T4_COMPRESSION_ITU_T6);
    /* We start at -1 EOLs for 1D and 2D decoding, as an indication we are waiting for the
       first EOL. T.6 coding starts without any preamble. */
    s->consecutive_eols = (s->line_encoding == T4_COMPRESSION_ITU_T6)  ?  0  :  -1;

    s->bad_rows = 0;
    s->longest_bad_row_run = 0;
    s->curr_bad_row_run = 0;
    s->image_length = 0;
    s->tx_bitstream = 0;
    s->tx_bits = 8;
    s->image_size = 0;
    s->line_image_size = 0;
    s->last_row_starts_at = 0;

    s->row_len = 0;
    s->its_black = FALSE;
    s->black_white = 0;

    /* Initialise the reference line to all white */
    s->ref_runs[0] =
    s->ref_runs[1] =
    s->ref_runs[2] =
    s->ref_runs[3] = s->image_width;
    s->ref_steps = 1;

    s->b_cursor = 1;
    s->a_cursor = 0;
    s->b1 = s->ref_runs[0];
    s->a0 = 0;

    s->run_length = 0;

    time (&s->page_start_time);

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_release(t4_state_t *s)
{
    if (!s->rx)
        return -1;
    if (s->tiff.tiff_file)
        close_tiff_output_file(s);
    free_buffers(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_free(t4_state_t *s)
{
    int ret;

    ret = t4_rx_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_rx_encoding(t4_state_t *s, int encoding)
{
    s->line_encoding = encoding;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_image_width(t4_state_t *s, int width)
{
    s->image_width = width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_y_resolution(t4_state_t *s, int resolution)
{
    s->y_resolution = resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_x_resolution(t4_state_t *s, int resolution)
{
    s->x_resolution = resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_dcs(t4_state_t *s, const char *dcs)
{
    s->tiff.dcs = (dcs  &&  dcs[0])  ?  dcs  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_sub_address(t4_state_t *s, const char *sub_address)
{
    s->tiff.sub_address = (sub_address  &&  sub_address[0])  ?  sub_address  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_far_ident(t4_state_t *s, const char *ident)
{
    s->tiff.far_ident = (ident  &&  ident[0])  ?  ident  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_vendor(t4_state_t *s, const char *vendor)
{
    s->tiff.vendor = vendor;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_model(t4_state_t *s, const char *model)
{
    s->tiff.model = model;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int put_encoded_bits(t4_state_t *s, uint32_t bits, int length)
{
    uint8_t *t;

    /* We might be called with a large length value, to spew out a mass of zero bits for
       minimum row length padding. */
    s->tx_bitstream |= (bits << s->tx_bits);
    s->tx_bits += length;
    s->row_bits += length;
    if ((s->image_size + (s->tx_bits + 7)/8) >= s->image_buffer_size)
    {
        if ((t = realloc(s->image_buffer, s->image_buffer_size + 100*s->bytes_per_row)) == NULL)
            return -1;
        s->image_buffer = t;
        s->image_buffer_size += 100*s->bytes_per_row;
    }
    while (s->tx_bits >= 8)
    {
        s->image_buffer[s->image_size++] = (uint8_t) (s->tx_bitstream & 0xFF);
        s->tx_bitstream >>= 8;
        s->tx_bits -= 8;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

/*
 * Write the sequence of codes that describes
 * the specified span of zero's or one's.  The
 * appropriate table that holds the make-up and
 * terminating codes is supplied.
 */
static __inline__ int put_1d_span(t4_state_t *s, int32_t span, const t4_run_table_entry_t *tab)
{
    const t4_run_table_entry_t *te;

    te = &tab[63 + (2560 >> 6)];
    while (span >= 2560 + 64)
    {
        if (put_encoded_bits(s, te->code, te->length))
            return -1;
        span -= te->run_length;
    }
    te = &tab[63 + (span >> 6)];
    if (span >= 64)
    {
        if (put_encoded_bits(s, te->code, te->length))
            return -1;
        span -= te->run_length;
    }
    if (put_encoded_bits(s, tab[span].code, tab[span].length))
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

#define pixel_is_black(x,bit) (((x)[(bit) >> 3] << ((bit) & 7)) & 0x80)

/*
 * Write an EOL code to the output stream.  We also handle writing the tag
 * bit for the next scanline when doing 2D encoding.
 */
static void encode_eol(t4_state_t *s)
{
    uint32_t code;
    int length;

    if (s->line_encoding == T4_COMPRESSION_ITU_T4_2D)
    {
        code = 0x0800 | ((!s->row_is_2d) << 12);
        length = 13;
    }
    else
    {
        /* T.4 1D EOL, or T.6 EOFB */
        code = 0x800;
        length = 12;
    }
    if (s->row_bits)
    {
        /* We may need to pad the row to a minimum length, unless we are in T.6 mode.
           In T.6 we only come here at the end of the page to add the EOFB marker, which
           is like two 1D EOLs. */
        if (s->line_encoding != T4_COMPRESSION_ITU_T6)
        {
            if (s->row_bits + length < s->min_bits_per_row)
                put_encoded_bits(s, 0, s->min_bits_per_row - (s->row_bits + length));
        }
        put_encoded_bits(s, code, length);
        update_row_bit_info(s);
    }
    else
    {
        /* We don't pad zero length rows. They are the consecutive EOLs which end a page. */
        put_encoded_bits(s, code, length);
        /* Don't do the full update row bit info, or the minimum suddenly drops to the
           length of an EOL. Just clear the row bits, so we treat the next EOL as an
           end of page EOL, with no padding. */
        s->row_bits = 0;
    }
}
/*- End of function --------------------------------------------------------*/

/*
 * 2D-encode a row of pixels.  Consult ITU specification T.4 for the algorithm.
 */
static void encode_2d_row(t4_state_t *s)
{
    static const t4_run_table_entry_t codes[] =
    {
        { 7, 0x60, 0 },         /* VR3          0000 011 */
        { 6, 0x30, 0 },         /* VR2          0000 11 */
        { 3, 0x06, 0 },         /* VR1          011 */
        { 1, 0x01, 0 },         /* V0           1 */
        { 3, 0x02, 0 },         /* VL1          010 */
        { 6, 0x10, 0 },         /* VL2          0000 10 */
        { 7, 0x20, 0 },         /* VL3          0000 010 */
        { 3, 0x04, 0 },         /* horizontal   001 */
        { 4, 0x08, 0 }          /* pass         0001 */
    };

    /* The reference or starting changing element on the coding line. At the start of the coding
       line, a0 is set on an imaginary white changing element situated just before the first element
       on the line. During the coding of the coding line, the position of a0 is defined by the
       previous coding mode. (See T.4/4.2.1.3.2.) */
    int a0;
    /* The next changing element to the right of a0 on the coding line. */
    int a1;
    /* The next changing element to the right of a1 on the coding line. */
    int a2;
    /* The first changing element on the reference line to the right of a0 and of opposite colour to a0. */
    int b1;
    /* The next changing element to the right of b1 on the reference line. */
    int b2;
    int diff;
    int a_cursor;
    int b_cursor;
    int cur_steps;
    uint32_t *p;

    /*
                                                    b1          b2 
            XX  XX  XX  XX  XX  --  --  --  --  --  XX  XX  XX  --  --  --  --  --
            XX  XX  XX  --  --  --  --  --  XX  XX  XX  XX  XX  XX  --  --  --  --
                        a0                  a1                      a2


        a)  Pass mode
            This mode is identified when the position of b2 lies to the left of a1. When this mode
            has been coded, a0 is set on the element of the coding line below b2 in preparation for
            the next coding (i.e. on a0').
            
                                    b1          b2 
            XX  XX  XX  XX  --  --  XX  XX  XX  --  --  --  --  --
            XX  XX  --  --  --  --  --  --  --  --  --  --  XX  XX 
                    a0                          a0'         a1
                                Pass mode
                                

            However, the state where b2 occurs just above a1, as shown in the figure below, is not
            considered as a pass mode.

                                    b1          b2 
            XX  XX  XX  XX  --  --  XX  XX  XX  --  --  --  --  --
            XX  XX  --  --  --  --  --  --  --  XX  XX  XX  XX  XX
                    a0                          a1
                                Not pass mode


        b)  Vertical mode
            When this mode is identified, the position of a1 is coded relative to the position of b1.
            The relative distance a1b1 can take on one of seven values V(0), VR(1), VR(2), VR(3),
            VL(1), VL(2) and VL(3), each of which is represented by a separate code word. The
            subscripts R and L indicate that a1 is to the right or left respectively of b1, and the
            number in brackets indicates the value of the distance a1b1. After vertical mode coding
            has occurred, the position of a0 is set on a1 (see figure below).

        c)  Horizontal mode
            When this mode is identified, both the run-lengths a0a1 and a1a2 are coded using the code
            words H + M(a0a1) + M(a1a2). H is the flag code word 001 taken from the two-dimensional
            code table. M(a0a1) and M(a1a2) are code words which represent the length and "colour"
            of the runs a0a1 and a1a2 respectively and are taken from the appropriate white or black
            one-dimensional code tables. After a horizontal mode coding, the position of a0 is set on
            a2 (see figure below).

                                                            Vertical
                                                            <a1 b1>
                                                                    b1              b2 
            --  XX  XX  XX  XX  XX  --  --  --  --  --  --  --  --  XX  XX  XX  XX  --  --  --
            --  --  --  --  --  --  --  --  --  --  --  --  XX  XX  XX  XX  XX  XX  XX  --  --
                                    a0                      a1                          a2
                                   <-------- a0a1 --------><-------- a1a2 ------------>
                                                    Horizontal mode
                          Vertical and horizontal modes
     */
    /* The following implements the 2-D encoding section of the flow chart in Figure7/T.4 */
    cur_steps = row_to_run_lengths(s->cur_runs, s->row_buf, s->image_width);
    /* Stretch the row a little, so when we step by 2 we are guaranteed to
       hit an entry showing the row length */
    s->cur_runs[cur_steps] =
    s->cur_runs[cur_steps + 1] =
    s->cur_runs[cur_steps + 2] = s->cur_runs[cur_steps - 1];

    a0 = 0;
    a1 = s->cur_runs[0];
    b1 = s->ref_runs[0];
    a_cursor = 0;
    b_cursor = 0;
    for (;;)
    {
        b2 = s->ref_runs[b_cursor + 1];
        if (b2 >= a1)
        {
            diff = b1 - a1;
            if (abs(diff) <= 3)
            {
                /* Vertical mode coding */
                put_encoded_bits(s, codes[diff + 3].code, codes[diff + 3].length);
                a0 = a1;
                a_cursor++;
            }
            else
            {
                /* Horizontal mode coding */
                a2 = s->cur_runs[a_cursor + 1];
                put_encoded_bits(s, codes[7].code, codes[7].length);
                if (a0 + a1 == 0  ||  pixel_is_black(s->row_buf, a0) == 0)
                {
                    put_1d_span(s, a1 - a0, t4_white_codes);
                    put_1d_span(s, a2 - a1, t4_black_codes);
                }
                else
                {
                    put_1d_span(s, a1 - a0, t4_black_codes);
                    put_1d_span(s, a2 - a1, t4_white_codes);
                }
                a0 = a2;
                a_cursor += 2;
            }
            if (a0 >= s->image_width)
                break;
            if (a_cursor >= cur_steps)
                a_cursor = cur_steps - 1;
            a1 = s->cur_runs[a_cursor];
        }
        else
        {
            /* Pass mode coding */
            put_encoded_bits(s, codes[8].code, codes[8].length);
            /* We now set a0 to somewhere in the middle of its current run,
               but we know are aren't moving beyond that run. */
            a0 = b2;
            if (a0 >= s->image_width)
                break;
        }
        /* We need to hunt for the correct position in the reference row, as the
           runs there have no particular alignment with the runs in the current
           row. */
        if (pixel_is_black(s->row_buf, a0))
            b_cursor |= 1;
        else
            b_cursor &= ~1;
        if (a0 < (int) s->ref_runs[b_cursor])
        {
            for (  ;  b_cursor >= 0;  b_cursor -= 2)
            {
                if (a0 >= (int) s->ref_runs[b_cursor])
                    break;
            }
            b_cursor += 2;
        }
        else
        {
            for (  ;  b_cursor < s->ref_steps;  b_cursor += 2)
            {
                if (a0 < (int) s->ref_runs[b_cursor])
                    break;
            }
            if (b_cursor >= s->ref_steps)
                b_cursor = s->ref_steps - 1;
        }
        b1 = s->ref_runs[b_cursor];
    }
    /* Swap the buffers */
    s->ref_steps = cur_steps;
    p = s->cur_runs;
    s->cur_runs = s->ref_runs;
    s->ref_runs = p;
}
/*- End of function --------------------------------------------------------*/

/*
 * 1D-encode a row of pixels.  The encoding is
 * a sequence of all-white or all-black spans
 * of pixels encoded with Huffman codes.
 */
static void encode_1d_row(t4_state_t *s)
{
    int i;

    /* Do our work in the reference row buffer, and it is already in place if
       we need a reference row for a following 2D encoded row. */
    s->ref_steps = row_to_run_lengths(s->ref_runs, s->row_buf, s->image_width);
    put_1d_span(s, s->ref_runs[0], t4_white_codes);
    for (i = 1;  i < s->ref_steps;  i++)
        put_1d_span(s, s->ref_runs[i] - s->ref_runs[i - 1], (i & 1)  ?  t4_black_codes  :  t4_white_codes);
    /* Stretch the row a little, so when we step by 2 we are guaranteed to
       hit an entry showing the row length */
    s->ref_runs[s->ref_steps] =
    s->ref_runs[s->ref_steps + 1] =
    s->ref_runs[s->ref_steps + 2] = s->ref_runs[s->ref_steps - 1];
}
/*- End of function --------------------------------------------------------*/

static int encode_row(t4_state_t *s)
{
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_ITU_T6:
        /* T.6 compression is a trivial step up from T.4 2D, so we just
           throw it in here. T.6 is only used with error correction,
           so it does not need independantly compressed (i.e. 1D) lines
           to recover from data errors. It doesn't need EOLs, either. */
        if (s->row_bits)
            update_row_bit_info(s);
        encode_2d_row(s);
        break;
    case T4_COMPRESSION_ITU_T4_2D:
        encode_eol(s);
        if (s->row_is_2d)
        {
            encode_2d_row(s);
            s->rows_to_next_1d_row--;
        }
        else
        {
            encode_1d_row(s);
            s->row_is_2d = TRUE;
        }
        if (s->rows_to_next_1d_row <= 0)
        {
            /* Insert a row of 1D encoding */
            s->row_is_2d = FALSE;
            s->rows_to_next_1d_row = s->max_rows_to_next_1d_row - 1;
        }
        break;
    default:
    case T4_COMPRESSION_ITU_T4_1D:
        encode_eol(s);
        encode_1d_row(s);
        break;
    }
    s->row++;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_set_row_read_handler(t4_state_t *s, t4_row_read_handler_t handler, void *user_data)
{
    s->row_read_handler = handler;
    s->row_read_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t4_state_t *) t4_tx_init(t4_state_t *s, const char *file, int start_page, int stop_page)
{
    int run_space;

    if (s == NULL)
    {
        if ((s = (t4_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.4");
    s->rx = FALSE;

    span_log(&s->logging, SPAN_LOG_FLOW, "Start tx document\n");

    if (open_tiff_input_file(s, file) < 0)
        return NULL;
    s->file = strdup(file);
    s->current_page =
    s->start_page = (start_page >= 0)  ?  start_page  :  0;
    s->stop_page = (stop_page >= 0)  ?  stop_page : INT_MAX;

    if (!TIFFSetDirectory(s->tiff.tiff_file, (tdir_t) s->current_page))
        return NULL;
    if (get_tiff_directory_info(s))
    {
        close_tiff_input_file(s);
        return NULL;
    }

    s->rows_to_next_1d_row = s->max_rows_to_next_1d_row - 1;

    s->pages_in_file = -1;

    run_space = (s->image_width + 4)*sizeof(uint32_t);
    if ((s->cur_runs = (uint32_t *) malloc(run_space)) == NULL)
        return NULL;
    if ((s->ref_runs = (uint32_t *) malloc(run_space)) == NULL)
    {
        free_buffers(s);
        close_tiff_input_file(s);
        return NULL;
    }
    if ((s->row_buf = malloc(s->bytes_per_row)) == NULL)
    {
        free_buffers(s);
        close_tiff_input_file(s);
        return NULL;
    }
    s->ref_runs[0] =
    s->ref_runs[1] =
    s->ref_runs[2] =
    s->ref_runs[3] = s->image_width;
    s->ref_steps = 1;
    s->image_buffer_size = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

static void make_header(t4_state_t *s, char *header)
{
    time_t now;
    struct tm tm;
    static const char *months[] =
    {
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec"
    };

    time(&now);
    tm = *localtime(&now);
    snprintf(header,
             132,
             "  %2d-%s-%d  %02d:%02d    %-50s %-21s   p.%d",
             tm.tm_mday,
             months[tm.tm_mon],
             tm.tm_year + 1900,
             tm.tm_hour,
             tm.tm_min,
             s->header_info,
             s->tiff.local_ident,
             s->current_page + 1);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_start_page(t4_state_t *s)
{
    int row;
    int i;
    int repeats;
    int pattern;
    int row_bufptr;
    int run_space;
    int len;
    int old_image_width;
    char *t;
    char header[132 + 1];
    uint8_t *bufptr8;
    uint32_t *bufptr;

    span_log(&s->logging, SPAN_LOG_FLOW, "Start tx page %d\n", s->current_page);
    if (s->current_page > s->stop_page)
        return -1;
    if (s->tiff.tiff_file == NULL)
        return -1;
    old_image_width = s->image_width;
    if (s->row_read_handler == NULL)
    {
#if defined(HAVE_LIBTIFF)
        if (!TIFFSetDirectory(s->tiff.tiff_file, (tdir_t) s->current_page))
            return -1;
        get_tiff_directory_info(s);
#endif
    }
    s->image_size = 0;
    s->tx_bitstream = 0;
    s->tx_bits = 0;
    s->row_is_2d = (s->line_encoding == T4_COMPRESSION_ITU_T6);
    s->rows_to_next_1d_row = s->max_rows_to_next_1d_row - 1;

    /* Allow for pages being of different width. */
    run_space = (s->image_width + 4)*sizeof(uint32_t);
    if (old_image_width != s->image_width)
    {
        s->bytes_per_row = (s->image_width + 7)/8;

        if ((bufptr = (uint32_t *) realloc(s->cur_runs, run_space)) == NULL)
            return -1;
        s->cur_runs = bufptr;
        if ((bufptr = (uint32_t *) realloc(s->ref_runs, run_space)) == NULL)
            return -1;
        s->ref_runs = bufptr;
        if ((bufptr8 = realloc(s->row_buf, s->bytes_per_row)) == NULL)
            return -1;
        s->row_buf = bufptr8;
    }
    s->ref_runs[0] =
    s->ref_runs[1] =
    s->ref_runs[2] =
    s->ref_runs[3] = s->image_width;
    s->ref_steps = 1;

    s->row_bits = 0;
    s->min_row_bits = INT_MAX;
    s->max_row_bits = 0;

    if (s->header_info  &&  s->header_info[0])
    {
        /* Modify the resulting image to include a header line, typical of hardware FAX machines */
        make_header(s, header);
        switch (s->y_resolution)
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
        for (row = 0;  row < 16;  row++)
        {
            t = header;
            row_bufptr = 0;
            for (t = header;  *t  &&  row_bufptr <= s->bytes_per_row - 2;  t++)
            {
                pattern = header_font[(uint8_t) *t][row];
                s->row_buf[row_bufptr++] = (uint8_t) (pattern >> 8);
                s->row_buf[row_bufptr++] = (uint8_t) (pattern & 0xFF);
            }
            for (  ;  row_bufptr < s->bytes_per_row;  )
                s->row_buf[row_bufptr++] = 0;
            for (i = 0;  i < repeats;  i++)
            {
                if (encode_row(s))
                    return -1;
            }
        }
    }
    if (s->row_read_handler)
    {
        for (row = 0;  ;  row++)
        {
            if ((len = s->row_read_handler(s->row_read_user_data, s->row_buf, s->bytes_per_row)) < 0)
            {
                span_log(&s->logging, SPAN_LOG_WARNING, "%s: Read error at row %d.\n", s->file, row);
                break;
            }
            if (len == 0)
                break;
            if (encode_row(s))
                return -1;
        }
        s->image_length = row;
    }
    else
    {
        if ((s->image_length = read_tiff_image(s)) < 0)
            return -1;
    }
    if (s->line_encoding == T4_COMPRESSION_ITU_T6)
    {
        /* Attach an EOFB (end of facsimile block == 2 x EOLs) to the end of the page */
        for (i = 0;  i < EOLS_TO_END_T6_TX_PAGE;  i++)
            encode_eol(s);
    }
    else
    {
        /* Attach an RTC (return to control == 6 x EOLs) to the end of the page */
        s->row_is_2d = FALSE;
        for (i = 0;  i < EOLS_TO_END_T4_TX_PAGE;  i++)
            encode_eol(s);
    }

    /* Force any partial byte in progress to flush using ones. Any post EOL padding when
       sending is normally ones, so this is consistent. */
    put_encoded_bits(s, 0xFF, 7);
    s->bit_pos = 7;
    s->bit_ptr = 0;
    s->line_image_size = s->image_size*8;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_next_page_has_different_format(t4_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Checking for the existance of page %d\n", s->current_page + 1);
    if (s->current_page >= s->stop_page)
        return -1;
    if (s->row_read_handler == NULL)
    {
#if defined(HAVE_LIBTIFF)
        if (s->tiff.tiff_file == NULL)
            return -1;
        if (!TIFFSetDirectory(s->tiff.tiff_file, (tdir_t) s->current_page + 1))
            return -1;
        return test_tiff_directory_info(s);
#endif
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_restart_page(t4_state_t *s)
{
    s->bit_pos = 7;
    s->bit_ptr = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_end_page(t4_state_t *s)
{
    s->current_page++;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_bit(t4_state_t *s)
{
    int bit;

    if (s->bit_ptr >= s->image_size)
        return SIG_STATUS_END_OF_DATA;
    bit = (s->image_buffer[s->bit_ptr] >> (7 - s->bit_pos)) & 1;
    if (--s->bit_pos < 0)
    {
        s->bit_pos = 7;
        s->bit_ptr++;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_byte(t4_state_t *s)
{
    if (s->bit_ptr >= s->image_size)
        return 0x100;
    return s->image_buffer[s->bit_ptr++];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_chunk(t4_state_t *s, uint8_t buf[], int max_len)
{
    if (s->bit_ptr >= s->image_size)
        return 0;
    if (s->bit_ptr + max_len > s->image_size)
        max_len = s->image_size - s->bit_ptr;
    memcpy(buf, &s->image_buffer[s->bit_ptr], max_len);
    s->bit_ptr += max_len;
    return max_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_check_bit(t4_state_t *s)
{
    int bit;

    if (s->bit_ptr >= s->image_size)
        return SIG_STATUS_END_OF_DATA;
    bit = (s->image_buffer[s->bit_ptr] >> s->bit_pos) & 1;
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_release(t4_state_t *s)
{
    if (s->rx)
        return -1;
    if (s->tiff.tiff_file)
        close_tiff_input_file(s);
    free_buffers(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_free(t4_state_t *s)
{
    int ret;

    ret = t4_tx_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_tx_encoding(t4_state_t *s, int encoding)
{
    s->line_encoding = encoding;
    s->rows_to_next_1d_row = s->max_rows_to_next_1d_row - 1;
    s->row_is_2d = FALSE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_min_row_bits(t4_state_t *s, int bits)
{
    s->min_bits_per_row = bits;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_local_ident(t4_state_t *s, const char *ident)
{
    s->tiff.local_ident = (ident  &&  ident[0])  ?  ident  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_header_info(t4_state_t *s, const char *info)
{
    s->header_info = (info  &&  info[0])  ?  info  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_y_resolution(t4_state_t *s)
{
    return s->y_resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_x_resolution(t4_state_t *s)
{
    return s->x_resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_image_width(t4_state_t *s)
{
    return s->image_width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_pages_in_file(t4_state_t *s)
{
    int max;

    max = 0;
    if (s->row_write_handler == NULL)
        max = get_tiff_total_pages(s);
    if (max >= 0)
        s->pages_in_file = max;
    return max;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_current_page_in_file(t4_state_t *s)
{
    return s->current_page;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_get_transfer_statistics(t4_state_t *s, t4_stats_t *t)
{
    t->pages_transferred = s->current_page - s->start_page;
    t->pages_in_file = s->pages_in_file;
    t->width = s->image_width;
    t->length = s->image_length;
    t->bad_rows = s->bad_rows;
    t->longest_bad_row_run = s->longest_bad_row_run;
    t->x_resolution = s->x_resolution;
    t->y_resolution = s->y_resolution;
    t->encoding = s->line_encoding;
    t->line_image_size = s->line_image_size/8;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t4_encoding_to_str(int encoding)
{
    switch (encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
        return "T.4 1-D";
    case T4_COMPRESSION_ITU_T4_2D:
        return "T.4 2-D";
    case T4_COMPRESSION_ITU_T6:
        return "T.6";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
