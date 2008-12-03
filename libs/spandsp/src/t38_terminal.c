/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_terminal.c - T.38 termination, less the packet exchange part
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
 * $Id: t38_terminal.c,v 1.101 2008/09/07 12:45:17 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/queue.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/fsk.h"
#include "spandsp/v29rx.h"
#include "spandsp/v29tx.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/v17rx.h"
#include "spandsp/v17tx.h"
#include "spandsp/t4.h"
#include "spandsp/t30_fcf.h"
#include "spandsp/t35.h"
#include "spandsp/t30.h"
#include "spandsp/t30_api.h"
#include "spandsp/t30_logging.h"
#include "spandsp/t38_core.h"
#include "spandsp/t38_terminal.h"

/* Settings suitable for paced transmission over a UDP transport */
#define MS_PER_TX_CHUNK                         30

#define INDICATOR_TX_COUNT                      3
#define DATA_TX_COUNT                           1
#define DATA_END_TX_COUNT                       3

/* Settings suitable for unpaced transmission over a TCP transport */
#define MAX_OCTETS_PER_UNPACED_CHUNK            300

/* Backstop timeout if reception of packets stops in the middle of a burst */
#define MID_RX_TIMEOUT                          15000

#define T38_CHUNKING_MERGE_FCS_WITH_DATA        0x0001
#define T38_CHUNKING_WHOLE_FRAMES               0x0002
#define T38_CHUNKING_ALLOW_TEP_TIME             0x0004

enum
{
    T38_TIMED_STEP_NONE = 0,
    T38_TIMED_STEP_NON_ECM_MODEM,
    T38_TIMED_STEP_NON_ECM_MODEM_2,
    T38_TIMED_STEP_NON_ECM_MODEM_3,
    T38_TIMED_STEP_NON_ECM_MODEM_4,
    T38_TIMED_STEP_NON_ECM_MODEM_5,
    T38_TIMED_STEP_HDLC_MODEM,
    T38_TIMED_STEP_HDLC_MODEM_2,
    T38_TIMED_STEP_HDLC_MODEM_3,
    T38_TIMED_STEP_HDLC_MODEM_4,
    T38_TIMED_STEP_CED,
    T38_TIMED_STEP_CED_2,
    T38_TIMED_STEP_CNG,
    T38_TIMED_STEP_CNG_2,
    T38_TIMED_STEP_PAUSE
};

