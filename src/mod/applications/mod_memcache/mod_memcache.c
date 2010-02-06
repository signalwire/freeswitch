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
 * Rupa Schomaker <rupa@rupa.com>
 *
 * mod_memcache.c -- API for memcache
 *
 */
#include <switch.h>
#include <libmemcached/memcached.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_memcache_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_memcache_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_memcache_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_memcache, mod_memcache_load, mod_memcache_shutdown, NULL);

static char *SYNTAX = "memcache <set|replace|add> <key> <value> [expiration [flags]]\n"
	"memcache <get|getflags> <key>\n"
	"memcache <delete> <key>\n" "memcache <increment|decrement> <key> [offset [expires [flags]]]\n" "memcache <flush>\n" "memcache <status> [verbose]\n";

static struct {
	memcached_st *memcached;
	char *memcached_str;
} globals;

static switch_event_node_t *NODE = NULL;

static switch_status_t config_callback_memcached(switch_xml_config_item_t *data, const char *newvalue, switch_config_callback_type_t callback_type,
												 switch_bool_t changed)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	memcached_server_st *memcached_server = NULL;
	memcached_st *newmemcached = NULL;
	memcached_st *oldmemcached = NULL;
	const char *memcached_str = NULL;
	memcached_return rc;
	unsigned int servercount;

	if ((callback_type == CONFIG_LOAD || callback_type == CONFIG_RELOAD) && changed) {
		memcached_str = newvalue;

		/* initialize main ptr */
		memcached_server = memcached_servers_parse(memcached_str);
		if (!memcached_server) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to initialize memcached data structure (server_list).\n");
			switch_goto_status(SWITCH_STATUS_GENERR, end);
		}

		if ((servercount = memcached_server_list_count(memcached_server)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No memcache servers defined.  Server string: %s.\n", memcached_str);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%u servers defined (%s).\n", servercount, memcached_str);
		}

		/* setup memcached */
		newmemcached = memcached_create(NULL);
		if (!newmemcached) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to initialize memcached data structure (memcached_st).\n");
			switch_goto_status(SWITCH_STATUS_GENERR, end);
		}
		rc = memcached_server_push(newmemcached, memcached_server);
		if (rc != MEMCACHED_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memcache error adding server list: %s\n", memcached_strerror(newmemcached, rc));
			switch_goto_status(SWITCH_STATUS_GENERR, end);
		}
		/* memcached_behavior_set(newmemcached, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1); */

		/* swap pointers */
		oldmemcached = globals.memcached;
		globals.memcached = newmemcached;
		newmemcached = NULL;
	}

	switch_goto_status(SWITCH_STATUS_SUCCESS, end);

  end:
	if (memcached_server) {
		memcached_server_list_free(memcached_server);
	}
	if (newmemcached) {
		memcached_free(newmemcached);
	}
	if (oldmemcached) {
		memcached_free(oldmemcached);
	}
	return status;
}

static switch_xml_config_string_options_t config_opt_memcache_servers = { NULL, 0, NULL };	/* anything ok */

static switch_xml_config_item_t instructions[] = {
	/* parameter name        type                 reloadable   pointer                         default value     options structure */
	SWITCH_CONFIG_ITEM_CALLBACK("memcache-servers", SWITCH_CONFIG_STRING, CONFIG_REQUIRED | CONFIG_RELOAD, &globals.memcached_str, "",
								config_callback_memcached, &config_opt_memcache_servers,
								"host,host:port,host", "List of memcached servers."),
	SWITCH_CONFIG_ITEM_END()
};

static switch_status_t do_config(switch_bool_t reload)
{
	if (switch_xml_config_parse_module_settings("memcache.conf", reload, instructions) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}


	return SWITCH_STATUS_SUCCESS;
}

static void event_handler(switch_event_t *event)
{
	do_config(SWITCH_TRUE);
}

