/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Brian K. West <brian@freeswitch.org>
 *
 * mod_opus.c -- The OPUS ultra-low delay audio codec (http://www.opus-codec.org/)
 *
 */

#include "switch.h"
#include "opus.h"


SWITCH_MODULE_LOAD_FUNCTION(mod_opus_load);
SWITCH_MODULE_DEFINITION(mod_opus, mod_opus_load, NULL, NULL);

struct opus_context {
	OpusEncoder *encoder_object;
	OpusDecoder *decoder_object;
	int frame_size;
};

static switch_status_t switch_opus_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct opus_context *context = NULL;
	int encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}

	context->frame_size = codec->implementation->samples_per_packet;
	
	if (encoding) {
		/* come up with a way to specify these */
		int bitrate_bps = codec->implementation->bits_per_second;
		int use_vbr = 1;
		int complexity = 10;
		int use_inbandfec = 1;
		int use_dtx = 1;
		int bandwidth = OPUS_BANDWIDTH_FULLBAND;
		int err;

		context->encoder_object = opus_encoder_create(codec->implementation->actual_samples_per_second, 
													  codec->implementation->number_of_channels, OPUS_APPLICATION_VOIP, &err);

       if (err != OPUS_OK) {
		   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create encoder: %s\n", opus_strerror(err));
		   return SWITCH_STATUS_GENERR;
       }

		opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(bitrate_bps));
		opus_encoder_ctl(context->encoder_object, OPUS_SET_BANDWIDTH(bandwidth));
		opus_encoder_ctl(context->encoder_object, OPUS_SET_VBR(use_vbr));
		opus_encoder_ctl(context->encoder_object, OPUS_SET_COMPLEXITY(complexity));
		opus_encoder_ctl(context->encoder_object, OPUS_SET_INBAND_FEC(use_inbandfec));
		opus_encoder_ctl(context->encoder_object, OPUS_SET_DTX(use_dtx));

	}
	
	if (decoding) {
		int err;

		context->decoder_object = opus_decoder_create(codec->implementation->actual_samples_per_second, 
													  codec->implementation->number_of_channels, &err);

		
		if (err != OPUS_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create decoder: %s\n", opus_strerror(err));

			if (context->encoder_object) {
				opus_encoder_destroy(context->encoder_object);
				context->encoder_object = NULL;
			}

			return SWITCH_STATUS_GENERR;
		}

	}

	codec->private_info = context;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opus_destroy(switch_codec_t *codec)
{
	struct opus_context *context = codec->private_info;

	if (context) {
		if (context->decoder_object) {
			opus_decoder_destroy(context->decoder_object);
			context->decoder_object = NULL;
		}
		if (context->encoder_object) {
			opus_encoder_destroy(context->encoder_object);
			context->encoder_object = NULL;
		}
	}

	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opus_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct opus_context *context = codec->private_info;
	int bytes = 0;
	int len = (int) *encoded_data_len;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (len > 1275) len = 1275;

	bytes = opus_encode(context->encoder_object, (void *) decoded_data, decoded_data_len / 2, (unsigned char *) encoded_data, len);
	
	if (bytes > 0) {
		*encoded_data_len = (uint32_t) bytes;
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoder Error!\n");
	return SWITCH_STATUS_GENERR;
}

static switch_status_t switch_opus_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct opus_context *context = codec->private_info;
	int samples = 0;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}
	
	samples = opus_decode(context->decoder_object, (*flag & SFF_PLC) ? NULL : encoded_data, encoded_data_len, decoded_data, *decoded_data_len, 0);

	if (samples < 0) {
		return SWITCH_STATUS_GENERR;
	}

	*decoded_data_len = samples * 2;
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_opus_load)
{
	switch_codec_interface_t *codec_interface;
	int samples = 480;
	int bytes = 960;
	int mss = 10000;
	int x = 0;
	int rate = 48000;
	int bits = 32000;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "OPUS (STANDARD)");

	for (x = 0; x < 3; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 rate,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
		
		bytes *= 2;
		samples *= 2;
		mss *= 2;

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
