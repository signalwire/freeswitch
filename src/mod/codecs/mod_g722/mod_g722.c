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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / G722 codec module
 *
 * The Initial Developer of the Original Code is
 * Brian K. West <brian.west@mac.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Brian K. West <brian.west@mac.com>
 * Anthony Minessale II <anthmct@yahoo.com>
 * Michael Jerris <mike@jerris.com>
 *
 * mod_g722.c -- G722 Codec Module
 *
 */
#include <switch.h>
#include "g7xx/g722.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_g722_load);
SWITCH_MODULE_DEFINITION(mod_g722, mod_g722_load, NULL, NULL);

struct g722_context {
	g722_decode_state_t decoder_object;
	g722_encode_state_t encoder_object;
};

static switch_status_t switch_g722_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	int encoding, decoding;
	struct g722_context *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct g722_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		if (encoding) {
			if (codec->implementation->actual_samples_per_second == 16000) {
				g722_encode_init(&context->encoder_object, 64000, G722_PACKED);
			} else {
				g722_encode_init(&context->encoder_object, 64000, G722_SAMPLE_RATE_8000);
			}
		}
		if (decoding) {
			if (codec->implementation->actual_samples_per_second == 16000) {
				g722_decode_init(&context->decoder_object, 64000, G722_PACKED);
			} else {
				g722_decode_init(&context->decoder_object, 64000, G722_SAMPLE_RATE_8000);
			}
		}
	}

	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g722_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
										  unsigned int *flag)
{
	struct g722_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = g722_encode(&context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g722_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
										  unsigned int *flag)
{
	struct g722_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * g722_decode(&context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g722_destroy(switch_codec_t *codec)
{
	/* We do not need to use release here as the pool memory is taken care of for us */
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

/* Registration */

static switch_codec_implementation_t g722_8k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 109,
	/*.iananame */ "G722_8",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 64000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 320,
	/*.encoded_bytes_per_frame */ 160,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g722_init,
	/*.encode */ switch_g722_encode,
	/*.decode */ switch_g722_decode,
	/*.destroy */ switch_g722_destroy
};

static switch_codec_implementation_t g722_16k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 9,
	/*.iananame */ "G722",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 16000,
	/*.bits_per_second */ 64000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 640,
	/*.encoded_bytes_per_frame */ 160,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g722_init,
	/*.encode */ switch_g722_encode,
	/*.decode */ switch_g722_decode,
	/*.destroy */ switch_g722_destroy,
	/*.next */ 
};

SWITCH_MODULE_LOAD_FUNCTION(mod_g722_load)
{
	switch_codec_interface_t *codec_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_CODEC(codec_interface, "G722", &g722_16k_implementation);
	SWITCH_ADD_CODEC(codec_interface, "G722_8", &g722_8k_implementation);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
