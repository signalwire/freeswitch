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
 * Rupa Schomaker <rupa@rupa.com>
 *
 * mod_memcache.c -- API for memcache
 *
 */
#include <switch.h>
#include <switch_private.h>
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

#define BYTES_PER_SAMPLE 2

struct memcache_context {
	memcached_st *memcached;
	char *path;
	int ok;
	size_t offset;
	size_t remaining;
	void *data;
};
typedef struct memcache_context memcache_context_t;

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

#if HAVE_MEMCACHED_SERVER_NAME
#if HAVE_MEMCACHED_INSTANCE_STP
#define MCD_SERVER memcached_instance_st*
#else
#define MCD_SERVER memcached_server_instance_st
#endif
#define MCD_SERVER_NAME memcached_server_name(server)
#define MCD_SERVER_PORT memcached_server_port(server)
#else
#define MCD_SERVER memcached_server_st*
#define MCD_SERVER_NAME memcached_server_name(ptr, *server)
#define MCD_SERVER_PORT memcached_server_port(ptr, *server)
#endif

struct stream_server
{
	switch_stream_handle_t *stream;
	MCD_SERVER server;
};

static memcached_return server_show(const memcached_st *ptr, const MCD_SERVER server, void *context)
{
	struct stream_server *ss = (struct stream_server*) context;
	ss->stream->write_function(ss->stream, "  %s (%u)\n", MCD_SERVER_NAME, MCD_SERVER_PORT);
	return MEMCACHED_SUCCESS;
}

static memcached_return server_stat(const MCD_SERVER server, const char *key, size_t key_length, const char *value, size_t value_length, void *context)
{
	struct stream_server *ss = (struct stream_server*) context;

	if (ss->server != server) {
		server_show(NULL, server, context);
		ss->server = (MCD_SERVER) server;
	}

	ss->stream->write_function(ss->stream, "    %s: %s\n", key, value);
	return MEMCACHED_SUCCESS;
}

#if !HAVE_MEMCACHED_STAT_EXECUTE
static memcached_return server_stats(memcached_st *ptr, MCD_SERVER server, void *context)
{
	char **keys, **key, *value;
	memcached_stat_st stat;
	memcached_return rc;

	struct stream_server *ss = (struct stream_server*) context;

	rc = memcached_stat_servername(&stat, NULL, MCD_SERVER_NAME, MCD_SERVER_PORT);
	if (rc != MEMCACHED_SUCCESS) goto mcache_error;

	keys = memcached_stat_get_keys(ptr, &stat, &rc);
	if (rc != MEMCACHED_SUCCESS) goto mcache_error;

	for (key = keys; *key; key++) {
		value = memcached_stat_get_value(ptr, &stat, *key, &rc);

		if (rc == MEMCACHED_SUCCESS && value) {
			server_stat(server, *key, 0, value, 0, context);
			switch_safe_free(value);
		} else {
			server_stat(server, *key, 0, "N/A", 0, context);
		}
	}

	switch_safe_free(keys);
	return MEMCACHED_SUCCESS;

mcache_error:
	ss->stream->write_function(ss->stream, "-ERR %s\n", memcached_strerror(ptr, rc));
	return MEMCACHED_SUCCESS;
}
#endif

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

	memcached_return rc = 0;
	memcached_st *memcached = NULL;
	memcached_stat_st *stat = NULL;

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
				stream->raw_write_function(stream, (uint8_t*)val, (int)string_length);
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
				expires_str = argv[2];
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
				expires_str = argv[1];
				expires = (time_t) strtol(expires_str, NULL, 10);
			}
			rc = memcached_flush(memcached, expires);
			if (rc == MEMCACHED_SUCCESS) {
				stream->write_function(stream, "+OK\n", key);
			} else {
				switch_goto_status(SWITCH_STATUS_SUCCESS, mcache_error);
			}
		} else if (!strcasecmp(subcmd, "status")) {
			struct stream_server ss;
			ss.stream = stream;
			ss.server = NULL;

			stream->write_function(stream, "Lib version: %s\n", memcached_lib_version());

			server_count = memcached_server_count(memcached);
			stream->write_function(stream, "Servers: %d\n", server_count);

			if (argc > 1 && !strcasecmp(argv[1], "verbose")) {
#if HAVE_MEMCACHED_STAT_EXECUTE
				rc = memcached_stat_execute(memcached, NULL, server_stat, (void*) &ss);
#else
				memcached_server_function callbacks[] = { (memcached_server_function) server_stats };
				rc = memcached_server_cursor(memcached, callbacks, (void*) &ss, 1);
#endif
			} else {
				memcached_server_function callbacks[] = { (memcached_server_function) server_show };
				rc = memcached_server_cursor(memcached, callbacks, (void*) &ss, 1);
			}

			if (rc != MEMCACHED_SUCCESS) {
				switch_goto_status(SWITCH_STATUS_SUCCESS, mcache_error);
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


static switch_status_t memcache_file_open(switch_file_handle_t *handle, const char *path)
{
	memcache_context_t *context;
	size_t string_length = 0;
	uint32_t flags = 0;
	memcached_return rc;

	if (handle->offset_pos) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Offset unsupported.\n");
		return SWITCH_STATUS_GENERR;
	}

	context = switch_core_alloc(handle->memory_pool, sizeof(*context));

	/* Clone memcached struct so we're thread safe */
	context->memcached = memcached_clone(NULL, globals.memcached);
	if (!context->memcached) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error cloning memcached object\n");
		return SWITCH_STATUS_FALSE;
	}

	/* All of the actual data is read into the buffer here, the memcache_file_read
	 * function just iterates over the buffer
	 */
	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		handle->private_info = context;

		context->data = memcached_get(context->memcached, path, strlen(path), &string_length, &flags, &rc);

		if (context->data && rc == MEMCACHED_SUCCESS) {
			context->ok = 1;
			context->offset = 0;
			context->remaining = string_length / BYTES_PER_SAMPLE;
			return SWITCH_STATUS_SUCCESS;
		} else {
			memcached_free(context->memcached);
			context->memcached = NULL;
			switch_safe_free(context->data);
			context->data = NULL;
			context->ok = 0;
			return SWITCH_STATUS_FALSE;
		}
	} else if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		if (switch_test_flag(handle, SWITCH_FILE_WRITE_OVER)) {
			memcached_free(context->memcached);
			context->memcached = NULL;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unsupported file mode.\n");
			return SWITCH_STATUS_GENERR;
		}

		context->path = switch_core_strdup(handle->memory_pool, path);

		if (! switch_test_flag(handle, SWITCH_FILE_WRITE_APPEND)) {
			/* If not appending, we need to write an empty string to the key so that
			 * memcache_file_write appends do the right thing
			*/
			rc = memcached_set(context->memcached, context->path, strlen(context->path), "", 0, 0, 0);
			if (rc != MEMCACHED_SUCCESS) {
				memcached_free(context->memcached);
				context->memcached = NULL;
				return SWITCH_STATUS_GENERR;
			}
		}

		context->ok = 1;
		handle->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File opened with unknown flags!\n");
	return SWITCH_STATUS_GENERR;
}

