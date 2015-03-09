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
 * Tamas Cseke <tamas.cseke@virtual-call-center.eu>
 * Seven Du <dujinfang@gmail.com>
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
static void destroy_listener(listener_t *listener);
static switch_status_t state_handler(switch_core_session_t *session);

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_ip, prefs.ip);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_cookie, prefs.cookie);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_nodename, prefs.nodename);

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj);
static void launch_listener_thread(listener_t *listener);

session_elem_t *find_session_elem_by_uuid(listener_t *listener, const char *uuid);

static switch_status_t socket_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	listener_t *l;

	switch_thread_rwlock_rdlock(globals.listener_rwlock);
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
	switch_thread_rwlock_unlock(globals.listener_rwlock);

	return SWITCH_STATUS_SUCCESS;
}


static void remove_binding(listener_t *listener, erlang_pid * pid)
{
	struct erlang_binding *ptr, *lst = NULL;

	switch_thread_rwlock_wrlock(globals.bindings_rwlock);

	switch_xml_set_binding_sections(bindings.search_binding, SWITCH_XML_SECTION_MAX);

	for (ptr = bindings.head; ptr; lst = ptr, ptr = ptr->next) {
		if ((listener && ptr->listener == listener) || (pid && (ptr->process.type == ERLANG_PID) && !ei_compare_pids(&ptr->process.pid, pid))) {
			if (bindings.head == ptr) {
				if (ptr->next) {
					bindings.head = ptr->next;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removed all (only?) binding\n");
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removed binding\n");
		} else {
			switch_xml_set_binding_sections(bindings.search_binding, switch_xml_get_binding_sections(bindings.search_binding) | ptr->section);
		}
	}

	switch_thread_rwlock_unlock(globals.bindings_rwlock);
}


static void send_event_to_attached_sessions(listener_t *listener, switch_event_t *event)
{
	char *uuid = switch_event_get_header(event, "unique-id");
	switch_event_t *clone = NULL;
	session_elem_t *s = NULL;

	if (!uuid) {
		return;
	}

	if ((s = (session_elem_t*)find_session_elem_by_uuid(listener, uuid))) {
		int send = 0;

		switch_thread_rwlock_rdlock(s->event_rwlock);

		if (s->event_list[SWITCH_EVENT_ALL]) {
			send = 1;
		} else if ((s->event_list[event->event_id])) {
			if (event->event_id != SWITCH_EVENT_CUSTOM || !event->subclass_name || (switch_core_hash_find(s->event_hash, event->subclass_name))) {
				send = 1;
			}
		}

		switch_thread_rwlock_unlock(s->event_rwlock);

		if (send) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(s->uuid_str), SWITCH_LOG_DEBUG, "Sending event %s to attached session %s\n",
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
		} else {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(s->uuid_str), SWITCH_LOG_DEBUG, "Ignoring event %s for attached session %s\n",
					switch_event_name(event->event_id), s->uuid_str);
		}
		switch_thread_rwlock_unlock(s->rwlock);
	}

}

