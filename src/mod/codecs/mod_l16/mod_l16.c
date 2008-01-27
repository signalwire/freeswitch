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
 * mod_l16.c -- Raw Signed Linear Codec
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_l16_load);
SWITCH_MODULE_DEFINITION(mod_l16, mod_l16_load, NULL, NULL);

static switch_status_t switch_raw_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	int encoding, decoding;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	} else {
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_raw_encode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *decoded_data,
										 uint32_t decoded_data_len,
										 uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
										 unsigned int *flag)
{
	/* NOOP indicates that the audio in is already the same as the audio out, so no conversion was necessary. */
	if (codec && other_codec && codec->implementation && other_codec->implementation && 
		codec->implementation->actual_samples_per_second != other_codec->implementation->actual_samples_per_second) {
		memcpy(encoded_data, decoded_data, decoded_data_len);
		*encoded_data_len = decoded_data_len;
		return SWITCH_STATUS_RESAMPLE;
	}
	return SWITCH_STATUS_NOOP;
}

static switch_status_t switch_raw_decode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *encoded_data,
										 uint32_t encoded_data_len,
										 uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
										 unsigned int *flag)
{
	if (codec && other_codec && codec->implementation && other_codec->implementation &&
		codec->implementation->actual_samples_per_second != other_codec->implementation->actual_samples_per_second) {
		memcpy(decoded_data, encoded_data, encoded_data_len);
		*decoded_data_len = encoded_data_len;
		return SWITCH_STATUS_RESAMPLE;
	}
	return SWITCH_STATUS_NOOP;
}

static switch_status_t switch_raw_destroy(switch_codec_t *codec)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_codec_implementation_t raw_32k_60ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 119,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 32000,
	/*.actual_samples_per_second = */ 32000,
	/*.bits_per_second = */ 512000,
	/*.microseconds_per_frame = */ 60000,
	/*.samples_per_frame = */ 1920,
	/*.bytes_per_frame = */ 3840,
	/*.encoded_bytes_per_frame = */ 3840,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy
	/*.next = */
};

static switch_codec_implementation_t raw_32k_30ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 119,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 32000,
	/*.actual_samples_per_second = */ 32000,
	/*.bits_per_second = */ 512000,
	/*.microseconds_per_frame = */ 30000,
	/*.samples_per_frame = */ 960,
	/*.bytes_per_frame = */ 1920,
	/*.encoded_bytes_per_frame = */ 1920,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy,
	/*.next = */ &raw_32k_60ms_implementation
};

static switch_codec_implementation_t raw_32k_20ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 119,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 32000,
	/*.actual_samples_per_second = */ 32000,
	/*.bits_per_second = */ 512000,
	/*.microseconds_per_frame = */ 20000,
	/*.samples_per_frame = */ 640,
	/*.bytes_per_frame = */ 1280,
	/*.encoded_bytes_per_frame = */ 1280,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy,
	/*.next = */ &raw_32k_30ms_implementation
};

static switch_codec_implementation_t raw_32k_10ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 119,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 32000,
	/*.actual_samples_per_second = */ 32000,
	/*.bits_per_second = */ 512000,
	/*.microseconds_per_frame = */ 10000,
	/*.samples_per_frame = */ 320,
	/*.bytes_per_frame = */ 640,
	/*.encoded_bytes_per_frame = */ 640,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy,
	/*.next = */ &raw_32k_20ms_implementation
};

static switch_codec_implementation_t raw_22k_20ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 118,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 22050,
	/*.actual_samples_per_second = */ 22050,
	/*.bits_per_second = */ 352800,
	/*.microseconds_per_frame = */ 20000,
	/*.samples_per_frame = */ 441,
	/*.bytes_per_frame = */ 882,
	/*.encoded_bytes_per_frame = */ 882,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy,
	/*.next = */ &raw_32k_10ms_implementation
};

static switch_codec_implementation_t raw_16k_120ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 117,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 16000,
	/*.actual_samples_per_second */ 16000,
	/*.bits_per_second */ 256000,
	/*.microseconds_per_frame */ 120000,
	/*.samples_per_frame */ 1920,
	/*.bytes_per_frame */ 3840,
	/*.encoded_bytes_per_frame */ 3840,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_raw_init,
	/*.encode */ switch_raw_encode,
	/*.decode */ switch_raw_decode,
	/*.destroy */ switch_raw_destroy,
	/*.next */ &raw_22k_20ms_implementation
};

