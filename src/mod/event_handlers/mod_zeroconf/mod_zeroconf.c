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
 * mod_zeroconf.c -- Framework Demo Module
 *
 */
#include <switch.h>
#include <howl.h>

static const char modname[] = "mod_zeroconf";

static switch_memory_pool *module_pool = NULL;

static struct {
	sw_discovery discovery;
	sw_discovery_publish_id disc_id;
	switch_mutex_t *zc_lock;
} globals;



static sw_result HOWL_API my_browser(sw_discovery discovery,
									 sw_discovery_oid oid,
									 sw_discovery_browse_status status,
									 sw_uint32 interface_index,
									 sw_const_string name,
									 sw_const_string type,
									 sw_const_string domain,
									 sw_opaque_t extra) {
	//sw_discovery_resolve_id rid;

	switch (status) {
		case SW_DISCOVERY_BROWSE_INVALID:
		{
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "browse reply: Invalid\n");
		}
		break;

		case SW_DISCOVERY_BROWSE_RELEASE:
		{
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "browse reply: Release\n");
		}
		break;

		case SW_DISCOVERY_BROWSE_ADD_DOMAIN:
		{
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "browse reply: Add Domain\n");
		}
		break;

		case SW_DISCOVERY_BROWSE_ADD_DEFAULT_DOMAIN:
		{
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "browse reply: Add Default Domain\n");
		}
		break;

		case SW_DISCOVERY_BROWSE_REMOVE_DOMAIN:
		{
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "browse reply: Remove Domain\n");
		}
		break;

		case SW_DISCOVERY_BROWSE_REMOVE_SERVICE:
		{
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "browse reply: Remove Service\n");
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "remove service: 0x%x %s %s %s\n", interface_index, name, type, domain);
		}
		break;

		case SW_DISCOVERY_BROWSE_RESOLVED:
		{
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "browse reply: Resolved\n");
		}
		break;
		case SW_DISCOVERY_BROWSE_ADD_SERVICE:
		break;
	}

	return SW_OKAY;
}


static sw_result HOWL_API my_service_reply(sw_discovery	discovery,
   sw_discovery_oid	oid,
   sw_discovery_publish_status status,
   sw_opaque extra) {
	static sw_string status_text[] = {
		"Started",
		"Stopped",
		"Name Collision",
		"Invalid"
	};
	
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "publish reply: %s\n", status_text[status]);
	return SW_OKAY;
}


static void event_handler(switch_event *event)
{
	sw_text_record text_record;	
	sw_result result;

	switch (event->event_id) {
	case SWITCH_EVENT_PUBLISH:
		if (sw_text_record_init(&text_record) != SW_OKAY) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "sw_text_record_init() failed\n");
			return;
		} else {
			switch_event_header *hp;
			char *service = switch_event_get_header(event, "service");
			char *port = switch_event_get_header(event, "port");
			sw_port porti = 0;
			for (hp = event->headers; hp; hp = hp->next) {
				size_t len = strlen(hp->name) + strlen(hp->value) + 2;
				char *data = malloc(len);

				if (!data) {
					return;
				}

				snprintf(data, len, "%s=%s", hp->name, hp->value);
				if (sw_text_record_add_string(text_record, data) != SW_OKAY) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "unable to add service text: %s\n", data);
					free(data);
					return;
				}
				free(data);
			}
			if (!service) {
				service = "_freeswitch._tcp";
			}
			if (port) {
				porti = (sw_port)atoi(port);				
			}

			switch_mutex_lock(globals.zc_lock);			
			if ((result = sw_discovery_publish(globals.discovery,
											   0,
											   "freeswitch",
											   service,
											   NULL,
											   NULL,
											   porti,
											   sw_text_record_bytes(text_record),
											   sw_text_record_len(text_record),
											   my_service_reply,
											   NULL,
											   &globals.disc_id)) != SW_OKAY) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "publish failed: %d\n", result);
				sw_text_record_fina(text_record);
				switch_mutex_unlock(globals.zc_lock);
				return;
			}
			switch_mutex_unlock(globals.zc_lock);
			sw_text_record_fina(text_record);
		}
		
		break;
	case SWITCH_EVENT_UNPUBLISH:

		break;
	default:
		break;
	}
}


static switch_status load_config(void)
{
	switch_config cfg;
	switch_status status = SWITCH_STATUS_SUCCESS;
	char *var, *val;
	char *cf = "zeroconf.conf";
	int count = 0;
	sw_discovery_oid *oid;

	if (!switch_config_open_file(&cfg, cf)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "settings")) {
			if (!strcmp(var, "browse")) {
				if ((oid = switch_core_alloc(module_pool, sizeof(*oid))) != 0) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Bind browser to to %s\n", val);
					switch_mutex_lock(globals.zc_lock);	
					sw_discovery_browse(globals.discovery, 0, val, NULL, my_browser, NULL, oid);
					switch_mutex_unlock(globals.zc_lock);
					count++;
				} else {
					return SWITCH_STATUS_MEMERR;
				}
			} else if (!strcasecmp(var, "publish") && !strcasecmp(val, "yes")) {
				if (switch_event_bind((char *) modname, SWITCH_EVENT_PUBLISH, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) !=
					SWITCH_STATUS_SUCCESS) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Couldn't bind!\n");
					return SWITCH_STATUS_GENERR;
				}
				
				if (switch_event_bind((char *) modname, SWITCH_EVENT_UNPUBLISH, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) !=
					SWITCH_STATUS_SUCCESS) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Couldn't bind!\n");
					return SWITCH_STATUS_GENERR;
				}
			}
		}
	}

	switch_config_close_file(&cfg);

	return status;

}


static switch_loadable_module_interface zeroconf_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL
};

#define MY_EVENT_PUBLISH "zeroconf::broadcast"
#define MY_EVENT_UNPUBLISH "zeroconf::unbroadcast"

static int RUNNING = 0;

SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
{
	if (RUNNING == 1) {
		RUNNING = -1;
		switch_yield(100000);
	}
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	memset(&globals, 0, sizeof(globals));

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_init(&globals.zc_lock, SWITCH_MUTEX_NESTED, module_pool);

	if (sw_discovery_init(&globals.discovery) != SW_OKAY) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "sw_discovery_init() failed\n");
		return SWITCH_STATUS_TERM;
	}

	if (load_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}
	
	if (switch_event_reserve_subclass(MY_EVENT_PUBLISH) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Couldn't register subclass!");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_reserve_subclass(MY_EVENT_UNPUBLISH) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Couldn't register subclass!");
		return SWITCH_STATUS_GENERR;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &zeroconf_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MOD_DECLARE(switch_status) switch_module_runtime(void)
{

	RUNNING = 1;
	while(RUNNING == 1) {
		sw_uint32 ms;
		ms = 100;
		sw_discovery_step(globals.discovery, &ms);
		switch_yield(1000);
	}
	RUNNING = 0;
	return SWITCH_STATUS_TERM;
}
