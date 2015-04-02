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
 * kazoo_fetch.c -- XML fetch request handler
 *
 */
#include "mod_kazoo.h"

struct xml_fetch_reply_s {
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *xml_str;
	struct xml_fetch_reply_s *next;
};
typedef struct xml_fetch_reply_s xml_fetch_reply_t;

struct fetch_handler_s {
	erlang_pid pid;
	struct fetch_handler_s *next;
};
typedef struct fetch_handler_s fetch_handler_t;

struct ei_xml_client_s {
	ei_node_t *ei_node;
	fetch_handler_t *fetch_handlers;
	struct ei_xml_client_s *next;
};
typedef struct ei_xml_client_s ei_xml_client_t;

struct ei_xml_agent_s {
	switch_memory_pool_t *pool;
	switch_xml_section_t section;
	switch_thread_rwlock_t *lock;
	ei_xml_client_t *clients;
	switch_mutex_t *current_client_mutex;
	ei_xml_client_t *current_client;
	switch_mutex_t *replies_mutex;
	switch_thread_cond_t *new_reply;
	xml_fetch_reply_t *replies;
};
typedef struct ei_xml_agent_s ei_xml_agent_t;

static char *xml_section_to_string(switch_xml_section_t section) {
	switch(section) {
	case SWITCH_XML_SECTION_CONFIG:
		return "configuration";
	case SWITCH_XML_SECTION_DIRECTORY:
		return "directory";
	case SWITCH_XML_SECTION_DIALPLAN:
		return "dialplan";
	case SWITCH_XML_SECTION_CHATPLAN:
		return "chatplan";
	case SWITCH_XML_SECTION_CHANNELS:
		return "channels";
	default:
		return "unknown";
	}
}

static char *expand_vars(char *xml_str) {
	char *var, *val;
	char *rp = xml_str; /* read pointer */
	char *ep, *wp, *buff; /* end pointer, write pointer, write buffer */

	if (!(strstr(xml_str, "$${"))) {
		return xml_str;
	}

	switch_zmalloc(buff, strlen(xml_str) * 2);
	wp = buff;
	ep = buff + (strlen(xml_str) * 2) - 1;

	while (*rp && wp < ep) {
		if (*rp == '$' && *(rp + 1) == '$' && *(rp + 2) == '{') {
			char *e = switch_find_end_paren(rp + 2, '{', '}');

			if (e) {
				rp += 3;
				var = rp;
				*e++ = '\0';
				rp = e;

				if ((val = switch_core_get_variable_dup(var))) {
					char *p;
					for (p = val; p && *p && wp <= ep; p++) {
						*wp++ = *p;
					}
					switch_safe_free(val);
				}
				continue;
			}
		}

		*wp++ = *rp++;
	}

	*wp++ = '\0';

	switch_safe_free(xml_str);
	return buff;
}

