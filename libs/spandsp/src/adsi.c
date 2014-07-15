/*
 * SpanDSP - a series of DSP components for telephony
 *
 * adsi.c - Analogue display service interfaces of various types, including
 *          ADSI, TDD and most caller ID formats.
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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
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
#include "spandsp/fast_convert.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"
#include "spandsp/async.h"
#include "spandsp/crc.h"
#include "spandsp/fsk.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/dtmf.h"
#include "spandsp/adsi.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/queue.h"
#include "spandsp/private/tone_generate.h"
#include "spandsp/private/async.h"
#include "spandsp/private/power_meter.h"
#include "spandsp/private/fsk.h"
#include "spandsp/private/dtmf.h"
#include "spandsp/private/adsi.h"

/*! The baudot code to shift from alpha to digits and symbols */
#define BAUDOT_FIGURE_SHIFT     0x1B
/*! The baudot code to shift from digits and symbols to alpha */
#define BAUDOT_LETTER_SHIFT     0x1F

enum
{
    SOH = 0x01,
    STX = 0x02,
    ETX = 0x03,
    DLE = 0x10,
    SUB = 0x1A
};

static uint16_t adsi_encode_baudot(adsi_tx_state_t *s, uint8_t ch);
static uint8_t adsi_decode_baudot(adsi_rx_state_t *s, uint8_t ch);

static int adsi_tx_get_bit(void *user_data)
{
    int bit;
    adsi_tx_state_t *s;

    s = (adsi_tx_state_t *) user_data;
    /* This is similar to the async. handling code in fsk.c, but a few special
       things are needed in the preamble, and postamble of an ADSI message. */
    if (s->bit_no < s->preamble_len)
    {
        /* Alternating bit preamble */
        bit = s->bit_no & 1;
        s->bit_no++;
    }
    else if (s->bit_no < s->preamble_len + s->preamble_ones_len)
    {
        /* All 1s for receiver conditioning */
        /* NB: The receiver is an async one. It needs a rest after the
               alternating 1/0 sequence so it can reliably pick up on
               the next start bit, and sync to the byte stream. */
        /* The length of this period varies with the circumstance */
        bit = 1;
        s->bit_no++;
    }
    else if (s->bit_no <= s->preamble_len + s->preamble_ones_len)
    {
        /* Push out the 8 bit async. chars, with an appropriate number of stop bits */
        if (s->bit_pos == 0)
        {
            /* Start bit */
            bit = 0;
            s->bit_pos++;
        }
        else if (s->bit_pos < 1 + 8)
        {
            bit = (s->msg[s->byte_no] >> (s->bit_pos - 1)) & 1;
            s->bit_pos++;
        }
        else if (s->bit_pos < 1 + 8 + s->stop_bits - 1)
        {
            /* Stop bit */
            bit = 1;
            s->bit_pos++;
        }
        else
        {
            /* Stop bit */
            bit = 1;
            s->bit_pos = 0;
            if (++s->byte_no >= s->msg_len)
                s->bit_no++;
        }
    }
    else if (s->bit_no <= s->preamble_len + s->preamble_ones_len + s->postamble_ones_len)
    {
        /* Extra stop bits beyond the last character, to meet the specs., and ensure
           all bits are out of the DSP before we shut off the FSK modem. */
        bit = 1;
        s->bit_no++;
    }
    else
    {
        bit = SIG_STATUS_END_OF_DATA;
        if (s->tx_signal_on)
        {
            /* The FSK should now be switched off. */
            s->tx_signal_on = false;
            s->msg_len = 0;
        }
    }
    //printf("Tx bit %d\n", bit);
    return bit;
}
/*- End of function --------------------------------------------------------*/

static int adsi_tdd_get_async_byte(void *user_data)
{
    adsi_tx_state_t *s;

    s = (adsi_tx_state_t *) user_data;
    if (s->byte_no < s->msg_len)
        return s->msg[s->byte_no++];
    if (s->tx_signal_on)
    {
        /* The FSK should now be switched off. */
        s->tx_signal_on = false;
        s->msg_len = 0;
    }
    return 0x1F;
}
/*- End of function --------------------------------------------------------*/

