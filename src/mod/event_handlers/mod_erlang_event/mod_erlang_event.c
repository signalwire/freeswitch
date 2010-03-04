/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * mod_erlang_event.c -- Erlang Event Handler derived from mod_event_socket
 *
 */
#include <switch.h>
#include <ei.h>
#define DEFINE_GLOBALS
#include "mod_erlang_event.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_erlang_event_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_erlang_event_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_erlang_event_runtime);
SWITCH_MODULE_DEFINITION(mod_erlang_event, mod_erlang_event_load, mod_erlang_event_shutdown, mod_erlang_event_runtime);

static switch_memory_pool_t *module_pool = NULL;

static void remove_listener(listener_t *listener);
static switch_status_t state_handler(switch_core_session_t *session);

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_ip, prefs.ip);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_cookie, prefs.cookie);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_nodename, prefs.nodename);

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj);
static void launch_listener_thread(listener_t *listener);

static switch_status_t socket_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	listener_t *l;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (switch_test_flag(l, LFLAG_LOG) && l->level >= node->level) {

			switch_log_node_t *dnode = switch_log_node_dup(node);

			if (switch_queue_trypush(l->log_queue, dnode) == SWITCH_STATUS_SUCCESS) {
				if (l->lost_logs) {
					int ll = l->lost_logs;
					switch_event_t *event;
					l->lost_logs = 0;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Lost %d log lines!\n", ll);
					if (switch_event_create(&event, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "info", "lost %d log lines", ll);
						switch_event_fire(&event);
					}
				}
			} else {
				switch_log_node_free(&dnode);
				l->lost_logs++;
			}
		}
	}
	switch_mutex_unlock(globals.listener_mutex);

	return SWITCH_STATUS_SUCCESS;
}


static void expire_listener(listener_t ** listener)
{
	void *pop;

	switch_thread_rwlock_unlock((*listener)->rwlock);
	switch_core_hash_destroy(&(*listener)->event_hash);
	switch_core_destroy_memory_pool(&(*listener)->pool);

	while (switch_queue_trypop((*listener)->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_event_t *pevent = (switch_event_t *) pop;
		switch_event_destroy(&pevent);
	}

	*listener = NULL;
}


static void remove_binding(listener_t *listener, erlang_pid * pid)
{
	struct erlang_binding *ptr, *lst = NULL;

	switch_mutex_lock(globals.listener_mutex);

	switch_xml_set_binding_sections(bindings.search_binding, SWITCH_XML_SECTION_MAX);

	for (ptr = bindings.head; ptr; lst = ptr, ptr = ptr->next) {
		if ((listener && ptr->listener == listener) || (pid && (ptr->process.type == ERLANG_PID) && !ei_compare_pids(&ptr->process.pid, pid))) {
			if (bindings.head == ptr) {
				if (ptr->next) {
					bindings.head = ptr->next;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removed all (only?) listeners\n");
					bindings.head = NULL;
					break;
				}
			} else {
				if (ptr->next) {
					lst->next = ptr->next;
				} else {
					lst->next = NULL;
				}
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removed listener\n");
		} else {
			switch_xml_set_binding_sections(bindings.search_binding, switch_xml_get_binding_sections(bindings.search_binding) | ptr->section);
		}
	}

	switch_mutex_unlock(globals.listener_mutex);
}


static void send_event_to_attached_sessions(listener_t *listener, switch_event_t *event)
{
	char *uuid = switch_event_get_header(event, "unique-id");
	switch_event_t *clone = NULL;
	session_elem_t *s;

	if (!uuid)
		return;
	switch_mutex_lock(listener->session_mutex);
	for (s = listener->session_list; s; s = s->next) {
		/* check the event uuid against the uuid of each session */
		if (!strcmp(uuid, s->uuid_str)) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(s->uuid_str), SWITCH_LOG_DEBUG, "Sending event %s to attached session for %s\n",
							  switch_event_name(event->event_id), s->uuid_str);
			if (switch_event_dup(&clone, event) == SWITCH_STATUS_SUCCESS) {
				/* add the event to the queue for this session */
				if (switch_queue_trypush(s->event_queue, clone) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(s->uuid_str), SWITCH_LOG_ERROR, "Lost event!\n");
					switch_event_destroy(&clone);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(s->uuid_str), SWITCH_LOG_ERROR, "Memory Error!\n");
			}
		}
	}
	switch_mutex_unlock(listener->session_mutex);
}

static void event_handler(switch_event_t *event)
{
	switch_event_t *clone = NULL;
	listener_t *l, *lp;

	switch_assert(event != NULL);

	if (!listen_list.ready) {
		return;
	}

	lp = listen_list.listeners;

	switch_mutex_lock(globals.listener_mutex);
	while (lp) {
		uint8_t send = 0;

		l = lp;
		lp = lp->next;

		/* test all of the sessions attached to this listener in case
		   one of them should receive the event as well
		 */

		send_event_to_attached_sessions(l, event);

		if (!switch_test_flag(l, LFLAG_EVENTS)) {
			continue;
		}

		if (switch_test_flag(l, LFLAG_STATEFUL) && l->timeout && switch_epoch_time_now(NULL) - l->last_flush > l->timeout) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Stateful Listener %u has expired\n", l->id);
			remove_listener(l);
			expire_listener(&l);
			continue;
		}

		if (l->event_list[SWITCH_EVENT_ALL]) {
			send = 1;
		} else if ((l->event_list[event->event_id])) {
			if (event->event_id != SWITCH_EVENT_CUSTOM || !event->subclass_name || (switch_core_hash_find(l->event_hash, event->subclass_name))) {
				send = 1;
			}
		}


		if (send) {
			if (switch_event_dup(&clone, event) == SWITCH_STATUS_SUCCESS) {
				if (switch_queue_trypush(l->event_queue, clone) == SWITCH_STATUS_SUCCESS) {
					if (l->lost_events) {
						int le = l->lost_events;
						l->lost_events = 0;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Lost %d events!\n", le);
						clone = NULL;
						if (switch_event_create(&clone, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
							switch_event_add_header(clone, SWITCH_STACK_BOTTOM, "info", "lost %d events", le);
							switch_event_fire(&clone);
						}
					}
				} else {
					l->lost_events++;
					switch_event_destroy(&clone);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			}
		}

	}
	switch_mutex_unlock(globals.listener_mutex);
}


#ifdef WIN32
static void close_socket(SOCKET * sock)
#else
static void close_socket(int *sock)
#endif
{
	if (*sock) {
#ifdef WIN32
		shutdown(*sock, SD_BOTH);
		closesocket(*sock);
#else
		shutdown(*sock, SHUT_RDWR);
		close(*sock);
#endif
		sock = NULL;
	}
}


static void add_listener(listener_t *listener)
{
	/* add me to the listeners so I get events */
	switch_mutex_lock(globals.listener_mutex);
	listener->next = listen_list.listeners;
	listen_list.listeners = listener;
	switch_mutex_unlock(globals.listener_mutex);
}


static void remove_listener(listener_t *listener)
{
	listener_t *l, *last = NULL;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (l == listener) {
			if (last) {
				last->next = l->next;
			} else {
				listen_list.listeners = l->next;
			}
		}
		last = l;
	}
	switch_mutex_unlock(globals.listener_mutex);
}

