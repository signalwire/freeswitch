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
 * Rupa Schomaker <rupa@rupa.com>
 * Yossi Neiman <mishehu@freeswitch.org>
 *
 * mod_curl.c -- API for performing http queries
 *
 */

#include <switch.h>
#include <switch_curl.h>
#include <json.h>
#ifdef  _MSC_VER
#include <WinSock2.h>
#else
#include <sys/socket.h>
#endif


/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_curl_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_curl_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_curl_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_curl, mod_curl_load, mod_curl_shutdown, NULL);

static char *SYNTAX = "curl url [headers|json|content-type <mime-type>] [get|head|post [post_data]]";

#define HTTP_SENDFILE_ACK_EVENT "curl_sendfile::ack"
#define HTTP_SENDFILE_RESPONSE_SIZE 32768

static struct {
	switch_memory_pool_t *pool;
} globals;

typedef enum {
	CSO_NONE = (1 << 0),
	CSO_EVENT = (1 << 1),
	CSO_STREAM = (1 << 2)
} curlsendfile_output_t;

struct http_data_obj {
	switch_stream_handle_t stream;
	switch_size_t bytes;
	switch_size_t max_bytes;
	switch_memory_pool_t *pool;
	int err;
	long http_response_code;
	char *http_response;
	switch_curl_slist_t *headers;
};
typedef struct http_data_obj http_data_t;

struct http_sendfile_data_obj {
	switch_memory_pool_t *pool;
	switch_file_t *file_handle;
	long http_response_code;
	char *http_response;
	switch_curl_slist_t *headers;
	char *mydata;
	char *url;
	char *identifier_str;
	char *filename_element;
	char *filename_element_name;
	char *extrapost_elements;
	switch_CURL *curl_handle;
	struct curl_httppost *formpost;
	struct curl_httppost *lastptr;
	uint8_t flags; /* This is for where to send output of the curl_sendfile commands */
	switch_stream_handle_t *stream;
	char *sendfile_response;
	switch_size_t sendfile_response_count;
};

typedef struct http_sendfile_data_obj http_sendfile_data_t;

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

	http_data->headers = switch_curl_slist_append(http_data->headers, header);

	return realsize;
}

static http_data_t *do_lookup_url(switch_memory_pool_t *pool, const char *url, const char *method, const char *data, const char *content_type)
{
	switch_CURL *curl_handle = NULL;
	long httpRes = 0;
	http_data_t *http_data = NULL;
	switch_curl_slist_t *headers = NULL;

	http_data = switch_core_alloc(pool, sizeof(http_data_t));
	memset(http_data, 0, sizeof(http_data_t));
	http_data->pool = pool;

	http_data->max_bytes = 64000;
	SWITCH_STANDARD_STREAM(http_data->stream);

	if (!method) {
		method = "get";
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "method: %s, url: %s, content-type: %s\n", method, url, content_type);
	curl_handle = switch_curl_easy_init();

	if (!strncasecmp(url, "https", 5)) {
		switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}
	if (!strcasecmp(method, "head")) {
		switch_curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);
	} else if (!strcasecmp(method, "post")) {
		switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, strlen(data));
		switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, (void *) data);
		if (content_type) {
			char *ct = switch_mprintf("Content-Type: %s", content_type);
			headers = switch_curl_slist_append(headers, ct);
			switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
			switch_safe_free(ct);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Post data: %s\n", data);
	} else {
		switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1);
	}
	switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 15);
	switch_curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
	switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) http_data);
	switch_curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_callback);
	switch_curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *) http_data);
	switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-curl/1.0");

	switch_curl_easy_perform(curl_handle);
	switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	switch_curl_easy_cleanup(curl_handle);
	switch_curl_slist_free_all(headers);

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
	switch_curl_slist_t *header = http_data->headers;

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

static size_t http_sendfile_response_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	http_sendfile_data_t *http_data = data;
	
	if(http_data->sendfile_response_count + realsize < HTTP_SENDFILE_RESPONSE_SIZE)
	{
		// I'm not sure why we need the (realsize+1) here, but it truncates the data by 1 char if I don't do this
		switch_copy_string(&http_data->sendfile_response[http_data->sendfile_response_count], ptr, (realsize+1));
		http_data->sendfile_response_count += realsize;
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Response page is more than %d bytes long, truncating.\n", HTTP_SENDFILE_RESPONSE_SIZE);
		realsize = 0;
	}
	
	return realsize;
}