static switch_xml_t fetch_handler(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params, void *user_data) {
	switch_xml_t xml = NULL;
	switch_uuid_t uuid;
	switch_time_t now = 0;
	ei_xml_agent_t *agent = (ei_xml_agent_t *) user_data;
	ei_xml_client_t *client;
	fetch_handler_t *fetch_handler;
	xml_fetch_reply_t reply, *pending, *prev = NULL;

	now = switch_micro_time_now();

	if (!switch_test_flag(&globals, LFLAG_RUNNING)) {
		return xml;
	}

	/* read-lock the agent */
	switch_thread_rwlock_rdlock(agent->lock);

	/* serialize access to current, used to round-robin requests */
	/* TODO: check globals for round-robin boolean or loop all clients */
	switch_mutex_lock(agent->current_client_mutex);
	if (!agent->current_client) {
		client = agent->clients;
	} else {
		client = agent->current_client;
	}
	if (client) {
		agent->current_client = client->next;
	}
	switch_mutex_unlock(agent->current_client_mutex);

	/* no client, no work required */
	if (!client || !client->fetch_handlers) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "No %s XML erlang handler currently available\n"
						  ,section);
		switch_thread_rwlock_unlock(agent->lock);
		return xml;
	}

	/* prepare the reply collector */
	switch_uuid_get(&uuid);
	switch_uuid_format(reply.uuid_str, &uuid);
	reply.next = NULL;
	reply.xml_str = NULL;

	/* add our reply placeholder to the replies list */
	switch_mutex_lock(agent->replies_mutex);
	if (!agent->replies) {
		agent->replies = &reply;
	} else {
		reply.next = agent->replies;
		agent->replies = &reply;
	}
	switch_mutex_unlock(agent->replies_mutex);

	fetch_handler = client->fetch_handlers;
	while (fetch_handler != NULL) {
		ei_send_msg_t *send_msg;

		switch_malloc(send_msg, sizeof(*send_msg));
		memcpy(&send_msg->pid, &fetch_handler->pid, sizeof(erlang_pid));

		ei_x_new_with_version(&send_msg->buf);

		ei_x_encode_tuple_header(&send_msg->buf, 7);
		ei_x_encode_atom(&send_msg->buf, "fetch");
		ei_x_encode_atom(&send_msg->buf, section);
		_ei_x_encode_string(&send_msg->buf, tag_name ? tag_name : "undefined");
		_ei_x_encode_string(&send_msg->buf, key_name ? key_name : "undefined");
		_ei_x_encode_string(&send_msg->buf, key_value ? key_value : "undefined");
		_ei_x_encode_string(&send_msg->buf, reply.uuid_str);

		if (params) {
			ei_encode_switch_event_headers(&send_msg->buf, params);
		} else {
			ei_x_encode_empty_list(&send_msg->buf);
		}

		if (switch_queue_trypush(client->ei_node->send_msgs, send_msg) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send %s XML request to %s <%d.%d.%d>\n"
							  ,section
							  ,fetch_handler->pid.node
							  ,fetch_handler->pid.creation
							  ,fetch_handler->pid.num
							  ,fetch_handler->pid.serial);
			ei_x_free(&send_msg->buf);
			switch_safe_free(send_msg);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending %s XML request (%s) to %s <%d.%d.%d>\n"
							  ,section
							  ,reply.uuid_str
							  ,fetch_handler->pid.node
							  ,fetch_handler->pid.creation
							  ,fetch_handler->pid.num
							  ,fetch_handler->pid.serial);
		}

		fetch_handler = fetch_handler->next;
	}

	/* wait for a reply (if there isnt already one...amazingly improbable but lets not take shortcuts */
    switch_mutex_lock(agent->replies_mutex);

	switch_thread_rwlock_unlock(agent->lock);

	if (!reply.xml_str) {
		switch_time_t timeout;

		timeout = switch_micro_time_now() + 3000000;
		while (switch_micro_time_now() < timeout) {
			/* unlock the replies list and go to sleep, calculate a three second timeout before we started the loop
			 * plus 100ms to add a little hysteresis between the timeout and the while loop */
			switch_thread_cond_timedwait(agent->new_reply, agent->replies_mutex, (timeout - switch_micro_time_now() + 100000));

			/* if we woke up (and therefore have locked replies again) check if we got our reply
			 * otherwise we either timed-out (the while condition will fail) or one of
			 * our sibling processes got a reply and we should go back to sleep */
			if (reply.xml_str) {
				break;
			}
		}
	}

	/* find our reply placeholder in the linked list and remove it */
	pending = agent->replies;
	while (pending != NULL) {
		if (pending->uuid_str == reply.uuid_str) {
			break;
		}

		prev = pending;
		pending = pending->next;
	}

	if (pending) {
		if (!prev) {
			agent->replies = reply.next;
		} else {
			prev->next = reply.next;
		}
	}

	/* we are done with the replies link-list */
	switch_mutex_unlock(agent->replies_mutex);

	/* after all that did we get what we were after?! */
	if (reply.xml_str) {
		/* HELL YA WE DID */
		reply.xml_str = expand_vars(reply.xml_str);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received %s XML (%s) after %dms: %s\n"
						  ,section
						  ,reply.uuid_str
						  ,(unsigned int) (switch_micro_time_now() - now) / 1000
						  ,reply.xml_str);

		xml = switch_xml_parse_str_dynamic(reply.xml_str, SWITCH_FALSE);
	} else {
		/* facepalm */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Request for %s XML (%s) timed-out after %dms\n"
						  ,section
						  ,reply.uuid_str
						  ,(unsigned int) (switch_micro_time_now() - now) / 1000);
	}

	return xml;
}

