/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_modems.c - the analogue modem set for fax processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2005, 2006, 2008 Steve Underwood
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
 * $Id: fax_modems.c,v 1.8 2009/11/02 13:25:20 steveu Exp $
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
#include "floating_fudge.h"
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#if defined(LOG_FAX_AUDIO)
#include <unistd.h>
#endif

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
#include "spandsp/super_tone_rx.h"
#include "spandsp/modem_connect_tones.h"
#include "spandsp/fax_modems.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/silence_gen.h"
#include "spandsp/private/fsk.h"
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

SPAN_DECLARE(int) fax_modems_v17_v21_rx(void *user_data, const int16_t amp[], int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v17_rx(&s->v17_rx, amp, len);
    fsk_rx(&s->v21_rx, amp, len);
    if (s->rx_frame_received)
    {
        /* We have received something, and the fast modem has not trained. We must
           be receiving valid V.21 */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&s->v21_rx));
        s->rx_handler = (span_rx_handler_t *) &fsk_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t *) &fsk_rx_fillin;
        s->rx_user_data = &s->v21_rx;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v17_v21_rx_fillin(void *user_data, int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v17_rx_fillin(&s->v17_rx, len);
    fsk_rx_fillin(&s->v21_rx, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v27ter_v21_rx(void *user_data, const int16_t amp[], int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v27ter_rx(&s->v27ter_rx, amp, len);
    fsk_rx(&s->v21_rx, amp, len);
    if (s->rx_frame_received)
    {
        /* We have received something, and the fast modem has not trained. We must
           be receiving valid V.21 */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&s->v21_rx));
        s->rx_handler = (span_rx_handler_t *) &fsk_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t *) &fsk_rx_fillin;
        s->rx_user_data = &s->v21_rx;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v27ter_v21_rx_fillin(void *user_data, int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v27ter_rx_fillin(&s->v27ter_rx, len);
    fsk_rx_fillin(&s->v21_rx, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v29_v21_rx(void *user_data, const int16_t amp[], int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v29_rx(&s->v29_rx, amp, len);
    fsk_rx(&s->v21_rx, amp, len);
    if (s->rx_frame_received)
    {
        /* We have received something, and the fast modem has not trained. We must
           be receiving valid V.21 */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.21 (%.2fdBm0)\n", fsk_rx_signal_power(&s->v21_rx));
        s->rx_handler = (span_rx_handler_t *) &fsk_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t *) &fsk_rx_fillin;
        s->rx_user_data = &s->v21_rx;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) fax_modems_v29_v21_rx_fillin(void *user_data, int len)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    v29_rx_fillin(&s->v29_rx, len);
    fsk_rx_fillin(&s->v21_rx, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void v21_rx_status_handler(void *user_data, int status)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
}
/*- End of function --------------------------------------------------------*/

static void v17_rx_status_handler(void *user_data, int status)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    switch (status)
    {
    case SIG_STATUS_TRAINING_SUCCEEDED:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching to V.17 (%.2fdBm0)\n", v17_rx_signal_power(&s->v17_rx));
        s->rx_handler = (span_rx_handler_t *) &v17_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t *) &v17_rx_fillin;
        s->rx_user_data = &s->v17_rx;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void v27ter_rx_status_handler(void *user_data, int status)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    switch (status)
    {
    case SIG_STATUS_TRAINING_SUCCEEDED:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching to V.27ter (%.2fdBm0)\n", v27ter_rx_signal_power(&s->v27ter_rx));
        s->rx_handler = (span_rx_handler_t *) &v27ter_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t *) &v27ter_rx_fillin;
        s->rx_user_data = &s->v27ter_rx;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void v29_rx_status_handler(void *user_data, int status)
{
    fax_modems_state_t *s;

    s = (fax_modems_state_t *) user_data;
    switch (status)
    {
    case SIG_STATUS_TRAINING_SUCCEEDED:
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching to V.29 (%.2fdBm0)\n", v29_rx_signal_power(&s->v29_rx));
        s->rx_handler = (span_rx_handler_t *) &v29_rx;
        s->rx_fillin_handler = (span_rx_fillin_handler_t *) &v29_rx_fillin;
        s->rx_user_data = &s->v29_rx;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_start_rx_modem(fax_modems_state_t *s, int which)
{
    switch (which)
    {
    case FAX_MODEM_V17_RX:
        v17_rx_set_modem_status_handler(&s->v17_rx, v17_rx_status_handler, s);
        break;
    case FAX_MODEM_V27TER_RX:
        v27ter_rx_set_modem_status_handler(&s->v27ter_rx, v27ter_rx_status_handler, s);
        break;
    case FAX_MODEM_V29_RX:
        v29_rx_set_modem_status_handler(&s->v29_rx, v29_rx_status_handler, s);
        break;
    }
    fsk_rx_set_modem_status_handler(&s->v21_rx, v21_rx_status_handler, s);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) fax_modems_set_tep_mode(fax_modems_state_t *s, int use_tep)
{
    s->use_tep = use_tep;
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
        if ((s = (fax_modems_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->use_tep = use_tep;

    hdlc_rx_init(&s->hdlc_rx, FALSE, FALSE, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, user_data);
    hdlc_tx_init(&s->hdlc_tx, FALSE, 2, FALSE, hdlc_tx_underflow, user_data);
    fsk_rx_init(&s->v21_rx, &preset_fsk_specs[FSK_V21CH2], FSK_FRAME_MODE_SYNC, (put_bit_func_t) hdlc_rx_put_bit, &s->hdlc_rx);
    fsk_rx_signal_cutoff(&s->v21_rx, -39.09f);
    fsk_tx_init(&s->v21_tx, &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &s->hdlc_tx);
    v17_rx_init(&s->v17_rx, 14400, non_ecm_put_bit, user_data);
    v17_tx_init(&s->v17_tx, 14400, s->use_tep, non_ecm_get_bit, user_data);
    v29_rx_init(&s->v29_rx, 9600, non_ecm_put_bit, user_data);
    v29_rx_signal_cutoff(&s->v29_rx, -45.5f);
    v29_tx_init(&s->v29_tx, 9600, s->use_tep, non_ecm_get_bit, user_data);
    v27ter_rx_init(&s->v27ter_rx, 4800, non_ecm_put_bit, user_data);
    v27ter_tx_init(&s->v27ter_tx, 4800, s->use_tep, non_ecm_get_bit, user_data);
    silence_gen_init(&s->silence_gen, 0);
    modem_connect_tones_tx_init(&s->connect_tx, MODEM_CONNECT_TONES_FAX_CNG);
    if (tone_callback)
    {
        modem_connect_tones_rx_init(&s->connect_rx,
                                    MODEM_CONNECT_TONES_FAX_CNG,
                                    tone_callback,
                                    user_data);
    }
    dc_restore_init(&s->dc_restore);

    s->rx_signal_present = FALSE;
    s->rx_handler = (span_rx_handler_t *) &span_dummy_rx;
    s->rx_fillin_handler = (span_rx_fillin_handler_t *) &span_dummy_rx;
    s->rx_user_data = NULL;
    s->tx_handler = (span_tx_handler_t *) &silence_gen;
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
        free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
