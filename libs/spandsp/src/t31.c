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
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"
#include <assert.h>
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
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
#include "spandsp/vector_int.h"
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
#include "spandsp/private/power_meter.h"
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
#define US_PER_TX_CHUNK                         30000
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
    T38_CHUNKING_MERGE_FCS_WITH_DATA = 0x0001,
    T38_CHUNKING_WHOLE_FRAMES = 0x0002,
    T38_CHUNKING_ALLOW_TEP_TIME = 0x0004,
    T38_CHUNKING_SEND_REGULAR_INDICATORS = 0x0008,
    T38_CHUNKING_SEND_2S_REGULAR_INDICATORS = 0x0010
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
    T38_TIMED_STEP_CED = 0x30,
    T38_TIMED_STEP_CED_2 = 0x31,
    T38_TIMED_STEP_CED_3 = 0x32,
    T38_TIMED_STEP_CNG = 0x40,
    T38_TIMED_STEP_CNG_2 = 0x41,
    T38_TIMED_STEP_PAUSE = 0x50,
    T38_TIMED_STEP_NO_SIGNAL = 0x60
};

static int restart_modem(t31_state_t *s, int new_modem);
static void hdlc_accept_frame(void *user_data, const uint8_t *msg, int len, int ok);
static void hdlc_accept_t38_frame(void *user_data, const uint8_t *msg, int len, int ok);
static void hdlc_accept_non_ecm_frame(void *user_data, const uint8_t *msg, int len, int ok);
static int silence_rx(void *user_data, const int16_t amp[], int len);
static int initial_timed_rx(void *user_data, const int16_t amp[], int len);
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

static int front_end_status(t31_state_t *s, int status)
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
                s->at_state.do_hangup = false;
            }
            else
            {
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            }
            /*endif*/
            break;
        case FAX_MODEM_CED_TONE_TX:
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
        /*endswitch*/
        break;
    case T30_FRONT_END_RECEIVE_COMPLETE:
        break;
    }
    /*endswitch*/
    if (s->t38_fe.timed_step == T38_TIMED_STEP_NONE)
        return -1;
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int extra_bits_in_stuffed_frame(const uint8_t buf[], int len)
{
    int bitstream;
    int ones;
    int stuffed;
    int i;
    int j;

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
                /*endif*/
            }
            else
            {
                ones = 0;
            }
            /*endif*/
            bitstream >>= 1;
        }
        /*endfor*/
    }
    /*endfor*/
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
    s->t38_fe.rx_data_missing = true;
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
    /*endif*/
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
        /*endif*/
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
    /*endswitch*/
    fe->hdlc_rx.len = 0;
    fe->rx_data_missing = false;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void process_hdlc_data(t31_t38_front_end_state_t *fe, const uint8_t *buf, int len)
{
    if (fe->hdlc_rx.len + len <= T31_T38_MAX_HDLC_LEN)
    {
        bit_reverse(fe->hdlc_rx.buf + fe->hdlc_rx.len, buf, len);
        fe->hdlc_rx.len += len;
    }
    else
    {
        fe->rx_data_missing = true;
    }
    /*endif*/
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
    /*endswitch*/
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
                fe->rx_data_missing = true;
            /*endif*/
        }
        /*endif*/
        if (len > 0)
        {
            process_hdlc_data(fe, buf, len);
        }
        /*endif*/
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        break;
    case T38_FIELD_HDLC_FCS_OK:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_OK!\n");
            /* The sender has incorrectly included data in this message. Cisco implemented inserting
               HDLC data here and Commetrex followed for compatibility reasons. We should, too. */
            process_hdlc_data(fe, buf, len);
        }
        /*endif*/
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_OK messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (fe->hdlc_rx.len > 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC OK (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            if (data_type == T38_DATA_V21)
            {
                if (fe->hdlc_rx.len >= 3)
                {
                    if ((fe->hdlc_rx.buf[2] & 0xFE) == T30_DCS)
                    {
                        /* We need to know if ECM is about to be used, so we can fake HDLC stuff. */
                        fe->ecm_mode = (fe->hdlc_rx.len >= 7  &&  (fe->hdlc_rx.buf[6] & DISBIT3))  ?  1  :  0;
                        span_log(&s->logging, SPAN_LOG_FLOW, "ECM mode: %d\n", fe->ecm_mode);
                    }
                    else if (s->t38_fe.ecm_mode == 1  &&  (fe->hdlc_rx.buf[2] & 0xFE) == T30_CFR)
                    {
                        s->t38_fe.ecm_mode = 2;
                    }
                    /*endif*/
                }
                /*endif*/
                crc_itu16_append(fe->hdlc_rx.buf, fe->hdlc_rx.len);
                hdlc_accept_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, !fe->rx_data_missing);
            }
            else
            {
                hdlc_accept_t38_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, !fe->rx_data_missing);
            }
            /*endif*/
            fe->hdlc_rx.len = 0;
        }
        /*endif*/
        fe->rx_data_missing = false;
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        break;
    case T38_FIELD_HDLC_FCS_BAD:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_BAD!\n");
            /* The sender has incorrectly included data in this message. Cisco implemented inserting
               HDLC data here and Commetrex followed for compatibility reasons. We should, too. */
            process_hdlc_data(fe, buf, len);
        }
        /*endif*/
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_BAD messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (fe->hdlc_rx.len > 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC bad (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            if (data_type == T38_DATA_V21)
                hdlc_accept_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, false);
            else
                hdlc_accept_t38_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, false);
            /*endif*/
            fe->hdlc_rx.len = 0;
        }
        /*endif*/
        fe->rx_data_missing = false;
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        break;
    case T38_FIELD_HDLC_FCS_OK_SIG_END:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_OK_SIG_END!\n");
            /* The sender has incorrectly included data in this message. Cisco implemented inserting
               HDLC data here and Commetrex followed for compatibility reasons. We should, too. */
            process_hdlc_data(fe, buf, len);
        }
        /*endif*/
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_OK_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (fe->hdlc_rx.len > 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC OK, sig end (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            if (data_type == T38_DATA_V21)
            {
                if (fe->hdlc_rx.len >= 3)
                {
                    if ((fe->hdlc_rx.buf[2] & 0xFE) == T30_DCS)
                    {
                        /* We need to know if ECM is about to be used, so we can fake HDLC stuff. */
                        fe->ecm_mode = (fe->hdlc_rx.len >= 7  &&  (fe->hdlc_rx.buf[6] & DISBIT3))  ?  1  :  0;
                        span_log(&s->logging, SPAN_LOG_FLOW, "ECM mode: %d\n", fe->ecm_mode);
                    }
                    else if (s->t38_fe.ecm_mode == 1  &&  (fe->hdlc_rx.buf[2] & 0xFE) == T30_CFR)
                    {
                        s->t38_fe.ecm_mode = 2;
                    }
                    /*endif*/
                }
                /*endif*/
                crc_itu16_append(fe->hdlc_rx.buf, fe->hdlc_rx.len);
                hdlc_accept_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, !fe->rx_data_missing);
            }
            else
            {
                hdlc_accept_t38_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, !fe->rx_data_missing);
            }
            /*endif*/
            fe->hdlc_rx.len = 0;
        }
        /*endif*/
        fe->rx_data_missing = false;
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            if (data_type == T38_DATA_V21)
                hdlc_rx_status(s, SIG_STATUS_CARRIER_DOWN);
            else
                non_ecm_rx_status(s, SIG_STATUS_CARRIER_DOWN);
            /*endif*/
        }
        /*endif*/
        fe->timeout_rx_samples = 0;
        break;
    case T38_FIELD_HDLC_FCS_BAD_SIG_END:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_BAD_SIG_END!\n");
            /* The sender has incorrectly included data in this message. Cisco implemented inserting
               HDLC data here and Commetrex followed for compatibility reasons. We should, too. */
            process_hdlc_data(fe, buf, len);
        }
        /*endif*/
        /* Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_BAD_SIG_END messages, in IFP packets with
           incrementing sequence numbers, which are actually repeats. They get through to this point because
           of the incrementing sequence numbers. We need to filter them here in a context sensitive manner. */
        if (fe->hdlc_rx.len > 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC bad, sig end (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->rx_data_missing)  ?  "missing octets"  :  "clean");
            if (data_type == T38_DATA_V21)
                hdlc_accept_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, false);
            else
                hdlc_accept_t38_frame(s, fe->hdlc_rx.buf, fe->hdlc_rx.len, false);
            /*endif*/
            fe->hdlc_rx.len = 0;
        }
        /*endif*/
        fe->rx_data_missing = false;
        if (t->current_rx_data_type != data_type  ||  t->current_rx_field_type != field_type)
        {
            if (data_type == T38_DATA_V21)
                hdlc_rx_status(s, SIG_STATUS_CARRIER_DOWN);
            else
                non_ecm_rx_status(s, SIG_STATUS_CARRIER_DOWN);
            /*endif*/
        }
        /*endif*/
        fe->timeout_rx_samples = 0;
        break;
    case T38_FIELD_HDLC_SIG_END:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_SIG_END!\n");
            /* The sender has incorrectly included data in this message, but there seems nothing meaningful
               it could be. There could not be an FCS good/bad report beyond this. */
        }
        /*endif*/
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
            fe->rx_data_missing = false;
            fe->timeout_rx_samples = 0;
            if (data_type == T38_DATA_V21)
                hdlc_rx_status(s, SIG_STATUS_CARRIER_DOWN);
            else
                non_ecm_rx_status(s, SIG_STATUS_CARRIER_DOWN);
            /*endif*/
        }
        /*endif*/
        break;
    case T38_FIELD_T4_NON_ECM_DATA:
        if (len > 0)
        {
            if (!s->at_state.rx_signal_present)
            {
                non_ecm_rx_status(s, SIG_STATUS_TRAINING_SUCCEEDED);
                s->at_state.rx_signal_present = true;
            }
            /*endif*/
            bit_reverse(buf2, buf, len);
            non_ecm_put(s, buf2, len);
        }
        /*endif*/
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
                    s->at_state.rx_signal_present = true;
                }
                /*endif*/
                bit_reverse(buf2, buf, len);
                non_ecm_put(s, buf2, len);
            }
            /*endif*/
            /* WORKAROUND: At least some Mediatrix boxes have a bug, where they can send HDLC signal end where
                           they should send non-ECM signal end. It is possible they also do the opposite.
                           We need to tolerate this, so we use the generic receive complete
                           indication, rather than the specific non-ECM carrier down. */
            non_ecm_rx_status(s, SIG_STATUS_CARRIER_DOWN);
        }
        /*endif*/
        s->at_state.rx_signal_present = false;
        fe->timeout_rx_samples = 0;
        break;
    case T38_FIELD_CM_MESSAGE:
        if (len >= 1)
            span_log(&s->logging, SPAN_LOG_FLOW, "CM profile %d - %s\n", buf[0] - '0', t38_cm_profile_to_str(buf[0]));
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for CM message - %d\n", len);
        /*endif*/
        break;
    case T38_FIELD_JM_MESSAGE:
        if (len >= 2)
            span_log(&s->logging, SPAN_LOG_FLOW, "JM - %s\n", t38_jm_to_str(buf, len));
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for JM message - %d\n", len);
        /*endif*/
        break;
    case T38_FIELD_CI_MESSAGE:
        if (len >= 1)
            span_log(&s->logging, SPAN_LOG_FLOW, "CI 0x%X\n", buf[0]);
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Bad length for CI message - %d\n", len);
        /*endif*/
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
        /*endif*/
        break;
    default:
        break;
    }
    /*endswitch*/
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
        if (len >= 3)
        {
            if ((s->hdlc_tx.buf[2] & 0xFE) == T30_DCS)
            {
                /* We need to know if ECM is about to be used, so we can fake HDLC stuff. */
                s->t38_fe.ecm_mode = (len >= 7  &&  (s->hdlc_tx.buf[6] & DISBIT3))  ?  1  :  0;
                span_log(&s->logging, SPAN_LOG_FLOW, "ECM mode: %d\n", s->t38_fe.ecm_mode);
            }
            else if (s->t38_fe.ecm_mode == 1  &&  (s->hdlc_tx.buf[2] & 0xFE) == T30_CFR)
            {
                s->t38_fe.ecm_mode = 2;
            }
            /*endif*/
        }
        /*endif*/
        s->t38_fe.hdlc_tx.extra_bits = extra_bits_in_stuffed_frame(msg, len);
        bit_reverse(s->hdlc_tx.buf, msg, len);
        s->hdlc_tx.len = len;
        s->hdlc_tx.ptr = 0;
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static __inline__ int bits_to_us(t31_state_t *s, int bits)
{
    if (s->t38_fe.us_per_tx_chunk == 0  ||  s->t38_fe.tx_bit_rate == 0)
        return 0;
    /*endif*/
    return bits*1000000/s->t38_fe.tx_bit_rate;
}
/*- End of function --------------------------------------------------------*/

