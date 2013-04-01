//#define T4_STATE_DEBUGGING
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_t6_encode.c - ITU T.4 and T.6 FAX image decompression
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
 */

/*
 * This file has origins in the T.4 and T.6 support in libtiff, which requires
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

/*! The number of centimetres in one inch */
#define CM_PER_INCH                 2.54f

/*! The number of EOLs to expect at the end of a T.4 page */
#define EOLS_TO_END_ANY_RX_PAGE     6
/*! The number of EOLs to check at the end of a T.4 page */
#define EOLS_TO_END_T4_RX_PAGE      5
/*! The number of EOLs to check at the end of a T.6 page */
#define EOLS_TO_END_T6_RX_PAGE      2

#include "t4_t6_decode_states.h"

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

static int free_buffers(t4_t6_decode_state_t *s)
{
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
    s->bytes_per_row = 0;
    return 0;
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

static __inline__ void add_run_to_row(t4_t6_decode_state_t *s)
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

static __inline__ void update_row_bit_info(t4_t6_decode_state_t *s)
{
    if (s->row_bits > s->max_row_bits)
        s->max_row_bits = s->row_bits;
    if (s->row_bits < s->min_row_bits)
        s->min_row_bits = s->row_bits;
    s->row_bits = 0;
}
/*- End of function --------------------------------------------------------*/

static int put_decoded_row(t4_t6_decode_state_t *s)
{
    static const int msbmask[9] =
    {
        0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
    };
    uint32_t i;
    uint32_t *p;
    int fudge;
    int x;
    int j;
    int row_pos;

    if (s->run_length)
        add_run_to_row(s);
    update_row_bit_info(s);
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
    row_pos = 0;
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
            if ((int) i >= s->pixels)
            {
                s->pixel_stream = (s->pixel_stream << s->pixels) | (msbmask[s->pixels] & fudge);
                for (i += (8 - s->pixels);  i >= 8;  i -= 8)
                {
                    s->pixels = 8;
                    s->row_buf[row_pos++] = (uint8_t) s->pixel_stream;
                    s->pixel_stream = fudge;
                }
            }
            s->pixel_stream = (s->pixel_stream << i) | (msbmask[i] & fudge);
            s->pixels -= i;
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
        /* Reuse the previous row as the current one. If this is the first row
           the row buffer will already contain a suitable white row */
        s->image_length++;
        s->bad_rows++;
        s->curr_bad_row_run++;
    }

    /* Pad the row as it becomes the reference row, so there are no odd runs to pick up if we
       step off the end of the list. */
    s->cur_runs[s->a_cursor] = 0;
    s->cur_runs[s->a_cursor + 1] = 0;

    /* Swap the buffers */
    p = s->cur_runs;
    s->cur_runs = s->ref_runs;
    s->ref_runs = p;

    s->b_cursor = 1;
    s->a_cursor = 0;
    s->b1 = s->ref_runs[0];
    s->a0 = 0;

    s->run_length = 0;
    if (s->row_write_handler)
        return s->row_write_handler(s->row_write_user_data, s->row_buf, s->bytes_per_row);

    return 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void drop_rx_bits(t4_t6_decode_state_t *s, int bits)
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

static __inline__ void force_drop_rx_bits(t4_t6_decode_state_t *s, int bits)
{
    /* This should only be called to drop the bits of an EOL, as that is the
       only place where it is safe to drop them all at once. */
    s->row_bits += bits;
    s->rx_skip_bits = 0;
    s->rx_bits -= bits;
    s->rx_bitstream >>= bits;
}
/*- End of function --------------------------------------------------------*/

static int put_bits(t4_t6_decode_state_t *s, uint32_t bit_string, int quantity)
{
    int bits;
    int old_a0;

    /* We decompress bit by bit, as the data stream is received. We need to
       scan continuously for EOLs, so we might as well work this way. */
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
            if (s->encoding == T4_COMPRESSION_T4_1D)
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
                if (s->encoding == T4_COMPRESSION_T6)
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
            }
            if (s->encoding == T4_COMPRESSION_T4_2D)
            {
                s->row_is_2d = !(s->rx_bitstream & 0x1000);
                force_drop_rx_bits(s, 13);
            }
            else
            {
                force_drop_rx_bits(s, 12);
            }
            s->in_black = FALSE;
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
                s->in_black = s->a_cursor & 1;
                s->black_white = 2;
                break;
            case S_Vert:
                STATE_TRACE("Vert[%d] %d %d %d %d\n",
                            t4_2d_table[bits].param,
                            s->image_width,
                            s->a0,
                            s->b1,
                            s->run_length);
                old_a0 = s->a0;
                s->a0 = s->b1 + t4_2d_table[bits].param;
                /* We need to check if a bad or malicious image is failing to move forward along the row.
                   Going back is obviously bad. We also need to avoid a stall on the spot, except for the
                   special case of the start of the row. Zero movement as the very first element in the
                   row is perfectly normal. */
                if (s->a0 <= old_a0)
                {
                    if (s->a0 < old_a0  ||  s->b_cursor > 1)
                    {
                        /* Undo the update we just started, and carry on as if this code does not exist */
                        /* TODO: we really should record that something wasn't right at this point. */
                        s->a0 = old_a0;
                        break;
                    }
                }
                s->run_length += (s->a0 - old_a0);
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
                old_a0 = s->a0;
                s->a0 = s->b1;
                s->run_length += (s->a0 - old_a0);
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
            if (s->in_black)
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
                    s->in_black = FALSE;
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
                    s->in_black = TRUE;
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

        if (s->encoding == T4_COMPRESSION_T6)
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
                if (put_decoded_row(s))
                    return TRUE;
                s->in_black = FALSE;
                s->black_white = 0;
                s->run_length = 0;
                s->row_len = 0;
            }
        }
    }
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

