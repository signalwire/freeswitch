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
 * rayo_input_component.c -- Rayo input component implementation
 *
 */
#include "rayo_components.h"
#include "rayo_cpa_detector.h"
#include "rayo_elements.h"
#include "srgs.h"
#include "nlsml.h"

#define MAX_DTMF 64

#define INPUT_MATCH_TAG "match"
#define INPUT_MATCH INPUT_MATCH_TAG, RAYO_INPUT_COMPLETE_NS
#define INPUT_NOINPUT "noinput", RAYO_INPUT_COMPLETE_NS
#define INPUT_NOMATCH "nomatch", RAYO_INPUT_COMPLETE_NS

#define RAYO_INPUT_COMPONENT_PRIVATE_VAR "__rayo_input_component"

struct input_handler;

static struct {
	/** grammar parser */
	struct srgs_parser *parser;
	/** default recognizer to use if none specified */
	const char *default_recognizer;
} globals;

/**
 * Input component state
 */
struct input_component {
	/** component base class */
	struct rayo_component base;
	/** true if speech detection */
	int speech_mode;
	/** Number of collected digits */
	int num_digits;
	/** Terminating digit */
	char term_digit;
	/** The collected digits */
	char digits[MAX_DTMF + 1];
	/** grammar to match */
	struct srgs_grammar *grammar;
	/** time when last digit was received */
	switch_time_t last_digit_time;
	/** timeout before first digit is received */
	int initial_timeout;
	/** maximum silence allowed */
	int max_silence;
	/** minimum speech detection confidence */
	double min_confidence;
	/** sensitivity to background noise */
	double sensitivity;
	/** timeout after first digit is received */
	int inter_digit_timeout;
	/** stop flag */
	int stop;
	/** true if input timers started */
	int start_timers;
	/** true if event fired for first digit / start of speech */
	int barge_event;
	/** optional language to use */
	const char *language;
	/** optional recognizer to use */
	const char *recognizer;
	/** global data */
	struct input_handler *handler;
};

#define INPUT_COMPONENT(x) ((struct input_component *)x)

/**
 * Call input state
 */
struct input_handler {
	/** media bug to monitor frames / control input lifecycle */
	switch_media_bug_t *bug;
	/** active voice input component */
	struct input_component *voice_component;
	/** active dtmf input component */
	struct input_component *dtmf_component;
	/** synchronizes media bug and dtmf callbacks */
	switch_mutex_t *mutex;
	/** last recognizer used */
	const char *last_recognizer;
};

/**
 * @param digit1 to match
 * @param digit2 to match
 * @return true if matching
 */
static int digit_test(char digit1, char digit2)
{
	return digit1 && digit2 && tolower(digit1) == tolower(digit2);
}

/**
 * Send match event to client
 */
static void send_match_event(struct rayo_component *component, iks *result)
{
	iks *event = rayo_component_create_complete_event(RAYO_COMPONENT(component), INPUT_MATCH);
	iks *match = iks_find(iks_find(event, "complete"), INPUT_MATCH_TAG);
	iks_insert_attrib(match, "content-type", "application/nlsml+xml");
	iks_insert_cdata(match, iks_string(iks_stack(result), result), 0);
	rayo_component_send_complete_event(component, event);
}

/**
 * Send barge-in event to client
 */
static void send_barge_event(struct rayo_component *component)
{
	iks *event = iks_new("presence");
	iks *x;
	iks_insert_attrib(event, "from", RAYO_JID(component));
	iks_insert_attrib(event, "to", component->client_jid);
	x = iks_insert(event, "start-of-input");
	iks_insert_attrib(x, "xmlns", RAYO_INPUT_NS);
	RAYO_SEND_REPLY(component, component->client_jid, event);
}

/**
 * Process DTMF press
 */
