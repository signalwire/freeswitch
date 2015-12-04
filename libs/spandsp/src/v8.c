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
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
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

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/async.h"
#include "spandsp/vector_int.h"
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
#include "spandsp/private/power_meter.h"
#include "spandsp/private/fsk.h"
#include "spandsp/private/modem_connect_tones.h"
#include "spandsp/private/v8.h"

enum
{
    V8_WAIT_1S = 0,     /* Start point when sending CI */
    V8_AWAIT_ANSAM,     /* Start point when sending initial silence */
    V8_CI_ON,
    V8_CI_OFF,
    V8_HEARD_ANSAM,
    V8_CM_ON,
    V8_CJ_ON,
    V8_CM_WAIT,

    V8_SIGC,
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

enum
{
    V8_CALL_FUNCTION_TAG = 0x01,
    V8_MODULATION_TAG = 0x05,
    V8_PROTOCOLS_TAG = 0x0A,
    V8_PSTN_ACCESS_TAG = 0x0D,
    V8_NSF_TAG = 0x0F,
    V8_PCM_MODEM_AVAILABILITY_TAG = 0x07,
    V8_T66_TAG = 0x0E
};

enum
{
    V8_CI_SYNC_OCTET = 0x00,
    V8_CM_JM_SYNC_OCTET = 0xE0,
    V8_V92_SYNC_OCTET = 0x55
};

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
        return "Call function is in extension octet";
    }
    return "Unknown call function";
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
    case V8_MOD_V23HDX:
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
    case V8_MOD_V34HDX:
        return "V.34 half-duplex";
    case V8_MOD_V34:
        return "V.34 duplex";
    case V8_MOD_V90:
        return "V.90 duplex";
    case V8_MOD_V92:
        return "V.92 duplex";
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
    switch (pstn_access)
    {
    case V8_PSTN_ACCESS_CALL_DCE_CELLULAR:
        return "Calling modem on cellular";
    case V8_PSTN_ACCESS_ANSWER_DCE_CELLULAR:
        return "Answering modem on cellular";
    case (V8_PSTN_ACCESS_ANSWER_DCE_CELLULAR | V8_PSTN_ACCESS_CALL_DCE_CELLULAR):
        return "Answering and calling modems on cellular";
    case V8_PSTN_ACCESS_DCE_ON_DIGITAL:
        return "DCE on digital";
    case (V8_PSTN_ACCESS_DCE_ON_DIGITAL | V8_PSTN_ACCESS_CALL_DCE_CELLULAR):
        return "DCE on digital, and calling modem on cellular";
    case (V8_PSTN_ACCESS_DCE_ON_DIGITAL | V8_PSTN_ACCESS_ANSWER_DCE_CELLULAR):
        return "DCE on digital, answering modem on cellular";
    case (V8_PSTN_ACCESS_DCE_ON_DIGITAL | V8_PSTN_ACCESS_ANSWER_DCE_CELLULAR | V8_PSTN_ACCESS_CALL_DCE_CELLULAR):
        return "DCE on digital, and answering and calling modems on cellular";
    }
    return "PSTN access unknown";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v8_nsf_to_str(int nsf)
{
    switch (nsf)
    {
    case 0:
        return "???";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v8_pcm_modem_availability_to_str(int pcm_modem_availability)
{
    switch (pcm_modem_availability)
    {
    case 0:
        return "PCM unavailable";
    case V8_PSTN_PCM_MODEM_V90_V92_ANALOGUE:
        return "V.90/V.92 analogue available";
    case V8_PSTN_PCM_MODEM_V90_V92_DIGITAL:
        return "V.90/V.92 digital available";
    case (V8_PSTN_PCM_MODEM_V90_V92_DIGITAL | V8_PSTN_PCM_MODEM_V90_V92_ANALOGUE):
        return "V.90/V.92 digital/analogue available";
    case V8_PSTN_PCM_MODEM_V91:
        return "V.91 available";
    case (V8_PSTN_PCM_MODEM_V91 | V8_PSTN_PCM_MODEM_V90_V92_ANALOGUE):
        return "V.91 and V.90/V.92 analogue available";
    case (V8_PSTN_PCM_MODEM_V91 | V8_PSTN_PCM_MODEM_V90_V92_DIGITAL):
        return "V.91 and V.90/V.92 digital available";
    case (V8_PSTN_PCM_MODEM_V91 | V8_PSTN_PCM_MODEM_V90_V92_DIGITAL | V8_PSTN_PCM_MODEM_V90_V92_ANALOGUE):
        return "V.91 and V.90/V.92 digital/analogue available";
    }
    return "PCM availability unknown";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) v8_t66_to_str(int t66)
{
    /* T.66 doesn't really define any V.8 values. The bits are all reserved. */
    switch (t66)
    {
    case 0:
        return "???";
    case 1:
        return "Reserved TIA";
    case 2:
        return "Reserved";
    case 3:
        return "Reserved TIA + others";
    case 4:
        return "Reserved";
    case 5:
        return "Reserved TIA + others";
    case 6:
        return "Reserved";
    case 7:
        return "Reserved TIA + others";
    }
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

static int report_event(v8_state_t *s)
{
    if (s->result_handler)
        s->result_handler(s->result_handler_user_data, &s->result);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_call_function(v8_state_t *s, const uint8_t *p)
{
    s->result.call_function = (*p >> 5) & 0x07;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", v8_call_function_to_str(s->result.call_function));
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_modulation_mode(v8_state_t *s, const uint8_t *p)
{
    unsigned int modulations;

    /* Modulation mode octets */
    /* We must record the number of bytes of modulation information, so a resulting
       JM can be made to have the same number (V.8/8.2.3) */
    modulations = 0;
    s->modulation_bytes = 1;
    if (*p & 0x80)
        modulations |= V8_MOD_V34HDX;
    if (*p & 0x40)
        modulations |= V8_MOD_V34;
    if (*p & 0x20)
        modulations |= V8_MOD_V90;
    ++p;

    /* Check for an extension octet */
    if ((*p & 0x38) == 0x10)
    {
        s->modulation_bytes++;
        if (*p & 0x80)
            modulations |= V8_MOD_V27TER;
        if (*p & 0x40)
            modulations |= V8_MOD_V29;
        if (*p & 0x04)
            modulations |= V8_MOD_V17;
        if (*p & 0x02)
            modulations |= V8_MOD_V22;
        if (*p & 0x01)
            modulations |= V8_MOD_V32;
        ++p;

        /* Check for an extension octet */
        if ((*p & 0x38) == 0x10)
        {
            s->modulation_bytes++;
            if (*p & 0x80)
                modulations |= V8_MOD_V21;
            if (*p & 0x40)
                modulations |= V8_MOD_V23HDX;
            if (*p & 0x04)
                modulations |= V8_MOD_V23;
            if (*p & 0x02)
                modulations |= V8_MOD_V26BIS;
            if (*p & 0x01)
                modulations |= V8_MOD_V26TER;
             ++p;
        }
    }
    s->result.modulations = modulations;
    v8_log_supported_modulations(s, modulations);
    return p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_protocols(v8_state_t *s, const uint8_t *p)
{
    s->result.protocol = (*p >> 5) & 0x07;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", v8_protocol_to_str(s->result.protocol));
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_pstn_access(v8_state_t *s, const uint8_t *p)
{
    s->result.pstn_access = (*p >> 5) & 0x07;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", v8_pstn_access_to_str(s->result.pstn_access));
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_non_standard_facilities(v8_state_t *s, const uint8_t *p)
{
    /* TODO: This is wrong */
    s->result.nsf = (*p >> 5) & 0x07;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", v8_nsf_to_str(s->result.nsf));
    return p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_pcm_modem_availability(v8_state_t *s, const uint8_t *p)
{
    s->result.pcm_modem_availability = (*p >> 5) & 0x07;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", v8_pcm_modem_availability_to_str(s->result.pcm_modem_availability));
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static const uint8_t *process_t66(v8_state_t *s, const uint8_t *p)
{
    s->result.t66 = (*p >> 5) & 0x07;
    span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", v8_t66_to_str(s->result.t66));
    return ++p;
}
/*- End of function --------------------------------------------------------*/

static void ci_decode(v8_state_t *s)
{
    if ((s->rx_data[0] & 0x1F) == V8_CALL_FUNCTION_TAG)
        process_call_function(s, &s->rx_data[0]);
}
/*- End of function --------------------------------------------------------*/

static void cm_jm_decode(v8_state_t *s)
{
    const uint8_t *p;

    if (s->got_cm_jm)
        return;

    /* We must receive two consecutive identical CM or JM sequences to accept it. */
    if (s->cm_jm_len <= 0
        ||
        s->cm_jm_len != s->rx_data_ptr
        ||
        memcmp(s->cm_jm_data, s->rx_data, s->rx_data_ptr))
    {
        /* Save the current CM or JM sequence */
        s->cm_jm_len = s->rx_data_ptr;
        memcpy(s->cm_jm_data, s->rx_data, s->rx_data_ptr);
        return;
    }
    /* We have a matching pair of CMs or JMs, so we are happy this is correct. */
    s->got_cm_jm = true;

    span_log(&s->logging, SPAN_LOG_FLOW, "Decoding\n");

    /* Zero indicates the end */
    s->cm_jm_data[s->cm_jm_len] = 0;

    s->result.modulations = 0;
    p = s->cm_jm_data;

    while (*p)
    {
        switch (*p & 0x1F)
        {
        case V8_CALL_FUNCTION_TAG:
            p = process_call_function(s, p);
            break;
        case V8_MODULATION_TAG:
            p = process_modulation_mode(s, p);
            break;
        case V8_PROTOCOLS_TAG:
            p = process_protocols(s, p);
            break;
        case V8_PSTN_ACCESS_TAG:
            p = process_pstn_access(s, p);
            break;
        case V8_NSF_TAG:
            p = process_non_standard_facilities(s, p);
            break;
        case V8_PCM_MODEM_AVAILABILITY_TAG:
            p = process_pcm_modem_availability(s, p);
            break;
        case V8_T66_TAG:
            p = process_t66(s, p);
            break;
        default:
            p++;
            break;
        }
        /* Skip any future extensions we do not understand */
        while ((*p & 0x38) == 0x10)
            p++;
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
    //span_log(&s->logging, SPAN_LOG_FLOW, "Bit %d\n", bit);
    /* Wait until we sync. */
    s->bit_stream = (s->bit_stream >> 1) | (bit << 19);
    /* CI preamble is 10 ones then a framed 0x00
       CM/JM preamble is 10 ones then a framed 0x07
       V.92 preamble is 10 ones then a framed 0x55
       Should we look at all 10 ones? The first couple might be
       settling down. */
    /* The preamble + synchronisation bit sequence should be unique in
       any bit stream, so we can rely on seeing this at any time as being
       a real sync code. */
    switch (s->bit_stream)
    {
    case 0x803FF:
        new_preamble_type = V8_SYNC_CI;
        break;
    case 0xF03FF:
        new_preamble_type = V8_SYNC_CM_JM;
        break;
    case 0xAABFF:
        new_preamble_type = V8_SYNC_V92;
        break;
    default:
        new_preamble_type = V8_SYNC_UNKNOWN;
        break;
    }
    if (new_preamble_type != V8_SYNC_UNKNOWN)
    {
        /* We have seen a fresh sync code */
        /* Debug */
        if (span_log_test(&s->logging, SPAN_LOG_FLOW))
        {
            if (s->preamble_type != V8_SYNC_UNKNOWN)
            {
                switch (s->preamble_type)
                {
                case V8_SYNC_CI:
                    tag = ">CI: ";
                    break;
                case V8_SYNC_CM_JM:
                    tag = (s->calling_party)  ?  ">JM: "  :  ">CM: ";
                    break;
                case V8_SYNC_V92:
                    tag = ">V.92: ";
                    break;
                default:
                    tag = ">??: ";
                    break;
                }
                span_log_buf(&s->logging, SPAN_LOG_FLOW, tag, s->rx_data, s->rx_data_ptr);
            }
        }
        /* If we were handling a valid sync code then we should process what has been
           received to date. */
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

    if (s->preamble_type != V8_SYNC_UNKNOWN)
    {
        /* Parse octets with 1 bit start, 1 bit stop */
        s->bit_cnt++;
        /* Start, stop? */
        if ((s->bit_stream & 0x80400) == 0x80000  &&  s->bit_cnt >= 10)
        {
            /* Store the available data */
            data = (uint8_t) ((s->bit_stream >> 11) & 0xFF);
            /* CJ (3 successive zero octets) detection */
            if (data == 0)
            {
                if (++s->zero_byte_count == 3)
                    s->got_cj = true;
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
    fsk_rx_init(&s->v21rx,
                &preset_fsk_specs[(s->calling_party)  ?  FSK_V21CH2  :  FSK_V21CH1],
                FSK_FRAME_MODE_ASYNC,
                put_bit,
                s);
    fsk_rx_signal_cutoff(&s->v21rx, -45.5f);
    s->preamble_type = V8_SYNC_UNKNOWN;
    s->bit_stream = 0;
    s->cm_jm_len = 0;
    s->got_cm_jm = false;
    s->got_cj = false;
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
        return SIG_STATUS_END_OF_DATA;
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void v8_put_preamble(v8_state_t *s)
{
    static const uint8_t preamble[10] =
    {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };

    queue_write(s->tx_queue, preamble, 10);
}
/*- End of function --------------------------------------------------------*/

static void v8_put_bytes(v8_state_t *s, uint8_t buf[], int len)
{
    int i;
    int j;
    uint8_t byte;
    uint8_t bits[10];

    /* Insert start & stop bits */
    for (i = 0;  i < len;  i++)
    {
        bits[0] = 0;
        byte = buf[i];
        for (j = 1;  j < 9;  j++)
        {
            bits[j] = byte & 1;
            byte >>= 1;
        }
        bits[9] = 1;
        queue_write(s->tx_queue, bits, 10);
    }
}
/*- End of function --------------------------------------------------------*/

static void send_cm_jm(v8_state_t *s)
{
    int val;
    unsigned int offered_modulations;
    int bytes;
    uint8_t buf[10];
    int ptr;

    /* Send a CM, or a JM as appropriate */
    v8_put_preamble(s);
    ptr = 0;
    buf[ptr++] = V8_CM_JM_SYNC_OCTET;
    /* Data call */
    buf[ptr++] = (s->result.call_function << 5) | V8_CALL_FUNCTION_TAG;

    /* Supported modulations */
    offered_modulations = s->result.modulations;
    bytes = 0;
    val = 0x05;
    if (offered_modulations & V8_MOD_V90)
        val |= 0x20;
    if (offered_modulations & V8_MOD_V34)
        val |= 0x40;
    if (offered_modulations & V8_MOD_V34HDX)
        val |= 0x80;
    buf[ptr++] = val;
    if (++bytes < s->modulation_bytes)
    {
        val = 0x10;
        if (offered_modulations & V8_MOD_V32)
            val |= 0x01;
        if (offered_modulations & V8_MOD_V22)
            val |= 0x02;
        if (offered_modulations & V8_MOD_V17)
            val |= 0x04;
        if (offered_modulations & V8_MOD_V29)
            val |= 0x40;
        if (offered_modulations & V8_MOD_V27TER)
            val |= 0x80;
        buf[ptr++] = val;
    }
    if (++bytes < s->modulation_bytes)
    {
        val = 0x10;
        if (offered_modulations & V8_MOD_V26TER)
            val |= 0x01;
        if (offered_modulations & V8_MOD_V26BIS)
            val |= 0x02;
        if (offered_modulations & V8_MOD_V23)
            val |= 0x04;
        if (offered_modulations & V8_MOD_V23HDX)
            val |= 0x40;
        if (offered_modulations & V8_MOD_V21)
            val |= 0x80;
        buf[ptr++] = val;
    }

    if (s->parms.protocol)
        buf[ptr++] = (s->parms.protocol << 5) | V8_PROTOCOLS_TAG;
    if (s->parms.pstn_access)
        buf[ptr++] = (s->parms.pstn_access << 5) | V8_PSTN_ACCESS_TAG;
    if (s->parms.pcm_modem_availability)
        buf[ptr++] = (s->parms.pcm_modem_availability << 5) | V8_PCM_MODEM_AVAILABILITY_TAG;
    if (s->parms.t66 >= 0)
        buf[ptr++] = (s->parms.t66 << 5) | V8_T66_TAG;
    /* No NSF */
    //buf[ptr++] = (0 << 5) | V8_NSF_TAG;
    span_log_buf(&s->logging, SPAN_LOG_FLOW, (s->calling_party)  ?  "<CM: "  :  "<JM: ", &buf[1], ptr - 1);
    v8_put_bytes(s, buf, ptr);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v8_tx(v8_state_t *s, int16_t *amp, int max_len)
{
    int len;

    //span_log(&s->logging, SPAN_LOG_FLOW, "v8_tx state %d\n", s->state);
    len = 0;
    if (s->modem_connect_tone_tx_on)
    {
        if (s->modem_connect_tone_tx_on == (ms_to_samples(75) + 2))
        {
            if (s->fsk_tx_on)
            {
                /* The initial silence is over */
                s->modem_connect_tone_tx_on = 0;
            }
        }
        else if (s->modem_connect_tone_tx_on == (ms_to_samples(75) + 1))
        {
            /* Send the ANSam tone */
            len = modem_connect_tones_tx(&s->ansam_tx, amp, max_len);
            if (len < max_len)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "ANSam or ANSam/ ended\n");
                s->modem_connect_tone_tx_on = ms_to_samples(75);
            }
        }
        else
        {
            /* Send the 75ms of silence after the ANSam tone */
            if (max_len > s->modem_connect_tone_tx_on)
                len = s->modem_connect_tone_tx_on;
            else
                len = max_len;
            vec_zeroi16(amp, len);
            s->modem_connect_tone_tx_on -= len;
        }
    }
    if (s->fsk_tx_on  &&  len < max_len)
    {
        len += fsk_tx(&s->v21tx, &amp[len], max_len - len);
        if (len < max_len)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "FSK ends (%d/%d) %d %d\n", len, max_len, s->fsk_tx_on, s->state);
            s->fsk_tx_on = false;
            //s->state = V8_PARKED;
        }
    }
    if (s->state != V8_PARKED  &&  len < max_len)
    {
        vec_zeroi16(&amp[len], max_len - len);
        len = max_len;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static void send_v92(v8_state_t *s)
{
    int i;
    uint8_t buf[2];

    if (s->result.v92 >= 0)
    {
        /* Send 2 V.92 packets */
        for (i = 0;  i < 2;  i++)
        {
            v8_put_preamble(s);
            buf[0] = V8_V92_SYNC_OCTET;
            buf[1] = s->result.v92;
            span_log_buf(&s->logging, SPAN_LOG_FLOW, "<V.92: ", &buf[1], 1);
            v8_put_bytes(s, buf, 2);
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void send_ci(v8_state_t *s)
{
    int i;
    uint8_t buf[2];

    /* Send 4 CI packets in a burst (the spec says at least 3) */
    for (i = 0;  i < 4;  i++)
    {
        v8_put_preamble(s);
        buf[0] = V8_CI_SYNC_OCTET;
        buf[1] = (s->result.call_function << 5) | V8_CALL_FUNCTION_TAG;
        span_log_buf(&s->logging, SPAN_LOG_FLOW, "<CI: ", &buf[1], 1);
        v8_put_bytes(s, buf, 2);
    }
}
/*- End of function --------------------------------------------------------*/

static void handle_modem_connect_tone(v8_state_t *s, int tone)
{
    s->result.modem_connect_tone = tone;
    span_log(&s->logging, SPAN_LOG_FLOW, "'%s' recognised\n", modem_connect_tone_to_str(tone));
    if (tone == MODEM_CONNECT_TONES_ANSAM
        ||
        tone == MODEM_CONNECT_TONES_ANSAM_PR)
    {
        /* Set the Te interval. The spec. says 500ms is the minimum,
           but gives reasons why 1 second is a better value (V.8/8.1.1). */
        s->state = V8_HEARD_ANSAM;
        s->ci_timer = ms_to_samples(1000);
    }
    else
    {
        /* If we found a connect tone, and it isn't one of the modulated answer tones,
           indicating V.8 startup, we are not going to do V.8 processing. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Non-V.8 modem connect tone detected\n");
        s->state = V8_PARKED;
        s->result.status = V8_STATUS_NON_V8_CALL;
        report_event(s);
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v8_rx(v8_state_t *s, const int16_t *amp, int len)
{
    int residual_samples;
    int tone;
    uint8_t buf[3];

    //span_log(&s->logging, SPAN_LOG_FLOW, "v8_rx state %d\n", s->state);
    residual_samples = 0;
    switch (s->state)
    {
    case V8_WAIT_1S:
        residual_samples = modem_connect_tones_rx(&s->ansam_rx, amp, len);
        /* Wait 1 second before sending the first CI packet */
        if ((s->negotiation_timer -= len) > 0)
            break;
        fsk_tx_restart(&s->v21tx, &preset_fsk_specs[FSK_V21CH1]);
        send_ci(s);
        s->state = V8_CI_ON;
        s->fsk_tx_on = true;
        break;
    case V8_CI_ON:
        residual_samples = modem_connect_tones_rx(&s->ansam_rx, amp, len);
        /* Check if an ANSam or ANSam/ tone has been detected */
        if ((tone = modem_connect_tones_rx_get(&s->ansam_rx)) != MODEM_CONNECT_TONES_NONE)
        {
            handle_modem_connect_tone(s, tone);
            break;
        }
        if (!s->fsk_tx_on)
        {
            s->state = V8_CI_OFF;
            s->ci_timer = ms_to_samples(500);
            break;
        }
        break;
    case V8_CI_OFF:
        residual_samples = modem_connect_tones_rx(&s->ansam_rx, amp, len);
        /* Check if an ANSam or ANSam/ tone has been detected */
        if ((tone = modem_connect_tones_rx_get(&s->ansam_rx)) != MODEM_CONNECT_TONES_NONE)
        {
            handle_modem_connect_tone(s, tone);
            break;
        }
        if ((s->ci_timer -= len) <= 0)
        {
            if (++s->ci_count >= 10)
            {
                /* The spec says we should give up now. */
                span_log(&s->logging, SPAN_LOG_FLOW, "Timeout waiting for modem connect tone\n");
                s->state = V8_PARKED;
                s->result.status = V8_STATUS_FAILED;
                report_event(s);
            }
            else
            {
                /* Try again */
                fsk_tx_restart(&s->v21tx, &preset_fsk_specs[FSK_V21CH1]);
                send_ci(s);
                s->state = V8_CI_ON;
                s->fsk_tx_on = true;
            }
        }
        break;
    case V8_AWAIT_ANSAM:
        residual_samples = modem_connect_tones_rx(&s->ansam_rx, amp, len);
        /* Check if an ANSam or ANSam/ tone has been detected */
        if ((tone = modem_connect_tones_rx_get(&s->ansam_rx)) != MODEM_CONNECT_TONES_NONE)
            handle_modem_connect_tone(s, tone);
        break;
    case V8_HEARD_ANSAM:
        /* We have heard the ANSam or ANSam/ signal, but we still need to wait for the
           end of the Te timeout period to comply with the spec. */
        if ((s->ci_timer -= len) <= 0)
        {
            v8_decode_init(s);
            s->negotiation_timer = ms_to_samples(5000);
            fsk_tx_restart(&s->v21tx, &preset_fsk_specs[FSK_V21CH1]);
            send_v92(s);
            send_cm_jm(s);
            s->state = V8_CM_ON;
            s->fsk_tx_on = true;
        }
        break;
    case V8_CM_ON:
        residual_samples = fsk_rx(&s->v21rx, amp, len);
        if (s->got_cm_jm)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "JM recognised\n");
            /* Now JM has been detected, we send CJ and wait for 75 ms
               before finishing the V.8 analysis. */
            fsk_tx_restart(&s->v21tx, &preset_fsk_specs[FSK_V21CH1]);
            memset(buf, 0, 3);
            v8_put_bytes(s, buf, 3);
            span_log_buf(&s->logging, SPAN_LOG_FLOW, "<CJ: ", &buf[1], 2);
            s->state = V8_CJ_ON;
            s->fsk_tx_on = true;
            break;
        }
        if ((s->negotiation_timer -= len) <= 0)
        {
            /* Timeout */
            span_log(&s->logging, SPAN_LOG_FLOW, "Timeout waiting for JM\n");
            s->state = V8_PARKED;
            s->result.status = V8_STATUS_FAILED;
            report_event(s);
        }
        if (queue_contents(s->tx_queue) < 10)
        {
            /* Send CM again */
            send_cm_jm(s);
        }
        break;
    case V8_CJ_ON:
        residual_samples = fsk_rx(&s->v21rx, amp, len);
        if (!s->fsk_tx_on)
        {
#if 0
            s->negotiation_timer = ms_to_samples(75);
            s->state = V8_SIGC;
        }
        break;
    case V8_SIGC:
        if ((s->negotiation_timer -= len) <= 0)
        {
#endif
            /* The V.8 negotiation has succeeded. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Negotiation succeeded\n");
            s->state = V8_PARKED;
            s->result.status = V8_STATUS_V8_CALL;
            report_event(s);
        }
        break;
    case V8_CM_WAIT:
        residual_samples = fsk_rx(&s->v21rx, amp, len);
        if (s->got_cm_jm)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "CM recognised\n");

            s->result.status = V8_STATUS_V8_OFFERED;
            report_event(s);

            /* Stop sending ANSam or ANSam/ and send JM instead */
            fsk_tx_init(&s->v21tx, &preset_fsk_specs[FSK_V21CH2], get_bit, s);
            /* Set the timeout for JM */
            s->negotiation_timer = ms_to_samples(5000);
            s->state = V8_JM_ON;
            send_cm_jm(s);
            s->modem_connect_tone_tx_on = ms_to_samples(75);
            s->fsk_tx_on = true;
            break;
        }
        if ((s->negotiation_timer -= len) <= 0)
        {
            /* Timeout */
            span_log(&s->logging, SPAN_LOG_FLOW, "Timeout waiting for CM\n");
            s->state = V8_PARKED;
            s->result.status = V8_STATUS_FAILED;
            report_event(s);
        }
        break;
    case V8_JM_ON:
        residual_samples = fsk_rx(&s->v21rx, amp, len);
        if (s->got_cj)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "CJ recognised\n");
            /* Stop sending JM, flushing anything left in the buffer, and wait 75 ms */
            queue_flush(s->tx_queue);
            s->negotiation_timer = ms_to_samples(75);
            s->state = V8_SIGA;
            break;
        }
        if ((s->negotiation_timer -= len) <= 0)
        {
            /* Timeout */
            span_log(&s->logging, SPAN_LOG_FLOW, "Timeout waiting for CJ\n");
            s->state = V8_PARKED;
            s->result.status = V8_STATUS_FAILED;
            report_event(s);
            break;
        }
        if (queue_contents(s->tx_queue) < 10)
        {
            /* Send JM */
            send_cm_jm(s);
        }
        break;
    case V8_SIGA:
        if (!s->fsk_tx_on)
        //if ((s->negotiation_timer -= len) <= 0)
        {
            /* The V.8 negotiation has succeeded. */
            span_log(&s->logging, SPAN_LOG_FLOW, "Negotiation succeeded\n");
            s->state = V8_PARKED;
            s->result.status = V8_STATUS_V8_CALL;
            report_event(s);
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

SPAN_DECLARE(int) v8_restart(v8_state_t *s, bool calling_party, v8_parms_t *parms)
{
    memcpy(&s->parms, parms, sizeof(s->parms));
    memset(&s->result, 0, sizeof(s->result));

    s->result.status = V8_STATUS_IN_PROGRESS;
    s->result.modem_connect_tone = MODEM_CONNECT_TONES_NONE;
    s->result.modulations = s->parms.modulations;
    s->result.call_function = s->parms.call_function;
    s->result.nsf = -1;
    s->result.t66 = -1;

    s->modulation_bytes = 3;

    s->ci_timer = 0;
    s->calling_party = calling_party;
    if (s->calling_party)
    {
        if (s->result.send_ci)
        {
            s->state = V8_WAIT_1S;
            s->negotiation_timer = ms_to_samples(1000);
            s->ci_count = 0;
        }
        else
        {
            s->state = V8_AWAIT_ANSAM;
        }
        modem_connect_tones_rx_init(&s->ansam_rx, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);
        fsk_tx_init(&s->v21tx, &preset_fsk_specs[FSK_V21CH1], get_bit, s);
        s->modem_connect_tone_tx_on = ms_to_samples(75) + 2;
    }
    else
    {
        /* Send the ANSam or ANSam/ tone */
        s->state = V8_CM_WAIT;
        s->negotiation_timer = ms_to_samples(200 + 5000);
        v8_decode_init(s);
        modem_connect_tones_tx_init(&s->ansam_tx, s->parms.modem_connect_tone);
        s->modem_connect_tone_tx_on = ms_to_samples(75) + 1;
    }
    if (s->tx_queue)
        queue_free(s->tx_queue);
    if ((s->tx_queue = queue_init(NULL, 1024, 0)) == NULL)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v8_state_t *) v8_init(v8_state_t *s,
                                   bool calling_party,
                                   v8_parms_t *parms,
                                   v8_result_handler_t result_handler,
                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (v8_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.8");
    s->result_handler = result_handler;
    s->result_handler_user_data = user_data;

    v8_restart(s, calling_party, parms);
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

    ret = v8_release(s);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