static void event_handler(switch_event_t *event)
{
	switch_event_t *clone = NULL;
	listener_t *l, *lp;

	switch_assert(event != NULL);

	if (!listen_list.ready) {
		return;
	}

	switch_thread_rwlock_rdlock(globals.listener_rwlock);

	lp = listen_list.listeners;

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

		switch_thread_rwlock_rdlock(l->event_rwlock);

		if (l->event_list[SWITCH_EVENT_ALL]) {
			send = 1;
		} else if ((l->event_list[event->event_id])) {
			if (event->event_id != SWITCH_EVENT_CUSTOM || !event->subclass_name || (switch_core_hash_find(l->event_hash, event->subclass_name))) {
				send = 1;
			}
		}

		if (send) {
			switch_mutex_lock(l->filter_mutex);

			if (l->filters && l->filters->headers) {
				switch_event_header_t *hp;
				const char *hval;

				for (hp = l->filters->headers; hp; hp = hp->next) {
					if ((hval = switch_event_get_header(event, hp->name))) {
						const char *comp_to = hp->value;
						int pos = 1, cmp = 0;

						while (comp_to && *comp_to) {
							if (*comp_to == '+') {
								pos = 1;
							} else if (*comp_to == '-') {
								pos = 0;
							} else if (*comp_to != ' ') {
								break;
							}
							comp_to++;
						}

						if (!(comp_to && *comp_to)) {
							if (pos) {
								send = 1;
								continue;
							} else {
								send = 0;
								break;
							}
						}

						if (*hp->value == '/') {
							switch_regex_t *re = NULL;
							int ovector[30];
							cmp = !!switch_regex_perform(hval, comp_to, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
							switch_regex_safe_free(re);
						} else {
							cmp = !strcasecmp(hval, comp_to);
						}

						if (cmp) {
							if (pos) {
								send = 1;
							} else {
								send = 0;
								break;
							}
						} else {
							if (pos) {
								send = 0;
								break;
							} else {
								send = 1;
							}
						}
					}
				}
			}

			switch_mutex_unlock(l->filter_mutex);
		}

		switch_thread_rwlock_unlock(l->event_rwlock);

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
	switch_thread_rwlock_unlock(globals.listener_rwlock);
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
	/*	add me to the listeners so I get events */
	switch_thread_rwlock_wrlock(globals.listener_rwlock);
	listener->next = listen_list.listeners;
	listen_list.listeners = listener;
	switch_thread_rwlock_unlock(globals.listener_rwlock);
}


static void remove_listener(listener_t *listener)
{
	listener_t *l, *last = NULL;

	switch_thread_rwlock_wrlock(globals.listener_rwlock);
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
	switch_thread_rwlock_unlock(globals.listener_rwlock);
}

/* Search for a listener already talking to the specified node and lock for reading*/
static listener_t *find_listener(char *nodename)
{
	listener_t *l = NULL;

	switch_thread_rwlock_rdlock(globals.listener_rwlock);
	for (l = listen_list.listeners; l; l = l->next) {
		if (!strncmp(nodename, l->peer_nodename, MAXNODELEN)) {
			switch_thread_rwlock_rdlock(l->rwlock);
			break;
		}
	}
	switch_thread_rwlock_unlock(globals.listener_rwlock);
	return l;
}


static void add_session_elem_to_listener(listener_t *listener, session_elem_t *session_element)
{
	switch_thread_rwlock_wrlock(listener->session_rwlock);
	switch_core_hash_insert(listener->sessions, session_element->uuid_str, (void*) session_element);
	switch_thread_rwlock_unlock(listener->session_rwlock);
}


static void remove_session_elem_from_listener(listener_t *listener, session_elem_t *session_element)
{
	switch_thread_rwlock_wrlock(listener->session_rwlock);
	switch_core_hash_delete(listener->sessions, session_element->uuid_str);
	switch_thread_rwlock_unlock(listener->session_rwlock);
}

static void destroy_session_elem(session_elem_t *session_element)
{
	switch_core_session_t *session;

	/* wait for readers */
	switch_thread_rwlock_wrlock(session_element->rwlock);
	switch_thread_rwlock_unlock(session_element->rwlock);

	if ((session = switch_core_session_force_locate(session_element->uuid_str))) {
		switch_channel_t *channel = switch_core_session_get_channel(session);

		switch_channel_set_private(channel, "_erlang_session_", NULL);
		switch_channel_clear_flag(channel, CF_CONTROLLED);
		switch_core_session_soft_unlock(session);
		switch_core_session_rwunlock(session);
	}
	switch_core_destroy_memory_pool(&session_element->pool);
}

session_elem_t *find_session_elem_by_uuid(listener_t *listener, const char *uuid)
{
	session_elem_t *session = NULL;
	
	switch_thread_rwlock_rdlock(listener->session_rwlock);
	if ((session = (session_elem_t*)switch_core_hash_find(listener->sessions, uuid))) {
		switch_thread_rwlock_rdlock(session->rwlock);
	}

	switch_thread_rwlock_unlock(listener->session_rwlock);

	return session;
 }

session_elem_t *find_session_elem_by_pid(listener_t *listener, erlang_pid *pid)
{
	switch_hash_index_t *iter = NULL;
	const void *key = NULL;
	void *val = NULL;
	session_elem_t *session = NULL;

	switch_thread_rwlock_rdlock(listener->session_rwlock);
	for (iter = switch_core_hash_first(listener->sessions); iter; iter = switch_core_hash_next(&iter)) {
		switch_core_hash_this(iter, &key, NULL, &val);
		
		if (((session_elem_t*)val)->process.type == ERLANG_PID && !ei_compare_pids(pid, &((session_elem_t*)val)->process.pid)) {
			session = (session_elem_t*)val;
			switch_thread_rwlock_rdlock(session->rwlock);
			break;
		}
	}
	switch_safe_free(iter);
	switch_thread_rwlock_unlock(listener->session_rwlock);

	return session;
}

static fetch_reply_t *new_fetch_reply(const char *uuid_str) 
{
	fetch_reply_t *reply = NULL;
	switch_memory_pool_t *pool = NULL;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		abort();
	}
	switch_assert(pool != NULL);
	reply = switch_core_alloc(pool, sizeof(*reply));
	switch_assert(reply != NULL);
	memset(reply, 0, sizeof(*reply));
	
	reply->uuid_str = switch_core_strdup(pool, uuid_str);
	reply->pool = pool;
	switch_thread_cond_create(&reply->ready_or_found, pool);
	switch_mutex_init(&reply->mutex, SWITCH_MUTEX_UNNESTED, pool);
	reply->state = reply_not_ready;
	reply->reply = NULL;

	switch_mutex_lock(reply->mutex);
	switch_core_hash_insert_locked(globals.fetch_reply_hash, uuid_str, reply, globals.fetch_reply_mutex);
	reply->state = reply_waiting;

	return reply;
}

static void destroy_fetch_reply(fetch_reply_t *reply) 
{
	switch_core_hash_delete_locked(globals.fetch_reply_hash, reply->uuid_str, globals.fetch_reply_mutex);
	/* lock so nothing can have it while we delete it */
	switch_mutex_lock(reply->mutex);
	switch_mutex_unlock(reply->mutex);

	switch_mutex_destroy(reply->mutex);
	switch_thread_cond_destroy(reply->ready_or_found);
	switch_safe_free(reply->reply);
	switch_core_destroy_memory_pool(&(reply->pool));
}

