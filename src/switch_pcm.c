/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
										 uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
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
										 uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
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
		memset(dbuf, 0, codec->implementation->decoded_bytes_per_packet);
		*decoded_data_len = codec->implementation->decoded_bytes_per_packet;
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
		memset(dbuf, 0, codec->implementation->decoded_bytes_per_packet);
		*decoded_data_len = codec->implementation->decoded_bytes_per_packet;
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


static void mod_g711_load(switch_loadable_module_interface_t ** module_interface, switch_memory_pool_t *pool)
{
	switch_codec_interface_t *codec_interface;
	int mpf = 10000, spf = 80, bpf = 160, ebpf = 80, count;

	SWITCH_ADD_CODEC(codec_interface, "G.711 ulaw");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 0,	/* the IANA code number */
											 "PCMU",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 64000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 spf * count,	/* number of frames per network packet */
											 switch_g711u_init,	/* function to initialize a codec handle using this implementation */
											 switch_g711u_encode,	/* function to encode raw data into encoded data */
											 switch_g711u_decode,	/* function to decode encoded data into raw data */
											 switch_g711u_destroy);	/* deinitalize a codec handle using this implementation */
	}

	SWITCH_ADD_CODEC(codec_interface, "G.711 alaw");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 8,	/* the IANA code number */
											 "PCMA",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 64000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 spf * count,	/* number of frames per network packet */
											 switch_g711a_init,	/* function to initialize a codec handle using this implementation */
											 switch_g711a_encode,	/* function to encode raw data into encoded data */
											 switch_g711a_decode,	/* function to decode encoded data into raw data */
											 switch_g711a_destroy);	/* deinitalize a codec handle using this implementation */
	}
}

