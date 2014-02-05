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
 * The amr codec itself is not distributed with this module.
 *
 * mod_amr.c -- GSM-AMR Codec Module
 *
 */

#include "switch.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_amr_load);
SWITCH_MODULE_DEFINITION(mod_amr, mod_amr_load, NULL, NULL);

#ifndef AMR_PASSTHROUGH
#include "interf_enc.h"
#include "interf_dec.h"

/*
 * Check section 8.1 of rfc3267 for possible sdp options.
 *  
 * SDP Example 
 * 
 * a=fmtp:97 mode-set=0,2,5,7; mode-change-period=2; mode-change-neighbor=1
 *
 *                                 Class A   total speech
 *                Index   Mode       bits       bits
 *                ----------------------------------------
 *                  0     AMR 4.75   42         95
 *                  1     AMR 5.15   49        103
 *                  2     AMR 5.9    55        118
 *                  3     AMR 6.7    58        134
 *                  4     AMR 7.4    61        148
 *                  5     AMR 7.95   75        159
 *                  6     AMR 10.2   65        204
 *                  7     AMR 12.2   81        244
 *                  8     AMR SID    39         39
 *
 *        Table 1.  The number of class A bits for the AMR codec.
 *
 */

typedef enum {
	AMR_OPT_OCTET_ALIGN = (1 << 0),
	AMR_OPT_CRC = (1 << 1),
	AMR_OPT_MODE_CHANGE_NEIGHBOR = (1 << 2),
	AMR_OPT_ROBUST_SORTING = (1 << 3),
	AMR_OPT_INTERLEAVING = (1 << 4)
} amr_flag_t;

typedef enum {
	GENERIC_PARAMETER_AMR_MAXAL_SDUFRAMES = 0,
	GENERIC_PARAMETER_AMR_BITRATE,
	GENERIC_PARAMETER_AMR_GSMAMRCOMFORTNOISE,
	GENERIC_PARAMETER_AMR_GSMEFRCOMFORTNOISE,
	GENERIC_PARAMETER_AMR_IS_641COMFORTNOISE,
	GENERIC_PARAMETER_AMR_PDCEFRCOMFORTNOISE
} amr_param_t;

typedef enum {
	AMR_BITRATE_475 = 0,
	AMR_BITRATE_515,
	AMR_BITRATE_590,
	AMR_BITRATE_670,
	AMR_BITRATE_740,
	AMR_BITRATE_795,
	AMR_BITRATE_1020,
	AMR_BITRATE_1220
} amr_bitrate_t;

typedef enum {
	AMR_DTX_DISABLED = 0,
	AMR_DTX_ENABLED
} amr_dtx_t;

/*! \brief Various codec settings */
struct amr_codec_settings {
	int dtx_mode;
	uint32_t change_period;
	switch_byte_t max_ptime;
	switch_byte_t ptime;
	switch_byte_t channels;
	switch_byte_t flags;
};
typedef struct amr_codec_settings amr_codec_settings_t;

static amr_codec_settings_t default_codec_settings = {
	/*.dtx_mode */ AMR_DTX_ENABLED,
	/*.change_period */ 0,
	/*.max_ptime */ 0,
	/*.ptime */ 0,
	/*.channels */ 0,
	/*.flags */ 0,
};


struct amr_context {
	void *encoder_state;
	void *decoder_state;
	switch_byte_t enc_modes;
	switch_byte_t enc_mode;
};

#define AMR_DEFAULT_BITRATE AMR_BITRATE_1220

static struct {
	switch_byte_t default_bitrate;
} globals;

