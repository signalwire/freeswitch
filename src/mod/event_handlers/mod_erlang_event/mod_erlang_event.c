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

static void remove_listener(listener_t *listener);

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

			switch_log_node_t *dnode = malloc(sizeof(*node));
			switch_assert(dnode);
			*dnode = *node;
			dnode->data = strdup(node->data);
			switch_assert(dnode->data);

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
				switch_safe_free(dnode->data);
				switch_safe_free(dnode);
				l->lost_logs++;
			}
		}
	}
	switch_mutex_unlock(globals.listener_mutex);

	return SWITCH_STATUS_SUCCESS;
}


static void expire_listener(listener_t **listener)
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


static void remove_binding(listener_t *listener, erlang_pid *pid) {
	struct erlang_binding *ptr, *lst = NULL;

	switch_mutex_lock(globals.listener_mutex);

	switch_xml_set_binding_sections(bindings.search_binding, (1 << sizeof(switch_xml_section_enum_t)));

	for (ptr = bindings.head; ptr; lst = ptr, ptr = ptr->next) {
		if ((listener && ptr->listener == listener) ||
			(pid && (ptr->process.type == ERLANG_PID) && !ei_compare_pids(&ptr->process.pid, pid)))  {
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


static void send_event_to_attached_sessions(listener_t* listener, switch_event_t *event)
{
	char *uuid = switch_event_get_header(event, "unique-id");
	switch_event_t *clone = NULL;
	session_elem_t* s;

	if (!uuid)
		return;
	switch_mutex_lock(listener->session_mutex);
	for (s = listener->session_list; s; s = s->next) {
		/* check the event uuid against the uuid of each session */
		if (!strcmp(uuid, switch_core_session_get_uuid(s->session))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending event to attached session\n");
			if (switch_event_dup(&clone, event) == SWITCH_STATUS_SUCCESS) {
				/* add the event to the queue for this session */
				if (switch_queue_trypush(s->event_queue, clone) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Lost event!\n");
					switch_event_destroy(&clone);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
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
	while(lp) {
		uint8_t send = 0;
		
		l = lp;
		lp = lp->next;

		/* test all of the sessions attached to this event in case
		   one of them should receive it as well
		 */

		send_event_to_attached_sessions(l,event);

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


static void close_socket(int *sock)
{
	switch_mutex_lock(listen_list.sock_mutex);
	if (*sock) {
		shutdown(*sock, SHUT_RDWR);
		close(*sock);
		sock = NULL;
	}
	switch_mutex_unlock(listen_list.sock_mutex);
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
static listener_t * find_listener(char* nodename)
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

static void remove_session_elem_from_listener(listener_t *listener, session_elem_t *session)
{
	session_elem_t *s, *last = NULL;
	
	if(!session)
		return;

	switch_mutex_lock(listener->session_mutex);
	for(s = listener->session_list; s; s = s->next) {
		if (s == session) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing session\n");
			if (last) {
				last->next = s->next;
			} else {
				listener->session_list = s->next;
			}
			switch_channel_clear_flag(switch_core_session_get_channel(s->session), CF_CONTROLLED);
			/* this allows the application threads to exit */
			switch_clear_flag_locked(s, LFLAG_SESSION_ALIVE);
			switch_core_session_rwunlock(s->session);
		}
		last = s;
	}
	switch_mutex_unlock(listener->session_mutex);
}


session_elem_t * find_session_elem_by_pid(listener_t *listener, erlang_pid *pid)
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
	switch_xml_t xml = NULL;
	struct erlang_binding *ptr;
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH+1];
	ei_x_buff buf;
	ei_x_new_with_version(&buf);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "looking for bindings\n");

	switch_xml_section_t section = switch_xml_parse_section_string((char *) sectionstr);

	for (ptr = bindings.head; ptr && ptr->section != section; ptr = ptr->next); /* just get the first match */

	if (!ptr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no binding for %s\n", sectionstr);
		return NULL;
	}

	if (!ptr->listener) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NULL pointer binding!\n");
		return NULL; /* our pointer is trash */
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "binding for %s in section %s with key %s and value %s requested from node %s\n", tag_name, sectionstr, key_name, key_value, ptr->process.pid.node);

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	/*switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Request-ID", uuid_str);*/

	ei_x_encode_tuple_header(&buf, 7);
	ei_x_encode_atom(&buf, "fetch");
	ei_x_encode_atom(&buf, sectionstr);
	_ei_x_encode_string(&buf, tag_name ? tag_name : "undefined");
	_ei_x_encode_string(&buf, key_name ? key_name : "undefined");
	_ei_x_encode_string(&buf, key_value ? key_value : "undefined");
	_ei_x_encode_string(&buf, uuid_str);
	ei_encode_switch_event_headers(&buf, params);

	/*switch_core_hash_insert(ptr->reply_hash, uuid_str, );*/

	switch_mutex_lock(ptr->listener->sock_mutex);
	ei_sendto(ptr->listener->ec, ptr->listener->sockfd, &ptr->process, &buf);
	switch_mutex_unlock(ptr->listener->sock_mutex);

	int i = 0;
	ei_x_buff *rep;
	/*int index = 3;*/
	while (!(rep = (ei_x_buff *) switch_core_hash_find(ptr->listener->fetch_reply_hash, uuid_str))) {
		if (i > 50) { /* half a second timeout */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Timed out when waiting for XML fetch response!\n");
			return NULL;
		}
		i++;
		switch_yield(10000); /* 10ms */
	}

	int type, size;

	ei_get_type(rep->buff, &rep->index, &type, &size);

	if (type != ERL_STRING_EXT && type != ERL_BINARY_EXT) /* XXX no unicode or character codes > 255 */
		return NULL;

	char *xmlstr = switch_core_alloc(ptr->listener->pool, size + 1);

	ei_decode_string_or_binary(rep->buff, &rep->index, size, xmlstr);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got data %s after %d milliseconds!\n", xmlstr, i*10);

	if (switch_strlen_zero(xmlstr)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Result\n");
	} else if (!(xml = switch_xml_parse_str(xmlstr, strlen(xmlstr)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing XML Result!\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "XML parsed OK!\n");
	}

	switch_core_hash_delete(ptr->listener->fetch_reply_hash, uuid_str);

	/*switch_safe_free(rep->buff);*/
	/*switch_safe_free(rep);*/
	/*switch_safe_free(xmlstr);*/

	return xml;
}


static switch_status_t notify_new_session(listener_t *listener, switch_core_session_t *session, struct erlang_process process)
{
	int result;
	switch_event_t *call_event=NULL;
	switch_channel_t *channel=NULL;

	/* Send a message to the associated registered process to let it know there is a call.
	   Message is a tuple of the form {call, <call-event>}
	*/
	channel = switch_core_session_get_channel(session);
	if (switch_event_create(&call_event, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		return SWITCH_STATUS_MEMERR;
	}
	switch_caller_profile_event_set_data(switch_channel_get_caller_profile(channel), "Channel", call_event);
	switch_channel_event_set_data(channel, call_event);
	switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Content-Type", "command/reply");
	switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Reply-Text", "+OK\n");
	
	ei_x_buff lbuf;
	ei_x_new_with_version(&lbuf);
	ei_x_encode_tuple_header(&lbuf, 2);
	ei_x_encode_atom(&lbuf, "call");
	ei_encode_switch_event(&lbuf, call_event);
	switch_mutex_lock(listener->sock_mutex);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending initial call event\n");
	result = ei_sendto(listener->ec, listener->sockfd, &process, &lbuf);

	switch_mutex_unlock(listener->sock_mutex);

	if (result) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send call event\n");
	}
	
	ei_x_free(&lbuf);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t check_attached_sessions(listener_t *listener)
{
	session_elem_t *last,*sp;
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
	while(sp) {
		if (switch_test_flag(sp, LFLAG_WAITING_FOR_PID)) {
			break;
		}

		if (!switch_test_flag(sp, LFLAG_OUTBOUND_INIT)) {
			status = notify_new_session(listener, sp->session, sp->process);
			if (status != SWITCH_STATUS_SUCCESS)
				break;
			switch_set_flag(sp, LFLAG_OUTBOUND_INIT);
		}
		/* check event queue for this session */
		if (switch_queue_trypop(sp->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *pevent = (switch_event_t *) pop;
			
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

			/* event is a hangup, so this session can be removed */
			if (pevent->event_id == SWITCH_EVENT_CHANNEL_HANGUP) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Hangup event for attached session\n");

				/* remove session from list */
				if (last)
					last->next = sp->next;
				else
					listener->session_list = sp->next;
					

				switch_channel_clear_flag(switch_core_session_get_channel(sp->session), CF_CONTROLLED);
				/* this allows the application threads to exit */
				switch_clear_flag_locked(sp, LFLAG_SESSION_ALIVE);
				switch_core_session_rwunlock(sp->session);

				/* TODO
				   if this listener was created outbound, and the last session has been detached
				   should the listener also exit? Does it matter?
				 */
			}
			
			ei_x_free(&ebuf);
			switch_event_destroy(&pevent);
		}
		last = sp;
		sp = sp->next;
	}
	switch_mutex_unlock(listener->session_mutex);
	return status;
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
				ei_x_encode_list_header(&lbuf, 6);
				
				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "level");
				ei_x_encode_char(&lbuf, (unsigned char)dnode->level);
				
				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "text_channel");
				ei_x_encode_char(&lbuf, (unsigned char)dnode->level);
				
				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "file");
				ei_x_encode_string(&lbuf, dnode->file);
				
				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "func");
				ei_x_encode_string(&lbuf, dnode->func);
				
				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "line");
				ei_x_encode_ulong(&lbuf, (unsigned long)dnode->line);
				
				ei_x_encode_tuple_header(&lbuf, 2);
				ei_x_encode_atom(&lbuf, "data");
				ei_x_encode_string(&lbuf, dnode->data);
				
				ei_x_encode_empty_list(&lbuf);
				
				switch_mutex_lock(listener->sock_mutex);
				ei_sendto(listener->ec, listener->sockfd, &listener->log_process, &lbuf);
				switch_mutex_unlock(listener->sock_mutex);
				
				ei_x_free(&lbuf);
				free(dnode->data);
				free(dnode);
			}
		}
	}
}

