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
 * mod_cidlookup.c -- API for querying cid->name services and local data
 *
 */

#include <switch.h>
#include <switch_curl.h>

#define SWITCH_REWIND_STREAM(s) s.end = s.data

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cidlookup_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_cidlookup_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_cidlookup_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_cidlookup, mod_cidlookup_load, mod_cidlookup_shutdown, NULL);

static char *SYNTAX = "cidlookup status|number [skipurl] [skipcitystate] [verbose]";

static struct {
	char *url;
	int curl_timeout;
	int curl_warnduration;

	char *whitepages_apikey;

	switch_bool_t cache;
	int cache_expire;

	char *odbc_dsn;
	char *sql;
	char *citystate_sql;

	switch_memory_pool_t *pool;
} globals;

struct http_data {
	switch_stream_handle_t stream;
	switch_size_t bytes;
	switch_size_t max_bytes;
	int err;
};

struct cid_data_obj {
	char *name;
	char *area;
	char *src;
};
typedef struct cid_data_obj cid_data_t;


struct callback_obj {
	switch_memory_pool_t *pool;
	char *name;
};
typedef struct callback_obj callback_t;

static switch_event_node_t *reload_xml_event = NULL;

static switch_cache_db_handle_t *cidlookup_get_db_handle(void)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	
	if (!zstr(globals.odbc_dsn)) {
		dsn = globals.odbc_dsn;
	} else {
		return NULL;
	}

	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}
	
	return dbh;
}


static switch_status_t config_callback_dsn(switch_xml_config_item_t *data, const char *newvalue, switch_config_callback_type_t callback_type,
										   switch_bool_t changed)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_cache_db_handle_t *dbh = NULL;

	if ((callback_type == CONFIG_LOAD || callback_type == CONFIG_RELOAD) && changed) {

		if (zstr(newvalue)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No local database defined.\n");
		} else {
			switch_safe_free(globals.odbc_dsn);
			globals.odbc_dsn = strdup(newvalue);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connecting to dsn: %s\n", globals.odbc_dsn);

			if (!(dbh = cidlookup_get_db_handle())) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open Database!\n");
				switch_goto_status(SWITCH_STATUS_FALSE, done);
			}
		}
	}

	switch_goto_status(SWITCH_STATUS_SUCCESS, done);

  done:
	switch_cache_db_release_db_handle(&dbh);
	return status;
}

