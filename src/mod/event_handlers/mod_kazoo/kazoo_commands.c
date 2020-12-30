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
 * Contributor(s):
 *
 * Karl Anderson <karl@2600hz.com>
 * Darren Schreiber <darren@2600hz.com>
 *
 *
 * kazoo_commands.c -- clones of mod_commands commands slightly modified for kazoo
 *
 */
#include "mod_kazoo.h"
#include <curl/curl.h>
#include <switch_curl.h>

#define UUID_SET_DESC "Set a variable"
#define UUID_SET_SYNTAX "<uuid> <var> [value]"

#define UUID_MULTISET_DESC "Set multiple variables"
#define UUID_MULTISET_SYNTAX "<uuid> <var>=<value>;<var>=<value>..."

#define KZ_HTTP_PUT_DESC "upload a local freeswitch file to a url"
#define KZ_HTTP_PUT_SYNTAX "localfile url"

#define KZ_FIRST_OF_DESC "returns first-of existing event header in params"
#define KZ_FIRST_OF_SYNTAX "list of headers to check"

#define MAX_FIRST_OF 25

SWITCH_STANDARD_API(kz_first_of)
{
	char delim = '|';
	char *mycmd = NULL, *mycmd_dup = NULL, *argv[MAX_FIRST_OF] = { 0 };
	int n, argc = 0;
	switch_event_header_t *header = NULL;
	switch_channel_t *channel = NULL;

	if (zstr(cmd)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid arg\n");
		return SWITCH_STATUS_GENERR;
	}

	if ( session ) {
		channel = switch_core_session_get_channel(session);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "GOT CHANNEL\n");
	}

	mycmd_dup = mycmd = strdup(cmd);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "FIRST-OF %s\n", mycmd);
	if (!zstr(mycmd) && *mycmd == '^' && *(mycmd+1) == '^') {
		mycmd += 2;
		delim = *mycmd++;
	}
	argc = switch_separate_string(mycmd, delim, argv, (sizeof(argv) / sizeof(argv[0])));
	for(n=0; n < argc; n++) {
		char* item = argv[n];
		if(*item == '#' || *item == '!' || *item == '?') {
			if(*(++item) != '\0') {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "RETURNING default %s\n", item);
				stream->write_function(stream, item);
				break;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "CHECKING %s\n", item);
			if (channel) {
				const char *var = switch_channel_get_variable_dup(channel, item, SWITCH_FALSE, -1);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "CHECKING CHANNEL %s\n", item);
				if (var) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "GOT FROM CHANNEL %s => %s\n", item, var);
					stream->write_function(stream, var);
					break;
				}
				if (!strncmp(item, "variable_", 9)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "CHECKING CHANNEL %s\n", item+9);
					var = switch_channel_get_variable_dup(channel, item+9, SWITCH_FALSE, -1);
					if (var) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "GOT FROM CHANNEL %s => %s\n", item+9, var);
						stream->write_function(stream, var);
						break;
					}
				}
			}
			header = switch_event_get_header_ptr(stream->param_event, item);
			if(header) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "RETURNING %s : %s\n", item, header->value);
				stream->write_function(stream, header->value);
				break;
			}
		}
	}

	switch_safe_free(mycmd_dup);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t kz_uuid_setvar(int urldecode, const char *cmd,  switch_core_session_t *session,  switch_stream_handle_t *stream)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if ((argc == 2 || argc == 3) && !zstr(argv[0])) {
			char *uuid = argv[0];
			char *var_name = argv[1];
			char *var_value = NULL;

			if (argc == 3) {
				var_value = argv[2];
			}

			if ((psession = switch_core_session_locate(uuid))) {
				switch_channel_t *channel;
				switch_event_t *event;
				channel = switch_core_session_get_channel(psession);

				if (zstr(var_name)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
					stream->write_function(stream, "-ERR No variable specified\n");
				} else {
					if(urldecode) {
						switch_url_decode(var_value);
					}
					switch_channel_set_variable(channel, var_name, var_value);
					kz_check_set_profile_var(channel, var_name, var_value);
					stream->write_function(stream, "+OK\n");
				}

				if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, event);
					switch_event_fire(&event);
				}

				switch_core_session_rwunlock(psession);

			} else {
				stream->write_function(stream, "-ERR No such channel!\n");
			}
			goto done;
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", UUID_SET_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_setvar_function)
{
	return kz_uuid_setvar(0, cmd, session, stream);
}

SWITCH_STANDARD_API(uuid_setvar_encoded_function)
{
	return kz_uuid_setvar(1, cmd, session, stream);
}

