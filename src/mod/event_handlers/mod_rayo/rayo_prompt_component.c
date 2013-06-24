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
 * rayo_prompt_component.c -- Rayo prompt component implementation
 *
 */
#include "rayo_components.h"
#include "rayo_elements.h"

enum prompt_component_state {
	/* initial state - no barge */
	PCS_START_OUTPUT,
	/* playing prompt */
	PCS_OUTPUT,
	/* start input - no barge */
	PCS_START_INPUT,
	/* input starting - start timers needed */
	PCS_START_INPUT_TIMERS,
	/* initial state - barge in */
	PCS_START_OUTPUT_BARGE,
	/* start input for barge-in */
	PCS_START_INPUT_OUTPUT,
	/* playing and detecting */
	PCS_INPUT_OUTPUT,
	/* barge in on output */
	PCS_STOP_OUTPUT,
	/* detecting */
	PCS_INPUT,
	/* finishing, stop output */
	PCS_DONE_STOP_OUTPUT,
	/* finished */
	PCS_DONE
};

/**
 * Prompt state
 */
struct prompt_component {
	struct rayo_component base;
	enum prompt_component_state state;
	iks *iq;
	iks *complete;
	const char *input_jid;
	const char *output_jid;
};

#define PROMPT_COMPONENT(x) ((struct prompt_component *)x)

static const char *prompt_component_state_to_string(enum prompt_component_state state)
{
	switch(state) {
		case PCS_START_OUTPUT_BARGE: return "START_OUTPUT_BARGE";
		case PCS_START_OUTPUT: return "START_OUTPUT";
		case PCS_START_INPUT_OUTPUT: return "START_INPUT_OUTPUT";
		case PCS_START_INPUT: return "START_INPUT";
		case PCS_START_INPUT_TIMERS: return "START_INPUT_TIMERS";
		case PCS_INPUT_OUTPUT: return "INPUT_OUTPUT";
		case PCS_STOP_OUTPUT: return "STOP_OUTPUT";
		case PCS_INPUT: return "INPUT";
		case PCS_OUTPUT: return "OUTPUT";
		case PCS_DONE_STOP_OUTPUT: return "DONE_STOP_OUTPUT";
		case PCS_DONE: return "DONE";
	}
	return "UNKNOWN";
}

/**
 * Send stop to component
 */
static void rayo_component_send_stop(struct rayo_actor *from, const char *to)
{
	iks *stop = iks_new("iq");
	iks *x;
	iks_insert_attrib(stop, "from", RAYO_JID(from));
	iks_insert_attrib(stop, "to", to);
	iks_insert_attrib(stop, "type", "set");
	iks_insert_attrib_printf(stop, "id", "mod_rayo-%d", RAYO_SEQ_NEXT(from));
	x = iks_insert(stop, "stop");
	iks_insert_attrib(x, "xmlns", RAYO_EXT_NS);
	RAYO_SEND(to, RAYO_MESSAGE_CREATE(from, stop));
}

/**
 * Start input component
 */
static void start_input(struct prompt_component *prompt, int start_timers, int barge_event)
{
	iks *iq = iks_new("iq");
	iks *input = iks_find(PROMPT_COMPONENT(prompt)->iq, "prompt");
	input = iks_find(input, "input");
	iks_insert_attrib(iq, "from", RAYO_JID(prompt));
	iks_insert_attrib(iq, "to", RAYO_JID(RAYO_COMPONENT(prompt)->parent));
	iks_insert_attrib_printf(iq, "id", "mod_rayo-%d", RAYO_SEQ_NEXT(prompt));
	iks_insert_attrib(iq, "type", "set");
	input = iks_copy_within(input, iks_stack(iq));
	iks_insert_attrib(input, "start-timers", start_timers ? "true" : "false");
	iks_insert_attrib(input, "barge-event", barge_event ? "true" : "false");
	iks_insert_node(iq, input);
	RAYO_SEND(RAYO_JID(RAYO_COMPONENT(prompt)->parent), RAYO_MESSAGE_CREATE(prompt, iq));
}

/**
 * Start input component timers
 */
