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

static void event_handler(switch_event_t *event)
{
	EventConsumer *E = (EventConsumer *) event->bind_user_data;
	switch_event_t *dup;

	switch_event_dup(&dup, event);

	if (switch_queue_trypush(E->events, dup) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot queue any more events.....\n");
		switch_event_destroy(&dup);
	}

}

SWITCH_DECLARE_CONSTRUCTOR EventConsumer::EventConsumer(const char *event_name, const char *subclass_name, int len)
{

	switch_core_new_memory_pool(&pool);
	switch_queue_create(&events, len, pool);
	node_index = 0;
	ready = 1;

	if (!zstr(event_name)) {
		bind(event_name, subclass_name);
	}
}

SWITCH_DECLARE(int) EventConsumer::bind(const char *event_name, const char *subclass_name)
{
	switch_event_types_t event_id = SWITCH_EVENT_CUSTOM;

	if (!ready) {
		return 0;
	}

	if (switch_name_event(event_name, &event_id) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't bind to %s, event not found\n", event_name);
		return 0;
	}

	if (zstr(subclass_name)) {
		subclass_name = NULL;
	}

	if (node_index <= SWITCH_EVENT_ALL &&
		switch_event_bind_removable(__FILE__, event_id, subclass_name, event_handler, this, &enodes[node_index]) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "bound to %s %s\n", event_name, switch_str_nil(subclass_name));
		node_index++;
		return 1;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot bind to %s %s\n", event_name, switch_str_nil(subclass_name));
	return 0;
}


SWITCH_DECLARE(Event *) EventConsumer::pop(int block, int timeout)
{
	void *pop = NULL;
	Event *ret = NULL;
	switch_event_t *event;

	if (!ready) {
		return NULL;
	}

	if (block) {
		if (timeout > 0) {
			switch_queue_pop_timeout(events, &pop, (switch_interval_time_t) timeout * 1000); // millisec rather than microsec
		} else {
			switch_queue_pop(events, &pop);
		}
	} else {
		switch_queue_trypop(events, &pop);
	}

	if ((event = (switch_event_t *) pop)) {
		ret = new Event(event, 1);
	}

	return ret;
}

SWITCH_DECLARE(void) EventConsumer::cleanup()
{

	uint32_t i;
	void *pop;

	if (!ready) {
		return;
	}

	ready = 0;

	for (i = 0; i < node_index; i++) {
		switch_event_unbind(&enodes[i]);
	}

	node_index = 0;

	if (events) {
		switch_queue_interrupt_all(events);
	}

	while(switch_queue_trypop(events, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_event_t *event = (switch_event_t *) pop;
		switch_event_destroy(&event);
	}


	switch_core_destroy_memory_pool(&pool);

}


SWITCH_DECLARE_CONSTRUCTOR EventConsumer::~EventConsumer()
{
	cleanup();
}

SWITCH_DECLARE_CONSTRUCTOR IVRMenu::IVRMenu(IVRMenu *main,
											const char *name,
											const char *greeting_sound,
											const char *short_greeting_sound,
											const char *invalid_sound,
											const char *exit_sound,
											const char *transfer_sound,
											const char *confirm_macro,
											const char *confirm_key,
											const char *tts_engine,
											const char *tts_voice,
											int confirm_attempts,
											int inter_timeout,
											int digit_len,
											int timeout,
											int max_failures,
											int max_timeouts)
{
	menu = NULL;
	switch_core_new_memory_pool(&pool);
	switch_assert(pool);
	if (zstr(name)) {
		name = "no name";
	}

	switch_ivr_menu_init(&menu, main ? main->menu : NULL, name, greeting_sound, short_greeting_sound, invalid_sound,
						 exit_sound, transfer_sound, confirm_macro, confirm_key, tts_engine, tts_voice, confirm_attempts, inter_timeout,
						 digit_len, timeout, max_failures, max_timeouts, pool);


}

SWITCH_DECLARE_CONSTRUCTOR IVRMenu::~IVRMenu()
{
	if (menu) {
		switch_ivr_menu_stack_free(menu);
	}
	switch_core_destroy_memory_pool(&pool);
}

SWITCH_DECLARE(void) IVRMenu::bindAction(char *action, const char *arg, const char *bind)
{
	switch_ivr_action_t ivr_action = SWITCH_IVR_ACTION_NOOP;

	this_check_void();

	if (switch_ivr_menu_str2action(action, &ivr_action) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "bind %s to %s(%s)\n", bind, action, arg);
		switch_ivr_menu_bind_action(menu, ivr_action, arg, bind);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid action %s\n", action);
	}
}

SWITCH_DECLARE(void) IVRMenu::execute(CoreSession *session, const char *name)
{
	this_check_void();
	switch_ivr_menu_execute(session->session, menu, (char *)name, NULL);
}

SWITCH_DECLARE_CONSTRUCTOR API::API(CoreSession *s)
{
	if (s) {
		session = s->session;
	} else {
		session = NULL;
	}
}