static int extra_bits_in_stuffed_frame(const uint8_t buf[], int len)
{
    int bitstream;
    int ones;
    int stuffed;
    int i;
    int j;
    
    bitstream = 0;
    ones = 0;
    stuffed = 0;
    /* We should really append the CRC, and included the stuffed bits for that, to get
       the exact number of bits in the frame. */
    //len = crc_itu16_append(buf, len);
    for (i = 0;  i < len;  i++)
    {
        bitstream = buf[i];
        for (j = 0;  j < 8;  j++)
        {
            if ((bitstream & 1))
            {
                if (++ones >= 5)
                {
                    ones = 0;
                    stuffed++;
                }
            }
            else
            {
                ones = 0;
            }
            bitstream >>= 1;
        }
    }
    /* The total length of the frame is:
          the number of bits in the body
        + the number of additional bits in the body due to stuffing
        + the number of bits in the CRC
        + the number of additional bits in the CRC due to stuffing
        + 16 bits for the two terminating flag octets.
       Lets just allow 3 bits for the CRC, which is the worst case. It
       avoids calculating the real CRC, and the worst it can do is cause
       a flag octet's worth of additional output.
    */
    return stuffed + 16 + 3 + 16;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_missing(t38_core_state_t *t, void *user_data, int rx_seq_no, int expected_seq_no)
{
    t38_terminal_state_t *s;
    
    s = (t38_terminal_state_t *) user_data;
    s->t38_fe.rx_data_missing = TRUE;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_indicator(t38_core_state_t *t, void *user_data, int indicator)
{
    t38_terminal_state_t *s;
    t38_terminal_front_end_state_t *fe;
    
    s = (t38_terminal_state_t *) user_data;
    fe = &s->t38_fe;

    if (t->current_rx_indicator == indicator)
    {
        /* This is probably due to the far end repeating itself, or slipping
           preamble messages in between HDLC frames. T.38/V.1.3 tells us to
           ignore it. Its harmless. */
        return 0;
    }
    /* In termination mode we don't care very much about indicators telling us training
       is starting. We only care about V.21 preamble starting, for timeout control, and
       the actual data. */
    switch (indicator)
    {
    case T38_IND_NO_SIGNAL:
        if (t->current_rx_indicator == T38_IND_V21_PREAMBLE
            &&
            (fe->current_rx_type == T30_MODEM_V21  ||  fe->current_rx_type == T30_MODEM_CNG))
        {
            t30_hdlc_accept(&s->t30, NULL, SIG_STATUS_CARRIER_DOWN, TRUE);
        }
        fe->timeout_rx_samples = 0;
        t30_front_end_status(&s->t30, T30_FRONT_END_SIGNAL_ABSENT);
        break;
    case T38_IND_CNG:
    case T38_IND_CED:
        /* We are completely indifferent to the startup tones. They serve no purpose for us.
           We can't even assume that the existance of a tone means the far end is achieving
           proper communication. Some T.38 gateways will just send out a CED or CNG indicator
           without having seen anything from the far end FAX terminal. */
        break;
    case T38_IND_V21_PREAMBLE:
        /* Some T.38 implementations insert these preamble indicators between HDLC frames, so
           we need to be tolerant of that. */
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        t30_front_end_status(&s->t30, T30_FRONT_END_SIGNAL_PRESENT);
        break;
    case T38_IND_V27TER_2400_TRAINING:
    case T38_IND_V27TER_4800_TRAINING:
    case T38_IND_V29_7200_TRAINING:
    case T38_IND_V29_9600_TRAINING:
    case T38_IND_V17_7200_SHORT_TRAINING:
    case T38_IND_V17_7200_LONG_TRAINING:
    case T38_IND_V17_9600_SHORT_TRAINING:
    case T38_IND_V17_9600_LONG_TRAINING:
    case T38_IND_V17_12000_SHORT_TRAINING:
    case T38_IND_V17_12000_LONG_TRAINING:
    case T38_IND_V17_14400_SHORT_TRAINING:
    case T38_IND_V17_14400_LONG_TRAINING:
    case T38_IND_V33_12000_TRAINING:
    case T38_IND_V33_14400_TRAINING:
        /* We really don't care what kind of modem is delivering the following image data.
           We only care that some kind of fast modem signal is coming next. */
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        t30_front_end_status(&s->t30, T30_FRONT_END_SIGNAL_PRESENT);
        break;
    case T38_IND_V8_ANSAM:
    case T38_IND_V8_SIGNAL:
    case T38_IND_V34_CNTL_CHANNEL_1200:
    case T38_IND_V34_PRI_CHANNEL:
    case T38_IND_V34_CC_RETRAIN:
        /* V.34 support is a work in progress. */
        t30_front_end_status(&s->t30, T30_FRONT_END_SIGNAL_PRESENT);
        break;
    default:
        t30_front_end_status(&s->t30, T30_FRONT_END_SIGNAL_ABSENT);
        break;
    }
    fe->hdlc_rx.len = 0;
    fe->rx_data_missing = FALSE;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_data(t38_core_state_t *t, void *user_data, int data_type, int field_type, const uint8_t *buf, int len)
{
    t38_terminal_state_t *s;
    t38_terminal_front_end_state_t *fe;
#if defined(_MSC_VER)
    uint8_t *buf2 = alloca(len);
#else
    uint8_t buf2[len];
#endif

    s = (t38_terminal_state_t *) user_data;
    fe = &s->t38_fe;
#if 0
    /* In termination mode we don't care very much what the data type is. */
    switch (data_type)
    {
    case T38_DATA_V21:
    case T38_DATA_V27TER_2400:
    case T38_DATA_V27TER_4800:
    case T38_DATA_V29_7200:
    case T38_DATA_V29_9600:
    case T38_DATA_V17_7200:
    case T38_DATA_V17_9600:
    case T38_DATA_V17_12000:
    case T38_DATA_V17_14400:
    case T38_DATA_V8:
    case T38_DATA_V34_PRI_RATE:
    case T38_DATA_V34_CC_1200:
    case T38_DATA_V34_PRI_CH:
    case T38_DATA_V33_12000:
    case T38_DATA_V33_14400:
    default:
        break;
    }
#endif

    switch (field_type)
    {
    case T38_FIELD_HDLC_DATA:
        if (fe->timeout_rx_samples == 0)
        {
            /* HDLC can just start without any signal indicator on some platforms, even when
               there is zero packet lost. Nasty, but true. Its a good idea to be tolerant of
               loss, though, so accepting a sudden start of HDLC data is the right thing to do. */
            fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
            t30_front_end_status(&s->t30, T30_FRONT_END_SIGNAL_PRESENT);
            /* All real HDLC messages in the FAX world start with 0xFF. If this one is not starting
               with 0xFF it would appear some octets must have been missed before this one. */
            if (buf[0] != 0xFF)
                fe->rx_data_missing = TRUE;
        }
        if (fe->hdlc_rx.len + len <= T38_MAX_HDLC_LEN)
        {
            bit_reverse(fe->hdlc_rx.buf + fe->hdlc_rx.len, buf, len);
            fe->hdlc_rx.len += len;
        }
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        break;
    case T38_FIELD_HDLC_FCS_OK:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_OK!\n");
            /* The sender has incorrectly included data in this message. It is unclear what we should do
               with it, to maximise tolerance of buggy implementations. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_OK messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC OK (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            t30_hdlc_accept(&s->t30, fe->hdlc_rx.buf, fe->hdlc_rx.len, !fe->rx_data_missing);
        }
        fe->hdlc_rx.len = 0;
        fe->rx_data_missing = FALSE;
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        break;
    case T38_FIELD_HDLC_FCS_BAD:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_BAD!\n");
            /* The sender has incorrectly included data in this message. We can safely ignore it, as the
               bad FCS means we will throw away the whole message, anyway. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_BAD messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC bad (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            t30_hdlc_accept(&s->t30, fe->hdlc_rx.buf, fe->hdlc_rx.len, FALSE);
        }
        fe->hdlc_rx.len = 0;
        fe->rx_data_missing = FALSE;
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        break;
    case T38_FIELD_HDLC_FCS_OK_SIG_END:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_OK_SIG_END!\n");
            /* The sender has incorrectly included data in this message. It is unclear what we should do
               with it, to maximise tolerance of buggy implementations. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_OK_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC OK, sig end (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            t30_hdlc_accept(&s->t30, fe->hdlc_rx.buf, fe->hdlc_rx.len, !fe->rx_data_missing);
            t30_hdlc_accept(&s->t30, NULL, SIG_STATUS_CARRIER_DOWN, TRUE);
        }
        fe->hdlc_rx.len = 0;
        fe->rx_data_missing = FALSE;
        fe->timeout_rx_samples = 0;
        break;
    case T38_FIELD_HDLC_FCS_BAD_SIG_END:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_BAD_SIG_END!\n");
            /* The sender has incorrectly included data in this message. We can safely ignore it, as the
               bad FCS means we will throw away the whole message, anyway. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_BAD_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC bad, sig end (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            t30_hdlc_accept(&s->t30, fe->hdlc_rx.buf, fe->hdlc_rx.len, FALSE);
            t30_hdlc_accept(&s->t30, NULL, SIG_STATUS_CARRIER_DOWN, TRUE);
        }
        fe->hdlc_rx.len = 0;
        fe->rx_data_missing = FALSE;
        fe->timeout_rx_samples = 0;
        break;
    case T38_FIELD_HDLC_SIG_END:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_SIG_END!\n");
            /* The sender has incorrectly included data in this message, but there seems nothing meaningful
               it could be. There could not be an FCS good/bad report beyond this. */
        }
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            /* WORKAROUND: At least some Mediatrix boxes have a bug, where they can send this message at the
                           end of non-ECM data. We need to tolerate this. We use the generic receive complete
                           indication, rather than the specific HDLC carrier down. */
            /* This message is expected under 2 circumstances. One is as an alternative to T38_FIELD_HDLC_FCS_OK_SIG_END - 
               i.e. they send T38_FIELD_HDLC_FCS_OK, and then T38_FIELD_HDLC_SIG_END when the carrier actually drops.
               The other is because the HDLC signal drops unexpectedly - i.e. not just after a final frame. */
            fe->hdlc_rx.len = 0;
            fe->rx_data_missing = FALSE;
            fe->timeout_rx_samples = 0;
            t30_front_end_status(&s->t30, T30_FRONT_END_RECEIVE_COMPLETE);
        }
        break;
    case T38_FIELD_T4_NON_ECM_DATA:
        if (!fe->rx_signal_present)
        {
            t30_non_ecm_put_bit(&s->t30, SIG_STATUS_TRAINING_SUCCEEDED);
            fe->rx_signal_present = TRUE;
        }
        bit_reverse(buf2, buf, len);
        t30_non_ecm_put_chunk(&s->t30, buf2, len);
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        break;
    case T38_FIELD_T4_NON_ECM_SIG_END:
        /* Some T.38 implementations send multiple T38_FIELD_T4_NON_ECM_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            if (len > 0)
            {
                if (!fe->rx_signal_present)
                {
                    t30_non_ecm_put_bit(&s->t30, SIG_STATUS_TRAINING_SUCCEEDED);
                    fe->rx_signal_present = TRUE;
                }
                bit_reverse(buf2, buf, len);
                t30_non_ecm_put_chunk(&s->t30, buf2, len);
            }
            /* WORKAROUND: At least some Mediatrix boxes have a bug, where they can send HDLC signal end where
                           they should send non-ECM signal end. It is possible they also do the opposite.
                           We need to tolerate this, so we use the generic receive complete
                           indication, rather than the specific non-ECM carrier down. */
            t30_front_end_status(&s->t30, T30_FRONT_END_RECEIVE_COMPLETE);
        }
        fe->rx_signal_present = FALSE;
        fe->timeout_rx_samples = 0;
        break;
    case T38_FIELD_CM_MESSAGE:
        if (len >= 1)
            span_log(&s->logging, SPAN_LOG_FLOW, "CM profile %d - %s\n", buf[0] - '0', t38_cm_profile_to_str(buf[0]));
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for CM message - %d\n", len);
        break;
    case T38_FIELD_JM_MESSAGE:
        if (len >= 2)
            span_log(&s->logging, SPAN_LOG_FLOW, "JM - %s\n", t38_jm_to_str(buf, len));
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for JM message - %d\n", len);
        break;
    case T38_FIELD_CI_MESSAGE:
        if (len >= 1)
            span_log(&s->logging, SPAN_LOG_FLOW, "CI 0x%X\n", buf[0]);
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for CI message - %d\n", len);
        break;
    case T38_FIELD_V34RATE:
        if (len >= 3)
        {
            fe->t38.v34_rate = t38_v34rate_to_bps(buf, len);
            span_log(&s->logging, SPAN_LOG_FLOW, "V.34 rate %d bps\n", fe->t38.v34_rate);
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for V34rate message - %d\n", len);
        }
        break;
    default:
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void send_hdlc(void *user_data, const uint8_t *msg, int len)
{
    t38_terminal_state_t *s;

    s = (t38_terminal_state_t *) user_data;
    if (len <= 0)
    {
        s->t38_fe.tx.len = -1;
    }
    else
    {
        s->t38_fe.tx.extra_bits = extra_bits_in_stuffed_frame(msg, len);
        bit_reverse(s->t38_fe.tx.buf, msg, len);
        s->t38_fe.tx.len = len;
        s->t38_fe.tx.ptr = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ int bits_to_samples(t38_terminal_state_t *s, int bits)
{
    /* This does not handle fractions properly, so they may accumulate. They
       shouldn't be able to accumulate far enough to be troublesome. */
    /* TODO: Is the above statement accurate when sending a long string of
             octets, one per IFP packet, at V.21 rate? */
    if (s->t38_fe.ms_per_tx_chunk == 0)
        return 0;
    return bits*8000/s->t38_fe.tx_bit_rate;
}
/*- End of function --------------------------------------------------------*/

static void set_octets_per_data_packet(t38_terminal_state_t *s, int bit_rate)
{
    s->t38_fe.tx_bit_rate = bit_rate;
    if (s->t38_fe.ms_per_tx_chunk == 0)
    {
        s->t38_fe.octets_per_data_packet = MAX_OCTETS_PER_UNPACED_CHUNK;
    }
    else
    {
        s->t38_fe.octets_per_data_packet = s->t38_fe.ms_per_tx_chunk*bit_rate/(8*1000);
        /* Make sure we have a positive number (i.e. we didn't truncate to zero). */
        if (s->t38_fe.octets_per_data_packet < 1)
            s->t38_fe.octets_per_data_packet = 1;
    }
}
/*- End of function --------------------------------------------------------*/

int t38_terminal_send_timeout(t38_terminal_state_t *s, int samples)
{
    int len;
    int i;
    int previous;
    uint8_t buf[MAX_OCTETS_PER_UNPACED_CHUNK + 50];
    t38_terminal_front_end_state_t *fe;
    t38_data_field_t data_fields[2];
    /* The times for training, the optional TEP, and the HDLC preamble, for all the modem options, in ms.
       Note that the preamble for V.21 is 1s+-15%, and for the other modems is 200ms+100ms. */
    static const struct
    {
        int tep;
        int training;
        int flags;
    } startup_time[] =
    {
        {   0,    0,    0},     /* T38_IND_NO_SIGNAL */
        {   0,    0,    0},     /* T38_IND_CNG */
        {   0,    0,    0},     /* T38_IND_CED */
        {   0,    0, 1000},     /* T38_IND_V21_PREAMBLE */ /* TODO: 850ms should be OK for this, but it causes trouble with some ATAs. Why? */
        { 215,  943,  200},     /* T38_IND_V27TER_2400_TRAINING */
        { 215,  708,  200},     /* T38_IND_V27TER_4800_TRAINING */
        { 215,  234,  200},     /* T38_IND_V29_7200_TRAINING */
        { 215,  234,  200},     /* T38_IND_V29_9600_TRAINING */
        { 215,  142,  200},     /* T38_IND_V17_7200_SHORT_TRAINING */
        { 215, 1393,  200},     /* T38_IND_V17_7200_LONG_TRAINING */
        { 215,  142,  200},     /* T38_IND_V17_9600_SHORT_TRAINING */
        { 215, 1393,  200},     /* T38_IND_V17_9600_LONG_TRAINING */
        { 215,  142,  200},     /* T38_IND_V17_12000_SHORT_TRAINING */
        { 215, 1393,  200},     /* T38_IND_V17_12000_LONG_TRAINING */
        { 215,  142,  200},     /* T38_IND_V17_14400_SHORT_TRAINING */
        { 215, 1393,  200},     /* T38_IND_V17_14400_LONG_TRAINING */
        { 215,    0,    0},     /* T38_IND_V8_ANSAM */
        { 215,    0,    0},     /* T38_IND_V8_SIGNAL */
        { 215,    0,    0},     /* T38_IND_V34_CNTL_CHANNEL_1200 */
        { 215,    0,    0},     /* T38_IND_V34_PRI_CHANNEL */
        { 215,    0,    0},     /* T38_IND_V34_CC_RETRAIN */
        { 215,    0,    0},     /* T38_IND_V33_12000_TRAINING */
        { 215,    0,    0}      /* T38_IND_V33_14400_TRAINING */
    };

    fe = &s->t38_fe;
    if (fe->current_rx_type == T30_MODEM_DONE  ||  fe->current_tx_type == T30_MODEM_DONE)
        return TRUE;

    fe->samples += samples;
    t30_timer_update(&s->t30, samples);
    if (fe->timeout_rx_samples  &&  fe->samples > fe->timeout_rx_samples)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Timeout mid-receive\n");
        fe->timeout_rx_samples = 0;
        t30_front_end_status(&s->t30, T30_FRONT_END_RECEIVE_COMPLETE);
    }
    if (fe->timed_step == T38_TIMED_STEP_NONE)
        return FALSE;
    /* Wait until the right time comes along, unless we are working in "no delays" mode, while talking to an
       IAF terminal. */
    if (fe->ms_per_tx_chunk  &&  fe->samples < fe->next_tx_samples)
        return FALSE;
    /* Its time to send something */
    switch (fe->timed_step)
    {
    case T38_TIMED_STEP_NON_ECM_MODEM:
        /* Create a 75ms silence */
        if (fe->t38.current_tx_indicator != T38_IND_NO_SIGNAL)
            t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL, fe->t38.indicator_tx_count);
        fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_2;
        fe->next_tx_samples += ms_to_samples(75);
        break;
    case T38_TIMED_STEP_NON_ECM_MODEM_2:
        /* Switch on a fast modem, and give the training time to complete */
        t38_core_send_indicator(&fe->t38, fe->next_tx_indicator, fe->t38.indicator_tx_count);
        fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_3;
        fe->next_tx_samples += ms_to_samples(startup_time[fe->next_tx_indicator].training);
        if ((fe->chunking_modes & T38_CHUNKING_ALLOW_TEP_TIME))
            fe->next_tx_samples += ms_to_samples(startup_time[fe->next_tx_indicator].tep);
        break;
    case T38_TIMED_STEP_NON_ECM_MODEM_3:
        /* Send a chunk of non-ECM image data */
        /* T.38 says it is OK to send the last of the non-ECM data in the signal end message.
           However, I think the early versions of T.38 said the signal end message should not
           contain data. Hopefully, following the current spec will not cause compatibility
           issues. */
        len = t30_non_ecm_get_chunk(&s->t30, buf, fe->octets_per_data_packet);
        bit_reverse(buf, buf, len);
        if (len < fe->octets_per_data_packet)
        {
            /* That's the end of the image data. Do a little padding now */
            memset(buf + len, 0, fe->octets_per_data_packet - len);
            fe->non_ecm_trailer_bytes = 3*fe->octets_per_data_packet + len;
            len = fe->octets_per_data_packet;
            fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_4;
        }
        t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_DATA, buf, len, fe->t38.data_tx_count);
        fe->next_tx_samples += bits_to_samples(s, 8*len);
        break;
    case T38_TIMED_STEP_NON_ECM_MODEM_4:
        /* This pads the end of the data with some zeros. If we just stop abruptly
           at the end of the EOLs, some ATAs fail to clean up properly before
           shutting down their transmit modem, and the last few rows of the image
           get corrupted. Simply delaying the no-signal message does not help for
           all implentations. It often appears to be ignored. */
        len = fe->octets_per_data_packet;
        fe->non_ecm_trailer_bytes -= len;
        if (fe->non_ecm_trailer_bytes <= 0)
        {
            len += fe->non_ecm_trailer_bytes;
            memset(buf, 0, len);
            t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_SIG_END, buf, len, fe->t38.data_end_tx_count);
            fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_5;
            /* Allow a bit more time than the data will take to play out, to ensure the far ATA does not
               cut things short. */
            fe->next_tx_samples += (bits_to_samples(s, 8*len) + ms_to_samples(60));
            break;
        }
        memset(buf, 0, len);
        t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_DATA, buf, len, fe->t38.data_tx_count);
        fe->next_tx_samples += bits_to_samples(s, 8*len);
        break;
    case T38_TIMED_STEP_NON_ECM_MODEM_5:
        /* This should not be needed, since the message above indicates the end of the signal, but it
           seems like it can improve compatibility with quirky implementations. */
        t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL, fe->t38.indicator_tx_count);
        fe->timed_step = T38_TIMED_STEP_NONE;
        t30_front_end_status(&s->t30, T30_FRONT_END_SEND_STEP_COMPLETE);
        break;
    case T38_TIMED_STEP_HDLC_MODEM:
        /* Send HDLC preambling */
        t38_core_send_indicator(&fe->t38, fe->next_tx_indicator, fe->t38.indicator_tx_count);
        fe->next_tx_samples += ms_to_samples(startup_time[fe->next_tx_indicator].training + startup_time[fe->next_tx_indicator].flags);
        if (fe->chunking_modes & T38_CHUNKING_ALLOW_TEP_TIME)
            fe->next_tx_samples += ms_to_samples(startup_time[fe->next_tx_indicator].tep);
        fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_2;
        break;
    case T38_TIMED_STEP_HDLC_MODEM_2:
        /* Send a chunk of HDLC data */
        i = fe->tx.len - fe->tx.ptr;
        if (fe->octets_per_data_packet >= i)
        {
            /* The last part of an HDLC frame */
            if (fe->chunking_modes & T38_CHUNKING_MERGE_FCS_WITH_DATA)
            {
                /* Copy the data, as we might be about to refill the buffer it is in */
                memcpy(buf, &fe->tx.buf[fe->tx.ptr], i);
                data_fields[0].field_type = T38_FIELD_HDLC_DATA;
                data_fields[0].field = buf;
                data_fields[0].field_len = i;

                /* Now see about the next HDLC frame. This will tell us whether to send FCS_OK or FCS_OK_SIG_END */
                previous = fe->current_tx_data_type;
                fe->tx.ptr = 0;
                fe->tx.len = 0;
                t30_front_end_status(&s->t30, T30_FRONT_END_SEND_STEP_COMPLETE);
                /* The above step should have got the next HDLC step ready - either another frame, or an instruction to stop transmission. */
                if (fe->tx.len < 0)
                {
                    data_fields[1].field_type = T38_FIELD_HDLC_FCS_OK_SIG_END;
                    fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_4;
                    /* We add a bit of extra time here, as with some implementations
                       the carrier falling too abruptly causes data loss. */
                    fe->next_tx_samples += (bits_to_samples(s, i*8 + fe->tx.extra_bits) + ms_to_samples(100));
                }
                else
                {
                    data_fields[1].field_type = T38_FIELD_HDLC_FCS_OK;
                    fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_2;
                    fe->next_tx_samples += bits_to_samples(s, i*8 + fe->tx.extra_bits);
                }
                data_fields[1].field = NULL;
                data_fields[1].field_len = 0;
                t38_core_send_data_multi_field(&fe->t38, fe->current_tx_data_type, data_fields, 2, fe->t38.data_tx_count);
            }
            else
            {
                t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_HDLC_DATA, &fe->tx.buf[fe->tx.ptr], i, fe->t38.data_tx_count);
                fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_3;
                fe->next_tx_samples += bits_to_samples(s, i*8);
            }
            break;
        }
        t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_HDLC_DATA, &fe->tx.buf[fe->tx.ptr], fe->octets_per_data_packet, fe->t38.data_tx_count);
        fe->tx.ptr += fe->octets_per_data_packet;
        fe->next_tx_samples += bits_to_samples(s, fe->octets_per_data_packet*8);
        break;
    case T38_TIMED_STEP_HDLC_MODEM_3:
        /* End of HDLC frame */
        previous = fe->current_tx_data_type;
        fe->tx.ptr = 0;
        fe->tx.len = 0;
        t30_front_end_status(&s->t30, T30_FRONT_END_SEND_STEP_COMPLETE);
        /* The above step should have got the next HDLC step ready - either another frame, or an instruction to stop transmission. */
        if (fe->tx.len < 0)
        {
            /* End of transmission */
            t38_core_send_data(&fe->t38, previous, T38_FIELD_HDLC_FCS_OK_SIG_END, NULL, 0, fe->t38.data_end_tx_count);
            fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_4;
            fe->next_tx_samples += (bits_to_samples(s, fe->tx.extra_bits) + ms_to_samples(100));
            break;
        }
        if (fe->tx.len == 0)
        {
            /* Now, how did we get here? We have finished a frame, but have no new frame to
               send, and no end of transmission condition. */
            span_log(&s->logging, SPAN_LOG_FLOW, "No new frame or end transmission condition.\n");
        }
        /* Finish the current frame off, and prepare for the next one. */
        t38_core_send_data(&fe->t38, previous, T38_FIELD_HDLC_FCS_OK, NULL, 0, fe->t38.data_tx_count);
        fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_2;
        /* We should now wait 3 octet times - the duration of the FCS + a flag octet - and send the
           next chunk. To give a little more latitude, and allow for stuffing in the FCS, add
           time for an extra flag octet. */
        fe->next_tx_samples += bits_to_samples(s, fe->tx.extra_bits);
        break;
    case T38_TIMED_STEP_HDLC_MODEM_4:
        /* Note that some boxes do not like us sending a T38_FIELD_HDLC_SIG_END at this point.
           A T38_IND_NO_SIGNAL should always be OK. */
        t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL, fe->t38.indicator_tx_count);
        fe->timed_step = T38_TIMED_STEP_NONE;
        t30_front_end_status(&s->t30, T30_FRONT_END_SEND_STEP_COMPLETE);
        break;
    case T38_TIMED_STEP_CED:
        /* It seems common practice to start with a no signal indicator, though
           this is not a specified requirement. Since we should be sending 200ms
           of silence, starting the delay with a no signal indication makes sense.
           We do need a 200ms delay, as that is a specification requirement. */
        fe->timed_step = T38_TIMED_STEP_CED_2;
        fe->next_tx_samples = fe->samples + ms_to_samples(200);
        t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL, fe->t38.indicator_tx_count);
        fe->current_tx_data_type = T38_DATA_NONE;
        break;
    case T38_TIMED_STEP_CED_2:
        /* Initial 200ms delay over. Send the CED indicator */
        fe->next_tx_samples = fe->samples + ms_to_samples(3000);
        fe->timed_step = T38_TIMED_STEP_PAUSE;
        t38_core_send_indicator(&fe->t38, T38_IND_CED, fe->t38.indicator_tx_count);
        fe->current_tx_data_type = T38_DATA_NONE;
        break;
    case T38_TIMED_STEP_CNG:
        /* It seems common practice to start with a no signal indicator, though
           this is not a specified requirement. Since we should be sending 200ms
           of silence, starting the delay with a no signal indication makes sense.
           We do need a 200ms delay, as that is a specification requirement. */
        fe->timed_step = T38_TIMED_STEP_CNG_2;
        fe->next_tx_samples = fe->samples + ms_to_samples(200);
        t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL, fe->t38.indicator_tx_count);
        fe->current_tx_data_type = T38_DATA_NONE;
        break;
    case T38_TIMED_STEP_CNG_2:
        /* Initial short delay over. Send the CNG indicator */
        fe->timed_step = T38_TIMED_STEP_NONE;
        t38_core_send_indicator(&fe->t38, T38_IND_CNG, fe->t38.indicator_tx_count);
        fe->current_tx_data_type = T38_DATA_NONE;
        break;
    case T38_TIMED_STEP_PAUSE:
        /* End of timed pause */
        fe->timed_step = T38_TIMED_STEP_NONE;
        t30_front_end_status(&s->t30, T30_FRONT_END_SEND_STEP_COMPLETE);
        break;
    }
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

