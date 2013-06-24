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
	/** Terminating digits */
	int term_digit_mask;
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
	int min_confidence;
	/** timeout after first digit is received */
	int inter_digit_timeout;
	/** stop flag */
	int stop;
	/** true if input timers started */
	int start_timers;
	/** true if event fired for first digit / start of speech */
	int barge_event;
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
	/** active input component - TODO multiple inputs */
	struct input_component *component;
	/** synchronizes media bug and dtmf callbacks */
	switch_mutex_t *mutex;
};

/**
 * @return digit mask
 */
static int get_digit_mask(char digit)
{
	switch(digit) {
		case '0': return 1;
		case '1': return 1 << 1;
		case '2': return 1 << 2;
		case '3': return 1 << 3;
		case '4': return 1 << 4;
		case '5': return 1 << 5;
		case '6': return 1 << 6;
		case '7': return 1 << 7;
		case '8': return 1 << 8;
		case '9': return 1 << 9;
		case 'A':
		case 'a': return 1 << 10;
		case 'B':
		case 'b': return 1 << 11;
		case 'C':
		case 'c': return 1 << 12;
		case 'D':
		case 'd': return 1 << 13;
		case '#': return 1 << 14;
		case '*': return 1 << 15;
	}
	return 0;
}

/**
 * @param digit_mask to check
 * @param digit to look for
 * @return true if set
 */
static int digit_mask_test(int digit_mask, char digit)
{
	return digit_mask & get_digit_mask(digit);
}

/**
 * @param digit_mask to set digit in
 * @param digit to set
 * @return the digit mask with the set digit
 */
static int digit_mask_set(int digit_mask, char digit)
{
	return digit_mask | get_digit_mask(digit);
}

/**
 * @param digit_mask to set digits in
 * @param digits to add to mask
 * @return the digit mask with the set digits
 */
static int digit_mask_set_from_digits(int digit_mask, const char *digits)
{
	if (!zstr(digits)) {
		int digits_len = strlen(digits);
		int i;
		for (i = 0; i < digits_len; i++) {
			digit_mask = digit_mask_set(digit_mask, digits[i]);
		}
	}
	return digit_mask;
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
	RAYO_SEND(component->client_jid, RAYO_REPLY_CREATE(component, event));
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

		switch_mutex_lock(handler->mutex);

		component = handler->component;
		/* additional paranoia check */
		if (!component) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Received DTMF without active input component\n");
			switch_mutex_unlock(handler->mutex);
			return SWITCH_STATUS_SUCCESS;
		}

		is_term_digit = digit_mask_test(component->term_digit_mask, dtmf->digit);

		if (!is_term_digit) {
			component->digits[component->num_digits] = dtmf->digit;
			component->num_digits++;
			component->digits[component->num_digits] = '\0';
			component->last_digit_time = switch_micro_time_now();
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Collected digits = \"%s\"\n", component->digits);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Collected term digit = \"%c\"\n", dtmf->digit);
		}

		match = srgs_grammar_match(component->grammar, component->digits);

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
				handler->component = NULL;
				switch_core_media_bug_remove(session, &handler->bug);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "NO MATCH = %s\n", component->digits);
				rayo_component_send_complete(RAYO_COMPONENT(component), INPUT_NOMATCH);
				break;
			}
			case SMT_MATCH_END: {
				iks *result = nlsml_create_dtmf_match(component->digits);
				/* notify of match and remove input component */
				handler->component = NULL;
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
	component = handler->component;

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
					handler->component = NULL;
					switch_core_media_bug_set_flag(bug, SMBF_PRUNE);

					/* we got some input, check for match */
					match = srgs_grammar_match(component->grammar, component->digits);
					if (match == SMT_MATCH || match == SMT_MATCH_END) {
						iks *result = nlsml_create_dtmf_match(component->digits);
						/* notify of match */
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "MATCH = %s\n", component->digits);
						send_match_event(RAYO_COMPONENT(component), result);
						iks_delete(result);
					} else {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "inter-digit-timeout\n");
						rayo_component_send_complete(RAYO_COMPONENT(component), INPUT_NOMATCH);
					}
				} else if (!component->num_digits && component->initial_timeout > 0 && elapsed_ms > component->initial_timeout) {
					handler->component = NULL;
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
					handler->component = NULL;
					rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_STOP);
				} else {
					handler->component = NULL;
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
	}
	handler->component = component;
	component->num_digits = 0;
	component->digits[0] = '\0';
	component->stop = 0;
	component->speech_mode = 0;
	component->initial_timeout = iks_find_int_attrib(input, "initial-timeout");
	component->inter_digit_timeout = iks_find_int_attrib(input, "inter-digit-timeout");
	component->max_silence = iks_find_int_attrib(input, "max-silence");
	component->min_confidence = (int)ceil(iks_find_decimal_attrib(input, "min-confidence") * 100.0);
	component->barge_event = iks_find_bool_attrib(input, "barge-event");
	component->start_timers = iks_find_bool_attrib(input, "start-timers");
	/* TODO this should just be a single digit terminator? */
	component->term_digit_mask = digit_mask_set_from_digits(0, iks_find_attrib_soft(input, "terminator"));
	/* TODO recognizer ignored */
	/* TODO language ignored */
	component->handler = handler;

	/* parse the grammar */
	if (!(component->grammar = srgs_parse(globals.parser, iks_find_cdata(input, "grammar")))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Failed to parse grammar body\n");
		RAYO_UNLOCK(component);
		RAYO_DESTROY(component);
		return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Failed to parse grammar body");
	}

	/* is this voice or dtmf srgs grammar? */
	if (!strcasecmp("dtmf", iks_find_attrib_soft(input, "mode"))) {
		component->last_digit_time = switch_micro_time_now();

		/* acknowledge command */
		rayo_component_send_start(RAYO_COMPONENT(component), iq);

		/* start dtmf input detection */
		if (switch_core_media_bug_add(session, "rayo_input_component", NULL, input_component_bug_callback, handler, 0, SMBF_READ_REPLACE, &handler->bug) != SWITCH_STATUS_SUCCESS) {
			rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_ERROR);
		}
	} else {
		char *grammar = NULL;
		const char *jsgf_path;
		component->speech_mode = 1;
		jsgf_path = srgs_grammar_to_jsgf_file(component->grammar, SWITCH_GLOBAL_dirs.grammar_dir, "gram");
		if (!jsgf_path) {
			RAYO_UNLOCK(component);
			RAYO_DESTROY(component);
			return iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "Grammar error");
		}

		/* acknowledge command */
		rayo_component_send_start(RAYO_COMPONENT(component), iq);

		/* TODO configurable speech detection - different engines, grammar passthrough, dtmf handled by recognizer */
		grammar = switch_mprintf("{no-input-timeout=%s,speech-timeout=%s,start-input-timers=%s,confidence-threshold=%d}%s",
			component->initial_timeout, component->max_silence,
			component->start_timers ? "true" : "false",
			component->min_confidence, jsgf_path);
		/* start speech detection */
		switch_channel_set_variable(switch_core_session_get_channel(session), "fire_asr_events", "true");
		if (switch_ivr_detect_speech(session, "pocketsphinx", grammar, "mod_rayo_grammar", "", NULL) != SWITCH_STATUS_SUCCESS) {
			rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_ERROR);
		}
		switch_safe_free(grammar);
	}

	return NULL;
}

