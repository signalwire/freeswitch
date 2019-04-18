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
* Based on mod_skel by
* Anthony Minessale II <anthm@freeswitch.org>
*
* Contributor(s):
*
* Daniel Bryars <danb@aeriandi.com>
* Tim Brown <tim.brown@aeriandi.com>
* Anthony Minessale II <anthm@freeswitch.org>
* William King <william.king@quentustech.com>
* Mike Jerris <mike@jerris.com>
*
* kazoo.c -- Sends FreeSWITCH events to an AMQP broker
*
*/

#include "mod_kazoo.h"

#define KZ_DEFAULT_STREAM_PRE_ALLOCATE 8192

#define KAZOO_DECLARE_GLOBAL_STRING_FUNC(fname, vname) static void __attribute__((__unused__)) fname(const char *string) { if (!string) return;\
		if (vname) {free(vname); vname = NULL;}vname = strdup(string);} static void fname(const char *string)

KAZOO_DECLARE_GLOBAL_STRING_FUNC(set_pref_ip, kazoo_globals.ip);
KAZOO_DECLARE_GLOBAL_STRING_FUNC(set_pref_ei_cookie, kazoo_globals.ei_cookie);
KAZOO_DECLARE_GLOBAL_STRING_FUNC(set_pref_ei_nodename, kazoo_globals.ei_nodename);
//KAZOO_DECLARE_GLOBAL_STRING_FUNC(set_pref_kazoo_var_prefix, kazoo_globals.kazoo_var_prefix);

static int read_cookie_from_file(char *filename) {
	int fd;
	char cookie[MAXATOMLEN + 1];
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

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set cookie from file %s: %s\n", filename, cookie);

		set_pref_ei_cookie(cookie);
		return 0;
	} else {
		/* don't error here, because we might be blindly trying to read $HOME/.erlang.cookie, and that can fail silently */
		return 1;
	}
}


