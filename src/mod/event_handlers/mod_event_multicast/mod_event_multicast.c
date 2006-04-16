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

static const char modname[] = "mod_event_multicast";

static switch_memory_pool *module_pool = NULL;

static struct {
	char *address;
	switch_port_t port;
	switch_sockaddr_t *addr;
	switch_socket_t *udp_socket;
	int running;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_address, globals.address)

#define MULTICAST_EVENT "multicast::event"


static switch_status load_config(void)
{
	switch_config cfg;
	switch_status status = SWITCH_STATUS_SUCCESS;
	char *var, *val;
	char *cf = "event_multicast.conf";

	if (!switch_config_open_file(&cfg, cf)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_reserve_subclass(MULTICAST_EVENT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!");
		return SWITCH_STATUS_GENERR;
	}

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "settings")) {
			if (!strcasecmp(var, "address")) {
				set_global_address(val);
			} else if (!strcasecmp(var, "port")) {
				globals.port = (switch_port_t)atoi(val);
			}
		}
	}

	switch_config_close_file(&cfg);

	return status;

}

static void event_handler(switch_event *event)
{
	char buf[1024];
	size_t len;

	if (event->subclass && !strcmp(event->subclass->name, MULTICAST_EVENT)) {
		/* ignore our own events to avoid ping pong*/
		return;
	}


	switch (event->event_id) {
	case SWITCH_EVENT_LOG:
		return;
	default:
		switch_event_serialize(event, buf, sizeof(buf), NULL);
		len = strlen(buf);
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nEVENT\n--------------------------------\n%s\n", buf);
		switch_socket_sendto(globals.udp_socket, globals.addr, 0, buf, &len);
		break;
	}
}


static switch_loadable_module_interface event_test_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL
};


SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{
	memset(&globals, 0, sizeof(globals));
	
	if (load_config() != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Configure\n");
		return SWITCH_STATUS_TERM;
	}

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
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
	*interface = &event_test_module_interface;

	if (switch_event_bind((char *) modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) !=
		SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		switch_socket_close(globals.udp_socket);
		return SWITCH_STATUS_GENERR;
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
{
	int x = 0;

	switch_socket_shutdown(globals.udp_socket, APR_SHUTDOWN_READWRITE);
	globals.running = -1;
	while(x < 100000 && globals.running) {
		x++;
		switch_yield(1000);
	}
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MOD_DECLARE(switch_status) switch_module_runtime(void)
{
	switch_event *local_event;
	char buf[1024];
	
	globals.running = 1;
	while(globals.running == 1) {
		switch_sockaddr_t addr = {0};
		size_t len = sizeof(buf);
		memset(buf, 0, len);
		if (switch_socket_recvfrom(&addr, globals.udp_socket, 0, buf, &len) == SWITCH_STATUS_SUCCESS) {
			if (switch_event_create_subclass(&local_event, SWITCH_EVENT_CUSTOM, MULTICAST_EVENT) == SWITCH_STATUS_SUCCESS) {
				char *var, *val, *term = NULL;
				switch_event_add_header(local_event, SWITCH_STACK_BOTTOM, "Multicast", "yes");
				var = buf;
				while(*var) {
					if ((val = strchr(var, ':')) != 0) {
						char varname[512];
						*val++ = '\0';
						while(*val == ' ') {
							val++;
						}
						if ((term = strchr(val, '\r')) != 0 || (term=strchr(val, '\n')) != 0) {
							*term = '\0';
							while(*term == '\r' || *term == '\n') {
								term++;
							}
						}
						snprintf(varname, sizeof(varname), "Remote-%s", var);
						switch_event_add_header(local_event, SWITCH_STACK_BOTTOM, varname, val);
						var = term + 1;
					} else {
						break;
					}
				} 

				switch_event_fire(&local_event);
			}
		}

	}
		
	globals.running = 0;
	return SWITCH_STATUS_TERM;
}



