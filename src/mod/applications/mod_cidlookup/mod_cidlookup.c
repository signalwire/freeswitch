/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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
 * mod_cidlookup.c -- API for querying cid->name services
 *
 */
 
#include <switch.h>
#include <curl/curl.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cidlookup_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_cidlookup_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_cidlookup_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_cidlookup, mod_cidlookup_load, mod_cidlookup_shutdown, NULL);

static char *SYNTAX = "cidlookup status|number [skipurl]";

static struct {
	char *url;

	switch_bool_t cache;
	int cache_expire;
	
	char *odbc_dsn;
	char *sql;

	switch_mutex_t *db_mutex;
	switch_memory_pool_t *pool;
	switch_odbc_handle_t *master_odbc;
} globals;

struct http_data {
	switch_stream_handle_t stream;
	switch_size_t bytes;
	switch_size_t max_bytes;
	int err;
};


struct callback_obj {
	switch_memory_pool_t *pool;
	char *name;
};
typedef struct callback_obj callback_t;


static switch_event_node_t *reload_xml_event = NULL;

static switch_status_t config_callback_dsn(switch_xml_config_item_t *data, const char *newvalue, switch_config_callback_type_t callback_type, switch_bool_t changed)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *odbc_user = NULL;
	char *odbc_pass = NULL;
	char *odbc_dsn = NULL;
	
	switch_odbc_handle_t *odbc = NULL;

	if (!switch_odbc_available()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC is not compiled in.  Do not configure odbc-dsn parameter!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (globals.db_mutex) {
		switch_mutex_lock(globals.db_mutex);
	}
	
	if ((callback_type == CONFIG_LOAD || callback_type == CONFIG_RELOAD) && changed) {
		odbc_dsn = strdup(newvalue);

		if(switch_strlen_zero(odbc_dsn)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No local database defined.\n");
		} else {
			if ((odbc_user = strchr(odbc_dsn, ':'))) {
				*odbc_user++ = '\0';
				if ((odbc_pass = strchr(odbc_user, ':'))) {
					*odbc_pass++ = '\0';
				}
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connecting to dsn: %s, %s, %s.\n", globals.odbc_dsn, odbc_user, odbc_pass);
			
			/* setup dsn */
			
			if (!(odbc = switch_odbc_handle_new(odbc_dsn, odbc_user, odbc_pass))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
				switch_goto_status(SWITCH_STATUS_FALSE, done);
			}
			if (switch_odbc_handle_connect(odbc) != SWITCH_ODBC_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
				switch_goto_status(SWITCH_STATUS_FALSE, done);
			}
		}
		
		/* ok, we have a new connection, tear down old one */
		if (globals.master_odbc) {
			switch_odbc_handle_disconnect(globals.master_odbc);
			switch_odbc_handle_destroy(&globals.master_odbc);
		}
		
		/* and swap in new connection */
		globals.master_odbc = odbc;
	}

	switch_goto_status(SWITCH_STATUS_SUCCESS, done);
	
done:
	if (globals.db_mutex) {
		switch_mutex_unlock(globals.db_mutex);
	}
	switch_safe_free(odbc_dsn);
	return status;
}

static switch_xml_config_string_options_t config_opt_dsn = {NULL, 0, NULL}; /* anything is ok here */
static switch_xml_config_item_t instructions[] = {
	/* parameter name        type                 reloadable   pointer                         default value     options structure */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("url", CONFIG_RELOAD, &globals.url, NULL, "http://server.example.com/app?number=${caller_id_number}", "URL for the CID lookup service"),
	SWITCH_CONFIG_ITEM("cache", SWITCH_CONFIG_BOOL, CONFIG_RELOAD, &globals.cache, SWITCH_FALSE, NULL, "true|false", "whether to cache via cidlookup"),
	SWITCH_CONFIG_ITEM("cache-expire", SWITCH_CONFIG_INT, CONFIG_RELOAD, &globals.cache_expire, (void *)300, NULL, "expire", "seconds to preserve num->name cache"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("sql", CONFIG_RELOAD, &globals.sql, "", "sql whre number=${caller_id_number}", "SQL to run if overriding CID"),
	SWITCH_CONFIG_ITEM_CALLBACK("odbc-dsn", SWITCH_CONFIG_STRING, CONFIG_RELOAD, &globals.odbc_dsn, "", config_callback_dsn, &config_opt_dsn,
		"db:user:passwd", "Database to use."),
	SWITCH_CONFIG_ITEM_END()
};

static switch_status_t do_config(switch_bool_t reload)
{
	if (switch_xml_config_parse_module_settings("cidlookup.conf", reload, instructions) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}
	
	return SWITCH_STATUS_SUCCESS;
}

static void event_handler(switch_event_t *event)
{
	do_config(SWITCH_TRUE);
}

static switch_bool_t cidlookup_execute_sql_callback(char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t retval = SWITCH_FALSE;
	
	switch_mutex_lock(globals.db_mutex);
	if (globals.odbc_dsn) {
		if (switch_odbc_handle_callback_exec(globals.master_odbc, sql, callback, pdata)
				== SWITCH_ODBC_FAIL) {
			retval = SWITCH_FALSE;
		} else {
			retval = SWITCH_TRUE;
		}
	}
	switch_mutex_unlock(globals.db_mutex);
	return retval;
}

static int cidlookup_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	callback_t *cbt = (callback_t *) pArg;
	switch_memory_pool_t *pool = cbt->pool;
	
	if (argc < 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							"Unexpected number of columns returned for SQL.  Returned column count: %d. ",
							argc);
		return SWITCH_STATUS_GENERR;
	}
	cbt->name = switch_core_strdup(pool, switch_str_nil(argv[0]));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Name: %s\n", cbt->name);
	
	return SWITCH_STATUS_SUCCESS;
}

