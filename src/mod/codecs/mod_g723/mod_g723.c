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
 * Brian K. West <brian.west@mac.com>
 *
 * The g723.1 codec itself is not distributed with this module.
 *
 * mod_g723.c -- G723.1 Codec Module
 *
 */  
#include "switch.h"
#include "g723.h"

#define TYPE_HIGH 0x0
#define TYPE_LOW 0x1
#define TYPE_SILENCE 0x2
#define TYPE_DONTSEND 0x3
#define TYPE_MASK 0x3

Flag UsePf = True;
Flag UseHp = True;
Flag UseVx = True;

enum Crate WrkRate = Rate63;

static const char modname[] = "mod_g723";

struct g723_context {
	struct cod_state encoder_object;
	struct dec_state decoder_object;
	float cod_float_buf[Frame];
	float dec_float_buf[Frame];
};

static switch_status_t switch_g723_init(switch_codec_t *codec, switch_codec_flag_t flags,
									  const switch_codec_settings_t *codec_settings) 
{
	struct g723_context *context = NULL;
	int encoding, decoding;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct g723_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {

		if (encoding) {
			Init_Coder(&context->encoder_object);
			if( UseVx ) {
				Init_Vad(&context->encoder_object);
				Init_Cod_Cng(&context->encoder_object);
			}		   
		}

		if (decoding) {
			Init_Decod(&context->decoder_object);
			Init_Dec_Cng(&context->decoder_object);
		}

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}
}


static switch_status_t switch_g723_destroy(switch_codec_t *codec) 
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g723_encode(switch_codec_t *codec, 
										switch_codec_t *other_codec, 
										void *decoded_data,

										uint32_t decoded_data_len, 
										uint32_t decoded_rate, 
										void *encoded_data,

										uint32_t *encoded_data_len, 
										uint32_t *encoded_rate, 
										unsigned int *flag) 
{
	struct g723_context *context = codec->private_info;
	int16_t *decoded_slin_buf = (int16_t *) decoded_data;
	char *ebuf = (char *) encoded_data;
	int x;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	for(x = 0; x < Frame; x++) {
		context->cod_float_buf[x] = decoded_slin_buf[x];
	}

	Coder(&context->encoder_object, (FLOAT *)context->cod_float_buf, ebuf);
	*encoded_data_len = codec->implementation->encoded_bytes_per_frame;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g723_decode(switch_codec_t *codec, 
										switch_codec_t *other_codec, 
										void *encoded_data,

										uint32_t encoded_data_len, 
										uint32_t encoded_rate, 
										void *decoded_data,

										uint32_t *decoded_data_len, 
										uint32_t *decoded_rate, 
										unsigned int *flag) 
{
	struct g723_context *context = codec->private_info;
	int x;
	int16_t *to_slin_buf = decoded_data;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}
	
	Decod(&context->decoder_object, (FLOAT *) context->dec_float_buf, (char *) decoded_data, 0);

	for (x=0;x<Frame;x++) {
		to_slin_buf[x] = context->dec_float_buf[x];
	}
	*decoded_data_len = codec->implementation->bytes_per_frame;

	return SWITCH_STATUS_SUCCESS;
}

/* Registration */ 

static const switch_codec_implementation_t g723_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 4, 
	/*.iananame */ "G723", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 6300, 
	/*.microseconds_per_frame */ 30000, 
	/*.samples_per_frame */ 240, 
	/*.bytes_per_frame */ 480, 
	/*.encoded_bytes_per_frame */ 24, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 2, 
	/*.max_frames_per_packet */ 2, 
	/*.init */ switch_g723_init, 
	/*.encode */ switch_g723_encode, 
	/*.decode */ switch_g723_decode, 
	/*.destroy */ switch_g723_destroy, 
};

static const switch_codec_interface_t g723_codec_interface = { 
	/*.interface_name */ "g723 6.3k", 
	/*.implementations */ &g723_implementation, 
};

static switch_loadable_module_interface_t g723_module_interface = { 
	/*.module_name */ modname, 
	/*.endpoint_interface */ NULL, 
	/*.timer_interface */ NULL, 
	/*.dialplan_interface */ NULL, 
	/*.codec_interface */ &g723_codec_interface, 
	/*.application_interface */ NULL 
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface,
													 char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */ 
	*module_interface = &g723_module_interface;

	/* indicate that the module should continue to be loaded */ 
	return SWITCH_STATUS_SUCCESS;
}
