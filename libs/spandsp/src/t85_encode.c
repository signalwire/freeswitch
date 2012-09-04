/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t85_encode.c - ITU T.85 JBIG for FAX image compression
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009, 2010 Steve Underwood
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
#include <time.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/timezone.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/t81_t82_arith_coding.h"
#include "spandsp/t85.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"

/* Image length update status */
enum
{
    NEWLEN_NONE = 0,
    NEWLEN_PENDING = 1,
    NEWLEN_HANDLED = 2
};

static __inline__ void unpack_32(uint8_t *s, int32_t value)
{
    s[3] = value & 0xFF;
    value >>= 8;
    s[2] = value & 0xFF;
    value >>= 8;
    s[1] = value & 0xFF;
    value >>= 8;
    s[0] = value & 0xFF;
}
/*- End of function --------------------------------------------------------*/

static void put_stuff(t85_encode_state_t *s, const uint8_t buf[], int len)
{
    uint8_t *new_buf;
    uint32_t bytes_per_row;

    if (s->bitstream_iptr + len >= s->bitstream_len)
    {
        /* TODO: Handle memory allocation errors properly */
        /* The number of uncompressed bytes per row seems like a reasonable measure
           of what to expect as a poor case for a compressed row. */
        bytes_per_row = (s->xd + 7) >> 3;
        if ((new_buf = realloc(s->bitstream, s->bitstream_len + len + bytes_per_row)) == NULL)
            return;
        s->bitstream = new_buf;
        s->bitstream_len += (len + bytes_per_row);
    }
    memcpy(&s->bitstream[s->bitstream_iptr], buf, len);
    s->bitstream_iptr += len;
    s->compressed_image_size += len;
}
/*- End of function --------------------------------------------------------*/

/* Callback function for the arithmetic encoder */
static void output_byte(void *user_data, int byte)
{
    t85_encode_state_t *s;
    uint8_t c = byte;

    s = (t85_encode_state_t *) user_data;
    c = (uint8_t) byte;
    put_stuff(s, &c, 1);
}
/*- End of function --------------------------------------------------------*/

static __inline__ void output_esc_code(t85_encode_state_t *s, int code)
{
    uint8_t buf[2];

    buf[0] = T82_ESC;
    buf[1] = code;
    put_stuff(s, buf, 2);
}
/*- End of function --------------------------------------------------------*/

