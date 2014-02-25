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
#include "dec_if.h"
#include "enc_if.h"

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
};

#define AMRWB_DEFAULT_BITRATE AMRWB_BITRATE_24K

static struct {
	switch_byte_t default_bitrate;
} globals;

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
					} else if (!strcasecmp(data, "interveaving")) {
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
					} else if (!strcasecmp(data, "mode-set")) {
						int y, m_argc;
						char *m_argv[8];
						m_argc = switch_separate_string(arg, ',', m_argv, (sizeof(m_argv) / sizeof(m_argv[0])));
						for (y = 0; y < m_argc; y++) {
							context->enc_modes |= (1 << atoi(m_argv[y]));
						}
					}
				}
			}
		}

		if (context->enc_modes) {
			for (i = 8; i > -1; i++) {
				if (context->enc_modes & (1 << i)) {
					context->enc_mode = (switch_byte_t) i;
					break;
				}
			}
		}

		if (!context->enc_mode) {
			context->enc_mode = globals.default_bitrate;
		}

		switch_snprintf(fmtptmp, sizeof(fmtptmp), "octet-align=%d; mode-set=%d", switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0,
						context->enc_mode);
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, fmtptmp);

		context->enc_mode = AMRWB_DEFAULT_BITRATE;
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

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = E_IF_encode(context->encoder_state, context->enc_mode, (int16_t *) decoded_data, (switch_byte_t *) encoded_data, 0);

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

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	D_IF_decode(context->decoder_state, (unsigned char *) encoded_data, (int16_t *) decoded_data, 0);
	*decoded_data_len = codec->implementation->decoded_bytes_per_packet;

	return SWITCH_STATUS_SUCCESS;
#endif
}

/* Registration */
SWITCH_MODULE_LOAD_FUNCTION(mod_amrwb_load)
{
	switch_codec_interface_t *codec_interface;
#ifndef AMRWB_PASSTHROUGH
	char *cf = "amrwb.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));
	globals.default_bitrate = AMRWB_DEFAULT_BITRATE;

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if (!strcasecmp(var, "default-bitrate")) {
					globals.default_bitrate = (switch_byte_t) atoi(val);
				}
			}
		}
	}
#endif

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "AMR-WB");
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 100, "AMR-WB", "octet-align=0", 16000, 16000, 23850,
										 20000, 320, 640, 0, 1, 1, switch_amrwb_init, switch_amrwb_encode, switch_amrwb_decode, switch_amrwb_destroy);
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
