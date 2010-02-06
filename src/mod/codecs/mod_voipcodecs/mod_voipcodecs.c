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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / G722 codec module
 *
 * The Initial Developer of the Original Code is
 * Brian K. West <brian@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brian K. West <brian@freeswitch.org>
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 *
 * mod_voipcodecs.c -- VoIP Codecs (G.711, G.722, G.726, GSM-FR, IMA_ADPCM, LPC10)
 *
 * This module wouldn't be possible without generous contributions from Steve Underwood.  Thanks!
 *
 */

#include <switch.h>
#include "spandsp.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_voipcodecs_load);
SWITCH_MODULE_DEFINITION(mod_voipcodecs, mod_voipcodecs_load, NULL, NULL);

/*  LPC10     - START */

struct lpc10_context {
	lpc10_encode_state_t *encoder_object;
	lpc10_decode_state_t *decoder_object;
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
			context->encoder_object = lpc10_encode_init(context->encoder_object, TRUE);
		}

		if (decoding) {
			context->decoder_object = lpc10_decode_init(context->decoder_object, TRUE);
		}

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_lpc10_destroy(switch_codec_t *codec)
{
	struct lpc10_context *context = codec->private_info;
	codec->private_info = NULL;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (context->encoder_object)
		lpc10_encode_free(context->encoder_object);
	context->encoder_object = NULL;
	if (context->decoder_object)
		lpc10_decode_free(context->decoder_object);
	context->decoder_object = NULL;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_lpc10_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										   unsigned int *flag)
{
	struct lpc10_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = lpc10_encode(context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_lpc10_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
	struct lpc10_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * lpc10_decode(context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

/*  LPC10     - END */


/*  GSM       - START */
struct gsm_context {
	gsm0610_state_t *decoder_object;
	gsm0610_state_t *encoder_object;
};

static switch_status_t switch_gsm_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct gsm_context *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		if (encoding) {
			context->encoder_object = gsm0610_init(context->encoder_object, GSM0610_PACKING_VOIP);
		}
		if (decoding) {
			context->decoder_object = gsm0610_init(context->decoder_object, GSM0610_PACKING_VOIP);
		}

		codec->private_info = context;
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_gsm_encode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *decoded_data,
										 uint32_t decoded_data_len,
										 uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										 unsigned int *flag)
{
	struct gsm_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = gsm0610_encode(context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_gsm_decode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *encoded_data,
										 uint32_t encoded_data_len,
										 uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										 unsigned int *flag)
{
	struct gsm_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * gsm0610_decode(context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_gsm_destroy(switch_codec_t *codec)
{
	struct gsm_context *context = codec->private_info;

	codec->private_info = NULL;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (context->decoder_object)
		gsm0610_free(context->decoder_object);
	context->decoder_object = NULL;
	if (context->encoder_object)
		gsm0610_free(context->encoder_object);
	context->encoder_object = NULL;

	return SWITCH_STATUS_SUCCESS;
}

/*  GSM       - END */

#ifdef ENABLE_G711
/*  G711      - START */
static switch_status_t switch_g711u_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;

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
	uint32_t encoding, decoding;

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

/*  G711      - END */
#endif


/*  G722      - START */

struct g722_context {
	g722_decode_state_t *decoder_object;
	g722_encode_state_t *encoder_object;
};

static switch_status_t switch_g722_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct g722_context *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct g722_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		if (encoding) {
			context->encoder_object = g722_encode_init(context->encoder_object, 64000, G722_PACKED);
		}
		if (decoding) {
			context->decoder_object = g722_decode_init(context->decoder_object, 64000, G722_PACKED);
		}
	}

	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g722_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct g722_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = g722_encode(context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g722_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct g722_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * g722_decode(context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g722_destroy(switch_codec_t *codec)
{
	struct g722_context *context = codec->private_info;

	codec->private_info = NULL;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (context->decoder_object)
		g722_decode_free(context->decoder_object);
	context->decoder_object = NULL;
	if (context->encoder_object)
		g722_encode_free(context->encoder_object);
	context->encoder_object = NULL;

	return SWITCH_STATUS_SUCCESS;
}

/*  G722      - END */

/*  G726      - START */

static switch_status_t switch_g726_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	int packing = G726_PACKING_RIGHT;
	g726_state_t *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((flags & SWITCH_CODEC_FLAG_AAL2 || strstr(codec->implementation->iananame, "AAL2"))) {
		packing = G726_PACKING_LEFT;
	}

	context = g726_init(context, codec->implementation->bits_per_second, G726_ENCODING_LINEAR, packing);

	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t switch_g726_destroy(switch_codec_t *codec)
{
	g726_state_t *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	g726_free(context);

	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g726_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
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
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	g726_state_t *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * g726_decode(context, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

/*  G726      - START */

/*  IMA_ADPCM - START */

struct ima_adpcm_context {
	ima_adpcm_state_t *decoder_object;
	ima_adpcm_state_t *encoder_object;
};

static switch_status_t switch_adpcm_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct ima_adpcm_context *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		if (encoding) {
			context->encoder_object = ima_adpcm_init(context->encoder_object, IMA_ADPCM_DVI4, 0);
		}
		if (decoding) {
			context->decoder_object = ima_adpcm_init(context->decoder_object, IMA_ADPCM_DVI4, 0);
		}

		codec->private_info = context;
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_adpcm_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										   unsigned int *flag)
{
	struct ima_adpcm_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = ima_adpcm_encode(context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_adpcm_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
	struct ima_adpcm_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * ima_adpcm_decode(context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_adpcm_destroy(switch_codec_t *codec)
{
	struct ima_adpcm_context *context = codec->private_info;

	codec->private_info = NULL;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (context->decoder_object)
		ima_adpcm_free(context->decoder_object);
	context->decoder_object = NULL;
	if (context->encoder_object)
		ima_adpcm_free(context->encoder_object);
	context->encoder_object = NULL;

	return SWITCH_STATUS_SUCCESS;
}

/*  IMA_ADPCM - END */


SWITCH_MODULE_LOAD_FUNCTION(mod_voipcodecs_load)
{
	switch_codec_interface_t *codec_interface;
	int mpf, spf, bpf, ebpf, count;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* IMA_ADPCM */
	mpf = 10000, spf = 80, bpf = 160, ebpf = 80;
	SWITCH_ADD_CODEC(codec_interface, "ADPCM (IMA)");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 5,	/* the IANA code number */
											 "DVI4",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 32000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 (ebpf * count) + 4,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 spf * count,	/* number of frames per network packet */
											 switch_adpcm_init,	/* function to initialize a codec handle using this implementation */
											 switch_adpcm_encode,	/* function to encode raw data into encoded data */
											 switch_adpcm_decode,	/* function to decode encoded data into raw data */
											 switch_adpcm_destroy);	/* deinitalize a codec handle using this implementation */
	}
	mpf = 10000, spf = 160, bpf = 320, ebpf = 160;
	for (count = 6; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 6,	/* the IANA code number */
											 "DVI4",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 16000,	/* samples transferred per second */
											 16000,	/* actual samples transferred per second */
											 64000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 (ebpf * count) + 4,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 spf * count,	/* number of frames per network packet */
											 switch_adpcm_init,	/* function to initialize a codec handle using this implementation */
											 switch_adpcm_encode,	/* function to encode raw data into encoded data */
											 switch_adpcm_decode,	/* function to decode encoded data into raw data */
											 switch_adpcm_destroy);	/* deinitalize a codec handle using this implementation */
	}

	/* G726 */
	mpf = 10000, spf = 80, bpf = 160, ebpf = 20;
	SWITCH_ADD_CODEC(codec_interface, "G.726 16k (AAL2)");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 124,	/* the IANA code number */
											 "AAL2-G726-16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 16000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 count * 10,	/* number of frames per network packet */
											 switch_g726_init,	/* function to initialize a codec handle using this implementation */
											 switch_g726_encode,	/* function to encode raw data into encoded data */
											 switch_g726_decode,	/* function to decode encoded data into raw data */
											 switch_g726_destroy);	/* deinitalize a codec handle using this implementation */
	}
	SWITCH_ADD_CODEC(codec_interface, "G.726 16k");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 124,	/* the IANA code number */
											 "G726-16",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 16000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 count * 10,	/* number of frames per network packet */
											 switch_g726_init,	/* function to initialize a codec handle using this implementation */
											 switch_g726_encode,	/* function to encode raw data into encoded data */
											 switch_g726_decode,	/* function to decode encoded data into raw data */
											 switch_g726_destroy);	/* deinitalize a codec handle using this implementation */
	}
	/* Increase encoded bytes per frame by 10 */
	ebpf = ebpf + 10;

	SWITCH_ADD_CODEC(codec_interface, "G.726 24k (AAL2)");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 123,	/* the IANA code number */
											 "AAL2-G726-24",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 24000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 count * 10,	/* number of frames per network packet */
											 switch_g726_init,	/* function to initialize a codec handle using this implementation */
											 switch_g726_encode,	/* function to encode raw data into encoded data */
											 switch_g726_decode,	/* function to decode encoded data into raw data */
											 switch_g726_destroy);	/* deinitalize a codec handle using this implementation */
	}

	SWITCH_ADD_CODEC(codec_interface, "G.726 24k");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 123,	/* the IANA code number */
											 "G726-24",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 24000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 count * 10,	/* number of frames per network packet */
											 switch_g726_init,	/* function to initialize a codec handle using this implementation */
											 switch_g726_encode,	/* function to encode raw data into encoded data */
											 switch_g726_decode,	/* function to decode encoded data into raw data */
											 switch_g726_destroy);	/* deinitalize a codec handle using this implementation */
	}
	/* Increase encoded bytes per frame by 10 */
	ebpf = ebpf + 10;

	SWITCH_ADD_CODEC(codec_interface, "G.726 32k (AAL2)");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 122,	/* the IANA code number */
											 "AAL2-G726-32",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 32000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 count * 10,	/* number of frames per network packet */
											 switch_g726_init,	/* function to initialize a codec handle using this implementation */
											 switch_g726_encode,	/* function to encode raw data into encoded data */
											 switch_g726_decode,	/* function to decode encoded data into raw data */
											 switch_g726_destroy);	/* deinitalize a codec handle using this implementation */
	}
	SWITCH_ADD_CODEC(codec_interface, "G.726 32k");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 122,	/* the IANA code number */
											 "G726-32",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 32000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 count * 10,	/* number of frames per network packet */
											 switch_g726_init,	/* function to initialize a codec handle using this implementation */
											 switch_g726_encode,	/* function to encode raw data into encoded data */
											 switch_g726_decode,	/* function to decode encoded data into raw data */
											 switch_g726_destroy);	/* deinitalize a codec handle using this implementation */
	}
	/* Increase encoded bytes per frame by 10 */
	ebpf = ebpf + 10;

	SWITCH_ADD_CODEC(codec_interface, "G.726 40k (AAL2)");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 121,	/* the IANA code number */
											 "AAL2-G726-40",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 40000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 count * 10,	/* number of frames per network packet */
											 switch_g726_init,	/* function to initialize a codec handle using this implementation */
											 switch_g726_encode,	/* function to encode raw data into encoded data */
											 switch_g726_decode,	/* function to decode encoded data into raw data */
											 switch_g726_destroy);	/* deinitalize a codec handle using this implementation */
	}
	SWITCH_ADD_CODEC(codec_interface, "G.726 40k");
	for (count = 12; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 121,	/* the IANA code number */
											 "G726-40",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 40000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 count * 10,	/* number of frames per network packet */
											 switch_g726_init,	/* function to initialize a codec handle using this implementation */
											 switch_g726_encode,	/* function to encode raw data into encoded data */
											 switch_g726_decode,	/* function to decode encoded data into raw data */
											 switch_g726_destroy);	/* deinitalize a codec handle using this implementation */
	}
	/* G722 */
	mpf = 10000, spf = 80, bpf = 320, ebpf = 80;
	SWITCH_ADD_CODEC(codec_interface, "G.722");
	for (count = 6; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 9,	/* the IANA code number */
											 "G722",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 16000,	/* actual samples transferred per second */
											 64000,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 spf * count,	/* number of frames per network packet */
											 switch_g722_init,	/* function to initialize a codec handle using this implementation */
											 switch_g722_encode,	/* function to encode raw data into encoded data */
											 switch_g722_decode,	/* function to decode encoded data into raw data */
											 switch_g722_destroy);	/* deinitalize a codec handle using this implementation */
	}

