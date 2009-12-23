//#define T4_STATE_DEBUGGING
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_tx.c - ITU T.4 FAX transmit processing
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
 * $Id: t4_tx.c,v 1.13.2.9 2009/12/21 17:18:40 steveu Exp $
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

/*! The number of EOLs to be sent at the end of a T.4 page */
#define EOLS_TO_END_T4_TX_PAGE      6
/*! The number of EOLs to be sent at the end of a T.6 page */
#define EOLS_TO_END_T6_TX_PAGE      2

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

#include "faxfont.h"

/* Legitimate runs of zero bits which are the tail end of one code
   plus the start of the next code do not exceed 10 bits. */

/*
 * Note that these tables are ordered such that the index into the table
 * is known to be either the run length, or (run length / 64) + a fixed
 * offset.
 */
static const t4_run_table_entry_t t4_white_codes[] =
{
    { 8, 0x00AC,    0},         /* 0011 0101 */
    { 6, 0x0038,    1},         /* 0001 11 */
    { 4, 0x000E,    2},         /* 0111 */
    { 4, 0x0001,    3},         /* 1000 */
    { 4, 0x000D,    4},         /* 1011 */
    { 4, 0x0003,    5},         /* 1100 */
    { 4, 0x0007,    6},         /* 1110 */
    { 4, 0x000F,    7},         /* 1111 */
    { 5, 0x0019,    8},         /* 1001 1 */
    { 5, 0x0005,    9},         /* 1010 0 */
    { 5, 0x001C,   10},         /* 0011 1 */
    { 5, 0x0002,   11},         /* 0100 0 */
    { 6, 0x0004,   12},         /* 0010 00 */
    { 6, 0x0030,   13},         /* 0000 11 */
    { 6, 0x000B,   14},         /* 1101 00 */
    { 6, 0x002B,   15},         /* 1101 01 */
    { 6, 0x0015,   16},         /* 1010 10 */
    { 6, 0x0035,   17},         /* 1010 11 */
    { 7, 0x0072,   18},         /* 0100 111 */
    { 7, 0x0018,   19},         /* 0001 100 */
    { 7, 0x0008,   20},         /* 0001 000 */
    { 7, 0x0074,   21},         /* 0010 111 */
    { 7, 0x0060,   22},         /* 0000 011 */
    { 7, 0x0010,   23},         /* 0000 100 */
    { 7, 0x000A,   24},         /* 0101 000 */
    { 7, 0x006A,   25},         /* 0101 011 */
    { 7, 0x0064,   26},         /* 0010 011 */
    { 7, 0x0012,   27},         /* 0100 100 */
    { 7, 0x000C,   28},         /* 0011 000 */
    { 8, 0x0040,   29},         /* 0000 0010 */
    { 8, 0x00C0,   30},         /* 0000 0011 */
    { 8, 0x0058,   31},         /* 0001 1010 */
    { 8, 0x00D8,   32},         /* 0001 1011 */
    { 8, 0x0048,   33},         /* 0001 0010 */
    { 8, 0x00C8,   34},         /* 0001 0011 */
    { 8, 0x0028,   35},         /* 0001 0100 */
    { 8, 0x00A8,   36},         /* 0001 0101 */
    { 8, 0x0068,   37},         /* 0001 0110 */
    { 8, 0x00E8,   38},         /* 0001 0111 */
    { 8, 0x0014,   39},         /* 0010 1000 */
    { 8, 0x0094,   40},         /* 0010 1001 */
    { 8, 0x0054,   41},         /* 0010 1010 */
    { 8, 0x00D4,   42},         /* 0010 1011 */
    { 8, 0x0034,   43},         /* 0010 1100 */
    { 8, 0x00B4,   44},         /* 0010 1101 */
    { 8, 0x0020,   45},         /* 0000 0100 */
    { 8, 0x00A0,   46},         /* 0000 0101 */
    { 8, 0x0050,   47},         /* 0000 1010 */
    { 8, 0x00D0,   48},         /* 0000 1011 */
    { 8, 0x004A,   49},         /* 0101 0010 */
    { 8, 0x00CA,   50},         /* 0101 0011 */
    { 8, 0x002A,   51},         /* 0101 0100 */
    { 8, 0x00AA,   52},         /* 0101 0101 */
    { 8, 0x0024,   53},         /* 0010 0100 */
    { 8, 0x00A4,   54},         /* 0010 0101 */
    { 8, 0x001A,   55},         /* 0101 1000 */
    { 8, 0x009A,   56},         /* 0101 1001 */
    { 8, 0x005A,   57},         /* 0101 1010 */
    { 8, 0x00DA,   58},         /* 0101 1011 */
    { 8, 0x0052,   59},         /* 0100 1010 */
    { 8, 0x00D2,   60},         /* 0100 1011 */
    { 8, 0x004C,   61},         /* 0011 0010 */
    { 8, 0x00CC,   62},         /* 0011 0011 */
    { 8, 0x002C,   63},         /* 0011 0100 */
    { 5, 0x001B,   64},         /* 1101 1 */
    { 5, 0x0009,  128},         /* 1001 0 */
    { 6, 0x003A,  192},         /* 0101 11 */
    { 7, 0x0076,  256},         /* 0110 111 */
    { 8, 0x006C,  320},         /* 0011 0110 */
    { 8, 0x00EC,  384},         /* 0011 0111 */
    { 8, 0x0026,  448},         /* 0110 0100 */
    { 8, 0x00A6,  512},         /* 0110 0101 */
    { 8, 0x0016,  576},         /* 0110 1000 */
    { 8, 0x00E6,  640},         /* 0110 0111 */
    { 9, 0x0066,  704},         /* 0110 0110 0 */
    { 9, 0x0166,  768},         /* 0110 0110 1 */
    { 9, 0x0096,  832},         /* 0110 1001 0 */
    { 9, 0x0196,  896},         /* 0110 1001 1 */
    { 9, 0x0056,  960},         /* 0110 1010 0 */
    { 9, 0x0156, 1024},         /* 0110 1010 1 */
    { 9, 0x00D6, 1088},         /* 0110 1011 0 */
    { 9, 0x01D6, 1152},         /* 0110 1011 1 */
    { 9, 0x0036, 1216},         /* 0110 1100 0 */
    { 9, 0x0136, 1280},         /* 0110 1100 1 */
    { 9, 0x00B6, 1344},         /* 0110 1101 0 */
    { 9, 0x01B6, 1408},         /* 0110 1101 1 */
    { 9, 0x0032, 1472},         /* 0100 1100 0 */
    { 9, 0x0132, 1536},         /* 0100 1100 1 */
    { 9, 0x00B2, 1600},         /* 0100 1101 0 */
    { 6, 0x0006, 1664},         /* 0110 00 */
    { 9, 0x01B2, 1728},         /* 0100 1101 1 */
    {11, 0x0080, 1792},         /* 0000 0001 000 */
    {11, 0x0180, 1856},         /* 0000 0001 100 */
    {11, 0x0580, 1920},         /* 0000 0001 101 */
    {12, 0x0480, 1984},         /* 0000 0001 0010 */
    {12, 0x0C80, 2048},         /* 0000 0001 0011 */
    {12, 0x0280, 2112},         /* 0000 0001 0100 */
    {12, 0x0A80, 2176},         /* 0000 0001 0101 */
    {12, 0x0680, 2240},         /* 0000 0001 0110 */
    {12, 0x0E80, 2304},         /* 0000 0001 0111 */
    {12, 0x0380, 2368},         /* 0000 0001 1100 */
    {12, 0x0B80, 2432},         /* 0000 0001 1101 */
    {12, 0x0780, 2496},         /* 0000 0001 1110 */
    {12, 0x0F80, 2560},         /* 0000 0001 1111 */
};