SWITCH_DECLARE_CONSTRUCTOR API::~API()
{
	return;
}


SWITCH_DECLARE(const char *) API::execute(const char *cmd, const char *arg)
{
	switch_stream_handle_t stream = { 0 };
	this_check("");

	SWITCH_STANDARD_STREAM(stream);

	if (zstr(cmd)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No application specified\n");
		stream.write_function(&stream, "-ERR No application specified");
	} else {
		switch_api_execute(cmd, arg, session, &stream);
	}

	return (char *) stream.data;
}


/* we have to do this as a string because swig and languages can't find an embedded way to pass a big int */
SWITCH_DECLARE(char *) API::getTime(void)
{
	switch_time_t now = switch_micro_time_now() / 1000;
	snprintf(time_buf, sizeof(time_buf), "%" SWITCH_TIME_T_FMT, now);
	return time_buf;
}



SWITCH_DECLARE(const char *) API::executeString(const char *cmd)
{
	char *arg;
	switch_stream_handle_t stream = { 0 };
	char *mycmd = NULL;

	this_check("");

	SWITCH_STANDARD_STREAM(stream);

	if (zstr(cmd)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No application specified\n");
		stream.write_function(&stream, "-ERR No application specified");
	} else {
		mycmd = strdup(cmd);

		switch_assert(mycmd);

		if ((arg = strchr(mycmd, ' '))) {
			*arg++ = '\0';
		}

		switch_api_execute(mycmd, arg, session, &stream);
		switch_safe_free(mycmd);
	}

	return (char *) stream.data;
}

SWITCH_DECLARE_CONSTRUCTOR Event::Event(const char *type, const char *subclass_name)
{
	switch_event_types_t event_id;

	if (!strcasecmp(type, "json") && !zstr(subclass_name)) {
        if (switch_event_create_json(&event, subclass_name) != SWITCH_STATUS_SUCCESS) {
			return;
		}

		event_id = event->event_id;

    } else {
		if (switch_name_event(type, &event_id) != SWITCH_STATUS_SUCCESS) {
			event_id = SWITCH_EVENT_MESSAGE;
		}

		if (!zstr(subclass_name) && event_id != SWITCH_EVENT_CUSTOM) {
			switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_WARNING, "Changing event type to custom because you specified a subclass name!\n");
			event_id = SWITCH_EVENT_CUSTOM;
		}

		if (switch_event_create_subclass(&event, event_id, subclass_name) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Failed to create event!\n");
			event = NULL;
		}
	}

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

SWITCH_DECLARE(int)Event::chat_execute(const char *app, const char *data)
{
	return (int) switch_core_execute_chat_app(event, app, data);
}

SWITCH_DECLARE(int)Event::chat_send(const char *dest_proto)
{
	if (zstr(dest_proto)) {
		dest_proto = switch_event_get_header(event, "dest_proto");
	}

	return (int) switch_core_chat_send(dest_proto, event);
}

