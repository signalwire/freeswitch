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
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * 
 *
 * mod_lpc10.c -- LPC10 Codec Module
 *
 */

#include <switch.h>
#include <lpc10.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_lpc10_load);
SWITCH_MODULE_DEFINITION(mod_lpc10, mod_lpc10_load, NULL, NULL);

enum {
	SamplesPerFrame = 180,
	BitsPerFrame = 54,
	BytesPerFrame = (BitsPerFrame + 7) / 8,
	BitsPerSecond = 2400
};

#define   SampleValueScale 32768.0
#define   MaxSampleValue   32767.0
#define   MinSampleValue   -32767.0

struct lpc10_context {
	struct lpc10_encoder_state encoder_object;
	struct lpc10_decoder_state decoder_object;
};

static switch_status_t switch_lpc10_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct lpc10_context *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct lpc10_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {

		if (encoding) {
			init_lpc10_encoder_state(&context->encoder_object);
		}

		if (decoding) {
			init_lpc10_decoder_state(&context->decoder_object);
		}

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_lpc10_destroy(switch_codec_t *codec)
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_lpc10_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
										   unsigned int *flag)
{
	struct lpc10_context *context = codec->private_info;
	uint8_t i;
	int32_t bits[BitsPerFrame];
	real speech[SamplesPerFrame];
	const short *sampleBuffer = (const short *) decoded_data;
	unsigned char *buffer = (unsigned char *) encoded_data;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	for (i = 0; i < SamplesPerFrame; i++)
		speech[i] = (real) (sampleBuffer[i] / SampleValueScale);

	lpc10_encode(speech, bits, &context->encoder_object);

	memset(encoded_data, 0, BytesPerFrame);
	for (i = 0; i < BitsPerFrame; i++) {
		if (bits[i])
			buffer[i >> 3] |= 1 << (i & 7);
	}

	*encoded_data_len = BytesPerFrame;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_lpc10_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
										   unsigned int *flag)
{
	struct lpc10_context *context = codec->private_info;
	int i;
	INT32 bits[BitsPerFrame];
	real speech[SamplesPerFrame];
	short *sampleBuffer = (short *) decoded_data;
	const unsigned char *buffer = (const unsigned char *) encoded_data;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	for (i = 0; i < BitsPerFrame; i++)
		bits[i] = (buffer[i >> 3] & (1 << (i & 7))) != 0;

	lpc10_decode(bits, speech, &context->decoder_object);

	for (i = 0; i < SamplesPerFrame; i++) {
		real sample = (real) (speech[i] * SampleValueScale);
		if (sample < MinSampleValue)
			sample = MinSampleValue;
		else if (sample > MaxSampleValue)
			sample = MaxSampleValue;
		sampleBuffer[i] = (short) sample;
	}

	*decoded_data_len = SamplesPerFrame * 2;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_lpc10_load)
{
	switch_codec_interface_t *codec_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_CODEC(codec_interface, "LPC-10 2.4kbps");
    switch_core_codec_add_implementation(pool, codec_interface,
                                         SWITCH_CODEC_TYPE_AUDIO, 7, "LPC", NULL, 8000, 8000, 2400,
                                         22500, 180, 360, 7, 1, 1, 1,
                                         switch_lpc10_init, switch_lpc10_encode, switch_lpc10_decode, switch_lpc10_destroy);

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