static switch_status_t memcache_file_close(switch_file_handle_t *handle)
{
	memcache_context_t *memcache_context = handle->private_info;

	if (memcache_context->data) {
		switch_safe_free(memcache_context->data);
		memcache_context->data = NULL;
	}
	if (memcache_context->memcached) {
		memcached_free(memcache_context->memcached);
		memcache_context->memcached = NULL;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t memcache_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	memcache_context_t *context= handle->private_info;

	if (context->ok) {
		if (context->remaining <= 0) {
			return SWITCH_STATUS_FALSE;
		}

		if (*len > (size_t) context->remaining) {
			*len = context->remaining;
		}

		memcpy(data, (uint8_t *)context->data + (context->offset * BYTES_PER_SAMPLE), *len * BYTES_PER_SAMPLE);

		context->offset += (int32_t)*len;
		context->remaining -= (int32_t)*len;

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t memcache_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	memcache_context_t *context = handle->private_info;
	memcached_return rc;

	/* Append this chunk */
	if (context->ok) {
		rc = memcached_append(context->memcached, context->path, strlen(context->path), data, *len, 0, 0);

		if (rc != MEMCACHED_SUCCESS) {
			context->ok = 0;
			return SWITCH_STATUS_FALSE;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

/* Macro expands to: switch_status_t mod_memcache_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_memcache_load)
{
	switch_api_interface_t *api_interface;
	switch_file_interface_t *file_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));

	do_config(SWITCH_FALSE);

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event!\n");
		return SWITCH_STATUS_TERM;
	}

	SWITCH_ADD_API(api_interface, "memcache", "Memcache API", memcache_function, "syntax");

	/* file interface */
	supported_formats[0] = "memcache";
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens         = supported_formats;
	file_interface->file_open      = memcache_file_open;
	file_interface->file_close     = memcache_file_close;
	file_interface->file_read      = memcache_file_read;
	file_interface->file_write     = memcache_file_write;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_NOUNLOAD;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
