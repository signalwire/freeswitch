/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Karl Anderson <karl@2600hz.com>
 * Darren Schreiber <darren@2600hz.com>
 *
 *
 * kazoo_event_streams.c -- Event Publisher
 *
 */
#include "mod_kazoo.h"

/* Blatantly repurposed from switch_eventc */
static char *my_dup(const char *s) {
	size_t len = strlen(s) + 1;
	void *new = malloc(len);
	switch_assert(new);

	return (char *) memcpy(new, s, len);
}

#ifndef DUP
#define DUP(str) my_dup(str)
#endif

static const char* private_headers[] = {"variable_sip_h_", "sip_h_", "P-", "X-"};

static int is_private_header(const char *name) {
	for(int i=0; i < 4; i++) {
		if(!strncmp(name, private_headers[i], strlen(private_headers[i]))) {
			return 1;
		}
	}
	return 0;
}

static switch_status_t kazoo_event_dup(switch_event_t **clone, switch_event_t *event, switch_hash_t *filter) {
	switch_event_header_t *header;

	if (switch_event_create_subclass(clone, SWITCH_EVENT_CLONE, event->subclass_name) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	(*clone)->event_id = event->event_id;
	(*clone)->event_user_data = event->event_user_data;
	(*clone)->bind_user_data = event->bind_user_data;
	(*clone)->flags = event->flags;

	for (header = event->headers; header; header = header->next) {
		if (event->subclass_name && !strcmp(header->name, "Event-Subclass")) {
			continue;
		}

		if (strncmp(header->name, globals.kazoo_var_prefix, globals.var_prefix_length)
			&& filter
			&& !switch_core_hash_find(filter, header->name)
			&& (!globals.send_all_headers)
			&& (!(globals.send_all_private_headers && is_private_header(header->name)))
			)
			{
				continue;
			}

        if (header->idx) {
            int i;
            for (i = 0; i < header->idx; i++) {
                switch_event_add_header_string(*clone, SWITCH_STACK_PUSH, header->name, header->array[i]);
            }
        } else {
            switch_event_add_header_string(*clone, SWITCH_STACK_BOTTOM, header->name, header->value);
        }
    }

    if (event->body) {
        (*clone)->body = DUP(event->body);
    }

    (*clone)->key = event->key;

    return SWITCH_STATUS_SUCCESS;
}

static void event_handler(switch_event_t *event) {
	switch_event_t *clone = NULL;
	ei_event_stream_t *event_stream = (ei_event_stream_t *) event->bind_user_data;

	/* if mod_kazoo or the event stream isn't running dont push a new event */
	if (!switch_test_flag(event_stream, LFLAG_RUNNING) || !switch_test_flag(&globals, LFLAG_RUNNING)) {
		return;
	}

	if (event->event_id == SWITCH_EVENT_CUSTOM) {
		ei_event_binding_t *event_binding = event_stream->bindings;
		unsigned short int found = 0;

		if (!event->subclass_name) {
			return;
		}

		while(event_binding != NULL) {
			if (event_binding->type == SWITCH_EVENT_CUSTOM) {
				if(event_binding->subclass_name
				   && !strcmp(event->subclass_name, event_binding->subclass_name)) {
					found = 1;
					break;
				}
			}
			event_binding = event_binding->next;
		}

		if (!found) {
			return;
		}
	}

	/* try to clone the event and push it to the event stream thread */
	/* TODO: someday maybe the filter comes from the event_stream (set during init only)
	 * and is per-binding so we only send headers that a process requests */
	if (kazoo_event_dup(&clone, event, globals.event_filter) == SWITCH_STATUS_SUCCESS) {
		if (switch_queue_trypush(event_stream->queue, clone) != SWITCH_STATUS_SUCCESS) {
			/* if we couldn't place the cloned event into the listeners */
			/* event queue make sure we destroy it, real good like */
			switch_event_destroy(&clone);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory error: Have a good trip? See you next fall!\n");
	}
}

static void *SWITCH_THREAD_FUNC event_stream_loop(switch_thread_t *thread, void *obj) {
    ei_event_stream_t *event_stream = (ei_event_stream_t *) obj;
	ei_event_binding_t *event_binding;
	switch_sockaddr_t *sa;
	uint16_t port;
    char ipbuf[25];
    const char *ip_addr;
	void *pop;
	short event_stream_framing = globals.event_stream_framing;

	switch_atomic_inc(&globals.threads);

	switch_assert(event_stream != NULL);

	/* figure out what socket we just opened */
	switch_socket_addr_get(&sa, SWITCH_FALSE, event_stream->acceptor);
	port = switch_sockaddr_get_port(sa);
    ip_addr = switch_get_addr(ipbuf, sizeof(ipbuf), sa);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Starting erlang event stream %p on %s:%u for %s <%d.%d.%d>\n"
					  ,(void *)event_stream, ip_addr, port, event_stream->pid.node, event_stream->pid.creation
					  ,event_stream->pid.num, event_stream->pid.serial);

	while (switch_test_flag(event_stream, LFLAG_RUNNING) && switch_test_flag(&globals, LFLAG_RUNNING)) {
		const switch_pollfd_t *fds;
		int32_t numfds;

		/* check if a new connection is pending */
		if (switch_pollset_poll(event_stream->pollset, 0, &numfds, &fds) == SWITCH_STATUS_SUCCESS) {
			for (int32_t i = 0; i < numfds; i++) {
				switch_socket_t *newsocket;

				/* accept the new client connection */
				if (switch_socket_accept(&newsocket, event_stream->acceptor, event_stream->pool) == SWITCH_STATUS_SUCCESS) {
					switch_sockaddr_t *sa;

                    if (switch_socket_opt_set(newsocket, SWITCH_SO_NONBLOCK, TRUE)) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't set socket as non-blocking\n");
                    }

                    if (switch_socket_opt_set(newsocket, SWITCH_SO_TCP_NODELAY, 1)) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't disable Nagle.\n");
                    }

					/* close the current client, if there is one */
					close_socket(&event_stream->socket);

					switch_mutex_lock(event_stream->socket_mutex);
					/* start sending to the new client */
					event_stream->socket = newsocket;

					switch_socket_addr_get(&sa, SWITCH_TRUE, newsocket);
					event_stream->local_port = switch_sockaddr_get_port(sa);
					switch_get_addr(event_stream->remote_ip, sizeof (event_stream->remote_ip), sa);

					switch_socket_addr_get(&sa, SWITCH_FALSE, newsocket);
					event_stream->remote_port = switch_sockaddr_get_port(sa);
					switch_get_addr(event_stream->local_ip, sizeof (event_stream->local_ip), sa);

					event_stream->connected = SWITCH_TRUE;
					switch_mutex_unlock(event_stream->socket_mutex);

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Erlang event stream %p client %s:%u\n", (void *)event_stream, event_stream->remote_ip, event_stream->remote_port);
				}
			}
		}

		/* if there was an event waiting in our queue send it to the client */
		if (switch_queue_pop_timeout(event_stream->queue, &pop, 500000) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *event = (switch_event_t *) pop;

			if (event_stream->socket) {
				ei_x_buff ebuf;
				char byte;
				short i = event_stream_framing;
				switch_size_t size = 1;

				if(globals.event_stream_preallocate > 0) {
					ebuf.buff = malloc(globals.event_stream_preallocate);
					ebuf.buffsz = globals.event_stream_preallocate;
					ebuf.index = 0;
					if(ebuf.buff == NULL) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not pre-allocate memory for mod_kazoo message\n");
						break;
					}
					ei_x_encode_version(&ebuf);
				} else {
					ei_x_new_with_version(&ebuf);
				}

				ei_encode_switch_event(&ebuf, event);

				if (globals.event_stream_preallocate > 0 && ebuf.buffsz > globals.event_stream_preallocate) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "increased event stream buffer size to %d\n", ebuf.buffsz);
				}

				while (i) {
					byte = ebuf.index >> (8 * --i);
					switch_socket_send(event_stream->socket, &byte, &size);
				}

				size = (switch_size_t)ebuf.index;
				switch_socket_send(event_stream->socket, ebuf.buff, &size);

				ei_x_free(&ebuf);
			}

			switch_event_destroy(&event);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Shutting down erlang event stream %p\n", (void *)event_stream);

	/* unbind from the system events */
	event_binding = event_stream->bindings;
	while(event_binding != NULL) {
		switch_event_unbind(&event_binding->node);
		event_binding = event_binding->next;
	}
	event_stream->bindings = NULL;

	/* clear and destroy any remaining queued events */
	while (switch_queue_trypop(event_stream->queue, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_event_t *event = (switch_event_t *) pop;
		switch_event_destroy(&event);
	}

	/* remove the acceptor pollset */
	switch_pollset_remove(event_stream->pollset, event_stream->pollfd);

	/* close any open sockets */
	close_socket(&event_stream->acceptor);

	switch_mutex_lock(event_stream->socket_mutex);
	event_stream->connected = SWITCH_FALSE;
	close_socket(&event_stream->socket);
	switch_mutex_unlock(event_stream->socket_mutex);

	switch_mutex_destroy(event_stream->socket_mutex);

	/* clean up the memory */
	switch_core_destroy_memory_pool(&event_stream->pool);

	switch_atomic_dec(&globals.threads);

	return NULL;
}