static void start_input_timers(struct prompt_component *prompt)
{
	iks *x;
	iks *iq = iks_new("iq");
	iks_insert_attrib(iq, "from", RAYO_JID(prompt));
	iks_insert_attrib(iq, "to", prompt->input_jid);
	iks_insert_attrib(iq, "type", "set");
	iks_insert_attrib_printf(iq, "id", "mod_rayo-%d", RAYO_SEQ_NEXT(prompt));
	x = iks_insert(iq, "start-timers");
	iks_insert_attrib(x, "xmlns", RAYO_INPUT_NS);
	RAYO_SEND(prompt->input_jid, RAYO_MESSAGE_CREATE(prompt, iq));
}

/**
 * Handle start of output.
 */
static iks *prompt_component_handle_output_start(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) output start\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state));

	switch (PROMPT_COMPONENT(prompt)->state) {
		case PCS_START_OUTPUT:
			PROMPT_COMPONENT(prompt)->output_jid = switch_core_strdup(RAYO_POOL(prompt), msg->from_jid);
			PROMPT_COMPONENT(prompt)->state = PCS_OUTPUT;
			/* send ref to client */
			rayo_component_send_start(RAYO_COMPONENT(prompt), PROMPT_COMPONENT(prompt)->iq);
			break;
		case PCS_START_OUTPUT_BARGE:
			PROMPT_COMPONENT(prompt)->output_jid = switch_core_strdup(RAYO_POOL(prompt), msg->from_jid);
			PROMPT_COMPONENT(prompt)->state = PCS_START_INPUT_OUTPUT;
			/* start input without timers and with barge events */
			start_input(PROMPT_COMPONENT(prompt), 0, 1);
			break;
		case PCS_OUTPUT:
		case PCS_START_INPUT_OUTPUT:
		case PCS_START_INPUT:
		case PCS_START_INPUT_TIMERS:
		case PCS_INPUT_OUTPUT:
		case PCS_STOP_OUTPUT:
		case PCS_INPUT:
		case PCS_DONE_STOP_OUTPUT:
		case PCS_DONE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, unexpected start output event\n", RAYO_JID(prompt));
			break;
	}

	return NULL;
}

/**
 * Handle start of input.
 */
static iks *prompt_component_handle_input_start(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) input start\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state));

	switch (PROMPT_COMPONENT(prompt)->state) {
		case PCS_START_INPUT:
			PROMPT_COMPONENT(prompt)->input_jid = switch_core_strdup(RAYO_POOL(prompt), msg->from_jid);
			PROMPT_COMPONENT(prompt)->state = PCS_INPUT;
			break;
		case PCS_START_INPUT_OUTPUT:
			PROMPT_COMPONENT(prompt)->input_jid = switch_core_strdup(RAYO_POOL(prompt), msg->from_jid);
			PROMPT_COMPONENT(prompt)->state = PCS_INPUT_OUTPUT;
			/* send ref to client */
			rayo_component_send_start(RAYO_COMPONENT(prompt), PROMPT_COMPONENT(prompt)->iq);
			iks_delete(PROMPT_COMPONENT(prompt)->iq);
			break;
		case PCS_START_INPUT_TIMERS:
			PROMPT_COMPONENT(prompt)->input_jid = switch_core_strdup(RAYO_POOL(prompt), msg->from_jid);
			PROMPT_COMPONENT(prompt)->state = PCS_INPUT;
			/* send ref to client */
			rayo_component_send_start(RAYO_COMPONENT(prompt), PROMPT_COMPONENT(prompt)->iq);
			iks_delete(PROMPT_COMPONENT(prompt)->iq);
			start_input_timers(PROMPT_COMPONENT(prompt));
			break;
		case PCS_DONE:
			/* stopped by client */
			PROMPT_COMPONENT(prompt)->input_jid = switch_core_strdup(RAYO_POOL(prompt), msg->from_jid);
			rayo_component_send_stop(prompt, msg->from_jid);
			break;
		case PCS_START_OUTPUT:
		case PCS_START_OUTPUT_BARGE:
		case PCS_INPUT_OUTPUT:
		case PCS_INPUT:
		case PCS_STOP_OUTPUT:
		case PCS_OUTPUT:
		case PCS_DONE_STOP_OUTPUT:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, unexpected start input event\n", RAYO_JID(prompt));
			break;
	}
	return NULL;
}

