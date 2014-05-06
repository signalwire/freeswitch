/*
 * SpanDSP - a series of DSP components for telephony
 *
 * data_modems.c - the analogue modem set for data processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2005, 2006, 2008, 2011, 2013 Steve Underwood
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
#include <fcntl.h>
#include <time.h>
#if defined(LOG_FAX_AUDIO)
#include <unistd.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/dc_restore.h"
#include "spandsp/queue.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/silence_gen.h"
#include "spandsp/fsk.h"
#include "spandsp/v29rx.h"
#include "spandsp/v22bis.h"
#if defined(SPANDSP_SUPPORT_V32BIS)
#include "spandsp/v17tx.h"
#include "spandsp/v17rx.h"
#include "spandsp/modem_echo.h"
#include "spandsp/v32bis.h"
#endif
#if defined(SPANDSP_SUPPORT_V34)
#include "spandsp/bitstream.h"
#include "spandsp/v34.h"
#endif
#include "spandsp/super_tone_rx.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/hdlc.h"
#include "spandsp/v42.h"
#include "spandsp/v42bis.h"
#include "spandsp/v8.h"
#include "spandsp/data_modems.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/silence_gen.h"
#include "spandsp/private/power_meter.h"
#include "spandsp/private/fsk.h"
#include "spandsp/private/v22bis.h"
#if defined(SPANDSP_SUPPORT_V32BIS)
#include "spandsp/private/v17tx.h"
#include "spandsp/private/v17rx.h"
#include "spandsp/private/modem_echo.h"
#include "spandsp/private/v32bis.h"
#endif
#if defined(SPANDSP_SUPPORT_V34)
#include "spandsp/private/bitstream.h"
#include "spandsp/private/v34.h"
#endif
#include "spandsp/private/modem_connect_tones.h"
#include "spandsp/private/hdlc.h"
#include "spandsp/private/v42.h"
#include "spandsp/private/v42bis.h"
#include "spandsp/private/v8.h"
#include "spandsp/private/async.h"
#include "spandsp/private/data_modems.h"

SPAN_DECLARE(const char *) data_modems_modulation_to_str(int modulation_scheme)
{
    switch (modulation_scheme)
    {
    case DATA_MODEM_NONE:
        return "None";
    case DATA_MODEM_FLUSH:
        return "Flush";
    case DATA_MODEM_SILENCE:
        return "Silence";
    case DATA_MODEM_CED_TONE:
        return "CED";
    case DATA_MODEM_CNG_TONE:
        return "CNG";
    case DATA_MODEM_V8:
        return "V.8";
    case DATA_MODEM_BELL103:
        return "B103 duplex";
    case DATA_MODEM_BELL202:
        return "B202 duplex";
    case DATA_MODEM_V21:
        return "V.21 duplex";
    case DATA_MODEM_V23:
        return "V.23 duplex";
    case DATA_MODEM_V22BIS:
        return "V.22/V.22bis duplex";
    case DATA_MODEM_V32BIS:
        return "V.32/V.32bis duplex";
    case DATA_MODEM_V34:
        return "V.34 duplex";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) data_modems_get_logging_state(data_modems_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

static int async_get_byte(void *user_data)
{
    data_modems_state_t *s;
    uint8_t msg[1];

    s = (data_modems_state_t *) user_data;
    s->get_msg(s->user_data, msg, 1);
    return msg[0];
}
/*- End of function --------------------------------------------------------*/

static void async_put_byte(void *user_data, int byte)
{
    data_modems_state_t *s;
    uint8_t msg[1];

    s = (data_modems_state_t *) user_data;
    msg[0] = byte;
    if (byte < 0)
        s->put_msg(s->user_data,  msg, byte);
    s->put_msg(s->user_data,  msg, 1);
}
/*- End of function --------------------------------------------------------*/

static void tone_callback(void *user_data, int tone, int level, int delay)
{
    printf("%s declared (%ddBm0)\n", modem_connect_tone_to_str(tone), level);
}
/*- End of function --------------------------------------------------------*/

