/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013, Grasshopper
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
 * The Original Code is mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * record_component.c -- Rayo record component implementation
 *
 */
#include "rayo_components.h"
#include "rayo_elements.h"

/* TODO timeouts / durations are affected by pause/resume */

/**
 * settings
 */
static struct {
	const char *record_file_prefix;
	const char *record_file_format;
} globals;

/**
 * A record component
 */
struct record_component {
	/** component base class */
	struct rayo_component base;
	/** maximum duration allowed */
	int max_duration;
	/** timeout for total silence */
	int initial_timeout;
	/** timeout for silence after initial utterance */
	int final_timeout;
	/** duplex/send/recv */
	const char *direction;
	/** true if mixed (mono) */
	int mix;
	/** true if start beep to be played */
	int start_beep;
	/** true if stop beep to be played */
	int stop_beep;
	/** time recording started */
	switch_time_t start_time;
	/** duration of this recording */
	int duration_ms;
	/** path on local filesystem */
	char *local_file_path;
	/** true if recording was stopped */
	int stop;
};

#define RECORD_COMPONENT(x) ((struct record_component *)x)

/* 1000 Hz beep for 250ms */
#define RECORD_BEEP "tone_stream://%(250,0,1000)"

#define RECORD_COMPLETE_MAX_DURATION "max-duration", RAYO_RECORD_COMPLETE_NS
#define RECORD_COMPLETE_INITIAL_TIMEOUT "initial-timeout", RAYO_RECORD_COMPLETE_NS
#define RECORD_COMPLETE_FINAL_TIMEOUT "final-timeout", RAYO_RECORD_COMPLETE_NS

/**
 * Notify completion of record component
 */
static void complete_record(struct rayo_component *component, const char *reason, const char *reason_namespace)
{
	switch_core_session_t *session = NULL;
	const char *uuid = component->parent->id;
	char *uri = switch_mprintf("file://%s", RECORD_COMPONENT(component)->local_file_path);
	iks *recording;
	switch_size_t file_size = 0;
/* TODO this doesn't work with HTTP */
#if 0
	switch_file_t *file;

	if (switch_file_open(&file, RECORD_COMPONENT(component)->local_file_path, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD, RAYO_POOL(component)) == SWITCH_STATUS_SUCCESS) {
		file_size = switch_file_get_size(file);
		switch_file_close(file);
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_INFO, "Failed to open %s.\n", RECORD_COMPONENT(component)->local_file_path);
	}
#endif

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Recording %s done.\n", RECORD_COMPONENT(component)->local_file_path);

	if (RECORD_COMPONENT(component)->stop_beep && (session = switch_core_session_locate(uuid))) {
		switch_ivr_displace_session(session, RECORD_BEEP, 0, "");
		switch_core_session_rwunlock(session);
	}

	/* send complete event to client */
	recording = iks_new("recording");
	iks_insert_attrib(recording, "xmlns", RAYO_RECORD_COMPLETE_NS);
	iks_insert_attrib(recording, "uri", uri);
	iks_insert_attrib_printf(recording, "duration", "%i", RECORD_COMPONENT(component)->duration_ms);
	iks_insert_attrib_printf(recording, "size", "%"SWITCH_SIZE_T_FMT, file_size);
	rayo_component_send_complete_with_metadata(component, reason, reason_namespace, recording, 1);
	iks_delete(recording);

	RAYO_UNLOCK(component);

	switch_safe_free(uri);
}

/**
 * Handle RECORD_STOP event from FreeSWITCH.
 * @param event received from FreeSWITCH core.  It will be destroyed by the core after this function returns.
 */
static void on_call_record_stop_event(switch_event_t *event)
{
	const char *file_path = switch_event_get_header(event, "Record-File-Path");
	struct rayo_component *component = RAYO_COMPONENT_LOCATE(file_path);

	if (component) {
		RECORD_COMPONENT(component)->duration_ms += (switch_micro_time_now() - RECORD_COMPONENT(component)->start_time) / 1000;
		if (RECORD_COMPONENT(component)->stop) {
			complete_record(component, COMPONENT_COMPLETE_STOP);
		} else {
			/* TODO assume final timeout, for now */
			complete_record(component, RECORD_COMPLETE_FINAL_TIMEOUT);
		}
	}
}