static switch_xml_config_string_options_t config_opt_dsn = { NULL, 0, NULL };	/* anything is ok here */
static switch_xml_config_item_t instructions[] = {
	/* parameter name        type                 reloadable   pointer                         default value     options structure */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("url", CONFIG_RELOAD, &globals.url, NULL, "http://server.example.com/app?number=${caller_id_number}",
									 "URL for the CID lookup service"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("whitepages-apikey", CONFIG_RELOAD, &globals.whitepages_apikey, NULL, "api key for whitepages.com",
									 "api key for whitepages.com"),
	SWITCH_CONFIG_ITEM("cache", SWITCH_CONFIG_BOOL, CONFIG_RELOAD, &globals.cache, SWITCH_FALSE, NULL, "true|false", "whether to cache via cidlookup"),
	SWITCH_CONFIG_ITEM("cache-expire", SWITCH_CONFIG_INT, CONFIG_RELOAD, &globals.cache_expire, (void *) 300, NULL, "expire",
					   "seconds to preserve num->name cache"),
	SWITCH_CONFIG_ITEM("curl-timeout", SWITCH_CONFIG_INT, CONFIG_RELOAD, &globals.curl_timeout, (void *) 2000, NULL, "timeout for curl",
					   "milliseconds to timeout"),
	SWITCH_CONFIG_ITEM("curl-warning-duration", SWITCH_CONFIG_INT, CONFIG_RELOAD, &globals.curl_warnduration, (void *) 1000, NULL,
					   "warning when curl queries are longer than specified time", "milliseconds"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("sql", CONFIG_RELOAD, &globals.sql, NULL, "sql whre number=${caller_id_number}", "SQL to run if overriding CID"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("citystate-sql", CONFIG_RELOAD, &globals.citystate_sql, NULL, "sql to look up city/state info",
									 "SQL to run if overriding CID"),
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

static switch_bool_t cidlookup_execute_sql_callback(char *sql, switch_core_db_callback_func_t callback, callback_t *cbt, char **err)
{
	switch_bool_t retval = SWITCH_FALSE;
	switch_cache_db_handle_t *dbh = NULL;

	if (!zstr(globals.odbc_dsn) && (dbh = cidlookup_get_db_handle())) {
		if (switch_cache_db_execute_sql_callback(dbh, sql, callback, (void *) cbt, err) != SWITCH_STATUS_SUCCESS) {
			retval = SWITCH_FALSE;
		} else {
			retval = SWITCH_TRUE;
		}
	} else {
		*err = switch_core_sprintf(cbt->pool, "Unable to get database handle.  dsn: [%s]\n", switch_str_nil(globals.odbc_dsn));
	}

	switch_cache_db_release_db_handle(&dbh);
	return retval;
}

static int cidlookup_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	callback_t *cbt = (callback_t *) pArg;
	switch_memory_pool_t *pool = cbt->pool;

	if (argc < 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unexpected number of columns returned for SQL.  Returned column count: %d. ", argc);
		return SWITCH_STATUS_GENERR;
	}
	cbt->name = switch_core_strdup(pool, switch_str_nil(argv[0]));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Name: %s\n", cbt->name);

	return SWITCH_STATUS_SUCCESS;
}

/* make a new string with digits only */
static char *string_digitsonly(switch_memory_pool_t *pool, const char *str)
{
	char *p, *np, *newstr;
	size_t len;

	p = (char *) str;

	len = strlen(str);
	newstr = switch_core_alloc(pool, len + 1);
	switch_assert(newstr);
	np = newstr;

	while (*p) {
		if (switch_isdigit(*p)) {
			*np = *p;
			np++;
		}
		p++;
	}
	*np = '\0';

	return newstr;
}

static cid_data_t *check_cache(switch_memory_pool_t *pool, const char *number)
{
	char *cmd;
	char *name = NULL;
	char *area = NULL;
	char *src = NULL;
	cid_data_t *cid = NULL;
	switch_stream_handle_t stream = { 0 };

	SWITCH_STANDARD_STREAM(stream);

	cmd = switch_core_sprintf(pool, "get fs:cidlookup:name:%s", number);
	if (switch_api_execute("memcache", cmd, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		if (strncmp("-ERR", stream.data, 4)) {
			name = switch_core_strdup(pool, stream.data);
		} else {
			name = NULL;
		}
	}

	SWITCH_REWIND_STREAM(stream);
	cmd = switch_core_sprintf(pool, "get fs:cidlookup:area:%s", number);
	if (switch_api_execute("memcache", cmd, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		if (strncmp("-ERR", stream.data, 4)) {
			area = switch_core_strdup(pool, stream.data);
		} else {
			area = NULL;
		}
	}

	SWITCH_REWIND_STREAM(stream);
	cmd = switch_core_sprintf(pool, "get fs:cidlookup:src:%s", number);
	if (switch_api_execute("memcache", cmd, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		if (strncmp("-ERR", stream.data, 4)) {
			src = switch_core_strdup(pool, stream.data);
		} else {
			src = NULL;
		}
	}

	if (name || area || src) {
		cid = switch_core_alloc(pool, sizeof(cid_data_t));
		switch_assert(cid);
		cid->name = name;
		cid->area = area;
		cid->src = src;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "memcache: k:'%s', vn:'%s', va:'%s', vs:'%s'\n",
					  cmd, (name) ? name : "(null)", (area) ? area : "(null)", (src) ? src : "(null)");
	switch_safe_free(stream.data);
	return cid;
}

switch_bool_t set_cache(switch_memory_pool_t *pool, char *number, cid_data_t *cid)
{
	char *cmd;
	switch_bool_t success = SWITCH_TRUE;
	switch_stream_handle_t stream = { 0 };

	SWITCH_STANDARD_STREAM(stream);

	cmd = switch_core_sprintf(pool, "set fs:cidlookup:name:%s '%s' %d", number, cid->name, globals.cache_expire);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "memcache: %s\n", cmd);
	if (switch_api_execute("memcache", cmd, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		if (strncmp("-ERR", stream.data, 4)) {
			success = SWITCH_TRUE;
		} else {
			success = SWITCH_FALSE;
			goto done;
		}
	}

	SWITCH_REWIND_STREAM(stream);
	cmd = switch_core_sprintf(pool, "set fs:cidlookup:area:%s '%s' %d", number, cid->area, globals.cache_expire);
	if (switch_api_execute("memcache", cmd, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		if (strncmp("-ERR", stream.data, 4)) {
			success = SWITCH_TRUE;
		} else {
			success = SWITCH_FALSE;
			goto done;
		}
	}

	SWITCH_REWIND_STREAM(stream);
	cmd = switch_core_sprintf(pool, "set fs:cidlookup:src:%s '%s' %d", number, cid->src, globals.cache_expire);
	if (switch_api_execute("memcache", cmd, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		if (strncmp("-ERR", stream.data, 4)) {
			success = SWITCH_TRUE;
		} else {
			success = SWITCH_FALSE;
			goto done;
		}
	}

  done:
	switch_safe_free(stream.data);
	return success;
}

static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	struct http_data *http_data = data;

	http_data->bytes += realsize;

	if (http_data->bytes > http_data->max_bytes) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Oversized file detected [%d bytes]\n", (int) http_data->bytes);
		http_data->err = 1;
		return 0;
	}

	http_data->stream.write_function(&http_data->stream, "%.*s", realsize, ptr);
	return realsize;
}

static long do_lookup_url(switch_memory_pool_t *pool, switch_event_t *event, char **response, const char *query, struct curl_httppost *post,
						  switch_curl_slist_t *headers, int timeout)
{
	switch_time_t start_time = switch_micro_time_now();
	switch_time_t time_diff = 0;
	CURL *curl_handle = NULL;
	long httpRes = 0;

	struct http_data http_data;

	memset(&http_data, 0, sizeof(http_data));

	http_data.max_bytes = 10240;
	SWITCH_STANDARD_STREAM(http_data.stream);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "url: %s\n", query);
	curl_handle = switch_curl_easy_init();

	switch_curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0);
	switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);

	if (!strncasecmp(query, "https", 5)) {
		switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}
	if (post) {
		switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPPOST, post);
	} else {
		switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1);
	}
	if (headers) {
		switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
	}
	switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
	/*
	   TIMEOUT_MS is introduced in 7.16.2, we have 7.16.0 in tree 
	 */
#ifdef CURLOPT_TIMEOUT_MS
	if (timeout > 0) {
		switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, timeout);
	} else {
		switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, globals.curl_timeout);
	}