static void check_event_queue(listener_t *listener)
{
	void* pop;

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

static void handle_exit(listener_t *listener, erlang_pid *pid)
{
	session_elem_t *s;

	remove_binding(NULL, pid); /* TODO - why don't we pass the listener as the first argument? */
	if ((s = find_session_elem_by_pid(listener, pid))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Outbound session for %s exited unexpectedly!\n",
				switch_core_session_get_uuid(s->session));
		/* TODO - if a spawned process that was handling an outbound call fails.. what do we do with the call? */
		remove_session_elem_from_listener(listener, s);
	}

	if (listener->log_process.type == ERLANG_PID && !ei_compare_pids(&listener->log_process.pid, pid)) {
		void *pop;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Log handler process for node %s exited\n", pid->node);
		/*purge the log queue */
		while (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS);

		if (switch_test_flag(listener, LFLAG_LOG)) {
			switch_clear_flag_locked(listener, LFLAG_LOG);
		}
	}

	if (listener->event_process.type == ERLANG_PID && !ei_compare_pids(&listener->event_process.pid, pid)) {
		void *pop;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Event handler process for node %s exited\n", pid->node);
		/*purge the event queue */
		while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS);

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
		ei_x_new(&buf);

		ei_x_buff rbuf;
		ei_x_new_with_version(&rbuf);

		switch_mutex_lock(listener->sock_mutex);
		status = ei_xreceive_msg_tmo(listener->sockfd, &msg, &buf, 100);
		switch_mutex_unlock(listener->sock_mutex);

		switch(status) {
			case ERL_TICK :
				break;
			case ERL_MSG :
				switch(msg.msgtype) {
					case ERL_SEND :
#ifdef EI_DEBUG
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_send\n");

						ei_x_print_msg(&buf, &msg.from, 0);
						/*ei_s_print_term(&pbuf, buf.buff, &i);*/
						/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_send was message %s\n", pbuf);*/
#endif

						if (handle_msg(listener, &msg, &buf, &rbuf)) {
							return;
						}
						break;
					case ERL_REG_SEND :
#ifdef EI_DEBUG
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_reg_send to %s\n", msg.toname);

						ei_x_print_reg_msg(&buf, msg.toname, 0);
						/*i = 1;*/
						/*ei_s_print_term(&pbuf, buf.buff, &i);*/
						/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_reg_send was message %s\n", pbuf);*/
#endif

						if (handle_msg(listener, &msg, &buf, &rbuf)) {
						    return;
						}
						break;
					case ERL_LINK :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_link\n");
						break;
					case ERL_UNLINK :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_unlink\n");
						break;
					case ERL_EXIT :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_exit from %s <%d.%d.%d>\n", msg.from.node, msg.from.creation, msg.from.num, msg.from.serial);
						handle_exit(listener, &msg.from);
						break;
					default :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unexpected msg type %d\n", (int)(msg.msgtype));
						break;
				}
				break;
			case ERL_ERROR :
				if (erl_errno != ETIMEDOUT && erl_errno != EAGAIN) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_error\n");
				}
				break;
			default :
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unexpected status %d \n", status);
				break;
		}

		ei_x_free(&buf);
		ei_x_free(&rbuf);

		check_log_queue(listener);
		check_event_queue(listener);
		if (SWITCH_STATUS_SUCCESS != check_attached_sessions(listener)) {
			return;
		}
	}
}

