/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_core.c - Encode and decode the ASN.1 of a T.38 IFP message
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005, 2006 Steve Underwood
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
 * $Id: t38_core.c,v 1.54 2009/10/09 14:53:57 steveu Exp $
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
#include <memory.h>
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/t38_core.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t38_core.h"

#define ACCEPTABLE_SEQ_NO_OFFSET    2000

/* The times for training, the optional TEP, and the HDLC preamble, for all the modem options, in ms.
   Note that the preamble for V.21 is 1s+-15%, and for the other modems is 200ms+100ms. */
static const struct
{
    int tep;
    int training;
    int flags;
} modem_startup_time[] =
{
    {      0,   75000,       0},    /* T38_IND_NO_SIGNAL */
    {      0,       0,       0},    /* T38_IND_CNG */
    {      0, 3000000,       0},    /* T38_IND_CED */
    {      0,       0, 1000000},    /* T38_IND_V21_PREAMBLE */ /* TODO: 850ms should be OK for this, but it causes trouble with some ATAs. Why? */
    { 215000,  943000,  200000},    /* T38_IND_V27TER_2400_TRAINING */
    { 215000,  708000,  200000},    /* T38_IND_V27TER_4800_TRAINING */
    { 215000,  234000,  200000},    /* T38_IND_V29_7200_TRAINING */
    { 215000,  234000,  200000},    /* T38_IND_V29_9600_TRAINING */
    { 215000,  142000,  200000},    /* T38_IND_V17_7200_SHORT_TRAINING */
    { 215000, 1393000,  200000},    /* T38_IND_V17_7200_LONG_TRAINING */
    { 215000,  142000,  200000},    /* T38_IND_V17_9600_SHORT_TRAINING */
    { 215000, 1393000,  200000},    /* T38_IND_V17_9600_LONG_TRAINING */
    { 215000,  142000,  200000},    /* T38_IND_V17_12000_SHORT_TRAINING */
    { 215000, 1393000,  200000},    /* T38_IND_V17_12000_LONG_TRAINING */
    { 215000,  142000,  200000},    /* T38_IND_V17_14400_SHORT_TRAINING */
    { 215000, 1393000,  200000},    /* T38_IND_V17_14400_LONG_TRAINING */
    { 215000,       0,       0},    /* T38_IND_V8_ANSAM */
    { 215000,       0,       0},    /* T38_IND_V8_SIGNAL */
    { 215000,       0,       0},    /* T38_IND_V34_CNTL_CHANNEL_1200 */
    { 215000,       0,       0},    /* T38_IND_V34_PRI_CHANNEL */
    { 215000,       0,       0},    /* T38_IND_V34_CC_RETRAIN */
    { 215000,       0,       0},    /* T38_IND_V33_12000_TRAINING */
    { 215000,       0,       0}     /* T38_IND_V33_14400_TRAINING */
};

