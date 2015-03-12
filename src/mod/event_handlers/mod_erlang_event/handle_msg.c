/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Andrew Thompson <andrew@hijacked.us>
 * Rob Charlton <rob.charlton@savageminds.com>
 * Darren Schreiber <d@d-man.org>
 * Mike Jerris <mike@jerris.com>
 * Tamas Cseke <tamas.cseke@virtual-call-center.eu>
 *
 *
 * handle_msg.c -- handle messages received from erlang nodes
 *
 */
#include <switch.h>
#include <ei.h>
#include "mod_erlang_event.h"

static char *MARKER = "1";

static switch_status_t handle_ref_tuple(listener_t *listener, erlang_msg * msg, ei_x_buff * buf, ei_x_buff * rbuf);

static void *SWITCH_THREAD_FUNC api_exec(switch_thread_t *thread, void *obj)
{
	switch_bool_t r = SWITCH_TRUE;
	struct api_command_struct *acs = (struct api_command_struct *) obj;
	switch_stream_handle_t stream = { 0 };
	char *reply, *freply = NULL;
	switch_status_t status;

	if (!acs) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Internal error.\n");
		return NULL;
	}

	if (!acs->listener || !acs->listener->rwlock || switch_thread_rwlock_tryrdlock(acs->listener->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error! cannot get read lock.\n");
		goto done;
	}

	SWITCH_STANDARD_STREAM(stream);

	if ((status = switch_api_execute(acs->api_cmd, acs->arg, NULL, &stream)) == SWITCH_STATUS_SUCCESS) {
		reply = stream.data;
	} else {
		freply = switch_mprintf("%s: Command not found!\n", acs->api_cmd);
		reply = freply;
		r = SWITCH_FALSE;
	}

	if (!reply) {
		reply = "Command returned no output!";
		r = SWITCH_FALSE;
	}

	if (*reply == '-')
		r = SWITCH_FALSE;

	if (acs->bg) {
		switch_event_t *event;

		if (switch_event_create(&event, SWITCH_EVENT_BACKGROUND_JOB) == SWITCH_STATUS_SUCCESS) {
			ei_x_buff ebuf;

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-UUID", acs->uuid_str);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command", acs->api_cmd);

			ei_x_new_with_version(&ebuf);

			if (acs->arg) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command-Arg", acs->arg);
			}

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Successful", r ? "true" : "false");
			switch_event_add_body(event, "%s", reply);

			switch_event_fire(&event);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending bgapi reply to %s\n", acs->pid.node);

			ei_x_encode_tuple_header(&ebuf, 3);

			if (r)
				ei_x_encode_atom(&ebuf, "bgok");
			else
				ei_x_encode_atom(&ebuf, "bgerror");

			_ei_x_encode_string(&ebuf, acs->uuid_str);
			_ei_x_encode_string(&ebuf, reply);

			switch_mutex_lock(acs->listener->sock_mutex);
			ei_send(acs->listener->sockfd, &acs->pid, ebuf.buff, ebuf.index);
			switch_mutex_unlock(acs->listener->sock_mutex);
#ifdef EI_DEBUG
			ei_x_print_msg(&ebuf, &acs->pid, 1);
#endif

			ei_x_free(&ebuf);
		}
	} else {
		ei_x_buff rbuf;
		ei_x_new_with_version(&rbuf);
		ei_x_encode_tuple_header(&rbuf, 2);

		if (!strlen(reply)) {
			reply = "Command returned no output!";
			r = SWITCH_FALSE;
		}

		if (r) {
			ei_x_encode_atom(&rbuf, "ok");
		} else {
			ei_x_encode_atom(&rbuf, "error");
		}

		_ei_x_encode_string(&rbuf, reply);


		switch_mutex_lock(acs->listener->sock_mutex);
		ei_send(acs->listener->sockfd, &acs->pid, rbuf.buff, rbuf.index);
		switch_mutex_unlock(acs->listener->sock_mutex);
#ifdef EI_DEBUG
		ei_x_print_msg(&rbuf, &acs->pid, 1);
#endif

		ei_x_free(&rbuf);
	}

	switch_safe_free(stream.data);
	switch_safe_free(freply);

	if (acs->listener->rwlock) {
		switch_thread_rwlock_unlock(acs->listener->rwlock);
	}

  done:
	if (acs->bg) {
		switch_memory_pool_t *pool = acs->pool;
		acs = NULL;
		switch_core_destroy_memory_pool(&pool);
		pool = NULL;
	}
	return NULL;

}