/**
 * Handle start of input/output.
 */
static iks *prompt_component_handle_io_start(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, got <ref> from %s: %s\n",
		RAYO_JID(prompt), msg->from_jid, iks_string(iks_stack(iq), iq));
	if (!strcmp("input", msg->from_subtype)) {
		return prompt_component_handle_input_start(prompt, msg, data);
	} else if (!strcmp("output", msg->from_subtype)) {
		return prompt_component_handle_output_start(prompt, msg, data);
	}
	return NULL;
}

/**
 * Handle barge event
 */
static iks *prompt_component_handle_input_start_timers_error(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	/* this is only expected if input component is gone */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) start timers error\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state));

	return NULL;
}

/**
 * Handle input failure.
 */
static iks *prompt_component_handle_input_error(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *error = iks_find(iq, "error");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) input error\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state));

	switch (PROMPT_COMPONENT(prompt)->state) {
		case PCS_START_INPUT_TIMERS:
		case PCS_START_INPUT:
			/* send error to client */
			PROMPT_COMPONENT(prompt)-> state = PCS_DONE;
			iks_delete(PROMPT_COMPONENT(prompt)->iq);
			rayo_component_send_complete(RAYO_COMPONENT(prompt), COMPONENT_COMPLETE_ERROR);
			break;
			break;
		case PCS_START_INPUT_OUTPUT:
			PROMPT_COMPONENT(prompt)->state = PCS_DONE_STOP_OUTPUT;

			/* forward error to client */
			iq = PROMPT_COMPONENT(prompt)->iq;
			iks_insert_attrib(iq, "from", RAYO_JID(RAYO_COMPONENT(prompt)->parent));
			iks_insert_attrib(iq, "to", RAYO_COMPONENT(prompt)->client_jid);
			iks_insert_node(iq, iks_copy_within(error, iks_stack(iq)));
			PROMPT_COMPONENT(prompt)->complete = iq;

			rayo_component_send_stop(prompt, PROMPT_COMPONENT(prompt)->output_jid);
			break;
		case PCS_START_OUTPUT:
		case PCS_START_OUTPUT_BARGE:
		case PCS_INPUT_OUTPUT:
		case PCS_STOP_OUTPUT:
		case PCS_INPUT:
		case PCS_OUTPUT:
		case PCS_DONE_STOP_OUTPUT:
		case PCS_DONE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, unexpected start input error event\n", RAYO_JID(prompt));
			break;
	}

	return NULL;
}

/**
 * Handle output failure.
 */
static iks *prompt_component_handle_output_error(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *error = iks_find(iq, "error");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) output error\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state));

	switch (PROMPT_COMPONENT(prompt)->state) {
		case PCS_START_OUTPUT:
		case PCS_START_OUTPUT_BARGE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, <output> error: %s\n", RAYO_JID(prompt), iks_string(iks_stack(iq), iq));
			PROMPT_COMPONENT(prompt)->state = PCS_DONE;

			/* forward error to client */
			iq = PROMPT_COMPONENT(prompt)->iq;
			iks_insert_attrib(iq, "from", RAYO_JID(RAYO_COMPONENT(prompt)->parent));
			iks_insert_attrib(iq, "to", RAYO_COMPONENT(prompt)->client_jid);
			iks_insert_node(iq, iks_copy_within(error, iks_stack(iq)));
			RAYO_SEND(RAYO_COMPONENT(prompt)->client_jid, RAYO_REPLY_CREATE(prompt, iq));

			/* done */
			RAYO_UNLOCK(prompt);
			RAYO_DESTROY(prompt);

			break;
		case PCS_START_INPUT_OUTPUT:
		case PCS_START_INPUT_TIMERS:
		case PCS_START_INPUT:
		case PCS_INPUT_OUTPUT:
		case PCS_STOP_OUTPUT:
		case PCS_INPUT:
		case PCS_OUTPUT:
		case PCS_DONE_STOP_OUTPUT:
		case PCS_DONE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, unexpected start output error event\n", RAYO_JID(prompt));
			break;
	}

	return NULL;
}