/* make a new string with digits only */
static char *string_digitsonly(switch_memory_pool_t *pool, const char *str) 
{
	char *p, *np, *newstr;
	size_t len;

	p = (char *)str;

	len = strlen(str);
	newstr = switch_core_alloc(pool, len+1);
	np = newstr;
	
	while(*p) {
		if (switch_isdigit(*p)) {
			*np = *p;
			np++;
		}
		p++;
	}
	*np = '\0';

	return newstr;
}

static char *check_cache(switch_memory_pool_t *pool, const char *number) {
	char *cmd;
	char *name = NULL;
	switch_stream_handle_t stream = { 0 };
	
	SWITCH_STANDARD_STREAM(stream);
	cmd = switch_core_sprintf(pool, "get fs:cidlookup:%s", number);
	
	if (switch_api_execute("memcache", cmd, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		if (strncmp("-ERR", stream.data, 4)) {
			name = switch_core_strdup(pool, stream.data);
		} else {
			name = NULL;
		}
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "memcache: k:'%s', v:'%s'\n", cmd, (name) ? name : "(null)");
	switch_safe_free(stream.data);
	return name;
}

switch_bool_t set_cache(switch_memory_pool_t *pool, const char *number, const char *cid) {
	char *cmd;
	switch_bool_t success = SWITCH_TRUE;
	switch_stream_handle_t stream = { 0 };
	
	SWITCH_STANDARD_STREAM(stream);
	cmd = switch_core_sprintf(pool, "set fs:cidlookup:%s '%s' %d", number, cid, globals.cache_expire);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "memcache: %s\n", cmd);
	
	if (switch_api_execute("memcache", cmd, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		if (strncmp("-ERR", stream.data, 4)) {
			success = SWITCH_TRUE;
		} else {
			success = SWITCH_FALSE;
		}
	}
	
	switch_safe_free(stream.data);
	return success;
}

static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	struct http_data *http_data = data;

	http_data->bytes += realsize;

	if (http_data->bytes > http_data->max_bytes) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Oversized file detected [%d bytes]\n", (int)http_data->bytes);
		http_data->err = 1;
		return 0;
	}

	http_data->stream.write_function(
		&http_data->stream, "%.*s",  realsize, ptr);
	return realsize;
}

