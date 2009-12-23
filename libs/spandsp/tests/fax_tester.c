/*
 * SpanDSP - a series of DSP components for telephony
 *
 * faxtester_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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
 * $Id: fax_tester.c,v 1.23 2009/11/02 13:25:20 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
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
#include "floating_fudge.h"
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#if defined(HAVE_LIBXML_XMLMEMORY_H)
#include <libxml/xmlmemory.h>
#endif
#if defined(HAVE_LIBXML_PARSER_H)
#include <libxml/parser.h>
#endif
#if defined(HAVE_LIBXML_XINCLUDE_H)
#include <libxml/xinclude.h>
#endif

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

#include "fax_tester.h"

#define HDLC_FRAMING_OK_THRESHOLD       5

static void timer_update(faxtester_state_t *s, int len)
{
    s->timer += len;
    if (s->timer > s->timeout)
    {
        s->timeout = 0x7FFFFFFFFFFFFFFFLL;
        if (s->front_end_step_timeout_handler)
            s->front_end_step_timeout_handler(s, s->front_end_step_timeout_user_data);
    }
}
/*- End of function --------------------------------------------------------*/

static void front_end_step_complete(faxtester_state_t *s)
{
    if (s->front_end_step_complete_handler)
        s->front_end_step_complete_handler(s, s->front_end_step_complete_user_data);
}
/*- End of function --------------------------------------------------------*/

void faxtester_send_hdlc_flags(faxtester_state_t *s, int flags)
{
    hdlc_tx_flags(&(s->modems.hdlc_tx), flags);
}
/*- End of function --------------------------------------------------------*/

void faxtester_send_hdlc_msg(faxtester_state_t *s, const uint8_t *msg, int len, int crc_ok)
{
    hdlc_tx_frame(&(s->modems.hdlc_tx), msg, len);
    if (!crc_ok)
        hdlc_tx_corrupt_frame(&(s->modems.hdlc_tx));
}
/*- End of function --------------------------------------------------------*/

static void hdlc_underflow_handler(void *user_data)
{
    faxtester_state_t *s;
    uint8_t buf[400];

    s = (faxtester_state_t *) user_data;
    
    if (s->image_buffer)
    {
        /* We are sending an ECM image */
        if (s->image_ptr < s->image_len)
        {
            buf[0] = 0xFF;
            buf[1] = 0x03;
            buf[2] = 0x06;
            buf[3] = s->image_ptr/s->ecm_frame_size;
            memcpy(buf + 4, &s->image_buffer[s->image_ptr], s->ecm_frame_size);
            hdlc_tx_frame(&(s->modems.hdlc_tx), buf, 4 + s->ecm_frame_size);
            if (s->corrupt_crc >= 0  &&  s->corrupt_crc == s->image_ptr/s->ecm_frame_size)
                hdlc_tx_corrupt_frame(&(s->modems.hdlc_tx));
            s->image_ptr += s->ecm_frame_size;
            return;
        }
        else
        {
            /* The actual image is over. We are sending the final RCP frames. */
            if (s->image_bit_ptr > 2)
            {
                s->image_bit_ptr--;
                buf[0] = 0xFF;
                buf[1] = 0x03;
                buf[2] = 0x86;
                hdlc_tx_frame(&(s->modems.hdlc_tx), buf, 3);
                return;
            }
            else
            {
                /* All done. */
                s->image_buffer = NULL;
            }
        }
    }
    front_end_step_complete(s);
}
/*- End of function --------------------------------------------------------*/

static void modem_tx_status(void *user_data, int status)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    printf("Tx status is %s (%d)\n", signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_SHUTDOWN_COMPLETE:
        front_end_step_complete(s);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void tone_detected(void *user_data, int tone, int level, int delay)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "%s (%d) declared (%ddBm0)\n",
             modem_connect_tone_to_str(tone),
             tone,
             level);
    if (tone != MODEM_CONNECT_TONES_NONE)
    {
        s->tone_on_time = s->timer;
    }
    else
    {
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "Tone was on for %fs\n",
                 (float) (s->timer - s->tone_on_time)/SAMPLE_RATE + 0.55);
    }    
    s->tone_state = tone;
    if (tone == MODEM_CONNECT_TONES_NONE)
        front_end_step_complete(s);
}
/*- End of function --------------------------------------------------------*/

