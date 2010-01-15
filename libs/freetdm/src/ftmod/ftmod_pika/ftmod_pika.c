/*
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "freetdm.h"
#include "ftdm_pika.h"


#define MAX_NUMBER_OF_TRUNKS 64
#define PIKA_BLOCK_SIZE 160
#define PIKA_BLOCK_LEN 20
#define PIKA_NUM_BUFFERS 8
#define TRY_OR_DIE(__code, __status, __label) if ((status = __code ) != __status) goto __label
#define pk_atof(__a) (PK_FLOAT) atof(__a)

PK_VOID PK_CALLBACK media_out_callback(PKH_TPikaEvent *event);

FTDM_ENUM_NAMES(PIKA_SPAN_NAMES, PIKA_SPAN_STRINGS)
PIKA_STR2ENUM(pika_str2span, pika_span2str, PIKA_TSpanFraming, PIKA_SPAN_NAMES, PIKA_SPAN_INVALID)

FTDM_ENUM_NAMES(PIKA_SPAN_ENCODING_NAMES, PIKA_SPAN_ENCODING_STRINGS)
PIKA_STR2ENUM(pika_str2span_encoding, pika_span_encoding2str, PIKA_TSpanEncoding, PIKA_SPAN_ENCODING_NAMES, PIKA_SPAN_ENCODING_INVALID)

FTDM_ENUM_NAMES(PIKA_LL_NAMES, PIKA_LL_STRINGS)
PIKA_STR2ENUM(pika_str2loop_length, pika_loop_length2str, PIKA_TSpanLoopLength, PIKA_LL_NAMES, PIKA_SPAN_LOOP_INVALID)

FTDM_ENUM_NAMES(PIKA_LBO_NAMES, PIKA_LBO_STRINGS)
PIKA_STR2ENUM(pika_str2lbo, pika_lbo2str, PIKA_TSpanBuildOut, PIKA_LBO_NAMES, PIKA_SPAN_LBO_INVALID)

FTDM_ENUM_NAMES(PIKA_SPAN_COMPAND_MODE_NAMES, PIKA_SPAN_COMPAND_MODE_STRINGS)
PIKA_STR2ENUM(pika_str2compand_mode, pika_compand_mode2str, PIKA_TSpanCompandMode, PIKA_SPAN_COMPAND_MODE_NAMES, PIKA_SPAN_COMPAND_MODE_INVALID)


typedef enum {
	PK_FLAG_READY = (1 << 0),
	PK_FLAG_LOCKED = (1 << 1)
} pk_flag_t;

struct general_config {
	uint32_t region;
};
typedef struct general_config general_config_t;

struct pika_channel_profile {
	char name[80];
	PKH_TRecordConfig record_config;
	PKH_TPlayConfig play_config;
	int ec_enabled;
	PKH_TECConfig ec_config;
	PKH_TSpanConfig span_config;
	general_config_t general_config;
	int cust_span;
};
typedef struct pika_channel_profile pika_channel_profile_t;

static struct {
	PKH_TSystemDeviceList board_list;
	TPikaHandle open_boards[MAX_NUMBER_OF_TRUNKS];
	TPikaHandle system_handle;
	PKH_TSystemConfig system_config;
	PKH_TRecordConfig record_config;
	PKH_TPlayConfig play_config;
	PKH_TECConfig ec_config;
	PKH_TSpanConfig t1_span_config;
	PKH_TSpanConfig e1_span_config;
	ftdm_hash_t *profile_hash;
	general_config_t general_config;
} globals;


struct pika_span_data {
	TPikaHandle event_queue;
	PKH_TPikaEvent last_oob_event;
	uint32_t boardno;
	PKH_TSpanConfig span_config;
	TPikaHandle handle;
	uint32_t flags;
};
typedef struct pika_span_data pika_span_data_t;

struct pika_chan_data {
	TPikaHandle handle;
	TPikaHandle media_in;
	TPikaHandle media_out;
	TPikaHandle media_in_queue;
	TPikaHandle media_out_queue;
	PKH_TPikaEvent last_media_event;
	PKH_TPikaEvent last_oob_event;
	PKH_TRecordConfig record_config;
	PKH_TPlayConfig play_config;
	int ec_enabled;
	PKH_TECConfig ec_config;
	PKH_THDLCConfig hdlc_config;
	ftdm_buffer_t *digit_buffer;
	ftdm_mutex_t *digit_mutex;
	ftdm_size_t dtmf_len;
	uint32_t flags;
	uint32_t hdlc_bytes;
};
typedef struct pika_chan_data pika_chan_data_t;

static const char *pika_board_type_string(PK_UINT type)
{
	if (type == PKH_BOARD_TYPE_DIGITAL_GATEWAY) {
		return "digital_gateway";
	}

	if (type == PKH_BOARD_TYPE_ANALOG_GATEWAY) {
		return "analog_gateway";
	}

	return "unknown";
}

/**
 * \brief Process configuration variable for a pika profile
 * \param category Pika profile name
 * \param var Variable name
 * \param val Variable value
 * \param lineno Line number from configuration file (unused)
 * \return Success
 */