static switch_status_t handle_msg_fetch_reply(listener_t *listener, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	fetch_reply_t *p;

	if (ei_decode_string_or_binary(buf->buff, &buf->index, SWITCH_UUID_FORMATTED_LENGTH, uuid_str)) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {

		/* reply mutex is locked */
		if ((p = find_fetch_reply(uuid_str))) {
			switch (p->state) {
			case reply_waiting: 
				{
					/* clone the reply so it doesn't get destroyed on us */
					ei_x_buff *nbuf = malloc(sizeof(*nbuf));
					nbuf->buff = malloc(buf->buffsz);
					memcpy(nbuf->buff, buf->buff, buf->buffsz);
					nbuf->index = buf->index;
					nbuf->buffsz = buf->buffsz;
					
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got reply for %s\n", uuid_str);
					
					/* copy info into the reply struct */
					p->state = reply_found;
					p->reply = nbuf;
					strncpy(p->winner, listener->peer_nodename, MAXNODELEN);
					
					/* signal waiting thread that its time to wake up */
					switch_thread_cond_signal(p->ready_or_found);
					/* reply OK */
					ei_x_encode_tuple_header(rbuf, 2);
					ei_x_encode_atom(rbuf, "ok");
					_ei_x_encode_string(rbuf, uuid_str);
					break;
				};
			case reply_found:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Reply for already complete request %s\n", uuid_str);
				ei_x_encode_tuple_header(rbuf, 3);
				ei_x_encode_atom(rbuf, "error");
				_ei_x_encode_string(rbuf, uuid_str);
				ei_x_encode_atom(rbuf, "duplicate_response");
				break;
			case reply_timeout:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Reply for timed out request %s\n", uuid_str);
				ei_x_encode_tuple_header(rbuf, 3);
				ei_x_encode_atom(rbuf, "error");
				_ei_x_encode_string(rbuf, uuid_str);
				ei_x_encode_atom(rbuf, "timeout");
				break;
			case reply_not_ready:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Request %s is not ready?!\n", uuid_str);
				ei_x_encode_tuple_header(rbuf, 3);
				ei_x_encode_atom(rbuf, "error");
				_ei_x_encode_string(rbuf, uuid_str);
				ei_x_encode_atom(rbuf, "not_ready");
				break;
			}

			switch_mutex_unlock(p->mutex);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Could not find request for reply %s\n", uuid_str);
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "invalid_uuid");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_set_log_level(listener_t *listener, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	switch_log_level_t ltype = SWITCH_LOG_DEBUG;
	char loglevelstr[MAXATOMLEN];
	if (arity != 2 || ei_decode_atom(buf->buff, &buf->index, loglevelstr)) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		ltype = switch_log_str2level(loglevelstr);

		if (ltype && ltype != SWITCH_LOG_INVALID) {
			listener->level = ltype;
			ei_x_encode_atom(rbuf, "ok");
		} else {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "badarg");
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_event(listener_t *listener, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char atom[MAXATOMLEN];

	if (arity == 1) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		int custom = 0;
		switch_event_types_t type;
		int i = 0;

		if (!switch_test_flag(listener, LFLAG_EVENTS)) {
			switch_set_flag_locked(listener, LFLAG_EVENTS);
		}

		switch_thread_rwlock_wrlock(listener->event_rwlock);

		for (i = 1; i < arity; i++) {
			if (!ei_decode_atom(buf->buff, &buf->index, atom)) {

				if (custom) {
					switch_core_hash_insert(listener->event_hash, atom, MARKER);
				} else if (switch_name_event(atom, &type) == SWITCH_STATUS_SUCCESS) {
					if (type == SWITCH_EVENT_ALL) {
						uint32_t x = 0;

						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "ALL events enabled\n");
						for (x = 0; x < SWITCH_EVENT_ALL; x++) {
							listener->event_list[x] = 1;
						}
					}
					if (type <= SWITCH_EVENT_ALL) {
						listener->event_list[type] = 1;
					}
					if (type == SWITCH_EVENT_CUSTOM) {
						custom++;
					}

				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "enable event %s\n", atom);
			}
		}
		switch_thread_rwlock_unlock(listener->event_rwlock);

		ei_x_encode_atom(rbuf, "ok");
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_filter(listener_t *listener, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char atom[MAXATOMLEN];
	char reply[MAXATOMLEN]= "";
	char *header_name = NULL;
	char *header_val = NULL;

	if (arity == 1) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		int i = 0;

		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "filter_command_processing_log");
		ei_x_encode_list_header(rbuf, arity - 1);

		switch_thread_rwlock_wrlock(listener->event_rwlock);

		switch_mutex_lock(listener->filter_mutex);
		if (!listener->filters) {
			switch_event_create_plain(&listener->filters, SWITCH_EVENT_CLONE);
			switch_clear_flag(listener->filters, EF_UNIQ_HEADERS);
		}

		for (i = 1; i < arity; i++) {
			if (!ei_decode_atom(buf->buff, &buf->index, atom)) {
				header_name=atom;

				while (header_name && *header_name && *header_name == ' ')
				header_name++;

				if ((header_val = strchr(atom, ' '))) {
					*header_val++ = '\0';
				}

				if (!strcasecmp(header_name, "delete") && header_val) {
					header_name = header_val;
					if ((header_val = strchr(header_name, ' '))) {
						*header_val++ = '\0';
					}
					if (!strcasecmp(header_name, "all")) {
						switch_event_destroy(&listener->filters);
						switch_event_create_plain(&listener->filters, SWITCH_EVENT_CLONE);
					} else {
						switch_event_del_header_val(listener->filters, header_name, header_val);
					}
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "+OK filter deleted. [%s]=[%s]", header_name, switch_str_nil(header_val));
					ei_x_encode_tuple_header(rbuf, 3);
					_ei_x_encode_string(rbuf, "deleted");
					_ei_x_encode_string(rbuf, header_name);
					_ei_x_encode_string(rbuf, switch_str_nil(header_val));
				} else if (header_val) {
					if (!strcasecmp(header_name, "add")) {
						header_name = header_val;
						if ((header_val = strchr(header_name, ' '))) {
							*header_val++ = '\0';
						}
					}
					switch_event_add_header_string(listener->filters, SWITCH_STACK_BOTTOM, header_name, header_val);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "+OK filter added. [%s]=[%s]", header_name, header_val);
					ei_x_encode_tuple_header(rbuf, 3);
					_ei_x_encode_string(rbuf, "added");
					_ei_x_encode_string(rbuf, header_name);
					_ei_x_encode_string(rbuf, header_val);
				} else {
					switch_snprintf(reply, MAXATOMLEN, "-ERR invalid syntax");
					ei_x_encode_atom(rbuf, "-ERR invalid syntax");
				}
			}
		}

		switch_mutex_unlock(listener->filter_mutex);
		switch_thread_rwlock_unlock(listener->event_rwlock);

		ei_x_encode_empty_list(rbuf);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_session_event(listener_t *listener, erlang_msg *msg, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char atom[MAXATOMLEN];

	if (arity == 1) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		session_elem_t *session;
		if ((session = find_session_elem_by_pid(listener, &msg->from))) {

			int custom = 0;
			switch_event_types_t type;
			int i = 0;

			switch_thread_rwlock_wrlock(session->event_rwlock);

			for (i = 1; i < arity; i++) {
				if (!ei_decode_atom(buf->buff, &buf->index, atom)) {

					if (custom) {
						switch_core_hash_insert(session->event_hash, atom, MARKER);
					} else if (switch_name_event(atom, &type) == SWITCH_STATUS_SUCCESS) {
						if (type == SWITCH_EVENT_ALL) {
							uint32_t x = 0;

							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "ALL events enabled for %s\n", session->uuid_str);
							for (x = 0; x < SWITCH_EVENT_ALL; x++) {
								session->event_list[x] = 1;
							}
						}
						if (type <= SWITCH_EVENT_ALL) {
							session->event_list[type] = 1;
						}
						if (type == SWITCH_EVENT_CUSTOM) {
							custom++;
						}

					}
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "enable event %s for session %s\n", atom, session->uuid_str);
				}
			}

			switch_thread_rwlock_unlock(session->event_rwlock);
			switch_thread_rwlock_unlock(session->rwlock);
			
			ei_x_encode_atom(rbuf, "ok");
		} else {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "notlistening");
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_nixevent(listener_t *listener, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char atom[MAXATOMLEN];

	if (arity == 1) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		int custom = 0;
		int i = 0;
		switch_event_types_t type;

		switch_thread_rwlock_wrlock(listener->event_rwlock);

		for (i = 1; i < arity; i++) {
			if (!ei_decode_atom(buf->buff, &buf->index, atom)) {

				if (custom) {

					switch_core_hash_delete(listener->event_hash, atom);
				} else if (switch_name_event(atom, &type) == SWITCH_STATUS_SUCCESS) {
					uint32_t x = 0;

					if (type == SWITCH_EVENT_CUSTOM) {
						custom++;
					} else if (type == SWITCH_EVENT_ALL) {
						for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
							listener->event_list[x] = 0;
						}
					} else {
						if (listener->event_list[SWITCH_EVENT_ALL]) {
							listener->event_list[SWITCH_EVENT_ALL] = 0;
							for (x = 0; x < SWITCH_EVENT_ALL; x++) {
								listener->event_list[x] = 1;
							}
						}
						listener->event_list[type] = 0;
					}
				}
			}
		}

		switch_thread_rwlock_unlock(listener->event_rwlock);
		ei_x_encode_atom(rbuf, "ok");
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_session_nixevent(listener_t *listener, erlang_msg *msg, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char atom[MAXATOMLEN];

	if (arity == 1) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		session_elem_t *session;
		if ((session = find_session_elem_by_pid(listener, &msg->from))) {
			int custom = 0;
			int i = 0;
			switch_event_types_t type;

			switch_thread_rwlock_wrlock(session->event_rwlock);

			for (i = 1; i < arity; i++) {
				if (!ei_decode_atom(buf->buff, &buf->index, atom)) {

					if (custom) {
						switch_core_hash_delete(session->event_hash, atom);
					} else if (switch_name_event(atom, &type) == SWITCH_STATUS_SUCCESS) {
						uint32_t x = 0;

						if (type == SWITCH_EVENT_CUSTOM) {
							custom++;
						} else if (type == SWITCH_EVENT_ALL) {
							for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
								session->event_list[x] = 0;
							}
						} else {
							if (session->event_list[SWITCH_EVENT_ALL]) {
								session->event_list[SWITCH_EVENT_ALL] = 0;
								for (x = 0; x < SWITCH_EVENT_ALL; x++) {
									session->event_list[x] = 1;
								}
							}
							session->event_list[type] = 0;
						}
					}
				}
			}
			switch_thread_rwlock_unlock(session->event_rwlock);
			switch_thread_rwlock_unlock(session->rwlock);

			ei_x_encode_atom(rbuf, "ok");
		} else { /* no session for this pid */
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "notlistening");
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

