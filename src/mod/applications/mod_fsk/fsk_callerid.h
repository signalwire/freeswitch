/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * mod_fsk -- FSK data transfer
 *
 */
#ifndef __FSK_CALLER_ID_H
#define __FSK_CALLER_ID_H
SWITCH_BEGIN_EXTERN_C
#include "fsk.h"
#include "uart.h"

#define FSK_MOD_FACTOR 0x10000

typedef enum {
	ENDIAN_BIG = 1,
	ENDIAN_LITTLE = -1
} endian_t;

typedef enum {
	CID_TYPE_SDMF = 0x04,
	CID_TYPE_MDMF = 0x80
} cid_type_t;

typedef enum {
	MDMF_DATETIME = 1,
	MDMF_PHONE_NUM = 2,
	MDMF_DDN = 3,
	MDMF_NO_NUM = 4,
	MDMF_PHONE_NAME = 7,
	MDMF_NO_NAME = 8,
	MDMF_ALT_ROUTE = 9,
	MDMF_NAME_VALUE = 10,
	MDMF_INVALID = 11
} mdmf_type_t;

struct bitstream {
	uint8_t *data;
	uint32_t datalen;
	uint32_t byte_index;
	uint8_t bit_index;
	int8_t endian;
	uint8_t top;
	uint8_t bot;
	uint8_t ss;
	uint8_t ssv;
};

struct fsk_data_state {
	dsp_fsk_handle_t *fsk1200_handle;
	uint8_t init;
	uint8_t *buf;
	size_t bufsize;
	size_t blen;
	size_t bpos;
	size_t dlen;
	size_t ppos;
	int checksum;
};

typedef struct bitstream bitstream_t;
typedef struct fsk_data_state fsk_data_state_t;
typedef switch_status_t (*fsk_write_sample_t)(int16_t *buf, size_t buflen, void *user_data);

struct fsk_modulator {
	teletone_dds_state_t dds;
	bitstream_t bs;
	uint32_t carrier_bits_start;
	uint32_t carrier_bits_stop;
	uint32_t chan_sieze_bits;
	uint32_t bit_factor;
	uint32_t bit_accum;
	uint32_t sample_counter;
	int32_t samples_per_bit;
	int32_t est_bytes;
	fsk_modem_types_t modem_type;
	fsk_data_state_t *fsk_data;
	fsk_write_sample_t write_sample_callback;
	void *user_data;
	int16_t sample_buffer[64];
};


typedef int (*fsk_data_decoder_t)(fsk_data_state_t *state);

typedef void (*logger_t)(const char *file, const char *func, int line, int level, const char *fmt, ...);
typedef struct fsk_modulator fsk_modulator_t;

switch_status_t fsk_data_init(fsk_data_state_t *state, uint8_t *data, uint32_t datalen);
void bitstream_init(bitstream_t *bsp, uint8_t *data, uint32_t datalen, endian_t endian, uint8_t ss);
int8_t bitstream_get_bit(bitstream_t *bsp);
switch_status_t fsk_data_add_mdmf(fsk_data_state_t *state, mdmf_type_t type, const uint8_t *data, uint32_t datalen);
switch_status_t fsk_data_add_checksum(fsk_data_state_t *state);
switch_status_t fsk_data_parse(fsk_data_state_t *state, size_t *type, char **data, size_t *len);
switch_status_t fsk_demod_feed(fsk_data_state_t *state, int16_t *data, size_t samples);
switch_status_t fsk_demod_destroy(fsk_data_state_t *state);
int fsk_demod_init(fsk_data_state_t *state, int rate, uint8_t *buf, size_t bufsize);
size_t fsk_modulator_generate_bit(fsk_modulator_t *fsk_trans, int8_t bit, int16_t *buf, size_t buflen);
int32_t fsk_modulator_generate_carrier_bits(fsk_modulator_t *fsk_trans, uint32_t bits);
void fsk_modulator_generate_chan_sieze(fsk_modulator_t *fsk_trans);
void fsk_modulator_send_data(fsk_modulator_t *fsk_trans);
switch_status_t fsk_modulator_init(fsk_modulator_t *fsk_trans,
								   fsk_modem_types_t modem_type,
								   uint32_t sample_rate,
								   fsk_data_state_t *fsk_data,
								   float db_level,
								   uint32_t carrier_bits_start,
								   uint32_t carrier_bits_stop,
								   uint32_t chan_sieze_bits,
								   fsk_write_sample_t write_sample_callback,
								   void *user_data);


#define fsk_modulator_send_all(_it) fsk_modulator_generate_chan_sieze(_it); \
	fsk_modulator_generate_carrier_bits(_it, _it->carrier_bits_start); \
	fsk_modulator_send_data(_it); \
	fsk_modulator_generate_carrier_bits(_it, _it->carrier_bits_stop)

SWITCH_END_EXTERN_C
#endif