static int non_ecm_get_bit(void *user_data)
{
    faxtester_state_t *s;
    int bit;

    s = (faxtester_state_t *) user_data;
    if (s->image_bit_ptr == 0)
    {
        if (s->image_ptr >= s->image_len)
        {
            s->image_buffer = NULL;
            return SIG_STATUS_END_OF_DATA;
        }
        s->image_bit_ptr = 8;
        s->image_ptr++;
    }
    s->image_bit_ptr--;
    bit = (s->image_buffer[s->image_ptr] >> (7 - s->image_bit_ptr)) & 0x01;
    //printf("Rx bit - %d\n", bit);
    return bit;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_non_ecm_image_buffer(faxtester_state_t *s, const uint8_t *buf, int len)
{
    s->image_ptr = 0;
    s->image_bit_ptr = 8;
    s->image_len = len;
    s->image_buffer = buf;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_ecm_image_buffer(faxtester_state_t *s, const uint8_t *buf, int len, int block, int frame_size, int crc_hit)
{
    int start;

    start = 256*frame_size*block;
    if (len > start + 256*frame_size)
        len = start + 256*frame_size;

    s->ecm_frame_size = frame_size;
    s->image_ptr = start;
    s->image_bit_ptr = 8;
    s->image_len = len;
    s->image_buffer = buf;
    s->corrupt_crc = crc_hit;
    /* Send the first frame */
    hdlc_underflow_handler(s);
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_rx_status(void *user_data, int status)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier status is %s (%d)\n", signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_TRAINING_FAILED:
        s->modems.rx_trained = FALSE;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        s->modems.rx_trained = TRUE;
        break;
    case SIG_STATUS_CARRIER_UP:
        s->modems.rx_signal_present = TRUE;
        break;
    case SIG_STATUS_CARRIER_DOWN:
        if (s->modems.rx_trained)
        {
            if (s->real_time_frame_handler)
                s->real_time_frame_handler(s, s->real_time_frame_user_data, TRUE, NULL, 0);
        }
        s->modems.rx_signal_present = FALSE;
        s->modems.rx_trained = FALSE;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_put_bit(void *user_data, int bit)
{
    faxtester_state_t *s;

    if (bit < 0)
    {
        non_ecm_rx_status(user_data, bit);
        return;
    }
    s = (faxtester_state_t *) user_data;
}
/*- End of function --------------------------------------------------------*/

static void hdlc_rx_status(void *user_data, int status)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    fprintf(stderr, "HDLC carrier status is %s (%d)\n", signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_TRAINING_FAILED:
        s->modems.rx_trained = FALSE;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        s->modems.rx_trained = TRUE;
        break;
    case SIG_STATUS_CARRIER_UP:
        s->modems.rx_signal_present = TRUE;
        break;
    case SIG_STATUS_CARRIER_DOWN:
        s->modems.rx_signal_present = FALSE;
        s->modems.rx_trained = FALSE;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept(void *user_data, const uint8_t *msg, int len, int ok)
{
    faxtester_state_t *s;

    if (len < 0)
    {
        hdlc_rx_status(user_data, len);
        return;
    }
    s = (faxtester_state_t *) user_data;
    if (s->real_time_frame_handler)
        s->real_time_frame_handler(s, s->real_time_frame_user_data, TRUE, msg, len);
}
/*- End of function --------------------------------------------------------*/

static int v17_v21_rx(void *user_data, const int16_t amp[], int len)
{
    faxtester_state_t *t;
    fax_modems_state_t *s;

    t = (faxtester_state_t *) user_data;
    s = &t->modems;
    v17_rx(&s->v17_rx, amp, len);
    fsk_rx(&s->v21_rx, amp, len);
    if (s->rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&t->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.17 (%.2fdBm0)\n", v17_rx_signal_power(&s->v17_rx));
        s->rx_handler = (span_rx_handler_t *) &v17_rx;
        s->rx_user_data = &s->v17_rx;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v27ter_v21_rx(void *user_data, const int16_t amp[], int len)
{
    faxtester_state_t *t;
    fax_modems_state_t *s;

    t = (faxtester_state_t *) user_data;
    s = &t->modems;
    v27ter_rx(&s->v27ter_rx, amp, len);
    fsk_rx(&s->v21_rx, amp, len);
    if (s->rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&t->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.27ter (%.2fdBm0)\n", v27ter_rx_signal_power(&s->v27ter_rx));
        s->rx_handler = (span_rx_handler_t *) &v27ter_rx;
        s->rx_user_data = &s->v27ter_rx;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v29_v21_rx(void *user_data, const int16_t amp[], int len)
{
    faxtester_state_t *t;
    fax_modems_state_t *s;

    t = (faxtester_state_t *) user_data;
    s = &t->modems;
    v29_rx(&s->v29_rx, amp, len);
    fsk_rx(&s->v21_rx, amp, len);
    if (s->rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&t->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.29 (%.2fdBm0)\n", v29_rx_signal_power(&s->v29_rx));
        s->rx_handler = (span_rx_handler_t *) &v29_rx;
        s->rx_user_data = &s->v29_rx;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int faxtester_rx(faxtester_state_t *s, int16_t *amp, int len)
{
    int i;

    for (i = 0;  i < len;  i++)
        amp[i] = dc_restore(&(s->modems.dc_restore), amp[i]);
    s->modems.rx_handler(s->modems.rx_user_data, amp, len);
    timer_update(s, len);
    if (s->wait_for_silence)
    {
        if (!s->modems.rx_signal_present)
        {
            s->wait_for_silence = FALSE;
            front_end_step_complete(s);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int faxtester_tx(faxtester_state_t *s, int16_t *amp, int max_len)
{
    int len;
    int required_len;
    
    required_len = max_len;
    len = 0;
    if (s->transmit)
    {
        while ((len += s->modems.tx_handler(s->modems.tx_user_data, amp + len, max_len - len)) < max_len)
        {
            /* Allow for a change of tx handler within a block */
            front_end_step_complete(s);
            if (!s->transmit)
            {
                if (s->modems.transmit_on_idle)
                {
                    /* Pad to the requested length with silence */
                    memset(amp + len, 0, (max_len - len)*sizeof(int16_t));
                    len = max_len;        
                }
                break;
            }
        }
    }
    else
    {
        if (s->modems.transmit_on_idle)
        {
            /* Pad to the requested length with silence */
            memset(amp, 0, max_len*sizeof(int16_t));
            len = max_len;        
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

void faxtest_set_rx_silence(faxtester_state_t *s)
{
    s->wait_for_silence = TRUE;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_rx_type(void *user_data, int type, int bit_rate, int short_train, int use_hdlc)
{
    faxtester_state_t *s;
    put_bit_func_t put_bit_func;
    void *put_bit_user_data;
    fax_modems_state_t *t;
    int tone;

    s = (faxtester_state_t *) user_data;
    t = &s->modems;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set rx type %d\n", type);
    if (s->current_rx_type == type)
        return;
    s->current_rx_type = type;
    if (use_hdlc)
    {
        put_bit_func = (put_bit_func_t) hdlc_rx_put_bit;
        put_bit_user_data = (void *) &t->hdlc_rx;
        hdlc_rx_init(&t->hdlc_rx, FALSE, FALSE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, s);
    }
    else
    {
        put_bit_func = non_ecm_put_bit;
        put_bit_user_data = (void *) s;
    }
    switch (type)
    {
    case T30_MODEM_CED:
    case T30_MODEM_CNG:
        if (type == T30_MODEM_CED)
            tone = MODEM_CONNECT_TONES_FAX_CED;
        else
            tone = MODEM_CONNECT_TONES_FAX_CNG;
        modem_connect_tones_rx_init(&t->connect_rx,
                                    tone,
                                    tone_detected,
                                    (void *) s);
        t->rx_handler = (span_rx_handler_t *) &modem_connect_tones_rx;
        t->rx_user_data = &t->connect_rx;
        s->tone_state = MODEM_CONNECT_TONES_NONE;
        break;
    case T30_MODEM_V21:
        if (s->flush_handler)
            s->flush_handler(s, s->flush_user_data, 3);
        fsk_rx_init(&t->v21_rx, &preset_fsk_specs[FSK_V21CH2], FSK_FRAME_MODE_SYNC, (put_bit_func_t) hdlc_rx_put_bit, put_bit_user_data);
        fsk_rx_signal_cutoff(&t->v21_rx, -45.5);
        t->rx_handler = (span_rx_handler_t *) &fsk_rx;
        t->rx_user_data = &t->v21_rx;
        break;
    case T30_MODEM_V27TER:
        v27ter_rx_restart(&t->v27ter_rx, bit_rate, FALSE);
        v27ter_rx_set_put_bit(&t->v27ter_rx, put_bit_func, put_bit_user_data);
        t->rx_handler = (span_rx_handler_t *) &v27ter_v21_rx;
        t->rx_user_data = s;
        break;
    case T30_MODEM_V29:
        v29_rx_restart(&t->v29_rx, bit_rate, FALSE);
        v29_rx_set_put_bit(&t->v29_rx, put_bit_func, put_bit_user_data);
        t->rx_handler = (span_rx_handler_t *) &v29_v21_rx;
        t->rx_user_data = s;
        break;
    case T30_MODEM_V17:
        v17_rx_restart(&t->v17_rx, bit_rate, short_train);
        v17_rx_set_put_bit(&t->v17_rx, put_bit_func, put_bit_user_data);
        t->rx_handler = (span_rx_handler_t *) &v17_v21_rx;
        t->rx_user_data = s;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
    default:
        t->rx_handler = (span_rx_handler_t *) &span_dummy_rx;
        t->rx_user_data = s;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_tx_type(void *user_data, int type, int bit_rate, int short_train, int use_hdlc)
{
    faxtester_state_t *s;
    get_bit_func_t get_bit_func;
    void *get_bit_user_data;
    fax_modems_state_t *t;
    int tone;

    s = (faxtester_state_t *) user_data;
    t = &s->modems;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set tx type %d\n", type);
    if (s->current_tx_type == type)
        return;
    if (use_hdlc)
    {
        get_bit_func = (get_bit_func_t) hdlc_tx_get_bit;
        get_bit_user_data = (void *) &t->hdlc_tx;
    }
    else
    {
        get_bit_func = non_ecm_get_bit;
        get_bit_user_data = (void *) s;
    }
    switch (type)
    {
    case T30_MODEM_PAUSE:
        silence_gen_alter(&t->silence_gen, ms_to_samples(short_train));
        t->tx_handler = (span_tx_handler_t *) &silence_gen;
        t->tx_user_data = &t->silence_gen;
        s->transmit = TRUE;
        break;
    case T30_MODEM_CED:
    case T30_MODEM_CNG:
        if (type == T30_MODEM_CED)
            tone = MODEM_CONNECT_TONES_FAX_CED;
        else
            tone = MODEM_CONNECT_TONES_FAX_CNG;
        modem_connect_tones_tx_init(&t->connect_tx, tone);
        t->tx_handler = (span_tx_handler_t *) &modem_connect_tones_tx;
        t->tx_user_data = &t->connect_tx;
        s->transmit = TRUE;
        break;
    case T30_MODEM_V21:
        fsk_tx_init(&t->v21_tx, &preset_fsk_specs[FSK_V21CH2], get_bit_func, get_bit_user_data);
        fsk_tx_set_modem_status_handler(&t->v21_tx, modem_tx_status, (void *) s);
        t->tx_handler = (span_tx_handler_t *) &fsk_tx;
        t->tx_user_data = &t->v21_tx;
        s->transmit = TRUE;
        break;
    case T30_MODEM_V27TER:
        v27ter_tx_restart(&t->v27ter_tx, bit_rate, t->use_tep);
        v27ter_tx_set_get_bit(&t->v27ter_tx, get_bit_func, get_bit_user_data);
        v27ter_tx_set_modem_status_handler(&t->v27ter_tx, modem_tx_status, (void *) s);
        t->tx_handler = (span_tx_handler_t *) &v27ter_tx;
        t->tx_user_data = &t->v27ter_tx;
        /* For any fast modem, set 200ms of preamble flags */
        hdlc_tx_flags(&t->hdlc_tx, bit_rate/(8*5));
        s->transmit = TRUE;
        break;
    case T30_MODEM_V29:
        v29_tx_restart(&t->v29_tx, bit_rate, t->use_tep);
        v29_tx_set_get_bit(&t->v29_tx, get_bit_func, get_bit_user_data);
        v29_tx_set_modem_status_handler(&t->v29_tx, modem_tx_status, (void *) s);
        t->tx_handler = (span_tx_handler_t *) &v29_tx;
        t->tx_user_data = &t->v29_tx;
        /* For any fast modem, set 200ms of preamble flags */
        hdlc_tx_flags(&t->hdlc_tx, bit_rate/(8*5));
        s->transmit = TRUE;
        break;
    case T30_MODEM_V17:
        v17_tx_restart(&t->v17_tx, bit_rate, t->use_tep, short_train);
        v17_tx_set_get_bit(&t->v17_tx, get_bit_func, get_bit_user_data);
        v17_tx_set_modem_status_handler(&t->v17_tx, modem_tx_status, (void *) s);
        t->tx_handler = (span_tx_handler_t *) &v17_tx;
        t->tx_user_data = &t->v17_tx;
        /* For any fast modem, set 200ms of preamble flags */
        hdlc_tx_flags(&t->hdlc_tx, bit_rate/(8*5));
        s->transmit = TRUE;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
        /* Fall through */
    default:
        silence_gen_alter(&t->silence_gen, 0);
        t->tx_handler = (span_tx_handler_t *) &silence_gen;
        t->tx_user_data = &t->silence_gen;
        s->transmit = FALSE;
        break;
    }
    s->current_tx_type = type;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_timeout(faxtester_state_t *s, int timeout)
{
    if (timeout >= 0)
        s->timeout = s->timer + timeout*SAMPLE_RATE/1000;
    else
        s->timeout = 0x7FFFFFFFFFFFFFFFLL;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_transmit_on_idle(faxtester_state_t *s, int transmit_on_idle)
{
    s->modems.transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_tep_mode(faxtester_state_t *s, int use_tep)
{
    s->modems.use_tep = use_tep;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_real_time_frame_handler(faxtester_state_t *s, faxtester_real_time_frame_handler_t *handler, void *user_data)
{
    s->real_time_frame_handler = handler;
    s->real_time_frame_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_front_end_step_complete_handler(faxtester_state_t *s, faxtester_front_end_step_complete_handler_t *handler, void *user_data)
{
    s->front_end_step_complete_handler = handler;
    s->front_end_step_complete_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_front_end_step_timeout_handler(faxtester_state_t *s, faxtester_front_end_step_complete_handler_t *handler, void *user_data)
{
    s->front_end_step_timeout_handler = handler;
    s->front_end_step_timeout_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

static void faxtester_fax_modems_init(fax_modems_state_t *s, int use_tep, void *user_data)
{
    s->use_tep = use_tep;

    hdlc_rx_init(&s->hdlc_rx, FALSE, FALSE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, user_data);
    hdlc_tx_init(&s->hdlc_tx, FALSE, 2, FALSE, hdlc_underflow_handler, user_data);
    fsk_rx_init(&s->v21_rx, &preset_fsk_specs[FSK_V21CH2], FSK_FRAME_MODE_SYNC, (put_bit_func_t) hdlc_rx_put_bit, &s->hdlc_rx);
    fsk_rx_signal_cutoff(&s->v21_rx, -45.5);
    fsk_tx_init(&s->v21_tx, &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &s->hdlc_tx);
    fsk_tx_set_modem_status_handler(&s->v21_tx, modem_tx_status, user_data);
    v17_rx_init(&s->v17_rx, 14400, non_ecm_put_bit, user_data);
    v17_tx_init(&s->v17_tx, 14400, s->use_tep, non_ecm_get_bit, user_data);
    v17_tx_set_modem_status_handler(&s->v17_tx, modem_tx_status, user_data);
    v29_rx_init(&s->v29_rx, 9600, non_ecm_put_bit, user_data);
    v29_rx_signal_cutoff(&s->v29_rx, -45.5);
    v29_tx_init(&s->v29_tx, 9600, s->use_tep, non_ecm_get_bit, user_data);
    v29_tx_set_modem_status_handler(&s->v29_tx, modem_tx_status, user_data);
    v27ter_rx_init(&s->v27ter_rx, 4800, non_ecm_put_bit, user_data);
    v27ter_tx_init(&s->v27ter_tx, 4800, s->use_tep, non_ecm_get_bit, user_data);
    v27ter_tx_set_modem_status_handler(&s->v27ter_tx, modem_tx_status, user_data);
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

faxtester_state_t *faxtester_init(faxtester_state_t *s, int calling_party)
{
    if (s == NULL)
    {
        if ((s = (faxtester_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }

    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "TST");
    faxtester_fax_modems_init(&s->modems, FALSE, s);
    faxtester_set_timeout(s, -1);
    faxtester_set_tx_type(s, T30_MODEM_NONE, 0, FALSE, FALSE);

    return s;
}
/*- End of function --------------------------------------------------------*/

int faxtester_release(faxtester_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

int faxtester_free(faxtester_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_flush_handler(faxtester_state_t *s, faxtester_flush_handler_t *handler, void *user_data)
{
    s->flush_handler = handler;
    s->flush_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