// Nix's all events, then sets up a listener for the given ones.
// meant to ensure that no events are missed during this common operation.
static switch_status_t handle_msg_setevent(listener_t *listener, erlang_msg *msg, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char atom[MAXATOMLEN];

	if(arity == 1) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		uint8_t event_list[SWITCH_EVENT_ALL + 1];
		switch_hash_t *event_hash;
		uint32_t x = 0;
		int custom = 0;
		switch_event_types_t type;
		int i = 0;

		/* clear any previous event registrations */
		for(x = 0; x <= SWITCH_EVENT_ALL; x++) {
			event_list[x] = 0;
		}

		/* create new hash */
		switch_core_hash_init(&event_hash);

		if(!switch_test_flag(listener, LFLAG_EVENTS)) {
			switch_set_flag_locked(listener, LFLAG_EVENTS);
		}

		for(i = 1; i < arity; i++){
			if(!ei_decode_atom(buf->buff, &buf->index, atom)){

				if(custom){
					switch_core_hash_insert(event_hash, atom, MARKER);
				} else if (switch_name_event(atom, &type) == SWITCH_STATUS_SUCCESS) {
					if (type == SWITCH_EVENT_ALL) {
						ei_x_encode_tuple_header(rbuf, 2);
						ei_x_encode_atom(rbuf, "error");
						ei_x_encode_atom(rbuf, "badarg");
						break;
					}
					if (type <= SWITCH_EVENT_ALL) {
						event_list[type] = 1;
					}
					if (type == SWITCH_EVENT_CUSTOM) {
						custom++;
					}
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "enable event %s\n", atom);
			}
		}
		/* update the event subscriptions with the new ones */
		switch_thread_rwlock_wrlock(listener->event_rwlock);
		memcpy(listener->event_list, event_list, sizeof(uint8_t) * (SWITCH_EVENT_ALL + 1));
		switch_core_hash_destroy(&listener->event_hash);
		listener->event_hash = event_hash;
		switch_thread_rwlock_unlock(listener->event_rwlock);

		/* TODO - we should flush any non-matching events from the queue */
		ei_x_encode_atom(rbuf, "ok");
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_session_setevent(listener_t *listener, erlang_msg *msg, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char atom[MAXATOMLEN];

	if (arity == 1){
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		session_elem_t *session;
		if ((session = find_session_elem_by_pid(listener, &msg->from))) {
			uint8_t event_list[SWITCH_EVENT_ALL + 1];
			switch_hash_t *event_hash;
			int custom = 0;
			int i = 0;
			switch_event_types_t type;
			uint32_t x = 0;

			/* clear any previous event registrations */
			for (x = 0; x <= SWITCH_EVENT_ALL; x++){
				event_list[x] = 0;
			}

			/* create new hash */
			switch_core_hash_init(&event_hash);

			for (i = 1; i < arity; i++){
				if (!ei_decode_atom(buf->buff, &buf->index, atom)) {
					if (custom) {
						switch_core_hash_insert(event_hash, atom, MARKER);
					} else if (switch_name_event(atom, &type) == SWITCH_STATUS_SUCCESS) {
						if (type == SWITCH_EVENT_ALL) {
							ei_x_encode_tuple_header(rbuf, 1);
							ei_x_encode_atom(rbuf, "error");
							ei_x_encode_atom(rbuf, "badarg");
							break;
						}
						if (type <= SWITCH_EVENT_ALL) {
							event_list[type] = 1;
						}
						if (type == SWITCH_EVENT_CUSTOM) {
							custom++;
						}
					}
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "enable event %s for session %s\n", atom, session->uuid_str);
				}
			}

			/* update the event subscriptions with the new ones */
			switch_thread_rwlock_wrlock(session->event_rwlock);
			memcpy(session->event_list, event_list, sizeof(uint8_t) * (SWITCH_EVENT_ALL + 1));
			/* wipe the old hash, and point the pointer at the new one */
			switch_core_hash_destroy(&session->event_hash);
			session->event_hash = event_hash;
			switch_thread_rwlock_unlock(session->event_rwlock);

			switch_thread_rwlock_unlock(session->rwlock);

			/* TODO - we should flush any non-matching events from the queue */
			ei_x_encode_atom(rbuf, "ok");
		} else { /* no session for this pid */
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "notlistening");
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_api(listener_t *listener, erlang_msg * msg, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char api_cmd[MAXATOMLEN];
	int type;
	int size;
	char *arg;
	switch_bool_t fail = SWITCH_FALSE;

	if (arity < 3) {
		fail = SWITCH_TRUE;
	}

	ei_get_type(buf->buff, &buf->index, &type, &size);

	if ((size > (sizeof(api_cmd) - 1)) || ei_decode_atom(buf->buff, &buf->index, api_cmd)) {
		fail = SWITCH_TRUE;
	}

	ei_get_type(buf->buff, &buf->index, &type, &size);
	arg = malloc(size + 1);

	if (ei_decode_string_or_binary(buf->buff, &buf->index, size, arg)) {
		fail = SWITCH_TRUE;
	}

	if (!fail) {
		struct api_command_struct acs = { 0 };
		acs.listener = listener;
		acs.api_cmd = api_cmd;
		acs.arg = arg;
		acs.bg = 0;
		acs.pid = msg->from;
		api_exec(NULL, (void *) &acs);

		switch_safe_free(arg);

		/* don't reply */
		return SWITCH_STATUS_FALSE;
	} else {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
		return SWITCH_STATUS_SUCCESS;

	}
}

#define ARGLEN 2048
static switch_status_t handle_msg_bgapi(listener_t *listener, erlang_msg * msg, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char api_cmd[MAXATOMLEN];
	char arg[ARGLEN];

	if (arity < 3 || ei_decode_atom(buf->buff, &buf->index, api_cmd) || ei_decode_string_or_binary(buf->buff, &buf->index, ARGLEN - 1, arg)) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		struct api_command_struct *acs = NULL;
		switch_memory_pool_t *pool;
		switch_thread_t *thread;
		switch_threadattr_t *thd_attr = NULL;
		switch_uuid_t uuid;

		switch_core_new_memory_pool(&pool);
		acs = switch_core_alloc(pool, sizeof(*acs));
		switch_assert(acs);
		acs->pool = pool;
		acs->listener = listener;
		acs->api_cmd = switch_core_strdup(acs->pool, api_cmd);
		acs->arg = switch_core_strdup(acs->pool, arg);
		acs->bg = 1;
		acs->pid = msg->from;

		switch_threadattr_create(&thd_attr, acs->pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

		switch_uuid_get(&uuid);
		switch_uuid_format(acs->uuid_str, &uuid);
		switch_thread_create(&thread, thd_attr, api_exec, acs, acs->pool);

		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "ok");
		_ei_x_encode_string(rbuf, acs->uuid_str);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_sendevent(listener_t *listener, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char ename[MAXATOMLEN + 1];
	char esname[MAXATOMLEN + 1];
	int headerlength;

	memset(esname, 0, MAXATOMLEN);

	if (ei_decode_atom(buf->buff, &buf->index, ename) ||
		(!strncasecmp(ename, "CUSTOM", MAXATOMLEN) &&
		 ei_decode_atom(buf->buff, &buf->index, esname)) || ei_decode_list_header(buf->buff, &buf->index, &headerlength)) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		switch_event_types_t etype;
		if (switch_name_event(ename, &etype) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *event;
			if ((strlen(esname) && switch_event_create_subclass(&event, etype, esname) == SWITCH_STATUS_SUCCESS) ||
				switch_event_create(&event, etype) == SWITCH_STATUS_SUCCESS) {
				char key[1024];
				char *value;
                                int type;
                                int size;
				int i = 0;
				switch_bool_t fail = SWITCH_FALSE;

				while (!ei_decode_tuple_header(buf->buff, &buf->index, &arity) && arity == 2) {
					i++;

					ei_get_type(buf->buff, &buf->index, &type, &size);

					if ((size > (sizeof(key) - 1)) || ei_decode_string(buf->buff, &buf->index, key)) {
						fail = SWITCH_TRUE;
						break;
					}

					ei_get_type(buf->buff, &buf->index, &type, &size);
					value = malloc(size + 1);

					if (ei_decode_string(buf->buff, &buf->index, value)) {
       						fail = SWITCH_TRUE;
						break;
					}

					if (!fail && !strcmp(key, "body")) {
						switch_safe_free(event->body);
						event->body = value;
					} else if (!fail)  {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, key, value);
					}
					
					/* Do not free malloc here! The above commands utilize the raw allocated memory and skip any copying/duplication. Faster. */
				}

				if (headerlength != i || fail) {
					ei_x_encode_tuple_header(rbuf, 2);
					ei_x_encode_atom(rbuf, "error");
					ei_x_encode_atom(rbuf, "badarg");
				} else {
					switch_event_fire(&event);
					ei_x_encode_atom(rbuf, "ok");
				}
			}
			/* If the event wasn't successfully fired, or failed for any other reason, then make sure not to leak it. */
			if ( event ) {
				switch_event_destroy(&event);
			}
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_sendmsg(listener_t *listener, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int headerlength;

	if (ei_decode_string_or_binary(buf->buff, &buf->index, SWITCH_UUID_FORMATTED_LENGTH, uuid) ||
		ei_decode_list_header(buf->buff, &buf->index, &headerlength)) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		switch_core_session_t *session;
		if (!zstr_buf(uuid) && (session = switch_core_session_locate(uuid))) {
			switch_event_t *event;
			if (switch_event_create(&event, SWITCH_EVENT_SEND_MESSAGE) == SWITCH_STATUS_SUCCESS) {

				char key[1024];
				char *value;
				int type;
				int size;
				int i = 0;
				switch_bool_t fail = SWITCH_FALSE;

				while (!ei_decode_tuple_header(buf->buff, &buf->index, &arity) && arity == 2) {
					i++;
					ei_get_type(buf->buff, &buf->index, &type, &size);

					if ((size > (sizeof(key) - 1)) || ei_decode_string(buf->buff, &buf->index, key)) {
						fail = SWITCH_TRUE;
						break;
					}
					
					ei_get_type(buf->buff, &buf->index, &type, &size);
					value = malloc(size + 1);

					if (ei_decode_string(buf->buff, &buf->index, value)) {
						fail = SWITCH_TRUE;
						break;
					}

					if (!fail) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, key, value);
					}
				}

				if (headerlength != i || fail) {
					ei_x_encode_tuple_header(rbuf, 2);
					ei_x_encode_atom(rbuf, "error");
					ei_x_encode_atom(rbuf, "badarg");
					switch_event_destroy(&event);
				} else {
					if (switch_core_session_queue_private_event(session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
						ei_x_encode_atom(rbuf, "ok");
					} else {
						ei_x_encode_tuple_header(rbuf, 2);
						ei_x_encode_atom(rbuf, "error");
						ei_x_encode_atom(rbuf, "badmem");
						switch_event_destroy(&event);
					}

				}
			}
			/* release the lock returned by session locate */
			switch_core_session_rwunlock(session);

		} else {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "nosession");
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_msg_bind(listener_t *listener, erlang_msg * msg, ei_x_buff * buf, ei_x_buff * rbuf)
{
	/* format is (result|config|directory|dialplan|phrases)  */
	char sectionstr[MAXATOMLEN];
	switch_xml_section_t section;

	if (ei_decode_atom(buf->buff, &buf->index, sectionstr) || !(section = switch_xml_parse_section_string(sectionstr))) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		struct erlang_binding *binding, *ptr;

		if (!(binding = switch_core_alloc(listener->pool, sizeof(*binding)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "badmem");
		} else {
			binding->section = section;
			binding->process.type = ERLANG_PID;
			binding->process.pid = msg->from;
			binding->listener = listener;

			switch_thread_rwlock_wrlock(globals.bindings_rwlock);

			for (ptr = bindings.head; ptr && ptr->next; ptr = ptr->next);

			if (ptr) {
				ptr->next = binding;
			} else {
				bindings.head = binding;
			}

			switch_xml_set_binding_sections(bindings.search_binding, switch_xml_get_binding_sections(bindings.search_binding) | section);
			switch_thread_rwlock_unlock(globals.bindings_rwlock);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sections %d\n", switch_xml_get_binding_sections(bindings.search_binding));

			ei_link(listener, ei_self(listener->ec), &msg->from);
			ei_x_encode_atom(rbuf, "ok");
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

/* {handlecall,<uuid>,<handler process registered name>}
   or
   {handlecall,<uuid>} to send messages back to the sender
 */
static switch_status_t handle_msg_handlecall(listener_t *listener, erlang_msg * msg, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char reg_name[MAXATOMLEN];
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	if (arity < 2 || arity > 3 ||
		(arity == 3 && ei_decode_atom(buf->buff, &buf->index, reg_name)) ||
		ei_decode_string_or_binary(buf->buff, &buf->index, SWITCH_UUID_FORMATTED_LENGTH, uuid_str)) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		switch_core_session_t *session;
		if (!zstr_buf(uuid_str)) {
			if ((session = switch_core_session_locate(uuid_str))) {
				/* create a new session list element and attach it to this listener */
				if ((arity == 2 && attach_call_to_pid(listener, &msg->from, session)) ||
					(arity == 3 && attach_call_to_registered_process(listener, reg_name, session))) {
					ei_x_encode_atom(rbuf, "ok");
				} else {
					ei_x_encode_tuple_header(rbuf, 2);
					ei_x_encode_atom(rbuf, "error");
					ei_x_encode_atom(rbuf, "session_attach_failed");
				}
				/* release the lock returned by session locate */
				switch_core_session_rwunlock(session);
			} else {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badsession");
			}
		} else {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "baduuid");
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

/* catch the response to ei_rpc_to (which comes back as {rex, {Ref, Pid}}
   The {Ref,Pid} bit can be handled by handle_ref_tuple
 */
static switch_status_t handle_msg_rpcresponse(listener_t *listener, erlang_msg * msg, int arity, ei_x_buff * buf, ei_x_buff * rbuf)
{
	int type, size, arity2, tmpindex;

	ei_get_type(buf->buff, &buf->index, &type, &size);
	switch (type) {
	case ERL_SMALL_TUPLE_EXT:
	case ERL_LARGE_TUPLE_EXT:
		tmpindex = buf->index;
		ei_decode_tuple_header(buf->buff, &tmpindex, &arity2);
		return handle_ref_tuple(listener, msg, buf, rbuf);
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unknown rpc response\n");
		break;
	}
	/* no reply */
	return SWITCH_STATUS_FALSE;
}

static switch_status_t handle_msg_tuple(listener_t *listener, erlang_msg * msg, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char tupletag[MAXATOMLEN];
	int arity;
	switch_status_t ret = SWITCH_STATUS_SUCCESS;

	ei_decode_tuple_header(buf->buff, &buf->index, &arity);
	if (ei_decode_atom(buf->buff, &buf->index, tupletag)) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else {
		if (!strncmp(tupletag, "fetch_reply", MAXATOMLEN)) {
			ret = handle_msg_fetch_reply(listener, buf, rbuf);
		} else if (!strncmp(tupletag, "set_log_level", MAXATOMLEN)) {
			ret = handle_msg_set_log_level(listener, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "event", MAXATOMLEN)) {
			ret = handle_msg_event(listener, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "filter", MAXATOMLEN)) {
			ret = handle_msg_filter(listener, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "session_event", MAXATOMLEN)) {
			ret = handle_msg_session_event(listener, msg, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "nixevent", MAXATOMLEN)) {
			ret = handle_msg_nixevent(listener, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "session_nixevent", MAXATOMLEN)) {
			ret = handle_msg_session_nixevent(listener, msg, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "api", MAXATOMLEN)) {
			ret = handle_msg_api(listener, msg, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "bgapi", MAXATOMLEN)) {
			ret = handle_msg_bgapi(listener, msg, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "sendevent", MAXATOMLEN)) {
			ret = handle_msg_sendevent(listener, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "sendmsg", MAXATOMLEN)) {
			ret = handle_msg_sendmsg(listener, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "bind", MAXATOMLEN)) {
			ret = handle_msg_bind(listener, msg, buf, rbuf);
		} else if (!strncmp(tupletag, "handlecall", MAXATOMLEN)) {
			ret = handle_msg_handlecall(listener, msg, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "rex", MAXATOMLEN)) {
			ret = handle_msg_rpcresponse(listener, msg, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "setevent", MAXATOMLEN)) {
			ret = handle_msg_setevent(listener, msg, arity, buf, rbuf);
		} else if (!strncmp(tupletag, "session_setevent", MAXATOMLEN)) {
			ret = handle_msg_session_setevent(listener, msg, arity, buf, rbuf);
		} else {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "undef");
		}
	}
	return ret;
}

static switch_status_t handle_msg_atom(listener_t *listener, erlang_msg * msg, ei_x_buff * buf, ei_x_buff * rbuf)
{
	char atom[MAXATOMLEN];
	switch_status_t ret = SWITCH_STATUS_SUCCESS;

	if (ei_decode_atom(buf->buff, &buf->index, atom)) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	} else if (!strncmp(atom, "nolog", MAXATOMLEN)) {
		if (switch_test_flag(listener, LFLAG_LOG)) {
			void *pop;
			/*purge the log queue */
			while (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS);
			switch_clear_flag_locked(listener, LFLAG_LOG);
		}
		ei_x_encode_atom(rbuf, "ok");
	} else if (!strncmp(atom, "register_log_handler", MAXATOMLEN)) {
		ei_link(listener, ei_self(listener->ec), &msg->from);
		listener->log_process.type = ERLANG_PID;
		memcpy(&listener->log_process.pid, &msg->from, sizeof(erlang_pid));
		listener->level = SWITCH_LOG_DEBUG;
		switch_set_flag(listener, LFLAG_LOG);
		ei_x_encode_atom(rbuf, "ok");
	} else if (!strncmp(atom, "register_event_handler", MAXATOMLEN)) {
		ei_link(listener, ei_self(listener->ec), &msg->from);
		listener->event_process.type = ERLANG_PID;
		memcpy(&listener->event_process.pid, &msg->from, sizeof(erlang_pid));
		if (!switch_test_flag(listener, LFLAG_EVENTS)) {
			switch_set_flag_locked(listener, LFLAG_EVENTS);
		}
		ei_x_encode_atom(rbuf, "ok");
	} else if (!strncmp(atom, "noevents", MAXATOMLEN)) {
		void *pop;
		/*purge the event queue */
		while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS);

		if (switch_test_flag(listener, LFLAG_EVENTS)) {
			uint8_t x = 0;
			switch_clear_flag_locked(listener, LFLAG_EVENTS);

			switch_thread_rwlock_wrlock(listener->event_rwlock);
			for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
				listener->event_list[x] = 0;
			}

			switch_core_hash_delete_multi(listener->event_hash, NULL, NULL);

			switch_thread_rwlock_unlock(listener->event_rwlock);
			ei_x_encode_atom(rbuf, "ok");
		} else {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "notlistening");
		}
	} else if (!strncmp(atom, "session_noevents", MAXATOMLEN)) {
		session_elem_t *session;
		if ((session = find_session_elem_by_pid(listener, &msg->from))) {
			void *pop;
			uint8_t x = 0;

			/*purge the event queue */
			while (switch_queue_trypop(session->event_queue, &pop) == SWITCH_STATUS_SUCCESS);

			switch_thread_rwlock_wrlock(session->event_rwlock);
			for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
				session->event_list[x] = 0;
			}
			/* wipe the hash */
			switch_core_hash_delete_multi(session->event_hash, NULL, NULL);
			switch_thread_rwlock_unlock(session->event_rwlock);

			switch_thread_rwlock_unlock(session->rwlock);

			ei_x_encode_atom(rbuf, "ok");
		} else {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "notlistening");
		}
	} else if (!strncmp(atom, "exit", MAXATOMLEN)) {
		ei_x_encode_atom(rbuf, "ok");
		ret = SWITCH_STATUS_TERM;
	} else if (!strncmp(atom, "getpid", MAXATOMLEN)) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "ok");
		ei_x_encode_pid(rbuf, ei_self(listener->ec));
	} else if (!strncmp(atom, "link", MAXATOMLEN)) {
		/* debugging */
		ei_link(listener, ei_self(listener->ec), &msg->from);
		ret = SWITCH_STATUS_FALSE;
	} else {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "undef");
	}

	return ret;
}


