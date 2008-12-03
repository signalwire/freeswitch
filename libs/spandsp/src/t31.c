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
 * Copyright (C) 2004, 2005, 2006 Steve Underwood
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
 * $Id: t31.c,v 1.120 2008/09/07 12:45:17 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>
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
#include "spandsp/dc_restore.h"
#include "spandsp/queue.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/silence_gen.h"
#include "spandsp/fsk.h"
#include "spandsp/v29rx.h"
#include "spandsp/v29tx.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/v17rx.h"
#include "spandsp/v17tx.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/t4.h"
#include "spandsp/t30.h"
#include "spandsp/t30_logging.h"
#include "spandsp/t38_core.h"

#include "spandsp/at_interpreter.h"
#include "spandsp/fax_modems.h"
#include "spandsp/t31.h"

/* Settings suitable for paced transmission over a UDP transport */
#define MS_PER_TX_CHUNK                 30
#define INDICATOR_TX_COUNT              3
#define DATA_TX_COUNT                   1
#define DATA_END_TX_COUNT               3
#define DEFAULT_DTE_TIMEOUT             5

/* Settings suitable for unpaced transmission over a TCP transport */
#define MAX_OCTETS_PER_UNPACED_CHUNK    300

/* Backstop timeout if reception of packets stops in the middle of a burst */
#define MID_RX_TIMEOUT                  15000

#define HDLC_FRAMING_OK_THRESHOLD       5

typedef const char *(*at_cmd_service_t)(t31_state_t *s, const char *cmd);

#define ETX 0x03
#define DLE 0x10
#define SUB 0x1A

/* BEWARE: right now this must match up with a list in the AT interpreter code. */
enum
{
    T31_NONE = -1,
    T31_FLUSH = 0,
    T31_SILENCE_TX,
    T31_SILENCE_RX,
    T31_CED_TONE,
    T31_CNG_TONE,
    T31_NOCNG_TONE,
    T31_V21_TX,
    T31_V17_TX,
    T31_V27TER_TX,
    T31_V29_TX,
    T31_V21_RX,
    T31_V17_RX,
    T31_V27TER_RX,
    T31_V29_RX
};

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

static int restart_modem(t31_state_t *s, int new_modem);
static void hdlc_accept(void *user_data, const uint8_t *msg, int len, int ok);
static int v17_v21_rx(void *user_data, const int16_t amp[], int len);
static int v27ter_v21_rx(void *user_data, const int16_t amp[], int len);
static int v29_v21_rx(void *user_data, const int16_t amp[], int len);
static int silence_rx(void *user_data, const int16_t amp[], int len);
static int cng_rx(void *user_data, const int16_t amp[], int len);