fetch_reply_t *find_fetch_reply(const char *uuid) 
{
	fetch_reply_t *reply = NULL;

	switch_mutex_lock(globals.fetch_reply_mutex);
	if ((reply = switch_core_hash_find(globals.fetch_reply_hash, uuid))) {
		if (switch_mutex_lock(reply->mutex) != SWITCH_STATUS_SUCCESS) {
			reply = NULL;
		}
	}
	switch_mutex_unlock(globals.fetch_reply_mutex);
	return reply;
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

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	ei_x_encode_tuple_header(&buf, 7);
	ei_x_encode_atom(&buf, "fetch");
	ei_x_encode_atom(&buf, sectionstr);
	_ei_x_encode_string(&buf, tag_name ? tag_name : "undefined");
	_ei_x_encode_string(&buf, key_name ? key_name : "undefined");
	_ei_x_encode_string(&buf, key_value ? key_value : "undefined");
	_ei_x_encode_string(&buf, uuid_str);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "looking for bindings\n");

	section = switch_xml_parse_section_string((char *) sectionstr);

	switch_thread_rwlock_rdlock(globals.bindings_rwlock);

	/* Keep the listener from getting pulled out from under us */
	switch_thread_rwlock_rdlock(globals.listener_rwlock);

	for (ptr = bindings.head; ptr; ptr = ptr->next) {
		/* If we got listener_rwlock while a listner thread was dying after removing the listener
		   from listener_list but before locking for the bindings removal (now pending our lock) check
		   if it already closed the socket.  Our listener pointer should still be good (pointed at an orphan
		   listener) until it is removed from the binding...*/
		if (!ptr->listener) {
			continue;
		}

		if (ptr->section != section) {
			continue;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "binding for %s in section %s with key %s and value %s requested from node %s\n", tag_name, sectionstr, key_name, key_value, ptr->process.pid.node);

		if (params) {
			ei_encode_switch_event_headers(&buf, params);
		} else {
			ei_x_encode_empty_list(&buf);
		}

		if (!p) {
			p = new_fetch_reply(uuid_str);
			now = switch_micro_time_now();
		}
		/* We don't need to lock here because everybody is waiting
		   on our condition before the action starts. */

		switch_mutex_lock(ptr->listener->sock_mutex);
 		if (ptr->listener->sockfd) {
			ei_sendto(ptr->listener->ec, ptr->listener->sockfd, &ptr->process, &buf);
		}
		switch_mutex_unlock(ptr->listener->sock_mutex);
	}

	switch_thread_rwlock_unlock(globals.bindings_rwlock);
	switch_thread_rwlock_unlock(globals.listener_rwlock);

	ei_x_free(&buf);

	if (!p) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no binding for %s\n", sectionstr);
		goto cleanup;
	}

	/* Wait five seconds for a reply. */
	switch_thread_cond_timedwait(p->ready_or_found, p->mutex, 5000000);
	if (!p->reply) {
		p->state = reply_timeout;
		switch_mutex_unlock(p->mutex);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Timed out after %d milliseconds when waiting for XML fetch response for %s\n", (int) (switch_micro_time_now() - now) / 1000, uuid_str);
		goto cleanup;
	}

	rep = p->reply;
	switch_mutex_unlock(p->mutex);

	ei_get_type(rep->buff, &rep->index, &type, &size);

	if (type == ERL_NIL_EXT) {
		// empty string returned
		goto cleanup;
	}

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

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got data %s after %d milliseconds from %s for %s!\n", xmlstr, (int) (switch_micro_time_now() - now) / 1000, p->winner, uuid_str);
	}

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
		destroy_fetch_reply(p);
	}

	return xml;
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
		switch_event_destroy(&call_event);
		return SWITCH_STATUS_FALSE;
	}

	channel = switch_core_session_get_channel(session);

	switch_caller_profile_event_set_data(switch_channel_get_caller_profile(channel), "Channel", call_event);
	switch_channel_event_set_data(channel, call_event);
	switch_core_session_rwunlock(session);
	/* TODO reply? sure? */
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

	switch_event_destroy(&call_event);
	ei_x_free(&lbuf);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t check_attached_sessions(listener_t *listener, int *msgs_sent)
{
	session_elem_t *sp;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	void *pop;
	const void *key;
	void * value;
	switch_hash_index_t *iter;
	/* event used to track sessions to remove */
	switch_event_t *event = NULL;
	switch_event_header_t *header = NULL;

	switch_event_create_subclass(&event, SWITCH_EVENT_CLONE, NULL);
	switch_assert(event);
	/* check up on all the attached sessions -
	   if they have not yet sent an initial call event to the associated erlang process then do so
	   if they have pending events in their queues then send them
	   if the session has finished then clean it up
	 */

	/* TODO try to minimize critical section */
	switch_thread_rwlock_rdlock(listener->session_rwlock);
	for (iter = switch_core_hash_first(listener->sessions); iter; iter = switch_core_hash_next(&iter)) {
		switch_core_hash_this(iter, &key, NULL, &value);
		sp = (session_elem_t*)value;
		if (switch_test_flag(sp, LFLAG_WAITING_FOR_PID)) {
			continue;
		}

		if (!switch_test_flag(sp, LFLAG_OUTBOUND_INIT)) {
			status = notify_new_session(listener, sp);
			if (status != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(sp->uuid_str), SWITCH_LOG_DEBUG, "Notifying new session failed\n");
				/* mark this session for removal */
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delete", (const char *) key);
				continue;
			}
			switch_set_flag_locked(sp, LFLAG_OUTBOUND_INIT);
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
				(*msgs_sent)++;
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
			(*msgs_sent)++;
			switch_mutex_unlock(listener->sock_mutex);
			ei_x_free(&ebuf);

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delete", (const char *) key);
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
			(*msgs_sent)++;
			ei_x_free(&ebuf);

			switch_event_destroy(&pevent);
		}
	}
	switch_thread_rwlock_unlock(listener->session_rwlock);

	/* do the deferred remove */
	for (header = event->headers; header; header = header->next) {
		if ((sp = (session_elem_t*)find_session_elem_by_uuid(listener, header->value))) {
			remove_session_elem_from_listener(listener, sp);
			switch_thread_rwlock_unlock(sp->rwlock);
			destroy_session_elem(sp);
		}
	}

	/* remove the temporary event */
	switch_event_destroy(&event);

	if (prefs.done) {
		return SWITCH_STATUS_FALSE;	/* we're shutting down */
	} else {
		return SWITCH_STATUS_SUCCESS;
	}
}