switch_status_t kazoo_ei_config(switch_xml_t cfg) {
	switch_xml_t child, param;
	char* kazoo_var_prefix = NULL;
	char* profile_vars_prefix = NULL;
	char* sep_array[KZ_MAX_SEPARATE_STRINGS];
	int array_len, i;
	kazoo_globals.send_all_headers = 0;
	kazoo_globals.send_all_private_headers = 1;
	kazoo_globals.connection_timeout = 500;
	kazoo_globals.receive_timeout = 200;
	kazoo_globals.receive_msg_preallocate = 2000;
	kazoo_globals.event_stream_preallocate = KZ_DEFAULT_STREAM_PRE_ALLOCATE;
	kazoo_globals.send_msg_batch = 10;
	kazoo_globals.event_stream_framing = 2;
	kazoo_globals.port = 0;
	kazoo_globals.io_fault_tolerance = 10;
	kazoo_globals.json_encoding = ERLANG_TUPLE;

	kazoo_globals.legacy_events = SWITCH_FALSE;

	kz_set_tweak(KZ_TWEAK_INTERACTION_ID);
	kz_set_tweak(KZ_TWEAK_EXPORT_VARS);
	kz_set_tweak(KZ_TWEAK_SWITCH_URI);
	kz_set_tweak(KZ_TWEAK_REPLACES_CALL_ID);
	kz_set_tweak(KZ_TWEAK_LOOPBACK_VARS);
	kz_set_tweak(KZ_TWEAK_CALLER_ID);
	kz_set_tweak(KZ_TWEAK_TRANSFERS);
	kz_set_tweak(KZ_TWEAK_BRIDGE);
	kz_set_tweak(KZ_TWEAK_BRIDGE_REPLACES_ALEG);
	kz_set_tweak(KZ_TWEAK_BRIDGE_REPLACES_CALL_ID);
	kz_set_tweak(KZ_TWEAK_BRIDGE_VARIABLES);
	kz_set_tweak(KZ_TWEAK_RESTORE_CALLER_ID_ON_BLIND_XFER);



	if ((child = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(child, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "listen-ip")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set bind ip address: %s\n", val);
				set_pref_ip(val);
			} else if (!strcmp(var, "listen-port")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set bind port: %s\n", val);
				kazoo_globals.port = atoi(val);
			} else if (!strcmp(var, "cookie")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set cookie: %s\n", val);
				set_pref_ei_cookie(val);
			} else if (!strcmp(var, "cookie-file")) {
				if (read_cookie_from_file(val) == 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to read cookie from %s\n", val);
				}
			} else if (!strcmp(var, "nodename")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set node name: %s\n", val);
				set_pref_ei_nodename(val);
			} else if (!strcmp(var, "shortname")) {
				kazoo_globals.ei_shortname = switch_true(val);
			} else if (!strcmp(var, "kazoo-var-prefix")) {
				kazoo_var_prefix = switch_core_strdup(kazoo_globals.pool, val);
			} else if (!strcmp(var, "set-profile-vars-prefix")) {
				profile_vars_prefix = switch_core_strdup(kazoo_globals.pool, val);
			} else if (!strcmp(var, "compat-rel")) {
				if (atoi(val) >= 7)
					kazoo_globals.ei_compat_rel = atoi(val);
				else
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid compatibility release '%s' specified\n", val);
			} else if (!strcmp(var, "nat-map")) {
				kazoo_globals.nat_map = switch_true(val);
			} else if (!strcmp(var, "send-all-headers")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set send-all-headers: %s\n", val);
				kazoo_globals.send_all_headers = switch_true(val);
			} else if (!strcmp(var, "send-all-private-headers")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set send-all-private-headers: %s\n", val);
				kazoo_globals.send_all_private_headers = switch_true(val);
			} else if (!strcmp(var, "connection-timeout")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set connection-timeout: %s\n", val);
				kazoo_globals.connection_timeout = atoi(val);
			} else if (!strcmp(var, "receive-timeout")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set receive-timeout: %s\n", val);
				kazoo_globals.receive_timeout = atoi(val);
			} else if (!strcmp(var, "receive-msg-preallocate")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set receive-msg-preallocate: %s\n", val);
				kazoo_globals.receive_msg_preallocate = atoi(val);
			} else if (!strcmp(var, "event-stream-preallocate")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set event-stream-preallocate: %s\n", val);
				kazoo_globals.event_stream_preallocate = atoi(val);
			} else if (!strcmp(var, "send-msg-batch-size")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set send-msg-batch-size: %s\n", val);
				kazoo_globals.send_msg_batch = atoi(val);
			} else if (!strcmp(var, "event-stream-framing")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set event-stream-framing: %s\n", val);
				kazoo_globals.event_stream_framing = atoi(val);
			} else if (!strcmp(var, "io-fault-tolerance")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set io-fault-tolerance: %s\n", val);
				kazoo_globals.io_fault_tolerance = atoi(val);
			} else if (!strcmp(var, "num-worker-threads")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set num-worker-threads: %s\n", val);
				kazoo_globals.num_worker_threads = atoi(val);
			} else if (!strcmp(var, "json-term-encoding")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set json-term-encoding: %s\n", val);
				if(!strcmp(val, "map")) {
					kazoo_globals.json_encoding = ERLANG_MAP;
				}
			} else if (!strcmp(var, "legacy-events")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set legacy-events: %s\n", val);
				kazoo_globals.legacy_events = switch_true(val);
			}
		}
	}

	if ((child = switch_xml_child(cfg, "tweaks"))) {
		for (param = switch_xml_child(child, "tweak"); param; param = param->next) {
			kz_tweak_t tweak = KZ_TWEAK_MAX;
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if(var && val && kz_name_tweak(var, &tweak) == SWITCH_STATUS_SUCCESS) {
				if(switch_true(val)) {
					kz_set_tweak(tweak);
				} else {
					kz_clear_tweak(tweak);
				}
			}
		}
	}

	if ((child = switch_xml_child(cfg, "variables"))) {
		for (param = switch_xml_child(child, "variable"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if(var && val) {
				switch_core_set_variable(var, val);
			}
		}
	}

	if ((child = switch_xml_child(cfg, "event-filter"))) {
		switch_hash_t *filter;

		switch_core_hash_init(&filter);
		for (param = switch_xml_child(child, "header"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			switch_core_hash_insert(filter, var, "1");
		}
		kazoo_globals.event_filter = filter;
	}

	if (kazoo_globals.receive_msg_preallocate < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid receive message preallocate value, disabled\n");
		kazoo_globals.receive_msg_preallocate = 0;
	}

	if (kazoo_globals.event_stream_preallocate < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid event stream preallocate value, disabled\n");
		kazoo_globals.event_stream_preallocate = 0;
	}

	if (kazoo_globals.send_msg_batch < 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid send message batch size, reverting to default\n");
		kazoo_globals.send_msg_batch = 10;
	}

	if (kazoo_globals.io_fault_tolerance < 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid I/O fault tolerance, reverting to default\n");
		kazoo_globals.io_fault_tolerance = 10;
	}

	if (!kazoo_globals.event_filter) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Event filter not found in configuration, using default\n");
		kazoo_globals.event_filter = create_default_filter();
	}

	if (kazoo_globals.event_stream_framing < 1 || kazoo_globals.event_stream_framing > 4) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid event stream framing value, using default\n");
		kazoo_globals.event_stream_framing = 2;
	}

	if (zstr(kazoo_var_prefix)) {
		kazoo_var_prefix = switch_core_strdup(kazoo_globals.pool, "ecallmgr_;cav_");
	}

	if (zstr(profile_vars_prefix)) {
		profile_vars_prefix = switch_core_strdup(kazoo_globals.pool, "effective_;origination_");
	}

	kazoo_globals.kazoo_var_prefixes = switch_core_alloc(kazoo_globals.pool, sizeof(char*) * KZ_MAX_SEPARATE_STRINGS);
	array_len = switch_separate_string(kazoo_var_prefix, ';', sep_array, KZ_MAX_SEPARATE_STRINGS - 1);
	for(i=0; i < array_len; i++) {
		char var[100];
		sprintf(var, "variable_%s", sep_array[i]);
		kazoo_globals.kazoo_var_prefixes[i] = switch_core_strdup(kazoo_globals.pool, var);
	}

	kazoo_globals.profile_vars_prefixes = switch_core_alloc(kazoo_globals.pool, sizeof(char*) * KZ_MAX_SEPARATE_STRINGS);
	array_len = switch_separate_string(profile_vars_prefix, ';', sep_array, KZ_MAX_SEPARATE_STRINGS - 1);
	for(i=0; i < array_len; i++) {
		kazoo_globals.profile_vars_prefixes[i] = switch_core_strdup(kazoo_globals.pool, sep_array[i]);
	}

	if (!kazoo_globals.num_worker_threads) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Number of worker threads not found in configuration, using default\n");
		kazoo_globals.num_worker_threads = 10;
	}

	if (zstr(kazoo_globals.ip)) {
		set_pref_ip("0.0.0.0");
	}

	if (zstr(kazoo_globals.ei_cookie)) {
		int res;
		char *home_dir = getenv("HOME");
		char path_buf[1024];

		if (!zstr(home_dir)) {
			/* $HOME/.erlang.cookie */
			switch_snprintf(path_buf, sizeof (path_buf), "%s%s%s", home_dir, SWITCH_PATH_SEPARATOR, ".erlang.cookie");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Checking for cookie at path: %s\n", path_buf);

			res = read_cookie_from_file(path_buf);
			if (res) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No cookie or valid cookie file specified, using default cookie\n");
				set_pref_ei_cookie("ClueCon");
			}
		}
	}

	if (!kazoo_globals.ei_nodename) {
		set_pref_ei_nodename("freeswitch");
	}

	if (!kazoo_globals.nat_map) {
		kazoo_globals.nat_map = 0;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t kazoo_config_handlers(switch_xml_t cfg)
{
		switch_xml_t def = NULL;
		switch_xml_t child, param;
		char* xml = NULL;
		kazoo_config_ptr definitions = NULL, fetch_handlers = NULL, event_handlers = NULL;
		kazoo_event_profile_ptr events = NULL;

		xml = strndup(kz_default_config, kz_default_config_size);
		def = switch_xml_parse_str_dup(xml);

		kz_xml_process(def);
		kz_xml_process(cfg);

		if ((child = switch_xml_child(cfg, "variables"))) {
			for (param = switch_xml_child(child, "variable"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if(var && val) {
					switch_core_set_variable(var, val);
				}
			}
		} else if ((child = switch_xml_child(def, "variables"))) {
			for (param = switch_xml_child(child, "variable"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if(var && val) {
					switch_core_set_variable(var, val);
				}
			}
		}

		definitions = kazoo_config_definitions(cfg);
		if(definitions == NULL) {
			if(kazoo_globals.definitions == NULL) {
				definitions = kazoo_config_definitions(def);
			} else {
				definitions = kazoo_globals.definitions;
			}
		}

		fetch_handlers = kazoo_config_fetch_handlers(definitions, cfg);
		if(fetch_handlers == NULL) {
			if(kazoo_globals.fetch_handlers == NULL) {
				fetch_handlers = kazoo_config_fetch_handlers(definitions, def);
			} else {
				fetch_handlers = kazoo_globals.fetch_handlers;
			}
		}

		event_handlers = kazoo_config_event_handlers(definitions, cfg);
		if(event_handlers == NULL) {
			if(kazoo_globals.event_handlers == NULL) {
				event_handlers = kazoo_config_event_handlers(definitions, def);
			} else {
				event_handlers = kazoo_globals.event_handlers;
			}
		}

		if(event_handlers != NULL) {
			events = (kazoo_event_profile_ptr) switch_core_hash_find(event_handlers->hash, "default");
		}

		if(events == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get default handler for events\n");
			if(kazoo_globals.event_handlers != event_handlers) destroy_config(&event_handlers);
			if(kazoo_globals.fetch_handlers != fetch_handlers) destroy_config(&fetch_handlers);
			if(kazoo_globals.definitions != definitions) destroy_config(&definitions);
			switch_xml_free(def);
			switch_safe_free(xml);
			return SWITCH_STATUS_GENERR;
		}

		if(kazoo_globals.events != events) {
			bind_event_profiles(events->events);
			kazoo_globals.events = events;
		}

		if(kazoo_globals.event_handlers != event_handlers) {
			kazoo_config_ptr tmp = kazoo_globals.event_handlers;
			kazoo_globals.event_handlers = event_handlers;
			destroy_config(&tmp);
		}

		if(kazoo_globals.fetch_handlers != fetch_handlers) {
			kazoo_config_ptr tmp = kazoo_globals.fetch_handlers;
			kazoo_globals.fetch_handlers = fetch_handlers;
			rebind_fetch_profiles(fetch_handlers);
			destroy_config(&tmp);
		}

		if(kazoo_globals.definitions != definitions) {
			kazoo_config_ptr tmp = kazoo_globals.definitions;
			kazoo_globals.definitions = definitions;
			destroy_config(&tmp);
		}


		switch_xml_free(def);
		switch_safe_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t kazoo_load_config()
{
	char *cf = "kazoo.conf";
	switch_xml_t cfg, xml;
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open configuration file %s\n", cf);
		return SWITCH_STATUS_FALSE;
	} else {
		kazoo_ei_config(cfg);
		kazoo_config_handlers(cfg);
		switch_xml_free(xml);
	}

	return SWITCH_STATUS_SUCCESS;
}

void kazoo_destroy_config()
{
	destroy_config(&kazoo_globals.event_handlers);
	destroy_config(&kazoo_globals.fetch_handlers);
	destroy_config(&kazoo_globals.definitions);
}

switch_status_t kazoo_config_events(kazoo_config_ptr definitions, switch_memory_pool_t *pool, switch_xml_t cfg, kazoo_event_profile_ptr profile)
{
	switch_xml_t events, event;
	kazoo_event_ptr prv = NULL, cur = NULL;


	if ((events = switch_xml_child(cfg, "events")) != NULL) {
		for (event = switch_xml_child(events, "event"); event; event = event->next) {
			const char *var = switch_xml_attr(event, "name");
			cur = (kazoo_event_ptr) switch_core_alloc(pool, sizeof(kazoo_event_t));
			memset(cur, 0, sizeof(kazoo_event_t));
			if(prv == NULL) {
				profile->events = prv = cur;
			} else {
				prv->next = cur;
				prv = cur;
			}
			cur->profile = profile;
			cur->name = switch_core_strdup(pool, var);
			kazoo_config_filters(pool, event, &cur->filter);
			kazoo_config_fields(definitions, pool, event, &cur->fields);

		}

	}

	return SWITCH_STATUS_SUCCESS;

}


switch_status_t kazoo_config_fetch_handler(kazoo_config_ptr definitions, kazoo_config_ptr root, switch_xml_t cfg, kazoo_fetch_profile_ptr *ptr)
{
	kazoo_fetch_profile_ptr profile = NULL;
	switch_xml_t params, param;
	switch_xml_section_t fetch_section;
	int fetch_timeout = 2000000;
	switch_memory_pool_t *pool = NULL;

	char *name = (char *) switch_xml_attr_soft(cfg, "name");
	if (zstr(name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "missing name in profile\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error allocation pool for new profile : %s\n", name);
		return SWITCH_STATUS_GENERR;
	}

	profile = switch_core_alloc(pool, sizeof(kazoo_fetch_profile_t));
	profile->pool = pool;
	profile->root = root;
	profile->name = switch_core_strdup(profile->pool, name);

	fetch_section = switch_xml_parse_section_string(name);

	if ((params = switch_xml_child(cfg, "params")) != NULL) {
		for (param = switch_xml_child(params, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!var) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] param missing 'name' attribute\n", name);
				continue;
			}

			if (!val) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] param[%s] missing 'value' attribute\n", name, var);
				continue;
			}

			if (!strncmp(var, "fetch-timeout", 13)) {
				fetch_timeout = atoi(val);
			} else if (!strncmp(var, "fetch-section", 13)) {
				fetch_section = switch_xml_parse_section_string(val);
			}
		}
	}

	if (fetch_section == SWITCH_XML_SECTION_RESULT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Fetch Profile[%s] invalid fetch-section: %s\n", name, switch_xml_toxml(cfg, SWITCH_FALSE));
		goto err;
	}


	profile->fetch_timeout = fetch_timeout;
	profile->section = fetch_section;
	kazoo_config_fields(definitions, pool, cfg, &profile->fields);
	kazoo_config_loglevels(pool, cfg, &profile->logging);

	if(root) {
		if ( switch_core_hash_insert(root->hash, name, (void *) profile) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to insert new fetch profile [%s] into kazoo profile hash\n", name);
			goto err;
		}
	}

	if(ptr)
		*ptr = profile;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "fetch handler profile %s successfully configured\n", name);
	return SWITCH_STATUS_SUCCESS;

 err:
	/* Cleanup */
    if(pool) {
    	switch_core_destroy_memory_pool(&pool);
    }
	return SWITCH_STATUS_GENERR;

}

