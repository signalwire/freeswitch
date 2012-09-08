/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t31.c - A T.31 compatible class 1 FAX modem interface.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Special thanks to Lee Howard <faxguy@howardsilvan.com>
 * for his great work debugging and polishing this code.
 *
 * Copyright (C) 2004, 2005, 2006, 2008 Steve Underwood
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
#include <memory.h>
#include <string.h>
#include <ctype.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <assert.h>
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/bitstream.h"
#include "spandsp/dc_restore.h"
#include "spandsp/queue.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/crc.h"
#include "spandsp/hdlc.h"
#include "spandsp/silence_gen.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/fsk.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/v8.h"
#include "spandsp/v29tx.h"
#include "spandsp/v29rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v17tx.h"
#include "spandsp/v17rx.h"
#if defined(SPANDSP_SUPPORT_V34)
#include "spandsp/v34.h"
#endif
#include "spandsp/timezone.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/t30.h"
#include "spandsp/t30_logging.h"
#include "spandsp/t38_core.h"

#include "spandsp/at_interpreter.h"
#include "spandsp/fax_modems.h"
#include "spandsp/t31.h"
#include "spandsp/t30_fcf.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/bitstream.h"
#include "spandsp/private/t38_core.h"
#include "spandsp/private/silence_gen.h"
#include "spandsp/private/fsk.h"
#include "spandsp/private/modem_connect_tones.h"
#include "spandsp/private/v8.h"
#if defined(SPANDSP_SUPPORT_V34)
#include "spandsp/private/v34.h"
#endif
#include "spandsp/private/v17tx.h"
#include "spandsp/private/v17rx.h"
#include "spandsp/private/v27ter_tx.h"
#include "spandsp/private/v27ter_rx.h"
#include "spandsp/private/v29tx.h"
#include "spandsp/private/v29rx.h"
#include "spandsp/private/hdlc.h"
#include "spandsp/private/fax_modems.h"
#include "spandsp/private/at_interpreter.h"
#include "spandsp/private/t31.h"

/* Settings suitable for paced transmission over a UDP transport */
/*! The default number of milliseconds per transmitted IFP when sending bulk T.38 data */
#define MS_PER_TX_CHUNK                         30
/*! The number of transmissions of indicator IFP packets */
#define INDICATOR_TX_COUNT                      3
/*! The number of transmissions of data IFP packets */
#define DATA_TX_COUNT                           1
/*! The number of transmissions of terminating data IFP packets */
#define DATA_END_TX_COUNT                       3
/*! The default DTE timeout, in seconds */
#define DEFAULT_DTE_TIMEOUT                     5

/* Settings suitable for unpaced transmission over a TCP transport */
#define MAX_OCTETS_PER_UNPACED_CHUNK            300

/* Backstop timeout if reception of packets stops in the middle of a burst */
#define MID_RX_TIMEOUT                          15000

#define HDLC_FRAMING_OK_THRESHOLD               5

typedef const char *(*at_cmd_service_t)(t31_state_t *s, const char *cmd);

enum
{
    ETX = 0x03,
    DLE = 0x10,
    SUB = 0x1A
};

enum
{
    DISBIT1 = 0x01,
    DISBIT2 = 0x02,
    DISBIT3 = 0x04,
    DISBIT4 = 0x08,
    DISBIT5 = 0x10,
    DISBIT6 = 0x20,
    DISBIT7 = 0x40,
    DISBIT8 = 0x80
};

enum
{
    T38_CHUNKING_MERGE_FCS_WITH_DATA    = 0x0001,
    T38_CHUNKING_WHOLE_FRAMES           = 0x0002,
    T38_CHUNKING_ALLOW_TEP_TIME         = 0x0004
};

enum
{
    T38_TIMED_STEP_NONE = 0,
    T38_TIMED_STEP_NON_ECM_MODEM = 0x10,
    T38_TIMED_STEP_NON_ECM_MODEM_2 = 0x11,
    T38_TIMED_STEP_NON_ECM_MODEM_3 = 0x12,
    T38_TIMED_STEP_NON_ECM_MODEM_4 = 0x13,
    T38_TIMED_STEP_NON_ECM_MODEM_5 = 0x14,
    T38_TIMED_STEP_HDLC_MODEM = 0x20,
    T38_TIMED_STEP_HDLC_MODEM_2 = 0x21,
    T38_TIMED_STEP_HDLC_MODEM_3 = 0x22,
    T38_TIMED_STEP_HDLC_MODEM_4 = 0x23,
    T38_TIMED_STEP_HDLC_MODEM_5 = 0x24,
    T38_TIMED_STEP_FAKE_HDLC_MODEM = 0x30,
    T38_TIMED_STEP_FAKE_HDLC_MODEM_2 = 0x31,
    T38_TIMED_STEP_FAKE_HDLC_MODEM_3 = 0x32,
    T38_TIMED_STEP_FAKE_HDLC_MODEM_4 = 0x33,
    T38_TIMED_STEP_FAKE_HDLC_MODEM_5 = 0x34,
    T38_TIMED_STEP_CED = 0x40,
    T38_TIMED_STEP_CED_2 = 0x41,
    T38_TIMED_STEP_CED_3 = 0x42,
    T38_TIMED_STEP_CNG = 0x50,
    T38_TIMED_STEP_CNG_2 = 0x51,
    T38_TIMED_STEP_PAUSE = 0x60
};

static int restart_modem(t31_state_t *s, int new_modem);
static void hdlc_accept_frame(void *user_data, const uint8_t *msg, int len, int ok);
static int silence_rx(void *user_data, const int16_t amp[], int len);
static int cng_rx(void *user_data, const int16_t amp[], int len);
static void non_ecm_put_bit(void *user_data, int bit);
static void non_ecm_put(void *user_data, const uint8_t buf[], int len);
static int non_ecm_get(void *user_data, uint8_t buf[], int len);
static void non_ecm_rx_status(void *user_data, int status);
static void hdlc_rx_status(void *user_data, int status);

static __inline__ void t31_set_at_rx_mode(t31_state_t *s, int new_mode)
{
    s->at_state.at_rx_mode = new_mode;
}
/*- End of function --------------------------------------------------------*/