SWITCH_STANDARD_API(memcache_function)
{
	switch_status_t status;
	char *argv[5] = { 0 };
	int argc;
	char *subcmd = NULL;
	char *key = NULL;
	char *val = NULL;
	char *expires_str = NULL;
	char *flags_str = NULL;
	char *mydata = NULL;
	size_t string_length = 0;
	time_t expires = 0;
	uint32_t flags = 0;
	unsigned int server_count = 0;

	memcached_return rc;
	memcached_st *memcached = NULL;
	memcached_stat_st *stat = NULL;
	memcached_server_st *server_list;

	if (zstr(cmd)) {
		goto usage;
	}

	mydata = strdup(cmd);
	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc < 1) {
			goto usage;
		}

		/* clone memcached struct so we're thread safe */
		memcached = memcached_clone(NULL, globals.memcached);
		if (!memcached) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error cloning memcached object");
			stream->write_function(stream, "-ERR Error cloning memcached object\n");
		}

		subcmd = argv[0];

		if ((!strcasecmp(subcmd, "set") || !strcasecmp(subcmd, "replace") || !strcasecmp(subcmd, "add")) && argc > 2) {
			key = argv[1];
			val = argv[2];

			if (argc > 3) {
				expires_str = argv[3];
				expires = (time_t) strtol(expires_str, NULL, 10);
			}
			if (argc > 4) {
				flags_str = argv[4];
				flags = (uint32_t) strtol(flags_str, NULL, 16);
			}
			if (!strcasecmp(subcmd, "set")) {
				rc = memcached_set(memcached, key, strlen(key), val, strlen(val), expires, flags);
			} else if (!strcasecmp(subcmd, "replace")) {
				rc = memcached_replace(memcached, key, strlen(key), val, strlen(val), expires, flags);
			} else if (!strcasecmp(subcmd, "add")) {
				rc = memcached_add(memcached, key, strlen(key), val, strlen(val), expires, flags);
			}

			if (rc == MEMCACHED_SUCCESS) {
				stream->write_function(stream, "+OK\n");
			} else {
				switch_goto_status(SWITCH_STATUS_SUCCESS, mcache_error);
			}
		} else if (!strcasecmp(subcmd, "get") && argc > 1) {
			key = argv[1];

			val = memcached_get(memcached, key, strlen(key), &string_length, &flags, &rc);
			if (rc == MEMCACHED_SUCCESS) {
				stream->write_function(stream, "%.*s", (int) string_length, val);
			} else {
				switch_safe_free(val);
				switch_goto_status(SWITCH_STATUS_SUCCESS, mcache_error);
			}
			switch_safe_free(val);
		} else if (!strcasecmp(subcmd, "getflags") && argc > 1) {
			key = argv[1];

			val = memcached_get(memcached, key, strlen(key), &string_length, &flags, &rc);
			if (rc == MEMCACHED_SUCCESS) {
				stream->write_function(stream, "%x", flags);
			} else {
				switch_safe_free(val);
				switch_goto_status(SWITCH_STATUS_SUCCESS, mcache_error);
			}
			switch_safe_free(val);
		} else if ((!strcasecmp(subcmd, "increment") || !strcasecmp(subcmd, "decrement")) && argc > 1) {
			uint64_t ivalue;
			unsigned int offset = 1;
			switch_bool_t increment = SWITCH_TRUE;
			char *svalue = NULL;
			key = argv[1];

			if (argc > 2) {
				offset = (unsigned int) strtol(argv[2], NULL, 10);
				svalue = argv[2];
			} else {
				svalue = "1";
			}
			if (argc > 3) {
				expires_str = argv[3];
				expires = (time_t) strtol(expires_str, NULL, 10);
			}
			if (argc > 4) {
				flags_str = argv[4];
				flags = (uint32_t) strtol(flags_str, NULL, 16);
			}

			if (!strcasecmp(subcmd, "increment")) {
				increment = SWITCH_TRUE;
				rc = memcached_increment(memcached, key, strlen(key), offset, &ivalue);
			} else if (!strcasecmp(subcmd, "decrement")) {
				increment = SWITCH_FALSE;
				rc = memcached_decrement(memcached, key, strlen(key), offset, &ivalue);
			}
			if (rc == MEMCACHED_NOTFOUND) {
				/* ok, trying to incr / decr a value that doesn't exist yet.
				   Try to add an appropriate initial value.  If someone else beat
				   us to it, then redo incr/decr.  Otherwise we're good.
				 */
				rc = memcached_add(memcached, key, strlen(key), (increment) ? svalue : "0", strlen(svalue), expires, flags);
				if (rc == MEMCACHED_SUCCESS) {
					ivalue = (increment) ? offset : 0;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Initialized inc/dec memcache key: %s to value %d\n", key,
									  offset);
				} else {
					if (increment) {
						rc = memcached_increment(memcached, key, strlen(key), offset, &ivalue);
					} else {
						rc = memcached_decrement(memcached, key, strlen(key), offset, &ivalue);
					}
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "Someone else created incr/dec memcache key, resubmitting inc/dec request.\n");
				}
			}
			if (rc == MEMCACHED_SUCCESS) {
				stream->write_function(stream, "%ld", ivalue);
			} else {
				switch_goto_status(SWITCH_STATUS_SUCCESS, mcache_error);
			}
		} else if (!strcasecmp(subcmd, "delete") && argc > 1) {
			key = argv[1];
			if (argc > 2) {
				expires_str = argv[3];
				expires = (time_t) strtol(expires_str, NULL, 10);
			}
			rc = memcached_delete(memcached, key, strlen(key), expires);
			if (rc == MEMCACHED_SUCCESS) {
				stream->write_function(stream, "+OK\n", key);
			} else {
				switch_goto_status(SWITCH_STATUS_SUCCESS, mcache_error);
			}
		} else if (!strcasecmp(subcmd, "flush")) {
			if (argc > 1) {
				expires_str = argv[3];
				expires = (time_t) strtol(expires_str, NULL, 10);
			}
			rc = memcached_flush(memcached, expires);
			if (rc == MEMCACHED_SUCCESS) {
				stream->write_function(stream, "+OK\n", key);
			} else {
				switch_goto_status(SWITCH_STATUS_SUCCESS, mcache_error);
			}
		} else if (!strcasecmp(subcmd, "status")) {
			switch_bool_t verbose = SWITCH_FALSE;
			int x;

			if (argc > 1) {
				if (!strcasecmp(argv[1], "verbose")) {
					verbose = SWITCH_TRUE;
				}
			}

			stream->write_function(stream, "Lib version: %s\n", memcached_lib_version());
			stat = memcached_stat(memcached, NULL, &rc);
			if (rc != MEMCACHED_SUCCESS && rc != MEMCACHED_SOME_ERRORS) {
				stream->write_function(stream, "-ERR Error communicating with servers (%s)\n", memcached_strerror(memcached, rc));
			}
			server_list = memcached_server_list(memcached);
			server_count = memcached_server_count(memcached);
			stream->write_function(stream, "Servers: %d\n", server_count);
			for (x = 0; x < server_count; x++) {
				stream->write_function(stream, "  %s (%u)\n", memcached_server_name(memcached, server_list[x]),
									   memcached_server_port(memcached, server_list[x]));
				if (verbose == SWITCH_TRUE) {
					char **list;
					char **ptr;
					char *value;
					memcached_return rc2;

					list = memcached_stat_get_keys(memcached, &stat[x], &rc);
					for (ptr = list; *ptr; ptr++) {
						value = memcached_stat_get_value(memcached, &stat[x], *ptr, &rc2);
						stream->write_function(stream, "    %s: %s\n", *ptr, value);
						switch_safe_free(value);
					}
					switch_safe_free(list);
					stream->write_function(stream, "\n");
				}
			}
		} else {
			goto usage;
		}
	}
	switch_goto_status(SWITCH_STATUS_SUCCESS, done);

  mcache_error:
	if (rc != MEMCACHED_NOTFOUND) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error while running command %s: %s\n", subcmd,
						  memcached_strerror(memcached, rc));
	}
	stream->write_function(stream, "-ERR %s\n", memcached_strerror(memcached, rc));
	goto done;

  usage:
	stream->write_function(stream, "-ERR\n%s\n", SYNTAX);
	switch_goto_status(SWITCH_STATUS_SUCCESS, done);

  done:
	if (memcached) {
		memcached_quit(memcached);
		memcached_free(memcached);
	}
	switch_safe_free(mydata);
	switch_safe_free(stat);

	return status;
}

/* Macro expands to: switch_status_t mod_memcache_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_memcache_load)
{
	switch_api_interface_t *api_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));

	do_config(SWITCH_FALSE);

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event!\n");
		return SWITCH_STATUS_TERM;
	}

	SWITCH_ADD_API(api_interface, "memcache", "Memcache API", memcache_function, "syntax");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_memcache_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_memcache_shutdown)
{
	/* Cleanup dynamically allocated config settings */
	switch_xml_config_cleanup(instructions);
	if (globals.memcached) {
		memcached_free(globals.memcached);
	}

	switch_event_unbind(&NODE);

	return SWITCH_STATUS_SUCCESS;
}


/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automatically
  Macro expands to: switch_status_t mod_memcache_runtime()
SWITCH_MODULE_RUNTIME_FUNCTION(mod_memcache_runtime)
{
	while(looping)
	{
		switch_cond_next();
	}
	return SWITCH_STATUS_TERM;
}
*/

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
