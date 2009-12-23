//#define T4_STATE_DEBUGGING
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_rx.c - ITU T.4 FAX receive processing
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
 * $Id: t4_rx.c,v 1.12.2.8 2009/12/21 17:18:39 steveu Exp $
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
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/version.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"

/*! The number of centimetres in one inch */
#define CM_PER_INCH                 2.54f

/*! The number of EOLs to expect at the end of a T.4 page */
#define EOLS_TO_END_ANY_RX_PAGE     6
/*! The number of EOLs to check at the end of a T.4 page */
#define EOLS_TO_END_T4_RX_PAGE      5
/*! The number of EOLs to check at the end of a T.6 page */
#define EOLS_TO_END_T6_RX_PAGE      2

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

#include "t4_t6_decode_states.h"

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
    s->tiff.pages_in_file = s->current_page;
    if (t->output_compression == COMPRESSION_CCITT_T4)
    {
        if (s->t4_t6_rx.bad_rows)
        {
            TIFFSetField(t->tiff_file, TIFFTAG_BADFAXLINES, s->t4_t6_rx.bad_rows);
            TIFFSetField(t->tiff_file, TIFFTAG_CLEANFAXDATA, CLEANFAXDATA_REGENERATED);
            TIFFSetField(t->tiff_file, TIFFTAG_CONSECUTIVEBADFAXLINES, s->t4_t6_rx.longest_bad_row_run);
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
        span_log(&s->logging, SPAN_LOG_WARNING, "%s: Error writing TIFF strip.\n", s->tiff.file);
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
    if (t->file)
    {
        /* Try not to leave a file behind, if we didn't receive any pages to
           put in it. */
        if (s->current_page == 0)
            remove(t->file);
        free((char *) t->file);
        t->file = NULL;
    }
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
    if (s->t4_t6_rx.run_length >= 0)
    {
        s->row_len += s->t4_t6_rx.run_length;
        /* Don't allow rows to grow too long, and overflow the buffers */
        if (s->row_len <= s->image_width)
            s->cur_runs[s->t4_t6_rx.a_cursor++] = s->t4_t6_rx.run_length;
    }
    s->t4_t6_rx.run_length = 0;
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

    if (s->t4_t6_rx.run_length)
        add_run_to_row(s);
#if defined(T4_STATE_DEBUGGING)
    /* Dump the runs of black and white for analysis */
    {
        int total;

        total = 0;
        for (x = 0;  x < s->t4_t6_rx.b_cursor;  x++)
            total += s->ref_runs[x];
        printf("Ref (%d)", total);
        for (x = 0;  x < s->t4_t6_rx.b_cursor;  x++)
            printf(" %" PRIu32, s->ref_runs[x]);
        printf("\n");
        total = 0;
        for (x = 0;  x < s->t4_t6_rx.a_cursor;  x++)
            total += s->cur_runs[x];
        printf("Cur (%d)", total);
        for (x = 0;  x < s->t4_t6_rx.a_cursor;  x++)
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
        if (s->t4_t6_rx.curr_bad_row_run)
        {
            if (s->t4_t6_rx.curr_bad_row_run > s->t4_t6_rx.longest_bad_row_run)
                s->t4_t6_rx.longest_bad_row_run = s->t4_t6_rx.curr_bad_row_run;
            s->t4_t6_rx.curr_bad_row_run = 0;
        }
        /* Convert the runs to a bit image of the row */
        /* White/black/white... runs, always starting with white. That means the first run could be
           zero length. */
        for (x = 0, fudge = 0;  x < s->t4_t6_rx.a_cursor;  x++, fudge ^= 0xFF)
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
        for (j = 0, fudge = 0;  j < s->t4_t6_rx.a_cursor  &&  fudge < s->image_width;  j++)
            fudge += s->cur_runs[j];
        if (fudge < s->image_width)
        {
            /* Try to pad with white, and avoid black, to minimise mess on the image. */
            if ((s->t4_t6_rx.a_cursor & 1))
            {
                /* We currently finish in white. We could extend that, but it is probably of
                   the right length. Changing it would only further mess up what happens in the
                   next row. It seems better to add a black spot, and an extra white run. */
                s->cur_runs[s->t4_t6_rx.a_cursor++] = 1;
                fudge++;
                if (fudge < s->image_width)
                    s->cur_runs[s->t4_t6_rx.a_cursor++] = s->image_width - fudge;
            }
            else
            {
                /* We currently finish on black, so we add an extra white run to fill out the line. */
                s->cur_runs[s->t4_t6_rx.a_cursor++] = s->image_width - fudge;
            }
        }
        else
        {
            /* Trim the last element to align with the proper image width */
            s->cur_runs[s->t4_t6_rx.a_cursor] += (s->image_width - fudge);
        }
        /* Ensure there is a previous line to copy from. */
        if (s->image_size != s->t4_t6_rx.last_row_starts_at)
        {
            /* Copy the previous row over this one */
            memcpy(s->image_buffer + s->image_size, s->image_buffer + s->t4_t6_rx.last_row_starts_at, s->bytes_per_row);
            s->image_size += s->bytes_per_row;
            s->image_length++;
        }
        s->t4_t6_rx.bad_rows++;
        s->t4_t6_rx.curr_bad_row_run++;
    }

    /* Pad the row as it becomes the reference row, so there are no odd runs to pick up if we
       step off the end of the list. */
    s->cur_runs[s->t4_t6_rx.a_cursor] = 0;
    s->cur_runs[s->t4_t6_rx.a_cursor + 1] = 0;

    /* Prepare the buffers for the next row. */
    s->t4_t6_rx.last_row_starts_at = row_starts_at;
    /* Swap the buffers */
    p = s->cur_runs;
    s->cur_runs = s->ref_runs;
    s->ref_runs = p;

    s->t4_t6_rx.b_cursor = 1;
    s->t4_t6_rx.a_cursor = 0;
    s->t4_t6_rx.b1 = s->ref_runs[0];
    s->t4_t6_rx.a0 = 0;
    
    s->t4_t6_rx.run_length = 0;

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
    if (s->t4_t6_rx.curr_bad_row_run)
    {
        if (s->t4_t6_rx.curr_bad_row_run > s->t4_t6_rx.longest_bad_row_run)
            s->t4_t6_rx.longest_bad_row_run = s->t4_t6_rx.curr_bad_row_run;
        s->t4_t6_rx.curr_bad_row_run = 0;
    }

    if (s->image_size == 0)
        return -1;

    if (s->t4_t6_rx.row_write_handler)
    {
        for (row = 0;  row < s->image_length;  row++)
        {
            if (s->t4_t6_rx.row_write_handler(s->t4_t6_rx.row_write_user_data, s->image_buffer + row*s->bytes_per_row, s->bytes_per_row) < 0)
            {
                span_log(&s->logging, SPAN_LOG_WARNING, "Write error at row %d.\n", row);
                break;
            }
        }
        /* Write a blank row to indicate the end of the image. */
        if (s->t4_t6_rx.row_write_handler(s->t4_t6_rx.row_write_user_data, NULL, 0) < 0)
            span_log(&s->logging, SPAN_LOG_WARNING, "Write error at row %d.\n", row);
    }
    else
    {
        write_tiff_image(s);
    }
    s->t4_t6_rx.rx_bits = 0;
    s->t4_t6_rx.rx_skip_bits = 0;
    s->t4_t6_rx.rx_bitstream = 0;
    s->t4_t6_rx.consecutive_eols = EOLS_TO_END_ANY_RX_PAGE;

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
    s->t4_t6_rx.rx_skip_bits += (bits - 1);
    s->t4_t6_rx.rx_bits--;
    s->t4_t6_rx.rx_bitstream >>= 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void force_drop_rx_bits(t4_state_t *s, int bits)
{
    /* This should only be called to drop the bits of an EOL, as that is the
       only place where it is safe to drop them all at once. */
    s->row_bits += bits;
    s->t4_t6_rx.rx_skip_bits = 0;
    s->t4_t6_rx.rx_bits -= bits;
    s->t4_t6_rx.rx_bitstream >>= bits;
}
/*- End of function --------------------------------------------------------*/

static int rx_put_bits(t4_state_t *s, uint32_t bit_string, int quantity)
{
    int bits;

    /* We decompress bit by bit, as the data stream is received. We need to
       scan continuously for EOLs, so we might as well work this way. */
    s->line_image_size += quantity;
    s->t4_t6_rx.rx_bitstream |= (bit_string << s->t4_t6_rx.rx_bits);
    /* The longest item we need to scan for is 13 bits long (a 2D EOL), so we
       need a minimum of 13 bits in the buffer to proceed with any bit stream
       analysis. */
    if ((s->t4_t6_rx.rx_bits += quantity) < 13)
        return FALSE;
    if (s->t4_t6_rx.consecutive_eols)
    {
        /* Check if the image has already terminated. */
        if (s->t4_t6_rx.consecutive_eols >= EOLS_TO_END_ANY_RX_PAGE)
            return TRUE;
        /* Check if the image hasn't even started. */
        if (s->t4_t6_rx.consecutive_eols < 0)
        {
            /* We are waiting for the very first EOL (1D or 2D only). */
            /* We need to take this bit by bit, as the EOL could be anywhere,
               and any junk could preceed it. */
            while ((s->t4_t6_rx.rx_bitstream & 0xFFF) != 0x800)
            {
                s->t4_t6_rx.rx_bitstream >>= 1;
                if (--s->t4_t6_rx.rx_bits < 13)
                    return FALSE;
            }
            /* We have an EOL, so now the page begins and we can proceed to
               process the bit stream as image data. */
            s->t4_t6_rx.consecutive_eols = 0;
            if (s->line_encoding == T4_COMPRESSION_ITU_T4_1D)
            {
                s->row_is_2d = FALSE;
                force_drop_rx_bits(s, 12);
            }
            else
            {
                s->row_is_2d = !(s->t4_t6_rx.rx_bitstream & 0x1000);
                force_drop_rx_bits(s, 13);
            }
        }
    }

    while (s->t4_t6_rx.rx_bits >= 13)
    {
        /* We need to check for EOLs bit by bit through the whole stream. If
           we just try looking between code words, we will miss an EOL when a bit
           error has throw the code words completely out of step. The can mean
           recovery takes many lines, and the image gets really messed up. */
        /* Although EOLs are not inserted at the end of each row of a T.6 image,
           they are still perfectly valid, and can terminate an image. */
        if ((s->t4_t6_rx.rx_bitstream & 0x0FFF) == 0x0800)
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
                s->t4_t6_rx.consecutive_eols++;
                if (s->line_encoding == T4_COMPRESSION_ITU_T6)
                {
                    if (s->t4_t6_rx.consecutive_eols >= EOLS_TO_END_T6_RX_PAGE)
                    {
                        s->t4_t6_rx.consecutive_eols = EOLS_TO_END_ANY_RX_PAGE;
                        return TRUE;
                    }
                }
                else
                {
                    if (s->t4_t6_rx.consecutive_eols >= EOLS_TO_END_T4_RX_PAGE)
                    {
                        s->t4_t6_rx.consecutive_eols = EOLS_TO_END_ANY_RX_PAGE;
                        return TRUE;
                    }
                }
            }
            else
            {
                /* The EOLs are not back-to-back, so they are not part of the
                   end of page condition. */
                if (s->t4_t6_rx.run_length > 0)
                    add_run_to_row(s);
                s->t4_t6_rx.consecutive_eols = 0;
                if (put_decoded_row(s))
                    return TRUE;
                update_row_bit_info(s);
            }
            if (s->line_encoding == T4_COMPRESSION_ITU_T4_2D)
            {
                s->row_is_2d = !(s->t4_t6_rx.rx_bitstream & 0x1000);
                force_drop_rx_bits(s, 13);
            }
            else
            {
                force_drop_rx_bits(s, 12);
            }
            s->t4_t6_rx.its_black = FALSE;
            s->t4_t6_rx.black_white = 0;
            s->t4_t6_rx.run_length = 0;
            s->row_len = 0;
            continue;
        }
        if (s->t4_t6_rx.rx_skip_bits)
        {
            /* We are clearing out the remaining bits of the last code word we
               absorbed. */
            s->t4_t6_rx.rx_skip_bits--;
            s->t4_t6_rx.rx_bits--;
            s->t4_t6_rx.rx_bitstream >>= 1;
            continue;
        }
        if (s->row_is_2d  &&  s->t4_t6_rx.black_white == 0)
        {
            bits = s->t4_t6_rx.rx_bitstream & 0x7F;
            STATE_TRACE("State %d, %d - ",
                        t4_2d_table[bits].state,
                        t4_2d_table[bits].width);
            if (s->row_len >= s->image_width)
            {
                drop_rx_bits(s, t4_2d_table[bits].width);
                continue;
            }
            if (s->t4_t6_rx.a_cursor)
            {
                /* Move past a0, always staying on the current colour */
                for (  ;  s->t4_t6_rx.b1 <= s->t4_t6_rx.a0;  s->t4_t6_rx.b_cursor += 2)
                    s->t4_t6_rx.b1 += (s->ref_runs[s->t4_t6_rx.b_cursor] + s->ref_runs[s->t4_t6_rx.b_cursor + 1]);
            }
            switch (t4_2d_table[bits].state)
            {
            case S_Horiz:
                STATE_TRACE("Horiz %d %d %d\n",
                            s->image_width,
                            s->t4_t6_rx.a0,
                            s->t4_t6_rx.a_cursor);
                /* We now need to extract a white/black or black/white pair of runs, using the 1D
                   method. If the first of the pair takes us exactly to the end of the row, there
                   should still be a zero length element for the second of the pair. */
                s->t4_t6_rx.its_black = s->t4_t6_rx.a_cursor & 1;
                s->t4_t6_rx.black_white = 2;
                break;
            case S_Vert:
                STATE_TRACE("Vert[%d] %d %d %d %d\n",
                            t4_2d_table[bits].param,
                            s->image_width,
                            s->t4_t6_rx.a0,
                            s->t4_t6_rx.b1,
                            s->t4_t6_rx.run_length);
                s->t4_t6_rx.run_length += (s->t4_t6_rx.b1 - s->t4_t6_rx.a0 + t4_2d_table[bits].param);
                s->t4_t6_rx.a0 = s->t4_t6_rx.b1 + t4_2d_table[bits].param;
                add_run_to_row(s);
                /* We need to move one step in one direction or the other, to change to the
                   opposite colour */
                if (t4_2d_table[bits].param >= 0)
                {
                    s->t4_t6_rx.b1 += s->ref_runs[s->t4_t6_rx.b_cursor++];
                }
                else
                {
                    if (s->t4_t6_rx.b_cursor)
                        s->t4_t6_rx.b1 -= s->ref_runs[--s->t4_t6_rx.b_cursor];
                }
                break;
            case S_Pass:
                STATE_TRACE("Pass %d %d %d %d %d\n",
                            s->image_width,
                            s->t4_t6_rx.a0,
                            s->t4_t6_rx.b1,
                            s->ref_runs[s->t4_t6_rx.b_cursor],
                            s->ref_runs[s->t4_t6_rx.b_cursor + 1]);
                s->t4_t6_rx.b1 += s->ref_runs[s->t4_t6_rx.b_cursor++];
                s->t4_t6_rx.run_length += (s->t4_t6_rx.b1 - s->t4_t6_rx.a0);
                s->t4_t6_rx.a0 = s->t4_t6_rx.b1;
                s->t4_t6_rx.b1 += s->ref_runs[s->t4_t6_rx.b_cursor++];
                break;
            case S_Ext:
                /* We do not currently handle any kind of extension */
                STATE_TRACE("Ext %d %d %d 0x%x\n",
                            s->image_width,
                            s->t4_t6_rx.a0,
                            ((s->t4_t6_rx.rx_bitstream >> t4_2d_table[bits].width) & 0x7),
                            s->t4_t6_rx.rx_bitstream);
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
            if (s->t4_t6_rx.its_black)
            {
                bits = s->t4_t6_rx.rx_bitstream & 0x1FFF;
                STATE_TRACE("State %d, %d - Black %d %d %d\n",
                            t4_1d_black_table[bits].state,
                            t4_1d_black_table[bits].width,
                            s->image_width,
                            s->t4_t6_rx.a0,
                            t4_1d_black_table[bits].param);
                switch (t4_1d_black_table[bits].state)
                {
                case S_MakeUpB:
                case S_MakeUp:
                    s->t4_t6_rx.run_length += t4_1d_black_table[bits].param;
                    s->t4_t6_rx.a0 += t4_1d_black_table[bits].param;
                    break;
                case S_TermB:
                    s->t4_t6_rx.its_black = FALSE;
                    if (s->row_len < s->image_width)
                    {
                        s->t4_t6_rx.run_length += t4_1d_black_table[bits].param;
                        s->t4_t6_rx.a0 += t4_1d_black_table[bits].param;
                        add_run_to_row(s);
                    }
                    if (s->t4_t6_rx.black_white)
                        s->t4_t6_rx.black_white--;
                    break;
                default:
                    /* Bad black */
                    s->t4_t6_rx.black_white = 0;
                    break;
                }
                drop_rx_bits(s, t4_1d_black_table[bits].width);
            }
            else
            {
                bits = s->t4_t6_rx.rx_bitstream & 0xFFF;
                STATE_TRACE("State %d, %d - White %d %d %d\n",
                            t4_1d_white_table[bits].state,
                            t4_1d_white_table[bits].width,
                            s->image_width,
                            s->t4_t6_rx.a0,
                            t4_1d_white_table[bits].param);
                switch (t4_1d_white_table[bits].state)
                {
                case S_MakeUpW:
                case S_MakeUp:
                    s->t4_t6_rx.run_length += t4_1d_white_table[bits].param;
                    s->t4_t6_rx.a0 += t4_1d_white_table[bits].param;
                    break;
                case S_TermW:
                    s->t4_t6_rx.its_black = TRUE;
                    if (s->row_len < s->image_width)
                    {
                        s->t4_t6_rx.run_length += t4_1d_white_table[bits].param;
                        s->t4_t6_rx.a0 += t4_1d_white_table[bits].param;
                        add_run_to_row(s);
                    }
                    if (s->t4_t6_rx.black_white)
                        s->t4_t6_rx.black_white--;
                    break;
                default:
                    /* Bad white */
                    s->t4_t6_rx.black_white = 0;
                    break;
                }
                drop_rx_bits(s, t4_1d_white_table[bits].width);
            }
        }
        if (s->t4_t6_rx.a0 >= s->image_width)
            s->t4_t6_rx.a0 = s->image_width - 1;

        if (s->line_encoding == T4_COMPRESSION_ITU_T6)
        {
            /* T.6 has no EOL markers. We sense the end of a line by its length alone. */
            /* The last test here is a backstop protection, so a corrupt image cannot
               cause us to do bad things. Bad encoders have actually been seen, which
               demand such protection. */
            if (s->t4_t6_rx.black_white == 0  &&  s->row_len >= s->image_width)
            {
                STATE_TRACE("EOL T.6\n");
                if (s->t4_t6_rx.run_length > 0)
                    add_run_to_row(s);
                update_row_bit_info(s);
                if (put_decoded_row(s))
                    return TRUE;
                s->t4_t6_rx.its_black = FALSE;
                s->t4_t6_rx.black_white = 0;
                s->t4_t6_rx.run_length = 0;
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
    s->t4_t6_rx.row_write_handler = handler;
    s->t4_t6_rx.row_write_user_data = user_data;
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
    s->tiff.file = strdup(file);
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
    s->tiff.pages_in_file = 0;
    s->tiff.start_page = 0;
    s->tiff.stop_page = INT_MAX;

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

    s->t4_t6_rx.rx_bits = 0;
    s->t4_t6_rx.rx_skip_bits = 0;
    s->t4_t6_rx.rx_bitstream = 0;
    s->row_bits = 0;
    s->min_row_bits = INT_MAX;
    s->max_row_bits = 0;

    s->row_is_2d = (s->line_encoding == T4_COMPRESSION_ITU_T6);
    /* We start at -1 EOLs for 1D and 2D decoding, as an indication we are waiting for the
       first EOL. T.6 coding starts without any preamble. */
    s->t4_t6_rx.consecutive_eols = (s->line_encoding == T4_COMPRESSION_ITU_T6)  ?  0  :  -1;

    s->t4_t6_rx.bad_rows = 0;
    s->t4_t6_rx.longest_bad_row_run = 0;
    s->t4_t6_rx.curr_bad_row_run = 0;
    s->image_length = 0;
    s->tx_bitstream = 0;
    s->tx_bits = 8;
    s->image_size = 0;
    s->line_image_size = 0;
    s->t4_t6_rx.last_row_starts_at = 0;

    s->row_len = 0;
    s->t4_t6_rx.its_black = FALSE;
    s->t4_t6_rx.black_white = 0;

    /* Initialise the reference line to all white */
    s->ref_runs[0] =
    s->ref_runs[1] =
    s->ref_runs[2] =
    s->ref_runs[3] = s->image_width;

    s->t4_t6_rx.b_cursor = 1;
    s->t4_t6_rx.a_cursor = 0;
    s->t4_t6_rx.b1 = s->ref_runs[0];
    s->t4_t6_rx.a0 = 0;

    s->t4_t6_rx.run_length = 0;

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

SPAN_DECLARE(void) t4_get_transfer_statistics(t4_state_t *s, t4_stats_t *t)
{
    t->pages_transferred = s->current_page - s->tiff.start_page;
    t->pages_in_file = s->tiff.pages_in_file;
    t->width = s->image_width;
    t->length = s->image_length;
    t->bad_rows = s->t4_t6_rx.bad_rows;
    t->longest_bad_row_run = s->t4_t6_rx.longest_bad_row_run;
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
