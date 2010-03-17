/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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

struct silk_context {
	SKP_SILK_SDK_EncControlStruct encoder_object;
	SKP_SILK_SDK_DecControlStruct decoder_object;
	void *enc_state;
	void *dec_state;
};

static switch_status_t switch_silk_init(switch_codec_t *codec, 
										switch_codec_flag_t freeswitch_flags, 
										const switch_codec_settings_t *codec_settings)
{
	struct silk_context *context = NULL;
	SKP_int useinbandfec = 0, usedtx = 0, maxaveragebitrate = 0, plpct =0;
	SKP_int32 encSizeBytes;
	SKP_int32 decSizeBytes;
	int encoding = (freeswitch_flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (freeswitch_flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}
	
	if (codec->fmtp_in) {
		int x, argc;
		char *argv[10];
		argc = switch_separate_string(codec->fmtp_in, ';', argv, (sizeof(argv) / sizeof(argv[0])));
		for (x = 0; x < argc; x++) {
			char *data = argv[x];
			char *arg;
			switch_assert(data);
			while (*data == ' ') {
				data++;
			}
			if ((arg = strchr(data, '='))) {
				*arg++ = '\0';
				if (!strcasecmp(data, "useinbandfec")) {
					if (switch_true(arg)) {
						useinbandfec = 1;
						plpct = 10;// 10% for now
					}
				}
				if (!strcasecmp(data, "usedtx")) {
					if (switch_true(arg)) {
						usedtx = 1;
					}
				}
				if (!strcasecmp(data, "maxaveragebitrate")) {
					maxaveragebitrate = atoi(arg);
					switch(codec->implementation->actual_samples_per_second) {
					case 8000:
						{
							if(maxaveragebitrate < 6000 || maxaveragebitrate > 20000) {
								maxaveragebitrate = 20000;
							}
							break;
						}
					case 12000:
						{
							if(maxaveragebitrate < 7000 || maxaveragebitrate > 25000) {
								maxaveragebitrate = 25000;
							}
							break;
						}
					case 16000:
						{
							if(maxaveragebitrate < 8000 || maxaveragebitrate > 30000) {
								maxaveragebitrate = 30000;
							}
							break;
						}
					case 24000:
						{
							if(maxaveragebitrate < 12000 || maxaveragebitrate > 40000) {
								maxaveragebitrate = 40000;
							}
							break;
						}
						
					default:
						/* this should never happen but 20000 is common among all rates */
						maxaveragebitrate = 20000;
						break;
					}
					
				}
			}
		}
	}
	
	codec->fmtp_out = switch_core_sprintf(codec->memory_pool, "useinbandfec=%s; usedtx=%s; maxaveragebitrate=%d",
										  useinbandfec ? "true" : "false",
										  usedtx ? "true" : "false",
										  maxaveragebitrate ? maxaveragebitrate : codec->implementation->bits_per_second);

	if (encoding) {
		if (SKP_Silk_SDK_Get_Encoder_Size(&encSizeBytes)) {
			return SWITCH_STATUS_FALSE;
		}
		
		context->enc_state = switch_core_alloc(codec->memory_pool, encSizeBytes);

		if (SKP_Silk_SDK_InitEncoder(context->enc_state, &context->encoder_object)) {
			return SWITCH_STATUS_FALSE;
		}
		
		context->encoder_object.sampleRate = codec->implementation->actual_samples_per_second;
		context->encoder_object.packetSize = codec->implementation->samples_per_packet;
		context->encoder_object.useInBandFEC = useinbandfec;
		context->encoder_object.complexity = 2;
		context->encoder_object.bitRate = maxaveragebitrate ? maxaveragebitrate : codec->implementation->bits_per_second;
		context->encoder_object.useDTX = usedtx;
		context->encoder_object.packetLossPercentage = plpct;;
	}

	if (decoding) {
		if (SKP_Silk_SDK_Get_Decoder_Size(&decSizeBytes)) {
			return SWITCH_STATUS_FALSE;
		}
		context->dec_state = switch_core_alloc(codec->memory_pool, decSizeBytes);

		if (SKP_Silk_SDK_InitDecoder(context->dec_state)) {
			return SWITCH_STATUS_FALSE;
		}
		context->decoder_object.sampleRate = codec->implementation->actual_samples_per_second;
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
		case SKP_SILK_ENC_WRONG_LOSS_RATE: 
			message = " Loss rate not between  0 and 100 % ";
			break;
		case SKP_SILK_ENC_WRONG_COMPLEXITY_SETTING:
			message = "Complexity setting not valid, use 0 ,1 or 2";
			break;
		case SKP_SILK_ENC_WRONG_INBAND_FEC_SETTING: 
			message = "Inband FEC setting not valid, use 0 or 1	";
			break;
		case SKP_SILK_ENC_WRONG_DTX_SETTING:
			message = "DTX setting not valid, use 0 or 1";
			break;
		case SKP_SILK_ENC_INTERNAL_ERROR:
			message = "Internal Encoder Error ";
			break;
		case SKP_SILK_DEC_WRONG_SAMPLING_FREQUENCY:
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
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," Silk Error message= %s\n",message);
}

static switch_status_t switch_silk_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate, unsigned int *flag)
{
	struct silk_context *context = codec->private_info;
	SKP_int16 ret;
	SKP_int16 pktsz  = context->encoder_object.packetSize;
	SKP_int16 nBytes = MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES; 
	SKP_int16 *lindata = (SKP_int16 *)decoded_data;
	SKP_int16 samples = decoded_data_len /sizeof(SKP_int16);
	*encoded_data_len = 0;
	while (samples >= pktsz){
		ret = SKP_Silk_SDK_Encode(context->enc_state,
							  &context->encoder_object,
							  lindata,
							  pktsz,
							  encoded_data,
							  &nBytes);
	
		if (ret) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SKP_Silk_Encode returned %d!\n", ret);
			printSilkError(ret);
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

	*decoded_data_len = 0;

	do {
		ret = SKP_Silk_SDK_Decode(context->dec_state,
								  &context->decoder_object,
								  ((*flag & SFF_PLC)),
								  encoded_data,
								  encoded_data_len,
								  target,
								  &len);
		if (ret){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SKP_Silk_Decode returned %d!\n", ret);
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
	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 117,						/* the IANA code number */
										 "SILK",					/* the IANA code name */
										 "useinbandfec=true; usedtx=false",	/* default fmtp to send (can be overridden by the init function) */
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
										 "useinbandfec=false; usedtx=false",	/* default fmtp to send (can be overridden by the init function) */
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
										 "useinbandfec=false; usedtx=false",	/* default fmtp to send (can be overridden by the init function) */
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
										 "useinbandfec=false; usedtx=false",	/* default fmtp to send (can be overridden by the init function) */
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