static void set_octets_per_data_packet(t31_state_t *s, int bit_rate)
{
    s->t38_fe.tx_bit_rate = bit_rate;
    if (s->t38_fe.us_per_tx_chunk)
    {
        s->t38_fe.octets_per_data_packet = (s->t38_fe.us_per_tx_chunk/1000)*bit_rate/(8*1000);
        /* Make sure we have a positive number (i.e. we didn't truncate to zero). */
        if (s->t38_fe.octets_per_data_packet < 1)
            s->t38_fe.octets_per_data_packet = 1;
        /*endif*/
    }
    else
    {
        s->t38_fe.octets_per_data_packet = MAX_OCTETS_PER_UNPACED_CHUNK;
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int set_no_signal(t31_state_t *s)
{
    int delay;

    if ((s->t38_fe.chunking_modes & T38_CHUNKING_SEND_REGULAR_INDICATORS))
    {
        if ((delay = t38_core_send_indicator(&s->t38_fe.t38, 0x100 | T38_IND_NO_SIGNAL)) < 0)
            return delay;
        /*endif*/
        s->t38_fe.timed_step = T38_TIMED_STEP_NO_SIGNAL;
        if ((s->t38_fe.chunking_modes & T38_CHUNKING_SEND_2S_REGULAR_INDICATORS))
            s->t38_fe.timeout_tx_samples = s->t38_fe.next_tx_samples + us_to_samples(2000000);
        else
            s->t38_fe.timeout_tx_samples = 0;
        /*endif*/
        return s->t38_fe.us_per_tx_chunk;
    }
    /*endif*/
    if ((delay = t38_core_send_indicator(&s->t38_fe.t38, T38_IND_NO_SIGNAL)) < 0)
        return delay;
    /*endif*/
    s->t38_fe.timed_step = T38_TIMED_STEP_NONE;
    return delay;
}
/*- End of function --------------------------------------------------------*/

static int stream_no_signal(t31_state_t *s)
{
    int delay;

    if ((delay = t38_core_send_indicator(&s->t38_fe.t38, 0x100 | T38_IND_NO_SIGNAL)) < 0)
        return delay;
    /*endif*/
    if (s->t38_fe.timeout_tx_samples  &&  s->t38_fe.next_tx_samples >= s->t38_fe.timeout_tx_samples)
        s->t38_fe.timed_step = T38_TIMED_STEP_NONE;
    /*endif*/
    return s->t38_fe.us_per_tx_chunk;
}
/*- End of function --------------------------------------------------------*/

static int stream_non_ecm(t31_state_t *s)
{
    t31_t38_front_end_state_t *fe;
    uint8_t buf[MAX_OCTETS_PER_UNPACED_CHUNK + 50];
    int res;
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
                    return delay;
                /*endif*/
            }
            else
            {
                if (fe->us_per_tx_chunk)
                    delay = 75000;
                /*endif*/
            }
            /*endif*/
            fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_2;
            fe->timeout_tx_samples = fe->next_tx_samples
                                   + us_to_samples(t38_core_send_training_delay(&fe->t38, fe->next_tx_indicator));
            fe->next_tx_samples = fe->samples;
            break;
        case T38_TIMED_STEP_NON_ECM_MODEM_2:
            /* Switch on a fast modem, and give the training time to complete */
            if ((fe->chunking_modes & T38_CHUNKING_SEND_REGULAR_INDICATORS))
            {
                if ((delay = t38_core_send_indicator(&fe->t38, 0x100 | fe->next_tx_indicator)) < 0)
                    return delay;
                /*endif*/
                if (fe->next_tx_samples >= fe->timeout_tx_samples)
                    fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_3;
                /*endif*/
                return fe->us_per_tx_chunk;
            }
            /*endif*/
            if ((delay = t38_core_send_indicator(&fe->t38, fe->next_tx_indicator)) < 0)
                return delay;
            /*endif*/
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
            /*endif*/
            if (len < fe->octets_per_data_packet)
            {
                /* That's the end of the image data. */
                if (fe->us_per_tx_chunk)
                {
                    /* Pad the end of the data with some zeros. If we just stop abruptly
                       at the end of the EOLs, some ATAs fail to clean up properly before
                       shutting down their transmit modem, and the last few rows of the image
                       are lost or corrupted. Simply delaying the no-signal message does not
                       help for all implentations. It is usually ignored, which is probably
                       the right thing to do after receiving a message saying the signal has
                       ended. */
                    memset(&buf[len], 0, fe->octets_per_data_packet - len);
                    fe->non_ecm_trailer_bytes = 3*fe->octets_per_data_packet + len;
                    len = fe->octets_per_data_packet;
                    fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_4;
                }
                else
                {
                    /* If we are sending quickly there seems no point in doing any padding */
                    if ((res = t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_SIG_END, buf, len, T38_PACKET_CATEGORY_IMAGE_DATA_END)) < 0)
                        return res;
                    /*endif*/
                    fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_5;
                    if (front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE) < 0)
                        return -1;
                    /*endif*/
                    break;
                }
                /*endif*/
            }
            /*endif*/
            if ((res = t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_DATA, buf, len, T38_PACKET_CATEGORY_IMAGE_DATA)) < 0)
                return res;
            /*endif*/
            if (fe->us_per_tx_chunk)
                delay = bits_to_us(s, 8*len);
            /*endif*/
            break;
        case T38_TIMED_STEP_NON_ECM_MODEM_4:
            /* Send padding */
            len = fe->octets_per_data_packet;
            fe->non_ecm_trailer_bytes -= fe->octets_per_data_packet;
            if (fe->non_ecm_trailer_bytes <= 0)
            {
                len += fe->non_ecm_trailer_bytes;
                memset(buf, 0, len);
                if ((res = t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_SIG_END, buf, len, T38_PACKET_CATEGORY_IMAGE_DATA_END)) < 0)
                    return res;
                /*endif*/
                fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_5;
                /* Allow a bit more time than the data will take to play out, to ensure the far ATA does not
                   cut things short. */
                if (fe->us_per_tx_chunk)
                    delay = bits_to_us(s, 8*len) + 60000;
                /*endif*/
                if (front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE) < 0)
                    return -1;
                /*endif*/
                break;
            }
            /*endif*/
            memset(buf, 0, len);
            if ((res = t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_DATA, buf, len, T38_PACKET_CATEGORY_IMAGE_DATA)) < 0)
                return res;
            /*endif*/
            if (fe->us_per_tx_chunk)
                delay = bits_to_us(s, 8*len);
            /*endif*/
            break;
        case T38_TIMED_STEP_NON_ECM_MODEM_5:
            /* This should not be needed, since the message above indicates the end of the signal, but it
               seems like it can improve compatibility with quirky implementations. */
            delay = set_no_signal(s);
            fe->timed_step = T38_TIMED_STEP_NONE;
            return delay;
        }
        /*endswitch*/
    }
    /*endfor*/
    return delay;
}
/*- End of function --------------------------------------------------------*/