static void log_supported_modulations(data_modems_state_t *s, int modulation_schemes)
{
    const char *comma;
    int i;

    comma = "";
    span_log(&s->logging, SPAN_LOG_FLOW, "    ");
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

static void v8_handler(void *user_data, v8_parms_t *result)
{
    data_modems_state_t *s;

    s = (data_modems_state_t *) user_data;
    switch (result->status)
    {
    case V8_STATUS_IN_PROGRESS:
        span_log(&s->logging, SPAN_LOG_FLOW, "V.8 negotiation in progress\n");
        return;
    case V8_STATUS_V8_OFFERED:
        span_log(&s->logging, SPAN_LOG_FLOW, "V.8 offered by the other party\n");
        break;
    case V8_STATUS_V8_CALL:
        span_log(&s->logging, SPAN_LOG_FLOW, "V.8 call negotiation successful\n");
        break;
    case V8_STATUS_NON_V8_CALL:
        span_log(&s->logging, SPAN_LOG_FLOW, "Non-V.8 call negotiation successful\n");
        break;
    case V8_STATUS_FAILED:
        span_log(&s->logging, SPAN_LOG_FLOW, "V.8 call negotiation failed\n");
        return;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "Unexpected V.8 status %d\n", result->status);
        break;
    }
    /*endswitch*/

    span_log(&s->logging, SPAN_LOG_FLOW, "  Modem connect tone '%s' (%d)\n", modem_connect_tone_to_str(result->modem_connect_tone), result->modem_connect_tone);
    span_log(&s->logging, SPAN_LOG_FLOW, "  Call function '%s' (%d)\n", v8_call_function_to_str(result->call_function), result->call_function);
    span_log(&s->logging, SPAN_LOG_FLOW, "  Far end modulations 0x%X\n", result->modulations);
    log_supported_modulations(s, result->modulations);
    span_log(&s->logging, SPAN_LOG_FLOW, "  Protocol '%s' (%d)\n", v8_protocol_to_str(result->protocol), result->protocol);
    span_log(&s->logging, SPAN_LOG_FLOW, "  PSTN access '%s' (%d)\n", v8_pstn_access_to_str(result->pstn_access), result->pstn_access);
    span_log(&s->logging, SPAN_LOG_FLOW, "  PCM modem availability '%s' (%d)\n", v8_pcm_modem_availability_to_str(result->pcm_modem_availability), result->pcm_modem_availability);
    if (result->t66 >= 0)
        span_log(&s->logging, SPAN_LOG_FLOW, "  T.66 '%s' (%d)\n", v8_t66_to_str(result->t66), result->t66);
    /*endif*/
    if (result->nsf >= 0)
        span_log(&s->logging, SPAN_LOG_FLOW, "  NSF %d\n", result->nsf);
    /*endif*/

    switch (result->status)
    {
    case V8_STATUS_V8_OFFERED:
        /* V.8 mode has been offered. */
        span_log(&s->logging, SPAN_LOG_FLOW, "  Offered\n");
        /* We now need to edit the offered list of usable modem modulations to reflect
           the set of modulations both ends share */
        //result->call_function = V8_CALL_T30_TX;
        result->modulations &= (V8_MOD_V21
                              | V8_MOD_V22
                              | V8_MOD_V23HDX
                              | V8_MOD_V23
#if defined(SPANDSP_SUPPORT_V32BIS)
                              | V8_MOD_V32
#endif
#if defined(SPANDSP_SUPPORT_V34)
                              | V8_MOD_V34
#endif
                              | 0);
        span_log(&s->logging, SPAN_LOG_FLOW, "  Mutual modulations 0x%X\n", result->modulations);
        log_supported_modulations(s, result->modulations);
        break;
    case V8_STATUS_V8_CALL:
        span_log(&s->logging, SPAN_LOG_FLOW, "  Call\n");
        if (result->call_function == V8_CALL_V_SERIES)
        {
            /* Negotiations OK */
            if (result->protocol == V8_PROTOCOL_LAPM_V42)
            {
            }
            /*endif*/

#if defined(SPANDSP_SUPPORT_V34)
            if ((result->modulations & V8_MOD_V34))
            {
                s->queued_baud_rate = 2400;
                s->queued_bit_rate = 28800;
                s->queued_modem = DATA_MODEM_V34;
            }
            else
#endif
#if defined(SPANDSP_SUPPORT_V32BIS)
            if ((result->modulations & V8_MOD_V32))
            {
                s->queued_baud_rate = 2400;
                s->queued_bit_rate = 14400;
                s->queued_modem = DATA_MODEM_V32BIS;
            }
            else
#endif
            if ((result->modulations & V8_MOD_V22))
            {
                s->queued_baud_rate = 600;
                s->queued_bit_rate = 2400;
                s->queued_modem = DATA_MODEM_V22BIS;
            }
            else if ((result->modulations & V8_MOD_V21))
            {
                s->queued_baud_rate = 300;
                s->queued_bit_rate = 300;
                s->queued_modem = DATA_MODEM_V21;
            }
            else
            {
                s->queued_modem = DATA_MODEM_NONE;
            }
            /*endif*/
            span_log(&s->logging, SPAN_LOG_FLOW, "  Negotiated modulation '%s' %d\n", data_modems_modulation_to_str(s->queued_modem), s->queued_modem);
        }
        /*endif*/
        break;
    case V8_STATUS_NON_V8_CALL:
        span_log(&s->logging, SPAN_LOG_FLOW, "  Non-V.8 call\n");
        s->queued_modem = DATA_MODEM_V22BIS;
        break;
    default:
        span_log(&s->logging, SPAN_LOG_FLOW, "  Huh? %d\n", result->status);
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) data_modems_set_async_mode(data_modems_state_t *s,
                                              int data_bits,
                                              int parity_bit,
                                              int stop_bits)
{
    async_tx_init(&s->async_tx,
                  data_bits,
                  parity_bit,
                  stop_bits,
                  s->use_v14,
                  &async_get_byte,
                  s);
    async_rx_init(&s->async_rx,
                  data_bits,
                  parity_bit,
                  stop_bits,
                  s->use_v14,
                  &async_put_byte,
                  s);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) data_modems_set_modem_type(data_modems_state_t *s, int which, int baud_rate, int bit_rate)
{
    const fsk_spec_t *fsk_rx_spec;
    const fsk_spec_t *fsk_tx_spec;
    v8_parms_t v8_parms;
    logging_state_t *logging;
    int level;

    switch (which)
    {
    case DATA_MODEM_SILENCE:
        s->rx_handler = (span_rx_handler_t) &span_dummy_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &span_dummy_rx_fillin;
        s->rx_user_data = NULL;
        s->tx_handler = (span_tx_handler_t) &silence_gen;
        s->tx_user_data = &s->modems.silence_gen;
        silence_gen_init(&s->modems.silence_gen, 0);
        break;
    case DATA_MODEM_CNG_TONE:
        s->rx_handler = (span_rx_handler_t) &modem_connect_tones_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &span_dummy_rx_fillin;
        s->rx_user_data = &s->modems.tones.rx;
        s->tx_handler = (span_tx_handler_t) &modem_connect_tones_tx;
        s->tx_user_data = &s->modems.tones.tx;
        modem_connect_tones_rx_init(&s->modems.tones.rx,
                                    MODEM_CONNECT_TONES_FAX_CNG,
                                    tone_callback,
                                    s);
        modem_connect_tones_tx_init(&s->modems.tones.tx, MODEM_CONNECT_TONES_FAX_CNG);
        break;
    case DATA_MODEM_V8:
        s->rx_handler = (span_rx_handler_t) &v8_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &span_dummy_rx_fillin;
        s->rx_user_data = &s->modems.v8;
        s->tx_handler = (span_tx_handler_t) &v8_tx;
        s->tx_user_data = &s->modems.v8;
        if (s->calling_party)
            v8_parms.modem_connect_tone = MODEM_CONNECT_TONES_NONE;
        else
            v8_parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
        v8_parms.send_ci = false;
        v8_parms.v92 = -1;
        v8_parms.call_function = V8_CALL_V_SERIES;
#if 0
        v8_parms.modulations = V8_MOD_V17
                             | V8_MOD_V21
                             | V8_MOD_V22
                             | V8_MOD_V23HDX
                             | V8_MOD_V23
                             | V8_MOD_V27TER
                             | V8_MOD_V29
                             | 0;
        v8_parms.protocol = V8_PROTOCOL_LAPM_V42;
#elif 1
        v8_parms.modulations = V8_MOD_V21
                             | V8_MOD_V22
                             | V8_MOD_V23HDX
                             | V8_MOD_V23
#if defined(SPANDSP_SUPPORT_V32BIS)
                             | V8_MOD_V32
#endif
#if defined(SPANDSP_SUPPORT_V34)
                             | V8_MOD_V34
#endif
                             | 0;
        v8_parms.protocol = V8_PROTOCOL_LAPM_V42;
#endif
        v8_parms.pcm_modem_availability = 0;
        v8_parms.pstn_access = 0;
        v8_parms.nsf = -1;
        v8_parms.t66 = -1;
        v8_init(&s->modems.v8, s->calling_party, &v8_parms, v8_handler, (void *) s);
    logging = v8_get_logging_state(&s->modems.v8);
    level = span_log_get_level(&s->logging);
    span_log_set_level(logging, level);
    span_log_set_tag(logging, "V.8");
        break;
    case DATA_MODEM_BELL103:
        s->rx_handler = (span_rx_handler_t) &fsk_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &fsk_rx_fillin;
        s->rx_user_data = &s->modems.fsk.rx;
        s->tx_handler = (span_tx_handler_t) &fsk_tx;
        s->tx_user_data = &s->modems.fsk.tx;
        if (s->calling_party)
        {
            fsk_rx_spec = &preset_fsk_specs[FSK_BELL103CH2];
            fsk_tx_spec = &preset_fsk_specs[FSK_BELL103CH1];
        }
        else
        {
            fsk_rx_spec = &preset_fsk_specs[FSK_BELL103CH1];
            fsk_tx_spec = &preset_fsk_specs[FSK_BELL103CH2];
        }
        fsk_rx_init(&s->modems.fsk.rx, fsk_rx_spec, FSK_FRAME_MODE_SYNC, s->put_bit, s->put_user_data);
        fsk_tx_init(&s->modems.fsk.tx, fsk_tx_spec, s->get_bit, s->get_user_data);
        break;
    case DATA_MODEM_V21:
        s->rx_handler = (span_rx_handler_t) &fsk_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &fsk_rx_fillin;
        s->rx_user_data = &s->modems.fsk.rx;
        s->tx_handler = (span_tx_handler_t) &fsk_tx;
        s->tx_user_data = &s->modems.fsk.tx;
        if (s->calling_party)
        {
            fsk_rx_spec = &preset_fsk_specs[FSK_V21CH2];
            fsk_tx_spec = &preset_fsk_specs[FSK_V21CH1];
        }
        else
        {
            fsk_rx_spec = &preset_fsk_specs[FSK_V21CH1];
            fsk_tx_spec = &preset_fsk_specs[FSK_V21CH2];
        }
        fsk_rx_init(&s->modems.fsk.rx, fsk_rx_spec, FSK_FRAME_MODE_SYNC, s->put_bit, s->put_user_data);
        fsk_tx_init(&s->modems.fsk.tx, fsk_tx_spec, s->get_bit, s->get_user_data);
        break;
    case DATA_MODEM_BELL202:
        s->rx_handler = (span_rx_handler_t) &fsk_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &fsk_rx_fillin;
        s->rx_user_data = &s->modems.fsk.rx;
        s->tx_handler = (span_tx_handler_t) &fsk_tx;
        s->tx_user_data = &s->modems.fsk.tx;
        if (s->calling_party)
        {
            fsk_rx_spec = &preset_fsk_specs[FSK_BELL202];
            fsk_tx_spec = &preset_fsk_specs[FSK_BELL202];
        }
        else
        {
            fsk_rx_spec = &preset_fsk_specs[FSK_BELL202];
            fsk_tx_spec = &preset_fsk_specs[FSK_BELL202];
        }
        fsk_rx_init(&s->modems.fsk.rx, fsk_rx_spec, FSK_FRAME_MODE_SYNC, s->put_bit, s->put_user_data);
        fsk_tx_init(&s->modems.fsk.tx, fsk_tx_spec, s->get_bit, s->get_user_data);
        break;
    case DATA_MODEM_V23:
        s->rx_handler = (span_rx_handler_t) &fsk_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &fsk_rx_fillin;
        s->rx_user_data = &s->modems.fsk.rx;
        s->tx_handler = (span_tx_handler_t) &fsk_tx;
        s->tx_user_data = &s->modems.fsk.tx;
        if (s->calling_party)
        {
            fsk_rx_spec = &preset_fsk_specs[FSK_V23CH2];
            fsk_tx_spec = &preset_fsk_specs[FSK_V23CH1];
        }
        else
        {
            fsk_rx_spec = &preset_fsk_specs[FSK_V23CH1];
            fsk_tx_spec = &preset_fsk_specs[FSK_V23CH2];
        }
        fsk_rx_init(&s->modems.fsk.rx, fsk_rx_spec, FSK_FRAME_MODE_SYNC, s->put_bit, s->put_user_data);
        fsk_tx_init(&s->modems.fsk.tx, fsk_tx_spec, s->get_bit, s->get_user_data);
        break;
    case DATA_MODEM_V22BIS:
        s->rx_handler = (span_rx_handler_t) &v22bis_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &v22bis_rx_fillin;
        s->rx_user_data = &s->modems.v22bis;
        s->tx_handler = (span_tx_handler_t) &v22bis_tx;
        s->tx_user_data = &s->modems.v22bis;
        v22bis_init(&s->modems.v22bis, bit_rate, 0, s->calling_party, s->get_bit, s->get_user_data, s->put_bit, s->put_user_data);
    logging = v22bis_get_logging_state(&s->modems.v22bis);
    level = span_log_get_level(&s->logging);
    span_log_set_level(logging, level);
    span_log_set_tag(logging, "V.22bis");
        break;
#if defined(SPANDSP_SUPPORT_V32BIS)
    case DATA_MODEM_V32BIS:
        s->rx_handler = (span_rx_handler_t) &v32bis_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &v32bis_rx_fillin;
        s->rx_user_data = &s->modems.v32bis;
        s->tx_handler = (span_tx_handler_t) &v32bis_tx;
        s->tx_user_data = &s->modems.v32bis;
        v32bis_init(&s->modems.v32bis, bit_rate, s->calling_party, s->get_bit, s->get_user_data, s->put_bit, s->put_user_data);
    logging = v32bis_get_logging_state(&s->modems.v32bis);
    level = span_log_get_level(&s->logging);
    span_log_set_level(logging, level);
    span_log_set_tag(logging, "V.32bis");
        break;
#endif
#if defined(SPANDSP_SUPPORT_V34)
    case DATA_MODEM_V34:
        s->rx_handler = (span_rx_handler_t) &v34_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t) &v34_rx_fillin;
        s->rx_user_data = &s->modems.v34;
        s->tx_handler = (span_tx_handler_t) &v34_tx;
        s->tx_user_data = &s->modems.v34;
        v34_init(&s->modems.v34, baud_rate, bit_rate, s->calling_party, true, s->get_bit, s->get_user_data, s->put_bit, s->put_user_data);
    logging = v34_get_logging_state(&s->modems.v34);
    level = span_log_get_level(&s->logging);
    span_log_set_level(logging, level);
    span_log_set_tag(logging, "V.34");
        break;
#endif
    }
    s->current_modem = which;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) data_modems_rx(data_modems_state_t *s, const int16_t amp[], int len)
{
    int res;

    if (s->rx_handler == NULL)
        return len;
    res = s->rx_handler(s->rx_user_data, amp, len);
    if (s->current_modem != s->queued_modem)
        data_modems_set_modem_type(s, s->queued_modem, s->queued_baud_rate, s->queued_bit_rate);
    return res;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) data_modems_rx_fillin(data_modems_state_t *s, int len)
{
    if (s->rx_fillin_handler == NULL)
        return len;
    return s->rx_fillin_handler(s->rx_user_data, len);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) data_modems_tx(data_modems_state_t *s, int16_t amp[], int max_len)
{
    int len;

    for (len = 0;  len < max_len;  )
    {
        if (s->tx_handler == NULL)
            break;
        len += s->tx_handler(s->tx_user_data, &amp[len], max_len - len);
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) data_modems_restart(data_modems_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(data_modems_state_t *) data_modems_init(data_modems_state_t *s,
                                                     bool calling_party,
                                                     put_msg_func_t put_msg,
                                                     get_msg_func_t get_msg,
                                                     void *user_data)
{
    if (s == NULL)
    {
        if ((s = (data_modems_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "Modem");

    dc_restore_init(&s->dc_restore);

    s->put_msg = put_msg;
    s->get_msg = get_msg;
    s->user_data = user_data;

    v42bis_init(&s->v42bis, 3, 512, 6, NULL, s, 512, put_msg, s, 512);
    v42_init(&s->v42, true, true, NULL, (put_msg_func_t) v42bis_decompress, &s->v42bis);

    data_modems_set_async_mode(s, 8, 1, 1);

    s->get_bit = async_tx_get_bit;
    s->get_user_data = &s->async_tx;
    s->put_bit = async_rx_put_bit;
    s->put_user_data = &s->async_rx;

    s->calling_party = calling_party;

    data_modems_set_modem_type(s, DATA_MODEM_V8, 0, 0);
    s->queued_modem = s->current_modem;

    s->rx_signal_present = false;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) data_modems_release(data_modems_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) data_modems_free(data_modems_state_t *s)
{
    if (s)
        span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
