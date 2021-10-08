/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_modems.c - the analogue modem set for fax processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2005, 2006, 2008, 2013 Steve Underwood
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
#include "spandsp/fsk.h"
#include "spandsp/v29tx.h"
#include "spandsp/v29rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v17tx.h"
#include "spandsp/v17rx.h"
#if defined(SPANDSP_SUPPORT_V34)
#include "spandsp/v34.h"
#endif
#include "spandsp/super_tone_rx.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/fax_modems.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/bitstream.h"
#include "spandsp/private/silence_gen.h"
#include "spandsp/private/power_meter.h"
#include "spandsp/private/fsk.h"
#if defined(SPANDSP_SUPPORT_V34)
#include "spandsp/private/v34.h"
#endif
#include "spandsp/private/v17tx.h"
#include "spandsp/private/v17rx.h"
#include "spandsp/private/v27ter_tx.h"
#include "spandsp/private/v27ter_rx.h"
#include "spandsp/private/v29tx.h"
#include "spandsp/private/v29rx.h"
#include "spandsp/private/modem_connect_tones.h"
#include "spandsp/private/hdlc.h"
#include "spandsp/private/fax_modems.h"

#define HDLC_FRAMING_OK_THRESHOLD               5

SPAN_DECLARE(const char *) fax_modem_to_str(int modem)
{
    switch (modem)
    {
    case FAX_MODEM_NONE:
        return "None";
    case FAX_MODEM_FLUSH:
        return "Flush";
    case FAX_MODEM_SILENCE_TX:
        return "Silence Tx";
    case FAX_MODEM_SILENCE_RX:
        return "Silence Rx";
    case FAX_MODEM_CED_TONE_TX:
        return "CED Tx";
    case FAX_MODEM_CNG_TONE_TX:
        return "CNG Tx";
    case FAX_MODEM_NOCNG_TONE_TX:
        return "No CNG Tx";
    case FAX_MODEM_CED_TONE_RX:
        return "CED Rx";
    case FAX_MODEM_CNG_TONE_RX:
        return "CNG Rx";
    case FAX_MODEM_V21_TX:
        return "V.21 Tx";
    case FAX_MODEM_V17_TX:
        return "V.17 Tx";
    case FAX_MODEM_V27TER_TX:
        return "V.27ter Tx";
    case FAX_MODEM_V29_TX:
        return "V.29 Tx";
    case FAX_MODEM_V21_RX:
        return "V.21 Rx";
    case FAX_MODEM_V17_RX:
        return "V.17 Rx";
    case FAX_MODEM_V27TER_RX:
        return "V.27ter Rx";
    case FAX_MODEM_V29_RX:
        return "V.29 Rx";
#if defined(SPANDSP_SUPPORT_V34)
    case FAX_MODEM_V34_TX:
        return "V.34 HDX Tx";
    case FAX_MODEM_V34_RX:
        return "V.34 HDX Rx";
#endif
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

//static void fax_modems_hdlc_accept(void *user_data, const uint8_t *msg, int len, int ok)
SPAN_DECLARE(void) fax_modems_hdlc_accept(void *user_data, const uint8_t *msg, int len, int ok)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    /* If this is a good frame report - i.e. not a status report, or a bad frame - we can
       say the current signal source is valid. */
    if (len >= 0  &&  ok)
        s->rx_frame_received = true;
    /*endif*/
    if (s->hdlc_accept)
        s->hdlc_accept(s->hdlc_accept_user_data, msg, len, ok);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_hdlc_tx_frame(void *user_data, const uint8_t *msg, int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;

    if (len == -1)
        hdlc_tx_restart(&s->hdlc_tx);
    else
        hdlc_tx_frame(&s->hdlc_tx, msg, len);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_hdlc_tx_flags(fax_modems_state_t *s, int flags)
{
    hdlc_tx_flags(&s->hdlc_tx, flags);
}
/*- End of function --------------------------------------------------------*/

static void v17_rx_status_handler(void *user_data, int status)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    switch (status)
    {
    case SIG_STATUS_TRAINING_SUCCEEDED:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.17 (%.2fdBm0)\n", v17_rx_signal_power(&s->fast_modems.v17_rx));
        fax_modems_set_rx_handler(s, (span_rx_handler_t) &v17_rx, &s->fast_modems.v17_rx, (span_rx_fillin_handler_t) &v17_rx_fillin, &s->fast_modems.v17_rx);
        v17_rx_set_modem_status_handler(&s->fast_modems.v17_rx, NULL, s);
        break;
    }
    /*endswitch*/
    s->fast_modems.v17_rx.put_bit(s->fast_modems.v17_rx.put_bit_user_data, status);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v17_v21_rx(void *user_data, const int16_t amp[], int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v17_rx(&s->fast_modems.v17_rx, amp, len);
    fsk_rx(&s->v21_rx, amp, len);
    if (s->rx_frame_received)
    {
        /* We have received something, and the fast modem has not trained. We must be receiving valid V.21 */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&s->v21_rx));
        fax_modems_set_rx_handler(s, (span_rx_handler_t) &fsk_rx, &s->v21_rx, (span_rx_fillin_handler_t) &fsk_rx_fillin, &s->v21_rx);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v17_v21_rx_fillin(void *user_data, int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v17_rx_fillin(&s->fast_modems.v17_rx, len);
    fsk_rx_fillin(&s->v21_rx, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void v27ter_rx_status_handler(void *user_data, int status)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    switch (status)
    {
    case SIG_STATUS_TRAINING_SUCCEEDED:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.27ter (%.2fdBm0)\n", v27ter_rx_signal_power(&s->fast_modems.v27ter_rx));
        fax_modems_set_rx_handler(s, (span_rx_handler_t) &v27ter_rx, &s->fast_modems.v27ter_rx, (span_rx_fillin_handler_t) &v27ter_rx_fillin, &s->fast_modems.v27ter_rx);
        v27ter_rx_set_modem_status_handler(&s->fast_modems.v27ter_rx, NULL, s);
        break;
    }
    /*endswitch*/
    s->fast_modems.v27ter_rx.put_bit(s->fast_modems.v27ter_rx.put_bit_user_data, status);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v27ter_v21_rx(void *user_data, const int16_t amp[], int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v27ter_rx(&s->fast_modems.v27ter_rx, amp, len);
    fsk_rx(&s->v21_rx, amp, len);
    if (s->rx_frame_received)
    {
        /* We have received something, and the fast modem has not trained. We must be receiving valid V.21 */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&s->v21_rx));
        fax_modems_set_rx_handler(s, (span_rx_handler_t) &fsk_rx, &s->v21_rx, (span_rx_fillin_handler_t) &fsk_rx_fillin, &s->v21_rx);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v27ter_v21_rx_fillin(void *user_data, int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v27ter_rx_fillin(&s->fast_modems.v27ter_rx, len);
    fsk_rx_fillin(&s->v21_rx, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void v29_rx_status_handler(void *user_data, int status)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    switch (status)
    {
    case SIG_STATUS_TRAINING_SUCCEEDED:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.29 (%.2fdBm0)\n", v29_rx_signal_power(&s->fast_modems.v29_rx));
        fax_modems_set_rx_handler(s, (span_rx_handler_t) &v29_rx, &s->fast_modems.v29_rx, (span_rx_fillin_handler_t) &v29_rx_fillin, &s->fast_modems.v29_rx);
        v29_rx_set_modem_status_handler(&s->fast_modems.v29_rx, NULL, s);
        break;
    }
    /*endswitch*/
    s->fast_modems.v29_rx.put_bit(s->fast_modems.v29_rx.put_bit_user_data, status);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v29_v21_rx(void *user_data, const int16_t amp[], int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v29_rx(&s->fast_modems.v29_rx, amp, len);
    fsk_rx(&s->v21_rx, amp, len);
    if (s->rx_frame_received)
    {
        /* We have received something, and the fast modem has not trained. We must be receiving valid V.21 */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&s->v21_rx));
        fax_modems_set_rx_handler(s, (span_rx_handler_t) &fsk_rx, &s->v21_rx, (span_rx_fillin_handler_t) &fsk_rx_fillin, &s->v21_rx);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v29_v21_rx_fillin(void *user_data, int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v29_rx_fillin(&s->fast_modems.v29_rx, len);
    fsk_rx_fillin(&s->v21_rx, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_start_slow_modem(fax_modems_state_t *s, int which)
{
    switch (which)
    {
    case FAX_MODEM_V21_RX:
        fsk_rx_init(&s->v21_rx, &preset_fsk_specs[FSK_V21CH2], FSK_FRAME_MODE_SYNC, (put_bit_func_t) hdlc_rx_put_bit, &s->hdlc_rx);
        fax_modems_set_rx_handler(s, (span_rx_handler_t) &fsk_rx, &s->v21_rx, (span_rx_fillin_handler_t) &fsk_rx_fillin, &s->v21_rx);
        fsk_rx_signal_cutoff(&s->v21_rx, -39.09f);
        //hdlc_rx_init(&s->hdlc_rx, false, true, HDLC_FRAMING_OK_THRESHOLD, fax_modems_hdlc_accept, s);
        break;
    case FAX_MODEM_CED_TONE_RX:
        modem_connect_tones_rx_init(&s->connect_rx, MODEM_CONNECT_TONES_FAX_CED, s->tone_callback, s->tone_callback_user_data);
        fax_modems_set_rx_handler(s, (span_rx_handler_t) &modem_connect_tones_rx, &s->connect_rx, (span_rx_fillin_handler_t) &modem_connect_tones_rx_fillin, &s->connect_rx);
        break;
    case FAX_MODEM_CNG_TONE_RX:
        modem_connect_tones_rx_init(&s->connect_rx, MODEM_CONNECT_TONES_FAX_CNG, s->tone_callback, s->tone_callback_user_data);
        fax_modems_set_rx_handler(s, (span_rx_handler_t) &modem_connect_tones_rx, &s->connect_rx, (span_rx_fillin_handler_t) &modem_connect_tones_rx_fillin, &s->connect_rx);
        break;
    case FAX_MODEM_V21_TX:
        fsk_tx_init(&s->v21_tx, &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &s->hdlc_tx);
        fax_modems_set_tx_handler(s, (span_tx_handler_t) &fsk_tx, &s->v21_tx);
        fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
        break;
    case FAX_MODEM_CED_TONE_TX:
        modem_connect_tones_tx_init(&s->connect_tx, MODEM_CONNECT_TONES_FAX_CED);
        fax_modems_set_tx_handler(s, (span_tx_handler_t) &modem_connect_tones_tx, &s->connect_tx);
        fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
        break;
    case FAX_MODEM_CNG_TONE_TX:
        modem_connect_tones_tx_init(&s->connect_tx, MODEM_CONNECT_TONES_FAX_CNG);
        fax_modems_set_tx_handler(s, (span_tx_handler_t) &modem_connect_tones_tx, &s->connect_tx);
        fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
        break;
    }
    /*endswitch*/
    s->rx_frame_received = false;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_start_fast_modem(fax_modems_state_t *s, int which, int bit_rate, int short_train, int hdlc_mode)
{
    put_bit_func_t put_bit;
    get_bit_func_t get_bit;
    void *get_bit_user_data;
    void *put_bit_user_data;

    s->bit_rate = bit_rate;
    if (hdlc_mode)
    {
        get_bit = (get_bit_func_t) hdlc_tx_get_bit;
        get_bit_user_data = (void *) &s->hdlc_tx;
        put_bit = (put_bit_func_t) hdlc_rx_put_bit;
        put_bit_user_data = (void *) &s->hdlc_rx;
        //hdlc_rx_init(&s->hdlc_rx, false, true, HDLC_FRAMING_OK_THRESHOLD, fax_modems_hdlc_accept, s);
    }
    else
    {
        get_bit = s->get_bit;
        get_bit_user_data = s->get_bit_user_data;
        put_bit = s->put_bit;
        put_bit_user_data = s->put_bit_user_data;
    }
    /*endif*/

    /* If we change modems we need to do a complete reinitialisation of the modem, because
       the modems use overlapping memory. */
    if (s->fast_modem != which)
    {
        s->current_rx_type = which;
        s->short_train = false;
        s->fast_modem = which;
        switch (s->fast_modem)
        {
        case FAX_MODEM_V27TER_RX:
            v27ter_rx_init(&s->fast_modems.v27ter_rx, s->bit_rate, put_bit, put_bit_user_data);
            v27ter_rx_set_modem_status_handler(&s->fast_modems.v27ter_rx, v27ter_rx_status_handler, s);
            fax_modems_set_rx_handler(s, (span_rx_handler_t) &fax_modems_v27ter_v21_rx, s, (span_rx_fillin_handler_t) &fax_modems_v27ter_v21_rx_fillin, s);
            break;
        case FAX_MODEM_V29_RX:
            v29_rx_init(&s->fast_modems.v29_rx, s->bit_rate, put_bit, put_bit_user_data);
            v29_rx_signal_cutoff(&s->fast_modems.v29_rx, -45.5f);
            v29_rx_set_modem_status_handler(&s->fast_modems.v29_rx, v29_rx_status_handler, s);
            fax_modems_set_rx_handler(s, (span_rx_handler_t) &fax_modems_v29_v21_rx, s, (span_rx_fillin_handler_t) &fax_modems_v29_v21_rx_fillin, s);
            break;
        case FAX_MODEM_V17_RX:
            v17_rx_init(&s->fast_modems.v17_rx, s->bit_rate, put_bit, put_bit_user_data);
            v17_rx_set_modem_status_handler(&s->fast_modems.v17_rx, v17_rx_status_handler, s);
            fax_modems_set_rx_handler(s, (span_rx_handler_t) &fax_modems_v17_v21_rx, s, (span_rx_fillin_handler_t) &fax_modems_v17_v21_rx_fillin, s);
            break;
        case FAX_MODEM_V27TER_TX:
            v27ter_tx_init(&s->fast_modems.v27ter_tx, s->bit_rate, s->use_tep, get_bit, get_bit_user_data);
            fax_modems_set_tx_handler(s, (span_tx_handler_t) &v27ter_tx, &s->fast_modems.v27ter_tx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
        case FAX_MODEM_V29_TX:
            v29_tx_init(&s->fast_modems.v29_tx, s->bit_rate, s->use_tep, get_bit, get_bit_user_data);
            fax_modems_set_tx_handler(s, (span_tx_handler_t) &v29_tx, &s->fast_modems.v29_tx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
        case FAX_MODEM_V17_TX:
            v17_tx_init(&s->fast_modems.v17_tx, s->bit_rate, s->use_tep, get_bit, get_bit_user_data);
            fax_modems_set_tx_handler(s, (span_tx_handler_t) &v17_tx, &s->fast_modems.v17_tx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
#if defined(SPANDSP_SUPPORT_V34)
        case FAX_MODEM_V34_RX:
            v34_init(&s->fast_modems.v34, 2400, s->bit_rate, true, false, NULL, NULL, put_bit, put_bit_user_data);
            //fax_modems_set_tx_handler(s, (span_tx_handler_t) &v34_rx, &s->fast_modems.v34_rx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
        case FAX_MODEM_V34_TX:
            v34_init(&s->fast_modems.v34, 2400, s->bit_rate, true, false, get_bit, get_bit_user_data, NULL, NULL);
            //fax_modems_set_tx_handler(s, (span_tx_handler_t) &v34_tx, &s->fast_modems.v34_tx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
#endif
        }
        /*endswitch*/
    }
    else
    {
        s->short_train = short_train;
        switch (s->fast_modem)
        {
        case FAX_MODEM_V27TER_RX:
            v27ter_rx_restart(&s->fast_modems.v27ter_rx, s->bit_rate, false);
            v27ter_rx_set_put_bit(&s->fast_modems.v27ter_rx, put_bit, put_bit_user_data);
            v27ter_rx_set_modem_status_handler(&s->fast_modems.v27ter_rx, v27ter_rx_status_handler, s);
            fax_modems_set_rx_handler(s, (span_rx_handler_t) &fax_modems_v27ter_v21_rx, s, (span_rx_fillin_handler_t) &fax_modems_v27ter_v21_rx_fillin, s);
            break;
        case FAX_MODEM_V29_RX:
            v29_rx_restart(&s->fast_modems.v29_rx, s->bit_rate, false);
            v29_rx_set_put_bit(&s->fast_modems.v29_rx, put_bit, put_bit_user_data);
            v29_rx_set_modem_status_handler(&s->fast_modems.v29_rx, v29_rx_status_handler, s);
            fax_modems_set_rx_handler(s, (span_rx_handler_t) &fax_modems_v29_v21_rx, s, (span_rx_fillin_handler_t) &fax_modems_v29_v21_rx_fillin, s);
            break;
        case FAX_MODEM_V17_RX:
            v17_rx_restart(&s->fast_modems.v17_rx, s->bit_rate, s->short_train);
            v17_rx_set_put_bit(&s->fast_modems.v17_rx, put_bit, put_bit_user_data);
            v17_rx_set_modem_status_handler(&s->fast_modems.v17_rx, v17_rx_status_handler, s);
            fax_modems_set_rx_handler(s, (span_rx_handler_t) &fax_modems_v17_v21_rx, s, (span_rx_fillin_handler_t) &fax_modems_v17_v21_rx_fillin, s);
            break;
        case FAX_MODEM_V27TER_TX:
            v27ter_tx_restart(&s->fast_modems.v27ter_tx, s->bit_rate, s->use_tep);
            v27ter_tx_set_get_bit(&s->fast_modems.v27ter_tx, get_bit, get_bit_user_data);
            fax_modems_set_tx_handler(s, (span_tx_handler_t) &v27ter_tx, &s->fast_modems.v27ter_tx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
        case FAX_MODEM_V29_TX:
            v29_tx_restart(&s->fast_modems.v29_tx, s->bit_rate, s->use_tep);
            v29_tx_set_get_bit(&s->fast_modems.v29_tx, get_bit, get_bit_user_data);
            fax_modems_set_tx_handler(s, (span_tx_handler_t) &v29_tx, &s->fast_modems.v29_tx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
        case FAX_MODEM_V17_TX:
            v17_tx_restart(&s->fast_modems.v17_tx, s->bit_rate, s->use_tep, s->short_train);
            v17_tx_set_get_bit(&s->fast_modems.v17_tx, get_bit, get_bit_user_data);
            fax_modems_set_tx_handler(s, (span_tx_handler_t) &v17_tx, &s->fast_modems.v17_tx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
#if defined(SPANDSP_SUPPORT_V34)
        case FAX_MODEM_V34_RX:
            v34_restart(&s->fast_modems.v34, 2400, s->bit_rate, false);
            //fax_modems_set_tx_handler(s, (span_tx_handler_t) &v34_rx, &s->fast_modems.v34_rx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
        case FAX_MODEM_V34_TX:
            v34_restart(&s->fast_modems.v34, 2400, s->bit_rate, false);
            //fax_modems_set_tx_handler(s, (span_tx_handler_t) &v34_tx, &s->fast_modems.v34_tx);
            fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
            break;
#endif
        }
        /*endswitch*/
    }
    /*endif*/
    s->rx_frame_received = false;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_set_put_bit(fax_modems_state_t *s, put_bit_func_t put_bit, void *user_data)
{
    s->put_bit = put_bit;
    s->put_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_set_get_bit(fax_modems_state_t *s, get_bit_func_t get_bit, void *user_data)
{
    s->get_bit = get_bit;
    s->get_bit_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_set_rx_handler(fax_modems_state_t *s,
                                             span_rx_handler_t rx_handler,
                                             void *rx_user_data,
                                             span_rx_fillin_handler_t rx_fillin_handler,
                                             void *rx_fillin_user_data)
{
    if (s->deferred_rx_handler_updates)
    {
        /* Only update the actual handlers if they are not currently sidelined to dummy targets */
        if (s->rx_handler != span_dummy_rx)
            s->rx_handler = rx_handler;
        /*endif*/
        s->base_rx_handler = rx_handler;

        if (s->rx_fillin_handler != span_dummy_rx_fillin)
            s->rx_fillin_handler = rx_fillin_handler;
        /*endif*/
        s->base_rx_fillin_handler = rx_fillin_handler;
    }
    else
    {
        s->rx_handler = rx_handler;
        s->rx_fillin_handler = rx_fillin_handler;
    }
    /*endif*/
    s->rx_user_data = rx_user_data;
    s->rx_fillin_user_data = rx_fillin_user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_set_rx_active(fax_modems_state_t *s, int active)
{
    s->rx_handler = (active)  ?  s->base_rx_handler  :  span_dummy_rx;
    s->rx_fillin_handler = (active)  ?  s->base_rx_fillin_handler  :  span_dummy_rx_fillin;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_set_tx_handler(fax_modems_state_t *s, span_tx_handler_t handler, void *user_data)
{
    s->tx_handler = handler;
    s->tx_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_set_next_tx_handler(fax_modems_state_t *s, span_tx_handler_t handler, void *user_data)
{
    s->next_tx_handler = handler;
    s->next_tx_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_set_next_tx_type(fax_modems_state_t *s)
{
    if (s->next_tx_handler)
    {
        fax_modems_set_tx_handler(s, s->next_tx_handler, s->next_tx_user_data);
        fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
        return 0;
    }
    /*endif*/
    /* There is nothing else to change to, so use zero length silence */
    silence_gen_alter(&s->silence_gen, 0);
    fax_modems_set_tx_handler(s, (span_tx_handler_t) &silence_gen, &s->silence_gen);
    fax_modems_set_next_tx_handler(s, (span_tx_handler_t) NULL, NULL);
    s->transmit = false;
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_set_tep_mode(fax_modems_state_t *s, int use_tep)
{
    s->use_tep = use_tep;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) fax_modems_get_logging_state(fax_modems_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_restart(fax_modems_state_t *s)
{
    s->current_tx_type = -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(fax_modems_state_t *) fax_modems_init(fax_modems_state_t *s,
                                                   int use_tep,
                                                   hdlc_frame_handler_t hdlc_accept,
                                                   hdlc_underflow_handler_t hdlc_tx_underflow,
                                                   put_bit_func_t non_ecm_put_bit,
                                                   get_bit_func_t non_ecm_get_bit,
                                                   tone_report_func_t tone_callback,
                                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (fax_modems_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        /*endif*/
    }
    /*endif*/
    memset(s, 0, sizeof(*s));
    s->use_tep = use_tep;

    modem_connect_tones_tx_init(&s->connect_tx, MODEM_CONNECT_TONES_FAX_CNG);
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "FAX modems");

    s->tone_callback = tone_callback;
    s->tone_callback_user_data = user_data;
    if (tone_callback)
    {
        modem_connect_tones_rx_init(&s->connect_rx,
                                    MODEM_CONNECT_TONES_FAX_CNG,
                                    s->tone_callback,
                                    s->tone_callback_user_data);
    }
    /*endif*/
    dc_restore_init(&s->dc_restore);

    s->get_bit = non_ecm_get_bit;
    s->get_bit_user_data = user_data;
    s->put_bit = non_ecm_put_bit;
    s->put_bit_user_data = user_data;

    s->hdlc_accept = hdlc_accept;
    s->hdlc_accept_user_data = user_data;

    hdlc_rx_init(&s->hdlc_rx, false, true, HDLC_FRAMING_OK_THRESHOLD, fax_modems_hdlc_accept, s);
    hdlc_tx_init(&s->hdlc_tx, false, 2, false, hdlc_tx_underflow, user_data);

    fax_modems_start_slow_modem(s, FAX_MODEM_V21_RX);
    fsk_tx_init(&s->v21_tx, &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &s->hdlc_tx);

    silence_gen_init(&s->silence_gen, 0);

    s->rx_signal_present = false;
    s->rx_handler = (span_rx_handler_t) &span_dummy_rx;
    s->rx_fillin_handler = (span_rx_fillin_handler_t) &span_dummy_rx;
    s->rx_user_data = NULL;
    s->rx_fillin_user_data = NULL;
    s->tx_handler = (span_tx_handler_t) &silence_gen;
    s->tx_user_data = &s->silence_gen;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_release(fax_modems_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_free(fax_modems_state_t *s)
{
    if (s)
        span_free(s);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
