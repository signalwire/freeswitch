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
 * mod_event_multicast.c -- Multicast Events
 *
 */
#include <switch.h>

#define MULTICAST_BUFFSIZE 65536

static char *MARKER = "1";

SWITCH_MODULE_LOAD_FUNCTION(mod_event_multicast_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_multicast_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_multicast_runtime);
SWITCH_MODULE_DEFINITION(mod_event_multicast, mod_event_multicast_load, mod_event_multicast_shutdown, mod_event_multicast_runtime);

static switch_memory_pool_t *module_pool = NULL;

static struct {
	char *address;
	char *bindings;
	uint32_t key_count;
	char hostname[80];
	uint64_t host_hash;
	switch_port_t port;
	switch_sockaddr_t *addr;
	switch_socket_t *udp_socket;
	switch_hash_t *event_hash;
	uint8_t event_list[SWITCH_EVENT_ALL + 1];
	int running;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_address, globals.address);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_bindings, globals.bindings);
#define MULTICAST_EVENT "multicast::event"
static switch_status_t load_config(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *cf = "event_multicast.conf";
	switch_xml_t cfg, xml, settings, param;
	char *next, *cur;
	uint32_t count = 0;
	uint8_t custom = 0;
	switch_ssize_t hlen = -1;

	gethostname(globals.hostname, sizeof(globals.hostname));
	globals.host_hash = switch_hashfunc_default(globals.hostname, &hlen);
	globals.key_count = 0;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_reserve_subclass(MULTICAST_EVENT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!\n");
		return SWITCH_STATUS_GENERR;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "address")) {
				set_global_address(val);
			} else if (!strcasecmp(var, "bindings")) {
				set_global_bindings(val);
			} else if (!strcasecmp(var, "port")) {
				globals.port = (switch_port_t) atoi(val);
			}
		}
	}

	switch_xml_free(xml);


	if (globals.bindings) {
		for (cur = globals.bindings; cur; count++) {
			switch_event_types_t type;

			if ((next = strchr(cur, ' '))) {
				*next++ = '\0';
			}

			if (custom) {
				switch_core_hash_insert(globals.event_hash, cur, MARKER);
			} else if (switch_name_event(cur, &type) == SWITCH_STATUS_SUCCESS) {
				globals.key_count++;
				if (type == SWITCH_EVENT_ALL) {
					uint32_t x = 0;
					for (x = 0; x < SWITCH_EVENT_ALL; x++) {
						globals.event_list[x] = 0;
					}
				}
				if (type <= SWITCH_EVENT_ALL) {
					globals.event_list[type] = 1;
				}
				if (type == SWITCH_EVENT_CUSTOM) {
					custom++;
				}
			}

			cur = next;
		}
	}

	if (!globals.key_count) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "No Bindings\n");
	}

	return status;

}