static FIO_CONFIGURE_FUNCTION(pika_configure)
{
	pika_channel_profile_t *profile = NULL;
	int ok = 1;

	if (!(profile = (pika_channel_profile_t *) hashtable_search(globals.profile_hash, (char *)category))) {
		profile = ftdm_malloc(sizeof(*profile));
		memset(profile, 0, sizeof(*profile));
		ftdm_set_string(profile->name, category);
		profile->ec_config = globals.ec_config;
		profile->record_config = globals.record_config;
		profile->play_config = globals.play_config;
		hashtable_insert(globals.profile_hash, (void *)profile->name, profile, HASHTABLE_FLAG_NONE);
		ftdm_log(FTDM_LOG_INFO, "creating profile [%s]\n", category);
	}

	if (!strcasecmp(var, "rx-gain")) {
		profile->record_config.gain = pk_atof(val);
	} else if (!strcasecmp(var, "rx-agc-enabled")) {
		profile->record_config.AGC.enabled = ftdm_true(val);
	} else if (!strcasecmp(var, "rx-agc-targetPower")) {
		profile->record_config.AGC.targetPower = pk_atof(val);
	} else if (!strcasecmp(var, "rx-agc-minGain")) {
		profile->record_config.AGC.minGain = pk_atof(val);
	} else if (!strcasecmp(var, "rx-agc-maxGain")) {
		profile->record_config.AGC.maxGain = pk_atof(val);
	} else if (!strcasecmp(var, "rx-agc-attackRate")) {
		profile->record_config.AGC.attackRate = atoi(val);
	} else if (!strcasecmp(var, "rx-agc-decayRate")) {
		profile->record_config.AGC.decayRate = atoi(val);
	} else if (!strcasecmp(var, "rx-agc-speechThreshold")) {
		profile->record_config.AGC.speechThreshold = pk_atof(val);
	} else if (!strcasecmp(var, "rx-vad-enabled")) {
		profile->record_config.VAD.enabled = ftdm_true(val);
	} else if (!strcasecmp(var, "rx-vad-activationThreshold")) {
		profile->record_config.VAD.activationThreshold = pk_atof(val);
	} else if (!strcasecmp(var, "rx-vad-activationDebounceTime")) {
		profile->record_config.VAD.activationDebounceTime = atoi(val);
	} else if (!strcasecmp(var, "rx-vad-deactivationThreshold")) {
		profile->record_config.VAD.deactivationThreshold = pk_atof(val);
	} else if (!strcasecmp(var, "rx-vad-deactivationDebounceTime")) {
		profile->record_config.VAD.deactivationDebounceTime = atoi(val);
	} else if (!strcasecmp(var, "rx-vad-preSpeechBufferSize")) {
		profile->record_config.VAD.preSpeechBufferSize = atoi(val);
	} else if (!strcasecmp(var, "tx-gain")) {
		profile->play_config.gain = pk_atof(val);
	} else if (!strcasecmp(var, "tx-agc-enabled")) {
		profile->play_config.AGC.enabled = ftdm_true(val);
	} else if (!strcasecmp(var, "tx-agc-targetPower")) {
		profile->play_config.AGC.targetPower = pk_atof(val);
	} else if (!strcasecmp(var, "tx-agc-minGain")) {
		profile->play_config.AGC.minGain = pk_atof(val);
	} else if (!strcasecmp(var, "tx-agc-maxGain")) {
		profile->play_config.AGC.maxGain = pk_atof(val);
	} else if (!strcasecmp(var, "tx-agc-attackRate")) {
		profile->play_config.AGC.attackRate = atoi(val);
	} else if (!strcasecmp(var, "tx-agc-decayRate")) {
		profile->play_config.AGC.decayRate = atoi(val);
	} else if (!strcasecmp(var, "tx-agc-speechThreshold")) {
		profile->play_config.AGC.speechThreshold = pk_atof(val);
	} else if (!strcasecmp(var, "ec-enabled")) {
		profile->ec_enabled = ftdm_true(val);
	} else if (!strcasecmp(var, "ec-doubleTalkerThreshold")) {
		profile->ec_config.doubleTalkerThreshold = pk_atof(val);
	} else if (!strcasecmp(var, "ec-speechPresentThreshold")) {
		profile->ec_config.speechPresentThreshold = pk_atof(val);
	} else if (!strcasecmp(var, "ec-echoSuppressionThreshold")) {
		profile->ec_config.echoSuppressionThreshold = pk_atof(val);
	} else if (!strcasecmp(var, "ec-echoSuppressionEnabled")) {
		profile->ec_config.echoSuppressionEnabled = ftdm_true(val);
	} else if (!strcasecmp(var, "ec-comfortNoiseEnabled")) {
		profile->ec_config.comfortNoiseEnabled = ftdm_true(val);
	} else if (!strcasecmp(var, "ec-adaptationModeEnabled")) {
		profile->ec_config.adaptationModeEnabled = ftdm_true(val);
	} else if (!strcasecmp(var, "framing")) {
		profile->span_config.framing = pika_str2span(val);
		profile->cust_span++;
	} else if (!strcasecmp(var, "encoding")) {
		profile->span_config.encoding = pika_str2span_encoding(val);
		profile->cust_span++;
	} else if (!strcasecmp(var, "loopLength")) {
		profile->span_config.loopLength = pika_str2loop_length(val);
		profile->cust_span++;
	} else if (!strcasecmp(var, "buildOut")) {
		profile->span_config.buildOut = pika_str2lbo(val);
		profile->cust_span++;
	} else if (!strcasecmp(var, "compandMode")) {
		profile->span_config.compandMode = pika_str2compand_mode(val);
		profile->cust_span++;
	} else if (!strcasecmp(var, "region")) {
		if (!strcasecmp(val, "eu")) {
			profile->general_config.region = PKH_TRUNK_EU;
		} else {
			profile->general_config.region = PKH_TRUNK_NA;
		}
	} else {
		ok = 0;
	}

	if (ok) {
		ftdm_log(FTDM_LOG_INFO, "setting param [%s]=[%s] for profile [%s]\n", var, val, category);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "unknown param [%s]\n", var);
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Pika event handler
 * \param event Pika event
 */
PK_VOID PK_CALLBACK media_out_callback(PKH_TPikaEvent *event)
{
	PK_STATUS pk_status;
	ftdm_channel_t *ftdmchan = event->userData;
	pika_chan_data_t *chan_data = (pika_chan_data_t *) ftdmchan->mod_data;

	//PK_CHAR event_text[PKH_EVENT_MAX_NAME_LENGTH];
	//PKH_EVENT_GetText(event->id, event_text, sizeof(event_text));
	//ftdm_log(FTDM_LOG_DEBUG, "Event: %s\n", event_text);
	
	switch (event->id) {
	case PKH_EVENT_PLAY_IDLE:
		{
			while (ftdm_buffer_inuse(chan_data->digit_buffer)) {
				char dtmf[128] = "";
				ftdm_mutex_lock(chan_data->digit_mutex);
				chan_data->dtmf_len = ftdm_buffer_read(chan_data->digit_buffer, dtmf, sizeof(dtmf));
				pk_status = PKH_TG_PlayDTMF(chan_data->media_out, dtmf);
				ftdm_mutex_unlock(chan_data->digit_mutex);
			}
		}
		break;
	case PKH_EVENT_TG_TONE_PLAYED:
		{

			if (!event->p1) {
				ftdm_mutex_lock(chan_data->digit_mutex);
				PKH_PLAY_Start(chan_data->media_out);
				chan_data->dtmf_len = 0;
				ftdm_mutex_unlock(chan_data->digit_mutex);
			}

				
		}
		break;
	default:
		break;
	}

}

/**
 * \brief Initialises a range of pika channels
 * \param span FreeTDM span
 * \param boardno Pika board number
 * \param spanno Pika span number
 * \param start Initial pika channel number
 * \param end Final pika channel number
 * \param type FreeTDM channel type
 * \param name FreeTDM span name
 * \param number FreeTDM span number
 * \param profile Pika channel profile
 * \return number of spans configured
 */
static unsigned pika_open_range(ftdm_span_t *span, unsigned boardno, unsigned spanno, unsigned start, unsigned end, 
								ftdm_chan_type_t type, char *name, char *number, pika_channel_profile_t *profile)
{
	unsigned configured = 0, x;
	PK_STATUS status;
	PK_CHAR error_text[PKH_ERROR_MAX_NAME_LENGTH];
	pika_span_data_t *span_data;

	if (boardno >= globals.board_list.numberOfBoards) {
		ftdm_log(FTDM_LOG_ERROR, "Board %u is not present!\n", boardno);
		return 0;
	}

	if (!globals.open_boards[boardno]) {
		status = PKH_BOARD_Open(globals.board_list.board[boardno].id,
								NULL,
								&globals.open_boards[boardno]);
		if(status != PK_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error: PKH_BOARD_Open %d failed(%s)!\n",  boardno,
					PKH_ERROR_GetText(status, error_text, sizeof(error_text)));
			return 0;
		}

		ftdm_log(FTDM_LOG_DEBUG, "Open board %u\n",  boardno);

		//PKH_BOARD_SetDebugTrace(globals.open_boards[boardno], 1, 0);
		
	}

	if (span->mod_data) {
		span_data = span->mod_data;
	} else {
		span_data = ftdm_malloc(sizeof(*span_data));
		assert(span_data != NULL);
		memset(span_data, 0, sizeof(*span_data));
		span_data->boardno = boardno;
		
		status = PKH_QUEUE_Create(PKH_QUEUE_TYPE_NORMAL, &span_data->event_queue);
		
		if (status != PK_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error: PKH_QUEUE_Create failed(%s)!\n",
					PKH_ERROR_GetText(status, error_text, sizeof(error_text)));
			ftdm_safe_free(span_data);
			return 0;
		}

		//PKH_QUEUE_Attach(span_data->event_queue, globals.open_boards[boardno], NULL);

		span->mod_data = span_data;
	}

	if (type == FTDM_CHAN_TYPE_FXS || type == FTDM_CHAN_TYPE_FXO) {
		start--;
		end--;
	}

	for(x = start; x < end; x++) {
		ftdm_channel_t *chan;
		pika_chan_data_t *chan_data = NULL;

		chan_data = ftdm_malloc(sizeof *chan_data);
		assert(chan_data);
		memset(chan_data, 0, sizeof(*chan_data));
		ftdm_span_add_channel(span, 0, type, &chan);
		chan->mod_data = chan_data;

		if ((type == FTDM_CHAN_TYPE_B || type == FTDM_CHAN_TYPE_DQ921) && !span_data->handle) {
			PKH_TBoardConfig boardConfig;
			
			TRY_OR_DIE(PKH_BOARD_GetConfig(globals.open_boards[boardno], &boardConfig), PK_SUCCESS, error);
			if ((profile && profile->general_config.region == PKH_TRUNK_EU) || ftdm_test_flag(span_data, PK_FLAG_LOCKED)) {
				if (span->trunk_type == FTDM_TRUNK_T1) {
					ftdm_log(FTDM_LOG_WARNING, "Changing trunk type to E1 based on previous config.\n");
				}
				span->trunk_type = FTDM_TRUNK_E1;
			}

			if (span->trunk_type == FTDM_TRUNK_T1) {
				if (ftdm_test_flag(span_data, PK_FLAG_LOCKED)) {
					ftdm_log(FTDM_LOG_WARNING, "Already locked into E1 mode!\n");
				}
			} else if (span->trunk_type == FTDM_TRUNK_E1) {
				boardConfig.specific.DigitalGateway.interfaceType = PKH_BOARD_INTERFACE_TYPE_E1;
				if ((status = PKH_BOARD_SetConfig(globals.open_boards[boardno], &boardConfig)) != PK_SUCCESS) {
					ftdm_log(FTDM_LOG_ERROR, "Error: [%s]\n",
							PKH_ERROR_GetText(status, error_text, sizeof(error_text)));
				}
				ftdm_set_flag(span_data, PK_FLAG_LOCKED);
			}
			
			TRY_OR_DIE(PKH_SPAN_Open(globals.open_boards[boardno], spanno, NULL, &span_data->handle), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_SPAN_GetConfig(span_data->handle, &span_data->span_config), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(span_data->event_queue, span_data->handle, (PK_VOID*) span), PK_SUCCESS, error);
		}

		if (type == FTDM_CHAN_TYPE_FXO) {
			PKH_TTrunkConfig trunkConfig;
			
			TRY_OR_DIE(PKH_TRUNK_Open(globals.open_boards[boardno], x, &chan_data->handle), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_TRUNK_Seize(chan_data->handle), PK_SUCCESS, error);

			if (profile && profile->general_config.region == PKH_TRUNK_EU) {
				TRY_OR_DIE(PKH_TRUNK_GetConfig(chan_data->handle, &trunkConfig), PK_SUCCESS, error);
				trunkConfig.internationalControl = PKH_PHONE_INTERNATIONAL_CONTROL_EU;
				trunkConfig.audioFormat = PKH_AUDIO_ALAW;
				trunkConfig.compandMode = PKH_PHONE_AUDIO_ALAW;
				chan->native_codec = chan->effective_codec = FTDM_CODEC_ALAW;
				TRY_OR_DIE(PKH_TRUNK_SetConfig(chan_data->handle, &trunkConfig), PK_SUCCESS, error);
			} else {
				chan->native_codec = chan->effective_codec = FTDM_CODEC_ULAW;
			}

			
			TRY_OR_DIE(PKH_QUEUE_Attach(span_data->event_queue, chan_data->handle, (PK_VOID*) chan), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_TRUNK_GetMediaStreams(chan_data->handle, &chan_data->media_in, &chan_data->media_out), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Create(PKH_QUEUE_TYPE_NORMAL, &chan_data->media_in_queue), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(chan_data->media_in_queue, chan_data->media_in, (PK_VOID*) chan), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Create(PKH_QUEUE_TYPE_CALLBACK, &chan_data->media_out_queue), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_SetEventHandler(chan_data->media_out_queue, media_out_callback), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(chan_data->media_out_queue, chan_data->media_out, (PK_VOID*) chan), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_TRUNK_Start(chan_data->handle), PK_SUCCESS, error);
		} else if (type == FTDM_CHAN_TYPE_FXS) {
			PKH_TPhoneConfig phoneConfig;

			if (profile && profile->general_config.region == PKH_TRUNK_EU) {
				TRY_OR_DIE(PKH_PHONE_GetConfig(chan_data->handle, &phoneConfig), PK_SUCCESS, error);
				phoneConfig.internationalControl = PKH_PHONE_INTERNATIONAL_CONTROL_EU;
				phoneConfig.compandMode = PKH_PHONE_AUDIO_ALAW;
				chan->native_codec = chan->effective_codec = FTDM_CODEC_ALAW;
				TRY_OR_DIE(PKH_PHONE_SetConfig(chan_data->handle, &phoneConfig), PK_SUCCESS, error);
			} else {
				chan->native_codec = chan->effective_codec = FTDM_CODEC_ULAW;
			}

			TRY_OR_DIE(PKH_PHONE_Open(globals.open_boards[boardno], x, &chan_data->handle), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_PHONE_Seize(chan_data->handle), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_PHONE_GetMediaStreams(chan_data->handle, &chan_data->media_in, &chan_data->media_out), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(span_data->event_queue, chan_data->handle, (PK_VOID*) chan), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Create(PKH_QUEUE_TYPE_NORMAL, &chan_data->media_in_queue), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(chan_data->media_in_queue, chan_data->media_in, (PK_VOID*) chan), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Create(PKH_QUEUE_TYPE_CALLBACK, &chan_data->media_out_queue), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_SetEventHandler(chan_data->media_out_queue, media_out_callback), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(chan_data->media_out_queue, chan_data->media_out, (PK_VOID*) chan), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_PHONE_Start(chan_data->handle), PK_SUCCESS, error);
		} else if (type == FTDM_CHAN_TYPE_B) {
			TRY_OR_DIE(PKH_SPAN_SeizeChannel(span_data->handle, x), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_SPAN_GetMediaStreams(span_data->handle, x, &chan_data->media_in, &chan_data->media_out), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Create(PKH_QUEUE_TYPE_NORMAL, &chan_data->media_in_queue), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(chan_data->media_in_queue, chan_data->media_in, (PK_VOID*) chan), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Create(PKH_QUEUE_TYPE_CALLBACK, &chan_data->media_out_queue), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_SetEventHandler(chan_data->media_out_queue, media_out_callback), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(chan_data->media_out_queue, chan_data->media_out, (PK_VOID*) chan), PK_SUCCESS, error);
		} else if (type == FTDM_CHAN_TYPE_DQ921) {
			TRY_OR_DIE(PKH_SPAN_HDLC_Open(span_data->handle, PKH_SPAN_HDLC_MODE_NORMAL, &chan_data->handle), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_SPAN_HDLC_GetConfig(chan_data->handle, &chan_data->hdlc_config), PK_SUCCESS, error);
			chan_data->hdlc_config.channelId = x;
			TRY_OR_DIE(PKH_SPAN_HDLC_SetConfig(chan_data->handle, &chan_data->hdlc_config), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Create(PKH_QUEUE_TYPE_NORMAL, &chan_data->media_in_queue), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(chan_data->media_in_queue, chan_data->handle, (PK_VOID*) chan), PK_SUCCESS, error);
			TRY_OR_DIE(PKH_QUEUE_Attach(span_data->event_queue, chan_data->handle, (PK_VOID*) chan), PK_SUCCESS, error);

			if (profile) {
				if (profile->cust_span) {
					span_data->span_config.framing = profile->span_config.framing;
					span_data->span_config.encoding = profile->span_config.encoding;
					span_data->span_config.loopLength = profile->span_config.loopLength;
					span_data->span_config.buildOut = profile->span_config.buildOut;
					span_data->span_config.compandMode = profile->span_config.compandMode;
				} else {
					if (profile->general_config.region == PKH_TRUNK_EU) {
						span_data->span_config = globals.e1_span_config;
					} else {
						span_data->span_config = globals.t1_span_config;
					}
				}
			} else {
				if (span->trunk_type == FTDM_TRUNK_E1) {
					span_data->span_config = globals.e1_span_config;
				} else {
					span_data->span_config = globals.t1_span_config;
				}
			}
			
			PKH_SPAN_SetConfig(span_data->handle, &span_data->span_config);
			TRY_OR_DIE(PKH_SPAN_Start(span_data->handle), PK_SUCCESS, error);
		}
		
		goto ok;

	error:
		PKH_ERROR_GetText(status, error_text, sizeof(error_text));
		ftdm_log(FTDM_LOG_ERROR, "failure configuring device b%ds%dc%d [%s]\n", boardno, spanno, x, error_text);
		continue;
	ok:
		ftdm_set_flag(chan_data, PK_FLAG_READY);
		status = PKH_RECORD_GetConfig(chan_data->media_in, &chan_data->record_config);
		chan_data->record_config.encoding = PKH_RECORD_ENCODING_MU_LAW;
		chan_data->record_config.samplingRate = PKH_RECORD_SAMPLING_RATE_8KHZ;
		chan_data->record_config.bufferSize = PIKA_BLOCK_SIZE;
		chan_data->record_config.numberOfBuffers = PIKA_NUM_BUFFERS;
		chan_data->record_config.VAD.enabled = PK_FALSE;
		//chan_data->record_config.speechSegmentEventsEnabled = PK_FALSE;
		//chan_data->record_config.gain = rxgain;

		status = PKH_PLAY_GetConfig(chan_data->media_out, &chan_data->play_config);
		chan_data->play_config.encoding = PKH_RECORD_ENCODING_MU_LAW;
		chan_data->play_config.samplingRate = PKH_RECORD_SAMPLING_RATE_8KHZ;
		chan_data->play_config.AGC.enabled = PK_FALSE;
		ftdm_log(FTDM_LOG_INFO, "configuring device b%ds%dc%d as FreeTDM device %d:%d\n", boardno, spanno, x, chan->span_id, chan->chan_id);

		if (profile) {
			ftdm_log(FTDM_LOG_INFO, "applying config profile %s to device %d:%d\n", profile->name, chan->span_id, chan->chan_id);
			chan_data->record_config.gain = profile->record_config.gain;
			chan_data->record_config.AGC = profile->record_config.AGC;
			chan_data->record_config.VAD = profile->record_config.VAD;
			chan_data->play_config.gain = profile->play_config.gain;
			chan_data->play_config.AGC = profile->play_config.AGC;
			chan_data->ec_enabled = profile->ec_enabled;
			chan_data->ec_config = profile->ec_config;
		}
		
		if (type == FTDM_CHAN_TYPE_B) {
			if (span_data->span_config.compandMode == PKH_SPAN_COMPAND_MODE_A_LAW) {
				chan->native_codec = chan->effective_codec = FTDM_CODEC_ALAW;
			} else {
				chan->native_codec = chan->effective_codec = FTDM_CODEC_ULAW;
			}
		}

		status = PKH_RECORD_SetConfig(chan_data->media_in, &chan_data->record_config);
		status = PKH_PLAY_SetConfig(chan_data->media_out, &chan_data->play_config);

		chan->physical_span_id = spanno;
		chan->physical_chan_id = x;

		chan->rate = 8000;
		chan->packet_len = (uint32_t)chan_data->record_config.bufferSize;
		chan->effective_interval = chan->native_interval = chan->packet_len / 8;

		PKH_RECORD_Start(chan_data->media_in);
		PKH_PLAY_Start(chan_data->media_out);
		if (chan_data->ec_enabled) {
			PKH_EC_SetConfig(chan_data->media_in, &chan_data->ec_config);
			PKH_EC_Start(chan_data->media_in, chan_data->media_in, chan_data->media_out);
		}

		if (!ftdm_strlen_zero(name)) {
			ftdm_copy_string(chan->chan_name, name, sizeof(chan->chan_name));
		}
		
		if (!ftdm_strlen_zero(number)) {
			ftdm_copy_string(chan->chan_number, number, sizeof(chan->chan_number));
		}

		ftdm_channel_set_feature(chan, FTDM_CHANNEL_FEATURE_DTMF_GENERATE);
		ftdm_buffer_create(&chan_data->digit_buffer, 128, 128, 0);
		ftdm_mutex_create(&chan_data->digit_mutex);

		configured++;
	} 

	
	return configured;
}

