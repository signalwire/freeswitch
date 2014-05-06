/*
 * SpanDSP - a series of DSP components for telephony
 *
 * data_modems.h - definitions for the analogue modem set for data processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011 Steve Underwood
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

#if !defined(_SPANDSP_DATA_MODEMS_H_)
#define _SPANDSP_DATA_MODEMS_H_

enum
{
    DATA_MODEM_NONE = -1,
    DATA_MODEM_FLUSH = 0,
    DATA_MODEM_SILENCE,
    DATA_MODEM_CED_TONE,
    DATA_MODEM_CNG_TONE,
    DATA_MODEM_V8,
    DATA_MODEM_BELL103,
    DATA_MODEM_BELL202,
    DATA_MODEM_V21,
    DATA_MODEM_V23,
    DATA_MODEM_V22BIS,
    DATA_MODEM_V32BIS,
    DATA_MODEM_V34
};

/*!
    The set of modems needed for data, plus the auxilliary stuff, like tone generation.
*/
typedef struct data_modems_state_s data_modems_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/* N.B. the following are currently a work in progress */
SPAN_DECLARE(const char *) data_modems_modulation_to_str(int modulation_scheme);

SPAN_DECLARE(void) data_modems_set_tep_mode(data_modems_state_t *s, int use_tep);

SPAN_DECLARE(logging_state_t *) data_modems_get_logging_state(data_modems_state_t *s);

SPAN_DECLARE(int) data_modems_restart(data_modems_state_t *s);

SPAN_DECLARE(void) data_modems_set_async_mode(data_modems_state_t *s,
                                              int data_bits,
                                              int parity_bits,
                                              int stop_bits);

SPAN_DECLARE(void) data_modems_set_modem_type(data_modems_state_t *s, int which, int baud_rate, int bit_rate);

SPAN_DECLARE(int) data_modems_rx(data_modems_state_t *s, const int16_t amp[], int len);

SPAN_DECLARE(int) data_modems_rx_fillin(data_modems_state_t *s, int len);

SPAN_DECLARE(int) data_modems_tx(data_modems_state_t *s, int16_t amp[], int max_len);

SPAN_DECLARE(data_modems_state_t *) data_modems_init(data_modems_state_t *s,
                                                     bool calling_party,
                                                     put_msg_func_t put_msg,
                                                     get_msg_func_t get_msg,
                                                     void *user_data);

SPAN_DECLARE(int) data_modems_release(data_modems_state_t *s);

SPAN_DECLARE(int) data_modems_free(data_modems_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