static switch_status_t input_component_on_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf, switch_dtmf_direction_t direction)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct input_handler *handler = (struct input_handler *)switch_channel_get_private(channel, RAYO_INPUT_COMPONENT_PRIVATE_VAR);

	if (handler) {
		int is_term_digit = 0;
		struct input_component *component;
		enum srgs_match_type match;
		const char *interpretation = NULL;

		switch_mutex_lock(handler->mutex);

		component = handler->dtmf_component;
		/* additional paranoia check */
		if (!component) {
			switch_mutex_unlock(handler->mutex);
			return SWITCH_STATUS_SUCCESS;
		}

		is_term_digit = digit_test(component->term_digit, dtmf->digit);

		if (!is_term_digit) {
			component->digits[component->num_digits] = dtmf->digit;
			component->num_digits++;
			component->digits[component->num_digits] = '\0';
			component->last_digit_time = switch_micro_time_now();
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Collected digits = \"%s\"\n", component->digits);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Collected term digit = \"%c\"\n", dtmf->digit);
		}

		match = srgs_grammar_match(component->grammar, component->digits, &interpretation);

		/* adjust result if terminating digit was pressed */
		if (is_term_digit) {
			if (match == SMT_MATCH_PARTIAL) {
				match = SMT_NO_MATCH;
			} else if (match == SMT_MATCH) {
				match = SMT_MATCH_END;
			}
		}

		switch (match) {
			case SMT_MATCH:
			case SMT_MATCH_PARTIAL: {
				/* need more digits */
				if (component->num_digits == 1) {
					send_barge_event(RAYO_COMPONENT(component));
				}
				break;
			}
			case SMT_NO_MATCH: {
				/* notify of no-match and remove input component */
				handler->dtmf_component = NULL;
				switch_core_media_bug_remove(session, &handler->bug);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "NO MATCH = %s\n", component->digits);
				rayo_component_send_complete(RAYO_COMPONENT(component), INPUT_NOMATCH);
				break;
			}
			case SMT_MATCH_END: {
				iks *result = nlsml_create_dtmf_match(component->digits, interpretation);
				/* notify of match and remove input component */
				handler->dtmf_component = NULL;
				switch_core_media_bug_remove(session, &handler->bug);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "MATCH = %s\n", component->digits);
				send_match_event(RAYO_COMPONENT(component), result);
				iks_delete(result);
				break;
			}
		}
		switch_mutex_unlock(handler->mutex);
	}
    return SWITCH_STATUS_SUCCESS;
}

/**
 * Monitor for input
 */
static switch_bool_t input_component_bug_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);
	struct input_handler *handler = (struct input_handler *)user_data;
	struct input_component *component;

	switch_mutex_lock(handler->mutex);
	component = handler->dtmf_component;

	switch(type) {
		case SWITCH_ABC_TYPE_INIT: {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding DTMF callback\n");
			switch_core_event_hook_add_recv_dtmf(session, input_component_on_dtmf);
			break;
		}
		case SWITCH_ABC_TYPE_READ_REPLACE: {
			switch_frame_t *rframe = switch_core_media_bug_get_read_replace_frame(bug);
			/* check for timeout */
			if (component && component->start_timers) {
				int elapsed_ms = (switch_micro_time_now() - component->last_digit_time) / 1000;
				if (component->num_digits && component->inter_digit_timeout > 0 && elapsed_ms > component->inter_digit_timeout) {
					enum srgs_match_type match;
					const char *interpretation = NULL;
					handler->dtmf_component = NULL;
					switch_core_media_bug_set_flag(bug, SMBF_PRUNE);

					/* we got some input, check for match */
					match = srgs_grammar_match(component->grammar, component->digits, &interpretation);
					if (match == SMT_MATCH || match == SMT_MATCH_END) {
						iks *result = nlsml_create_dtmf_match(component->digits, interpretation);
						/* notify of match */
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "MATCH = %s\n", component->digits);
						send_match_event(RAYO_COMPONENT(component), result);
						iks_delete(result);
					} else {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "inter-digit-timeout\n");
						rayo_component_send_complete(RAYO_COMPONENT(component), INPUT_NOMATCH);
					}
				} else if (!component->num_digits && component->initial_timeout > 0 && elapsed_ms > component->initial_timeout) {
					handler->dtmf_component = NULL;
					switch_core_media_bug_set_flag(bug, SMBF_PRUNE);
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "initial-timeout\n");
					rayo_component_send_complete(RAYO_COMPONENT(component), INPUT_NOINPUT);
				}
			}
			switch_core_media_bug_set_read_replace_frame(bug, rframe);
			break;
		}
		case SWITCH_ABC_TYPE_CLOSE:
			/* check for hangup */
			if (component) {
				if (component->stop) {
					handler->dtmf_component = NULL;
					rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_STOP);
				} else {
					handler->dtmf_component = NULL;
					rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_HANGUP);
				}
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Removing DTMF callback\n");
			switch_core_event_hook_remove_recv_dtmf(session, input_component_on_dtmf);
			break;
		default:
			break;
	}
	switch_mutex_unlock(handler->mutex);
	return SWITCH_TRUE;
}