/**
 * \brief Initialises an freetdm pika span from a configuration string
 * \param span FreeTDM span
 * \param str Configuration string
 * \param type FreeTDM span type
 * \param name FreeTDM span name
 * \param number FreeTDM span number
 * \return Success or failure
 */
static FIO_CONFIGURE_SPAN_FUNCTION(pika_configure_span)
{
	int items, i;
    char *mydata, *item_list[10];
	char *bd, *sp, *ch = NULL, *mx;
	int boardno;
	int channo;
	int spanno;
	int top = 0;
	unsigned configured = 0;
	char *profile_name = NULL;
	pika_channel_profile_t *profile = NULL;

	assert(str != NULL);

	mydata = ftdm_strdup(str);
	assert(mydata != NULL);

	if ((profile_name = strchr(mydata, '@'))) {
		*profile_name++ = '\0';
		if (!ftdm_strlen_zero(profile_name)) {
			profile = (pika_channel_profile_t *) hashtable_search(globals.profile_hash, (char *)profile_name);
		}
	}
		
	items = ftdm_separate_string(mydata, ',', item_list, (sizeof(item_list) / sizeof(item_list[0])));

	for(i = 0; i < items; i++) {
		bd = item_list[i];
		if ((sp = strchr(bd, ':'))) {
			*sp++ = '\0';
			if ((ch = strchr(sp, ':'))) {
				*ch++ = '\0';
			}
		}
		
		if (!(bd && sp && ch)) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid input\n");
			continue;
		}

		boardno = atoi(bd);
		channo = atoi(ch);
		spanno = atoi(sp);


		if (boardno < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid board number %d\n", boardno);
			continue;
		}

		if (channo < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid channel number %d\n", channo);
			continue;
		}

		if (spanno < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid span number %d\n", channo);
			continue;
		}
		
		if ((mx = strchr(ch, '-'))) {
			mx++;
			top = atoi(mx) + 1;
		} else {
			top = channo + 1;
		}
		
		
		if (top < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid range number %d\n", top);
			continue;
		}

		configured += pika_open_range(span, boardno, spanno, channo, top, type, name, number, profile);

	}
	
	ftdm_safe_free(mydata);

	return configured;
}