static switch_status_t switch_amr_fmtp_parse(const char *fmtp, switch_codec_fmtp_t *codec_fmtp)
{
	if (codec_fmtp) {
		amr_codec_settings_t *codec_settings = NULL;
		if (codec_fmtp->private_info) {
			codec_settings = codec_fmtp->private_info;
			memcpy(codec_settings, &default_codec_settings, sizeof(*codec_settings));
		}

		if (fmtp) {
			int x, argc;
			char *argv[10];
			char *fmtp_dup = strdup(fmtp);

			switch_assert(fmtp_dup);

			argc = switch_separate_string((char *) fmtp_dup, ';', argv, (sizeof(argv) / sizeof(argv[0])));
			for (x = 0; x < argc; x++) {
				char *data = argv[x];
				char *arg;
				switch_assert(data);
				while (*data == ' ') {
					data++;
				}
				if ((arg = strchr(data, '='))) {
					*arg++ = '\0';
					/*
					   if (!strcasecmp(data, "bitrate")) {
					   bit_rate = atoi(arg);
					   }
					 */
					if (codec_settings) {
						if (!strcasecmp(data, "octet-align")) {
							if (atoi(arg)) {
								switch_set_flag(codec_settings, AMR_OPT_OCTET_ALIGN);
							}
						} else if (!strcasecmp(data, "mode-change-neighbor")) {
							if (atoi(arg)) {
								switch_set_flag(codec_settings, AMR_OPT_MODE_CHANGE_NEIGHBOR);
							}
						} else if (!strcasecmp(data, "crc")) {
							if (atoi(arg)) {
								switch_set_flag(codec_settings, AMR_OPT_CRC);
							}
						} else if (!strcasecmp(data, "robust-sorting")) {
							if (atoi(arg)) {
								switch_set_flag(codec_settings, AMR_OPT_ROBUST_SORTING);
							}
						} else if (!strcasecmp(data, "interveaving")) {
							if (atoi(arg)) {
								switch_set_flag(codec_settings, AMR_OPT_INTERLEAVING);
							}
						} else if (!strcasecmp(data, "mode-change-period")) {
							codec_settings->change_period = atoi(arg);
						} else if (!strcasecmp(data, "ptime")) {
							codec_settings->ptime = (switch_byte_t) atoi(arg);
						} else if (!strcasecmp(data, "channels")) {
							codec_settings->channels = (switch_byte_t) atoi(arg);
						} else if (!strcasecmp(data, "maxptime")) {
							codec_settings->max_ptime = (switch_byte_t) atoi(arg);
						} else if (!strcasecmp(data, "mode-set")) {
							int y, m_argc;
							char *m_argv[7];
							m_argc = switch_separate_string(arg, ',', m_argv, (sizeof(m_argv) / sizeof(m_argv[0])));
							for (y = 0; y < m_argc; y++) {
								codec_settings->enc_modes |= (1 << atoi(m_argv[y]));
							}
						} else if (!strcasecmp(data, "dtx")) {
							codec_settings->dtx_mode = (atoi(arg)) ? AMR_DTX_ENABLED : AMR_DTX_DISABLED;
						}
					}

				}
			}
			free(fmtp_dup);
		}
		//codec_fmtp->bits_per_second = bit_rate;
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

#endif

static switch_status_t switch_amr_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
#ifdef AMR_PASSTHROUGH
	codec->flags |= SWITCH_CODEC_FLAG_PASSTHROUGH;
	if (codec->fmtp_in) {
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
	}
	return SWITCH_STATUS_SUCCESS;
#else

	struct amr_context *context = NULL;
	switch_codec_fmtp_t codec_fmtp;
	amr_codec_settings_t amr_codec_settings;
	int encoding, decoding;
	int x, i, argc;
	char *argv[10];
	char fmtptmp[128];

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct amr_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {

		memset(&codec_fmtp, '\0', sizeof(struct switch_codec_fmtp));
		codec_fmtp.private_info = &amr_codec_settings;
		switch_amr_fmtp_parse(codec->fmtp_in, &codec_fmtp);

		if (context->enc_modes) {
			for (i = 7; i > -1; i++) {
				if (context->enc_modes & (1 << i)) {
					context->enc_mode = (switch_byte_t) i;
					break;
				}
			}
		}

		if (!context->enc_mode) {
			context->enc_mode = globals.default_bitrate;
		}

		switch_snprintf(fmtptmp, sizeof(fmtptmp), "octet-align=%d; mode-set=%d", switch_test_flag(context, AMR_OPT_OCTET_ALIGN) ? 1 : 0,
						context->enc_mode);
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, fmtptmp);

		context->enc_mode = AMR_DEFAULT_BITRATE;
		context->encoder_state = NULL;
		context->decoder_state = NULL;

		if (encoding) {
			context->encoder_state = Encoder_Interface_init(context->dtx_mode);
		}

		if (decoding) {
			context->decoder_state = Decoder_Interface_init();
		}

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}
#endif
}

static switch_status_t switch_amr_destroy(switch_codec_t *codec)
{
#ifndef AMR_PASSTHROUGH
	struct amr_context *context = codec->private_info;

	if (context->encoder_state) {
		Encoder_Interface_exit(context->encoder_state);
	}
	if (context->decoder_state) {
		Decoder_Interface_exit(context->decoder_state);
	}
	codec->private_info = NULL;
#endif
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_amr_encode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *decoded_data,
										 uint32_t decoded_data_len,
										 uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										 unsigned int *flag)
{
#ifdef AMR_PASSTHROUGH
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This codec is only usable in passthrough mode!\n");
	return SWITCH_STATUS_FALSE;
#else
	struct amr_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = Encoder_Interface_Encode(context->encoder_state, context->enc_mode, (int16_t *) decoded_data, (switch_byte_t *) encoded_data, 0);

	return SWITCH_STATUS_SUCCESS;
#endif
}

static switch_status_t switch_amr_decode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *encoded_data,
										 uint32_t encoded_data_len,
										 uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										 unsigned int *flag)
{
#ifdef AMR_PASSTHROUGH
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This codec is only usable in passthrough mode!\n");
	return SWITCH_STATUS_FALSE;
#else
	struct amr_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	Decoder_Interface_Decode(context->decoder_state, (unsigned char *) encoded_data, (int16_t *) decoded_data, 0);
	*decoded_data_len = codec->implementation->decoded_bytes_per_packet;

	return SWITCH_STATUS_SUCCESS;
#endif
}

/* Registration */
SWITCH_MODULE_LOAD_FUNCTION(mod_amr_load)
{
	switch_codec_interface_t *codec_interface;
#ifndef AMR_PASSTHROUGH
	char *cf = "amr.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));
	globals.default_bitrate = AMR_DEFAULT_BITRATE;

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

	SWITCH_ADD_CODEC(codec_interface, "AMR");
#ifndef AMR_PASSTHROUGH
	codec_interface->parse_fmtp = switch_amr_fmtp_parse;
#endif 
	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 96,	/* the IANA code number */
										 "AMR",	/* the IANA code name */
										 "octet-align=0",	/* default fmtp to send (can be overridden by the init function) */
										 8000,	/* samples transferred per second */
										 8000,	/* actual samples transferred per second */
										 12200,	/* bits transferred per second */
										 20000,	/* number of microseconds per frame */
										 160,	/* number of samples per frame */
										 320,	/* number of bytes per frame decompressed */
										 0,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_amr_init,	/* function to initialize a codec handle using this implementation */
										 switch_amr_encode,	/* function to encode raw data into encoded data */
										 switch_amr_decode,	/* function to decode encoded data into raw data */
										 switch_amr_destroy);	/* deinitalize a codec handle using this implementation */

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
