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
#include "switch_bitpack.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_g726_load);
SWITCH_MODULE_DEFINITION(mod_g726, mod_g726_load, NULL, NULL);

typedef int (*encoder_t) (int, int, g726_state *);
typedef int (*decoder_t) (int, int, g726_state *);

typedef struct {
	g726_state context;
	switch_byte_t bits_per_frame;
	encoder_t encoder;
	decoder_t decoder;
	switch_bitpack_t pack;
	switch_bitpack_t unpack;
	switch_bitpack_mode_t mode;
	switch_byte_t loops;
	switch_byte_t bytes;
	switch_byte_t buf[160];
} g726_handle_t;

static switch_status_t switch_g726_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	g726_handle_t *handle;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(handle = switch_core_alloc(codec->memory_pool, sizeof(*handle))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		handle->bytes = (switch_byte_t) codec->implementation->encoded_bytes_per_frame;

		switch (handle->bytes) {
		case 100:
			handle->encoder = g726_40_encoder;
			handle->decoder = g726_40_decoder;
			handle->loops = 160;
			break;
		case 80:
			handle->encoder = g726_32_encoder;
			handle->decoder = g726_32_decoder;
			handle->loops = 40;
			break;
		case 60:
			handle->encoder = g726_24_encoder;
			handle->decoder = g726_24_decoder;
			handle->loops = 160;
			break;
		case 40:
			handle->encoder = g726_16_encoder;
			handle->decoder = g726_16_decoder;
			handle->loops = 160;
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid Encoding Size %d!\n", codec->implementation->encoded_bytes_per_frame);
			return SWITCH_STATUS_FALSE;
		}

		g726_init_state(&handle->context);
		codec->private_info = handle;
		handle->bits_per_frame = (switch_byte_t) (codec->implementation->bits_per_second / (codec->implementation->actual_samples_per_second));
		handle->mode = (flags & SWITCH_CODEC_FLAG_AAL2 || strstr(codec->implementation->iananame, "AAL2"))
			? SWITCH_BITPACK_MODE_AAL2 : SWITCH_BITPACK_MODE_RFC3551;
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_g726_destroy(switch_codec_t *codec)
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g726_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
										  unsigned int *flag)
{
	g726_handle_t *handle = codec->private_info;
	g726_state *context = &handle->context;
	uint32_t len = codec->implementation->bytes_per_frame;

	if (!context) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (decoded_data_len % len == 0) {
		uint32_t new_len = 0;
		int16_t *ddp = decoded_data;
		switch_byte_t *edp = encoded_data;
		uint32_t x, loops = decoded_data_len / (sizeof(*ddp));

		switch_bitpack_init(&handle->pack, handle->bits_per_frame, edp, *encoded_data_len, handle->mode);

		for (x = 0; x < loops && new_len < *encoded_data_len; x++) {
			int edata = handle->encoder(*ddp, AUDIO_ENCODING_LINEAR, context);
			switch_bitpack_in(&handle->pack, (switch_byte_t) edata);
			ddp++;
		}
		switch_bitpack_done(&handle->pack);
		new_len = handle->pack.bytes;

		if (new_len <= *encoded_data_len) {
			*encoded_data_len = new_len;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer overflow!!! %u >= %u\n", new_len, *encoded_data_len);
			return SWITCH_STATUS_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g726_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
										  unsigned int *flag)
{
	g726_handle_t *handle = codec->private_info;
	g726_state *context = &handle->context;
	int16_t *ddp = decoded_data;
	uint32_t new_len = 0, z = 0, y;

	switch_byte_t *in = (switch_byte_t *) encoded_data;

	if (!handle || !context) {
		return SWITCH_STATUS_FALSE;
	}

	while (z < encoded_data_len && new_len <= *decoded_data_len) {
		switch_bitpack_init(&handle->unpack, handle->bits_per_frame, handle->buf, sizeof(handle->buf), handle->mode);
		for (y = 0; y < handle->loops; y++) {
			switch_bitpack_out(&handle->unpack, in[z++]);
		}
		for (y = 0; y < handle->bytes; y++) {
			*ddp++ = (int16_t) handle->decoder(handle->buf[y], AUDIO_ENCODING_LINEAR, context);
			new_len += 2;
		}
		switch_bitpack_done(&handle->unpack);
	}

	if (new_len <= *decoded_data_len) {
		*decoded_data_len = new_len;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer overflow!!!\n");
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Registration */

static switch_codec_implementation_t g726_16k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 127,
	/*.iananame */ "G726-16",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 16000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 320,
	/*.encoded_bytes_per_frame */ 40,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g726_init,
	/*.encode */ switch_g726_encode,
	/*.decode */ switch_g726_decode,
	/*.destroy */ switch_g726_destroy,
};

static switch_codec_implementation_t g726_24k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 126,
	/*.iananame */ "G726-24",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 24000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 320,
	/*.encoded_bytes_per_frame */ 60,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g726_init,
	/*.encode */ switch_g726_encode,
	/*.decode */ switch_g726_decode,
	/*.destroy */ switch_g726_destroy,
};

static switch_codec_implementation_t g726_32k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 2,
	/*.iananame */ "G726-32",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 32000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 320,
	/*.encoded_bytes_per_frame */ 80,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g726_init,
	/*.encode */ switch_g726_encode,
	/*.decode */ switch_g726_decode,
	/*.destroy */ switch_g726_destroy,
};

static switch_codec_implementation_t g726_40k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 125,
	/*.iananame */ "G726-40",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 40000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 320,
	/*.encoded_bytes_per_frame */ 100,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g726_init,
	/*.encode */ switch_g726_encode,
	/*.decode */ switch_g726_decode,
	/*.destroy */ switch_g726_destroy,
};