SWITCH_DECLARE(const char *)Event::serialize(const char *format)
{
	this_check("");


	switch_safe_free(serialized_string);

	if (!event) {
		return "";
	}

	if (format && !strcasecmp(format, "xml")) {
		switch_xml_t xml;
		if ((xml = switch_event_xmlize(event, SWITCH_VA_NONE))) {
			serialized_string = switch_xml_toxml(xml, SWITCH_FALSE);
			switch_xml_free(xml);
			return serialized_string;
		} else {
			return "";
		}
	} else if (format && !strcasecmp(format, "json")) {
		switch_event_serialize_json(event, &serialized_string);
		return serialized_string;
	} else {
		if (switch_event_serialize(event, &serialized_string, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
			char *new_serialized_string = switch_mprintf("%s", serialized_string);
			free(serialized_string);
			serialized_string = new_serialized_string;
			return serialized_string;
		}
	}

	return "";

}

SWITCH_DECLARE(bool) Event::fire(void)
{

	this_check(false);

	if (!mine) {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Not My event!\n");
		return false;
	}

	if (event) {
		switch_event_t *new_event;
		if (switch_event_dup(&new_event, event) == SWITCH_STATUS_SUCCESS) {
			if (switch_event_fire(&new_event) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Failed to fire the event!\n");
				switch_event_destroy(&new_event);
				return false;
			}
			return true;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Failed to dup the event!\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to fire an event that does not exist!\n");
	}
	return false;
}

SWITCH_DECLARE(bool) Event::setPriority(switch_priority_t priority)
{
	this_check(false);

	if (event) {
        switch_event_set_priority(event, priority);
		return true;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to setPriority an event that does not exist!\n");
    }
	return false;
}

SWITCH_DECLARE(const char *)Event::getHeader(const char *header_name)
{
	this_check("");

    if (zstr(header_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Trying to getHeader an invalid header!\n");
		return NULL;
    }

	if (event) {
		return switch_event_get_header(event, header_name);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to getHeader an event that does not exist!\n");
	}
	return NULL;
}

SWITCH_DECLARE(bool) Event::addHeader(const char *header_name, const char *value)
{
	this_check(false);

	if (event) {
		return switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, value) == SWITCH_STATUS_SUCCESS ? true : false;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to addHeader an event that does not exist!\n");
	}

	return false;
}

SWITCH_DECLARE(bool) Event::delHeader(const char *header_name)
{
	this_check(false);

	if (zstr(header_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Trying to delHeader an invalid header!\n");
		return false;
	}

	if (event) {
		return switch_event_del_header(event, header_name) == SWITCH_STATUS_SUCCESS ? true : false;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to delHeader an event that does not exist!\n");
	}

	return false;
}


SWITCH_DECLARE(bool) Event::addBody(const char *value)
{
	this_check(false);

	if (event) {
		return switch_event_add_body(event, "%s", value) == SWITCH_STATUS_SUCCESS ? true : false;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to addBody an event that does not exist!\n");
	}

	return false;
}

SWITCH_DECLARE(char *)Event::getBody(void)
{

	this_check((char *)"");

	if (event) {
		return switch_event_get_body(event);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to getBody an event that does not exist!\n");
	}

	return NULL;
}

SWITCH_DECLARE(const char *)Event::getType(void)
{
	this_check("");

	if (event) {
		return switch_event_name(event->event_id);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to getType an event that does not exist!\n");
	}

	return (char *) "invalid";
}

SWITCH_DECLARE(bool)Event::merge(Event *to_merge)
{
	this_check(false);

	if (!event) {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to merge to an event that does not exist!\n");
		return false;
	}

	if (!to_merge || !to_merge->event) {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Trying to merge from an event that does not exist!\n");
		return false;
	}

	switch_event_merge(event, to_merge->event);

	return true;
}

SWITCH_DECLARE_CONSTRUCTOR DTMF::DTMF(char idigit, uint32_t iduration)
{
	digit = idigit;

	if (iduration == 0) {
		iduration = SWITCH_DEFAULT_DTMF_DURATION;
	}

	duration = iduration;
}

SWITCH_DECLARE_CONSTRUCTOR DTMF::~DTMF()
{

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

/* WARNING!! you are not encouraged to use this unless you understand the risk!!! */
SWITCH_DECLARE(const char *) Stream::read(int *len)
{
	uint8_t *buff;

	this_check(NULL);

	if (!stream_p->read_function) return NULL;

	buff = stream_p->read_function(stream_p, len);

	if (!buff || *len <= 0) {
		*len = 0;
		return NULL;
	}

	return (const char *)buff;
}

SWITCH_DECLARE(void) Stream::write(const char *data)
{
	this_check_void();
	stream_p->write_function(stream_p, "%s", data);
}

SWITCH_DECLARE(void) Stream::raw_write(const char *data, int len)
{
	this_check_void();
	stream_p->raw_write_function(stream_p, (uint8_t *)data, len);
}

SWITCH_DECLARE(const char *)Stream::get_data()
{
	this_check("");

	return stream_p ? (const char *)stream_p->data : NULL;
}


SWITCH_DECLARE_CONSTRUCTOR CoreSession::CoreSession()
{
	init_vars();
}

SWITCH_DECLARE_CONSTRUCTOR CoreSession::CoreSession(char *nuuid, CoreSession *a_leg)
{
	switch_channel_t *other_channel = NULL;

	init_vars();

	if (a_leg && a_leg->session) {
		other_channel = switch_core_session_get_channel(a_leg->session);
	}

	if (!strchr(nuuid, '/') && (session = switch_core_session_force_locate(nuuid))) {
		uuid = strdup(nuuid);
		channel = switch_core_session_get_channel(session);
		allocated = 1;
    } else {
		cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		if (switch_ivr_originate(a_leg ? a_leg->session : NULL, &session, &cause, nuuid, 60, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL)
			== SWITCH_STATUS_SUCCESS) {
			channel = switch_core_session_get_channel(session);
			allocated = 1;
			switch_set_flag(this, S_HUP);
			uuid = strdup(switch_core_session_get_uuid(session));
			switch_channel_set_state(switch_core_session_get_channel(session), CS_SOFT_EXECUTE);
			switch_channel_wait_for_state(channel, other_channel, CS_SOFT_EXECUTE);
		}
	}
}

SWITCH_DECLARE_CONSTRUCTOR CoreSession::CoreSession(switch_core_session_t *new_session)
{
	init_vars();

	if (new_session && switch_core_session_read_lock_hangup(new_session) == SWITCH_STATUS_SUCCESS) {
		session = new_session;
		channel = switch_core_session_get_channel(session);
		allocated = 1;
		uuid = strdup(switch_core_session_get_uuid(session));
	}
}

SWITCH_DECLARE_CONSTRUCTOR CoreSession::~CoreSession()
{
	this_check_void();
	if (allocated) destroy();
}

SWITCH_DECLARE(char *) CoreSession::getXMLCDR()
{

	switch_xml_t cdr = NULL;

	this_check((char *)"");
	sanity_check((char *)"");

	switch_safe_free(xml_cdr_text);

	if (switch_ivr_generate_xml_cdr(session, &cdr) == SWITCH_STATUS_SUCCESS) {
		xml_cdr_text = switch_xml_toxml(cdr, SWITCH_FALSE);
		switch_xml_free(cdr);
	}

	return (char *) (xml_cdr_text ? xml_cdr_text : "");
}

SWITCH_DECLARE(void) CoreSession::setEventData(Event *e)
{
	this_check_void();
	sanity_check_noreturn;

	if (channel && e->event) {
		switch_channel_event_set_data(channel, e->event);
	}
}

SWITCH_DECLARE(int) CoreSession::answer()
{
    switch_status_t status;
	this_check(-1);
	sanity_check(-1);
    status = switch_channel_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}


SWITCH_DECLARE(int) CoreSession::print(char *txt)
{
    switch_status_t status;
	status = switch_core_session_print(session, switch_str_nil(txt));
	this_check(-1);
	sanity_check(-1);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(int) CoreSession::insertFile(const char *file, const char *insert_file, int sample_point)
{
    switch_status_t status;
	this_check(-1);
	sanity_check(-1);
    status = switch_ivr_insert_file(session, file, insert_file, (switch_size_t)sample_point);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(int) CoreSession::preAnswer()
{
    switch_status_t status;
	this_check(-1);
	sanity_check(-1);
    status = switch_channel_pre_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(void) CoreSession::hangupState(void)
{
	sanity_check_noreturn;
	this->begin_allow_threads();
	if (switch_channel_down(channel)) {
		switch_core_session_hangup_state(session, SWITCH_FALSE);
	}
	this->end_allow_threads();
}

SWITCH_DECLARE(void) CoreSession::hangup(const char *cause)
{
	this_check_void();
	sanity_check_noreturn;
    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CoreSession::hangup\n");
	this->begin_allow_threads();
    switch_channel_hangup(channel, switch_channel_str2cause(cause));
	this->end_allow_threads();
}

SWITCH_DECLARE(void) CoreSession::setPrivate(char *var, void *val)
{
	this_check_void();
	sanity_check_noreturn;
    switch_channel_set_private(channel, var, val);
}

SWITCH_DECLARE(void *)CoreSession::getPrivate(char *var)
{
	this_check(NULL);
	sanity_check(NULL);
    return switch_channel_get_private(channel, var);
}

SWITCH_DECLARE(void) CoreSession::setVariable(char *var, char *val)
{
	this_check_void();
	sanity_check_noreturn;
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CoreSession::setVariable('%s', '%s')\n", var, val);
	switch_channel_set_variable_var_check(channel, var, val, SWITCH_FALSE);
}

SWITCH_DECLARE(const char *)CoreSession::getVariable(char *var)
{
	this_check("");
	sanity_check("");
    return switch_channel_get_variable(channel, var);
}

SWITCH_DECLARE(void) CoreSession::execute(const char *app, const char *data)
{
	this_check_void();
	sanity_check_noreturn;

	if (zstr(app)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No application specified\n");
		return;
	}

	begin_allow_threads();
	switch_core_session_execute_application(session, app, data);
	end_allow_threads();
}

SWITCH_DECLARE(void) CoreSession::setDTMFCallback(void *cbfunc, char *funcargs) {

	this_check_void();
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
	this_check_void();
	sanity_check_noreturn;

	if (sendME->event) {
		switch_event_t *new_event;
		if (switch_event_dup(&new_event, sendME->event) == SWITCH_STATUS_SUCCESS) {
			switch_core_session_receive_event(session, &new_event);
		}
	}
}

SWITCH_DECLARE(int) CoreSession::speak(char *text)
{
    switch_status_t status;

	this_check(-1);
	sanity_check(-1);

	if (!tts_name) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No TTS engine specified\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!voice_name) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No TTS voice specified\n");
		return SWITCH_STATUS_FALSE;
	}


	begin_allow_threads();
	status = switch_ivr_speak_text(session, tts_name, voice_name, text, ap);
	end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

SWITCH_DECLARE(void) CoreSession::set_tts_parms(char *tts_name_p, char *voice_name_p)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "set_tts_parms is deprecated. Use set_tts_params.\n");
	this_check_void();
	sanity_check_noreturn;
	switch_safe_free(tts_name);
	switch_safe_free(voice_name);
    tts_name = strdup(tts_name_p);
    voice_name = strdup(voice_name_p);
}

SWITCH_DECLARE(void) CoreSession::set_tts_params(char *tts_name_p, char *voice_name_p)
{
	this_check_void();
	sanity_check_noreturn;
	switch_safe_free(tts_name);
	switch_safe_free(voice_name);
	tts_name = strdup(switch_str_nil(tts_name_p));
	voice_name = strdup(switch_str_nil(voice_name_p));
}

SWITCH_DECLARE(int) CoreSession::collectDigits(int abs_timeout) {
	return collectDigits(0, abs_timeout);
}

SWITCH_DECLARE(int) CoreSession::collectDigits(int digit_timeout, int abs_timeout) {
	this_check(-1);
	sanity_check(-1);
    begin_allow_threads();
	switch_ivr_collect_digits_callback(session, ap, digit_timeout, abs_timeout);
    end_allow_threads();
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(char *) CoreSession::getDigits(int maxdigits, char *terminators, int timeout)
{
    return getDigits(maxdigits, terminators, timeout, 0);
}

SWITCH_DECLARE(char *) CoreSession::getDigits(int maxdigits, char *terminators, int timeout, int interdigit)
{
    return getDigits(maxdigits, terminators, timeout, interdigit, 0);
}

SWITCH_DECLARE(char *) CoreSession::getDigits(int maxdigits,
											  char *terminators,
											  int timeout,
											  int interdigit,
											  int abstimeout)
{
	this_check((char *)"");
	sanity_check((char *)"");
	begin_allow_threads();
	char terminator;

	memset(dtmf_buf, 0, sizeof(dtmf_buf));
	switch_ivr_collect_digits_count(session,
									dtmf_buf,
									sizeof(dtmf_buf),
									maxdigits,
									terminators,
									&terminator,
									(uint32_t) timeout, (uint32_t)interdigit, (uint32_t)abstimeout);
									
	/* Only log DTMF buffer if sensitive_dtmf channel variable not set to true */
	if (!(switch_channel_var_true(switch_core_session_get_channel(session), SWITCH_SENSITIVE_DTMF_VARIABLE))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "getDigits dtmf_buf: %s\n", dtmf_buf);
	}
	
	end_allow_threads();
	return dtmf_buf;
}

SWITCH_DECLARE(int) CoreSession::transfer(char *extension, char *dialplan, char *context)
{
    switch_status_t status;
	this_check(-1);
	sanity_check(-1);
    begin_allow_threads();
    status = switch_ivr_session_transfer(session, extension, dialplan, context);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "transfer result: %d\n", status);
    end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}


SWITCH_DECLARE(char *) CoreSession::read(int min_digits,
										 int max_digits,
										 const char *prompt_audio_file,
										 int timeout,
										 const char *valid_terminators,
										 int digit_timeout)
{
	this_check((char *)"");
	sanity_check((char *)"");
	if (min_digits < 1) {
		min_digits = 1;
	}

	if (max_digits < 1) {
		max_digits = 1;
	}

	if (timeout < 1) {
		timeout = 1;
	}

    begin_allow_threads();
	switch_ivr_read(session, min_digits, max_digits, prompt_audio_file, NULL, dtmf_buf,
					sizeof(dtmf_buf), timeout, valid_terminators, (uint32_t)digit_timeout);
    end_allow_threads();

	return dtmf_buf;
}

SWITCH_DECLARE(char *) CoreSession::playAndGetDigits(int min_digits,
													 int max_digits,
													 int max_tries,
													 int timeout,
													 char *terminators,
													 char *audio_files,
													 char *bad_input_audio_files,
													 char *digits_regex,
													 const char *var_name,
													 int digit_timeout,
													 const char *transfer_on_failure)
{
	sanity_check((char *)"");
	this_check((char *)"");
	begin_allow_threads();
	memset(dtmf_buf, 0, sizeof(dtmf_buf));
	switch_play_and_get_digits( session,
								(uint32_t) min_digits,
								(uint32_t) max_digits,
								(uint32_t) max_tries,
								(uint32_t) timeout,
								terminators,
								audio_files,
								bad_input_audio_files,
								var_name,
								dtmf_buf,
								sizeof(dtmf_buf),
								digits_regex,
								(uint32_t) digit_timeout,
								transfer_on_failure);

	end_allow_threads();
	return dtmf_buf;
}

SWITCH_DECLARE(void) CoreSession::detectSpeech(char *arg0, char *arg1, char *arg2, char *arg3)
{
	this_check_void();
	sanity_check_noreturn;

	begin_allow_threads();

	if (!arg0) return;

	if (!strcasecmp(arg0, "grammar") && arg1 && arg2) {
		switch_ivr_detect_speech_load_grammar(session, arg1, arg2);
	} else if (!strcasecmp(arg0, "nogrammar") && arg1) {
		switch_ivr_detect_speech_unload_grammar(session, arg1);
	} else if (!strcasecmp(arg0, "grammaron") && arg1) {
		switch_ivr_detect_speech_enable_grammar(session, arg1);
	} else if (!strcasecmp(arg0, "grammaroff") && arg1) {
		switch_ivr_detect_speech_disable_grammar(session, arg1);
	} else if (!strcasecmp(arg0, "grammarsalloff")) {
		switch_ivr_detect_speech_disable_all_grammars(session);
	} else if (!strcasecmp(arg0, "init") && arg1 && arg2) {
		switch_ivr_detect_speech_init(session, arg1, arg2, NULL);
	} else if (!strcasecmp(arg0, "pause")) {
		switch_ivr_pause_detect_speech(session);
	} else if (!strcasecmp(arg0, "resume")) {
		switch_ivr_resume_detect_speech(session);
	} else if (!strcasecmp(arg0, "stop")) {
		switch_ivr_stop_detect_speech(session);
	} else if (!strcasecmp(arg0, "param") && arg1 && arg2) {
		switch_ivr_set_param_detect_speech(session, arg1, arg2);
	} else if (!strcasecmp(arg0, "start-input-timers")) {
		switch_ivr_detect_speech_start_input_timers(session);
	} else if (!strcasecmp(arg0, "start_input_timers")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "start_input_timers is deprecated, please use start-input-timers instead!\n");
		switch_ivr_detect_speech_start_input_timers(session);
	} else if (arg1 && arg2 && arg3) {
		switch_ivr_detect_speech(session, arg0, arg1, arg2, arg3, NULL);
	}

	end_allow_threads();
}

SWITCH_DECLARE(char *) CoreSession::playAndDetectSpeech(char *file, char *engine, char *grammar)
{
	sanity_check((char *)"");
	this_check((char *)"");
	begin_allow_threads();

	char *result = NULL;

	switch_status_t status = switch_ivr_play_and_detect_speech(session, file, engine, grammar, &result, 0, NULL);
	if (status == SWITCH_STATUS_SUCCESS) {
		// good
	} else if (status == SWITCH_STATUS_GENERR) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "GRAMMAR ERROR\n");
	} else if (status == SWITCH_STATUS_NOT_INITALIZED) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "ASR INIT ERROR\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "ERROR\n");
	}

	end_allow_threads();

	return result; // remeber to free me
}

