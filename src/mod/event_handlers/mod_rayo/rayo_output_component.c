/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2016, Grasshopper
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
 * output_component.c -- Rayo output component implementation
 *
 */
#include "rayo_components.h"
#include "rayo_elements.h"

/**
 * An output component
 */
struct output_component {
	/** component base class */
	struct rayo_component base;
	/** document to play */
	iks *document;
	/** where to start playing in document */
	int start_offset_ms;
	/** maximum time to play */
	int max_time_ms;
	/** silence between repeats */
	int repeat_interval_ms;
	/** number of times to repeat */
	int repeat_times;
	/** true if started paused */
	switch_bool_t start_paused;
	/** true if stopped */
	int stop;
	/** output renderer to use */
	const char *renderer;
	/** optional headers to pass to renderer */
	const char *headers;
};

#define OUTPUT_FINISH "finish", RAYO_OUTPUT_COMPLETE_NS
#define OUTPUT_MAX_TIME "max-time", RAYO_OUTPUT_COMPLETE_NS

#define OUTPUT_COMPONENT(x) ((struct output_component *)x)

/**
 * Create new output component
 */
static struct rayo_component *create_output_component(struct rayo_actor *actor, const char *type, iks *output, const char *client_jid)
{
	switch_memory_pool_t *pool;
	struct output_component *output_component = NULL;

	switch_core_new_memory_pool(&pool);
	output_component = switch_core_alloc(pool, sizeof(*output_component));
	output_component = OUTPUT_COMPONENT(rayo_component_init((struct rayo_component *)output_component, pool, type, "output", NULL, actor, client_jid));
	if (output_component) {
		output_component->document = iks_copy(output);
		output_component->start_offset_ms = iks_find_int_attrib(output, "start-offset");
		output_component->repeat_interval_ms = iks_find_int_attrib(output, "repeat-interval");
		output_component->repeat_times = iks_find_int_attrib(output, "repeat-times");
		output_component->max_time_ms = iks_find_int_attrib(output, "max-time");
		output_component->start_paused = iks_find_bool_attrib(output, "start-paused");
		output_component->renderer = switch_core_strdup(RAYO_POOL(output_component), iks_find_attrib_soft(output, "renderer"));
		/* get custom headers */
		{
			switch_stream_handle_t headers = { 0 };
			iks *header = NULL;
			int first = 1;
			SWITCH_STANDARD_STREAM(headers);
			for (header = iks_find(output, "header"); header; header = iks_next_tag(header)) {
				if (!strcmp("header", iks_name(header))) {
					const char *name = iks_find_attrib_soft(header, "name");
					const char *value = iks_find_attrib_soft(header, "value");
					if (!zstr(name) && !zstr(value)) {
						headers.write_function(&headers, "%s%s=%s", first ? "{" : ",", name, value);
						first = 0;
					}
				}
			}
			if (headers.data) {
				headers.write_function(&headers, "}");
				output_component->headers = switch_core_strdup(RAYO_POOL(output_component), (char *)headers.data);
				free(headers.data);
			}
		}
	} else {
		switch_core_destroy_memory_pool(&pool);
	}

	return RAYO_COMPONENT(output_component);
}

/**
 * Start execution of call output component
 * @param component to start
 * @param session the session to output to
 * @param output the output request
 * @param iq the original request
 */
static iks *start_call_output(struct rayo_component *component, switch_core_session_t *session, iks *output, iks *iq)
{
	switch_stream_handle_t stream = { 0 };

	/* acknowledge command */
	rayo_component_send_start(component, iq);

	/* build playback command */
	SWITCH_STANDARD_STREAM(stream);
	stream.write_function(&stream, "{id=%s,session=%s,pause=%s",
		RAYO_JID(component), switch_core_session_get_uuid(session),
		OUTPUT_COMPONENT(component)->start_paused ? "true" : "false");
	if (OUTPUT_COMPONENT(component)->max_time_ms > 0) {
		stream.write_function(&stream, ",timeout=%i", OUTPUT_COMPONENT(component)->max_time_ms);
	}
	if (OUTPUT_COMPONENT(component)->start_offset_ms > 0) {
		stream.write_function(&stream, ",start_offset_ms=%i", OUTPUT_COMPONENT(component)->start_offset_ms);
	}
	stream.write_function(&stream, "}fileman://rayo://%s", RAYO_JID(component));

	if (switch_ivr_displace_session(session, stream.data, 0, "m") == SWITCH_STATUS_SUCCESS) {
		RAYO_RELEASE(component);
	} else {
		if (component->complete) {
			/* component is already destroyed */
			RAYO_RELEASE(component);
		} else {
			/* need to destroy component */
			if (OUTPUT_COMPONENT(component)->document) {
				iks_delete(OUTPUT_COMPONENT(component)->document);
			}
			if (switch_channel_get_state(switch_core_session_get_channel(session)) >= CS_HANGUP) {
				rayo_component_send_complete(component, COMPONENT_COMPLETE_HANGUP);
			} else {
				rayo_component_send_complete(component, COMPONENT_COMPLETE_ERROR);
			}
		}
	}
	switch_safe_free(stream.data);
	return NULL;
}

/**
 * Start execution of call output component
 */
