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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library
 *
 * The Initial Developer of the Original Code is
 * Brian West <brian@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Mathieu Rene <mrene@avgs.ca>
 * Dragos Oancea <droancea@yahoo.com>
 *
 * mod_codec2 -- FreeSWITCH CODEC2 Module
 *
 */

#include <switch.h>
#include <codec2.h>

/* Uncomment to log input/output data for debugging 
#define LOG_DATA 
#define CODEC2_DEBUG
*/

#define CODEC2_SAMPLES_PER_FRAME 160

#ifdef CODEC2_DEBUG
#define codec2_assert(_x) switch_assert(_x)
#else
#define codec2_assert(_x) 
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_codec2_load);

SWITCH_MODULE_DEFINITION(mod_codec2, mod_codec2_load, NULL, NULL);

struct codec2_context {
	void *encoder;
	void *decoder;
	int mode; /* codec2 operation mode */
	int nbit; /* nr of bits per frame */
	int nbyte; /* nr of bytes per frame */
	int nsam; /* nr of samples per frame */
#ifdef LOG_DATA	
	FILE *encoder_in;
	FILE *encoder_out;
	FILE *encoder_out_unpacked;
	FILE *decoder_in;
	FILE *decoder_in_unpacked;
	FILE *decoder_out;
#endif
};

struct {
	int mode;
	int ptime;
	int samples_per_frame;
} codec2_prefs;

#ifdef LOG_DATA
static int c2_count = 0;
#endif

static switch_status_t switch_codec2_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct codec2_context *context = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context)))) {
		return SWITCH_STATUS_FALSE;
	}

	if (!codec2_prefs.mode) {
		codec2_prefs.mode = CODEC2_MODE_2400;
	}

	if (codec2_prefs.mode == 3200) {
			context->mode = CODEC2_MODE_3200;
	} else if (codec2_prefs.mode == 2400) {
			context->mode = CODEC2_MODE_2400;
	} else if (codec2_prefs.mode == 1400) {
			context->mode = CODEC2_MODE_1400;
	} else if (codec2_prefs.mode == 1200) {
			context->mode = CODEC2_MODE_1200;
	} else {
		/* 3200 might be better for VOIP, but forcing 2400 for backwards compatibility  */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mode not supported, forcing CODEC2_MODE_2400. You can try mode 3200 too!\n");
		context->mode = CODEC2_MODE_2400; 
	}
	
	if (encoding) {
		context->encoder = codec2_create(context->mode);
	}
	
	if (decoding) {
		context->decoder = codec2_create(context->mode);
	}

	context->nsam = codec2_samples_per_frame(context->encoder);
	if (!context->nsam) {
		context->nsam = CODEC2_SAMPLES_PER_FRAME;
	}
	context->nbit = codec2_bits_per_frame(context->encoder);
	context->nbyte = (context->nbit + 7) / 8;

	codec->private_info = context;
	
#ifdef LOG_DATA		
	{
		
		int c = c2_count++;
		char buf[1024];
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Logging as /tmp/c2-%d-*\n", c);
		
		if (encoding) {
			snprintf(buf, sizeof(buf), "/tmp/c2-%d-enc-in", c);
			context->encoder_in = fopen(buf, "w");
		
			snprintf(buf, sizeof(buf), "/tmp/c2-%d-enc-out", c);
			context->encoder_out = fopen(buf, "w");
		
			snprintf(buf, sizeof(buf), "/tmp/c2-%d-enc-out-unpacked", c);
			context->encoder_out_unpacked = fopen(buf, "w");
		}
		if (decoding) {
			snprintf(buf, sizeof(buf), "/tmp/c2-%d-dec-in", c);
			context->decoder_in = fopen(buf, "w");
		
			snprintf(buf, sizeof(buf), "/tmp/c2-%d-dec-out", c);
			context->decoder_out = fopen(buf, "w");
		
			snprintf(buf, sizeof(buf), "/tmp/c2-%d-dec-out-unpacked", c);
			context->decoder_in_unpacked = fopen(buf, "w");
		}
	}