/* Search for a listener already talking to the specified node */
static listener_t *find_listener(char *nodename)
{
	listener_t *l = NULL;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (!strncmp(nodename, l->peer_nodename, MAXNODELEN)) {
			break;
		}
	}
	switch_mutex_unlock(globals.listener_mutex);
	return l;
}


static void add_session_elem_to_listener(listener_t *listener, session_elem_t *session_element)
{
	switch_mutex_lock(listener->session_mutex);
	session_element->next = listener->session_list;
	listener->session_list = session_element;
	switch_mutex_unlock(listener->session_mutex);
}


static void remove_session_elem_from_listener(listener_t *listener, session_elem_t *session_element)
{
	session_elem_t *s, *last = NULL;

	if (!session_element)
		return;

	for (s = listener->session_list; s; s = s->next) {
		if (s == session_element) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(session_element->uuid_str), SWITCH_LOG_DEBUG, "Removing session element for %s\n",
							  session_element->uuid_str);
			if (last) {
				last->next = s->next;
			} else {
				listener->session_list = s->next;
			}
			break;
		}
		last = s;
	}
}

static void destroy_session_elem(session_elem_t *session_element)
{
	switch_core_session_t *session;

	if ((session = switch_core_session_locate(session_element->uuid_str))) {
		switch_channel_clear_flag(switch_core_session_get_channel(session), CF_CONTROLLED);
		switch_core_session_rwunlock(session);
	}
	/* this allows the application threads to exit */
	switch_clear_flag_locked(session_element, LFLAG_SESSION_ALIVE);
	switch_core_destroy_memory_pool(&session_element->pool);
	/*switch_safe_free(s); */
}

static void remove_session_elem_from_listener_locked(listener_t *listener, session_elem_t *session_element)
{
	switch_mutex_lock(listener->session_mutex);
	remove_session_elem_from_listener(listener, session_element);
	switch_mutex_unlock(listener->session_mutex);
}


session_elem_t *find_session_elem_by_pid(listener_t *listener, erlang_pid * pid)
{
	session_elem_t *s = NULL;

	switch_mutex_lock(listener->session_mutex);
	for (s = listener->session_list; s; s = s->next) {
		if (s->process.type == ERLANG_PID && !ei_compare_pids(pid, &s->process.pid)) {
			break;
		}
	}
	switch_mutex_unlock(listener->session_mutex);

	return s;
}


static switch_xml_t erlang_fetch(const char *sectionstr, const char *tag_name, const char *key_name, const char *key_value,
								 switch_event_t *params, void *user_data)
{
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int type, size;
	fetch_reply_t *p = NULL;
	switch_time_t now = 0;
	char *xmlstr;
	struct erlang_binding *ptr;
	switch_uuid_t uuid;
	switch_xml_section_t section;
	switch_xml_t xml = NULL;
	ei_x_buff *rep;
	ei_x_buff buf;
	ei_x_new_with_version(&buf);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "looking for bindings\n");

	section = switch_xml_parse_section_string((char *) sectionstr);

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	for (ptr = bindings.head; ptr; ptr = ptr->next) {
		if (ptr->section != section)
			continue;

		if (!ptr->listener) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NULL pointer binding!\n");
			goto cleanup; /* our pointer is trash */
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "binding for %s in section %s with key %s and value %s requested from node %s\n", tag_name, sectionstr, key_name, key_value, ptr->process.pid.node);

		ei_x_encode_tuple_header(&buf, 7);
		ei_x_encode_atom(&buf, "fetch");
		ei_x_encode_atom(&buf, sectionstr);
		_ei_x_encode_string(&buf, tag_name ? tag_name : "undefined");
		_ei_x_encode_string(&buf, key_name ? key_name : "undefined");
		_ei_x_encode_string(&buf, key_value ? key_value : "undefined");
		_ei_x_encode_string(&buf, uuid_str);
		if (params) {
			ei_encode_switch_event_headers(&buf, params);
		} else {
			ei_x_encode_empty_list(&buf);
		}

		if (!p) {
			/* Create a new fetch object. */
			p = malloc(sizeof(*p));
			switch_thread_cond_create(&p->ready_or_found, module_pool);
			p->usecount = 1;
			p->state = reply_not_ready;
			p->reply = NULL;
			switch_core_hash_insert_locked(globals.fetch_reply_hash, uuid_str, p, globals.fetch_reply_mutex);
			now = switch_micro_time_now();
		}
		/* We don't need to lock here because everybody is waiting
		   on our condition before the action starts. */
		p->usecount ++;

		switch_mutex_lock(ptr->listener->sock_mutex);
		ei_sendto(ptr->listener->ec, ptr->listener->sockfd, &ptr->process, &buf);
		switch_mutex_unlock(ptr->listener->sock_mutex);
	}

	if (!p) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no binding for %s\n", sectionstr);
		goto cleanup;
	}

	/* Tell the threads to be ready, and wait five seconds for a reply. */
	switch_mutex_lock(globals.fetch_reply_mutex);
	p->state = reply_waiting;
	switch_thread_cond_broadcast(p->ready_or_found);
	switch_thread_cond_timedwait(p->ready_or_found,
			globals.fetch_reply_mutex, 5000000);
	if (!p->reply) {
		p->state = reply_timeout;
		switch_mutex_unlock(globals.fetch_reply_mutex);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Timed out after %d milliseconds when waiting for XML fetch response for %s\n", (int) (switch_micro_time_now() - now) / 1000, uuid_str);
		goto cleanup;
	}

	rep = p->reply;
	switch_mutex_unlock(globals.fetch_reply_mutex);

	ei_get_type(rep->buff, &rep->index, &type, &size);

	if (type != ERL_STRING_EXT && type != ERL_BINARY_EXT) {	/* XXX no unicode or character codes > 255 */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "XML fetch response contained non ASCII characters? (was type %d of size %d)\n", type,
						  size);
		goto cleanup;
	}


	if (!(xmlstr = malloc(size + 1))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error\n");
		goto cleanup;
	}

	ei_decode_string_or_binary(rep->buff, &rep->index, size, xmlstr);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got data %s after %d milliseconds from %s for %s!\n", xmlstr, (int) (switch_micro_time_now() - now) / 1000, p->winner, uuid_str);

	if (zstr(xmlstr)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Result\n");
	} else if (!(xml = switch_xml_parse_str_dynamic(xmlstr, SWITCH_FALSE))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing XML Result!\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "XML parsed OK!\n");
	}

	/* cleanup */
 cleanup:
	if (p) {
		switch_mutex_lock(globals.fetch_reply_mutex);
		put_reply_unlock(p, uuid_str);
	}

	return xml;
}


