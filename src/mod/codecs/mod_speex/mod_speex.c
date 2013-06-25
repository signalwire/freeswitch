/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Christopher M. Rienzo <chris@rienzo.com>
 *
 *
 * mod_speex.c -- Speex Codec Module
 *
 */

#include <switch.h>
#include <speex/speex.h>
#include <speex/speex_preprocess.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_speex_load);
SWITCH_MODULE_DEFINITION(mod_speex, mod_speex_load, NULL, NULL);

/* nobody has more setting than speex so we will let them set the standard */
/*! \brief Various codec settings (currently only relevant to speex) */
struct speex_codec_settings {
	/*! desired quality */
	int quality;
	/*! desired complexity */
	int complexity;
	/*! desired enhancement */
	int enhancement;
	/*! desired vad level */
	int vad;
	/*! desired vbr level */
	int vbr;
	/*! desired vbr quality */
	float vbr_quality;
	/*! desired abr level */
	int abr;
	/*! desired dtx setting */
	int dtx;
	/*! desired preprocessor settings */
	int preproc;
	/*! preprocessor vad settings */
	int pp_vad;
	/*! preprocessor gain control settings */
	int pp_agc;
	/*! preprocessor gain level */
	float pp_agc_level;
	/*! preprocessor denoise level */
	int pp_denoise;
	/*! preprocessor dereverb settings */
	int pp_dereverb;
	/*! preprocessor dereverb decay level */
	float pp_dereverb_decay;
	/*! preprocessor dereverb level */
	float pp_dereverb_level;
};

typedef struct speex_codec_settings speex_codec_settings_t;

static speex_codec_settings_t default_codec_settings = {
	/*.quality */ 5,
	/*.complexity */ 5,
	/*.enhancement */ 1,
	/*.vad */ 0,
	/*.vbr */ 0,
	/*.vbr_quality */ 4,
	/*.abr */ 0,
	/*.dtx */ 0,
	/*.preproc */ 0,
	/*.pp_vad */ 0,
	/*.pp_agc */ 0,
	/*.pp_agc_level */ 8000,
	/*.pp_denoise */ 0,
	/*.pp_dereverb */ 0,
	/*.pp_dereverb_decay */ 0.4f,
	/*.pp_dereverb_level */ 0.3f,
};

struct speex_context {
	switch_codec_t *codec;
	speex_codec_settings_t codec_settings; 
	unsigned int flags;

	/* Encoder */
	void *encoder_state;
	struct SpeexBits encoder_bits;
	unsigned int encoder_frame_size;
	int encoder_mode;
	SpeexPreprocessState *pp;

	/* Decoder */
	void *decoder_state;
	struct SpeexBits decoder_bits;
	unsigned int decoder_frame_size;
	int decoder_mode;
};