static iks *start_call_output_component(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *iq = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	struct rayo_component *output_component = NULL;
	iks *output = iks_find(iq, "output");
	iks *document = NULL;

	/* validate output attributes */
	if (!VALIDATE_RAYO_OUTPUT(output)) {
		return iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
	}

	/* check if <document> exists */
	document = iks_find(output, "document");
	if (!document) {
		return iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
	}

	output_component = create_output_component(call, RAT_CALL_COMPONENT, output, iks_find_attrib(iq, "from"));
	if (!output_component) {
		return iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "Failed to create output entity");
	}
	return start_call_output(output_component, session, output, iq);
}

/**
 * Start execution of mixer output component
 */
static iks *start_mixer_output_component(struct rayo_actor *mixer, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	struct rayo_component *component = NULL;
	iks *output = iks_find(iq, "output");
	iks *document = NULL;
	switch_stream_handle_t stream = { 0 };

	/* validate output attributes */
	if (!VALIDATE_RAYO_OUTPUT(output)) {
		return iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
	}

	/* check if <document> exists */
	document = iks_find(output, "document");
	if (!document) {
		return iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
	}

	component = create_output_component(mixer, RAT_MIXER_COMPONENT, output, iks_find_attrib(iq, "from"));
	if (!component) {
		return iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "Failed to create output entity");
	}

	/* build conference command */
	SWITCH_STANDARD_STREAM(stream);
	stream.write_function(&stream, "%s play ", rayo_mixer_get_name(RAYO_MIXER(mixer)), RAYO_ID(component));

	stream.write_function(&stream, "{id=%s,pause=%s",
		RAYO_JID(component),
		OUTPUT_COMPONENT(component)->start_paused ? "true" : "false");
	if (OUTPUT_COMPONENT(component)->max_time_ms > 0) {
		stream.write_function(&stream, ",timeout=%i", OUTPUT_COMPONENT(component)->max_time_ms);
	}
	if (OUTPUT_COMPONENT(component)->start_offset_ms > 0) {
		stream.write_function(&stream, ",start_offset_ms=%i", OUTPUT_COMPONENT(component)->start_offset_ms);
	}
	stream.write_function(&stream, "}fileman://rayo://%s", RAYO_JID(component));

	/* acknowledge command */
	rayo_component_send_start(component, iq);

	rayo_component_api_execute_async(component, "conference", stream.data);

	switch_safe_free(stream.data);
	RAYO_RELEASE(component);

	return NULL;
}

/**
 * Stop execution of output component
 */
static iks *stop_output_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *result = NULL;
	switch_stream_handle_t stream = { 0 };
	char *command = switch_mprintf("%s stop", RAYO_JID(component));
	SWITCH_STANDARD_STREAM(stream);
	OUTPUT_COMPONENT(component)->stop = 1;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s stopping\n", RAYO_JID(component));
	switch_api_execute("fileman", command, NULL, &stream);
	if (!zstr((char *)stream.data) && !strncmp((char *)stream.data, "+OK", 3)) {
		result = iks_new_iq_result(iq);
	} else if (!zstr((char *)stream.data)) {
		result = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "%s", (char *)stream.data);
	} else {
		result = iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
	}
	switch_safe_free(stream.data);
	switch_safe_free(command);
	return result;
}

/**
 * Pause execution of output component
 */
static iks *pause_output_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *result = NULL;
	switch_stream_handle_t stream = { 0 };
	char *command = switch_mprintf("%s pause", RAYO_JID(component));
	SWITCH_STANDARD_STREAM(stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s pausing\n", RAYO_JID(component));
	switch_api_execute("fileman", command, NULL, &stream);
	if (!zstr((char *)stream.data) && !strncmp((char *)stream.data, "+OK", 3)) {
		result = iks_new_iq_result(iq);
	} else if (!zstr((char *)stream.data)) {
		result = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "%s", (char *)stream.data);
	} else {
		result = iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
	}
	switch_safe_free(stream.data);
	switch_safe_free(command);
	return result;
}

/**
 * Resume execution of output component
 */
static iks *resume_output_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *result = NULL;
	switch_stream_handle_t stream = { 0 };
	char *command = switch_mprintf("%s resume", RAYO_JID(component));
	SWITCH_STANDARD_STREAM(stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s resuming\n", RAYO_JID(component));
	switch_api_execute("fileman", command, NULL, &stream);
	if (!zstr((char *)stream.data) && !strncmp((char *)stream.data, "+OK", 3)) {
		result = iks_new_iq_result(iq);
	} else if (!zstr((char *)stream.data)) {
		result = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "%s", (char *)stream.data);
	} else {
		result = iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
	}
	switch_safe_free(stream.data);
	switch_safe_free(command);
	return result;
}

/**
 * Speed up execution of output component
 */
static iks *speed_up_output_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *result = NULL;
	switch_stream_handle_t stream = { 0 };
	char *command = switch_mprintf("%s speed:+", RAYO_JID(component));
	SWITCH_STANDARD_STREAM(stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s speeding up\n", RAYO_JID(component));
	switch_api_execute("fileman", command, NULL, &stream);
	if (!zstr((char *)stream.data) && !strncmp((char *)stream.data, "+OK", 3)) {
		result = iks_new_iq_result(iq);
	} else if (!zstr((char *)stream.data)) {
		result = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "%s", (char *)stream.data);
	} else {
		result = iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
	}
	switch_safe_free(stream.data);
	switch_safe_free(command);
	return result;
}