void put_reply_unlock(fetch_reply_t *p, char *uuid_str)
{
	if (-- p->usecount == 0) {
		switch_core_hash_delete(globals.fetch_reply_hash, uuid_str);
		switch_thread_cond_destroy(p->ready_or_found);
		if (p->reply) {
			switch_safe_free(p->reply->buff);
			switch_safe_free(p->reply);
		}
		switch_safe_free(p);
	}
	switch_mutex_unlock(globals.fetch_reply_mutex);
}


static switch_status_t notify_new_session(listener_t *listener, session_elem_t *session_element)
{
	int result;
	switch_core_session_t *session;
	switch_event_t *call_event = NULL;
	switch_channel_t *channel = NULL;
	ei_x_buff lbuf;

	/* Send a message to the associated registered process to let it know there is a call.
	   Message is a tuple of the form {call, <call-event>}
	 */

	if (switch_event_create(&call_event, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(session_element->uuid_str), SWITCH_LOG_CRIT, "Memory Error!\n");
		return SWITCH_STATUS_MEMERR;
	}

	if (!(session = switch_core_session_locate(session_element->uuid_str))) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(session_element->uuid_str), SWITCH_LOG_WARNING, "Can't locate session %s\n", session_element->uuid_str);
		return SWITCH_STATUS_FALSE;
	}

	channel = switch_core_session_get_channel(session);

	switch_caller_profile_event_set_data(switch_channel_get_caller_profile(channel), "Channel", call_event);
	switch_channel_event_set_data(channel, call_event);
	switch_core_session_rwunlock(session);
	switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Content-Type", "command/reply");
	switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Reply-Text", "+OK\n");

	ei_x_new_with_version(&lbuf);
	ei_x_encode_tuple_header(&lbuf, 2);
	ei_x_encode_atom(&lbuf, "call");
	ei_encode_switch_event(&lbuf, call_event);
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(session_element->uuid_str), SWITCH_LOG_DEBUG, "Sending initial call event for %s\n",
					  session_element->uuid_str);

	switch_mutex_lock(listener->sock_mutex);
	result = ei_sendto(listener->ec, listener->sockfd, &session_element->process, &lbuf);
	switch_mutex_unlock(listener->sock_mutex);

	if (result) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(session_element->uuid_str), SWITCH_LOG_ERROR, "Failed to send call event for %s\n",
						  session_element->uuid_str);
	}

	ei_x_free(&lbuf);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t check_attached_sessions(listener_t *listener)
{
	session_elem_t *last, *sp, *removed;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	void *pop;
	/* check up on all the attached sessions -
	   if they have not yet sent an initial call event to the associated erlang process then do so
	   if they have pending events in their queues then send them
	   if the session has finished then clean it up
	 */
	switch_mutex_lock(listener->session_mutex);
	sp = listener->session_list;
	last = NULL;
	while (sp) {
		removed = NULL;
		if (switch_test_flag(sp, LFLAG_WAITING_FOR_PID)) {
			sp = sp->next;
			continue;
		}

		if (!switch_test_flag(sp, LFLAG_OUTBOUND_INIT)) {
			status = notify_new_session(listener, sp);
			if (status != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(sp->uuid_str), SWITCH_LOG_DEBUG, "Notifying new session failed\n");
				removed = sp;
				sp = removed->next;

				remove_session_elem_from_listener(listener, removed);
				destroy_session_elem(removed);
				continue;
			}
			switch_set_flag(sp, LFLAG_OUTBOUND_INIT);
		}

		if (switch_test_flag(sp, LFLAG_SESSION_COMPLETE)) {
			ei_x_buff ebuf;

			/* flush the event queue */
			while (switch_queue_trypop(sp->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
				switch_event_t *pevent = (switch_event_t *) pop;

				/*switch_log_printf(SWITCH_CHANNEL_UUID_LOG(sp->uuid_str), SWITCH_LOG_DEBUG, "flushed event %s for %s\n", switch_event_name(pevent->event_id), sp->uuid_str); */

				/* events from attached sessions are wrapped in a {call_event,<EVT>} tuple 
				   to distinguish them from normal events (if they are sent to the same process)
				 */

				ei_x_new_with_version(&ebuf);
				ei_x_encode_tuple_header(&ebuf, 2);
				ei_x_encode_atom(&ebuf, "call_event");
				ei_encode_switch_event(&ebuf, pevent);

				switch_mutex_lock(listener->sock_mutex);
				ei_sendto(listener->ec, listener->sockfd, &sp->process, &ebuf);
				switch_mutex_unlock(listener->sock_mutex);
				ei_x_free(&ebuf);
				switch_event_destroy(&pevent);
			}
			/* this session can be removed */
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(sp->uuid_str), SWITCH_LOG_DEBUG, "Destroy event for attached session for %s in state %s\n",
							  sp->uuid_str, switch_channel_state_name(sp->channel_state));

			ei_x_new_with_version(&ebuf);
			ei_x_encode_atom(&ebuf, "call_hangup");
			switch_mutex_lock(listener->sock_mutex);
			ei_sendto(listener->ec, listener->sockfd, &sp->process, &ebuf);
			switch_mutex_unlock(listener->sock_mutex);
			ei_x_free(&ebuf);
			removed = sp;
			sp = removed->next;

			remove_session_elem_from_listener(listener, removed);
			destroy_session_elem(removed);
			continue;
		} else if (switch_queue_trypop(sp->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {

			/* check event queue for this session */
			switch_event_t *pevent = (switch_event_t *) pop;

			/*switch_log_printf(SWITCH_CHANNEL_UUID_LOG(sp->uuid_str), SWITCH_LOG_DEBUG, "popped event %s for %s\n", switch_event_name(pevent->event_id), sp->uuid_str); */

			/* events from attached sessions are wrapped in a {call_event,<EVT>} tuple 
			   to distinguish them from normal events (if they are sent to the same process)
			 */
			ei_x_buff ebuf;

			ei_x_new_with_version(&ebuf);
			ei_x_encode_tuple_header(&ebuf, 2);
			ei_x_encode_atom(&ebuf, "call_event");
			ei_encode_switch_event(&ebuf, pevent);

			switch_mutex_lock(listener->sock_mutex);
			ei_sendto(listener->ec, listener->sockfd, &sp->process, &ebuf);
			switch_mutex_unlock(listener->sock_mutex);
			ei_x_free(&ebuf);

			switch_event_destroy(&pevent);
		}
		sp = sp->next;
	}
	switch_mutex_unlock(listener->session_mutex);
	if (prefs.done) {
		return SWITCH_STATUS_FALSE;	/* we're shutting down */
	} else {
		return SWITCH_STATUS_SUCCESS;
	}
}

static void check_log_queue(listener_t *listener)
{
	void *pop;

	/* send out any pending crap in the log queue */
	if (switch_test_flag(listener, LFLAG_LOG)) {
		if (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_log_node_t *dnode = (switch_log_node_t *) pop;

			if (dnode->data) {
				ei_x_buff lbuf;
				ei_x_new_with_version(&lbuf);
				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "log");
				ei_x_encode_list_header(&lbuf, 7);

				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "level");
				ei_x_encode_char(&lbuf, (unsigned char) dnode->level);

				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "text_channel");
				ei_x_encode_char(&lbuf, (unsigned char) dnode->level);

				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "file");
				ei_x_encode_string(&lbuf, dnode->file);

				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "func");
				ei_x_encode_string(&lbuf, dnode->func);

				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "line");
				ei_x_encode_ulong(&lbuf, (unsigned long) dnode->line);

				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "data");
				ei_x_encode_string(&lbuf, dnode->data);

				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "user_data");
				ei_x_encode_string(&lbuf, switch_str_nil(dnode->userdata));

				ei_x_encode_empty_list(&lbuf);

				switch_mutex_lock(listener->sock_mutex);
				ei_sendto(listener->ec, listener->sockfd, &listener->log_process, &lbuf);
				switch_mutex_unlock(listener->sock_mutex);

				ei_x_free(&lbuf);
				switch_log_node_free(&dnode);
			}
		}
	}
}