static int check_log_queue(listener_t *listener)
{
	void *pop;
	int msgs_sent = 0;

	/* send out any pending crap in the log queue */
	if (switch_test_flag(listener, LFLAG_LOG)) {
		while (msgs_sent < prefs.max_log_bulk && switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS) {
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
				msgs_sent ++;

				ei_x_free(&lbuf);
				switch_log_node_free(&dnode);
			}
		}
	}

	listener->total_logs += msgs_sent;
	return msgs_sent;
}

static int check_event_queue(listener_t *listener)
{
	void *pop;
	int msgs_sent = 0;

	/* send out any pending crap in the event queue */
	if (switch_test_flag(listener, LFLAG_EVENTS)) {
		while (msgs_sent < prefs.max_event_bulk && switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {

			switch_event_t *pevent = (switch_event_t *) pop;

			ei_x_buff ebuf;
			ei_x_new_with_version(&ebuf);

			ei_encode_switch_event(&ebuf, pevent);

			switch_mutex_lock(listener->sock_mutex);
			ei_sendto(listener->ec, listener->sockfd, &listener->event_process, &ebuf);
			switch_mutex_unlock(listener->sock_mutex);

			msgs_sent++;

			ei_x_free(&ebuf);

			if (pevent->event_id == SWITCH_EVENT_CHANNEL_CREATE) {
				listener->create++;
			} else if (pevent->event_id == SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE) {
				listener->hangup++;
			}

			switch_event_destroy(&pevent);
		}
	}

	listener->total_events += msgs_sent;
	return msgs_sent;
}

static void handle_exit(listener_t *listener, erlang_pid * pid)
{
	session_elem_t *s;

	remove_binding(NULL, pid);	/* TODO - why don't we pass the listener as the first argument? */

	if ((s = find_session_elem_by_pid(listener, pid))) {
		switch_set_flag_locked(s, LFLAG_SESSION_COMPLETE);
		switch_thread_rwlock_unlock(s->rwlock);
	}

	if (listener->log_process.type == ERLANG_PID && !ei_compare_pids(&listener->log_process.pid, pid)) {
		void *pop;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Log handler process for node %s exited\n", pid->node);
		/*purge the log queue */
		/* TODO don't we want to clear flag first? */
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
		/* TODO don't we want to clear flag first? */
		while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *pevent = (switch_event_t *) pop;
			switch_event_destroy(&pevent);
		}

		if (switch_test_flag(listener, LFLAG_EVENTS)) {
			uint8_t x = 0;
			switch_clear_flag_locked(listener, LFLAG_EVENTS);

			switch_thread_rwlock_wrlock(listener->event_rwlock);
			for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
				listener->event_list[x] = 0;
			}
			switch_core_hash_delete_multi(listener->event_hash, NULL, NULL);
			switch_thread_rwlock_unlock(listener->event_rwlock);
		}
	}
}

