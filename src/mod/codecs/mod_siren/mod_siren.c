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
 * Brian K. West <brian@freeswitch.org>
 * Steve Underwood <steveu@coppice.org>
 *
 * mod_siren.c -- ITU-T G.722.1 (Siren7) and ITU-T G.722.1 Annex C (Siren14) licensed from Polycom(R)
 *
 * Using mod_siren in a commercial product will require you to acquire a patent 
 * license directly from Polycom(R) for your company.
 *
 * http://www.polycom.com/usa/en/company/about_us/technology/siren_g7221/siren_g7221.html
 * http://www.polycom.com/usa/en/company/about_us/technology/siren14_g7221c/siren14_g7221c.html
 *
 */

#include "switch.h"
#include "g722_1.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_siren_load);
SWITCH_MODULE_DEFINITION(mod_siren, mod_siren_load, NULL, NULL);

struct siren_context {
	g722_1_decode_state_t decoder_object;
	g722_1_encode_state_t encoder_object;
};

static switch_status_t switch_siren_fmtp_parse(const char *fmtp, switch_codec_fmtp_t *codec_fmtp)
{
	if (codec_fmtp) {
		int bit_rate = 0;
		memset(codec_fmtp, '\0', sizeof(struct switch_codec_fmtp));
		if (fmtp) {
			int x, argc;
			char *argv[10];
			char *fmtp_dup = strdup(fmtp);

			switch_assert(fmtp_dup);
			argc = switch_separate_string(fmtp_dup, ';', argv, (sizeof(argv) / sizeof(argv[0])));
			for (x = 0; x < argc; x++) {
				char *data = argv[x];
				char *arg;
				switch_assert(data);
				while (*data == ' ') {
					data++;
				}
				if ((arg = strchr(data, '='))) {
					*arg++ = '\0';
					if (!strcasecmp(data, "bitrate")) {
						bit_rate = atoi(arg);
					}
				}
			}
			free(fmtp_dup);
		}
		codec_fmtp->bits_per_second = bit_rate;
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_siren_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct siren_context *context = NULL;
	int encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (flags & SWITCH_CODEC_FLAG_DECODE);
	int bit_rate = codec->implementation->bits_per_second;

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}

	codec->fmtp_out = switch_core_sprintf(codec->memory_pool, "bitrate=%d", bit_rate);

	if (encoding) {
		g722_1_encode_init(&context->encoder_object, bit_rate, codec->implementation->samples_per_second);
	}

	if (decoding) {
		g722_1_decode_init(&context->decoder_object, bit_rate, codec->implementation->samples_per_second);
	}

	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_siren_destroy(switch_codec_t *codec)
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_siren_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										   unsigned int *flag)
{
	struct siren_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = g722_1_encode(&context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / sizeof(int16_t));
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_siren_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
	struct siren_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = g722_1_decode(&context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len) * sizeof(int16_t);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_siren_load)
{
	switch_codec_interface_t *codec_interface;
	int mpf = 20000, spf = 320, bpf = 640, count;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Audio coding: ITU-T Rec. G.722.1, licensed from Polycom(R)\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Audio coding: ITU-T Rec. G.722.1 Annex C, licensed from Polycom(R)\n");

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "Polycom(R) G722.1/G722.1C");
	codec_interface->parse_fmtp = switch_siren_fmtp_parse;

	spf = 320, bpf = 640;
	for (count = 3; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,    /* enumeration defining the type of the codec */
				107,   /* the IANA code number */
				"G7221",       /* the IANA code name */
				"bitrate=24000",       /* default fmtp to send (can be overridden by the init function) */
				16000, /* samples transferred per second */
				16000, /* actual samples transferred per second */
				24000, /* bits transferred per second */
				mpf * count,   /* number of microseconds per frame */
				spf * count,   /* number of samples per frame */
				bpf * count,   /* number of bytes per frame decompressed */
				0,     /* number of bytes per frame compressed */
				1,     /* number of channels represented */
				1,     /* number of frames per network packet */
				switch_siren_init,     /* function to initialize a codec handle using this implementation */
				switch_siren_encode,   /* function to encode raw data into encoded data */
				switch_siren_decode,   /* function to decode encoded data into raw data */
				switch_siren_destroy); /* deinitalize a codec handle using this implementation */
	}

	spf = 320, bpf = 640;
	for (count = 3; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 107,	/* the IANA code number */
											 "G7221",	/* the IANA code name */
											 "bitrate=32000",	/* default fmtp to send (can be overridden by the init function) */
											 16000,	/* samples transferred per second */
											 16000,	/* actual samples transferred per second */
											 32000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_siren_init,	/* function to initialize a codec handle using this implementation */
											 switch_siren_encode,	/* function to encode raw data into encoded data */
											 switch_siren_decode,	/* function to decode encoded data into raw data */
											 switch_siren_destroy);	/* deinitalize a codec handle using this implementation */
	}
	spf = 640, bpf = 1280;
	for (count = 3; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 115,	/* the IANA code number */
											 "G7221",	/* the IANA code name */
											 "bitrate=48000",	/* default fmtp to send (can be overridden by the init function) */
											 32000,	/* samples transferred per second */
											 32000,	/* actual samples transferred per second */
											 48000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_siren_init,	/* function to initialize a codec handle using this implementation */
											 switch_siren_encode,	/* function to encode raw data into encoded data */
											 switch_siren_decode,	/* function to decode encoded data into raw data */
											 switch_siren_destroy);	/* deinitalize a codec handle using this implementation */

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