static void check_event_queue(listener_t *listener)
{
	void *pop;

	/* send out any pending crap in the event queue */
	if (switch_test_flag(listener, LFLAG_EVENTS)) {
		if (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {

			switch_event_t *pevent = (switch_event_t *) pop;

			ei_x_buff ebuf;
			ei_x_new_with_version(&ebuf);

			ei_encode_switch_event(&ebuf, pevent);

			switch_mutex_lock(listener->sock_mutex);
			ei_sendto(listener->ec, listener->sockfd, &listener->event_process, &ebuf);
			switch_mutex_unlock(listener->sock_mutex);

			ei_x_free(&ebuf);
			switch_event_destroy(&pevent);
		}
	}
}

static void handle_exit(listener_t *listener, erlang_pid * pid)
{
	session_elem_t *s;

	remove_binding(NULL, pid);	/* TODO - why don't we pass the listener as the first argument? */
	if ((s = find_session_elem_by_pid(listener, pid))) {
		if (s->channel_state < CS_HANGUP) {
			switch_core_session_t *session;
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(s->uuid_str), SWITCH_LOG_WARNING, "Outbound session for %s exited unexpectedly!\n", s->uuid_str);

			if ((session = switch_core_session_locate(s->uuid_str))) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				switch_channel_set_private(channel, "_erlang_session_", NULL);
				switch_channel_set_private(channel, "_erlang_listener_", NULL);
				switch_core_event_hook_remove_state_change(session, state_handler);
				switch_core_session_rwunlock(session);
			}
			/* TODO - if a spawned process that was handling an outbound call fails.. what do we do with the call? */
		}
		remove_session_elem_from_listener_locked(listener, s);
		destroy_session_elem(s);
	}

	if (listener->log_process.type == ERLANG_PID && !ei_compare_pids(&listener->log_process.pid, pid)) {
		void *pop;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Log handler process for node %s exited\n", pid->node);
		/*purge the log queue */
		while (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_log_node_t *dnode = (switch_log_node_t *) pop;
			switch_log_node_free(&dnode);
		}

		if (switch_test_flag(listener, LFLAG_LOG)) {
			switch_clear_flag_locked(listener, LFLAG_LOG);
		}
	}

	if (listener->event_process.type == ERLANG_PID && !ei_compare_pids(&listener->event_process.pid, pid)) {
		void *pop;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Event handler process for node %s exited\n", pid->node);
		/*purge the event queue */
		while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *pevent = (switch_event_t *) pop;
			switch_event_destroy(&pevent);
		}

		if (switch_test_flag(listener, LFLAG_EVENTS)) {
			uint8_t x = 0;
			switch_clear_flag_locked(listener, LFLAG_EVENTS);
			for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
				listener->event_list[x] = 0;
			}
			/* wipe the hash */
			switch_core_hash_destroy(&listener->event_hash);
			switch_core_hash_init(&listener->event_hash, listener->pool);
		}
	}
}

static void listener_main_loop(listener_t *listener)
{
	int status = 1;

	while ((status >= 0 || erl_errno == ETIMEDOUT || erl_errno == EAGAIN) && !prefs.done) {
		erlang_msg msg;
		ei_x_buff buf;
		ei_x_buff rbuf;

		ei_x_new(&buf);
		ei_x_new_with_version(&rbuf);

		/* do we need the mutex when reading? */
		/*switch_mutex_lock(listener->sock_mutex); */
		status = ei_xreceive_msg_tmo(listener->sockfd, &msg, &buf, 100);
		/*switch_mutex_unlock(listener->sock_mutex); */

		switch (status) {
		case ERL_TICK:
			break;
		case ERL_MSG:
			switch (msg.msgtype) {
			case ERL_SEND:
#ifdef EI_DEBUG
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_send\n");

				ei_x_print_msg(&buf, &msg.from, 0);
				/*ei_s_print_term(&pbuf, buf.buff, &i); */
				/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_send was message %s\n", pbuf); */
#endif

				if (handle_msg(listener, &msg, &buf, &rbuf)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "handle_msg requested exit\n");
					return;
				}
				break;
			case ERL_REG_SEND:
#ifdef EI_DEBUG
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_reg_send to %s\n", msg.toname);

				ei_x_print_reg_msg(&buf, msg.toname, 0);
				/*i = 1; */
				/*ei_s_print_term(&pbuf, buf.buff, &i); */
				/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_reg_send was message %s\n", pbuf); */
#endif

				if (handle_msg(listener, &msg, &buf, &rbuf)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "handle_msg requested exit\n");
					return;
				}
				break;
			case ERL_LINK:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_link\n");
				break;
			case ERL_UNLINK:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_unlink\n");
				break;
			case ERL_EXIT:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_exit from %s <%d.%d.%d>\n", msg.from.node, msg.from.creation, msg.from.num,
								  msg.from.serial);
				handle_exit(listener, &msg.from);
				break;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unexpected msg type %d\n", (int) (msg.msgtype));
				break;
			}
			break;
		case ERL_ERROR:
			if (erl_errno != ETIMEDOUT && erl_errno != EAGAIN) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_error\n");
			}
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unexpected status %d \n", status);
			break;
		}

		ei_x_free(&buf);
		ei_x_free(&rbuf);

		check_log_queue(listener);
		check_event_queue(listener);
		if (check_attached_sessions(listener) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "check_attached_sessions requested exit\n");
			return;
		}
	}
}

