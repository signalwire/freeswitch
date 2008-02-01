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
 * mod_g722.c -- G.722 Codec Module
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

SWITCH_MODULE_LOAD_FUNCTION(mod_g722_load)
{
	switch_codec_interface_t *codec_interface;
    int mpf = 10000, spf = 80, bpf = 320, ebpf = 80, count;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "G.722");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 9, "G722", NULL, 8000, 16000, 64000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g722_init, switch_g722_encode, switch_g722_decode, switch_g722_destroy);
    }
	SWITCH_ADD_CODEC(codec_interface, "G.722_8");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 109, "G722_8", NULL, 8000, 8000, 64000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g722_init, switch_g722_encode, switch_g722_decode, switch_g722_destroy);
    }

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