/**
 * Start execution of input component
 */
static iks *start_call_input_component(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *iq = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	char *component_id = switch_mprintf("%s-input", switch_core_session_get_uuid(session));
	switch_memory_pool_t *pool = NULL;
	struct input_component *input_component = NULL;
	iks *input = iks_find(iq, "input");
	const char *error = NULL;

	if (!validate_call_input(input, &error)) {
		return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, error);
	}

	/* create component */
	switch_core_new_memory_pool(&pool);
	input_component = switch_core_alloc(pool, sizeof(*input_component));
	rayo_component_init(RAYO_COMPONENT(input_component), pool, RAT_CALL_COMPONENT, "input", component_id, call, iks_find_attrib(iq, "from"));
	switch_safe_free(component_id);

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
				rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_STOP);
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
	switch_event_serialize(event, &event_str, SWITCH_FALSE);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s\n", event_str);
	if (!speech_type) {
		return;
	}
	if (!strcasecmp("detected-speech", speech_type)) {
		const char *uuid = switch_event_get_header(event, "Unique-ID");
		char *component_id = switch_mprintf("%s-input", uuid);
		struct rayo_component *component = RAYO_COMPONENT_LOCATE(component_id);
		switch_safe_free(component_id);
		if (component) {
			const char *result = switch_event_get_body(event);
			switch_mutex_lock(INPUT_COMPONENT(component)->handler->mutex);
			INPUT_COMPONENT(component)->handler->component = NULL;
			switch_mutex_unlock(INPUT_COMPONENT(component)->handler->mutex);
			if (zstr(result)) {
				rayo_component_send_complete(component, INPUT_NOMATCH);
			} else {
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
			}
			RAYO_UNLOCK(component);
		}
	} else if (!strcasecmp("begin-speaking", speech_type)) {
		const char *uuid = switch_event_get_header(event, "Unique-ID");
		char *component_id = switch_mprintf("%s-input", uuid);
		struct rayo_component *component = RAYO_COMPONENT_LOCATE(component_id);
		switch_safe_free(component_id);
		if (component && INPUT_COMPONENT(component)->barge_event) {
			send_barge_event(component);
		}
		RAYO_UNLOCK(component);
	} else if (!strcasecmp("closed", speech_type)) {
		const char *uuid = switch_event_get_header(event, "Unique-ID");
		char *component_id = switch_mprintf("%s-input", uuid);
		struct rayo_component *component = RAYO_COMPONENT_LOCATE(component_id);
		switch_safe_free(component_id);
		if (component) {
			char *channel_state = switch_event_get_header(event, "Channel-State");
			switch_mutex_lock(INPUT_COMPONENT(component)->handler->mutex);
			INPUT_COMPONENT(component)->handler->component = NULL;
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
 * Initialize input component
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_input_component_load(void)
{
	srgs_init();
	nlsml_init();

	globals.parser = srgs_parser_new(NULL);

	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_INPUT_NS":input", start_call_input_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "input", "set:"RAYO_EXT_NS":stop", stop_call_input_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "input", "set:"RAYO_INPUT_NS":start-timers", start_timers_call_input_component);
	switch_event_bind("rayo_input_component", SWITCH_EVENT_DETECTED_SPEECH, SWITCH_EVENT_SUBCLASS_ANY, on_detected_speech_event, NULL);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Shutdown input component
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_input_component_shutdown(void)
{
	srgs_parser_destroy(globals.parser);
	switch_event_unbind_callback(on_detected_speech_event);
	return SWITCH_STATUS_SUCCESS;
}