switch_status_t kazoo_config_event_handler(kazoo_config_ptr definitions, kazoo_config_ptr root, switch_xml_t cfg, kazoo_event_profile_ptr *ptr)
{
	kazoo_event_profile_ptr profile = NULL;
	switch_memory_pool_t *pool = NULL;

	char *name = (char *) switch_xml_attr_soft(cfg, "name");
	if (zstr(name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "missing name in profile\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error allocation pool for new profile : %s\n", name);
		return SWITCH_STATUS_GENERR;
	}

	profile = switch_core_alloc(pool, sizeof(kazoo_event_profile_t));
	profile->pool = pool;
	profile->root = root;
	profile->name = switch_core_strdup(profile->pool, name);

	kazoo_config_filters(pool, cfg, &profile->filter);
	kazoo_config_fields(definitions, pool, cfg, &profile->fields);
	kazoo_config_events(definitions, pool, cfg, profile);
	kazoo_config_loglevels(pool, cfg, &profile->logging);

	if(root) {
		if ( switch_core_hash_insert(root->hash, name, (void *) profile) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to insert new profile [%s] into kazoo profile hash\n", name);
			goto err;
		}
	}

	if(ptr)
		*ptr = profile;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "event handler profile %s successfully configured\n", name);
	return SWITCH_STATUS_SUCCESS;

 err:
	/* Cleanup */
    if(pool) {
    	switch_core_destroy_memory_pool(&pool);
    }
	return SWITCH_STATUS_GENERR;

}



/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