static __inline__ void t31_set_at_rx_mode(t31_state_t *s, int new_mode)
{
    s->at_state.at_rx_mode = new_mode;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_missing(t38_core_state_t *t, void *user_data, int rx_seq_no, int expected_seq_no)
{
    t31_state_t *s;
    
    s = (t31_state_t *) user_data;
    s->t38_fe.missing_data = TRUE;
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
        /* This is probably due to the far end repeating itself. Ignore it. Its harmless */
        return 0;
    }
    switch (indicator)
    {
    case T38_IND_NO_SIGNAL:
        if (t->current_rx_indicator == T38_IND_V21_PREAMBLE
            &&
            (fe->current_rx_type == T30_MODEM_V21  ||  fe->current_rx_type == T30_MODEM_CNG))
        {
            /* TODO: report carrier down */
        }
        fe->timeout_rx_samples = 0;
        /* TODO: report end of signal */
        break;
    case T38_IND_CNG:
        break;
    case T38_IND_CED:
        break;
    case T38_IND_V21_PREAMBLE:
        /* Some people pop these preamble indicators between HDLC frames, so we need to be tolerant of that. */
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        /* TODO: report signal present */
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
        fe->timeout_rx_samples = fe->samples + ms_to_samples(MID_RX_TIMEOUT);
        /* TODO: report signal present */
        break;
    case T38_IND_V8_ANSAM:
    case T38_IND_V8_SIGNAL:
    case T38_IND_V34_CNTL_CHANNEL_1200:
    case T38_IND_V34_PRI_CHANNEL:
    case T38_IND_V34_CC_RETRAIN:
        /* TODO: report signal present */
        break;
    default:
        /* TODO: report end of signal */
        break;
    }
    fe->hdlc_rx.len = 0;
    fe->missing_data = FALSE;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int process_rx_data(t38_core_state_t *t, void *user_data, int data_type, int field_type, const uint8_t *buf, int len)
{
    t31_state_t *s;
    t31_t38_front_end_state_t *fe;
#if defined(_MSC_VER)
    uint8_t *buf2 = (uint8_t *) alloca(len);
#else
    uint8_t buf2[len];
#endif

    s = (t31_state_t *) user_data;
    fe = &s->t38_fe;
#if 0
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
            /* TODO: report signal present */
            /* All real HDLC messages in the FAX world start with 0xFF. If this one is not starting
               with 0xFF it would appear some octets must have been missed before this one. */
            if (buf[0] != 0xFF)
                fe->missing_data = TRUE;
        }
        if (fe->hdlc_rx.len + len <= T31_T38_MAX_HDLC_LEN)
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
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC OK (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->missing_data)  ?  "missing octets"  :  "clean");
            hdlc_accept((void *) s, fe->hdlc_rx.buf, fe->hdlc_rx.len, !fe->missing_data);
        }
        fe->hdlc_rx.len = 0;
        fe->missing_data = FALSE;
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
            span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC bad (%s)\n", (fe->hdlc_rx.len >= 3)  ?  t30_frametype(fe->hdlc_rx.buf[2])  :  "???", (fe->missing_data)  ?  "missing octets"  :  "clean");
            hdlc_accept((void *) s, fe->hdlc_rx.buf, fe->hdlc_rx.len, FALSE);
        }
        fe->hdlc_rx.len = 0;
        fe->missing_data = FALSE;
        break;
    case T38_FIELD_HDLC_FCS_OK_SIG_END:
        if (len > 0)
        {
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_OK_SIG_END!\n");
            /* The sender has incorrectly included data in this message. It is unclear what we should do
               with it, to maximise tolerance of buggy implementations. */
        }
        span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC OK, sig end (%s)\n", t30_frametype(s->tx.data[2]), (fe->missing_data)  ?  "missing octets"  :  "clean");
        if (fe->current_rx_type == T31_V21_RX)
        {
            /* Don't deal with zero length frames. Some T.38 implementations send multiple T38_FIELD_HDLC_FCS_OK
               packets, when they have sent no data for the body of the frame. */
            if (s->tx.out_bytes > 0)
                hdlc_accept((void *) s, fe->hdlc_rx.buf, fe->hdlc_rx.len, TRUE);
            hdlc_accept((void *) s, NULL, SIG_STATUS_CARRIER_DOWN, TRUE);
        }
        s->tx.out_bytes = 0;
        fe->missing_data = FALSE;
        fe->hdlc_rx.len = 0;
        break;
    case T38_FIELD_HDLC_FCS_BAD_SIG_END:
        if (len > 0)
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_FCS_BAD_SIG_END!\n");
        span_log(&s->logging, SPAN_LOG_FLOW, "Type %s - CRC bad, sig end (%s)\n", t30_frametype(s->tx.data[2]), (fe->missing_data)  ?  "missing octets"  :  "clean");
        if (fe->current_rx_type == T31_V21_RX)
            hdlc_accept((void *) s, NULL, SIG_STATUS_CARRIER_DOWN, TRUE);
        fe->hdlc_rx.len = 0;
        fe->missing_data = FALSE;
        break;
    case T38_FIELD_HDLC_SIG_END:
        if (len > 0)
            span_log(&s->logging, SPAN_LOG_WARNING, "There is data in a T38_FIELD_HDLC_SIG_END!\n");
        /* This message is expected under 2 circumstances. One is as an alternative to T38_FIELD_HDLC_FCS_OK_SIG_END - 
           i.e. they send T38_FIELD_HDLC_FCS_OK, and then T38_FIELD_HDLC_SIG_END when the carrier actually drops.
           The other is because the HDLC signal drops unexpectedly - i.e. not just after a final frame. */
        if (fe->current_rx_type == T31_V21_RX)
            hdlc_accept((void *) s, NULL, SIG_STATUS_CARRIER_DOWN, TRUE);
        fe->hdlc_rx.len = 0;
        fe->missing_data = FALSE;
        break;
    case T38_FIELD_T4_NON_ECM_DATA:
        if (!s->at_state.rx_signal_present)
        {
            /* TODO: report training succeeded */
            s->at_state.rx_signal_present = TRUE;
        }
        bit_reverse(buf2, buf, len);
        /* TODO: put the chunk */
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
                    /* TODO: report training succeeded */
                    s->at_state.rx_signal_present = TRUE;
                }
                bit_reverse(buf2, buf, len);
                /* TODO: put the chunk */
            }
            /* WORKAROUND: At least some Mediatrix boxes have a bug, where they can send HDLC signal end where
                           they should send non-ECM signal end. It is possible they also do the opposite.
                           We need to tolerate this, so we use the generic receive complete
                           indication, rather than the specific non-ECM carrier down. */
            /* TODO: report receive complete */
        }
        s->at_state.rx_signal_present = FALSE;
        fe->timeout_rx_samples = 0;
        break;
    case T38_FIELD_CM_MESSAGE:
    case T38_FIELD_JM_MESSAGE:
    case T38_FIELD_CI_MESSAGE:
    case T38_FIELD_V34RATE:
    default:
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int t31_t38_send_timeout(t31_state_t *s, int samples)
{
    int len;
    int i;
    int previous;
    uint8_t buf[MAX_OCTETS_PER_UNPACED_CHUNK + 50];
    t31_t38_front_end_state_t *fe;
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
    if (fe->timeout_rx_samples  &&  fe->samples > fe->timeout_rx_samples)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Timeout mid-receive\n");
        fe->timeout_rx_samples = 0;
        /* TODO: report completion */
    }
    if (fe->timed_step == T38_TIMED_STEP_NONE)
        return FALSE;
    if (fe->samples < fe->next_tx_samples)
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
        if (s->audio.modems.use_tep)
            fe->next_tx_samples += ms_to_samples(startup_time[fe->next_tx_indicator].tep);
        break;
    case T38_TIMED_STEP_NON_ECM_MODEM_3:
        /* Send a chunk of non-ECM image data */
        /* T.38 says it is OK to send the last of the non-ECM data in the signal end message.
           However, I think the early versions of T.38 said the signal end message should not
           contain data. Hopefully, following the current spec will not cause compatibility
           issues. */
        /* Get a chunk of data */
        len = fe->octets_per_data_packet;
        bit_reverse(buf, buf, len);
        if (len < fe->octets_per_data_packet)
        {
            /* That's the end of the image data. Do a little padding now */
            memset(buf + len, 0, fe->octets_per_data_packet - len);
            fe->trailer_bytes = 3*fe->octets_per_data_packet + len;
            len = fe->octets_per_data_packet;
            fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_4;
        }
        t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_DATA, buf, len, DATA_TX_COUNT);
        fe->next_tx_samples += ms_to_samples(fe->ms_per_tx_chunk);
        break;
    case T38_TIMED_STEP_NON_ECM_MODEM_4:
        /* This pads the end of the data with some zeros. If we just stop abruptly
           at the end of the EOLs, some ATAs fail to clean up properly before
           shutting down their transmit modem, and the last few rows of the image
           get corrupted. Simply delaying the no-signal message does not help for
           all implentations. It often appears to be ignored. */
        len = fe->octets_per_data_packet;
        fe->trailer_bytes -= len;
        if (fe->trailer_bytes <= 0)
        {
            len += fe->trailer_bytes;
            memset(buf, 0, len);
            t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_SIG_END, buf, len, fe->data_end_tx_count);
            fe->timed_step = T38_TIMED_STEP_NON_ECM_MODEM_5;
            fe->next_tx_samples += ms_to_samples(60);
            break;
        }
        memset(buf, 0, len);
        t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_T4_NON_ECM_DATA, buf, len, DATA_TX_COUNT);
        fe->next_tx_samples += ms_to_samples(fe->ms_per_tx_chunk);
        break;
    case T38_TIMED_STEP_NON_ECM_MODEM_5:
        /* This should not be needed, since the message above indicates the end of the signal, but it
           seems like it can improve compatibility with quirky implementations. */
        t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL, fe->t38.indicator_tx_count);
        fe->timed_step = T38_TIMED_STEP_NONE;
        /* TODO: report send complete */
        break;
    case T38_TIMED_STEP_HDLC_MODEM:
        /* Send HDLC preambling */
        t38_core_send_indicator(&fe->t38, fe->next_tx_indicator, fe->t38.indicator_tx_count);
        fe->next_tx_samples += ms_to_samples(startup_time[fe->next_tx_indicator].training + startup_time[fe->next_tx_indicator].flags);
        if (s->audio.modems.use_tep)
            fe->next_tx_samples += ms_to_samples(startup_time[fe->next_tx_indicator].tep);
        fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_2;
        break;
    case T38_TIMED_STEP_HDLC_MODEM_2:
        /* Send a chunk of HDLC data */
        i = s->hdlc_tx.len - s->hdlc_tx.ptr;
        if (fe->octets_per_data_packet >= i)
        {
            /* The last part of the HDLC frame */
            if (fe->merge_tx_fields)
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
                /* TODO: report completion */
                /* The above step should have got the next HDLC step ready - either another frame, or an instruction to stop transmission. */
                if (s->hdlc_tx.len < 0)
                {
                    data_fields[1].field_type = T38_FIELD_HDLC_FCS_OK_SIG_END;
                    fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_4;
                }
                else
                {
                    data_fields[1].field_type = T38_FIELD_HDLC_FCS_OK;
                    fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_2;
                }
                data_fields[1].field = NULL;
                data_fields[1].field_len = 0;
                t38_core_send_data_multi_field(&fe->t38, fe->current_tx_data_type, data_fields, 2, DATA_TX_COUNT);
            }
            else
            {
                t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_HDLC_DATA, &s->hdlc_tx.buf[s->hdlc_tx.ptr], i, DATA_TX_COUNT);
                fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_3;
            }
            fe->next_tx_samples += ms_to_samples(fe->ms_per_tx_chunk);
            break;
        }
        t38_core_send_data(&fe->t38, fe->current_tx_data_type, T38_FIELD_HDLC_DATA, &s->hdlc_tx.buf[s->hdlc_tx.ptr], fe->octets_per_data_packet, DATA_TX_COUNT);
        s->hdlc_tx.ptr += fe->octets_per_data_packet;
        fe->next_tx_samples += ms_to_samples(fe->ms_per_tx_chunk);
        break;
    case T38_TIMED_STEP_HDLC_MODEM_3:
        /* End of HDLC frame */
        previous = fe->current_tx_data_type;
        s->hdlc_tx.ptr = 0;
        s->hdlc_tx.len = 0;
        /* TODO: report completion */
        /* The above step should have got the next HDLC step ready - either another frame, or an instruction to stop transmission. */
        if (s->hdlc_tx.len < 0)
        {
            t38_core_send_data(&fe->t38, previous, T38_FIELD_HDLC_FCS_OK_SIG_END, NULL, 0, fe->data_end_tx_count);
            fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_4;
            fe->next_tx_samples += ms_to_samples(100);
            break;
        }
        t38_core_send_data(&fe->t38, previous, T38_FIELD_HDLC_FCS_OK, NULL, 0, DATA_TX_COUNT);
        if (s->hdlc_tx.len)
            fe->timed_step = T38_TIMED_STEP_HDLC_MODEM_2;
        fe->next_tx_samples += ms_to_samples(fe->ms_per_tx_chunk);
        break;
    case T38_TIMED_STEP_HDLC_MODEM_4:
        /* Note that some boxes do not like us sending a T38_FIELD_HDLC_SIG_END at this point.
           A T38_IND_NO_SIGNAL should always be OK. */
        t38_core_send_indicator(&fe->t38, T38_IND_NO_SIGNAL, fe->t38.indicator_tx_count);
        s->hdlc_tx.len = 0;
        //t30_front_end_status(&(s->t30), T30_FRONT_END_SEND_STEP_COMPLETE);
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
        //t30_front_end_status(&(s->t30), T30_FRONT_END_SEND_STEP_COMPLETE);
        break;
    }
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
            t->at_state.at_tx_handler(&t->at_state, t->at_state.at_tx_user_data, t->at_state.rx_data, t->at_state.rx_data_bytes);
            t->at_state.rx_data_bytes = 0;
        }
        restart_modem(t, T31_SILENCE_TX);
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
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
        s->at_state.rx_signal_present = TRUE;
        s->at_state.rx_trained = TRUE;
        break;
    case SIG_STATUS_CARRIER_UP:
        break;
    case SIG_STATUS_CARRIER_DOWN:
        if (s->at_state.rx_signal_present)
        {
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = DLE;
            s->at_state.rx_data[s->at_state.rx_data_bytes++] = ETX;
            s->at_state.at_tx_handler(&s->at_state, s->at_state.at_tx_user_data, s->at_state.rx_data, s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_NO_CARRIER);
            t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
        }
        s->at_state.rx_signal_present = FALSE;
        s->at_state.rx_trained = FALSE;
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
            s->at_state.at_tx_handler(&s->at_state, s->at_state.at_tx_user_data, s->at_state.rx_data, s->at_state.rx_data_bytes);
            s->at_state.rx_data_bytes = 0;
        }
        s->audio.bit_no = 0;
        s->audio.current_byte = 0;
    }
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
                fprintf(stderr, "End of transmit buffer reached!\n");
            }
            if (s->tx.holding)
            {
                /* See if the buffer is approaching empty. It might be time to release flow control. */
                if (s->tx.out_bytes > 1024)
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

static void tone_detected(void *user_data, int on, int level, int delay)
{
    t31_state_t *s;

    s = (t31_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "FAX tone declared %s (%ddBm0)\n", (on)  ?  "on"  :  "off", level);
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
        hdlc_tx_frame(&(s->audio.modems.hdlc_tx), NULL, 0);
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
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        s->at_state.rx_signal_present = TRUE;
        s->at_state.rx_trained = TRUE;
        break;
    case SIG_STATUS_CARRIER_UP:
        if (s->modem == T31_CNG_TONE  ||  s->modem == T31_NOCNG_TONE  ||  s->modem == T31_V21_RX)
        {
            s->at_state.rx_signal_present = TRUE;
            s->rx_frame_received = FALSE;
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
        break;
    case SIG_STATUS_FRAMING_OK:
        if (s->modem == T31_CNG_TONE  ||  s->modem == T31_NOCNG_TONE)
        {
            /* Once we get any valid HDLC the CNG tone stops, and we drop
               to the V.21 receive modem on its own. */
            s->modem = T31_V21_RX;
            s->at_state.transmit = FALSE;
        }
        if (s->modem == T31_V17_RX  ||  s->modem == T31_V27TER_RX  ||  s->modem == T31_V29_RX)
        {
            /* V.21 has been detected while expecting a different carrier.
               If +FAR=0 then result +FCERROR and return to command-mode.
               If +FAR=1 then report +FRH:3 and CONNECT, switching to
               V.21 receive mode. */
            if (s->at_state.p.adaptive_receive)
            {
                s->at_state.rx_signal_present = TRUE;
                s->rx_frame_received = TRUE;
                s->modem = T31_V21_RX;
                s->at_state.transmit = FALSE;
                s->at_state.dte_is_waiting = TRUE;
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_FRH3);
                at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
            }
            else
            {
                s->modem = T31_SILENCE_TX;
                t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
                s->rx_frame_received = FALSE;
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

static void hdlc_accept(void *user_data, const uint8_t *msg, int len, int ok)
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
    hdlc_rx_init(&(s->audio.modems.hdlc_rx), FALSE, TRUE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, s);
    s->at_state.ok_is_pending = FALSE;
    s->hdlc_tx.final = FALSE;
    s->hdlc_tx.len = 0;
    s->dled = FALSE;
    fsk_rx_init(&(s->audio.modems.v21_rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_put_bit, &(s->audio.modems.hdlc_rx));
    fsk_rx_signal_cutoff(&(s->audio.modems.v21_rx), -39.09);
    s->at_state.transmit = TRUE;
}
/*- End of function --------------------------------------------------------*/

static int restart_modem(t31_state_t *s, int new_modem)
{
    int ind;
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
    s->rx_frame_received = FALSE;
    t->rx_handler = (span_rx_handler_t *) &span_dummy_rx;
    t->rx_user_data = NULL;
    switch (s->modem)
    {
    case T31_CNG_TONE:
        if (s->t38_mode)
        {
            t38_core_send_indicator(&s->t38_fe.t38, T38_IND_CNG, INDICATOR_TX_COUNT);
        }
        else
        {
            modem_connect_tones_tx_init(&t->connect_tx, MODEM_CONNECT_TONES_FAX_CNG);
            /* CNG is special, since we need to receive V.21 HDLC messages while sending the
               tone. Everything else in FAX processing sends only one way at a time. */
            /* Do V.21/HDLC receive in parallel. The other end may send its
               first message at any time. The CNG tone will continue until
               we get a valid preamble. */
            t->rx_handler = (span_rx_handler_t *) &cng_rx;
            t->rx_user_data = s;
            t31_v21_rx(s);
            t->tx_handler = (span_tx_handler_t *) &modem_connect_tones_tx;
            t->tx_user_data = &t->connect_tx;
            s->audio.next_tx_handler = NULL;
        }
        s->at_state.transmit = TRUE;
        break;
    case T31_NOCNG_TONE:
        if (s->t38_mode)
        {
        }
        else
        {
            t->rx_handler = (span_rx_handler_t *) &cng_rx;
            t->rx_user_data = s;
            t31_v21_rx(s);
            silence_gen_set(&t->silence_gen, 0);
            t->tx_handler = (span_tx_handler_t *) &silence_gen;
            t->tx_user_data = &t->silence_gen;
        }
        s->at_state.transmit = FALSE;
        break;
    case T31_CED_TONE:
        if (s->t38_mode)
        {
            t38_core_send_indicator(&s->t38_fe.t38, T38_IND_CED, INDICATOR_TX_COUNT);
        }
        else
        {
            modem_connect_tones_tx_init(&t->connect_tx, MODEM_CONNECT_TONES_FAX_CED);
            t->tx_handler = (span_tx_handler_t *) &modem_connect_tones_tx;
            t->tx_user_data = &t->connect_tx;
            s->audio.next_tx_handler = NULL;
        }
        s->at_state.transmit = TRUE;
        break;
    case T31_V21_TX:
        if (s->t38_mode)
        {
            t38_core_send_indicator(&s->t38_fe.t38, T38_IND_V21_PREAMBLE, INDICATOR_TX_COUNT);
        }
        else
        {
            hdlc_tx_init(&t->hdlc_tx, FALSE, 2, FALSE, hdlc_tx_underflow, s);
            /* The spec says 1s +-15% of preamble. So, the minimum is 32 octets. */
            hdlc_tx_flags(&t->hdlc_tx, 32);
            fsk_tx_init(&t->v21_tx, &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &t->hdlc_tx);
            t->tx_handler = (span_tx_handler_t *) &fsk_tx;
            t->tx_user_data = &t->v21_tx;
            s->audio.next_tx_handler = NULL;
        }
        s->hdlc_tx.final = FALSE;
        s->hdlc_tx.len = 0;
        s->dled = FALSE;
        s->at_state.transmit = TRUE;
        break;
    case T31_V21_RX:
        if (s->t38_mode)
        {
        }
        else
        {
            t->rx_handler = (span_rx_handler_t *) &fsk_rx;
            t->rx_user_data = &t->v21_rx;
            t31_v21_rx(s);
        }
        break;
    case T31_V17_TX:
        if (s->t38_mode)
        {
            switch (s->bit_rate)
            {
            case 7200:
                ind = (s->short_train)  ?  T38_IND_V17_7200_SHORT_TRAINING  :  T38_IND_V17_7200_LONG_TRAINING;
                break;
            case 9600:
                ind = (s->short_train)  ?  T38_IND_V17_9600_SHORT_TRAINING  :  T38_IND_V17_9600_LONG_TRAINING;
                break;
            case 12000:
                ind = (s->short_train)  ?  T38_IND_V17_12000_SHORT_TRAINING  :  T38_IND_V17_12000_LONG_TRAINING;
                break;
            case 14400:
            default:
                ind = (s->short_train)  ?  T38_IND_V17_14400_SHORT_TRAINING  :  T38_IND_V17_14400_LONG_TRAINING;
                break;
            }
            t38_core_send_indicator(&s->t38_fe.t38, ind, INDICATOR_TX_COUNT);
        }
        else
        {
            v17_tx_restart(&t->v17_tx, s->bit_rate, FALSE, s->short_train);
            t->tx_handler = (span_tx_handler_t *) &v17_tx;
            t->tx_user_data = &t->v17_tx;
            s->audio.next_tx_handler = NULL;
        }
        s->tx.out_bytes = 0;
        s->tx.data_started = FALSE;
        s->at_state.transmit = TRUE;
        break;
    case T31_V17_RX:
        if (!s->t38_mode)
        {
            t->rx_handler = (span_rx_handler_t *) &v17_v21_rx;
            t->rx_user_data = s;
            v17_rx_restart(&t->v17_rx, s->bit_rate, s->short_train);
            /* Allow for +FCERROR/+FRH:3 */
            t31_v21_rx(s);
        }
        s->at_state.transmit = FALSE;
        break;
    case T31_V27TER_TX:
        if (s->t38_mode)
        {
            switch (s->bit_rate)
            {
            case 2400:
                ind = T38_IND_V27TER_2400_TRAINING;
                break;
            case 4800:
            default:
                ind = T38_IND_V27TER_4800_TRAINING;
                break;
            }
            t38_core_send_indicator(&s->t38_fe.t38, ind, INDICATOR_TX_COUNT);
        }
        else
        {
            v27ter_tx_restart(&t->v27ter_tx, s->bit_rate, FALSE);
            t->tx_handler = (span_tx_handler_t *) &v27ter_tx;
            t->tx_user_data = &t->v27ter_tx;
            s->audio.next_tx_handler = NULL;
        }
        s->tx.out_bytes = 0;
        s->tx.data_started = FALSE;
        s->at_state.transmit = TRUE;
        break;
    case T31_V27TER_RX:
        if (!s->t38_mode)
        {
            t->rx_handler = (span_rx_handler_t *) &v27ter_v21_rx;
            t->rx_user_data = s;
            v27ter_rx_restart(&t->v27ter_rx, s->bit_rate, FALSE);
            /* Allow for +FCERROR/+FRH:3 */
            t31_v21_rx(s);
        }
        s->at_state.transmit = FALSE;
        break;
    case T31_V29_TX:
        if (s->t38_mode)
        {
            switch (s->bit_rate)
            {
            case 7200:
                ind = T38_IND_V29_7200_TRAINING;
                break;
            case 9600:
            default:
                ind = T38_IND_V29_9600_TRAINING;
                break;
            }
            t38_core_send_indicator(&s->t38_fe.t38, ind, INDICATOR_TX_COUNT);
        }
        else
        {
            v29_tx_restart(&t->v29_tx, s->bit_rate, FALSE);
            t->tx_handler = (span_tx_handler_t *) &v29_tx;
            t->tx_user_data = &t->v29_tx;
            s->audio.next_tx_handler = NULL;
        }
        s->tx.out_bytes = 0;
        s->tx.data_started = FALSE;
        s->at_state.transmit = TRUE;
        break;
    case T31_V29_RX:
        if (!s->t38_mode)
        {
            t->rx_handler = (span_rx_handler_t *) &v29_v21_rx;
            t->rx_user_data = s;
            v29_rx_restart(&t->v29_rx, s->bit_rate, FALSE);
            /* Allow for +FCERROR/+FRH:3 */
            t31_v21_rx(s);
        }
        s->at_state.transmit = FALSE;
        break;
    case T31_SILENCE_TX:
        if (s->t38_mode)
        {
            t38_core_send_indicator(&s->t38_fe.t38, T38_IND_NO_SIGNAL, INDICATOR_TX_COUNT);
        }
        else
        {
            silence_gen_set(&t->silence_gen, 0);
            t->tx_handler = (span_tx_handler_t *) &silence_gen;
            t->tx_user_data = &t->silence_gen;
            s->audio.next_tx_handler = NULL;
        }
        s->at_state.transmit = FALSE;
        break;
    case T31_SILENCE_RX:
        if (!s->t38_mode)
        {
            t->rx_handler = (span_rx_handler_t *) &silence_rx;
            t->rx_user_data = s;

            silence_gen_set(&t->silence_gen, 0);
            t->tx_handler = (span_tx_handler_t *) &silence_gen;
            t->tx_user_data = &t->silence_gen;
            s->audio.next_tx_handler = NULL;
        }
        s->at_state.transmit = FALSE;
        break;
    case T31_FLUSH:
        /* Send 200ms of silence to "push" the last audio out */
        if (s->t38_mode)
        {
            t38_core_send_indicator(&s->t38_fe.t38, T38_IND_NO_SIGNAL, INDICATOR_TX_COUNT);
        }
        else
        {
            s->modem = T31_SILENCE_TX;
            silence_gen_alter(&t->silence_gen, ms_to_samples(200));
            t->tx_handler = (span_tx_handler_t *) &silence_gen;
            t->tx_user_data = &t->silence_gen;
            s->audio.next_tx_handler = NULL;
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
                if (s->t38_mode)
                {
                }
                else
                {
                    hdlc_tx_frame(&(s->audio.modems.hdlc_tx), s->hdlc_tx.buf, s->hdlc_tx.len);
                }
                s->hdlc_tx.final = (s->hdlc_tx.buf[1] & 0x10);
                s->hdlc_tx.len = 0;
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
            fprintf(stderr, "No room in buffer for new data!\n");
            return;
        }
    }
    if (!s->tx.holding)
    {
        /* See if the buffer is approaching full. We might need to apply flow control. */
        if (s->tx.in_bytes > T31_TX_BUF_LEN - 1024)
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
            restart_modem(s, T31_SILENCE_TX);
            silence_gen_alter(&(s->audio.modems.silence_gen), ms_to_samples(val*10));
            s->at_state.transmit = TRUE;
        }
        else
        {
            /* Wait until we have received a specified period of silence. */
            queue_flush(s->rx_queue);
            s->silence_awaited = ms_to_samples(val*10);
            t31_set_at_rx_mode(s, AT_MODE_DELIVERY);
            restart_modem(s, T31_SILENCE_RX);
        }
        immediate_response = FALSE;
        span_log(&s->logging, SPAN_LOG_FLOW, "Silence %dms\n", val*10);
        break;
    case 'H':
        switch (val)
        {
        case 3:
            new_modem = (new_transmit)  ?  T31_V21_TX  :  T31_V21_RX;
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
            at_put_response_code(&s->at_state, AT_RESPONSE_CODE_CONNECT);
        }
        else
        {
            /* Send straight away, if there is something queued. */
            t31_set_at_rx_mode(s, AT_MODE_DELIVERY);
            s->rx_frame_received = FALSE;
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
            new_modem = (new_transmit)  ?  T31_V27TER_TX  :  T31_V27TER_RX;
            s->short_train = FALSE;
            s->bit_rate = 2400;
            break;
        case 48:
            new_modem = (new_transmit)  ?  T31_V27TER_TX  :  T31_V27TER_RX;
            s->short_train = FALSE;
            s->bit_rate = 4800;
            break;
        case 72:
            new_modem = (new_transmit)  ?  T31_V29_TX  :  T31_V29_RX;
            s->short_train = FALSE;
            s->bit_rate = 7200;
            break;
        case 96:
            new_modem = (new_transmit)  ?  T31_V29_TX  :  T31_V29_RX;
            s->short_train = FALSE;
            s->bit_rate = 9600;
            break;
        case 73:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 7200;
            break;
        case 74:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 7200;
            break;
        case 97:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 9600;
            break;
        case 98:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 9600;
            break;
        case 121:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 12000;
            break;
        case 122:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = TRUE;
            s->bit_rate = 12000;
            break;
        case 145:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
            s->short_train = FALSE;
            s->bit_rate = 14400;
            break;
        case 146:
            new_modem = (new_transmit)  ?  T31_V17_TX  :  T31_V17_RX;
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

void t31_call_event(t31_state_t *s, int event)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Call event %d received\n", event);
    at_call_event(&s->at_state, event);
}
/*- End of function --------------------------------------------------------*/

int t31_at_rx(t31_state_t *s, const char *t, int len)
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
            s->modem = T31_SILENCE_TX;
            s->audio.modems.rx_handler = span_dummy_rx;
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
            s->tx.in_bytes = &(s->tx.data[s->tx.in_bytes]) - &(s->tx.data[s->tx.out_bytes]);
            memmove(&(s->tx.data[0]), &(s->tx.data[s->tx.out_bytes]), s->tx.in_bytes);
            s->tx.out_bytes = 0;
        }
        dle_unstuff(s, t, len);
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
        restart_modem(s, T31_SILENCE_TX);
        at_modem_control(&s->at_state, AT_MODEM_CONTROL_HANGUP, NULL);
        t31_set_at_rx_mode(s, AT_MODE_ONHOOK_COMMAND);
    }
    else
    {
        fsk_rx(&(s->audio.modems.v21_rx), amp, len);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v17_v21_rx(void *user_data, const int16_t amp[], int len)
{
    t31_state_t *t;
    fax_modems_state_t *s;

    t = (t31_state_t *) user_data;
    s = &t->audio.modems;
    v17_rx(&s->v17_rx, amp, len);
    if (t->at_state.rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&t->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.17 (%.2fdBm0)\n", v17_rx_signal_power(&s->v17_rx));
        s->rx_handler = (span_rx_handler_t *) &v17_rx;
        s->rx_user_data = &s->v17_rx;
    }
    else
    {
        fsk_rx(&s->v21_rx, amp, len);
        if (t->rx_frame_received)
        {
            /* We have received something, and the fast modem has not trained. We must
               be receiving valid V.21 */
            span_log(&t->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&s->v21_rx));
            s->rx_handler = (span_rx_handler_t *) &fsk_rx;
            s->rx_user_data = &s->v21_rx;
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v27ter_v21_rx(void *user_data, const int16_t amp[], int len)
{
    t31_state_t *t;
    fax_modems_state_t *s;

    t = (t31_state_t *) user_data;
    s = &t->audio.modems;
    v27ter_rx(&s->v27ter_rx, amp, len);
    if (t->at_state.rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&t->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.27ter (%.2fdBm0)\n", v27ter_rx_signal_power(&s->v27ter_rx));
        s->rx_handler = (span_rx_handler_t *) &v27ter_rx;
        s->rx_user_data = &s->v27ter_rx;
    }
    else
    {
        fsk_rx(&s->v21_rx, amp, len);
        if (t->rx_frame_received)
        {
            /* We have received something, and the fast modem has not trained. We must
               be receiving valid V.21 */
            span_log(&t->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&s->v21_rx));
            s->rx_handler = (span_rx_handler_t *) &fsk_rx;
            s->rx_user_data = &s->v21_rx;
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static int v29_v21_rx(void *user_data, const int16_t amp[], int len)
{
    t31_state_t *t;
    fax_modems_state_t *s;

    t = (t31_state_t *) user_data;
    s = &t->audio.modems;
    v29_rx(&s->v29_rx, amp, len);
    if (t->at_state.rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.29 (%.2fdBm0)\n", v29_rx_signal_power(&s->v29_rx));
        s->rx_handler = (span_rx_handler_t *) &v29_rx;
        s->rx_user_data = &s->v29_rx;
    }
    else
    {
        fsk_rx(&s->v21_rx, amp, len);
        if (t->rx_frame_received)
        {
            /* We have received something, and the fast modem has not trained. We must
               be receiving valid V.21 */
            span_log(&t->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&s->v21_rx));
            s->rx_handler = (span_rx_handler_t *) &fsk_rx;
            s->rx_user_data = &s->v21_rx;
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static void t31_fax_modems_init(fax_modems_state_t *s, int use_tep, void *user_data)
{
    s->use_tep = use_tep;

    hdlc_rx_init(&s->hdlc_rx, FALSE, TRUE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, user_data);
    hdlc_tx_init(&s->hdlc_tx, FALSE, 2, FALSE, hdlc_tx_underflow, user_data);
    fsk_rx_init(&s->v21_rx, &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_put_bit, &s->hdlc_rx);
    fsk_rx_signal_cutoff(&s->v21_rx, -39.09);
    fsk_tx_init(&s->v21_tx, &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &s->hdlc_tx);
    v17_rx_init(&s->v17_rx, 14400, non_ecm_put_bit, user_data);
    v17_tx_init(&s->v17_tx, 14400, s->use_tep, non_ecm_get_bit, user_data);
    v29_rx_init(&s->v29_rx, 9600, non_ecm_put_bit, user_data);
    v29_rx_signal_cutoff(&s->v29_rx, -45.5);
    v29_tx_init(&s->v29_tx, 9600, s->use_tep, non_ecm_get_bit, user_data);
    v27ter_rx_init(&s->v27ter_rx, 4800, non_ecm_put_bit, user_data);
    v27ter_tx_init(&s->v27ter_tx, 4800, s->use_tep, non_ecm_get_bit, user_data);
    silence_gen_init(&s->silence_gen, 0);
    modem_connect_tones_tx_init(&s->connect_tx, MODEM_CONNECT_TONES_FAX_CNG);
    modem_connect_tones_rx_init(&s->connect_rx,
                                MODEM_CONNECT_TONES_FAX_CNG,
                                tone_detected,
                                user_data);
    dc_restore_init(&s->dc_restore);

    s->rx_signal_present = FALSE;
    s->rx_handler = (span_rx_handler_t *) &span_dummy_rx;
    s->rx_user_data = NULL;
    s->tx_handler = (span_tx_handler_t *) &silence_gen;
    s->tx_user_data = &s->silence_gen;
}
/*- End of function --------------------------------------------------------*/

int t31_rx(t31_state_t *s, int16_t amp[], int len)
{
    int i;
    int32_t power;

    /* Monitor for received silence.  Maximum needed detection is AT+FRS=255 (255*10ms). */
    /* We could probably only run this loop if (s->modem == T31_SILENCE_RX), however,
       the spec says "when silence has been present on the line for the amount of
       time specified".  That means some of the silence may have occurred before
       the AT+FRS=n command. This condition, however, is not likely to ever be the
       case.  (AT+FRS=n will usually be issued before the remote goes silent.) */
    for (i = 0;  i < len;  i++)
    {
        /* Clean up any DC influence. */
        power = power_meter_update(&(s->audio.rx_power), amp[i] - s->audio.last_sample);
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
        restart_modem(s, T31_SILENCE_TX);
    }

    if (!s->at_state.transmit  ||  s->modem == T31_CNG_TONE)
        s->audio.modems.rx_handler(s->audio.modems.rx_user_data, amp, len);
    return  0;
}
/*- End of function --------------------------------------------------------*/

static int set_next_tx_type(t31_state_t *s)
{
    if (s->audio.next_tx_handler)
    {
        s->audio.modems.tx_handler = s->audio.next_tx_handler;
        s->audio.modems.tx_user_data = s->audio.next_tx_user_data;
        s->audio.next_tx_handler = NULL;
        return 0;
    }
    /* If there is nothing else to change to, so use zero length silence */
    silence_gen_alter(&(s->audio.modems.silence_gen), 0);
    s->audio.modems.tx_handler = (span_tx_handler_t *) &silence_gen;
    s->audio.modems.tx_user_data = &(s->audio.modems.silence_gen);
    s->audio.next_tx_handler = NULL;
    return -1;
}
/*- End of function --------------------------------------------------------*/

int t31_tx(t31_state_t *s, int16_t amp[], int max_len)
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
            {
                switch (s->modem)
                {
                case T31_SILENCE_TX:
                    s->modem = T31_NONE;
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
                case T31_CED_TONE:
                    /* Go directly to V.21/HDLC transmit. */
                    s->modem = T31_NONE;
                    restart_modem(s, T31_V21_TX);
                    t31_set_at_rx_mode(s, AT_MODE_HDLC);
                    break;
                case T31_V21_TX:
                case T31_V17_TX:
                case T31_V27TER_TX:
                case T31_V29_TX:
                    s->modem = T31_NONE;
                    at_put_response_code(&s->at_state, AT_RESPONSE_CODE_OK);
                    t31_set_at_rx_mode(s, AT_MODE_OFFHOOK_COMMAND);
                    restart_modem(s, T31_SILENCE_TX);
                    break;
                }
            }
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

void t31_set_transmit_on_idle(t31_state_t *s, int transmit_on_idle)
{
    s->audio.modems.transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

void t31_set_tep_mode(t31_state_t *s, int use_tep)
{
    s->audio.modems.use_tep = use_tep;
}
/*- End of function --------------------------------------------------------*/

void t31_set_t38_config(t31_state_t *s, int without_pacing)
{
    if (without_pacing)
    {
        /* Continuous streaming mode, as used for TPKT over TCP transport */
        s->t38_fe.t38.indicator_tx_count = 0;
        s->t38_fe.t38.data_end_tx_count = 1;
        s->t38_fe.ms_per_tx_chunk = 0;
    }
    else
    {
        /* Paced streaming mode, as used for UDP transports */
        s->t38_fe.t38.indicator_tx_count = INDICATOR_TX_COUNT;
        s->t38_fe.t38.data_end_tx_count = DATA_END_TX_COUNT;
        s->t38_fe.ms_per_tx_chunk = MS_PER_TX_CHUNK;
    }
}
/*- End of function --------------------------------------------------------*/

t31_state_t *t31_init(t31_state_t *s,
                      at_tx_handler_t *at_tx_handler,
                      void *at_tx_user_data,
                      t31_modem_control_handler_t *modem_control_handler,
                      void *modem_control_user_data,
                      t38_tx_packet_handler_t *tx_t38_packet_handler,
                      void *tx_t38_packet_user_data)
{
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
    t31_fax_modems_init(&s->audio.modems, FALSE, s);
    power_meter_init(&(s->audio.rx_power), 4);
    s->audio.last_sample = 0;
    s->audio.silence_threshold_power = power_meter_level_dbm0(-36);
    s->at_state.rx_signal_present = FALSE;
    s->at_state.rx_trained = FALSE;

    s->at_state.do_hangup = FALSE;
    s->at_state.line_ptr = 0;
    s->audio.silence_heard = 0;
    s->silence_awaited = 0;
    s->call_samples = 0;
    s->modem = T31_NONE;
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
        t38_core_init(&s->t38_fe.t38,
                      process_rx_indicator,
                      process_rx_data,
                      process_rx_missing,
                      (void *) s,
                      tx_t38_packet_handler,
                      tx_t38_packet_user_data);
        t31_set_t38_config(s, FALSE);
    }
    s->t38_mode = FALSE;
    return s;
}
/*- End of function --------------------------------------------------------*/

int t31_release(t31_state_t *s)
{
    at_reset_call_info(&s->at_state);
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
