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
 * mod_lumenvox.cpp -- Lumenvox Interface
 * 
 *
 */
#ifdef __ICC
#pragma warning (disable:188)
#endif

#include <LVSpeechPort.h>
#include <switch.h>
#include <sstream>

SWITCH_MODULE_LOAD_FUNCTION(mod_lumenvox_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lumenvox_shutdown);
SWITCH_MODULE_DEFINITION(mod_lumenvox, mod_lumenvox_load, mod_lumenvox_shutdown, NULL);

typedef enum {
	LVFLAG_HAS_TEXT = (1 << 0),
	LVFLAG_BARGE = (1 << 1),
	LVFLAG_READY = (1 << 2)
} lvflag_t;

typedef struct {
	LVSpeechPort port;
	uint32_t flags;
	int sound_format;
	int channel;
	switch_mutex_t *flag_mutex;
} lumenvox_t;

static void log_callback(const char* message, void* userdata)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s\n", message);
}

static const char *state_names[] = {
	"NOT_READY",
	"READY",
	"BARGE_IN",
	"END_SPEECH",
	"STOPPED",
	"BARGE_IN_TIMEOUT",
	"END_SPEECH_TIMEOUT",
	"BEEP"
};

std::ostream& operator << (std::ostream& os ,const LVSemanticData& Data)
{
	int i;
	LVSemanticObject Obj;
	switch (Data.Type())
		{
		case SI_TYPE_BOOL:
			os << Data.GetBool();
			break;
		case SI_TYPE_INT:
			os << Data.GetInt();
			break;
		case SI_TYPE_DOUBLE:
			os << Data.GetDouble();
			break;
		case SI_TYPE_STRING:
			os << Data.GetString();
			break;
		case SI_TYPE_OBJECT:
			Obj = Data.GetSemanticObject();
			for (i = 0; i < Obj.NumberOfProperties(); ++i)
				{
					os << "<" << Obj.PropertyName(i) << ">";
					os << Obj.PropertyValue(i);
					os << "</" << Obj.PropertyName(i) << ">";
				}
			break;
		case SI_TYPE_ARRAY:
			for (i = 0; i < Data.GetSemanticArray().Size(); ++i)
				{
					os << Data.GetArray().At(i);
				}
			break;
		}
	return os;
}

//==============================================================================================
// code to plug LVInterpretation into any standard stream
std::ostream& operator << (std::ostream& os, const LVInterpretation& Interp)
{
	os << "<interpretation grammar=\""<<Interp.GrammarLabel()
	   <<"\" score=\""<<Interp.Score()<<"\">";
	os << "<result name=\""<<Interp.ResultName()<<"\">";
	os << Interp.ResultData();
	os << "</result>";
	os << "<input>";
	os << Interp.InputSentence();
	os << "</input>";
	os << "</interpretation>";
	return os;
}


static void state_change_callback(long new_state, unsigned long total_bytes,
				   unsigned long recorded_bytes, void *user_data)
{
	lumenvox_t *lv = (lumenvox_t *) user_data;
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "State: [%s] total bytes: [%ld] recorded bytes: [%ld]\n",
					  state_names[new_state],
					  total_bytes,
					  recorded_bytes);

	switch (new_state)
		{
		case STREAM_STATUS_READY:
			break;
		case STREAM_STATUS_STOPPED:
			switch_clear_flag_locked(lv, LVFLAG_READY);
			break;
		case STREAM_STATUS_END_SPEECH:
			switch_set_flag_locked(lv, LVFLAG_HAS_TEXT);
			break;
		case STREAM_STATUS_BARGE_IN:
			switch_set_flag_locked(lv, LVFLAG_BARGE);
			break;
		}
}