/**
 * Handle barge event
 */
static iks *prompt_component_handle_input_barge(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	iks *presence = msg->payload;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) input barge\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state));

	switch (PROMPT_COMPONENT(prompt)->state) {
		case PCS_INPUT_OUTPUT:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, got <start-of-input> from %s: %s\n",
				RAYO_JID(prompt), msg->from_jid, iks_string(iks_stack(presence), presence));
			PROMPT_COMPONENT(prompt)->state = PCS_STOP_OUTPUT;
			rayo_component_send_stop(prompt, PROMPT_COMPONENT(prompt)->output_jid);
			break;
		case PCS_STOP_OUTPUT:
		case PCS_INPUT:
			/* don't care */
			break;
		case PCS_OUTPUT:
		case PCS_START_OUTPUT:
		case PCS_START_OUTPUT_BARGE:
		case PCS_START_INPUT:
		case PCS_START_INPUT_OUTPUT:
		case PCS_START_INPUT_TIMERS:
		case PCS_DONE_STOP_OUTPUT:
		case PCS_DONE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, unexpected start output error event\n", RAYO_JID(prompt));
			break;
	}
	return NULL;
}

/**
 * Handle completion event
 */
static iks *prompt_component_handle_input_complete(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	iks *presence = msg->payload;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) input complete\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state));

	switch (PROMPT_COMPONENT(prompt)->state) {
		case PCS_INPUT_OUTPUT:
			PROMPT_COMPONENT(prompt)->state = PCS_DONE_STOP_OUTPUT;
			presence = iks_copy(presence);
			iks_insert_attrib(presence, "from", RAYO_JID(prompt));
			iks_insert_attrib(presence, "to", RAYO_COMPONENT(prompt)->client_jid);
			PROMPT_COMPONENT(prompt)->complete = presence;
			rayo_component_send_stop(prompt, PROMPT_COMPONENT(prompt)->output_jid);
			break;
		case PCS_STOP_OUTPUT:
			PROMPT_COMPONENT(prompt)->state = PCS_DONE_STOP_OUTPUT;
			presence = iks_copy(presence);
			iks_insert_attrib(presence, "from", RAYO_JID(prompt));
			iks_insert_attrib(presence, "to", RAYO_COMPONENT(prompt)->client_jid);
			PROMPT_COMPONENT(prompt)->complete = presence;
			break;
		case PCS_INPUT:
			PROMPT_COMPONENT(prompt)->state = PCS_DONE;
			/* pass through */
		case PCS_DONE:
			presence = iks_copy(presence);
			iks_insert_attrib(presence, "from", RAYO_JID(prompt));
			iks_insert_attrib(presence, "to", RAYO_COMPONENT(prompt)->client_jid);
			rayo_component_send_complete_event(RAYO_COMPONENT(prompt), presence);
			break;
		case PCS_OUTPUT:
		case PCS_START_OUTPUT:
		case PCS_START_OUTPUT_BARGE:
		case PCS_START_INPUT:
		case PCS_START_INPUT_OUTPUT:
		case PCS_START_INPUT_TIMERS:
		case PCS_DONE_STOP_OUTPUT:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, unexpected start output error event\n", RAYO_JID(prompt));
			break;
	}

	return NULL;
}

/**
 * Handle completion event
 */
