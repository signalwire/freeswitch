/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library
 *
 * The Initial Developer of the Original Code is
 * Brian West <brian@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * mod_codec2 -- FreeSWITCH CODEC2 Module
 *
 */

#include <switch.h>
#include <codec2.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_codec2_load);

SWITCH_MODULE_DEFINITION(mod_codec2, mod_codec2_load, NULL, NULL);

struct codec2_context {
	void *encoder;
	void *decoder;
};

static switch_status_t switch_codec2_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct codec2_context *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context)))) {
		return SWITCH_STATUS_FALSE;
	}
	context->encoder = codec2_create();
	context->decoder = codec2_create();

	codec->private_info = context;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_codec2_encode(switch_codec_t *codec, switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate,
										  void *encoded_data,
										  uint32_t *encoded_data_len,
										  uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct codec2_context *context = codec->private_info;
	
	switch_assert(decoded_data_len == 160 * 2);
	
	codec2_encode(context->encoder, encoded_data, decoded_data);
	
	*encoded_data_len = 7; /* 51 bits */
	*encoded_rate = 8000;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_codec2_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate,
										  void *decoded_data,
										  uint32_t *decoded_data_len,
										  uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct codec2_context *context = codec->private_info;

	switch_assert(encoded_data_len == 7);

	codec2_encode(context->decoder, decoded_data, encoded_data);

	*decoded_data_len = 160 * 2; /* 160 samples */
	*decoded_rate = 8000;
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_codec2_destroy(switch_codec_t *codec)
{
	struct codec2_context *context = codec->private_info;
	
	codec2_destroy(context->encoder);
	codec2_destroy(context->decoder);
	
	
	context->encoder = NULL;
	context->decoder = NULL;
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_codec2_load)
{
	switch_codec_interface_t *codec_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "CODEC2 2550bps");

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO,
										 0,
										 "CODEC2",
										 NULL,
										 8000, /* samples/sec */
										 8000, /* samples/sec */
										 2550, /* bps */
										 20000, /* ptime */
										 160,	/* samples decoded */
										 320,	/* bytes decoded */
										 7,	/* bytes encoded */
										 1,	/* channels */
										 1,	/* frames/packet */
										 switch_codec2_init,
										 switch_codec2_encode,
										 switch_codec2_decode,
										 switch_codec2_destroy);

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
