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
 * Brian K. West <brian@freeswitch.org>
 * Dragos Oancea <dragos.oancea@athonet.com>
 * Federico Favaro <federico.favaro@athonet.com>
 * Marco Sinibaldi <marco.sinibaldi@athonet.com>
 *
 * The amrwb codec itself is not distributed with this module.
 *
 * mod_amrwb.c -- GSM-AMRWB Codec Module
 *
 */

#include "switch.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_amrwb_load);
SWITCH_MODULE_DEFINITION(mod_amrwb, mod_amrwb_load, NULL, NULL);

#ifndef AMRWB_PASSTHROUGH
#include "opencore-amrwb/dec_if.h" /*AMR-WB decoder API*/
#include "vo-amrwbenc/enc_if.h" /*AMR-WB encoder API*/

#include "bitshift.h"
#include "amrwb_be.h"

typedef enum {
	AMRWB_OPT_OCTET_ALIGN = (1 << 0),
	AMRWB_OPT_CRC = (1 << 1),
	AMRWB_OPT_MODE_CHANGE_NEIGHBOR = (1 << 2),
	AMRWB_OPT_ROBUST_SORTING = (1 << 3),
	AMRWB_OPT_INTERLEAVING = (1 << 4)
} amrwb_flag_t;

typedef enum {
	AMRWB_BITRATE_7K = 0,
	AMRWB_BITRATE_8K,
	AMRWB_BITRATE_12K,
	AMRWB_BITRATE_14K,
	AMRWB_BITRATE_16K,
	AMRWB_BITRATE_18K,
	AMRWB_BITRATE_20K,
	AMRWB_BITRATE_23K,
	AMRWB_BITRATE_24K
} amrwb_bitrate_t;

struct amrwb_context {
	void *encoder_state;
	void *decoder_state;
	switch_byte_t enc_modes;
	switch_byte_t enc_mode;
	uint32_t change_period;
	switch_byte_t max_ptime;
	switch_byte_t ptime;
	switch_byte_t channels;
	switch_byte_t flags;
	int max_red;
	int debug;
};

#define SWITCH_AMRWB_DEFAULT_BITRATE AMRWB_BITRATE_24K

static struct {
	switch_byte_t default_bitrate;
	switch_byte_t volte;
	switch_byte_t adjust_bitrate;
	int debug;
} globals;

const int switch_amrwb_frame_sizes[] = {17, 23, 32, 36, 40, 46, 50, 58, 60, 5};

#define SWITCH_AMRWB_OUT_MAX_SIZE 61
#define SWITCH_AMRWB_MODES 10 /* Silence Indicator (SID) included */

static switch_bool_t switch_amrwb_unpack_oa(unsigned char *buf, uint8_t *tmp, int encoded_data_len)
{
	uint8_t *tocs;
	int index;
	int framesz;

	buf++;/* CMR skip */
	tocs = buf;
	index = ((tocs[0]>>3) & 0xf);
	buf++; /* point to voice payload */

	if (index > 10) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB decoder (OA): Invalid TOC: 0x%x", index);
		return SWITCH_FALSE;
	}
	framesz = switch_amrwb_frame_sizes[index];
	if (framesz > encoded_data_len - 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB decoder (OA): Invalid frame size: %d\n", framesz);
		return SWITCH_FALSE;
	}
	tmp[0] = tocs[0];
	memcpy(&tmp[1], buf, framesz);

	return SWITCH_TRUE;
}

static switch_bool_t switch_amrwb_pack_oa(unsigned char *shift_buf, int n)
{
/* Interleaving code here */
	return SWITCH_TRUE;
}