/**
 * Slow down execution of output component
 */
static iks *speed_down_output_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *result = NULL;
	switch_stream_handle_t stream = { 0 };
	char *command = switch_mprintf("%s speed:-", RAYO_JID(component));
	SWITCH_STANDARD_STREAM(stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s slowing down\n", RAYO_JID(component));
	switch_api_execute("fileman", command, NULL, &stream);
	if (!zstr((char *)stream.data) && !strncmp((char *)stream.data, "+OK", 3)) {
		result = iks_new_iq_result(iq);
	} else if (!zstr((char *)stream.data)) {
		result = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "%s", (char *)stream.data);
	} else {
		result = iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
	}
	switch_safe_free(stream.data);
	switch_safe_free(command);
	return result;
}

/**
 * Increase volume of output component
 */
static iks *volume_up_output_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *result = NULL;
	switch_stream_handle_t stream = { 0 };
	char *command = switch_mprintf("%s volume:+", RAYO_JID(component));
	SWITCH_STANDARD_STREAM(stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s increasing volume\n", RAYO_JID(component));
	switch_api_execute("fileman", command, NULL, &stream);
	if (!zstr((char *)stream.data) && !strncmp((char *)stream.data, "+OK", 3)) {
		result = iks_new_iq_result(iq);
	} else if (!zstr((char *)stream.data)) {
		result = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "%s", (char *)stream.data);
	} else {
		result = iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
	}
	switch_safe_free(stream.data);
	switch_safe_free(command);
	return result;
}

/**
 * Lower volume of output component
 */
static iks *volume_down_output_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *result = NULL;
	switch_stream_handle_t stream = { 0 };
	char *command = switch_mprintf("%s volume:-", RAYO_JID(component));
	SWITCH_STANDARD_STREAM(stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s lowering volume\n", RAYO_JID(component));
	switch_api_execute("fileman", command, NULL, &stream);
	if (!zstr((char *)stream.data) && !strncmp((char *)stream.data, "+OK", 3)) {
		result = iks_new_iq_result(iq);
	} else if (!zstr((char *)stream.data)) {
		result = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "%s", (char *)stream.data);
	} else {
		result = iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
	}
	switch_safe_free(stream.data);
	switch_safe_free(command);
	return result;
}

/**
 * Seek output component
 */
static iks *seek_output_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *seek = iks_find(iq, "seek");

	if (VALIDATE_RAYO_OUTPUT_SEEK(seek)) {
		iks *result = NULL;
		int is_forward = !strcmp("forward", iks_find_attrib(seek, "direction"));
		int amount_ms = iks_find_int_attrib(seek, "amount");
		char *command = switch_mprintf("%s seek:%s%i", RAYO_JID(component),
			is_forward ? "+" : "-", amount_ms);
		switch_stream_handle_t stream = { 0 };
		SWITCH_STANDARD_STREAM(stream);

		switch_api_execute("fileman", command, NULL, &stream);
		if (!zstr((char *)stream.data) && !strncmp((char *)stream.data, "+OK", 3)) {
			result = iks_new_iq_result(iq);
		} else if (!zstr((char *)stream.data)) {
			result = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "%s", (char *)stream.data);
		} else {
			result = iks_new_error(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR);
		}
		switch_safe_free(stream.data);
		switch_safe_free(command);

		return result;
	}
	return iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
}

/**
 * Rayo document playback state
 */
struct rayo_file_context {
	/** handle to current file */
	switch_file_handle_t fh;
	/** current document being played */
	iks *cur_doc;
	/** current file string being played */
	char *ssml;
	/** The component */
	struct rayo_component *component;
	/** number of times played */
	int play_count;
	/** have any files successfully opened? */
	int could_open;
};

/**
 * open next file for reading
 * @param handle the file handle
 */