// This function and do_lookup_url functions could possibly be merged together.  Or at least have do_lookup_url call this up as part of the initialization routine as it is a subset of the operations.
static void http_sendfile_initialize_curl(http_sendfile_data_t *http_data)
{
	uint8_t count;
	http_data->curl_handle = curl_easy_init();
	
	if (!strncasecmp(http_data->url, "https", 5))
	{
		curl_easy_setopt(http_data->curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(http_data->curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}
	
	/* From the docs: 
	 * Optionally, you can provide data to POST using the CURLOPT_READFUNCTION and CURLOPT_READDATA 
	 * options but then you must make sure to not set CURLOPT_POSTFIELDS to anything but NULL
	 * curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, strlen(data));
	 * curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, (void *) data);
	 */
	
	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Post data: %s\n", data);
	
	curl_easy_setopt(http_data->curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(http_data->curl_handle, CURLOPT_MAXREDIRS, 15);
	curl_easy_setopt(http_data->curl_handle, CURLOPT_URL, http_data->url);
	curl_easy_setopt(http_data->curl_handle, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(http_data->curl_handle, CURLOPT_USERAGENT, "freeswitch-curl/1.0");
	
	http_data->sendfile_response = switch_core_alloc(http_data->pool, sizeof(char) * HTTP_SENDFILE_RESPONSE_SIZE);
	memset(http_data->sendfile_response, 0, sizeof(char) * HTTP_SENDFILE_RESPONSE_SIZE);
	
	// Set the function where we will copy out the response body data to
	curl_easy_setopt(http_data->curl_handle, CURLOPT_WRITEFUNCTION, http_sendfile_response_callback);
	curl_easy_setopt(http_data->curl_handle, CURLOPT_WRITEDATA, (void *) http_data);
	
	/* Add the file to upload as a POST form field */ 
	curl_formadd(&http_data->formpost, &http_data->lastptr, CURLFORM_COPYNAME, http_data->filename_element_name, CURLFORM_FILE, http_data->filename_element, CURLFORM_END);
	
	if(!zstr(http_data->extrapost_elements))
	{
		// Now to parse out the individual post element/value pairs
		char *argv[64] = { 0 }; // Probably don't need 64 but eh does it really use that much memory?
		uint32_t argc = 0;
		char *temp_extrapost = switch_core_strdup(http_data->pool, http_data->extrapost_elements);
		
		argc = switch_separate_string(temp_extrapost, '&', argv, (sizeof(argv) / sizeof(argv[0])));
		
		for(count = 0; count < argc; count++)
		{
			char *argv2[4] = { 0 };
			uint32_t argc2 = switch_separate_string(argv[count], '=', argv2, (sizeof(argv2) / sizeof(argv2[0])));
			
			if(argc2 == 2)
				curl_formadd(&http_data->formpost, &http_data->lastptr, CURLFORM_COPYNAME, argv2[0], CURLFORM_COPYCONTENTS, argv2[1], CURLFORM_END);
		}
	}
	
	/* Fill in the submit field too, even if this isn't really needed */ 
	curl_formadd(&http_data->formpost, &http_data->lastptr, CURLFORM_COPYNAME, "submit", CURLFORM_COPYCONTENTS, "or_die", CURLFORM_END);
	
	/* what URL that receives this POST */ 
	curl_easy_setopt(http_data->curl_handle, CURLOPT_HTTPPOST, http_data->formpost);
	
	// This part actually fires off the curl, captures the HTTP response code, and then frees up the handle.
	curl_easy_perform(http_data->curl_handle);
	curl_easy_getinfo(http_data->curl_handle, CURLINFO_RESPONSE_CODE, &http_data->http_response_code);
	
	curl_easy_cleanup(http_data->curl_handle);
		
	// Clean up the form data from POST
	curl_formfree(http_data->formpost);
}

static switch_status_t http_sendfile_test_file_open(http_sendfile_data_t *http_data, switch_event_t *event)
{
	switch_status_t retval = switch_file_open(&http_data->file_handle, http_data->filename_element, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD,http_data->pool);
	if(retval != SWITCH_STATUS_SUCCESS)
	{
		if(switch_test_flag(http_data, CSO_EVENT))
		{
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, HTTP_SENDFILE_ACK_EVENT) == SWITCH_STATUS_SUCCESS)
			{
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Command-Execution-Identifier", http_data->identifier_str);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Filename", http_data->filename_element);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File-Access", "Failure");
				switch_event_fire(&event);
				switch_event_destroy(&event);
			}
			else
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create event to notify of failure to open file %s\n", http_data->filename_element);
		}
		
		if((switch_test_flag(http_data, CSO_STREAM) || switch_test_flag(http_data, CSO_NONE)) && http_data->stream)
			http_data->stream->write_function(http_data->stream, "-Err Unable to open file %s\n", http_data->filename_element);
		
		if(switch_test_flag(http_data, CSO_NONE) && !http_data->stream)
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "curl_sendfile: Unable to open file %s\n", http_data->filename_element);
	}
	
	return retval;
}