switch_status_t kz_uuid_setvar_multi(int urldecode, const char *cmd,  switch_core_session_t *session,  switch_stream_handle_t *stream)
{
	switch_core_session_t *psession = NULL;
	char delim = ';';
	char *mycmd = NULL, *vars, *argv[64] = { 0 };
	int argc = 0;
	char *var_name, *var_value = NULL;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		char *uuid = mycmd;
		if (!(vars = strchr(uuid, ' '))) {
			goto done;
		}
		*vars++ = '\0';
		if (*vars == '^' && *(vars+1) == '^') {
			vars += 2;
			delim = *vars++;
		}
		if ((psession = switch_core_session_locate(uuid))) {
			switch_channel_t *channel = switch_core_session_get_channel(psession);
			switch_event_t *event;
			int x, y = 0;
			argc = switch_separate_string(vars, delim, argv, (sizeof(argv) / sizeof(argv[0])));

			for (x = 0; x < argc; x++) {
				var_name = argv[x];
				if (var_name && (var_value = strchr(var_name, '='))) {
					*var_value++ = '\0';
				}
				if (zstr(var_name)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
					stream->write_function(stream, "-ERR No variable specified\n");
				} else {
					if(urldecode) {
						switch_url_decode(var_value);
					}
					switch_channel_set_variable(channel, var_name, var_value);
					kz_check_set_profile_var(channel, var_name, var_value);

					y++;
				}
			}

                        /* keep kazoo nodes in sync */
			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(channel, event);
				switch_event_fire(&event);
			}

			switch_core_session_rwunlock(psession);
			if (y) {
				stream->write_function(stream, "+OK\n");
				goto done;
			}
		} else {
			stream->write_function(stream, "-ERR No such channel!\n");
		}
	}

	stream->write_function(stream, "-USAGE: %s\n", UUID_MULTISET_SYNTAX);

  done:
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_setvar_multi_function)
{
	return kz_uuid_setvar_multi(0, cmd, session, stream);
}

SWITCH_STANDARD_API(uuid_setvar_multi_encoded_function)
{
	return kz_uuid_setvar_multi(1, cmd, session, stream);
}

static size_t body_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
	return size * nitems;
}

static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
	switch_event_t* event = (switch_event_t*)userdata;
	int len = strlen(buffer);
	char buf[1024];
	if(len > 2 && len < 1024) {
		snprintf(buf, sizeof(buf), "%s", buffer);
		switch_event_add_header_string(event, SWITCH_STACK_PUSH | SWITCH_STACK_BOTTOM, "Reply-Headers", buf);
	}
	return nitems * size;
}