static switch_status_t next_file(switch_file_handle_t *handle)
{
	int loops = 0;
	struct rayo_file_context *context = handle->private_info;
	struct output_component *output = context->component ? OUTPUT_COMPONENT(context->component) : NULL;

  top:

	if (switch_test_flag((&context->fh), SWITCH_FILE_OPEN)) {
		switch_core_file_close(&context->fh);
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		/* unsupported */
		return SWITCH_STATUS_FALSE;
	}

	if (!context->cur_doc) {
		context->cur_doc = iks_find(output->document, "document");
		if (!context->cur_doc) {
			iks_delete(output->document);
			output->document = NULL;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing <document>\n");
			return SWITCH_STATUS_FALSE;
		}
	} else {
		context->cur_doc = iks_next_tag(context->cur_doc);
	}

	/* done? */
	if (!context->cur_doc) {
		if (context->could_open && ++loops < 2 && (output->repeat_times == 0 || ++context->play_count < output->repeat_times)) {
			/* repeat all document(s) */
			if (!output->repeat_interval_ms) {
				goto top;
			}
		} else {
			/* no more files to play */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Done playing\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	if (!context->cur_doc) {
		/* play silence between repeats */
		switch_safe_free(context->ssml);
		context->ssml = switch_mprintf("silence_stream://%i", output->repeat_interval_ms);
	} else {
		/* play next document */
		iks *speak = NULL;

		switch_safe_free(context->ssml);
		context->ssml = NULL;
 		speak = iks_find(context->cur_doc, "speak");
		if (speak) {
			/* <speak> is child node */
			char *ssml_str = iks_string(NULL, speak);
			if (zstr(output->renderer)) {
				/* FS must parse the SSML */
				context->ssml = switch_mprintf("ssml://%s", ssml_str);
			} else {
				/* renderer will parse the SSML */
				if (!zstr(output->headers) && !strncmp("unimrcp", output->renderer, 7)) {
					/* pass MRCP headers */
					context->ssml = switch_mprintf("tts://%s||%s%s", output->renderer, output->headers, ssml_str);
				} else {
					context->ssml = switch_mprintf("tts://%s||%s", output->renderer, ssml_str);
				}
			}
			iks_free(ssml_str);
		} else if (iks_has_children(context->cur_doc)) {
			/* check if <speak> is in CDATA */
			const char *ssml_str = NULL;
			iks *ssml = iks_child(context->cur_doc);
			if (ssml && iks_type(ssml) == IKS_CDATA) {
				ssml_str = iks_cdata(ssml);
			}
			if (zstr(ssml_str)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing <document> CDATA\n");
				return SWITCH_STATUS_FALSE;
			}
			if (zstr(output->renderer)) {
				/* FS must parse the SSML */
				context->ssml = switch_mprintf("ssml://%s", ssml_str);
			} else {
				/* renderer will parse the SSML */
				if (!zstr(output->headers) && !strncmp("unimrcp", output->renderer, 7)) {
					/* pass MRCP headers */
					context->ssml = switch_mprintf("tts://%s||%s%s", output->renderer, output->headers, ssml_str);
				} else {
					context->ssml = switch_mprintf("tts://%s||%s", output->renderer, ssml_str);
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing <speak>\n");
			return SWITCH_STATUS_FALSE;
		}
	}
	if (switch_core_file_open(&context->fh, context->ssml, handle->channels, handle->samplerate, handle->flags, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Failed to open %s\n", context->ssml);
		goto top;
	} else {
		context->could_open = 1;
	}

	handle->samples = context->fh.samples;
	handle->format = context->fh.format;
	handle->sections = context->fh.sections;
	handle->seekable = context->fh.seekable;
	handle->speed = context->fh.speed;
	handle->vol = context->fh.vol;
	handle->offset_pos = context->fh.offset_pos;
	handle->interval = context->fh.interval;

	if (switch_test_flag((&context->fh), SWITCH_FILE_NATIVE)) {
		switch_set_flag(handle, SWITCH_FILE_NATIVE);
	} else {
		switch_clear_flag(handle, SWITCH_FILE_NATIVE);
	}

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Transforms Rayo document into sub-format and opens file_string.
 * @param handle
 * @param path the inline Rayo document
 * @return SWITCH_STATUS_SUCCESS if opened
 */
static switch_status_t rayo_file_open(switch_file_handle_t *handle, const char *path)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	struct rayo_file_context *context = switch_core_alloc(handle->memory_pool, sizeof(*context));

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got path %s\n", path);

	context->component = RAYO_COMPONENT_LOCATE(path);

	if (context->component) {
		handle->private_info = context;
		context->cur_doc = NULL;
		context->play_count = 0;
		context->could_open = 0;
		status = next_file(handle);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "File error! %s\n", path);
		return SWITCH_STATUS_FALSE;
	}

	if (status != SWITCH_STATUS_SUCCESS && context->component) {
		/* complete error event will be sent by calling thread */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Status = %i\n", status);
		RAYO_RELEASE(context->component);
	}

	return status;
}

/**
 * Close SSML document.
 * @param handle
 * @return SWITCH_STATUS_SUCCESS
 */
static switch_status_t rayo_file_close(switch_file_handle_t *handle)
{
	struct rayo_file_context *context = (struct rayo_file_context *)handle->private_info;

	if (context && context->component) {
		struct output_component *output = OUTPUT_COMPONENT(context->component);

		/* send completion and destroy */
		if (output->stop) {
			rayo_component_send_complete(context->component, COMPONENT_COMPLETE_STOP);
		} else {
			if (!strcmp(RAYO_ACTOR(context->component)->type, RAT_CALL_COMPONENT)) {
				/* call output... check for hangup */
				switch_core_session_t *session = switch_core_session_locate(RAYO_ACTOR(context->component)->parent->id);
				if (session) {
					if (switch_channel_get_state(switch_core_session_get_channel(session)) >= CS_HANGUP) {
						rayo_component_send_complete(context->component, COMPONENT_COMPLETE_HANGUP);
					} else {
						rayo_component_send_complete(context->component, OUTPUT_FINISH);
					}
					switch_core_session_rwunlock(session);
				} else {
					/* session is gone */
					rayo_component_send_complete(context->component, COMPONENT_COMPLETE_HANGUP);
				}
			} else {
				/* mixer output... finished */
				rayo_component_send_complete(context->component, OUTPUT_FINISH);
			}
		}
		/* TODO timed out */

		/* cleanup internals */
		switch_safe_free(context->ssml);
		context->ssml = NULL;
		if (output->document) {
			iks_delete(output->document);
			output->document = NULL;
		}

		/* close SSML file */
		if (switch_test_flag((&context->fh), SWITCH_FILE_OPEN)) {
			return switch_core_file_close(&context->fh);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Read from SSML document
 * @param handle
 * @param data
 * @param len
 * @return
 */
static switch_status_t rayo_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_status_t status;
	struct rayo_file_context *context = (struct rayo_file_context *)handle->private_info;
	size_t llen = *len;

	if (OUTPUT_COMPONENT(context->component)->stop) {
		return SWITCH_STATUS_FALSE;
	} else {
		status = switch_core_file_read(&context->fh, data, len);
		if (status != SWITCH_STATUS_SUCCESS) {
			if ((status = next_file(handle)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			*len = llen;
			status = switch_core_file_read(&context->fh, data, len);
		}
	}

	return status;
}

/**
 * Seek file
 */
static switch_status_t rayo_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	struct rayo_file_context *context = handle->private_info;

	if (samples == 0 && whence == SWITCH_SEEK_SET) {
		/* restart from beginning */
		context->cur_doc = NULL;
		context->play_count = 0;
		return next_file(handle);
	}

	if (!handle->seekable) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File is not seekable\n");
		return SWITCH_STATUS_NOTIMPL;
	}

	return switch_core_file_seek(&context->fh, cur_sample, samples, whence);
}

/**
 * Manages access to fileman controls
 */
struct {
	/** synchronizes access to fileman hash */
	switch_mutex_t *mutex;
	/** fileman mapped by id */
	switch_hash_t *hash;
} fileman_globals;

#define FILE_STARTBYTES 1024 * 32
#define FILE_BLOCKSIZE 1024 * 8
#define FILE_BUFSIZE 1024 * 64

/**
 * Fileman playback state
 */
struct fileman_file_context {
	/** handle to current file */
	switch_file_handle_t fh;
	/** file buffer */
	int16_t *abuf;
	/** end of file */
	int eof;
	/** maximum size of a packet in 2-byte samples */
	switch_size_t max_frame_len;
	/** optional session UUID */
	const char *uuid;
	/** fileman control ID */
	const char *id;
	/** done flag */
	int done;
};

/**
 * Wraps file with interface that can be controlled by fileman flags
 * @param handle
 * @param path the file to play
 * @return SWITCH_STATUS_SUCCESS if opened
 */
static switch_status_t fileman_file_open(switch_file_handle_t *handle, const char *path)
{
	int start_offset_ms = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;
	struct fileman_file_context *context = switch_core_alloc(handle->memory_pool, sizeof(*context));
	handle->private_info = context;

	if (handle->params) {
		const char *id = switch_event_get_header(handle->params, "id");
		const char *uuid = switch_event_get_header(handle->params, "session");
		const char *start_offset_ms_str = switch_event_get_header(handle->params, "start_offset_ms");
		if (!zstr(id)) {
			context->id = switch_core_strdup(handle->memory_pool, id);
		}
		if (!zstr(uuid)) {
			context->uuid = switch_core_strdup(handle->memory_pool, uuid);
		}
		if (!zstr(start_offset_ms_str) && switch_is_number(start_offset_ms_str)) {
			start_offset_ms = atoi(start_offset_ms_str);
			if (start_offset_ms < 0) {
				start_offset_ms = 0;
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Got path %s\n", path);

	if ((status = switch_core_file_open(&context->fh, path, handle->channels, handle->samplerate, handle->flags, NULL)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/* set up handle for external control */
	if (!context->id) {
		/* use filename as ID */
		context->id = switch_core_strdup(handle->memory_pool, path);
	}
	switch_mutex_lock(fileman_globals.mutex);
	if (!switch_core_hash_find(fileman_globals.hash, context->id)) {
		switch_core_hash_insert(fileman_globals.hash, context->id, handle);
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_WARNING, "Duplicate fileman ID: %s\n", context->id);
		return SWITCH_STATUS_FALSE;
	}
	switch_mutex_unlock(fileman_globals.mutex);

	context->max_frame_len = (handle->samplerate / 1000 * SWITCH_MAX_INTERVAL);
	switch_zmalloc(context->abuf, FILE_STARTBYTES * sizeof(*context->abuf));

	if (!context->fh.audio_buffer) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Create audio buffer\n");
		switch_buffer_create_dynamic(&context->fh.audio_buffer, FILE_BLOCKSIZE, FILE_BUFSIZE, 0);
		switch_assert(context->fh.audio_buffer);
	}

	handle->samples = context->fh.samples;
	handle->format = context->fh.format;
	handle->sections = context->fh.sections;
	handle->seekable = context->fh.seekable;
	handle->speed = context->fh.speed;
	handle->vol = context->fh.vol;
	handle->offset_pos = context->fh.offset_pos;
	handle->interval = context->fh.interval;

	if (switch_test_flag((&context->fh), SWITCH_FILE_NATIVE)) {
		switch_set_flag(handle, SWITCH_FILE_NATIVE);
	} else {
		switch_clear_flag(handle, SWITCH_FILE_NATIVE);
	}

	if (handle->params && switch_true(switch_event_get_header(handle->params, "pause"))) {
		switch_set_flag(handle, SWITCH_FILE_PAUSE);
	}

	if (handle->seekable && start_offset_ms) {
		unsigned int pos = 0;
		int32_t target = start_offset_ms * (handle->samplerate / 1000);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "seek to position %d\n", target);
		switch_core_file_seek(&context->fh, &pos, target, SEEK_SET);
	}

	return status;
}

/**
 * Close file.
 * @param handle
 * @return SWITCH_STATUS_SUCCESS
 */
static switch_status_t fileman_file_close(switch_file_handle_t *handle)
{
	struct fileman_file_context *context = (struct fileman_file_context *)handle->private_info;
	switch_file_handle_t *fh = &context->fh;

	if (context->id) {
		switch_mutex_lock(fileman_globals.mutex);
		switch_core_hash_delete(fileman_globals.hash, context->id);
		switch_mutex_unlock(fileman_globals.mutex);
	}

	if (switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		free(context->abuf);

		if (fh->audio_buffer) {
			switch_buffer_destroy(&fh->audio_buffer);
		}

		if (fh->sp_audio_buffer) {
			switch_buffer_destroy(&fh->sp_audio_buffer);
		}
		return switch_core_file_close(fh);
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Write to file
 * @param handle
 * @param data
 * @param len
 * @return
 */
static switch_status_t fileman_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	struct fileman_file_context *context = (struct fileman_file_context *)handle->private_info;
	switch_file_handle_t *fh = &context->fh;
	if (!switch_test_flag(handle, SWITCH_FILE_PAUSE)) {
		return switch_core_file_write(fh, data, len);
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Read from file
 * @param handle
 * @param data
 * @param len
 * @return
 */
static switch_status_t fileman_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	struct fileman_file_context *context = (struct fileman_file_context *)handle->private_info;
	switch_file_handle_t *fh = &context->fh;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_size_t o_len = 0;

	/* anything called "_len" is measured in 2-byte samples */

	if (switch_test_flag(fh, SWITCH_FILE_NATIVE)) {
		return switch_core_file_read(fh, data, len);
	}

	//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "len = %"SWITCH_SIZE_T_FMT"\n", *len);
	if (*len > context->max_frame_len) {
		*len = context->max_frame_len;
	}

	for (;;) {
		int do_speed = 1;
		size_t read_bytes = 0;

		if (context->done) {
			/* done with this file */
			status = SWITCH_STATUS_FALSE;
			goto done;
		} else if (switch_test_flag(handle, SWITCH_FILE_PAUSE)) {
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Read pause frame\n");
			memset(context->abuf, 255, *len * 2);
			do_speed = 0;
			o_len = *len;
		} else if (fh->sp_audio_buffer && (context->eof || (switch_buffer_inuse(fh->sp_audio_buffer) > (switch_size_t) (*len * 2)))) {
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Read speed frame\n");
			/* get next speed frame */
			if (!(read_bytes = switch_buffer_read(fh->sp_audio_buffer, context->abuf, *len * 2))) {
				/* This is the reverse of what happens in switch_ivr_play_file... i think that implementation is wrong */
				if (context->eof) {
					/* done with file */
					status = SWITCH_STATUS_FALSE;
					goto done;
				} else {
					/* try again to fetch frame */
					continue;
				}
			}

			/* pad short frame with silence */
			if (read_bytes < *len * 2) {
				//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Padding speed frame %"SWITCH_SIZE_T_FMT" bytes\n", (context->frame_len * 2) - read_bytes);
				memset(context->abuf + read_bytes, 255, (*len * 2) - read_bytes);
			}
			o_len = *len;
			do_speed = 0;
		} else if (fh->audio_buffer && (context->eof || (switch_buffer_inuse(fh->audio_buffer) > (switch_size_t) (*len * 2)))) {
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "(2) Read audio frame\n");
			/* get next file frame */
			if (!(read_bytes = switch_buffer_read(fh->audio_buffer, context->abuf, *len * 2))) {
				if (context->eof) {
					/* done with file */
					status = SWITCH_STATUS_FALSE;
					goto done;
				} else {
					/* try again to fetch frame */
					continue;
				}
			}
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "(2) Read audio frame %"SWITCH_SIZE_T_FMT" bytes\n", read_bytes);
			fh->offset_pos += read_bytes / 2;
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "(2) file pos = %i\n", fh->offset_pos);

			/* pad short frame with silence */
			if (read_bytes < (*len * 2)) {
				//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Padding audio frame %"SWITCH_SIZE_T_FMT" bytes\n", (context->frame_len * 2) - read_bytes);
				memset(context->abuf + read_bytes, 255, (*len * 2) - read_bytes);
			}

			o_len = *len;
		} else {
			if (context->eof) {
				/* done with file */
				status = SWITCH_STATUS_FALSE;
				goto done;
			}
			o_len = FILE_STARTBYTES / 2;
			if (switch_core_file_read(fh, context->abuf, &o_len) != SWITCH_STATUS_SUCCESS) {
				context->eof++;
				/* at end of file... need to clear buffers before giving up */
				continue;
			}
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Read file %"SWITCH_SIZE_T_FMT" bytes\n", o_len * 2);

			/* add file data to audio bufer */
			read_bytes = switch_buffer_write(fh->audio_buffer, context->abuf, o_len * 2);
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Write audio frame %"SWITCH_SIZE_T_FMT" bytes\n", read_bytes);

			read_bytes = switch_buffer_read(fh->audio_buffer, context->abuf, *len * 2);
			o_len = read_bytes / 2;
			fh->offset_pos += o_len;
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Read audio frame %"SWITCH_SIZE_T_FMT" bytes\n", read_bytes);
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "file pos = %i\n", fh->offset_pos);
		}

		if (o_len <= 0) {
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "o_len <= 0 (%"SWITCH_SIZE_T_FMT")\n", o_len);
			status = SWITCH_STATUS_FALSE;
			goto done;
		}

		/* limit speed... there is a .25 factor change in packet size relative to original packet size for each increment.
		   Too many increments and we cause badness when (factor * speed * o_len) > o_len */
		if (handle->speed > 2) {
			handle->speed = 2;
		} else if (handle->speed < -2) {
			handle->speed = -2;
		}

		if (switch_test_flag(fh, SWITCH_FILE_SEEK)) {
			/* file position has changed flush the buffer */
			switch_buffer_zero(fh->audio_buffer);
			switch_clear_flag(fh, SWITCH_FILE_SEEK);
		}

		/* generate speed frames */
		if (handle->speed && do_speed) {
			float factor = 0.25f * abs(handle->speed);
			switch_size_t new_len, supplement_len, step_len;
			short *bp = context->abuf;
			switch_size_t wrote_len = 0;
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Generate speed frame (%i)\n", handle->speed);

			supplement_len = (int) (factor * o_len);
			if (!supplement_len) {
				supplement_len = 1;
			}
			new_len = (handle->speed > 0) ? o_len - supplement_len : o_len + supplement_len;

			step_len = (handle->speed > 0) ? (new_len / supplement_len) : (o_len / supplement_len);

			if (!fh->sp_audio_buffer) {
				switch_buffer_create_dynamic(&fh->sp_audio_buffer, 1024, 1024, 0);
			}

			while ((wrote_len + step_len) < new_len) {
				switch_buffer_write(fh->sp_audio_buffer, bp, step_len * 2);
				wrote_len += step_len;
				bp += step_len;
				if (handle->speed > 0) {
					bp++;
				} else {
					float f;
					short s;
					f = (float) (*bp + *(bp + 1) + *(bp - 1));
					f /= 3;
					s = (short) f;
					switch_buffer_write(fh->sp_audio_buffer, &s, 2);
					wrote_len++;
				}
			}
			if (wrote_len < new_len) {
				switch_size_t r_len = new_len - wrote_len;
				switch_buffer_write(fh->sp_audio_buffer, bp, r_len * 2);
				wrote_len += r_len;
			}
			continue;
		}

		/* adjust volume on frame */
		if (handle->vol) {
			//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Adjust volume to = %i\n", handle->vol);
			switch_change_sln_volume(context->abuf, *len, handle->vol);
		}
		break;
	}

done:

	/* copy frame over to return to caller */
	memcpy(data, context->abuf, *len * 2);
	handle->offset_pos = context->fh.offset_pos;

	return status;
}

/**
 * Seek file
 */
static switch_status_t fileman_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	struct fileman_file_context *context = handle->private_info;

	if (!handle->seekable) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_WARNING, "File is not seekable\n");
		return SWITCH_STATUS_NOTIMPL;
	}
	return switch_core_file_seek(&context->fh, cur_sample, samples, whence);
}

/**
 * Process fileman command
 */
static switch_status_t fileman_process_cmd(const char *cmd, switch_file_handle_t *fhp)
{
	if (zstr(cmd)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (fhp) {
		struct fileman_file_context *context = (struct fileman_file_context *)fhp->private_info;
		if (!switch_test_flag(fhp, SWITCH_FILE_OPEN)) {
			return SWITCH_STATUS_FALSE;
		}

		if (!strncasecmp(cmd, "speed", 5)) {
			char *p;

			if ((p = strchr(cmd, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						if (*p == '+') {
							step = 1;
						} else {
							step = -1;
						}
					}
					fhp->speed += step;
				} else {
					int speed = atoi(p);
					fhp->speed = speed;
				}
				return SWITCH_STATUS_SUCCESS;
			}

			return SWITCH_STATUS_FALSE;

		} else if (!strncasecmp(cmd, "volume", 6)) {
			char *p;

			if ((p = strchr(cmd, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						if (*p == '+') {
							step = 1;
						} else {
							step = -1;
						}
					}
					fhp->vol += step;
				} else {
					int vol = atoi(p);
					fhp->vol = vol;
				}
				return SWITCH_STATUS_SUCCESS;
			}

			if (fhp->vol) {
				switch_normalize_volume(fhp->vol);
			}

			return SWITCH_STATUS_FALSE;
		} else if (!strcasecmp(cmd, "pause")) {
			switch_set_flag(fhp, SWITCH_FILE_PAUSE);
			return SWITCH_STATUS_SUCCESS;
		} else if (!strcasecmp(cmd, "resume")) {
			switch_clear_flag(fhp, SWITCH_FILE_PAUSE);
			return SWITCH_STATUS_SUCCESS;
		} else if (!strcasecmp(cmd, "stop")) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "Stopping file\n");
			context->done = 1;
			switch_set_flag(fhp, SWITCH_FILE_DONE);
			return SWITCH_STATUS_SUCCESS;
		} else if (!strcasecmp(cmd, "truncate")) {
			switch_core_file_truncate(fhp, 0);
		} else if (!strcasecmp(cmd, "restart")) {
			unsigned int pos = 0;
			fhp->speed = 0;
			switch_core_file_seek(fhp, &pos, 0, SEEK_SET);
			return SWITCH_STATUS_SUCCESS;
		} else if (!strncasecmp(cmd, "seek", 4)) {
			unsigned int samps = 0;
			unsigned int pos = 0;
			char *p;

			if ((p = strchr(cmd, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					int32_t target;
					if (!(step = atoi(p))) {
						if (*p == '+') {
							step = 1000;
						} else {
							step = -1000;
						}
					}

					samps = step * (fhp->samplerate / 1000);
					target = (int32_t)fhp->pos + samps;

					if (target < 0) {
						target = 0;
					}

					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "seek to position %d\n", target);
					switch_core_file_seek(fhp, &pos, target, SEEK_SET);

				} else {
					samps = switch_atoui(p) * (fhp->samplerate / 1000);
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->uuid), SWITCH_LOG_DEBUG, "seek to position %d\n", samps);
					switch_core_file_seek(fhp, &pos, samps, SEEK_SET);
				}
			}

			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (!strcmp(cmd, "true") || !strcmp(cmd, "undefined")) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

#define FILEMAN_SYNTAX "<id> <cmd>:<val>"
SWITCH_STANDARD_API(fileman_api)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argc >= 2 && !zstr(argv[0])) {
			char *id = argv[0];
			char *cmd = argv[1];
			switch_file_handle_t *fh = NULL;
			switch_mutex_lock(fileman_globals.mutex);
			fh = (switch_file_handle_t *)switch_core_hash_find(fileman_globals.hash, id);
			if (fh) {
				if (fileman_process_cmd(cmd, fh) == SWITCH_STATUS_SUCCESS) {
					stream->write_function(stream, "+OK\n");
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "fileman API failed for file %s\n", zstr(fh->file_path) ? "<null>" : fh->file_path);
					stream->write_function(stream, "-ERR API call failed");
				}
				switch_mutex_unlock(fileman_globals.mutex);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "fileman API failed for ID %s\n", zstr(id) ? "<null>" : id);
				switch_mutex_unlock(fileman_globals.mutex);
				stream->write_function(stream, "-ERR file handle not found\n");
			}
			goto done;
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", FILEMAN_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