static void t4_t6_decode_rx_status(t4_t6_decode_state_t *s, int status)
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
        t4_t6_decode_put(s, NULL, 0);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected rx status - %d!\n", status);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_t6_decode_put_bit(t4_t6_decode_state_t *s, int bit)
{
    if (bit < 0)
    {
        t4_t6_decode_rx_status(s, bit);
        return TRUE;
    }
    s->compressed_image_size++;
    if (put_bits(s, bit & 1, 1))
        return T4_DECODE_OK;
    return T4_DECODE_MORE_DATA;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_t6_decode_put(t4_t6_decode_state_t *s, const uint8_t buf[], size_t len)
{
    int i;
    uint8_t byte;

    if (len == 0)
    {
        /* Finalise the image */
        if (s->consecutive_eols != EOLS_TO_END_ANY_RX_PAGE)
        {
            /* Push enough zeros (13) through the decoder to flush out any remaining codes */
            put_bits(s, 0, 8);
            put_bits(s, 0, 5);
        }
        if (s->curr_bad_row_run)
        {
            if (s->curr_bad_row_run > s->longest_bad_row_run)
                s->longest_bad_row_run = s->curr_bad_row_run;
            s->curr_bad_row_run = 0;
        }
        /* Don't worry about the return value here. We are finishing anyway. */
        if (s->row_write_handler)
            s->row_write_handler(s->row_write_user_data, NULL, 0);
        s->rx_bits = 0;
        s->rx_skip_bits = 0;
        s->rx_bitstream = 0;
        s->consecutive_eols = EOLS_TO_END_ANY_RX_PAGE;
        return T4_DECODE_OK;
    }

    for (i = 0;  i < len;  i++)
    {
        s->compressed_image_size += 8;
        byte = buf[i];
        if (put_bits(s, byte & 0xFF, 8))
            return T4_DECODE_OK;
    }
    return T4_DECODE_MORE_DATA;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_t6_decode_set_row_write_handler(t4_t6_decode_state_t *s,
                                                     t4_row_write_handler_t handler,
                                                     void *user_data)
{
    s->row_write_handler = handler;
    s->row_write_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_t6_decode_set_encoding(t4_t6_decode_state_t *s, int encoding)
{
    switch (encoding)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        s->encoding = encoding;
        return 0;
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t4_t6_decode_get_image_width(t4_t6_decode_state_t *s)
{
    return s->image_width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t4_t6_decode_get_image_length(t4_t6_decode_state_t *s)
{
    return s->image_length;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_t6_decode_get_compressed_image_size(t4_t6_decode_state_t *s)
{
    return s->compressed_image_size;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t4_t6_decode_get_logging_state(t4_t6_decode_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_t6_decode_restart(t4_t6_decode_state_t *s, int image_width)
{
    int bytes_per_row;
    int run_space;
    uint32_t *bufptr;
    uint8_t *bufptr8;

    /* Calculate the scanline/tile width. */
    run_space = (image_width + 4)*sizeof(uint32_t);
    if (s->bytes_per_row == 0  ||  image_width != s->image_width)
    {
        /* Allocate the space required for decoding the new row length. */
        if ((bufptr = (uint32_t *) realloc(s->cur_runs, run_space)) == NULL)
            return -1;
        s->cur_runs = bufptr;
        if ((bufptr = (uint32_t *) realloc(s->ref_runs, run_space)) == NULL)
            return -1;
        s->ref_runs = bufptr;
        s->image_width = image_width;
    }
    bytes_per_row = (image_width + 7)/8;
    if (bytes_per_row != s->bytes_per_row)
    {
        if ((bufptr8 = (uint8_t *) realloc(s->row_buf, bytes_per_row)) == NULL)
            return -1;
        s->row_buf = bufptr8;
        s->bytes_per_row = bytes_per_row;
    }

    s->rx_bits = 0;
    s->rx_skip_bits = 0;
    s->rx_bitstream = 0;
    s->row_bits = 0;
    s->min_row_bits = INT_MAX;
    s->max_row_bits = 0;

    s->compressed_image_size = 0;
    s->bad_rows = 0;
    s->longest_bad_row_run = 0;
    s->curr_bad_row_run = 0;
    s->image_length = 0;
    s->pixel_stream = 0;
    s->pixels = 8;

    s->row_len = 0;
    s->in_black = FALSE;
    s->black_white = 0;
    s->b_cursor = 1;
    s->a_cursor = 0;
    s->b1 = s->image_width;
    s->a0 = 0;
    s->run_length = 0;
    s->row_is_2d = (s->encoding == T4_COMPRESSION_T6);
    /* We start at -1 EOLs for 1D and 2D decoding, as an indication we are waiting for the
       first EOL. T.6 coding starts without any preamble. */
    s->consecutive_eols = (s->encoding == T4_COMPRESSION_T6)  ?  0  :  -1;

    if (s->cur_runs)
        memset(s->cur_runs, 0, run_space);
    /* Initialise the reference line to all white */
    if (s->ref_runs)
    {
        memset(s->ref_runs, 0, run_space);
        s->ref_runs[0] = s->image_width;
    }
    if (s->row_buf)
        memset(s->row_buf, 0, s->bytes_per_row);

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t4_t6_decode_state_t *) t4_t6_decode_init(t4_t6_decode_state_t *s,
                                                       int encoding,
                                                       int image_width,
                                                       t4_row_write_handler_t handler,
                                                       void *user_data)
{
    if (s == NULL)
    {
        if ((s = (t4_t6_decode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.4/T.6");

    s->encoding = encoding;
    s->row_write_handler = handler;
    s->row_write_user_data = user_data;
    t4_t6_decode_restart(s, image_width);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_t6_decode_release(t4_t6_decode_state_t *s)
{
    free_buffers(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_t6_decode_free(t4_t6_decode_state_t *s)
{
    int ret;

    ret = t4_t6_decode_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
