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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / G722 codec module
 *
 * The Initial Developer of the Original Code is
 * Brian K. West <brian.west@mac.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brian K. West <brian.west@mac.com>
 * Anthony Minessale II <anthmct@yahoo.com>
 * Michael Jerris <mike@jerris.com>
 *
 * mod_voipcodecs.c -- VoIP Codecs (G.711, G.722, G.726, GSM-FR, IMA_ADPCM, LPC10)
 *
 * This module wouldn't be possible without generous contributions from Steve Underwood.  Thanks!
 *
 */

#include <switch.h>
#include "voipcodecs.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_voipcodecs_load);
SWITCH_MODULE_DEFINITION(mod_voipcodecs, mod_voipcodecs_load, NULL, NULL);

/*  LPC10     - START */

struct lpc10_context {
	lpc10_encode_state_t encoder_object;
	lpc10_decode_state_t decoder_object;
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
			lpc10_encode_init(&context->encoder_object, TRUE);
		}

		if (decoding) {
			lpc10_decode_init(&context->decoder_object, TRUE);
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

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = lpc10_encode(&context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

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

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * lpc10_decode(&context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

/*  LPC10     - END */


/*  GSM       - START */
struct gsm_context {
	gsm0610_state_t decoder_object;
	gsm0610_state_t encoder_object;
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
			gsm0610_init(&context->encoder_object, GSM0610_PACKING_VOIP);
		}
		if (decoding) {
			gsm0610_init(&context->decoder_object, GSM0610_PACKING_VOIP);
 		}
		
		codec->private_info = context;
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_gsm_encode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *decoded_data,
										 uint32_t decoded_data_len,
										 uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
										 unsigned int *flag)
{
	struct gsm_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = gsm0610_encode(&context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 320);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_gsm_decode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *encoded_data,
										 uint32_t encoded_data_len,
										 uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
										 unsigned int *flag)
{
	struct gsm_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * gsm0610_decode(&context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len / 33));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_gsm_destroy(switch_codec_t *codec)
{
	/* We do not need to use release here as the pool memory is taken care of for us */
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}
/*  GSM       - END */

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
										   uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
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
										   uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
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
										   uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
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
										   uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
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

/*  G711      - END */

/*  G722      - START */

struct g722_context {
	g722_decode_state_t decoder_object;
	g722_encode_state_t encoder_object;
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
			g722_encode_init(&context->encoder_object, 64000, G722_PACKED);
		}
		if (decoding) {
			g722_decode_init(&context->decoder_object, 64000, G722_PACKED);
		}
	}

	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g722_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
										  unsigned int *flag)
{
	struct g722_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = g722_encode(&context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g722_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
										  unsigned int *flag)
{
	struct g722_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * g722_decode(&context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_g722_destroy(switch_codec_t *codec)
{
	/* We do not need to use release here as the pool memory is taken care of for us */
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

/*  G722      - END */

/*  G726      - START */

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

/*  G726      - START */

/*  IMA_ADPCM - START */

struct ima_adpcm_context {
	ima_adpcm_state_t decoder_object;
	ima_adpcm_state_t encoder_object;
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
			ima_adpcm_init(&context->encoder_object, IMA_ADPCM_DVI4, 0);
		}
		if (decoding) {
			ima_adpcm_init(&context->decoder_object, IMA_ADPCM_DVI4, 0);
 		}
		
		codec->private_info = context;
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_adpcm_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t * encoded_data_len, uint32_t * encoded_rate,
										   unsigned int *flag)
{
	struct ima_adpcm_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = ima_adpcm_encode(&context->encoder_object, (uint8_t *) encoded_data, (int16_t *) decoded_data, decoded_data_len / 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_adpcm_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t * decoded_data_len, uint32_t * decoded_rate,
										   unsigned int *flag)
{
	struct ima_adpcm_context *context = codec->private_info;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	*decoded_data_len = (2 * ima_adpcm_decode(&context->decoder_object, (int16_t *) decoded_data, (uint8_t *) encoded_data, encoded_data_len));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_adpcm_destroy(switch_codec_t *codec)
{
	/* We do not need to use release here as the pool memory is taken care of for us */
	codec->private_info = NULL;
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
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 5, "DVI4", NULL, 8000, 8000, 32000,
                                             mpf * count, spf * count, bpf * count, (ebpf * count) + 4, 1, 1, 12,
                                             switch_adpcm_init, switch_adpcm_encode, switch_adpcm_decode, switch_adpcm_destroy);
    }
	mpf = 10000, spf = 160, bpf = 320, ebpf = 160;
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 6, "DVI4", NULL, 16000, 16000, 64000,
                                             mpf * count, spf * count, bpf * count, (ebpf * count) + 4, 1, 1, 12,
                                             switch_adpcm_init, switch_adpcm_encode, switch_adpcm_decode, switch_adpcm_destroy);
    }

	/* G726 */
	mpf = 10000, spf = 80, bpf = 160, ebpf = 20;
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

	/* G722 */
	mpf = 10000, spf = 80, bpf = 320, ebpf = 80;
	SWITCH_ADD_CODEC(codec_interface, "G.722");
    for (count = 12; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 9, "G722", NULL, 8000, 16000, 64000,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 12,
                                             switch_g722_init, switch_g722_encode, switch_g722_decode, switch_g722_destroy);
    }

	/* G711 */
	mpf = 10000, spf = 80, bpf = 160, ebpf = 80;
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

	/* GSM */
	mpf = 20000, spf = 160, bpf = 320, ebpf = 33;
	SWITCH_ADD_CODEC(codec_interface, "GSM");
    for (count = 6; count > 0; count--) {
        switch_core_codec_add_implementation(pool, codec_interface,
                                             SWITCH_CODEC_TYPE_AUDIO, 3, "GSM", NULL, 8000, 8000, 13200,
                                             mpf * count, spf * count, bpf * count, ebpf * count, 1, 1, 6,
                                             switch_gsm_init, switch_gsm_encode, switch_gsm_decode, switch_gsm_destroy);
    }
	/* LPC10 */
	SWITCH_ADD_CODEC(codec_interface, "LPC-10");
    switch_core_codec_add_implementation(pool, codec_interface,
                                         SWITCH_CODEC_TYPE_AUDIO, 7, "LPC", NULL, 8000, 8000, 2400,
										 90000, 720, 1440, 28, 1, 1, 1,
                                         switch_lpc10_init, switch_lpc10_encode, switch_lpc10_decode, switch_lpc10_destroy);
	
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
