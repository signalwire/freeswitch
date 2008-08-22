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
 * switch_pcm.c -- Raw and Encoded PCM 16 bit Signed Linear
 *
 */

#include <switch.h>
#include <g711.h>

#ifdef WIN32
#undef SWITCH_MOD_DECLARE_DATA
#define SWITCH_MOD_DECLARE_DATA __declspec(dllexport)
#endif
SWITCH_MODULE_LOAD_FUNCTION(core_pcm_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(core_pcm_shutdown);
SWITCH_MODULE_DEFINITION(CORE_PCM_MODULE, core_pcm_load, core_pcm_shutdown, NULL);

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
										 uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate, unsigned int *flag)
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
										 uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate, unsigned int *flag)
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



static switch_status_t switch_proxy_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_proxy_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										   unsigned int *flag)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_proxy_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_proxy_destroy(switch_codec_t *codec)
{
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t switch_g711u_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
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

static switch_status_t switch_g711u_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										   unsigned int *flag)
{
	short *dbuf;
	unsigned char *ebuf;
	uint32_t i;

	dbuf = decoded_data;
	ebuf = encoded_data;

	for (i = 0; i < decoded_data_len / sizeof(short); i++) {
		ebuf[i] = linear_to_ulaw(dbuf[i]);
	}

	*encoded_data_len = i;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g711u_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
	short *dbuf;
	unsigned char *ebuf;
	uint32_t i;

	dbuf = decoded_data;
	ebuf = encoded_data;

	if (*flag & SWITCH_CODEC_FLAG_SILENCE) {
		memset(dbuf, 0, codec->implementation->bytes_per_frame);
		*decoded_data_len = codec->implementation->bytes_per_frame;
	} else {
		for (i = 0; i < encoded_data_len; i++) {
			dbuf[i] = ulaw_to_linear(ebuf[i]);
		}

		*decoded_data_len = i * 2;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g711u_destroy(switch_codec_t *codec)
{
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t switch_g711a_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
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

static switch_status_t switch_g711a_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										   unsigned int *flag)
{
	short *dbuf;
	unsigned char *ebuf;
	uint32_t i;

	dbuf = decoded_data;
	ebuf = encoded_data;

	for (i = 0; i < decoded_data_len / sizeof(short); i++) {
		ebuf[i] = linear_to_alaw(dbuf[i]);
	}

	*encoded_data_len = i;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g711a_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
	short *dbuf;
	unsigned char *ebuf;
	uint32_t i;

	dbuf = decoded_data;
	ebuf = encoded_data;

	if (*flag & SWITCH_CODEC_FLAG_SILENCE) {
		memset(dbuf, 0, codec->implementation->bytes_per_frame);
		*decoded_data_len = codec->implementation->bytes_per_frame;
	} else {
		for (i = 0; i < encoded_data_len; i++) {
			dbuf[i] = alaw_to_linear(ebuf[i]);
		}

		*decoded_data_len = i * 2;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g711a_destroy(switch_codec_t *codec)
{
	return SWITCH_STATUS_SUCCESS;
}


static void mod_g711_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
{
	switch_codec_interface_t *codec_interface;
	int mpf = 10000, spf = 80, bpf = 160, ebpf = 80, count;

	SWITCH_ADD_CODEC(codec_interface, "G.711 ulaw");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface,
											 SWITCH_CODEC_TYPE_AUDIO, 0, "PCMU", NULL, 8000, 8000, 64000,
											 mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
											 switch_g711u_init, switch_g711u_encode, switch_g711u_decode, switch_g711u_destroy);
	}

	SWITCH_ADD_CODEC(codec_interface, "G.711 alaw");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface,
											 SWITCH_CODEC_TYPE_AUDIO, 8, "PCMA", NULL, 8000, 8000, 64000,
											 mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
											 switch_g711a_init, switch_g711a_encode, switch_g711a_decode, switch_g711a_destroy);
	}

}

SWITCH_MODULE_LOAD_FUNCTION(core_pcm_load)
{
	switch_codec_interface_t *codec_interface;
	int mpf = 10000, spf = 80, bpf = 160, ebpf = 160, bps = 128000, rate = 8000, counta = 1, countb = 12;
	switch_payload_t ianacode[4] = { 0, 10, 117, 119 };

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "PROXY VIDEO PASS-THROUGH");
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_VIDEO, 31, "PROXY-VID", NULL, 90000, 90000, 0,
										 0, 0, 0, 0, 1, 1, 1, switch_proxy_init, switch_proxy_encode, switch_proxy_decode, switch_proxy_destroy);


	SWITCH_ADD_CODEC(codec_interface, "PROXY PASS-THROUGH");
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 0, "PROXY", NULL, 8000, 8000, 0,
										 20000, 160, 320, 320, 1, 1, 12,
										 switch_proxy_init, switch_proxy_encode, switch_proxy_decode, switch_proxy_destroy);

	SWITCH_ADD_CODEC(codec_interface, "RAW Signed Linear (16 bit)");

	for (counta = 1; counta <= 3; counta++) {
		for (countb = 12; countb > 0; countb--) {
			switch_core_codec_add_implementation(pool, codec_interface,
												 SWITCH_CODEC_TYPE_AUDIO, ianacode[counta], "L16", NULL, rate, rate, bps,
												 mpf * countb, spf * countb, bpf * countb, ebpf * countb, 1, 1, 12,
												 switch_raw_init, switch_raw_encode, switch_raw_decode, switch_raw_destroy);
		}
		rate = rate * 2;
		bps = bps * 2;
		spf = spf * 2;
		bpf = bpf * 2;
		ebpf = ebpf * 2;
	}
	/* these formats below are for file playing. */

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 118, "L16", NULL, 22050, 22050, 352800,
										 20000, 441, 882, 882, 1, 1, 1, switch_raw_init, switch_raw_encode, switch_raw_decode, switch_raw_destroy);

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 118, "L16", NULL, 11025, 11025, 176400,
										 40000, 441, 882, 882, 1, 1, 1, switch_raw_init, switch_raw_encode, switch_raw_decode, switch_raw_destroy);



	/* indicate that the module should continue to be loaded */

	mod_g711_load(module_interface, pool);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(core_pcm_shutdown)
{
	return SWITCH_STATUS_NOUNLOAD;
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