static iks *prompt_component_handle_output_complete(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) output complete\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state));

	switch (PROMPT_COMPONENT(prompt)->state) {
		case PCS_OUTPUT:
			PROMPT_COMPONENT(prompt)->state = PCS_START_INPUT;
			/* start input with timers enabled and barge events disabled */
			start_input(PROMPT_COMPONENT(prompt), 1, 0);
			iks_delete(PROMPT_COMPONENT(prompt)->iq);
			break;
		case PCS_START_INPUT_OUTPUT:
			PROMPT_COMPONENT(prompt)->state = PCS_INPUT;
			break;
		case PCS_INPUT_OUTPUT:
			PROMPT_COMPONENT(prompt)->state = PCS_INPUT;
			start_input_timers(PROMPT_COMPONENT(prompt));
			break;
		case PCS_STOP_OUTPUT:
			PROMPT_COMPONENT(prompt)->state = PCS_INPUT;
			start_input_timers(PROMPT_COMPONENT(prompt));
			break;
		case PCS_DONE_STOP_OUTPUT:
			if (PROMPT_COMPONENT(prompt)->complete) {
				rayo_component_send_complete_event(RAYO_COMPONENT(prompt), PROMPT_COMPONENT(prompt)->complete);
			}
			break;
		case PCS_INPUT:
		case PCS_START_OUTPUT:
		case PCS_START_OUTPUT_BARGE:
		case PCS_START_INPUT:
		case PCS_START_INPUT_TIMERS:
		case PCS_DONE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, unexpected start output error event\n", RAYO_JID(prompt));
			break;
	}

	return NULL;
}

/**
 * Start execution of prompt component
 */
static iks *start_call_prompt_component(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *iq = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	switch_memory_pool_t *pool;
	struct prompt_component *prompt_component = NULL;
	iks *prompt = iks_find(iq, "prompt");
	iks *input;
	iks *output;
	iks *cmd;

	if (!VALIDATE_RAYO_PROMPT(prompt)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Bad <prompt> attrib\n");
		return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Bad <prompt> attrib value");
	}

	output = iks_find(prompt, "output");
	if (!output) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Missing <output>\n");
		return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Missing <output>");
	}

	input = iks_find(prompt, "input");
	if (!input) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Missing <input>\n");
		return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Missing <input>");
	}

	/* create prompt component, linked to call */
	switch_core_new_memory_pool(&pool);
	prompt_component = switch_core_alloc(pool, sizeof(*prompt_component));
	rayo_component_init(RAYO_COMPONENT(prompt_component), pool, RAT_CALL_COMPONENT, "prompt", NULL, call, iks_find_attrib(iq, "from"));
	prompt_component->iq = iks_copy(iq);

	/* start output */
	if (iks_find_bool_attrib(prompt, "barge-in")) {
		prompt_component->state = PCS_START_OUTPUT_BARGE;
	} else {
		prompt_component->state = PCS_START_OUTPUT;
	}
	cmd = iks_new("iq");
	iks_insert_attrib(cmd, "from", RAYO_JID(prompt_component));
	iks_insert_attrib(cmd, "to", RAYO_JID(call));
	iks_insert_attrib(cmd, "id", iks_find_attrib(iq, "id"));
	iks_insert_attrib(cmd, "type", "set");
	output = iks_copy_within(output, iks_stack(cmd));
	iks_insert_node(cmd, output);
	RAYO_SEND(RAYO_JID(call), RAYO_MESSAGE_CREATE(prompt_component, cmd));

	return NULL;
}

/**
 * Stop execution of prompt component
 */
static iks *stop_call_prompt_component(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *reply = NULL;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) stop prompt\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state));

	switch (PROMPT_COMPONENT(prompt)->state) {
		case PCS_OUTPUT:
			/* input hasn't started yet */
			PROMPT_COMPONENT(prompt)->state = PCS_DONE_STOP_OUTPUT;
			PROMPT_COMPONENT(prompt)->complete = rayo_component_create_complete_event(RAYO_COMPONENT(prompt), COMPONENT_COMPLETE_STOP);
			rayo_component_send_stop(prompt, PROMPT_COMPONENT(prompt)->output_jid);
			break;
		case PCS_INPUT_OUTPUT:
		case PCS_INPUT:
		case PCS_STOP_OUTPUT:
			/* stopping input will trigger completion */
			rayo_component_send_stop(prompt, PROMPT_COMPONENT(prompt)->input_jid);
			break;
		case PCS_START_INPUT:
			/* stop input as soon as it starts */
			PROMPT_COMPONENT(prompt)->state = PCS_DONE;
			break;
		case PCS_DONE_STOP_OUTPUT:
		case PCS_DONE:
			/* already done */
			break;
		case PCS_START_OUTPUT:
		case PCS_START_OUTPUT_BARGE:
		case PCS_START_INPUT_OUTPUT:
		case PCS_START_INPUT_TIMERS:
			/* ref hasn't been sent yet */
			reply = iks_new_error(iq, STANZA_ERROR_UNEXPECTED_REQUEST);
			break;
	}

	if (!reply) {
		reply = iks_new_iq_result(iq);
	}
	return reply;
}

