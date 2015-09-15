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
 * Brian K. West <brian@freeswitch.org>
 *
 * mod_silk.c -- Skype(tm) SILK Codec Module
 *
 */

#include "switch.h"
#include "SKP_Silk_SDK_API.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_silk_load);
SWITCH_MODULE_DEFINITION(mod_silk, mod_silk_load, NULL, NULL);

#define MAX_BYTES_PER_FRAME		250 
#define MAX_INPUT_FRAMES		5
#define MAX_LBRR_DELAY			2
#define MAX_FRAME_LENGTH		480

/*! \brief Various codec settings */
struct silk_codec_settings {
	SKP_int useinbandfec;
	SKP_int usedtx;
	SKP_int maxaveragebitrate;
	SKP_int plpct;
};
typedef struct silk_codec_settings silk_codec_settings_t;

static silk_codec_settings_t default_codec_settings = {
	/*.useinbandfec */ 1,
	/*.usedtx */ 0,
	/*.maxaveragebitrate */ 0,
	/*.plpct */ 20, // 20% for now
};

struct silk_context {
	SKP_SILK_SDK_EncControlStruct encoder_object;
	SKP_SILK_SDK_DecControlStruct decoder_object;
	void *enc_state;
	void *dec_state;
	SKP_uint8 recbuff[SWITCH_RTP_MAX_BUF_LEN];
	SKP_int16 reclen;
};

static switch_status_t switch_silk_fmtp_parse(const char *fmtp, switch_codec_fmtp_t *codec_fmtp)
{
	if (codec_fmtp) {
		silk_codec_settings_t *codec_settings = NULL;

		if (codec_fmtp->private_info) {
			codec_settings = codec_fmtp->private_info;
			memcpy(codec_settings, &default_codec_settings, sizeof(*codec_settings));
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
							if (switch_true(arg)) {
								codec_settings->useinbandfec = 1;
							}
						}
						if (!strcasecmp(data, "usedtx")) {
							if (switch_true(arg)) {
								codec_settings->usedtx = 1;
							}
						}
						if (!strcasecmp(data, "maxaveragebitrate")) {
							codec_settings->maxaveragebitrate = atoi(arg);
							switch(codec_fmtp->actual_samples_per_second) {
								case 8000:
									{
										if(codec_settings->maxaveragebitrate < 6000 || codec_settings->maxaveragebitrate > 20000) {
											codec_settings->maxaveragebitrate = 20000;
										}
										break;
									}
								case 12000:
									{
										if(codec_settings->maxaveragebitrate < 7000 || codec_settings->maxaveragebitrate > 25000) {
											codec_settings->maxaveragebitrate = 25000;
										}
										break;
									}
								case 16000:
									{
										if(codec_settings->maxaveragebitrate < 8000 || codec_settings->maxaveragebitrate > 30000) {
											codec_settings->maxaveragebitrate = 30000;
										}
										break;
									}
								case 24000:
									{
										if(codec_settings->maxaveragebitrate < 12000 || codec_settings->maxaveragebitrate > 40000) {
											codec_settings->maxaveragebitrate = 40000;
										}
										break;
									}

								default:
									/* this should never happen but 20000 is common among all rates */
									codec_settings->maxaveragebitrate = 20000;
									break;
							}

						}

					}
				}
			}
			free(fmtp_dup);
		}
		//codec_fmtp->bits_per_second = bit_rate;
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}