static switch_status_t bind_fetch_agent(switch_xml_section_t section, switch_xml_binding_t **binding) {
	switch_memory_pool_t *pool = NULL;
	ei_xml_agent_t *agent;

	/* create memory pool for this xml search binging (lives for duration of mod_kazoo runtime) */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory: They're not people; they're hippies!\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* allocate some memory to store the fetch bindings for this section */
    if (!(agent = switch_core_alloc(pool, sizeof (*agent)))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory: Oh, Jesus tap-dancing Christ!\n");
        return SWITCH_STATUS_MEMERR;
    }

	/* try to bind to the switch */
	if (switch_xml_bind_search_function_ret(fetch_handler, section, agent, binding) != SWITCH_STATUS_SUCCESS) {
		switch_core_destroy_memory_pool(&pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not bind to FreeSWITCH %s XML requests\n"
						  ,xml_section_to_string(section));
		return SWITCH_STATUS_GENERR;
	}

	agent->pool = pool;
	agent->section = section;
	switch_thread_rwlock_create(&agent->lock, pool);
	agent->clients = NULL;
	switch_mutex_init(&agent->current_client_mutex, SWITCH_MUTEX_DEFAULT, pool);
	agent->current_client = NULL;
	switch_mutex_init(&agent->replies_mutex, SWITCH_MUTEX_DEFAULT, pool);
	switch_thread_cond_create(&agent->new_reply, pool);
	agent->replies = NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Bound to %s XML requests\n"
					  ,xml_section_to_string(section));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unbind_fetch_agent(switch_xml_binding_t **binding) {
	ei_xml_agent_t *agent;
	ei_xml_client_t *client;

	/* get a pointer to our user_data */
	agent = (ei_xml_agent_t *)switch_xml_get_binding_user_data(*binding);

	/* unbind from the switch */
	switch_xml_unbind_search_function(binding);

	/* LOCK ALL THE THINGS */
	switch_thread_rwlock_wrlock(agent->lock);
	switch_mutex_lock(agent->current_client_mutex);
	switch_mutex_lock(agent->replies_mutex);

	/* cleanly destroy each client */
	client = agent->clients;
	while(client != NULL) {
		ei_xml_client_t *tmp_client = client;
		fetch_handler_t *fetch_handler;

		fetch_handler = client->fetch_handlers;
		while(fetch_handler != NULL) {
			fetch_handler_t *tmp_fetch_handler = fetch_handler;

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removed %s XML handler %s <%d.%d.%d>\n"
							  ,xml_section_to_string(agent->section)
							  ,fetch_handler->pid.node
							  ,fetch_handler->pid.creation
							  ,fetch_handler->pid.num
							  ,fetch_handler->pid.serial);

			fetch_handler = fetch_handler->next;
			switch_safe_free(tmp_fetch_handler);
		}

		client = client->next;
		switch_safe_free(tmp_client);
	}

	/* keep the pointers clean, even if its just for a moment */
	agent->clients = NULL;
	agent->current_client = NULL;

	/* release the locks! */
	switch_thread_rwlock_unlock(agent->lock);
	switch_mutex_unlock(agent->current_client_mutex);
	switch_mutex_unlock(agent->replies_mutex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unbound from %s XML requests\n"
					  ,xml_section_to_string(agent->section));

	/* cleanly destroy the bindings */
	switch_thread_rwlock_destroy(agent->lock);
	switch_mutex_destroy(agent->current_client_mutex);
	switch_mutex_destroy(agent->replies_mutex);
	switch_thread_cond_destroy(agent->new_reply);
	switch_core_destroy_memory_pool(&agent->pool);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t remove_xml_client(ei_node_t *ei_node, switch_xml_binding_t *binding) {
	ei_xml_agent_t *agent;
	ei_xml_client_t *client, *prev = NULL;
	int found = 0;

	agent = (ei_xml_agent_t *)switch_xml_get_binding_user_data(binding);

	/* write-lock the agent */
	switch_thread_rwlock_wrlock(agent->lock);

	client = agent->clients;
	while (client != NULL) {
		if (client->ei_node == ei_node) {
			found = 1;
			break;
		}

		prev = client;
		client = client->next;
	}

	if (found) {
		fetch_handler_t *fetch_handler;

		if (!prev) {
			agent->clients = client->next;
		} else {
			prev->next = client->next;
		}

		/* the mutex lock is not required since we have the write lock
		 * but hey its fun and safe so do it anyway */
		switch_mutex_lock(agent->current_client_mutex);
		if (agent->current_client == client) {
			agent->current_client = agent->clients;
		}
		switch_mutex_unlock(agent->current_client_mutex);

		fetch_handler = client->fetch_handlers;
		while(fetch_handler != NULL) {
			fetch_handler_t *tmp_fetch_handler = fetch_handler;

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removed %s XML handler %s <%d.%d.%d>\n"
							  ,xml_section_to_string(agent->section)
							  ,fetch_handler->pid.node
							  ,fetch_handler->pid.creation
							  ,fetch_handler->pid.num
							  ,fetch_handler->pid.serial);

			fetch_handler = fetch_handler->next;
			switch_safe_free(tmp_fetch_handler);
		}

		switch_safe_free(client);
	}

	switch_thread_rwlock_unlock(agent->lock);

	return SWITCH_STATUS_SUCCESS;
}