#if 0
static void monitor_control_messages(t31_state_t *s, const uint8_t *buf, int len)
{
    /* Monitor the control messages, at the point where we have the whole message, so we can
       see what is happening to things like training success/failure. */
    span_log(&s->logging, SPAN_LOG_FLOW, "Monitoring %s\n", t30_frametype(buf[2]));
    if (len < 3)
        return;
    /*endif*/
    switch (buf[2])
    {
    case T30_DCS:
    case T30_DCS | 1:
        /* We need to know if ECM is about to be used, so we can fake HDLC stuff. */
        s->t38_fe.ecm_mode = (len >= 7)  &&  (buf[6] & DISBIT3);
        break;
    default:
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/
#endif

static void front_end_status(t31_state_t *s, int status)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Front end status %d\n", status);
    switch (status)
    {
    case T30_FRONT_END_SEND_STEP_COMPLETE:
        switch (s->modem)
        {
        case FAX_MODEM_SILENCE_TX:
            s->modem = FAX_MODEM_NONE;
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
            if (s->at_state.do_hangup)
            {
                at_modem_control(&s->at_state, AT_MODEM_CONTROL_HANGUP, NULL);
                t31_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
                s->at_state.do_hangup = FALSE;
            }
            else
            {
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            }
            break;
        case FAX_MODEM_CED_TONE:
            /* Go directly to V.21/HDLC transmit. */
            s->modem = FAX_MODEM_NONE;
            restart_modem(s, FAX_MODEM_V21_TX);
            t31_set_at_rx_mode(s, AT_MODE_HDLC);
            break;
        case FAX_MODEM_V21_TX:
        case FAX_MODEM_V17_TX:
        case FAX_MODEM_V27TER_TX:
        case FAX_MODEM_V29_TX:
            s->modem = FAX_MODEM_NONE;
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
            t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            restart_modem(s, FAX_MODEM_SILENCE_TX);
            break;
        }
        break;
    case T30_FRONT_END_RECEIVE_COMPLETE:
        break;
    }
}
/*- End of function --------------------------------------------------------*/

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
    /* We should really append the CRC, and include the stuffed bits for that, to get
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
    t31_state_t *s;
    
    s = (t31_state_t *) user_data;
    s->t38_fe.rx_data_missing = TRUE;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_indicator(t38_core_state_t *t, void *user_data, int indicator)
{
    t31_state_t *s;
    t31_t38_front_end_state_t *fe;

    s = (t31_state_t *) user_data;
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
            hdlc_rx_status(s, SIG_STATUS_CARRIER_DOWN);
        }
        fe->timeout_rx_samples = 0;
        front_end_status(s, T30_FRONT_END_SIGNAL_ABSENT);
        break;
    case T38_IND_CNG:
        front_end_status(s, T30_FRONT_END_CNG_PRESENT);
        break;
    case T38_IND_CED:
        front_end_status(s, T30_FRONT_END_CED_PRESENT);
        break;
    case T38_IND_V21_PREAMBLE:
        /* Some T.38 implementations insert these preamble indicators between HDLC frames, so
           we need to be tolerant of that. */
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        front_end_status(s, T30_FRONT_END_SIGNAL_PRESENT);
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
        front_end_status(s, T30_FRONT_END_SIGNAL_PRESENT);
        break;
    case T38_IND_V8_ANSAM:
    case T38_IND_V8_SIGNAL:
    case T38_IND_V34_CNTL_CHANNEL_1200:
    case T38_IND_V34_PRI_CHANNEL:
    case T38_IND_V34_CC_RETRAIN:
        /* V.34 support is a work in progress. */
        front_end_status(s, T30_FRONT_END_SIGNAL_PRESENT);
        break;
    default:
        front_end_status(s, T30_FRONT_END_SIGNAL_ABSENT);
        break;
    }
    fe->hdlc_rx.len = 0;
    fe->rx_data_missing = FALSE;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_data(t38_core_state_t *t, void *user_data, int data_type, int field_type, const uint8_t *buf, int len)
{
    t31_state_t *s;
    t31_t38_front_end_state_t *fe;
#if defined(_MSC_VER)
    uint8_t *buf2 = (uint8_t *) _alloca(len);
#else
    uint8_t buf2[len];
#endif

    s = (t31_state_t *) user_data;
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
            front_end_status(s, T30_FRONT_END_SIGNAL_PRESENT);
            /* All real HDLC messages in the FAX world start with 0xFF. If this one is not starting
               with 0xFF it would appear some octets must have been missed before this one. */
            if (len <= 0  ||  buf[0] != 0xFF)
                fe->rx_data_missing = TRUE;
            /*endif*/
        }
        /*endif*/
        if (len > 0)
        {
            if (fe->hdlc_rx.len + len <= T31_T38_MAX_HDLC_LEN)
            {
                bit_reverse(fe->hdlc_rx.buf + fe->hdlc_rx.len, buf, len);
                fe->hdlc_rx.len += len;
            }
            else
            {
                fe->rx_data_missing = TRUE;
            }
            /*endif*/
        }
        /*endif*/
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        break;
    case T38_FIELD_HDLC_FCS_OK:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_OK!\n");
            /* The sender has incorrectly included data in this message. It is unclear what we should do
               with it, to maximise tolerance of buggy implementations. */
        }
        /*endif*/
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_OK messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC OK (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            crc_itu16_append(fe->hdlc_rx.buf, fe->hdlc_rx.len);
            hdlc_accept_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, !fe->rx_data_missing);
        }
        /*endif*/
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
        /*endif*/
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_BAD messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC bad (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            hdlc_accept_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, FALSE);
        }
        /*endif*/
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
            crc_itu16_append(fe->hdlc_rx.buf, fe->hdlc_rx.len);
            hdlc_accept_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, !fe->rx_data_missing);
            hdlc_rx_status(s, SIG_STATUS_CARRIER_DOWN);
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
            hdlc_accept_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, FALSE);
            hdlc_rx_status(s, SIG_STATUS_CARRIER_DOWN);
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
            hdlc_rx_status(s, SIG_STATUS_CARRIER_DOWN);
        }
        break;
    case T38_FIELD_T4_NON_ECM_DATA:
        if (!s->at_state.rx_signal_present)
        {
            non_ecm_rx_status(s, SIG_STATUS_TRAINING_SUCCEEDED);
            s->at_state.rx_signal_present = TRUE;
        }
        if (len > 0)
        {
            bit_reverse(buf2, buf, len);
            non_ecm_put(s, buf, len);
        }
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
                if (!s->at_state.rx_signal_present)
                {
                    non_ecm_rx_status(s, SIG_STATUS_TRAINING_SUCCEEDED);
                    s->at_state.rx_signal_present = TRUE;
                }
                bit_reverse(buf2, buf, len);
                non_ecm_put(s, buf, len);
            }
            /* WORKAROUND: At least some Mediatrix boxes have a bug, where they can send HDLC signal end where
                           they should send non-ECM signal end. It is possible they also do the opposite.
                           We need to tolerate this, so we use the generic receive complete
                           indication, rather than the specific non-ECM carrier down. */
            non_ecm_rx_status(s, SIG_STATUS_CARRIER_DOWN);
        }
        s->at_state.rx_signal_present = FALSE;
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
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    if (len <= 0)
    {
        s->hdlc_tx.len = -1;
    }
    else
    {
        s->t38_fe.hdlc_tx.extra_bits = extra_bits_in_stuffed_frame(msg, len);
        bit_reverse(s->hdlc_tx.buf, msg, len);
        s->hdlc_tx.len = len;
        s->hdlc_tx.ptr = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ int bits_to_us(t31_state_t *s, int bits)
{
    if (s->t38_fe.ms_per_tx_chunk == 0  ||  s->t38_fe.tx_bit_rate == 0)
        return 0;
    return bits*1000000/s->t38_fe.tx_bit_rate;
}
/*- End of function --------------------------------------------------------*/

static void set_octets_per_data_packet(t31_state_t *s, int bit_rate)
{
    s->t38_fe.tx_bit_rate = bit_rate;
    if (s->t38_fe.ms_per_tx_chunk)
    {
        s->t38_fe.octets_per_data_packet = s->t38_fe.ms_per_tx_chunk*bit_rate/(8*1000);
        /* Make sure we have a positive number (i.e. we didn't truncate to zero). */
        if (s->t38_fe.octets_per_data_packet < 1)
            s->t38_fe.octets_per_data_packet = 1;
    }
    else
    {
        s->t38_fe.octets_per_data_packet = MAX_OCTETS_PER_UNPACED_CHUNK;
    }
}
/*- End of function --------------------------------------------------------*/

static int stream_non_ecm(t31_state_t *s)
{
    t31_t38_front_end_state_t *fe;
    uint8_t buf[MAX_OCTETS_PER_UNPACED_CHUNK + 50];
    int delay;
    int len;

    fe = &s->t38_fe;
    for (delay = 0;  delay == 0;  )
    {
        switch (fe->timed_step)
        {
        case T38_TIMED_STEP_NON_ECM_MODEM:
            /* Create a 75ms silence */
            if (fe->t38.current_tx_indicator != T38_IND_NO_SIGNAL)
            {
                if ((delay = t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL)) < 0)
                {
                    /* ???????? */
                }
            }
            fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_2;
            fe->next_tx_samples = fe->samples;
            break;
        case T38_TIMED_STEP_NON_ECM_MODEM_2:
            /* Switch on a fast modem, and give the training time to complete */
            if ((delay = t38_core_send_indicator(&fe->t38, fe->next_tx_indicator)) < 0)
            {
                /* ???????? */
            }
            fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_3;
            break;
        case T38_TIMED_STEP_NON_ECM_MODEM_3:
            /* Send a chunk of non-ECM image data */
            /* T.38 says it is OK to send the last of the non-ECM data in the signal end message.
               However, I think the early versions of T.38 said the signal end message should not
               contain data. Hopefully, following the current spec will not cause compatibility
               issues. */
            len = non_ecm_get(s, buf, fe->octets_per_data_packet);
            if (len > 0)
                bit_reverse(buf, buf, len);
            if (len < fe->octets_per_data_packet)
            {
                /* That's the end of the image data. */
                if (s->t38_fe.ms_per_tx_chunk)
                {
                    /* Pad the end of the data with some zeros. If we just stop abruptly
                       at the end of the EOLs, some ATAs fail to clean up properly before
                       shutting down their transmit modem, and the last few rows of the image
                       are lost or corrupted. Simply delaying the no-signal message does not
                       help for all implentations. It is usually ignored, which is probably
                       the right thing to do after receiving a message saying the signal has
                       ended. */
                    memset(buf + len, 0, fe->octets_per_data_packet - len);
                    fe->non_ecm_trailer_bytes = 3*fe->octets_per_data_packet + len;
                    len = fe->octets_per_data_packet;
                    fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_4;
                }
                else
                {
                    /* If we are sending quickly there seems no point in doing any padding */
                    if (t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_SIG_END, buf, len, T38_PACKET_CATEGORY_IMAGE_DATA_END) < 0)
                    {
                        /* ???????? */
                    }
                    fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_5;
                    delay = 0;
                }
            }
            if (t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_DATA, buf, len, T38_PACKET_CATEGORY_IMAGE_DATA) < 0)
            {
                /* ???????? */
            }
            delay = bits_to_us(s, 8*len);
            break;
        case T38_TIMED_STEP_NON_ECM_MODEM_4:
            /* Send padding */
            len = fe->octets_per_data_packet;
            fe->non_ecm_trailer_bytes -= len;
            if (fe->non_ecm_trailer_bytes <= 0)
            {
                len += fe->non_ecm_trailer_bytes;
                memset(buf, 0, len);
                if (t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_SIG_END, buf, len, T38_PACKET_CATEGORY_IMAGE_DATA_END) < 0)
                {
                    /* ???????? */
                }
                fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_5;
                /* Allow a bit more time than the data will take to play out, to ensure the far ATA does not
                   cut things short. */
                delay = bits_to_us(s, 8*len);
                if (s->t38_fe.ms_per_tx_chunk)
                    delay += 60000;
                front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
                break;
            }
            memset(buf, 0, len);
            if (t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_DATA, buf, len, T38_PACKET_CATEGORY_IMAGE_DATA) < 0)
            {
                /* ???????? */
            }
            delay = bits_to_us(s, 8*len);
            break;
        case T38_TIMED_STEP_NON_ECM_MODEM_5:
            /* This should not be needed, since the message above indicates the end of the signal, but it
               seems like it can improve compatibility with quirky implementations. */
            if ((delay = t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL)) < 0)
            {
                /* ???????? */
            }
            fe->timed_step = T38_TIMED_STEP_NONE;
            return delay;
        }
    }
    return delay;
}
/*- End of function --------------------------------------------------------*/