/**
 * Pass output component command
 */
static iks *forward_output_component_request(struct rayo_actor *prompt, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s) %s prompt\n",
		RAYO_JID(prompt), prompt_component_state_to_string(PROMPT_COMPONENT(prompt)->state), iks_name(iks_first_tag(iq)));

	switch (PROMPT_COMPONENT(prompt)->state) {
		case PCS_OUTPUT:
		case PCS_START_INPUT_OUTPUT:
		case PCS_INPUT_OUTPUT: {
			/* forward request to output component */
			iks_insert_attrib(iq, "from", RAYO_JID(prompt));
			iks_insert_attrib(iq, "to", RAYO_JID(PROMPT_COMPONENT(prompt)->output_jid));
			RAYO_SEND(PROMPT_COMPONENT(prompt)->output_jid, RAYO_MESSAGE_CREATE_DUP(prompt, iq));
			return NULL;
		}
		case PCS_START_INPUT_TIMERS:
		case PCS_START_OUTPUT:
		case PCS_START_OUTPUT_BARGE:
			/* ref hasn't been sent yet */
			return iks_new_error(iq, STANZA_ERROR_UNEXPECTED_REQUEST);
			break;
		case PCS_START_INPUT:
		case PCS_STOP_OUTPUT:
		case PCS_DONE_STOP_OUTPUT:
		case PCS_INPUT:
		case PCS_DONE:
			return iks_new_error_detailed(iq, STANZA_ERROR_UNEXPECTED_REQUEST, "output is finished");
	}
	return NULL;
}

/**
 * Initialize prompt component
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_prompt_component_load(void)
{
	/* Prompt is a convenience component that wraps <input> and <output> */
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_PROMPT_NS":prompt", start_call_prompt_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "set:"RAYO_EXT_NS":stop", stop_call_prompt_component);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "result:"RAYO_NS":ref", prompt_component_handle_io_start);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "error:"RAYO_OUTPUT_NS":output", prompt_component_handle_output_error);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "error:"RAYO_INPUT_NS":input", prompt_component_handle_input_error);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "error:"RAYO_INPUT_NS":start-timers", prompt_component_handle_input_start_timers_error);
	rayo_actor_event_handler_add(RAT_CALL_COMPONENT, "input", RAT_CALL_COMPONENT, "prompt", ":"RAYO_INPUT_NS":start-of-input", prompt_component_handle_input_barge);
	rayo_actor_event_handler_add(RAT_CALL_COMPONENT, "input", RAT_CALL_COMPONENT, "prompt", "unavailable:"RAYO_EXT_NS":complete", prompt_component_handle_input_complete);
	rayo_actor_event_handler_add(RAT_CALL_COMPONENT, "output", RAT_CALL_COMPONENT, "prompt", "unavailable:"RAYO_EXT_NS":complete", prompt_component_handle_output_complete);

	/* wrap output commands */
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "set:"RAYO_OUTPUT_NS":pause", forward_output_component_request);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "set:"RAYO_OUTPUT_NS":resume", forward_output_component_request);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "set:"RAYO_OUTPUT_NS":speed-up", forward_output_component_request);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "set:"RAYO_OUTPUT_NS":speed-down", forward_output_component_request);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "set:"RAYO_OUTPUT_NS":volume-up", forward_output_component_request);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "set:"RAYO_OUTPUT_NS":volume-down", forward_output_component_request);
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "prompt", "set:"RAYO_OUTPUT_NS":seek", forward_output_component_request);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Shutdown prompt component
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_prompt_component_shutdown(void)
{
	return SWITCH_STATUS_SUCCESS;
}
