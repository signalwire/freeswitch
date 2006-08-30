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
 * mod_amr.c -- GSM-AMR Codec Module
 *
 */  
#include "switch.h"
#include "amr.h"

static const char modname[] = "mod_amr";

struct amr_context {
	void *encoder_state;
	int mode;
};

enum
	{
		GENERIC_PARAMETER_AMR_MAXAL_SDUFRAMES = 0,
		GENERIC_PARAMETER_AMR_BITRATE,
		GENERIC_PARAMETER_AMR_GSMAMRCOMFORTNOISE,
		GENERIC_PARAMETER_AMR_GSMEFRCOMFORTNOISE,
		GENERIC_PARAMETER_AMR_IS_641COMFORTNOISE,
		GENERIC_PARAMETER_AMR_PDCEFRCOMFORTNOISE
	};

enum
	{
		AMR_BITRATE_475 = 0,
		AMR_BITRATE_515,
		AMR_BITRATE_590,
		AMR_BITRATE_670,
		AMR_BITRATE_740,
		AMR_BITRATE_795,
		AMR_BITRATE_1020,
		AMR_BITRATE_1220
	};

//static short bytes_per_frame[16]={ 13, 14, 16, 18, 19, 21, 26, 31, 6, 0, 0, 0, 0, 0, 0, 0 };

#define AMR_Mode  7

static switch_status_t switch_amr_init(switch_codec_t *codec, switch_codec_flag_t flags,
									  const switch_codec_settings_t *codec_settings) 
{
	struct amr_context *context = NULL;
	int encoding, decoding;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct amr_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {

		context->mode = AMR_Mode;

		if (encoding) {
			context->encoder_state = Encoder_Interface_init(0);
		}

		if (decoding) {
			Decoder_Interface_init();
		}

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_amr_destroy(switch_codec_t *codec) 
{
	struct amr_context *context = codec->private_info;
	Encoder_Interface_exit(context->encoder_state);
	Decoder_Interface_exit(context->encoder_state);
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_amr_encode(switch_codec_t *codec, 
										switch_codec_t *other_codec, 
										void *decoded_data,

										uint32_t decoded_data_len, 
										uint32_t decoded_rate, 
										void *encoded_data,

										uint32_t *encoded_data_len, 
										uint32_t *encoded_rate, 
										unsigned int *flag) 
{
	struct amr_context *context = codec->private_info;
	
	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = Encoder_Interface_Encode(context->encoder_state, context->mode, (void *)decoded_data, encoded_data, 0);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_amr_decode(switch_codec_t *codec, 
										switch_codec_t *other_codec, 
										void *encoded_data,

										uint32_t encoded_data_len, 
										uint32_t encoded_rate, 
										void *decoded_data,

										uint32_t *decoded_data_len, 
										uint32_t *decoded_rate, 
										unsigned int *flag) 
{
	struct amr_context *context = codec->private_info;
	
	context->mode = *(char *)encoded_data & 0xF;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	Decoder_Interface_Decode( context, (void *)encoded_data, (void *)decoded_data, 0 );

	*decoded_data_len = codec->implementation->samples_per_frame;

	return SWITCH_STATUS_SUCCESS;
}

/* Registration */ 

static const switch_codec_implementation_t amr_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 118, 
	/*.iananame */ "AMR", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 0, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 0, 
	/*.encoded_bytes_per_frame */ 0, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_amr_init, 
	/*.encode */ switch_amr_encode, 
	/*.decode */ switch_amr_decode, 
	/*.destroy */ switch_amr_destroy, 
};

static const switch_codec_interface_t amr_codec_interface = { 
	/*.interface_name */ "GSM-AMR", 
	/*.implementations */ &amr_implementation, 
};

static switch_loadable_module_interface_t amr_module_interface = { 
	/*.module_name */ modname, 
	/*.endpoint_interface */ NULL, 
	/*.timer_interface */ NULL, 
	/*.dialplan_interface */ NULL, 
	/*.codec_interface */ &amr_codec_interface, 
	/*.application_interface */ NULL 
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface,
													 char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */ 
	*module_interface = &amr_module_interface;

	/* indicate that the module should continue to be loaded */ 
	return SWITCH_STATUS_SUCCESS;
}
