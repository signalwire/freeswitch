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
#include <switch.h>
#include <switch_channel.h>

struct switch_cause_table {
	const char *name;
	switch_call_cause_t cause;
};

static struct switch_cause_table CAUSE_CHART[] = {
	{ "UNALLOCATED", SWITCH_CAUSE_UNALLOCATED },
	{ "NO_ROUTE_TRANSIT_NET", SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET },
	{ "NO_ROUTE_DESTINATION", SWITCH_CAUSE_NO_ROUTE_DESTINATION },
	{ "CHANNEL_UNACCEPTABLE", SWITCH_CAUSE_CHANNEL_UNACCEPTABLE },
	{ "CALL_AWARDED_DELIVERED", SWITCH_CAUSE_CALL_AWARDED_DELIVERED },
	{ "NORMAL_CLEARING", SWITCH_CAUSE_NORMAL_CLEARING },
	{ "USER_BUSY", SWITCH_CAUSE_USER_BUSY },
	{ "NO_USER_RESPONSE", SWITCH_CAUSE_NO_USER_RESPONSE },
	{ "NO_ANSWER", SWITCH_CAUSE_NO_ANSWER },
	{ "CALL_REJECTED", SWITCH_CAUSE_CALL_REJECTED },
	{ "NUMBER_CHANGED", SWITCH_CAUSE_NUMBER_CHANGED },
	{ "DESTINATION_OUT_OF_ORDER", SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER },
	{ "INVALID_NUMBER_FORMAT", SWITCH_CAUSE_INVALID_NUMBER_FORMAT },
	{ "FACILITY_REJECTED", SWITCH_CAUSE_FACILITY_REJECTED },
	{ "RESPONSE_TO_STATUS_ENQUIRY", SWITCH_CAUSE_RESPONSE_TO_STATUS_ENQUIRY },
	{ "NORMAL_UNSPECIFIED", SWITCH_CAUSE_NORMAL_UNSPECIFIED },
	{ "NORMAL_CIRCUIT_CONGESTION", SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION },
	{ "NETWORK_OUT_OF_ORDER", SWITCH_CAUSE_NETWORK_OUT_OF_ORDER },
	{ "NORMAL_TEMPORARY_FAILURE", SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE },
	{ "SWITCH_CONGESTION", SWITCH_CAUSE_SWITCH_CONGESTION },
	{ "ACCESS_INFO_DISCARDED", SWITCH_CAUSE_ACCESS_INFO_DISCARDED },
	{ "REQUESTED_CHAN_UNAVAIL", SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL },
	{ "PRE_EMPTED", SWITCH_CAUSE_PRE_EMPTED },
	{ "FACILITY_NOT_SUBSCRIBED", SWITCH_CAUSE_FACILITY_NOT_SUBSCRIBED },
	{ "OUTGOING_CALL_BARRED", SWITCH_CAUSE_OUTGOING_CALL_BARRED },
	{ "INCOMING_CALL_BARRED", SWITCH_CAUSE_INCOMING_CALL_BARRED },
	{ "BEARERCAPABILITY_NOTAUTH", SWITCH_CAUSE_BEARERCAPABILITY_NOTAUTH },
	{ "BEARERCAPABILITY_NOTAVAIL", SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL },
	{ "BEARERCAPABILITY_NOTIMPL", SWITCH_CAUSE_BEARERCAPABILITY_NOTIMPL },
	{ "CHAN_NOT_IMPLEMENTED", SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED },
	{ "FACILITY_NOT_IMPLEMENTED", SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED },
	{ "INVALID_CALL_REFERENCE", SWITCH_CAUSE_INVALID_CALL_REFERENCE },
	{ "INCOMPATIBLE_DESTINATION", SWITCH_CAUSE_INCOMPATIBLE_DESTINATION },
	{ "INVALID_MSG_UNSPECIFIED", SWITCH_CAUSE_INVALID_MSG_UNSPECIFIED },
	{ "MANDATORY_IE_MISSING", SWITCH_CAUSE_MANDATORY_IE_MISSING },
	{ "MESSAGE_TYPE_NONEXIST", SWITCH_CAUSE_MESSAGE_TYPE_NONEXIST },
	{ "WRONG_MESSAGE", SWITCH_CAUSE_WRONG_MESSAGE },
	{ "IE_NONEXIST", SWITCH_CAUSE_IE_NONEXIST },
	{ "INVALID_IE_CONTENTS", SWITCH_CAUSE_INVALID_IE_CONTENTS },
	{ "WRONG_CALL_STATE", SWITCH_CAUSE_WRONG_CALL_STATE },
	{ "RECOVERY_ON_TIMER_EXPIRE", SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE },
	{ "MANDATORY_IE_LENGTH_ERROR", SWITCH_CAUSE_MANDATORY_IE_LENGTH_ERROR },
	{ "PROTOCOL_ERROR", SWITCH_CAUSE_PROTOCOL_ERROR },
	{ "INTERWORKING", SWITCH_CAUSE_INTERWORKING },
	{ "CRASH", SWITCH_CAUSE_CRASH },
	{ "SYSTEM_SHUTDOWN", SWITCH_CAUSE_SYSTEM_SHUTDOWN },
	{ NULL, 0 }
};

