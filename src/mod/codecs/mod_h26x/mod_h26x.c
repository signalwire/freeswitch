/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
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
		if (codec->fmtp_in) {
			codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
		}
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_h26x_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_h26x_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_h26x_destroy(switch_codec_t *codec)
{
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_h26x_load)
{
	switch_codec_interface_t *codec_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_CODEC(codec_interface, "H.264 Video (passthru)");
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_VIDEO, 99, "H264", NULL, 90000, 90000, 0,
										 0, 0, 0, 0, 1, 1, switch_h26x_init, switch_h26x_encode, switch_h26x_decode, switch_h26x_destroy);
	SWITCH_ADD_CODEC(codec_interface, "H.263 Video (passthru)");
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_VIDEO, 34, "H263", NULL, 90000, 90000, 0,
										 0, 0, 0, 0, 1, 1, switch_h26x_init, switch_h26x_encode, switch_h26x_decode, switch_h26x_destroy);
	SWITCH_ADD_CODEC(codec_interface, "H.263+ Video (passthru)");
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_VIDEO, 115, "H263-1998", NULL, 90000, 90000, 0,
										 0, 0, 0, 0, 1, 1, switch_h26x_init, switch_h26x_encode, switch_h26x_decode, switch_h26x_destroy);
	SWITCH_ADD_CODEC(codec_interface, "H.263++ Video (passthru)");
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_VIDEO, 121, "H263-2000", NULL, 90000, 90000, 0,
										 0, 0, 0, 0, 1, 1, switch_h26x_init, switch_h26x_encode, switch_h26x_decode, switch_h26x_destroy);
	SWITCH_ADD_CODEC(codec_interface, "H.261 Video (passthru)");
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_VIDEO, 31, "H261", NULL, 90000, 90000, 0,
										 0, 0, 0, 0, 1, 1, switch_h26x_init, switch_h26x_encode, switch_h26x_decode, switch_h26x_destroy);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