static __inline__ void output_newlen(t85_encode_state_t *s)
{
    uint8_t buf[6];

    if (s->newlen == NEWLEN_PENDING)
    {
        buf[0] = T82_ESC;
        buf[1] = T82_NEWLEN;
        unpack_32(&buf[2], s->yd);
        put_stuff(s, buf, 6);
        if (s->y == s->yd)
        {
            /* See T.82/6.2.6.2 */
            output_esc_code(s, T82_SDNORM);
        }
        s->newlen = NEWLEN_HANDLED;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void output_comment(t85_encode_state_t *s)
{
    uint8_t buf[6];

    if (s->comment)
    {
        buf[0] = T82_ESC;
        buf[1] = T82_COMMENT;
        unpack_32(&buf[2], s->comment_len);
        put_stuff(s, buf, 6);
        put_stuff(s, s->comment, s->comment_len);
        s->comment = NULL;
        s->comment_len = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void output_atmove(t85_encode_state_t *s)
{
    uint8_t buf[8];

    if (s->new_tx >= 0  &&  s->new_tx != s->tx)
    {
        s->tx = s->new_tx;
        buf[0] = T82_ESC;
        buf[1] = T82_ATMOVE;
        unpack_32(&buf[2], 0);
        buf[6] = s->tx;
        buf[7] = 0;
        put_stuff(s, buf, 8);
    }
}
/*- End of function --------------------------------------------------------*/

static void generate_bih(t85_encode_state_t *s, uint8_t *buf)
{
    /* DL - Initial layer to be transmitted */
    buf[0] = 0;
    /* D - Number of differential layers */
    buf[1] = 0;
    /* P - Number of bit planes */
    buf[2] = s->bit_planes;
    /* Unspecified */
    buf[3] = 0;
    /* XD - Horizontal image size at layer D */
    unpack_32(&buf[4], s->xd);
    /* YD - Vertical image size at layer D */
    unpack_32(&buf[8], s->yd);
    /* L0 - Rows per stripe, at the lowest resolution */
    unpack_32(&buf[12], s->l0);
    /* MX - Maximum horizontal offset allowed for AT pixel */
    buf[16] = s->mx;
    /* MY - Maximum vertical offset allowed for AT pixel */
    buf[17] = 0;
    /* Order byte: */
    /*  4 unused bits */
    /*  HITOLO - transmission order of differential layers */
    /*  SEQ - indication of progressive-compatible sequential coding */
    /*  ILEAVE - interleaved transmission order of multiple bit plane */
    /*  SMID - transmission order of stripes */
    /* Note that none of these are relevant to T.85 */
    buf[18] = 0;
    /* Options byte: */
    /*  1 unused bit */
    /*  LRLTWO - number of reference rows */
    /*  VLENGTH - indication of possible use of NEWLEN marker segment */
    /*  TPDON - use of TP for Typical Prediction for differential layers */
    /*  TPBON - use of TP for base layer */
    /*  DPON - use of Deterministic Prediction */
    /*  DPPRIV - use of private DP table */
    /*  DPLAST - use of last DP table */
    /* Note that only T85_TPBON, T85_VLENGTH, and T85_LRLTWO are relevant to T.85 */
    buf[19] = s->options;
}
/*- End of function --------------------------------------------------------*/
    
SPAN_DECLARE(void) t85_encode_set_options(t85_encode_state_t *s,
                                          uint32_t l0,
                                          int mx,
                                          int options)
{
    if (s->y > 0)
        return;

    /* Its still OK to change things */
    if (l0 >= 1  &&  l0 <= s->yd)
        s->l0 = l0;
    if (mx >= 0  &&  mx <= 127)
        s->mx = mx;
    if (options >= 0)
        s->options = options & (T85_TPBON | T85_VLENGTH | T85_LRLTWO);
}
/*- End of function --------------------------------------------------------*/

static int get_next_row(t85_encode_state_t *s)
{
    uint8_t buf[20];
    uint32_t bytes_per_row;
    const uint8_t *hp[3];
    uint8_t *z;
    uint32_t row_h[3];
    uint32_t j;
    int32_t o;
    uint32_t a;
    uint32_t p;
    uint32_t t;
    uint32_t c_min;
    uint32_t c_max;
    uint32_t cl_min;
    uint32_t cl_max;
    int ltp;
    int cx;
    int t_max;
    int i;

    if (s->y >= s->yd)
    {
        /* We have already finished pumping out the image */
        return -1;
    }

    s->bitstream_iptr = 0;
    s->bitstream_optr = 0;
    bytes_per_row = (s->xd + 7) >> 3;
    /* Rotate the three rows which we buffer */
    z = s->prev_row[2];
    s->prev_row[2] = s->prev_row[1];
    s->prev_row[1] = s->prev_row[0];
    s->prev_row[0] = z;

    /* Copy the new row to our buffer, and ensure the last byte of the row is
       zero padded. The need to tweak this is actually the only reason for
       storing a third row. We do not want to tamper with the source buffer. */
    if (s->fill_with_white)
    {
        memset(s->prev_row[0], 0, bytes_per_row);
    }
    else
    {
        if (s->row_read_handler(s->row_read_user_data, s->prev_row[0], bytes_per_row) <= 0)
        {
            /* The source has stopped feeding us rows early. Try to clip the image
               to the current size. */
            if (t85_encode_set_image_length(s, 1) == 0)
                return 0;
            /* We can't clip the image to the current length. We will have to
               continue up to the original length with blank (all white) rows. */
            s->fill_with_white = TRUE;
            memset(s->prev_row[0], 0, bytes_per_row);
        }
    }
    if ((s->xd & 7))
        s->prev_row[0][bytes_per_row - 1] &= ~((1 << (8 - (s->xd & 7))) - 1);

    if (s->current_bit_plane == 0  &&  s->y == 0)
    {
        /* Things that need to be done before the first row is encoded */
        generate_bih(s, buf);
        put_stuff(s, buf, 20);
    }

    if (s->i == 0)
    {
        /* Things that need to be done before the next SDE is encoded */
        output_newlen(s);
        output_comment(s);
        output_atmove(s);
        if (s->mx == 0)
        {
            /* Disable ATMOVE analysis */
            s->new_tx = 0;
        }
        else
        {
            /* Enable ATMOVE analysis */
            s->new_tx = -1;
            s->c_all = 0;
            for (i = 0;  i <= s->mx;  i++)
                s->c[i] = 0;
        }
        t81_t82_arith_encode_restart(&s->s, TRUE);
    }

    /* Typical prediction */
    ltp = FALSE;
    if ((s->options & T85_TPBON))
    {
        /* Look for a match between the rows */
        ltp = (memcmp(s->prev_row[0], s->prev_row[1], bytes_per_row) == 0);
        t81_t82_arith_encode(&s->s,
                             (s->options & T85_LRLTWO)  ?  TPB2CX  :  TPB3CX,
                             (ltp == s->prev_ltp));
        s->prev_ltp = ltp;
    }

    if (!ltp)
    {
        /* Pointer to the first image byte in each the three rows of interest */
        hp[0] = s->prev_row[0];
        hp[1] = s->prev_row[1];
        hp[2] = s->prev_row[2];

        row_h[0] = 0;
        row_h[1] = (uint32_t) hp[1][0] << 8;
        row_h[2] = (uint32_t) hp[2][0] << 8;

        /* Encode row */
        for (j = 0;  j < s->xd;  )
        {
            row_h[0] |= hp[0][0];
            if (j < (bytes_per_row - 1)*8)
            {
                row_h[1] |= hp[1][1];
                row_h[2] |= hp[2][1];
            }
            if ((s->options & T85_LRLTWO))
            {
                /* Two row template */
                do
                {
                    row_h[0] <<= 1;
                    row_h[1] <<= 1;
                    row_h[2] <<= 1;
                    cx = (row_h[0] >> 9) & 0x00F;
                    if (s->tx)
                    {
                        cx |= ((row_h[1] >> 10) & 0x3E0);
                        if (j >= (uint32_t) s->tx)
                        {
                            o = (j - s->tx) - (j & ~7);
                            cx |= (((hp[0][o >> 3] >> (7 - (o & 7))) & 1) << 4);
                        }
                    }
                    else
                    {
                        cx |= ((row_h[1] >> 10) & 0x3F0);
                    }
                    p = (row_h[0] >> 8) & 1;
                    t81_t82_arith_encode(&s->s, cx, p);

                    /* Update the statistics for adaptive template changes,
                       if this analysis is in progress. */
                    if (s->new_tx < 0  &&  j >= s->mx  &&  j < s->xd - 2)
                    {
                        if (p == ((row_h[1] >> 14) & 1))
                            s->c[0]++;
                        for (t = 5;  t <= s->mx  &&  t <= j;  t++)
                        {
                            o = (j - t) - (j & ~7);
                            a = (hp[0][o >> 3] >> (7 - (o & 7))) & 1;
                            if (a == p)
                                s->c[t]++;
                        }
                        if (p == 0)
                        {
                            for (  ;  t <= s->mx;  t++)
                                s->c[t]++;
                        }
                        ++s->c_all;
                    }
                }
                while ((++j & 7)  &&  j < s->xd);
            }
            else
            {
                /* Three row template */
                do
                {
                    row_h[0] <<= 1;
                    row_h[1] <<= 1;
                    row_h[2] <<= 1;
                    cx = ((row_h[2] >> 8) & 0x380) | ((row_h[0] >> 9) & 0x003);
                    if (s->tx)
                    {
                        cx |= ((row_h[1] >> 12) & 0x078);
                        if (j >= (uint32_t) s->tx)
                        {
                            o = (j - s->tx) - (j & ~7);
                            cx |= (((hp[0][o >> 3] >> (7 - (o & 7))) & 1) << 2);
                        }
                    }
                    else
                    {
                        cx |= ((row_h[1] >> 12) & 0x07C);
                    }
                    p = (row_h[0] >> 8) & 1;
                    t81_t82_arith_encode(&s->s, cx, p);

                    /* Update the statistics for adaptive template changes,
                       if this analysis is in progress. */
                    if (s->new_tx < 0  &&  j >= s->mx  &&  j < s->xd - 2)
                    {
                        if (p == ((row_h[1] >> 14) & 1))
                            s->c[0]++;
                        for (t = 3;  t <= s->mx  &&  t <= j;  t++)
                        {
                            o = (j - t) - (j & ~7);
                            a = (hp[0][o >> 3] >> (7 - (o & 7))) & 1;
                            if (a == p)
                                s->c[t]++;
                        }
                        if (p == 0)
                        {
                            for (  ;  t <= s->mx;  t++)
                                s->c[t]++;
                        }
                        ++s->c_all;
                    }
                }
                while ((++j & 7)  &&  j < s->xd);
            }
            hp[0]++;
            hp[1]++;
            hp[2]++;
        }
    }

    s->i++;
    s->y++;
    if (s->i == s->l0  ||  s->y == s->yd)
    {
        /* We are at the end of the stripe */
        t81_t82_arith_encode_flush(&s->s);
        output_esc_code(s, T82_SDNORM);
        s->i = 0;
        output_newlen(s);
    }

    /* T.82/Annex C - is it time for an adaptive template change? */
    if (s->new_tx < 0  &&  s->c_all > 2048)
    {
        c_min =
        cl_min = UINT32_MAX;
        c_max =
        cl_max = 0;
        t_max = 0;
        for (i = (s->options & T85_LRLTWO)  ?  5  :  3;  i <= s->mx;  i++)
        {
            if (s->c[i] > c_max)
                c_max = s->c[i];
            if (s->c[i] < c_min)
                c_min = s->c[i];
            if (s->c[i] > s->c[t_max])
                t_max = i;
        }
        cl_min = (s->c[0] < c_min)  ?  s->c[0]  :  c_min;
        cl_max = (s->c[0] > c_max)  ?  s->c[0]  :  c_max;
        /* This test is straight from T.82/Figure C.2 */
        if ((s->c_all - c_max) < (s->c_all >> 3)
            &&
            (c_max - s->c[s->tx]) > (s->c_all - c_max)
            &&
            (c_max - s->c[s->tx]) > (s->c_all >> 4)
            &&
            (c_max - (s->c_all - s->c[s->tx])) > (s->c_all - c_max)
            &&
            (c_max - (s->c_all - s->c[s->tx])) > (s->c_all >> 4)
            &&
            (c_max - c_min) > (s->c_all >> 2)
            &&
            (s->tx  ||  (cl_max - cl_min) > (s->c_all >> 3)))
        {
            /* It is time to perform an ATMOVE */
            s->new_tx = t_max;
        }
        else
        {
            /* Disable further analysis */
            s->new_tx = s->tx;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_encode_set_image_width(t85_encode_state_t *s, uint32_t image_width)
{
    int bytes_per_row;
    uint8_t *t;

    if (s->xd == image_width)
        return 0;
    /* Are we too late to change the width for this page? */
    if (s->y > 0)
        return -1;
    s->xd = image_width;
    bytes_per_row = (s->xd + 7) >> 3;
    if ((t = (uint8_t *) realloc(s->row_buf, 3*bytes_per_row)) == NULL)
        return -1;
    s->row_buf = t;
    memset(s->row_buf, 0, 3*bytes_per_row);
    s->prev_row[0] = s->row_buf;
    s->prev_row[1] = s->row_buf + bytes_per_row;
    s->prev_row[2] = s->row_buf + 2*bytes_per_row;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_encode_set_image_length(t85_encode_state_t *s, uint32_t length)
{
    /* We must have variable length enabled.
       We do not allow the length to be changed multiple times.
       We only allow an image to be shrunk, and not stretched.
       We do not allow the length to become zero. */
    if (!(s->options & T85_VLENGTH)  ||  s->newlen == NEWLEN_HANDLED  ||  length >= s->yd  ||  length < 1)
    {
        /* Invalid parameter */
        return -1;
    }
    if (s->y > 0)
    {
        /* TODO: If we are already beyond the new length, we scale back the new length silently.
                 Is there any downside to this? */
        if (length < s->y)
            length = s->y;
        if (s->yd != length)
            s->newlen = NEWLEN_PENDING;
    }
    s->yd = length;
    if (s->y == s->yd)
    {
        /* We are already at the end of the image, so finish it off. */
        if (s->i > 0)
        {
            t81_t82_arith_encode_flush(&s->s);
            output_esc_code(s, T82_SDNORM);
            s->i = 0;
        }
        output_newlen(s);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t85_encode_abort(t85_encode_state_t *s)
{
    output_esc_code(s, T82_ABORT);
    /* Make the image appear to be complete, so the encoder stops outputting.
       Take care, as this means s->y is now telling lies. */
    s->y = s->yd;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t85_encode_comment(t85_encode_state_t *s, const uint8_t comment[], size_t len)
{
    s->comment = comment;
    s->comment_len = len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_encode_image_complete(t85_encode_state_t *s)
{
    if (s->y >= s->yd)
        return SIG_STATUS_END_OF_DATA;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_encode_get(t85_encode_state_t *s, uint8_t buf[], size_t max_len)
{
    int len;
    int n;

    for (len = 0;  len < max_len;  len += n)
    {
        if (s->bitstream_optr >= s->bitstream_iptr)
        {
            if (get_next_row(s) < 0)
                return len;
        }
        n = s->bitstream_iptr - s->bitstream_optr;
        if (n > max_len - len)
            n = max_len - len;
        memcpy(&buf[len], &s->bitstream[s->bitstream_optr], n);
        s->bitstream_optr += n;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t85_encode_get_image_width(t85_encode_state_t *s)
{
    return s->xd;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t85_encode_get_image_length(t85_encode_state_t *s)
{
    return s->yd;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_encode_get_compressed_image_size(t85_encode_state_t *s)
{
    return s->compressed_image_size*8;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_encode_set_row_read_handler(t85_encode_state_t *s,
                                                  t4_row_read_handler_t handler,
                                                  void *user_data)
{
    s->row_read_handler = handler;
    s->row_read_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_encode_restart(t85_encode_state_t *s, uint32_t image_width, uint32_t image_length)
{
    int bytes_per_row;

    /* Allow the image width to be anything, although only a few values are actually
       permitted by the T.85 and T.4 specs. Higher levels in the stack need to impose
       these restrictions. */
    t85_encode_set_image_width(s, image_width);
    bytes_per_row = (s->xd + 7) >> 3;
    memset(s->row_buf, 0, 3*bytes_per_row);
    s->yd = image_length;

    s->comment = NULL;
    s->comment_len = 0;
    s->y = 0;
    s->i = 0;
    s->newlen = NEWLEN_NONE;
    s->new_tx = -1;
    s->tx = 0;
    s->prev_ltp = FALSE;
    s->bitstream_iptr = 0;
    s->bitstream_optr = 0;
    if (s->bitstream)
    {
        free(s->bitstream);
        s->bitstream = NULL;
    }
    s->bitstream_len = 0;
    s->fill_with_white = FALSE;
    s->compressed_image_size = 0;

    t81_t82_arith_encode_init(&s->s, output_byte, s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t85_encode_get_logging_state(t85_encode_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t85_encode_state_t *) t85_encode_init(t85_encode_state_t *s,
                                                   uint32_t image_width,
                                                   uint32_t image_length,
                                                   t4_row_read_handler_t handler,
                                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (t85_encode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.85");

    s->row_read_handler = handler;
    s->row_read_user_data = user_data;

    /* T.85 BASIC setting for L0. In T.85 this may only be changed if T.30 negotiation permits. */
    s->l0 = 128;
    /* No ATMOVE pending */
    s->mx = 127;
    /* Default options */
    s->options = T85_TPBON | T85_VLENGTH;

    s->bitstream = NULL;
    s->bitstream_len = 0;

    s->bit_planes = 1;
    s->current_bit_plane = 0;

    t85_encode_restart(s, image_width, image_length);
    
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_encode_release(t85_encode_state_t *s)
{
    if (s->row_buf)
    {
        free(s->row_buf);
        s->row_buf = NULL;
    }
    if (s->bitstream)
    {
        free(s->bitstream);
        s->bitstream = NULL;
        s->bitstream_len = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_encode_free(t85_encode_state_t *s)
{
    int ret;

    ret = t85_encode_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