static switch_bool_t switch_amrwb_info(unsigned char *encoded_buf, int encoded_data_len, int payload_format, char *print_text)
{
	uint8_t *tocs;
	int framesz, index, not_last_frame, q, ft;
	uint8_t shift_tocs[2] = {0x00, 0x00};

	if (!encoded_buf) {
		return SWITCH_FALSE;
	}

	/* payload format can be OA (octed-aligned) or BE (bandwidth efficient)*/
	if (payload_format) {
		/* OA */
		encoded_buf++; /* CMR skip */
		tocs = encoded_buf;
		index = (tocs[0] >> 3) & 0x0f;
		if (index > SWITCH_AMRWB_MODES) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB decoder (OA): Invalid TOC 0x%x\n", index);
			return SWITCH_FALSE;
		}
		framesz = switch_amrwb_frame_sizes[index];
		not_last_frame = (tocs[0] >> 7) & 1;
		q = (tocs[0] >> 2) & 1;
		ft = tocs[0] >> 3;
		ft &= ~(1 << 5); /* Frame Type */
	} else {
		/* BE */
		memcpy(shift_tocs, encoded_buf, 2);
		/* shift for BE */
		switch_amr_array_lshift(4, shift_tocs, 2);
		not_last_frame = (shift_tocs[0] >> 7) & 1;
		q = (shift_tocs[0] >> 2) & 1;
		ft = shift_tocs[0] >> 3;
		ft &= ~(1 << 5); /* Frame Type */
		index = (shift_tocs[0] >> 3) & 0x0f;
		if (index > SWITCH_AMRWB_MODES) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB decoder (BE): Invalid TOC 0x%x\n", index);
			return SWITCH_FALSE;
		}
		framesz = switch_amrwb_frame_sizes[index];
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s): FT: [0x%x] Q: [0x%x] Frame flag: [%d]\n",
													print_text, payload_format ? "OA":"BE", ft, q, not_last_frame);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s): AMRWB encoded voice payload sz: [%d] : | encoded_data_len: [%d]\n",
													print_text, payload_format ? "OA":"BE", framesz, encoded_data_len);

	return SWITCH_TRUE;
}
#endif

static switch_status_t switch_amrwb_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
#ifdef AMRWB_PASSTHROUGH
	codec->flags |= SWITCH_CODEC_FLAG_PASSTHROUGH;
	if (codec->fmtp_in) {
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
	}
	return SWITCH_STATUS_SUCCESS;
#else
	struct amrwb_context *context = NULL;
	int encoding, decoding;
	int x, i, argc;
	char *argv[10];
	char fmtptmp[128];

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct amrwb_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {

		/* "mode" may mean two different things:
		 * "Octed Aligned" or "Bandwidth Efficient" encoding mode ,
		 * or the actual bitrate  which is set with FMTP param "mode-set". */
		/* https://tools.ietf.org/html/rfc4867 */

		/* set the default mode just in case there's no "mode-set" FMTP param */
		context->enc_mode = globals.default_bitrate;

		/* octet-align = 0  - per RFC - if there's no `octet-align` FMTP value then BE is employed */
		switch_clear_flag(context, AMRWB_OPT_OCTET_ALIGN);

		if (codec->fmtp_in) {
			argc = switch_separate_string(codec->fmtp_in, ';', argv, (sizeof(argv) / sizeof(argv[0])));
			for (x = 0; x < argc; x++) {
				char *data = argv[x];
				char *arg;
				while (*data && *data == ' ') {
					data++;
				}
				if ((arg = strchr(data, '='))) {
					*arg++ = '\0';
					if (!strcasecmp(data, "octet-align")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_OCTET_ALIGN);
						}
					} else if (!strcasecmp(data, "mode-change-neighbor")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_MODE_CHANGE_NEIGHBOR);
						}
					} else if (!strcasecmp(data, "crc")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_CRC);
						}
					} else if (!strcasecmp(data, "robust-sorting")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_ROBUST_SORTING);
						}
					} else if (!strcasecmp(data, "interleaving")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_INTERLEAVING);
						}
					} else if (!strcasecmp(data, "mode-change-period")) {
						context->change_period = atoi(arg);
					} else if (!strcasecmp(data, "ptime")) {
						context->ptime = (switch_byte_t) atoi(arg);
					} else if (!strcasecmp(data, "channels")) {
						context->channels = (switch_byte_t) atoi(arg);
					} else if (!strcasecmp(data, "maxptime")) {
						context->max_ptime = (switch_byte_t) atoi(arg);
					} else if (!strcasecmp(data, "max-red")) {
						context->max_red = atoi(arg);
					} else if (!strcasecmp(data, "mode-set")) {
						int y, m_argc;
						char *m_argv[SWITCH_AMRWB_MODES-1]; /* AMRWB has 9 modes */
						m_argc = switch_separate_string(arg, ',', m_argv, (sizeof(m_argv) / sizeof(m_argv[0])));
						for (y = 0; y < m_argc; y++) {
							context->enc_modes |= (1 << atoi(m_argv[y]));
							context->enc_mode = atoi(m_argv[y]);
						}
					}
				}
			}
		}

		if (context->enc_modes) {
			/* choose the highest mode (bitrate) for high audio quality. */
			for (i = 8; i > -1; i--) {
				if (context->enc_modes & (1 << i)) {
					context->enc_mode = (switch_byte_t) i;
					break;
				}
			}
		}

		if (globals.adjust_bitrate) {
			switch_set_flag(codec, SWITCH_CODEC_FLAG_HAS_ADJ_BITRATE);
		}

		if (!globals.volte) {
			switch_snprintf(fmtptmp, sizeof(fmtptmp), "octet-align=%d; mode-set=%d",
					switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0, context->enc_mode);
		} else {
			switch_snprintf(fmtptmp, sizeof(fmtptmp), "octet-align=%d; mode-set=%d; max-red=0; mode-change-capability=2",
					switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0, context->enc_mode);
		}
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, fmtptmp);

		context->encoder_state = NULL;
		context->decoder_state = NULL;

		if (encoding) {
			context->encoder_state = E_IF_init();
		}

		if (decoding) {
			context->decoder_state = D_IF_init();
		}

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}
#endif
}