/**
 * \brief Opens Pika channel
 * \param ftdmchan Channel to open
 * \return Success or failure
 */
static FIO_OPEN_FUNCTION(pika_open) 
{
	pika_chan_data_t *chan_data = (pika_chan_data_t *) ftdmchan->mod_data;

	if (!chan_data && !ftdm_test_flag(chan_data, PK_FLAG_READY)) {
		return FTDM_FAIL;
	}

	if (chan_data->media_in_queue) {
		PKH_QUEUE_Flush(chan_data->media_in_queue);
	}

	if (ftdmchan->type == FTDM_CHAN_TYPE_FXS || ftdmchan->type == FTDM_CHAN_TYPE_FXO || ftdmchan->type == FTDM_CHAN_TYPE_B) {
		PKH_PLAY_Start(chan_data->media_out);
	}
	return FTDM_SUCCESS;
}

/**
 * \brief Closes Pika channel
 * \param ftdmchan Channel to close
 * \return Success
 */
static FIO_CLOSE_FUNCTION(pika_close)
{
	return FTDM_SUCCESS;
}

/**
 * \brief Waits for an event on a Pika channel
 * \param ftdmchan Channel to open
 * \param flags Type of event to wait for
 * \param to Time to wait (in ms)
 * \return Success, failure or timeout
 */