static void event_handler(switch_event_t *event)
{
	char *buf;
	size_t len;
	uint8_t send = 0;

	buf = (char *) malloc(MULTICAST_BUFFSIZE);
	switch_assert(buf);

	if (event->subclass && !strcmp(event->subclass->name, MULTICAST_EVENT)) {
		/* ignore our own events to avoid ping pong */
		return;
	}

	if (globals.event_list[(uint8_t) SWITCH_EVENT_ALL]) {
		send = 1;
	} else if ((globals.event_list[(uint8_t) event->event_id])) {
		if (event->event_id != SWITCH_EVENT_CUSTOM || (event->subclass && switch_core_hash_find(globals.event_hash, event->subclass->name))) {
			send = 1;
		}
	}

	if (send) {
		char *packet;

		switch (event->event_id) {
		case SWITCH_EVENT_LOG:
			goto end;
		default:
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Multicast-Sender", "%s", globals.hostname);
			if (switch_event_serialize(event, &packet, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
				memcpy(buf, &globals.host_hash, sizeof(globals.host_hash));
				switch_copy_string(buf + sizeof(globals.host_hash), packet, sizeof(buf) - sizeof(globals.host_hash));
				len = strlen(packet) + sizeof(globals.host_hash);;
				//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nEVENT\n--------------------------------\n%s\n", buf);
				switch_socket_sendto(globals.udp_socket, globals.addr, 0, buf, &len);
				switch_safe_free(packet);
			}
			break;
		}
	}
end:
	free(buf);
	return;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_event_multicast_load)
{

	memset(&globals, 0, sizeof(globals));

	module_pool = pool;

	switch_core_hash_init(&globals.event_hash, module_pool);

	if (load_config() != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Configure\n");
		return SWITCH_STATUS_TERM;
	}

	if (switch_sockaddr_info_get(&globals.addr, globals.address, SWITCH_UNSPEC, globals.port, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find address\n");
		return SWITCH_STATUS_TERM;
	}

	if (switch_socket_create(&globals.udp_socket, AF_INET, SOCK_DGRAM, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error\n");
		return SWITCH_STATUS_TERM;
	}

	if (switch_socket_opt_set(globals.udp_socket, SWITCH_SO_REUSEADDR, 1) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Option Error\n");
		switch_socket_close(globals.udp_socket);
		return SWITCH_STATUS_TERM;
	}

	if (switch_mcast_join(globals.udp_socket, globals.addr, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Multicast Error\n");
		switch_socket_close(globals.udp_socket);
		return SWITCH_STATUS_TERM;
	}

	if (switch_socket_bind(globals.udp_socket, globals.addr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bind Error\n");
		switch_socket_close(globals.udp_socket);
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);


	if (switch_event_bind((char *) modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		switch_socket_close(globals.udp_socket);
		return SWITCH_STATUS_GENERR;
	}


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_multicast_shutdown)
{
	int x = 0;

	switch_socket_shutdown(globals.udp_socket, SWITCH_SHUTDOWN_READWRITE);
	globals.running = -1;
	while (x < 100000 && globals.running) {
		x++;
		switch_yield(1000);
	}
	switch_core_hash_destroy(&globals.event_hash);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_multicast_runtime)
{
	switch_event_t *local_event;
	char *buf;
	switch_sockaddr_t *addr;

	buf = (char *) malloc(MULTICAST_BUFFSIZE);
	switch_assert(buf);
	switch_sockaddr_info_get(&addr, NULL, SWITCH_UNSPEC, 0, 0, module_pool);
	globals.running = 1;
	while (globals.running == 1) {
		char *myaddr;
		size_t len = sizeof(buf);
		memset(buf, 0, len);

		switch_sockaddr_ip_get(&myaddr, globals.addr);

		if (switch_socket_recvfrom(addr, globals.udp_socket, 0, buf, &len) == SWITCH_STATUS_SUCCESS) {
			char *packet;
			uint64_t host_hash = 0;

			memcpy(&host_hash, buf, sizeof(host_hash));
			packet = buf + sizeof(host_hash);

			if (host_hash == globals.host_hash) {
				continue;
			}
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nEVENT %d\n--------------------------------\n%s\n", (int) len, packet);
			if (switch_event_create_subclass(&local_event, SWITCH_EVENT_CUSTOM, MULTICAST_EVENT) == SWITCH_STATUS_SUCCESS) {
				char *var, *val, *term = NULL, tmpname[128];
				switch_event_add_header(local_event, SWITCH_STACK_BOTTOM, "Multicast", "yes");
				var = packet;
				while (*var) {
					if ((val = strchr(var, ':')) != 0) {
						*val++ = '\0';
						while (*val == ' ') {
							val++;
						}
						if ((term = strchr(val, '\r')) != 0 || (term = strchr(val, '\n')) != 0) {
							*term = '\0';
							while (*term == '\r' || *term == '\n') {
								term++;
							}
						}
						switch_snprintf(tmpname, sizeof(tmpname), "Orig-%s", var);
						switch_event_add_header(local_event, SWITCH_STACK_BOTTOM, tmpname, "%s", val);
						var = term + 1;
					} else {
						break;
					}
				}

				if (var && strlen(var) > 1) {
					switch_event_add_body(local_event, var);
				}

				switch_event_fire(&local_event);

			}
		}
	}

	globals.running = 0;
	free(buf);
	return SWITCH_STATUS_TERM;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
