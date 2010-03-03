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
 * mod_curl.c -- API for performing http queries
 *
 */

#include <switch.h>
#include <curl/curl.h>
#include <json.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_curl_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_curl_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_curl_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_curl, mod_curl_load, mod_curl_shutdown, NULL);

static char *SYNTAX = "curl url [headers|json] [get|head|post [url_encoded_data]]";

static struct {
	switch_memory_pool_t *pool;
} globals;

struct http_data_obj {
	switch_stream_handle_t stream;
	switch_size_t bytes;
	switch_size_t max_bytes;
	switch_memory_pool_t *pool;
	int err;
	long http_response_code;
	char *http_response;
	struct curl_slist *headers;
};
typedef struct http_data_obj http_data_t;

struct callback_obj {
	switch_memory_pool_t *pool;
	char *name;
};
typedef struct callback_obj callback_t;

static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	http_data_t *http_data = data;

	http_data->bytes += realsize;

	if (http_data->bytes > http_data->max_bytes) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Oversized file detected [%d bytes]\n", (int) http_data->bytes);
		http_data->err = 1;
		return 0;
	}

	http_data->stream.write_function(&http_data->stream, "%.*s", realsize, ptr);
	return realsize;
}

static size_t header_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	http_data_t *http_data = data;
	char *header = NULL;

	header = switch_core_alloc(http_data->pool, realsize + 1);
	switch_copy_string(header, ptr, realsize);
	header[realsize] = '\0';

	http_data->headers = curl_slist_append(http_data->headers, header);

	return realsize;
}

static http_data_t *do_lookup_url(switch_memory_pool_t *pool, const char *url, const char *method, const char *data)
{

	CURL *curl_handle = NULL;
	long httpRes = 0;
	char hostname[256] = "";

	http_data_t *http_data = NULL;

	http_data = switch_core_alloc(pool, sizeof(http_data_t));
	memset(http_data, 0, sizeof(http_data_t));
	http_data->pool = pool;

	http_data->max_bytes = 64000;
	SWITCH_STANDARD_STREAM(http_data->stream);

	gethostname(hostname, sizeof(hostname));

	if (!method) {
		method = "get";
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "method: %s, url: %s\n", method, url);
	curl_handle = curl_easy_init();

	if (!strncasecmp(url, "https", 5)) {
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}
	if (!strcasecmp(method, "head")) {
		curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);
	} else if (!strcasecmp(method, "post")) {
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, strlen(data));
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, (void *) data);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Post data: %s\n", data);
	} else {
		curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1);
	}
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 15);
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) http_data);
	curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *) http_data);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-curl/1.0");

	curl_easy_perform(curl_handle);
	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	curl_easy_cleanup(curl_handle);

	if (http_data->stream.data && !zstr((char *) http_data->stream.data) && strcmp(" ", http_data->stream.data)) {

		http_data->http_response = switch_core_strdup(pool, http_data->stream.data);
	}

	http_data->http_response_code = httpRes;

	switch_safe_free(http_data->stream.data);
	return http_data;
}


