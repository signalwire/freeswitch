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
 * mod_bv.c -- BroadVoice16 and BroadVoice32 audio codecs (http://www.broadcom.com/support/broadvoice/)
 *
 */

#include "switch.h"
#include "broadvoice.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_bv_load);
SWITCH_MODULE_DEFINITION(mod_bv, mod_bv_load, NULL, NULL);

#define FRAME_SIZE 40
#define CODE_SIZE 5

struct bv16_context {
	bv16_decode_state_t *decoder_object;
	bv16_encode_state_t *encoder_object;
};

static switch_status_t switch_bv16_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct bv16_context *context = NULL;
	int encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}

	if (encoding) {
		context->encoder_object = bv16_encode_init(NULL);
	}

	if (decoding) {
		context->decoder_object = bv16_decode_init(NULL);
	}

	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_bv16_destroy(switch_codec_t *codec)
{
	struct bv16_context *context = codec->private_info;

	if (context->encoder_object) {
		bv16_encode_free(context->encoder_object);
	}

	if (context->decoder_object) {
		bv16_decode_free(context->decoder_object);
	}

	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_bv16_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct bv16_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = bv16_encode(context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_bv16_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct bv16_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if ((*flag & SFF_PLC)) {
		*decoded_data_len = (2 * bv16_fillin(context->decoder_object, (int16_t *) decoded_data, encoded_data_len));
	} else {
		*decoded_data_len = (2 * bv16_decode(context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));
	}

	return SWITCH_STATUS_SUCCESS;
}

struct bv32_context {
	bv32_decode_state_t *decoder_object;
	bv32_encode_state_t *encoder_object;
};

static switch_status_t switch_bv32_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct bv32_context *context = NULL;
	int encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}

	if (encoding) {
		context->encoder_object = bv32_encode_init(NULL);
	}

	if (decoding) {
		context->decoder_object = bv32_decode_init(NULL);
	}

	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_bv32_destroy(switch_codec_t *codec)
{
	struct bv32_context *context = codec->private_info;

	if (context->encoder_object) {
		bv32_encode_free(context->encoder_object);
	}

	if (context->decoder_object) {
		bv32_decode_free(context->decoder_object);
	}

	codec->private_info = NULL;


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_bv32_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct bv32_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = bv32_encode(context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_bv32_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct bv32_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if ((*flag & SFF_PLC)) {
		*decoded_data_len = (2 * bv32_fillin(context->decoder_object, (int16_t *) decoded_data, encoded_data_len));
	} else {
		*decoded_data_len = (2 * bv32_decode(context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_bv_load)
{
	switch_codec_interface_t *codec_interface;
	int mpf, spf, bpf, ebpf, count;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "BroadVoice16 (BV16)");

	mpf = 10000, spf = 80, bpf = 160, ebpf = 20;

	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 106,	/* the IANA code number */
											 "BV16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 16000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_bv16_init,	/* function to initialize a codec handle using this implementation */
											 switch_bv16_encode,	/* function to encode raw data into encoded data */
											 switch_bv16_decode,	/* function to decode encoded data into raw data */
											 switch_bv16_destroy);	/* deinitalize a codec handle using this implementation */
	}

	SWITCH_ADD_CODEC(codec_interface, "BroadVoice32 (BV32)");

	mpf = 10000, spf = 160, bpf = 320, ebpf = 40;

	for (count = 6; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 127,	/* the IANA code number */
											 "BV32",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 16000,	/* samples transferred per second */
											 16000,	/* actual samples transferred per second */
											 32000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_bv32_init,	/* function to initialize a codec handle using this implementation */
											 switch_bv32_encode,	/* function to encode raw data into encoded data */
											 switch_bv32_decode,	/* function to decode encoded data into raw data */
											 switch_bv32_destroy);	/* deinitalize a codec handle using this implementation */
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
