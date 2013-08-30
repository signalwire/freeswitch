/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t85_decode.c - ITU T.85 JBIG for FAX image decompression
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
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
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

static __inline__ int32_t pack_32(const uint8_t *s)
{
    int32_t value;

    value = (((int32_t) s[0] << 24) | ((int32_t) s[1] << 16) | ((int32_t) s[2] << 8) | (int32_t) s[3]);
    return value;
}
/*- End of function --------------------------------------------------------*/

/* Decode some PSCD bytes, output the decoded rows as they are completed. Return
   the number of bytes which have actually been read. This will be less than len
   if a marker segment was part of the data or if the final byte was 0xFF, meaning
   that this code can not determine whether we have a marker segment. */
static size_t decode_pscd(t85_decode_state_t *s, const uint8_t data[], size_t len)
{
    uint8_t *hp[3];
    int32_t o;
    int cx;
    int i;
    int pix;
    int slntp;
    int buffered_rows;

    buffered_rows = (s->options & T85_LRLTWO)  ?  2  :  3;
    /* Forward data to the arithmetic decoder */
    s->s.pscd_ptr = data;
    s->s.pscd_end = data + len;

    for (s->interrupt = false;  s->i < s->l0  &&  s->y < s->yd  &&  !s->interrupt;  s->i++, s->y++)
    {
        /* Point to the current image bytes */
        for (i = 0;  i < 3;  i++)
            hp[i] = s->row_buf + s->p[i]*s->bytes_per_row + (s->x >> 3);

        /* Adaptive template changes */
        if (s->x == 0  &&  s->pseudo)
        {
            for (i = 0;  i < s->at_moves;  i++)
            {
                if (s->at_row[i] == s->i)
                    s->tx = s->at_tx[i];
            }
        }

        /* Typical prediction */
        if ((s->options & T85_TPBON)  &&  s->pseudo)
        {
            slntp = t81_t82_arith_decode(&s->s, (s->options & T85_LRLTWO)  ?  TPB2CX  :  TPB3CX);
            if (slntp < 0)
                return s->s.pscd_ptr - data;
            s->lntp = !(slntp ^ s->lntp);
            if (!s->lntp)
            {
                /* This row is 'typical' (i.e. identical to the previous one) */
                if (s->p[1] < 0)
                {
                    /* First row of page or (following SDRST) of stripe */
                    for (i = 0;  i < s->bytes_per_row;  i++)
                        hp[0][i] = 0;
                    s->interrupt = s->row_write_handler(s->row_write_user_data, hp[0], s->bytes_per_row);
                    /* Rotate the ring buffer that holds the last few rows */
                    s->p[2] = s->p[1];
                    s->p[1] = s->p[0];
                    if (++(s->p[0]) >= buffered_rows)
                        s->p[0] = 0;
                }
                else
                {
                    s->interrupt = s->row_write_handler(s->row_write_user_data, hp[1], s->bytes_per_row);
                    /* Duplicate the last row in the ring buffer */
                    s->p[2] = s->p[1];
                }
                continue;
            }
            /* This row is 'not typical' and has to be coded completely */
        }
        s->pseudo = false;

        if (s->x == 0)
        {
            s->row_h[0] = 0;
            s->row_h[1] = (s->p[1] >= 0)  ?  ((int32_t) hp[1][0] << 8)  :  0;
            s->row_h[2] = (s->p[2] >= 0)  ?  ((int32_t) hp[2][0] << 8)  :  0;
        }

        /* Decode row */
        while (s->x < s->xd)
        {
            if ((s->x & 7) == 0)
            {
                if (s->x < (s->bytes_per_row - 1)*8  &&  s->p[1] >= 0)
                {
                    s->row_h[1] |= hp[1][1];
                    if (s->p[2] >= 0)
                        s->row_h[2] |= hp[2][1];
                }
            }
            if ((s->options & T85_LRLTWO))
            {
                /* Two row template */
                do
                {
                    cx = (s->row_h[0] & 0x00F);
                    if (s->tx)
                    {
                        cx |= ((s->row_h[1] >> 9) & 0x3E0);
                        if (s->x >= (uint32_t) s->tx)
                        {
                            if (s->tx < 8)
                            {
                                cx |= ((s->row_h[0] >> (s->tx - 5)) & 0x010);
                            }
                            else
                            {
                                o = (s->x - s->tx) - (s->x & ~7);
                                cx |= (((hp[0][o >> 3] >> (7 - (o & 7))) & 1) << 4);
                            }
                        }
                    }
                    else
                    {
                        cx |= ((s->row_h[1] >> 9) & 0x3F0);
                    }
                    pix = t81_t82_arith_decode(&s->s, cx);
                    if (pix < 0)
                        return s->s.pscd_ptr - data;
                    s->row_h[0] = (s->row_h[0] << 1) | pix;
                    s->row_h[1] <<= 1;
                }
                while ((++s->x & 7)  &&  s->x < s->xd);
            }
            else
            {
                /* Three row template */
                do
                {
                    cx = ((s->row_h[2] >> 7) & 0x380) | (s->row_h[0] & 0x003);
                    if (s->tx)
                    {
                        cx |= ((s->row_h[1] >> 11) & 0x078);
                        if (s->x >= (uint32_t) s->tx)
                        {
                            if (s->tx < 8)
                            {
                                cx |= ((s->row_h[0] >> (s->tx - 3)) & 0x004);
                            }
                            else
                            {
                                o = (s->x - s->tx) - (s->x & ~7);
                                cx |= (((hp[0][o >> 3] >> (7 - (o & 7))) & 1) << 2);
                            }
                        }
                    }
                    else
                    {
                        cx |= ((s->row_h[1] >> 11) & 0x07C);
                    }
                    pix = t81_t82_arith_decode(&s->s, cx);
                    if (pix < 0)
                        return s->s.pscd_ptr - data;
                    s->row_h[0] = (s->row_h[0] << 1) | pix;
                    s->row_h[1] <<= 1;
                    s->row_h[2] <<= 1;
                }
                while ((++s->x & 7)  &&  s->x < s->xd);
            }
            *hp[0]++ = s->row_h[0];
            hp[1]++;
            hp[2]++;
        }
        *(hp[0] - 1) <<= (s->bytes_per_row*8 - s->xd);
        s->interrupt = s->row_write_handler(s->row_write_user_data, &s->row_buf[s->p[0]*s->bytes_per_row], s->bytes_per_row);
        s->x = 0;
        s->pseudo = true;
        /* Shuffle the row buffers */
        s->p[2] = s->p[1];
        s->p[1] = s->p[0];
        if (++(s->p[0]) >= buffered_rows)
            s->p[0] = 0;
    }
    return s->s.pscd_ptr - data;
}
/*- End of function --------------------------------------------------------*/