/**
 * Validate input request
 * @param input request to validate
 * @param error message
 * @return 0 if error, 1 if valid
 */
static int validate_call_input(iks *input, const char **error)
{
	iks *grammar;
	const char *content_type;

	/* validate input attributes */
	if (!VALIDATE_RAYO_INPUT(input)) {
		*error = "Bad <input> attrib value";
		return 0;
	}

	/* missing grammar */
	grammar = iks_find(input, "grammar");
	if (!grammar) {
		*error = "Missing <grammar>";
		return 0;
	}

	/* only support srgs */
	content_type = iks_find_attrib(grammar, "content-type");
	if (!zstr(content_type) && strcmp("application/srgs+xml", content_type)) {
		*error = "Unsupported content type";
		return 0;
	}

	/* missing grammar body */
	if (zstr(iks_find_cdata(input, "grammar"))) {
		*error = "Grammar content is missing";
		return 0;
	}

	return 1;
}

/**
 * Start call input for the given component
 * @param component the input or prompt component
 * @param session the session
 * @param input the input request
 * @param iq the original input/prompt request
 */
static iks *start_call_input(struct input_component *component, switch_core_session_t *session, iks *input, iks *iq, const char *output_file, int barge_in)
{
	/* set up input component for new detection */
	struct input_handler *handler = (struct input_handler *)switch_channel_get_private(switch_core_session_get_channel(session), RAYO_INPUT_COMPONENT_PRIVATE_VAR);
	if (!handler) {
		/* create input component */
		handler = switch_core_session_alloc(session, sizeof(*handler));
		switch_mutex_init(&handler->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		switch_channel_set_private(switch_core_session_get_channel(session), RAYO_INPUT_COMPONENT_PRIVATE_VAR, handler);
		handler->last_recognizer = "";
	}

	/* TODO break up this function by mode... dtmf/voice/fax/etc */
	component->speech_mode = strcmp(iks_find_attrib_soft(input, "mode"), "dtmf");
	if (component->speech_mode && handler->voice_component) {
		/* don't allow multi voice input */
		return iks_new_error_detailed(iq, STANZA_ERROR_CONFLICT, "Multiple voice input is not allowed");
	}
	if (!component->speech_mode && handler->dtmf_component) {
		/* don't allow multi dtmf input */
		return iks_new_error_detailed(iq, STANZA_ERROR_CONFLICT, "Multiple dtmf input is not allowed");
	}

	if (component->speech_mode) {
		handler->voice_component = component;
	} else {
		handler->dtmf_component = component;
	}

	component->grammar = NULL;
	component->num_digits = 0;
	component->digits[0] = '\0';
	component->stop = 0;
	component->initial_timeout = iks_find_int_attrib(input, "initial-timeout");
	component->inter_digit_timeout = iks_find_int_attrib(input, "inter-digit-timeout");
	component->max_silence = iks_find_int_attrib(input, "max-silence");
	component->min_confidence = iks_find_decimal_attrib(input, "min-confidence");
	component->sensitivity = iks_find_decimal_attrib(input, "sensitivity");
	component->barge_event = iks_find_bool_attrib(input, "barge-event");
	component->start_timers = iks_find_bool_attrib(input, "start-timers");
	component->term_digit = iks_find_char_attrib(input, "terminator");
	component->recognizer = iks_find_attrib(input, "recognizer");
	component->language = iks_find_attrib(input, "language");
	component->handler = handler;

	/* is this voice or dtmf srgs grammar? */
	if (!component->speech_mode) {

		/* parse the grammar */
		if (!(component->grammar = srgs_parse(globals.parser, iks_find_cdata(input, "grammar")))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Failed to parse grammar body\n");
			RAYO_UNLOCK(component);
			RAYO_DESTROY(component);
			return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Failed to parse grammar body");
		}

		component->last_digit_time = switch_micro_time_now();

		/* acknowledge command */
		rayo_component_send_start(RAYO_COMPONENT(component), iq);

		/* start dtmf input detection */
		if (switch_core_media_bug_add(session, "rayo_input_component", NULL, input_component_bug_callback, handler, 0, SMBF_READ_REPLACE, &handler->bug) != SWITCH_STATUS_SUCCESS) {
			handler->dtmf_component = NULL;
			rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_ERROR);
		}
	} else {
		switch_stream_handle_t grammar = { 0 };
		SWITCH_STANDARD_STREAM(grammar);

		if (zstr(component->recognizer)) {
			component->recognizer = globals.default_recognizer;
		}

		/* if recognition engine is different, we can't handle this request */
		if (!zstr(handler->last_recognizer) && strcmp(component->recognizer, handler->last_recognizer)) {
			handler->voice_component = NULL;
			RAYO_UNLOCK(component);
			RAYO_DESTROY(component);
			return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Must use the same recognizer for the entire call");
		}
		handler->last_recognizer = switch_core_session_strdup(session, component->recognizer);

		if (!strcmp(component->recognizer, "pocketsphinx")) {
			const char *jsgf_path;

			/* transform SRGS grammar to JSGF */
			if (!(component->grammar = srgs_parse(globals.parser, iks_find_cdata(input, "grammar")))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Failed to parse grammar body\n");
				handler->voice_component = NULL;
				RAYO_UNLOCK(component);
				RAYO_DESTROY(component);
				return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Failed to parse grammar body");
			}
			jsgf_path = srgs_grammar_to_jsgf_file(component->grammar, SWITCH_GLOBAL_dirs.grammar_dir, "gram");
			if (!jsgf_path) {
				handler->voice_component = NULL;
				RAYO_UNLOCK(component);
				RAYO_DESTROY(component);
				return iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "Grammar conversion to JSGF error");
			}

			/* build pocketsphinx grammar string */
			grammar.write_function(&grammar,
				"{start-input-timers=%s,no-input-timeout=%d,speech-timeout=%d,confidence-threshold=%d}%s",
				component->start_timers ? "true" : "false",
				component->initial_timeout,
				component->max_silence,
				(int)ceil(component->min_confidence * 100.0),
				jsgf_path);
		} else if (!strncmp(component->recognizer, "unimrcp", strlen("unimrcp"))) {
			/* send inline grammar to unimrcp */
			grammar.write_function(&grammar, "{start-input-timers=%s,confidence-threshold=%f,sensitivity-level=%f",
									component->start_timers ? "true" : "false",
									component->min_confidence,
									component->sensitivity);

			if (component->initial_timeout > 0) {
				grammar.write_function(&grammar, ",no-input-timeout=%d",
					component->initial_timeout);
			}

			if (component->max_silence > 0) {
				grammar.write_function(&grammar, ",speech-complete-timeout=%d,speech-incomplete-timeout=%d",
					component->max_silence,
					component->max_silence);
			}

			if (!zstr(component->language)) {
				grammar.write_function(&grammar, ",speech-language=%s", component->language);
			}

			if (!strcmp(iks_find_attrib_soft(input, "mode"), "any")) {
				/* set dtmf params */
				if (component->inter_digit_timeout > 0) {
					grammar.write_function(&grammar, ",dtmf-interdigit-timeout=%d", component->inter_digit_timeout);
				}
				if (component->term_digit) {
					grammar.write_function(&grammar, ",dtmf-term-char=%c", component->term_digit);
				}
			}

			grammar.write_function(&grammar, "}inline:%s", iks_find_cdata(input, "grammar"));
		} else {
			/* passthrough to unknown ASR module */
			grammar.write_function(&grammar, "%s", iks_find_cdata(input, "grammar"));
		}

		/* acknowledge command */
		rayo_component_send_start(RAYO_COMPONENT(component), iq);

		/* start speech detection */
		switch_channel_set_variable(switch_core_session_get_channel(session), "fire_asr_events", "true");
		if (switch_ivr_detect_speech(session, component->recognizer, grammar.data, "mod_rayo_grammar", "", NULL) != SWITCH_STATUS_SUCCESS) {
			handler->voice_component = NULL;
			rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_ERROR);
		}
		switch_safe_free(grammar.data);
	}

	return NULL;
}