/**
 * Create a record component
 */
static struct rayo_component *record_component_create(struct rayo_actor *actor, const char *type, const char *client_jid, iks *record)
{
	switch_memory_pool_t *pool;
	struct record_component *record_component = NULL;
	char *local_file_path;
	char *fs_file_path;
	switch_bool_t start_paused;

	/* validate record attributes */
	if (!VALIDATE_RAYO_RECORD(record)) {
		return NULL;
	}

	start_paused = iks_find_bool_attrib(record, "start-paused");

	/* create record filename from session UUID and ref */
	/* for example: prefix/1234-1234-1234-1234-30.wav */
	local_file_path = switch_mprintf("%s%s-%i.%s",
		globals.record_file_prefix,
		actor->id, rayo_actor_seq_next(actor), iks_find_attrib(record, "format"));

	fs_file_path = switch_mprintf("{pause=%s}fileman://%s",
		start_paused ? "true" : "false",
		local_file_path);

	switch_core_new_memory_pool(&pool);
	record_component = switch_core_alloc(pool, sizeof(*record_component));
	rayo_component_init(RAYO_COMPONENT(record_component), pool, type, "record", fs_file_path, actor, client_jid);
	record_component->max_duration = iks_find_int_attrib(record, "max-duration");
	record_component->initial_timeout = iks_find_int_attrib(record, "initial-timeout");
	record_component->final_timeout = iks_find_int_attrib(record, "final-timeout");
	record_component->direction = switch_core_strdup(RAYO_POOL(record_component), iks_find_attrib_soft(record, "direction"));
	record_component->mix = iks_find_bool_attrib(record, "mix");
	record_component->start_beep = iks_find_bool_attrib(record, "start-beep");
	record_component->stop_beep = iks_find_bool_attrib(record, "stop-beep");
	record_component->start_time = start_paused ? 0 : switch_micro_time_now();
	record_component->local_file_path = switch_core_strdup(RAYO_POOL(record_component), local_file_path);

	switch_safe_free(local_file_path);
	switch_safe_free(fs_file_path);

	return RAYO_COMPONENT(record_component);
}

/**
 * Start recording call
 * @param session the session to record
 * @param record the record component
 */
static int start_call_record(switch_core_session_t *session, struct rayo_component *component)
{
	struct record_component *record_component = RECORD_COMPONENT(component);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int max_duration_sec = 0;

	switch_channel_set_variable(channel, "RECORD_HANGUP_ON_ERROR", "false");
	switch_channel_set_variable(channel, "RECORD_TOGGLE_ON_REPEAT", "");
	switch_channel_set_variable(channel, "RECORD_CHECK_BRIDGE", "");
	switch_channel_set_variable(channel, "RECORD_MIN_SEC", "0");
	switch_channel_set_variable(channel, "RECORD_STEREO", "");
	switch_channel_set_variable(channel, "RECORD_READ_ONLY", "");
	switch_channel_set_variable(channel, "RECORD_WRITE_ONLY", "");
	switch_channel_set_variable(channel, "RECORD_APPEND", "");
	switch_channel_set_variable(channel, "RECORD_WRITE_OVER", "true");
	switch_channel_set_variable(channel, "RECORD_ANSWER_REQ", "");
	switch_channel_set_variable(channel, "RECORD_SILENCE_THRESHOLD", "200");
	if (record_component->initial_timeout > 0) {
		switch_channel_set_variable_printf(channel, "RECORD_INITIAL_TIMEOUT_MS", "%i", record_component->initial_timeout);
	} else {
		switch_channel_set_variable(channel, "RECORD_INITIAL_TIMEOUT_MS", "");
	}
	if (record_component->final_timeout > 0) {
		switch_channel_set_variable_printf(channel, "RECORD_FINAL_TIMEOUT_MS", "%i", record_component->final_timeout);
	} else {
		switch_channel_set_variable(channel, "RECORD_FINAL_TIMEOUT_MS", "");
	}
	/* allow dialplan override for these variables */
	//switch_channel_set_variable(channel, "RECORD_PRE_BUFFER_FRAMES", "");
	//switch_channel_set_variable(channel, "record_sample_rate", "");
	//switch_channel_set_variable(channel, "enable_file_write_buffering", "");

	/* max duration attribute is in milliseconds- convert to seconds */
	if (record_component->max_duration > 0) {
		max_duration_sec = ceil((double)(record_component->max_duration - record_component->duration_ms) / 1000.0);
	}

	if (!strcmp(record_component->direction, "duplex")) {
		if (!record_component->mix) {
			/* STEREO */
			switch_channel_set_variable(channel, "RECORD_STEREO", "true");
		} /* else MONO (default) */
	} else if (!strcmp(record_component->direction, "send")) {
		/* record audio sent from the caller */
		switch_channel_set_variable(channel, "RECORD_READ_ONLY", "true");
	} else if (!strcmp(record_component->direction, "recv")) {
		/* record audio received by the caller */
		switch_channel_set_variable(channel, "RECORD_WRITE_ONLY", "true");
	};

	if (record_component->start_beep) {
		switch_ivr_displace_session(session, RECORD_BEEP, 0, "");
		record_component->start_time = switch_micro_time_now();
	}

	if (switch_ivr_record_session(session, (char *)RAYO_ID(component), max_duration_sec, NULL) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Recording started: file = %s\n", RAYO_ID(component));
		return 1;
	}

	return 0;
}