static const t4_run_table_entry_t t4_black_codes[] =
{
    {10, 0x03B0,    0},         /* 0000 1101 11 */
    { 3, 0x0002,    1},         /* 010 */
    { 2, 0x0003,    2},         /* 11 */
    { 2, 0x0001,    3},         /* 10 */
    { 3, 0x0006,    4},         /* 011 */
    { 4, 0x000C,    5},         /* 0011 */
    { 4, 0x0004,    6},         /* 0010 */
    { 5, 0x0018,    7},         /* 0001 1 */
    { 6, 0x0028,    8},         /* 0001 01 */
    { 6, 0x0008,    9},         /* 0001 00 */
    { 7, 0x0010,   10},         /* 0000 100 */
    { 7, 0x0050,   11},         /* 0000 101 */
    { 7, 0x0070,   12},         /* 0000 111 */
    { 8, 0x0020,   13},         /* 0000 0100 */
    { 8, 0x00E0,   14},         /* 0000 0111 */
    { 9, 0x0030,   15},         /* 0000 1100 0 */
    {10, 0x03A0,   16},         /* 0000 0101 11 */
    {10, 0x0060,   17},         /* 0000 0110 00 */
    {10, 0x0040,   18},         /* 0000 0010 00 */
    {11, 0x0730,   19},         /* 0000 1100 111 */
    {11, 0x00B0,   20},         /* 0000 1101 000 */
    {11, 0x01B0,   21},         /* 0000 1101 100 */
    {11, 0x0760,   22},         /* 0000 0110 111 */
    {11, 0x00A0,   23},         /* 0000 0101 000 */
    {11, 0x0740,   24},         /* 0000 0010 111 */
    {11, 0x00C0,   25},         /* 0000 0011 000 */
    {12, 0x0530,   26},         /* 0000 1100 1010 */
    {12, 0x0D30,   27},         /* 0000 1100 1011 */
    {12, 0x0330,   28},         /* 0000 1100 1100 */
    {12, 0x0B30,   29},         /* 0000 1100 1101 */
    {12, 0x0160,   30},         /* 0000 0110 1000 */
    {12, 0x0960,   31},         /* 0000 0110 1001 */
    {12, 0x0560,   32},         /* 0000 0110 1010 */
    {12, 0x0D60,   33},         /* 0000 0110 1011 */
    {12, 0x04B0,   34},         /* 0000 1101 0010 */
    {12, 0x0CB0,   35},         /* 0000 1101 0011 */
    {12, 0x02B0,   36},         /* 0000 1101 0100 */
    {12, 0x0AB0,   37},         /* 0000 1101 0101 */
    {12, 0x06B0,   38},         /* 0000 1101 0110 */
    {12, 0x0EB0,   39},         /* 0000 1101 0111 */
    {12, 0x0360,   40},         /* 0000 0110 1100 */
    {12, 0x0B60,   41},         /* 0000 0110 1101 */
    {12, 0x05B0,   42},         /* 0000 1101 1010 */
    {12, 0x0DB0,   43},         /* 0000 1101 1011 */
    {12, 0x02A0,   44},         /* 0000 0101 0100 */
    {12, 0x0AA0,   45},         /* 0000 0101 0101 */
    {12, 0x06A0,   46},         /* 0000 0101 0110 */
    {12, 0x0EA0,   47},         /* 0000 0101 0111 */
    {12, 0x0260,   48},         /* 0000 0110 0100 */
    {12, 0x0A60,   49},         /* 0000 0110 0101 */
    {12, 0x04A0,   50},         /* 0000 0101 0010 */
    {12, 0x0CA0,   51},         /* 0000 0101 0011 */
    {12, 0x0240,   52},         /* 0000 0010 0100 */
    {12, 0x0EC0,   53},         /* 0000 0011 0111 */
    {12, 0x01C0,   54},         /* 0000 0011 1000 */
    {12, 0x0E40,   55},         /* 0000 0010 0111 */
    {12, 0x0140,   56},         /* 0000 0010 1000 */
    {12, 0x01A0,   57},         /* 0000 0101 1000 */
    {12, 0x09A0,   58},         /* 0000 0101 1001 */
    {12, 0x0D40,   59},         /* 0000 0010 1011 */
    {12, 0x0340,   60},         /* 0000 0010 1100 */
    {12, 0x05A0,   61},         /* 0000 0101 1010 */
    {12, 0x0660,   62},         /* 0000 0110 0110 */
    {12, 0x0E60,   63},         /* 0000 0110 0111 */
    {10, 0x03C0,   64},         /* 0000 0011 11 */
    {12, 0x0130,  128},         /* 0000 1100 1000 */
    {12, 0x0930,  192},         /* 0000 1100 1001 */
    {12, 0x0DA0,  256},         /* 0000 0101 1011 */
    {12, 0x0CC0,  320},         /* 0000 0011 0011 */
    {12, 0x02C0,  384},         /* 0000 0011 0100 */
    {12, 0x0AC0,  448},         /* 0000 0011 0101 */
    {13, 0x06C0,  512},         /* 0000 0011 0110 0 */
    {13, 0x16C0,  576},         /* 0000 0011 0110 1 */
    {13, 0x0A40,  640},         /* 0000 0010 0101 0 */
    {13, 0x1A40,  704},         /* 0000 0010 0101 1 */
    {13, 0x0640,  768},         /* 0000 0010 0110 0 */
    {13, 0x1640,  832},         /* 0000 0010 0110 1 */
    {13, 0x09C0,  896},         /* 0000 0011 1001 0 */
    {13, 0x19C0,  960},         /* 0000 0011 1001 1 */
    {13, 0x05C0, 1024},         /* 0000 0011 1010 0 */
    {13, 0x15C0, 1088},         /* 0000 0011 1010 1 */
    {13, 0x0DC0, 1152},         /* 0000 0011 1011 0 */
    {13, 0x1DC0, 1216},         /* 0000 0011 1011 1 */
    {13, 0x0940, 1280},         /* 0000 0010 1001 0 */
    {13, 0x1940, 1344},         /* 0000 0010 1001 1 */
    {13, 0x0540, 1408},         /* 0000 0010 1010 0 */
    {13, 0x1540, 1472},         /* 0000 0010 1010 1 */
    {13, 0x0B40, 1536},         /* 0000 0010 1101 0 */
    {13, 0x1B40, 1600},         /* 0000 0010 1101 1 */
    {13, 0x04C0, 1664},         /* 0000 0011 0010 0 */
    {13, 0x14C0, 1728},         /* 0000 0011 0010 1 */
    {11, 0x0080, 1792},         /* 0000 0001 000 */
    {11, 0x0180, 1856},         /* 0000 0001 100 */
    {11, 0x0580, 1920},         /* 0000 0001 101 */
    {12, 0x0480, 1984},         /* 0000 0001 0010 */
    {12, 0x0C80, 2048},         /* 0000 0001 0011 */
    {12, 0x0280, 2112},         /* 0000 0001 0100 */
    {12, 0x0A80, 2176},         /* 0000 0001 0101 */
    {12, 0x0680, 2240},         /* 0000 0001 0110 */
    {12, 0x0E80, 2304},         /* 0000 0001 0111 */
    {12, 0x0380, 2368},         /* 0000 0001 1100 */
    {12, 0x0B80, 2432},         /* 0000 0001 1101 */
    {12, 0x0780, 2496},         /* 0000 0001 1110 */
    {12, 0x0F80, 2560},         /* 0000 0001 1111 */
};