SWITCH_DECLARE(void) CoreSession::say(const char *tosay, const char *module_name, const char *say_type, const char *say_method, const char *say_gender)
{
	this_check_void();
	sanity_check_noreturn;
	if (!(tosay && module_name && say_type && say_method)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! invalid args.\n");
		return;
	}
	begin_allow_threads();
	switch_ivr_say(session, tosay, module_name, say_type, say_method, say_gender, ap);
    end_allow_threads();
}

SWITCH_DECLARE(void) CoreSession::sayPhrase(const char *phrase_name, const char *phrase_data, const char *phrase_lang)
{
	this_check_void();
	sanity_check_noreturn;

	if (!(phrase_name)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! invalid args.\n");
		return;
	}

	begin_allow_threads();
	switch_ivr_phrase_macro(session, phrase_name, phrase_data, phrase_lang, ap);
    end_allow_threads();
}

SWITCH_DECLARE(int) CoreSession::streamFile(char *file, int starting_sample_count) {

    switch_status_t status;
    //switch_file_handle_t fh = { 0 };
	const char *prebuf;
	switch_file_handle_t local_fh;

	this_check(-1);
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

    begin_allow_threads();
    status = switch_ivr_play_file(session, fhp, file, ap);
    end_allow_threads();

	fhp = NULL;

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;

}

