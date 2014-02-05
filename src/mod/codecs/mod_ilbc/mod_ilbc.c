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
 *
 * mod_ilbc.c -- ilbc Codec Module
 *
 */

#include "switch.h"
#include "ilbc.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_ilbc_load);
SWITCH_MODULE_DEFINITION(mod_ilbc, mod_ilbc_load, NULL, NULL);

struct ilbc_context {
	ilbc_encode_state_t encoder_object;
	ilbc_decode_state_t decoder_object;
};

static switch_status_t switch_ilbc_fmtp_parse(const char *fmtp, switch_codec_fmtp_t *codec_fmtp)
{
	if (codec_fmtp) {
		char *mode = NULL;
		int codec_ms = 0;

		memset(codec_fmtp, '\0', sizeof(struct switch_codec_fmtp));

		if (fmtp && (mode = strstr(fmtp, "mode=")) && (mode + 5)) {
			codec_ms = atoi(mode + 5);
		}
		if (!codec_ms) {
			/* default to 30 when no mode is defined for ilbc ONLY */
			codec_ms = 30;
		}
		codec_fmtp->microseconds_per_packet = (codec_ms * 1000);
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_ilbc_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct ilbc_context *context;
	int encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (flags & SWITCH_CODEC_FLAG_DECODE);
	int mode = codec->implementation->microseconds_per_packet / 1000;

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}

	codec->fmtp_out = switch_core_sprintf(codec->memory_pool, "mode=%d", mode);

	if (encoding) {
		ilbc_encode_init(&context->encoder_object, mode);
	}

	if (decoding) {
		ilbc_decode_init(&context->decoder_object, mode, 0);
	}

	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_ilbc_destroy(switch_codec_t *codec)
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_ilbc_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct ilbc_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = ilbc_encode(&context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_ilbc_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct ilbc_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * ilbc_decode(&context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_ilbc_load)
{
	switch_codec_interface_t *codec_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "iLBC");
	codec_interface->parse_fmtp = switch_ilbc_fmtp_parse;

	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 98,	/* the IANA code number */
										 "iLBC",	/* the IANA code name */
										 "mode=20",	/* default fmtp to send (can be overridden by the init function) */
										 8000,	/* samples transferred per second */
										 8000,	/* actual samples transferred per second */
										 15200,	/* bits transferred per second */
										 20000,	/* number of microseconds per frame */
										 ILBC_BLOCK_LEN_20MS,	/* number of samples per frame */
										 ILBC_BLOCK_LEN_20MS * 2,	/* number of bytes per frame decompressed */
										 ILBC_NO_OF_BYTES_20MS,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_ilbc_init,	/* function to initialize a codec handle using this implementation */
										 switch_ilbc_encode,	/* function to encode raw data into encoded data */
										 switch_ilbc_decode,	/* function to decode encoded data into raw data */
										 switch_ilbc_destroy);	/* deinitalize a codec handle using this implementation */

	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 97,	/* the IANA code number */
										 "iLBC",	/* the IANA code name */
										 "mode=30",	/* default fmtp to send (can be overridden by the init function) */
										 8000,	/* samples transferred per second */
										 8000,	/* actual samples transferred per second */
										 13330,	/* bits transferred per second */
										 30000,	/* number of microseconds per frame */
										 ILBC_BLOCK_LEN_30MS,	/* number of samples per frame */
										 ILBC_BLOCK_LEN_30MS * 2,	/* number of bytes per frame decompressed */
										 ILBC_NO_OF_BYTES_30MS,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_ilbc_init,	/* function to initialize a codec handle using this implementation */
										 switch_ilbc_encode,	/* function to encode raw data into encoded data */
										 switch_ilbc_decode,	/* function to decode encoded data into raw data */
										 switch_ilbc_destroy);	/* deinitalize a codec handle using this implementation */

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