SWITCH_STANDARD_API(kz_http_put)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_memory_pool_t *lpool = NULL;
	switch_memory_pool_t *pool = NULL;
	char *args = NULL;
	char *argv[10] = { 0 };
	int argc = 0;
	switch_event_t *params = NULL;
	char *url = NULL;
	char *filename = NULL;
	int delete_file = 0;

	switch_curl_slist_t *headers = NULL;  /* optional linked-list of HTTP headers */
	char *ext;  /* file extension, used for MIME type identification */
	const char *mime_type = "application/octet-stream";
	char *buf = NULL;
	char *error = NULL;

	CURL *curl_handle = NULL;
	long httpRes = 0;
	struct stat file_info = {0};
	FILE *file_to_put = NULL;

	if (session) {
		pool = switch_core_session_get_pool(session);
	} else {
		switch_core_new_memory_pool(&lpool);
		pool = lpool;
	}

	if (zstr(cmd)) {
		stream->write_function(stream, "USAGE: %s\n", KZ_HTTP_PUT_SYNTAX);
		status = SWITCH_STATUS_SUCCESS;
		goto done;
	}

	args = strdup(cmd);
	argc = switch_separate_string(args, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	if (argc != 2) {
		stream->write_function(stream, "USAGE: %s\n", KZ_HTTP_PUT_SYNTAX);
		status = SWITCH_STATUS_SUCCESS;
		goto done;
	}

	/* parse params and get profile */
	url = switch_core_strdup(pool, argv[0]);
	if (*url == '{') {
		if (switch_event_create_brackets(url, '{', '}', ',', &params, &url, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
			status = SWITCH_STATUS_FALSE;
			stream->write_function(stream, "-ERR error parsing parameters\n");
			goto done;
		}
	}

	filename = switch_core_strdup(pool, argv[1]);

	/* guess what type of mime content this is going to be */
	if ((ext = strrchr(filename, '.'))) {
		ext++;
		if (!(mime_type = switch_core_mime_ext2type(ext))) {
			mime_type = "application/octet-stream";
		}
	}

	buf = switch_mprintf("Content-Type: %s", mime_type);
	headers = switch_curl_slist_append(headers, buf);

	/* open file and get the file size */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "opening %s for upload to %s\n", filename, url);

	/* libcurl requires FILE* */
 	file_to_put = fopen(filename, "rb");
	if (!file_to_put) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "fopen() error: %s\n", strerror(errno));
		stream->write_function(stream, "-ERR error opening file\n");
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if (fstat(fileno(file_to_put), &file_info) == -1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "fstat() error: %s\n", strerror(errno));
		stream->write_function(stream, "-ERR fstat error\n");
		goto done;
	}

	curl_handle = switch_curl_easy_init();
	if (!curl_handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_curl_easy_init() failure\n");
		status = SWITCH_STATUS_FALSE;
		stream->write_function(stream, "-ERR switch_curl_easy init failure\n");
		goto done;
	}
	switch_curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_PUT, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
	switch_curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	switch_curl_easy_setopt(curl_handle, CURLOPT_READDATA, file_to_put);
	switch_curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);
	switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
	switch_curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-kazoo/1.0");
	switch_curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, stream->param_event);
	switch_curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_callback);
	switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, body_callback);

	switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
	switch_curl_easy_perform(curl_handle);
	switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	switch_curl_easy_cleanup(curl_handle);

	if (httpRes == 200 || httpRes == 201 || httpRes == 202 || httpRes == 204) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s saved to %s\n", filename, url);
		switch_event_add_header(stream->param_event, SWITCH_STACK_BOTTOM, "API-Output", "%s saved to %s", filename, url);
		stream->write_function(stream, "+OK %s saved to %s", filename, url);
		delete_file = 1;
	} else {
		error = switch_mprintf("Received HTTP error %ld trying to save %s to %s", httpRes, filename, url);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", error);
		switch_event_add_header(stream->param_event, SWITCH_STACK_BOTTOM, "API-Error", "%s", error);
		switch_event_add_header(stream->param_event, SWITCH_STACK_BOTTOM, "API-HTTP-Error", "%ld", httpRes);
		stream->write_function(stream, "-ERR %s", error);
		status = SWITCH_STATUS_GENERR;
	}

done:
	if (file_to_put) {
		fclose(file_to_put);
		if(delete_file) {
			remove(filename);
		}
	}

	if (headers) {
		switch_curl_slist_free_all(headers);
	}

	switch_safe_free(buf);
	switch_safe_free(error);

	switch_safe_free(args);

	if (lpool) {
		switch_core_destroy_memory_pool(&lpool);
	}

	if (params) {
		switch_event_destroy(&params);
	}

	return status;
}

SWITCH_STANDARD_API(kz_expand_api)
{
	char *p = NULL, *input = NULL;
	char *uuid = NULL, *mycmd;

	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR invalid input");
		return SWITCH_STATUS_GENERR;
	}

	if (!(mycmd = strdup(cmd))) {
		stream->write_function(stream, "-ERR no memory");
		return SWITCH_STATUS_GENERR;
	}

	if (!strncasecmp(mycmd, "uuid:", 5)) {
		uuid = mycmd + 5;
		if ((input = strchr(uuid, ' ')) != NULL) {
			*input++ = '\0';
		} else {
			stream->write_function(stream, "-ERR invalid argument");
			switch_safe_free(mycmd);
			return SWITCH_STATUS_GENERR;
		}
	} else {
		input = mycmd;
	}

	p = kz_expand(input, uuid);
	stream->write_function(stream, "+OK %s", p);
	if (p != input) {
		switch_safe_free(p);
	}
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(kz_eval_api)
{
	char *p = NULL, *input = NULL;
	char *uuid = NULL, *mycmd;
	switch_core_session_t *nsession = NULL;
	switch_channel_t *channel = NULL;


	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR invalid input");
		return SWITCH_STATUS_GENERR;
	}

	if (!(mycmd = strdup(cmd))) {
		stream->write_function(stream, "-ERR no memory");
		return SWITCH_STATUS_GENERR;
	}

	if (!strncasecmp(mycmd, "uuid:", 5)) {
		uuid = mycmd + 5;
		if ((input = strchr(uuid, ' ')) != NULL) {
			*input++ = '\0';
			if ((nsession = switch_core_session_locate(uuid)) != NULL) {
				channel = switch_core_session_get_channel(nsession);
			} else {
				stream->write_function(stream, "-ERR invalid session");
				switch_safe_free(mycmd);
				return SWITCH_STATUS_GENERR;
			}
		} else {
			stream->write_function(stream, "-ERR invalid argument");
			switch_safe_free(mycmd);
			return SWITCH_STATUS_GENERR;
		}
	} else if (session == NULL) {
		stream->write_function(stream, "-ERR invalid argument");
		switch_safe_free(mycmd);
		return SWITCH_STATUS_GENERR;
	} else {
		channel = switch_core_session_get_channel(session);
		input = mycmd;
	}

	p = switch_channel_expand_variables_check(channel, input, NULL, NULL, 0);
	stream->write_function(stream, "+OK %s", p);
	if (p != input) {
		switch_safe_free(p);
	}
	switch_safe_free(mycmd);
	if (nsession) {
		switch_core_session_rwunlock(nsession);
	}
	return SWITCH_STATUS_SUCCESS;
}

