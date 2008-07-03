/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Michael Jerris <mike@jerris.com>
 *
 * mod_ilbc.c -- ilbc Codec Module
 *
 */

#include "switch.h"
#include "iLBC_encode.h"
#include "iLBC_decode.h"
#include "iLBC_define.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_ilbc_load);
SWITCH_MODULE_DEFINITION(mod_ilbc, mod_ilbc_load, NULL, NULL);

struct ilbc_context {
	iLBC_Enc_Inst_t encoder;
	iLBC_Dec_Inst_t decoder;
	uint8_t ms;
	uint16_t bytes;
	uint16_t dbytes;
};

static switch_status_t switch_ilbc_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct ilbc_context *context;
	int encoding, decoding;
	uint8_t ms = (uint8_t) (codec->implementation->microseconds_per_frame / 1000);

	if (ms != 20 && ms != 30) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid speed! (I should never happen)\n");
		return SWITCH_STATUS_FALSE;
	}

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	} else {
		context = switch_core_alloc(codec->memory_pool, sizeof(*context));
		context->ms = ms;
		if (context->ms == 20) {
			context->bytes = NO_OF_BYTES_20MS;
			context->dbytes = 320;
		} else {
			context->bytes = NO_OF_BYTES_30MS;
			context->dbytes = 480;
		}

		if (encoding) {
			initEncode(&context->encoder, context->ms);
		}
		if (decoding) {
			initDecode(&context->decoder, context->ms, 1);
		}
	}

	codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->implementation->fmtp);

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
	if (decoded_data_len % context->dbytes == 0) {
		unsigned int new_len = 0;
		unsigned char *edp = encoded_data;
		short *ddp = decoded_data;
		int x;
		uint16_t y;
		int loops = (int) decoded_data_len / context->dbytes;
		float buf[240];

		for (x = 0; x < loops && new_len < *encoded_data_len; x++) {
			for (y = 0; y < context->dbytes / sizeof(short) && y < 240; y++) {
				buf[y] = ddp[y];
			}
			iLBC_encode(edp, buf, &context->encoder);
			edp += context->bytes;
			ddp += context->dbytes;
			new_len += context->bytes;
		}
		if (new_len <= *encoded_data_len) {
			*encoded_data_len = new_len;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer overflow!!! %u >= %u\n", new_len, *encoded_data_len);
			return SWITCH_STATUS_FALSE;
		}
	}
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

	if (encoded_data_len % context->bytes == 0) {
		int loops = (int) encoded_data_len / context->bytes;
		unsigned char *edp = encoded_data;
		short *ddp = decoded_data;
		int x;
		uint16_t y;
		unsigned int new_len = 0;
		float buf[240];

		for (x = 0; x < loops && new_len < *decoded_data_len; x++) {
			iLBC_decode(buf, edp, &context->decoder, 1);
			for (y = 0; y < context->dbytes / sizeof(short) && y < 240; y++) {
				ddp[y] = (short) buf[y];
			}
			ddp += context->dbytes / sizeof(short);
			edp += context->bytes;
			new_len += context->dbytes;
		}
		if (new_len <= *decoded_data_len) {
			*decoded_data_len = new_len;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer overflow!!!\n");
			return SWITCH_STATUS_FALSE;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "yo this frame is an odd size [%d]\n", encoded_data_len);
		return SWITCH_STATUS_FALSE;
	}
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_ilbc_load)
{
	switch_codec_interface_t *codec_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "iLBC");

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 97, "iLBC", "mode=20", 8000, 8000, NO_OF_BYTES_20MS * 8 * 8000 / BLOCKL_20MS,
										 20000, 160, 320, NO_OF_BYTES_20MS, 1, 1, 1,
										 switch_ilbc_init, switch_ilbc_encode, switch_ilbc_decode, switch_ilbc_destroy);
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 102, "iLBC", "mode=20", 8000, 8000, NO_OF_BYTES_20MS * 8 * 8000 / BLOCKL_20MS,
										 20000, 160, 320, NO_OF_BYTES_20MS, 1, 1, 1,
										 switch_ilbc_init, switch_ilbc_encode, switch_ilbc_decode, switch_ilbc_destroy);
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 102, "iLBC102", "mode=20", 8000, 8000, NO_OF_BYTES_20MS * 8 * 8000 / BLOCKL_20MS,
										 20000, 160, 320, NO_OF_BYTES_20MS, 1, 1, 1,
										 switch_ilbc_init, switch_ilbc_encode, switch_ilbc_decode, switch_ilbc_destroy);
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 102, "iLBC20ms", "mode=20", 8000, 8000, NO_OF_BYTES_20MS * 8 * 8000 / BLOCKL_20MS,
										 20000, 160, 320, NO_OF_BYTES_20MS, 1, 1, 1,
										 switch_ilbc_init, switch_ilbc_encode, switch_ilbc_decode, switch_ilbc_destroy);
	/* 30ms variants */
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 98, "iLBC", "mode=30", 8000, 8000, NO_OF_BYTES_30MS * 8 * 8000 / BLOCKL_30MS,
										 30000, 240, 480, NO_OF_BYTES_30MS, 1, 1, 1,
										 switch_ilbc_init, switch_ilbc_encode, switch_ilbc_decode, switch_ilbc_destroy);
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 102, "iLBC", "mode=30", 8000, 8000, NO_OF_BYTES_30MS * 8 * 8000 / BLOCKL_30MS,
										 30000, 240, 480, NO_OF_BYTES_30MS, 1, 1, 1,
										 switch_ilbc_init, switch_ilbc_encode, switch_ilbc_decode, switch_ilbc_destroy);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