static void http_sendfile_success_report(http_sendfile_data_t *http_data, switch_event_t *event)
{
	if(switch_test_flag(http_data, CSO_EVENT))
	{
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, HTTP_SENDFILE_ACK_EVENT) == SWITCH_STATUS_SUCCESS)
		{
			char *code_as_string = switch_core_alloc(http_data->pool, 16);
			memset(code_as_string, 0, 16);
			switch_snprintf(code_as_string, 16, "%d", http_data->http_response_code);
			
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Command-Execution-Identifier", http_data->identifier_str);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Filename", http_data->filename_element);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "File-Access", "Success");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "REST-HTTP-Code", code_as_string);
			switch_event_add_body(event, "%s", http_data->sendfile_response);
			
			switch_event_fire(&event);
			switch_event_destroy(&event);
		}
		else
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create a event to report on success of curl_sendfile.\n");
	}
	
	if((switch_test_flag(http_data, CSO_STREAM) || switch_test_flag(http_data, CSO_NONE) || switch_test_flag(http_data, CSO_EVENT)) && http_data->stream)
	{
		if(http_data->http_response_code == 200)
			http_data->stream->write_function(http_data->stream, "+200 Ok\n");
		else
			http_data->stream->write_function(http_data->stream, "-%d Err\n", http_data->http_response_code);
		
		if(http_data->sendfile_response_count && switch_test_flag(http_data, CSO_STREAM))
			http_data->stream->write_function(http_data->stream, "%s\n", http_data->sendfile_response);
	}
	
	if(switch_test_flag(http_data, CSO_NONE) && !http_data->stream)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Sending of file %s to url %s resulted with code %lu\n", http_data->filename_element, http_data->url, http_data->http_response_code);
}