/**
 * Start execution of call record component
 */
static iks *start_call_record_component(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *iq = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	struct rayo_component *component = NULL;
	iks *record = iks_find(iq, "record");

	component = record_component_create(call, RAT_CALL_COMPONENT, iks_find_attrib(iq, "from"), record);
	if (!component) {
		return iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
	}

	if (start_call_record(session, component)) {
		rayo_component_send_start(component, iq);
	} else {
		RAYO_UNLOCK(component);
		RAYO_DESTROY(component);
		return iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
	}

	return NULL;
}

/**
 * Stop execution of record component
 */
static iks *stop_call_record_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	switch_core_session_t *session = switch_core_session_locate(RAYO_COMPONENT(component)->parent->id);
	if (session) {
		RECORD_COMPONENT(component)->stop = 1;
		switch_ivr_stop_record_session(session, RAYO_ID(component));
		switch_core_session_rwunlock(session);
	}
	return iks_new_iq_result(iq);
}

/**
 * Pause execution of record component
 */
static iks *pause_record_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	struct record_component *record = RECORD_COMPONENT(component);
	switch_stream_handle_t stream = { 0 };
	char *command = switch_mprintf("%s pause", record->local_file_path);
	SWITCH_STANDARD_STREAM(stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s pausing\n", RAYO_ID(component));
	if (record->start_time) {
		record->duration_ms += (switch_micro_time_now() - record->start_time) / 1000;
		record->start_time = 0;
	}
	switch_api_execute("fileman", command, NULL, &stream);
	switch_safe_free(stream.data);
	switch_safe_free(command);

	return iks_new_iq_result(iq);
}

/**
 * Resume execution of record component
 */
static iks *resume_record_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	struct record_component *record = RECORD_COMPONENT(component);
	switch_stream_handle_t stream = { 0 };
	char *command = switch_mprintf("%s resume", record->local_file_path);
	SWITCH_STANDARD_STREAM(stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s resuming\n", RAYO_ID(component));
	if (!record->start_time) {
		record->start_time = switch_micro_time_now();
	}
	switch_api_execute("fileman", command, NULL, &stream);
	switch_safe_free(stream.data);
	switch_safe_free(command);

	return iks_new_iq_result(iq);
}

/**
 * Handle conference events from FreeSWITCH.
 * @param event received from FreeSWITCH core.  It will be destroyed by the core after this function returns.
 */
static void on_mixer_record_event(switch_event_t *event)
{
	const char *file_path = switch_event_get_header(event, "Path");
	const char *action = switch_event_get_header(event, "Action");
	struct rayo_component *component = RAYO_COMPONENT_LOCATE(file_path);

	if (component) {
		struct record_component *record = RECORD_COMPONENT(component);
		if (!strcmp("stop-recording", action)) {
			record->duration_ms += (switch_micro_time_now() - record->start_time) / 1000;
			if (record->stop) {
				complete_record(component, COMPONENT_COMPLETE_STOP);
			} else {
				/* TODO assume final timeout, for now */
				complete_record(component, RECORD_COMPLETE_FINAL_TIMEOUT);
			}
		}
	}
}