/**
 * Create input component id for session.
 * @param session requesting component
 * @param input request
 * @return the ID
 */
static char *create_input_component_id(switch_core_session_t *session, iks *input)
{
	const char *mode = "unk";
	if (input) {
		mode = iks_find_attrib_soft(input, "mode");
		if (!strcmp(mode, "any")) {
			mode = "voice";
		}
	}
	return switch_core_session_sprintf(session, "%s-input-%s", switch_core_session_get_uuid(session), mode);
}

/**
 * Start execution of input component
 */
static iks *start_call_input_component(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *iq = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *input = iks_find(iq, "input");
	char *component_id = create_input_component_id(session, input);
	switch_memory_pool_t *pool = NULL;
	struct input_component *input_component = NULL;
	const char *error = NULL;

	if (!validate_call_input(input, &error)) {
		return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, error);
	}

	/* create component */
	switch_core_new_memory_pool(&pool);
	input_component = switch_core_alloc(pool, sizeof(*input_component));
	rayo_component_init(RAYO_COMPONENT(input_component), pool, RAT_CALL_COMPONENT, "input", component_id, call, iks_find_attrib(iq, "from"));

	/* start input */
	return start_call_input(input_component, session, iks_find(iq, "input"), iq, NULL, 0);
}

/**
 * Stop execution of input component
 */