struct switch_channel {
	char *name;
	switch_buffer_t *dtmf_buffer;
	switch_mutex_t *dtmf_mutex;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *profile_mutex;
	switch_core_session_t *session;
	switch_channel_state_t state;
	uint32_t flags;
	switch_caller_profile_t *caller_profile;
	switch_caller_profile_t *originator_caller_profile;
	switch_caller_profile_t *originatee_caller_profile;
	switch_caller_extension_t *caller_extension;
	const switch_state_handler_table_t *state_handlers[SWITCH_MAX_STATE_HANDLERS];
	int state_handler_index;
	switch_hash_t *variables;
	switch_channel_timetable_t *times;
	void *private_info;
	switch_call_cause_t hangup_cause;
	int freq;
	int bits;
	int channels;
	int ms;
	int kbps;
};


SWITCH_DECLARE(char *) switch_channel_cause2str(switch_call_cause_t cause)
{
	uint8_t x;
	char *str = "UNALLOCATED";

	for(x = 0; CAUSE_CHART[x].name; x++) {
		if (CAUSE_CHART[x].cause == cause) {
			str = (char *) CAUSE_CHART[x].name;
		}
	}

	return str;
}

SWITCH_DECLARE(switch_call_cause_t) switch_channel_str2cause(char *str)
{
	uint8_t x;
	switch_call_cause_t cause = SWITCH_CAUSE_UNALLOCATED;

	for(x = 0; CAUSE_CHART[x].name; x++) {
		if (!strcasecmp(CAUSE_CHART[x].name, str)) {
			cause = CAUSE_CHART[x].cause;
		}
	}
	return cause;
}

SWITCH_DECLARE(switch_call_cause_t) switch_channel_get_cause(switch_channel_t *channel)
{
	assert(channel != NULL);
	return channel->hangup_cause;
}

SWITCH_DECLARE(switch_channel_timetable_t *) switch_channel_get_timetable(switch_channel_t *channel)
{
	assert(channel != NULL);
	return channel->times;
}

