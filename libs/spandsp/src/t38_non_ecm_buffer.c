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
 *
 * $Id: t38_non_ecm_buffer.c,v 1.8 2009/02/10 13:06:46 steveu Exp $
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
#include "floating_fudge.h"
#include <assert.h>
#if defined(LOG_FAX_AUDIO)
#include <unistd.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/dc_restore.h"
#include "spandsp/bit_operations.h"
#include "spandsp/async.h"
#include "spandsp/t38_non_ecm_buffer.h"

#include "spandsp/private/t38_non_ecm_buffer.h"

static void restart_buffer(t38_non_ecm_buffer_state_t *s)
{
    /* This should be called when draining the buffer is complete, which should
       occur before any fresh data can possibly arrive to begin refilling it. */
    s->octet = 0xFF;
    s->flow_control_fill_octet = 0xFF;
    s->at_initial_all_ones = TRUE;
    s->bit_stream = 0xFFFF;
    s->out_ptr = 0;
    s->in_ptr = 0;
    s->latest_eol_ptr = 0;
    s->data_finished = FALSE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) t38_non_ecm_buffer_get_bit(void *user_data)
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
    s->data_finished = TRUE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_non_ecm_buffer_inject(t38_non_ecm_buffer_state_t *s, const uint8_t *buf, int len)
{
    int i;
    int upper;
    int lower;

    i = 0;
    if (s->at_initial_all_ones)
    {
        /* Dump initial 0xFF bytes. We will add enough of our own to makes things flow
           smoothly. If we don't strip these off, we might end up delaying the start of
           forwarding by a large amount, as we could end up with a large block of 0xFF
           bytes before the real data begins. This is especially true with PC FAX
           systems. This test is very simplistic, as a single bit error will throw it
           off course. */
        for (  ;  i < len;  i++)
        {
            if (buf[i] != 0xFF)
            {
                s->at_initial_all_ones = FALSE;
                break;
            }
        }
    }
    if (s->image_data_mode)
    {
        /* This is image data */
        for (  ;  i < len;  i++)
        {
            /* Check for EOLs, because at an EOL we can pause and pump out zeros while
               waiting for more incoming data. */
            if (buf[i])
            {
                /* There might be an EOL here. Look for at least 11 zeros, followed by a one, split
                   between two octets. Between those two octets we can insert numerous zero octets
                   as a means of flow control. Note that we stuff in blocks of 8 bits, and not at
                   the minimal level. */
                /* Or'ing with 0x800 here is simply to avoid zero words looking like they have -1
                   trailing zeros */
                upper = bottom_bit(s->bit_stream | 0x800);
                lower = top_bit(buf[i]);
                if (upper - lower > 3)
                {
                    s->row_bits += (8 - lower);
                    /* If the row is too short, extend it in chunks of a whole byte. */
                    /* TODO: extend by the precise amount we should, instead of this
                             rough approach. */
                    while (s->row_bits < s->min_row_bits)
                    {
                        s->min_row_bits_fill_octets++;
                        s->data[s->in_ptr] = 0;
                        s->row_bits += 8;
                        /* TODO: We can't buffer overflow, since we wrap around. However, the tail could
                                 overwrite itself if things fall badly behind. */
                        s->in_ptr = (s->in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
                    }
                    /* Start a new row */
                    s->row_bits = lower - 8;
                    s->in_rows++;
                    s->latest_eol_ptr = s->in_ptr;
                    s->flow_control_fill_octet = 0x00;
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
    }
    else
    {
        /* This is TCF data */
        for (  ;  i < len;  i++)
        {
            /* Check for zero bytes, as we can pause and pump out zeros while waiting
               for more incoming data. Of course, the entire TCF data should be zero,
               but it might not be, due to bit errors, or something weird happening. */
            if (buf[i] == 0x00)
            {
                s->latest_eol_ptr = s->in_ptr;
                s->flow_control_fill_octet = 0x00;
            }
            s->data[s->in_ptr] = buf[i];
            /* TODO: We can't buffer overflow, since we wrap around. However, the tail could
                     overwrite itself if things fall badly behind. */
            s->in_ptr = (s->in_ptr + 1) & (T38_NON_ECM_TX_BUF_LEN - 1);
            s->in_octets++;
        }
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

SPAN_DECLARE(void) t38_non_ecm_buffer_set_mode(t38_non_ecm_buffer_state_t *s, int mode, int min_row_bits)
{
    s->image_data_mode = mode;
    s->min_row_bits = min_row_bits;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t38_non_ecm_buffer_state_t *) t38_non_ecm_buffer_init(t38_non_ecm_buffer_state_t *s, int mode, int min_row_bits)
{
    if (s == NULL)
    {
        if ((s = (t38_non_ecm_buffer_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->octet = 0xFF;
    s->flow_control_fill_octet = 0xFF;
    s->at_initial_all_ones = TRUE;
    s->bit_stream = 0xFFFF;
    s->image_data_mode = mode;
    s->min_row_bits = min_row_bits;
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
        free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