SWITCH_DECLARE(int) CoreSession::sleep(int ms, int sync) {

    switch_status_t status;

	this_check(-1);
    sanity_check(-1);

    begin_allow_threads();
    status = switch_ivr_sleep(session, ms, (switch_bool_t) sync, ap);
    end_allow_threads();

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;

}

SWITCH_DECLARE(bool) CoreSession::ready() {

	this_check(false);

	if (!session) {
		return false;
	}
	sanity_check(false);

	return switch_channel_ready(channel) != 0;
}


SWITCH_DECLARE(bool) CoreSession::bridged() {

	this_check(false);

	if (!session) {
		return false;
	}
	sanity_check(false);

	return (switch_channel_up(channel) && switch_channel_test_flag(channel, CF_BRIDGED));
}

SWITCH_DECLARE(bool) CoreSession::mediaReady() {

	this_check(false);
	sanity_check(false);
	return switch_channel_media_ready(channel) != 0;
}

SWITCH_DECLARE(bool) CoreSession::answered() {

	this_check(false);
	sanity_check(false);
	return switch_channel_test_flag(channel, CF_ANSWERED) != 0;
}

SWITCH_DECLARE(void) CoreSession::destroy(void)
{
	this_check_void();

	if (!allocated) {
		return;
	}

	allocated = 0;

	switch_safe_free(xml_cdr_text);
	switch_safe_free(uuid);
	switch_safe_free(tts_name);
	switch_safe_free(voice_name);

	if (session) {
		if (!channel) {
			channel = switch_core_session_get_channel(session);
		}

		if (channel) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
							  "%s destroy/unlink session from object\n", switch_channel_get_name(channel));
			switch_channel_set_private(channel, "CoreSession", NULL);
			if (switch_channel_up(channel) && switch_test_flag(this, S_HUP) && !switch_channel_test_flag(channel, CF_TRANSFER)) {
				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			}
		}

        switch_core_session_rwunlock(session);
		session = NULL;
		channel = NULL;
    }

	init_vars();

}

