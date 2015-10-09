/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_non_ecm_buffer.c - A rate adapting buffer for T.38 non-ECM image
 *                        and TCF data
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005, 2006, 2007, 2008 Steve Underwood
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
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
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
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/dc_restore.h"
#include "spandsp/bit_operations.h"
#include "spandsp/async.h"
#include "spandsp/t38_non_ecm_buffer.h"

#include "spandsp/private/t38_non_ecm_buffer.h"

/* Phases */
enum
{
    TCF_AT_INITIAL_ALL_ONES = 0,
    TCF_AT_ALL_ZEROS = 1,
    IMAGE_WAITING_FOR_FIRST_EOL = 2,
    IMAGE_IN_PROGRESS = 3
};

static void restart_buffer(t38_non_ecm_buffer_state_t *s)
{
    /* This should be called when draining the buffer is complete, which should
       occur before any fresh data can possibly arrive to begin refilling it. */
    s->octet = 0xFF;
    s->flow_control_fill_octet = 0xFF;
    s->input_phase = (s->image_data_mode)  ?  IMAGE_WAITING_FOR_FIRST_EOL  :  TCF_AT_INITIAL_ALL_ONES;
    s->bit_stream = 0xFFFF;
    s->out_ptr = 0;
    s->in_ptr = 0;
    s->latest_eol_ptr = 0;
    s->data_finished = false;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_non_ecm_buffer_get_bit(void *user_data)
{
    t38_non_ecm_buffer_state_t *s;
    int bit;

    s = (t38_non_ecm_buffer_state_t *) user_data;

    if (s->bit_no <= 0)
    {
        /* We need another byte */
        if (s->out_ptr != s->latest_eol_ptr)
        {
            s->octet = s->data[s->out_ptr];
            s->out_ptr = (s->out_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
        }
        else
        {
            if (s->data_finished)
            {
                /* The queue is empty, and we have received the end of data signal. This must
                   really be the end to transmission. */
                restart_buffer(s);
                return SIG_STATUS_END_OF_DATA;
            }
            /* The queue is blocked, but this does not appear to be the end of the data. Idle with
               fill octets, which should be safe at this point. */
            s->octet = s->flow_control_fill_octet;
            s->flow_control_fill_octets++;
        }
        s->out_octets++;
        s->bit_no = 8;
    }
    s->bit_no--;
    bit = (s->octet >> 7) & 1;
    s->octet <<= 1;
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_non_ecm_buffer_push(t38_non_ecm_buffer_state_t *s)
{
    /* Don't flow control the data any more. Just push out the remainder of the data
       in the buffer as fast as we can, and shut down. */
    s->latest_eol_ptr = s->in_ptr;
    s->data_finished = true;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_non_ecm_buffer_inject(t38_non_ecm_buffer_state_t *s, const uint8_t *buf, int len)
{
    int i;
    int upper;
    int lower;

    /* TCF consists of:
            - zero or more ones, followed by
            - about 1.5s of zeros
       There may be a little junk at the end, as the modem shuts down.

       We can stuff with extra ones in the initial period of all ones, and we can stuff with extra
       zeros once the zeros start. The thing we need to be wary about is the odd zero bit in the
       midst of the ones, due to a bit error. */

    /* Non-ECM image data consists of:
            - zero or more ones, followed by
            - zero or more zeros, followed by
            - an EOL (end of line), which marks the start of the image, followed by
            - a succession of data rows, with an EOL at the end of each, followed by
            - an RTC (return to control)
       There may be a little junk at the end, as the modem shuts down.

       An EOL 11 zeros followed by a one in a T.4 1D image or 11 zeros followed by a one followed
       by a one or a zero in a T.4 2D image. An RTC consists of 6 EOLs in succession, with no
       pixel data between them.

       We can stuff with ones until we get the first EOL into our buffer, then we can stuff with
       zeros in front of each EOL at any point up the the RTC. We should not pad between the EOLs
       which make up the RTC. Most FAX machines don't care about this, but a few will not recognise
       the RTC if here is padding between the EOLs.

       We need to buffer whole rows before we output their beginning, so there is no possibility
       of underflow mid-row. */

    /* FoIP has latency issues, because of the fairly tight timeouts in the T.30 spec. We must
       ensure our buffering does everything needed to avoid underflows, and to meet the minimum
       row length requirements imposed by many mechanical FAX machines. We cannot, however,
       afford to bulk up the data, by sending superfluous bytes. The resulting loop delay could
       provoke an erroneous timeout of the acknowledgement signal. */

    i = 0;
    switch (s->input_phase)
    {
    case TCF_AT_INITIAL_ALL_ONES:
        /* Dump initial 0xFF bytes. We will add enough of our own to makes things flow
           smoothly. If we don't strip these off, we might end up delaying the start of
           forwarding by a substantial amount, as we could end up with a large block of 0xFF
           bytes before the real data begins. This is especially true with PC FAX
           systems. This test is very simplistic, as bit errors could confuse it. */
        for (  ;  i < len;  i++)
        {
            if (buf[i] != 0xFF)
            {
                s->input_phase = TCF_AT_ALL_ZEROS;
                s->flow_control_fill_octet = 0x00;
                break;
            }
        }
        /* Fall through */
    case TCF_AT_ALL_ZEROS:
        for (  ;  i < len;  i++)
        {
            s->data[s->in_ptr] = buf[i];
            s->latest_eol_ptr = s->in_ptr;
            /* TODO: We can't buffer overflow, since we wrap around. However, the tail could
                     overwrite itself if things fall badly behind. */
            s->in_ptr = (s->in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
            s->in_octets++;
        }
        break;
    case IMAGE_WAITING_FOR_FIRST_EOL:
        /* Dump anything up to the first EOL. Let the output side stuff with 0xFF bytes while waiting
           for that first EOL. What occurs before the first EOL is expected to be a period of all ones
           and then a period of all zeros. We really don't care what junk might be there. By definition,
           the image only starts at the first EOL. */
        for (  ;  i < len;  i++)
        {
            if (buf[i])
            {
                /* There might be an EOL here. Look for at least 11 zeros, followed by a one, split
                   between two octets. Between those two octets we can insert numerous zero octets
                   as a means of flow control. Note that we stuff in blocks of 8 bits, and not at
                   the minimal level. */
                /* Or'ing with 0x800 here is to avoid zero words looking like they have -1
                   trailing zeros */
                upper = bottom_bit(s->bit_stream | 0x800);
                lower = top_bit(buf[i]);
                if ((upper - lower) > (11 - 8))
                {
                    /* This is an EOL - our first row is beginning. */
                    s->input_phase = IMAGE_IN_PROGRESS;
                    /* Start a new row */
                    s->row_bits = lower - 8;
                    s->latest_eol_ptr = s->in_ptr;
                    s->flow_control_fill_octet = 0x00;

                    /* If we push out two bytes of zero, and our latest non-zero byte
                       we should definitely form a proper EOL to begin things, with a
                       few harmless extra zero bits at the front. */
                    s->data[s->in_ptr] = 0x00;
                    s->in_ptr = (s->in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
                    s->data[s->in_ptr] = 0x00;
                    s->in_ptr = (s->in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
                    s->data[s->in_ptr] = buf[i];
                    s->in_ptr = (s->in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
                    s->in_octets += 3;
                    s->bit_stream = (s->bit_stream << 8) | buf[i];
                    i++;
                    break;
                }
            }
            s->bit_stream = (s->bit_stream << 8) | buf[i];
        }
        if (i >= len)
            break;
        /* Fall through */
    case IMAGE_IN_PROGRESS:
        /* Now we have seen an EOL, we can stuff with zeros just in front of that EOL, or any
           subsequent EOL that does not immediately follow a previous EOL (i.e. a candidate RTC).
           We need to track our way through the image data, allowing the output side to only send
           up to the last EOL. This prevents the possibility of underflow mid-row, where we cannot
           safely stuff anything in the bit stream. */
        for (  ;  i < len;  i++)
        {
            if (buf[i])
            {
                /* There might be an EOL here. Look for at least 11 zeros, followed by a one, split
                   between two octets. Between those two octets we can insert numerous zero octets
                   as a means of flow control. Note that we stuff in blocks of 8 bits, and not at
                   the minimal level. */
                /* Or'ing with 0x800 here is to avoid zero words looking like they have -1
                   trailing zeros */
                upper = bottom_bit(s->bit_stream | 0x800);
                lower = top_bit(buf[i]);
                if ((upper - lower) > (11 - 8))
                {
                    /* This is an EOL. */
                    s->row_bits += (8 - lower);
                    /* Make sure we don't stretch back to back EOLs, as that could spoil the RTC.
                       This is a slightly crude check, as we don't know if we are processing a T.4 1D
                       or T.4 2D image. Accepting 12 or 12 bits apart as meaning back to back is fine,
                       as no 1D image row could be 1 bit long. */
                    if (s->row_bits < 12  ||  s->row_bits > 13)
                    {
                        /* If the row is too short, extend it in chunks of a whole byte. */
                        /* TODO: extend by the precise amount we should, instead of this
                                 rough approach. */
                        while (s->row_bits < s->min_bits_per_row)
                        {
                            s->min_row_bits_fill_octets++;
                            s->data[s->in_ptr] = 0;
                            s->row_bits += 8;
                            /* TODO: We can't buffer overflow, since we wrap around. However,
                                     the tail could overwrite itself if things fall badly behind. */
                            s->in_ptr = (s->in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
                        }
                        /* This is now the limit for the output side, before it starts
                           stuffing. */
                        s->latest_eol_ptr = s->in_ptr;
                    }
                    /* Start a new row */
                    s->row_bits = lower - 8;
                    s->in_rows++;
                }
            }
            s->bit_stream = (s->bit_stream << 8) | buf[i];
            s->data[s->in_ptr] = buf[i];
            s->row_bits += 8;
            /* TODO: We can't buffer overflow, since we wrap around. However, the tail could overwrite
                     itself if things fall badly behind. */
            s->in_ptr = (s->in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
            s->in_octets++;
        }
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_non_ecm_buffer_report_input_status(t38_non_ecm_buffer_state_t *s, logging_state_t *logging)
{
    if (s->in_octets  ||  s->min_row_bits_fill_octets)
    {
        span_log(logging,
                 SPAN_LOG_FLOW,
                 "%d+%d incoming non-ECM octets, %d rows.\n",
                 s->in_octets,
                 s->min_row_bits_fill_octets,
                 s->in_rows);
        s->in_octets = 0;
        s->in_rows = 0;
        s->min_row_bits_fill_octets = 0;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_non_ecm_buffer_report_output_status(t38_non_ecm_buffer_state_t *s, logging_state_t *logging)
{
    if (s->out_octets  ||  s->flow_control_fill_octets)
    {
        span_log(logging,
                 SPAN_LOG_FLOW,
                 "%d+%d outgoing non-ECM octets, %d rows.\n",
                 s->out_octets - s->flow_control_fill_octets,
                 s->flow_control_fill_octets,
                 s->out_rows);
        s->out_octets = 0;
        s->out_rows = 0;
        s->flow_control_fill_octets = 0;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_non_ecm_buffer_set_mode(t38_non_ecm_buffer_state_t *s, bool image_mode, int min_bits_per_row)
{
    s->image_data_mode = image_mode;
    s->min_bits_per_row = min_bits_per_row;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t38_non_ecm_buffer_state_t *) t38_non_ecm_buffer_init(t38_non_ecm_buffer_state_t *s, bool image_mode, int min_bits_per_row)
{
    if (s == NULL)
    {
        if ((s = (t38_non_ecm_buffer_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->image_data_mode = image_mode;
    s->min_bits_per_row = min_bits_per_row;
    restart_buffer(s);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_non_ecm_buffer_release(t38_non_ecm_buffer_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_non_ecm_buffer_free(t38_non_ecm_buffer_state_t *s)
{
    if (s)
        span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