static int stream_hdlc(t31_state_t *s)
{
    t31_t38_front_end_state_t *fe;
    uint8_t buf[MAX_OCTETS_PER_UNPACED_CHUNK + 50];
    t38_data_field_t data_fields[2];
    int category;
    int previous;
    int res;
    int delay;
    int i;

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
                    return delay;
                /*endif*/
            }
            else
            {
                delay = (fe->us_per_tx_chunk)  ?  75000  :  0;
            }
            /*endif*/
            fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_2;
            fe->timeout_tx_samples = fe->next_tx_samples
                                   + us_to_samples(t38_core_send_training_delay(&fe->t38, fe->next_tx_indicator))
                                   + us_to_samples(t38_core_send_flags_delay(&fe->t38, fe->next_tx_indicator))
                                   + us_to_samples(delay);
            fe->next_tx_samples = fe->samples;
            break;
        case T38_TIMED_STEP_HDLC_MODEM_2:
            /* Send HDLC preambling */
            if ((fe->chunking_modes & T38_CHUNKING_SEND_REGULAR_INDICATORS))
            {
                if ((delay = t38_core_send_indicator(&fe->t38, 0x100 | fe->next_tx_indicator)) < 0)
                    return delay;
                /*endif*/
                if (fe->next_tx_samples >= fe->timeout_tx_samples)
                    fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_3;
                /*endif*/
                return fe->us_per_tx_chunk;
            }
            /*endif*/
            if ((delay = t38_core_send_indicator(&fe->t38, fe->next_tx_indicator)) < 0)
                return delay;
            /*endif*/
            delay += t38_core_send_flags_delay(&fe->t38, fe->next_tx_indicator);
            if (fe->current_tx_data_type == T38_DATA_V21)
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
            /*endif*/
            fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_3;
            break;
        case T38_TIMED_STEP_HDLC_MODEM_3:
            /* Send a chunk of HDLC data */
            if (s->hdlc_tx.len == 0)
            {
                /* We don't have a frame ready yet, so wait a little */
                if (fe->current_tx_data_type != T38_DATA_V21
                    &&
                    s->t38_fe.hdlc_from_t31.in != s->t38_fe.hdlc_from_t31.out)
                {
                    bit_reverse(s->hdlc_tx.buf, s->t38_fe.hdlc_from_t31.buf[s->t38_fe.hdlc_from_t31.out].buf, s->t38_fe.hdlc_from_t31.buf[s->t38_fe.hdlc_from_t31.out].len);
                    s->hdlc_tx.len = s->t38_fe.hdlc_from_t31.buf[s->t38_fe.hdlc_from_t31.out].len;
                    s->hdlc_tx.ptr = 0;
                    if (++s->t38_fe.hdlc_from_t31.out >= T31_TX_HDLC_BUFS)
                        s->t38_fe.hdlc_from_t31.out = 0;
                    /*endif*/
                    if (s->t38_fe.hdlc_from_t31.in == s->t38_fe.hdlc_from_t31.out)
                        s->hdlc_tx.final = s->non_ecm_tx.final;
                    /*endif*/
                }
                else
                {
                    delay = US_PER_TX_CHUNK;
                    break;
                }
                /*endif*/
            }
            /*endif*/
            i = s->hdlc_tx.len - s->hdlc_tx.ptr;
            if (fe->octets_per_data_packet >= i)
            {
                /* The last part of an HDLC frame */
                if ((fe->chunking_modes & T38_CHUNKING_MERGE_FCS_WITH_DATA))
                {
                    /* Copy the data, as we might be about to refill the buffer it is in */
                    memcpy(buf, &s->hdlc_tx.buf[s->hdlc_tx.ptr], i);
                    data_fields[0].field_type = T38_FIELD_HDLC_DATA;
                    data_fields[0].field = buf;
                    data_fields[0].field_len = i;

                    /* Now see about the next HDLC frame. This will tell us whether to send FCS_OK or FCS_OK_SIG_END */
                    s->hdlc_tx.ptr = 0;
                    s->hdlc_tx.len = 0;
                    if (front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE) < 0)
                        return -1;
                    /*endif*/
                    if (!s->hdlc_tx.final)
                    {
                        data_fields[1].field_type = T38_FIELD_HDLC_FCS_OK;
                        data_fields[1].field = NULL;
                        data_fields[1].field_len = 0;
                        category = (fe->current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA  :  T38_PACKET_CATEGORY_IMAGE_DATA;
                        if ((res = t38_core_send_data_multi_field(&fe->t38, fe->current_tx_data_type, data_fields, 2, category)) < 0)
                            return res;
                        /*endif*/
                        fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_3;
                        delay = bits_to_us(s, i*8 + fe->hdlc_tx.extra_bits);
                        if (fe->current_tx_data_type == T38_DATA_V21)
                            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
                        /*endif*/
                    }
                    else
                    {
                        data_fields[1].field_type = T38_FIELD_HDLC_FCS_OK_SIG_END;
                        data_fields[1].field = NULL;
                        data_fields[1].field_len = 0;
                        category = (fe->current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA_END  :  T38_PACKET_CATEGORY_IMAGE_DATA_END;
                        if ((res = t38_core_send_data_multi_field(&fe->t38, fe->current_tx_data_type, data_fields, 2, category)) < 0)
                            return res;
                        /*endif*/
                        fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_5;
                        /* We add a bit of extra time here, as with some implementations
                           the carrier falling too abruptly causes data loss. */
                        delay = bits_to_us(s, i*8 + fe->hdlc_tx.extra_bits);
                        if (fe->us_per_tx_chunk)
                            delay += 100000;
                        /*endif*/
                        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
                        t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
                    }
                    /*endif*/
                    break;
                }
                /*endif*/
                category = (fe->current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA  :  T38_PACKET_CATEGORY_IMAGE_DATA;
                if ((res = t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_HDLC_DATA, &s->hdlc_tx.buf[s->hdlc_tx.ptr], i, category)) < 0)
                    return res;
                /*endif*/
                fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_4;
            }
            else
            {
                i = fe->octets_per_data_packet;
                category = (fe->current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA  :  T38_PACKET_CATEGORY_IMAGE_DATA;
                if ((res = t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_HDLC_DATA, &s->hdlc_tx.buf[s->hdlc_tx.ptr], i, category)) < 0)
                    return res;
                /*endif*/
                s->hdlc_tx.ptr += i;
            }
            /*endif*/
            delay = bits_to_us(s, i*8);
            break;
        case T38_TIMED_STEP_HDLC_MODEM_4:
            /* End of HDLC frame */
            previous = fe->current_tx_data_type;
            s->hdlc_tx.ptr = 0;
            s->hdlc_tx.len = 0;
            if (!s->hdlc_tx.final)
            {
                /* Finish the current frame off, and prepare for the next one. */
                category = (fe->current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA  :  T38_PACKET_CATEGORY_IMAGE_DATA;
                if ((res = t38_core_send_data(&fe->t38, previous, T38_FIELD_HDLC_FCS_OK, NULL, 0, category)) < 0)
                    return res;
                /*endif*/
                fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_3;
                if (fe->current_tx_data_type == T38_DATA_V21)
                    at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
                /*endif*/
                /* We should now wait enough time for everything to clear through an analogue modem at the far end. */
                delay = bits_to_us(s, fe->hdlc_tx.extra_bits);
            }
            else
            {
                /* End of transmission */
                s->hdlc_tx.final = false;
                category = (fe->current_tx_data_type == T38_DATA_V21)  ?  T38_PACKET_CATEGORY_CONTROL_DATA_END  :  T38_PACKET_CATEGORY_IMAGE_DATA_END;
                if ((res = t38_core_send_data(&fe->t38, previous, T38_FIELD_HDLC_FCS_OK_SIG_END, NULL, 0, category)) < 0)
                    return res;
                /*endif*/
                fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_5;
                /* We add a bit of extra time here, as with some implementations
                   the carrier falling too abruptly causes data loss. */
                delay = bits_to_us(s, fe->hdlc_tx.extra_bits);
                if (fe->us_per_tx_chunk)
                    delay += 100000;
                /*endif*/
                if (front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE) < 0)
                    return -1;
                /*endif*/
            }
            /*endif*/
            break;
        case T38_TIMED_STEP_HDLC_MODEM_5:
            /* Note that some boxes do not like us sending a T38_FIELD_HDLC_SIG_END at this point.
               A T38_IND_NO_SIGNAL should always be OK. */
            delay = set_no_signal(s);
            fe->timed_step = T38_TIMED_STEP_NONE;
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
            t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            return delay;
        }
        /*endswitch*/
    }
    /*endfor*/
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
                return delay;
            /*endif*/
            delay = (fe->us_per_tx_chunk)  ?  200000  :  0;
            fe->next_tx_samples = fe->samples;
            break;
        case T38_TIMED_STEP_CED_2:
            /* Initial 200ms delay over. Send the CED indicator */
            fe->timed_step = T38_TIMED_STEP_CED_3;
            if ((delay = t38_core_send_indicator(&fe->t38, T38_IND_CED)) < 0)
                return delay;
            /*endif*/
            fe->current_tx_data_type = T38_DATA_NONE;
            break;
        case T38_TIMED_STEP_CED_3:
            /* End of CED */
            fe->timed_step = T38_TIMED_STEP_NONE;
            if (front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE) < 0)
                return -1;
            /*endif*/
            return 0;
        }
        /*endswitch*/
    }
    /*endfor*/
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
                return delay;
            /*endif*/
            delay = (fe->us_per_tx_chunk)  ?  200000  :  0;
            fe->next_tx_samples = fe->samples;
            break;
        case T38_TIMED_STEP_CNG_2:
            /* Initial short delay over. Send the CNG indicator. CNG persists until something
               coming the other way interrupts it, or a long timeout controlled by the T.30 engine
               expires. */
            delay = t38_core_send_indicator(&fe->t38, T38_IND_CNG);
            fe->timed_step = T38_TIMED_STEP_NONE;
            fe->current_tx_data_type = T38_DATA_NONE;
            return delay;
        }
        /*endswitch*/
    }
    /*endfor*/
    return delay;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_t38_send_timeout(t31_state_t *s, int samples)
{
    t31_t38_front_end_state_t *fe;
    int delay;

    fe = &s->t38_fe;
    if (fe->current_rx_type == T30_MODEM_DONE  ||  fe->current_tx_type == T30_MODEM_DONE)
        return true;
    /*endif*/

    fe->samples += samples;
    if (fe->timeout_rx_samples  &&  fe->samples > fe->timeout_rx_samples)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Timeout mid-receive\n");
        fe->timeout_rx_samples = 0;
        front_end_status(s, T30_FRONT_END_RECEIVE_COMPLETE);
    }
    /*endif*/
    if (fe->timed_step == T38_TIMED_STEP_NONE)
        return false;
    /*endif*/
    /* Wait until the right time comes along, unless we are working in "no delays" mode, while talking to an
       IAF terminal. */
    if (fe->us_per_tx_chunk  &&  fe->samples < fe->next_tx_samples)
        return false;
    /*endif*/
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
    case T38_TIMED_STEP_NO_SIGNAL:
        delay = stream_no_signal(s);
        break;
    }
    /*endswitch*/
    fe->next_tx_samples += us_to_samples(delay);
    return false;
}
/*- End of function --------------------------------------------------------*/