static switch_status_t switch_speex_fmtp_parse(const char *fmtp, switch_codec_fmtp_t *codec_fmtp)
{
	speex_codec_settings_t *codec_settings = NULL;
	int x, argc;
	char *argv[10];
	char *fmtp_dup = NULL;

	if (!codec_fmtp) {
		return SWITCH_STATUS_FALSE;
	}

	/* load default settings */
	if (codec_fmtp->private_info) {
		codec_settings = codec_fmtp->private_info;
		memcpy(codec_settings, &default_codec_settings, sizeof(*codec_settings));
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "codec_fmtp->private_info is NULL\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!fmtp) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "got fmtp: %s\n", fmtp);

	fmtp_dup = strdup(fmtp);
	switch_assert(fmtp_dup);

	/* parse ; separated fmtp args */
	argc = switch_separate_string(fmtp_dup, ';', argv, (sizeof(argv) / sizeof(argv[0])));
	for (x = 0; x < argc; x++) {
		char *data = argv[x];
		char *arg;
		switch_assert(data);
		while (*data == ' ') {
			data++;
		}
		if (!(arg = strchr(data, '='))) {
			continue;
		}
		*arg++ = '\0';
		if (zstr(arg)) {
			continue;
		}

		if (!strcasecmp("vbr", data)) {
			/* vbr can be on/off/vad */
			if (!strcasecmp("vad", arg)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "enabling speex vbr=vad\n");
				codec_settings->vbr = 0;
				codec_settings->vad = 1;
				codec_settings->pp_vad = 1;
			} else {
				if (switch_true(arg)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "enabling speex vbr\n");
					codec_settings->vbr = 1;
					codec_settings->vad = 0;
					codec_settings->pp_vad = 1;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "disabling speex vbr\n");
					codec_settings->vbr = 0;
					codec_settings->vad = 0;
					codec_settings->pp_vad = 0;
				}
			}
		} else if (!strcasecmp("cng", data)) {
			/* TODO don't know how to turn on CNG */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "speex cng is unsupported\n");
		} else if (!strcasecmp("mode", data)) {
			/* mode is a comma-separate list of preferred modes.  Use the first mode in the list */
			char *arg_dup;
			char *mode[2];
			if (!strncasecmp("any", arg, 3)) {
				/* "any", keep the default setting */
				continue;
			}
			arg_dup = strdup(arg);
			if (switch_separate_string(arg_dup, ',', mode, (sizeof(mode) / sizeof(mode[0])))) {
				int mode_num = -1;
				char *mode_str = mode[0];
				if (mode_str[0] == '"') {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "mode starts with \"\n");
					mode_str++;
				}
				if (switch_is_number(mode_str)) {
					mode_num = atoi(mode_str);
				}
				/* TODO there might be a way to set the mode directly instead of changing the quality */
				if (codec_fmtp->actual_samples_per_second == 8000) {
					switch (mode_num) {
					case 1:
						codec_settings->quality = 0;
						break;
					case 2:
						codec_settings->quality = 2;
						break;
					case 3:
						codec_settings->quality = 4;
						break;
					case 4:
						codec_settings->quality = 6;
						break;
					case 5:
						codec_settings->quality = 8;
						break;
					case 6:
						codec_settings->quality = 9;
						break;
					case 7:
						codec_settings->quality = 10;
						break;
					case 8:
						codec_settings->quality = 1;
						break;
					default:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "ignoring invalid speex/8000 mode %s\n", mode_str);
						continue;
					}
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "choosing speex/8000 mode %s\n", mode_str);
					codec_settings->quality = codec_settings->quality;
					codec_settings->vbr_quality = codec_settings->quality;
				} else {
					if (mode_num >= 0 && mode_num <= 10) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "choosing speex/%d mode %s\n", codec_fmtp->actual_samples_per_second, mode_str);
						codec_settings->quality = mode_num;
						codec_settings->vbr_quality = mode_num;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "ignoring invalid speex/%d mode %s\n", codec_fmtp->actual_samples_per_second, mode_str);
						continue;
					}
				}
			}
			free(arg_dup);
		}
	}
	free(fmtp_dup);
	/*codec_fmtp->bits_per_second = bit_rate;*/
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t switch_speex_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct speex_context *context = NULL;
	int encoding, decoding;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || ((context = switch_core_alloc(codec->memory_pool, sizeof(*context))) == 0)) {
		return SWITCH_STATUS_FALSE;
	} else {
		const SpeexMode *mode = NULL;
		switch_codec_fmtp_t codec_fmtp;
		speex_codec_settings_t codec_settings;

		memset(&codec_fmtp, '\0', sizeof(struct switch_codec_fmtp));
		codec_fmtp.private_info = &codec_settings;
		codec_fmtp.actual_samples_per_second = codec->implementation->actual_samples_per_second;
		switch_speex_fmtp_parse(codec->fmtp_in, &codec_fmtp);

		memcpy(&context->codec_settings, &codec_settings, sizeof(context->codec_settings));

		context->codec = codec;
		if (codec->implementation->actual_samples_per_second == 8000) {
			mode = &speex_nb_mode;
		} else if (codec->implementation->actual_samples_per_second == 16000) {
			mode = &speex_wb_mode;
		} else if (codec->implementation->actual_samples_per_second == 32000) {
			mode = &speex_uwb_mode;
		}

		if (!mode) {
			return SWITCH_STATUS_FALSE;
		}

		if (encoding) {
			speex_bits_init(&context->encoder_bits);
			context->encoder_state = speex_encoder_init(mode);
			speex_encoder_ctl(context->encoder_state, SPEEX_GET_FRAME_SIZE, &context->encoder_frame_size);
			speex_encoder_ctl(context->encoder_state, SPEEX_SET_COMPLEXITY, &context->codec_settings.complexity);
			if (context->codec_settings.preproc) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "preprocessor on\n");
				context->pp = speex_preprocess_state_init(context->encoder_frame_size, codec->implementation->actual_samples_per_second);
				if (context->codec_settings.pp_vad) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "preprocessor vad on\n");
				}
				speex_preprocess_ctl(context->pp, SPEEX_PREPROCESS_SET_VAD, &context->codec_settings.pp_vad);
				if (context->codec_settings.pp_agc) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "preprocessor agc on\n");
				}
				speex_preprocess_ctl(context->pp, SPEEX_PREPROCESS_SET_AGC, &context->codec_settings.pp_agc);
				speex_preprocess_ctl(context->pp, SPEEX_PREPROCESS_SET_AGC_LEVEL, &context->codec_settings.pp_agc_level);
				if (context->codec_settings.pp_denoise) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "preprocessor denoise on\n");
				}
				speex_preprocess_ctl(context->pp, SPEEX_PREPROCESS_SET_DENOISE, &context->codec_settings.pp_denoise);
				if (context->codec_settings.pp_dereverb) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "preprocessor dereverb on\n");
				}
				speex_preprocess_ctl(context->pp, SPEEX_PREPROCESS_SET_DEREVERB, &context->codec_settings.pp_dereverb);
				speex_preprocess_ctl(context->pp, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &context->codec_settings.pp_dereverb_decay);
				speex_preprocess_ctl(context->pp, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &context->codec_settings.pp_dereverb_level);
			}

			if (!context->codec_settings.abr && !context->codec_settings.vbr) {
				speex_encoder_ctl(context->encoder_state, SPEEX_SET_QUALITY, &context->codec_settings.quality);
				if (context->codec_settings.vad) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "vad on\n");
					speex_encoder_ctl(context->encoder_state, SPEEX_SET_VAD, &context->codec_settings.vad);
				}
			}
			if (context->codec_settings.vbr) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "vbr on\n");
				speex_encoder_ctl(context->encoder_state, SPEEX_SET_VBR, &context->codec_settings.vbr);
				speex_encoder_ctl(context->encoder_state, SPEEX_SET_VBR_QUALITY, &context->codec_settings.vbr_quality);
			}
			if (context->codec_settings.abr) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "abr on\n");
				speex_encoder_ctl(context->encoder_state, SPEEX_SET_ABR, &context->codec_settings.abr);
			}
			if (context->codec_settings.dtx) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "dtx on\n");
				speex_encoder_ctl(context->encoder_state, SPEEX_SET_DTX, &context->codec_settings.dtx);
			}
		}

		if (decoding) {
			speex_bits_init(&context->decoder_bits);
			context->decoder_state = speex_decoder_init(mode);
			if (context->codec_settings.enhancement) {
				speex_decoder_ctl(context->decoder_state, SPEEX_SET_ENH, &context->codec_settings.enhancement);
			}
		}



		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "initialized Speex codec \n");
		codec->private_info = context;
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_speex_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										   unsigned int *flag)
{
	struct speex_context *context = codec->private_info;
	short *buf;
	int is_speech = 1;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	buf = decoded_data;

	if (context->pp) {
		is_speech = speex_preprocess(context->pp, buf, NULL);
	}

	if (is_speech) {
		is_speech = speex_encode_int(context->encoder_state, buf, &context->encoder_bits)
			|| !context->codec_settings.dtx;
	} else {
		speex_bits_pack(&context->encoder_bits, 0, 5);
	}


	if (is_speech) {
		switch_clear_flag(context, SWITCH_CODEC_FLAG_SILENCE);
		*flag &= ~SFF_CNG;
	} else {
		if (switch_test_flag(context, SWITCH_CODEC_FLAG_SILENCE)) {
			*encoded_data_len = 0;
			*flag |= SFF_CNG;
			return SWITCH_STATUS_SUCCESS;
		}

		switch_set_flag(context, SWITCH_CODEC_FLAG_SILENCE);
	}


	speex_bits_pack(&context->encoder_bits, 15, 5);
	*encoded_data_len = speex_bits_write(&context->encoder_bits, (char *) encoded_data, context->encoder_frame_size);
	speex_bits_reset(&context->encoder_bits);
	(*encoded_data_len)--;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_speex_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
	struct speex_context *context = codec->private_info;
	short *buf;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}


	buf = decoded_data;
	if (*flag & SWITCH_CODEC_FLAG_SILENCE) {
		speex_decode_int(context->decoder_state, NULL, buf);
	} else {
		speex_bits_read_from(&context->decoder_bits, (char *) encoded_data, (int) encoded_data_len);
		speex_decode_int(context->decoder_state, &context->decoder_bits, buf);
	}
	*decoded_data_len = codec->implementation->decoded_bytes_per_packet;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_speex_destroy(switch_codec_t *codec)
{
	int encoding, decoding;
	struct speex_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	encoding = (codec->flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (codec->flags & SWITCH_CODEC_FLAG_DECODE);

	if (encoding) {
		speex_bits_destroy(&context->encoder_bits);
		speex_encoder_destroy(context->encoder_state);
	}

	if (decoding) {
		speex_bits_destroy(&context->decoder_bits);
		speex_decoder_destroy(context->decoder_state);
	}

	codec->private_info = NULL;

	return SWITCH_STATUS_SUCCESS;
}

/**
 * read default settings from speex.conf
 */
static void load_configuration()
{
	switch_xml_t xml = NULL, cfg = NULL;

	if ((xml = switch_xml_open_cfg("speex.conf", &cfg, NULL))) {
		switch_xml_t x_lists;
		if ((x_lists = switch_xml_child(cfg, "settings"))) {
			const char *settings_name = switch_xml_attr(x_lists, "name");
			switch_xml_t x_list;
			if (zstr(settings_name)) {
				settings_name = "";
			}
			for (x_list = switch_xml_child(x_lists, "param"); x_list; x_list = x_list->next) {
				const char *name = switch_xml_attr(x_list, "name");
				const char *value = switch_xml_attr(x_list, "value");
				if (zstr(name)) {
					continue;
				}

				if (zstr(value)) {
					continue;
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s %s = %s\n", settings_name, name, value);

				if (!strcasecmp("quality", name)) {
					/* compression quality, integer 0-10 */
					int tmp = atoi(value);
					if (switch_is_number(value) && tmp >= 0 && tmp <= 10) {
						default_codec_settings.quality = tmp;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ignoring invalid quality value: %s\n", value);
					}
				} else if (!strcasecmp("complexity", name)) {
					/* compression complexity, integer 1-10 */
					int tmp = atoi(value);
					if (switch_is_number(value) && tmp >= 1 && tmp <= 10) {
						default_codec_settings.complexity = tmp;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ignoring invalid complexity value: %s\n", value);
					}
				} else if (!strcasecmp("enhancement", name)) {
					/* enable perceptual enhancement, boolean */
					default_codec_settings.enhancement = switch_true(value);
				} else if (!strcasecmp("vad", name)) {
					/* enable voice activity detection, boolean */
					default_codec_settings.vad = switch_true(value);
				} else if (!strcasecmp("vbr", name)) {
					/* enable variable bit rate, boolean */
					default_codec_settings.vbr = switch_true(value);
				} else if (!strcasecmp("vbr-quality", name)) {
					/* variable bit rate quality, float 0-10 */
					float tmp = atof(value);
					if (switch_is_number(value) && tmp >= 0 && tmp <= 10) {
						default_codec_settings.vbr_quality = tmp;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ignoring invalid vbr-quality value: %s\n", value);
					}
				} else if (!strcasecmp("abr", name)) {
					/* average bit rate, integer bits per sec */
					int tmp = atoi(value);
					if (switch_is_number(value) && tmp >= 0) {
						default_codec_settings.abr = tmp;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ignoring invalid abr value: %s\n", value);
					}
				} else if (!strcasecmp("dtx", name)) {
					/* discontinuous transmit, boolean */
					default_codec_settings.dtx = switch_true(value);
				} else if (!strcasecmp("preproc", name)) {
					/* enable preprocessor, boolean */
					default_codec_settings.preproc = switch_true(value);
				} else if (!strcasecmp("pp-vad", name)) {
					/* enable preprocessor VAD, boolean */
					default_codec_settings.pp_vad = switch_true(value);
				} else if (!strcasecmp("pp-agc", name)) {
					/* enable preprocessor automatic gain control, boolean */
					default_codec_settings.pp_agc = switch_true(value);
				} else if (!strcasecmp("pp-agc-level", name)) {
					/* agc level, float */
					float tmp = atof(value);
					if (switch_is_number(value) && tmp >= 0.0f) {
						default_codec_settings.pp_agc_level = tmp;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ignoring invalid pp-agc-level value: %s\n", value);
					}
				} else if (!strcasecmp("pp-denoise", name)) {
					/* enable preprocessor denoiser, boolean */
					default_codec_settings.pp_denoise = switch_true(value);
				} else if (!strcasecmp("pp-dereverb", name)) {
					/* enable preprocessor reverberation removal, boolean */
					default_codec_settings.pp_dereverb = switch_true(value);
				} else if (!strcasecmp("pp-dereverb-decay", name)) {
					/* reverberation removal decay, float */
					float tmp = atof(value);
					if (switch_is_number(value) && tmp >= 0.0f) {
						default_codec_settings.pp_dereverb_decay = tmp;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ignoring invalid pp-dereverb-decay value: %s\n", value);
					}
				} else if (!strcasecmp("pp-dereverb-level", name)) {
					/* reverberation removal level, float */
					float tmp = atof(value);
					if (switch_is_number(value) && tmp >= 0.0f) {
						default_codec_settings.pp_dereverb_level = tmp;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ignoring invalid pp-dereverb-level value: %s\n", value);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ignoring invalid unknown param: %s = %s\n", name, value);
				}
			}
		}
		switch_xml_free(xml);
	}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_speex_load)
{
	switch_codec_interface_t *codec_interface;
	int mpf = 20000, spf = 160, bpf = 320, rate = 8000, counta, countb;
	switch_payload_t ianacode[4] = { 0, 99, 99, 99 };
	int bps[4] = { 0, 24600, 42200, 44000 };
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	load_configuration();

	SWITCH_ADD_CODEC(codec_interface, "Speex");
	codec_interface->parse_fmtp = switch_speex_fmtp_parse;
	for (counta = 1; counta <= 3; counta++) {
		for (countb = 1; countb > 0; countb--) {
			switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
												 ianacode[counta],	/* the IANA code number */
												 "SPEEX",	/* the IANA code name */
												 NULL,	/* default fmtp to send (can be overridden by the init function) */
												 rate,	/* samples transferred per second */
												 rate,	/* actual samples transferred per second */
												 bps[counta],	/* bits transferred per second */
												 mpf * countb,	/* number of microseconds per frame */
												 spf * countb,	/* number of samples per frame */
												 bpf * countb,	/* number of bytes per frame decompressed */
												 0,	/* number of bytes per frame compressed */
												 1,	/* number of channels represented */
												 1,	/* number of frames per network packet */
												 switch_speex_init,	/* function to initialize a codec handle using this implementation */
												 switch_speex_encode,	/* function to encode raw data into encoded data */
												 switch_speex_decode,	/* function to decode encoded data into raw data */
												 switch_speex_destroy);	/* deinitalize a codec handle using this implementation */
		}
		rate = rate * 2;
		spf = spf * 2;
		bpf = bpf * 2;
	}




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