static switch_bool_t check_inbound_acl(listener_t *listener)
{
	/* check acl to see if inbound connection is allowed */
	if (prefs.acl_count && !zstr(listener->remote_ip)) {
		uint32_t x = 0;
		for (x = 0; x < prefs.acl_count; x++) {
			if (!switch_check_network_list_ip(listener->remote_ip, prefs.acl[x])) {
				int status = 1;
				erlang_msg msg;

				ei_x_buff buf;
				ei_x_new(&buf);

				status = ei_xreceive_msg(listener->sockfd, &msg, &buf);
				/* get data off the socket, just so we can get the pid on the other end */
				if (status == ERL_MSG) {
					/* if we got a message, return an ACL error. */
					ei_x_buff rbuf;
					ei_x_new_with_version(&rbuf);

					ei_x_encode_tuple_header(&rbuf, 2);
					ei_x_encode_atom(&rbuf, "error");
					ei_x_encode_atom(&rbuf, "acldeny");

					switch_mutex_lock(listener->sock_mutex);
					ei_send(listener->sockfd, &msg.from, rbuf.buff, rbuf.index);
					switch_mutex_unlock(listener->sock_mutex);
#ifdef EI_DEBUG
					ei_x_print_msg(&rbuf, &msg.from, 1);
#endif

					ei_x_free(&rbuf);
				}

				ei_x_free(&buf);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection from %s denied by acl %s\n", listener->remote_ip, prefs.acl[x]);
				return SWITCH_FALSE;
			}
		}
	}
	return SWITCH_TRUE;
}

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj)
{
	listener_t *listener = (listener_t *) obj;
	session_elem_t *s;
	switch_core_session_t *session;

	switch_mutex_lock(globals.listener_mutex);
	prefs.threads++;
	switch_mutex_unlock(globals.listener_mutex);

	switch_assert(listener != NULL);

	if (check_inbound_acl(listener)) {
		if (zstr(listener->remote_ip)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open from %s\n", listener->remote_ip);	/*, listener->remote_port); */
		}

		add_listener(listener);
		listener_main_loop(listener);
	}

	/* clean up */
	remove_listener(listener);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Session complete, waiting for children\n");

	switch_thread_rwlock_wrlock(listener->rwlock);

	if (listener->sockfd) {
		close_socket(&listener->sockfd);
	}

	switch_thread_rwlock_unlock(listener->rwlock);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Closed\n");
	switch_core_hash_destroy(&listener->event_hash);

	/* remove any bindings for this connection */
	remove_binding(listener, NULL);

	/* clean up all the attached sessions */
	switch_mutex_lock(listener->session_mutex);
	/* TODO destroy memory pools since they're not children of the listener's pool */
	for (s = listener->session_list; s; s = s->next) {
		if ((session = switch_core_session_locate(s->uuid_str))) {
			switch_channel_clear_flag(switch_core_session_get_channel(session), CF_CONTROLLED);
			switch_core_session_rwunlock(session);
		}
		/* this allows the application threads to exit */
		switch_clear_flag_locked(s, LFLAG_SESSION_ALIVE);
	}
	switch_mutex_unlock(listener->session_mutex);

	if (listener->pool) {
		switch_memory_pool_t *pool = listener->pool;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(globals.listener_mutex);
	prefs.threads--;
	switch_mutex_unlock(globals.listener_mutex);

	return NULL;
}


/* Create a thread for the socket and launch it */
static void launch_listener_thread(listener_t *listener)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, listener->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, listener_run, listener, listener->pool);

}


static int config(void)
{
	char *cf = "erlang_event.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&prefs, 0, sizeof(prefs));

	prefs.shortname = SWITCH_TRUE;
	prefs.encoding = ERLANG_STRING;
	prefs.compat_rel = 0;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
	} else {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcmp(var, "listen-ip")) {
					set_pref_ip(val);
				} else if (!strcmp(var, "listen-port")) {
					prefs.port = (uint16_t) atoi(val);
				} else if (!strcmp(var, "cookie")) {
					set_pref_cookie(val);
				} else if (!strcmp(var, "nodename")) {
					set_pref_nodename(val);
				} else if (!strcmp(var, "compat-rel")) {
					if (atoi(val) >= 7)
						prefs.compat_rel = atoi(val);
					else
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid compatability release '%s' specified\n", val);
				} else if (!strcmp(var, "shortname")) {
					prefs.shortname = switch_true(val);
				} else if (!strcmp(var, "encoding")) {
					if (!strcasecmp(val, "string")) {
						prefs.encoding = ERLANG_STRING;
					} else if (!strcasecmp(val, "binary")) {
						prefs.encoding = ERLANG_BINARY;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid encoding strategy '%s' specified\n", val);
					}
				} else if (!strcasecmp(var, "apply-inbound-acl")) {
					if (prefs.acl_count < MAX_ACL) {
						prefs.acl[prefs.acl_count++] = strdup(val);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", MAX_ACL);
					}
				}
			}
		}
		switch_xml_free(xml);
	}

	if (zstr(prefs.ip)) {
		set_pref_ip("0.0.0.0");
	}

	if (zstr(prefs.cookie)) {
		set_pref_cookie("ClueCon");
	}

	if (!prefs.port) {
		prefs.port = 8031;
	}

	if (!prefs.nodename) {
		set_pref_nodename("freeswitch");
	}

	return 0;
}