static void set_rx_type(void *user_data, int type, int bit_rate, int short_train, int use_hdlc)
{
    t38_terminal_state_t *s;

    s = (t38_terminal_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set rx type %d\n", type);
    s->t38_fe.current_rx_type = type;
}
/*- End of function --------------------------------------------------------*/

static void set_tx_type(void *user_data, int type, int bit_rate, int short_train, int use_hdlc)
{
    t38_terminal_state_t *s;

    s = (t38_terminal_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set tx type %d\n", type);
    if (s->t38_fe.current_tx_type == type)
        return;

    set_octets_per_data_packet(s, bit_rate);
    switch (type)
    {
    case T30_MODEM_NONE:
        s->t38_fe.timed_step = T38_TIMED_STEP_NONE;
        s->t38_fe.current_tx_data_type = T38_DATA_NONE;
        break;
    case T30_MODEM_PAUSE:
        s->t38_fe.next_tx_samples = s->t38_fe.samples + ms_to_samples(short_train);
        s->t38_fe.timed_step = T38_TIMED_STEP_PAUSE;
        s->t38_fe.current_tx_data_type = T38_DATA_NONE;
        break;
    case T30_MODEM_CED:
    case T30_MODEM_CNG:
        if (type == T30_MODEM_CED)
        {
            /* A 200ms initial delay is specified. Delay this amount before the CED indicator is sent. */
            s->t38_fe.next_tx_samples = s->t38_fe.samples;
            s->t38_fe.timed_step = T38_TIMED_STEP_CED;
        }
        else
        {
            /* Allow a short initial delay, so the chances of the other end actually being ready to receive
               the CNG indicator are improved. */
            s->t38_fe.next_tx_samples = s->t38_fe.samples;
            s->t38_fe.timed_step = T38_TIMED_STEP_CNG;
        }
        s->t38_fe.current_tx_data_type = T38_DATA_NONE;
        break;
    case T30_MODEM_V21:
        if (s->t38_fe.current_tx_type > T30_MODEM_V21)
        {
            /* Pause before switching from phase C, as per T.30. If we omit this, the receiver
               might not see the carrier fall between the high speed and low speed sections. */
            s->t38_fe.next_tx_samples = s->t38_fe.samples + ms_to_samples(75);
        }
        else
        {
            s->t38_fe.next_tx_samples = s->t38_fe.samples;
        }
        s->t38_fe.next_tx_indicator = T38_IND_V21_PREAMBLE;
        s->t38_fe.current_tx_data_type = T38_DATA_V21;
        s->t38_fe.timed_step = (use_hdlc)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        break;
    case T30_MODEM_V27TER:
        switch (bit_rate)
        {
        case 2400:
            s->t38_fe.next_tx_indicator = T38_IND_V27TER_2400_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V27TER_2400;
            break;
        case 4800:
            s->t38_fe.next_tx_indicator = T38_IND_V27TER_4800_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V27TER_4800;
            break;
        }
        s->t38_fe.timed_step = (use_hdlc)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        s->t38_fe.next_tx_samples = s->t38_fe.samples + ms_to_samples(30);
        break;
    case T30_MODEM_V29:
        switch (bit_rate)
        {
        case 7200:
            s->t38_fe.next_tx_indicator = T38_IND_V29_7200_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V29_7200;
            break;
        case 9600:
            s->t38_fe.next_tx_indicator = T38_IND_V29_9600_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V29_9600;
            break;
        }
        s->t38_fe.timed_step = (use_hdlc)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        s->t38_fe.next_tx_samples = s->t38_fe.samples + ms_to_samples(30);
        break;
    case T30_MODEM_V17:
        switch (bit_rate)
        {
        case 7200:
            s->t38_fe.next_tx_indicator = (short_train)  ?  T38_IND_V17_7200_SHORT_TRAINING  :  T38_IND_V17_7200_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_7200;
            break;
        case 9600:
            s->t38_fe.next_tx_indicator = (short_train)  ?  T38_IND_V17_9600_SHORT_TRAINING  :  T38_IND_V17_9600_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_9600;
            break;
        case 12000:
            s->t38_fe.next_tx_indicator = (short_train)  ?  T38_IND_V17_12000_SHORT_TRAINING  :  T38_IND_V17_12000_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_12000;
            break;
        case 14400:
            s->t38_fe.next_tx_indicator = (short_train)  ?  T38_IND_V17_14400_SHORT_TRAINING  :  T38_IND_V17_14400_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_14400;
            break;
        }
        s->t38_fe.timed_step = (use_hdlc)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        s->t38_fe.next_tx_samples = s->t38_fe.samples + ms_to_samples(30);
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
        s->t38_fe.timed_step = T38_TIMED_STEP_NONE;
        s->t38_fe.current_tx_data_type = T38_DATA_NONE;
        break;
    }
    s->t38_fe.current_tx_type = type;
}
/*- End of function --------------------------------------------------------*/

void t38_terminal_set_config(t38_terminal_state_t *s, int without_pacing)
{
    if (without_pacing)
    {
        /* Continuous streaming mode, as used for TPKT over TCP transport */
        /* Inhibit indicator packets */
        s->t38_fe.t38.indicator_tx_count = 0;
        s->t38_fe.t38.data_tx_count = 1;
        s->t38_fe.t38.data_end_tx_count = 1;
        s->t38_fe.ms_per_tx_chunk = 0;
    }
    else
    {
        /* Paced streaming mode, as used for UDP transports */
        s->t38_fe.t38.indicator_tx_count = INDICATOR_TX_COUNT;
        s->t38_fe.t38.data_tx_count = DATA_TX_COUNT;
        s->t38_fe.t38.data_end_tx_count = DATA_END_TX_COUNT;
        s->t38_fe.ms_per_tx_chunk = MS_PER_TX_CHUNK;
    }
    set_octets_per_data_packet(s, 300);
}
/*- End of function --------------------------------------------------------*/

void t38_terminal_set_tep_mode(t38_terminal_state_t *s, int use_tep)
{
    if (use_tep)
        s->t38_fe.chunking_modes |= T38_CHUNKING_ALLOW_TEP_TIME;
    else
        s->t38_fe.chunking_modes &= ~T38_CHUNKING_ALLOW_TEP_TIME;
}
/*- End of function --------------------------------------------------------*/

void t38_terminal_set_fill_bit_removal(t38_terminal_state_t *s, int remove)
{
    if (remove)
        s->t38_fe.iaf |= T30_IAF_MODE_NO_FILL_BITS;
    else
        s->t38_fe.iaf &= ~T30_IAF_MODE_NO_FILL_BITS;
    t30_set_iaf_mode(&s->t30, s->t38_fe.iaf);
}
/*- End of function --------------------------------------------------------*/

t30_state_t *t38_terminal_get_t30_state(t38_terminal_state_t *s)
{
    return &s->t30;
}
/*- End of function --------------------------------------------------------*/

static int t38_terminal_t38_fe_init(t38_terminal_state_t *t,
                                    t38_tx_packet_handler_t *tx_packet_handler,
                                    void *tx_packet_user_data)
{
    t38_terminal_front_end_state_t *s;
    
    s = &t->t38_fe;
    t38_core_init(&s->t38,
                  process_rx_indicator,
                  process_rx_data,
                  process_rx_missing,
                  (void *) t,
                  tx_packet_handler,
                  tx_packet_user_data);
    s->rx_signal_present = FALSE;
    s->timed_step = T38_TIMED_STEP_NONE;
    s->tx.ptr = 0;
    s->iaf = T30_IAF_MODE_T37 | T30_IAF_MODE_T38;

    s->current_tx_data_type = T38_DATA_NONE;
    s->next_tx_samples = 0;
    s->chunking_modes = T38_CHUNKING_ALLOW_TEP_TIME;

    s->t38.fastest_image_data_rate = 14400;

    return 0;
}
/*- End of function --------------------------------------------------------*/

t38_terminal_state_t *t38_terminal_init(t38_terminal_state_t *s,
                                        int calling_party,
                                        t38_tx_packet_handler_t *tx_packet_handler,
                                        void *tx_packet_user_data)
{
    if (tx_packet_handler == NULL)
        return NULL;

    if (s == NULL)
    {
        if ((s = (t38_terminal_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.38T");

    t38_terminal_t38_fe_init(s, tx_packet_handler, tx_packet_user_data);

    t38_terminal_set_config(s, FALSE);

    t30_init(&s->t30,
             calling_party,
             set_rx_type,
             (void *) s,
             set_tx_type,
             (void *) s,
             send_hdlc,
             (void *) s);
    t30_set_iaf_mode(&s->t30, s->t38_fe.iaf);
    t30_set_supported_modems(&s->t30,
                             T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17 | T30_SUPPORT_IAF);
    t30_restart(&s->t30);
    return s;
}
/*- End of function --------------------------------------------------------*/

int t38_terminal_release(t38_terminal_state_t *s)
{
    t30_release(&s->t30);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t38_terminal_free(t38_terminal_state_t *s)
{
    t30_release(&s->t30);
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