SWITCH_DECLARE(switch_status_t) switch_channel_alloc(switch_channel_t **channel, switch_memory_pool_t *pool)
{
	assert(pool != NULL);

	if (((*channel) = switch_core_alloc(pool, sizeof(switch_channel_t))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	switch_core_hash_init(&(*channel)->variables, pool);
	switch_buffer_create(pool, &(*channel)->dtmf_buffer, 128);
	switch_mutex_init(&(*channel)->dtmf_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&(*channel)->flag_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&(*channel)->profile_mutex, SWITCH_MUTEX_NESTED, pool);
	(*channel)->hangup_cause = SWITCH_CAUSE_UNALLOCATED;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_channel_set_raw_mode(switch_channel_t *channel, int freq, int bits, int channels,
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

SWITCH_DECLARE(switch_status_t) switch_channel_get_raw_mode(switch_channel_t *channel, int *freq, int *bits, int *channels,
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


SWITCH_DECLARE(switch_size_t) switch_channel_has_dtmf(switch_channel_t *channel)
{
	switch_size_t has;

	assert(channel != NULL);
	switch_mutex_lock(channel->dtmf_mutex);
	has = switch_buffer_inuse(channel->dtmf_buffer);
	switch_mutex_unlock(channel->dtmf_mutex);

	return has;
}

SWITCH_DECLARE(switch_status_t) switch_channel_queue_dtmf(switch_channel_t *channel, char *dtmf)
{
	switch_status_t status;
	register switch_size_t len, inuse;
	switch_size_t wr = 0;
	char *p;

	assert(channel != NULL);

	switch_mutex_lock(channel->dtmf_mutex);

	inuse = switch_buffer_inuse(channel->dtmf_buffer);
	len = strlen(dtmf);
	
	if (len + inuse > switch_buffer_len(channel->dtmf_buffer)) {
		switch_buffer_toss(channel->dtmf_buffer, strlen(dtmf));
	}

	p = dtmf;
	while(wr < len && p) {
		if (is_dtmf(*p)) {
			wr++;
		} else {
			break;
		}
		p++;
	}
	status = switch_buffer_write(channel->dtmf_buffer, dtmf, wr) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_MEMERR;
	switch_mutex_unlock(channel->dtmf_mutex);

	return status;
}


SWITCH_DECLARE(switch_size_t) switch_channel_dequeue_dtmf(switch_channel_t *channel, char *dtmf, switch_size_t len)
{
	switch_size_t bytes;
	switch_event_t *event;

	assert(channel != NULL);

	switch_mutex_lock(channel->dtmf_mutex);
	if ((bytes = switch_buffer_read(channel->dtmf_buffer, dtmf, len)) > 0) {
		*(dtmf + bytes) = '\0';
	}
	switch_mutex_unlock(channel->dtmf_mutex);

	if (bytes && switch_event_create(&event, SWITCH_EVENT_DTMF) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "DTMF-String", dtmf);
		switch_event_fire(&event);
	}

	return bytes;

}

SWITCH_DECLARE(switch_status_t) switch_channel_init(switch_channel_t *channel,
												  switch_core_session_t *session,
												  switch_channel_state_t state, uint32_t flags)
{
	assert(channel != NULL);
	channel->state = state;
	channel->flags = flags;
	channel->session = session;
	switch_channel_set_raw_mode(channel, 8000, 16, 1, 20, 8);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(char *) switch_channel_get_variable(switch_channel_t *channel, char *varname)
{
	assert(channel != NULL);
	return switch_core_hash_find(channel->variables, varname);
}

SWITCH_DECLARE(switch_hash_index_t *) switch_channel_variable_first(switch_channel_t *channel, switch_memory_pool_t *pool)
{
	assert(channel != NULL);
	return switch_hash_first(pool, channel->variables);
}

SWITCH_DECLARE(switch_status_t) switch_channel_set_private(switch_channel_t *channel, void *private_info)
{
	assert(channel != NULL);
	channel->private_info = private_info;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void *) switch_channel_get_private(switch_channel_t *channel)
{
	assert(channel != NULL);
	return channel->private_info;
}

SWITCH_DECLARE(switch_status_t) switch_channel_set_name(switch_channel_t *channel, char *name)
{
	assert(channel != NULL);
	channel->name = NULL;
	if (name) {
		char *uuid = switch_core_session_get_uuid(channel->session);
		channel->name = switch_core_session_strdup(channel->session, name);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "New Chan %s [%s]\n", name, uuid);
	}
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(char *) switch_channel_get_name(switch_channel_t *channel)
{
	assert(channel != NULL);
	return channel->name ? channel->name : "N/A";
}

SWITCH_DECLARE(switch_status_t) switch_channel_set_variable(switch_channel_t *channel, char *varname, char *value)
{
	assert(channel != NULL);

	if (varname) {
		switch_core_hash_delete(channel->variables, varname);
		if (value) {
			switch_core_hash_insert_dup(channel->variables, varname, switch_core_session_strdup(channel->session, value));
		} else {
			switch_core_hash_delete(channel->variables, varname);
		}
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(int) switch_channel_test_flag(switch_channel_t *channel, switch_channel_flag_t flags)
{
	assert(channel != NULL);
	return switch_test_flag(channel, flags) ? 1 : 0;
}

SWITCH_DECLARE(void) switch_channel_set_flag(switch_channel_t *channel, switch_channel_flag_t flags)
{
	assert(channel != NULL);
	switch_set_flag_locked(channel, flags);
}

SWITCH_DECLARE(void) switch_channel_clear_flag(switch_channel_t *channel, switch_channel_flag_t flags)
{
	assert(channel != NULL);
	switch_clear_flag_locked(channel, flags);
}

SWITCH_DECLARE(switch_channel_state_t) switch_channel_get_state(switch_channel_t *channel)
{
	assert(channel != NULL);
	return channel->state;
}

SWITCH_DECLARE(unsigned int) switch_channel_ready(switch_channel_t *channel)
{
	assert(channel != NULL);
	return (channel->state > CS_RING && channel->state < CS_HANGUP) ? 1 : 0;
}

static const char *state_names[] = {
	"CS_NEW",
	"CS_INIT",
	"CS_RING",
	"CS_TRANSMIT",
	"CS_EXECUTE",
	"CS_LOOPBACK",
	"CS_HOLD",
	"CS_HANGUP",
	"CS_DONE",
	NULL
};

SWITCH_DECLARE(const char *) switch_channel_state_name(switch_channel_state_t state)
{
	return state_names[state];
}


SWITCH_DECLARE(switch_channel_state_t) switch_channel_name_state(char *name)
{
	uint32_t x = 0;
	for(x = 0; state_names[x]; x++) {
		if (!strcasecmp(state_names[x], name)) {
			return (switch_channel_state_t) x;
		}
	}

	return CS_DONE;
}

SWITCH_DECLARE(switch_channel_state_t) switch_channel_perform_set_state(switch_channel_t *channel,
																	  const char *file,
																	  const char *func,
																	  int line,
																	  switch_channel_state_t state)
{
	switch_channel_state_t last_state;
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
		case CS_HOLD:
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
		case CS_HOLD:
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
		case CS_HOLD:
			ok++;
		default:
			break;
		}
		break;

	case CS_HOLD:
		switch (state) {
		case CS_LOOPBACK:
		case CS_RING:
		case CS_EXECUTE:
		case CS_TRANSMIT:
			ok++;
		default:
			break;
		}
		break;

	case CS_RING:
		switch_clear_flag(channel, CF_TRANSFER);
		switch (state) {
		case CS_LOOPBACK:
		case CS_EXECUTE:
		case CS_TRANSMIT:
		case CS_HOLD:
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
		case CS_HOLD:
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
		
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_DEBUG, "%s State Change %s -> %s\n", 
						  channel->name,
						  state_names[last_state], 
						  state_names[state]);
		channel->state = state;

		if (state == CS_HANGUP && channel->hangup_cause == SWITCH_CAUSE_UNALLOCATED) {
			channel->hangup_cause = SWITCH_CAUSE_NORMAL_CLEARING;
		}
		if (state < CS_HANGUP) {
			switch_event_t *event;
			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_STATE) == SWITCH_STATUS_SUCCESS) {
				if (state == CS_RING) {
					switch_channel_event_set_data(channel, event);
				} else {
					char state_num[25];
					snprintf(state_num, sizeof(state_num), "%d", channel->state);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-State", (char *) switch_channel_state_name(channel->state));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-State-Number", (char *) state_num);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Name", switch_channel_get_name(channel));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(channel->session));
				}
				switch_event_fire(&event);
			}
		}
		
		if (state < CS_DONE) {
			switch_core_session_signal_state_change(channel->session);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_WARNING, "%s Invalid State Change %s -> %s\n", 
						  channel->name,
						  state_names[last_state],
						  state_names[state]);

		//we won't tolerate an invalid state change so we can make sure we are as robust as a nice cup of dark coffee!
		if (channel->state < CS_HANGUP) {
			// not cool lets crash this bad boy and figure out wtf is going on
			assert(0);
		}
	}
	return channel->state;
}

SWITCH_DECLARE(void) switch_channel_event_set_data(switch_channel_t *channel, switch_event_t *event)
{
	switch_caller_profile_t *caller_profile, *originator_caller_profile, *originatee_caller_profile;
	switch_hash_index_t *hi;
	switch_codec_t *codec;
	void *val;
	const void *var;
	char state_num[25];

	caller_profile = switch_channel_get_caller_profile(channel);
	originator_caller_profile = switch_channel_get_originator_caller_profile(channel);
	originatee_caller_profile = switch_channel_get_originatee_caller_profile(channel);

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-State", (char *) switch_channel_state_name(channel->state));
	snprintf(state_num, sizeof(state_num), "%d", channel->state);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-State-Number", (char *) state_num);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Name", switch_channel_get_name(channel));
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(channel->session));
	
	if ((codec = switch_core_session_get_read_codec(channel->session))) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Read-Codec-Name", codec->implementation->iananame);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Read-Codec-Rate", "%u", codec->implementation->samples_per_second);
	}
	
	if ((codec = switch_core_session_get_write_codec(channel->session))) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Write-Codec-Name", codec->implementation->iananame);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Write-Codec-Rate", "%u", codec->implementation->samples_per_second);
	}

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
		switch_hash_this(hi, &var, NULL, &val);
		snprintf(buf, sizeof(buf), "variable_%s", (char *) var);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, buf, (char *) val);
	}



}