static void listener_main_loop(listener_t *listener)
{
	int status = 1;
	int msgs_sent = 0; /* how many messages we sent in a loop */

	while ((status >= 0 || erl_errno == ETIMEDOUT || erl_errno == EAGAIN) && !prefs.done) {
		erlang_msg msg;
		ei_x_buff buf;
		ei_x_buff rbuf;

		ei_x_new(&buf);
		ei_x_new_with_version(&rbuf);

		msgs_sent = 0;

		/* do we need the mutex when reading? */
		/*switch_mutex_lock(listener->sock_mutex); */
		status = ei_xreceive_msg_tmo(listener->sockfd, &msg, &buf, 1);
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
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "erl_error: status=%d, erl_errno=%d errno=%d\n", status,  erl_errno, errno);
			}
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unexpected status %d \n", status);
			break;
		}

		ei_x_free(&buf);
		ei_x_free(&rbuf);

		msgs_sent += check_log_queue(listener);
		msgs_sent += check_event_queue(listener);
		if (check_attached_sessions(listener, &msgs_sent) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "check_attached_sessions requested exit\n");
			return;
		}

		if (msgs_sent > SWITCH_CORE_QUEUE_LEN / 2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%d messages sent in a loop\n", msgs_sent);
		} else if (msgs_sent > 0) {
#ifdef EI_DEBUG
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%d messages sent in a loop\n", msgs_sent);
#endif
		} else { /* no more messages right now, relax */
			switch_yield(10000);
		}
	}
	if (prefs.done) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "shutting down listener\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "listener exit: status=%d, erl_errno=%d errno=%d\n", status,  erl_errno, errno);
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

	switch_mutex_lock(globals.listener_count_mutex);
	prefs.threads++;
	switch_mutex_unlock(globals.listener_count_mutex);

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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Closed\n");

	remove_listener(listener);
	destroy_listener(listener);

	switch_mutex_lock(globals.listener_count_mutex);
	prefs.threads--;
	switch_mutex_unlock(globals.listener_count_mutex);

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

static int read_cookie_from_file(char *filename) {
	int fd;
	char cookie[MAXATOMLEN+1];
	char *end;
	struct stat buf;
	ssize_t res;

	if (!stat(filename, &buf)) {
		if ((buf.st_mode & S_IRWXG) || (buf.st_mode & S_IRWXO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s must only be accessible by owner only.\n", filename);
			return 2;
		}
		if (buf.st_size > MAXATOMLEN) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s contains a cookie larger than the maximum atom size of %d.\n", filename, MAXATOMLEN);
			return 2;
		}
		fd = open(filename, O_RDONLY);
		if (fd < 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open cookie file %s : %d.\n", filename, errno);
			return 2;
		}

		if ((res = read(fd, cookie, MAXATOMLEN)) < 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to read cookie file %s : %d.\n", filename, errno);
		}

		cookie[MAXATOMLEN] = '\0';

		/* replace any end of line characters with a null */
		if ((end = strchr(cookie, '\n'))) {
			*end = '\0';
		}

		if ((end = strchr(cookie, '\r'))) {
			*end = '\0';
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Read %d bytes from cookie file %s.\n", (int)res, filename);

		set_pref_cookie(cookie);
		return 0;
	} else {
		/* don't error here, because we might be blindly trying to read $HOME/.erlang.cookie, and that can fail silently */
		return 1;
	}
}


static int config(void)
{
	char *cf = "erlang_event.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&prefs, 0, sizeof(prefs));

	prefs.shortname = SWITCH_TRUE;
	prefs.encoding = ERLANG_STRING;
	prefs.compat_rel = 0;
	prefs.max_event_bulk = 1;
	prefs.max_log_bulk = 1;
	

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
				} else if (!strcmp(var, "cookie-file")) {
					if (read_cookie_from_file(val) == 1) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to read cookie from %s\n", val);
					}
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
				} else if (!strcasecmp(var, "apply-inbound-acl") && ! zstr(val)) {
					if (prefs.acl_count < MAX_ACL) {
						prefs.acl[prefs.acl_count++] = strdup(val);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", MAX_ACL);
					}
				} else if (!strcasecmp(var, "max-event-bulk") && !zstr(val)) {
					prefs.max_event_bulk = atoi(val);
				} else if (!strcasecmp(var, "max-log-bulk") && !zstr(val)) {
					prefs.max_log_bulk = atoi(val);
				}
			}
		}
		switch_xml_free(xml);
	}

	if (zstr(prefs.ip)) {
		set_pref_ip("0.0.0.0");
	}

	if (zstr(prefs.cookie)) {
		int res;
		char* home_dir = getenv("HOME");
		char path_buf[1024];

		if (!zstr(home_dir)) {
			/* $HOME/.erlang.cookie */
			switch_snprintf(path_buf, sizeof(path_buf), "%s%s%s", home_dir, SWITCH_PATH_SEPARATOR, ".erlang.cookie");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Checking for cookie at path: %s\n", path_buf);

			res = read_cookie_from_file(path_buf);
			if (res) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No cookie or valid cookie file specified, using default cookie\n");
				set_pref_cookie("ClueCon");
			}
		}
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
	switch_memory_pool_t *pool = NULL;
	listener_t *listener = NULL;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return NULL;
	}

	if (!(listener = switch_core_alloc(pool, sizeof(*listener)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
		switch_core_destroy_memory_pool(&pool);
		return NULL;
	}
	memset(listener, 0, sizeof(*listener));

	switch_queue_create(&listener->event_queue, SWITCH_CORE_QUEUE_LEN, pool);
	switch_queue_create(&listener->log_queue, SWITCH_CORE_QUEUE_LEN, pool);

	listener->sockfd = clientfd;
	listener->pool = pool;
	listener->ec = switch_core_alloc(listener->pool, sizeof(ei_cnode));
	memcpy(listener->ec, ec, sizeof(ei_cnode));
	listener->level = SWITCH_LOG_DEBUG;
	switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);
	switch_mutex_init(&listener->sock_mutex, SWITCH_MUTEX_NESTED, listener->pool);
	switch_mutex_init(&listener->filter_mutex, SWITCH_MUTEX_NESTED, listener->pool);

	switch_thread_rwlock_create(&listener->rwlock, pool);
	switch_thread_rwlock_create(&listener->event_rwlock, pool);
	switch_thread_rwlock_create(&listener->session_rwlock, listener->pool);

	switch_core_hash_init(&listener->event_hash);
	switch_core_hash_init(&listener->sessions);

	return listener;
}


