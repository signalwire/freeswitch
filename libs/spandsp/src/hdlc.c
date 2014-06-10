/*
 * SpanDSP - a series of DSP components for telephony
 *
 * hdlc.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
#include <string.h>
#include <stdio.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/async.h"
#include "spandsp/crc.h"
#include "spandsp/bit_operations.h"
#include "spandsp/hdlc.h"
#include "spandsp/private/hdlc.h"

static void report_status_change(hdlc_rx_state_t *s, int status)
{
    if (s->status_handler)
        s->status_handler(s->status_user_data, status);
    else if (s->frame_handler)
        s->frame_handler(s->frame_user_data, NULL, status, true);
}
/*- End of function --------------------------------------------------------*/

static void rx_special_condition(hdlc_rx_state_t *s, int status)
{
    /* Special conditions */
    switch (status)
    {
    case SIG_STATUS_CARRIER_UP:
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* Reset the HDLC receiver. */
        s->raw_bit_stream = 0;
        s->len = 0;
        s->num_bits = 0;
        s->flags_seen = 0;
        s->framing_ok_announced = false;
        /* Fall through */
    case SIG_STATUS_TRAINING_IN_PROGRESS:
    case SIG_STATUS_TRAINING_FAILED:
    case SIG_STATUS_CARRIER_DOWN:
    case SIG_STATUS_END_OF_DATA:
        report_status_change(s, status);
        break;
    default:
        //printf("Eh!\n");
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void octet_set_and_count(hdlc_rx_state_t *s)
{
    if (s->octet_count_report_interval == 0)
        return;

    /* If we are not in octet counting mode, we start it.
       If we are in octet counting mode, we update it. */
    if (s->octet_counting_mode)
    {
        if (--s->octet_count <= 0)
        {
            s->octet_count = s->octet_count_report_interval;
            report_status_change(s, SIG_STATUS_OCTET_REPORT);
        }
    }
    else
    {
        s->octet_counting_mode = true;
        s->octet_count = s->octet_count_report_interval;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void octet_count(hdlc_rx_state_t *s)
{
    if (s->octet_count_report_interval == 0)
        return;

    /* If we are not in octet counting mode, we start it.
       If we are in octet counting mode, we update it. */
    if (s->octet_counting_mode)
    {
        if (--s->octet_count <= 0)
        {
            s->octet_count = s->octet_count_report_interval;
            report_status_change(s, SIG_STATUS_OCTET_REPORT);
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void rx_flag_or_abort(hdlc_rx_state_t *s)
{
    if ((s->raw_bit_stream & 0x8000))
    {
        /* Hit HDLC abort */
        s->rx_aborts++;
        report_status_change(s, SIG_STATUS_ABORT);
        /* If we have not yet seen enough flags, restart the count. If we
           are beyond that point, just back off one step, so we need to see
           another flag before proceeding to collect frame octets. */
        if (s->flags_seen < s->framing_ok_threshold - 1)
            s->flags_seen = 0;
        else
            s->flags_seen = s->framing_ok_threshold - 1;
        /* An abort starts octet counting */
        octet_set_and_count(s);
    }
    else
    {
        /* Hit HDLC flag */
        /* A flag clears octet counting */
        s->octet_counting_mode = false;
        if (s->flags_seen >= s->framing_ok_threshold)
        {
            /* We may have a frame, or we may have back to back flags */
            if (s->len)
            {
                if (s->num_bits == 7  &&  s->len >= (size_t) s->crc_bytes  &&  s->len <= s->max_frame_len)
                {
                    if ((s->crc_bytes == 2  &&  crc_itu16_check(s->buffer, s->len))
                        ||
                        (s->crc_bytes != 2  &&  crc_itu32_check(s->buffer, s->len)))
                    {
                        s->rx_frames++;
                        s->rx_bytes += s->len - s->crc_bytes;
                        s->len -= s->crc_bytes;
                        if (s->frame_handler)
                            s->frame_handler(s->frame_user_data, s->buffer, s->len, true);
                    }
                    else
                    {
                        s->rx_crc_errors++;
                        if (s->report_bad_frames)
                        {
                            s->len -= s->crc_bytes;
                            if (s->frame_handler)
                                s->frame_handler(s->frame_user_data, s->buffer, s->len, false);
                        }
                    }
                }
                else
                {
                    /* Frame too short or too long, or the flag is misaligned with its octets. */
                    if (s->report_bad_frames)
                    {
                        /* Don't let the length go below zero, or it will be confused
                           with one of the special conditions. */
                        if (s->len >= (size_t) s->crc_bytes)
                            s->len -= s->crc_bytes;
                        else
                            s->len = 0;
                        if (s->frame_handler)
                            s->frame_handler(s->frame_user_data, s->buffer, s->len, false);
                    }
                    s->rx_length_errors++;
                }
            }
        }
        else
        {
            /* Check the flags are back-to-back when testing for valid preamble. This
               greatly reduces the chances of false preamble detection, and anything
               which doesn't send them back-to-back is badly broken. */
            if (s->num_bits != 7)
            {
                /* Don't set the flags seen indicator back to zero too aggressively.
                   We want to pick up with the minimum of discarded data when there
                   is a bit error in the stream, and a bit error could emulate a
                   misaligned flag. */
                if (s->flags_seen < s->framing_ok_threshold - 1)
                    s->flags_seen = 0;
                else
                    s->flags_seen = s->framing_ok_threshold - 1;
            }
            if (++s->flags_seen >= s->framing_ok_threshold  &&  !s->framing_ok_announced)
            {
                report_status_change(s, SIG_STATUS_FRAMING_OK);
                s->framing_ok_announced = true;
            }
        }
    }
    s->len = 0;
    s->num_bits = 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void hdlc_rx_put_bit_core(hdlc_rx_state_t *s)
{
    if ((s->raw_bit_stream & 0x3F00) == 0x3E00)
    {
        /* Its time to either skip a bit, for stuffing, or process a
           flag or abort */
        if ((s->raw_bit_stream & 0x4000))
            rx_flag_or_abort(s);
        return;
    }
    s->num_bits++;
    if (s->flags_seen < s->framing_ok_threshold)
    {
        if ((s->num_bits & 0x7) == 0)
            octet_count(s);
        return;
    }
    s->byte_in_progress = (s->byte_in_progress | (s->raw_bit_stream & 0x100)) >> 1;
    if (s->num_bits == 8)
    {
        /* Ensure we do not accept an overlength frame, and especially that
           we do not overflow our buffer */
        if (s->len < s->max_frame_len)
        {
            s->buffer[s->len++] = (uint8_t) s->byte_in_progress;
        }
        else
        {
            /* This is too long. Abandon the frame, and wait for the next
               flag octet. */
            s->len = sizeof(s->buffer) + 1;
            s->flags_seen = s->framing_ok_threshold - 1;
            octet_set_and_count(s);
        }
        s->num_bits = 0;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(void) hdlc_rx_put_bit(hdlc_rx_state_t *s, int new_bit)
{
    if (new_bit < 0)
    {
        rx_special_condition(s, new_bit);
        return;
    }
    s->raw_bit_stream = (s->raw_bit_stream << 1) | ((new_bit << 8) & 0x100);
    hdlc_rx_put_bit_core(s);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(void) hdlc_rx_put_byte(hdlc_rx_state_t *s, int new_byte)
{
    int i;

    if (new_byte < 0)
    {
        rx_special_condition(s, new_byte);
        return;
    }
    s->raw_bit_stream |= new_byte;
    for (i = 0;  i < 8;  i++)
    {
        s->raw_bit_stream <<= 1;
        hdlc_rx_put_bit_core(s);
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(void) hdlc_rx_put(hdlc_rx_state_t *s, const uint8_t buf[], int len)
{
    int i;

    for (i = 0;  i < len;  i++)
        hdlc_rx_put_byte(s, buf[i]);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) hdlc_rx_set_max_frame_len(hdlc_rx_state_t *s, size_t max_len)
{
    max_len += s->crc_bytes;
    s->max_frame_len = (max_len <= sizeof(s->buffer))  ?  max_len  :  sizeof(s->buffer);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) hdlc_rx_set_octet_counting_report_interval(hdlc_rx_state_t *s, int interval)
{
    s->octet_count_report_interval = interval;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_rx_restart(hdlc_rx_state_t *s)
{
    s->framing_ok_announced = false;
    s->flags_seen = 0;
    s->raw_bit_stream = 0;
    s->byte_in_progress = 0;
    s->num_bits = 0;
    s->octet_counting_mode = false;
    s->octet_count = 0;
    s->len = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(hdlc_rx_state_t *) hdlc_rx_init(hdlc_rx_state_t *s,
                                             bool crc32,
                                             bool report_bad_frames,
                                             int framing_ok_threshold,
                                             hdlc_frame_handler_t handler,
                                             void *user_data)
{
    if (s == NULL)
    {
        if ((s = (hdlc_rx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->frame_handler = handler;
    s->frame_user_data = user_data;
    s->crc_bytes = (crc32)  ?  4  :  2;
    s->report_bad_frames = report_bad_frames;
    s->framing_ok_threshold = (framing_ok_threshold < 1)  ?  1  :  framing_ok_threshold;
    s->max_frame_len = sizeof(s->buffer);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) hdlc_rx_set_frame_handler(hdlc_rx_state_t *s, hdlc_frame_handler_t handler, void *user_data)
{
    s->frame_handler = handler;
    s->frame_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) hdlc_rx_set_status_handler(hdlc_rx_state_t *s, modem_status_func_t handler, void *user_data)
{
    s->status_handler = handler;
    s->status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_rx_release(hdlc_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_rx_free(hdlc_rx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_rx_get_stats(hdlc_rx_state_t *s,
                                    hdlc_rx_stats_t *t)
{
    t->bytes = s->rx_bytes;
    t->good_frames = s->rx_frames;
    t->crc_errors = s->rx_crc_errors;
    t->length_errors = s->rx_length_errors;
    t->aborts = s->rx_aborts;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_tx_frame(hdlc_tx_state_t *s, const uint8_t *frame, size_t len)
{
    if (len == 0)
    {
        s->tx_end = true;
        return 0;
    }
    if (s->len + len > s->max_frame_len)
        return -1;
    if (s->progressive)
    {
        /* Only lock out if we are in the CRC section. */
        if (s->pos >= HDLC_MAXFRAME_LEN)
            return -1;
    }
    else
    {
        /* Lock out if there is anything in the buffer. */
        if (s->len)
            return -1;
    }
    memcpy(&s->buffer[s->len], frame, len);
    if (s->crc_bytes == 2)
        s->crc = crc_itu16_calc(frame, len, (uint16_t) s->crc);
    else
        s->crc = crc_itu32_calc(frame, len, s->crc);
    if (s->progressive)
        s->len += len;
    else
        s->len = len;
    s->tx_end = false;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_tx_flags(hdlc_tx_state_t *s, int len)
{
    /* Some HDLC applications require the ability to force a period of HDLC
       flag words. */
    if (s->pos)
        return -1;
    if (len < 0)
        s->flag_octets += -len;
    else
        s->flag_octets = len;
    s->report_flag_underflow = true;
    s->tx_end = false;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_tx_abort(hdlc_tx_state_t *s)
{
    /* TODO: This is a really crude way of just fudging an abort out for simple
             test purposes. */
    s->flag_octets++;
    s->abort_octets++;
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_tx_corrupt_frame(hdlc_tx_state_t *s)
{
    if (s->len <= 0)
        return -1;
    s->crc ^= 0xFFFF;
    s->buffer[HDLC_MAXFRAME_LEN] ^= 0xFF;
    s->buffer[HDLC_MAXFRAME_LEN + 1] ^= 0xFF;
    s->buffer[HDLC_MAXFRAME_LEN + 2] ^= 0xFF;
    s->buffer[HDLC_MAXFRAME_LEN + 3] ^= 0xFF;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) hdlc_tx_get_byte(hdlc_tx_state_t *s)
{
    int i;
    int byte_in_progress;
    int txbyte;

    if (s->flag_octets > 0)
    {
        /* We are in a timed flag section (preamble, inter frame gap, etc.) */
        if (--s->flag_octets <= 0  &&  s->report_flag_underflow)
        {
            s->report_flag_underflow = false;
            if (s->len == 0)
            {
                /* The timed flags have finished, there is nothing else queued to go,
                   and we have been told to report this underflow. */
                if (s->underflow_handler)
                    s->underflow_handler(s->user_data);
            }
        }
        if (s->abort_octets)
        {
            s->abort_octets = 0;
            return 0x7F;
        }
        return s->idle_octet;
    }
    if (s->len)
    {
        if (s->num_bits >= 8)
        {
            s->num_bits -= 8;
            return (s->octets_in_progress >> s->num_bits) & 0xFF;
        }
        if (s->pos >= s->len)
        {
            if (s->pos == s->len)
            {
                s->crc ^= 0xFFFFFFFF;
                s->buffer[HDLC_MAXFRAME_LEN] = (uint8_t) s->crc;
                s->buffer[HDLC_MAXFRAME_LEN + 1] = (uint8_t) (s->crc >> 8);
                if (s->crc_bytes == 4)
                {
                    s->buffer[HDLC_MAXFRAME_LEN + 2] = (uint8_t) (s->crc >> 16);
                    s->buffer[HDLC_MAXFRAME_LEN + 3] = (uint8_t) (s->crc >> 24);
                }
                s->pos = HDLC_MAXFRAME_LEN;
            }
            else if (s->pos == (size_t) (HDLC_MAXFRAME_LEN + s->crc_bytes))
            {
                /* Finish off the current byte with some flag bits. If we are at the
                   start of a byte we need a at least one whole byte of flag to ensure
                   we cannot end up with back to back frames, and no flag octet at all */
                txbyte = (uint8_t) ((s->octets_in_progress << (8 - s->num_bits)) | (0x7E >> s->num_bits));
                /* Create a rotated octet of flag for idling... */
                s->idle_octet = (0x7E7E >> s->num_bits) & 0xFF;
                /* ...and the partial flag octet needed to start off the next message. */
                s->octets_in_progress = s->idle_octet >> (8 - s->num_bits);
                s->flag_octets = s->inter_frame_flags - 1;
                s->len = 0;
                s->pos = 0;
                if (s->crc_bytes == 2)
                    s->crc = 0xFFFF;
                else
                    s->crc = 0xFFFFFFFF;
                /* Report the underflow now. If there are timed flags still in progress, loading the
                   next frame right now will be harmless. */
                s->report_flag_underflow = false;
                if (s->underflow_handler)
                    s->underflow_handler(s->user_data);
                /* Make sure we finish off with at least one flag octet, if the underflow report did not result
                   in a new frame being sent. */
                if (s->len == 0  &&  s->flag_octets < 2)
                    s->flag_octets = 2;
                return txbyte;
            }
        }
        byte_in_progress = s->buffer[s->pos++];
        i = bottom_bit(byte_in_progress | 0x100);
        s->octets_in_progress <<= i;
        byte_in_progress >>= i;
        for (  ;  i < 8;  i++)
        {
            s->octets_in_progress = (s->octets_in_progress << 1) | (byte_in_progress & 0x01);
            byte_in_progress >>= 1;
            if ((s->octets_in_progress & 0x1F) == 0x1F)
            {
                /* There are 5 ones - stuff */
                s->octets_in_progress <<= 1;
                s->num_bits++;
            }
        }
        /* An input byte will generate between 8 and 10 output bits */
        return (s->octets_in_progress >> s->num_bits) & 0xFF;
    }
    /* Untimed idling on flags */
    if (s->tx_end)
    {
        s->tx_end = false;
        return SIG_STATUS_END_OF_DATA;
    }
    return s->idle_octet;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) hdlc_tx_get_bit(hdlc_tx_state_t *s)
{
    int txbit;

    if (s->bits == 0)
    {
        if ((s->byte = hdlc_tx_get_byte(s)) < 0)
            return s->byte;
        s->bits = 8;
    }
    s->bits--;
    txbit = (s->byte >> s->bits) & 0x01;
    return txbit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) hdlc_tx_get(hdlc_tx_state_t *s, uint8_t buf[], size_t max_len)
{
    size_t i;
    int x;

    for (i = 0;  i < max_len;  i++)
    {
        if ((x = hdlc_tx_get_byte(s)) == SIG_STATUS_END_OF_DATA)
            return i;
        buf[i] = (uint8_t) x;
    }
    return (int) i;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) hdlc_tx_set_max_frame_len(hdlc_tx_state_t *s, size_t max_len)
{
    s->max_frame_len = (max_len <= HDLC_MAXFRAME_LEN)  ?  max_len  :  HDLC_MAXFRAME_LEN;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_tx_restart(hdlc_tx_state_t *s)
{
    s->octets_in_progress = 0;
    s->num_bits = 0;
    s->idle_octet = 0x7E;
    s->flag_octets = 0;
    s->abort_octets = 0;
    s->report_flag_underflow = false;
    s->len = 0;
    s->pos = 0;
    if (s->crc_bytes == 2)
        s->crc = 0xFFFF;
    else
        s->crc = 0xFFFFFFFF;
    s->byte = 0;
    s->bits = 0;
    s->tx_end = false;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(hdlc_tx_state_t *) hdlc_tx_init(hdlc_tx_state_t *s,
                                             bool crc32,
                                             int inter_frame_flags,
                                             bool progressive,
                                             hdlc_underflow_handler_t handler,
                                             void *user_data)
{
    if (s == NULL)
    {
        if ((s = (hdlc_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->underflow_handler = handler;
    s->user_data = user_data;
    s->inter_frame_flags = (inter_frame_flags < 1)  ?  1  :  inter_frame_flags;
    if (crc32)
    {
        s->crc_bytes = 4;
        s->crc = 0xFFFFFFFF;
    }
    else
    {
        s->crc_bytes = 2;
        s->crc = 0xFFFF;
    }
    s->idle_octet = 0x7E;
    s->progressive = progressive;
    s->max_frame_len = HDLC_MAXFRAME_LEN;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_tx_release(hdlc_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) hdlc_tx_free(hdlc_tx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