static iks *stop_call_input_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	struct input_component *input_component = INPUT_COMPONENT(component);

	if (input_component && !input_component->stop) {
		switch_core_session_t *session = switch_core_session_locate(RAYO_COMPONENT(component)->parent->id);
		if (session) {
			switch_mutex_lock(input_component->handler->mutex);
			if (input_component->speech_mode) {
				input_component->stop = 1;
				switch_ivr_stop_detect_speech(session);
				rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_STOP);
			} else if (input_component->handler->bug) {
				input_component->stop = 1;
				switch_core_media_bug_remove(session, &input_component->handler->bug);
			}
			switch_mutex_unlock(input_component->handler->mutex);
			switch_core_session_rwunlock(session);
		}
	}
	return iks_new_iq_result(iq);
}

/**
 * Start input component timers
 */
static iks *start_timers_call_input_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	struct input_component *input_component = INPUT_COMPONENT(component);
	if (input_component) {
		switch_core_session_t *session = switch_core_session_locate(RAYO_COMPONENT(component)->parent->id);
		if (session) {
			switch_mutex_lock(input_component->handler->mutex);
			if (input_component->speech_mode) {
				switch_ivr_detect_speech_start_input_timers(session);
			} else {
				input_component->last_digit_time = switch_micro_time_now();
				input_component->start_timers = 1;
			}
			switch_mutex_unlock(input_component->handler->mutex);
			switch_core_session_rwunlock(session);
		}
	}
	return iks_new_iq_result(iq);
}

/**
 * Handle speech detection event
 */
