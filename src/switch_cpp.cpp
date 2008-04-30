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
 * switch_cpp.cpp -- C++ wrapper
 *
 */

#include <switch.h>
#include <switch_cpp.h>

#ifdef _MSC_VER
#pragma warning(disable:4127 4003)
#endif


SWITCH_DECLARE_CONSTRUCTOR Event::Event(const char *type, const char *subclass_name)
{
	switch_event_types_t event_id;
	
	if (switch_name_event(type, &event_id) != SWITCH_STATUS_SUCCESS) {
		event_id = SWITCH_EVENT_MESSAGE;
	}

	switch_event_create_subclass(&event, event_id, subclass_name);
	serialized_string = NULL;
	mine = 1;
}

SWITCH_DECLARE_CONSTRUCTOR Event::Event(switch_event_t *wrap_me, int free_me)
{
	event = wrap_me;
	mine = free_me;
	serialized_string = NULL;
}

SWITCH_DECLARE_CONSTRUCTOR Event::~Event()
{

	if (serialized_string) {
		free(serialized_string);
	}

	if (event && mine) {
		switch_event_destroy(&event);
	}
}


SWITCH_DECLARE(const char *)Event::serialize(const char *format)
{
	int isxml = 0;

	if (serialized_string) {
		free(serialized_string);
	}

	if (!event) {
		return "";
	}

	if (format && !strcasecmp(format, "xml")) {
		isxml++;
	}

	if (isxml) {
		switch_xml_t xml;
		if ((xml = switch_event_xmlize(event, SWITCH_VA_NONE))) {
			serialized_string = switch_xml_toxml(xml, SWITCH_FALSE);
			switch_xml_free(xml);
			return serialized_string;
		} else {
			return "";
		}
	} else {
		if (switch_event_serialize(event, &serialized_string, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
			return serialized_string;
		}
	}
	
	return "";

}

SWITCH_DECLARE(bool) Event::fire(void)
{
	if (!mine) {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Not My event!\n");
		return false;
	}

	if (event) {
		switch_event_fire(&event);
		return true;
	}
	return false;
}

SWITCH_DECLARE(bool) Event::setPriority(switch_priority_t priority)
{
	if (event) {
        switch_event_set_priority(event, priority);
		return true;
    }
	return false;
}

SWITCH_DECLARE(char *)Event::getHeader(char *header_name)
{
	if (event) {
		return switch_event_get_header(event, header_name);
	}
	return NULL;
}

SWITCH_DECLARE(bool) Event::addHeader(const char *header_name, const char *value)
{
	if (event) {
		return switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, value) == SWITCH_STATUS_SUCCESS ? true : false;
	}

	return false;
}

SWITCH_DECLARE(bool) Event::delHeader(const char *header_name)
{
	if (event) {
		return switch_event_del_header(event, header_name) == SWITCH_STATUS_SUCCESS ? true : false;
	}

	return false;
}


SWITCH_DECLARE(bool) Event::addBody(const char *value)
{
	if (event) {
		return switch_event_add_body(event, "%s", value) == SWITCH_STATUS_SUCCESS ? true : false;
	}
	
	return false;
}

SWITCH_DECLARE(char *)Event::getBody(void)
{
	if (event) {
		return switch_event_get_body(event);
	}
	
	return NULL;
}

SWITCH_DECLARE(char *)Event::getType(void)
{
	if (event) {
		return switch_event_name(event->event_id);
	}
	
	return "invalid";
}

SWITCH_DECLARE_CONSTRUCTOR Stream::Stream()
{
	SWITCH_STANDARD_STREAM(mystream);
	stream_p = &mystream;
	mine = 1;
}

SWITCH_DECLARE_CONSTRUCTOR Stream::Stream(switch_stream_handle_t *sp)
{
	stream_p = sp;
	mine = 0;
}


SWITCH_DECLARE_CONSTRUCTOR Stream::~Stream()
{
	if (mine) {
		switch_safe_free(mystream.data);
	}
}

