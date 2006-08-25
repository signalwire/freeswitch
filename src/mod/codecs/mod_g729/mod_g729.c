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
 * mod_codec_g729.c -- G729 Codec Module
 *
 */  
#include "switch.h"
#include "g729.h"

static const char modname[] = "mod_g729";


struct g729_context {

	struct dec_state decoder_object;
	struct cod_state encoder_object;
};


static switch_status_t switch_g729_init(switch_codec_t *codec, switch_codec_flag_t flags,
									  const switch_codec_settings_t *codec_settings) 
{

	struct g729_context *context = NULL;
	int encoding, decoding;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct g729_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {

		if (encoding) {
			g729_init_coder(&context->encoder_object, 0);
		}

		if (decoding) {
			g729_init_decoder(&context->decoder_object);
		}

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;

	}
}


static switch_status_t switch_g729_destroy(switch_codec_t *codec) 
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t switch_g729_encode(switch_codec_t *codec, 
										switch_codec_t *other_codec, 
										void *decoded_data,

										uint32_t decoded_data_len, 
										uint32_t decoded_rate, 
										void *encoded_data,

										uint32_t *encoded_data_len, 
										uint32_t *encoded_rate, 
										unsigned int *flag) 
{

	struct g729_context *context = codec->private_info;
	int cbret = 0;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (decoded_data_len % 160 == 0) {
		uint32_t new_len = 0;
		INT16 * ddp = decoded_data;
		char *edp = encoded_data;
		int x;
		int loops = (int) decoded_data_len / 160;

		for (x = 0; x < loops && new_len < *encoded_data_len; x++) {
			g729_coder(&context->encoder_object, ddp, edp, &cbret);
			edp += 10;
			ddp += 80;
			new_len += 10;
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



static switch_status_t switch_g729_decode(switch_codec_t *codec, 
										switch_codec_t *other_codec, 
										void *encoded_data,

										uint32_t encoded_data_len, 
										uint32_t encoded_rate, 
										void *decoded_data,

										uint32_t *decoded_data_len, 
										uint32_t *decoded_rate, 
										unsigned int *flag) 
{

	struct g729_context *context = codec->private_info;
	int divisor = 10;
	int plen = 10;

	if (!context) {
		return SWITCH_STATUS_FALSE;

	}


	if (encoded_data_len % 2 == 0) {

		if (encoded_data_len % 12 == 0) {
			return SWITCH_STATUS_BREAK;
			//divisor = 12;
			//plen = 10;
		} else if (encoded_data_len % 10 != 0) {
			//*decoded_data_len = (encoded_data_len / 2) * 160;
			//memset(decoded_data, 255, *decoded_data_len);
			return SWITCH_STATUS_BREAK;
		} else {
			divisor = plen = 10;
		}

		if (encoded_data_len % divisor == 0) {
			uint8_t *test;

			int loops = (int) encoded_data_len / divisor;

			char *edp = encoded_data;

			short *ddp = decoded_data;

			int x;

			uint32_t new_len = 0;

			test = (uint8_t *) encoded_data;
			if (*test == 0 && *(test+1) == 0) {
				*decoded_data_len = (encoded_data_len / divisor) * 160;
				memset(decoded_data, 0, *decoded_data_len);
				return SWITCH_STATUS_SUCCESS;
			}

			for (x = 0; x < loops && new_len < *decoded_data_len; x++) {
				g729_decoder(&context->decoder_object, ddp, edp, plen);

				ddp += 80;
				edp += divisor;
				new_len += 160;
			}

			if (new_len <= *decoded_data_len) {

				*decoded_data_len = new_len;

			} else {

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer overflow!!!\n");
				return SWITCH_STATUS_FALSE;

			}
		} 
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "yo this frame is an odd size [%d]\n", encoded_data_len);
		return SWITCH_STATUS_FALSE;
	}



	return SWITCH_STATUS_SUCCESS;

}



/* Registration */ 

static const switch_codec_implementation_t g729_10ms_8k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 18, 
	/*.iananame */ "G729", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 32000, 
	/*.microseconds_per_frame */ 10000, 
	/*.samples_per_frame */ 80, 
	/*.bytes_per_frame */ 160, 
	/*.encoded_bytes_per_frame */ 10, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g729_init, 
	/*.encode */ switch_g729_encode, 
	/*.decode */ switch_g729_decode, 
	/*.destroy */ switch_g729_destroy, 
};

static const switch_codec_implementation_t g729_8k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 18, 
	/*.iananame */ "G729", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 64000, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 320, 
	/*.encoded_bytes_per_frame */ 20, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g729_init, 
	/*.encode */ switch_g729_encode, 
	/*.decode */ switch_g729_decode, 
	/*.destroy */ switch_g729_destroy, 
	&g729_10ms_8k_implementation
};


static const switch_codec_interface_t g729_codec_interface = { 
	/*.interface_name */ "g729", 
	/*.implementations */ &g729_8k_implementation, 
};


static switch_loadable_module_interface_t g729_module_interface = { 
	/*.module_name */ modname, 
	/*.endpoint_interface */ NULL, 
	/*.timer_interface */ NULL, 
	/*.dialplan_interface */ NULL, 
	/*.codec_interface */ &g729_codec_interface, 
	/*.application_interface */ NULL 
};



SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface,
													 char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */ 
	*module_interface = &g729_module_interface;


	/* indicate that the module should continue to be loaded */ 
	return SWITCH_STATUS_SUCCESS;

}