static switch_status_t switch_silk_init(switch_codec_t *codec, 
										switch_codec_flag_t freeswitch_flags, 
										const switch_codec_settings_t *codec_settings)
{
	struct silk_context *context = NULL;
	switch_codec_fmtp_t codec_fmtp;
	silk_codec_settings_t silk_codec_settings;
	SKP_int32 encSizeBytes;
	SKP_int32 decSizeBytes;
	int encoding = (freeswitch_flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (freeswitch_flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}

	memset(&codec_fmtp, '\0', sizeof(struct switch_codec_fmtp));
	codec_fmtp.private_info = &silk_codec_settings;
	switch_silk_fmtp_parse(codec->fmtp_in, &codec_fmtp);
	
	codec->fmtp_out = switch_core_sprintf(codec->memory_pool, "useinbandfec=%s; usedtx=%s; maxaveragebitrate=%d",
										  silk_codec_settings.useinbandfec ? "1" : "0",
										  silk_codec_settings.usedtx ? "1" : "0",
										  silk_codec_settings.maxaveragebitrate ? silk_codec_settings.maxaveragebitrate : codec->implementation->bits_per_second);

	if (encoding) {
		if (SKP_Silk_SDK_Get_Encoder_Size(&encSizeBytes)) {
			return SWITCH_STATUS_FALSE;
		}
		
		context->enc_state = switch_core_alloc(codec->memory_pool, encSizeBytes);

		if (SKP_Silk_SDK_InitEncoder(context->enc_state, &context->encoder_object)) {
			return SWITCH_STATUS_FALSE;
		}
		

		context->encoder_object.API_sampleRate = codec->implementation->actual_samples_per_second;
		context->encoder_object.maxInternalSampleRate = codec->implementation->actual_samples_per_second;
		context->encoder_object.packetSize = codec->implementation->samples_per_packet;
		context->encoder_object.useInBandFEC = silk_codec_settings.useinbandfec;
		context->encoder_object.complexity = 0;
		context->encoder_object.bitRate = silk_codec_settings.maxaveragebitrate ? silk_codec_settings.maxaveragebitrate : codec->implementation->bits_per_second;
		context->encoder_object.useDTX = silk_codec_settings.usedtx;
		context->encoder_object.packetLossPercentage = silk_codec_settings.plpct;
	}

	if (decoding) {

		switch_set_flag(codec, SWITCH_CODEC_FLAG_HAS_PLC);
		
		if (SKP_Silk_SDK_Get_Decoder_Size(&decSizeBytes)) {
			return SWITCH_STATUS_FALSE;
		}
		context->dec_state = switch_core_alloc(codec->memory_pool, decSizeBytes);

		if (SKP_Silk_SDK_InitDecoder(context->dec_state)) {
			return SWITCH_STATUS_FALSE;
		}
		context->decoder_object.API_sampleRate = codec->implementation->actual_samples_per_second;
	}

	codec->private_info = context;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_silk_destroy(switch_codec_t *codec)
{
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

void printSilkError(SKP_int16 ret){
	char * message;
	switch (ret) {
		case SKP_SILK_NO_ERROR: message = "No errors";
			break;
		case SKP_SILK_ENC_INPUT_INVALID_NO_OF_SAMPLES: 
			message = "Input length is not multiplum of 10 ms, or length is longer than the packet length";
			break;
		case SKP_SILK_ENC_FS_NOT_SUPPORTED: 
			message = "Sampling frequency not 8000 , 12000, 16000 or 24000 Hertz";
			break;
		case SKP_SILK_ENC_PACKET_SIZE_NOT_SUPPORTED:
			message ="Packet size not 20, 40 , 60 , 80 or 100 ms ";
			break; 
		case SKP_SILK_ENC_PAYLOAD_BUF_TOO_SHORT: 
			message = "Allocated payload buffer too short";
			break;
		case SKP_SILK_ENC_INVALID_LOSS_RATE: 
			message = " Loss rate not between  0 and 100 % ";
			break;
		case SKP_SILK_ENC_INVALID_COMPLEXITY_SETTING:
			message = "Complexity setting not valid, use 0 ,1 or 2";
			break;
		case SKP_SILK_ENC_INVALID_INBAND_FEC_SETTING: 
			message = "Inband FEC setting not valid, use 0 or 1	";
			break;
		case SKP_SILK_ENC_INVALID_DTX_SETTING:
			message = "DTX setting not valid, use 0 or 1";
			break;
		case SKP_SILK_ENC_INTERNAL_ERROR:
			message = "Internal Encoder Error ";
			break;
		case SKP_SILK_DEC_INVALID_SAMPLING_FREQUENCY:
			message = "Output sampling frequency lower than internal decoded sampling frequency";
			break;
		case SKP_SILK_DEC_PAYLOAD_TOO_LARGE: 
			message = "Payload size exceeded the maximum allowed 1024 bytes";
			break;
		case  SKP_SILK_DEC_PAYLOAD_ERROR	:
			message = "Payload has bit errors";
			break;
		default:
			message = "unknown";
			break;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," Silk Error: %s\n",message);
}

static switch_status_t switch_silk_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate, unsigned int *flag)
{
	struct silk_context *context = codec->private_info;
	SKP_int16 ret;
	SKP_int16 pktsz  = (SKP_int16)context->encoder_object.packetSize;
	SKP_int16 nBytes = MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES; 
	SKP_int16 *lindata = (SKP_int16 *)decoded_data;
	SKP_int16 samples = (SKP_int16)(decoded_data_len /sizeof(SKP_int16));
	*encoded_data_len = 0;
	while (samples >= pktsz){
		ret = (SKP_int16)SKP_Silk_SDK_Encode(context->enc_state,
							  &context->encoder_object,
							  lindata,
							  pktsz,
							  encoded_data,
							  &nBytes);
	
		if (ret) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SKP_Silk_Encode returned %d!\n", ret);
			printSilkError(ret);
			return SWITCH_STATUS_FALSE;
		}
		*encoded_data_len += nBytes;
		samples-=pktsz;
		lindata+= pktsz;
	}
	if (samples != 0){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_silk_encode dumping partial frame %d!\n", samples);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_silk_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate, unsigned int *flag)
{
	struct silk_context *context = codec->private_info;
	SKP_int16 ret, len; 
	int16_t *target = decoded_data;
	switch_core_session_t *session = codec->session;
	switch_jb_t *jb = NULL;
	SKP_int lost_flag = (*flag & SFF_PLC);
	switch_bool_t did_lbrr = SWITCH_FALSE;
	int i;

	*decoded_data_len = 0;

	if (lost_flag) {
		*flag &= ~SFF_PLC;

		if (session) {
			jb = switch_core_session_get_jb(session, SWITCH_MEDIA_TYPE_AUDIO);
		}

		if (jb && codec->cur_frame) {
			switch_frame_t frame = { 0 };
			uint8_t buf[SWITCH_RTP_MAX_BUF_LEN];
			frame.data = buf;
			frame.buflen = sizeof(buf);

			for (i = 1; i <= MAX_LBRR_DELAY; i++) {
				if (switch_jb_peek_frame(jb, codec->cur_frame->timestamp, 0, (uint16_t)i, &frame)) {
					SKP_Silk_SDK_search_for_LBRR(frame.data, (const int)frame.datalen, i, (SKP_uint8*) &context->recbuff, &context->reclen);

					if (context->reclen) {
						encoded_data = &context->recbuff;
						encoded_data_len = context->reclen;
						lost_flag = SKP_FALSE;
						did_lbrr = SWITCH_TRUE;
						break;
					}
				}
			}
		}
	}

	do {
		ret = (SKP_int16)SKP_Silk_SDK_Decode(context->dec_state,
								  &context->decoder_object,
								  lost_flag,
								  encoded_data,
								  encoded_data_len,
								  target,
								  &len);
		if (ret){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SKP_Silk_Decode returned %d!\n", ret);
			printSilkError(ret);
			/* if FEC was activated, we can ignore bit errors*/
			if (! (ret == SKP_SILK_DEC_PAYLOAD_ERROR && did_lbrr))
			return SWITCH_STATUS_FALSE;
		}

		target += len;
		*decoded_data_len += (len * 2);
	} while (context->decoder_object.moreInternalDecoderFrames);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_silk_load)
{
	switch_codec_interface_t *codec_interface;
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "SILK");
	codec_interface->parse_fmtp = switch_silk_fmtp_parse;
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 117,						/* the IANA code number */
										 "SILK",					/* the IANA code name */
										 "useinbandfec=1; usedtx=0",	/* default fmtp to send (can be overridden by the init function) */
										 8000,						/* samples transferred per second */
										 8000,						/* actual samples transferred per second */
										 20000,						/* bits transferred per second */
										 20000,						/* number of microseconds per frame */
										 160,						/* number of samples per frame */
										 320,						/* number of bytes per frame decompressed */
										 0,							/* number of bytes per frame compressed */
										 1,							/* number of channels represented */
										 1, 						/* number of frames per network packet */
										 switch_silk_init,			/* function to initialize a codec handle using this implementation */
										 switch_silk_encode,		/* function to encode raw data into encoded data */
										 switch_silk_decode,		/* function to decode encoded data into raw data */
										 switch_silk_destroy);		/* deinitalize a codec handle using this implementation */

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 118,						/* the IANA code number */
										 "SILK",					/* the IANA code name */
										 "useinbandfec=1; usedtx=0",	/* default fmtp to send (can be overridden by the init function) */
										 12000,						/* samples transferred per second */
										 12000,						/* actual samples transferred per second */
										 25000,						/* bits transferred per second */
										 20000,						/* number of microseconds per frame */
										 240,						/* number of samples per frame */
										 480,						/* number of bytes per frame decompressed */
										 0,							/* number of bytes per frame compressed */
										 1,							/* number of channels represented */
										 1, 						/* number of frames per network packet */
										 switch_silk_init,			/* function to initialize a codec handle using this implementation */
										 switch_silk_encode,		/* function to encode raw data into encoded data */
										 switch_silk_decode,		/* function to decode encoded data into raw data */
										 switch_silk_destroy);		/* deinitalize a codec handle using this implementation */

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 119,						/* the IANA code number */
										 "SILK",					/* the IANA code name */
										 "useinbandfec=1; usedtx=0",	/* default fmtp to send (can be overridden by the init function) */
										 16000,						/* samples transferred per second */
										 16000,						/* actual samples transferred per second */
										 30000,						/* bits transferred per second */
										 20000,						/* number of microseconds per frame */
										 320,						/* number of samples per frame */
										 640,						/* number of bytes per frame decompressed */
										 0,							/* number of bytes per frame compressed */
										 1,							/* number of channels represented */
										 1, 						/* number of frames per network packet */
										 switch_silk_init,			/* function to initialize a codec handle using this implementation */
										 switch_silk_encode,		/* function to encode raw data into encoded data */
										 switch_silk_decode,		/* function to decode encoded data into raw data */
										 switch_silk_destroy);		/* deinitalize a codec handle using this implementation */

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 120,						/* the IANA code number */
										 "SILK",					/* the IANA code name */
										 "useinbandfec=1; usedtx=0",	/* default fmtp to send (can be overridden by the init function) */
										 24000,						/* samples transferred per second */
										 24000,						/* actual samples transferred per second */
										 40000,						/* bits transferred per second */
										 20000,						/* number of microseconds per frame */
										 480,						/* number of samples per frame */
										 960,						/* number of bytes per frame decompressed */
										 0,							/* number of bytes per frame compressed */
										 1,							/* number of channels represented */
										 1, 						/* number of frames per network packet */
										 switch_silk_init,			/* function to initialize a codec handle using this implementation */
										 switch_silk_encode,		/* function to encode raw data into encoded data */
										 switch_silk_decode,		/* function to decode encoded data into raw data */
										 switch_silk_destroy);		/* deinitalize a codec handle using this implementation */


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