static FIO_WAIT_FUNCTION(pika_wait)
{
	pika_chan_data_t *chan_data = (pika_chan_data_t *) ftdmchan->mod_data;
	PK_STATUS status;
	ftdm_wait_flag_t myflags = *flags;
	PK_CHAR event_text[PKH_EVENT_MAX_NAME_LENGTH];

	*flags = FTDM_NO_FLAGS;	

	if (myflags & FTDM_READ) {
		if (chan_data->hdlc_bytes) {
			*flags |= FTDM_READ;
			return FTDM_SUCCESS;
		}
		status = PKH_QUEUE_WaitOnEvent(chan_data->media_in_queue, to, &chan_data->last_media_event);
		
		if (status == PK_SUCCESS) {
			if (chan_data->last_media_event.id == PKH_EVENT_QUEUE_TIMEOUT || chan_data->last_media_event.id == PKH_EVENT_RECORD_BUFFER_OVERFLOW) {
				return FTDM_TIMEOUT;
			}
			
			*flags |= FTDM_READ;
			return FTDM_SUCCESS;
		}

		PKH_EVENT_GetText(chan_data->last_media_event.id, event_text, sizeof(event_text));
		ftdm_log(FTDM_LOG_DEBUG, "Event: %s\n", event_text);
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Reads data from a Pika channel
 * \param ftdmchan Channel to read from
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success or failure
 */
static FIO_READ_FUNCTION(pika_read)
{
	pika_chan_data_t *chan_data = (pika_chan_data_t *) ftdmchan->mod_data;
	PK_STATUS status;
	PK_CHAR event_text[PKH_EVENT_MAX_NAME_LENGTH];
	uint32_t len;

	if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921) {
		if ((status = PKH_SPAN_HDLC_GetMessage(chan_data->handle, data, *datalen)) == PK_SUCCESS) {
			*datalen = chan_data->hdlc_bytes;
			chan_data->hdlc_bytes = 0;
			return FTDM_SUCCESS;
		}
		return FTDM_FAIL;
	}

	if (!(len = chan_data->last_media_event.p0)) {
		len = ftdmchan->packet_len;
	}
	
	if (len < *datalen) {
		*datalen = len;
	}

	if ((status = PKH_RECORD_GetData(chan_data->media_in, data, *datalen)) == PK_SUCCESS) {
		return FTDM_SUCCESS;
	}


	PKH_ERROR_GetText(status, event_text, sizeof(event_text));
	ftdm_log(FTDM_LOG_DEBUG, "ERR: %s\n", event_text);
	return FTDM_FAIL;
}

/**
 * \brief Writes data to a Pika channel
 * \param ftdmchan Channel to write to
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success or failure
 */
static FIO_WRITE_FUNCTION(pika_write)
{
	pika_chan_data_t *chan_data = (pika_chan_data_t *) ftdmchan->mod_data;
	PK_STATUS status;

	if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921) {
		if ((status = PKH_SPAN_HDLC_SendMessage(chan_data->handle, data, *datalen)) == PK_SUCCESS) {
			return FTDM_SUCCESS;
		}
		return FTDM_FAIL;
	}

	if (PKH_PLAY_AddData(chan_data->media_out, 0, data, *datalen) == PK_SUCCESS) {
		return FTDM_SUCCESS;
	}

	return FTDM_FAIL;
}

/**
 * \brief Executes an FreeTDM command on a Pika channel
 * \param ftdmchan Channel to execute command on
 * \param command FreeTDM command to execute
 * \param obj Object (unused)
 * \return Success or failure
 */
static FIO_COMMAND_FUNCTION(pika_command)
{
	pika_chan_data_t *chan_data = (pika_chan_data_t *) ftdmchan->mod_data;
	//pika_span_data_t *span_data = (pika_span_data_t *) ftdmchan->span->mod_data;
	PK_STATUS pk_status;
	ftdm_status_t status = FTDM_SUCCESS;

	switch(command) {
	case FTDM_COMMAND_OFFHOOK:
		{
			if ((pk_status = PKH_TRUNK_SetHookSwitch(chan_data->handle, PKH_TRUNK_OFFHOOK)) != PK_SUCCESS) {
				PKH_ERROR_GetText(pk_status, ftdmchan->last_error, sizeof(ftdmchan->last_error));
				GOTO_STATUS(done, FTDM_FAIL);
			} else {
				ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_OFFHOOK);
			}
		}
		break;
	case FTDM_COMMAND_ONHOOK:
		{
			if ((pk_status = PKH_TRUNK_SetHookSwitch(chan_data->handle, PKH_TRUNK_ONHOOK)) != PK_SUCCESS) {
				PKH_ERROR_GetText(pk_status, ftdmchan->last_error, sizeof(ftdmchan->last_error));
				GOTO_STATUS(done, FTDM_FAIL);
			} else {
				ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_OFFHOOK);
			}
		}
		break;
	case FTDM_COMMAND_GENERATE_RING_ON:
		{
			if ((pk_status = PKH_PHONE_RingStart(chan_data->handle, 0, 0)) != PK_SUCCESS) {
				PKH_ERROR_GetText(pk_status, ftdmchan->last_error, sizeof(ftdmchan->last_error));
				GOTO_STATUS(done, FTDM_FAIL);
			} else {
				ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_RINGING);
			}
		}
		break;
	case FTDM_COMMAND_GENERATE_RING_OFF:
		{
			if ((pk_status = PKH_PHONE_RingStop(chan_data->handle)) != PK_SUCCESS) {
				PKH_ERROR_GetText(pk_status, ftdmchan->last_error, sizeof(ftdmchan->last_error));
				GOTO_STATUS(done, FTDM_FAIL);
			} else {
				ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_RINGING);
			}
		}
		break;
	case FTDM_COMMAND_GET_INTERVAL:
		{

			FTDM_COMMAND_OBJ_INT = ftdmchan->native_interval;

		}
		break;
	case FTDM_COMMAND_SET_INTERVAL: 
		{
			int interval = FTDM_COMMAND_OBJ_INT;
			int len = interval * 8;
			chan_data->record_config.bufferSize = len;
			chan_data->record_config.numberOfBuffers = (PK_UINT)chan_data->record_config.bufferSize;
			ftdmchan->packet_len = (uint32_t)chan_data->record_config.bufferSize;
			ftdmchan->effective_interval = ftdmchan->native_interval = ftdmchan->packet_len / 8;
			PKH_RECORD_SetConfig(chan_data->media_in, &chan_data->record_config);
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_GET_DTMF_ON_PERIOD:
		{

			FTDM_COMMAND_OBJ_INT = ftdmchan->dtmf_on;
			GOTO_STATUS(done, FTDM_SUCCESS);

		}
		break;
	case FTDM_COMMAND_GET_DTMF_OFF_PERIOD:
		{			
				FTDM_COMMAND_OBJ_INT = ftdmchan->dtmf_on;
				GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	case FTDM_COMMAND_SET_DTMF_ON_PERIOD:
		{
			int val = FTDM_COMMAND_OBJ_INT;
			if (val > 10 && val < 1000) {
				ftdmchan->dtmf_on = val;
				GOTO_STATUS(done, FTDM_SUCCESS);
			} else {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "invalid value %d range 10-1000", val);
				GOTO_STATUS(done, FTDM_FAIL);
			}
		}
		break;
	case FTDM_COMMAND_SET_DTMF_OFF_PERIOD:
		{
			int val = FTDM_COMMAND_OBJ_INT;
			if (val > 10 && val < 1000) {
				ftdmchan->dtmf_off = val;
				GOTO_STATUS(done, FTDM_SUCCESS);
			} else {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "invalid value %d range 10-1000", val);
				GOTO_STATUS(done, FTDM_FAIL);
			}
		}
		break;
	case FTDM_COMMAND_SEND_DTMF:
		{
			char *digits = FTDM_COMMAND_OBJ_CHAR_P;
			ftdm_log(FTDM_LOG_DEBUG, "Adding DTMF SEQ [%s]\n", digits);
			ftdm_mutex_lock(chan_data->digit_mutex);
			ftdm_buffer_write(chan_data->digit_buffer, digits, strlen(digits));
			ftdm_mutex_unlock(chan_data->digit_mutex);
			pk_status = PKH_PLAY_Stop(chan_data->media_out);
			
			if (pk_status != PK_SUCCESS) {
				PKH_ERROR_GetText(pk_status, ftdmchan->last_error, sizeof(ftdmchan->last_error));
				GOTO_STATUS(done, FTDM_FAIL);
			}
			GOTO_STATUS(done, FTDM_SUCCESS);
		}
		break;
	default:
		break;
	};

 done:
	return status;
}

/**
 * \brief Checks for events on a Pika span
 * \param span Span to check for events
 * \param ms Time to wait for event
 * \return Success if event is waiting or failure if not
 */