#define KZ_CONTACT_DESC "returns kazoo contact"
#define KZ_CONTACT_SYNTAX "endpoint@account"

SWITCH_STANDARD_API(kz_contact_fun)
{
	switch_event_t *params = NULL;
	const char *var = NULL;
	switch_xml_t xml_node = NULL;
	switch_xml_t xml_root = NULL;

	const char *reply = "error/subscriber_absent";

	if (!cmd) {
		stream->write_function(stream, "%s", reply);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "call");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Fetch-Exclude-Cache", "true");

	/*
	if (stream->param_event) {
		switch_event_merge(params, stream->param_event);
	}
	*/

	/*
	if (session) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Fetch-Call-UUID", switch_core_session_get_uuid(session));
	} else if (stream->param_event && (var = switch_event_get_header(stream->param_event, "ent_originate_aleg_uuid")) != NULL) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Fetch-Call-UUID", var);
	}
	*/

	if (switch_xml_locate("directory", "location", "id", cmd, &xml_root, &xml_node, params, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "%s", reply);
		return SWITCH_STATUS_SUCCESS;
	}

	var = switch_xml_attr(xml_node, "value");
	if (!zstr(var)) {
		reply = var;
	}

	stream->write_function(stream, "%s", reply);

	switch_xml_free(xml_root);
	return SWITCH_STATUS_SUCCESS;
}

void add_kz_commands(switch_loadable_module_interface_t **module_interface) {
	switch_api_interface_t *api_interface = NULL;
	SWITCH_ADD_API(api_interface, "kz_uuid_setvar_multi", UUID_SET_DESC, uuid_setvar_multi_function, UUID_MULTISET_SYNTAX);
	SWITCH_ADD_API(api_interface, "kz_uuid_setvar_multi_encoded", UUID_SET_DESC, uuid_setvar_multi_encoded_function, UUID_MULTISET_SYNTAX);
	switch_console_set_complete("add kz_uuid_setvar_multi ::console::list_uuid");
	switch_console_set_complete("add kz_uuid_setvar_multi_encoded ::console::list_uuid");
	SWITCH_ADD_API(api_interface, "kz_uuid_setvar", UUID_MULTISET_DESC, uuid_setvar_function, UUID_SET_SYNTAX);
	SWITCH_ADD_API(api_interface, "kz_uuid_setvar_encoded", UUID_MULTISET_DESC, uuid_setvar_encoded_function, UUID_SET_SYNTAX);
	switch_console_set_complete("add kz_uuid_setvar ::console::list_uuid");
	switch_console_set_complete("add kz_uuid_setvar_encoded ::console::list_uuid");
	SWITCH_ADD_API(api_interface, "kz_http_put", KZ_HTTP_PUT_DESC, kz_http_put, KZ_HTTP_PUT_SYNTAX);
	SWITCH_ADD_API(api_interface, "first-of", KZ_FIRST_OF_DESC, kz_first_of, KZ_FIRST_OF_SYNTAX);
	SWITCH_ADD_API(api_interface, "kz_expand", KZ_FIRST_OF_DESC, kz_expand_api, KZ_FIRST_OF_SYNTAX);
	SWITCH_ADD_API(api_interface, "kz_eval", KZ_FIRST_OF_DESC, kz_eval_api, KZ_FIRST_OF_SYNTAX);
	SWITCH_ADD_API(api_interface, "kz_contact", KZ_CONTACT_DESC, kz_contact_fun, KZ_CONTACT_SYNTAX);
}