SWITCH_DECLARE(const char *) CoreSession::hangupCause()
{
	this_check(NULL);
	return switch_channel_cause2str(cause);
}

SWITCH_DECLARE(const char *) CoreSession::getState()
{
	this_check(NULL);

	if (channel) {
		return switch_channel_state_name(switch_channel_get_state(channel));
	}

	return "ERROR";

}

SWITCH_DECLARE(int) CoreSession::originate(CoreSession *a_leg_session, char *dest, int timeout, switch_state_handler_table_t *handlers)
{

	switch_core_session_t *aleg_core_session = NULL;

	this_check(0);

	cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

	if (a_leg_session != NULL) {
		aleg_core_session = a_leg_session->session;
	}

	// this session has no valid switch_core_session_t at this point, and therefore
	// no valid channel.  since the threadstate is stored in the channel, and there
	// is none, if we try to call begin_alllow_threads it will fail miserably.
	// use the 'a leg session' to do the thread swapping stuff.
    if (a_leg_session) a_leg_session->begin_allow_threads();

	if (switch_ivr_originate(aleg_core_session,
							 &session,
							 &cause,
							 dest,
							 timeout,
							 handlers,
							 NULL,
							 NULL,
							 NULL,
							 NULL,
							 SOF_NONE,
							 NULL,
							 NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Error Creating Outgoing Channel! [%s]\n", dest);
		goto failed;

	}

    if (a_leg_session) a_leg_session->end_allow_threads();
	channel = switch_core_session_get_channel(session);
	allocated = 1;
	switch_safe_free(uuid);
	uuid = strdup(switch_core_session_get_uuid(session));
	switch_channel_set_state(switch_core_session_get_channel(session), CS_SOFT_EXECUTE);

	return SWITCH_STATUS_SUCCESS;

 failed:
    if (a_leg_session) a_leg_session->end_allow_threads();
	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(int) CoreSession::recordFile(char *file_name, int time_limit, int silence_threshold, int silence_hits)
{
	switch_status_t status;
	switch_file_handle_t local_fh;

	this_check(-1);
	sanity_check(-1);

	if (!file_name) return 0;
	memset(&local_fh, 0, sizeof(local_fh));
	fhp = &local_fh;
	local_fh.thresh = silence_threshold;
	local_fh.silence_hits = silence_hits;

	begin_allow_threads();
	status = switch_ivr_record_file(session, &local_fh, file_name, ap, time_limit);
	end_allow_threads();

	fhp = NULL;

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;

}

SWITCH_DECLARE(int) CoreSession::flushEvents()
{
	switch_event_t *event;

	this_check(-1);
	sanity_check(-1);

	if (!session) {
		return SWITCH_STATUS_FALSE;
	}

	while (switch_core_session_dequeue_event(session, &event, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
		switch_event_destroy(&event);
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) CoreSession::flushDigits()
{
	this_check(-1);
	sanity_check(-1);
	switch_channel_flush_dtmf(switch_core_session_get_channel(session));
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) CoreSession::setAutoHangup(bool val)
{
	this_check(-1);
	sanity_check(-1);

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

SWITCH_DECLARE(void) CoreSession::waitForAnswer(CoreSession *calling_session)
{
	this_check_void();
	sanity_check_noreturn;

	switch_ivr_wait_for_answer(calling_session ? calling_session->session : NULL, session);

}

SWITCH_DECLARE(void) CoreSession::setHangupHook(void *hangup_func) {

	this_check_void();
	sanity_check_noreturn;

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CoreSession::seHangupHook, hangup_func: %p\n", hangup_func);
    on_hangup = hangup_func;
    switch_channel_t *channel = switch_core_session_get_channel(session);

    hook_state = switch_channel_get_state(channel);
    switch_channel_set_private(channel, "CoreSession", this);
    switch_core_event_hook_add_state_change(session, hanguphook);
}

SWITCH_DECLARE(void) CoreSession::consoleLog(char *level_str, char *msg)
{
	switch_log_level_t level = SWITCH_LOG_DEBUG;
	if (level_str) {
		level = switch_log_str2level(level_str);
		if (level == SWITCH_LOG_INVALID) {
			level = SWITCH_LOG_DEBUG;
		}
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), level, "%s", switch_str_nil(msg));
}

SWITCH_DECLARE(void) CoreSession::consoleLog2(char *level_str, char *file, char *func, int line, char *msg)
{
	switch_log_level_t level = SWITCH_LOG_DEBUG;
	if (level_str) {
		level = switch_log_str2level(level_str);
		if (level == SWITCH_LOG_INVALID) {
			level = SWITCH_LOG_DEBUG;
		}
	}
    switch_log_printf(SWITCH_CHANNEL_ID_SESSION, file, func, line, (const char*)session,
					  level, "%s", switch_str_nil(msg));
}

/* ---- methods not bound to CoreSession instance ---- */


SWITCH_DECLARE(int) globalSetVariable(const char *var, const char *val, const char *val2)
{
	if (zstr(val)) val = NULL;
	if (zstr(val2)) val2 = NULL;

	if (val2) {
		return switch_core_set_var_conditional(var, val, val2);
	} else {
		switch_core_set_variable(var, val);
		return SWITCH_STATUS_SUCCESS;
	}
}

SWITCH_DECLARE(void) setGlobalVariable(char *var_name, char *var_val)
{
	switch_core_set_variable(var_name, var_val);
}

SWITCH_DECLARE(char *) getGlobalVariable(char *var_name)
{
	return switch_core_get_variable_dup(var_name);
}


SWITCH_DECLARE(bool) running(void)
{
	return switch_core_running() ? true : false;
}

SWITCH_DECLARE(void) consoleLog(char *level_str, char *msg)
{
	return console_log(level_str, msg);
}

SWITCH_DECLARE(void) consoleLog2(char *level_str, char *file, char *func, int line, char *msg)
{
	return console_log2(level_str, file, func, line, msg);
}

SWITCH_DECLARE(void) consoleCleanLog(char *msg)
{
	return console_clean_log(msg);
}

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

SWITCH_DECLARE(void) console_log2(char *level_str, char *file, char *func, int line, char *msg)
{
    switch_log_level_t level = SWITCH_LOG_DEBUG;
    if (level_str) {
        level = switch_log_str2level(level_str);
        if (level == SWITCH_LOG_INVALID) {
            level = SWITCH_LOG_DEBUG;
        }
    }
    switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, level, "%s", switch_str_nil(msg));
}

SWITCH_DECLARE(void) console_clean_log(char *msg)
{
    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN,SWITCH_LOG_DEBUG, "%s", switch_str_nil(msg));
}

SWITCH_DECLARE(bool) email(char *to, char *from, char *headers, char *body, char *file, char *convert_cmd, char *convert_ext)
{
    if (switch_simple_email(to, from, headers, body, file, convert_cmd, convert_ext) == SWITCH_TRUE) {
      return true;
    }
    return false;
}

SWITCH_DECLARE(void) switch_msleep(unsigned ms)
{
	switch_sleep(ms * 1000);
	return;
}

SWITCH_DECLARE(void) bridge(CoreSession &session_a, CoreSession &session_b)
{
	switch_input_callback_function_t dtmf_func = NULL;
	switch_input_args_t args;
	switch_channel_t *channel_a = NULL, *channel_b = NULL;
	const char *err = "Channels not ready\n";

	if (session_a.allocated && session_a.session && session_b.allocated && session_b.session) {
		channel_a = switch_core_session_get_channel(session_a.session);
		channel_b = switch_core_session_get_channel(session_b.session);

		if (switch_channel_ready(channel_a) && switch_channel_ready(channel_b)) {
			session_a.begin_allow_threads();
			if (switch_channel_direction(channel_a) == SWITCH_CALL_DIRECTION_INBOUND && !switch_channel_media_ready(channel_a)) {
				switch_channel_pre_answer(channel_a);
			}

			if (switch_channel_ready(channel_a) && switch_channel_ready(channel_b)) {
				args = session_a.get_cb_args();  // get the cb_args data structure for session a
				dtmf_func = args.input_callback;   // get the call back function
				err = NULL;
				switch_ivr_multi_threaded_bridge(session_a.session, session_b.session, dtmf_func, args.buf, args.buf);
			}
			session_a.end_allow_threads();
		}
	}

	if (err) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a.session), SWITCH_LOG_ERROR, "%s", err);
	}


}