static int t31_modem_control_handler(void *user_data, int op, const char *num)
{
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    switch (op)
    {
    case AT_MODEM_CONTROL_CALL:
        s->call_samples = 0;
        t38_core_restart(&s->t38_fe.t38);
        break;
    case AT_MODEM_CONTROL_ANSWER:
        s->call_samples = 0;
        t38_core_restart(&s->t38_fe.t38);
        break;
    case AT_MODEM_CONTROL_ONHOOK:
        if (s->non_ecm_tx.holding)
        {
            s->non_ecm_tx.holding = false;
            /* Tell the application to release further data */
            at_modem_control(&s->at_state, AT_MODEM_CONTROL_CTS, (void *) 1);
        }
        /*endif*/
        if (s->at_state.rx_signal_present)
        {
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = ETX;
            s->at_state.at_tx_handler(s->at_state.at_tx_user_data,
                                      s->at_state.rx_data,
                                      s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
        }
        /*endif*/
        restart_modem(s, FAX_MODEM_SILENCE_TX);
        break;
    case AT_MODEM_CONTROL_RESTART:
        restart_modem(s, (int) (intptr_t) num);
        return 0;
    case AT_MODEM_CONTROL_DTE_TIMEOUT:
        if (num)
            s->dte_data_timeout = s->call_samples + ms_to_samples((intptr_t) num);
        else
            s->dte_data_timeout = 0;
        /*endif*/
        return 0;
    }
    /*endswitch*/
    return s->modem_control_handler(s, s->modem_control_user_data, op, num);
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
        s->at_state.rx_trained = false;
        s->audio.modems.rx_trained = false;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
        s->at_state.rx_signal_present = true;
        s->at_state.rx_trained = true;
        s->audio.modems.rx_trained = true;
        break;
    case SIG_STATUS_CARRIER_UP:
        break;
    case SIG_STATUS_CARRIER_DOWN:
        if (s->at_state.rx_signal_present)
        {
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = ETX;
            s->at_state.at_tx_handler(s->at_state.at_tx_user_data,
                                      s->at_state.rx_data,
                                      s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_NO_CARRIER);
            t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
        }
        /*endif*/
        s->at_state.rx_signal_present = false;
        s->at_state.rx_trained = false;
        s->audio.modems.rx_trained = false;
        break;
    default:
        if (s->at_state.p.result_code_format)
            span_log(&s->logging, SPAN_LOG_FLOW, "Eh!\n");
        /*endif*/
        break;
    }
    /*endswitch*/
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
    /*endif*/
    s = (t31_state_t *) user_data;
    s->audio.current_byte = (s->audio.current_byte >> 1) | (bit << 7);
    if (++s->audio.bit_no >= 8)
    {
        if (s->audio.current_byte == DLE)
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
        /*endif*/
        s->at_state.rx_data[s->at_state.rx_data_bytes++] = (uint8_t) s->audio.current_byte;
        if (s->at_state.rx_data_bytes >= 250)
        {
            s->at_state.at_tx_handler(s->at_state.at_tx_user_data,
                                      s->at_state.rx_data,
                                      s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
        }
        /*endif*/
        s->audio.bit_no = 0;
        s->audio.current_byte = 0;
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_put(void *user_data, const uint8_t buf[], int len)
{
    t31_state_t *s;
    int i;

    s = (t31_state_t *) user_data;
    if (!s->at_state.rx_signal_present)
    {
        non_ecm_rx_status(s, SIG_STATUS_TRAINING_SUCCEEDED);
        s->at_state.rx_signal_present = true;
    }
    /*endif*/
    /* Ignore any fractional bytes which may have accumulated */
    for (i = 0;  i < len;  i++)
    {
        if (buf[i] == DLE)
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
        /*endif*/
        s->at_state.rx_data[s->at_state.rx_data_bytes++] = buf[i];
        if (s->at_state.rx_data_bytes >= 250)
        {
            s->at_state.at_tx_handler(s->at_state.at_tx_user_data,
                                      s->at_state.rx_data,
                                      s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
        }
        /*endif*/
    }
    /*endfor*/
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
        if (s->non_ecm_tx.out_bytes != s->non_ecm_tx.in_bytes)
        {
            /* There is real data available to send */
            s->audio.current_byte = s->non_ecm_tx.buf[s->non_ecm_tx.out_bytes++];
            if (s->non_ecm_tx.out_bytes > T31_TX_BUF_LEN - 1)
            {
                s->non_ecm_tx.out_bytes = T31_TX_BUF_LEN - 1;
                span_log(&s->logging, SPAN_LOG_FLOW, "End of transmit buffer reached!\n");
            }
            /*endif*/
            if (s->non_ecm_tx.holding)
            {
                /* See if the buffer is approaching empty. It might be time to
                   release flow control. */
                if (s->non_ecm_tx.out_bytes > T31_TX_BUF_LOW_TIDE)
                {
                    s->non_ecm_tx.holding = false;
                    /* Tell the application to release further data */
                    at_modem_control(&s->at_state, AT_MODEM_CONTROL_CTS, (void *) 1);
                }
                /*endif*/
            }
            /*endif*/
            s->non_ecm_tx.data_started = true;
        }
        else
        {
            if (s->non_ecm_tx.final)
            {
                s->non_ecm_tx.final = false;
                /* This will put the modem into its shutdown sequence. When
                   it has finally shut down, an OK response will be sent. */
                return SIG_STATUS_END_OF_DATA;
            }
            /*endif*/
            /* Fill with 0xFF bytes at the start of transmission, or 0x00 if we are in
               the middle of transmission. This follows T.31 and T.30 practice. */
            s->audio.current_byte = (s->non_ecm_tx.data_started)  ?  0x00  :  0xFF;
        }
        /*endif*/
        s->audio.bit_no = 8;
    }
    /*endif*/
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
        if (s->non_ecm_tx.out_bytes != s->non_ecm_tx.in_bytes)
        {
            /* There is real data available to send */
            buf[i] = s->non_ecm_tx.buf[s->non_ecm_tx.out_bytes++];
            if (s->non_ecm_tx.out_bytes > T31_TX_BUF_LEN - 1)
            {
                s->non_ecm_tx.out_bytes = T31_TX_BUF_LEN - 1;
                span_log(&s->logging, SPAN_LOG_FLOW, "End of transmit buffer reached!\n");
            }
            /*endif*/
            if (s->non_ecm_tx.holding)
            {
                /* See if the buffer is approaching empty. It might be time to release flow control. */
                if (s->non_ecm_tx.out_bytes > T31_TX_BUF_LOW_TIDE)
                {
                    s->non_ecm_tx.holding = false;
                    /* Tell the application to release further data */
                    at_modem_control(&s->at_state, AT_MODEM_CONTROL_CTS, (void *) 1);
                }
                /*endif*/
            }
            /*endif*/
            s->non_ecm_tx.data_started = true;
        }
        else
        {
            if (s->non_ecm_tx.final)
            {
                s->non_ecm_tx.final = false;
                /* This will put the modem into its shutdown sequence. When
                   it has finally shut down, an OK response will be sent. */
                return i;
            }
            /*endif*/
            /* Fill with 0xFF bytes at the start of transmission, or 0x00 if we are in
               the middle of transmission. This follows T.31 and T.30 practice. */
            buf[i] = (s->non_ecm_tx.data_started)  ?  0x00  :  0xFF;
        }
        /*endif*/
    }
    /*endfor*/
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
        s->hdlc_tx.final = false;
        /* Schedule an orderly shutdown of the modem */
        hdlc_tx_frame(&s->audio.modems.hdlc_tx, NULL, 0);
    }
    else
    {
        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void hdlc_tx_underflow2(void *user_data)
{
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
        s->at_state.rx_trained = false;
        s->audio.modems.rx_trained = false;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        s->at_state.rx_signal_present = true;
        s->at_state.rx_trained = true;
        s->audio.modems.rx_trained = true;
        break;
    case SIG_STATUS_CARRIER_UP:
        if (s->modem == FAX_MODEM_CNG_TONE_TX  ||  s->modem == FAX_MODEM_NOCNG_TONE_TX  ||  s->modem == FAX_MODEM_V21_RX)
        {
            s->at_state.rx_signal_present = true;
            s->rx_frame_received = false;
            s->audio.modems.rx_frame_received = false;
        }
        /*endif*/
        break;
    case SIG_STATUS_CARRIER_DOWN:
        if (s->rx_frame_received)
        {
            if (s->at_state.dte_is_waiting)
            {
                if (s->at_state.ok_is_pending)
                {
                    at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
                    s->at_state.ok_is_pending = false;
                }
                else
                {
                    at_put_response_code(&s->at_state, AT_RESPONSE_CODE_NO_CARRIER);
                }
                /*endif*/
                s->at_state.dte_is_waiting = false;
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            }
            else
            {
                buf[0] = AT_RESPONSE_CODE_NO_CARRIER;
                queue_write_msg(s->rx_queue, buf, 1);
            }
            /*endif*/
        }
        /*endif*/
        s->at_state.rx_signal_present = false;
        s->at_state.rx_trained = false;
        s->audio.modems.rx_trained = false;
        break;
    case SIG_STATUS_FRAMING_OK:
        if (s->modem == FAX_MODEM_CNG_TONE_TX  ||  s->modem == FAX_MODEM_NOCNG_TONE_TX)
        {
            /* Once we get any valid HDLC the CNG tone stops, and we drop
               to the V.21 receive modem on its own. */
            s->modem = FAX_MODEM_V21_RX;
            s->at_state.transmit = false;
        }
        /*endif*/
        if (s->modem == FAX_MODEM_V17_RX  ||  s->modem == FAX_MODEM_V27TER_RX  ||  s->modem == FAX_MODEM_V29_RX)
        {
            /* V.21 has been detected while expecting a different carrier.
               If +FAR=0 then result +FCERROR and return to command-mode.
               If +FAR=1 then report +FRH:3 and CONNECT, switching to
               V.21 receive mode. */
            if (s->at_state.p.adaptive_receive)
            {
                s->at_state.rx_signal_present = true;
                s->rx_frame_received = true;
                s->audio.modems.rx_frame_received = true;
                s->modem = FAX_MODEM_V21_RX;
                s->at_state.transmit = false;
                s->at_state.dte_is_waiting = true;
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_FRH3);
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
            }
            else
            {
                s->modem = FAX_MODEM_SILENCE_TX;
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
                s->rx_frame_received = false;
                s->audio.modems.rx_frame_received = false;
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_FCERROR);
            }
            /*endif*/
        }
        else
        {
            if (!s->rx_frame_received)
            {
                if (s->at_state.dte_is_waiting)
                {
                    /* Report CONNECT as soon as possible to avoid a timeout. */
                    at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
                    s->rx_frame_received = true;
                    s->audio.modems.rx_frame_received = true;
                }
                else
                {
                    buf[0] = AT_RESPONSE_CODE_CONNECT;
                    queue_write_msg(s->rx_queue, buf, 1);
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
        break;
    case SIG_STATUS_ABORT:
        /* Just ignore these */
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected HDLC rx status - %d!\n", status);
        break;
    }
    /*endswitch*/
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
    /*endif*/
    s = (t31_state_t *) user_data;
    if (!s->rx_frame_received)
    {
        if (s->at_state.dte_is_waiting)
        {
            /* Report CONNECT as soon as possible to avoid a timeout. */
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
            s->rx_frame_received = true;
            s->audio.modems.rx_frame_received = true;
        }
        else
        {
            buf[0] = AT_RESPONSE_CODE_CONNECT;
            queue_write_msg(s->rx_queue, buf, 1);
        }
        /*endif*/
    }
    /*endif*/
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
                /*endif*/
                s->at_state.rx_data[s->at_state.rx_data_bytes++] = msg[i];
            }
            /*endfor*/
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = ETX;
            s->at_state.at_tx_handler(s->at_state.at_tx_user_data, s->at_state.rx_data, s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
            if (msg[1] == 0x13  &&  ok)
            {
                /* This is the last frame.  We don't send OK until the carrier drops to avoid
                   redetecting it later. */
                s->at_state.ok_is_pending = true;
            }
            else
            {
                at_put_response_code(&s->at_state, (ok)  ?  AT_RESPONSE_CODE_OK  :  AT_RESPONSE_CODE_ERROR);
                s->at_state.dte_is_waiting = false;
                s->rx_frame_received = false;
                s->audio.modems.rx_frame_received = false;
            }
            /*endif*/
        }
        else
        {
            /* Queue it */
            buf[0] = (ok)  ?  AT_RESPONSE_CODE_OK  :  AT_RESPONSE_CODE_ERROR;
            /* It is safe to look at the two bytes beyond the length of the message,
               and expect to find the FCS there. */
            memcpy(&buf[1], msg, len + 2);
            queue_write_msg(s->rx_queue, buf, len + 3);
        }
        /*endif*/
    }
    /*endif*/
    t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept_t38_frame(void *user_data, const uint8_t *msg, int len, int ok)
{
    t31_state_t *s;
    int i;
    int byte_in_progress;
    int txbyte;
    int pos;
    int ptr;
    uint16_t crc;
#if defined(_MSC_VER)
    uint8_t *buf2 = (uint8_t *) _alloca(2*len + 20);
#else
    uint8_t buf2[2*len + 20];
#endif

    /* Accept an ECM image mode HDLC frame, received as T.38, and convert to an HDLC
       bit stream to be fed to the FAX software. */
    if (len < 0)
        return;
    /*endif*/
    s = (t31_state_t  *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Accept2 %d %d\n", len, ok);
    crc = crc_itu16_calc(msg, len, 0xFFFF);
    /* If the frame is not good, don't flip the CRC to the correct value */
    if (ok)
        crc ^= 0xFFFF;
    /*endif*/
    ptr = 0;
    buf2[ptr++] = s->t38_fe.hdlc_tx_non_ecm.idle_octet;
    buf2[ptr++] = s->t38_fe.hdlc_tx_non_ecm.idle_octet;
    for (pos = 0;  pos < len;  pos++)
    {
        byte_in_progress = msg[pos];
        i = bottom_bit(byte_in_progress | 0x100);
        s->t38_fe.hdlc_tx_non_ecm.octets_in_progress <<= i;
        byte_in_progress >>= i;
        for (  ;  i < 8;  i++)
        {
            s->t38_fe.hdlc_tx_non_ecm.octets_in_progress = (s->t38_fe.hdlc_tx_non_ecm.octets_in_progress << 1) | (byte_in_progress & 0x01);
            byte_in_progress >>= 1;
            if ((s->t38_fe.hdlc_tx_non_ecm.octets_in_progress & 0x1F) == 0x1F)
            {
                /* There are 5 ones - stuff */
                s->t38_fe.hdlc_tx_non_ecm.octets_in_progress <<= 1;
                s->t38_fe.hdlc_tx_non_ecm.num_bits++;
            }
            /*endif*/
        }
        /*endfor*/
        /* An input byte will generate between 8 and 10 output bits */
        buf2[ptr++] = (s->t38_fe.hdlc_tx_non_ecm.octets_in_progress >> s->t38_fe.hdlc_tx_non_ecm.num_bits) & 0xFF;
        if (s->t38_fe.hdlc_tx_non_ecm.num_bits >= 8)
        {
            s->t38_fe.hdlc_tx_non_ecm.num_bits -= 8;
            buf2[ptr++] = (s->t38_fe.hdlc_tx_non_ecm.octets_in_progress >> s->t38_fe.hdlc_tx_non_ecm.num_bits) & 0xFF;
        }
        /*endif*/
    }
    /*endfor*/

    for (pos = 0;  pos < 2;  pos++)
    {
        byte_in_progress = crc & 0xFF;
        crc >>= 8;
        i = bottom_bit(byte_in_progress | 0x100);
        s->t38_fe.hdlc_tx_non_ecm.octets_in_progress <<= i;
        byte_in_progress >>= i;
        for (  ;  i < 8;  i++)
        {
            s->t38_fe.hdlc_tx_non_ecm.octets_in_progress = (s->t38_fe.hdlc_tx_non_ecm.octets_in_progress << 1) | (byte_in_progress & 0x01);
            byte_in_progress >>= 1;
            if ((s->t38_fe.hdlc_tx_non_ecm.octets_in_progress & 0x1F) == 0x1F)
            {
                /* There are 5 ones - stuff */
                s->t38_fe.hdlc_tx_non_ecm.octets_in_progress <<= 1;
                s->t38_fe.hdlc_tx_non_ecm.num_bits++;
            }
            /*endif*/
        }
        /*endfor*/
        /* An input byte will generate between 8 and 10 output bits */
        buf2[ptr++] = (s->t38_fe.hdlc_tx_non_ecm.octets_in_progress >> s->t38_fe.hdlc_tx_non_ecm.num_bits) & 0xFF;
        if (s->t38_fe.hdlc_tx_non_ecm.num_bits >= 8)
        {
            s->t38_fe.hdlc_tx_non_ecm.num_bits -= 8;
            buf2[ptr++] = (s->t38_fe.hdlc_tx_non_ecm.octets_in_progress >> s->t38_fe.hdlc_tx_non_ecm.num_bits) & 0xFF;
        }
        /*endif*/
    }
    /*endif*/

    /* Finish off the current byte with some flag bits. If we are at the
       start of a byte we need a at least one whole byte of flag to ensure
       we cannot end up with back to back frames, and no flag octet at all */
    txbyte = (uint8_t) ((s->t38_fe.hdlc_tx_non_ecm.octets_in_progress << (8 - s->t38_fe.hdlc_tx_non_ecm.num_bits)) | (0x7E >> s->t38_fe.hdlc_tx_non_ecm.num_bits));
    /* Create a rotated octet of flag for idling... */
    s->t38_fe.hdlc_tx_non_ecm.idle_octet = (0x7E7E >> s->t38_fe.hdlc_tx_non_ecm.num_bits) & 0xFF;
    /* ...and the partial flag octet needed to start off the next message. */
    s->t38_fe.hdlc_tx_non_ecm.octets_in_progress = s->t38_fe.hdlc_tx_non_ecm.idle_octet >> (8 - s->t38_fe.hdlc_tx_non_ecm.num_bits);
    buf2[ptr++] = txbyte;

    buf2[ptr++] = s->t38_fe.hdlc_tx_non_ecm.idle_octet;
    buf2[ptr++] = s->t38_fe.hdlc_tx_non_ecm.idle_octet;
    bit_reverse(buf2, buf2, ptr);
    non_ecm_put(s, buf2, ptr);
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept_non_ecm_frame(void *user_data, const uint8_t *msg, int len, int ok)
{
    t31_state_t *s;

    /* Accept an ECM image mode HDLC frame received as a bit stream from the FAX software,
       and to be send as T.38 HDLC data. */
    if (len < 0)
        return;
    /*endif*/
    s = (t31_state_t *) user_data;
    memcpy(s->t38_fe.hdlc_from_t31.buf[s->t38_fe.hdlc_from_t31.in].buf, msg, len);
    s->t38_fe.hdlc_from_t31.buf[s->t38_fe.hdlc_from_t31.in].len = len;
    if (++s->t38_fe.hdlc_from_t31.in >= T31_TX_HDLC_BUFS)
        s->t38_fe.hdlc_from_t31.in = 0;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void t31_v21_rx(t31_state_t *s)
{
    s->at_state.ok_is_pending = false;
    s->hdlc_tx.len = 0;
    s->hdlc_tx.final = false;
    s->dled = false;
    fax_modems_start_slow_modem(&s->audio.modems, FAX_MODEM_V21_RX);
    hdlc_rx_init(&s->audio.modems.hdlc_rx, false, true, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept_frame, s);
    s->at_state.transmit = true;
}
/*- End of function --------------------------------------------------------*/

static int restart_modem(t31_state_t *s, int new_modem)
{
    int use_hdlc;
    int res;
    fax_modems_state_t *t;

    t = &s->audio.modems;
    span_log(&s->logging, SPAN_LOG_FLOW, "Restart modem %d\n", new_modem);
    if (s->modem == new_modem)
        return 0;
    /*endif*/
    queue_flush(s->rx_queue);
    s->modem = new_modem;
    s->non_ecm_tx.final = false;
    s->at_state.rx_signal_present = false;
    s->at_state.rx_trained = false;
    s->audio.modems.rx_trained = false;
    s->rx_frame_received = false;
    s->audio.modems.rx_frame_received = false;
    fax_modems_set_rx_handler(t, (span_rx_handler_t) &span_dummy_rx, NULL, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
    use_hdlc = false;
    switch (s->modem)
    {
    case FAX_MODEM_CNG_TONE_TX:
        if (s->t38_mode)
        {
            s->t38_fe.next_tx_samples = s->t38_fe.samples;
            s->t38_fe.timed_step = T38_TIMED_STEP_CNG;
            s->t38_fe.current_tx_data_type = T38_DATA_NONE;
        }
        else
        {
            fax_modems_start_slow_modem(t, FAX_MODEM_CNG_TONE_TX);
            /* CNG is special, since we need to receive V.21 HDLC messages while sending the
               tone. Everything else in FAX processing sends only one way at a time. */
            /* Do V.21/HDLC receive in parallel. The other end may send its
               first message at any time. The CNG tone will continue until
               we get a valid preamble. */
            t31_v21_rx(s);
            fax_modems_set_rx_handler(t, (span_rx_handler_t) &initial_timed_rx, s, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        }
        /*endif*/
        s->at_state.transmit = true;
        break;
    case FAX_MODEM_NOCNG_TONE_TX:
        if (s->t38_mode)
        {
        }
        else
        {
            t31_v21_rx(s);
            fax_modems_set_rx_handler(t, (span_rx_handler_t) &initial_timed_rx, s, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
            silence_gen_set(&t->silence_gen, 0);
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        }
        /*endif*/
        s->at_state.transmit = false;
        break;
    case FAX_MODEM_CED_TONE_TX:
        if (s->t38_mode)
        {
            s->t38_fe.next_tx_samples = s->t38_fe.samples;
            s->t38_fe.timed_step = T38_TIMED_STEP_CED;
            s->t38_fe.current_tx_data_type = T38_DATA_NONE;
        }
        else
        {
            fax_modems_start_slow_modem(t, FAX_MODEM_CED_TONE_TX);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        }
        /*endif*/
        s->at_state.transmit = true;
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
        /*endif*/
        break;
    case FAX_MODEM_V21_TX:
        if (s->t38_mode)
        {
            s->t38_fe.next_tx_indicator = T38_IND_V21_PREAMBLE;
            s->t38_fe.current_tx_data_type = T38_DATA_V21;
            s->t38_fe.timed_step = T38_TIMED_STEP_HDLC_MODEM;
            set_octets_per_data_packet(s, 300);
        }
        else
        {
            hdlc_tx_init(&t->hdlc_tx, false, 2, false, hdlc_tx_underflow, s);
            /* The spec says 1s +-15% of preamble. So, the minimum is 32 octets. */
            hdlc_tx_flags(&t->hdlc_tx, 32);
            fax_modems_start_slow_modem(t, FAX_MODEM_V21_TX);
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &fsk_tx, &t->v21_tx);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        }
        /*endif*/
        s->hdlc_tx.len = 0;
        s->hdlc_tx.final = false;
        s->dled = false;
        s->at_state.transmit = true;
        break;
    case FAX_MODEM_V17_RX:
    case FAX_MODEM_V27TER_RX:
    case FAX_MODEM_V29_RX:
        if (!s->t38_mode)
        {
            /* Allow for +FCERROR/+FRH:3 */
            t31_v21_rx(s);
            fax_modems_start_fast_modem(t, s->modem, s->bit_rate, s->short_train, use_hdlc);
        }
        /*endif*/
        s->at_state.transmit = false;
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
            /*endswitch*/
            set_octets_per_data_packet(s, s->bit_rate);
            s->t38_fe.timed_step = (s->t38_fe.ecm_mode == 2)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        }
        else
        {
            fax_modems_start_fast_modem(t, s->modem, s->bit_rate, s->short_train, use_hdlc);
        }
        /*endif*/
        s->non_ecm_tx.out_bytes = 0;
        s->non_ecm_tx.data_started = false;
        s->at_state.transmit = true;
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
            /*endswitch*/
            set_octets_per_data_packet(s, s->bit_rate);
            s->t38_fe.timed_step = (s->t38_fe.ecm_mode == 2)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        }
        else
        {
            fax_modems_start_fast_modem(t, s->modem, s->bit_rate, s->short_train, use_hdlc);
        }
        /*endif*/
        s->non_ecm_tx.out_bytes = 0;
        s->non_ecm_tx.data_started = false;
        s->at_state.transmit = true;
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
            /*endswitch*/
            set_octets_per_data_packet(s, s->bit_rate);
            s->t38_fe.timed_step = (s->t38_fe.ecm_mode == 2)  ?  T38_TIMED_STEP_HDLC_MODEM  :  T38_TIMED_STEP_NON_ECM_MODEM;
        }
        else
        {
            fax_modems_start_fast_modem(t, s->modem, s->bit_rate, s->short_train, use_hdlc);
        }
        /*endif*/
        s->non_ecm_tx.out_bytes = 0;
        s->non_ecm_tx.data_started = false;
        s->at_state.transmit = true;
        break;
    case FAX_MODEM_SILENCE_TX:
        if (s->t38_mode)
        {
            if ((res = t38_core_send_indicator(&s->t38_fe.t38, T38_IND_NO_SIGNAL)) < 0)
                return res;
            /*endif*/
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
        /*endif*/
        s->at_state.transmit = false;
        break;
    case FAX_MODEM_SILENCE_RX:
        if (!s->t38_mode)
        {
            fax_modems_set_rx_handler(t, (span_rx_handler_t) &silence_rx, s, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
            silence_gen_set(&t->silence_gen, 0);
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
        }
        /*endif*/
        s->at_state.transmit = false;
        break;
    case FAX_MODEM_FLUSH:
        /* Send 200ms of silence to "push" the last audio out */
        if (s->t38_mode)
        {
            if ((res = t38_core_send_indicator(&s->t38_fe.t38, T38_IND_NO_SIGNAL)) < 0)
                return res;
            /*endif*/
        }
        else
        {
            s->modem = FAX_MODEM_SILENCE_TX;
            silence_gen_alter(&t->silence_gen, ms_to_samples(200));
            fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
            fax_modems_set_next_tx_handler(t, (span_tx_handler_t) NULL, NULL);
            s->at_state.transmit = true;
        }
        /*endif*/
        break;
    }
    /*endswitch*/
    s->audio.bit_no = 0;
    s->audio.current_byte = 0xFF;
    s->non_ecm_tx.in_bytes = 0;
    s->non_ecm_tx.out_bytes = 0;
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
            s->dled = false;
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
                /*endif*/
            }
            else if (s->at_state.p.double_escape  &&  stuffed[i] == SUB)
            {
                s->hdlc_tx.buf[s->hdlc_tx.len++] = DLE;
                s->hdlc_tx.buf[s->hdlc_tx.len++] = DLE;
            }
            else
            {
                s->hdlc_tx.buf[s->hdlc_tx.len++] = stuffed[i];
            }
            /*endif*/
        }
        else
        {
            if (stuffed[i] == DLE)
                s->dled = true;
            else
                s->hdlc_tx.buf[s->hdlc_tx.len++] = stuffed[i];
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static __inline__ void dle_unstuff_fake_hdlc(t31_state_t *s, const char *stuffed, int len)
{
    int i;

    for (i = 0;  i < len;  i++)
    {
        if (s->dled)
        {
            s->dled = false;
            if (stuffed[i] == ETX)
            {
                s->non_ecm_tx.final = true;
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
                return;
            }
            else if (s->at_state.p.double_escape  &&  stuffed[i] == SUB)
            {
                hdlc_rx_put_byte(&s->t38_fe.hdlc_rx_non_ecm, bit_reverse8(DLE));
                hdlc_rx_put_byte(&s->t38_fe.hdlc_rx_non_ecm, bit_reverse8(DLE));
            }
            else
            {
                hdlc_rx_put_byte(&s->t38_fe.hdlc_rx_non_ecm, bit_reverse8(stuffed[i]));
            }
            /*endif*/
        }
        else
        {
            if (stuffed[i] == DLE)
                s->dled = true;
            else
                hdlc_rx_put_byte(&s->t38_fe.hdlc_rx_non_ecm, bit_reverse8(stuffed[i]));
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static __inline__ void dle_unstuff(t31_state_t *s, const char *stuffed, int len)
{
    int i;

    for (i = 0;  i < len;  i++)
    {
        if (s->dled)
        {
            s->dled = false;
            if (stuffed[i] == ETX)
            {
                s->non_ecm_tx.final = true;
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
                return;
            }
            /*endif*/
            if (s->at_state.p.double_escape  &&  stuffed[i] == SUB)
            {
                s->non_ecm_tx.buf[s->non_ecm_tx.in_bytes++] = DLE;
                s->non_ecm_tx.buf[s->non_ecm_tx.in_bytes++] = DLE;
            }
            else
            {
                s->non_ecm_tx.buf[s->non_ecm_tx.in_bytes++] = stuffed[i];
            }
            /*endif*/
        }
        else
        {
            if (stuffed[i] == DLE)
                s->dled = true;
            else
                s->non_ecm_tx.buf[s->non_ecm_tx.in_bytes++] = stuffed[i];
            /*endif*/
        }
        /*endif*/
        if (s->non_ecm_tx.in_bytes > T31_TX_BUF_LEN - 2)
        {
            /* Oops. We hit the end of the buffer. Give up. Loose stuff. :-( */
            span_log(&s->logging, SPAN_LOG_FLOW, "No room in buffer for new data!\n");
            return;
        }
        /*endif*/
    }
    /*endfor*/
    if (!s->non_ecm_tx.holding)
    {
        /* See if the buffer is approaching full. We might need to apply flow control. */
        if (s->non_ecm_tx.in_bytes > T31_TX_BUF_HIGH_TIDE)
        {
            s->non_ecm_tx.holding = true;
            /* Tell the application to hold further data */
            at_modem_control(&s->at_state, AT_MODEM_CONTROL_CTS, (void *) 0);
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int process_class1_cmd(void *user_data, int direction, int operation, int val)
{
    int new_modem;
    int new_transmit;
    int i;
    int len;
    t31_state_t *s;
    uint8_t msg[256];

    s = (t31_state_t *) user_data;
    new_transmit = direction;
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
            /*endif*/
            s->at_state.transmit = true;
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
            /*endif*/
        }
        /*endif*/
        span_log(&s->logging, SPAN_LOG_FLOW, "Silence %dms\n", val*10);
        break;
    case 'H':
        switch (val)
        {
        case 3:
            new_modem = (new_transmit)  ?  FAX_MODEM_V21_TX  :  FAX_MODEM_V21_RX;
            s->short_train = false;
            s->bit_rate = 300;
            break;
        default:
            return -1;
        }
        /*endswitch*/
        span_log(&s->logging, SPAN_LOG_FLOW, "HDLC\n");
        if (new_modem != s->modem)
            restart_modem(s, new_modem);
        /*endif*/
        s->at_state.transmit = new_transmit;
        if (new_transmit)
        {
            t31_set_at_rx_mode(s, AT_MODE_HDLC);
            if (!s->t38_mode)
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
            /*endif*/
        }
        else
        {
            /* Send straight away, if there is something queued. */
            t31_set_at_rx_mode(s, AT_MODE_DELIVERY);
            s->rx_frame_received = false;
            s->audio.modems.rx_frame_received = false;
            do
            {
                if (!queue_empty(s->rx_queue))
                {
                    len = queue_read_msg(s->rx_queue, msg, 256);
                    if (len > 1)
                    {
                        if (msg[0] == AT_RESPONSE_CODE_OK)
                            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
                        /*endif*/
                        for (i = 1;  i < len;  i++)
                        {
                            if (msg[i] == DLE)
                                s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
                            /*endif*/
                            s->at_state.rx_data[s->at_state.rx_data_bytes++] = msg[i];
                        }
                        /*endfor*/
                        s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
                        s->at_state.rx_data[s->at_state.rx_data_bytes++] = ETX;
                        s->at_state.at_tx_handler(s->at_state.at_tx_user_data, s->at_state.rx_data, s->at_state.rx_data_bytes);
                        s->at_state.rx_data_bytes = 0;
                    }
                    /*endif*/
                    at_put_response_code(&s->at_state, msg[0]);
                }
                else
                {
                    s->at_state.dte_is_waiting = true;
                    break;
                }
                /*endif*/
            }
            while (msg[0] == AT_RESPONSE_CODE_CONNECT);
        }
        /*endif*/
        break;
    default:
        switch (val)
        {
        case 24:
            s->t38_fe.next_tx_indicator = T38_IND_V27TER_2400_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V27TER_2400;
            new_modem = (new_transmit)  ?  FAX_MODEM_V27TER_TX  :  FAX_MODEM_V27TER_RX;
            s->short_train = false;
            s->bit_rate = 2400;
            break;
        case 48:
            s->t38_fe.next_tx_indicator = T38_IND_V27TER_4800_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V27TER_4800;
            new_modem = (new_transmit)  ?  FAX_MODEM_V27TER_TX  :  FAX_MODEM_V27TER_RX;
            s->short_train = false;
            s->bit_rate = 4800;
            break;
        case 72:
            s->t38_fe.next_tx_indicator = T38_IND_V29_7200_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V29_7200;
            new_modem = (new_transmit)  ?  FAX_MODEM_V29_TX  :  FAX_MODEM_V29_RX;
            s->short_train = false;
            s->bit_rate = 7200;
            break;
        case 96:
            s->t38_fe.next_tx_indicator = T38_IND_V29_9600_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V29_9600;
            new_modem = (new_transmit)  ?  FAX_MODEM_V29_TX  :  FAX_MODEM_V29_RX;
            s->short_train = false;
            s->bit_rate = 9600;
            break;
        case 73:
            s->t38_fe.next_tx_indicator = T38_IND_V17_7200_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_7200;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = false;
            s->bit_rate = 7200;
            break;
        case 74:
            s->t38_fe.next_tx_indicator = T38_IND_V17_7200_SHORT_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_7200;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = true;
            s->bit_rate = 7200;
            break;
        case 97:
            s->t38_fe.next_tx_indicator = T38_IND_V17_9600_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_9600;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = false;
            s->bit_rate = 9600;
            break;
        case 98:
            s->t38_fe.next_tx_indicator = T38_IND_V17_9600_SHORT_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_9600;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = true;
            s->bit_rate = 9600;
            break;
        case 121:
            s->t38_fe.next_tx_indicator = T38_IND_V17_12000_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_12000;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = false;
            s->bit_rate = 12000;
            break;
        case 122:
            s->t38_fe.next_tx_indicator = T38_IND_V17_12000_SHORT_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_12000;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = true;
            s->bit_rate = 12000;
            break;
        case 145:
            s->t38_fe.next_tx_indicator = T38_IND_V17_14400_LONG_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_14400;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = false;
            s->bit_rate = 14400;
            break;
        case 146:
            s->t38_fe.next_tx_indicator = T38_IND_V17_14400_SHORT_TRAINING;
            s->t38_fe.current_tx_data_type = T38_DATA_V17_14400;
            new_modem = (new_transmit)  ?  FAX_MODEM_V17_TX  :  FAX_MODEM_V17_RX;
            s->short_train = true;
            s->bit_rate = 14400;
            break;
        default:
            return -1;
        }
        /*endswitch*/
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
        /*endif*/
        restart_modem(s, new_modem);
        break;
    }
    /*endswitch*/
    return false;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_call_event(t31_state_t *s, int event)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Call event %s (%d) received\n", at_call_state_to_str(event), event);
    at_call_event(&s->at_state, event);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_at_rx_free_space(t31_state_t *s)
{
    return T31_TX_BUF_LEN - (s->non_ecm_tx.in_bytes - s->non_ecm_tx.out_bytes) - 1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_at_rx(t31_state_t *s, const char *t, int len)
{
    if (s->dte_data_timeout)
        s->dte_data_timeout = s->call_samples + ms_to_samples(5000);
    /*endif*/
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
                s->at_state.at_tx_handler(s->at_state.at_tx_user_data, s->at_state.rx_data, s->at_state.rx_data_bytes);
            }
            /*endif*/
            s->at_state.rx_data_bytes = 0;
            s->at_state.transmit = false;
            s->modem = FAX_MODEM_SILENCE_TX;
            fax_modems_set_rx_handler(&s->audio.modems, (span_rx_handler_t) &span_dummy_rx, NULL, (span_rx_fillin_handler_t) &span_dummy_rx_fillin, NULL);
            t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
        }
        /*endif*/
        break;
    case AT_MODE_HDLC:
        dle_unstuff_hdlc(s, t, len);
        break;
    case AT_MODE_STUFFED:
        if (s->non_ecm_tx.out_bytes)
        {
            /* Make room for new data in existing data buffer. */
            s->non_ecm_tx.in_bytes -= s->non_ecm_tx.out_bytes;
            memmove(&s->non_ecm_tx.buf[0], &s->non_ecm_tx.buf[s->non_ecm_tx.out_bytes], s->non_ecm_tx.in_bytes);
            s->non_ecm_tx.out_bytes = 0;
        }
        /*endif*/
        if (s->t38_fe.ecm_mode == 2)
            dle_unstuff_fake_hdlc(s, t, len);
        else
            dle_unstuff(s, t, len);
        /*endif*/
        break;
    case AT_MODE_CONNECTED:
        /* TODO: Implement for data modem operation */
        break;
    }
    /*endswitch*/
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
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int initial_timed_rx(void *user_data, const int16_t amp[], int len)
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
        return 0;
    }
    fsk_rx(&s->audio.modems.v21_rx, amp, len);
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
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/

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
    /*endif*/

    if (s->audio.modems.rx_handler)
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
    /*endif*/

    s->audio.modems.rx_fillin_handler(s->audio.modems.rx_fillin_user_data, len);
    return 0;
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
            fax_modems_set_next_tx_type(&s->audio.modems);
            if ((len += s->audio.modems.tx_handler(s->audio.modems.tx_user_data, &amp[len], max_len - len)) < max_len)
                front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
            /*endif*/
        }
        /*endif*/
    }
    /*endif*/
    if (s->audio.modems.transmit_on_idle)
    {
        /* Pad to the requested length with silence */
        vec_zeroi16(&amp[len], max_len - len);
        len = max_len;
    }
    /*endif*/
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_set_transmit_on_idle(t31_state_t *s, bool transmit_on_idle)
{
    s->audio.modems.transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_set_tep_mode(t31_state_t *s, bool use_tep)
{
    s->audio.modems.use_tep = use_tep;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_set_t38_config(t31_state_t *s, bool without_pacing)
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
        s->t38_fe.us_per_tx_chunk = 0;
    }
    else
    {
        /* Paced streaming mode, as used for UDP transports */
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_INDICATOR, INDICATOR_TX_COUNT);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_CONTROL_DATA, DATA_TX_COUNT);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_CONTROL_DATA_END, DATA_END_TX_COUNT);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_IMAGE_DATA, DATA_TX_COUNT);
        t38_set_redundancy_control(&s->t38_fe.t38, T38_PACKET_CATEGORY_IMAGE_DATA_END, DATA_END_TX_COUNT);
        s->t38_fe.us_per_tx_chunk = US_PER_TX_CHUNK;
    }
    /*endif*/
    set_octets_per_data_packet(s, 300);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t31_set_mode(t31_state_t *s, bool t38_mode)
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

SPAN_DECLARE(at_state_t *) t31_get_at_state(t31_state_t *s)
{
    return &s->at_state;
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

    /* Prepare the non-ecm HDLC bit stream -> T.38 HDLC -> non-ecm HDLC bit stream path */
    hdlc_tx_init(&s->hdlc_tx_non_ecm, false, 1, false, hdlc_tx_underflow2, s);
    hdlc_rx_init(&s->hdlc_rx_non_ecm, false, true, 2, hdlc_accept_non_ecm_frame, t);
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
    /*endif*/

    alloced = false;
    if (s == NULL)
    {
        if ((s = (t31_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
        alloced = true;
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.31");

    s->modem_control_handler = modem_control_handler;
    s->modem_control_user_data = modem_control_user_data;
    fax_modems_init(&s->audio.modems,
                    false,
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
    v8_init(&s->audio.v8, false, &v8_parms, v8_handler, s);

    power_meter_init(&s->audio.rx_power, 4);
    s->audio.last_sample = 0;
    s->audio.silence_threshold_power = power_meter_level_dbm0(-36);
    s->at_state.rx_signal_present = false;
    s->at_state.rx_trained = false;
    s->audio.modems.rx_trained = false;

    s->at_state.do_hangup = false;
    s->at_state.line_ptr = 0;
    s->audio.silence_heard = 0;
    s->silence_awaited = 0;
    s->call_samples = 0;
    s->modem = FAX_MODEM_NONE;
    s->at_state.transmit = true;

    if (s->rx_queue)
        queue_free(s->rx_queue);
    if ((s->rx_queue = queue_init(NULL, 4096, QUEUE_WRITE_ATOMIC | QUEUE_READ_ATOMIC)) == NULL)
    {
        if (alloced)
            span_free(s);
        /*endif*/
        return NULL;
    }
    /*endif*/
    at_init(&s->at_state, at_tx_handler, at_tx_user_data, t31_modem_control_handler, s);
    at_set_class1_handler(&s->at_state, process_class1_cmd, s);
    s->at_state.dte_inactivity_timeout = DEFAULT_DTE_TIMEOUT;
    if (tx_t38_packet_handler)
    {
        t31_t38_fe_init(s, tx_t38_packet_handler, tx_t38_packet_user_data);
        t31_set_t38_config(s, false);
    }
    /*endif*/
    s->t38_mode = false;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_release(t31_state_t *s)
{
    at_reset_call_info(&s->at_state);
    v8_release(&s->audio.v8);
    fax_modems_release(&s->audio.modems);
    queue_free(s->rx_queue);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t31_free(t31_state_t *s)
{
    t31_release(s);
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
