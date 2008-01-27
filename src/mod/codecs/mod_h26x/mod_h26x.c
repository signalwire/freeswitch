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
 * mod_h26x.c -- H26X Signed Linear Codec
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_h26x_load);
SWITCH_MODULE_DEFINITION(mod_h26x, mod_h26x_load, NULL, NULL);

static switch_status_t switch_h26x_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
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

static switch_status_t switch_h26x_encode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *decoded_data,
										 uint32_t decoded_data_len,
										 uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
										 unsigned int *flag)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_h26x_decode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *encoded_data,
										 uint32_t encoded_data_len,
										 uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
										 unsigned int *flag)
{
	return SWITCH_STATUS_FALSE;
}


static switch_status_t switch_h26x_destroy(switch_codec_t *codec)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_codec_implementation_t h264_90000_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_VIDEO,
	/*.ianacode */ 99,
	/*.iananame */ "H264",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 90000,
	/*.actual_samples_per_second = */ 90000,
	/*.bits_per_second = */ 0,
	/*.microseconds_per_frame = */ 0,
	/*.samples_per_frame = */ 0,
	/*.bytes_per_frame = */ 0,
	/*.encoded_bytes_per_frame = */ 0,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_h26x_init,
	/*.encode = */ switch_h26x_encode,
	/*.decode = */ switch_h26x_decode,
	/*.destroy = */ switch_h26x_destroy
	/*.next = */
};

static switch_codec_implementation_t h263_90000_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_VIDEO,
	/*.ianacode */ 34,
	/*.iananame */ "H263",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 90000,
	/*.actual_samples_per_second = */ 90000,
	/*.bits_per_second = */ 0,
	/*.microseconds_per_frame = */ 0,
	/*.samples_per_frame = */ 0,
	/*.bytes_per_frame = */ 0,
	/*.encoded_bytes_per_frame = */ 0,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_h26x_init,
	/*.encode = */ switch_h26x_encode,
	/*.decode = */ switch_h26x_decode,
	/*.destroy = */ switch_h26x_destroy,
	/*.next = */&h264_90000_implementation
};

static switch_codec_implementation_t h261_90000_implementation = {
	/*.codec_type */ SWITCH_CODEC_TYPE_VIDEO,
	/*.ianacode */ 31,
	/*.iananame */ "H261",
	/*.fmtp */ NULL,
	/*.samples_per_second = */ 90000,
	/*.actual_samples_per_second = */ 90000,
	/*.bits_per_second = */ 0,
	/*.microseconds_per_frame = */ 0,
	/*.samples_per_frame = */ 0,
	/*.bytes_per_frame = */ 0,
	/*.encoded_bytes_per_frame = */ 0,
	/*.number_of_channels = */ 1,
	/*.pref_frames_per_packet = */ 1,
	/*.max_frames_per_packet = */ 1,
	/*.init = */ switch_h26x_init,
	/*.encode = */ switch_h26x_encode,
	/*.decode = */ switch_h26x_decode,
	/*.destroy = */ switch_h26x_destroy,
	/*.next = */&h263_90000_implementation
};

SWITCH_MODULE_LOAD_FUNCTION(mod_h26x_load)
{
	switch_codec_interface_t *codec_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_CODEC(codec_interface, "h26x video (passthru)", &h261_90000_implementation);

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
