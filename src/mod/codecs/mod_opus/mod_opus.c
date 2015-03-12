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
 * Brian K. West <brian@freeswitch.org>
 * Noel Morgan <noel@vwci.com>
 *
 * mod_opus.c -- The OPUS ultra-low delay audio codec (http://www.opus-codec.org/)
 *
 */

#include "switch.h"
#include "opus.h"


SWITCH_MODULE_LOAD_FUNCTION(mod_opus_load);
SWITCH_MODULE_DEFINITION(mod_opus, mod_opus_load, NULL, NULL);

/*! \brief Various codec settings */
struct opus_codec_settings {
	int useinbandfec;
	int usedtx;
	int maxaveragebitrate;
	int maxplaybackrate;
	int stereo;
	int cbr;
	int sprop_maxcapturerate;
	int sprop_stereo;
	int maxptime;
	int minptime;
	int ptime;
	int samplerate;
};
typedef struct opus_codec_settings opus_codec_settings_t;

static opus_codec_settings_t default_codec_settings = {
	/*.useinbandfec */ 1,
	/*.usedtx */ 1,
	/*.maxaveragebitrate */ 30000,
	/*.maxplaybackrate */ 48000,
	/*.stereo*/ 0,
	/*.cbr*/ 0,
	/*.sprop_maxcapturerate*/ 0,
	/*.sprop_stereo*/ 0,
	/*.maxptime*/ 0,
	/*.minptime*/ 0,
	/*.ptime*/ 0,
	/*.samplerate*/ 0
};

struct opus_context {
	OpusEncoder *encoder_object;
	OpusDecoder *decoder_object;
	uint32_t enc_frame_size;
	uint32_t dec_frame_size;
};

struct {
    int use_vbr;
    int complexity;
    int maxaveragebitrate;
    int maxplaybackrate;
    switch_mutex_t *mutex;
} opus_prefs;


static switch_status_t switch_opus_fmtp_parse(const char *fmtp, switch_codec_fmtp_t *codec_fmtp)
{
	if (codec_fmtp) {
		opus_codec_settings_t local_settings = { 0 };
		opus_codec_settings_t *codec_settings = &local_settings;
        
		if (codec_fmtp->private_info) {
			codec_settings = codec_fmtp->private_info;
			if (zstr(fmtp)) {
				memcpy(codec_settings, &default_codec_settings, sizeof(*codec_settings));
			}
		}
        
		if (fmtp) {
			int x, argc;
			char *argv[10];
			char *fmtp_dup = strdup(fmtp);
            
			switch_assert(fmtp_dup);
            
			argc = switch_separate_string(fmtp_dup, ';', argv, (sizeof(argv) / sizeof(argv[0])));
			for (x = 0; x < argc; x++) {
				char *data = argv[x];
				char *arg;
				switch_assert(data);
				while (*data == ' ') {
					data++;
				}
				
                
				if ((arg = strchr(data, '='))) {
					*arg++ = '\0';
                    
					if (codec_settings) {
						if (!strcasecmp(data, "useinbandfec")) {
							codec_settings->useinbandfec = switch_true(arg);
						}
                        
						if (!strcasecmp(data, "usedtx")) {
							codec_settings->usedtx = switch_true(arg);
						}
                        
						if (!strcasecmp(data, "sprop-maxcapturerate")) {
							codec_settings->sprop_maxcapturerate = atoi(arg);
						}
                        
						if (!strcasecmp(data, "maxptime")) {
							codec_settings->maxptime = atoi(arg);
						}
                        
						if (!strcasecmp(data, "minptime")) {
							codec_settings->minptime = atoi(arg);
						}
						
						if (!strcasecmp(data, "ptime")) {
							codec_settings->ptime = atoi(arg);
							codec_fmtp->microseconds_per_packet = codec_settings->ptime * 1000;
						}
                        
						if (!strcasecmp(data, "samplerate")) {
							codec_settings->samplerate = atoi(arg);
							codec_fmtp->actual_samples_per_second = codec_settings->samplerate;
						}

						if (!strcasecmp(data, "stereo")) {
							codec_settings->stereo = atoi(arg);
							codec_fmtp->stereo = codec_settings->stereo;
						}
                        
						if (!strcasecmp(data, "maxaveragebitrate")) {
							codec_settings->maxaveragebitrate = atoi(arg);
							if ( codec_settings->maxaveragebitrate < 6000 || codec_settings->maxaveragebitrate > 510000 ) {
								codec_settings->maxaveragebitrate = 0; /* values outside the range between 6000 and 510000 SHOULD be ignored */
							}
						}
                        
						if (!strcasecmp(data, "maxplaybackrate")) {
							codec_settings->maxplaybackrate = atoi(arg);
							if ( codec_settings->maxplaybackrate != 8000 && codec_settings->maxplaybackrate != 12000 && codec_settings->maxplaybackrate != 16000
									&& codec_settings->maxplaybackrate != 24000 && codec_settings->maxplaybackrate != 48000) {
								codec_settings->maxplaybackrate = 0; /* value not supported */
							}
						}
					}
				}
			}
			free(fmtp_dup);
		}
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

static char *gen_fmtp(opus_codec_settings_t *settings, switch_memory_pool_t *pool)
{
	char buf[256] = "";
    
	if (settings->useinbandfec) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "useinbandfec=1; ");
	}
    
	if (settings->usedtx) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "usedtx=1; ");
	}
    
	if (settings->maxaveragebitrate) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "maxaveragebitrate=%d; ", settings->maxaveragebitrate);
	}
    
	if (settings->maxplaybackrate) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "maxplaybackrate=%d; ", settings->maxplaybackrate);
	}
    
	if (settings->ptime) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "ptime=%d; ", settings->ptime);
	}
    
	if (settings->minptime) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "minptime=%d; ", settings->minptime);
	}

	if (settings->maxptime) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "maxptime=%d; ", settings->maxptime);
	}
    
	if (settings->samplerate) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "samplerate=%d; ", settings->samplerate);
	}

	if (settings->stereo) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "stereo=%d; ", settings->stereo);
	}

	if (end_of(buf) == ' ') {
		*(end_of_p(buf) - 1) = '\0';
	}
	
	return switch_core_strdup(pool, buf);
    
}