SWITCH_DECLARE(void) switch_channel_set_caller_profile(switch_channel_t *channel, switch_caller_profile_t *caller_profile)
{
	switch_channel_timetable_t *times;

	assert(channel != NULL);
	assert(channel->session != NULL);
	switch_mutex_lock(channel->profile_mutex);

	if (!caller_profile->uuid) {
		caller_profile->uuid = switch_core_session_strdup(channel->session, switch_core_session_get_uuid(channel->session));
	}

	if (!caller_profile->chan_name) {
		caller_profile->chan_name = switch_core_session_strdup(channel->session, channel->name);
	}

	if (!caller_profile->context) {
		caller_profile->chan_name = switch_core_session_strdup(channel->session, "default");
	}

	if (!channel->caller_profile) {
		switch_event_t *event;

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_CREATE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}
	} 
	
	if ((times = (switch_channel_timetable_t *) switch_core_session_alloc(channel->session, sizeof(*times)))) {
		times->next = channel->times;
		channel->times = times;
	}
	channel->times->created = switch_time_now();

	caller_profile->next = channel->caller_profile;
	channel->caller_profile = caller_profile;

	switch_mutex_unlock(channel->profile_mutex);
}

SWITCH_DECLARE(switch_caller_profile_t *) switch_channel_get_caller_profile(switch_channel_t *channel)
{
	switch_caller_profile_t *profile;
	assert(channel != NULL);

	switch_mutex_lock(channel->profile_mutex);
	profile = channel->caller_profile;
	switch_mutex_unlock(channel->profile_mutex);

	return profile;
}