static switch_status_t handle_ref_tuple(listener_t *listener, erlang_msg * msg, ei_x_buff * buf, ei_x_buff * rbuf)
{
	erlang_ref ref;
	erlang_pid pid;
	char hash[100];
	int arity;
	const void *key;
	void *val;
	session_elem_t *se;
	switch_hash_index_t *iter;
	int found = 0;

	ei_decode_tuple_header(buf->buff, &buf->index, &arity);

	if (ei_decode_ref(buf->buff, &buf->index, &ref)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid reference\n");
		return SWITCH_STATUS_FALSE;
	}

	if (ei_decode_pid(buf->buff, &buf->index, &pid)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid pid in a reference/pid tuple\n");
		return SWITCH_STATUS_FALSE;
	}

	ei_hash_ref(&ref, hash);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Hashed ref to %s\n", hash);

	switch_thread_rwlock_rdlock(listener->session_rwlock);
	for (iter = switch_core_hash_first(listener->sessions); iter; iter = switch_core_hash_next(&iter)) {
		switch_core_hash_this(iter, &key, NULL, &val);
		se = (session_elem_t*)val;
		if (se->spawn_reply && !strncmp(se->spawn_reply->hash, hash, 100)) {
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "found matching session for %s : %s\n", hash, se->uuid_str);

			switch_mutex_lock(se->spawn_reply->mutex);

			se->spawn_reply->pid = switch_core_alloc(se->pool, sizeof(erlang_pid));
			switch_assert(se->spawn_reply->pid != NULL);
			memcpy(se->spawn_reply->pid, &pid, sizeof(erlang_pid));

			switch_thread_cond_signal(se->spawn_reply->ready_or_found);

			switch_mutex_unlock(se->spawn_reply->mutex);

			found++;

			break;
		}
	}
	switch_safe_free(iter);
	switch_thread_rwlock_unlock(listener->session_rwlock);

	if (found) {
		return SWITCH_STATUS_FALSE;
	} 

	ei_x_encode_tuple_header(rbuf, 2);
	ei_x_encode_atom(rbuf, "error");
	ei_x_encode_atom(rbuf, "notfound");

	return SWITCH_STATUS_SUCCESS;
}