static switch_status_t switch_opus_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct opus_context *context = NULL;
	int encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (flags & SWITCH_CODEC_FLAG_DECODE);
	switch_codec_fmtp_t codec_fmtp;
	opus_codec_settings_t opus_codec_settings = { 0 };
    
	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}
    
	context->enc_frame_size = codec->implementation->actual_samples_per_second * (codec->implementation->microseconds_per_packet / 1000) / 1000;


	memset(&codec_fmtp, '\0', sizeof(struct switch_codec_fmtp));
	codec_fmtp.private_info = &opus_codec_settings;
	switch_opus_fmtp_parse(codec->fmtp_in, &codec_fmtp);

	/* Verify if the local or remote configuration are lowering maxaveragebitrate and/or maxplaybackrate */
	if ( opus_prefs.maxaveragebitrate && (opus_prefs.maxaveragebitrate < opus_codec_settings.maxaveragebitrate || !opus_codec_settings.maxaveragebitrate) ) {
		opus_codec_settings.maxaveragebitrate = opus_prefs.maxaveragebitrate;
	}
	if ( opus_prefs.maxplaybackrate && (opus_prefs.maxplaybackrate < opus_codec_settings.maxplaybackrate || !opus_codec_settings.maxplaybackrate) ) {
		opus_codec_settings.maxplaybackrate = opus_prefs.maxplaybackrate;
	}

	codec->fmtp_out = gen_fmtp(&opus_codec_settings, codec->memory_pool);

	if (encoding) {
		/* come up with a way to specify these */
		int bitrate_bps = OPUS_AUTO;
		int use_vbr = opus_prefs.use_vbr;
		int complexity = opus_prefs.complexity;
		int err;
		int samplerate = opus_codec_settings.samplerate ? opus_codec_settings.samplerate : codec->implementation->actual_samples_per_second;
        
		context->encoder_object = opus_encoder_create(samplerate,
													  codec->implementation->number_of_channels,
													  codec->implementation->number_of_channels == 1 ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO, &err);
        
        if (err != OPUS_OK) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create encoder: %s\n", opus_strerror(err));
            return SWITCH_STATUS_GENERR;
        }
        

		/* Setting documented in "RTP Payload Format for Opus Speech and Audio Codec"  draft-spittka-payload-rtp-opus-03 */
		if( opus_codec_settings.maxaveragebitrate ) { /* Remote codec settings found in SDP "fmtp", we accept to tune the Encoder */
			opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(opus_codec_settings.maxaveragebitrate));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder set bitrate based on maxaveragebitrate found in SDP [%dbps]\n", opus_codec_settings.maxaveragebitrate);
		} else {
			/* Default codec settings used, may have been modified by SDP "samplerate" */
			opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(bitrate_bps));
			if (codec->implementation->actual_samples_per_second == 8000) {
				opus_encoder_ctl(context->encoder_object, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
				opus_encoder_ctl(context->encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
			} else {
				opus_encoder_ctl(context->encoder_object, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder set bitrate to local settings [%dbps]\n", bitrate_bps);
		}

		/* Another setting from "RTP Payload Format for Opus Speech and Audio Codec" */
		if ( opus_codec_settings.maxplaybackrate ) {
			if (opus_codec_settings.maxplaybackrate == 8000) {       /* Audio Bandwidth: 0-4000Hz  Sampling Rate: 8000Hz */
				opus_encoder_ctl(context->encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
			} else if (opus_codec_settings.maxplaybackrate == 12000) { /* Audio Bandwidth: 0-6000Hz  Sampling Rate: 12000Hz */
				opus_encoder_ctl(context->encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_MEDIUMBAND));
			} else if (opus_codec_settings.maxplaybackrate == 16000) { /* Audio Bandwidth: 0-8000Hz  Sampling Rate: 16000Hz */
				opus_encoder_ctl(context->encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
			} else if (opus_codec_settings.maxplaybackrate == 24000) { /* Audio Bandwidth: 0-12000Hz Sampling Rate: 24000Hz */
				opus_encoder_ctl(context->encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_SUPERWIDEBAND));
			} else if (opus_codec_settings.maxplaybackrate == 48000) { /* Audio Bandwidth: 0-20000Hz Sampling Rate: 48000Hz */
				opus_encoder_ctl(context->encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder set bandwidth based on maxplaybackrate found in SDP [%dHz]\n", opus_codec_settings.maxplaybackrate);
		}

		if (use_vbr) {
			opus_encoder_ctl(context->encoder_object, OPUS_SET_VBR(use_vbr));
		}
		if (complexity) {
			opus_encoder_ctl(context->encoder_object, OPUS_SET_COMPLEXITY(complexity));
        }

		if (opus_codec_settings.useinbandfec) {
			opus_encoder_ctl(context->encoder_object, OPUS_SET_INBAND_FEC(opus_codec_settings.useinbandfec));
		}
        
		if (opus_codec_settings.usedtx) {
			opus_encoder_ctl(context->encoder_object, OPUS_SET_DTX(opus_codec_settings.usedtx));
		}
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
	
	bytes = opus_encode(context->encoder_object, (void *) decoded_data, context->enc_frame_size, (unsigned char *) encoded_data, len);
	
	if (bytes > 0) {
		*encoded_data_len = (uint32_t) bytes;
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoder Error: %s Decoded Datalen %u Codec NumberChans %u Len %u DecodedDate %p EncodedData %p ContextEncoderObject %p!\n", opus_strerror(bytes),decoded_data_len,codec->implementation->number_of_channels,len,(void *) decoded_data,(void *) encoded_data,(void *) context->encoder_object);

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
    uint32_t frame_size;
	uint32_t frame_samples;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	frame_samples = *decoded_data_len / 2 / codec->implementation->number_of_channels;
	frame_size = frame_samples - (frame_samples % (codec->implementation->actual_samples_per_second / 400));
	
	samples = opus_decode(context->decoder_object, (*flag & SFF_PLC) ? NULL : encoded_data, encoded_data_len, decoded_data, frame_size, !!(*flag & SFF_PLC));

	if (samples < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Error: %s fs:%u plc:%d!\n", opus_strerror(samples), frame_size, !!(*flag & SFF_PLC));
		return SWITCH_STATUS_GENERR;
	}

	*decoded_data_len = samples * 2 * codec->implementation->number_of_channels;
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opus_load_config(switch_bool_t reload)
{
	char *cf = "opus.conf";
	switch_xml_t cfg, xml = NULL, param, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
    
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Opening of %s failed\n", cf);
		return status;
	}
    
	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *key = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			
			if (!strcasecmp(key, "use-vbr") && !zstr(val)) {
				opus_prefs.use_vbr = atoi(val);
			} else if (!strcasecmp(key, "complexity")) {
				opus_prefs.complexity = atoi(val);
			} else if (!strcasecmp(key, "maxaveragebitrate")) {
				opus_prefs.maxaveragebitrate = atoi(val);
				if ( opus_prefs.maxaveragebitrate < 6000 || opus_prefs.maxaveragebitrate > 510000 ) {
					opus_prefs.maxaveragebitrate = 0; /* values outside the range between 6000 and 510000 SHOULD be ignored */
				}
			} else if (!strcasecmp(key, "maxplaybackrate")) {
				opus_prefs.maxplaybackrate = atoi(val);
				if ( opus_prefs.maxplaybackrate != 8000 && opus_prefs.maxplaybackrate != 12000 && opus_prefs.maxplaybackrate != 16000
							&& opus_prefs.maxplaybackrate != 24000 && opus_prefs.maxplaybackrate != 48000) {
					opus_prefs.maxplaybackrate = 0; /* value not supported */
				}
			}
		}
	}
    
    if (xml) {
        switch_xml_free(xml);
    }
    
    return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_opus_load)
{
	switch_codec_interface_t *codec_interface;
	int samples = 480;
	int bytes = 960;
	int mss = 10000;
	int x = 0;
	int rate = 48000;
	int bits = 0;
	char *dft_fmtp = NULL;
	opus_codec_settings_t settings = { 0 };
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    
	if ((status = opus_load_config(SWITCH_FALSE)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}
    
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
    
	SWITCH_ADD_CODEC(codec_interface, "OPUS (STANDARD)");
    
	codec_interface->parse_fmtp = switch_opus_fmtp_parse;
    
	settings = default_codec_settings;
    
    
	for (x = 0; x < 3; x++) {
        
		settings.ptime = mss / 1000;
		settings.maxptime = settings.ptime;
		settings.minptime = settings.ptime;
		settings.samplerate = rate;
		settings.stereo = 0;
		dft_fmtp = gen_fmtp(&settings, pool);

		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
		
		settings.stereo = 1;
		if (x < 2) {
		dft_fmtp = gen_fmtp(&settings, pool);
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 rate,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes * 2,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 2,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
		}
		bytes *= 2;
		samples *= 2;
		mss *= 2;
        
	}
	
    
	samples = 80;
	bytes = 160;
	mss = 10000;
	rate = 8000;

	for (x = 0; x < 3; x++) {
		settings.stereo = 0;        
		settings.ptime = mss / 1000;
		settings.maxptime = settings.ptime;
		settings.minptime = settings.ptime;
		settings.samplerate = rate;
		dft_fmtp = gen_fmtp(&settings, pool);
        
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */

		settings.stereo = 1;
		dft_fmtp = gen_fmtp(&settings, pool);
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes * 2,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 2,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
        
		bytes += 160;
		samples += 80;
		mss += 10000;
		
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