SWITCH_DECLARE(void) switch_channel_set_originator_caller_profile(switch_channel_t *channel,
																  switch_caller_profile_t *caller_profile)
{
	assert(channel != NULL);
	switch_mutex_lock(channel->profile_mutex);
	caller_profile->next = channel->originator_caller_profile;
	channel->originator_caller_profile = caller_profile;
	switch_mutex_unlock(channel->profile_mutex);
}

SWITCH_DECLARE(void) switch_channel_set_originatee_caller_profile(switch_channel_t *channel,
																  switch_caller_profile_t *caller_profile)
{
	assert(channel != NULL);
	switch_mutex_lock(channel->profile_mutex);
	caller_profile->next = channel->originatee_caller_profile;
	channel->originatee_caller_profile = caller_profile;
	switch_mutex_unlock(channel->profile_mutex);
}

SWITCH_DECLARE(switch_caller_profile_t *) switch_channel_get_originator_caller_profile(switch_channel_t *channel)
{
	switch_caller_profile_t *profile;
	assert(channel != NULL);

	switch_mutex_lock(channel->profile_mutex);
	profile = channel->originator_caller_profile;
	switch_mutex_unlock(channel->profile_mutex);

	return profile;
}

SWITCH_DECLARE(char *) switch_channel_get_uuid(switch_channel_t *channel)
{
	assert(channel != NULL);
	assert(channel->session != NULL);
	return switch_core_session_get_uuid(channel->session);
}

SWITCH_DECLARE(switch_caller_profile_t *) switch_channel_get_originatee_caller_profile(switch_channel_t *channel)
{
	switch_caller_profile_t *profile;
	assert(channel != NULL);

	switch_mutex_lock(channel->profile_mutex);
	profile = channel->originatee_caller_profile;
	switch_mutex_unlock(channel->profile_mutex);

	return profile;
}