SPAN_DECLARE(const char *) t38_indicator_to_str(int indicator)
{
    switch (indicator)
    {
    case T38_IND_NO_SIGNAL:
        return "no-signal";
    case T38_IND_CNG:
        return "cng";
    case T38_IND_CED:
        return "ced";
    case T38_IND_V21_PREAMBLE:
        return "v21-preamble";
    case T38_IND_V27TER_2400_TRAINING:
        return "v27-2400-training";
    case T38_IND_V27TER_4800_TRAINING:
        return "v27-4800-training";
    case T38_IND_V29_7200_TRAINING:
        return "v29-7200-training";
    case T38_IND_V29_9600_TRAINING:
        return "v29-9600-training";
    case T38_IND_V17_7200_SHORT_TRAINING:
        return "v17-7200-short-training";
    case T38_IND_V17_7200_LONG_TRAINING:
        return "v17-7200-long-training";
    case T38_IND_V17_9600_SHORT_TRAINING:
        return "v17-9600-short-training";
    case T38_IND_V17_9600_LONG_TRAINING:
        return "v17-9600-long-training";
    case T38_IND_V17_12000_SHORT_TRAINING:
        return "v17-12000-short-training";
    case T38_IND_V17_12000_LONG_TRAINING:
        return "v17-12000-long-training";
    case T38_IND_V17_14400_SHORT_TRAINING:
        return "v17-14400-short-training";
    case T38_IND_V17_14400_LONG_TRAINING:
        return "v17-14400-long-training";
    case T38_IND_V8_ANSAM:
        return "v8-ansam";
    case T38_IND_V8_SIGNAL:
        return "v8-signal";
    case T38_IND_V34_CNTL_CHANNEL_1200:
        return "v34-cntl-channel-1200";
    case T38_IND_V34_PRI_CHANNEL:
        return "v34-pri-channel";
    case T38_IND_V34_CC_RETRAIN:
        return "v34-CC-retrain";
    case T38_IND_V33_12000_TRAINING:
        return "v33-12000-training";
    case T38_IND_V33_14400_TRAINING:
        return "v33-14400-training";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t38_data_type_to_str(int data_type)
{
    switch (data_type)
    {
    case T38_DATA_V21:
        return "v21";
    case T38_DATA_V27TER_2400:
        return "v27-2400";
    case T38_DATA_V27TER_4800:
        return "v27-4800";
    case T38_DATA_V29_7200:
        return "v29-7200";
    case T38_DATA_V29_9600:
        return "v29-9600";
    case T38_DATA_V17_7200:
        return "v17-7200";
    case T38_DATA_V17_9600:
        return "v17-9600";
    case T38_DATA_V17_12000:
        return "v17-12000";
    case T38_DATA_V17_14400:
        return "v17-14400";
    case T38_DATA_V8:
        return "v8";
    case T38_DATA_V34_PRI_RATE:
        return "v34-pri-rate";
    case T38_DATA_V34_CC_1200:
        return "v34-CC-1200";
    case T38_DATA_V34_PRI_CH:
        return "v34-pri-vh";
    case T38_DATA_V33_12000:
        return "v33-12000";
    case T38_DATA_V33_14400:
        return "v33-14400";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t38_field_type_to_str(int field_type)
{
    switch (field_type)
    {
    case T38_FIELD_HDLC_DATA:
        return "hdlc-data";
    case T38_FIELD_HDLC_SIG_END:
        return "hdlc-sig-end";
    case T38_FIELD_HDLC_FCS_OK:
        return "hdlc-fcs-OK";
    case T38_FIELD_HDLC_FCS_BAD:
        return "hdlc-fcs-BAD";
    case T38_FIELD_HDLC_FCS_OK_SIG_END:
        return "hdlc-fcs-OK-sig-end";
    case T38_FIELD_HDLC_FCS_BAD_SIG_END:
        return "hdlc-fcs-BAD-sig-end";
    case T38_FIELD_T4_NON_ECM_DATA:
        return "t4-non-ecm-data";
    case T38_FIELD_T4_NON_ECM_SIG_END:
        return "t4-non-ecm-sig-end";
    case T38_FIELD_CM_MESSAGE:
        return "cm-message";
    case T38_FIELD_JM_MESSAGE:
        return "jm-message";
    case T38_FIELD_CI_MESSAGE:
        return "ci-message";
    case T38_FIELD_V34RATE:
        return "v34rate";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t38_cm_profile_to_str(int profile)
{
    switch (profile)
    {
    case '1':
        return "G3 FAX sending terminal";
    case '2':
        return "G3 FAX receiving terminal";
    case '3':
        return "V.34 HDX and G3 FAX sending terminal";
    case '4':
        return "V.34 HDX and G3 FAX receiving terminal";
    case '5':
        return "V.34 HDX-only FAX sending terminal";
    case '6':
        return "V.34 HDX-only FAX receiving terminal";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t38_jm_to_str(const uint8_t *data, int len)
{
    if (len < 2)
        return "???";
    switch (data[0])
    {
    case 'A':
        switch (data[1])
        {
        case '0':
            return "ACK";
        }
        break;
    case 'N':
        switch (data[1])
        {
        case '0':
            return "NACK: No compatible mode available";
        case '1':
            /* Response for profiles 1 and 2 */
            return "NACK: No V.34 FAX, use G3 FAX";
        case '2':
            /* Response for profiles 5 and 6 */
            return "NACK: V.34 only FAX.";
        }
        break;
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_v34rate_to_bps(const uint8_t *data, int len)
{
    int i;
    int rate;

    if (len < 3)
        return -1;
    for (i = 0, rate = 0;  i < 3;  i++)
    {
        if (data[i] < '0'  ||  data[i] > '9')
            return -1;
        rate = rate*10 + data[i] - '0';
    }
    return rate*100;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int classify_seq_no_offset(int expected, int actual)
{
    /* Classify the mismatch between expected and actual sequence numbers
       according to whether the actual is a little in the past (late), a
       little in the future (some packets have been lost), or a large jump
       that represents the sequence being lost (possibly when some RTP
       gets dumped to a UDPTL port). */
    /* This assumes they are not equal */
    if (expected > actual)
    {
        if (expected > actual + 0x10000 - ACCEPTABLE_SEQ_NO_OFFSET)
        {
            /* In the near future */
            return 1;
        }
        if (expected < actual + ACCEPTABLE_SEQ_NO_OFFSET)
        {
            /* In the recent past */
            return -1;
        }
    }
    else
    {
        if (expected + ACCEPTABLE_SEQ_NO_OFFSET > actual)
        {
            /* In the near future */
            return 1;
        }
        if (expected + 0x10000 - ACCEPTABLE_SEQ_NO_OFFSET < actual)
        {
            /* In the recent past */
            return -1;
        }
    }
    /* There has been a huge step in the sequence */
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_core_rx_ifp_packet(t38_core_state_t *s, const uint8_t *buf, int len, uint16_t seq_no)
{
    int i;
    int t30_indicator;
    int t30_data;
    int ptr;
    int other_half;
    int numocts;
    int log_seq_no;
    const uint8_t *msg;
    unsigned int count;
    unsigned int t30_field_type;
    uint8_t type;
    uint8_t data_field_present;
    uint8_t field_data_present;
    char tag[20];

    log_seq_no = (s->check_sequence_numbers)  ?  seq_no  :  s->rx_expected_seq_no;

    if (span_log_test(&s->logging, SPAN_LOG_FLOW))
    {
        sprintf(tag, "Rx %5d: IFP", log_seq_no);
        span_log_buf(&s->logging, SPAN_LOG_FLOW, tag, buf, len);
    }
    if (len < 1)
    {
        span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Bad packet length - %d\n", log_seq_no, len);
        return -1;
    }
    if (s->check_sequence_numbers)
    {
        seq_no &= 0xFFFF;
        if (seq_no != s->rx_expected_seq_no)
        {
            /* An expected value of -1 indicates this is the first received packet, and will accept
               anything for that. We can't assume they will start from zero, even though they should. */
            if (s->rx_expected_seq_no != -1)
            {
                /* We have a packet with a serial number that is not in sequence. The cause could be:
                    - 1. a repeat copy of a recent packet. Many T.38 implementations can preduce quite a lot of these.
                    - 2. a late packet, whose point in the sequence we have already passed.
                    - 3. the result of a hop in the sequence numbers cause by something weird from the other
                         end. Stream switching might cause this
                    - 4. missing packets.
    
                    In cases 1 and 2 we need to drop this packet. In case 2 it might make sense to try to do
                    something with it in the terminal case. Currently we don't. For gateway operation it will be
                    too late to do anything useful.
                 */
                if (((seq_no + 1) & 0xFFFF) == s->rx_expected_seq_no)
                {
                    /* Assume this is truly a repeat packet, and don't bother checking its contents. */
                    span_log(&s->logging, SPAN_LOG_FLOW, "Rx %5d: Repeat packet number\n", log_seq_no);
                    return 0;
                }
                /* Distinguish between a little bit out of sequence, and a huge hop. */
                switch (classify_seq_no_offset(s->rx_expected_seq_no, seq_no))
                {
                case -1:
                    /* This packet is in the near past, so its late. */
                    span_log(&s->logging, SPAN_LOG_FLOW, "Rx %5d: Late packet - expected %d\n", log_seq_no, s->rx_expected_seq_no);
                    return 0;
                case 1:
                    /* This packet is in the near future, so some packets have been lost */
                    span_log(&s->logging, SPAN_LOG_FLOW, "Rx %5d: Missing from %d\n", log_seq_no, s->rx_expected_seq_no);
                    s->rx_missing_handler(s, s->rx_user_data, s->rx_expected_seq_no, seq_no);
                    s->missing_packets += (seq_no - s->rx_expected_seq_no);
                    break;
                default:
                    /* The sequence has jumped wildly */
                    span_log(&s->logging, SPAN_LOG_FLOW, "Rx %5d: Sequence restart\n", log_seq_no);
                    s->rx_missing_handler(s, s->rx_user_data, -1, -1);
                    s->missing_packets++;
                    break;
                }
            }
            s->rx_expected_seq_no = seq_no;
        }
    }
    /* The sequence numbering is defined as rolling from 0xFFFF to 0x0000. Some implementations
       of T.38 roll from 0xFFFF to 0x0001. Isn't standardisation a wonderful thing? The T.38
       document specifies only a small fraction of what it should, yet then they actually nail
       something properly, people ignore it. Developers in this industry truly deserves the ****
       **** **** **** **** **** documents they have to live with. Anyway, when the far end has a
       broken rollover behaviour we will get a hiccup at the rollover point. Don't worry too
       much. We will just treat the message in progress as one with some missing data. With any
       luck a retry will ride over the problem. Rollovers don't occur that often. It takes quite
       a few FAX pages to reach rollover. */
    s->rx_expected_seq_no = (s->rx_expected_seq_no + 1) & 0xFFFF;
    data_field_present = (buf[0] >> 7) & 1;
    type = (buf[0] >> 6) & 1;
    ptr = 0;
    switch (type)
    {
    case T38_TYPE_OF_MSG_T30_INDICATOR:
        /* Indicators should never have a data field */
        if (data_field_present)
        {
            span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Data field with indicator\n", log_seq_no);
            return -1;
        }
        /* Any received indicator should mean we no longer have a valid concept of "last received data/field type". */
        s->current_rx_data_type = -1;
        s->current_rx_field_type = -1;
        if ((buf[0] & 0x20))
        {
            /* Extension */
            if (len != 2)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for indicator (A)\n", log_seq_no);
                return -1;
            }
            t30_indicator = T38_IND_V8_ANSAM + (((buf[0] << 2) & 0x3C) | ((buf[1] >> 6) & 0x3));
            if (t30_indicator > T38_IND_V33_14400_TRAINING)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Unknown indicator - %d\n", log_seq_no, t30_indicator);
                return -1;
            }
        }
        else
        {
            if (len != 1)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for indicator (B)\n", log_seq_no);
                return -1;
            }
            t30_indicator = (buf[0] >> 1) & 0xF;
        }
        span_log(&s->logging, SPAN_LOG_FLOW, "Rx %5d: indicator %s\n", log_seq_no, t38_indicator_to_str(t30_indicator));
        s->rx_indicator_handler(s, s->rx_user_data, t30_indicator);
        /* This must come after the indicator handler, so the handler routine sees the existing state of the
           indicator. */
        s->current_rx_indicator = t30_indicator;
        break;
    case T38_TYPE_OF_MSG_T30_DATA:
        if ((buf[0] & 0x20))
        {
            /* Extension */
            if (len < 2)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for data (A)\n", log_seq_no);
                return -1;
            }
            t30_data = T38_DATA_V8 + (((buf[0] << 2) & 0x3C) | ((buf[1] >> 6) & 0x3));
            if (t30_data > T38_DATA_V33_14400)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Unknown data type - %d\n", log_seq_no, t30_data);
                return -1;
            }
            ptr = 2;
        }
        else
        {
            t30_data = (buf[0] >> 1) & 0xF;
            if (t30_data > T38_DATA_V17_14400)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Unknown data type - %d\n", log_seq_no, t30_data);
                return -1;
            }
            ptr = 1;
        }
        if (!data_field_present)
        {
            /* This is kinda weird, but I guess if the length checks out we accept it. */
            span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Data type with no data field\n", log_seq_no);
            if (ptr != len)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for data (B)\n", log_seq_no);
                return -1;
            }
            break;
        }
        if (ptr >= len)
        {
            span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for data (C)\n", log_seq_no);
            return -1;
        }
        count = buf[ptr++];
        //printf("Count is %d\n", count);
        other_half = FALSE;
        t30_field_type = 0;
        for (i = 0;  i < (int) count;  i++)
        {
            if (ptr >= len)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for data (D)\n", log_seq_no);
                return -1;
            }
            if (s->t38_version == 0)
            {
                /* The original version of T.38 with a typo in the ASN.1 spec. */
                if (other_half)
                {
                    /* The lack of a data field in the previous message means
                       we are currently in the middle of an octet. */
                    field_data_present = (buf[ptr] >> 3) & 1;
                    /* Decode field_type */
                    t30_field_type = buf[ptr] & 0x7;
                    ptr++;
                    other_half = FALSE;
                }
                else
                {
                    field_data_present = (buf[ptr] >> 7) & 1;
                    /* Decode field_type */
                    t30_field_type = (buf[ptr] >> 4) & 0x7;
                    if (field_data_present)
                        ptr++;
                    else
                        other_half = TRUE;
                }
                if (t30_field_type > T38_FIELD_T4_NON_ECM_SIG_END)
                {
                    span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Unknown field type - %d\n", log_seq_no, t30_field_type);
                    return -1;
                }
            }
            else
            {
                field_data_present = (buf[ptr] >> 7) & 1;
                /* Decode field_type */
                if ((buf[ptr] & 0x40))
                {
                    if (ptr > len - 2)
                    {
                        span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for data (E)\n", log_seq_no);
                        return -1;
                    }
                    t30_field_type = T38_FIELD_CM_MESSAGE + (((buf[ptr] << 2) & 0x3C) | ((buf[ptr + 1] >> 6) & 0x3));
                    if (t30_field_type > T38_FIELD_V34RATE)
                    {
                        span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Unknown field type - %d\n", log_seq_no, t30_field_type);
                        return -1;
                    }
                    ptr += 2;
                }
                else
                {
                    t30_field_type = (buf[ptr++] >> 3) & 0x7;
                }
            }
            /* Decode field_data */
            if (field_data_present)
            {
                if (ptr > len - 2)
                {
                    span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for data (F)\n", log_seq_no);
                    return -1;
                }
                numocts = ((buf[ptr] << 8) | buf[ptr + 1]) + 1;
                msg = buf + ptr + 2;
                ptr += numocts + 2;
            }
            else
            {
                numocts = 0;
                msg = NULL;
            }
            if (ptr > len)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for data (G)\n", log_seq_no);
                return -1;
            }
            span_log(&s->logging,
                     SPAN_LOG_FLOW,
                     "Rx %5d: (%d) data %s/%s + %d byte(s)\n",
                     log_seq_no,
                     i,
                     t38_data_type_to_str(t30_data),
                     t38_field_type_to_str(t30_field_type),
                     numocts);
            s->rx_data_handler(s, s->rx_user_data, t30_data, t30_field_type, msg, numocts);
            s->current_rx_data_type = t30_data;
            s->current_rx_field_type = t30_field_type;
        }
        if (ptr != len)
        {
            if (s->t38_version != 0  ||  ptr != (len - 1)  ||  !other_half)
            {
                span_log(&s->logging, SPAN_LOG_PROTOCOL_WARNING, "Rx %5d: Invalid length for data (H) - %d %d\n", log_seq_no, ptr, len);
                return -1;
            }
        }
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int t38_encode_indicator(t38_core_state_t *s, uint8_t buf[], int indicator)
{
    int len;

    /* Build the IFP packet */
    /* Data field not present */
    /* Indicator packet */
    /* Type of indicator */
    if (indicator <= T38_IND_V17_14400_LONG_TRAINING)
    {
        buf[0] = (uint8_t) (indicator << 1);
        len = 1;
    }
    else if (s->t38_version != 0  &&  indicator <= T38_IND_V33_14400_TRAINING)
    {
        buf[0] = (uint8_t) (0x20 | (((indicator - T38_IND_V8_ANSAM) & 0xF) >> 2));
        buf[1] = (uint8_t) (((indicator - T38_IND_V8_ANSAM) << 6) & 0xFF);
        len = 2;
    }
    else
    {
        len = -1;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static int t38_encode_data(t38_core_state_t *s, uint8_t buf[], int data_type, const t38_data_field_t field[], int fields)
{
    int len;
    int i;
    int enclen;
    int multiplier;
    int data_field_no;
    const t38_data_field_t *q;
    unsigned int encoded_len;
    unsigned int fragment_len;
    unsigned int value;
    uint8_t data_field_present;
    uint8_t field_data_present;
    char tag[20];

    /* Build the IFP packet */

    /* There seems no valid reason why a packet would ever be generated without a data field present */
    data_field_present = TRUE;

    for (data_field_no = 0;  data_field_no < fields;  data_field_no++)
    {
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "Tx %5d: (%d) data %s/%s + %d byte(s)\n",
                 s->tx_seq_no,
                 data_field_no,
                 t38_data_type_to_str(data_type),
                 t38_field_type_to_str(field[data_field_no].field_type),
                 field[data_field_no].field_len);
    }
    
    data_field_no = 0;
    len = 0;
    /* Data field present */
    /* Data packet */
    /* Type of data */
    if (data_type <= T38_DATA_V17_14400)
    {
        buf[len++] = (uint8_t) ((data_field_present << 7) | 0x40 | (data_type << 1));
    }
    else if (s->t38_version != 0  &&  data_type <= T38_DATA_V33_14400)
    {
        buf[len++] = (uint8_t) ((data_field_present << 7) | 0x60 | (((data_type - T38_DATA_V8) & 0xF) >> 2));
        buf[len++] = (uint8_t) (((data_type - T38_DATA_V8) << 6) & 0xFF);
    }
    else
    {
        return -1;
    }
    if (data_field_present)
    {
        encoded_len = 0;
        data_field_no = 0;
        do
        {
            value = fields - encoded_len;
            if (value < 0x80)
            {
                /* 1 octet case */
                buf[len++] = (uint8_t) value;
                enclen = value;
            }
            else if (value < 0x4000)
            {
                /* 2 octet case */
                buf[len++] = (uint8_t) (0x80 | ((value >> 8) & 0xFF));
                buf[len++] = (uint8_t) (value & 0xFF);
                enclen = value;
            }
            else
            {
                /* Fragmentation case */
                multiplier = (value/0x4000 < 4)  ?  value/0x4000  :  4;
                buf[len++] = (uint8_t) (0xC0 | multiplier);
                enclen = 0x4000*multiplier;
            }

            fragment_len = enclen;
            encoded_len += fragment_len;
            /* Encode the elements */
            for (i = 0;  i < (int) encoded_len;  i++)
            {
                q = &field[data_field_no];
                field_data_present = (uint8_t) (q->field_len > 0);
                /* Encode field_type */
                if (s->t38_version == 0)
                {
                    /* Original version of T.38 with a typo */
                    if (q->field_type > T38_FIELD_T4_NON_ECM_SIG_END)
                        return -1;
                    buf[len++] = (uint8_t) ((field_data_present << 7) | (q->field_type << 4));
                }
                else
                {
                    if (q->field_type <= T38_FIELD_T4_NON_ECM_SIG_END)
                    {
                        buf[len++] = (uint8_t) ((field_data_present << 7) | (q->field_type << 3));
                    }
                    else if (q->field_type <= T38_FIELD_V34RATE)
                    {
                        buf[len++] = (uint8_t) ((field_data_present << 7) | 0x40 | ((q->field_type - T38_FIELD_CM_MESSAGE) >> 2));
                        buf[len++] = (uint8_t) (((q->field_type - T38_FIELD_CM_MESSAGE) << 6) & 0xC0);
                    }
                    else
                    {
                        return -1;
                    }
                }
                /* Encode field_data */
                if (field_data_present)
                {
                    if (q->field_len < 1  ||  q->field_len > 65535)
                        return -1;
                    buf[len++] = (uint8_t) (((q->field_len - 1) >> 8) & 0xFF);
                    buf[len++] = (uint8_t) ((q->field_len - 1) & 0xFF);
                    memcpy(buf + len, q->field, q->field_len);
                    len += q->field_len;
                }
                data_field_no++;
            }
        }
        while (fields != (int) encoded_len  ||  fragment_len >= 16384);
    }

    if (span_log_test(&s->logging, SPAN_LOG_FLOW))
    {
        sprintf(tag, "Tx %5d: IFP", s->tx_seq_no);
        span_log_buf(&s->logging, SPAN_LOG_FLOW, tag, buf, len);
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_core_send_indicator(t38_core_state_t *s, int indicator)
{
    uint8_t buf[100];
    int len;
    int delay;

    delay = 0;
    /* Only send an indicator if it represents a change of state. */
    if (s->current_tx_indicator != indicator)
    {
        /* Zero is a valid count, to suppress the transmission of indicators when the
           transport means they are not needed - e.g. TPKT/TCP. */
        if (s->category_control[T38_PACKET_CATEGORY_INDICATOR])
        {
            if ((len = t38_encode_indicator(s, buf, indicator)) < 0)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "T.38 indicator len is %d\n", len);
                return len;
            }
            span_log(&s->logging, SPAN_LOG_FLOW, "Tx %5d: indicator %s\n", s->tx_seq_no, t38_indicator_to_str(indicator));
            s->tx_packet_handler(s, s->tx_packet_user_data, buf, len, s->category_control[T38_PACKET_CATEGORY_INDICATOR]);
            s->tx_seq_no = (s->tx_seq_no + 1) & 0xFFFF;
            delay = modem_startup_time[indicator].training;
            if (s->allow_for_tep)
                delay += modem_startup_time[indicator].tep;
        }
        s->current_tx_indicator = indicator;
    }
    return delay;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_core_send_flags_delay(t38_core_state_t *s, int indicator)
{
    return modem_startup_time[indicator].flags;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_core_send_data(t38_core_state_t *s, int data_type, int field_type, const uint8_t field[], int field_len, int category)
{
    t38_data_field_t field0;
    uint8_t buf[1000];
    int len;

    field0.field_type = field_type;
    field0.field = field;
    field0.field_len = field_len;
    if ((len = t38_encode_data(s, buf, data_type, &field0, 1)) < 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "T.38 data len is %d\n", len);
        return len;
    }
    s->tx_packet_handler(s, s->tx_packet_user_data, buf, len, s->category_control[category]);
    s->tx_seq_no = (s->tx_seq_no + 1) & 0xFFFF;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_core_send_data_multi_field(t38_core_state_t *s, int data_type, const t38_data_field_t field[], int fields, int category)
{
    uint8_t buf[1000];
    int len;

    if ((len = t38_encode_data(s, buf, data_type, field, fields)) < 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "T.38 data len is %d\n", len);
        return len;
    }
    s->tx_packet_handler(s, s->tx_packet_user_data, buf, len, s->category_control[category]);
    s->tx_seq_no = (s->tx_seq_no + 1) & 0xFFFF;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_data_rate_management_method(t38_core_state_t *s, int method)
{
    s->data_rate_management_method = method;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_data_transport_protocol(t38_core_state_t *s, int data_transport_protocol)
{
    s->data_transport_protocol = data_transport_protocol;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_fill_bit_removal(t38_core_state_t *s, int fill_bit_removal)
{
    s->fill_bit_removal = fill_bit_removal;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_mmr_transcoding(t38_core_state_t *s, int mmr_transcoding)
{
    s->mmr_transcoding = mmr_transcoding;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_jbig_transcoding(t38_core_state_t *s, int jbig_transcoding)
{
    s->jbig_transcoding = jbig_transcoding;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_max_buffer_size(t38_core_state_t *s, int max_buffer_size)
{
    s->max_buffer_size = max_buffer_size;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_max_datagram_size(t38_core_state_t *s, int max_datagram_size)
{
    s->max_datagram_size = max_datagram_size;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_t38_version(t38_core_state_t *s, int t38_version)
{
    s->t38_version = t38_version;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_sequence_number_handling(t38_core_state_t *s, int check)
{
    s->check_sequence_numbers = check;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_tep_handling(t38_core_state_t *s, int allow_for_tep)
{
    s->allow_for_tep = allow_for_tep;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_redundancy_control(t38_core_state_t *s, int category, int setting)
{
    s->category_control[category] = setting;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t38_set_fastest_image_data_rate(t38_core_state_t *s, int max_rate)
{
    s->fastest_image_data_rate = max_rate;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_get_fastest_image_data_rate(t38_core_state_t *s)
{
    return s->fastest_image_data_rate;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t38_core_get_logging_state(t38_core_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t38_core_state_t *) t38_core_init(t38_core_state_t *s,
                                               t38_rx_indicator_handler_t *rx_indicator_handler,
                                               t38_rx_data_handler_t *rx_data_handler,
                                               t38_rx_missing_handler_t *rx_missing_handler,
                                               void *rx_user_data,
                                               t38_tx_packet_handler_t *tx_packet_handler,
                                               void *tx_packet_user_data)
{
    if (s == NULL)
    {
        if ((s = (t38_core_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.38");

    /* Set some defaults for the parameters configurable from outside the
       T.38 domain - e.g. from SDP data. */
    s->data_rate_management_method = T38_DATA_RATE_MANAGEMENT_TRANSFERRED_TCF;
    s->data_transport_protocol = T38_TRANSPORT_UDPTL;
    s->fill_bit_removal = FALSE;
    s->mmr_transcoding = FALSE;
    s->jbig_transcoding = FALSE;
    s->max_buffer_size = 400;
    s->max_datagram_size = 100;
    s->t38_version = 0;
    s->check_sequence_numbers = TRUE;

    /* Set some defaults */
    s->category_control[T38_PACKET_CATEGORY_INDICATOR] = 1;
    s->category_control[T38_PACKET_CATEGORY_CONTROL_DATA] = 1;
    s->category_control[T38_PACKET_CATEGORY_CONTROL_DATA_END] = 1;
    s->category_control[T38_PACKET_CATEGORY_IMAGE_DATA] = 1;
    s->category_control[T38_PACKET_CATEGORY_IMAGE_DATA_END] = 1;

    /* Set the initial current receive states to something invalid, so the
       first data received is seen as a change of state. */
    s->current_rx_indicator = -1;
    s->current_rx_data_type = -1;
    s->current_rx_field_type = -1;

    /* Set the initial current indicator state to something invalid, so the
       first attempt to send an indicator will work. */
    s->current_tx_indicator = -1;

    s->rx_indicator_handler = rx_indicator_handler;
    s->rx_data_handler = rx_data_handler;
    s->rx_missing_handler = rx_missing_handler;
    s->rx_user_data = rx_user_data;
    s->tx_packet_handler = tx_packet_handler;
    s->tx_packet_user_data = tx_packet_user_data;

    /* We have no initial expectation of the received packet sequence number.
       They most often start at 0 or 1 for a UDPTL transport, but random
       starting numbers are possible. */
    s->rx_expected_seq_no = -1;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_core_release(t38_core_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t38_core_free(t38_core_state_t *s)
{
    if (s)
        free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
