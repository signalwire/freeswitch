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
 * Brian K. West <brian.west@mac.com>
 * 
 *
 * mod_g726.c -- G726 Codec Module
 *
 */  
#include "switch.h"
#include "g72x.h"

static const char modname[] = "mod_g726";

static switch_status_t switch_g726_init(switch_codec_t *codec, switch_codec_flag_t flags,
									  const switch_codec_settings_t *codec_settings) 
{
	int encoding, decoding;
	struct g726_state_s *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct g726_state_s))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		g726_init_state(context);
		codec->private_info = context;
		return SWITCH_STATUS_SUCCESS;
	}
}


static switch_status_t switch_g726_destroy(switch_codec_t *codec) 
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

#define define_encoder(func, coder, datalen)\
static switch_status_t func(switch_codec_t *codec,\
							switch_codec_t *other_codec,\
							void *decoded_data,\
\
							uint32_t decoded_data_len,\
							uint32_t decoded_rate,\
							void *encoded_data,\
\
							uint32_t *encoded_data_len,\
							uint32_t *encoded_rate,\
							unsigned int *flag)\
{\
	struct g726_state_s *context = codec->private_info;\
\
	if (!context) {\
		return SWITCH_STATUS_FALSE;\
	}\
\
	if (decoded_data_len % datalen == 0) {\
		coder(*(int *)decoded_data , AUDIO_ENCODING_LINEAR, context);\
	}\
\
	return SWITCH_STATUS_SUCCESS;\
}\

#define define_decoder(func, coder, datalen)\
static switch_status_t func(switch_codec_t *codec,\
							switch_codec_t *other_codec,\
							void *encoded_data,\
\
							uint32_t encoded_data_len,\
							uint32_t encoded_rate,\
							void *decoded_data,\
\
							uint32_t *decoded_data_len,\
							uint32_t *decoded_rate,\
							unsigned int *flag)\
{\
	struct g726_state_s *context = codec->private_info;\
\
	if (!context) {\
		return SWITCH_STATUS_FALSE;\
	}\
\
	if (encoded_data_len % datalen == 0) {\
		coder(*(int *)encoded_data, AUDIO_ENCODING_LINEAR, context);\
}\
\
	return SWITCH_STATUS_SUCCESS;\
}\

define_encoder(switch_g726_16k_encode, g726_16_encoder, 160) 
define_decoder(switch_g726_16k_decode, g726_16_decoder, 40)

define_encoder(switch_g726_24k_encode, g726_24_encoder, 160) 
define_decoder(switch_g726_24k_decode, g726_24_decoder, 60)

define_encoder(switch_g726_32k_encode, g726_32_encoder, 160) 
define_decoder(switch_g726_32k_decode, g726_32_decoder, 80)

define_encoder(switch_g726_40k_encode, g726_40_encoder, 160) 
define_decoder(switch_g726_40k_decode, g726_40_decoder, 100)


/* Registration */ 

static const switch_codec_implementation_t g726_16k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 2, 
	/*.iananame */ "G726-16", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 16000, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 40, 
	/*.encoded_bytes_per_frame */ 10, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g726_init, 
	/*.encode */ switch_g726_16k_encode, 
	/*.decode */ switch_g726_16k_decode, 
	/*.destroy */ switch_g726_destroy, 
};


static const switch_codec_implementation_t g726_24k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 2, 
	/*.iananame */ "G726-24", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 24000, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 60, 
	/*.encoded_bytes_per_frame */ 10, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g726_init, 
	/*.encode */ switch_g726_24k_encode, 
	/*.decode */ switch_g726_24k_decode, 
	/*.destroy */ switch_g726_destroy, 
};

static const switch_codec_implementation_t g726_32k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 2, 
	/*.iananame */ "G726-32", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 32000, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 80, 
	/*.encoded_bytes_per_frame */ 10, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g726_init, 
	/*.encode */ switch_g726_32k_encode, 
	/*.decode */ switch_g726_32k_decode, 
	/*.destroy */ switch_g726_destroy, 
};

static const switch_codec_implementation_t g726_40k_implementation = { 
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO, 
	/*.ianacode */ 2, 
	/*.iananame */ "G726-40", 
	/*.samples_per_second */ 8000, 
	/*.bits_per_second */ 40000, 
	/*.microseconds_per_frame */ 20000, 
	/*.samples_per_frame */ 160, 
	/*.bytes_per_frame */ 100, 
	/*.encoded_bytes_per_frame */ 10, 
	/*.number_of_channels */ 1, 
	/*.pref_frames_per_packet */ 1, 
	/*.max_frames_per_packet */ 1, 
	/*.init */ switch_g726_init, 
	/*.encode */ switch_g726_40k_encode, 
	/*.decode */ switch_g726_40k_decode, 
	/*.destroy */ switch_g726_destroy, 
};

const switch_codec_interface_t g726_16k_codec_interface = { 
	/*.interface_name */ "G.726 16k", 
	/*.implementations */ &g726_16k_implementation, 
};
const switch_codec_interface_t g726_24k_codec_interface = { 
	/*.interface_name */ "G.726 24k", 
	/*.implementations */ &g726_24k_implementation, 
	/*.next */ &g726_16k_codec_interface
};
const switch_codec_interface_t g726_32k_codec_interface = { 
	/*.interface_name */ "G.726 32k", 
	/*.implementations */ &g726_32k_implementation, 
	/*.next */ &g726_24k_codec_interface
};
const switch_codec_interface_t g726_40k_codec_interface = { 
	/*.interface_name */ "G.726 40k", 
	/*.implementations */ &g726_40k_implementation, 
	/*.next */ &g726_32k_codec_interface
};

static switch_loadable_module_interface_t g726_module_interface = { 
	/*.module_name */ modname, 
	/*.endpoint_interface */ NULL, 
	/*.timer_interface */ NULL, 
	/*.dialplan_interface */ NULL, 
	/*.codec_interface */ &g726_40k_codec_interface, 
	/*.application_interface */ NULL 
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface,
													 char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */ 
	*module_interface = &g726_module_interface;

	/* indicate that the module should continue to be loaded */ 
	return SWITCH_STATUS_SUCCESS;
}