static char *print_json(switch_memory_pool_t *pool, http_data_t *http_data)
{
	struct json_object *top = NULL;
	struct json_object *headers = NULL;
	char *data = NULL;
	struct curl_slist *header = http_data->headers;

	top = json_object_new_object();
	headers = json_object_new_array();
	json_object_object_add(top, "status_code", json_object_new_int(http_data->http_response_code));
	if (http_data->http_response) {
		json_object_object_add(top, "body", json_object_new_string(http_data->http_response));
	}

	/* parse header data */
	while (header) {
		struct json_object *obj = NULL;
		/* remove trailing \r */
		if ((data =  strrchr(header->data, '\r'))) {
			*data = '\0';
		}

		if (zstr(header->data)) {
			header = header->next;
			continue;
		}

		if ((data = strchr(header->data, ':'))) {
			*data = '\0';
			data++;
			while (*data == ' ' && *data != '\0') {
				data++;
			}
			obj = json_object_new_object();
			json_object_object_add(obj, "key", json_object_new_string(header->data));
			json_object_object_add(obj, "value", json_object_new_string(data));
			json_object_array_add(headers, obj);
		} else {
			if (!strncmp("HTTP", header->data, 4)) {
				char *argv[3] = { 0 };
				int argc;
				if ((argc = switch_separate_string(header->data, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
					if (argc > 2) {
						json_object_object_add(top, "version", json_object_new_string(argv[0]));
						json_object_object_add(top, "phrase", json_object_new_string(argv[2]));
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unparsable header: argc: %d\n", argc);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Starts with HTTP but not parsable: %s\n", header->data);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unparsable header: %s\n", header->data);
			}
		}
		header = header->next;
	}
	json_object_object_add(top, "headers", headers);
	data = switch_core_strdup(pool, json_object_to_json_string(top));
	json_object_put(top);		/* should free up all children */

	return data;
}

SWITCH_STANDARD_APP(curl_app_function)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	char *argv[10] = { 0 };
	int argc;
	char *mydata = NULL;

	switch_memory_pool_t *pool = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *url = NULL;
	char *method = NULL;
	char *postdata = NULL;
	switch_bool_t do_headers = SWITCH_FALSE;
	switch_bool_t do_json = SWITCH_FALSE;
	http_data_t *http_data = NULL;
	struct curl_slist *slist = NULL;
	switch_stream_handle_t stream = { 0 };
	int i = 0;


	if (session) {
		pool = switch_core_session_get_pool(session);
	} else {
		switch_core_new_memory_pool(&pool);
	}
	if (!(mydata = switch_core_session_strdup(session, data))) {
		return;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc == 0) {
			switch_goto_status(SWITCH_STATUS_SUCCESS, usage);
		}

		url = switch_core_strdup(pool, argv[0]);

		for (i = 1; i < argc; i++) {
			if (!strcasecmp("headers", argv[i])) {
				do_headers = SWITCH_TRUE;
			} else if (!strcasecmp("json", argv[i])) {
				do_json = SWITCH_TRUE;
			} else if (!strcasecmp("get", argv[i]) || !strcasecmp("head", argv[i])) {
				method = switch_core_strdup(pool, argv[i]);
			} else if (!strcasecmp("post", argv[i])) {
				if (++i < argc) {
					postdata = switch_core_strdup(pool, argv[i]);
					switch_url_decode(postdata);
				} else {
					postdata = "";
				}
			}
		}
	}

	http_data = do_lookup_url(pool, url, method, postdata);
	if (do_json) {
		switch_channel_set_variable(channel, "curl_response_data", print_json(pool, http_data));
	} else {
		SWITCH_STANDARD_STREAM(stream);
		if (do_headers) {
			slist = http_data->headers;
			while (slist) {
				stream.write_function(&stream, "%s\n", slist->data);
				slist = slist->next;
			}
			stream.write_function(&stream, "\n");
		}
		stream.write_function(&stream, "%s", http_data->http_response ? http_data->http_response : "");
		switch_channel_set_variable(channel, "curl_response_data", stream.data);
	}
	switch_channel_set_variable(channel, "curl_response_code", switch_core_sprintf(pool, "%ld", http_data->http_response_code));
	switch_channel_set_variable(channel, "curl_method", method);

	switch_goto_status(SWITCH_STATUS_SUCCESS, done);

  usage:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", SYNTAX);
	switch_goto_status(status, done);

  done:
	switch_safe_free(stream.data);
	if (http_data && http_data->headers) {
		curl_slist_free_all(http_data->headers);
	}
	if (!session && pool) {
		switch_core_destroy_memory_pool(&pool);
	}
}

SWITCH_STANDARD_API(curl_function)
{
	switch_status_t status;
	char *argv[10] = { 0 };
	int argc;
	char *mydata = NULL;
	char *url = NULL;
	char *method = NULL;
	char *postdata = NULL;
	switch_bool_t do_headers = SWITCH_FALSE;
	switch_bool_t do_json = SWITCH_FALSE;
	struct curl_slist *slist = NULL;
	http_data_t *http_data = NULL;
	int i = 0;

	switch_memory_pool_t *pool = NULL;

	if (zstr(cmd)) {
		switch_goto_status(SWITCH_STATUS_SUCCESS, usage);
	}

	if (session) {
		pool = switch_core_session_get_pool(session);
	} else {
		switch_core_new_memory_pool(&pool);
	}

	mydata = strdup(cmd);
	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc < 1) {
			switch_goto_status(SWITCH_STATUS_SUCCESS, usage);
		}

		url = switch_core_strdup(pool, argv[0]);

		for (i = 1; i < argc; i++) {
			if (!strcasecmp("headers", argv[i])) {
				do_headers = SWITCH_TRUE;
			} else if (!strcasecmp("json", argv[i])) {
				do_json = SWITCH_TRUE;
			} else if (!strcasecmp("get", argv[i]) || !strcasecmp("head", argv[i])) {
				method = switch_core_strdup(pool, argv[i]);
			} else if (!strcasecmp("post", argv[i])) {
				method = "post";
				if (++i < argc) {
					postdata = switch_core_strdup(pool, argv[i]);
					switch_url_decode(postdata);
				} else {
					postdata = "";
				}
			}
		}

		http_data = do_lookup_url(pool, url, method, postdata);
		if (do_json) {
			stream->write_function(stream, "%s", print_json(pool, http_data));
		} else {
			if (do_headers) {
				slist = http_data->headers;
				while (slist) {
					stream->write_function(stream, "%s\n", slist->data);
					slist = slist->next;
				}
				stream->write_function(stream, "\n");
			}
			stream->write_function(stream, "%s", http_data->http_response ? http_data->http_response : "");
		}
	}
	switch_goto_status(SWITCH_STATUS_SUCCESS, done);

  usage:
	stream->write_function(stream, "-ERR\n%s\n", SYNTAX);
	switch_goto_status(status, done);

  done:
	if (http_data && http_data->headers) {
		curl_slist_free_all(http_data->headers);
	}
	switch_safe_free(mydata);
	if (!session && pool) {
		switch_core_destroy_memory_pool(&pool);
	}
	return status;
}

/* Macro expands to: switch_status_t mod_cidlookup_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_curl_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));

	globals.pool = pool;

	SWITCH_ADD_API(api_interface, "curl", "curl API", curl_function, SYNTAX);
	SWITCH_ADD_APP(app_interface, "curl", "Perform a http request", "Perform a http request",
				   curl_app_function, SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_cidlookup_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_curl_shutdown)
{
	/* Cleanup dynamically allocated config settings */

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