SWITCH_DECLARE(int) switch_channel_add_state_handler(switch_channel_t *channel,
													 const switch_state_handler_table_t *state_handler)
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

SWITCH_DECLARE(const switch_state_handler_table_t *) switch_channel_get_state_handler(switch_channel_t *channel, int index)
{
	assert(channel != NULL);

	if (index > SWITCH_MAX_STATE_HANDLERS || index > channel->state_handler_index) {
		return NULL;
	}

	return channel->state_handlers[index];
}

SWITCH_DECLARE(void) switch_channel_clear_state_handler(switch_channel_t *channel, const switch_state_handler_table_t *state_handler)
{
	int index, i = 0;
	const switch_state_handler_table_t *new_handlers[SWITCH_MAX_STATE_HANDLERS] = {0};

	assert(channel != NULL);


	for (index = 0; index < channel->state_handler_index; index++) {
		if (channel->state_handlers[index] != state_handler) {
			new_handlers[i++] = channel->state_handlers[index];
		}
	}
	for (index = 0; index < SWITCH_MAX_STATE_HANDLERS; index++) {
		channel->state_handlers[index] = NULL;
	}
	for (index = 0; index < i; index++) {
		channel->state_handlers[index] = new_handlers[i];
	}

}

SWITCH_DECLARE(void) switch_channel_set_caller_extension(switch_channel_t *channel,
														 switch_caller_extension_t *caller_extension)
{
	assert(channel != NULL);

	switch_mutex_lock(channel->profile_mutex);
	caller_extension->next = channel->caller_extension;
	channel->caller_extension = caller_extension;
	switch_mutex_unlock(channel->profile_mutex);
}


SWITCH_DECLARE(switch_caller_extension_t *) switch_channel_get_caller_extension(switch_channel_t *channel)
{

	assert(channel != NULL);
	return channel->caller_extension;
}


SWITCH_DECLARE(switch_channel_state_t) switch_channel_perform_hangup(switch_channel_t *channel, 
																   const char *file,
																   const char *func,
																   int line,
																   switch_call_cause_t hangup_cause)
{
	assert(channel != NULL);

	if (channel->times && !channel->times->hungup) {
		channel->times->hungup = switch_time_now();
	}

	if (channel->state < CS_HANGUP) {
		switch_event_t *event;
		switch_channel_state_t last_state = channel->state;

		channel->state = CS_HANGUP;
		channel->hangup_cause = hangup_cause;
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_NOTICE, "Hangup %s [%s] [%s]\n", 
						  channel->name,
						  state_names[last_state], switch_channel_cause2str(channel->hangup_cause));
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_STATE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}

		switch_core_session_kill_channel(channel->session, SWITCH_SIG_KILL);
		switch_core_session_signal_state_change(channel->session);
	}
	return channel->state;
}

SWITCH_DECLARE(switch_status_t) switch_channel_perform_pre_answer(switch_channel_t *channel,
																const char *file,
																const char *func,
																int line)
{
	switch_core_session_message_t msg;
	char *uuid = switch_core_session_get_uuid(channel->session);
	switch_status_t status;

	assert(channel != NULL);

	if (channel->state >= CS_HANGUP) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_test_flag(channel, CF_ANSWERED)) {
		return SWITCH_STATUS_SUCCESS;
	}

	msg.message_id = SWITCH_MESSAGE_INDICATE_PROGRESS;
	msg.from = channel->name;
	status = switch_core_session_message_send(uuid, &msg);

	if (status == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_NOTICE, "Pre-Answer %s!\n", channel->name);
		switch_channel_set_flag(channel, CF_EARLY_MEDIA);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_channel_perform_answer(switch_channel_t *channel,
																const char *file,
																const char *func,
																int line)
{
	assert(channel != NULL);

	if (channel->state >= CS_HANGUP) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_test_flag(channel, CF_ANSWERED)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_core_session_answer_channel(channel->session) == SWITCH_STATUS_SUCCESS) {
		switch_event_t *event;

		channel->times->answered = switch_time_now();
		switch_channel_set_flag(channel, CF_ANSWERED);
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_NOTICE, "Answer %s!\n", channel->name);
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_ANSWER) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}
