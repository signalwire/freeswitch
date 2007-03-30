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
#include <switch.h>
#include "private/switch_core.h"

static struct {
	switch_memory_pool_t *memory_pool;
	switch_hash_t *session_table;
	switch_mutex_t *session_table_mutex;
	uint32_t session_count;
	uint32_t session_limit;
	uint32_t session_id;
} runtime;


#ifdef SWITCH_DEBUG_RWLOCKS
SWITCH_DECLARE(switch_core_session_t *) switch_core_session_perform_locate(char *uuid_str, const char *file, const char *func, int line)
#else
SWITCH_DECLARE(switch_core_session_t *) switch_core_session_locate(char *uuid_str)
#endif
{
	switch_core_session_t *session = NULL;

	if (uuid_str) {
		switch_mutex_lock(runtime.session_table_mutex);
		if ((session = switch_core_hash_find(runtime.session_table, uuid_str))) {
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
		switch_mutex_unlock(runtime.session_table_mutex);
	}

	/* if its not NULL, now it's up to you to rwunlock this */
	return session;
}

SWITCH_DECLARE(void) switch_core_session_hupall(switch_call_cause_t cause)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;
	switch_channel_t *channel;
	uint32_t loops = 0;

	switch_mutex_lock(runtime.session_table_mutex);
	for (hi = switch_hash_first(runtime.memory_pool, runtime.session_table); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			session = (switch_core_session_t *) val;
			channel = switch_core_session_get_channel(session);
			switch_channel_hangup(channel, cause);
			switch_core_session_kill_channel(session, SWITCH_SIG_KILL);
		}
	}
	switch_mutex_unlock(runtime.session_table_mutex);

	while (runtime.session_count > 0) {
		switch_yield(100000);
		if (++loops == 100) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Giving up with %d session%s remaining\n",
							  runtime.session_count, runtime.session_count == 1 ? "" : "s");
			break;
		}
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_session_message_send(char *uuid_str, switch_core_session_message_t *message)
{
	switch_core_session_t *session = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(runtime.session_table_mutex);
	if ((session = switch_core_hash_find(runtime.session_table, uuid_str)) != 0) {
		/* Acquire a read lock on the session or forget it the channel is dead */
		if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
			if (switch_channel_get_state(session->channel) < CS_HANGUP) {
				status = switch_core_session_receive_message(session, message);
			}
			switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.session_table_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_event_send(char *uuid_str, switch_event_t **event)
{
	switch_core_session_t *session = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(runtime.session_table_mutex);
	if ((session = switch_core_hash_find(runtime.session_table, uuid_str)) != 0) {
		/* Acquire a read lock on the session or forget it the channel is dead */
		if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
			if (switch_channel_get_state(session->channel) < CS_HANGUP) {
				status = switch_core_session_queue_event(session, event);
			}
			switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.session_table_mutex);

	return status;
}


SWITCH_DECLARE(void *) switch_core_session_get_private(switch_core_session_t *session)
{
	assert(session != NULL);
	return session->private_info;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_set_private(switch_core_session_t *session, void *private_info)
{
	assert(session != NULL);
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

SWITCH_DECLARE(switch_call_cause_t) switch_core_session_outgoing_channel(switch_core_session_t *session,
																		 char *endpoint_name,
																		 switch_caller_profile_t *caller_profile,
																		 switch_core_session_t **new_session, switch_memory_pool_t ** pool)
{
	switch_io_event_hook_outgoing_channel_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	const switch_endpoint_interface_t *endpoint_interface;
	switch_channel_t *channel = NULL;
	switch_caller_profile_t *outgoing_profile = caller_profile;
	switch_call_cause_t cause = SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL;

	if ((endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate channel type %s\n", endpoint_name);
		return SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED;
	}

	if (endpoint_interface->io_routines->outgoing_channel) {
		if (session) {
			channel = switch_core_session_get_channel(session);
			if (caller_profile) {
				char *ecaller_id_name = NULL, *ecaller_id_number = NULL;

				ecaller_id_name = switch_channel_get_variable(channel, "effective_caller_id_name");
				ecaller_id_number = switch_channel_get_variable(channel, "effective_caller_id_number");

				if (ecaller_id_name || ecaller_id_number) {
					if (!ecaller_id_name) {
						ecaller_id_name = caller_profile->caller_id_name;
					}
					if (!ecaller_id_number) {
						ecaller_id_number = caller_profile->caller_id_number;
					}
					outgoing_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																 caller_profile->username,
																 caller_profile->dialplan,
																 ecaller_id_name,
																 ecaller_id_number,
																 caller_profile->network_addr,
																 caller_profile->ani,
																 caller_profile->aniii,
																 caller_profile->rdnis,
																 caller_profile->source, caller_profile->context, caller_profile->destination_number);
					outgoing_profile->flags = caller_profile->flags;
				}
			}
			if (!outgoing_profile) {
				outgoing_profile = switch_channel_get_caller_profile(channel);
			}
		}

		if ((cause = endpoint_interface->io_routines->outgoing_channel(session, outgoing_profile, new_session, pool)) == SWITCH_CAUSE_SUCCESS) {
			if (session) {
				for (ptr = session->event_hooks.outgoing_channel; ptr; ptr = ptr->next) {
					if ((status = ptr->outgoing_channel(session, caller_profile, *new_session)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				}
			}
		} else {
			return cause;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate outgoing channel interface for %s\n", endpoint_name);
		return SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED;
	}

	if (*new_session) {
		switch_caller_profile_t *profile = NULL, *peer_profile = NULL, *cloned_profile = NULL;
		switch_event_t *event;
		switch_channel_t *peer_channel = switch_core_session_get_channel(*new_session);


		if (session && channel) {
			profile = switch_channel_get_caller_profile(channel);
		}
		if (peer_channel) {
			peer_profile = switch_channel_get_caller_profile(peer_channel);
		}

		if (channel && peer_channel) {
			char *export_vars, *val;
			switch_codec_t *read_codec = switch_core_session_get_read_codec(session);

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
						char *val;
						if ((val = switch_channel_get_variable(channel, argv[x]))) {
							switch_channel_set_variable(peer_channel, argv[x], val);
						}
					}
				}
			}

			if ((val = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE))) {
				switch_channel_set_variable(peer_channel, SWITCH_B_SDP_VARIABLE, val);
			}

			if ((val = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE))) {
				switch_channel_set_variable(peer_channel, SWITCH_MAX_FORWARDS_VARIABLE, val);
			}

			if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
				switch_channel_set_flag(peer_channel, CF_NOMEDIA);
			}

			if (profile) {
				if ((cloned_profile = switch_caller_profile_clone(*new_session, profile)) != 0) {
					switch_channel_set_originator_caller_profile(peer_channel, cloned_profile);
				}
			}

			if (peer_profile) {
				if (session && (cloned_profile = switch_caller_profile_clone(session, peer_profile)) != 0) {
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

SWITCH_DECLARE(switch_status_t) switch_core_session_answer_channel(switch_core_session_t *session)
{
	switch_io_event_hook_answer_channel_t *ptr;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	assert(session != NULL);

	if (session->endpoint_interface->io_routines->answer_channel) {
		status = session->endpoint_interface->io_routines->answer_channel(session);
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		for (ptr = session->event_hooks.answer_channel; ptr; ptr = ptr->next) {
			if ((status = ptr->answer_channel(session)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}


	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_receive_message(switch_core_session_t *session, switch_core_session_message_t *message)
{
	switch_io_event_hook_receive_message_t *ptr;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	assert(session != NULL);

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
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_indication(switch_core_session_t *session, switch_core_session_message_types_t indication)
{
	switch_core_session_message_t *msg;

	if ((msg = malloc(sizeof(*msg)))) {
		memset(msg, 0, sizeof(*msg));
		msg->message_id = indication;
		msg->from = __FILE__;
		switch_core_session_queue_message(session, msg);
		switch_set_flag(msg, SCSMF_DYNAMIC);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_message(switch_core_session_t *session, switch_core_session_message_t *message)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	assert(session != NULL);

	if (!session->message_queue) {
		switch_queue_create(&session->message_queue, SWITCH_MESSAGE_QUEUE_LEN, session->pool);
	}

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

	assert(session != NULL);

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

	assert(session != NULL);

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

	assert(session != NULL);

	if (!session->event_queue) {
		switch_queue_create(&session->event_queue, SWITCH_EVENT_QUEUE_LEN, session->pool);
	}

	if (session->event_queue) {
		if (switch_queue_trypush(session->event_queue, *event) == SWITCH_STATUS_SUCCESS) {
			*event = NULL;
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

SWITCH_DECLARE(int32_t) switch_core_session_event_count(switch_core_session_t *session)
{
	if (session->event_queue) {
		return (int32_t) switch_queue_size(session->event_queue);
	}

	return -1;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;

	assert(session != NULL);

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

	assert(session != NULL);

	if (!session->private_event_queue) {
		switch_queue_create(&session->private_event_queue, SWITCH_EVENT_QUEUE_LEN, session->pool);
	}

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

SWITCH_DECLARE(int32_t) switch_core_session_private_event_count(switch_core_session_t *session)
{
	if (session->private_event_queue) {
		return (int32_t) switch_queue_size(session->private_event_queue);
	}

	return -1;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_private_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;
	switch_channel_t *channel;

	assert(session != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_channel_test_flag(channel, CF_EVENT_PARSE)) {
		return status;
	}


	if (session->private_event_queue) {
		if ((status = (switch_status_t) switch_queue_trypop(session->private_event_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*event = (switch_event_t *) pop;
		}
	}

	return status;
}

SWITCH_DECLARE(void) switch_core_session_reset(switch_core_session_t *session)
{
	switch_channel_t *channel;
	char buf[256];
	switch_size_t has;

	/* sweep theese under the rug, they wont be leaked they will be reclaimed
	   when the session ends.
	 */
	session->read_resampler = NULL;
	session->write_resampler = NULL;

	/* clear indications */
	switch_core_session_flush_message(session);

	/* wipe theese, they will be recreated if need be */
	switch_buffer_destroy(&session->raw_read_buffer);
	switch_buffer_destroy(&session->raw_write_buffer);

	/* flush dtmf */
	channel = switch_core_session_get_channel(session);

	while ((has = switch_channel_has_dtmf(channel))) {
		switch_channel_dequeue_dtmf(channel, buf, sizeof(buf));
	}

}


SWITCH_DECLARE(switch_channel_t *) switch_core_session_get_channel(switch_core_session_t *session)
{
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
}

SWITCH_DECLARE(unsigned int) switch_core_session_running(switch_core_session_t *session)
{
	return session->thread_running;
}

#ifdef CRASH_PROT
#if defined (__GNUC__) && defined (LINUX)
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#define STACK_LEN 10

/* Obtain a backtrace and print it to stdout. */
static void print_trace(void)
{
	void *array[STACK_LEN];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace(array, STACK_LEN);
	strings = backtrace_symbols(array, size);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Obtained %zd stack frames.\n", size);

	for (i = 0; i < size; i++) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CRIT, "%s\n", strings[i]);
	}

	free(strings);
}
#else
static void print_trace(void)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Trace not avaliable =(\n");
}
#endif


static void handle_fatality(int sig)
{
	switch_thread_id_t thread_id;
	jmp_buf *env;

	if (sig && (thread_id = switch_thread_self())
		&& (env = (jmp_buf *) apr_hash_get(runtime.stack_table, &thread_id, sizeof(thread_id)))) {
		print_trace();
		longjmp(*env, sig);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Caught signal %d for unmapped thread!", sig);
		abort();
	}
}
#endif


SWITCH_DECLARE(void) switch_core_session_destroy(switch_core_session_t **session)
{
	switch_memory_pool_t *pool;
	switch_event_t *event;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Close Channel %s\n", switch_channel_get_name((*session)->channel));

	switch_scheduler_del_task_group((*session)->uuid_str);

	switch_mutex_lock(runtime.session_table_mutex);
	switch_core_hash_delete(runtime.session_table, (*session)->uuid_str);
	if (runtime.session_count) {
		runtime.session_count--;
	}
	switch_mutex_unlock(runtime.session_table_mutex);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DESTROY) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data((*session)->channel, event);
		switch_event_fire(&event);
	}

	switch_core_media_bug_remove_all(*session);
	switch_buffer_destroy(&(*session)->raw_read_buffer);
	switch_buffer_destroy(&(*session)->raw_write_buffer);
	switch_channel_uninit((*session)->channel);

	pool = (*session)->pool;
	*session = NULL;
	apr_pool_destroy(pool);
	pool = NULL;

}

static void *SWITCH_THREAD_FUNC switch_core_session_thread(switch_thread_t * thread, void *obj)
{
	switch_core_session_t *session = obj;
	session->thread = thread;



	switch_core_session_run(session);
	switch_core_media_bug_remove_all(session);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Session %u (%s) Locked, Waiting on external entities\n",
					  session->id, switch_channel_get_name(session->channel));
	switch_core_session_write_lock(session);
	switch_set_flag(session, SSF_DESTROYED);
	switch_core_session_rwunlock(session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Session %u (%s) Ended\n", session->id, switch_channel_get_name(session->channel));
	switch_core_session_destroy(&session);
	return NULL;
}


SWITCH_DECLARE(void) switch_core_session_thread_launch(switch_core_session_t *session)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr;;
	switch_threadattr_create(&thd_attr, session->pool);
	switch_threadattr_detach_set(thd_attr, 1);

	if (!session->thread_running) {
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		if (switch_thread_create(&thread, thd_attr, switch_core_session_thread, session, session->pool) != SWITCH_STATUS_SUCCESS) {
			switch_core_session_destroy(&session);
		}
	}
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
																	*endpoint_interface, switch_memory_pool_t ** pool)
{
	switch_memory_pool_t *usepool;
	switch_core_session_t *session;
	switch_uuid_t uuid;
	uint32_t count = 0;

	assert(endpoint_interface != NULL);

	switch_mutex_lock(runtime.session_table_mutex);
	count = runtime.session_count;
	switch_mutex_unlock(runtime.session_table_mutex);

	if ((count + 1) > runtime.session_limit) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Over Session Limit!\n");
		return NULL;
	}

	if (!switch_core_ready()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read my lips: no new sessions!\n");
		return NULL;
	}

	if (pool && *pool) {
		usepool = *pool;
		*pool = NULL;
	} else if (switch_core_new_memory_pool(&usepool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate memory pool\n");
		return NULL;
	}

	if ((session = switch_core_alloc(usepool, sizeof(switch_core_session_t))) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate session\n");
		apr_pool_destroy(usepool);
		return NULL;
	}

	if (switch_channel_alloc(&session->channel, usepool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate channel structure\n");
		apr_pool_destroy(usepool);
		return NULL;
	}

	switch_channel_init(session->channel, session, CS_NEW, 0);

	/* The session *IS* the pool you may not alter it because you have no idea how
	   its all private it will be passed to the thread run function */

	switch_uuid_get(&uuid);
	switch_uuid_format(session->uuid_str, &uuid);

	session->pool = usepool;
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
	switch_thread_rwlock_create(&session->bug_rwlock, session->pool);
	switch_thread_cond_create(&session->cond, session->pool);
	switch_thread_rwlock_create(&session->rwlock, session->pool);

	snprintf(session->name, sizeof(session->name), "%u", session->id);
	switch_mutex_lock(runtime.session_table_mutex);
	session->id = runtime.session_id++;
	switch_core_hash_insert(runtime.session_table, session->uuid_str, session);
	runtime.session_count++;
	switch_mutex_unlock(runtime.session_table_mutex);

	return session;
}

SWITCH_DECLARE(uint32_t) switch_core_session_count(void)
{
	return runtime.session_count;
}

SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request_by_name(char *endpoint_name, switch_memory_pool_t ** pool)
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
	assert(a != NULL);
	assert(b != NULL);

	return (uint8_t) (a->endpoint_interface == b->endpoint_interface);
}

SWITCH_DECLARE(char *) switch_core_session_get_uuid(switch_core_session_t *session)
{
	return session->uuid_str;
}


SWITCH_DECLARE(uint32_t) switch_core_session_limit(uint32_t new_limit)
{
	if (new_limit) {
		runtime.session_limit = new_limit;
	}

	return runtime.session_limit;
}


SWITCH_DECLARE(void) switch_core_session_init(switch_memory_pool_t * pool)
{
	memset(&runtime, 0, sizeof(runtime));
	runtime.session_limit = 1000;
	runtime.session_id = 1;
	runtime.memory_pool = pool;
	switch_core_hash_init(&runtime.session_table, runtime.memory_pool);
	switch_mutex_init(&runtime.session_table_mutex, SWITCH_MUTEX_NESTED, runtime.memory_pool);
}