static void on_detected_speech_event(switch_event_t *event)
{
	const char *speech_type = switch_event_get_header(event, "Speech-Type");
	char *event_str = NULL;
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	switch_event_serialize(event, &event_str, SWITCH_FALSE);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s\n", event_str);
	if (!speech_type || !uuid) {
		return;
	}

	if (!strcasecmp("detected-speech", speech_type)) {
		char *component_id = switch_mprintf("%s-input-voice", uuid);
		struct rayo_component *component = RAYO_COMPONENT_LOCATE(component_id);

		switch_safe_free(component_id);
		if (component) {
			const char *result = switch_event_get_body(event);
			switch_mutex_lock(INPUT_COMPONENT(component)->handler->mutex);
			INPUT_COMPONENT(component)->handler->voice_component = NULL;
			switch_mutex_unlock(INPUT_COMPONENT(component)->handler->mutex);
			if (zstr(result)) {
				rayo_component_send_complete(component, INPUT_NOMATCH);
			} else {
				if (strchr(result, '<')) {
					/* got an XML result */
					enum nlsml_match_type match_type = nlsml_parse(result, uuid);
					switch (match_type) {
					case NMT_NOINPUT:
						rayo_component_send_complete(component, INPUT_NOINPUT);
						break;
					case NMT_MATCH: {
						iks *result_xml = nlsml_normalize(result);
						send_match_event(RAYO_COMPONENT(component), result_xml);
						iks_delete(result_xml);
						break;
					}
					case NMT_BAD_XML:
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_WARNING, "Failed to parse NLSML result: %s!\n", result);
						rayo_component_send_complete(component, INPUT_NOMATCH);
						break;
					case NMT_NOMATCH:
						rayo_component_send_complete(component, INPUT_NOMATCH);
						break;
					default:
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_CRIT, "Unknown NLSML match type: %i, %s!\n", match_type, result);
						rayo_component_send_complete(component, INPUT_NOMATCH);
						break;
					}
				} else if (strstr(result, "002")) {
					/* Completion-Cause: 002 no-input-timeout */
					rayo_component_send_complete(component, INPUT_NOINPUT);
				} else {
					/* assume no match */
					rayo_component_send_complete(component, INPUT_NOMATCH);
				}
			}
			RAYO_UNLOCK(component);
		}
	} else if (!strcasecmp("begin-speaking", speech_type)) {
		char *component_id = switch_mprintf("%s-input-voice", uuid);
		struct rayo_component *component = RAYO_COMPONENT_LOCATE(component_id);
		switch_safe_free(component_id);
		if (component && INPUT_COMPONENT(component)->barge_event) {
			send_barge_event(component);
		}
		RAYO_UNLOCK(component);
	} else if (!strcasecmp("closed", speech_type)) {
		char *component_id = switch_mprintf("%s-input-voice", uuid);
		struct rayo_component *component = RAYO_COMPONENT_LOCATE(component_id);
		switch_safe_free(component_id);
		if (component) {
			char *channel_state = switch_event_get_header(event, "Channel-State");
			switch_mutex_lock(INPUT_COMPONENT(component)->handler->mutex);
			INPUT_COMPONENT(component)->handler->voice_component = NULL;
			switch_mutex_unlock(INPUT_COMPONENT(component)->handler->mutex);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Recognizer closed\n");
			if (channel_state && !strcmp("CS_HANGUP", channel_state)) {
				rayo_component_send_complete(component, COMPONENT_COMPLETE_HANGUP);
			} else {
				/* shouldn't get here... */
				rayo_component_send_complete(component, COMPONENT_COMPLETE_ERROR);
			}
			RAYO_UNLOCK(component);
		}
	}
	switch_safe_free(event_str);
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
	globals.default_recognizer = "pocketsphinx";

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Configuring module\n");
	if (!(xml = switch_xml_open_cfg(config_file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", config_file);
		return SWITCH_STATUS_TERM;
	}

	/* get params */
	{
		switch_xml_t settings = switch_xml_child(cfg, "input");
		if (settings) {
			switch_xml_t param;
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				const char *var = switch_xml_attr_soft(param, "name");
				const char *val = switch_xml_attr_soft(param, "value");
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "param: %s = %s\n", var, val);
				if (!strcasecmp(var, "default-recognizer")) {
					if (!zstr(val)) {
						globals.default_recognizer = switch_core_strdup(pool, val);
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
 * Initialize input component
 * @param module_interface
 * @param pool memory pool to allocate from
 * @param config_file to use
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_input_component_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file)
{
	if (do_config(pool, config_file) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	srgs_init();
	nlsml_init();

	globals.parser = srgs_parser_new(NULL);

	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_INPUT_NS":input", start_call_input_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "input", "set:"RAYO_EXT_NS":stop", stop_call_input_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "input", "set:"RAYO_INPUT_NS":start-timers", start_timers_call_input_component);
	switch_event_bind("rayo_input_component", SWITCH_EVENT_DETECTED_SPEECH, SWITCH_EVENT_SUBCLASS_ANY, on_detected_speech_event, NULL);

	return rayo_cpa_detector_load(module_interface, pool, config_file);
}

/**
 * Shutdown input component
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_input_component_shutdown(void)
{
	srgs_parser_destroy(globals.parser);
	switch_event_unbind_callback(on_detected_speech_event);

	rayo_cpa_detector_shutdown();

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