static listener_t *new_outbound_listener_locked(char *node)
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

	switch_thread_rwlock_rdlock(listener->rwlock);

	return listener;
}

void destroy_listener(listener_t * listener)
{
	session_elem_t *s = NULL;
	const void *key;
	void *value;
	switch_hash_index_t *iter;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Session complete, waiting for children\n");
	switch_thread_rwlock_wrlock(listener->rwlock);

	switch_mutex_lock(listener->sock_mutex);
	if (listener->sockfd) {
		close_socket(&listener->sockfd);
	}
	switch_mutex_unlock(listener->sock_mutex);

	switch_core_hash_destroy(&listener->event_hash);

	/* remove any bindings for this connection */
	remove_binding(listener, NULL);

	/* clean up all the attached sessions */
	switch_thread_rwlock_wrlock(listener->session_rwlock);
	for (iter = switch_core_hash_first(listener->sessions); iter; iter = switch_core_hash_next(&iter)) {
		switch_core_hash_this(iter, &key, NULL, &value);
		s = (session_elem_t*)value;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Orphaning call %s\n", s->uuid_str);
		destroy_session_elem(s);
	}
	switch_thread_rwlock_unlock(listener->session_rwlock);
	switch_thread_rwlock_unlock(listener->rwlock);

	if (listener->pool) {
		switch_memory_pool_t *pool = listener->pool;
		switch_core_destroy_memory_pool(&pool);
	}

}

static switch_status_t state_handler(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	session_elem_t *session_element = switch_channel_get_private(channel, "_erlang_session_");

	if (session_element) {
		session_element->channel_state = state;
		if (state == CS_DESTROY) {
			/* indicate that once all the events in the event queue are done
			 * we can throw this away */
			switch_set_flag_locked(session_element, LFLAG_SESSION_COMPLETE);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

session_elem_t *session_elem_create(listener_t *listener, switch_core_session_t *session)
{
	/* create a session list element */
	switch_memory_pool_t *session_elem_pool;
	session_elem_t *session_element;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int x;

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
	switch_core_hash_init(&session_element->event_hash);
	session_element->spawn_reply = NULL;

	for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
		session_element->event_list[x] = 0;
	}

	switch_thread_rwlock_create(&session_element->rwlock, session_element->pool);
	switch_thread_rwlock_create(&session_element->event_rwlock, session_element->pool);

	session_element->event_list[SWITCH_EVENT_ALL] = 1; /* defaults to everything */

	switch_channel_set_private(channel, "_erlang_session_", session_element);
	switch_core_session_soft_lock(session, 5);

	switch_core_event_hook_add_state_change(session, state_handler);

	return session_element;
}

session_elem_t *attach_call_to_registered_process(listener_t *listener, char *reg_name, switch_core_session_t *session)
{
	/* create a session list element */
	session_elem_t *session_element = session_elem_create(listener, session);

	session_element->process.type = ERLANG_REG_PROCESS;
	session_element->process.reg_name = switch_core_session_strdup(session, reg_name);
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
	/* attach the session to the listener */
	add_session_elem_to_listener(listener, session_element);
	/* TODO link before added to listener? */
	ei_link(listener, ei_self(listener->ec), pid);

	return session_element;
}

session_elem_t *attach_call_to_spawned_process(listener_t *listener, char *module, char *function, switch_core_session_t *session)
{
	/* create a session list element */
	session_elem_t *session_element = session_elem_create(listener, session);
	char hash[100];
	spawn_reply_t *p;
	erlang_ref ref;


	ei_init_ref(listener->ec, &ref);
	ei_hash_ref(&ref, hash);

	p = switch_core_alloc(session_element->pool, sizeof(*p));
	switch_thread_cond_create(&p->ready_or_found, session_element->pool);
	switch_mutex_init(&p->mutex, SWITCH_MUTEX_UNNESTED, session_element->pool);
	p->hash = switch_core_strdup(session_element->pool, hash);
	p->pid = NULL;

	session_element->spawn_reply = p;

	/* insert the waiting marker */
	switch_set_flag(session_element, LFLAG_WAITING_FOR_PID);
	
	/* attach the session to the listener */
	add_session_elem_to_listener(listener, session_element);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Added session to listener\n");

	switch_mutex_lock(p->mutex);
	
	if (!strcmp(function, "!")) {
		/* send a message to request a pid */
		ei_x_buff rbuf;
		ei_x_new_with_version(&rbuf);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "get_pid\n");

		ei_x_encode_tuple_header(&rbuf, 4);
		ei_x_encode_atom(&rbuf, "get_pid");
		_ei_x_encode_string(&rbuf, session_element->uuid_str);
		ei_x_encode_ref(&rbuf, &ref);
		ei_x_encode_pid(&rbuf, ei_self(listener->ec));
		/* should lock with mutex? */
		ei_reg_send(listener->ec, listener->sockfd, module, rbuf.buff, rbuf.index);
#ifdef EI_DEBUG
		ei_x_print_reg_msg(&rbuf, module, 1);
#endif
		ei_x_free(&rbuf);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "rpc call: %s:%s(Ref)\n", module, function);
		/* should lock with mutex? */
		switch_mutex_lock(listener->sock_mutex);
		ei_pid_from_rpc(listener->ec, listener->sockfd, &ref, module, function);
		switch_mutex_unlock(listener->sock_mutex);
		/*
		   char *argv[1];
		   ei_spawn(listener->ec, listener->sockfd, &ref, module, function, 0, argv);
		 */
	}


	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Waiting for reply %s %s\n", hash, session_element->uuid_str);
	switch_thread_cond_timedwait(p->ready_or_found, p->mutex, 5000000);
	switch_mutex_unlock(p->mutex);
	if (!p->pid) {
		switch_channel_t *channel = switch_core_session_get_channel(session);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Timed out when waiting for outbound pid %s %s\n", hash, session_element->uuid_str);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		/* Destroy erlang session elements when the outbound erlang process gets killed for some unknown reason */
        remove_session_elem_from_listener(listener, session_element);
        destroy_session_elem(session_element);
		return NULL;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "got pid! %s %s\n", hash, session_element->uuid_str);

	session_element->process.type = ERLANG_PID;
	memcpy(&session_element->process.pid, p->pid, sizeof(erlang_pid));
	session_element->spawn_reply = NULL;

	switch_clear_flag_locked(session_element, LFLAG_WAITING_FOR_PID);

	ei_link(listener, ei_self(listener->ec), &session_element->process.pid);

	return session_element;
}