static int stream_hdlc(t31_state_t *s)
{
    t31_t38_front_end_state_t *fe;
    uint8_t buf[MAX_OCTETS_PER_UNPACED_CHUNK + 50];
    t38_data_field_t data_fields[2];
    int previous;
    int delay;
    int i;
    int category;

    fe = &s->t38_fe;
    for (delay = 0;  delay == 0;  )
    {
        switch (fe->timed_step)
        {
        case T38_TIMED_STEP_HDLC_MODEM:
            /* Create a 75ms silence */
            if (fe->t38.current_tx_indicator != T38_IND_NO_SIGNAL)
            {
                if ((delay = t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL)) < 0)
                {
                    /* ???????? */
                }
            }
            fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_2;
            fe->next_tx_samples = fe->samples + ms_to_samples(75);
            break;
        case T38_TIMED_STEP_HDLC_MODEM_2:
            /* Send HDLC preambling */
            if ((delay = t38_core_send_indicator(&fe->t38, fe->next_tx_indicator)) < 0)
            {
                /* ???????? */
            }
            delay += t38_core_send_flags_delay(&fe->t38, fe->next_tx_indicator);
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
            fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_3;
            break;
        case T38_TIMED_STEP_HDLC_MODEM_3:
            /* Send a chunk of HDLC data */
            if (s->hdlc_tx.len == 0)
            {
                /* We don't have a frame ready yet, so wait a little */
                delay = MS_PER_TX_CHUNK*1000;
                break;
            }
            i = s->hdlc_tx.len - s->hdlc_tx.ptr;
            if (fe->octets_per_data_packet >= i)
            {
                /* The last part of an HDLC frame */
                if (fe->chunking_modes & T38_CHUNKING_MERGE_FCS_WITH_DATA)
                {
                    /* Copy the data, as we might be about to refill the buffer it is in */
                    memcpy(buf, &s->hdlc_tx.buf[s->hdlc_tx.ptr], i);
                    data_fields[0].field_type = T38_FIELD_HDLC_DATA;
                    data_fields[0].field = buf;
                    data_fields[0].field_len = i;

                    /* Now see about the next HDLC frame. This will tell us whether to send FCS_OK or FCS_OK_SIG_END */
                    previous = fe->current_tx_data_type;
                    s->hdlc_tx.ptr = 0;
                    s->hdlc_tx.len = 0;
                    front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
                    if (s->hdlc_tx.final)
                    {
                        data_fields[1].field_type = T38_FIELD_HDLC_FCS_OK_SIG_END;
                        data_fields[1].field = NULL;
                        data_fields[1].field_len = 0;
                        category = (s->t38_fe.current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA_END  :  T38_PACKET_CATEGORY_IMAGE_DATA_END;
                        if (t38_core_send_data_multi_field(&fe->t38, fe->current_tx_data_type, data_fields, 2, category) < 0)
                        {
                            /* ???????? */
                        }
                        fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_5;
                        /* We add a bit of extra time here, as with some implementations
                           the carrier falling too abruptly causes data loss. */
                        delay = bits_to_us(s, i*8 + fe->hdlc_tx.extra_bits);
                        if (s->t38_fe.ms_per_tx_chunk)
                            delay += 100000;
                        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
                        t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
                    }
                    else
                    {
                        data_fields[1].field_type = T38_FIELD_HDLC_FCS_OK;
                        data_fields[1].field = NULL;
                        data_fields[1].field_len = 0;
                        category = (s->t38_fe.current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA  :  T38_PACKET_CATEGORY_IMAGE_DATA;
                        if (t38_core_send_data_multi_field(&fe->t38, fe->current_tx_data_type, data_fields, 2, category) < 0)
                        {
                            /* ???????? */
                        }
                        fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_3;
                        delay = bits_to_us(s, i*8 + fe->hdlc_tx.extra_bits);
                        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
                    }
                    break;
                }
                category = (s->t38_fe.current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA  :  T38_PACKET_CATEGORY_IMAGE_DATA;
                if (t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_HDLC_DATA, &s->hdlc_tx.buf[s->hdlc_tx.ptr], i, category) < 0)
                {
                    /* ???????? */
                }
                fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_4;
            }
            else
            {
                i = fe->octets_per_data_packet;
                category = (s->t38_fe.current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA  :  T38_PACKET_CATEGORY_IMAGE_DATA;
                if (t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_HDLC_DATA, &s->hdlc_tx.buf[s->hdlc_tx.ptr], i, category) < 0)
                {
                    /* ???????? */
                }
                s->hdlc_tx.ptr += i;
            }
            delay = bits_to_us(s, i*8);
            break;
        case T38_TIMED_STEP_HDLC_MODEM_4:
            /* End of HDLC frame */
            previous = fe->current_tx_data_type;
            s->hdlc_tx.ptr = 0;
            s->hdlc_tx.len = 0;
            if (s->hdlc_tx.final)
            {
                /* End of transmission */
                s->hdlc_tx.len = 0;
                s->hdlc_tx.final = FALSE;
                category = (s->t38_fe.current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA  :  T38_PACKET_CATEGORY_IMAGE_DATA;
                if (t38_core_send_data(&fe->t38, previous, T38_FIELD_HDLC_FCS_OK, NULL, 0, category) < 0)
                {
                    /* ???????? */
                }
                fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_5;
                /* We add a bit of extra time here, as with some implementations
                   the carrier falling too abruptly causes data loss. */
                delay = bits_to_us(s, fe->hdlc_tx.extra_bits);
                if (s->t38_fe.ms_per_tx_chunk)
                    delay += 100000;
                front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
                break;
            }
            /* Finish the current frame off, and prepare for the next one. */
            category = (s->t38_fe.current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA  :  T38_PACKET_CATEGORY_IMAGE_DATA;
            if (t38_core_send_data(&fe->t38, previous, T38_FIELD_HDLC_FCS_OK, NULL, 0, category) < 0)
            {
                /* ???????? */
            }
            fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_3;
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
            /* We should now wait enough time for everything to clear through an analogue modem at the far end. */
            delay = bits_to_us(s, fe->hdlc_tx.extra_bits);
            if (s->hdlc_tx.len == 0)
                span_log(&s->logging, SPAN_LOG_FLOW, "No new frame or end transmission condition.\n");
            break;
        case T38_TIMED_STEP_HDLC_MODEM_5:
            /* Note that some boxes do not like us sending a T38_FIELD_HDLC_SIG_END at this point.
               A T38_IND_NO_SIGNAL should always be OK. */
            category = (s->t38_fe.current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA_END  :  T38_PACKET_CATEGORY_IMAGE_DATA_END;
            if (t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_HDLC_SIG_END, NULL, 0, category) < 0)
            {
                /* ???????? */
            }
            if ((delay = t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL)) < 0)
            {
                /* ???????? */
            }
            fe->timed_step = T38_TIMED_STEP_NONE;
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
            t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            return 0;
        }
    }
    return delay;
}
/*- End of function --------------------------------------------------------*/

static int stream_ced(t31_state_t *s)
{
    t31_t38_front_end_state_t *fe;
    int delay;

    fe = &s->t38_fe;
    for (delay = 0;  delay == 0;  )
    {
        switch (fe->timed_step)
        {
        case T38_TIMED_STEP_CED:
            /* It seems common practice to start with a no signal indicator, though
               this is not a specified requirement. Since we should be sending 200ms
               of silence, starting the delay with a no signal indication makes sense.
               We do need a 200ms delay, as that is a specification requirement. */
            fe->timed_step = T38_TIMED_STEP_CED_2;
            if ((delay = t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL)) < 0)
            {
                /* ???????? */
            }
            delay = 200000;
            fe->next_tx_samples = fe->samples;
            break;
        case T38_TIMED_STEP_CED_2:
            /* Initial 200ms delay over. Send the CED indicator */
            fe->timed_step = T38_TIMED_STEP_CED_3;
            if ((delay = t38_core_send_indicator(&fe->t38, T38_IND_CED)) < 0)
            {
                /* ???????? */
            }
            fe->current_tx_data_type = T38_DATA_NONE;
            break;
        case T38_TIMED_STEP_CED_3:
            /* End of CED */
            fe->timed_step = T38_TIMED_STEP_NONE;
            front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
            return 0;
        }
    }
    return delay;
}
/*- End of function --------------------------------------------------------*/

static int stream_cng(t31_state_t *s)
{
    t31_t38_front_end_state_t *fe;
    int delay;

    fe = &s->t38_fe;
    for (delay = 0;  delay == 0;  )
    {
        switch (fe->timed_step)
        {
        case T38_TIMED_STEP_CNG:
            /* It seems common practice to start with a no signal indicator, though
               this is not a specified requirement of the T.38 spec. Since we should
               be sending 200ms of silence, according to T.30, starting that delay with
               a no signal indication makes sense. */
            fe->timed_step = T38_TIMED_STEP_CNG_2;
            if ((delay = t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL)) < 0)
            {
                /* ???????? */
            }
            delay = 200000;
            fe->next_tx_samples = fe->samples;
            break;
        case T38_TIMED_STEP_CNG_2:
            /* Initial short delay over. Send the CNG indicator. CNG persists until something
               coming the other way interrupts it, or a long timeout controlled by the T.30 engine
               expires. */
            fe->timed_step = T38_TIMED_STEP_NONE;
            if ((delay = t38_core_send_indicator(&fe->t38, T38_IND_CNG)) < 0)
            {
                /* ???????? */
            }
            fe->current_tx_data_type = T38_DATA_NONE;
            return delay;
        }
    }
    return delay;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_t38_send_timeout(t31_state_t *s, int samples)
{
    t31_t38_front_end_state_t *fe;
    int delay;

    fe = &s->t38_fe;
    if (fe->current_rx_type == T30_MODEM_DONE  ||  fe->current_tx_type == T30_MODEM_DONE)
        return TRUE;

    fe->samples += samples;
    if (fe->timeout_rx_samples  &&  fe->samples > fe->timeout_rx_samples)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Timeout mid-receive\n");
        fe->timeout_rx_samples = 0;
        front_end_status(s, T30_FRONT_END_RECEIVE_COMPLETE);
    }
    if (fe->timed_step == T38_TIMED_STEP_NONE)
        return FALSE;
    /* Wait until the right time comes along, unless we are working in "no delays" mode, while talking to an
       IAF terminal. */
    if (fe->ms_per_tx_chunk  &&  fe->samples < fe->next_tx_samples)
        return FALSE;
    /* Its time to send something */
    delay = 0;
    switch (fe->timed_step & 0xFFF0)
    {
    case T38_TIMED_STEP_NON_ECM_MODEM:
        delay = stream_non_ecm(s);
        break;
    case T38_TIMED_STEP_HDLC_MODEM:
        delay = stream_hdlc(s);
        break;
    //case T38_TIMED_STEP_FAKE_HDLC_MODEM:
    //    delay = stream_fake_hdlc(s);
    //    break;
    case T38_TIMED_STEP_CED:
        delay = stream_ced(s);
        break;
    case T38_TIMED_STEP_CNG:
        delay = stream_cng(s);
        break;
    case T38_TIMED_STEP_PAUSE:
        /* End of timed pause */
        fe->timed_step = T38_TIMED_STEP_NONE;
        front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
        break;
    }
    fe->next_tx_samples += us_to_samples(delay);
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

static int t31_modem_control_handler(at_state_t *s, void *user_data, int op, const char *num)
{
    t31_state_t *t;
    
    t = (t31_state_t *) user_data;
    switch (op)
    {
    case AT_MODEM_CONTROL_CALL:
        t->call_samples = 0;
        break;
    case AT_MODEM_CONTROL_ANSWER:
        t->call_samples = 0;
        break;
    case AT_MODEM_CONTROL_ONHOOK:
        if (t->tx.holding)
        {
            t->tx.holding = FALSE;
            /* Tell the application to release further data */
            at_modem_control(&t->at_state, AT_MODEM_CONTROL_CTS, (void *) 1);
        }
        if (t->at_state.rx_signal_present)
        {
            t->at_state.rx_data[t->at_state.rx_data_bytes++] = DLE;
            t->at_state.rx_data[t->at_state.rx_data_bytes++] = ETX;
            t->at_state.at_tx_handler(&t->at_state,
                                      t->at_state.at_tx_user_data,
                                      t->at_state.rx_data,
                                      t->at_state.rx_data_bytes);
            t->at_state.rx_data_bytes = 0;
        }
        restart_modem(t, FAX_MODEM_SILENCE_TX);
        break;
    case AT_MODEM_CONTROL_RESTART:
        restart_modem(t, (int) (intptr_t) num);
        return 0;
    case AT_MODEM_CONTROL_DTE_TIMEOUT:
        if (num)
            t->dte_data_timeout = t->call_samples + ms_to_samples((intptr_t) num);
        else
            t->dte_data_timeout = 0;
        return 0;
    }
    return t->modem_control_handler(t, t->modem_control_user_data, op, num);
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_rx_status(void *user_data, int status)
{
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    switch (status)
    {
    case SIG_STATUS_TRAINING_IN_PROGRESS:
        break;
    case SIG_STATUS_TRAINING_FAILED:
        s->at_state.rx_trained = FALSE;
        s->audio.modems.rx_trained = FALSE;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
        s->at_state.rx_signal_present = TRUE;
        s->at_state.rx_trained = TRUE;
        s->audio.modems.rx_trained = TRUE;
        break;
    case SIG_STATUS_CARRIER_UP:
        break;
    case SIG_STATUS_CARRIER_DOWN:
        if (s->at_state.rx_signal_present)
        {
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = ETX;
            s->at_state.at_tx_handler(&s->at_state,
                                      s->at_state.at_tx_user_data,
                                      s->at_state.rx_data,
                                      s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_NO_CARRIER);
            t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
        }
        s->at_state.rx_signal_present = FALSE;
        s->at_state.rx_trained = FALSE;
        s->audio.modems.rx_trained = FALSE;
        break;
    default:
        if (s->at_state.p.result_code_format)
            span_log(&s->logging, SPAN_LOG_FLOW, "Eh!\n");
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_put_bit(void *user_data, int bit)
{
    t31_state_t *s;

    if (bit < 0)
    {
        non_ecm_rx_status(user_data, bit);
        return;
    }
    s = (t31_state_t *) user_data;
    s->audio.current_byte = (s->audio.current_byte >> 1) | (bit << 7);
    if (++s->audio.bit_no >= 8)
    {
        if (s->audio.current_byte == DLE)
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
        s->at_state.rx_data[s->at_state.rx_data_bytes++] = (uint8_t) s->audio.current_byte;
        if (s->at_state.rx_data_bytes >= 250)
        {
            s->at_state.at_tx_handler(&s->at_state,
                                      s->at_state.at_tx_user_data,
                                      s->at_state.rx_data,
                                      s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
        }
        s->audio.bit_no = 0;
        s->audio.current_byte = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_put(void *user_data, const uint8_t buf[], int len)
{
    t31_state_t *s;
    int i;

    s = (t31_state_t *) user_data;
    /* Ignore any fractional bytes which may have accumulated */
    for (i = 0;  i < len;  i++)
    {
        if (buf[i] == DLE)
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
        s->at_state.rx_data[s->at_state.rx_data_bytes++] = buf[i];
        if (s->at_state.rx_data_bytes >= 250)
        {
            s->at_state.at_tx_handler(&s->at_state,
                                      s->at_state.at_tx_user_data,
                                      s->at_state.rx_data,
                                      s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
        }
    }
    s->audio.bit_no = 0;
    s->audio.current_byte = 0;
}
/*- End of function --------------------------------------------------------*/

static int non_ecm_get_bit(void *user_data)
{
    t31_state_t *s;
    int bit;

    s = (t31_state_t *) user_data;
    if (s->audio.bit_no <= 0)
    {
        if (s->tx.out_bytes != s->tx.in_bytes)
        {
            /* There is real data available to send */
            s->audio.current_byte = s->tx.data[s->tx.out_bytes++];
            if (s->tx.out_bytes > T31_TX_BUF_LEN - 1)
            {
                s->tx.out_bytes = T31_TX_BUF_LEN - 1;
                span_log(&s->logging, SPAN_LOG_FLOW, "End of transmit buffer reached!\n");
            }
            if (s->tx.holding)
            {
                /* See if the buffer is approaching empty. It might be time to
                   release flow control. */
                if (s->tx.out_bytes > T31_TX_BUF_LOW_TIDE)
                {
                    s->tx.holding = FALSE;
                    /* Tell the application to release further data */
                    at_modem_control(&s->at_state, AT_MODEM_CONTROL_CTS, (void *) 1);
                }
            }
            s->tx.data_started = TRUE;
        }
        else
        {
            if (s->tx.final)
            {
                s->tx.final = FALSE;
                /* This will put the modem into its shutdown sequence. When
                   it has finally shut down, an OK response will be sent. */
                return SIG_STATUS_END_OF_DATA;
            }
            /* Fill with 0xFF bytes at the start of transmission, or 0x00 if we are in
               the middle of transmission. This follows T.31 and T.30 practice. */
            s->audio.current_byte = (s->tx.data_started)  ?  0x00  :  0xFF;
        }
        s->audio.bit_no = 8;
    }
    s->audio.bit_no--;
    bit = s->audio.current_byte & 1;
    s->audio.current_byte >>= 1;
    return bit;
}
/*- End of function --------------------------------------------------------*/

static int non_ecm_get(void *user_data, uint8_t buf[], int len)
{
    t31_state_t *s;
    int i;

    s = (t31_state_t *) user_data;
    for (i = 0;  i < len;  i++)
    {
        if (s->tx.out_bytes != s->tx.in_bytes)
        {
            /* There is real data available to send */
            buf[i] = s->tx.data[s->tx.out_bytes++];
            if (s->tx.out_bytes > T31_TX_BUF_LEN - 1)
            {
                s->tx.out_bytes = T31_TX_BUF_LEN - 1;
                span_log(&s->logging, SPAN_LOG_FLOW, "End of transmit buffer reached!\n");
            }
            if (s->tx.holding)
            {
                /* See if the buffer is approaching empty. It might be time to release flow control. */
                if (s->tx.out_bytes > T31_TX_BUF_LOW_TIDE)
                {
                    s->tx.holding = FALSE;
                    /* Tell the application to release further data */
                    at_modem_control(&s->at_state, AT_MODEM_CONTROL_CTS, (void *) 1);
                }
            }
            s->tx.data_started = TRUE;
        }
        else
        {
            if (s->tx.final)
            {
                s->tx.final = FALSE;
                /* This will put the modem into its shutdown sequence. When
                   it has finally shut down, an OK response will be sent. */
                //return SIG_STATUS_END_OF_DATA;
                return i;
            }
            /* Fill with 0xFF bytes at the start of transmission, or 0x00 if we are in
               the middle of transmission. This follows T.31 and T.30 practice. */
            buf[i] = (s->tx.data_started)  ?  0x00  :  0xFF;
        }
    }
    s->audio.bit_no = 0;
    s->audio.current_byte = 0;
    return len;
}
/*- End of function --------------------------------------------------------*/

static void tone_detected(void *user_data, int tone, int level, int delay)
{
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s detected (%ddBm0)\n", modem_connect_tone_to_str(tone), level);
}
/*- End of function --------------------------------------------------------*/

static void v8_handler(void *user_data, v8_parms_t *result)
{
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "V.8 report received\n");
}
/*- End of function --------------------------------------------------------*/

static void hdlc_tx_underflow(void *user_data)
{
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    if (s->hdlc_tx.final)
    {
        s->hdlc_tx.final = FALSE;
        /* Schedule an orderly shutdown of the modem */
        hdlc_tx_frame(&s->audio.modems.hdlc_tx, NULL, 0);
    }
    else
    {
        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
    }
}
/*- End of function --------------------------------------------------------*/

static void hdlc_rx_status(void *user_data, int status)
{
    t31_state_t *s;
    uint8_t buf[2];

    s = (t31_state_t *) user_data;
    switch (status)
    {
    case SIG_STATUS_TRAINING_IN_PROGRESS:
        break;
    case SIG_STATUS_TRAINING_FAILED:
        s->at_state.rx_trained = FALSE;
        s->audio.modems.rx_trained = FALSE;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        s->at_state.rx_signal_present = TRUE;
        s->at_state.rx_trained = TRUE;
        s->audio.modems.rx_trained = TRUE;
        break;
    case SIG_STATUS_CARRIER_UP:
        if (s->modem == FAX_MODEM_CNG_TONE  ||  s->modem == FAX_MODEM_NOCNG_TONE  ||  s->modem == FAX_MODEM_V21_RX)
        {
            s->at_state.rx_signal_present = TRUE;
            s->rx_frame_received = FALSE;
            s->audio.modems.rx_frame_received = FALSE;
        }
        break;
    case SIG_STATUS_CARRIER_DOWN:
        if (s->rx_frame_received)
        {
            if (s->at_state.dte_is_waiting)
            {
                if (s->at_state.ok_is_pending)
                {
                    at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
                    s->at_state.ok_is_pending = FALSE;
                }
                else
                {
                    at_put_response_code(&s->at_state, AT_RESPONSE_CODE_NO_CARRIER);
                }
                s->at_state.dte_is_waiting = FALSE;
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            }
            else
            {
                buf[0] = AT_RESPONSE_CODE_NO_CARRIER;
                queue_write_msg(s->rx_queue, buf, 1);
            }
        }
        s->at_state.rx_signal_present = FALSE;
        s->at_state.rx_trained = FALSE;
        s->audio.modems.rx_trained = FALSE;
        break;
    case SIG_STATUS_FRAMING_OK:
        if (s->modem == FAX_MODEM_CNG_TONE  ||  s->modem == FAX_MODEM_NOCNG_TONE)
        {
            /* Once we get any valid HDLC the CNG tone stops, and we drop
               to the V.21 receive modem on its own. */
            s->modem = FAX_MODEM_V21_RX;
            s->at_state.transmit = FALSE;
        }
        if (s->modem == FAX_MODEM_V17_RX  ||  s->modem == FAX_MODEM_V27TER_RX  ||  s->modem == FAX_MODEM_V29_RX)
        {
            /* V.21 has been detected while expecting a different carrier.
               If +FAR=0 then result +FCERROR and return to command-mode.
               If +FAR=1 then report +FRH:3 and CONNECT, switching to
               V.21 receive mode. */
            if (s->at_state.p.adaptive_receive)
            {
                s->at_state.rx_signal_present = TRUE;
                s->rx_frame_received = TRUE;
                s->audio.modems.rx_frame_received = TRUE;
                s->modem = FAX_MODEM_V21_RX;
                s->at_state.transmit = FALSE;
                s->at_state.dte_is_waiting = TRUE;
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_FRH3);
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
            }
            else
            {
                s->modem = FAX_MODEM_SILENCE_TX;
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
                s->rx_frame_received = FALSE;
                s->audio.modems.rx_frame_received = FALSE;
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_FCERROR);
            }
        }
        else
        {
            if (!s->rx_frame_received)
            {
                if (s->at_state.dte_is_waiting)
                {
                    /* Report CONNECT as soon as possible to avoid a timeout. */
                    at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
                    s->rx_frame_received = TRUE;
                    s->audio.modems.rx_frame_received = TRUE;
                }
                else
                {
                    buf[0] = AT_RESPONSE_CODE_CONNECT;
                    queue_write_msg(s->rx_queue, buf, 1);
                }
            }
        }
        break;
    case SIG_STATUS_ABORT:
        /* Just ignore these */
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected HDLC rx status - %d!\n", status);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept_frame(void *user_data, const uint8_t *msg, int len, int ok)
{
    t31_state_t *s;
    uint8_t buf[256];
    int i;

    if (len < 0)
    {
        hdlc_rx_status(user_data, len);
        return;
    }
    s = (t31_state_t *) user_data;
    if (!s->rx_frame_received)
    {
        if (s->at_state.dte_is_waiting)
        {
            /* Report CONNECT as soon as possible to avoid a timeout. */
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
            s->rx_frame_received = TRUE;
            s->audio.modems.rx_frame_received = TRUE;
        }
        else
        {
            buf[0] = AT_RESPONSE_CODE_CONNECT;
            queue_write_msg(s->rx_queue, buf, 1);
        }
    }
    /* If OK is pending then we just ignore whatever comes in */
    if (!s->at_state.ok_is_pending)
    {
        if (s->at_state.dte_is_waiting)
        {
            /* Send straight away */
            /* It is safe to look at the two bytes beyond the length of the message,
               and expect to find the FCS there. */
            for (i = 0;  i < len + 2;  i++)
            {
                if (msg[i] == DLE)
                    s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
                s->at_state.rx_data[s->at_state.rx_data_bytes++] = msg[i];
            }
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = ETX;
            s->at_state.at_tx_handler(&s->at_state, s->at_state.at_tx_user_data, s->at_state.rx_data, s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
            if (msg[1] == 0x13  &&  ok)
            {
                /* This is the last frame.  We don't send OK until the carrier drops to avoid
                   redetecting it later. */
                s->at_state.ok_is_pending = TRUE;
            }
            else
            {
                at_put_response_code(&s->at_state, (ok)  ?  AT_RESPONSE_CODE_OK  :  AT_RESPONSE_CODE_ERROR);
                s->at_state.dte_is_waiting = FALSE;
                s->rx_frame_received = FALSE;
                s->audio.modems.rx_frame_received = FALSE;
            }
        }
        else
        {
            /* Queue it */
            buf[0] = (ok)  ?  AT_RESPONSE_CODE_OK  :  AT_RESPONSE_CODE_ERROR;
            /* It is safe to look at the two bytes beyond the length of the message,
               and expect to find the FCS there. */
            memcpy(buf + 1, msg, len + 2);
            queue_write_msg(s->rx_queue, buf, len + 3);
        }
    }
    t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
}
/*- End of function --------------------------------------------------------*/

static void t31_v21_rx(t31_state_t *s)
{
    s->at_state.ok_is_pending = FALSE;
    s->hdlc_tx.final = FALSE;
    s->hdlc_tx.len = 0;
    s->dled = FALSE;
    hdlc_rx_init(&s->audio.modems.hdlc_rx, FALSE, TRUE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept_frame, s);
    fax_modems_start_slow_modem(&s->audio.modems, FAX_MODEM_V21_RX);
    s->at_state.transmit = TRUE;
}
/*- End of function --------------------------------------------------------*/

static int restart_modem(t31_state_t *s, int new_modem)
{
    int use_hdlc;
    fax_modems_state_t *t;

    t = &s->audio.modems;
    span_log(&s->logging, SPAN_LOG_FLOW, "Restart modem %d\n", new_modem);
    if (s->modem == new_modem)
        return 0;
    queue_flush(s->rx_queue);
    s->modem = new_modem;
    s->tx.final = FALSE;
    s->at_state.rx_signal_present = FALSE;
    s->at_state.rx_trained = FALSE;
    s->audio.modems.rx_trained = FALSE;
    s->rx_frame_received = FALSE;
    s->audio.modems.rx_frame_received = FALSE;
    fax_modems_set_rx_handler(t, (span_rx_handler_t) &span_dummy_rx, NULL, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
    use_hdlc = FALSE;
    switch (s->modem)
    {
    case FAX_MODEM_CNG_TONE:
        if (s->t38_mode)
        {
            s->t38_fe.next_tx_samples = s->t38_fe.samples;
            s->t38_fe.timed_step = T38_TIMED_STEP_CNG;
            s->t38_fe.current_tx_data_type = T38_DATA_NONE;
        }
        else
        {
            modem_connect_tones_tx_init(&t->connect_tx, MODEM_CONNECT_TONES_FAX_CNG);
            /* CNG is special, since we need to receive V.21 HDLC messages while sending the
               tone. Everything else in FAX processing sends only one way at a time. */
            /* Do V.21/HDLC receive in parallel. The other end may send its
               first message at any time. The CNG tone will continue until
               we get a valid preamble. */
            t31_v21_rx(s);
            fax_modems_set_rx_handler(t, (span_rx_handler_t) &cng_rx, s, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &modem_connect_tones_tx, &t->connect_tx);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        }
        s->at_state.transmit = TRUE;
        break;
    case FAX_MODEM_NOCNG_TONE:
        if (s->t38_mode)
        {
        }
        else
        {
            t31_v21_rx(s);
            fax_modems_set_rx_handler(t, (span_rx_handler_t) &cng_rx, s, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
            silence_gen_set(&t->silence_gen, 0);
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        }
        s->at_state.transmit = FALSE;
        break;
    case FAX_MODEM_CED_TONE:
        if (s->t38_mode)
        {
            s->t38_fe.next_tx_samples = s->t38_fe.samples;
            s->t38_fe.timed_step = T38_TIMED_STEP_CED;
            s->t38_fe.current_tx_data_type = T38_DATA_NONE;
        }
        else
        {
            modem_connect_tones_tx_init(&t->connect_tx, MODEM_CONNECT_TONES_FAX_CED);
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &modem_connect_tones_tx, &t->connect_tx);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        }
        s->at_state.transmit = TRUE;
        break;
    case FAX_MODEM_V21_RX:
        if (s->t38_mode)
        {
        }
        else
        {
            t31_v21_rx(s);
            fax_modems_set_rx_handler(t, (span_rx_handler_t) &fsk_rx, &t->v21_rx, (span_rx_fillin_handler_t) &fsk_rx_fillin, &t->v21_rx);
        }
        break;
    case FAX_MODEM_V21_TX:
        if (s->t38_mode)
        {
            s->t38_fe.next_tx_indicator = T38_IND_V21_PREAMBLE;
            s->t38_fe.current_tx_data_type = T38_DATA_V21;
            use_hdlc = TRUE;
            s->t38_fe.timed_step = (use_hdlc)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
            set_octets_per_data_packet(s, 300);
        }
        else
        {
            hdlc_tx_init(&t->hdlc_tx, FALSE, 2, FALSE, hdlc_tx_underflow, s);
            /* The spec says 1s +-15% of preamble. So, the minimum is 32 octets. */
            hdlc_tx_flags(&t->hdlc_tx, 32);
            fax_modems_start_slow_modem(t, FAX_MODEM_V21_TX);
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &fsk_tx, &t->v21_tx);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        }
        s->hdlc_tx.final = FALSE;
        s->hdlc_tx.len = 0;
        s->dled = FALSE;
        s->at_state.transmit = TRUE;
        break;
    case FAX_MODEM_V17_RX:
    case FAX_MODEM_V27TER_RX:
    case FAX_MODEM_V29_RX:
        if (!s->t38_mode)
        {
            fax_modems_start_fast_modem(t, s->modem, s->bit_rate, s->short_train, use_hdlc);
            /* Allow for +FCERROR/+FRH:3 */
            t31_v21_rx(s);
        }
        s->at_state.transmit = FALSE;
        break;
    case FAX_MODEM_V17_TX:
        if (s->t38_mode)
        {
            switch (s->bit_rate)
            {
            case 7200:
                s->t38_fe.next_tx_indicator = (s->short_train)  ?  T38_IND_V17_7200_SHORT_TRAINING  :  T38_IND_V17_7200_LONG_TRAINING;
                s->t38_fe.current_tx_data_type = T38_DATA_V17_7200;
                break;
            case 9600:
                s->t38_fe.next_tx_indicator = (s->short_train)  ?  T38_IND_V17_9600_SHORT_TRAINING  :  T38_IND_V17_9600_LONG_TRAINING;
                s->t38_fe.current_tx_data_type = T38_DATA_V17_9600;
                break;
            case 12000:
                s->t38_fe.next_tx_indicator = (s->short_train)  ?  T38_IND_V17_12000_SHORT_TRAINING  :  T38_IND_V17_12000_LONG_TRAINING;
                s->t38_fe.current_tx_data_type = T38_DATA_V17_12000;
                break;
            case 14400:
                s->t38_fe.next_tx_indicator = (s->short_train)  ?  T38_IND_V17_14400_SHORT_TRAINING  :  T38_IND_V17_14400_LONG_TRAINING;
                s->t38_fe.current_tx_data_type = T38_DATA_V17_14400;
                break;
            }
            set_octets_per_data_packet(s, s->bit_rate);
            s->t38_fe.timed_step = (use_hdlc)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        }
        else
        {
            fax_modems_start_fast_modem(t, s->modem, s->bit_rate, s->short_train, use_hdlc);
        }
        s->tx.out_bytes = 0;
        s->tx.data_started = FALSE;
        s->at_state.transmit = TRUE;
        break;
    case FAX_MODEM_V27TER_TX:
        if (s->t38_mode)
        {
            switch (s->bit_rate)
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
            set_octets_per_data_packet(s, s->bit_rate);
            s->t38_fe.timed_step = (use_hdlc)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        }
        else
        {
            fax_modems_start_fast_modem(t, s->modem, s->bit_rate, s->short_train, use_hdlc);
        }
        s->tx.out_bytes = 0;
        s->tx.data_started = FALSE;
        s->at_state.transmit = TRUE;
        break;
    case FAX_MODEM_V29_TX:
        if (s->t38_mode)
        {
            switch (s->bit_rate)
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
            set_octets_per_data_packet(s, s->bit_rate);
            s->t38_fe.timed_step = (use_hdlc)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        }
        else
        {
            fax_modems_start_fast_modem(t, s->modem, s->bit_rate, s->short_train, use_hdlc);
        }
        s->tx.out_bytes = 0;
        s->tx.data_started = FALSE;
        s->at_state.transmit = TRUE;
        break;
    case FAX_MODEM_SILENCE_TX:
        if (s->t38_mode)
        {
            if (t38_core_send_indicator(&s->t38_fe.t38, T38_IND_NO_SIGNAL) < 0)
            {
                /* ???????? */
            }
            s->t38_fe.next_tx_samples = s->t38_fe.samples + ms_to_samples(700);
            s->t38_fe.timed_step = T38_TIMED_STEP_PAUSE;
            s->t38_fe.current_tx_data_type = T38_DATA_NONE;
        }
        else
        {
            silence_gen_set(&t->silence_gen, 0);
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        }
        s->at_state.transmit = FALSE;
        break;
    case FAX_MODEM_SILENCE_RX:
        if (!s->t38_mode)
        {
            fax_modems_set_rx_handler(t, (span_rx_handler_t) &silence_rx, s, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
            silence_gen_set(&t->silence_gen, 0);
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        }
        s->at_state.transmit = FALSE;
        break;
    case FAX_MODEM_FLUSH:
        /* Send 200ms of silence to "push" the last audio out */
        if (s->t38_mode)
        {
            if (t38_core_send_indicator(&s->t38_fe.t38, T38_IND_NO_SIGNAL) < 0)
            {
                /* ???????? */
            }
        }
        else
        {
            s->modem = FAX_MODEM_SILENCE_TX;
            silence_gen_alter(&t->silence_gen, ms_to_samples(200));
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
            s->at_state.transmit = TRUE;
        }
        break;
    }
    s->audio.bit_no = 0;
    s->audio.current_byte = 0xFF;
    s->tx.in_bytes = 0;
    s->tx.out_bytes = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void dle_unstuff_hdlc(t31_state_t *s, const char *stuffed, int len)
{
    int i;

    for (i = 0;  i < len;  i++)
    {
        if (s->dled)
        {
            s->dled = FALSE;
            if (stuffed[i] == ETX)
            {
                s->hdlc_tx.final = (s->hdlc_tx.buf[1] & 0x10);
                if (s->t38_mode)
                {
                    send_hdlc(s, s->hdlc_tx.buf, s->hdlc_tx.len);
                }
                else
                {
                    hdlc_tx_frame(&s->audio.modems.hdlc_tx, s->hdlc_tx.buf, s->hdlc_tx.len);
                    s->hdlc_tx.len = 0;
                }
            }
            else if (stuffed[i] == SUB)
            {
                s->hdlc_tx.buf[s->hdlc_tx.len++] = DLE;
                s->hdlc_tx.buf[s->hdlc_tx.len++] = DLE;
            }
            else
            {
                s->hdlc_tx.buf[s->hdlc_tx.len++] = stuffed[i];
            }
        }
        else
        {
            if (stuffed[i] == DLE)
                s->dled = TRUE;
            else
                s->hdlc_tx.buf[s->hdlc_tx.len++] = stuffed[i];
        }
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void dle_unstuff(t31_state_t *s, const char *stuffed, int len)
{
    int i;
    
    for (i = 0;  i < len;  i++)
    {
        if (s->dled)
        {
            s->dled = FALSE;
            if (stuffed[i] == ETX)
            {
                s->tx.final = TRUE;
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
                return;
            }
        }
        else if (stuffed[i] == DLE)
        {
            s->dled = TRUE;
            continue;
        }
        s->tx.data[s->tx.in_bytes++] = stuffed[i];
        if (s->tx.in_bytes > T31_TX_BUF_LEN - 1)
        {
            /* Oops. We hit the end of the buffer. Give up. Loose stuff. :-( */
            span_log(&s->logging, SPAN_LOG_FLOW, "No room in buffer for new data!\n");
            return;
        }
    }
    if (!s->tx.holding)
    {
        /* See if the buffer is approaching full. We might need to apply flow control. */
        if (s->tx.in_bytes > T31_TX_BUF_HIGH_TIDE)
        {
            s->tx.holding = TRUE;
            /* Tell the application to hold further data */
            at_modem_control(&s->at_state, AT_MODEM_CONTROL_CTS, (void *) 0);
        }
    }
}
/*- End of function --------------------------------------------------------*/

static int process_class1_cmd(at_state_t *t, void *user_data, int direction, int operation, int val)
{
    int new_modem;
    int new_transmit;
    int i;
    int len;
    int immediate_response;
    t31_state_t *s;
    uint8_t msg[256];

    s = (t31_state_t *) user_data;
    new_transmit = direction;
    immediate_response = TRUE;
    switch (operation)
    {
    case 'S':
        s->at_state.transmit = new_transmit;
        if (new_transmit)
        {
            /* Send a specified period of silence, to space transmissions. */
            restart_modem(s, FAX_MODEM_SILENCE_TX);
            if (s->t38_mode)
                s->t38_fe.next_tx_samples = s->t38_fe.samples + ms_to_samples(val*10);
            else
                silence_gen_alter(&s->audio.modems.silence_gen, ms_to_samples(val*10));
            s->at_state.transmit = TRUE;
        }
        else
        {
            /* Wait until we have received a specified period of silence. */
            queue_flush(s->rx_queue);
            s->silence_awaited = ms_to_samples(val*10);
            t31_set_at_rx_mode(s, AT_MODE_DELIVERY);
            if (s->t38_mode)
            {
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            }
            else
            {
                restart_modem(s, FAX_MODEM_SILENCE_RX);
            }
        }
        immediate_response = FALSE;
        span_log(&s->logging, SPAN_LOG_FLOW, "Silence %dms\n", val*10);
        break;
    case 'H':
        switch (val)
        {
        case 3:
            new_modem = (new_transmit)  ?  FAX_MODEM_V21_TX  :  FAX_MODEM_V21_RX;
            s->short_train = FALSE;
            s->bit_rate = 300;
            break;
        default:
            return -1;
        }
        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC\n");
        if (new_modem != s->modem)
        {
            restart_modem(s, new_modem);
            immediate_response = FALSE;
        }
        s->at_state.transmit = new_transmit;
        if (new_transmit)
        {
            t31_set_at_rx_mode(s, AT_MODE_HDLC);
            if (!s->t38_mode)
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
        }
        else
        {
            /* Send straight away, if there is something queued. */
            t31_set_at_rx_mode(s, AT_MODE_DELIVERY);
            s->rx_frame_received = FALSE;
            s->audio.modems.rx_frame_received = FALSE;
            do
            {
                if (!queue_empty(s->rx_queue))
                {
                    len = queue_read_msg(s->rx_queue, msg, 256);
                    if (len > 1)
                    {
                        if (msg[0] == AT_RESPONSE_CODE_OK)
                            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
                        for (i = 1;  i < len;  i++)
                        {
                            if (msg[i] == DLE)
                                s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
                            s->at_state.rx_data[s->at_state.rx_data_bytes++] = msg[i];
                        }
                        s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
                        s->at_state.rx_data[s->at_state.rx_data_bytes++] = ETX;
                        s->at_state.at_tx_handler(&s->at_state, s->at_state.at_tx_user_data, s->at_state.rx_data, s->at_state.rx_data_bytes);
                        s->at_state.rx_data_bytes = 0;
                    }
                    at_put_response_code(&s->at_state, msg[0]);
                }
                else
                {
                    s->at_state.dte_is_waiting = TRUE;
                    break;
                }
            }
            while (msg[0] == AT_RESPONSE_CODE_CONNECT);
        }
        immediate_response = FALSE;
        break;
    default:
        switch (val)
        {
        case 24:
            s->t38_fe.next_tx_indicator = T38_IND_V27TER_2400_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V27TER_2400;
            new_modem = (new_transmit)  ?  FAX_MODEM_V27TER_TX  :  FAX_MODEM_V27TER_RX;
            s->short_train = FALSE;
            s->bit_rate = 2400;
            break;
        case 48:
            s->t38_fe.next_tx_indicator = T38_IND_V27TER_4800_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V27TER_4800;
            new_modem = (new_transmit)  ?  FAX_MODEM_V27TER_TX  :  FAX_MODEM_V27TER_RX;
            s->short_train = FALSE;
            s->bit_rate = 4800;
            break;
        case 72:
            s->t38_fe.next_tx_indicator = T38_IND_V29_7200_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V29_7200;
            new_modem = (new_transmit)  ?  FAX_MODEM_V29_TX  :  FAX_MODEM_V29_RX;
            s->short_train = FALSE;
            s->bit_rate = 7200;
            break;
        case 96:
            s->t38_fe.next_tx_indicator = T38_IND_V29_9600_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V29_9600;
            new_modem = (new_transmit)  ?  FAX_MODEM_V29_TX  :  FAX_MODEM_V29_RX;
            s->short_train = FALSE;
            s->bit_rate = 9600;
            break;
        case 73:
            s->t38_fe.next_tx_indicator = T38_IND_V17_7200_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_7200;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 7200;
            break;
        case 74:
            s->t38_fe.next_tx_indicator = T38_IND_V17_7200_SHORT_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_7200;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 7200;
            break;
        case 97:
            s->t38_fe.next_tx_indicator = T38_IND_V17_9600_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_9600;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 9600;
            break;
        case 98:
            s->t38_fe.next_tx_indicator = T38_IND_V17_9600_SHORT_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_9600;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 9600;
            break;
        case 121:
            s->t38_fe.next_tx_indicator = T38_IND_V17_12000_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_12000;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 12000;
            break;
        case 122:
            s->t38_fe.next_tx_indicator = T38_IND_V17_12000_SHORT_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_12000;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 12000;
            break;
        case 145:
            s->t38_fe.next_tx_indicator = T38_IND_V17_14400_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_14400;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 14400;
            break;
        case 146:
            s->t38_fe.next_tx_indicator = T38_IND_V17_14400_SHORT_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_14400;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 14400;
            break;
        default:
            return -1;
        }
        span_log(&s->logging, SPAN_LOG_FLOW, "Short training = %d, bit rate = %d\n", s->short_train, s->bit_rate);
        if (new_transmit)
        {
            t31_set_at_rx_mode(s, AT_MODE_STUFFED);
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
        }
        else
        {
            t31_set_at_rx_mode(s, AT_MODE_DELIVERY);
        }
        restart_modem(s, new_modem);
        immediate_response = FALSE;
        break;
    }
    return immediate_response;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_call_event(t31_state_t *s, int event)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Call event %d received\n", event);
    at_call_event(&s->at_state, event);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_at_rx_free_space(t31_state_t *s)
{
    return T31_TX_BUF_LEN - (s->tx.in_bytes - s->tx.out_bytes) - 1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_at_rx(t31_state_t *s, const char *t, int len)
{
    if (s->dte_data_timeout)
        s->dte_data_timeout = s->call_samples + ms_to_samples(5000);
    switch (s->at_state.at_rx_mode)
    {
    case AT_MODE_ONHOOK_COMMAND:
    case AT_MODE_OFFHOOK_COMMAND:
        at_interpreter(&s->at_state, t, len);
        break;
    case AT_MODE_DELIVERY:
        /* Data from the DTE in this state returns us to command mode */
        if (len)
        {
            if (s->at_state.rx_signal_present)
            {
                s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
                s->at_state.rx_data[s->at_state.rx_data_bytes++] = ETX;
                s->at_state.at_tx_handler(&s->at_state, s->at_state.at_tx_user_data, s->at_state.rx_data, s->at_state.rx_data_bytes);
            }
            s->at_state.rx_data_bytes = 0;
            s->at_state.transmit = FALSE;
            s->modem = FAX_MODEM_SILENCE_TX;
            fax_modems_set_rx_handler(&s->audio.modems, (span_rx_handler_t) &span_dummy_rx, NULL, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
            t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
        }
        break;
    case AT_MODE_HDLC:
        dle_unstuff_hdlc(s, t, len);
        break;
    case AT_MODE_STUFFED:
        if (s->tx.out_bytes)
        {
            /* Make room for new data in existing data buffer. */
            s->tx.in_bytes -= s->tx.out_bytes;
            memmove(&s->tx.data[0], &s->tx.data[s->tx.out_bytes], s->tx.in_bytes);
            s->tx.out_bytes = 0;
        }
        dle_unstuff(s, t, len);
        break;
    case AT_MODE_CONNECTED:
        /* TODO: Implement for data modem operation */
        break;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static int silence_rx(void *user_data, const int16_t amp[], int len)
{
    t31_state_t *s;

    /* Searching for a specified minimum period of silence. */
    s = (t31_state_t *) user_data;
    if (s->silence_awaited  &&  s->audio.silence_heard >= s->silence_awaited)
    {
        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
        t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
        s->audio.silence_heard = 0;
        s->silence_awaited = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int cng_rx(void *user_data, const int16_t amp[], int len)
{
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    if (s->call_samples > ms_to_samples(s->at_state.p.s_regs[7]*1000))
    {
        /* After calling, S7 has elapsed... no carrier found. */
        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_NO_CARRIER);
        restart_modem(s, FAX_MODEM_SILENCE_TX);
        at_modem_control(&s->at_state, AT_MODEM_CONTROL_HANGUP, NULL);
        t31_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
    }
    else
    {
        fsk_rx(&s->audio.modems.v21_rx, amp, len);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) t31_rx(t31_state_t *s, int16_t amp[], int len)
{
    int i;
    int32_t power;

    /* Monitor for received silence.  Maximum needed detection is AT+FRS=255 (255*10ms). */
    /* We could probably only run this loop if (s->modem == FAX_MODEM_SILENCE_RX), however,
       the spec says "when silence has been present on the line for the amount of
       time specified".  That means some of the silence may have occurred before
       the AT+FRS=n command. This condition, however, is not likely to ever be the
       case.  (AT+FRS=n will usually be issued before the remote goes silent.) */
    for (i = 0;  i < len;  i++)
    {
        /* Clean up any DC influence. */
        power = power_meter_update(&s->audio.rx_power, amp[i] - s->audio.last_sample);
        s->audio.last_sample = amp[i];
        if (power > s->audio.silence_threshold_power)
        {
            s->audio.silence_heard = 0;
        }
        else
        {        
            if (s->audio.silence_heard <= ms_to_samples(255*10))
                s->audio.silence_heard++;
        }
    }

    /* Time is determined by counting the samples in audio packets coming in. */
    s->call_samples += len;

    /* In HDLC transmit mode, if 5 seconds elapse without data from the DTE
       we must treat this as an error. We return the result ERROR, and change
       to command-mode. */
    if (s->dte_data_timeout  &&  s->call_samples > s->dte_data_timeout)
    {
        t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_ERROR);
        restart_modem(s, FAX_MODEM_SILENCE_TX);
    }

    s->audio.modems.rx_handler(s->audio.modems.rx_user_data, amp, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) t31_rx_fillin(t31_state_t *s, int len)
{
    /* To mitigate the effect of lost packets on a packet network we should
       try to sustain the status quo. If there is no receive modem running, keep
       things that way. If there is a receive modem running, try to sustain its
       operation, without causing a phase hop, or letting its adaptive functions
       diverge. */
    /* Time is determined by counting the samples in audio packets coming in. */
    s->call_samples += len;

    /* In HDLC transmit mode, if 5 seconds elapse without data from the DTE
       we must treat this as an error. We return the result ERROR, and change
       to command-mode. */
    if (s->dte_data_timeout  &&  s->call_samples > s->dte_data_timeout)
    {
        t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_ERROR);
        restart_modem(s, FAX_MODEM_SILENCE_TX);
    }

    s->audio.modems.rx_fillin_handler(s->audio.modems.rx_fillin_user_data, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int set_next_tx_type(t31_state_t *s)
{
    if (s->audio.next_tx_handler)
    {
        fax_modems_set_tx_handler(&s->audio.modems, s->audio.next_tx_handler, s->audio.next_tx_user_data);
        fax_modems_set_next_tx_handler(&s->audio.modems, (span_tx_handler_t) NULL, NULL);
        return 0;
    }
    /* There is nothing else to change to, so use zero length silence */
    silence_gen_alter(&s->audio.modems.silence_gen, 0);
    fax_modems_set_tx_handler(&s->audio.modems, (span_tx_handler_t) &silence_gen, &s->audio.modems.silence_gen);
    fax_modems_set_next_tx_handler(&s->audio.modems, (span_tx_handler_t) NULL, NULL);
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(int) t31_tx(t31_state_t *s, int16_t amp[], int max_len)
{
    int len;

    len = 0;
    if (s->at_state.transmit)
    {
        if ((len = s->audio.modems.tx_handler(s->audio.modems.tx_user_data, amp, max_len)) < max_len)
        {
            /* Allow for one change of tx handler within a block */
            set_next_tx_type(s);
            if ((len += s->audio.modems.tx_handler(s->audio.modems.tx_user_data, amp + len, max_len - len)) < max_len)
                front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
        }
    }
    if (s->audio.modems.transmit_on_idle)
    {
        /* Pad to the requested length with silence */
        memset(amp + len, 0, (max_len - len)*sizeof(int16_t));
        len = max_len;        
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_set_transmit_on_idle(t31_state_t *s, int transmit_on_idle)
{
    s->audio.modems.transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_set_tep_mode(t31_state_t *s, int use_tep)
{
    s->audio.modems.use_tep = use_tep;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_set_t38_config(t31_state_t *s, int without_pacing)
{
    if (without_pacing)
    {
        /* Continuous streaming mode, as used for TPKT over TCP transport */
        /* Inhibit indicator packets */
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_INDICATOR, 0);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_CONTROL_DATA, 1);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_CONTROL_DATA_END, 1);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_IMAGE_DATA, 1);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_IMAGE_DATA_END, 1);
        s->t38_fe.ms_per_tx_chunk = 0;
    }
    else
    {
        /* Paced streaming mode, as used for UDP transports */
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_INDICATOR, INDICATOR_TX_COUNT);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_CONTROL_DATA, DATA_TX_COUNT);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_CONTROL_DATA_END, DATA_END_TX_COUNT);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_IMAGE_DATA, DATA_TX_COUNT);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_IMAGE_DATA_END, DATA_END_TX_COUNT);
        s->t38_fe.ms_per_tx_chunk = MS_PER_TX_CHUNK;
    }
    set_octets_per_data_packet(s, 300);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_set_mode(t31_state_t *s, int t38_mode)
{
    s->t38_mode = t38_mode;
    span_log(&s->logging, SPAN_LOG_FLOW, "Mode set to %d\n", s->t38_mode);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t31_get_logging_state(t31_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t38_core_state_t *) t31_get_t38_core_state(t31_state_t *s)
{
    return &s->t38_fe.t38;
}
/*- End of function --------------------------------------------------------*/

static int t31_t38_fe_init(t31_state_t *t,
                           t38_tx_packet_handler_t tx_packet_handler,
                           void *tx_packet_user_data)
{
    t31_t38_front_end_state_t *s;
    
    s = &t->t38_fe;

    t38_core_init(&s->t38,
                  process_rx_indicator,
                  process_rx_data,
                  process_rx_missing,
                  (void *) t,
                  tx_packet_handler,
                  tx_packet_user_data);
    s->t38.fastest_image_data_rate = 14400;

    s->timed_step = T38_TIMED_STEP_NONE;
    //s->iaf = T30_IAF_MODE_T37 | T30_IAF_MODE_T38;
    s->iaf = T30_IAF_MODE_T38;

    s->current_tx_data_type = T38_DATA_NONE;
    s->next_tx_samples = 0;
    s->chunking_modes = T38_CHUNKING_ALLOW_TEP_TIME;

    t->hdlc_tx.ptr = 0;

    hdlc_tx_init(&s->hdlc_tx_term, FALSE, 1, FALSE, NULL, NULL);
    hdlc_rx_init(&s->hdlc_rx_term, FALSE, TRUE, 2, NULL, NULL);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t31_state_t *) t31_init(t31_state_t *s,
                                     at_tx_handler_t at_tx_handler,
                                     void *at_tx_user_data,
                                     t31_modem_control_handler_t modem_control_handler,
                                     void *modem_control_user_data,
                                     t38_tx_packet_handler_t tx_t38_packet_handler,
                                     void *tx_t38_packet_user_data)
{
    v8_parms_t v8_parms;
    int alloced;

    if (at_tx_handler == NULL  ||  modem_control_handler == NULL)
        return NULL;

    alloced = FALSE;
    if (s == NULL)
    {
        if ((s = (t31_state_t *) malloc(sizeof (*s))) == NULL)
            return NULL;
        alloced = TRUE;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.31");

    s->modem_control_handler = modem_control_handler;
    s->modem_control_user_data = modem_control_user_data;
    fax_modems_init(&s->audio.modems,
                    FALSE,
                    hdlc_accept_frame,
                    hdlc_tx_underflow,
                    non_ecm_put_bit,
                    non_ecm_get_bit,
                    tone_detected,
                    (void *) s);
    fax_modems_set_rx_handler(&s->audio.modems, (span_rx_handler_t) &span_dummy_rx, NULL, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
    v8_parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
    v8_parms.call_function = V8_CALL_T30_RX;
    v8_parms.modulations = V8_MOD_V21
#if 0
                         | V8_MOD_V34HALF
#endif
                         | V8_MOD_V17
                         | V8_MOD_V29
                         | V8_MOD_V27TER;
    v8_parms.protocol = V8_PROTOCOL_NONE;
    v8_parms.pcm_modem_availability = 0;
    v8_parms.pstn_access = 0;
    v8_parms.nsf = -1;
    v8_parms.t66 = -1;
    v8_init(&s->audio.v8, FALSE, &v8_parms, v8_handler, s);

    power_meter_init(&s->audio.rx_power, 4);
    s->audio.last_sample = 0;
    s->audio.silence_threshold_power = power_meter_level_dbm0(-36);
    s->at_state.rx_signal_present = FALSE;
    s->at_state.rx_trained = FALSE;
    s->audio.modems.rx_trained = FALSE;

    s->at_state.do_hangup = FALSE;
    s->at_state.line_ptr = 0;
    s->audio.silence_heard = 0;
    s->silence_awaited = 0;
    s->call_samples = 0;
    s->modem = FAX_MODEM_NONE;
    s->at_state.transmit = TRUE;

    if ((s->rx_queue = queue_init(NULL, 4096, QUEUE_WRITE_ATOMIC | QUEUE_READ_ATOMIC)) == NULL)
    {
        if (alloced)
            free(s);
        return NULL;
    }
    at_init(&s->at_state, at_tx_handler, at_tx_user_data, t31_modem_control_handler, s);
    at_set_class1_handler(&s->at_state, process_class1_cmd, s);
    s->at_state.dte_inactivity_timeout = DEFAULT_DTE_TIMEOUT;
    if (tx_t38_packet_handler)
    {
        t31_t38_fe_init(s, tx_t38_packet_handler, tx_t38_packet_user_data);
        t31_set_t38_config(s, FALSE);
    }
    s->t38_mode = FALSE;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_release(t31_state_t *s)
{
    at_reset_call_info(&s->at_state);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_free(t31_state_t *s)
{
    t31_release(s);
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