static switch_codec_implementation_t aal2_g726_16k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 124,
	/*.iananame */ "AAL2-G726-16",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 16000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 320,
	/*.encoded_bytes_per_frame */ 40,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g726_init,
	/*.encode */ switch_g726_encode,
	/*.decode */ switch_g726_decode,
	/*.destroy */ switch_g726_destroy,
};

static switch_codec_implementation_t aal2_g726_24k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 123,
	/*.iananame */ "AAL2-G726-24",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 24000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 320,
	/*.encoded_bytes_per_frame */ 60,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g726_init,
	/*.encode */ switch_g726_encode,
	/*.decode */ switch_g726_decode,
	/*.destroy */ switch_g726_destroy,
};

static switch_codec_implementation_t aal2_g726_32k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 2,
	/*.iananame */ "AAL2-G726-32",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 32000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 320,
	/*.encoded_bytes_per_frame */ 80,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g726_init,
	/*.encode */ switch_g726_encode,
	/*.decode */ switch_g726_decode,
	/*.destroy */ switch_g726_destroy,
};

static switch_codec_implementation_t aal2_g726_40k_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 122,
	/*.iananame */ "AAL2-G726-40",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 40000,
	/*.microseconds_per_frame */ 20000,
	/*.samples_per_frame */ 160,
	/*.bytes_per_frame */ 320,
	/*.encoded_bytes_per_frame */ 100,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_g726_init,
	/*.encode */ switch_g726_encode,
	/*.decode */ switch_g726_decode,
	/*.destroy */ switch_g726_destroy,
};

SWITCH_MODULE_LOAD_FUNCTION(mod_g726_load)
{
	switch_codec_interface_t *codec_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_CODEC(codec_interface, "G.726 40k (aal2)", &aal2_g726_40k_implementation);
	SWITCH_ADD_CODEC(codec_interface, "G.726 32k (aal2)", &aal2_g726_32k_implementation);
	SWITCH_ADD_CODEC(codec_interface, "G.726 24k (aal2)", &aal2_g726_24k_implementation);
	SWITCH_ADD_CODEC(codec_interface, "G.726 16k (aal2)", &aal2_g726_16k_implementation);
	SWITCH_ADD_CODEC(codec_interface, "G.726 40k", &g726_40k_implementation);
	SWITCH_ADD_CODEC(codec_interface, "G.726 32k", &g726_32k_implementation);
	SWITCH_ADD_CODEC(codec_interface, "G.726 24k", &g726_24k_implementation);
	SWITCH_ADD_CODEC(codec_interface, "G.726 16k", &g726_16k_implementation);

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