/* fake enough of the net_kernel module to be able to respond to net_adm:ping */
/* {'$gen_call', {<cpx@freecpx.128.0>, #Ref<254770.4.0>}, {is_auth, cpx@freecpx} */
static switch_status_t handle_net_kernel_msg(listener_t *listener, erlang_msg * msg, ei_x_buff * buf, ei_x_buff * rbuf)
{
	int version, size, type, arity;
	char atom[MAXATOMLEN];
	erlang_ref ref;
	erlang_pid pid;

	buf->index = 0;
	ei_decode_version(buf->buff, &buf->index, &version);
	ei_get_type(buf->buff, &buf->index, &type, &size);

	if (type != ERL_SMALL_TUPLE_EXT && type != ERL_SMALL_TUPLE_EXT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "not a tuple\n");
		return SWITCH_STATUS_FALSE;
	}

	ei_decode_tuple_header(buf->buff, &buf->index, &arity);

	if (arity != 3) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "wrong arity\n");
		return SWITCH_STATUS_FALSE;
	}

	if (ei_decode_atom(buf->buff, &buf->index, atom) || strncmp(atom, "$gen_call", MAXATOMLEN)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "not gen_call\n");
		return SWITCH_STATUS_FALSE;
	}

	ei_get_type(buf->buff, &buf->index, &type, &size);

	if (type != ERL_SMALL_TUPLE_EXT && type != ERL_SMALL_TUPLE_EXT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "not a tuple\n");
		return SWITCH_STATUS_FALSE;
	}

	ei_decode_tuple_header(buf->buff, &buf->index, &arity);

	if (arity != 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "wrong arity\n");
		return SWITCH_STATUS_FALSE;
	}

	if (ei_decode_pid(buf->buff, &buf->index, &pid) || ei_decode_ref(buf->buff, &buf->index, &ref)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "decoding pid and ref error\n");
		return SWITCH_STATUS_FALSE;
	}

	ei_get_type(buf->buff, &buf->index, &type, &size);

	if (type != ERL_SMALL_TUPLE_EXT && type != ERL_SMALL_TUPLE_EXT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "not a tuple\n");
		return SWITCH_STATUS_FALSE;
	}

	ei_decode_tuple_header(buf->buff, &buf->index, &arity);

	if (arity != 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "bad arity\n");
		return SWITCH_STATUS_FALSE;
	}

	if (ei_decode_atom(buf->buff, &buf->index, atom) || strncmp(atom, "is_auth", MAXATOMLEN)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "not is_auth\n");
		return SWITCH_STATUS_FALSE;
	}

	/* To ! {Tag, Reply} */
	ei_x_encode_tuple_header(rbuf, 2);
	ei_x_encode_ref(rbuf, &ref);
	ei_x_encode_atom(rbuf, "yes");

	switch_mutex_lock(listener->sock_mutex);
	ei_send(listener->sockfd, &pid, rbuf->buff, rbuf->index);
	switch_mutex_unlock(listener->sock_mutex);