static void adsi_rx_put_bit(void *user_data, int bit)
{
    adsi_rx_state_t *s;
    int i;
    int sum;

    s = (adsi_rx_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions */
        span_log(&s->logging, SPAN_LOG_FLOW, "ADSI signal status is %s (%d)\n", signal_status_to_str(bit), bit);
        switch (bit)
        {
        case SIG_STATUS_CARRIER_UP:
            s->consecutive_ones = 0;
            s->bit_pos = 0;
            s->in_progress = 0;
            s->msg_len = 0;
            break;
        case SIG_STATUS_CARRIER_DOWN:
            break;
        default:
            span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected special put bit value - %d!\n", bit);
            break;
        }
        return;
    }
    bit &= 1;
    if (s->bit_pos == 0)
    {
        if (bit == 0)
        {
            /* Start bit */
            s->bit_pos++;
            if (s->consecutive_ones > 10)
            {
                /* This is a line idle condition, which means we should
                   restart message acquisition */
                s->msg_len = 0;
            }
            s->consecutive_ones = 0;
        }
        else
        {
            s->consecutive_ones++;
        }
    }
    else if (s->bit_pos <= 8)
    {
        s->in_progress >>= 1;
        if (bit)
            s->in_progress |= 0x80;
        s->bit_pos++;
    }
    else
    {
        /* Stop bit */
        if (bit)
        {
            if (s->msg_len < 256)
            {
                if (s->standard == ADSI_STANDARD_JCLIP)
                {
                    if (s->msg_len == 0)
                    {
                        /* A message should start DLE SOH, but let's just check
                           we are starting with a DLE for now */
                        if (s->in_progress == (0x80 | DLE))
                            s->msg[s->msg_len++] = (uint8_t) s->in_progress;
                    }
                    else
                    {
                        s->msg[s->msg_len++] = (uint8_t) s->in_progress;
                    }
                    if (s->msg_len >= 11  &&  s->msg_len == ((s->msg[6] & 0x7F) + 11))
                    {
                        /* Test the CRC-16 */
                        if (crc_itu16_calc(s->msg + 2, s->msg_len - 2, 0) == 0)
                        {
                            /* Strip off the parity bits. It doesn't seem
                               worthwhile actually checking the parity if a
                               CRC check has succeeded. */
                            for (i = 0;  i < s->msg_len - 2;  i++)
                                s->msg[i] &= 0x7F;
                            /* Put everything, except the CRC octets */
                            s->put_msg(s->user_data, s->msg, s->msg_len - 2);
                        }
                        else
                        {
                            span_log(&s->logging, SPAN_LOG_WARNING, "CRC failed\n");
                        }
                        s->msg_len = 0;
                    }
                }
                else
                {
                    s->msg[s->msg_len++] = (uint8_t) s->in_progress;
                    if (s->msg_len >= 3  &&  s->msg_len == (s->msg[1] + 3))
                    {
                        /* Test the checksum */
                        sum = 0;
                        for (i = 0;  i < s->msg_len - 1;  i++)
                            sum += s->msg[i];
                        if ((-sum & 0xFF) == s->msg[i])
                            s->put_msg(s->user_data, s->msg, s->msg_len - 1);
                        else
                            span_log(&s->logging, SPAN_LOG_WARNING, "Sumcheck failed\n");
                        s->msg_len = 0;
                    }
                }
            }
        }
        else
        {
            s->framing_errors++;
        }
        s->bit_pos = 0;
        s->in_progress = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static void adsi_tdd_put_async_byte(void *user_data, int byte)
{
    adsi_rx_state_t *s;
    uint8_t octet;

    s = (adsi_rx_state_t *) user_data;
    //printf("Rx bit %x\n", bit);
    if (byte < 0)
    {
        /* Special conditions */
        span_log(&s->logging, SPAN_LOG_FLOW, "ADSI signal status is %s (%d)\n", signal_status_to_str(byte), byte);
        switch (byte)
        {
        case SIG_STATUS_CARRIER_UP:
            s->consecutive_ones = 0;
            s->bit_pos = 0;
            s->in_progress = 0;
            s->msg_len = 0;
            break;
        case SIG_STATUS_CARRIER_DOWN:
            if (s->msg_len > 0)
            {
                /* Whatever we have to date constitutes the message */
                s->put_msg(s->user_data, s->msg, s->msg_len);
                s->msg_len = 0;
            }
            break;
        default:
            span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected special put byte value - %d!\n", byte);
            break;
        }
        return;
    }
    if ((octet = adsi_decode_baudot(s, (uint8_t) (byte & 0x1F))))
        s->msg[s->msg_len++] = octet;
    if (s->msg_len >= 256)
    {
        s->put_msg(s->user_data, s->msg, s->msg_len);
        s->msg_len = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static void adsi_rx_dtmf(void *user_data, const char *digits, int len)
{
    adsi_rx_state_t *s;

    s = (adsi_rx_state_t *) user_data;
    if (s->msg_len == 0)
    {
        /* Message starting. Start a 10s timeout, to make things more noise
           tolerant for a detector running continuously when on hook. */
        s->in_progress = 80000;
    }
    /* It seems all the DTMF variants are a string of digits and letters,
       terminated by a "#", or a "C". It appears these are unambiguous, and
       non-conflicting. */
    for (  ;  len  &&  s->msg_len < 256;  len--)
    {
        s->msg[s->msg_len++] = *digits;
        if (*digits == '#'  ||  *digits == 'C')
        {
            s->put_msg(s->user_data, s->msg, s->msg_len);
            s->msg_len = 0;
        }
        digits++;
    }
}
/*- End of function --------------------------------------------------------*/

static void start_tx(adsi_tx_state_t *s)
{
    switch (s->standard)
    {
    case ADSI_STANDARD_CLASS:
        fsk_tx_init(&s->fsktx, &preset_fsk_specs[FSK_BELL202], adsi_tx_get_bit, s);
        break;
    case ADSI_STANDARD_CLIP:
    case ADSI_STANDARD_ACLIP:
    case ADSI_STANDARD_JCLIP:
        fsk_tx_init(&s->fsktx, &preset_fsk_specs[FSK_V23CH1], adsi_tx_get_bit, s);
        break;
    case ADSI_STANDARD_CLIP_DTMF:
        dtmf_tx_init(&s->dtmftx, NULL, NULL);
        break;
    case ADSI_STANDARD_TDD:
        fsk_tx_init(&s->fsktx, &preset_fsk_specs[FSK_WEITBRECHT_4545], async_tx_get_bit, &s->asynctx);
        async_tx_init(&s->asynctx, 5, ASYNC_PARITY_NONE, 2, false, adsi_tdd_get_async_byte, s);
        /* Schedule an explicit shift at the start of baudot transmission */
        s->baudot_shift = 2;
        break;
    }
    s->tx_signal_on = true;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) adsi_rx(adsi_rx_state_t *s, const int16_t amp[], int len)
{
    switch (s->standard)
    {
    case ADSI_STANDARD_CLIP_DTMF:
        /* Apply a message timeout. */
        s->in_progress -= len;
        if (s->in_progress <= 0)
            s->msg_len = 0;
        dtmf_rx(&s->dtmfrx, amp, len);
        break;
    default:
        fsk_rx(&s->fskrx, amp, len);
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) adsi_rx_get_logging_state(adsi_rx_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(adsi_rx_state_t *) adsi_rx_init(adsi_rx_state_t *s,
                                             int standard,
                                             put_msg_func_t put_msg,
                                             void *user_data)
{
    if (s == NULL)
    {
        if ((s = (adsi_rx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->put_msg = put_msg;
    s->user_data = user_data;
    switch (standard)
    {
    case ADSI_STANDARD_CLASS:
        fsk_rx_init(&s->fskrx, &preset_fsk_specs[FSK_BELL202], FSK_FRAME_MODE_ASYNC, adsi_rx_put_bit, s);
        break;
    case ADSI_STANDARD_CLIP:
    case ADSI_STANDARD_ACLIP:
    case ADSI_STANDARD_JCLIP:
        fsk_rx_init(&s->fskrx, &preset_fsk_specs[FSK_V23CH1], FSK_FRAME_MODE_ASYNC, adsi_rx_put_bit, s);
        break;
    case ADSI_STANDARD_CLIP_DTMF:
        dtmf_rx_init(&s->dtmfrx, adsi_rx_dtmf, s);
        break;
    case ADSI_STANDARD_TDD:
        /* TDD uses 5 bit data, no parity and 1.5 stop bits. We scan for the first stop bit, and
           ride over the fraction. */
        fsk_rx_init(&s->fskrx, &preset_fsk_specs[FSK_WEITBRECHT_4545], FSK_FRAME_MODE_5N1_FRAMES, adsi_tdd_put_async_byte, s);
        break;
    }
    s->standard = standard;
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) adsi_rx_release(adsi_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) adsi_rx_free(adsi_rx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) adsi_tx(adsi_tx_state_t *s, int16_t amp[], int max_len)
{
    int len;
    int lenx;

    len = tone_gen(&s->alert_tone_gen, amp, max_len);
    if (s->tx_signal_on)
    {
        switch (s->standard)
        {
        case ADSI_STANDARD_CLIP_DTMF:
            if (len < max_len)
                len += dtmf_tx(&s->dtmftx, amp, max_len - len);
            break;
        default:
            if (len < max_len)
            {
                if ((lenx = fsk_tx(&s->fsktx, amp + len, max_len - len)) <= 0)
                    s->tx_signal_on = false;
                len += lenx;
            }
            break;
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) adsi_tx_send_alert_tone(adsi_tx_state_t *s)
{
    tone_gen_init(&s->alert_tone_gen, &s->alert_tone_desc);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) adsi_tx_set_preamble(adsi_tx_state_t *s,
                                        int preamble_len,
                                        int preamble_ones_len,
                                        int postamble_ones_len,
                                        int stop_bits)
{
    if (preamble_len < 0)
    {
        if (s->standard == ADSI_STANDARD_JCLIP)
            s->preamble_len = 0;
        else
            s->preamble_len = 300;
    }
    else
    {
        s->preamble_len = preamble_len;
    }
    if (preamble_ones_len < 0)
    {
        if (s->standard == ADSI_STANDARD_JCLIP)
            s->preamble_ones_len = 75;
        else
            s->preamble_ones_len = 80;
    }
    else
    {
        s->preamble_ones_len = preamble_ones_len;
    }
    if (postamble_ones_len < 0)
    {
#if 0
        if (s->standard == ADSI_STANDARD_JCLIP)
            s->postamble_ones_len = 5;
        else
#endif
            s->postamble_ones_len = 5;
    }
    else
    {
        s->postamble_ones_len = postamble_ones_len;
    }
    if (stop_bits < 0)
    {
        if (s->standard == ADSI_STANDARD_JCLIP)
            s->stop_bits = 4;
        else
            s->stop_bits = 1;
    }
    else
    {
        s->stop_bits = stop_bits;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) adsi_tx_put_message(adsi_tx_state_t *s, const uint8_t *msg, int len)
{
    int i;
    int j;
    int k;
    int byte;
    int parity;
    int sum;
    size_t ii;
    uint16_t crc_value;

    /* Don't inject a new message when a previous one is still in progress */
    if (s->msg_len > 0)
        return 0;
    if (!s->tx_signal_on)
    {
        /* We need to restart the modem */
        start_tx(s);
    }
    switch (s->standard)
    {
    case ADSI_STANDARD_CLIP_DTMF:
        if (len >= 128)
            return -1;
        len -= (int) dtmf_tx_put(&s->dtmftx, (char *) msg, len);
        break;
    case ADSI_STANDARD_JCLIP:
        if (len > 128 - 9)
            return -1;
        i = 0;
        s->msg[i++] = DLE;
        s->msg[i++] = SOH;
        s->msg[i++] = 0x07; //header
        s->msg[i++] = DLE;
        s->msg[i++] = STX;
        s->msg[i++] = msg[0];
        s->msg[i++] = (uint8_t) (len - 2);
        /* We might need to byte stuff the overall length, but the rest of the
           message should already be stuffed. */
        if (len - 2 == DLE)
            s->msg[i++] = DLE;
        memcpy(&s->msg[i], &msg[2], len - 2);
        i += len - 2;
        s->msg[i++] = DLE;
        s->msg[i++] = ETX;

        /* Set the parity bits */
        for (j = 0;  j < i;  j++)
        {
            byte = s->msg[j];
            parity = 0;
            for (k = 1;  k <= 7;  k++)
                parity ^= (byte << k);
            s->msg[j] = (s->msg[j] & 0x7F) | ((uint8_t) parity & 0x80);
        }

        crc_value = crc_itu16_calc(s->msg + 2, i - 2, 0);
        s->msg[i++] = (uint8_t) (crc_value & 0xFF);
        s->msg[i++] = (uint8_t) ((crc_value >> 8) & 0xFF);
        s->msg_len = i;
        break;
    case ADSI_STANDARD_TDD:
        if (len > 255)
            return -1;
        memcpy(s->msg, msg, len);
        s->msg_len = len;
        break;
    default:
        if (len > 255)
            return -1;
        memcpy(s->msg, msg, len);
        /* Force the length in case it is wrong */
        s->msg[1] = (uint8_t) (len - 2);
        /* Add the sumcheck */
        sum = 0;
        for (ii = 0;  ii < (size_t) len;  ii++)
            sum += s->msg[ii];
        s->msg[len] = (uint8_t) ((-sum) & 0xFF);
        s->msg_len = len + 1;
        break;
    }
    /* Prepare the bit sequencing */
    s->byte_no = 0;
    s->bit_pos = 0;
    s->bit_no = 0;
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) adsi_tx_get_logging_state(adsi_tx_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(adsi_tx_state_t *) adsi_tx_init(adsi_tx_state_t *s, int standard)
{
    if (s == NULL)
    {
        if ((s = (adsi_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    tone_gen_descriptor_init(&s->alert_tone_desc,
                             2130,
                             -13,
                             2750,
                             -13,
                             110,
                             60,
                             0,
                             0,
                             false);
    s->standard = standard;
    adsi_tx_set_preamble(s, -1, -1, -1, -1);
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    start_tx(s);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) adsi_tx_release(adsi_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) adsi_tx_free(adsi_tx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static uint16_t adsi_encode_baudot(adsi_tx_state_t *s, uint8_t ch)
{
    static const uint8_t conv[128] =
    {
        0x00, /* NUL */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0x42, /* LF */
        0xFF, /*   */
        0xFF, /*   */
        0x48, /* CR */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0xFF, /*   */
        0x44, /*   */
        0xFF, /* ! */
        0xFF, /* " */
        0x94, /* # */
        0x89, /* $ */
        0xFF, /* % */
        0xFF, /* & */
        0x85, /* ' */
        0x8F, /* ( */
        0x92, /* ) */
        0x8B, /* * */
        0x91, /* + */
        0x8C, /* , */
        0x83, /* - */
        0x9C, /* . */
        0x9D, /* / */
        0x96, /* 0 */
        0x97, /* 1 */
        0x93, /* 2 */
        0x81, /* 3 */
        0x8A, /* 4 */
        0x90, /* 5 */
        0x95, /* 6 */
        0x87, /* 7 */
        0x86, /* 8 */
        0x98, /* 9 */
        0x8E, /* : */
        0xFF, /* ; */
        0xFF, /* < */
        0x9E, /* = */
        0xFF, /* > */
        0x99, /* ? */
        0xFF, /* @ */
        0x03, /* A */
        0x19, /* B */
        0x0E, /* C */
        0x09, /* D */
        0x01, /* E */
        0x0D, /* F */
        0x1A, /* G */
        0x14, /* H */
        0x06, /* I */
        0x0B, /* J */
        0x0F, /* K */
        0x12, /* L */
        0x1C, /* M */
        0x0C, /* N */
        0x18, /* O */
        0x16, /* P */
        0x17, /* Q */
        0x0A, /* R */
        0x05, /* S */
        0x10, /* T */
        0x07, /* U */
        0x1E, /* V */
        0x13, /* W */
        0x1D, /* X */
        0x15, /* Y */
        0x11, /* Z */
        0xFF, /* [ */
        0xFF, /* \ */
        0xFF, /* ] */
        0x9B, /* ^ */
        0xFF, /* _ */
        0xFF, /* ` */
        0x03, /* a */
        0x19, /* b */
        0x0E, /* c */
        0x09, /* d */
        0x01, /* e */
        0x0D, /* f */
        0x1A, /* g */
        0x14, /* h */
        0x06, /* i */
        0x0B, /* j */
        0x0F, /* k */
        0x12, /* l */
        0x1C, /* m */
        0x0C, /* n */
        0x18, /* o */
        0x16, /* p */
        0x17, /* q */
        0x0A, /* r */
        0x05, /* s */
        0x10, /* t */
        0x07, /* u */
        0x1E, /* v */
        0x13, /* w */
        0x1D, /* x */
        0x15, /* y */
        0x11, /* z */
        0xFF, /* { */
        0xFF, /* | */
        0xFF, /* } */
        0xFF, /* ~ */
        0xFF, /* DEL */
    };
    uint16_t shift;

    ch = conv[ch];
    if (ch == 0xFF)
        return 0;
    if ((ch & 0x40))
        return ch & 0x1F;
    if ((ch & 0x80))
    {
        if (s->baudot_shift == 1)
            return ch & 0x1F;
        s->baudot_shift = 1;
        shift = BAUDOT_FIGURE_SHIFT;
    }
    else
    {
        if (s->baudot_shift == 0)
            return ch & 0x1F;
        s->baudot_shift = 0;
        shift = BAUDOT_LETTER_SHIFT;
    }
    return (shift << 5) | (ch & 0x1F);
}
/*- End of function --------------------------------------------------------*/

static uint8_t adsi_decode_baudot(adsi_rx_state_t *s, uint8_t ch)
{
    static const uint8_t conv[2][32] =
    {
        {"\000E\nA SIU\rDRJNFCKTZLWHYPQOBG^MXV^"},
        {"\0003\n- '87\r$4*,*:(5+)2#6019?*^./=^"}
    };

    switch (ch)
    {
    case BAUDOT_FIGURE_SHIFT:
        s->baudot_shift = 1;
        break;
    case BAUDOT_LETTER_SHIFT:
        s->baudot_shift = 0;
        break;
    default:
        return conv[s->baudot_shift][ch];
    }
    /* return 0 if we did not produce a character */
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) adsi_next_field(adsi_rx_state_t *s, const uint8_t *msg, int msg_len, int pos, uint8_t *field_type, uint8_t const **field_body, int *field_len)
{
    int i;

    /* Return -1 for no more fields. Return -2 for message structure corrupt. */
    switch (s->standard)
    {
    case ADSI_STANDARD_CLASS:
    case ADSI_STANDARD_CLIP:
    case ADSI_STANDARD_ACLIP:
        if (pos >= msg_len)
            return -1;
        /* For MDMF type messages, these standards all use "IE" type fields - type,
           length, contents - and similar headers */
        if (pos <= 0)
        {
            /* Return the message type */
            *field_type = msg[0];
            *field_len = 0;
            *field_body = NULL;
            pos = 2;
        }
        else
        {
            if ((msg[0] & 0x80))
            {
                /* MDMF messages seem to always have a message type with the MSB set. Is that
                   guaranteed? */
                *field_type = msg[pos++];
                *field_len = msg[pos++];
                *field_body = msg + pos;
            }
            else
            {
                /* SDMF */
                *field_type = 0;
                *field_len = msg_len - pos;
                *field_body = msg + pos;
            }
            pos += *field_len;
        }
        if (pos > msg_len)
            return -2;
        break;
    case ADSI_STANDARD_JCLIP:
        if (pos >= msg_len - 2)
            return -1;
        if (pos <= 0)
        {
            /* Return the message type */
            pos = 5;
            *field_type = msg[pos++];
            if (*field_type == DLE)
                pos++;
            if (msg[pos++] == DLE)
                pos++;
            *field_len = 0;
            *field_body = NULL;
        }
        else
        {
            *field_type = msg[pos++];
            if (*field_type == DLE)
                pos++;
            *field_len = msg[pos++];
            if (*field_len == DLE)
                pos++;
            /* TODO: we assume here that the body contains no DLE's that would have been stuffed */
            *field_body = msg + pos;
            pos += *field_len;
        }
        if (pos > msg_len - 2)
            return -2;
        break;
    case ADSI_STANDARD_CLIP_DTMF:
        if (pos > msg_len)
            return -1;
        if (pos <= 0)
        {
            pos = 1;
            *field_type = msg[msg_len - 1];
            *field_len = 0;
            *field_body = NULL;
        }
        else
        {
            /* Remove bias on the pos value */
            pos--;
            if (msg[pos] >= '0'  &&  msg[pos] <= '9')
                *field_type = CLIP_DTMF_HASH_UNSPECIFIED;
            else
                *field_type = msg[pos++];
            *field_body = msg + pos;
            i = pos;
            while (i < msg_len  &&  msg[i] >= '0'  &&  msg[i] <= '9')
                i++;
            *field_len = i - pos;
            pos = i;
            /* Check if we have reached the end of message marker. */
            if (msg[pos] == '#'  ||  msg[pos] == 'C')
                pos++;
            if (pos > msg_len)
                return -2;
            /* Bias the pos value, so we don't return 0 inappropriately */
            pos++;
        }
        break;
    case ADSI_STANDARD_TDD:
        if (pos >= msg_len)
            return -1;
        *field_type = 0;
        *field_body = msg;
        *field_len = msg_len;
        pos = msg_len;
        break;
    }
    return pos;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) adsi_add_field(adsi_tx_state_t *s, uint8_t *msg, int len, uint8_t field_type, uint8_t const *field_body, int field_len)
{
    int i;
    int x;

    switch (s->standard)
    {
    case ADSI_STANDARD_CLASS:
    case ADSI_STANDARD_CLIP:
    case ADSI_STANDARD_ACLIP:
        /* These standards all use "IE" type fields - type, length, value - and similar headers */
        if (len <= 0)
        {
            /* Initialise a new message. The field type is actually the message type. */
            msg[0] = field_type;
            msg[1] = 0;
            len = 2;
        }
        else
        {
            /* Add to a message in progress. */
            if (field_type)
            {
                msg[len++] = field_type;
                msg[len++] = (uint8_t) field_len;
                if (field_len == DLE)
                    msg[len++] = (uint8_t) field_len;
                memcpy(&msg[len], field_body, field_len);
                len += field_len;
            }
            else
            {
                /* No field type or length, for restricted single message formats */
                memcpy(&msg[len], field_body, field_len);
                len += field_len;
            }
        }
        break;
    case ADSI_STANDARD_JCLIP:
        /* This standard uses "IE" type fields - type, length, value - but escapes DLE characters,
           to prevent immitation of a control octet. */
        if (len <= 0)
        {
            /* Initialise a new message. The field type is actually the message type. */
            msg[0] = field_type;
            msg[1] = 0;
            len = 2;
        }
        else
        {
            /* Add to a message in progress. */
            msg[len++] = field_type;
            if (field_type == DLE)
                msg[len++] = field_type;
            msg[len++] = (uint8_t) field_len;
            if (field_len == DLE)
                msg[len++] = (uint8_t) field_len;
            for (i = 0;  i < field_len;  i++)
            {
                msg[len++] = field_body[i];
                if (field_body[i] == DLE)
                    msg[len++] = field_body[i];
            }
        }
        break;
    case ADSI_STANDARD_CLIP_DTMF:
        if (len <= 0)
        {
            /* Initialise a new message. The field type is actually the message type. */
            msg[0] = field_type;
            len = 1;
        }
        else
        {
            /* Save and reuse the terminator/message type */
            x = msg[--len];
            if (field_type != CLIP_DTMF_HASH_UNSPECIFIED)
                msg[len++] = field_type;
            memcpy(&msg[len], field_body, field_len);
            msg[len + field_len] = (uint8_t) x;
            len += (field_len + 1);
        }
        break;
    case ADSI_STANDARD_TDD:
        if (len < 0)
            len = 0;
        for (i = 0;  i < field_len;  i++)
        {
            if ((x = adsi_encode_baudot(s, field_body[i])))
            {
                if ((x & 0x3E0))
                    msg[len++] = (uint8_t) ((x >> 5) & 0x1F);
                msg[len++] = (uint8_t) (x & 0x1F);
            }
        }
        break;
    }

    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) adsi_standard_to_str(int standard)
{
    switch (standard)
    {
    case ADSI_STANDARD_CLASS:
        return "CLASS";
    case ADSI_STANDARD_CLIP:
        return "CLIP";
    case ADSI_STANDARD_ACLIP:
        return "A-CLIP";
    case ADSI_STANDARD_JCLIP:
        return "J-CLIP";
    case ADSI_STANDARD_CLIP_DTMF:
        return "CLIP-DTMF";
    case ADSI_STANDARD_TDD:
        return "TDD";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