#endif

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_codec2_encode(switch_codec_t *codec, switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate,
										  void *encoded_data,
										  uint32_t *encoded_data_len,
										  uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct codec2_context *context = codec->private_info;
	
	codec2_assert(decoded_data_len == context->nsam * 2);
	
#ifdef LOG_DATA	
	fwrite(decoded_data, decoded_data_len, 1, context->encoder_in);
	fflush(context->encoder_in);
#endif

	codec2_encode(context->encoder, encoded_data, decoded_data);
	
#ifdef LOG_DATA	
	fwrite(encode_buf, sizeof(encode_buf), 1, context->encoder_out_unpacked);
	fflush(context->encoder_out_unpacked);
	fwrite(encoded_data, context->nbyte, 1, context->encoder_out);
	fflush(context->encoder_out);
#endif
	
	*encoded_data_len = context->nbyte;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_codec2_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate,
										  void *decoded_data,
										  uint32_t *decoded_data_len,
										  uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct codec2_context *context = codec->private_info;
	
	codec2_assert(encoded_data_len == 8);
	
#ifdef LOG_DATA	
	fwrite(encoded_data, encoded_data_len, 1, context->decoder_in);
	fflush(context->decoder_in);
	fwrite(bits, sizeof(bits), 1, context->decoder_in_unpacked);
	fflush(context->decoder_in_unpacked);
#endif
	
	codec2_decode(context->decoder, decoded_data, encoded_data);

#ifdef LOG_DATA	
	fwrite(decoded_data, context->nsam, 2, context->decoder_out);
	fflush(context->decoder_out);
#endif

	*decoded_data_len = context->nsam * 2; /* eg: 160 samples for 3200,2400 */
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_codec2_destroy(switch_codec_t *codec)
{
	struct codec2_context *context = codec->private_info;
	
	codec2_destroy(context->encoder);
	codec2_destroy(context->decoder);

	context->encoder = NULL;
	context->decoder = NULL;

#ifdef LOG_DATA
	if (context->encoder_in) {
		fclose(context->encoder_in);
	}
	if (context->encoder_out) {
		fclose(context->encoder_out);
	}
	if (context->encoder_out_unpacked) {
		fclose(context->encoder_out_unpacked);
	}
	if (context->decoder_in) {
		fclose(context->decoder_in);
	}
	if (context->decoder_in_unpacked) {
		fclose(context->decoder_in_unpacked);
	}
	if (context->decoder_out) {
		fclose(context->decoder_out);
	}
#endif

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t codec2_load_config(switch_bool_t reload) {

	char *cf = "codec2.conf";
	switch_xml_t cfg, xml = NULL, param, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	codec2_prefs.mode = 2400 ; 

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Opening of %s failed\n", cf);
		return status;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *key = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(key, "mode") && !zstr(val)) {
				codec2_prefs.mode = atoi(val);
			}
		}
	}
	
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_codec2_load)
{
	switch_codec_interface_t *codec_interface;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if ((status = codec2_load_config(SWITCH_FALSE)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	/*there is no API call to retrieve ptime per mode, so hardcoding here*/
	if ((codec2_prefs.mode == 3200) ||(codec2_prefs.mode == 2400)) {
		codec2_prefs.ptime = 20000;
		codec2_prefs.samples_per_frame = CODEC2_SAMPLES_PER_FRAME;  
	} else {
		codec2_prefs.ptime = 40000;
		codec2_prefs.samples_per_frame = CODEC2_SAMPLES_PER_FRAME * 2;  
	}

	SWITCH_ADD_CODEC(codec_interface, "CODEC2 3200/2400/1400/1200bps");

	switch_core_codec_add_implementation(pool, codec_interface,
							 SWITCH_CODEC_TYPE_AUDIO,
							 111,
							 "CODEC2",
							 NULL,
							 8000, /* samples/sec */
							 8000, /* samples/sec */
							 codec2_prefs.mode, /* bps */
							 codec2_prefs.ptime, /* ptime */
							 codec2_prefs.samples_per_frame,	/* samples decoded */
							 codec2_prefs.samples_per_frame * 2,	/* bytes decoded */
							 0,	/* bytes encoded */
							 1,	/* channels */
							 1,	/* frames/packet */
							 switch_codec2_init,
							 switch_codec2_encode,
							 switch_codec2_decode,
							 switch_codec2_destroy);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
