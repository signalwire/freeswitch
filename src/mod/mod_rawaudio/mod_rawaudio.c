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
 * mod_rawaudio.c -- Raw Signed Linear Codec
 *
 */
#include <switch.h>

static const char modname[] = "mod_rawaudio";

static switch_status switch_raw_init(switch_codec *codec, switch_codec_flag flags, const struct switch_codec_settings *codec_settings)
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

static switch_status switch_raw_encode(switch_codec *codec,
								 switch_codec *other_codec,
								 void *decoded_data,
								 size_t decoded_data_len,
								 void *encoded_data,
								 size_t *encoded_data_len,
								 unsigned int *flag)
{
	/* NOOP indicates that the audio in is already the same as the audio out, so no conversion was necessary.
	   TBD look at other_codec to determine the original format of the data and determine if we need to resample
	   in the event the audio is the same format but different implementations.
	*/

	return SWITCH_STATUS_NOOP;
}

static switch_status switch_raw_decode(switch_codec *codec,
								 switch_codec *other_codec,
								 void *encoded_data,
								 size_t encoded_data_len,
								 void *decoded_data,
								 size_t *decoded_data_len,
								 unsigned int *flag) 
{

	//printf("decode %d %d->%d\n", encoded_data_len, other_codec->implementation->bytes_per_frame, codec->implementation->bytes_per_frame);
	return SWITCH_STATUS_NOOP;
}


static switch_status switch_raw_destroy(switch_codec *codec)
{
	return SWITCH_STATUS_SUCCESS;
}

#if 0
switch_status raw_file_open(switch_file_handle *handle, char *path)
{
	return SWITCH_STATUS_SUCCESS;
}

switch_status raw_file_close(switch_file_handle *handle)
{
	return SWITCH_STATUS_SUCCESS;
}

switch_status raw_file_seek(switch_file_handle *handle, unsigned int *cur_sample, unsigned int samples, int whence)
{
	return SWITCH_STATUS_NOTIMPL;
}


/* Registration */


static char *supported_formats[] = {"raw", "r8k", NULL};

static const switch_file_interface raw_file_interface = {
	/*.interface_name*/		"raw",
	/*.file_open*/			raw_file_open,
	/*.file_close*/			raw_file_close,
	/*.file_read*/			NULL,
	/*.file_write*/			NULL,
	/*.file_seek*/			raw_file_seek,
	/*.extens*/ 			supported_formats,
	/*.next*/				NULL,
};
#endif

static const switch_codec_implementation raw_32k_implementation = {
	/*.samples_per_second = */  32000,
	/*.bits_per_second = */ 512000,
	/*.samples_per_frame = */ 640,
	/*.bytes_per_frame = */ 1280,
	/*.encoded_bytes_per_frame = */ 1280,
	/*.microseconds_per_frame = */ 20000,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_raw_init,
	/*.encode = */ switch_raw_encode,
	/*.decode = */ switch_raw_decode,
	/*.destroy = */ switch_raw_destroy
};

static const switch_codec_implementation raw_16k_implementation = {
	/*.samples_per_second = */ 16000,
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
	/*.next = */ &raw_32k_implementation
};

static const switch_codec_implementation raw_8k_implementation = {
	/*.samples_per_second = */ 8000,
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
	/*.next = */ &raw_16k_implementation
};


static const switch_codec_implementation raw_8k_30ms_implementation = {
	/*.samples_per_second*/				8000,
	/*.bits_per_second*/				128000,
	/*.microseconds_per_frame*/			30000,
	/*.samples_per_frame*/				240,
	/*.bytes_per_frame*/				480,
	/*.encoded_bytes_per_frame*/		480,
	/*.number_of_channels*/				1,
	/*.pref_frames_per_packet*/			1,
	/*.max_frames_per_packet*/			1,
	/*.init*/							switch_raw_init,
	/*.encode*/							switch_raw_encode,
	/*.decode*/							switch_raw_decode,
	/*.destroy*/						switch_raw_destroy,
	/*.next*/							&raw_8k_implementation
};


static const switch_codec_interface raw_codec_interface = {
	/*.interface_name*/					"raw signed linear (16 bit)",
	/*.codec_type*/						SWITCH_CODEC_TYPE_AUDIO,
	/*.ianacode*/						10,
	/*.iananame*/						"L16",
	/*.implementations*/				&raw_8k_30ms_implementation
};

static switch_loadable_module_interface raw_module_interface = {
	/*.module_name*/					modname,
	/*.endpoint_interface*/				NULL,
	/*.timer_interface*/				NULL,
	/*.dialplan_interface*/				NULL,
	/*.codec_interface*/				&raw_codec_interface,
	/*.application_interface*/			NULL,
	/*.api_interface*/					NULL,
	///*.file_interface*/					&raw_file_interface
};


SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename) {
	/* connect my internal structure to the blank pointer passed to me */ 
	*interface = &raw_module_interface;

	/* indicate that the module should continue to be loaded */ 
	return SWITCH_STATUS_SUCCESS;
}