static FIO_SPAN_POLL_EVENT_FUNCTION(pika_poll_event)
{
	pika_span_data_t *span_data = (pika_span_data_t *) span->mod_data;
	PK_STATUS status;
	PK_CHAR event_text[PKH_EVENT_MAX_NAME_LENGTH];

	status = PKH_QUEUE_WaitOnEvent(span_data->event_queue, ms, &span_data->last_oob_event);

	if (status == PK_SUCCESS) {
		ftdm_channel_t *ftdmchan = NULL;
		uint32_t *data = (uint32_t *) span_data->last_oob_event.userData;
		ftdm_data_type_t data_type = FTDM_TYPE_NONE;

		if (span_data->last_oob_event.id == PKH_EVENT_QUEUE_TIMEOUT) {
			return FTDM_TIMEOUT;
		}

		if (data) {
			data_type = *data;
		}

		if (data_type == FTDM_TYPE_CHANNEL) {
			ftdmchan = span_data->last_oob_event.userData;
		} else if (data_type == FTDM_TYPE_SPAN) {
            ftdm_time_t last_event_time = ftdm_current_time_in_ms();
			uint32_t event_id = 0;

			switch (span_data->last_oob_event.id) {
			case PKH_EVENT_SPAN_ALARM_T1_RED:
			case PKH_EVENT_SPAN_ALARM_T1_YELLOW:
			case PKH_EVENT_SPAN_ALARM_T1_AIS:
			case PKH_EVENT_SPAN_ALARM_E1_RED:
			case PKH_EVENT_SPAN_ALARM_E1_RAI:
			case PKH_EVENT_SPAN_ALARM_E1_AIS:
			case PKH_EVENT_SPAN_ALARM_E1_RMAI:
			case PKH_EVENT_SPAN_ALARM_E1_TS16AIS:
			case PKH_EVENT_SPAN_ALARM_E1_TS16LOS:
			case PKH_EVENT_SPAN_OUT_OF_SYNC:
			case PKH_EVENT_SPAN_FRAMING_ERROR:
			case PKH_EVENT_SPAN_LOSS_OF_SIGNAL:
			case PKH_EVENT_SPAN_OUT_OF_CRC_MF_SYNC:
			case PKH_EVENT_SPAN_OUT_OF_CAS_MF_SYNC:
				event_id = FTDM_OOB_ALARM_TRAP;
				break;
			case PKH_EVENT_SPAN_ALARM_T1_RED_CLEAR:
			case PKH_EVENT_SPAN_ALARM_T1_YELLOW_CLEAR:
			case PKH_EVENT_SPAN_ALARM_T1_AIS_CLEAR:
			case PKH_EVENT_SPAN_ALARM_E1_RED_CLEAR:
			case PKH_EVENT_SPAN_ALARM_E1_RAI_CLEAR:
			case PKH_EVENT_SPAN_ALARM_E1_AIS_CLEAR:
			case PKH_EVENT_SPAN_ALARM_E1_RMAI_CLEAR:
			case PKH_EVENT_SPAN_ALARM_E1_TS16AIS_CLEAR:
			case PKH_EVENT_SPAN_ALARM_E1_TS16LOS_CLEAR:
			case PKH_EVENT_SPAN_IN_SYNC:
			case PKH_EVENT_SPAN_LOSS_OF_SIGNAL_CLEAR:
			case PKH_EVENT_SPAN_IN_CRC_MF_SYNC:
			case PKH_EVENT_SPAN_IN_CAS_MF_SYNC:
				event_id = FTDM_OOB_ALARM_CLEAR;
				break;
			case PKH_EVENT_SPAN_MESSAGE:
			case PKH_EVENT_SPAN_ABCD_SIGNAL_CHANGE:
				break;
			}

			if (event_id) {
				uint32_t x = 0;
				ftdm_channel_t *ftdmchan;
				pika_chan_data_t *chan_data;
				for(x = 1; x <= span->chan_count; x++) {
					ftdmchan = span->channels[x];
					assert(ftdmchan != NULL);
					chan_data = (pika_chan_data_t *) ftdmchan->mod_data;
					assert(chan_data != NULL);
					

					ftdm_set_flag(ftdmchan, FTDM_CHANNEL_EVENT);
					ftdmchan->last_event_time = last_event_time;
					chan_data->last_oob_event = span_data->last_oob_event;
				}

			}

		}
		
		PKH_EVENT_GetText(span_data->last_oob_event.id, event_text, sizeof(event_text));
		//ftdm_log(FTDM_LOG_DEBUG, "Event: %s\n", event_text);
		
		if (ftdmchan) {
			pika_chan_data_t *chan_data = (pika_chan_data_t *) ftdmchan->mod_data;

			assert(chan_data != NULL);
			ftdm_set_flag(ftdmchan, FTDM_CHANNEL_EVENT);
            ftdmchan->last_event_time = ftdm_current_time_in_ms();
			chan_data->last_oob_event = span_data->last_oob_event;
		}

		return FTDM_SUCCESS;
	}

	return FTDM_FAIL;
}

/**
 * \brief Retrieves an event from a Pika span
 * \param span Span to retrieve event from
 * \param event FreeTDM event to return
 * \return Success or failure
 */
static FIO_SPAN_NEXT_EVENT_FUNCTION(pika_next_event)
{
	uint32_t i, event_id = 0;
	
	for(i = 1; i <= span->chan_count; i++) {
		if (ftdm_test_flag(span->channels[i], FTDM_CHANNEL_EVENT)) {
			pika_chan_data_t *chan_data = (pika_chan_data_t *) span->channels[i]->mod_data;
			PK_CHAR event_text[PKH_EVENT_MAX_NAME_LENGTH];
			
			ftdm_clear_flag(span->channels[i], FTDM_CHANNEL_EVENT);
			
			PKH_EVENT_GetText(chan_data->last_oob_event.id, event_text, sizeof(event_text));

			switch(chan_data->last_oob_event.id) {
			case PKH_EVENT_HDLC_MESSAGE:
				chan_data->hdlc_bytes = chan_data->last_oob_event.p2;
				continue;
			case PKH_EVENT_TRUNK_HOOKFLASH:
				event_id = FTDM_OOB_FLASH;
				break;
			case PKH_EVENT_TRUNK_RING_OFF:
				event_id = FTDM_OOB_RING_STOP;
				break;
			case PKH_EVENT_TRUNK_RING_ON:
				event_id = FTDM_OOB_RING_START;
				break;

			case PKH_EVENT_PHONE_OFFHOOK:
				ftdm_set_flag_locked(span->channels[i], FTDM_CHANNEL_OFFHOOK);
				event_id = FTDM_OOB_OFFHOOK;
				break;

			case PKH_EVENT_TRUNK_BELOW_THRESHOLD:
			case PKH_EVENT_TRUNK_ABOVE_THRESHOLD:
			case PKH_EVENT_PHONE_ONHOOK:
				ftdm_clear_flag_locked(span->channels[i], FTDM_CHANNEL_OFFHOOK);
				event_id = FTDM_OOB_ONHOOK;
				break;



			case PKH_EVENT_SPAN_ALARM_T1_RED:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_RED);
				snprintf(span->channels[i]->last_error, sizeof(span->channels[i]->last_error), "RED ALARM");
				event_id = FTDM_OOB_ALARM_TRAP;
				break;
			case PKH_EVENT_SPAN_ALARM_T1_YELLOW:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_YELLOW);
				snprintf(span->channels[i]->last_error, sizeof(span->channels[i]->last_error), "YELLOW ALARM");
				event_id = FTDM_OOB_ALARM_TRAP;
				break;
			case PKH_EVENT_SPAN_ALARM_T1_AIS:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_AIS);
				snprintf(span->channels[i]->last_error, sizeof(span->channels[i]->last_error), "AIS ALARM");
				event_id = FTDM_OOB_ALARM_TRAP;
				break;
			case PKH_EVENT_SPAN_ALARM_E1_RED:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_RED);
				snprintf(span->channels[i]->last_error, sizeof(span->channels[i]->last_error), "RED ALARM");
				event_id = FTDM_OOB_ALARM_TRAP;
				break;
			case PKH_EVENT_SPAN_ALARM_E1_RAI:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_RAI);
				snprintf(span->channels[i]->last_error, sizeof(span->channels[i]->last_error), "RAI ALARM");
				event_id = FTDM_OOB_ALARM_TRAP;
				break;
			case PKH_EVENT_SPAN_ALARM_E1_AIS:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_AIS);
				snprintf(span->channels[i]->last_error, sizeof(span->channels[i]->last_error), "AIS ALARM");
				event_id = FTDM_OOB_ALARM_TRAP;
				break;
			case PKH_EVENT_SPAN_ALARM_E1_RMAI:
			case PKH_EVENT_SPAN_ALARM_E1_TS16AIS:
			case PKH_EVENT_SPAN_ALARM_E1_TS16LOS:
			case PKH_EVENT_SPAN_OUT_OF_SYNC:
			case PKH_EVENT_SPAN_FRAMING_ERROR:
			case PKH_EVENT_SPAN_LOSS_OF_SIGNAL:
			case PKH_EVENT_SPAN_OUT_OF_CRC_MF_SYNC:
			case PKH_EVENT_SPAN_OUT_OF_CAS_MF_SYNC:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_GENERAL);
				snprintf(span->channels[i]->last_error, sizeof(span->channels[i]->last_error), "GENERAL ALARM");
				event_id = FTDM_OOB_ALARM_TRAP;
				break;
			case PKH_EVENT_SPAN_ALARM_T1_RED_CLEAR:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_RED);
			case PKH_EVENT_SPAN_ALARM_T1_YELLOW_CLEAR:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_YELLOW);
			case PKH_EVENT_SPAN_ALARM_T1_AIS_CLEAR:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_AIS);
			case PKH_EVENT_SPAN_ALARM_E1_RED_CLEAR:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_RED);
			case PKH_EVENT_SPAN_ALARM_E1_RAI_CLEAR:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_RAI);
			case PKH_EVENT_SPAN_ALARM_E1_AIS_CLEAR:
				ftdm_set_alarm_flag(span->channels[i], FTDM_ALARM_AIS);
			case PKH_EVENT_SPAN_ALARM_E1_RMAI_CLEAR:
			case PKH_EVENT_SPAN_ALARM_E1_TS16AIS_CLEAR:
			case PKH_EVENT_SPAN_ALARM_E1_TS16LOS_CLEAR:
			case PKH_EVENT_SPAN_IN_SYNC:
			case PKH_EVENT_SPAN_LOSS_OF_SIGNAL_CLEAR:
			case PKH_EVENT_SPAN_IN_CRC_MF_SYNC:
			case PKH_EVENT_SPAN_IN_CAS_MF_SYNC:
				ftdm_clear_alarm_flag(span->channels[i], FTDM_ALARM_GENERAL);
				event_id = FTDM_OOB_ALARM_CLEAR;
				break;
			case PKH_EVENT_SPAN_MESSAGE:
			case PKH_EVENT_SPAN_ABCD_SIGNAL_CHANGE:
				break;




			case PKH_EVENT_TRUNK_ONHOOK:
			case PKH_EVENT_TRUNK_OFFHOOK:				
			case PKH_EVENT_TRUNK_DIALED :
			case PKH_EVENT_TRUNK_REVERSAL:
			case PKH_EVENT_TRUNK_LCSO:
			case PKH_EVENT_TRUNK_DROPOUT:
			case PKH_EVENT_TRUNK_LOF:
			case PKH_EVENT_TRUNK_RX_OVERLOAD:
			default:
				ftdm_log(FTDM_LOG_DEBUG, "Unhandled event %d on channel %d [%s]\n", chan_data->last_oob_event.id, i, event_text);
				event_id = FTDM_OOB_INVALID;
				break;
			}

			span->channels[i]->last_event_time = 0;
			span->event_header.e_type = FTDM_EVENT_OOB;
			span->event_header.enum_id = event_id;
			span->event_header.channel = span->channels[i];
			*event = &span->event_header;
			return FTDM_SUCCESS;
		}
	}

	return FTDM_FAIL;
}