static ei_xml_client_t *add_xml_client(ei_node_t *ei_node, ei_xml_agent_t *agent) {
    ei_xml_client_t *client;

	switch_malloc(client, sizeof(*client));

	client->ei_node = ei_node;
	client->fetch_handlers = NULL;
	client->next = NULL;

	if (agent->clients) {
		client->next = agent->clients;
	}

	agent->clients = client;

	return client;
}

static ei_xml_client_t *find_xml_client(ei_node_t *ei_node, ei_xml_agent_t *agent) {
    ei_xml_client_t *client;

	client = agent->clients;
	while (client != NULL) {
		if (client->ei_node == ei_node) {
			return client;
		}

		client = client->next;
	}

	return NULL;
}

static switch_status_t remove_fetch_handler(ei_node_t *ei_node, erlang_pid *from, switch_xml_binding_t *binding) {
	ei_xml_agent_t *agent;
	ei_xml_client_t *client;
	fetch_handler_t *fetch_handler, *prev = NULL;
	int found = 0;

	agent = (ei_xml_agent_t *)switch_xml_get_binding_user_data(binding);

    /* write-lock the agent */
    switch_thread_rwlock_wrlock(agent->lock);

	if (!(client = find_xml_client(ei_node, agent))) {
		switch_thread_rwlock_unlock(agent->lock);
		return SWITCH_STATUS_SUCCESS;
	}

	fetch_handler = client->fetch_handlers;
	while (fetch_handler != NULL) {
		if (ei_compare_pids(&fetch_handler->pid, from) == SWITCH_STATUS_SUCCESS) {
			found = 1;
			break;
		}

		prev = fetch_handler;
		fetch_handler = fetch_handler->next;
	}

	if (found) {
		if (!prev) {
			client->fetch_handlers = fetch_handler->next;
		} else {
			prev->next = fetch_handler->next;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removed %s XML handler %s <%d.%d.%d>\n"
						  ,xml_section_to_string(agent->section)
						  ,fetch_handler->pid.node
						  ,fetch_handler->pid.creation
						  ,fetch_handler->pid.num
						  ,fetch_handler->pid.serial);

		switch_safe_free(fetch_handler);
	}

	switch_thread_rwlock_unlock(agent->lock);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_api_command_stream(ei_node_t *ei_node, switch_stream_handle_t *stream, switch_xml_binding_t *binding) {
	ei_xml_agent_t *agent;
    ei_xml_client_t *client;

	if (!binding) {
		return SWITCH_STATUS_GENERR;
	}

	agent = (ei_xml_agent_t *)switch_xml_get_binding_user_data(binding);

	/* read-lock the agent */
	switch_thread_rwlock_rdlock(agent->lock);
	client = agent->clients;
	while (client != NULL) {
		if (client->ei_node == ei_node) {
			fetch_handler_t *fetch_handler;
			fetch_handler = client->fetch_handlers;
			while (fetch_handler != NULL) {
				stream->write_function(stream, "XML %s handler <%d.%d.%d>\n"
									   ,xml_section_to_string(agent->section)
									   ,fetch_handler->pid.creation
									   ,fetch_handler->pid.num
									   ,fetch_handler->pid.serial);
				fetch_handler = fetch_handler->next;
			}
			break;
		}

		client = client->next;
	}
	switch_thread_rwlock_unlock(agent->lock);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t bind_fetch_agents() {
	bind_fetch_agent(SWITCH_XML_SECTION_CONFIG, &globals.config_fetch_binding);
	bind_fetch_agent(SWITCH_XML_SECTION_DIRECTORY, &globals.directory_fetch_binding);
	bind_fetch_agent(SWITCH_XML_SECTION_DIALPLAN, &globals.dialplan_fetch_binding);
	bind_fetch_agent(SWITCH_XML_SECTION_CHATPLAN, &globals.chatplan_fetch_binding);
	bind_fetch_agent(SWITCH_XML_SECTION_CHANNELS, &globals.channels_fetch_binding);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t unbind_fetch_agents() {
	unbind_fetch_agent(&globals.config_fetch_binding);
	unbind_fetch_agent(&globals.directory_fetch_binding);
	unbind_fetch_agent(&globals.dialplan_fetch_binding);
	unbind_fetch_agent(&globals.chatplan_fetch_binding);
	unbind_fetch_agent(&globals.channels_fetch_binding);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t remove_xml_clients(ei_node_t *ei_node) {
	remove_xml_client(ei_node, globals.config_fetch_binding);
	remove_xml_client(ei_node, globals.directory_fetch_binding);
	remove_xml_client(ei_node, globals.dialplan_fetch_binding);
	remove_xml_client(ei_node, globals.chatplan_fetch_binding);
	remove_xml_client(ei_node, globals.channels_fetch_binding);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t add_fetch_handler(ei_node_t *ei_node, erlang_pid *from, switch_xml_binding_t *binding) {
	ei_xml_agent_t *agent;
	ei_xml_client_t *client;
	fetch_handler_t *fetch_handler;

	agent = (ei_xml_agent_t *)switch_xml_get_binding_user_data(binding);

    /* write-lock the agent */
    switch_thread_rwlock_wrlock(agent->lock);

	if (!(client = find_xml_client(ei_node, agent))) {
		client = add_xml_client(ei_node, agent);
	}

	fetch_handler = client->fetch_handlers;
    while (fetch_handler != NULL) {
		if (ei_compare_pids(&fetch_handler->pid, from) == SWITCH_STATUS_SUCCESS) {
			switch_thread_rwlock_unlock(agent->lock);
            return SWITCH_STATUS_SUCCESS;
        }
        fetch_handler = fetch_handler->next;
    }

	switch_malloc(fetch_handler, sizeof(*fetch_handler));

	memcpy(&fetch_handler->pid, from, sizeof(erlang_pid));;

	fetch_handler->next = NULL;
	if (client->fetch_handlers) {
		fetch_handler->next = client->fetch_handlers;
	}

	client->fetch_handlers = fetch_handler;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Added %s XML handler %s <%d.%d.%d>\n"
					  ,xml_section_to_string(agent->section)
					  ,fetch_handler->pid.node
					  ,fetch_handler->pid.creation
					  ,fetch_handler->pid.num
					  ,fetch_handler->pid.serial);

	switch_thread_rwlock_unlock(agent->lock);

	ei_link(ei_node, ei_self(&globals.ei_cnode), from);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t remove_fetch_handlers(ei_node_t *ei_node, erlang_pid *from) {
	remove_fetch_handler(ei_node, from, globals.config_fetch_binding);
	remove_fetch_handler(ei_node, from, globals.directory_fetch_binding);
	remove_fetch_handler(ei_node, from, globals.dialplan_fetch_binding);
	remove_fetch_handler(ei_node, from, globals.chatplan_fetch_binding);
	remove_fetch_handler(ei_node, from, globals.channels_fetch_binding);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t fetch_reply(char *uuid_str, char *xml_str, switch_xml_binding_t *binding) {
	ei_xml_agent_t *agent;
	xml_fetch_reply_t *reply;
	switch_status_t status = SWITCH_STATUS_NOTFOUND;

	agent = (ei_xml_agent_t *)switch_xml_get_binding_user_data(binding);

    switch_mutex_lock(agent->replies_mutex);
	reply = agent->replies;
	while (reply != NULL) {
		if (!strncmp(reply->uuid_str, uuid_str, SWITCH_UUID_FORMATTED_LENGTH)) {
			if (!reply->xml_str) {
				reply->xml_str = xml_str;
				switch_thread_cond_broadcast(agent->new_reply);
				status = SWITCH_STATUS_SUCCESS;
			}
			break;
		}

		reply = reply->next;
	}
    switch_mutex_unlock(agent->replies_mutex);

	return status;
}

switch_status_t handle_api_command_streams(ei_node_t *ei_node, switch_stream_handle_t *stream) {
	handle_api_command_stream(ei_node, stream, globals.config_fetch_binding);
	handle_api_command_stream(ei_node, stream, globals.directory_fetch_binding);
	handle_api_command_stream(ei_node, stream, globals.dialplan_fetch_binding);
	handle_api_command_stream(ei_node, stream, globals.chatplan_fetch_binding);
	handle_api_command_stream(ei_node, stream, globals.channels_fetch_binding);

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