static switch_status_t lumenvox_asr_pause(switch_asr_handle_t *ah)
{
	lumenvox_t *lv = (lumenvox_t *) ah->private_info;

	if (switch_test_flag(lv, LVFLAG_READY)) {
		lv->port.StreamStop();
		switch_clear_flag_locked(lv, LVFLAG_READY);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}

static switch_status_t lumenvox_asr_resume(switch_asr_handle_t *ah)
{
	lumenvox_t *lv = (lumenvox_t *) ah->private_info;

	switch_clear_flag_locked(lv, LVFLAG_HAS_TEXT);

	if (!switch_test_flag(lv, LVFLAG_READY)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Manually Resuming\n");
		if (lv->port.StreamStart()) {
			lv->port.ClosePort();
			return SWITCH_STATUS_GENERR;
		}
		switch_set_flag_locked(lv, LVFLAG_READY);		
		return SWITCH_STATUS_SUCCESS;
	}
	
	return SWITCH_STATUS_GENERR;
}

/*! function to open the asr interface */
static switch_status_t lumenvox_asr_open(switch_asr_handle_t *ah, char *codec, int rate, char *dest, switch_asr_flag_t *flags) 
{
	
	lumenvox_t *lv;
	int error_code;
	
	if (!(lv = (lumenvox_t *) switch_core_alloc(ah->memory_pool, sizeof(*lv)))) {
		return SWITCH_STATUS_MEMERR;
	}

	if (rate != 8000 && rate != 16000) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Rate: Pick 8000 or 16000\n");
		return SWITCH_STATUS_FALSE;
	}
	
	if (!strcasecmp(codec, "PCMU")) {
		switch (rate) {
		case 8000:
			lv->sound_format = ULAW_8KHZ;
			break;
		}
	} else if (!strcasecmp(codec, "PCMA")) {
		switch (rate) {
		case 8000:
			lv->sound_format = ULAW_8KHZ;
			break;
		}
	} else if (!strcasecmp(codec, "speex")) {
		switch (rate) {
		case 8000:
			lv->sound_format = SPX_8KHZ;
			break;
		case 16000:
			lv->sound_format = SPX_16KHZ;
			break;
		}
	}

	if (!lv->sound_format) {
		codec = "L16";
		switch (rate) {
		case 8000:
			lv->sound_format = PCM_8KHZ;
			break;
		case 16000:
			lv->sound_format = PCM_16KHZ;
			break;
		}
	}

	if (!lv->sound_format) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot negotiate sound format.\n");
		return SWITCH_STATUS_FALSE;
	}	

	ah->rate = rate;
	ah->codec = switch_core_strdup(ah->memory_pool, codec);
	
	lv->port.OpenPort(log_callback, NULL, 5);
	error_code = lv->port.GetOpenPortStatus();
	
	switch(error_code)
		{
		case LV_FAILURE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Licenses Exceeded!\n");
			return SWITCH_STATUS_GENERR;
		case LV_OPEN_PORT_FAILED__PRIMARY_SERVER_NOT_RESPONDING:
		case LV_NO_SERVER_RESPONDING:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SRE Server Unavailable!\n");
			return SWITCH_STATUS_GENERR;
		case LV_SUCCESS:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Port Opened %d %dkhz.\n", lv->sound_format, rate);
			break;
		}


	// turn on sound and response file logging
	/*
	  int save_sound_files = 1;
	  lv->port.SetPropertyEx(PROP_EX_SAVE_SOUND_FILES,
						   PROP_EX_VALUE_TYPE_INT_PTR,
						   &save_sound_files);
	*/
	lv->channel = 1;
	lv->flags = *flags;
	lv->port.StreamSetParameter(STREAM_PARM_DETECT_BARGE_IN, 1);
	lv->port.StreamSetParameter(STREAM_PARM_DETECT_END_OF_SPEECH, 1);
	lv->port.StreamSetParameter(STREAM_PARM_AUTO_DECODE, 1);
	lv->port.StreamSetParameter(STREAM_PARM_DECODE_FLAGS, LV_DECODE_SEMANTIC_INTERPRETATION);
	lv->port.StreamSetParameter(STREAM_PARM_VOICE_CHANNEL, lv->channel);
	lv->port.StreamSetParameter(STREAM_PARM_GRAMMAR_SET, (long unsigned int) LV_ACTIVE_GRAMMAR_SET); 

	
	lv->port.StreamSetStateChangeCallBack(state_change_callback, lv);
	lv->port.StreamSetParameter(STREAM_PARM_SOUND_FORMAT, lv->sound_format);

	if (dest) {
		lv->port.SetClientPropertyEx(PROP_EX_SRE_SERVERS, PROP_EX_VALUE_TYPE_STRING, (void *) dest);
	}

	switch_mutex_init(&lv->flag_mutex, SWITCH_MUTEX_NESTED, ah->memory_pool);

	switch_set_flag_locked(lv, LVFLAG_READY);
	if (lv->port.StreamStart()) {
		lv->port.ClosePort();
		return SWITCH_STATUS_GENERR;
	}
	
	ah->private_info = lv;

	return SWITCH_STATUS_SUCCESS;
}

/*! function to load a grammar to the asr interface */
static switch_status_t lumenvox_asr_load_grammar(switch_asr_handle_t *ah, char *grammar, char *path)
{
	lumenvox_t *lv = (lumenvox_t *) ah->private_info;

	if (path) {
		if (!lv->port.LoadGrammar(grammar, path)) {
			if (!lv->port.ActivateGrammar(grammar)) {
				return SWITCH_STATUS_SUCCESS;
			}
		}
	} else {
		if (lv->port.ActivateGrammar(grammar)) {
			return SWITCH_STATUS_GENERR;
		}
	}

	return SWITCH_STATUS_GENERR;
}


