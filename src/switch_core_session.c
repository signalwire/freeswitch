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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_session.c -- Main Core Library (session routines)
 *
 */

#include "switch.h"
#include "switch_core.h"
#include "private/switch_core_pvt.h"

static struct {
	switch_memory_pool_t *memory_pool;
	switch_hash_t *session_table;
	uint32_t session_count;
	uint32_t session_limit;
	switch_size_t session_id;
} session_manager;

#ifdef SWITCH_DEBUG_RWLOCKS
SWITCH_DECLARE(switch_core_session_t *) switch_core_session_perform_locate(const char *uuid_str, const char *file, const char *func, int line)
#else
SWITCH_DECLARE(switch_core_session_t *) switch_core_session_locate(const char *uuid_str)
#endif
{
	switch_core_session_t *session = NULL;

	if (uuid_str) {
		switch_mutex_lock(runtime.throttle_mutex);
		if ((session = switch_core_hash_find(session_manager.session_table, uuid_str))) {
			/* Acquire a read lock on the session */
#ifdef SWITCH_DEBUG_RWLOCKS
			if (switch_core_session_perform_read_lock(session, file, func, line) != SWITCH_STATUS_SUCCESS) {
#else
			if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
#endif
				/* not available, forget it */
				session = NULL;
			}
		}
		switch_mutex_unlock(runtime.throttle_mutex);
	}

	/* if its not NULL, now it's up to you to rwunlock this */
	return session;
}


SWITCH_DECLARE(void) switch_core_session_hupall_matching_var(const char *var_name, const char *var_val, switch_call_cause_t cause)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;

	switch_mutex_lock(runtime.throttle_mutex);
	for (hi = switch_hash_first(NULL, session_manager.session_table); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			const char *this_val;
			session = (switch_core_session_t *) val;
			switch_core_session_read_lock(session);
			if ((this_val = switch_channel_get_variable(session->channel, var_name)) && (!strcmp(this_val, var_val))) {
				switch_channel_hangup(switch_core_session_get_channel(session), cause);
			}
			switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.throttle_mutex);
}	

SWITCH_DECLARE(void) switch_core_session_hupall_endpoint(const switch_endpoint_interface_t *endpoint_interface, switch_call_cause_t cause)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;

	switch_mutex_lock(runtime.throttle_mutex);
	for (hi = switch_hash_first(NULL, session_manager.session_table); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			session = (switch_core_session_t *) val;
			switch_core_session_read_lock(session);
			if (session->endpoint_interface == endpoint_interface) {
				switch_channel_hangup(switch_core_session_get_channel(session), cause);
			}
			switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.throttle_mutex);
}	