SWITCH_DECLARE(void) Stream::write(const char *data)
{
	stream_p->write_function(stream_p, "%s", data);
}

SWITCH_DECLARE(const char *)Stream::get_data()
{
	return stream_p ? (const char *)stream_p->data : NULL;
}


SWITCH_DECLARE_CONSTRUCTOR CoreSession::CoreSession()
{
	session = NULL;
	channel = NULL;
	uuid = NULL;
	tts_name = NULL;
	voice_name = NULL;
	memset(&args, 0, sizeof(args));
	ap = NULL;
	on_hangup = NULL;
	cb_state.function = NULL;
	
	memset(&caller_profile, 0, sizeof(caller_profile)); 
	caller_profile.source = "mod_unknown";
	caller_profile.dialplan = "";
	caller_profile.context = "";
	caller_profile.caller_id_name = "";
	caller_profile.caller_id_number = "";
	caller_profile.network_addr = "";
	caller_profile.ani = "";
	caller_profile.aniii = "";
	caller_profile.rdnis = "";
	caller_profile.username = "";
		
}

SWITCH_DECLARE_CONSTRUCTOR CoreSession::CoreSession(char *nuuid)
{
	memset(&caller_profile, 0, sizeof(caller_profile)); 	
	init_vars();
	if (!strchr(nuuid, '/') && (session = switch_core_session_locate(nuuid))) {
		uuid = strdup(nuuid);
		channel = switch_core_session_get_channel(session);
		allocated = 1;
    } else {
		switch_call_cause_t cause;
		if (switch_ivr_originate(NULL, &session, &cause, nuuid, 60, NULL, NULL, NULL, NULL, SOF_NONE) == SWITCH_STATUS_SUCCESS) {
			channel = switch_core_session_get_channel(session);
			allocated = 1;
			switch_set_flag(this, S_HUP);
			uuid = strdup(switch_core_session_get_uuid(session));
			switch_channel_set_state(switch_core_session_get_channel(session), CS_TRANSMIT);
		}
	}
}

SWITCH_DECLARE_CONSTRUCTOR CoreSession::CoreSession(switch_core_session_t *new_session)
{
	memset(&caller_profile, 0, sizeof(caller_profile)); 
	init_vars();
	if (new_session) {
		session = new_session;
		channel = switch_core_session_get_channel(session);
		allocated = 1;
		switch_core_session_read_lock(session);
	}
}

SWITCH_DECLARE_CONSTRUCTOR CoreSession::~CoreSession()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CoreSession::~CoreSession desctructor\n");
	switch_channel_t *channel = NULL;

	if (session) {
		channel = switch_core_session_get_channel(session);
		if (switch_test_flag(this, S_HUP)) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		}
		switch_core_session_rwunlock(session);
	}

	switch_safe_free(uuid);	
	switch_safe_free(tts_name);
	switch_safe_free(voice_name);
}