static char *do_lookup_url(switch_memory_pool_t *pool, switch_event_t *event, const char *num) {
	char *name = NULL;
	char *newurl = NULL;
	
	CURL *curl_handle = NULL;
	long httpRes = 0;
	char hostname[256] = "";

	struct http_data http_data;
	
	memset(&http_data, 0, sizeof(http_data));
	
	http_data.max_bytes = 1024;
	SWITCH_STANDARD_STREAM(http_data.stream);

	gethostname(hostname, sizeof(hostname));
	
	newurl = switch_event_expand_headers(event, globals.url);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "url: %s\n", newurl);
	curl_handle = curl_easy_init();
	
	if (!strncasecmp(newurl, "https", 5)) {
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}
	curl_easy_setopt(curl_handle, CURLOPT_POST, SWITCH_FALSE);
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
	curl_easy_setopt(curl_handle, CURLOPT_URL, newurl);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &http_data);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-cidlookup/1.0");

	curl_easy_perform(curl_handle);
	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	curl_easy_cleanup(curl_handle);
	
	if (	http_data.stream.data &&
			!switch_strlen_zero((char *)http_data.stream.data) &&
			strcmp(" ", http_data.stream.data) ) {
		
		name = switch_core_strdup(pool, http_data.stream.data);
	}
	
	if (newurl != globals.url) {
		switch_safe_free(newurl);
	}
	switch_safe_free(http_data.stream.data);
	return name;
}

static char *do_db_lookup(switch_memory_pool_t *pool, switch_event_t *event, const char *num) {
	char *name = NULL;
	char *newsql = NULL;
	callback_t cbt = { 0 };
	cbt.pool = pool;
	
	if (globals.master_odbc) {
		newsql = switch_event_expand_headers(event, globals.sql);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL: %s\n", newsql);
		if (cidlookup_execute_sql_callback(newsql, cidlookup_callback, &cbt)) {
			name = cbt.name;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to lookup cid\n");
		}
	}
	if (newsql != globals.sql) {
		switch_safe_free(newsql);
	}
	return name;
}

static char *do_lookup(switch_memory_pool_t *pool, switch_event_t *event, const char *num, switch_bool_t skipurl) {
	char *number = NULL;
	char *name = NULL;
	
	number = string_digitsonly(pool, num);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "caller_id_number", number);

	/* database always wins */
	if (switch_odbc_available() && globals.master_odbc && globals.sql) {
		name = do_db_lookup(pool, event, number);
	}

	if (!name && globals.url) {
		if (globals.cache) {
			name = check_cache(pool, number);
		}
		if (!skipurl && !name) {
			name = do_lookup_url(pool, event, number);
			if (globals.cache && name) {
				set_cache(pool, number, name);
			}
		}
	}
	return name;
}

SWITCH_STANDARD_APP(cidlookup_app_function)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	
	char *argv[3] = { 0 };
	int argc;
	char *mydata = NULL;

	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *profile = switch_channel_get_caller_profile(channel);
	char *name = NULL;
	const char *number = NULL;
	switch_bool_t skipurl = SWITCH_FALSE;
	
	if (session) {
		pool = switch_core_session_get_pool(session);
	} else {
		switch_core_new_memory_pool(&pool);
	}
	switch_event_create(&event, SWITCH_EVENT_MESSAGE);

	if (!(mydata = switch_core_session_strdup(session, data))) {
		return;
	}
	
	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc > 0) {
			number = switch_core_session_strdup(session, argv[0]);
		}
		if (argc > 1) {
			skipurl = SWITCH_TRUE;
		}
	}
	
	if (!number && profile) {
		number = switch_caller_get_field_by_name(profile, "caller_id_number");
	}
	
	if (number) {
		name = do_lookup(pool, event, number, skipurl);
	}
	
	if (name && channel) {
		switch_channel_set_variable(channel, "original_caller_id_name", switch_core_strdup(pool, profile->caller_id_name));
		profile->caller_id_name = switch_core_strdup(profile->pool, name);;
	}
	
	switch_goto_status(SWITCH_STATUS_SUCCESS, done);
	