static int finish_sde(t85_decode_state_t *s)
{
    /* Decode final pixels based on trailing zero bytes */
    s->s.nopadding = false;
    if (decode_pscd(s, s->buffer, 2) != 2  &&  s->interrupt)
        return 1;

    /* Prepare decoder for next SDE */
    t81_t82_arith_decode_restart(&s->s, s->buffer[1] == T82_SDNORM);
    s->s.nopadding = s->options & T85_VLENGTH;

    s->x = 0;
    s->i = 0;
    s->pseudo = true;
    s->at_moves = 0;
    if (s->buffer[1] == T82_SDRST)
    {
        s->tx = 0;
        s->lntp = true;
        s->p[0] = 0;
        s->p[1] = -1;
        s->p[2] = -1;
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bool) t85_analyse_header(uint32_t *width, uint32_t *length, const uint8_t data[], size_t len)
{
    if (len < 20)
        return false;
    *width = pack_32(&data[6]);
    *length = pack_32(&data[10]);
    return true;
}
/*- End of function --------------------------------------------------------*/

static int check_bih(t85_decode_state_t *s)
{
    /* Check that the fixed parameters have the values they are expected to be
       fixed at - see T.85/Table 1 */
    /* DL - Initial layer to be transmitted */
    /* D - Number of differential layers */
    /* Unspecified byte */
    /* MY - Maximum vertical offset allowed for AT pixel */
    /* Order byte */
    if (s->buffer[0] != 0
        ||
        s->buffer[1] != 0
        ||
        s->buffer[3] != 0
        ||
        s->buffer[17] != 0
        ||
#if T85_STRICT_ORDER_BITS
        s->buffer[18] != 0)
#else
        (s->buffer[18] & 0xF0) != 0)
#endif
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "BIH invalid. Fixed bytes do not contain expected values.\n");
        return T4_DECODE_INVALID_DATA;
    }
    /* P - Number of bit planes */
    if (s->buffer[2] < s->min_bit_planes  ||  s->buffer[2] > s->max_bit_planes)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "BIH invalid. %d bit planes. Should be %d to %d.\n", s->buffer[2], s->min_bit_planes, s->max_bit_planes);
        return T4_DECODE_INVALID_DATA;
    }
    s->bit_planes = s->buffer[2];
    s->current_bit_plane = 0;
    /* Now look at the stuff which actually counts in a T.85 header. */
    /* XD - Horizontal image size at layer D */
    s->xd = pack_32(&s->buffer[4]);
    if (s->xd == 0  ||  (s->max_xd  &&  s->xd > s->max_xd))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "BIH invalid. Width is %" PRIu32 "\n", s->xd);
        return T4_DECODE_INVALID_DATA;
    }
    /* YD - Vertical image size at layer D */
    s->yd = pack_32(&s->buffer[8]);
    if (s->yd == 0  ||  (s->max_yd  &&  s->yd > s->max_yd))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "BIH invalid. Length is %" PRIu32 "\n", s->yd);
        return T4_DECODE_INVALID_DATA;
    }
    /* L0 - Rows per stripe, at the lowest resolution */
    s->l0 = pack_32(&s->buffer[12]);
    if (s->l0 == 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "BIH invalid. L0 is %" PRIu32 "\n", s->l0);
        return T4_DECODE_INVALID_DATA;
    }
    /* MX - Maximum horizontal offset allowed for AT pixel */
    s->mx = s->buffer[16];
    if (s->mx > 127)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "BIH invalid. MX is %d\n", s->mx);
        return T4_DECODE_INVALID_DATA;
    }
    /* Options byte */
    s->options = s->buffer[19];
    if ((s->options & 0x97))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "BIH invalid. Options are 0x%X\n", s->options);
        return T4_DECODE_INVALID_DATA;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "BIH is OK. Image is %" PRIu32 "x%" PRIu32 " pixels\n", s->xd, s->yd);
    return T4_DECODE_OK;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t85_decode_rx_status(t85_decode_state_t *s, int status)
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
        t85_decode_put(s, NULL, 0);
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected rx status - %d!\n", status);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_decode_put(t85_decode_state_t *s, const uint8_t data[], size_t len)
{
    int ret;
    uint32_t y;
    uint8_t *buf;
    size_t bytes_per_row;
    size_t min_len;
    size_t chunk;
    size_t cnt;
    int i;

    if (len == 0)
    {
        if (s->y >= s->yd)
            return T4_DECODE_OK;
        /* This is the end of image condition */
        s->end_of_data = 1;
    }

    s->compressed_image_size += len;
    cnt = 0;

    if (s->bie_len < 20)
    {
        /* Read in the 20-byte BIH */
        i = (s->bie_len + len > 20)  ?  (20 - s->bie_len)  :  len;
        memcpy(&s->buffer[s->bie_len], data, i);
        s->bie_len += i;
        cnt = i;
        if (s->bie_len < 20)
            return T4_DECODE_MORE_DATA;
        if ((ret = check_bih(s)) != T4_DECODE_OK)
            return ret;
        /* Set up the two/three row buffer */
        bytes_per_row = (s->xd + 7) >> 3;
        min_len = ((s->options & T85_LRLTWO)  ?  2  :  3)*bytes_per_row;
        if (min_len > s->row_buf_len)
        {
            /* We need to expand the 3 row buffer */
            if ((buf = (uint8_t *) span_realloc(s->row_buf, min_len)) == NULL)
                return T4_DECODE_NOMEM;
            s->row_buf = buf;
            s->row_buf_len = min_len;
        }

        t81_t82_arith_decode_init(&s->s);
        s->s.nopadding = s->options & T85_VLENGTH;
        if (s->comment)
        {
            span_free(s->comment);
            s->comment = NULL;
        }
        s->comment_len = 0;
        s->comment_progress = 0;
        s->buf_len = 0;
        s->buf_needed = 2;
        s->x = 0;
        s->y = 0;
        s->i = 0;
        s->pseudo = true;
        s->at_moves = 0;
        s->tx = 0;
        s->lntp = true;
        s->bytes_per_row = bytes_per_row;
        s->p[0] = 0;
        s->p[1] = -1;
        s->p[2] = -1;
    }

    /* BID processing loop */
    while (cnt < len  ||  s->end_of_data == 1)
    {
        if (s->end_of_data == 1)
        {
            s->buf_needed = 2;
            s->options &= ~T85_VLENGTH;
            s->end_of_data = 2;
        }
        if (s->comment_len)
        {
            /* We are in a COMMENT. Absorb its contents */
            chunk = len - cnt;
            if ((s->comment_progress + chunk) >= s->comment_len)
            {
                /* Finished */
                chunk = s->comment_len - s->comment_progress;
                /* If the comment was too long to be passed to the handler, we still
                   call the handler with the buffer set to NULL, so it knows a large
                   comment has occurred. */
                if (s->comment)
                    memcpy(&s->comment[s->comment_progress], &data[cnt], chunk);
                if (s->comment_handler)
                    s->interrupt = s->comment_handler(s->comment_user_data, s->comment, s->comment_len);
                if (s->comment)
                {
                    span_free(s->comment);
                    s->comment = NULL;
                }
                s->comment_len = 0;
                s->comment_progress = 0;
            }
            else
            {
                if (s->comment)
                    memcpy(&s->comment[s->comment_progress], &data[cnt], chunk);
                s->comment_progress += chunk;
            }
            cnt += chunk;
            continue;
        }

        /* Load marker segments into s->buffer for processing */
        if (s->buf_len > 0)
        {
            /* We are in a marker of some kind. Load the first 2 bytes of
               the marker, so we can determine its type, and hence its full
               length. */
            while (s->buf_len < s->buf_needed  &&  cnt < len)
                s->buffer[s->buf_len++] = data[cnt++];
            /* Check we have enough bytes to see the message type */
            if (s->buf_len < s->buf_needed)
                continue;
            switch (s->buffer[1])
            {
            case T82_STUFF:
                /* Forward stuffed 0xFF to arithmetic decoder. This is likely to be
                   the commonest thing for us to hit here. */
                decode_pscd(s, s->buffer, 2);
                s->buf_len = 0;

                if (s->interrupt)
                    return T4_DECODE_INTERRUPT;
                break;
            case T82_ABORT:
                s->buf_len = 0;
                return T4_DECODE_ABORTED;
            case T82_COMMENT:
                s->buf_needed = 6;
                if (s->buf_len < 6)
                    continue;
                s->buf_needed = 2;
                s->buf_len = 0;

                s->comment_len = pack_32(&s->buffer[2]);
                /* Only try to buffer and process the comment's contents if we have
                   a defined callback routine to do something with it. */
                /* If this allocate fails we carry on working just fine, and don't try to
                   process the contents of the comment. That is fairly benign, as
                   the comments are not generally of critical importance, so let's
                   not worry. */
                if (s->comment_handler  &&  s->comment_len > 0  &&  s->comment_len <= s->max_comment_len)
                    s->comment = span_alloc(s->comment_len);
                s->comment_progress = 0;
                continue;
            case T82_ATMOVE:
                s->buf_needed = 8;
                if (s->buf_len < 8)
                    continue;
                s->buf_needed = 2;
                s->buf_len = 0;

                if (s->at_moves >= T85_ATMOVES_MAX)
                    return T4_DECODE_INVALID_DATA;
                s->at_row[s->at_moves] = pack_32(&s->buffer[2]);
                s->at_tx[s->at_moves] = s->buffer[6];
                if (s->at_tx[s->at_moves] > s->mx
                    ||
                    (s->at_tx[s->at_moves] > 0  &&  s->at_tx[s->at_moves] < ((s->options & T85_LRLTWO)  ?  5  :  3))
                    ||
                    s->buffer[7] != 0)
                {
                    return T4_DECODE_INVALID_DATA;
                }
                s->at_moves++;
                break;
            case T82_NEWLEN:
                s->buf_needed = 6;
                if (s->buf_len < 6)
                    continue;
                s->buf_needed = 2;
                s->buf_len = 0;

                if (!(s->options & T85_VLENGTH))
                    return T4_DECODE_INVALID_DATA;
                s->options &= ~T85_VLENGTH;
                y = pack_32(&s->buffer[2]);
                /* An update to the image length is not allowed to stretch it. */
                if (y > s->yd)
                    return T4_DECODE_INVALID_DATA;
                s->yd = y;
                break;
            case T82_SDNORM:
            case T82_SDRST:
                if (!(s->options & T85_VLENGTH))
                {
                    /* A plain SDNORM or SDRST with no peek ahead required */
                    s->buf_len = 0;
                    if (finish_sde(s))
                        return T4_DECODE_INTERRUPT;
                    /* Check whether this was the last SDE */
                    if (s->y >= s->yd)
                    {
                        s->compressed_image_size -= (len - cnt);
                        return T4_DECODE_OK;
                    }
                    break;
                }
                /* This is the messy case. We need to peek ahead, as this element
                   might be immediately followed by a T82_NEWLEN which affects the
                   limit of what we decode here. */
                if (s->buf_needed < 3)
                    s->buf_needed = 3;
                if (s->buf_len < 3)
                    continue;
                /* Peek ahead to see whether a NEWLEN marker segment follows */
                if (s->buffer[2] != T82_ESC)
                {
                    /* This is not an escape sequence, so push the single peek-ahead
                       byte back into the buffer. We should always have just grabbed
                       at least one byte, so this should be safe. */
                    s->buf_needed = 2;
                    s->buf_len = 0;
                    cnt--;
                    /* Process the T82_SDNORM or T82_SDRST */
                    if (finish_sde(s))
                        return T4_DECODE_INTERRUPT;
                    /* Check whether this was the last SDE */
                    if (s->y >= s->yd)
                    {
                        s->compressed_image_size -= (len - cnt);
                        return T4_DECODE_OK;
                    }
                    break;
                }
                if (s->buf_needed < 4)
                    s->buf_needed = 4;
                if (s->buf_len < 4)
                    continue;
                if (s->buffer[3] != T82_NEWLEN)
                {
                    s->buf_needed = 2;

                    /* Process the T82_SDNORM or T82_SDRST */
                    if (finish_sde(s))
                        return T4_DECODE_INTERRUPT;
                    /* Check whether this was the last SDE */
                    if (s->y >= s->yd)
                    {
                        s->compressed_image_size -= (len - cnt);
                        return T4_DECODE_OK;
                    }
                    /* Recycle the two peek-ahead marker sequence bytes to
                       be processed later. */
                    s->buffer[0] = s->buffer[2];
                    s->buffer[1] = s->buffer[3];
                    s->buf_len = 2;
                    break;
                }
                if (s->buf_needed < 8)
                    s->buf_needed = 8;
                if (s->buf_len < 8)
                    continue;
                s->buf_needed = 2;
                s->buf_len = 0;
                /* We must have a complete T82_NEWLEN to be here, which we need
                   to process immediately. */
                s->options &= ~T85_VLENGTH;
                y = pack_32(&s->buffer[4]);
                /* An update to the image length is not allowed to stretch it. */
                if (y > s->yd)
                    return T4_DECODE_INVALID_DATA;
                /* Things look OK, so accept this new length, and proceed. */
                s->yd = y;
                /* Now process the T82_SDNORM or T82_SDRST */
                if (finish_sde(s))
                    return T4_DECODE_INTERRUPT;
                /* We might be at the end of the image now, but even if we are
                   there should still be a final training T82_SDNORM or T82_SDRST
                   that we should pick up. When we do, we won't wait for further
                   T82_NEWLEN entries, so we should stop crisply on the last byte
                   of the image. */
                break;
            default:
                s->buf_len = 0;
                return T4_DECODE_INVALID_DATA;
            }
        }
        else if (cnt < len  &&  data[cnt] == T82_ESC)
        {
            s->buffer[s->buf_len++] = data[cnt++];
        }
        else
        {
            /* We have found PSCD bytes */
            cnt += decode_pscd(s, data + cnt, len - cnt);
            if (s->interrupt)
                return T4_DECODE_INTERRUPT;
            /* We should only have stopped processing PSCD if
               we ran out of data, or hit a T82_ESC */
            if (cnt < len  &&  data[cnt] != T82_ESC)
                return T4_DECODE_INVALID_DATA;
        }
    }

    return T4_DECODE_MORE_DATA;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_decode_set_row_write_handler(t85_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data)
{
    s->row_write_handler = handler;
    s->row_write_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_decode_set_comment_handler(t85_decode_state_t *s,
                                                 uint32_t max_comment_len,
                                                 t4_row_write_handler_t handler,
                                                 void *user_data)
{
    s->max_comment_len = max_comment_len;
    s->comment_handler = handler;
    s->comment_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_decode_set_image_size_constraints(t85_decode_state_t *s,
                                                        uint32_t max_xd,
                                                        uint32_t max_yd)
{
    s->max_xd = max_xd;
    s->max_yd = max_yd;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t85_decode_get_image_width(t85_decode_state_t *s)
{
    return s->xd;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t85_decode_get_image_length(t85_decode_state_t *s)
{
    return s->yd;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_decode_get_compressed_image_size(t85_decode_state_t *s)
{
    return s->compressed_image_size*8;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_decode_new_plane(t85_decode_state_t *s)
{
    if (s->current_bit_plane >= s->bit_planes - 1)
        return -1;

    s->current_bit_plane++;
    s->tx = 0;
    memset(s->buffer, 0, sizeof(s->buffer));
    s->buf_len = 0;
    s->buf_needed = 0;
    s->at_moves = 0;
    memset(s->at_row, 0, sizeof(s->at_row));
    memset(s->at_tx, 0, sizeof(s->at_tx));
    memset(s->row_h, 0, sizeof(s->row_h));
    s->pseudo = false;
    s->lntp = false;
    s->interrupt = false;
    s->end_of_data = 0;
    if (s->comment)
    {
        span_free(s->comment);
        s->comment = NULL;
    }
    s->comment_len = 0;
    s->comment_progress = 0;
    s->compressed_image_size = 0;

    t81_t82_arith_decode_restart(&s->s, false);
    s->s.nopadding = s->options & T85_VLENGTH;

    s->buf_len = 0;
    s->buf_needed = 2;
    s->x = 0;
    s->y = 0;
    s->i = 0;
    s->pseudo = true;
    s->at_moves = 0;
    s->tx = 0;
    s->lntp = true;
    s->p[0] = 0;
    s->p[1] = -1;
    s->p[2] = -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_decode_restart(t85_decode_state_t *s)
{
    s->xd = 0;
    s->yd = 0;
    s->l0 = 0;
    s->mx = 0;
    s->bytes_per_row = 0;
    s->tx = 0;
    s->bie_len = 0;
    memset(s->buffer, 0, sizeof(s->buffer));
    s->buf_len = 0;
    s->buf_needed = 0;
    s->at_moves = 0;
    memset(s->at_row, 0, sizeof(s->at_row));
    memset(s->at_tx, 0, sizeof(s->at_tx));
    memset(s->row_h, 0, sizeof(s->row_h));
    s->pseudo = false;
    s->lntp = false;
    s->interrupt = false;
    s->end_of_data = 0;
    if (s->comment)
    {
        span_free(s->comment);
        s->comment = NULL;
    }
    s->comment_len = 0;
    s->comment_progress = 0;
    s->compressed_image_size = 0;

    t81_t82_arith_decode_restart(&s->s, false);

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t85_decode_get_logging_state(t85_decode_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t85_decode_state_t *) t85_decode_init(t85_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (t85_decode_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.85");

    s->row_write_handler = handler;
    s->row_write_user_data = user_data;

    s->min_bit_planes = 1;
    s->max_bit_planes = 1;

    s->max_xd = 0;
    s->max_yd = 0;

    t81_t82_arith_decode_init(&s->s);
    t85_decode_restart(s);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_decode_release(t85_decode_state_t *s)
{
    if (s->row_buf)
    {
        span_free(s->row_buf);
        s->row_buf = NULL;
    }
    if (s->comment)
    {
        span_free(s->comment);
        s->comment = NULL;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t85_decode_free(t85_decode_state_t *s)
{
    int ret;

    ret = t85_decode_release(s);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
