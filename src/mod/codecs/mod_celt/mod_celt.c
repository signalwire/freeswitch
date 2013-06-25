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
 * Brian K. West <brian@freeswitch.org>
 *
 * mod_celt.c -- The CELT ultra-low delay audio codec (http://www.celt-codec.org/)
 *
 */

#include "switch.h"
#include "celt.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_celt_load);
SWITCH_MODULE_DEFINITION(mod_celt, mod_celt_load, NULL, NULL);

struct celt_context {
	CELTEncoder *encoder_object;
	CELTDecoder *decoder_object;
	CELTMode *mode_object;
	int frame_size;
	int bytes_per_packet;
};

static switch_status_t switch_celt_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct celt_context *context = NULL;
	int encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}

	context->frame_size = codec->implementation->samples_per_packet;
	context->mode_object = celt_mode_create(codec->implementation->actual_samples_per_second, context->frame_size, NULL);
	context->bytes_per_packet = (codec->implementation->bits_per_second * context->frame_size / codec->implementation->actual_samples_per_second + 4) / 8;

	/*
	   if (codec->fmtp_in) {
	   int x, argc;
	   char *argv[10];
	   argc = switch_separate_string(codec->fmtp_in, ';', argv, (sizeof(argv) / sizeof(argv[0])));
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
	   }

	   codec->fmtp_out = switch_core_sprintf(codec->memory_pool, "bitrate=%d", bit_rate);
	 */
	if (encoding) {
		context->encoder_object = celt_encoder_create(context->mode_object, 1, NULL);
	}

	if (decoding) {
		context->decoder_object = celt_decoder_create(context->mode_object, 1, NULL);
	}

	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_celt_destroy(switch_codec_t *codec)
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_celt_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct celt_context *context = codec->private_info;
	int bytes = 0;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	bytes = (uint32_t) celt_encode(context->encoder_object, (void *) decoded_data, codec->implementation->samples_per_packet,
								   (unsigned char *) encoded_data, context->bytes_per_packet);

	if (bytes > 0) {
		*encoded_data_len = (uint32_t) bytes;
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoder Error!\n");
	return SWITCH_STATUS_GENERR;
}

static switch_status_t switch_celt_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct celt_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (celt_decode(context->decoder_object, encoded_data, encoded_data_len, decoded_data, codec->implementation->samples_per_packet)) {
		return SWITCH_STATUS_GENERR;
	}

	*decoded_data_len = codec->implementation->decoded_bytes_per_packet;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_celt_load)
{
	switch_codec_interface_t *codec_interface;
	int bytes_per_frame;
	int samples_per_frame;
	int ms_per_frame;
	int x;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "CELT ultra-low delay");

	ms_per_frame = 2000;
	samples_per_frame = 96;
	bytes_per_frame = 192;

	for (x = 0; x < 5; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 114,	/* the IANA code number */
											 "CELT",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 48000,	/* actual samples transferred per second */
											 48000,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_celt_init,	/* function to initialize a codec handle using this implementation */
											 switch_celt_encode,	/* function to encode raw data into encoded data */
											 switch_celt_decode,	/* function to decode encoded data into raw data */
											 switch_celt_destroy);	/* deinitalize a codec handle using this implementation */
		ms_per_frame += 2000;
		samples_per_frame += 96;
		bytes_per_frame += 192;
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