static int encode_row(t4_state_t *s);

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
             s->t4_t6_tx.header_info,
             s->tiff.local_ident,
             s->current_page + 1);
}
/*- End of function --------------------------------------------------------*/

static int t4_tx_put_fax_header(t4_state_t *s)
{
    int row;
    int i;
    int repeats;
    int pattern;
    int row_bufptr;
    char *t;
    char header[132 + 1];

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
    TIFFGetField(t->tiff_file, TIFFTAG_SAMPLESPERPIXEL, &parm16);
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
        span_log(&s->logging, SPAN_LOG_FLOW, "%s: Photometric needs swapping.\n", t->file);
    t->fill_order = FILLORDER_LSB2MSB;
#if 0
    TIFFGetField(t->tiff_file, TIFFTAG_FILLORDER, &t->fill_order);
    if (t->fill_order != FILLORDER_LSB2MSB)
        span_log(&s->logging, SPAN_LOG_FLOW, "%s: Fill order needs swapping.\n", t->file);
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
    s->t4_t6_tx.max_rows_to_next_1d_row = 2;
    for (i = 0;  y_res_table[i].code > 0;  i++)
    {
        if (test_resolution(res_unit, y_resolution, y_res_table[i].resolution))
        {
            s->y_resolution = y_res_table[i].code;
            s->t4_t6_tx.max_rows_to_next_1d_row = y_res_table[i].max_rows_to_next_1d_row;
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
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: Read error at row %d.\n", s->tiff.file, row);
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
    if (s->tiff.file)
        free((char *) s->tiff.file);
    s->tiff.file = NULL;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void update_row_bit_info(t4_state_t *s)
{
    if (s->row_bits > s->max_row_bits)
        s->max_row_bits = s->row_bits;
    if (s->row_bits < s->min_row_bits)
        s->min_row_bits = s->row_bits;
    s->row_bits = 0;
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
            if (s->row_bits + length < s->t4_t6_tx.min_bits_per_row)
                put_encoded_bits(s, 0, s->t4_t6_tx.min_bits_per_row - (s->row_bits + length));
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
            for (  ;  b_cursor < s->t4_t6_tx.ref_steps;  b_cursor += 2)
            {
                if (a0 < (int) s->ref_runs[b_cursor])
                    break;
            }
            if (b_cursor >= s->t4_t6_tx.ref_steps)
                b_cursor = s->t4_t6_tx.ref_steps - 1;
        }
        b1 = s->ref_runs[b_cursor];
    }
    /* Swap the buffers */
    s->t4_t6_tx.ref_steps = cur_steps;
    p = s->cur_runs;
    s->cur_runs = s->ref_runs;
    s->ref_runs = p;
}
/*- End of function --------------------------------------------------------*/

/*
 * 1D-encode a row of pixels. The encoding is a sequence of all-white or
 * all-black spans of pixels encoded with Huffman codes.
 */
static void encode_1d_row(t4_state_t *s)
{
    int i;

    /* Do our work in the reference row buffer, and it is already in place if
       we need a reference row for a following 2D encoded row. */
    s->t4_t6_tx.ref_steps = row_to_run_lengths(s->ref_runs, s->row_buf, s->image_width);
    put_1d_span(s, s->ref_runs[0], t4_white_codes);
    for (i = 1;  i < s->t4_t6_tx.ref_steps;  i++)
        put_1d_span(s, s->ref_runs[i] - s->ref_runs[i - 1], (i & 1)  ?  t4_black_codes  :  t4_white_codes);
    /* Stretch the row a little, so when we step by 2 we are guaranteed to
       hit an entry showing the row length */
    s->ref_runs[s->t4_t6_tx.ref_steps] =
    s->ref_runs[s->t4_t6_tx.ref_steps + 1] =
    s->ref_runs[s->t4_t6_tx.ref_steps + 2] = s->ref_runs[s->t4_t6_tx.ref_steps - 1];
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
            s->t4_t6_tx.rows_to_next_1d_row--;
        }
        else
        {
            encode_1d_row(s);
            s->row_is_2d = TRUE;
        }
        if (s->t4_t6_tx.rows_to_next_1d_row <= 0)
        {
            /* Insert a row of 1D encoding */
            s->row_is_2d = FALSE;
            s->t4_t6_tx.rows_to_next_1d_row = s->t4_t6_tx.max_rows_to_next_1d_row - 1;
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
    s->t4_t6_tx.row_read_handler = handler;
    s->t4_t6_tx.row_read_user_data = user_data;
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
    s->tiff.file = strdup(file);
    s->current_page =
    s->tiff.start_page = (start_page >= 0)  ?  start_page  :  0;
    s->tiff.stop_page = (stop_page >= 0)  ?  stop_page : INT_MAX;

    if (!TIFFSetDirectory(s->tiff.tiff_file, (tdir_t) s->current_page))
        return NULL;
    if (get_tiff_directory_info(s))
    {
        close_tiff_input_file(s);
        return NULL;
    }

    s->t4_t6_tx.rows_to_next_1d_row = s->t4_t6_tx.max_rows_to_next_1d_row - 1;

    s->tiff.pages_in_file = -1;

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
    s->t4_t6_tx.ref_steps = 1;
    s->image_buffer_size = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_start_page(t4_state_t *s)
{
    int row;
    int i;
    int run_space;
    int len;
    int old_image_width;
    uint8_t *bufptr8;
    uint32_t *bufptr;

    span_log(&s->logging, SPAN_LOG_FLOW, "Start tx page %d\n", s->current_page);
    if (s->current_page > s->tiff.stop_page)
        return -1;
    if (s->tiff.tiff_file == NULL)
        return -1;
    old_image_width = s->image_width;
    if (s->t4_t6_tx.row_read_handler == NULL)
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
    s->t4_t6_tx.rows_to_next_1d_row = s->t4_t6_tx.max_rows_to_next_1d_row - 1;

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
    s->t4_t6_tx.ref_steps = 1;

    s->row_bits = 0;
    s->min_row_bits = INT_MAX;
    s->max_row_bits = 0;

    if (s->t4_t6_tx.header_info  &&  s->t4_t6_tx.header_info[0])
    {
        if (t4_tx_put_fax_header(s))
            return -1;
    }
    if (s->t4_t6_tx.row_read_handler)
    {
        for (row = 0;  ;  row++)
        {
            if ((len = s->t4_t6_tx.row_read_handler(s->t4_t6_tx.row_read_user_data, s->row_buf, s->bytes_per_row)) < 0)
            {
                span_log(&s->logging, SPAN_LOG_WARNING, "%s: Read error at row %d.\n", s->tiff.file, row);
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
    s->t4_t6_tx.bit_pos = 7;
    s->t4_t6_tx.bit_ptr = 0;
    s->line_image_size = s->image_size*8;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_next_page_has_different_format(t4_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Checking for the existance of page %d\n", s->current_page + 1);
    if (s->current_page >= s->tiff.stop_page)
        return -1;
    if (s->t4_t6_tx.row_read_handler == NULL)
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
    s->t4_t6_tx.bit_pos = 7;
    s->t4_t6_tx.bit_ptr = 0;
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

    if (s->t4_t6_tx.bit_ptr >= s->image_size)
        return SIG_STATUS_END_OF_DATA;
    bit = (s->image_buffer[s->t4_t6_tx.bit_ptr] >> (7 - s->t4_t6_tx.bit_pos)) & 1;
    if (--s->t4_t6_tx.bit_pos < 0)
    {
        s->t4_t6_tx.bit_pos = 7;
        s->t4_t6_tx.bit_ptr++;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_byte(t4_state_t *s)
{
    if (s->t4_t6_tx.bit_ptr >= s->image_size)
        return 0x100;
    return s->image_buffer[s->t4_t6_tx.bit_ptr++];
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_chunk(t4_state_t *s, uint8_t buf[], int max_len)
{
    if (s->t4_t6_tx.bit_ptr >= s->image_size)
        return 0;
    if (s->t4_t6_tx.bit_ptr + max_len > s->image_size)
        max_len = s->image_size - s->t4_t6_tx.bit_ptr;
    memcpy(buf, &s->image_buffer[s->t4_t6_tx.bit_ptr], max_len);
    s->t4_t6_tx.bit_ptr += max_len;
    return max_len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_check_bit(t4_state_t *s)
{
    int bit;

    if (s->t4_t6_tx.bit_ptr >= s->image_size)
        return SIG_STATUS_END_OF_DATA;
    bit = (s->image_buffer[s->t4_t6_tx.bit_ptr] >> s->t4_t6_tx.bit_pos) & 1;
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
    s->t4_t6_tx.rows_to_next_1d_row = s->t4_t6_tx.max_rows_to_next_1d_row - 1;
    s->row_is_2d = FALSE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_min_row_bits(t4_state_t *s, int bits)
{
    s->t4_t6_tx.min_bits_per_row = bits;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_local_ident(t4_state_t *s, const char *ident)
{
    s->tiff.local_ident = (ident  &&  ident[0])  ?  ident  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_tx_set_header_info(t4_state_t *s, const char *info)
{
    s->t4_t6_tx.header_info = (info  &&  info[0])  ?  info  :  NULL;
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
    if (s->t4_t6_tx.row_read_handler == NULL)
        max = get_tiff_total_pages(s);
    if (max >= 0)
        s->tiff.pages_in_file = max;
    return max;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_tx_get_current_page_in_file(t4_state_t *s)
{
    return s->current_page;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