/**
 * Start recording mixer
 * @param record the record component
 */
static int start_mixer_record(struct rayo_component *component)
{
	switch_stream_handle_t stream = { 0 };
	char *args;
	SWITCH_STANDARD_STREAM(stream);

	args = switch_mprintf("%s recording start %s", component->parent->id, RAYO_ID(component));
	switch_api_execute("conference", args, NULL, &stream);
	switch_safe_free(args);
	switch_safe_free(stream.data);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Recording started: file = %s\n", RAYO_ID(component));
	return 1;
}

/**
 * Start execution of mixer record component
 */
static iks *start_mixer_record_component(struct rayo_actor *mixer, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	struct rayo_component *component = NULL;
	iks *record = iks_find(iq, "record");

	component = record_component_create(mixer, RAT_MIXER_COMPONENT, iks_find_attrib(iq, "from"), record);
	if (!component) {
		return iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
	}

	/* mixer doesn't allow "send" */
	if (!strcmp("send", iks_find_attrib_soft(record, "direction"))) {
		RAYO_UNLOCK(component);
		RAYO_DESTROY(component);
		return iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
	}

	if (start_mixer_record(component)) {
		rayo_component_send_start(component, iq);
	} else {
		RAYO_UNLOCK(component);
		RAYO_DESTROY(component);
		return iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
	}

	return NULL;
}

/**
 * Stop execution of record component
 */
static iks *stop_mixer_record_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	char *args;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);

	RECORD_COMPONENT(component)->stop = 1;
	args = switch_mprintf("%s recording stop %s", RAYO_COMPONENT(component)->parent->id, RAYO_ID(component));
	switch_api_execute("conference", args, NULL, &stream);
	switch_safe_free(args);
	switch_safe_free(stream.data);

	return iks_new_iq_result(iq);
}

/**
 * Process module XML configuration
 * @param pool memory pool to allocate from
 * @param config_file to use
 * @return SWITCH_STATUS_SUCCESS on successful configuration
 */
static switch_status_t do_config(switch_memory_pool_t *pool, const char *config_file)
{
	switch_xml_t cfg, xml;

	/* set defaults */
	globals.record_file_prefix = switch_core_sprintf(pool, "%s%s", SWITCH_GLOBAL_dirs.recordings_dir, SWITCH_PATH_SEPARATOR);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Configuring module\n");
	if (!(xml = switch_xml_open_cfg(config_file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", config_file);
		return SWITCH_STATUS_TERM;
	}

	/* get params */
	{
		switch_xml_t settings = switch_xml_child(cfg, "record");
		if (settings) {
			switch_xml_t param;
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				const char *var = switch_xml_attr_soft(param, "name");
				const char *val = switch_xml_attr_soft(param, "value");
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "param: %s = %s\n", var, val);
				if (!strcasecmp(var, "record-file-prefix")) {
					if (!zstr(val)) {
						globals.record_file_prefix = switch_core_strdup(pool, val);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported param: %s\n", var);
				}
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Initialize record component
 * @param module_interface
 * @param pool memory pool to allocate from
 * @param config_file to use
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_record_component_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file)
{
	if (do_config(pool, config_file) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	switch_event_bind("rayo_record_component", SWITCH_EVENT_RECORD_STOP, NULL, on_call_record_stop_event, NULL);
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_RECORD_NS":record", start_call_record_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "record", "set:"RAYO_RECORD_NS":pause", pause_record_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "record", "set:"RAYO_RECORD_NS":resume", resume_record_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "record", "set:"RAYO_EXT_NS":stop", stop_call_record_component);

	switch_event_bind("rayo_record_component", SWITCH_EVENT_CUSTOM, "conference::maintenance", on_mixer_record_event, NULL);
	rayo_actor_command_handler_add(RAT_MIXER, "", "set:"RAYO_RECORD_NS":record", start_mixer_record_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "record", "set:"RAYO_RECORD_NS":pause", pause_record_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "record", "set:"RAYO_RECORD_NS":resume", resume_record_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "record", "set:"RAYO_EXT_NS":stop", stop_mixer_record_component);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Shutdown record component
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_record_component_shutdown(void)
{
	switch_event_unbind_callback(on_call_record_stop_event);
	switch_event_unbind_callback(on_mixer_record_event);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */

