/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v8.c - V.8 modem negotiation processing.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: v8.c,v 1.38 2009/02/03 16:28:40 steveu Exp $
 */
 
/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/async.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/power_meter.h"
#include "spandsp/fsk.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/v8.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/fsk.h"
#include "spandsp/private/modem_connect_tones.h"
#include "spandsp/private/v8.h"

#define ms_to_samples(t)    (((t)*SAMPLE_RATE)/1000)

enum
{
    V8_WAIT_1S,
    V8_CI,
    V8_CI_ON,
    V8_CI_OFF,
    V8_HEARD_ANSAM,
    V8_CM_ON,
    V8_CJ_ON,
    V8_CM_WAIT,

    V8_SIGC,
    V8_WAIT_200MS,
    V8_JM_ON,
    V8_SIGA,

    V8_PARKED
} v8_states_e;

enum
{
    V8_SYNC_UNKNOWN = 0,
    V8_SYNC_CI,
    V8_SYNC_CM_JM,
    V8_SYNC_V92
} v8_sync_types_e;

SPAN_DECLARE(const char *) v8_call_function_to_str(int call_function)
{
    switch (call_function)
    {
    case V8_CALL_TBS:
        return "TBS";
    case V8_CALL_H324:
        return "H.324 PSTN multimedia terminal";
    case V8_CALL_V18:
        return "V.18 textphone";
    case V8_CALL_T101:
        return "T.101 videotext";
    case V8_CALL_T30_TX:
        return "T.30 Tx FAX";
    case V8_CALL_T30_RX:
        return "T.30 Rx FAX";
    case V8_CALL_V_SERIES:
        return "V series modem data";
    case V8_CALL_FUNCTION_EXTENSION:
        return "Call function is in extention octet";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v8_modulation_to_str(int modulation_scheme)
{
    switch (modulation_scheme)
    {
    case V8_MOD_V17:
        return "V.17 half-duplex";
    case V8_MOD_V21:
        return "V.21 duplex";
    case V8_MOD_V22:
        return "V.22/V.22bis duplex";
    case V8_MOD_V23HALF:
        return "V.23 half-duplex";
    case V8_MOD_V23:
        return "V.23 duplex";
    case V8_MOD_V26BIS:
        return "V.26bis duplex";
    case V8_MOD_V26TER:
        return "V.26ter duplex";
    case V8_MOD_V27TER:
        return "V.27ter duplex";
    case V8_MOD_V29:
        return "V.29 half-duplex";
    case V8_MOD_V32:
        return "V.32/V.32bis duplex";
    case V8_MOD_V34HALF:
        return "V.34 half-duplex";
    case V8_MOD_V34:
        return "V.34 duplex";
    case V8_MOD_V90:
        return "V.90 duplex";
    case V8_MOD_V92:
        return "V.92 duplex";
    case V8_MOD_FAILED:
        return "negotiation failed";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v8_protocol_to_str(int protocol)
{
    switch (protocol)
    {
    case V8_PROTOCOL_NONE:
        return "None";
    case V8_PROTOCOL_LAPM_V42:
        return "LAPM";
    case V8_PROTOCOL_EXTENSION:
        return "Extension";
    }
    return "Undefined";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v8_pstn_access_to_str(int pstn_access)
{
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v8_pcm_modem_availability_to_str(int pcm_modem_availability)
{
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v8_log_supported_modulations(v8_state_t *s, int modulation_schemes)
{
    const char *comma;
    int i;
    
    comma = "";
    span_log(&s->logging, SPAN_LOG_FLOW, "");
    for (i = 0;  i < 32;  i++)
    {
        if ((modulation_schemes & (1 << i)))
        {
            span_log(&s->logging, SPAN_LOG_FLOW | SPAN_LOG_SUPPRESS_LABELLING, "%s%s", comma, v8_modulation_to_str(modulation_schemes & (1 << i)));
            comma = ", ";
        }
    }
    span_log(&s->logging, SPAN_LOG_FLOW | SPAN_LOG_SUPPRESS_LABELLING, " supported\n");
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_call_function(v8_state_t *s, const uint8_t *p)
{
    int call_function;

    call_function = (*p >> 5) & 0x07;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", v8_call_function_to_str(call_function));
    s->call_function = call_function;
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_modulation_mode(v8_state_t *s, const uint8_t *p)
{
    int far_end_modulations;

    /* Modulation mode octet */
    far_end_modulations = 0;
    if (*p & 0x80)
        far_end_modulations |= V8_MOD_V34HALF;
    if (*p & 0x40)
        far_end_modulations |= V8_MOD_V34;
    if (*p & 0x20)
        far_end_modulations |= V8_MOD_V90;

    if ((*++p & 0x38) == 0x10)
    {
        if (*p & 0x80)
            far_end_modulations |= V8_MOD_V27TER;
        if (*p & 0x40)
            far_end_modulations |= V8_MOD_V29;
        if (*p & 0x04)
            far_end_modulations |= V8_MOD_V17;
        if (*p & 0x02)
            far_end_modulations |= V8_MOD_V22;
        if (*p & 0x01)
            far_end_modulations |= V8_MOD_V32;

        if ((*++p & 0x38) == 0x10)
        {
            if (*p & 0x80)
                far_end_modulations |= V8_MOD_V21;
            if (*p & 0x40)
                far_end_modulations |= V8_MOD_V23HALF;
            if (*p & 0x04)
                far_end_modulations |= V8_MOD_V23;
            if (*p & 0x02)
                far_end_modulations |= V8_MOD_V26BIS;
            if (*p & 0x01)
                far_end_modulations |= V8_MOD_V26TER;
            /* Skip any future extensions we do not understand */
            while  ((*++p & 0x38) == 0x10)
                /* dummy loop */;
        }
    }
    s->far_end_modulations = far_end_modulations;
    v8_log_supported_modulations(s, s->far_end_modulations);
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_protocols(v8_state_t *s, const uint8_t *p)
{
    int protocol;

    protocol = (*p >> 5) & 0x07;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", v8_protocol_to_str(protocol));
    s->protocol = protocol;
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_pstn_access(v8_state_t *s, const uint8_t *p)
{
    int pstn_access;

    pstn_access = (*p >> 5) & 0x07;
    if (pstn_access & V8_PSTN_ACCESS_DCE_ON_DIGTIAL)
        span_log(&s->logging, SPAN_LOG_FLOW, "DCE on digital network connection\n");
    else
        span_log(&s->logging, SPAN_LOG_FLOW, "DCE on analogue network connection\n");
    if (pstn_access & V8_PSTN_ACCESS_ANSWER_DCE_CELLULAR)
        span_log(&s->logging, SPAN_LOG_FLOW, "Answer DCE on cellular connection\n");
    if (pstn_access & V8_PSTN_ACCESS_CALL_DCE_CELLULAR)
        span_log(&s->logging, SPAN_LOG_FLOW, "Call DCE on cellular connection\n");
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_non_standard_facilities(v8_state_t *s, const uint8_t *p)
{
    ++p;
    p += *p;
    return p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_pcm_modem_availability(v8_state_t *s, const uint8_t *p)
{
    int pcm_availability;

    pcm_availability = (*p >> 5) & 0x07;
    if (pcm_availability & V8_PSTN_PCM_MODEM_V91)
        span_log(&s->logging, SPAN_LOG_FLOW, "V.91 available\n");
    if (pcm_availability & V8_PSTN_PCM_MODEM_V90_V92_DIGITAL)
        span_log(&s->logging, SPAN_LOG_FLOW, "V.90 or V.92 digital modem available\n");
    if (pcm_availability & V8_PSTN_PCM_MODEM_V90_V92_ANALOGUE)
        span_log(&s->logging, SPAN_LOG_FLOW, "V.90 or V.92 analogue modem available\n");
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_t66(v8_state_t *s, const uint8_t *p)
{
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static void ci_decode(v8_state_t *s)
{
    if ((s->rx_data[0] & 0x1F) == 0x01)
        process_call_function(s, &s->rx_data[0]);
}
/*- End of function --------------------------------------------------------*/

static void cm_jm_decode(v8_state_t *s)
{
    const uint8_t *p;

    if (s->got_cm_jm)
        return;

    /* We must receive two consecutive identical CM or JM sequences to accept it. */
    if (s->cm_jm_count <= 0
        ||
        s->cm_jm_count != s->rx_data_ptr
        ||
        memcmp(s->cm_jm_data, s->rx_data, s->rx_data_ptr))
    {
        /* Save the current CM or JM sequence */
        s->cm_jm_count = s->rx_data_ptr;
        memcpy(s->cm_jm_data, s->rx_data, s->rx_data_ptr);
        return;
    }
    /* We have a pair of matching CMs or JMs */
    s->got_cm_jm = TRUE;

    span_log(&s->logging, SPAN_LOG_FLOW, "Decoding\n");

    /* Zero indicates the end */
    s->cm_jm_data[s->cm_jm_count] = 0;

    s->far_end_modulations = 0;
    p = s->cm_jm_data;

    while (*p)
    {
        switch (*p & 0x1F)
        {
        case 0x01:
            p = process_call_function(s, p);
            break;
        case 0x05:
            p = process_modulation_mode(s, p);
            break;
        case 0x0A:
            p = process_protocols(s, p);
            break;
        case 0x0D:
            p = process_pstn_access(s, p);
            break;
        case 0x0F:
            p = process_non_standard_facilities(s, p);
            break;
        case 0x07:
            p = process_pcm_modem_availability(s, p);
            break;
        case 0x0E:
            p = process_t66(s, p);
            break;
        default:
            p++;
            break;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void put_bit(void *user_data, int bit)
{
    v8_state_t *s;
    int new_preamble_type;
    const char *tag;
    uint8_t data;

    s = user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case SIG_STATUS_CARRIER_UP:
        case SIG_STATUS_CARRIER_DOWN:
        case SIG_STATUS_TRAINING_SUCCEEDED:
        case SIG_STATUS_TRAINING_FAILED:
            break;
        default:
            break;
        }
        return;
    }
    /* Wait until we sync. */
    s->bit_stream = (s->bit_stream >> 1) | (bit << 19);
    if (s->bit_stream == 0x803FF)
        new_preamble_type = V8_SYNC_CI;
    else if (s->bit_stream == 0xF03FF)
        new_preamble_type = V8_SYNC_CM_JM;
    else if (s->bit_stream == 0xAABFF)
        new_preamble_type = V8_SYNC_V92;
    else
        new_preamble_type = V8_SYNC_UNKNOWN;
    if (new_preamble_type)
    {
        /* Debug */
        if (span_log_test(&s->logging, SPAN_LOG_FLOW))
        {
            if (s->preamble_type == V8_SYNC_CI)
            {
                tag = "CI: ";
            }
            else if (s->preamble_type == V8_SYNC_CM_JM)
            {
                if (s->caller)
                    tag = "JM: ";
                else
                    tag = "CM: ";
            }
            else if (s->preamble_type == V8_SYNC_V92)
            {
                tag = "V92: ";
            }
            else
            {
                tag = "??: ";
            }
            span_log_buf(&s->logging, SPAN_LOG_FLOW, tag, s->rx_data, s->rx_data_ptr);
        }
        /* Decode previous sequence */
        switch (s->preamble_type)
        {
        case V8_SYNC_CI:
            ci_decode(s);
            break;
        case V8_SYNC_CM_JM:
            cm_jm_decode(s);
            break;
        }
        s->preamble_type = new_preamble_type;
        s->bit_cnt = 0;
        s->rx_data_ptr = 0;
    }
    
    /* Parse octets with 1 bit start, 1 bit stop */
    if (s->preamble_type)
    {
        s->bit_cnt++;
        /* Start, stop? */
        if ((s->bit_stream & 0x80400) == 0x80000  &&  s->bit_cnt >= 10)
        {
            /* Store the available data */
            data = (uint8_t) ((s->bit_stream >> 11) & 0xFF);
            /* CJ detection */
            if (data == 0)
            {
                if (++s->zero_byte_count == 3)
                    s->got_cj = TRUE;
            }
            else
            {
                s->zero_byte_count = 0;
            }

            if (s->rx_data_ptr < (int) (sizeof(s->rx_data) - 1))
                s->rx_data[s->rx_data_ptr++] = data;
            s->bit_cnt = 0;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void v8_decode_init(v8_state_t *s)
{
    if (s->caller)
        fsk_rx_init(&s->v21rx, &preset_fsk_specs[FSK_V21CH2], FALSE, put_bit, s);
    else
        fsk_rx_init(&s->v21rx, &preset_fsk_specs[FSK_V21CH1], FALSE, put_bit, s);
    s->preamble_type = 0;
    s->bit_stream = 0;
    s->cm_jm_count = 0;
    s->got_cm_jm = FALSE;
    s->got_cj = FALSE;
    s->zero_byte_count = 0;
    s->rx_data_ptr = 0;
}
/*- End of function --------------------------------------------------------*/

static int get_bit(void *user_data)
{
    v8_state_t *s;
    uint8_t bit;

    s = user_data;
    if (queue_read(s->tx_queue, &bit, 1) <= 0)
        bit = 1;
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void v8_put_byte(v8_state_t *s, int data)
{
    int i;
    uint8_t bits[10];

    /* Insert start & stop bits */
    bits[0] = 0;
    for (i = 1;  i < 9;  i++)
    {
        bits[i] = (uint8_t) (data & 1);
        data >>= 1;
    }
    bits[9] = 1;
    queue_write(s->tx_queue, bits, 10);
}
/*- End of function --------------------------------------------------------*/

static void send_cm_jm(v8_state_t *s, int mod_mask)
{
    int val;
    static const uint8_t preamble[20] =
    {
        /* 10 1's (0x3FF), then 10 bits of CM sync (0x00F) */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1
    };

    /* Send a CM, or a JM as appropriate */
    queue_write(s->tx_queue, preamble, 20);
    
    /* Data call */
    v8_put_byte(s, (V8_CALL_V_SERIES << 5) | 0x01);
    
    /* Supported modulations */
    val = 0x05;
    if (mod_mask & V8_MOD_V90)
        val |= 0x20;
    if (mod_mask & V8_MOD_V34)
        val |= 0x40;
    v8_put_byte(s, val);

    val = 0x10;
    if (mod_mask & V8_MOD_V32)
        val |= 0x01;
    if (mod_mask & V8_MOD_V22)
        val |= 0x02;
    if (mod_mask & V8_MOD_V17)
        val |= 0x04;
    if (mod_mask & V8_MOD_V29)
        val |= 0x40;
    if (mod_mask & V8_MOD_V27TER)
        val |= 0x80;
    v8_put_byte(s, val);

    val = 0x10;
    if (mod_mask & V8_MOD_V26TER)
        val |= 0x01;
    if (mod_mask & V8_MOD_V26BIS)
        val |= 0x02;
    if (mod_mask & V8_MOD_V23)
        val |= 0x04;
    if (mod_mask & V8_MOD_V23HALF)
        val |= 0x40;
    if (mod_mask & V8_MOD_V21)
        val |= 0x80;
    v8_put_byte(s, val);

    v8_put_byte(s, (0 << 5) | 0x07);

    v8_put_byte(s, (V8_PROTOCOL_LAPM_V42 << 5) | 0x0A);

    /* No cellular right now */    
    v8_put_byte(s, (0 << 5) | 0x0D);
}
/*- End of function --------------------------------------------------------*/

static int select_modulation(int mask)
{
    if (mask & V8_MOD_V90)
        return V8_MOD_V90;
    if (mask & V8_MOD_V34)
        return V8_MOD_V34;
    if (mask & V8_MOD_V32)
        return V8_MOD_V32;
    if (mask & V8_MOD_V23)
        return V8_MOD_V23;
    if (mask & V8_MOD_V21)
        return V8_MOD_V21;
    return V8_MOD_FAILED;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v8_tx(v8_state_t *s, int16_t *amp, int max_len)
{
    int len;

    //span_log(&s->logging, SPAN_LOG_FLOW, "v8_tx state %d\n", s->state);
    len = 0;
    switch (s->state)
    {
    case V8_CI_ON:
    case V8_CM_ON:
    case V8_JM_ON:
    case V8_CJ_ON:
        len = fsk_tx(&s->v21tx, amp, max_len);
        break;
    case V8_CM_WAIT:
        /* Send the ANSam tone */
        len = modem_connect_tones_tx(&s->ansam_tx, amp, max_len);
        break;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v8_rx(v8_state_t *s, const int16_t *amp, int len)
{
    int i;
    int residual_samples;
    v8_result_t result;
    static const uint8_t preamble[20] =
    {
        /* 10 1's (0x3FF), then 10 bits of CI sync (0x001) */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
    };

    //span_log(&s->logging, SPAN_LOG_FLOW, "v8_rx state %d\n", s->state);
    residual_samples = 0;
    switch (s->state)
    {
    case V8_WAIT_1S:
        /* Wait 1 second before sending the first CI packet */
        if ((s->negotiation_timer -= len) > 0)
            break;
        s->state = V8_CI;
        s->ci_count = 0;
        modem_connect_tones_rx_init(&s->ansam_rx, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);
        fsk_tx_init(&s->v21tx, &preset_fsk_specs[FSK_V21CH1], get_bit, s);
        /* Fall through to the next state */
    case V8_CI:
        residual_samples = modem_connect_tones_rx(&s->ansam_rx, amp, len);
        /* Send 4 CI packets in a burst (the spec says at least 3) */
        for (i = 0;  i < 4;  i++)
        {
            /* 10 1's (0x3FF), then CI sync (0x001) */
            queue_write(s->tx_queue, preamble, 20);
            v8_put_byte(s, (V8_CALL_V_SERIES << 5) | 0x01);
        }
        s->state = V8_CI_ON;
        break;
    case V8_CI_ON:
        residual_samples = modem_connect_tones_rx(&s->ansam_rx, amp, len);
        if (queue_empty(s->tx_queue))
        {
            s->state = V8_CI_OFF;
            s->ci_timer = ms_to_samples(500);
        }
        break;
    case V8_CI_OFF:
        residual_samples = modem_connect_tones_rx(&s->ansam_rx, amp, len);
        /* Check if an ANSam tone has been detected */
        if (modem_connect_tones_rx_get(&s->ansam_rx))
        {
            /* Set the Te interval. The spec. says 500ms is the minimum,
               but gives reasons why 1 second is a better value. */
            s->ci_timer = ms_to_samples(1000);
            s->state = V8_HEARD_ANSAM;
            break;
        }
        if ((s->ci_timer -= len) <= 0)
        {
            if (++s->ci_count >= 10)
            {
                /* The spec says we should give up now. */
                s->state = V8_PARKED;
                if (s->result_handler)
                    s->result_handler(s->result_handler_user_data, NULL);
            }
            else
            {
                /* Try again */
                s->state = V8_CI;
            }
        }
        break;
    case V8_HEARD_ANSAM:
        /* We have heard the ANSam signal, but we still need to wait for the
           end of the Te timeout period to comply with the spec. */
        if ((s->ci_timer -= len) <= 0)
        {
            v8_decode_init(s);
            s->state = V8_CM_ON;
            s->negotiation_timer = ms_to_samples(5000);
            send_cm_jm(s, s->available_modulations);
        }
        break;
    case V8_CM_ON:
        residual_samples = fsk_rx(&s->v21rx, amp, len);
        if (s->got_cm_jm)
        {
            /* Now JM has been detected we send CJ and wait for 75 ms
               before finishing the V.8 analysis. */
            s->negotiated_modulation = select_modulation(s->far_end_modulations);

            queue_flush(s->tx_queue);
            for (i = 0;  i < 9;  i++)
                v8_put_byte(s, 0);
            s->state = V8_CJ_ON;
            break;
        }
        if ((s->negotiation_timer -= len) <= 0)
        {
            /* Timeout */
            s->state = V8_PARKED;
            if (s->result_handler)
                s->result_handler(s->result_handler_user_data, NULL);
        }
        if (queue_empty(s->tx_queue))
        {
            /* Send CM again */
            send_cm_jm(s, s->available_modulations);
        }
        break;
    case V8_CJ_ON:
        residual_samples = fsk_rx(&s->v21rx, amp, len);
        if (queue_empty(s->tx_queue))
        {
            s->negotiation_timer = ms_to_samples(75);
            s->state = V8_SIGC;
        }
        break;
    case V8_SIGC:
        if ((s->negotiation_timer -= len) <= 0)
        {
            /* The V.8 negotiation has succeeded. */
            s->state = V8_PARKED;
            if (s->result_handler)
            {
                result.call_function = s->call_function;
                result.available_modulations = s->far_end_modulations;
                result.negotiated_modulation = s->negotiated_modulation;
                result.protocol = s->protocol;
                result.pstn_access = s->pstn_access;
                result.nsf_seen = s->nsf_seen;
                result.pcm_modem_availability = s->pcm_modem_availability;
                result.t66_seen = s->t66_seen;
                s->result_handler(s->result_handler_user_data, &result);
            }
        }
        break;
    case V8_WAIT_200MS:
        if ((s->negotiation_timer -= len) <= 0)
        {
            /* Send the ANSam tone */
            modem_connect_tones_tx_init(&s->ansam_tx, MODEM_CONNECT_TONES_ANSAM_PR);
                
            v8_decode_init(s);
            s->state = V8_CM_WAIT;
            s->negotiation_timer = ms_to_samples(5000);
        }
        break;
    case V8_CM_WAIT:
        residual_samples = fsk_rx(&s->v21rx, amp, len);
        if (s->got_cm_jm)
        {
            /* Stop sending ANSam and send JM instead */
            fsk_tx_init(&s->v21tx, &preset_fsk_specs[FSK_V21CH2], get_bit, s);
            /* Set the timeout for JM */
            s->negotiation_timer = ms_to_samples(5000); 
            s->state = V8_JM_ON;
            s->common_modulations = s->available_modulations & s->far_end_modulations;
            s->negotiated_modulation = select_modulation(s->common_modulations);
            send_cm_jm(s, s->common_modulations);
            break;
        }
        if ((s->negotiation_timer -= len) <= 0)
        {
            /* Timeout */
            s->state = V8_PARKED;
            if (s->result_handler)
                s->result_handler(s->result_handler_user_data, NULL);
        }
        break;
    case V8_JM_ON:
        residual_samples = fsk_rx(&s->v21rx, amp, len);
        if (s->got_cj)
        {
            /* Stop sending JM, and wait 75 ms */
            s->negotiation_timer = ms_to_samples(75);
            s->state = V8_SIGA;
            break;
        }
        if ((s->negotiation_timer -= len) <= 0)
        {
            /* Timeout */
            s->state = V8_PARKED;
            if (s->result_handler)
                s->result_handler(s->result_handler_user_data, NULL);
            break;
        }
        if (queue_empty(s->tx_queue))
        {
            /* Send JM */
            send_cm_jm(s, s->common_modulations);
        }
        break;
    case V8_SIGA:
        if ((s->negotiation_timer -= len) <= 0)
        {
            /* The V.8 negotiation has succeeded. */
            s->state = V8_PARKED;
            if (s->result_handler)
            {
                result.call_function = s->call_function;
                result.available_modulations = s->far_end_modulations;
                result.negotiated_modulation = s->negotiated_modulation;
                result.protocol = s->protocol;
                result.pstn_access = s->pstn_access;
                result.nsf_seen = s->nsf_seen;
                result.pcm_modem_availability = s->pcm_modem_availability;
                result.t66_seen = s->t66_seen;
                s->result_handler(s->result_handler_user_data, &result);
            }
        }
        break;
    case V8_PARKED:
        residual_samples = len;
        break;
    }
    return residual_samples;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v8_get_logging_state(v8_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v8_state_t *) v8_init(v8_state_t *s,
                                   int caller,
                                   int available_modulations,
                                   v8_result_handler_t *result_handler,
                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (v8_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->caller = caller;
    s->available_modulations = available_modulations;
    s->result_handler = result_handler;
    s->result_handler_user_data = user_data;

    s->ci_timer = 0;
    if (s->caller)
    {
        s->state = V8_WAIT_1S;
        s->negotiation_timer = ms_to_samples(1000);
    }
    else
    {
        s->state = V8_WAIT_200MS;
        s->negotiation_timer = ms_to_samples(200);
    }
    if ((s->tx_queue = queue_init(NULL, 1024, 0)) == NULL)
        return NULL;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v8_release(v8_state_t *s)
{
    return queue_free(s->tx_queue);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v8_free(v8_state_t *s)
{
    int ret;
    
    ret = queue_free(s->tx_queue);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