#else
	if (timeout > 0) {
		switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, timeout);
	} else {
		switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, globals.curl_timeout / 1000);
	}
#endif
	switch_curl_easy_setopt(curl_handle, CURLOPT_URL, query);
	switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
	switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &http_data);
	switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-cidlookup/1.0");

	switch_curl_easy_perform(curl_handle);
	switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	switch_curl_easy_cleanup(curl_handle);

	if (http_data.stream.data && !zstr((char *) http_data.stream.data) && strcmp(" ", http_data.stream.data)) {

		/* don't return UNKNOWN or UNAVAILABLE */
		if (strcasecmp("UNKNOWN", http_data.stream.data) && strcasecmp("UNAVAILABLE", http_data.stream.data)) {
			*response = switch_core_strdup(pool, http_data.stream.data);
		}
	}

	time_diff = (switch_micro_time_now() - start_time);	/* convert to milli from micro */

	if ((time_diff / 1000) >= globals.curl_warnduration) {
		switch_core_time_duration_t duration;
		switch_core_measure_time(time_diff, &duration);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "SLOW LOOKUP ("
						  "%um, " "%us, " "%ums" "): url: %s\n", duration.min, duration.sec, duration.ms, query);
	}

	switch_safe_free(http_data.stream.data);
	return httpRes;
}