done:
	switch_event_destroy(&event);
	if (!session) {
		switch_core_destroy_memory_pool(&pool);
	}
}
SWITCH_STANDARD_API(cidlookup_function)
{
	switch_status_t status;
	char *argv[3] = { 0 };
	int argc;
	char *name = NULL;
	char *mydata = NULL;

	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;
	switch_bool_t skipurl = SWITCH_FALSE;
	
	if (switch_strlen_zero(cmd)) {
		switch_goto_status(SWITCH_STATUS_SUCCESS, usage);
	}

	if (session) {
		pool = switch_core_session_get_pool(session);
	} else {
		switch_core_new_memory_pool(&pool);
	}
	switch_event_create(&event, SWITCH_EVENT_MESSAGE);

	mydata = strdup(cmd);
	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc < 1) {
			switch_goto_status(SWITCH_STATUS_SUCCESS, usage);
		}
		
		if (!strcmp("status", argv[0])) {
			stream->write_function(stream, "+OK\n url: %s\n cache: %s\n cache-expire: %d\n", 
									globals.url,
									(globals.cache) ? "true" : "false",
									globals.cache_expire);
									
			stream->write_function(stream, " odbc-dsn: %s\n sql: %s\n", 
									globals.odbc_dsn,
									globals.sql);
			stream->write_function(stream, " ODBC Compiled: %s\n", switch_odbc_available() ? "true" : "false");

			switch_goto_status(SWITCH_STATUS_SUCCESS, done);
		}
		if (argc > 1 && !strcmp("skipurl", argv[1])) {
			skipurl = SWITCH_TRUE;
		}

		name = do_lookup(pool, event, argv[0], skipurl);
		if (name) {
			stream->write_function(stream, name);
		} else {
			stream->write_function(stream,"-ERR");
		}
	}
	switch_goto_status(SWITCH_STATUS_SUCCESS, done);

usage:
	stream->write_function(stream, "-ERR\n%s\n", SYNTAX);
	switch_goto_status(status, done);
	
done: 
	switch_safe_free(mydata);
	switch_event_destroy(&event);
	if (!session) {
		switch_core_destroy_memory_pool(&pool);
	}
	return status;
}

/* Macro expands to: switch_status_t mod_cidlookup_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_cidlookup_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));
	
	globals.pool = pool;

	if (switch_odbc_available() && !globals.db_mutex) {
		if (switch_mutex_init(&globals.db_mutex, SWITCH_MUTEX_UNNESTED, globals.pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to initialize db_mutex\n");
		}
	}

	do_config(SWITCH_FALSE);
	
	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &reload_xml_event) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event!\n");
		return SWITCH_STATUS_TERM;
	}
	
	SWITCH_ADD_API(api_interface, "cidlookup", "cidlookup API", cidlookup_function, SYNTAX);
	SWITCH_ADD_APP(app_interface, "cidlookup", "Perform a CID lookup", "Perform a CID lookup",
				   cidlookup_app_function, "[number [skipurl]]", SAF_SUPPORT_NOMEDIA);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_cidlookup_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cidlookup_shutdown)
{
	/* Cleanup dynamically allocated config settings */

	if (globals.db_mutex) {
		switch_mutex_destroy(globals.db_mutex);
	}
	if (globals.master_odbc) {
		switch_odbc_handle_disconnect(globals.master_odbc);
		switch_odbc_handle_destroy(&globals.master_odbc);
	}

	switch_event_unbind(&reload_xml_event);
	return SWITCH_STATUS_SUCCESS;
}


/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automatically
  Macro expands to: switch_status_t mod_cidlookup_runtime()
SWITCH_MODULE_RUNTIME_FUNCTION(mod_cidlookup_runtime)
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