SWITCH_MODULE_LOAD_FUNCTION(core_pcm_load)
{
	switch_codec_interface_t *codec_interface;
	int mpf = 10000, spf = 80, bpf = 160, ebpf = 160, bps = 128000, rate = 8000, counta = 1, countb = 12;
	int samples_per_frame, bytes_per_frame, ms_per_frame, x;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "PROXY VIDEO PASS-THROUGH");
	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_VIDEO,	/* enumeration defining the type of the codec */
										 31,	/* the IANA code number */
										 "PROXY-VID",	/* the IANA code name */
										 NULL,	/* default fmtp to send (can be overridden by the init function) */
										 90000,	/* samples transferred per second */
										 90000,	/* actual samples transferred per second */
										 0,	/* bits transferred per second */
										 0,	/* number of microseconds per frame */
										 0,	/* number of samples per frame */
										 0,	/* number of bytes per frame decompressed */
										 0,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_proxy_init,	/* function to initialize a codec handle using this implementation */
										 switch_proxy_encode,	/* function to encode raw data into encoded data */
										 switch_proxy_decode,	/* function to encode raw data into encoded data */
										 switch_proxy_destroy);	/* deinitalize a codec handle using this implementation */


	SWITCH_ADD_CODEC(codec_interface, "PROXY PASS-THROUGH");
	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 0,	/* the IANA code number */
										 "PROXY",	/* the IANA code name */
										 NULL,	/* default fmtp to send (can be overridden by the init function) */
										 8000,	/* samples transferred per second */
										 8000,	/* actual samples transferred per second */
										 0,	/* bits transferred per second */
										 20000,	/* number of microseconds per frame */
										 160,	/* number of samples per frame */
										 320,	/* number of bytes per frame decompressed */
										 320,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_proxy_init,	/* function to initialize a codec handle using this implementation */
										 switch_proxy_encode,	/* function to encode raw data into encoded data */
										 switch_proxy_decode,	/* function to decode encoded data into raw data */
										 switch_proxy_destroy);	/* deinitalize a codec handle using this implementation */


	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,  /* enumeration defining the type of the codec */
										 0, /* the IANA code number */
										 "PROXY", /* the IANA code name */
										 NULL,  /* default fmtp to send (can be overridden by the init function) */
										 8000,  /* samples transferred per second */
										 8000,  /* actual samples transferred per second */
										 0, /* bits transferred per second */
										 20000, /* number of microseconds per frame */
										 160, /* number of samples per frame */
										 320 * 2, /* number of bytes per frame decompressed */
										 320 * 2, /* number of bytes per frame compressed */
										 2, /* number of channels represented */
										 1, /* number of frames per network packet */
										 switch_proxy_init, /* function to initialize a codec handle using this implementation */
										 switch_proxy_encode, /* function to encode raw data into encoded data */
										 switch_proxy_decode, /* function to decode encoded data into raw data */
										 switch_proxy_destroy); /* deinitalize a codec handle using this implementation */

	SWITCH_ADD_CODEC(codec_interface, "RAW Signed Linear (16 bit)");

	for (counta = 1; counta <= 3; counta++) {
		if (rate == 8000) {
			countb = 12;
		} else {
			countb = 6;
		}
		for (; countb > 0; countb--) {
			switch_core_codec_add_implementation(pool, codec_interface,
												 SWITCH_CODEC_TYPE_AUDIO, 100, "L16", NULL, rate, rate, bps,
												 mpf * countb, spf * countb, bpf * countb, ebpf * countb, 1, spf * countb,
												 switch_raw_init, switch_raw_encode, switch_raw_decode, switch_raw_destroy);

			switch_core_codec_add_implementation(pool, codec_interface,
												 SWITCH_CODEC_TYPE_AUDIO, 100, "L16", NULL, rate, rate, bps,
												 mpf * countb, spf * countb, bpf * countb * 2, ebpf * countb, 2, spf * countb,
												 switch_raw_init, switch_raw_encode, switch_raw_decode, switch_raw_destroy);
		}
		rate = rate * 2;
		bps = bps * 2;
		spf = spf * 2;
		bpf = bpf * 2;
		ebpf = ebpf * 2;
	}

	samples_per_frame = 240;
	bytes_per_frame = 480;
	ms_per_frame = 20000;

	for (x = 0; x < 5; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 12000,	/* samples transferred per second */
											 12000,	/* actual samples transferred per second */
											 192000,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame,	/* number of bytes per frame decompressed */
											 bytes_per_frame,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

    switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,  /* enumeration defining the type of the codec */
                       100,  /* the IANA code number */
                       "L16", /* the IANA code name */
                       NULL,  /* default fmtp to send (can be overridden by the init function) */
                       12000, /* samples transferred per second */
                       12000, /* actual samples transferred per second */
                       192000 * 2,  /* bits transferred per second */
                       ms_per_frame,  /* number of microseconds per frame */
                       samples_per_frame, /* number of samples per frame */
                       bytes_per_frame * 2, /* number of bytes per frame decompressed */
                       bytes_per_frame * 2, /* number of bytes per frame compressed */
                       2, /* number of channels represented */
                       1, /* number of frames per network packet */
                       switch_raw_init, /* function to initialize a codec handle using this implementation */
                       switch_raw_encode, /* function to encode raw data into encoded data */
                       switch_raw_decode, /* function to decode encoded data into raw data */
                       switch_raw_destroy); /* deinitalize a codec handle using this implementation */

		samples_per_frame += 240;
		bytes_per_frame += 480;
		ms_per_frame += 20000;

	}

	samples_per_frame = 480;
	bytes_per_frame = 960;
	ms_per_frame = 20000;

	for (x = 0; x < 3; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 24000,	/* samples transferred per second */
											 24000,	/* actual samples transferred per second */
											 384000,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame,	/* number of bytes per frame decompressed */
											 bytes_per_frame,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

    switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,  /* enumeration defining the type of the codec */
                       100,  /* the IANA code number */
                       "L16", /* the IANA code name */
                       NULL,  /* default fmtp to send (can be overridden by the init function) */
                       24000, /* samples transferred per second */
                       24000, /* actual samples transferred per second */
                       384000 * 2,  /* bits transferred per second */
                       ms_per_frame,  /* number of microseconds per frame */
                       samples_per_frame, /* number of samples per frame */
                       bytes_per_frame * 2, /* number of bytes per frame decompressed */
                       bytes_per_frame * 2, /* number of bytes per frame compressed */
                       2, /* number of channels represented */
                       1, /* number of frames per network packet */
                       switch_raw_init, /* function to initialize a codec handle using this implementation */
                       switch_raw_encode, /* function to encode raw data into encoded data */
                       switch_raw_decode, /* function to decode encoded data into raw data */
                       switch_raw_destroy); /* deinitalize a codec handle using this implementation */

		samples_per_frame += 480;
		bytes_per_frame += 960;
		ms_per_frame += 20000;

	}

	/* these formats below are for file playing. */

	samples_per_frame = 96;
	bytes_per_frame = 192;
	ms_per_frame = 2000;

	for (x = 0; x < 5; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 48000,	/* actual samples transferred per second */
											 768000,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame,	/* number of bytes per frame decompressed */
											 bytes_per_frame,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 48000,	/* actual samples transferred per second */
											 768000 * 2,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame * 2,	/* number of bytes per frame decompressed */
											 bytes_per_frame * 2,	/* number of bytes per frame compressed */
											 2,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

		samples_per_frame += 96;
		bytes_per_frame += 192;
		ms_per_frame += 2000;

	}


	samples_per_frame = 16;
	bytes_per_frame = 32;
	ms_per_frame = 2000;

	for (x = 0; x < 4; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 128000,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame,	/* number of bytes per frame decompressed */
											 bytes_per_frame,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

    switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,  /* enumeration defining the type of the codec */
                       100,  /* the IANA code number */
                       "L16", /* the IANA code name */
                       NULL,  /* default fmtp to send (can be overridden by the init function) */
                       8000,  /* samples transferred per second */
                       8000,  /* actual samples transferred per second */
                       128000 * 2,  /* bits transferred per second */
                       ms_per_frame,  /* number of microseconds per frame */
                       samples_per_frame, /* number of samples per frame */
                       bytes_per_frame * 2, /* number of bytes per frame decompressed */
                       bytes_per_frame * 2, /* number of bytes per frame compressed */
                       2, /* number of channels represented */
                       1, /* number of frames per network packet */
                       switch_raw_init, /* function to initialize a codec handle using this implementation */
                       switch_raw_encode, /* function to encode raw data into encoded data */
                       switch_raw_decode, /* function to decode encoded data into raw data */
                       switch_raw_destroy); /* deinitalize a codec handle using this implementation */

		samples_per_frame += 16;
		bytes_per_frame += 32;
		ms_per_frame += 2000;

	}

	samples_per_frame = 32;
	bytes_per_frame = 64;
	ms_per_frame = 2000;

	for (x = 0; x < 4; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 16000,	/* samples transferred per second */
											 16000,	/* actual samples transferred per second */
											 256000,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame,	/* number of bytes per frame decompressed */
											 bytes_per_frame,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

    switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,  /* enumeration defining the type of the codec */
                       100,  /* the IANA code number */
                       "L16", /* the IANA code name */
                       NULL,  /* default fmtp to send (can be overridden by the init function) */
                       16000, /* samples transferred per second */
                       16000, /* actual samples transferred per second */
                       256000 * 2,  /* bits transferred per second */
                       ms_per_frame,  /* number of microseconds per frame */
                       samples_per_frame, /* number of samples per frame */
                       bytes_per_frame * 2, /* number of bytes per frame decompressed */
                       bytes_per_frame * 2, /* number of bytes per frame compressed */
                       2, /* number of channels represented */
                       1, /* number of frames per network packet */
                       switch_raw_init, /* function to initialize a codec handle using this implementation */
                       switch_raw_encode, /* function to encode raw data into encoded data */
                       switch_raw_decode, /* function to decode encoded data into raw data */
                       switch_raw_destroy); /* deinitalize a codec handle using this implementation */

		samples_per_frame += 32;
		bytes_per_frame += 64;
		ms_per_frame += 2000;

	}


	samples_per_frame = 64;
	bytes_per_frame = 128;
	ms_per_frame = 2000;

	for (x = 0; x < 4; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 32000,	/* samples transferred per second */
											 32000,	/* actual samples transferred per second */
											 512000,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame,	/* number of bytes per frame decompressed */
											 bytes_per_frame,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

    switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,  /* enumeration defining the type of the codec */
                       100,  /* the IANA code number */
                       "L16", /* the IANA code name */
                       NULL,  /* default fmtp to send (can be overridden by the init function) */
                       32000, /* samples transferred per second */
                       32000, /* actual samples transferred per second */
                       512000 * 2,  /* bits transferred per second */
                       ms_per_frame,  /* number of microseconds per frame */
                       samples_per_frame, /* number of samples per frame */
                       bytes_per_frame * 2, /* number of bytes per frame decompressed */
                       bytes_per_frame * 2, /* number of bytes per frame compressed */
                       2, /* number of channels represented */
                       1, /* number of frames per network packet */
                       switch_raw_init, /* function to initialize a codec handle using this implementation */
                       switch_raw_encode, /* function to encode raw data into encoded data */
                       switch_raw_decode, /* function to decode encoded data into raw data */
                       switch_raw_destroy); /* deinitalize a codec handle using this implementation */

		samples_per_frame += 64;
		bytes_per_frame += 128;
		ms_per_frame += 2000;

	}

	samples_per_frame = 960;
	bytes_per_frame = 1920;
	ms_per_frame = 20000;
	/* 10ms is already registered */
	for (x = 0; x < 3; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 48000,	/* actual samples transferred per second */
											 768000,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame,	/* number of bytes per frame decompressed */
											 bytes_per_frame,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 48000,	/* actual samples transferred per second */
											 768000 * 2,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame * 2,	/* number of bytes per frame decompressed */
											 bytes_per_frame * 2,	/* number of bytes per frame compressed */
											 2,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

		samples_per_frame += 480;
		bytes_per_frame += 960;
		ms_per_frame += 10000;
	}

	samples_per_frame = 441;
	bytes_per_frame = 882;
	ms_per_frame = 10000;

	for (x = 0; x < 3; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 44100,	/* samples transferred per second */
											 44100,	/* actual samples transferred per second */
											 705600,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame,	/* number of bytes per frame decompressed */
											 bytes_per_frame,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 100,	/* the IANA code number */
											 "L16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 44100,	/* samples transferred per second */
											 44100,	/* actual samples transferred per second */
											 705600,	/* bits transferred per second */
											 ms_per_frame,	/* number of microseconds per frame */
											 samples_per_frame,	/* number of samples per frame */
											 bytes_per_frame * 2,	/* number of bytes per frame decompressed */
											 bytes_per_frame * 2,	/* number of bytes per frame compressed */
											 2,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_raw_init,	/* function to initialize a codec handle using this implementation */
											 switch_raw_encode,	/* function to encode raw data into encoded data */
											 switch_raw_decode,	/* function to decode encoded data into raw data */
											 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

		samples_per_frame += 441;
		bytes_per_frame += 882;
		ms_per_frame += 10000;
		
	}	



	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 100,	/* the IANA code number */
										 "L16",	/* the IANA code name */
										 NULL,	/* default fmtp to send (can be overridden by the init function) */
										 22050,	/* samples transferred per second */
										 22050,	/* actual samples transferred per second */
										 352800,	/* bits transferred per second */
										 20000,	/* number of microseconds per frame */
										 441,	/* number of samples per frame */
										 882,	/* number of bytes per frame decompressed */
										 882,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_raw_init,	/* function to initialize a codec handle using this implementation */
										 switch_raw_encode,	/* function to encode raw data into encoded data */
										 switch_raw_decode,	/* function to decode encoded data into raw data */
										 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

  switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,  /* enumeration defining the type of the codec */
                     100,  /* the IANA code number */
                     "L16", /* the IANA code name */
                     NULL,  /* default fmtp to send (can be overridden by the init function) */
                     22050, /* samples transferred per second */
                     22050, /* actual samples transferred per second */
                     352800 * 2,  /* bits transferred per second */
                     20000, /* number of microseconds per frame */
                     441, /* number of samples per frame */
                     882 * 2, /* number of bytes per frame decompressed */
                     882 * 2, /* number of bytes per frame compressed */
                     2, /* number of channels represented */
                     1, /* number of frames per network packet */
                     switch_raw_init, /* function to initialize a codec handle using this implementation */
                     switch_raw_encode, /* function to encode raw data into encoded data */
                     switch_raw_decode, /* function to decode encoded data into raw data */
                     switch_raw_destroy); /* deinitalize a codec handle using this implementation */

	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 100,	/* the IANA code number */
										 "L16",	/* the IANA code name */
										 NULL,	/* default fmtp to send (can be overridden by the init function) */
										 11025,	/* samples transferred per second */
										 11025,	/* actual samples transferred per second */
										 176400,	/* bits transferred per second */
										 40000,	/* number of microseconds per frame */
										 441,	/* number of samples per frame */
										 882,	/* number of bytes per frame decompressed */
										 882,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_raw_init,	/* function to initialize a codec handle using this implementation */
										 switch_raw_encode,	/* function to encode raw data into encoded data */
										 switch_raw_decode,	/* function to decode encoded data into raw data */
										 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

  switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,  /* enumeration defining the type of the codec */
                     100,  /* the IANA code number */
                     "L16", /* the IANA code name */
                     NULL,  /* default fmtp to send (can be overridden by the init function) */
                     11025, /* samples transferred per second */
                     11025, /* actual samples transferred per second */
                     176400 * 2,  /* bits transferred per second */
                     40000, /* number of microseconds per frame */
                     441, /* number of samples per frame */
                     882 * 2, /* number of bytes per frame decompressed */
                     882 * 2, /* number of bytes per frame compressed */
                     2, /* number of channels represented */
                     1, /* number of frames per network packet */
                     switch_raw_init, /* function to initialize a codec handle using this implementation */
                     switch_raw_encode, /* function to encode raw data into encoded data */
                     switch_raw_decode, /* function to decode encoded data into raw data */
                     switch_raw_destroy); /* deinitalize a codec handle using this implementation */


	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 100,	/* the IANA code number */
										 "L16",	/* the IANA code name */
										 NULL,	/* default fmtp to send (can be overridden by the init function) */
										 11025,	/* samples transferred per second */
										 11025,	/* actual samples transferred per second */
										 176400,	/* bits transferred per second */
										 32000,	/* number of microseconds per frame */
										 256,	/* number of samples per frame */
										 512,	/* number of bytes per frame decompressed */
										 512,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_raw_init,	/* function to initialize a codec handle using this implementation */
										 switch_raw_encode,	/* function to encode raw data into encoded data */
										 switch_raw_decode,	/* function to decode encoded data into raw data */
										 switch_raw_destroy);	/* deinitalize a codec handle using this implementation */

  switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,  /* enumeration defining the type of the codec */
                     100,  /* the IANA code number */
                     "L16", /* the IANA code name */
                     NULL,  /* default fmtp to send (can be overridden by the init function) */
                     11025, /* samples transferred per second */
                     11025, /* actual samples transferred per second */
                     176400 * 2,  /* bits transferred per second */
                     32000, /* number of microseconds per frame */
                     256, /* number of samples per frame */
                     512 * 2, /* number of bytes per frame decompressed */
                     512 * 2, /* number of bytes per frame compressed */
                     2, /* number of channels represented */
                     1, /* number of frames per network packet */
                     switch_raw_init, /* function to initialize a codec handle using this implementation */
                     switch_raw_encode, /* function to encode raw data into encoded data */
                     switch_raw_decode, /* function to decode encoded data into raw data */
                     switch_raw_destroy); /* deinitalize a codec handle using this implementation */

	/* indicate that the module should continue to be loaded */

	mod_g711_load(module_interface, pool);

	return SWITCH_STATUS_SUCCESS;
	//return SWITCH_STATUS_NOUNLOAD;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