/**
 * \brief Destroys a Pika Span
 * \param span Span to destroy
 * \return Success
 */
static FIO_SPAN_DESTROY_FUNCTION(pika_span_destroy)
{
	pika_span_data_t *span_data = (pika_span_data_t *) span->mod_data;
	
	if (span_data) {
		PKH_QUEUE_Destroy(span_data->event_queue);
		ftdm_safe_free(span_data);
	}
	
	return FTDM_SUCCESS;
}

/**
 * \brief Destroys a Pika Channel
 * \param ftdmchan Channel to destroy
 * \return Success or failure
 */
static FIO_CHANNEL_DESTROY_FUNCTION(pika_channel_destroy)
{
	pika_chan_data_t *chan_data = (pika_chan_data_t *) ftdmchan->mod_data;
	pika_span_data_t *span_data = (pika_span_data_t *) ftdmchan->span->mod_data;
	
	if (!chan_data) {
		return FTDM_FAIL;
	}

	if (!ftdm_test_flag(chan_data, PK_FLAG_READY)) {
		goto end;
	}

	PKH_RECORD_Stop(chan_data->media_in);
	PKH_PLAY_Stop(chan_data->media_out);
	PKH_QUEUE_Destroy(chan_data->media_in_queue);
	PKH_QUEUE_Destroy(chan_data->media_out_queue);
	
	switch(ftdmchan->type) {
	case FTDM_CHAN_TYPE_FXS:
		PKH_QUEUE_Detach(span_data->event_queue, chan_data->handle);
		PKH_PHONE_Close(chan_data->handle);
		break;
	case FTDM_CHAN_TYPE_FXO:
		PKH_QUEUE_Detach(span_data->event_queue, chan_data->handle);
		PKH_TRUNK_Close(chan_data->handle);
		break;
	case FTDM_CHAN_TYPE_DQ921:
		PKH_SPAN_Stop(span_data->handle);
		break;
	default:
		break;
	}


	ftdm_mutex_destroy(&chan_data->digit_mutex);
	ftdm_buffer_destroy(&chan_data->digit_buffer);

 end:
	ftdm_safe_free(chan_data);
	
	return FTDM_SUCCESS;
}

/**
 * \brief Gets alarms from a Pika Channel (does nothing)
 * \param ftdmchan Channel to get alarms from
 * \return Failure
 */
static FIO_GET_ALARMS_FUNCTION(pika_get_alarms)
{
	return FTDM_FAIL;
}

static ftdm_io_interface_t pika_interface;

/**
 * \brief Loads Pika IO module
 * \param fio FreeTDM IO interface
 * \return Success or failure
 */
