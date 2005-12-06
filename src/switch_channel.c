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
 * switch_channel.c -- Media Channel Interface
 *
 */
#include <switch_channel.h>

struct switch_channel {
	time_t initiated;
	char *name;
	switch_buffer *dtmf_buffer;
	switch_core_session *session;
	switch_channel_state state;
	switch_channel_flag flags;
	switch_caller_profile *caller_profile;
	switch_caller_profile *originator_caller_profile;
	switch_caller_extension *caller_extension;
	const struct switch_event_handler_table *event_handlers;
	switch_hash *variables;
	void *private;
	int freq;
	int bits;
	int channels;
	int ms;
	int kbps;
};



SWITCH_DECLARE(switch_status) switch_channel_alloc(switch_channel **channel, switch_memory_pool *pool)
{
	assert(pool != NULL);

	if (!((*channel) = switch_core_alloc(pool, sizeof(switch_channel)))) {
		return SWITCH_STATUS_MEMERR;
	}

	switch_core_hash_init(&(*channel)->variables, pool);
	switch_buffer_create(pool, &(*channel)->dtmf_buffer, 128);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_channel_set_raw_mode (switch_channel *channel, int freq, int bits, int channels, int ms, int kbps)
{

	assert(channel != NULL);

	channel->freq = freq;
	channel->bits = bits;
	channel->channels = channels;
	channel->ms = ms;
	channel->kbps = kbps;


	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_channel_get_raw_mode (switch_channel *channel, int *freq, int *bits, int *channels, int *ms, int *kbps)
{
	if (freq) {
		*freq = channel->freq;
	}
	if (bits) {
		*bits = channel->bits;
	}
	if (channels) {
		*channels = channel->channels;
	}
	if (ms) {
		*ms = channel->ms;
	}
	if (kbps) {
		*kbps = channel->kbps;
	}

	return SWITCH_STATUS_SUCCESS;

}


SWITCH_DECLARE(int) switch_channel_has_dtmf(switch_channel *channel)
{

	assert(channel != NULL);
	return switch_buffer_inuse(channel->dtmf_buffer);
}

SWITCH_DECLARE(switch_status) switch_channel_queue_dtmf(switch_channel *channel, char *dtmf)
{
	assert(channel != NULL);

	if (switch_buffer_inuse(channel->dtmf_buffer) + strlen(dtmf) > (size_t)switch_buffer_len(channel->dtmf_buffer)) {
		switch_buffer_toss(channel->dtmf_buffer, strlen(dtmf));
	}
	return switch_buffer_write(channel->dtmf_buffer, dtmf, strlen(dtmf));

}


SWITCH_DECLARE(int) switch_channel_dequeue_dtmf(switch_channel *channel, char *dtmf, size_t len)
{
	int bytes;

	assert(channel != NULL);
	if ((bytes = switch_buffer_read(channel->dtmf_buffer, dtmf, len)) > 0) {
		*(dtmf + bytes) = '\0';
	}
	return bytes;

}

SWITCH_DECLARE(switch_status) switch_channel_init(switch_channel *channel,
								switch_core_session *session,
								switch_channel_state state,
								switch_channel_flag flags)
{
	assert(channel != NULL);
	channel->state = state;
	channel->flags = flags;
	channel->session = session;
	switch_channel_set_raw_mode(channel, 8000, 16, 1, 20, 8);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(char *) switch_channel_get_variable(switch_channel *channel, char *varname)
{
	return switch_core_hash_find(channel->variables, varname);
}

SWITCH_DECLARE(switch_status) switch_channel_set_private(switch_channel *channel, void *private)
{
	assert(channel != NULL);
	channel->private = private;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void *) switch_channel_get_private(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->private;
}

SWITCH_DECLARE(switch_status) switch_channel_set_name(switch_channel *channel, char *name)
{
	assert(channel != NULL);
	channel->name = NULL;
	if (name) {
		channel->name = switch_core_session_strdup(channel->session, name);
	}
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(char *) switch_channel_get_name(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->name;
}

SWITCH_DECLARE(switch_status) switch_channel_set_variable(switch_channel *channel, char *varname, char *value)
{
	assert(channel != NULL);
	switch_core_hash_delete(channel->variables, varname);

	if (value) {
		switch_core_hash_insert(channel->variables, varname, switch_core_session_strdup(channel->session, value));
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_channel_test_flag(switch_channel *channel, int flags)
{
	return switch_test_flag(channel, flags);
}

SWITCH_DECLARE(switch_status) switch_channel_set_flag(switch_channel *channel, int flags)
{
	switch_set_flag(channel, flags);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_channel_clear_flag(switch_channel *channel, int flags)
{
	switch_set_flag(channel, flags);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_channel_state) switch_channel_get_state(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->state;
}

SWITCH_DECLARE(switch_channel_state) switch_channel_set_state(switch_channel *channel, switch_channel_state state)
{
	switch_channel_state last_state;
	int ok = 0;
	const char *state_names[] = {
		"CS_NEW",
		"CS_INIT",
		"CS_RING",
		"CS_TRANSMIT",
		"CS_EXECUTE",
		"CS_LOOPBACK",
		"CS_HANGUP",
		"CS_DONE"
	};

	assert(channel != NULL);
	last_state = channel->state;

	if (last_state == state) {
		return state;
	}

	if (last_state >= CS_HANGUP) {
		return last_state;
	}

	/* STUB for more dev
	case CS_INIT:
		switch(state) {

		case CS_NEW:
		case CS_INIT:
		case CS_LOOPBACK:
		case CS_TRANSMIT:
		case CS_RING:
		case CS_EXECUTE:
		case CS_HANGUP:
		case CS_DONE:

		default:
			break;
		}
		break;
	*/

	switch(last_state) {
	case CS_NEW:
		switch(state) {
		default:
			ok++;
			break;
		}
		break;

	case CS_INIT:
		switch(state) {
		case CS_LOOPBACK:
		case CS_TRANSMIT:
		case CS_RING:
		case CS_EXECUTE:
		case CS_HANGUP:
			ok++;
		default:
			break;
		}
		break;

	case CS_LOOPBACK:
		switch(state) {
		case CS_TRANSMIT:
		case CS_RING:
		case CS_EXECUTE:
		case CS_HANGUP:
			ok++;
		default:
			break;
		}
		break;

	case CS_TRANSMIT:
		switch(state) {
		case CS_LOOPBACK:
		case CS_RING:
		case CS_EXECUTE:
		case CS_HANGUP:
			ok++;
		default:
			break;
		}
		break;

	case CS_RING:
		switch(state) {
		case CS_LOOPBACK:
		case CS_EXECUTE:
		case CS_HANGUP:
		case CS_TRANSMIT:
			ok++;
		default:
			break;
		}
		break;

	case CS_EXECUTE:
		switch(state) {
		case CS_LOOPBACK:
		case CS_TRANSMIT:
		case CS_RING:
		case CS_HANGUP:
			ok++;
		default:
			break;
		}
		break;

	case CS_HANGUP:
		switch(state) {
		case CS_DONE:
			ok++;
		default:
			break;
		}
		break;

	default:
		break;

	}


	if (ok) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s State Change %s -> %s\n", channel->name, state_names[last_state], state_names[state]);
		channel->state = state;
		pbx_core_session_signal_state_change(channel->session);
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s Invalid State Change %s -> %s\n", channel->name, state_names[last_state], state_names[state]);

		//we won't tolerate an invalid state change so we can make sure we are as robust as a nice cup of dark coffee!
		if (channel->state < CS_HANGUP) {
			// not cool lets crash this bad boy and figure out wtf is going on
			assert(0);
		}
	}
	return channel->state;
}

SWITCH_DECLARE(void) switch_channel_set_caller_profile(switch_channel *channel, switch_caller_profile *caller_profile)
{
	assert(channel != NULL);
	channel->caller_profile = caller_profile;
}

SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_caller_profile(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->caller_profile;
}

SWITCH_DECLARE(void) switch_channel_set_originator_caller_profile(switch_channel *channel, switch_caller_profile *caller_profile)
{
	assert(channel != NULL);
	channel->originator_caller_profile = caller_profile;
}

SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_originator_caller_profile(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->originator_caller_profile;
}

SWITCH_DECLARE(void) switch_channel_set_event_handlers(switch_channel *channel, const struct switch_event_handler_table *event_handlers)
{
	assert(channel != NULL);
	channel->event_handlers = event_handlers;
}

SWITCH_DECLARE(const struct switch_event_handler_table *) switch_channel_get_event_handlers(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->event_handlers;
}

SWITCH_DECLARE(void) switch_channel_set_caller_extension(switch_channel *channel, switch_caller_extension *caller_extension)
{
	assert(channel != NULL);
	channel->caller_extension = caller_extension;
}


SWITCH_DECLARE(switch_caller_extension *) switch_channel_get_caller_extension(switch_channel *channel)
{

	assert(channel != NULL);
	return channel->caller_extension;
}


SWITCH_DECLARE(switch_status) switch_channel_hangup(switch_channel *channel)
{
	assert(channel != NULL);
	if (channel->state < CS_HANGUP) {
		channel->state = CS_HANGUP;
		pbx_core_session_signal_state_change(channel->session);	
	}
	return channel->state;
}

SWITCH_DECLARE(switch_status) switch_channel_answer(switch_channel *channel)
{
	assert(channel != NULL);


	if (switch_core_session_answer_channel(channel->session) == SWITCH_STATUS_SUCCESS) {
		switch_channel_set_flag(channel, CF_ANSWERED);
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Answer!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}