SWITCH_DECLARE(void) switch_core_session_hupall(switch_call_cause_t cause)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;
	uint32_t loops = 0;

	switch_mutex_lock(runtime.throttle_mutex);
	for (hi = switch_hash_first(NULL, session_manager.session_table); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			session = (switch_core_session_t *) val;
			switch_core_session_read_lock(session);
			switch_channel_hangup(switch_core_session_get_channel(session), cause);
			switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.throttle_mutex);

	while (session_manager.session_count > 0) {
		switch_yield(100000);
		if (++loops == 100) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Giving up with %d session%s remaining\n",
							  session_manager.session_count, session_manager.session_count == 1 ? "" : "s");
			break;
		}
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_session_message_send(const char *uuid_str, switch_core_session_message_t *message)
{
	switch_core_session_t *session = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(runtime.throttle_mutex);
	if ((session = switch_core_hash_find(session_manager.session_table, uuid_str)) != 0) {
		/* Acquire a read lock on the session or forget it the channel is dead */
		if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
			if (switch_channel_get_state(session->channel) < CS_HANGUP) {
				status = switch_core_session_receive_message(session, message);
			}
			switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.throttle_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_event_send(const char *uuid_str, switch_event_t **event)
{
	switch_core_session_t *session = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(runtime.throttle_mutex);
	if ((session = switch_core_hash_find(session_manager.session_table, uuid_str)) != 0) {
		/* Acquire a read lock on the session or forget it the channel is dead */
		if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
			if (switch_channel_get_state(session->channel) < CS_HANGUP) {
				status = switch_core_session_queue_event(session, event);
			}
			switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.throttle_mutex);

	return status;
}


SWITCH_DECLARE(void *) switch_core_session_get_private(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	return session->private_info;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_set_private(switch_core_session_t *session, void *private_info)
{
	switch_assert(session != NULL);
	session->private_info = private_info;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) switch_core_session_add_stream(switch_core_session_t *session, void *private_info)
{
	session->streams[session->stream_count++] = private_info;
	return session->stream_count - 1;
}

SWITCH_DECLARE(void *) switch_core_session_get_stream(switch_core_session_t *session, int index)
{
	return session->streams[index];
}


SWITCH_DECLARE(int) switch_core_session_get_stream_count(switch_core_session_t *session)
{
	return session->stream_count;
}

SWITCH_DECLARE(switch_call_cause_t) switch_core_session_resurrect_channel(const char *endpoint_name,
																		  switch_core_session_t **new_session, switch_memory_pool_t **pool, void *data)
{
	const switch_endpoint_interface_t *endpoint_interface;

	if ((endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate channel type %s\n", endpoint_name);
		return SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED;
	}

	return endpoint_interface->io_routines->resurrect_session(new_session, pool, data);
}

SWITCH_DECLARE(switch_call_cause_t) switch_core_session_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
																		 const char *endpoint_name,
																		 switch_caller_profile_t *caller_profile,
																		 switch_core_session_t **new_session,
																		 switch_memory_pool_t **pool, switch_originate_flag_t flags)
{
	switch_io_event_hook_outgoing_channel_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	const switch_endpoint_interface_t *endpoint_interface;
	switch_channel_t *channel = NULL;
	switch_caller_profile_t *outgoing_profile = caller_profile;
	switch_call_cause_t cause = SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL;
	const char *forwardvar;
	int forwardval = 70;

	if ((endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate channel type %s\n", endpoint_name);
		return SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED;
	}

	if (!endpoint_interface->io_routines->outgoing_channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate outgoing channel interface for %s\n", endpoint_name);
		return SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED;
	}

	if (session) {
		channel = switch_core_session_get_channel(session);

		switch_assert(channel != NULL);

		forwardvar = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE);
		if (!switch_strlen_zero(forwardvar)) {
			forwardval = atoi(forwardvar) - 1;
		}
		if (forwardval <= 0) {
			return SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR;
		}

		if (caller_profile) {
			const char *ecaller_id_name = NULL, *ecaller_id_number = NULL;
			
			if (!(flags & SOF_NO_EFFECTIVE_CID_NAME)) {
				ecaller_id_name = switch_channel_get_variable(channel, "effective_caller_id_name");
			}

			if (!(flags & SOF_NO_EFFECTIVE_CID_NUM)) {
				ecaller_id_number = switch_channel_get_variable(channel, "effective_caller_id_number");
			}

			if (ecaller_id_name || ecaller_id_number) {
				outgoing_profile = switch_caller_profile_clone(session, caller_profile);

				if (ecaller_id_name) {
					outgoing_profile->caller_id_name = ecaller_id_name;
				}
				if (ecaller_id_number) {
					outgoing_profile->caller_id_number = ecaller_id_number;
				}
			}
		}
		if (!outgoing_profile) {
			outgoing_profile = switch_channel_get_caller_profile(channel);
		}
	}

	if ((cause =
		 endpoint_interface->io_routines->outgoing_channel(session, var_event, outgoing_profile, new_session, pool, flags)) != SWITCH_CAUSE_SUCCESS) {
		return cause;
	}

	if (session) {
		for (ptr = session->event_hooks.outgoing_channel; ptr; ptr = ptr->next) {
			if ((status = ptr->outgoing_channel(session, var_event, caller_profile, *new_session, flags)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}

	if (!*new_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "outgoing method for endpoint: [%s] returned: [%s] but there is no new session!\n",
						  endpoint_name, switch_channel_cause2str(cause));
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	} else {
		switch_caller_profile_t *profile = NULL, *peer_profile = NULL, *cloned_profile = NULL;
		switch_event_t *event;
		switch_channel_t *peer_channel = switch_core_session_get_channel(*new_session);

		switch_assert(peer_channel);

		peer_profile = switch_channel_get_caller_profile(peer_channel);

		if (channel) {
			const char *export_vars, *val;
			switch_codec_t *read_codec = switch_core_session_get_read_codec(session);
			const char *max_forwards = switch_core_session_sprintf(session, "%d", forwardval);

			switch_channel_set_variable(peer_channel, SWITCH_MAX_FORWARDS_VARIABLE, max_forwards);

			profile = switch_channel_get_caller_profile(channel);

			if (read_codec) {
				char tmp[80];
				switch_codec2str(read_codec, tmp, sizeof(tmp));
				switch_channel_set_variable(peer_channel, SWITCH_ORIGINATOR_CODEC_VARIABLE, tmp);
			}

			switch_channel_set_variable(peer_channel, SWITCH_ORIGINATOR_VARIABLE, switch_core_session_get_uuid(session));
			switch_channel_set_variable(peer_channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(session));
			switch_channel_set_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(*new_session));

			/* A comma (,) separated list of variable names that should ne propagated from originator to originatee */
			if ((export_vars = switch_channel_get_variable(channel, SWITCH_EXPORT_VARS_VARIABLE))) {
				char *cptmp = switch_core_session_strdup(session, export_vars);
				int argc;
				char *argv[256];

				if ((argc = switch_separate_string(cptmp, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
					int x;

					for (x = 0; x < argc; x++) {
						const char *vval;
						if ((vval = switch_channel_get_variable(channel, argv[x]))) {
							char *vvar = argv[x];
							if (!strncasecmp(vvar, "nolocal:", 8)) {
								vvar += 8;
							}
							switch_channel_set_variable(peer_channel, vvar, vval);
						}
					}
				}
			}

			if ((val = switch_channel_get_variable(channel, SWITCH_PROCESS_CDR_VARIABLE))) {
				switch_channel_set_variable(peer_channel, SWITCH_PROCESS_CDR_VARIABLE, val);
			}

			if ((val = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE))) {
				switch_channel_set_variable(peer_channel, SWITCH_B_SDP_VARIABLE, val);
			}

			if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
				switch_channel_set_flag(peer_channel, CF_PROXY_MODE);
			}

			if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
				switch_channel_set_flag(peer_channel, CF_PROXY_MEDIA);
				if (switch_channel_test_flag(channel, CF_VIDEO)) {
					switch_channel_set_flag(peer_channel, CF_VIDEO);
				}
			}

			if (profile) {
				if ((cloned_profile = switch_caller_profile_clone(*new_session, profile)) != 0) {
					switch_channel_set_originator_caller_profile(peer_channel, cloned_profile);
				}
			}

			if (peer_profile) {
				if ((cloned_profile = switch_caller_profile_clone(session, peer_profile)) != 0) {
					switch_channel_set_originatee_caller_profile(channel, cloned_profile);
				}
			}
		}

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_OUTGOING) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(peer_channel, event);
			switch_event_fire(&event);
		}
	}

	return cause;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_receive_message(switch_core_session_t *session, switch_core_session_message_t *message)
{
	switch_io_event_hook_receive_message_t *ptr;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(session != NULL);

	if (switch_channel_get_state(session->channel) >= CS_HANGUP) {
		return SWITCH_STATUS_FALSE;
	}

	if ((status = switch_core_session_read_lock(session)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	switch_core_session_signal_lock(session);

	if (session->endpoint_interface->io_routines->receive_message) {
		status = session->endpoint_interface->io_routines->receive_message(session, message);
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		for (ptr = session->event_hooks.receive_message; ptr; ptr = ptr->next) {
			if ((status = ptr->receive_message(session, message)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}

	switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);

	switch_core_session_signal_unlock(session);

	switch_core_session_rwunlock(session);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_pass_indication(switch_core_session_t *session, switch_core_session_message_types_t indication)
{
	switch_core_session_message_t msg = { 0 };
	switch_core_session_t *other_session;
	const char *uuid;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE)) && (other_session = switch_core_session_locate(uuid))) {
		msg.message_id = indication;
		msg.from = __FILE__;
		status = switch_core_session_receive_message(other_session, &msg);
		switch_core_session_rwunlock(other_session);
	} else {
		status = SWITCH_STATUS_FALSE;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_indication(switch_core_session_t *session, switch_core_session_message_types_t indication)
{
	switch_core_session_message_t *msg;

	if ((msg = malloc(sizeof(*msg)))) {
		memset(msg, 0, sizeof(*msg));
		msg->message_id = indication;
		msg->from = __FILE__;
		switch_set_flag(msg, SCSMF_DYNAMIC);
		switch_core_session_queue_message(session, msg);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_message(switch_core_session_t *session, switch_core_session_message_t *message)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(session != NULL);

	if (session->message_queue) {
		if (switch_queue_trypush(session->message_queue, message) == SWITCH_STATUS_SUCCESS) {
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_message(switch_core_session_t *session, switch_core_session_message_t **message)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;

	switch_assert(session != NULL);

	if (session->message_queue) {
		if ((status = (switch_status_t) switch_queue_trypop(session->message_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*message = (switch_core_session_message_t *) pop;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_flush_message(switch_core_session_t *session)
{
	switch_core_session_message_t *message;

	if (switch_core_session_dequeue_message(session, &message) == SWITCH_STATUS_SUCCESS) {
		if (switch_test_flag(message, SCSMF_DYNAMIC)) {
			switch_safe_free(message);
		} else {
			message = NULL;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_receive_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_io_event_hook_receive_event_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(session != NULL);

	/* Acquire a read lock on the session or forget it the channel is dead */
	if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
		if (switch_channel_get_state(session->channel) < CS_HANGUP) {
			if (session->endpoint_interface->io_routines->receive_event) {
				status = session->endpoint_interface->io_routines->receive_event(session, *event);
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				for (ptr = session->event_hooks.receive_event; ptr; ptr = ptr->next) {
					if ((status = ptr->receive_event(session, *event)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				}
			}

			if (status == SWITCH_STATUS_BREAK) {
				status = SWITCH_STATUS_SUCCESS;
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				switch_event_destroy(event);
			}
		}
		switch_core_session_rwunlock(session);
	}

	switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(session != NULL);

	if (session->event_queue) {
		if (switch_queue_trypush(session->event_queue, *event) == SWITCH_STATUS_SUCCESS) {
			*event = NULL;
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

SWITCH_DECLARE(uint32_t) switch_core_session_event_count(switch_core_session_t *session)
{
	if (session->event_queue) {
		return switch_queue_size(session->event_queue);
	}

	return 0;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;

	switch_assert(session != NULL);

	if (session->event_queue) {
		if ((status = (switch_status_t) switch_queue_trypop(session->event_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*event = (switch_event_t *) pop;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_private_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(session != NULL);

	if (session->private_event_queue) {
		(*event)->event_id = SWITCH_EVENT_PRIVATE_COMMAND;
		if (switch_queue_trypush(session->private_event_queue, *event) == SWITCH_STATUS_SUCCESS) {
			*event = NULL;
			switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

SWITCH_DECLARE(uint32_t) switch_core_session_private_event_count(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!switch_channel_test_flag(channel, CF_EVENT_LOCK) && session->private_event_queue) {
		return switch_queue_size(session->private_event_queue);
	}

	return 0;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_private_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_channel_test_flag(channel, CF_EVENT_LOCK)) {
		return status;
	}

	if (session->private_event_queue) {
		if ((status = (switch_status_t) switch_queue_trypop(session->private_event_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*event = (switch_event_t *) pop;
		}
	}

	return status;
}

SWITCH_DECLARE(uint32_t) switch_core_session_flush_private_events(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	int x = 0;
	void *pop;

	if (session->private_event_queue) {
		while ((status = (switch_status_t) switch_queue_trypop(session->private_event_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			x++;
		}
	}

	return x;
}

SWITCH_DECLARE(void) switch_core_session_reset(switch_core_session_t *session, switch_bool_t flush_dtmf)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_size_t has;

	switch_core_session_set_read_codec(session, NULL);
	
	/* clear resamplers */
	switch_mutex_lock(session->resample_mutex);
	switch_resample_destroy(&session->read_resampler);
	switch_resample_destroy(&session->write_resampler);
	switch_mutex_unlock(session->resample_mutex);
	/* clear indications */
	switch_core_session_flush_message(session);
	/* wipe theese, they will be recreated if need be */
	switch_buffer_destroy(&session->raw_read_buffer);
	switch_buffer_destroy(&session->raw_write_buffer);

	if (flush_dtmf) {
		while ((has = switch_channel_has_dtmf(channel))) {
			switch_channel_flush_dtmf(channel);
		}
	}

	switch_clear_flag(session, SSF_WARN_TRANSCODE);
	switch_ivr_deactivate_unicast(session);
	switch_channel_clear_flag(channel, CF_BREAK);
}


SWITCH_DECLARE(switch_channel_t *) switch_core_session_get_channel(switch_core_session_t *session)
{
	switch_assert(session->channel);
	return session->channel;
}

SWITCH_DECLARE(void) switch_core_session_signal_state_change(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_io_event_hook_state_change_t *ptr;

	/* If trylock fails the signal is already awake so we needn't bother */
	if (switch_mutex_trylock(session->mutex) == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_signal(session->cond);
		switch_mutex_unlock(session->mutex);
	}

	if (session->endpoint_interface->io_routines->state_change) {
		status = session->endpoint_interface->io_routines->state_change(session);
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		for (ptr = session->event_hooks.state_change; ptr; ptr = ptr->next) {
			if ((status = ptr->state_change(session)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}
	switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);
}

SWITCH_DECLARE(unsigned int) switch_core_session_running(switch_core_session_t *session)
{
	return session->thread_running;
}


SWITCH_DECLARE(void) switch_core_session_perform_destroy(switch_core_session_t **session, const char *file, const char *func, int line)
{
	switch_memory_pool_t *pool;
	switch_event_t *event;
	const switch_endpoint_interface_t *endpoint_interface = (*session)->endpoint_interface;


	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_NOTICE, "Close Channel %s [%s]\n",
					  switch_channel_get_name((*session)->channel), switch_channel_state_name(switch_channel_get_state((*session)->channel)));


	switch_core_session_reset(*session, TRUE);
	
	switch_core_media_bug_remove_all(*session);
	switch_ivr_deactivate_unicast(*session);

	switch_scheduler_del_task_group((*session)->uuid_str);

	switch_mutex_lock(runtime.throttle_mutex);
	switch_core_hash_delete(session_manager.session_table, (*session)->uuid_str);
	if (session_manager.session_count) {
		session_manager.session_count--;
	}
	switch_mutex_unlock(runtime.throttle_mutex);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DESTROY) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data((*session)->channel, event);
		switch_event_fire(&event);
	}


	switch_buffer_destroy(&(*session)->raw_read_buffer);
	switch_buffer_destroy(&(*session)->raw_write_buffer);
	switch_ivr_clear_speech_cache(*session);
	switch_channel_uninit((*session)->channel);

	pool = (*session)->pool;
	//#ifndef NDEBUG
	//memset(*session, 0, sizeof(switch_core_session_t));
	//#endif
	*session = NULL;
	switch_core_destroy_memory_pool(&pool);

	switch_thread_rwlock_unlock(endpoint_interface->rwlock);

}

static void *SWITCH_THREAD_FUNC switch_core_session_thread(switch_thread_t *thread, void *obj)
{
	switch_core_session_t *session = obj;
	switch_event_t *event;
	char *event_str = NULL;
	const char *val;

	session->thread = thread;
	
	switch_core_session_run(session);
	switch_core_media_bug_remove_all(session);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Session %" SWITCH_SIZE_T_FMT " (%s) Locked, Waiting on external entities\n",
					  session->id, switch_channel_get_name(session->channel));
	switch_core_session_write_lock(session);
	switch_set_flag(session, SSF_DESTROYED);

	if ((val = switch_channel_get_variable(session->channel, "memory_debug")) && switch_true(val)) {
		if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(session->channel, event);
			switch_event_serialize(event, &event_str, SWITCH_FALSE);
			switch_assert(event_str);
			switch_core_memory_pool_tag(switch_core_session_get_pool(session), switch_core_session_strdup(session, event_str));
			free(event_str);
			switch_event_destroy(&event);
		}
	}

	switch_core_session_rwunlock(session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Session %" SWITCH_SIZE_T_FMT " (%s) Ended\n",
					  session->id, switch_channel_get_name(session->channel));
	switch_core_session_destroy(&session);
	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_thread_launch(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr;;

	switch_threadattr_create(&thd_attr, session->pool);
	switch_threadattr_detach_set(thd_attr, 1);

	switch_mutex_lock(session->mutex);

	if (!session->thread_running) {
		session->thread_running = 1;
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		if (switch_thread_create(&thread, thd_attr, switch_core_session_thread, session, session->pool) == SWITCH_STATUS_SUCCESS) {
			status = SWITCH_STATUS_SUCCESS;
		} else {
			session->thread_running = 0;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot create thread!\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot double-launch thread!\n");
	}

	switch_mutex_unlock(session->mutex);

	return status;
}

SWITCH_DECLARE(void) switch_core_session_launch_thread(switch_core_session_t *session, switch_thread_start_t func, void *obj)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_threadattr_create(&thd_attr, session->pool);
	switch_threadattr_detach_set(thd_attr, 1);

	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, func, obj, session->pool);

}

SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request(const switch_endpoint_interface_t
																	*endpoint_interface, switch_memory_pool_t **pool)
{
	switch_memory_pool_t *usepool;
	switch_core_session_t *session;
	switch_uuid_t uuid;
	uint32_t count = 0;
	int32_t sps = 0;

	if (!switch_core_ready() || endpoint_interface == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The system cannot create any sessions at this time.\n");
		return NULL;
	}

	switch_mutex_lock(runtime.throttle_mutex);
	count = session_manager.session_count;
	sps = --runtime.sps;
	switch_mutex_unlock(runtime.throttle_mutex);

	if (sps <= 0) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Throttle Error!\n");
		return NULL;
	}

	if ((count + 1) > session_manager.session_limit) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Over Session Limit!\n");
		return NULL;
	}

	switch_thread_rwlock_rdlock(endpoint_interface->rwlock);

	if (pool && *pool) {
		usepool = *pool;
		*pool = NULL;
	} else {
		switch_core_new_memory_pool(&usepool);
	}

	session = switch_core_alloc(usepool, sizeof(*session));
	session->pool = usepool;

	if (switch_channel_alloc(&session->channel, session->pool) != SWITCH_STATUS_SUCCESS) {
		abort();
	}

	switch_channel_init(session->channel, session, CS_NEW, 0);

	/* The session *IS* the pool you may not alter it because you have no idea how
	   its all private it will be passed to the thread run function */

	switch_uuid_get(&uuid);
	switch_uuid_format(session->uuid_str, &uuid);
	session->endpoint_interface = endpoint_interface;
	session->raw_write_frame.data = session->raw_write_buf;
	session->raw_write_frame.buflen = sizeof(session->raw_write_buf);
	session->raw_read_frame.data = session->raw_read_buf;
	session->raw_read_frame.buflen = sizeof(session->raw_read_buf);


	session->enc_write_frame.data = session->enc_write_buf;
	session->enc_write_frame.buflen = sizeof(session->enc_write_buf);
	session->enc_read_frame.data = session->enc_read_buf;
	session->enc_read_frame.buflen = sizeof(session->enc_read_buf);

	switch_mutex_init(&session->mutex, SWITCH_MUTEX_NESTED, session->pool);
	switch_mutex_init(&session->resample_mutex, SWITCH_MUTEX_NESTED, session->pool);
	switch_mutex_init(&session->signal_mutex, SWITCH_MUTEX_NESTED, session->pool);
	switch_thread_rwlock_create(&session->bug_rwlock, session->pool);
	switch_thread_cond_create(&session->cond, session->pool);
	switch_thread_rwlock_create(&session->rwlock, session->pool);
	switch_queue_create(&session->message_queue, SWITCH_MESSAGE_QUEUE_LEN, session->pool);
	switch_queue_create(&session->event_queue, SWITCH_EVENT_QUEUE_LEN, session->pool);
	switch_queue_create(&session->private_event_queue, SWITCH_EVENT_QUEUE_LEN, session->pool);
	switch_mutex_lock(runtime.throttle_mutex);
	session->id = session_manager.session_id++;
	switch_core_hash_insert(session_manager.session_table, session->uuid_str, session);
	session_manager.session_count++;
	switch_mutex_unlock(runtime.throttle_mutex);

	return session;
}

SWITCH_DECLARE(uint32_t) switch_core_session_count(void)
{
	return session_manager.session_count;
}

SWITCH_DECLARE(switch_size_t) switch_core_session_id(void)
{
	return session_manager.session_id;
}


SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request_by_name(const char *endpoint_name, switch_memory_pool_t **pool)
{
	const switch_endpoint_interface_t *endpoint_interface;

	if ((endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate channel type %s\n", endpoint_name);
		return NULL;
	}

	return switch_core_session_request(endpoint_interface, pool);
}

#ifndef SWITCH_PREFIX_DIR
#define SWITCH_PREFIX_DIR "."
#endif

SWITCH_DECLARE(uint8_t) switch_core_session_compare(switch_core_session_t *a, switch_core_session_t *b)
{
	switch_assert(a != NULL);
	switch_assert(b != NULL);

	return (uint8_t) (a->endpoint_interface == b->endpoint_interface);
}

SWITCH_DECLARE(uint8_t) switch_core_session_check_interface(switch_core_session_t *session, const switch_endpoint_interface_t *endpoint_interface)
{
	switch_assert(session != NULL);
	switch_assert(endpoint_interface != NULL);

	return (uint8_t) (session->endpoint_interface == endpoint_interface);
}

SWITCH_DECLARE(char *) switch_core_session_get_uuid(switch_core_session_t *session)
{
	return session->uuid_str;
}


SWITCH_DECLARE(uint32_t) switch_core_session_limit(uint32_t new_limit)
{
	if (new_limit) {
		session_manager.session_limit = new_limit;
	}

	return session_manager.session_limit;
}

SWITCH_DECLARE(uint32_t) switch_core_sessions_per_second(uint32_t new_limit)
{
	if (new_limit) {
		runtime.sps_total = new_limit;
	}

	return runtime.sps_total;
}

void switch_core_session_init(switch_memory_pool_t *pool)
{
	memset(&session_manager, 0, sizeof(session_manager));
	session_manager.session_limit = 1000;
	session_manager.session_id = 1;
	session_manager.memory_pool = pool;
	switch_core_hash_init(&session_manager.session_table, session_manager.memory_pool);
}

void switch_core_session_uninit(void)
{
	switch_core_hash_destroy(&session_manager.session_table);
}

SWITCH_DECLARE(switch_app_log_t *) switch_core_session_get_app_log(switch_core_session_t *session)
{
	return session->app_log;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_execute_application(switch_core_session_t *session, const char *app, const char *arg)
{
	const switch_application_interface_t *application_interface;
	char *expanded = NULL;
	const char *var;

	if ((application_interface = switch_loadable_module_get_application_interface(app)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Application %s\n", app);
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}

	if (!application_interface->application_function) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Function for %s\n", app);
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_test_flag(session->channel, CF_PROXY_MODE) && !switch_test_flag(application_interface, SAF_SUPPORT_NOMEDIA)) {
		switch_ivr_media(session->uuid_str, SMF_NONE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Application %s Requires media on channel %s!\n",
						  app, switch_channel_get_name(session->channel));
	} else if (!switch_test_flag(application_interface, SAF_SUPPORT_NOMEDIA) && !switch_channel_media_ready(session->channel)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Application %s Requires media! pre_answering channel %s\n",
						  app, switch_channel_get_name(session->channel));
		if (switch_channel_pre_answer(session->channel) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Well, that didn't work very well did it? ...\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	if (arg && (expanded = switch_channel_expand_variables(session->channel, arg)) != arg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Expanded String %s(%s)\n", switch_channel_get_name(session->channel), app, expanded);
	}

	if ((var = switch_channel_get_variable(session->channel, "verbose_presence")) && switch_true(var)) {
		char *myarg = NULL;
		if (expanded) {
			myarg = switch_mprintf("%s(%s)", app, expanded);
		} else if (!switch_strlen_zero(arg)) {
			myarg = switch_mprintf("%s(%s)", app, arg);
		} else {
			myarg = switch_mprintf("%s", app);
		}
		if (myarg) {
			switch_channel_presence(session->channel, "unknown", myarg, NULL);
			switch_safe_free(myarg);
		}
	}

	switch_core_session_exec(session, application_interface, expanded);

	if (expanded != arg) {
		switch_safe_free(expanded);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_exec(switch_core_session_t *session,
														 const switch_application_interface_t *application_interface, const char *arg)
{
	switch_app_log_t *log, *lp;
	switch_event_t *event;
	const char *var;

	if (!arg) {
		arg = "";
	}

	if (!(var = switch_channel_get_variable(session->channel, SWITCH_DISABLE_APP_LOG_VARIABLE)) || (!(switch_true(var)))) {
		log = switch_core_session_alloc(session, sizeof(*log));

		log->app = switch_core_session_strdup(session, application_interface->interface_name);
		log->arg = switch_core_session_strdup(session, arg);

		for (lp = session->app_log; lp && lp->next; lp = lp->next);

		if (lp) {
			lp->next = log;
		} else {
			session->app_log = log;
		}
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(session->channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", application_interface->interface_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-Data", arg);
		switch_event_fire(&event);
	}

	switch_channel_clear_flag(session->channel, CF_BREAK);

	switch_assert(application_interface->application_function);

	switch_channel_set_variable(session->channel, SWITCH_CURRENT_APPLICATION_VARIABLE, application_interface->interface_name);

	switch_thread_rwlock_rdlock(application_interface->rwlock);
	application_interface->application_function(session, arg);
	switch_thread_rwlock_unlock(application_interface->rwlock);
	
	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(session->channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", application_interface->interface_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-Data", arg);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_execute_exten(switch_core_session_t *session, const char *exten, const char *dialplan,
																  const char *context)
{
	char *dp[25];
	char *dpstr;
	int argc, x, count = 0;
	switch_caller_profile_t *profile, *new_profile, *pp = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_dialplan_interface_t *dialplan_interface = NULL;
	switch_caller_extension_t *extension = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(profile = switch_channel_get_caller_profile(channel))) {
		return SWITCH_STATUS_FALSE;
	}

	if (session->stack_count > SWITCH_MAX_STACKS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error %s too many stacked extensions\n", switch_channel_get_name(session->channel));
		return SWITCH_STATUS_FALSE;
	}

	session->stack_count++;

	new_profile = switch_caller_profile_clone(session, profile);
	new_profile->destination_number = switch_core_session_strdup(session, exten);

	if (!switch_strlen_zero(dialplan)) {
		new_profile->dialplan = switch_core_session_strdup(session, dialplan);
	}

	if (!switch_strlen_zero(context)) {
		new_profile->context = switch_core_session_strdup(session, context);
	}

	if (!(dpstr = switch_core_session_strdup(session, new_profile->dialplan))) {
		abort();
	}

	argc = switch_separate_string(dpstr, ',', dp, (sizeof(dp) / sizeof(dp[0])));
	for (x = 0; x < argc; x++) {
		char *dpname = dp[x];
		char *dparg = NULL;

		if (dpname) {
			if ((dparg = strchr(dpname, ':'))) {
				*dparg++ = '\0';
			}
		} else {
			continue;
		}

		if (!(dialplan_interface = switch_loadable_module_get_dialplan_interface(dpname))) {
			continue;
		}

		count++;

		if ((extension = dialplan_interface->hunt_function(session, dparg, new_profile)) != 0) {
			break;
		}
	}

	if (!extension) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	new_profile->caller_extension = extension;

	if (profile->caller_extension) {
		for (pp = profile->caller_extension->children; pp && pp->next; pp = pp->next);

		if (pp) {
			pp->next = new_profile;
		} else {
			profile->caller_extension->children = new_profile;
		}
	}

	while (switch_channel_ready(channel) && extension->current_application) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Execute %s(%s)\n",
						  extension->current_application->application_name, switch_str_nil(extension->current_application->application_data));

		if (switch_core_session_execute_application(session,
													extension->current_application->application_name,
													extension->current_application->application_data) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}

		extension->current_application = extension->current_application->next;
	}

  done:
	session->stack_count--;
	return status;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