static cid_data_t *do_whitepages_lookup(switch_memory_pool_t *pool, switch_event_t *event, const char *num)
{
	char *xml_s = NULL;
	char *query = NULL;
	char *name = NULL;
	char *city = NULL;
	char *state = NULL;
	char *area = NULL;
	switch_xml_t xml = NULL;
	switch_xml_t node = NULL;
	cid_data_t *cid = NULL;

	/* NANPA check */
	if (strlen(num) == 11 && num[0] == '1') {
		num++;					/* skip past leading 1 */
	} else {
		goto done;
	}

	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "whitepages-cid", num);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "whitepages-api-key", globals.whitepages_apikey);

	query = switch_event_expand_headers(event, "http://api.whitepages.com/reverse_phone/1.0/?phone=${whitepages-cid};api_key=${whitepages-api-key}");
	do_lookup_url(pool, event, &xml_s, query, NULL, NULL, 0);
	
	if (zstr(xml_s)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No XML returned for number %s\n", num);
		goto done;
	}
	
	xml = switch_xml_parse_str_dup(xml_s);

	if (!xml) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to parse XML: %s\n", xml_s);
		goto done;
	}

	/* try for bizname first */
	node = switch_xml_get(xml, "wp:listings", 0, "wp:listing", 0, "wp:business", 0, "wp:businessname", -1);
	if (node) {
		name = switch_core_strdup(pool, switch_xml_txt(node));
		goto area;
	}

	node = switch_xml_get(xml, "wp:listings", 0, "wp:listing", 0, "wp:displayname", -1);
	if (node) {
		name = switch_core_strdup(pool, switch_xml_txt(node));
	}

  area:

	node = switch_xml_get(xml, "wp:listings", 0, "wp:listing", 0, "wp:address", 0, "wp:city", -1);
	if (node) {
		city = switch_xml_txt(node);
	}

	node = switch_xml_get(xml, "wp:listings", 0, "wp:listing", 0, "wp:address", 0, "wp:state", -1);
	if (node) {
		state = switch_xml_txt(node);
	}

	if (city || state) {
		area = switch_core_sprintf(pool, "%s %s", city ? city : "", state ? state : "");
	}

  done:

	if (query) {
		switch_safe_free(query);
	}
	if (xml) {
		switch_xml_free(xml);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "whitepages XML: %s\n", xml_s);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "whitepages name: %s, area: %s\n", name ? name : "(null)", area ? area : "(null)");

	cid = switch_core_alloc(pool, sizeof(cid_data_t));
	switch_assert(cid);

	cid->name = name;
	cid->area = area;
	cid->src = "whitepages";
	return cid;
}

static char *do_db_lookup(switch_memory_pool_t *pool, switch_event_t *event, const char *num, const char *sql)
{
	char *name = NULL;
	char *newsql = NULL;
	char *err = NULL;
	callback_t cbt = { 0 };
	cbt.pool = pool;

	if (!zstr(globals.odbc_dsn)) {
		newsql = switch_event_expand_headers(event, sql);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "SQL: %s\n", newsql);
		if (cidlookup_execute_sql_callback(newsql, cidlookup_callback, &cbt, &err)) {
			name = cbt.name;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to lookup cid: %s\n", err ? err : "(null)");
		}
	}
	if (newsql != globals.sql) {
		switch_safe_free(newsql);
	}
	return name;
}

