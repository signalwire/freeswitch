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
#include "g7xx/g726.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_g726_load);
SWITCH_MODULE_DEFINITION(mod_g726, mod_g726_load, NULL, NULL);

static switch_status_t switch_g726_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
    int packing = G726_PACKING_RIGHT;
    g726_state_t *context;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		if ((flags & SWITCH_CODEC_FLAG_AAL2 || strstr(codec->implementation->iananame, "AAL2"))) {
            packing = G726_PACKING_LEFT;
        } 

        g726_init(context, codec->implementation->bits_per_second, G726_ENCODING_LINEAR, packing);

		codec->private_info = context;
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
	g726_state_t *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

    *encoded_data_len = g726_encode(context, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g726_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
										  unsigned int *flag)
{
    g726_state_t *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

    *decoded_data_len = (2 * g726_decode(context, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_g726_load)
{
	switch_codec_interface_t *codec_interface;
    int mpf = 10000, spf = 80, bpf = 160, ebpf = 20, count;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "G.726 16k (AAL2)");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 124, "AAL2-G726-16", NULL, 8000, 8000, 16000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g726_init, switch_g726_encode, switch_g726_decode, switch_g726_destroy);
    }
	SWITCH_ADD_CODEC(codec_interface, "G.726 16k");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 127, "G726-16", NULL, 8000, 8000, 16000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g726_init, switch_g726_encode, switch_g726_decode, switch_g726_destroy);
    }
    /* Increase encoded bytes per frame by 10 */
    ebpf = ebpf + 10;

	SWITCH_ADD_CODEC(codec_interface, "G.726 24k (AAL2)");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 123, "AAL2-G726-24", NULL, 8000, 8000, 24000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g726_init, switch_g726_encode, switch_g726_decode, switch_g726_destroy);
    }
	SWITCH_ADD_CODEC(codec_interface, "G.726 24k");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 126, "G726-24", NULL, 8000, 8000, 24000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g726_init, switch_g726_encode, switch_g726_decode, switch_g726_destroy);
    }
    /* Increase encoded bytes per frame by 10 */
    ebpf = ebpf + 10;

	SWITCH_ADD_CODEC(codec_interface, "G.726 32k (AAL2)");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 2, "AAL2-G726-32", NULL, 8000, 8000, 32000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g726_init, switch_g726_encode, switch_g726_decode, switch_g726_destroy);
    }
	SWITCH_ADD_CODEC(codec_interface, "G.726 32k");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 2, "G726-32", NULL, 8000, 8000, 32000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g726_init, switch_g726_encode, switch_g726_decode, switch_g726_destroy);
    }
    /* Increase encoded bytes per frame by 10 */
    ebpf = ebpf + 10;

	SWITCH_ADD_CODEC(codec_interface, "G.726 40k (AAL2)");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 122, "AAL2-G726-40", NULL, 8000, 8000, 40000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g726_init, switch_g726_encode, switch_g726_decode, switch_g726_destroy);
    }
	SWITCH_ADD_CODEC(codec_interface, "G.726 40k");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 125, "G726-40", NULL, 8000, 8000, 40000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g726_init, switch_g726_encode, switch_g726_decode, switch_g726_destroy);
    }

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