#define HTTP_SENDFILE_APP_SYNTAX "<url> <filenameParamName=filepath> [nopost|postparam1=foo&postparam2=bar... [event|none  [identifier ]]]"
SWITCH_STANDARD_APP(http_sendfile_app_function)
{
	switch_event_t *event = NULL;
	char *argv[10] = { 0 }, *argv2[10] = { 0 };
	int argc = 0, argc2 = 0;
	http_sendfile_data_t *http_data = NULL;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	
	assert(channel != NULL);
	
	http_data = switch_core_alloc(pool, sizeof(http_sendfile_data_t));
	memset(http_data, 0, sizeof(http_sendfile_data_t));
	
	http_data->pool = pool;
	
	// Either the parameters are provided on the data="" or else they are provided as chanvars.  No mixing & matching
	if(!zstr(data))
	{
		http_data->mydata = switch_core_strdup(http_data->pool, data);
		
		if ((argc = switch_separate_string(http_data->mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) 
		{
			uint8_t i = 0;
			
			if (argc < 2 || argc > 5)
				goto http_sendfile_app_usage;
			
			http_data->url = switch_core_strdup(http_data->pool, argv[i++]);
			
			switch_url_decode(argv[i]);
			argc2 = switch_separate_string(argv[i++], '=', argv2, (sizeof(argv2) / sizeof(argv2[0])));
			
			if(argc2 == 2)
			{
				http_data->filename_element_name = switch_core_strdup(pool, argv2[0]);
				http_data->filename_element = switch_core_strdup(pool, argv2[1]);
			}
			else
				goto http_sendfile_app_usage;
			
			if(argc > 2)
			{
				http_data->extrapost_elements = switch_core_strdup(pool, argv[i++]);
				
				if(argc > 3)
				{
					if(!strncasecmp(argv[i++], "event", 5))
					{
						switch_set_flag(http_data, CSO_EVENT);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting output to event handler.\n");
					}
					
					if(argc > 4)
					{
						if(strncasecmp(argv[i], "uuid", 4))
							http_data->identifier_str = switch_core_session_get_uuid(session);
						else
							http_data->identifier_str = switch_core_strdup(pool, argv[i++]);
					}
				}
			}
		}
	}
	else
	{
		char *send_output = (char *) switch_channel_get_variable_dup(channel, "curl_sendfile_report", SWITCH_TRUE, -1);
		char *identifier = (char *) switch_channel_get_variable_dup(channel, "curl_sendfile_identifier", SWITCH_TRUE, -1);
		
		http_data->url = (char *) switch_channel_get_variable_dup(channel, "curl_sendfile_url", SWITCH_TRUE, -1);
		http_data->filename_element_name = (char *) switch_channel_get_variable_dup(channel, "curl_sendfile_filename_element", SWITCH_TRUE, -1);
		http_data->filename_element = (char *) switch_channel_get_variable_dup(channel, "curl_sendfile_filename", SWITCH_TRUE, -1);
		http_data->extrapost_elements = (char *) switch_channel_get_variable_dup(channel, "curl_sendfile_extrapost", SWITCH_TRUE, -1);
		
		
		if(zstr(http_data->url) || zstr(http_data->filename_element) || zstr(http_data->filename_element_name))
			goto http_sendfile_app_usage;
		
		if(!zstr(send_output))
		{
			if(!strncasecmp(send_output, "event", 5))
				switch_set_flag(http_data, CSO_EVENT);
			else if(!strncasecmp(send_output, "none", 4))
				switch_set_flag(http_data, CSO_NONE);
			else
			{
				switch_set_flag(http_data, CSO_NONE);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Invalid parameter %s specified for curl_sendfile_report.  Setting default of 'none'.\n", send_output);
			}
		}
		else
		{
			switch_set_flag(http_data, CSO_NONE);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No parameter specified for curl_sendfile_report.  Setting default of 'none'.\n");
		}
		
		if(!zstr(identifier))
		{
			if(!strncasecmp(identifier, "uuid", 4))
				http_data->identifier_str = switch_core_session_get_uuid(session);
			else if(!zstr(identifier))
				http_data->identifier_str = identifier;
		}
	}
	
	switch_url_decode(http_data->filename_element_name);
	switch_url_decode(http_data->filename_element);
	
	// We need to check the file now...
	if(http_sendfile_test_file_open(http_data, event) != SWITCH_STATUS_SUCCESS)
		goto http_sendfile_app_done;
	
	switch_file_close(http_data->file_handle);
	
	switch_url_decode(http_data->url);
	
	http_sendfile_initialize_curl(http_data);
	
	http_sendfile_success_report(http_data, event);
	
	goto http_sendfile_app_done;
	
http_sendfile_app_usage:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failure:  Usage: <data=\"%s\">\nOr you can set chanvars curl_senfile_url, curl_sendfile_filename_element, curl_sendfile_filename, curl_sendfile_extrapost\n", HTTP_SENDFILE_APP_SYNTAX);
	
http_sendfile_app_done:
	if (http_data && http_data->headers)
	{
		switch_curl_slist_free_all(http_data->headers);
	}
	
	return;
}

#define HTTP_SENDFILE_SYNTAX "<url> <filenameParamName=filepath> [nopost|postparam1=foo&postparam2=bar... [event|stream|both|none  [identifier ]]]"
SWITCH_STANDARD_API(http_sendfile_function)
{
	switch_status_t status;
	switch_bool_t new_memory_pool = SWITCH_FALSE;
	char *argv[10] = { 0 }, *argv2[10] = { 0 };
	int argc = 0, argc2 = 0;
	http_sendfile_data_t *http_data = NULL;
	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;
	
	if(zstr(cmd))
	{
		status = SWITCH_STATUS_SUCCESS;
		goto http_sendfile_usage;
	}
	if(session)
	{
		pool = switch_core_session_get_pool(session);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "We're using a session's memory pool for curl_sendfile.  Maybe we should consider always making a new memory pool?\n");
	}
	else
	{
		switch_core_new_memory_pool(&pool);
		new_memory_pool = SWITCH_TRUE;  // So we can properly destroy the memory pool
	}
	
	http_data = switch_core_alloc(pool, sizeof(http_sendfile_data_t));
	memset(http_data, 0, sizeof(http_sendfile_data_t));
	
	http_data->mydata = switch_core_strdup(pool, cmd);
	http_data->stream = stream;
	http_data->pool = pool;
	
	// stream->write_function(stream,"\ncmd is %s\nmydata is %s\n", cmd, http_data->mydata); 
	
	if ((argc = switch_separate_string(http_data->mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) 
	{
		uint8_t i = 0;
		
		if (argc < 2 || argc > 5)
		{
			status = SWITCH_STATUS_SUCCESS;
			goto http_sendfile_usage;
		}
		
		http_data->url = switch_core_strdup(pool, argv[i++]);
		
		switch_url_decode(argv[i]);
		argc2 = switch_separate_string(argv[i++], '=', argv2, (sizeof(argv2) / sizeof(argv2[0])));
		
		if(argc2 == 2)
		{
			http_data->filename_element_name = switch_core_strdup(pool, argv2[0]);
			http_data->filename_element = switch_core_strdup(pool, argv2[1]);
		}
		else
			goto http_sendfile_usage;
		
		switch_url_decode(http_data->filename_element_name);
		switch_url_decode(http_data->filename_element);
		
		if(argc > 2)
		{
			http_data->extrapost_elements = switch_core_strdup(pool, argv[i++]);
			
			if(argc > 3)
			{
				if(!strncasecmp(argv[i], "event", 5))
					switch_set_flag(http_data, CSO_EVENT);
				else if(!strncasecmp(argv[i], "stream", 6))
					switch_set_flag(http_data, CSO_STREAM);
				else if(!strncasecmp(argv[i], "both", 4))
				{
					switch_set_flag(http_data, CSO_EVENT);
					switch_set_flag(http_data, CSO_STREAM);
				}
				else
				{
					if(strncasecmp(argv[i], "none", 4))
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Invalid 4th parameter set for curl_sendfile.  Defaulting to \"none\"\n");
					
					switch_set_flag(http_data, CSO_NONE);
				}
				
				i++;
				
				if(argc > 4)
					http_data->identifier_str = switch_core_strdup(pool, argv[i++]);
			}
		}
	}
	
	// We need to check the file now...
	if(http_sendfile_test_file_open(http_data, event) != SWITCH_STATUS_SUCCESS)
		goto http_sendfile_done;
	
	
	switch_file_close(http_data->file_handle);
	
	switch_url_decode(http_data->url);
	
	http_sendfile_initialize_curl(http_data);
	
	http_sendfile_success_report(http_data, event);
	
	status = SWITCH_STATUS_SUCCESS;
	goto http_sendfile_done;

http_sendfile_usage:
	stream->write_function(stream, "-USAGE\n%s\n", HTTP_SENDFILE_SYNTAX);
	goto http_sendfile_done;

http_sendfile_done:
	if (http_data && http_data->headers)
	{
		switch_curl_slist_free_all(http_data->headers);
	}
	
	if (new_memory_pool == SWITCH_TRUE)
	{
		switch_core_destroy_memory_pool(&pool);
	}
	
	return status;
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
	char *content_type = NULL;
	switch_bool_t do_headers = SWITCH_FALSE;
	switch_bool_t do_json = SWITCH_FALSE;
	http_data_t *http_data = NULL;
	switch_curl_slist_t *slist = NULL;
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
				method = "post";
				if (++i < argc) {
					postdata = switch_core_strdup(pool, argv[i]);
					switch_url_decode(postdata);
				} else {
					postdata = "";
				}
			} else if (!strcasecmp("content-type", argv[i])) {
				if (++i < argc) {
					content_type = switch_core_strdup(pool, argv[i]);
				}
			}
		}
	}

	http_data = do_lookup_url(pool, url, method, postdata, content_type);
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
		switch_curl_slist_free_all(http_data->headers);
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
	char *content_type = NULL;
	switch_bool_t do_headers = SWITCH_FALSE;
	switch_bool_t do_json = SWITCH_FALSE;
	switch_curl_slist_t *slist = NULL;
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
			} else if (!strcasecmp("content-type", argv[i])) {
				if (++i < argc) {
					content_type = switch_core_strdup(pool, argv[i]);
				}
			}
		}

		http_data = do_lookup_url(pool, url, method, postdata, content_type);
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
		switch_curl_slist_free_all(http_data->headers);
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
	
	SWITCH_ADD_API(api_interface, "curl_sendfile", "curl_sendfile API", http_sendfile_function, HTTP_SENDFILE_SYNTAX);
	SWITCH_ADD_APP(app_interface, "curl_sendfile", "Send a file and some optional post variables via HTTP", "Send a file and some optional post variables via HTTP",
				   http_sendfile_app_function, HTTP_SENDFILE_APP_SYNTAX, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