static char *rayo_supported_formats[] = { "rayo", NULL };
static char *fileman_supported_formats[] = { "fileman", NULL };

/**
 * Initialize output component
 * @param module_interface
 * @param pool memory pool to allocate from
 * @param config_file to use
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_output_component_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file)
{
	switch_api_interface_t *api_interface;
	switch_file_interface_t *file_interface;

	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_OUTPUT_NS":output", start_call_output_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "output", "set:"RAYO_EXT_NS":stop", stop_output_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":pause", pause_output_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":resume", resume_output_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":speed-up", speed_up_output_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":speed-down", speed_down_output_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":volume-up", volume_up_output_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":volume-down", volume_down_output_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":seek", seek_output_component);

	rayo_actor_command_handler_add(RAT_MIXER, "", "set:"RAYO_OUTPUT_NS":output", start_mixer_output_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "output", "set:"RAYO_EXT_NS":stop", stop_output_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":pause", pause_output_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":resume", resume_output_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":speed-up", speed_up_output_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":speed-down", speed_down_output_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":volume-up", volume_up_output_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":volume-down", volume_down_output_component);
	rayo_actor_command_handler_add(RAT_MIXER_COMPONENT, "output", "set:"RAYO_OUTPUT_NS":seek", seek_output_component);

	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = "mod_rayo";
	file_interface->extens = rayo_supported_formats;
	file_interface->file_open = rayo_file_open;
	file_interface->file_close = rayo_file_close;
	file_interface->file_read = rayo_file_read;
	file_interface->file_seek = rayo_file_seek;

	switch_mutex_init(&fileman_globals.mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&fileman_globals.hash);

	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = "mod_rayo";
	file_interface->extens = fileman_supported_formats;
	file_interface->file_open = fileman_file_open;
	file_interface->file_close = fileman_file_close;
	file_interface->file_write = fileman_file_write;
	file_interface->file_read = fileman_file_read;
	file_interface->file_seek = fileman_file_seek;

	SWITCH_ADD_API(api_interface, "fileman", "Manage file audio", fileman_api, FILEMAN_SYNTAX);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Shutdown output component
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_output_component_shutdown(void)
{
	if (fileman_globals.hash) {
		switch_core_hash_destroy(&fileman_globals.hash);
	}

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