static switch_status_t switch_amrwb_destroy(switch_codec_t *codec)
{
#ifndef AMRWB_PASSTHROUGH
	struct amrwb_context *context = codec->private_info;

	if (context->encoder_state) {
		E_IF_exit(context->encoder_state);
}
	if (context->decoder_state) {
		D_IF_exit(context->decoder_state);
	}
	codec->private_info = NULL;
#endif
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_amrwb_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										   unsigned int *flag)
{
#ifdef AMRWB_PASSTHROUGH
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This codec is only usable in passthrough mode!\n");
	return SWITCH_STATUS_FALSE;
#else
	struct amrwb_context *context = codec->private_info;
	int n;
	unsigned char *shift_buf = encoded_data;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	n = E_IF_encode(context->encoder_state, context->enc_mode, (int16_t *) decoded_data, (switch_byte_t *) encoded_data + 1, 0);
	if (n < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB encoder: E_IF_encode() ERROR!\n");
		return SWITCH_STATUS_FALSE;
	}

	/* set CMR + TOC (F + 3 bits of FT), 1111 = CMR: No mode request */
	*(switch_byte_t *) encoded_data = 0xf0;
	*encoded_data_len  = n;

	if (switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN)) {
		switch_amrwb_pack_oa(shift_buf, n);  /* the payload is OA as it
												comes out of the encoding function */
		*encoded_data_len = n + 1;
	} else {
		switch_amrwb_pack_be(shift_buf, n);
	}

	if (globals.debug) {
		switch_amrwb_info(shift_buf, *encoded_data_len, switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0, "AMRWB encoder");
	}

	return SWITCH_STATUS_SUCCESS;

#endif
}

static switch_status_t switch_amrwb_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
#ifdef AMRWB_PASSTHROUGH
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This codec is only usable in passthrough mode!\n");
	return SWITCH_STATUS_FALSE;
#else
	struct amrwb_context *context = codec->private_info;
	unsigned char *buf = encoded_data;
	uint8_t tmp[SWITCH_AMRWB_OUT_MAX_SIZE];

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (globals.debug) {
		switch_amrwb_info(buf, encoded_data_len, switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0, "AMRWB decoder");
	}

	if (switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN)) {
		/* Octed Aligned */
		if (!switch_amrwb_unpack_oa(buf, tmp, encoded_data_len)) {
			return SWITCH_STATUS_FALSE;
		}
	} else {
		/* Bandwidth Efficient */
		if (!switch_amrwb_unpack_be(buf, tmp, encoded_data_len)) {
			return SWITCH_STATUS_FALSE;
		}
	}

	D_IF_decode(context->decoder_state, tmp, (int16_t *) decoded_data, 0);

	*decoded_data_len = codec->implementation->decoded_bytes_per_packet;

	return SWITCH_STATUS_SUCCESS;
#endif
}

#ifndef AMRWB_PASSTHROUGH
static switch_status_t switch_amrwb_control(switch_codec_t *codec,
										   switch_codec_control_command_t cmd,
										   switch_codec_control_type_t ctype,
										   void *cmd_data,
										   switch_codec_control_type_t atype,
										   void *cmd_arg,
										   switch_codec_control_type_t *rtype,
										   void **ret_data)
{
	struct amrwb_context *context = codec->private_info;

	switch(cmd) {
	case SCC_DEBUG:
		{
			int32_t level = *((uint32_t *) cmd_data);
			context->debug = level;
		}
		break;
	case SCC_AUDIO_ADJUST_BITRATE:
		{
			const char *cmd = (const char *)cmd_data;

			if (!strcasecmp(cmd, "increase")) {
				if (context->enc_mode < SWITCH_AMRWB_MODES - 1) {
					int mode_step = 2; /*this is the mode, not the actual bitrate*/
					context->enc_mode = context->enc_mode + mode_step;
					if (globals.debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"AMRWB encoder: Adjusting mode to %d (increase)\n", context->enc_mode);
					}
				}
			} else if (!strcasecmp(cmd, "decrease")) {
				if (context->enc_mode > 0) {
					int mode_step = 2; /*this is the mode, not the actual bitrate*/
					context->enc_mode = context->enc_mode - mode_step;
					if (globals.debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"AMRWB encoder: Adjusting mode to %d (decrease)\n", context->enc_mode);
					}
				}
			} else if (!strcasecmp(cmd, "default")) {
					context->enc_mode = globals.default_bitrate;
					if (globals.debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"AMRWB encoder: Adjusting mode to %d (default)\n", context->enc_mode);
					}
			} else {
				/*minimum bitrate (AMRWB mode)*/
				context->enc_mode = 0;
				if (globals.debug || context->debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							"AMRWB encoder: Adjusting mode to %d (minimum)\n", context->enc_mode);
				}
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}
#endif