static cid_data_t *do_lookup(switch_memory_pool_t *pool, switch_event_t *event, const char *num, switch_bool_t skipurl, switch_bool_t skipcitystate)
{
	char *number = NULL;
	char *name = NULL;
	char *url_query = NULL;
	cid_data_t *cid = NULL;
	cid_data_t *cidtmp = NULL;
	switch_bool_t save_cache = SWITCH_FALSE;

	cid = switch_core_alloc(pool, sizeof(cid_data_t));
	switch_assert(cid);

	number = string_digitsonly(pool, num);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "caller_id_number", number);

	/* database always wins */
	if (globals.odbc_dsn && globals.sql) {
		name = do_db_lookup(pool, event, number, globals.sql);
		if (name) {
			cid->name = name;
			cid->src = "phone_database";
			goto done;
		}
	}

	if (globals.cache) {
		cidtmp = check_cache(pool, number);
		if (cidtmp) {
			cid = cidtmp;
			cid->src = switch_core_sprintf(pool, "%s (cache)", cid->src);
			goto done;
		}
	}

	if (!skipurl && globals.whitepages_apikey) {
		cid = do_whitepages_lookup(pool, event, number);
		if (cid && cid->name) {	/* only cache if we have a name */
			save_cache = SWITCH_TRUE;
			goto done;
		}
	}

	if (!skipurl && globals.url) {
		url_query = switch_event_expand_headers(event, globals.url);
		do_lookup_url(pool, event, &name, url_query, NULL, NULL, 0);
		if (name) {
			cid->name = name;
			cid->src = "url";

			save_cache = SWITCH_TRUE;
		}
		if (url_query != globals.url) {
			switch_safe_free(url_query);
		}
	}

  done:
	if (!cid) {
		cid = switch_core_alloc(pool, sizeof(cid_data_t));
		switch_assert(cid);
	}
	/* append area if we can */
	if (!cid->area && !skipcitystate && strlen(number) == 11 && number[0] == '1' && globals.odbc_dsn && globals.citystate_sql) {

		/* yes, this is really area */
		name = do_db_lookup(pool, event, number, globals.citystate_sql);
		if (name) {
			cid->area = name;
			if (cid->src) {
				cid->src = switch_core_sprintf(pool, "%s,%s", cid->src, "npanxx_database");
			} else {
				cid->src = "npanxx_database";
			}
		}
	}

	if (!cid->area) {
		cid->area = "UNKNOWN";
	}
	if (!cid->name) {
		if (skipcitystate) {
			if (strlen(number) == 11 && number[0] == '1') {
				int a, b, c;
				sscanf(number, "1%3d%3d%4d", &a, &b, &c);
				cid->name = switch_core_sprintf(pool, "%03d-%03d-%04d", a, b, c);
			} else {
				cid->name = number;
			}
		} else {
			cid->name = cid->area;
		}
	}
	if (!cid->src) {
		cid->src = "UNKNOWN";
	}

	if (globals.cache && save_cache) {
		set_cache(pool, number, cid);
	}


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "cidlookup source: %s\n", cid->src);
	return cid;
}

SWITCH_STANDARD_APP(cidlookup_app_function)
{
	char *argv[4] = { 0 };
	int argc;
	char *mydata = NULL;
	int i;

	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *profile = switch_channel_get_caller_profile(channel);
	cid_data_t *cid = NULL;
	const char *number = NULL;
	switch_bool_t skipurl = SWITCH_FALSE;
	switch_bool_t skipcitystate = SWITCH_FALSE;

	pool = switch_core_session_get_pool(session);
	switch_event_create(&event, SWITCH_EVENT_MESSAGE);

	if (!(mydata = switch_core_session_strdup(session, data))) {
		return;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc > 0) {
			number = switch_core_session_strdup(session, argv[0]);
		}
		for (i = 1; i < argc; i++) {
			if (!strcasecmp(argv[i], "skipurl")) {
				skipurl = SWITCH_TRUE;
			} else if (!strcasecmp(argv[i], "skipcitystate")) {
				skipcitystate = SWITCH_TRUE;
			}
		}
	}

	if (!number && profile) {
		number = switch_caller_get_field_by_name(profile, "caller_id_number");
	}

	if (number) {
		cid = do_lookup(pool, event, number, skipurl, skipcitystate);
	}

	if (cid && channel) {
		switch_event_t *event;

		if (switch_string_var_check_const(cid->name)) {
			switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_CRIT, "Invalid CID data {%s} contains a variable\n", cid->name);
			goto done;
		}

		switch_channel_set_variable(channel, "original_caller_id_name", switch_core_strdup(pool, profile->caller_id_name));
		if (!zstr(cid->src)) {
			switch_channel_set_variable(channel, "cidlookup_source", cid->src);
		}
		if (!zstr(cid->area)) {
			switch_channel_set_variable(channel, "cidlookup_area", cid->area);
		}
		profile->caller_id_name = switch_core_strdup(profile->pool, cid->name);;


		if (switch_event_create(&event, SWITCH_EVENT_CALL_UPDATE) == SWITCH_STATUS_SUCCESS) {
			const char *uuid = switch_channel_get_partner_uuid(channel);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Direction", "RECV");
			
			if (uuid) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridged-To", uuid);
			}
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}

	}


  done:
	if (event) {
		switch_event_destroy(&event);
	}
	if (!session && pool) {
		switch_core_destroy_memory_pool(&pool);
	}
}

