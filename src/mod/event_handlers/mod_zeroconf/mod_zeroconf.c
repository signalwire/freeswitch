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
#ifdef _MSC_VER
#pragma warning(disable:4142)
#endif
#include <howl.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_zeroconf_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_zeroconf_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_zeroconf_runtime);
SWITCH_MODULE_DEFINITION(mod_zeroconf, mod_zeroconf_load, mod_zeroconf_shutdown, mod_zeroconf_runtime);

static switch_memory_pool_t *module_pool = NULL;

static struct {
	sw_discovery discovery;
	sw_discovery_publish_id disc_id;
	switch_mutex_t *zc_lock;
	switch_event_node_t *publish_node;
	switch_event_node_t *unpublish_node;
} globals;



static sw_result HOWL_API my_browser(sw_discovery discovery,
									 sw_discovery_oid oid,
									 sw_discovery_browse_status status,
									 sw_uint32 interface_index, sw_const_string name, sw_const_string type, sw_const_string domain, sw_opaque_t extra)
{
	//sw_discovery_resolve_id rid;

	switch (status) {
	case SW_DISCOVERY_BROWSE_INVALID:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "browse reply: Invalid\n");
		}
		break;

	case SW_DISCOVERY_BROWSE_RELEASE:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "browse reply: Release\n");
		}
		break;

	case SW_DISCOVERY_BROWSE_ADD_DOMAIN:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "browse reply: Add Domain\n");
		}
		break;

	case SW_DISCOVERY_BROWSE_ADD_DEFAULT_DOMAIN:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "browse reply: Add Default Domain\n");
		}
		break;

	case SW_DISCOVERY_BROWSE_REMOVE_DOMAIN:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "browse reply: Remove Domain\n");
		}
		break;

	case SW_DISCOVERY_BROWSE_REMOVE_SERVICE:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "browse reply: Remove Service\n");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "remove service: 0x%x %s %s %s\n", interface_index, name, type, domain);
		}
		break;

	case SW_DISCOVERY_BROWSE_RESOLVED:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "browse reply: Resolved\n");
		}
		break;
	case SW_DISCOVERY_BROWSE_ADD_SERVICE:
		break;
	}

	return SW_OKAY;
}


static sw_result HOWL_API my_service_reply(sw_discovery discovery, sw_discovery_oid oid, sw_discovery_publish_status status, sw_opaque extra)
{
	static sw_string status_text[] = {
		"Started",
		"Stopped",
		"Name Collision",
		"Invalid"
	};

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "publish reply: %s\n", status_text[status]);
	return SW_OKAY;
}


static void event_handler(switch_event_t *event)
{
	sw_text_record text_record;
	sw_result result;

	switch (event->event_id) {
	case SWITCH_EVENT_PUBLISH:
		if (sw_text_record_init(&text_record) != SW_OKAY) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sw_text_record_init() failed\n");
			return;
		} else {
			switch_event_header_t *hp;
			char *service = switch_event_get_header(event, "service");
			char *port = switch_event_get_header(event, "port");
			sw_port porti = 0;
			for (hp = event->headers; hp; hp = hp->next) {
				size_t len = strlen(hp->name) + strlen(hp->value) + 2;
				char *data = malloc(len);

				if (!data) {
					return;
				}

				switch_snprintf(data, len, "%s=%s", hp->name, hp->value);
				if (sw_text_record_add_string(text_record, data) != SW_OKAY) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unable to add service text: %s\n", data);
					free(data);
					return;
				}
				free(data);
			}
			if (!service) {
				service = "_freeswitch._tcp";
			}
			if (port) {
				porti = (sw_port) atoi(port);
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
											   sw_text_record_len(text_record), my_service_reply, NULL, &globals.disc_id)) != SW_OKAY) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "publish failed: %u\n", result);
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


static switch_status_t load_config(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *cf = "zeroconf.conf";
	int count = 0;
	sw_discovery_oid *oid;
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "browse")) {
				if ((oid = switch_core_alloc(module_pool, sizeof(*oid))) != 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Bind browser to to %s\n", val);
					switch_mutex_lock(globals.zc_lock);
					sw_discovery_browse(globals.discovery, 0, val, NULL, my_browser, NULL, oid);
					switch_mutex_unlock(globals.zc_lock);
					count++;
				} else {
					return SWITCH_STATUS_MEMERR;
				}
			} else if (!strcasecmp(var, "publish") && !strcasecmp(val, "yes")) {
				if (switch_event_bind_removable(modname, SWITCH_EVENT_PUBLISH, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.publish_node) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
					return SWITCH_STATUS_GENERR;
				}

				if (switch_event_bind_removable(modname, SWITCH_EVENT_UNPUBLISH, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.unpublish_node) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
					return SWITCH_STATUS_GENERR;
				}
			}
		}
	}

	switch_xml_free(xml);

	return status;

}

#define MY_EVENT_PUBLISH "zeroconf::broadcast"
#define MY_EVENT_UNPUBLISH "zeroconf::unbroadcast"

static int RUNNING = 0;

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_zeroconf_shutdown)
{
	if (RUNNING == 1) {
		RUNNING = -1;
		switch_yield(100000);
	}

	switch_event_unbind(&globals.publish_node);
	switch_event_unbind(&globals.unpublish_node);
	switch_event_free_subclass(MY_EVENT_PUBLISH);
	switch_event_free_subclass(MY_EVENT_UNPUBLISH);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_zeroconf_load)
{

	memset(&globals, 0, sizeof(globals));

	module_pool = pool;

	switch_mutex_init(&globals.zc_lock, SWITCH_MUTEX_NESTED, module_pool);

	if (sw_discovery_init(&globals.discovery) != SW_OKAY) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sw_discovery_init() failed\n");
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_reserve_subclass(MY_EVENT_PUBLISH) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_reserve_subclass(MY_EVENT_UNPUBLISH) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (load_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_RUNTIME_FUNCTION(mod_zeroconf_runtime)
{

	RUNNING = 1;
	while (RUNNING == 1) {
		sw_uint32 ms;
		ms = 100;
		sw_discovery_step(globals.discovery, &ms);
		switch_yield(1000);
	}
	RUNNING = 0;
	return SWITCH_STATUS_TERM;
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