static switch_codec_implementation_t raw_16k_60ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 117,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 16000,
	/*.actual_samples_per_second */ 16000,
	/*.bits_per_second */ 256000,
	/*.microseconds_per_frame */ 60000,
	/*.samples_per_frame */ 960,
	/*.bytes_per_frame */ 1920,
	/*.encoded_bytes_per_frame */ 1920,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_raw_init,
	/*.encode */ switch_raw_encode,
	/*.decode */ switch_raw_decode,
	/*.destroy */ switch_raw_destroy,
	/*.next */ &raw_16k_120ms_implementation
};

static switch_codec_implementation_t raw_16k_30ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 117,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 16000,
	/*.actual_samples_per_second */ 16000,
	/*.bits_per_second */ 256000,
	/*.microseconds_per_frame */ 30000,
	/*.samples_per_frame */ 480,
	/*.bytes_per_frame */ 960,
	/*.encoded_bytes_per_frame */ 960,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_raw_init,
	/*.encode */ switch_raw_encode,
	/*.decode */ switch_raw_decode,
	/*.destroy */ switch_raw_destroy,
	/*.next */ &raw_16k_60ms_implementation
};

static switch_codec_implementation_t raw_16k_20ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 117,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 16000,
	/*.actual_samples_per_second = */ 16000,
	/*.bits_per_second = */ 256000,
	/*.microseconds_per_frame = */ 20000,
	/*.samples_per_frame = */ 320,
	/*.bytes_per_frame = */ 640,
	/*.encoded_bytes_per_frame = */ 640,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy,
	/*.next = */ &raw_16k_30ms_implementation
};

static switch_codec_implementation_t raw_16k_10ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 117,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 16000,
	/*.actual_samples_per_second = */ 16000,
	/*.bits_per_second = */ 256000,
	/*.microseconds_per_frame = */ 10000,
	/*.samples_per_frame = */ 160,
	/*.bytes_per_frame = */ 320,
	/*.encoded_bytes_per_frame = */ 320,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy,
	/*.next = */ &raw_16k_20ms_implementation
};

///////////////////////////////

static switch_codec_implementation_t raw_8k_120ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 10,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 512000,
	/*.microseconds_per_frame */ 120000,
	/*.samples_per_frame */ 960,
	/*.bytes_per_frame */ 1920,
	/*.encoded_bytes_per_frame */ 1920,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_raw_init,
	/*.encode */ switch_raw_encode,
	/*.decode */ switch_raw_decode,
	/*.destroy */ switch_raw_destroy,
	/*.next */ &raw_16k_10ms_implementation
};

static switch_codec_implementation_t raw_8k_60ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 10,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 256000,
	/*.microseconds_per_frame */ 60000,
	/*.samples_per_frame */ 480,
	/*.bytes_per_frame */ 960,
	/*.encoded_bytes_per_frame */ 960,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_raw_init,
	/*.encode */ switch_raw_encode,
	/*.decode */ switch_raw_decode,
	/*.destroy */ switch_raw_destroy,
	/*.next */ &raw_8k_120ms_implementation
};

static switch_codec_implementation_t raw_8k_30ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 10,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second */ 8000,
	/*.actual_samples_per_second */ 8000,
	/*.bits_per_second */ 128000,
	/*.microseconds_per_frame */ 30000,
	/*.samples_per_frame */ 240,
	/*.bytes_per_frame */ 480,
	/*.encoded_bytes_per_frame */ 480,
	/*.number_of_channels */ 1,
	/*.pref_frames_per_packet */ 1,
	/*.max_frames_per_packet */ 1,
	/*.init */ switch_raw_init,
	/*.encode */ switch_raw_encode,
	/*.decode */ switch_raw_decode,
	/*.destroy */ switch_raw_destroy,
	/*.next */ &raw_8k_60ms_implementation
};

static switch_codec_implementation_t raw_8k_20ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 10,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 8000,
	/*.actual_samples_per_second = */ 8000,
	/*.bits_per_second = */ 128000,
	/*.microseconds_per_frame = */ 20000,
	/*.samples_per_frame = */ 160,
	/*.bytes_per_frame = */ 320,
	/*.encoded_bytes_per_frame = */ 320,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy,
	/*.next */ &raw_8k_30ms_implementation
};

static switch_codec_implementation_t raw_8k_10ms_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode */ 10,
	/*.iananame */ "L16",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 8000,
	/*.actual_samples_per_second = */ 8000,
	/*.bits_per_second = */ 128000,
	/*.microseconds_per_frame = */ 10000,
	/*.samples_per_frame = */ 80,
	/*.bytes_per_frame = */ 160,
	/*.encoded_bytes_per_frame = */ 160,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy,
	/*.next */ &raw_8k_20ms_implementation
};

SWITCH_MODULE_LOAD_FUNCTION(mod_l16_load)
{
	switch_codec_interface_t *codec_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_CODEC(codec_interface, "raw signed linear (16 bit)", &raw_8k_10ms_implementation);

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