SWITCH_STANDARD_API(cidlookup_function)
{
	switch_status_t status;
	char *argv[4] = { 0 };
	int argc;
	int i;
	cid_data_t *cid = NULL;
	char *mydata = NULL;

	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;
	switch_bool_t skipurl = SWITCH_FALSE;
	switch_bool_t skipcitystate = SWITCH_FALSE;
	switch_bool_t verbose = SWITCH_FALSE;

	if (zstr(cmd)) {
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
								   globals.url ? globals.url : "(null)", (globals.cache) ? "true" : "false", globals.cache_expire);
			stream->write_function(stream, " curl-timeout: %" SWITCH_TIME_T_FMT "\n curl-warn-duration: %" SWITCH_TIME_T_FMT "\n",
								   globals.curl_timeout, globals.curl_warnduration);

			stream->write_function(stream, " odbc-dsn: %s\n sql: %s\n citystate-sql: %s\n",
								   globals.odbc_dsn ? globals.odbc_dsn : "(null)",
								   globals.sql ? globals.sql : "(null)", globals.citystate_sql ? globals.citystate_sql : "(null)");


			switch_goto_status(SWITCH_STATUS_SUCCESS, done);
		}
		for (i = 1; i < argc; i++) {
			if (!strcasecmp(argv[i], "skipurl")) {
				skipurl = SWITCH_TRUE;
			} else if (!strcasecmp(argv[i], "skipcitystate")) {
				skipcitystate = SWITCH_TRUE;
			} else if (!strcasecmp(argv[i], "verbose")) {
				verbose = SWITCH_TRUE;
			}
		}

		cid = do_lookup(pool, event, argv[0], skipurl, skipcitystate);
		if (cid) {
			if (switch_string_var_check_const(cid->name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Invalid CID data {%s} contains a variable\n", cid->name);
				stream->write_function(stream, "-ERR Invalid CID data {%s} contains a variable\n", cid->name);
				switch_goto_status(SWITCH_STATUS_SUCCESS, done);
			}
			stream->write_function(stream, cid->name);
			if (verbose) {
				stream->write_function(stream, " (%s) [%s]", cid->area, cid->src);
			}
		} else {
			stream->write_function(stream, "UNKNOWN");
		}
	}
	switch_goto_status(SWITCH_STATUS_SUCCESS, done);

  usage:
	stream->write_function(stream, "-ERR\n%s\n", SYNTAX);
	switch_goto_status(status, done);

  done:
	switch_safe_free(mydata);
	if (event) {
		switch_event_destroy(&event);
	}
	if (!session && pool) {
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

	do_config(SWITCH_FALSE);

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &reload_xml_event) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event!\n");
		return SWITCH_STATUS_TERM;
	}

	SWITCH_ADD_API(api_interface, "cidlookup", "cidlookup API", cidlookup_function, SYNTAX);
	SWITCH_ADD_APP(app_interface, "cidlookup", "Perform a CID lookup", "Perform a CID lookup",
				   cidlookup_app_function, "[number [skipurl]]", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_cidlookup_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cidlookup_shutdown)
{
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