ei_event_stream_t *new_event_stream(ei_event_stream_t **event_streams, const erlang_pid *from) {
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = NULL;
	ei_event_stream_t *event_stream;

	/* create memory pool for this event stream */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory: How many Alzheimer's patients does it take to screw in a light bulb? To get to the other side.\n");
		return NULL;
	}

	/* from the memory pool, allocate the event stream structure */
	if (!(event_stream = switch_core_alloc(pool, sizeof (*event_stream)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory: I may have Alzheimers but at least I dont have Alzheimers.\n");
		return NULL;
	}

	/* prepare the event stream */
	memset(event_stream, 0, sizeof(*event_stream));
	event_stream->bindings = NULL;
	event_stream->pool = pool;
	event_stream->connected = SWITCH_FALSE;
	memcpy(&event_stream->pid, from, sizeof(erlang_pid));
	switch_queue_create(&event_stream->queue, MAX_QUEUE_LEN, pool);

	/* create a socket for accepting the event stream client */
    if (!(event_stream->acceptor = create_socket(pool))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Like car accidents, most hardware problems are due to driver error.\n");
		/* TODO: clean up */
        return NULL;
    }

	if (switch_socket_opt_set(event_stream->acceptor, SWITCH_SO_NONBLOCK, TRUE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Hey, it compiles!\n");
		/* TODO: clean up */
        return NULL;
	}

	/* create a pollset so we can efficiently check for new client connections */
	if (switch_pollset_create(&event_stream->pollset, 1000, pool, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "My software never has bugs. It just develops random features.\n");
		/* TODO: clean up */
        return NULL;
	}

	switch_socket_create_pollfd(&event_stream->pollfd, event_stream->acceptor, SWITCH_POLLIN | SWITCH_POLLERR, NULL, pool);
	if (switch_pollset_add(event_stream->pollset, event_stream->pollfd) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "If you saw a heat wave, would you wave back?\n");
		/* TODO: clean up */
        return NULL;
	}

	switch_mutex_init(&event_stream->socket_mutex, SWITCH_MUTEX_DEFAULT, pool);

	/* add the new event stream to the link list
	 * since the event streams link list is only
	 * accessed from the same thread no locks
	 * are required */
	if (!*event_streams) {
		*event_streams = event_stream;
	} else {
		event_stream->next = *event_streams;
		*event_streams = event_stream;
	}

	/* when we start we are running */
	switch_set_flag(event_stream, LFLAG_RUNNING);

	switch_threadattr_create(&thd_attr, event_stream->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, event_stream_loop, event_stream, event_stream->pool);

	return event_stream;
}

unsigned long get_stream_port(const ei_event_stream_t *event_stream) {
	switch_sockaddr_t *sa;
	switch_socket_addr_get(&sa, SWITCH_FALSE, event_stream->acceptor);
	return (unsigned long) switch_sockaddr_get_port(sa);
}

ei_event_stream_t *find_event_stream(ei_event_stream_t *event_stream, const erlang_pid *from) {
	while (event_stream != NULL) {
		if (ei_compare_pids(&event_stream->pid, from) == SWITCH_STATUS_SUCCESS) {
			return event_stream;
		}
		event_stream = event_stream->next;
	}

	return NULL;
}

switch_status_t remove_event_stream(ei_event_stream_t **event_streams, const erlang_pid *from) {
	ei_event_stream_t *event_stream, *prev = NULL;
	int found = 0;

	/* if there are no event bindings there is nothing to do */
	if (!*event_streams) {
		return SWITCH_STATUS_SUCCESS;
	}

	/* try to find the event stream for the client process */
	event_stream = *event_streams;
	while(event_stream != NULL) {
		if (ei_compare_pids(&event_stream->pid, from) == SWITCH_STATUS_SUCCESS) {
			found = 1;
			break;
		}

		prev = event_stream;
		event_stream = event_stream->next;
	}

	if (found) {
		/* if we found an event stream remove it from
		 * from the link list */
		if (!prev) {
			*event_streams = event_stream->next;
		} else {
			prev->next = event_stream->next;
		}

		/* stop the event stream thread */
		switch_clear_flag(event_stream, LFLAG_RUNNING);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t remove_event_streams(ei_event_stream_t **event_streams) {
	ei_event_stream_t *event_stream = *event_streams;

	while(event_stream != NULL) {
		/* stop the event bindings publisher thread */
		switch_clear_flag(event_stream, LFLAG_RUNNING);

		event_stream = event_stream->next;
	}

	*event_streams = NULL;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t add_event_binding(ei_event_stream_t *event_stream, const switch_event_types_t event_type, const char *subclass_name) {
	ei_event_binding_t *event_binding = event_stream->bindings;

	/* check if the event binding already exists, ignore if so */
	while(event_binding != NULL) {
		if (event_binding->type == SWITCH_EVENT_CUSTOM) {
			if(subclass_name
			   && event_binding->subclass_name
			   && !strcmp(subclass_name, event_binding->subclass_name)) {
				return SWITCH_STATUS_SUCCESS;
			}
		} else if (event_binding->type == event_type) {
			return SWITCH_STATUS_SUCCESS;
		}
		event_binding = event_binding->next;
	}

	/* from the event stream memory pool, allocate the event binding structure */
	if (!(event_binding = switch_core_alloc(event_stream->pool, sizeof (*event_binding)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of random-access memory, attempting write-only memory\n");
		return SWITCH_STATUS_FALSE;
	}

	/* prepare the event binding struct */
	event_binding->type = event_type;
	if (!subclass_name || zstr(subclass_name)) {
		event_binding->subclass_name = NULL;
	} else {
		/* TODO: free strdup? */
		event_binding->subclass_name = strdup(subclass_name);
	}
	event_binding->next = NULL;

	/* bind to the event with a unique ID and capture the event_node pointer */
	switch_uuid_str(event_binding->id, sizeof(event_binding->id));
	if (switch_event_bind_removable(event_binding->id, event_type, subclass_name, event_handler, event_stream, &event_binding->node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to bind to event %s %s!\n"
						  ,switch_event_name(event_binding->type), event_binding->subclass_name ? event_binding->subclass_name : "");
		return SWITCH_STATUS_GENERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding event binding %s to stream %p for %s <%d.%d.%d>: %s %s\n"
					  ,event_binding->id, (void *)event_stream, event_stream->pid.node, event_stream->pid.creation
					  ,event_stream->pid.num, event_stream->pid.serial, switch_event_name(event_binding->type)
					  ,event_binding->subclass_name ? event_binding->subclass_name : "");

	/* add the new binding to the list */
	if (!event_stream->bindings) {
		event_stream->bindings = event_binding;
	} else {
		event_binding->next = event_stream->bindings;
		event_stream->bindings = event_binding;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t remove_event_binding(ei_event_stream_t *event_stream, const switch_event_types_t event_type, const char *subclass_name) {
	ei_event_binding_t *event_binding = event_stream->bindings, *prev = NULL;
	int found = 0;

	/* if there are no bindings then there is nothing to do */
	if (!event_binding) {
		return SWITCH_STATUS_SUCCESS;
	}

	/* try to find the event binding specified */
	while(event_binding != NULL) {
		if (event_binding->type == SWITCH_EVENT_CUSTOM
			&& subclass_name
			&& event_binding->subclass_name
			&& !strcmp(subclass_name, event_binding->subclass_name)) {
			found = 1;
			break;
		} else if (event_binding->type == event_type) {
			found = 1;
			break;
		}

		prev = event_binding;
		event_binding = event_binding->next;
	}

	if (found) {
		/* if the event binding exists, unbind from the system */
		switch_event_unbind(&event_binding->node);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing event binding %s from %p for %s <%d.%d.%d>: %s %s\n"
						  ,event_binding->id, (void *)event_stream, event_stream->pid.node, event_stream->pid.creation
						  ,event_stream->pid.num, event_stream->pid.serial, switch_event_name(event_binding->type)
						  ,event_binding->subclass_name ? event_binding->subclass_name : "");

		/* remove the event binding from the list */
		if (!prev) {
			event_stream->bindings = event_binding->next;
		} else {
			prev->next = event_binding->next;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t remove_event_bindings(ei_event_stream_t *event_stream) {
	ei_event_binding_t *event_binding = event_stream->bindings;

	/* if there are no bindings then there is nothing to do */
	if (!event_binding) {
		return SWITCH_STATUS_SUCCESS;
	}

	/* try to find the event binding specified */
	while(event_binding != NULL) {
		/* if the event binding exists, unbind from the system */
		switch_event_unbind(&event_binding->node);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing event binding %s from %p for %s <%d.%d.%d>: %s %s\n"
						  ,event_binding->id, (void *)event_stream, event_stream->pid.node, event_stream->pid.creation
						  ,event_stream->pid.num, event_stream->pid.serial, switch_event_name(event_binding->type)
						  ,event_binding->subclass_name ? event_binding->subclass_name : "");

		event_binding = event_binding->next;
	}

	event_stream->bindings = NULL;

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
