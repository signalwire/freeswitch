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
 *
 *
 * mod_codec_g729.c -- G729 Codec Module
 *
 */
#include "switch.h"
#include "g729.h"

static const char modname[] = "mod_codec_g729";

struct g729_context {
	struct dec_state decoder_object;
	struct cod_state encoder_object;
};

static switch_status switch_g729_init(switch_codec *codec, switch_codec_flag flags, const struct switch_codec_settings *codec_settings)
{
	struct g729_context *context = NULL;
	int encoding, decoding;
	
	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);
	
	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct g729_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		if (encoding) {
			//init_coder is fucked if you comment it no more crash
			g729_init_coder(&context->encoder_object, 0);
		}
		if (decoding) {
			g729_init_decoder(&context->decoder_object);
		}

		codec->private = context;
		
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status switch_g729_destroy(switch_codec *codec)
{
	codec->private = NULL;
	return SWITCH_STATUS_SUCCESS;
}


static switch_status switch_g729_encode(switch_codec *codec,
								   switch_codec *other_codec,
								   void *decoded_data,
								   size_t decoded_data_len,
								   void *encoded_data,
								   size_t *encoded_data_len,
								   unsigned int *flag)
{
	struct g729_context *context = codec->private;
	short *dbuf;
	unsigned char *ebuf;
	int cbret = 0;

	if (!context)
		return SWITCH_STATUS_FALSE;

	dbuf = decoded_data;
	ebuf = encoded_data;
	
	if (decoded_data_len < (size_t)codec->implementation->samples_per_frame*2 || *encoded_data_len < (size_t)codec->implementation->encoded_bytes_per_frame)
		return SWITCH_STATUS_FALSE;

	g729_coder(&context->encoder_object, (short *) dbuf, ebuf, &cbret);

	*encoded_data_len   = (codec->implementation->encoded_bytes_per_frame / 2);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status switch_g729_decode(switch_codec *codec,
								   switch_codec *other_codec,
								   void *encoded_data,
								   size_t encoded_data_len,
								   void *decoded_data,
								   size_t *decoded_data_len,
								   unsigned int *flag) 
{
	struct g729_context *context = codec->private;
	short *dbuf;
	unsigned char *ebuf;

	if (!context)
		return SWITCH_STATUS_FALSE;

	dbuf = decoded_data;
	ebuf = encoded_data;

	if ((encoded_data_len * 2) < (size_t)codec->implementation->encoded_bytes_per_frame)
		return SWITCH_STATUS_FALSE;

	if (*flag & SWITCH_CODEC_FLAG_SILENCE) {
		memset(dbuf, 0, codec->implementation->bytes_per_frame);
		*decoded_data_len = codec->implementation->bytes_per_frame;
	} else {
		g729_decoder(&context->decoder_object, decoded_data, (void *) encoded_data, (int)encoded_data_len);
		*decoded_data_len = codec->implementation->bytes_per_frame;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Registration */

static const switch_codec_implementation g729_8k_implementation = {
	/*.samples_per_second*/				8000,
	/*.bits_per_second*/				64000,
	/*.microseconds_per_frame*/			20000,
	/*.samples_per_frame*/				160,
	/*.bytes_per_frame*/				320,
	/*.encoded_bytes_per_frame*/		20,
	/*.number_of_channels*/				1,
	/*.pref_frames_per_packet*/			6,
	/*.max_frames_per_packet*/			24,
	/*.init*/							switch_g729_init,
	/*.encode*/							switch_g729_encode,
	/*.decode*/							switch_g729_decode,
	/*.destroy*/						switch_g729_destroy,
};

static const switch_codec_interface g729_codec_interface = {
	/*.interface_name*/					"g729",
	/*.codec_type*/						SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode*/						18,
	/*.iananame*/						"G729",
	/*.implementations*/				&g729_8k_implementation,
};

static switch_loadable_module_interface g729_module_interface = {
	/*.module_name*/					modname,
	/*.endpoint_interface*/				NULL,
	/*.timer_interface*/				NULL,
	/*.dialplan_interface*/				NULL,
	/*.codec_interface*/				&g729_codec_interface,
	/*.application_interface*/			NULL
};


SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename) {
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &g729_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}