int count_listener_sessions(listener_t *listener)
{
	int count = 0;
	switch_hash_index_t *iter;

	switch_thread_rwlock_rdlock(listener->session_rwlock);
	for (iter = switch_core_hash_first(listener->sessions); iter; iter = switch_core_hash_next(&iter)) {
		count++;
	}
	switch_thread_rwlock_unlock(listener->session_rwlock);

	return count;
}


/* Module Hooks */

/* Entry point for outbound mode */
SWITCH_STANDARD_APP(erlang_outbound_function)
{
	char *reg_name = NULL, *node, *module = NULL, *function = NULL;
	listener_t *listener;
	int argc = 0, argc2 = 0;
	char *argv[80] = { 0 }, *argv2[80] = { 0 };
	char *mydata, *myarg;
	session_elem_t *session_element = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	/* process app arguments */
	if (data && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	/* XXX else? */

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


	if (switch_channel_test_flag(channel, CF_CONTROLLED)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Channel is already under control\n");
		return;
	}

	
	/* first work out if there is a listener already talking to the node we want to talk to */
	listener = find_listener(node);
	/* if there is no listener, then create one */
	if (!listener) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Creating new listener for session\n");
		if ((listener = new_outbound_listener_locked(node))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Launching new listener\n");
			launch_listener_thread(listener);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Using existing listener for session\n");

	}

	if (listener) {

		if ((session_element = find_session_elem_by_uuid(listener, switch_core_session_get_uuid(session)))) {
			switch_thread_rwlock_unlock(session_element->rwlock);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Session already exists\n");

		} else {
			if (module && function) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Creating new spawned session for listener\n");
				session_element = attach_call_to_spawned_process(listener, module, function, session);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Creating new registered session for listener\n");
				session_element = attach_call_to_registered_process(listener, reg_name, session);
			}
		}

		switch_thread_rwlock_unlock(listener->rwlock);

		if (session_element) {
			switch_ivr_park(session, NULL);
		}

	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "exit erlang_outbound_function\n");
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
		listener = new_outbound_listener_locked(node);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Using existing listener for sendmsg to %s\n", node);
	}

	if (listener) {
		ei_reg_send(listener->ec, listener->sockfd, reg_name, buf.buff, buf.index);

		switch_thread_rwlock_unlock(listener->rwlock);
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
		"erlang listeners\n"
		"erlang sessions <node_name>\n"
		"erlang bindings\n"
		"erlang handlers\n"
		"erlang debug <on|off>\n"
		"--------------------------------------------------------------------------------\n";

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
		switch_thread_rwlock_rdlock(globals.listener_rwlock);

		if (listen_list.listeners) {
			for (l = listen_list.listeners; l; l = l->next) {
				stream->write_function(stream, "Listener to %s with outbound sessions: %d events: %" SWITCH_UINT64_T_FMT
					" (lost:%d) logs: %" SWITCH_UINT64_T_FMT " (lost:%d) %d/%d\n",
					l->peer_nodename, count_listener_sessions(l),
					l->total_events, l->lost_events,
					l->total_logs, l->lost_logs, l->create, l->hangup);
			}
		} else {
			stream->write_function(stream, "No active listeners\n");
		}

		switch_thread_rwlock_unlock(globals.listener_rwlock);
	} else if (!strcasecmp(argv[0], "sessions") && argc == 2) {
		listener_t *l;
		int found = 0;

		switch_thread_rwlock_rdlock(globals.listener_rwlock);
		for (l = listen_list.listeners; l; l = l->next) {
			if (!strcasecmp(l->peer_nodename, argv[1])) {
				session_elem_t *sp;
				switch_hash_index_t *iter;
				int empty = 1;
				const void *key;
				void *value;

				found = 1;
				switch_thread_rwlock_rdlock(l->session_rwlock);
				for (iter = switch_core_hash_first(l->sessions); iter; iter = switch_core_hash_next(&iter)) {
					empty = 0;
					switch_core_hash_this(iter, &key, NULL, &value);
					sp = (session_elem_t*)value;
					stream->write_function(stream, "Outbound session for %s in state %s\n", sp->uuid_str,
							switch_channel_state_name(sp->channel_state));
				}
				switch_thread_rwlock_unlock(l->session_rwlock);

				if (empty) {
					stream->write_function(stream, "No active sessions for %s\n", argv[1]);
				}
				break;
			}
		}
		switch_thread_rwlock_unlock(globals.listener_rwlock);

		if (!found)
			stream->write_function(stream, "Could not find a listener for %s\n", argv[1]);

	} else if (!strcasecmp(argv[0], "handlers")) {
			listener_t *l;

			switch_thread_rwlock_rdlock(globals.listener_rwlock);

			if (listen_list.listeners) {
				for (l = listen_list.listeners; l; l = l->next) {
					int x;
					switch_hash_index_t *iter;
					const void *key;
					void *val;

					stream->write_function(stream, "Listener %s:\n--------------------------------\n", l->peer_nodename);

					for (x = SWITCH_EVENT_CUSTOM + 1; x < SWITCH_EVENT_ALL; x++) {
						if (l->event_list[x] == 1) {
							stream->write_function(stream, "%s\n", switch_event_name(x));
						}
					}
					stream->write_function(stream, "CUSTOM:\n", switch_event_name(x));

					for (iter = switch_core_hash_first(l->event_hash); iter; iter = switch_core_hash_next(&iter)) {
						switch_core_hash_this(iter, &key, NULL, &val);
						stream->write_function(stream, "\t%s\n", (char *)key);
					}
					stream->write_function(stream, "\n", (char *)key);
				}
			} else {
				stream->write_function(stream, "No active handlers\n");
			}

			switch_thread_rwlock_unlock(globals.listener_rwlock);

	} else if (!strcasecmp(argv[0], "bindings")) {
		int found = 0;
		struct erlang_binding *ptr;
		switch_thread_rwlock_rdlock(globals.bindings_rwlock);

		for (ptr = bindings.head; ptr; ptr = ptr->next) {

			if (ptr->process.type == ERLANG_PID) {
				stream->write_function(stream, "%s ", ptr->process.pid.node);
			}

			if (ptr->section == SWITCH_XML_SECTION_CONFIG) {
				stream->write_function(stream, "config\n");
			}else if (ptr->section == SWITCH_XML_SECTION_DIRECTORY) {
				stream->write_function(stream, "directory\n");
			} else if (ptr->section == SWITCH_XML_SECTION_DIALPLAN) {
				stream->write_function(stream, "dialplan\n");
			} else if (ptr->section == SWITCH_XML_SECTION_LANGUAGES) {
				stream->write_function(stream, "phrases\n");
			} else if (ptr->section == SWITCH_XML_SECTION_CHATPLAN) {
				stream->write_function(stream, "chatplan\n");
			} else {
				stream->write_function(stream, "unknown %d\n", ptr->section);
			}
			found++;
		}

		switch_thread_rwlock_unlock(globals.bindings_rwlock);

		if (!found) {
			stream->write_function(stream, "No bindings\n");
		}

	} else if (!strcasecmp(argv[0], "debug")) {
		if (argc == 2) {
			if (!strcasecmp(argv[1], "on")) {
				globals.debug = 1;
			} else {
				globals.debug = 0;
			}
		}
		stream->write_function(stream, "+OK debug %s\n", globals.debug ? "on" : "off");

	} else {
		stream->write_function(stream,  usage_string);
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

	switch_thread_rwlock_create(&globals.listener_rwlock, pool);
	switch_thread_rwlock_create(&globals.bindings_rwlock, pool);
	switch_mutex_init(&globals.fetch_reply_mutex, SWITCH_MUTEX_DEFAULT, pool);
	switch_mutex_init(&globals.listener_count_mutex, SWITCH_MUTEX_UNNESTED, pool);
	switch_core_hash_init(&globals.fetch_reply_hash);

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
	int rv;
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
		if ((clientfd = ei_accept_tmo(&ec, (int) listen_list.sockfd, &conn, 500)) == ERL_ERROR) {
			if (prefs.done) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Shutting Down\n");
				break;
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
			}
			continue;
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

	switch_thread_rwlock_wrlock(globals.listener_rwlock);

	for (l = listen_list.listeners; l; l = l->next) {
		close_socket(&l->sockfd);
	}

#ifdef WIN32
	WSACleanup();
#endif

	switch_thread_rwlock_unlock(globals.listener_rwlock);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