/*! function to unload a grammar to the asr interface */
static switch_status_t lumenvox_asr_unload_grammar(switch_asr_handle_t *ah, char *grammar)
{
	lumenvox_t *lv = (lumenvox_t *) ah->private_info;

	if (lv->port.DeactivateGrammar(grammar)) {
		return SWITCH_STATUS_GENERR;
	}

	return SWITCH_STATUS_SUCCESS;
}

/*! function to close the asr interface */
static switch_status_t lumenvox_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	lumenvox_t *lv = (lumenvox_t *) ah->private_info;

	switch_set_flag(ah, SWITCH_ASR_FLAG_CLOSED);
	lv->port.ClosePort();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Port Closed.\n");
	return SWITCH_STATUS_SUCCESS;
}

/*! function to feed audio to the ASR*/
static switch_status_t lumenvox_asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags)
{

	lumenvox_t *lv = (lumenvox_t *) ah->private_info;
	
	if (!switch_test_flag(lv, LVFLAG_HAS_TEXT) && switch_test_flag(lv, LVFLAG_READY)) {
		if (lv->port.StreamSendData(data, len)) {
			return SWITCH_STATUS_FALSE;		
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
}

/*! function to read results from the ASR*/
static switch_status_t lumenvox_asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	lumenvox_t *lv = (lumenvox_t *) ah->private_info;

	return (switch_test_flag(lv, LVFLAG_HAS_TEXT) || switch_test_flag(lv, LVFLAG_BARGE)) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

/*! function to read results from the ASR*/
static switch_status_t lumenvox_asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags)
{
	lumenvox_t *lv = (lumenvox_t *) ah->private_info;
	switch_status_t ret = SWITCH_STATUS_SUCCESS;

	if (switch_test_flag(lv, LVFLAG_BARGE)) {
		switch_clear_flag_locked(lv, LVFLAG_BARGE);
		ret = SWITCH_STATUS_BREAK;
	}

	if (switch_test_flag(lv, LVFLAG_HAS_TEXT)) {
		std::stringstream ss;
		int numInterp;
		int code;


		lv->port.StreamStop();
		code = lv->port.WaitForEngineToIdle(3000, lv->channel);
	
		if (code == LV_TIME_OUT) {
			return SWITCH_STATUS_FALSE;
		}
		
		numInterp = lv->port.GetNumberOfInterpretations(lv->channel);
		
		for (int t = 0; t < numInterp; ++t) {
			ss << lv->port.GetInterpretation(lv->channel, t);
		}
		
		*xmlstr = strdup((char *)ss.str().c_str());
		switch_clear_flag_locked(lv, LVFLAG_HAS_TEXT);

		
		if (switch_test_flag(lv, SWITCH_ASR_FLAG_AUTO_RESUME)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Auto Resuming\n");
			switch_set_flag_locked(lv, LVFLAG_READY);
			if (lv->port.StreamStart()) {
				lv->port.ClosePort();
				return SWITCH_STATUS_GENERR;
			}
		}

		ret = SWITCH_STATUS_SUCCESS;
	}
	
	return ret;
}

static const switch_asr_interface_t lumenvox_asr_interface = {
	/*.interface_name*/			"lumenvox",
	/*.asr_open*/				lumenvox_asr_open,
	/*.asr_load_grammar*/		lumenvox_asr_load_grammar,
	/*.asr_unload_grammar*/		lumenvox_asr_unload_grammar,
	/*.asr_close*/				lumenvox_asr_close,
	/*.asr_feed*/				lumenvox_asr_feed,
	/*.asr_resume*/				lumenvox_asr_resume,
	/*.asr_pause*/				lumenvox_asr_pause,
	/*.asr_check_results*/		lumenvox_asr_check_results,
	/*.asr_get_results*/		lumenvox_asr_get_results
};

static const switch_loadable_module_interface_t lumenvox_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL,
	/*.chat_interface */ NULL,
	/*.say_interface */ NULL,
	/*.asr_interface */ &lumenvox_asr_interface
};

SWITCH_MODULE_LOAD_FUNCTION(mod_lumenvox_load)
{
	LVSpeechPort::RegisterAppLogMsg(log_callback, NULL, 5);
	//LVSpeechPort::SetClientPropertyEx(PROP_EX_SRE_SERVERS, PROP_EX_VALUE_TYPE_STRING, (void *)"127.0.0.1");
	
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &lumenvox_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lumenvox_shutdown)
{
	return SWITCH_STATUS_UNLOAD;
}