#ifdef EI_DEBUG
	ei_x_print_msg(rbuf, &pid, 1);
#endif

	return SWITCH_STATUS_FALSE;
}


int handle_msg(listener_t *listener, erlang_msg * msg, ei_x_buff * buf, ei_x_buff * rbuf)
{
	int type, type2, size, version, arity, tmpindex;
	switch_status_t ret = SWITCH_STATUS_SUCCESS;

	if (msg->msgtype == ERL_REG_SEND && !strncmp(msg->toname, "net_kernel", MAXATOMLEN)) {
		/* try to respond to ping stuff */
		ret = handle_net_kernel_msg(listener, msg, buf, rbuf);
	} else {
		buf->index = 0;
		ei_decode_version(buf->buff, &buf->index, &version);
		ei_get_type(buf->buff, &buf->index, &type, &size);

		switch (type) {
		case ERL_SMALL_TUPLE_EXT:
		case ERL_LARGE_TUPLE_EXT:
			tmpindex = buf->index;
			ei_decode_tuple_header(buf->buff, &tmpindex, &arity);
			ei_get_type(buf->buff, &tmpindex, &type2, &size);

			switch (type2) {
			case ERL_ATOM_EXT:
				ret = handle_msg_tuple(listener, msg, buf, rbuf);
				break;
			case ERL_REFERENCE_EXT:
			case ERL_NEW_REFERENCE_EXT:
				ret = handle_ref_tuple(listener, msg, buf, rbuf);
				break;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WEEEEEEEE %d %d\n", type, type2);
				/* some other kind of erlang term */
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "undef");
				break;
			}

			break;

		case ERL_ATOM_EXT:
			ret = handle_msg_atom(listener, msg, buf, rbuf);
			break;

		default:
			/* some other kind of erlang term */
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "undef");
			break;
		}
	}

	if (SWITCH_STATUS_FALSE == ret) {
		return 0;
	} else if (rbuf->index > 1) {
		switch_mutex_lock(listener->sock_mutex);
		ei_send(listener->sockfd, &msg->from, rbuf->buff, rbuf->index);
		switch_mutex_unlock(listener->sock_mutex);
#ifdef EI_DEBUG
		ei_x_print_msg(rbuf, &msg->from, 1);
#endif
		return SWITCH_STATUS_SUCCESS != ret;

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Empty reply, supressing\n");
		return 0;
	}
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