static switch_bool_t check_inbound_acl(listener_t* listener)
{
	/* check acl to see if inbound connection is allowed */
	if (prefs.acl_count && !switch_strlen_zero(listener->remote_ip)) {
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

					ei_send(listener->sockfd, &msg.from, rbuf.buff, rbuf.index);
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
	session_elem_t* s;

	switch_mutex_lock(globals.listener_mutex);
	prefs.threads++;
	switch_mutex_unlock(globals.listener_mutex);

	switch_core_hash_init(&listener->fetch_reply_hash, listener->pool);
	switch_core_hash_init(&listener->spawn_pid_hash, listener->pool);

	switch_assert(listener != NULL);

	if (check_inbound_acl(listener)) {
		if (switch_strlen_zero(listener->remote_ip)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open from %s\n", listener->remote_ip);/*, listener->remote_port);*/
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
	for (s = listener->session_list; s; s = s->next) {
		switch_channel_clear_flag(switch_core_session_get_channel(s->session), CF_CONTROLLED);
		/* this allows the application threads to exit */
		switch_clear_flag_locked(s, LFLAG_SESSION_ALIVE);
		/* */
		switch_core_session_rwunlock(s->session);
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

	if (switch_strlen_zero(prefs.ip)) {
		set_pref_ip("127.0.0.1");
	}

	if (switch_strlen_zero(prefs.cookie)) {
		set_pref_cookie("ClueCon");
	}

	if (!prefs.port) {
		prefs.port = 8031;
	}

	if (!prefs.nodename) {
		prefs.nodename = "freeswitch";
	}

	return 0;
}


static listener_t* new_listener(struct ei_cnode_s *ec, int clientfd)
{
	switch_memory_pool_t *listener_pool = NULL;
	listener_t* listener = NULL;

	if (switch_core_new_memory_pool(&listener_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return NULL;
	}

	if (!(listener = switch_core_alloc(listener_pool, sizeof(*listener)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
		return NULL;
	}

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

static listener_t* new_outbound_listener(char* node)
{
	listener_t* listener = NULL;
	struct ei_cnode_s ec;
	int clientfd;

	if (SWITCH_STATUS_SUCCESS==initialise_ei(&ec)) {
		errno = 0;
		if ((clientfd=ei_connect(&ec,node)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error connecting to node %s (erl_errno=%d, errno=%d)!\n",node,erl_errno,errno);
			return NULL;
		}
		listener = new_listener(&ec,clientfd);
		listener->peer_nodename = switch_core_strdup(listener->pool,node);
	}
	return listener;
}


session_elem_t* attach_call_to_registered_process(listener_t* listener, char* reg_name, switch_core_session_t *session)
{
	/* create a session list element */
	session_elem_t* session_element = NULL;
	if (!(session_element = switch_core_alloc(switch_core_session_get_pool(session), sizeof(*session_element)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate session element\n");
	}
	else {
		if (SWITCH_STATUS_SUCCESS != switch_core_session_read_lock(session)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get session read lock\n");
		}
		else {
			session_element->session = session;
			session_element->process.type = ERLANG_REG_PROCESS;
			session_element->process.reg_name = switch_core_strdup(switch_core_session_get_pool(session),reg_name);
			switch_set_flag(session_element, LFLAG_SESSION_ALIVE);
			switch_clear_flag(session_element, LFLAG_OUTBOUND_INIT);
			switch_queue_create(&session_element->event_queue, SWITCH_CORE_QUEUE_LEN, switch_core_session_get_pool(session));
			switch_mutex_init(&session_element->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
			/* attach the session to the listener */
			add_session_elem_to_listener(listener,session_element);
		}
	}
	return session_element;
}


session_elem_t* attach_call_to_spawned_process(listener_t* listener, char *module, char *function, switch_core_session_t *session)
{
	/* create a session list element */
	session_elem_t* session_element=NULL;
	if (!(session_element = switch_core_alloc(switch_core_session_get_pool(session), sizeof(*session_element)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate session element\n");
	}
	else {
		if (SWITCH_STATUS_SUCCESS != switch_core_session_read_lock(session)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get session read lock\n");
		}
		else {
			char *argv[1], hash[100];
			int i = 0;
			session_element->session = session;
			erlang_pid *pid;
			erlang_ref ref;

			switch_set_flag(session_element, LFLAG_WAITING_FOR_PID);
			switch_queue_create(&session_element->event_queue, SWITCH_CORE_QUEUE_LEN, switch_core_session_get_pool(session));
			switch_mutex_init(&session_element->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
			/* attach the session to the listener */
			add_session_elem_to_listener(listener,session_element);

			if (!strcmp(function, "!")) {
				/* send a message to request a pid */
				ei_x_buff rbuf;
				ei_x_new_with_version(&rbuf);

				ei_init_ref(listener->ec, &ref);
				ei_x_encode_tuple_header(&rbuf, 3);
				ei_x_encode_atom(&rbuf, "new_pid");
				ei_x_encode_ref(&rbuf, &ref);
				ei_x_encode_pid(&rbuf, ei_self(listener->ec));
				ei_reg_send(listener->ec, listener->sockfd, module, rbuf.buff, rbuf.index);
#ifdef EI_DEBUG
				ei_x_print_reg_msg(&rbuf, module, 1);
#endif
				ei_x_free(&rbuf);
			} else {
				ei_spawn(listener->ec, listener->sockfd, &ref, module, function, 0, argv);
			}

			ei_hash_ref(&ref, hash);

			while (!(pid = (erlang_pid *) switch_core_hash_find(listener->spawn_pid_hash, hash))) {
				if (i > 50) { /* half a second timeout */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Timed out when waiting for outbound pid\n");
					switch_core_session_rwunlock(session);
					remove_session_elem_from_listener(listener,session_element);
					return NULL;
				}
				i++;
				switch_yield(10000); /* 10ms */
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got pid!\n");

			session_element->process.type = ERLANG_PID;
			memcpy(&session_element->process.pid, pid, sizeof(erlang_pid));
			switch_set_flag(session_element, LFLAG_SESSION_ALIVE);
			switch_clear_flag(session_element, LFLAG_OUTBOUND_INIT);
			switch_clear_flag(session_element, LFLAG_WAITING_FOR_PID);

			ei_link(listener, ei_self(listener->ec), pid);
		}
	}
	return session_element;
}


int count_listener_sessions(listener_t *listener) {
	session_elem_t *last,*sp;
	int count = 0;

	switch_mutex_lock(listener->session_mutex);
	sp = listener->session_list;
	last = NULL;
	while(sp) {
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
	int argc = 0, argc2=0;
	char *argv[80] = { 0 }, *argv2[80] = { 0 };
	char *mydata, *myarg;
	switch_bool_t new_session = SWITCH_FALSE;
	session_elem_t* session_element=NULL;

	/* process app arguments */
	if (data && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	} /* XXX else? */
	if (argc < 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error - need registered name and node!\n");
		return;
	}
	if (switch_strlen_zero(argv[0])) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing registered name or module:function!\n");
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
	if (switch_strlen_zero(node)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing node name!\n");
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "enter erlang_outbound_function %s %s\n",argv[0], node);

	/* first work out if there is a listener already talking to the node we want to talk to */
	listener = find_listener(node);
	/* if there is no listener, then create one */
	if (!listener) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Creating new listener for session\n");
		new_session = SWITCH_TRUE;
		listener = new_outbound_listener(node);
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Using existing listener for session\n");
	}

	if (listener) {
		if (new_session == SWITCH_TRUE) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Launching new listener\n");
			launch_listener_thread(listener);
		}

		if (module && function) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Creating new spawned session for listener\n");
			session_element=attach_call_to_spawned_process(listener, module, function, session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Creating new registered session for listener\n");
			session_element=attach_call_to_registered_process(listener, reg_name, session);
		}

		if (session_element) {

			switch_ivr_park(session, NULL);

			/* keep app thread running for lifetime of session */
			if (switch_channel_get_state(switch_core_session_get_channel(session)) >= CS_HANGUP) {
				while (switch_test_flag(session_element, LFLAG_SESSION_ALIVE)) {
					switch_yield(100000);
				}
			}
		}
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "exit erlang_outbound_function\n");
}


/* 'erlang' console stuff */
SWITCH_STANDARD_API(erlang_cmd)
{
	char *argv[1024] = { 0 };
	int argc = 0;
	char *mycmd = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	const char *usage_string = "Supply some arguments, maybe?";

	if (switch_strlen_zero(cmd)) {
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

		for (l = listen_list.listeners; l; l = l->next) {
			stream->write_function(stream, "Listener to %s with %d outbound sessions\n", l->peer_nodename, count_listener_sessions(l));
		}

		switch_mutex_unlock(globals.listener_mutex);
	} else {
		stream->write_function(stream, "I don't care for those arguments at all, sorry");
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

	switch_mutex_init(&globals.listener_mutex, SWITCH_MUTEX_NESTED, pool);
	
	/* intialize the unique reference stuff */
	switch_mutex_init(&globals.ref_mutex, SWITCH_MUTEX_NESTED, pool);
	globals.reference0 = 0;
	globals.reference1 = 0;
	globals.reference2 = 0;

	if (switch_event_bind_removable(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		close_socket(&listen_list.sockfd);
		return SWITCH_STATUS_GENERR;
	}

	switch_log_bind_logger(socket_logger, SWITCH_LOG_DEBUG, SWITCH_FALSE);

	memset(&bindings, 0, sizeof(bindings));

	if (switch_xml_bind_search_function_ret(erlang_fetch, (1 << sizeof(switch_xml_section_enum_t)), NULL, &bindings.search_binding) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		close_socket(&listen_list.sockfd);
		return SWITCH_STATUS_GENERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sections %d\n", switch_xml_get_binding_sections(bindings.search_binding));

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "erlang", "Connect to an erlang node", "Connect to erlang", erlang_outbound_function, "<registered name> <node@host>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_API(api_interface, "erlang", "PortAudio", erlang_cmd, "<command> [<args>]");
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

	memset(&listen_list, 0, sizeof(listen_list));
	config();

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_init(&listen_list.sock_mutex, SWITCH_MUTEX_NESTED, pool);

	/* zero out the struct before we use it */
	memset(&server_addr, 0, sizeof(server_addr));

	/* convert the configured IP to network byte order, handing errors */
	rv = inet_pton(AF_INET, prefs.ip, &server_addr.sin_addr.s_addr);
	if (rv == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not parse invalid ip address: %s\n", prefs.ip);
		return SWITCH_STATUS_GENERR;
	} else if (rv == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error when parsing ip address %s : %s\n", prefs.ip, strerror(errno));
		return SWITCH_STATUS_GENERR;
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

		if (setsockopt(listen_list.sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to enable SO_REUSEADDR for socket on %s:%u : %s\n", prefs.ip, prefs.port, strerror(errno));
			goto sock_fail;
		}
		
		if (bind(listen_list.sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to bind to %s:%u\n", prefs.ip, prefs.port);
			goto sock_fail;
		}
		
		if (listen(listen_list.sockfd, 5) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to listen on %s:%u\n", prefs.ip, prefs.port);
			goto sock_fail;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Socket up listening on %s:%u\n", prefs.ip, prefs.port);
		break;
	  sock_fail:
		switch_yield(100000);
	}

	if (SWITCH_STATUS_SUCCESS!=initialise_ei(&ec)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to init ei connection\n");
		close_socket(&listen_list.sockfd);
		return SWITCH_STATUS_GENERR;
	}

	/* return value is -1 for error, a descriptor pointing to epmd otherwise */
	if ((epmdfd = ei_publish(&ec, prefs.port)) == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to publish port to empd, trying to start empd manually\n");
		if (system("epmd -daemon")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to start empd manually\n");
			close_socket(&listen_list.sockfd);
			return SWITCH_STATUS_GENERR;
		}
		if ((epmdfd = ei_publish(&ec, prefs.port)) == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to publish port to empd AGAIN\n");
			close_socket(&listen_list.sockfd);
			return SWITCH_STATUS_GENERR;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connected and published erlang cnode at %s\n", ec.thisnodename);

	listen_list.ready = 1;

	for (;;) {
		/* zero out errno because ei_accept doesn't differentiate between a
		 * failed authentication or a socket failure, or a client version
		 * mismatch or a godzilla attack */
		errno = 0;
		if ((clientfd = ei_accept_tmo(&ec, listen_list.sockfd, &conn, 100)) == ERL_ERROR) {
			if (prefs.done) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Shutting Down\n");
			} else if (erl_errno == ETIMEDOUT) {
				continue;
			} else if (errno) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error %d %d\n", erl_errno, errno);
			} else {
				/* if errno didn't get set, assume nothing *too* horrible occured */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
						"Ignorable error in ei_accept - probable bad client version, bad cookie or bad nodename\n");
				continue;
			}
			break;
		}

		listener = new_listener(&ec,clientfd);
		if (listener) {
			/* store the IP and node name we are talking with */
			inet_ntop(AF_INET, conn.ipadr, listener->remote_ip, sizeof(listener->remote_ip));
			listener->peer_nodename = switch_core_strdup(listener->pool,conn.nodename);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Launching listener, connection from node %s, ip %s\n", conn.nodename, listener->remote_ip);
			launch_listener_thread(listener);
		}
		else
			/* if we fail to create a listener (memory error), then the module will exit */
			break;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Exiting module mod_erlang_event\n");

	/* cleanup epmd registration */
	ei_unpublish(&ec);
	close(epmdfd);

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

	prefs.done = 1;

	switch_log_unbind_logger(socket_logger);

	/*close_socket(&listen_list.sockfd);*/
	
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

	switch_mutex_unlock(globals.listener_mutex);

	switch_sleep(1500000); /* sleep for 1.5 seconds */

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