SWITCH_DECLARE_NONSTD(switch_status_t) hanguphook(switch_core_session_t *session_hungup)
{
	if (session_hungup) {
		switch_channel_t *channel = switch_core_session_get_channel(session_hungup);
		CoreSession *coresession = NULL;
		switch_channel_state_t state = switch_channel_get_state(channel);

		if ((coresession = (CoreSession *) switch_channel_get_private(channel, "CoreSession"))) {
			if (coresession->hook_state != state) {
				coresession->cause = switch_channel_get_cause(channel);
				coresession->hook_state = state;
				coresession->check_hangup_hook();
			}
		}

		return SWITCH_STATUS_SUCCESS;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hangup hook called with null session, something is horribly wrong\n");
		return SWITCH_STATUS_FALSE;
	}
}


SWITCH_DECLARE_NONSTD(switch_status_t) dtmf_callback(switch_core_session_t *session_cb,
													 void *input,
													 switch_input_type_t itype,
													 void *buf,
													 unsigned int buflen) {

	switch_channel_t *channel = switch_core_session_get_channel(session_cb);
	CoreSession *coresession = NULL;

	coresession = (CoreSession *) switch_channel_get_private(channel, "CoreSession");

	if (!coresession) {
		return SWITCH_STATUS_FALSE;
	}

	return coresession->run_dtmf_callback(input, itype);
}


SWITCH_DECLARE(switch_status_t) CoreSession::process_callback_result(char *result)
{

	this_check(SWITCH_STATUS_FALSE);
	sanity_check(SWITCH_STATUS_FALSE);

	return switch_ivr_process_fh(session, result, fhp);
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