SWITCH_DECLARE(int) CoreSession::answer()
{
    switch_status_t status;

	sanity_check(-1);
    status = switch_channel_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(int) CoreSession::preAnswer()
{
    switch_status_t status;
	sanity_check(-1);
    status = switch_channel_pre_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(void) CoreSession::hangup(char *cause)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CoreSession::hangup\n");
	sanity_check_noreturn;
    switch_channel_hangup(channel, switch_channel_str2cause(cause));
}

SWITCH_DECLARE(void) CoreSession::setPrivate(char *var, void *val)
{
	sanity_check_noreturn;
    switch_channel_set_private(channel, var, val);
}

SWITCH_DECLARE(void *)CoreSession::getPrivate(char *var)
{
	sanity_check(NULL);
    return switch_channel_get_private(channel, var);
}

SWITCH_DECLARE(void) CoreSession::setVariable(char *var, char *val)
{
	sanity_check_noreturn;
    switch_channel_set_variable(channel, var, val);
}

SWITCH_DECLARE(const char *)CoreSession::getVariable(char *var)
{
	sanity_check(NULL);
    return switch_channel_get_variable(channel, var);
}

SWITCH_DECLARE(void) CoreSession::execute(char *app, char *data)
{
	const switch_application_interface_t *application_interface;
	sanity_check_noreturn;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CoreSession::execute.  app: %s data:%s\n", app, data);
	if ((application_interface = switch_loadable_module_get_application_interface(app))) {
		begin_allow_threads();
		switch_core_session_exec(session, application_interface, data);
		end_allow_threads();
	}
}

SWITCH_DECLARE(void) CoreSession::setDTMFCallback(void *cbfunc, char *funcargs) {

	sanity_check_noreturn;

	cb_state.funcargs = funcargs;
	cb_state.function = cbfunc;

	args.buf = &cb_state; 
	args.buflen = sizeof(cb_state);  // not sure what this is used for, copy mod_spidermonkey

    switch_channel_set_private(channel, "CoreSession", this);
        
	// we cannot set the actual callback to a python function, because
	// the callback is a function pointer with a specific signature.
	// so, set it to the following c function which will act as a proxy,
	// finding the python callback in the args callback args structure
	args.input_callback = dtmf_callback;  
	ap = &args;


}

SWITCH_DECLARE(void) CoreSession::sendEvent(Event *sendME)
{
	if (sendME->mine) {
		switch_core_session_receive_event(session, &sendME->event);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Not My event!\n");
	}
}

SWITCH_DECLARE(int) CoreSession::speak(char *text)
{
    switch_status_t status;

	sanity_check(-1);

	// create and store an empty filehandle in callback args 
	// to workaround a bug in the presumptuous process_callback_result()
    switch_file_handle_t fh = { 0 };
	store_file_handle(&fh);

	if (!tts_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No TTS engine specified\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!voice_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No TTS voice specified\n");
		return SWITCH_STATUS_FALSE;
	}


	begin_allow_threads();
	status = switch_ivr_speak_text(session, tts_name, voice_name, text, ap);
	end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(void) CoreSession::set_tts_parms(char *tts_name_p, char *voice_name_p)
{
	sanity_check_noreturn;
	switch_safe_free(tts_name);
	switch_safe_free(voice_name);
    tts_name = strdup(tts_name_p);
    voice_name = strdup(voice_name_p);
}



SWITCH_DECLARE(int) CoreSession::collectDigits(int timeout) {
	sanity_check(-1);
    begin_allow_threads();
	switch_ivr_collect_digits_callback(session, ap, timeout);
    end_allow_threads();
    return SWITCH_STATUS_SUCCESS;
} 

SWITCH_DECLARE(int) CoreSession::getDigits(char *dtmf_buf, 
						   switch_size_t buflen, 
						   switch_size_t maxdigits, 
						   char *terminators, 
						   char *terminator, 
						   int timeout)
{
    switch_status_t status;
	sanity_check(-1);
	begin_allow_threads();

    status = switch_ivr_collect_digits_count(session, 
											 dtmf_buf,
											 buflen,
											 maxdigits, 
											 terminators, 
											 terminator, 
											 (uint32_t) timeout, 0, 0);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "getDigits dtmf_buf: %s\n", dtmf_buf);
	end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(int) CoreSession::transfer(char *extension, char *dialplan, char *context)
{
    switch_status_t status;
	sanity_check(-1);
    begin_allow_threads();
    status = switch_ivr_session_transfer(session, extension, dialplan, context);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "transfer result: %d\n", status);
    end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(int) CoreSession::playAndGetDigits(int min_digits, 
								  int max_digits, 
								  int max_tries, 
								  int timeout, 
								  char *terminators, 
								  char *audio_files, 
								  char *bad_input_audio_files, 
								  char *dtmf_buf, 
								  char *digits_regex)
{
    switch_status_t status;
	sanity_check(-1);
	begin_allow_threads();
    status = switch_play_and_get_digits( session, 
										 (uint32_t) min_digits,
										 (uint32_t) max_digits,
										 (uint32_t) max_tries, 
										 (uint32_t) timeout, 
										 terminators, 
										 audio_files, 
										 bad_input_audio_files, 
										 dtmf_buf, 
										 128, 
										 digits_regex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "playAndGetDigits dtmf_buf: %s\n", dtmf_buf);

	end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(int) CoreSession::streamFile(char *file, int starting_sample_count) {

    switch_status_t status;
    //switch_file_handle_t fh = { 0 };
	const char *prebuf;

    sanity_check(-1);
	
	memset(&local_fh, 0, sizeof(local_fh));
	fhp = &local_fh;
    local_fh.samples = starting_sample_count;


	if ((prebuf = switch_channel_get_variable(this->channel, "stream_prebuffer"))) {
        int maybe = atoi(prebuf);
        if (maybe > 0) {
            local_fh.prebuf = maybe;
        }
	}


	store_file_handle(&local_fh);

    begin_allow_threads();
    status = switch_ivr_play_file(session, fhp, file, ap);
    end_allow_threads();

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;

}

SWITCH_DECLARE(bool) CoreSession::ready() {

	sanity_check(false);	
	return switch_channel_ready(channel) != 0;
}

SWITCH_DECLARE(int) CoreSession::originate(CoreSession *a_leg_session, char *dest, int timeout)
{

	switch_memory_pool_t *pool = NULL;
	switch_core_session_t *aleg_core_session = NULL;
	switch_call_cause_t cause;

	cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

	if (a_leg_session != NULL) {
		aleg_core_session = a_leg_session->session;
	}

	// this session has no valid switch_core_session_t at this point, and therefore
	// no valid channel.  since the threadstate is stored in the channel, and there 
	// is none, if we try to call begin_alllow_threads it will fail miserably.
	// use the 'a leg session' to do the thread swapping stuff.
    if (a_leg_session) a_leg_session->begin_allow_threads();

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		goto failed;
	}

	if (switch_ivr_originate(aleg_core_session, 
							 &session, 
							 &cause, 
							 dest, 
							 timeout,
							 NULL, 
							 NULL, 
							 NULL, 
							 &caller_profile,
							 SOF_NONE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Error Creating Outgoing Channel! [%s]\n", dest);
		goto failed;

	}

    if (a_leg_session) a_leg_session->end_allow_threads();
	channel = switch_core_session_get_channel(session);
	allocated = 1;
	switch_channel_set_state(switch_core_session_get_channel(session), CS_TRANSMIT);

	return SWITCH_STATUS_SUCCESS;

 failed:
    if (a_leg_session) a_leg_session->end_allow_threads();
	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(int) CoreSession::recordFile(char *file_name, int max_len, int silence_threshold, int silence_secs) 
{
	switch_file_handle_t fh = { 0 };
	switch_status_t status;

	fh.thresh = silence_threshold;
	fh.silence_hits = silence_secs;
	store_file_handle(&fh);
	begin_allow_threads();
	status = switch_ivr_record_file(session, &fh, file_name, &args, max_len);
	end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;

}

SWITCH_DECLARE(int) CoreSession::flushEvents() 
{
	switch_event_t *event;
	switch_channel_t *channel;

	if (!session) {
		return SWITCH_STATUS_FALSE;
	}
	channel = switch_core_session_get_channel(session);

	while (switch_core_session_dequeue_event(session, &event) == SWITCH_STATUS_SUCCESS) {
		switch_event_destroy(&event);
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) CoreSession::flushDigits() 
{
	switch_channel_flush_dtmf(switch_core_session_get_channel(session));
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) CoreSession::setAutoHangup(bool val) 
{
	if (!session) {
		return SWITCH_STATUS_FALSE;
	}	
	if (val) {
		switch_set_flag(this, S_HUP);
	} else {
		switch_clear_flag(this, S_HUP);
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) CoreSession::setCallerData(char *var, char *val) {

	if (strcmp(var, "dialplan") == 0) {
		caller_profile.dialplan = val;
	}
	if (strcmp(var, "context") == 0) {
		caller_profile.context = val;
	}
	if (strcmp(var, "caller_id_name") == 0) {
		caller_profile.caller_id_name = val;
	}
	if (strcmp(var, "caller_id_number") == 0) {
		caller_profile.caller_id_number = val;
	}
	if (strcmp(var, "network_addr") == 0) {
		caller_profile.network_addr = val;
	}
	if (strcmp(var, "ani") == 0) {
		caller_profile.ani = val;
	}
	if (strcmp(var, "aniii") == 0) {
		caller_profile.aniii = val;
	}
	if (strcmp(var, "rdnis") == 0) {
		caller_profile.rdnis = val;
	}
	if (strcmp(var, "username") == 0) {
		caller_profile.username = val;
	}

}

SWITCH_DECLARE(void) CoreSession::setHangupHook(void *hangup_func) {

	sanity_check_noreturn;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CoreSession::seHangupHook, hangup_func: %p\n", hangup_func);
    on_hangup = hangup_func;
    switch_channel_t *channel = switch_core_session_get_channel(session);

    hook_state = switch_channel_get_state(channel);
    switch_channel_set_private(channel, "CoreSession", this);
    switch_core_event_hook_add_state_change(session, hanguphook);
}

/** \brief Store a file handle in the callback args
 * 
 * In a few of the methods like playFile and streamfile,
 * an empty switch_file_handle_t is created and passed
 * to core, and stored in callback args so that the callback
 * handler can retrieve it for pausing, ff, rewinding file ptr. 
 * 
 * \param fh - a switch_file_handle_t
 */
void CoreSession::store_file_handle(switch_file_handle_t *fh) {
    cb_state.extra = fh;  // set a file handle so callback handler can pause
    args.buf = &cb_state;     
    ap = &args;
}


/* ---- methods not bound to CoreSession instance ---- */


SWITCH_DECLARE(void) console_log(char *level_str, char *msg)
{
    switch_log_level_t level = SWITCH_LOG_DEBUG;
    if (level_str) {
        level = switch_log_str2level(level_str);
		if (level == SWITCH_LOG_INVALID) {
			level = SWITCH_LOG_DEBUG;
		}
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, level, "%s", switch_str_nil(msg));
}

SWITCH_DECLARE(void) console_clean_log(char *msg)
{
    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN,SWITCH_LOG_DEBUG, "%s", switch_str_nil(msg));
}


SWITCH_DECLARE(char *)api_execute(char *cmd, char *arg)
{
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute(cmd, arg, NULL, &stream);
	return (char *) stream.data;
}

SWITCH_DECLARE(void) api_reply_delete(char *reply)
{
	if (!switch_strlen_zero(reply)) {
		free(reply);
	}
}


SWITCH_DECLARE(void) bridge(CoreSession &session_a, CoreSession &session_b)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "bridge called, session_a uuid: %s\n", session_a.get_uuid());
	switch_input_callback_function_t dtmf_func = NULL;
	switch_input_args_t args;

	session_a.begin_allow_threads();
	args = session_a.get_cb_args();  // get the cb_args data structure for session a
	dtmf_func = args.input_callback;   // get the call back function
	switch_ivr_multi_threaded_bridge(session_a.session, session_b.session, dtmf_func, args.buf, args.buf);
	session_a.end_allow_threads();

}


SWITCH_DECLARE_NONSTD(switch_status_t) hanguphook(switch_core_session_t *session_hungup) 
{
	switch_channel_t *channel = switch_core_session_get_channel(session_hungup);
	CoreSession *coresession = NULL;
	switch_channel_state_t state = switch_channel_get_state(channel);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "hangup_hook called\n");
	

	if ((coresession = (CoreSession *) switch_channel_get_private(channel, "CoreSession"))) {
		if (coresession->hook_state != state) {
			coresession->hook_state = state;
			coresession->check_hangup_hook();
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE_NONSTD(switch_status_t) dtmf_callback(switch_core_session_t *session_cb, 
							  void *input, 
							  switch_input_type_t itype, 
							  void *buf,  
							  unsigned int buflen) {
	
	switch_channel_t *channel = switch_core_session_get_channel(session_cb);
	CoreSession *coresession = NULL;
	switch_status_t result;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "dtmf_callback called\n");


	//coresession = (CoreSession *) buf;
	coresession = (CoreSession *) switch_channel_get_private(channel, "CoreSession");

	if (!coresession) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid CoreSession\n");		
		return SWITCH_STATUS_FALSE;
	}

	result = coresession->run_dtmf_callback(input, itype);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "process_callback_result returned\n");
	if (result) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "process_callback_result returned: %d\n", result);
	}
	return result;

}



