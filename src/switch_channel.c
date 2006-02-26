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
	char *name;
	switch_buffer *dtmf_buffer;
	switch_mutex_t *dtmf_mutex;
	switch_core_session *session;
	switch_channel_state state;
	switch_channel_flag flags;
	switch_caller_profile *caller_profile;
	switch_caller_profile *originator_caller_profile;
	switch_caller_profile *originatee_caller_profile;
	switch_caller_extension *caller_extension;
	const struct switch_state_handler_table *state_handlers[SWITCH_MAX_STATE_HANDLERS];
	int state_handler_index;
	switch_hash *variables;
	switch_channel_timetable_t times;
	void *private_info;
	int freq;
	int bits;
	int channels;
	int ms;
	int kbps;
};

SWITCH_DECLARE(switch_channel_timetable_t *) switch_channel_get_timetable(switch_channel *channel)
{
	return &channel->times;
}

SWITCH_DECLARE(switch_status) switch_channel_alloc(switch_channel **channel, switch_memory_pool *pool)
{
	assert(pool != NULL);

	if (((*channel) = switch_core_alloc(pool, sizeof(switch_channel))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	switch_core_hash_init(&(*channel)->variables, pool);
	switch_buffer_create(pool, &(*channel)->dtmf_buffer, 128);
	switch_mutex_init(&(*channel)->dtmf_mutex, SWITCH_MUTEX_NESTED, pool);
	(*channel)->times.created = switch_time_now();

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_channel_set_raw_mode(switch_channel *channel, int freq, int bits, int channels,
														  int ms, int kbps)
{

	assert(channel != NULL);

	channel->freq = freq;
	channel->bits = bits;
	channel->channels = channels;
	channel->ms = ms;
	channel->kbps = kbps;


	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_channel_get_raw_mode(switch_channel *channel, int *freq, int *bits, int *channels,
														  int *ms, int *kbps)
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


SWITCH_DECLARE(size_t) switch_channel_has_dtmf(switch_channel *channel)
{
	size_t has;

	assert(channel != NULL);
	switch_mutex_lock(channel->dtmf_mutex);
	has = switch_buffer_inuse(channel->dtmf_buffer);
	switch_mutex_unlock(channel->dtmf_mutex);

	return has;
}

SWITCH_DECLARE(switch_status) switch_channel_queue_dtmf(switch_channel *channel, char *dtmf)
{
	switch_status status;

	assert(channel != NULL);

	switch_mutex_lock(channel->dtmf_mutex);
	if (switch_buffer_inuse(channel->dtmf_buffer) + strlen(dtmf) > (size_t) switch_buffer_len(channel->dtmf_buffer)) {
		switch_buffer_toss(channel->dtmf_buffer, strlen(dtmf));
	}

	status = switch_buffer_write(channel->dtmf_buffer, dtmf, strlen(dtmf));
	switch_mutex_unlock(channel->dtmf_mutex);

	return status;
}


SWITCH_DECLARE(int) switch_channel_dequeue_dtmf(switch_channel *channel, char *dtmf, size_t len)
{
	int bytes;
	switch_event *event;

	assert(channel != NULL);

	switch_mutex_lock(channel->dtmf_mutex);
	if ((bytes = switch_buffer_read(channel->dtmf_buffer, dtmf, len)) > 0) {
		*(dtmf + bytes) = '\0';
	}
	switch_mutex_unlock(channel->dtmf_mutex);

	if (bytes && switch_event_create(&event, SWITCH_EVENT_CHANNEL_ANSWER) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "DTMF-String", dtmf);
		switch_event_fire(&event);
	}

	return bytes;

}

SWITCH_DECLARE(switch_status) switch_channel_init(switch_channel *channel,
												  switch_core_session *session,
												  switch_channel_state state, switch_channel_flag flags)
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

SWITCH_DECLARE(switch_status) switch_channel_set_private(switch_channel *channel, void *private_info)
{
	assert(channel != NULL);
	channel->private_info = private_info;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void *) switch_channel_get_private(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->private_info;
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

	switch_core_hash_insert_dup(channel->variables, varname, switch_core_session_strdup(channel->session, value));

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

static const char *state_names[] = {
	"CS_NEW",
	"CS_INIT",
	"CS_RING",
	"CS_TRANSMIT",
	"CS_EXECUTE",
	"CS_LOOPBACK",
	"CS_HANGUP",
	"CS_DONE"
};

SWITCH_DECLARE(const char *) switch_channel_state_name(switch_channel_state state)
{
	return state_names[state];
}

SWITCH_DECLARE(switch_channel_state) switch_channel_set_state(switch_channel *channel, switch_channel_state state)
{
	switch_channel_state last_state;
	int ok = 0;


	assert(channel != NULL);
	last_state = channel->state;

	if (last_state == state) {
		return state;
	}

	if (last_state >= CS_HANGUP && state < last_state) {
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

	switch (last_state) {
	case CS_NEW:
		switch (state) {
		default:
			ok++;
			break;
		}
		break;

	case CS_INIT:
		switch (state) {
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
		switch (state) {
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
		switch (state) {
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
		switch (state) {
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
		switch (state) {
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
		switch (state) {
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
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s State Change %s -> %s\n", channel->name,
							  state_names[last_state], state_names[state]);
		channel->state = state;
		switch_core_session_signal_state_change(channel->session);
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s Invalid State Change %s -> %s\n", channel->name,
							  state_names[last_state], state_names[state]);

		//we won't tolerate an invalid state change so we can make sure we are as robust as a nice cup of dark coffee!
		if (channel->state < CS_HANGUP) {
			// not cool lets crash this bad boy and figure out wtf is going on
			assert(0);
		}
	}
	return channel->state;
}

SWITCH_DECLARE(void) switch_channel_event_set_data(switch_channel *channel, switch_event *event)
{
	switch_caller_profile *caller_profile, *originator_caller_profile, *originatee_caller_profile;
	switch_hash_index_t *hi;
	void *val;
	const void *var;

	caller_profile = switch_channel_get_caller_profile(channel);
	originator_caller_profile = switch_channel_get_originator_caller_profile(channel);
	originatee_caller_profile = switch_channel_get_originatee_caller_profile(channel);

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-State",
							(char *) switch_channel_state_name(channel->state));
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Name", switch_channel_get_name(channel));
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(channel->session));


	/* Index Caller's Profile */
	if (caller_profile) {
		switch_caller_profile_event_set_data(caller_profile, "Caller", event);
	}

	/* Index Originator's Profile */
	if (originator_caller_profile) {
		switch_caller_profile_event_set_data(originator_caller_profile, "Originator", event);
	}

	/* Index Originatee's Profile */
	if (originatee_caller_profile) {
		switch_caller_profile_event_set_data(originatee_caller_profile, "Originatee", event);
	}

	/* Index Variables */
	for (hi = switch_hash_first(switch_core_session_get_pool(channel->session), channel->variables); hi;
		 hi = switch_hash_next(hi)) {
		char buf[1024];
		switch_event_subclass *subclass;
		switch_hash_this(hi, &var, NULL, &val);
		subclass = val;
		snprintf(buf, sizeof(buf), "variable_%s", (char *) var);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, buf, (char *) val);
	}



}

SWITCH_DECLARE(void) switch_channel_set_caller_profile(switch_channel *channel, switch_caller_profile *caller_profile)
{
	assert(channel != NULL);
	assert(channel->session != NULL);

	if (!caller_profile->uuid) {
		caller_profile->uuid = switch_core_session_strdup(channel->session, switch_core_session_get_uuid(channel->session));
	}

	if (!caller_profile->chan_name) {
		caller_profile->chan_name = switch_core_session_strdup(channel->session, channel->name);
	}

	channel->caller_profile = caller_profile;
}

SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_caller_profile(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->caller_profile;
}

SWITCH_DECLARE(void) switch_channel_set_originator_caller_profile(switch_channel *channel,
																  switch_caller_profile *caller_profile)
{
	assert(channel != NULL);
	channel->originator_caller_profile = caller_profile;
}

SWITCH_DECLARE(void) switch_channel_set_originatee_caller_profile(switch_channel *channel,
																  switch_caller_profile *caller_profile)
{
	assert(channel != NULL);
	channel->originatee_caller_profile = caller_profile;
}

SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_originator_caller_profile(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->originator_caller_profile;
}

SWITCH_DECLARE(char *) switch_channel_get_uuid(switch_channel *channel)
{
	assert(channel != NULL);
	assert(channel->session != NULL);
	return switch_core_session_get_uuid(channel->session);
}

SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_originatee_caller_profile(switch_channel *channel)
{
	assert(channel != NULL);
	return channel->originatee_caller_profile;
}

SWITCH_DECLARE(int) switch_channel_add_state_handler(switch_channel *channel,
													 const switch_state_handler_table *state_handler)
{
	int index;

	assert(channel != NULL);
	index = channel->state_handler_index++;

	if (channel->state_handler_index >= SWITCH_MAX_STATE_HANDLERS) {
		return -1;
	}

	channel->state_handlers[index] = state_handler;
	return index;
}

SWITCH_DECLARE(const switch_state_handler_table *) switch_channel_get_state_handler(switch_channel *channel, int index)
{
	assert(channel != NULL);

	if (index > SWITCH_MAX_STATE_HANDLERS || index > channel->state_handler_index) {
		return NULL;
	}

	return channel->state_handlers[index];
}

SWITCH_DECLARE(void) switch_channel_set_caller_extension(switch_channel *channel,
														 switch_caller_extension *caller_extension)
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
		channel->times.hungup = switch_time_now();
		channel->state = CS_HANGUP;
		switch_core_session_signal_state_change(channel->session);
	}
	return channel->state;
}

SWITCH_DECLARE(switch_status) switch_channel_answer(switch_channel *channel)
{
	assert(channel != NULL);


	if (switch_core_session_answer_channel(channel->session) == SWITCH_STATUS_SUCCESS) {
		switch_event *event;

		channel->times.answered = switch_time_now();
		switch_channel_set_flag(channel, CF_ANSWERED);
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Answer %s!\n", channel->name);
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_ANSWER) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}