static listener_t *new_listener(struct ei_cnode_s *ec, int clientfd)
{
	switch_memory_pool_t *listener_pool = NULL;
	listener_t *listener = NULL;

	if (switch_core_new_memory_pool(&listener_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return NULL;
	}

	if (!(listener = switch_core_alloc(listener_pool, sizeof(*listener)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
		return NULL;
	}
	memset(listener, 0, sizeof(*listener));

	switch_thread_rwlock_create(&listener->rwlock, listener_pool);
	switch_queue_create(&listener->event_queue, SWITCH_CORE_QUEUE_LEN, listener_pool);
	switch_queue_create(&listener->log_queue, SWITCH_CORE_QUEUE_LEN, listener_pool);

	listener->sockfd = clientfd;
	listener->pool = listener_pool;
	listener_pool = NULL;
	listener->ec = switch_core_alloc(listener->pool, sizeof(ei_cnode));
	memcpy(listener->ec, ec, sizeof(ei_cnode));
	listener->level = SWITCH_LOG_DEBUG;
	switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);
	switch_mutex_init(&listener->sock_mutex, SWITCH_MUTEX_NESTED, listener->pool);
	switch_mutex_init(&listener->session_mutex, SWITCH_MUTEX_NESTED, listener->pool);
	switch_core_hash_init(&listener->event_hash, listener->pool);

	return listener;
}

static listener_t *new_outbound_listener(char *node)
{
	listener_t *listener = NULL;
	struct ei_cnode_s ec;
	int clientfd;

	if (SWITCH_STATUS_SUCCESS == initialise_ei(&ec)) {
#ifdef WIN32
		WSASetLastError(0);
#else
		errno = 0;
#endif
		if ((clientfd = ei_connect(&ec, node)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error connecting to node %s (erl_errno=%d, errno=%d)!\n", node, erl_errno, errno);
			return NULL;
		}
		listener = new_listener(&ec, clientfd);
		listener->peer_nodename = switch_core_strdup(listener->pool, node);
	}
	return listener;
}

static switch_status_t state_handler(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	session_elem_t *session_element = switch_channel_get_private(channel, "_erlang_session_");
	/*listener_t* listener = switch_channel_get_private(channel, "_erlang_listener_"); */

	if (session_element) {
		session_element->channel_state = state;
		if (state == CS_DESTROY) {
			/* indicate that once all the events in the event queue are done
			 * we can throw this away */
			switch_set_flag(session_element, LFLAG_SESSION_COMPLETE);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unable to update channel state for %s to %s\n", switch_core_session_get_uuid(session),
						  switch_channel_state_name(state));
	}

	return SWITCH_STATUS_SUCCESS;
}

session_elem_t *session_elem_create(listener_t *listener, switch_core_session_t *session)
{
	/* create a session list element */
	switch_memory_pool_t *session_elem_pool;
	session_elem_t *session_element;	/* = malloc(sizeof(*session_element)); */
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_core_new_memory_pool(&session_elem_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return NULL;
	}

	session_element = switch_core_alloc(session_elem_pool, sizeof(*session_element));

	memset(session_element, 0, sizeof(*session_element));

	memcpy(session_element->uuid_str, switch_core_session_get_uuid(session), SWITCH_UUID_FORMATTED_LENGTH);
	session_element->pool = session_elem_pool;
	session_elem_pool = NULL;

	switch_queue_create(&session_element->event_queue, SWITCH_CORE_QUEUE_LEN, session_element->pool);
	switch_mutex_init(&session_element->flag_mutex, SWITCH_MUTEX_NESTED, session_element->pool);

	switch_channel_set_private(channel, "_erlang_session_", session_element);
	switch_channel_set_private(channel, "_erlang_listener_", listener);

	switch_core_event_hook_add_state_change(session, state_handler);

	return session_element;
}

session_elem_t *attach_call_to_registered_process(listener_t *listener, char *reg_name, switch_core_session_t *session)
{
	/* create a session list element */
	session_elem_t *session_element = session_elem_create(listener, session);

	session_element->process.type = ERLANG_REG_PROCESS;
	session_element->process.reg_name = switch_core_session_strdup(session, reg_name);
	switch_set_flag(session_element, LFLAG_SESSION_ALIVE);
	/* attach the session to the listener */
	add_session_elem_to_listener(listener, session_element);

	return session_element;
}

session_elem_t *attach_call_to_pid(listener_t *listener, erlang_pid * pid, switch_core_session_t *session)
{
	/* create a session list element */
	session_elem_t *session_element = session_elem_create(listener, session);

	session_element->process.type = ERLANG_PID;
	memcpy(&session_element->process.pid, pid, sizeof(erlang_pid));
	switch_set_flag(session_element, LFLAG_SESSION_ALIVE);
	/* attach the session to the listener */
	add_session_elem_to_listener(listener, session_element);
	ei_link(listener, ei_self(listener->ec), pid);

	return session_element;
}

session_elem_t *attach_call_to_spawned_process(listener_t *listener, char *module, char *function, switch_core_session_t *session)
{
	/* create a session list element */
	session_elem_t *session_element = session_elem_create(listener, session);
	char hash[100];
	int i = 0;
	void *p = NULL;
	erlang_pid *pid;
	erlang_ref ref;

	switch_set_flag(session_element, LFLAG_WAITING_FOR_PID);

	/* attach the session to the listener */
	add_session_elem_to_listener(listener, session_element);

	ei_init_ref(listener->ec, &ref);
	ei_hash_ref(&ref, hash);
	/* insert the waiting marker */
	switch_core_hash_insert(listener->spawn_pid_hash, hash, &globals.WAITING);

	if (!strcmp(function, "!")) {
		/* send a message to request a pid */
		ei_x_buff rbuf;
		ei_x_new_with_version(&rbuf);

		ei_x_encode_tuple_header(&rbuf, 4);
		ei_x_encode_atom(&rbuf, "get_pid");
		_ei_x_encode_string(&rbuf, switch_core_session_get_uuid(session));
		ei_x_encode_ref(&rbuf, &ref);
		ei_x_encode_pid(&rbuf, ei_self(listener->ec));
		/* should lock with mutex? */
		ei_reg_send(listener->ec, listener->sockfd, module, rbuf.buff, rbuf.index);
#ifdef EI_DEBUG
		ei_x_print_reg_msg(&rbuf, module, 1);
#endif
		ei_x_free(&rbuf);

		ei_x_new_with_version(&rbuf);
		ei_x_encode_tuple_header(&rbuf, 3);
		ei_x_encode_atom(&rbuf, "new_pid");
		ei_x_encode_ref(&rbuf, &ref);
		ei_x_encode_pid(&rbuf, ei_self(listener->ec));
		/* should lock with mutex? */
		ei_reg_send(listener->ec, listener->sockfd, module, rbuf.buff, rbuf.index);
#ifdef EI_DEBUG
		ei_x_print_reg_msg(&rbuf, module, 1);
#endif

		ei_x_free(&rbuf);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "rpc call: %s:%s(Ref)\n", module, function);
		/* should lock with mutex? */
		ei_pid_from_rpc(listener->ec, listener->sockfd, &ref, module, function);
		/*
		   char *argv[1];
		   ei_spawn(listener->ec, listener->sockfd, &ref, module, function, 0, argv);
		 */
	}

	/* loop until either we timeout or we get a value that's not the waiting marker */
	while (!(p = switch_core_hash_find(listener->spawn_pid_hash, hash)) || p == &globals.WAITING) {
		if (i > 50) {			/* half a second timeout */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Timed out when waiting for outbound pid\n");
			remove_session_elem_from_listener_locked(listener, session_element);
			switch_core_hash_insert(listener->spawn_pid_hash, hash, &globals.TIMEOUT);	/* TODO lock this? */
			destroy_session_elem(session_element);
			return NULL;
		}
		i++;
		switch_yield(10000);	/* 10ms */
	}

	switch_core_hash_delete(listener->spawn_pid_hash, hash);

	pid = (erlang_pid *) p;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got pid!\n");

	session_element->process.type = ERLANG_PID;
	memcpy(&session_element->process.pid, pid, sizeof(erlang_pid));

	switch_set_flag(session_element, LFLAG_SESSION_ALIVE);
	switch_clear_flag(session_element, LFLAG_OUTBOUND_INIT);
	switch_clear_flag(session_element, LFLAG_WAITING_FOR_PID);

	ei_link(listener, ei_self(listener->ec), pid);
	switch_safe_free(pid);		/* malloced in handle_ref_tuple */

	return session_element;
}


int count_listener_sessions(listener_t *listener)
{
	session_elem_t *last, *sp;
	int count = 0;

	switch_mutex_lock(listener->session_mutex);
	sp = listener->session_list;
	last = NULL;
	while (sp) {
		count++;
		last = sp;
		sp = sp->next;
	}

	switch_mutex_unlock(listener->session_mutex);

	return count;
}


/* Module Hooks */

/* Entry point for outbound mode */
SWITCH_STANDARD_APP(erlang_outbound_function)
{
	char *reg_name = NULL, *node, *module = NULL, *function = NULL;
	listener_t *listener;
	int argc = 0, argc2 = 0;
	char *argv[80] = { 0 }, *argv2[80] = {
	0};
	char *mydata, *myarg;
	char uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_bool_t new_session = SWITCH_FALSE;
	session_elem_t *session_element = NULL;

	/* process app arguments */
	if (data && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	/* XXX else? */
	memcpy(uuid, switch_core_session_get_uuid(session), SWITCH_UUID_FORMATTED_LENGTH);

	if (argc < 2) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Parse Error - need registered name and node!\n");
		return;
	}
	if (zstr(argv[0])) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Missing registered name or module:function!\n");
		return;
	}

	if ((myarg = switch_core_session_strdup(session, argv[0]))) {
		argc2 = switch_separate_string(myarg, ':', argv2, (sizeof(argv2) / sizeof(argv2[0])));
	}

	if (argc2 == 2) {
		/* mod:fun style */
		module = argv2[0];
		function = argv2[1];
	} else {
		/* registered name style */
		reg_name = argv[0];
	}


	node = argv[1];
	if (zstr(node)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Missing node name!\n");
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "enter erlang_outbound_function %s %s\n", argv[0], node);

	/* first work out if there is a listener already talking to the node we want to talk to */
	listener = find_listener(node);
	/* if there is no listener, then create one */
	if (!listener) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Creating new listener for session\n");
		new_session = SWITCH_TRUE;
		listener = new_outbound_listener(node);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Using existing listener for session\n");
	}

	if (listener) {
		if (new_session == SWITCH_TRUE) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Launching new listener\n");
			launch_listener_thread(listener);
		}

		if (module && function) {
			switch_core_hash_init(&listener->spawn_pid_hash, listener->pool);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Creating new spawned session for listener\n");
			session_element = attach_call_to_spawned_process(listener, module, function, session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Creating new registered session for listener\n");
			session_element = attach_call_to_registered_process(listener, reg_name, session);
		}

		if (session_element) {

			switch_ivr_park(session, NULL);

			/* keep app thread running for lifetime of session */
			if (switch_channel_down(switch_core_session_get_channel(session))) {
				if ((session_element = switch_channel_get_private(switch_core_session_get_channel(session), "_erlang_session_"))) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "outbound session all done\n");
					switch_clear_flag_locked(session_element, LFLAG_SESSION_ALIVE);
				} else {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "outbound session already done\n");
				}
			}
		}
	}
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "exit erlang_outbound_function\n");
}


