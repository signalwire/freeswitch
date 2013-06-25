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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / ISAC codec module
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * mod_isac.c -- isac Codec Module
 *
 */

#include <switch.h>
#include "isac.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_isac_codec_load);
SWITCH_MODULE_DEFINITION(mod_isac, mod_isac_codec_load, NULL, NULL);

struct isac_context {
	ISACStruct *ISAC_main_inst;
};

static switch_status_t switch_isac_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	WebRtc_Word16 err;
	struct isac_context *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context)))) {
		return SWITCH_STATUS_FALSE;
	}

	codec->private_info = context;

	err = WebRtcIsac_Create(&context->ISAC_main_inst);
	
	if (err < 0) return SWITCH_STATUS_FALSE;


	if (encoding) {
		if (WebRtcIsac_EncoderInit(context->ISAC_main_inst, 0) < 0) {
			return SWITCH_STATUS_FALSE;
		}
		WebRtcIsac_SetEncSampRate(context->ISAC_main_inst, codec->implementation->actual_samples_per_second / 1000);
	}

	if (decoding) {
		if (WebRtcIsac_DecoderInit(context->ISAC_main_inst) < 0) {
			return SWITCH_STATUS_FALSE;
		}
		WebRtcIsac_SetDecSampRate(context->ISAC_main_inst, codec->implementation->actual_samples_per_second / 1000);
	}

	if (codec->implementation->actual_samples_per_second == 16000) {
		if (WebRtcIsac_ControlBwe(context->ISAC_main_inst, 32000, codec->implementation->microseconds_per_packet / 1000, 1) < 0) {
			return SWITCH_STATUS_FALSE;
		}

		if (WebRtcIsac_SetMaxPayloadSize(context->ISAC_main_inst, 400) < 0) {
			return SWITCH_STATUS_FALSE;
		}

	} else {
		if (WebRtcIsac_Control(context->ISAC_main_inst, 32000, codec->implementation->microseconds_per_packet / 1000) < 0) {
			return SWITCH_STATUS_FALSE;
		}

		if (WebRtcIsac_SetMaxPayloadSize(context->ISAC_main_inst, 600) < 0) {
			return SWITCH_STATUS_FALSE;
		}
	}
	

	
	if (WebRtcIsac_SetMaxRate(context->ISAC_main_inst, codec->implementation->bits_per_second) < 0) {
		return SWITCH_STATUS_FALSE;
	}


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_isac_encode(switch_codec_t *codec, switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate,
										  void *encoded_data,
										  uint32_t *encoded_data_len,
										  uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct isac_context *context = codec->private_info;
	WebRtc_Word16 len = 0, *in, *out;
	int rise = (codec->implementation->actual_samples_per_second / 100);

	in = decoded_data;
	out = encoded_data;

	while(len == 0) {
		len = WebRtcIsac_Encode(context->ISAC_main_inst, in, out);
		in += rise;
	}
	
	if (len < 0) {
		return SWITCH_STATUS_GENERR;
	}

	*encoded_data_len = (uint32_t) len;
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_isac_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate,
										  void *decoded_data,
										  uint32_t *decoded_data_len,
										  uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct isac_context *context = codec->private_info;
	WebRtc_Word16 len, speechType[1];

	if ((*flag & SFF_PLC)) {
		len = WebRtcIsac_DecodePlc(context->ISAC_main_inst, decoded_data, 1);
	} else {
		len = WebRtcIsac_Decode(context->ISAC_main_inst, encoded_data, encoded_data_len, decoded_data, speechType);
	}

	if (len < 0) {
		*decoded_data_len = 0;
		return SWITCH_STATUS_GENERR;
	}
	
	*decoded_data_len = (uint32_t) len * 2;
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_isac_destroy(switch_codec_t *codec)
{
	struct isac_context *context = codec->private_info;

	WebRtcIsac_Free(context->ISAC_main_inst);
	
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_isac_codec_load)
{
	switch_codec_interface_t *codec_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "isac");	/* 8.0kbit */

	switch_core_codec_add_implementation(pool, codec_interface,	
										 SWITCH_CODEC_TYPE_AUDIO,
										 99,	
										 "isac",
										 "ibitrate=32000;maxbitrate=53400",	
										 16000,	
										 16000,	
										 53400,	
										 30000,	
										 480,	
										 960,	
										 0,	
										 1,	
										 3,	
										 switch_isac_init,	
										 switch_isac_encode,
										 switch_isac_decode,
										 switch_isac_destroy);



	switch_core_codec_add_implementation(pool, codec_interface,	
										 SWITCH_CODEC_TYPE_AUDIO,
										 99,	
										 "isac",
										 "ibitrate=32000;maxbitrate=53400",	
										 16000,	
										 16000,	
										 53400,	
										 60000,	
										 960,	
										 1920,	
										 0,	
										 1,	
										 6,	
										 switch_isac_init,	
										 switch_isac_encode,
										 switch_isac_decode,
										 switch_isac_destroy);

	


	switch_core_codec_add_implementation(pool, codec_interface,	
										 SWITCH_CODEC_TYPE_AUDIO,
										 99,	
										 "isac",
										 "ibitrate=32000;maxbitrate=160000",	
										 32000,	
										 32000,	
										 160000,	
										 30000,	
										 960,	
										 1920,	
										 0,	
										 1,	
										 6,	
										 switch_isac_init,	
										 switch_isac_encode,
										 switch_isac_decode,
										 switch_isac_destroy);


	switch_core_codec_add_implementation(pool, codec_interface,	
										 SWITCH_CODEC_TYPE_AUDIO,
										 99,	
										 "isac",
										 "ibitrate=32000;maxbitrate=160000",	
										 32000,	
										 32000,	
										 160000,	
										 60000,	
										 1920,	
										 3840,	
										 0,	
										 1,	
										 6,	
										 switch_isac_init,	
										 switch_isac_encode,
										 switch_isac_decode,
										 switch_isac_destroy);
	

	
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