#ifdef ENABLE_G711
	/* G711 */
	mpf = 10000, spf = 80, bpf = 160, ebpf = 80;
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
#endif

	/* GSM */
	mpf = 20000, spf = 160, bpf = 320, ebpf = 33;
	SWITCH_ADD_CODEC(codec_interface, "GSM");
	for (count = 6; count > 0; count--) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 3,	/* the IANA code number */
											 "GSM",	/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 8000,	/* samples transferred per second */
											 8000,	/* actual samples transferred per second */
											 13200,	/* bits transferred per second */
											 mpf * count,	/* number of microseconds per frame */
											 spf * count,	/* number of samples per frame */
											 bpf * count,	/* number of bytes per frame decompressed */
											 ebpf * count,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 count,	/* number of frames per network packet */
											 switch_gsm_init,	/* function to initialize a codec handle using this implementation */
											 switch_gsm_encode,	/* function to encode raw data into encoded data */
											 switch_gsm_decode,	/* function to decode encoded data into raw data */
											 switch_gsm_destroy);	/* deinitalize a codec handle using this implementation */
	}
	/* LPC10 */
#if SWITCH_MAX_INTERVAL >= 90
	SWITCH_ADD_CODEC(codec_interface, "LPC-10");
	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 7,	/* the IANA code number */
										 "LPC",	/* the IANA code name */
										 NULL,	/* default fmtp to send (can be overridden by the init function) */
										 8000,	/* samples transferred per second */
										 8000,	/* actual samples transferred per second */
										 2400,	/* bits transferred per second */
										 90000,	/* number of microseconds per frame */
										 720,	/* number of samples per frame */
										 1440,	/* number of bytes per frame decompressed */
										 28,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 4,	/* number of frames per network packet */
										 switch_lpc10_init,	/* function to initialize a codec handle using this implementation */
										 switch_lpc10_encode,	/* function to encode raw data into encoded data */
										 switch_lpc10_decode,	/* function to decode encoded data into raw data */
										 switch_lpc10_destroy);	/* deinitalize a codec handle using this implementation */
#endif
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