/* Entry point for sendmsg */
SWITCH_STANDARD_APP(erlang_sendmsg_function)
{
	char *reg_name = NULL, *node;
	int argc = 0;
	char *argv[3] = { 0 };
	char *mydata;
	ei_x_buff buf;
	listener_t *listener;

	ei_x_new_with_version(&buf);

	/* process app arguments */
	if (data && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, 3);
	}
	/* XXX else? */
	if (argc < 3) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Parse Error - need node, registered name and message!\n");
		return;
	}

	reg_name = argv[0];
	node = argv[1];

	/*switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "sendmsg: {%s, %s} ! %s\n", reg_name, node, argv[2]); */

	ei_x_encode_tuple_header(&buf, 2);
	ei_x_encode_atom(&buf, "freeswitch_sendmsg");
	_ei_x_encode_string(&buf, argv[2]);

	listener = find_listener(node);
	if (!listener) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Creating new listener for sendmsg %s\n", node);
		listener = new_outbound_listener(node);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Using existing listener for sendmsg to %s\n", node);
	}

	if (listener) {
		ei_reg_send(listener->ec, listener->sockfd, reg_name, buf.buff, buf.index);
	}
}


/* 'erlang' console stuff */
SWITCH_STANDARD_API(erlang_cmd)
{
	char *argv[1024] = { 0 };
	int argc = 0;
	char *mycmd = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	const char *usage_string = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"erlang listeners\n" "erlang sessions <node_name>\n" "--------------------------------------------------------------------------------\n";

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}


	if (!(mycmd = strdup(cmd))) {
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!strcasecmp(argv[0], "listeners")) {

		listener_t *l;
		switch_mutex_lock(globals.listener_mutex);

		if (listen_list.listeners) {
			for (l = listen_list.listeners; l; l = l->next) {
				stream->write_function(stream, "Listener to %s with %d outbound sessions\n", l->peer_nodename, count_listener_sessions(l));
			}
		} else {
			stream->write_function(stream, "No active listeners\n");
		}

		switch_mutex_unlock(globals.listener_mutex);
	} else if (!strcasecmp(argv[0], "sessions") && argc == 2) {
		listener_t *l;
		int found = 0;

		switch_mutex_lock(globals.listener_mutex);
		for (l = listen_list.listeners; l; l = l->next) {
			if (!strcasecmp(l->peer_nodename, argv[1])) {
				session_elem_t *sp;

				found = 1;
				switch_mutex_lock(l->session_mutex);
				if ((sp = l->session_list)) {
					while (sp) {
						stream->write_function(stream, "Outbound session for %s in state %s\n", sp->uuid_str,
											   switch_channel_state_name(sp->channel_state));
						sp = sp->next;
					}
				} else {
					stream->write_function(stream, "No active sessions for %s\n", argv[1]);
				}
				switch_mutex_unlock(l->session_mutex);
				break;
			}
		}
		switch_mutex_unlock(globals.listener_mutex);

		if (!found)
			stream->write_function(stream, "Could not find a listener for %s\n", argv[1]);

	} else {
		stream->write_function(stream, "USAGE: erlang sessions <nodename>\n");
		goto done;
	}

  done:
	switch_safe_free(mycmd);
	return status;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_erlang_event_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	module_pool = pool;

	memset(&prefs, 0, sizeof(prefs));

	switch_mutex_init(&globals.listener_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&globals.fetch_reply_mutex, SWITCH_MUTEX_DEFAULT, pool);
	switch_core_hash_init(&globals.fetch_reply_hash, pool);

	/* intialize the unique reference stuff */
	switch_mutex_init(&globals.ref_mutex, SWITCH_MUTEX_NESTED, pool);
	globals.reference0 = 0;
	globals.reference1 = 0;
	globals.reference2 = 0;

	if (switch_event_bind_removable(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind to all events!\n");
		close_socket(&listen_list.sockfd);
		return SWITCH_STATUS_GENERR;
	}

	switch_log_bind_logger(socket_logger, SWITCH_LOG_DEBUG, SWITCH_FALSE);

	memset(&bindings, 0, sizeof(bindings));

	if (switch_xml_bind_search_function_ret(erlang_fetch, SWITCH_XML_SECTION_MAX, NULL, &bindings.search_binding) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't set up xml search bindings!\n");
		close_socket(&listen_list.sockfd);
		return SWITCH_STATUS_GENERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sections %d\n", switch_xml_get_binding_sections(bindings.search_binding));

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "erlang", "Yield call control to an erlang process", "Connect to erlang", erlang_outbound_function,
				   "<registered name> <node@host>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "erlang_sendmsg", "Send a message to an erlang process", "Connect to erlang", erlang_sendmsg_function,
				   "<registered name> <node@host> <message>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_API(api_interface, "erlang", "erlang information", erlang_cmd, "<command> [<args>]");
	switch_console_set_complete("add erlang listeners");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_RUNTIME_FUNCTION(mod_erlang_event_runtime)
{
	switch_memory_pool_t *pool = NULL, *listener_pool = NULL;
	switch_status_t rv;
	listener_t *listener;
	uint32_t x = 0;
	struct ei_cnode_s ec;
	ErlConnect conn;
	struct sockaddr_in server_addr;
	int on = 1;
	int clientfd;
	int epmdfd;
#ifdef WIN32
	/* borrowed from MSDN, stupid winsock */
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Winsock initialization failed, oh well\n");
		return SWITCH_STATUS_TERM;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Your winsock version doesn't support the 2.2 specification, bailing\n");
		return SWITCH_STATUS_TERM;
	}
#endif

	memset(&listen_list, 0, sizeof(listen_list));
	config();

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	/* zero out the struct before we use it */
	memset(&server_addr, 0, sizeof(server_addr));

	/* convert the configured IP to network byte order, handing errors */
	rv = switch_inet_pton(AF_INET, prefs.ip, &server_addr.sin_addr.s_addr);
	if (rv == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not parse invalid ip address: %s\n", prefs.ip);
		goto init_failed;
	} else if (rv == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error when parsing ip address %s : %s\n", prefs.ip, strerror(errno));
		goto init_failed;
	}

	/* set the address family and port */
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(prefs.port);

	/* do the socket setup ei is too lazy to do for us */
	for (;;) {

		if ((listen_list.sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate socket on %s:%u\n", prefs.ip, prefs.port);
			goto sock_fail;
		}
#ifdef WIN32
		if (setsockopt(listen_list.sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on))) {
#else
		if (setsockopt(listen_list.sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
#endif
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to enable SO_REUSEADDR for socket on %s:%u : %s\n", prefs.ip, prefs.port,
							  strerror(errno));
			goto sock_fail;
		}

		if (bind(listen_list.sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to bind to %s:%u\n", prefs.ip, prefs.port);
			goto sock_fail;
		}

		if (listen(listen_list.sockfd, 5) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to listen on %s:%u\n", prefs.ip, prefs.port);
			goto sock_fail;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Socket %d up listening on %s:%u\n", listen_list.sockfd, prefs.ip, prefs.port);
		break;
	  sock_fail:
		switch_yield(100000);
	}

	if (prefs.compat_rel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Compatability with OTP R%d requested\n", prefs.compat_rel);
		ei_set_compat_rel(prefs.compat_rel);
	}

	if (SWITCH_STATUS_SUCCESS != initialise_ei(&ec)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to init ei connection\n");
		goto init_failed;
	}

	/* return value is -1 for error, a descriptor pointing to epmd otherwise */
	if ((epmdfd = ei_publish(&ec, prefs.port)) == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to publish port to empd, trying to start empd via system()\n");
		if (system("epmd -daemon")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "Failed to start empd manually! Is epmd in $PATH? If not, start it yourself or run an erl shell with -sname or -name\n");
			goto init_failed;
		}
		switch_yield(100000);
		if ((epmdfd = ei_publish(&ec, prefs.port)) == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to publish port to empd AGAIN\n");
			goto init_failed;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connected to epmd and published erlang cnode at %s\n", ec.thisnodename);

	listen_list.ready = 1;

	for (;;) {
		/* zero out errno because ei_accept doesn't differentiate between a
		 * failed authentication or a socket failure, or a client version
		 * mismatch or a godzilla attack */
#ifdef WIN32
		WSASetLastError(0);
#else
		errno = 0;
#endif
		if ((clientfd = ei_accept_tmo(&ec, (int) listen_list.sockfd, &conn, 100)) == ERL_ERROR) {
			if (prefs.done) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Shutting Down\n");
			} else if (erl_errno == ETIMEDOUT) {
				continue;
#ifdef WIN32
			} else if (WSAGetLastError()) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error %d %d\n", erl_errno, WSAGetLastError());
#else
			} else if (errno) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error %d %d\n", erl_errno, errno);
#endif
			} else {
				/* if errno didn't get set, assume nothing *too* horrible occured */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
								  "Ignorable error in ei_accept - probable bad client version, bad cookie or bad nodename\n");
				continue;
			}
			break;
		}

		listener = new_listener(&ec, clientfd);
		if (listener) {
			/* store the IP and node name we are talking with */
			switch_inet_ntop(AF_INET, conn.ipadr, listener->remote_ip, sizeof(listener->remote_ip));
			listener->peer_nodename = switch_core_strdup(listener->pool, conn.nodename);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Launching listener, connection from node %s, ip %s\n", conn.nodename,
							  listener->remote_ip);
			launch_listener_thread(listener);
		} else
			/* if we fail to create a listener (memory error), then the module will exit */
			break;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Exiting module mod_erlang_event\n");

	/* cleanup epmd registration */
	ei_unpublish(&ec);
	close_socket(&epmdfd);

  init_failed:
	close_socket(&listen_list.sockfd);
	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}
	if (listener_pool) {
		switch_core_destroy_memory_pool(&listener_pool);
	}
	for (x = 0; x < prefs.acl_count; x++) {
		switch_safe_free(prefs.acl[x]);
	}

	prefs.done = 2;
	return SWITCH_STATUS_TERM;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_erlang_event_shutdown)
{
	listener_t *l;
	int sanity = 0;

	if (prefs.done == 0)		/* main thread might already have exited */
		prefs.done = 1;

	switch_log_unbind_logger(socket_logger);

	/*close_socket(&listen_list.sockfd); */

	while (prefs.threads || prefs.done == 1) {
		switch_yield(10000);
		if (++sanity == 1000) {
			break;
		}
	}

	switch_event_unbind(&globals.node);
	switch_xml_unbind_search_function_ptr(erlang_fetch);

	switch_mutex_lock(globals.listener_mutex);

	for (l = listen_list.listeners; l; l = l->next) {
		close_socket(&l->sockfd);
	}

#ifdef WIN32
	WSACleanup();
#endif

	switch_mutex_unlock(globals.listener_mutex);

	switch_sleep(1500000);		/* sleep for 1.5 seconds */

	switch_safe_free(prefs.ip);
	switch_safe_free(prefs.cookie);
	switch_safe_free(prefs.nodename);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