static char *generate_fmtp(switch_memory_pool_t *pool , int octet_align)
{
	char buf[256] = { 0 };

	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "octet-align=%d; ", octet_align);

#ifndef AMRWB_PASSTHROUGH
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "mode-set=%d; ", globals.default_bitrate);

	if (globals.volte) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "max-red=0; mode-change-capability=2; ");
	}
#endif

	if (end_of(buf) == ' ') {
		*(end_of_p(buf) - 1) = '\0';
	}

	return switch_core_strdup(pool, buf);
}

#ifndef AMRWB_PASSTHROUGH

#define AMRWB_DEBUG_SYNTAX "<on|off>"
SWITCH_STANDARD_API(mod_amrwb_debug)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", AMRWB_DEBUG_SYNTAX);
	} else {
		if (!strcasecmp(cmd, "on")) {
			globals.debug = 1;
			stream->write_function(stream, "AMRWB Debug: on\n");
		} else if (!strcasecmp(cmd, "off")) {
				globals.debug = 0;
				stream->write_function(stream, "AMRWB Debug: off\n");
		} else {
				stream->write_function(stream, "-USAGE: %s\n", AMRWB_DEBUG_SYNTAX);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}
#endif

/* Registration */
SWITCH_MODULE_LOAD_FUNCTION(mod_amrwb_load)
{
	switch_codec_interface_t *codec_interface;
	char *default_fmtp_oa = NULL;
	char *default_fmtp_be = NULL;

#ifndef AMRWB_PASSTHROUGH
	switch_api_interface_t *commands_api_interface;
	char *cf = "amrwb.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));
	globals.default_bitrate = SWITCH_AMRWB_DEFAULT_BITRATE;

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if (!strcasecmp(var, "default-bitrate")) {
					globals.default_bitrate = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "volte")) {
					/* ETSI TS 126 236 compatibility:  http://www.etsi.org/deliver/etsi_ts/126200_126299/126236/10.00.00_60/ts_126236v100000p.pdf */
					globals.volte = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "adjust-bitrate")) {
					globals.adjust_bitrate = (switch_byte_t) atoi(val);
				}
			}
		}
	}
#endif

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

#ifndef AMRWB_PASSTHROUGH
	SWITCH_ADD_API(commands_api_interface, "amrwb_debug", "Set AMR-WB Debug", mod_amrwb_debug, AMRWB_DEBUG_SYNTAX);

	switch_console_set_complete("add amrwb_debug on");
	switch_console_set_complete("add amrwb_debug off");
#endif

	SWITCH_ADD_CODEC(codec_interface, "AMR-WB / Octet Aligned");

	default_fmtp_oa = generate_fmtp(pool, 1);

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 100, "AMR-WB", default_fmtp_oa,
										 16000, 16000, 23850, 20000, 320, 640, 0, 1, 1,
										 switch_amrwb_init, switch_amrwb_encode, switch_amrwb_decode, switch_amrwb_destroy);
#ifndef AMRWB_PASSTHROUGH
	codec_interface->implementations->codec_control = switch_amrwb_control;
#endif

	SWITCH_ADD_CODEC(codec_interface, "AMR-WB / Bandwidth Efficient");

	default_fmtp_be = generate_fmtp(pool, 0);

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 110, "AMR-WB", default_fmtp_be,
										 16000, 16000, 23850, 20000, 320, 640, 0, 1, 1,
										 switch_amrwb_init, switch_amrwb_encode, switch_amrwb_decode, switch_amrwb_destroy);
#ifndef AMRWB_PASSTHROUGH
	codec_interface->implementations->codec_control = switch_amrwb_control;
#endif
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