static FIO_IO_LOAD_FUNCTION(pika_init)
{

	PK_STATUS status;
	PK_CHAR error_text[PKH_ERROR_MAX_NAME_LENGTH];
	uint32_t i;
	int ok = 0;
	PKH_TLogMasks m;
	TPikaHandle tmpHandle;

	assert(fio != NULL);
	memset(&pika_interface, 0, sizeof(pika_interface));
	memset(&globals, 0, sizeof(globals));
	globals.general_config.region = PKH_TRUNK_NA;

	globals.profile_hash = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);

	// Open the system object, to enumerate boards configured for this system
	if ((status = PKH_SYSTEM_Open(&globals.system_handle)) != PK_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Error: PKH_SYSTEM_Open failed(%s)!\n",
				PKH_ERROR_GetText(status, error_text, sizeof(error_text)));
		return FTDM_FAIL;
	}

	// Retrieves a list of all boards in this system, existing,
	// or listed in pika.cfg
	if ((status = PKH_SYSTEM_Detect(globals.system_handle, &globals.board_list)) != PK_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Error: PKH_SYSTEM_Detect failed(%s)!\n",
				PKH_ERROR_GetText(status, error_text, sizeof(error_text)));
		return FTDM_FAIL;
	}
	
	PKH_SYSTEM_GetConfig(globals.system_handle, &globals.system_config);
	globals.system_config.maxAudioProcessBlockSize = PIKA_BLOCK_LEN;
	globals.system_config.playBufferSize = PIKA_BLOCK_SIZE;
	globals.system_config.recordBufferSize = PIKA_BLOCK_SIZE;
	globals.system_config.recordNumberOfBuffers = PIKA_NUM_BUFFERS;
	PKH_SYSTEM_SetConfig(globals.system_handle, &globals.system_config);

	status = PKH_MEDIA_STREAM_Create(&tmpHandle);
	status = PKH_RECORD_GetConfig(tmpHandle, &globals.record_config);
	status = PKH_PLAY_GetConfig(tmpHandle, &globals.play_config);
	status = PKH_EC_GetConfig(tmpHandle, &globals.ec_config);
	status = PKH_MEDIA_STREAM_Destroy(tmpHandle);

	

	ftdm_log(FTDM_LOG_DEBUG, "Found %u board%s\n", globals.board_list.numberOfBoards, globals.board_list.numberOfBoards == 1 ? "" : "s");
	for(i = 0; i < globals.board_list.numberOfBoards; ++i) {
		ftdm_log(FTDM_LOG_INFO, "Found PIKA board type:[%s] id:[%u] serno:[%u]\n", 
				pika_board_type_string(globals.board_list.board[i].type), globals.board_list.board[i].id, (uint32_t)
				globals.board_list.board[i].serialNumber);

		if (globals.board_list.board[i].type == PKH_BOARD_TYPE_DIGITAL_GATEWAY) {
			TPikaHandle board_handle, span_handle;
			PKH_TBoardConfig boardConfig;
			PKH_BOARD_GetConfig(board_handle, &boardConfig);
			PKH_BOARD_Open(globals.board_list.board[i].id, NULL, &board_handle);
			PKH_SPAN_Open(board_handle, 0, NULL, &span_handle);
			PKH_SPAN_GetConfig(span_handle, &globals.t1_span_config);
			PKH_SPAN_Close(span_handle);
			boardConfig.specific.DigitalGateway.interfaceType = PKH_BOARD_INTERFACE_TYPE_E1;
			PKH_BOARD_SetConfig(board_handle, &boardConfig);
			PKH_SPAN_Open(board_handle, 0, NULL, &span_handle);
			PKH_SPAN_GetConfig(span_handle, &globals.e1_span_config);
			PKH_SPAN_Close(span_handle);
			boardConfig.specific.DigitalGateway.interfaceType = PKH_BOARD_INTERFACE_TYPE_T1;
			PKH_BOARD_SetConfig(board_handle, &boardConfig);
			PKH_BOARD_Close(board_handle);
		}
		ok++;

	}

	if (!ok) {
		return FTDM_FAIL;
	}
	
	pika_interface.name = "pika";
	pika_interface.configure =  pika_configure;
	pika_interface.configure_span = pika_configure_span;
	pika_interface.open = pika_open;
	pika_interface.close = pika_close;
	pika_interface.wait = pika_wait;
	pika_interface.read = pika_read;
	pika_interface.write = pika_write;
	pika_interface.command = pika_command;
	pika_interface.poll_event = pika_poll_event;
	pika_interface.next_event = pika_next_event;
	pika_interface.channel_destroy = pika_channel_destroy;
	pika_interface.span_destroy = pika_span_destroy;
	pika_interface.get_alarms = pika_get_alarms;
	*fio = &pika_interface;


	ftdm_log(FTDM_LOG_INFO, "Dumping Default configs:\n");
	ftdm_log(FTDM_LOG_INFO, "rx-gain => %0.2f\n", (float)globals.record_config.gain);
	ftdm_log(FTDM_LOG_INFO, "rx-agc-enabled => %s\n", globals.record_config.AGC.enabled ? "true" : "false");
	ftdm_log(FTDM_LOG_INFO, "rx-agc-targetPower => %0.2f\n", (float)globals.record_config.AGC.targetPower);
	ftdm_log(FTDM_LOG_INFO, "rx-agc-minGain => %0.2f\n", (float)globals.record_config.AGC.minGain);
	ftdm_log(FTDM_LOG_INFO, "rx-agc-maxGain => %0.2f\n", (float)globals.record_config.AGC.maxGain);
	ftdm_log(FTDM_LOG_INFO, "rx-agc-attackRate => %d\n", (int)globals.record_config.AGC.attackRate);
	ftdm_log(FTDM_LOG_INFO, "rx-agc-decayRate => %d\n", (int)globals.record_config.AGC.decayRate);
	ftdm_log(FTDM_LOG_INFO, "rx-agc-speechThreshold => %0.2f\n", (float)globals.record_config.AGC.speechThreshold);
	ftdm_log(FTDM_LOG_INFO, "rx-vad-enabled => %s\n", globals.record_config.VAD.enabled ? "true" : "false");
	ftdm_log(FTDM_LOG_INFO, "rx-vad-activationThreshold => %0.2f\n", (float)globals.record_config.VAD.activationThreshold);
	ftdm_log(FTDM_LOG_INFO, "rx-vad-activationDebounceTime => %d\n", (int)globals.record_config.VAD.activationDebounceTime);
	ftdm_log(FTDM_LOG_INFO, "rx-vad-deactivationThreshold => %0.2f\n", (float)globals.record_config.VAD.deactivationThreshold);
	ftdm_log(FTDM_LOG_INFO, "rx-vad-deactivationDebounceTime => %d\n", (int)globals.record_config.VAD.deactivationDebounceTime);
	ftdm_log(FTDM_LOG_INFO, "rx-vad-preSpeechBufferSize => %d\n", (int)globals.record_config.VAD.preSpeechBufferSize);
	ftdm_log(FTDM_LOG_INFO, "tx-gain => %0.2f\n", (float)globals.play_config.gain);
	ftdm_log(FTDM_LOG_INFO, "tx-agc-enabled => %s\n", globals.play_config.AGC.enabled ? "true" : "false");
	ftdm_log(FTDM_LOG_INFO, "tx-agc-targetPower => %0.2f\n", (float)globals.play_config.AGC.targetPower);
	ftdm_log(FTDM_LOG_INFO, "tx-agc-minGain => %0.2f\n", (float)globals.play_config.AGC.minGain);
	ftdm_log(FTDM_LOG_INFO, "tx-agc-maxGain => %0.2f\n", (float)globals.play_config.AGC.maxGain);
	ftdm_log(FTDM_LOG_INFO, "tx-agc-attackRate => %d\n", (int)globals.play_config.AGC.attackRate);
	ftdm_log(FTDM_LOG_INFO, "tx-agc-decayRate => %d\n", (int)globals.play_config.AGC.decayRate);
	ftdm_log(FTDM_LOG_INFO, "tx-agc-speechThreshold => %0.2f\n", (float)globals.play_config.AGC.speechThreshold);
	ftdm_log(FTDM_LOG_INFO, "ec-doubleTalkerThreshold => %0.2f\n", (float)globals.ec_config.doubleTalkerThreshold);
	ftdm_log(FTDM_LOG_INFO, "ec-speechPresentThreshold => %0.2f\n", (float)globals.ec_config.speechPresentThreshold);
	ftdm_log(FTDM_LOG_INFO, "ec-echoSuppressionThreshold => %0.2f\n", (float)globals.ec_config.echoSuppressionThreshold);
	ftdm_log(FTDM_LOG_INFO, "ec-echoSuppressionEnabled => %s\n", globals.ec_config.echoSuppressionEnabled ? "true" : "false");
	ftdm_log(FTDM_LOG_INFO, "ec-comfortNoiseEnabled => %s\n", globals.ec_config.comfortNoiseEnabled ? "true" : "false");
	ftdm_log(FTDM_LOG_INFO, "ec-adaptationModeEnabled => %s\n", globals.ec_config.adaptationModeEnabled ? "true" : "false");

	

	memset(&m, 0, sizeof(m));
	//m.apiMask = 0xffffffff;
	//PKH_LOG_SetMasks(&m);

	return FTDM_SUCCESS;
}

/**
 * \brief Unloads Pika IO module
 * \return Success
 */
static FIO_IO_UNLOAD_FUNCTION(pika_destroy)
{
	uint32_t x;
	PK_STATUS status;
	PK_CHAR error_text[PKH_ERROR_MAX_NAME_LENGTH];

	for (x = 0; x < MAX_NUMBER_OF_TRUNKS; x++) {
		if (globals.open_boards[x]) {
			ftdm_log(FTDM_LOG_INFO, "Closing board %u\n", x);
			PKH_BOARD_Close(globals.open_boards[x]);
		}
	}

	// The system can now be closed.
	if ((status = PKH_SYSTEM_Close(globals.system_handle)) != PK_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Error: PKH_SYSTEM_Close failed(%s)!\n",
				PKH_ERROR_GetText(status, error_text, sizeof(error_text)));
	} else {
		ftdm_log(FTDM_LOG_INFO, "Closing system handle\n");
	}

	hashtable_destroy(globals.profile_hash);

	return FTDM_SUCCESS;
}

/**
 * \brief Pika IO module definition
 */
EX_DECLARE_DATA ftdm_module_t ftdm_module = { 
	"pika",
	pika_init,
	pika_destroy,
};

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