SWITCH_DECLARE(switch_status_t) CoreSession::process_callback_result(char *ret)
{
	
    switch_file_handle_t *fh = NULL;	   

	if (fhp) {
		fh = fhp;
	} else {
		if (!cb_state.extra) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Process callback result aborted because cb_state.extra is null\n");
			return SWITCH_STATUS_FALSE;	
		}
		
		fh = (switch_file_handle_t *) cb_state.extra;    
	}


    if (!fh) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Process callback result aborted because fh is null\n");
		return SWITCH_STATUS_FALSE;	
    }

    if (!fh->file_interface) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Process callback result aborted because fh->file_interface is null\n");
		return SWITCH_STATUS_FALSE;	
    }

    if (!ret) {
		return SWITCH_STATUS_SUCCESS;	
    }

    if (!session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Process callback result aborted because session is null\n");
		return SWITCH_STATUS_FALSE;	
    }


    if (!strncasecmp(ret, "speed", 4)) {
		char *p;

		if ((p = strchr(ret, ':'))) {
			p++;
			if (*p == '+' || *p == '-') {
				int step;
				if (!(step = atoi(p))) {
					step = 1;
				}
				fh->speed += step;
			} else {
				int speed = atoi(p);
				fh->speed = speed;
			}
			return SWITCH_STATUS_SUCCESS;
		}

		return SWITCH_STATUS_FALSE;

    } else if (!strcasecmp(ret, "pause")) {
		if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
			switch_clear_flag(fh, SWITCH_FILE_PAUSE);
		} else {
			switch_set_flag(fh, SWITCH_FILE_PAUSE);
		}
		return SWITCH_STATUS_SUCCESS;
    } else if (!strcasecmp(ret, "stop")) {
		return SWITCH_STATUS_FALSE;
    } else if (!strcasecmp(ret, "restart")) {
		unsigned int pos = 0;
		fh->speed = 0;
		switch_core_file_seek(fh, &pos, 0, SEEK_SET);
		return SWITCH_STATUS_SUCCESS;
    } else if (!strncasecmp(ret, "seek", 4)) {
		switch_codec_t *codec;
		unsigned int samps = 0;
		unsigned int pos = 0;
		char *p;
		codec = switch_core_session_get_read_codec(session);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got codec\n");
		if ((p = strchr(ret, ':'))) {
			p++;
			if (*p == '+' || *p == '-') {
				int step;
				if (!(step = atoi(p))) {
					step = 1000;
				}
				if (step > 0) {
					samps = step * (codec->implementation->samples_per_second / 1000);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "going to seek\n");
					switch_core_file_seek(fh, &pos, samps, SEEK_CUR);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "done seek\n");
				} else {
					samps = step * (codec->implementation->samples_per_second / 1000);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "going to seek\n");
					switch_core_file_seek(fh, &pos, fh->pos - samps, SEEK_SET);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "done seek\n");
				}
			} else {
				samps = atoi(p) * (codec->implementation->samples_per_second / 1000);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "going to seek\n");
				switch_core_file_seek(fh, &pos, samps, SEEK_SET);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "done seek\n");
			}
		}

		return SWITCH_STATUS_SUCCESS;
    }

    if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "return success\n");
		return SWITCH_STATUS_SUCCESS;
    }

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no match, return false\n");

    return SWITCH_STATUS_FALSE;
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
