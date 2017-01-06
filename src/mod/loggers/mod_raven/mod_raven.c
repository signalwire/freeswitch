/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, James Martelletti <james@nerdc0re.com>
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
 * James Martelletti <james@nerdc0re.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Tamas Cseke <tamas.cseke@vcc.live>
 *
 * mod_raven.c -- Raven Logging
 *
 */
#include <switch.h>
#include <zlib.h>
#include <switch_curl.h>

#define RAVEN_ZLIB_CHUNK 16384
#define RAVEN_VERSION "5"
#define RAVEN_UA "freeswitch-raven/1.0"

SWITCH_MODULE_LOAD_FUNCTION(mod_raven_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_raven_shutdown);
SWITCH_MODULE_DEFINITION(mod_raven, mod_raven_load, mod_raven_shutdown, NULL);

static switch_status_t load_config(void);

static struct {
	char *uri;
	char *key;
	char *secret;
	char *project;
	switch_bool_t log_uuid;
	switch_log_level_t log_level;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_uri, globals.uri);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_key, globals.key);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_secret, globals.secret);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_project, globals.project);

static switch_loadable_module_interface_t raven_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

static switch_status_t encode(const char *raw, int raw_len, char **encoded_out)
{
    z_stream stream;
    unsigned char *encoded = NULL, *compressed = NULL;
    int ret;
	switch_size_t compressed_size = 0, compressed_len = 0, need_bytes;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    ret = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
		return SWITCH_STATUS_FALSE;
    }

    stream.next_in = (unsigned char *)raw;
    stream.avail_in = raw_len;

    do {
		compressed_size += RAVEN_ZLIB_CHUNK;
		compressed = realloc(compressed, compressed_size + 1);
		switch_assert(compressed != NULL);

		stream.avail_out = compressed_size - compressed_len;
		stream.next_out = compressed + compressed_len;

		ret = deflate(&stream, Z_FINISH);
		assert(ret != Z_STREAM_ERROR);
		compressed_len = compressed_size - stream.avail_out;
    } while (stream.avail_in != 0);

	deflateEnd(&stream);

	need_bytes = compressed_len * 3 + 1;
	encoded = malloc(need_bytes);
	switch_assert(encoded);
	memset(encoded, 0, need_bytes);
	switch_b64_encode(compressed, compressed_len, encoded, need_bytes);

	switch_safe_free(compressed);

    *encoded_out = (char *)encoded;

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t raven_capture(const char *userdata, const char *message, const char *level, const char *file, const char *func, int line)
{
    cJSON* json, *fingerprint;
    char *raw_body;
    char *encoded_body = NULL;
    switch_time_t timestamp = switch_micro_time_now();
    switch_status_t status = SWITCH_STATUS_SUCCESS;
	char uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];

	switch_uuid_str(uuid, sizeof(uuid));
    json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "event_id", (const char *)uuid);
    cJSON_AddNumberToObject(json, "timestamp", timestamp);
    cJSON_AddStringToObject(json, "platform", RAVEN_UA);
    cJSON_AddStringToObject(json, "project", globals.project);
    cJSON_AddStringToObject(json, "server_name", switch_core_get_hostname());
    cJSON_AddStringToObject(json, "level", level);

	if (globals.log_uuid && !zstr(userdata)) {
		cJSON_AddItemToObject(json, "message", cJSON_CreateStringPrintf("%s %s", userdata, message));
	} else {
		cJSON_AddStringToObject(json, "message", message);
	}

    fingerprint = cJSON_CreateArray();
    cJSON_AddItemToArray(fingerprint, cJSON_CreateString(file));
    cJSON_AddItemToArray(fingerprint, cJSON_CreateString(func));
    cJSON_AddItemToArray(fingerprint, cJSON_CreateNumber(line));
    cJSON_AddItemToObject(json, "fingerprint", fingerprint);

    raw_body = cJSON_PrintUnformatted(json);

    if ((status = encode(raw_body, strlen(raw_body), &encoded_body)) == SWITCH_STATUS_SUCCESS) {
		int response;
		CURL *curl_handle = switch_curl_easy_init();
		switch_curl_slist_t * list = NULL;
		char *auth_header = switch_mprintf("X-Sentry-Auth: Sentry sentry_version=%s,"
										   " sentry_client=%s,"
										   " sentry_timestamp=%d,"
										   " sentry_key=%s,"
										   " sentry_secret=%s",
										   RAVEN_VERSION, RAVEN_UA,
										   timestamp, globals.key, globals.secret);

		char *url = switch_mprintf( "%s/api/%s/store/", globals.uri, globals.project);
		switch_curl_easy_setopt(curl_handle, CURLOPT_URL,url);
		switch_curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
        switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, encoded_body);
        switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, strlen(encoded_body));

		switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, RAVEN_UA);

		list = switch_curl_slist_append(list, auth_header);
		list = switch_curl_slist_append(list, "Content-Type: application/octet-stream");
		switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, list);

		if (!strncasecmp(globals.uri, "https", 5)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}

		switch_curl_easy_perform(curl_handle);
		switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response);

		if (response != 200) {
			status = SWITCH_STATUS_FALSE;
		}

		switch_curl_easy_cleanup(curl_handle);
		switch_curl_slist_free_all(list);
		switch_safe_free(url);
		switch_safe_free(auth_header);
    }

    switch_safe_free(raw_body);
    switch_safe_free(encoded_body);
    cJSON_Delete(json);

    return status;
}


static switch_status_t mod_raven_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (level != SWITCH_LOG_CONSOLE && !zstr(node->data)) {
		const char * raven_level;

		switch (level) {
		case SWITCH_LOG_DEBUG:
			raven_level = "debug";
			break;
		case SWITCH_LOG_INFO:
			raven_level = "info";
			break;
		case SWITCH_LOG_NOTICE:
		case SWITCH_LOG_WARNING:
			raven_level = "warning";
			break;
		case SWITCH_LOG_ERROR:
			raven_level = "error";
			break;
		case SWITCH_LOG_CRIT:
		case SWITCH_LOG_ALERT:
			raven_level = "fatal";
			break;
		default:
			raven_level = "debug";
			break;
		}

		status = raven_capture(node->userdata, node->data, raven_level, node->file, node->func, node->line);
	}

	return status;
}

static switch_status_t load_config(void)
{
	char *cf = "raven.conf";
	switch_xml_t cfg, xml, settings, param;

	globals.log_level = SWITCH_LOG_WARNING;
	globals.log_uuid = SWITCH_TRUE;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_FALSE;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "uri")) {
				set_global_uri(val);
			} else if (!strcmp(var, "key")) {
				set_global_key(val);
			} else if (!strcmp(var, "secret")) {
				set_global_secret(val);
			} else if (!strcmp(var, "project")) {
				set_global_project(val);
			} else if (!strcasecmp(var, "loglevel") && !zstr(val)) {
				globals.log_level = switch_log_str2level(val);
				if (globals.log_level == SWITCH_LOG_INVALID) {
					globals.log_level = SWITCH_LOG_WARNING;
				}
			} else if (!strcasecmp(var, "uuid")) {
				globals.log_uuid = switch_true(val);
			}
		}
	}
	switch_xml_free(xml);

	if (zstr(globals.uri) || zstr(globals.project) || zstr(globals.key) || zstr(globals.secret)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing parameter\n");
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_raven_load)
{
	switch_status_t status;
	*module_interface = &raven_module_interface;

	memset(&globals, 0, sizeof(globals));

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	switch_log_bind_logger(mod_raven_logger, globals.log_level, SWITCH_FALSE);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_raven_shutdown)
{
	switch_safe_free(globals.uri);
	switch_safe_free(globals.key);
	switch_safe_free(globals.secret);
	switch_safe_free(globals.project);

	switch_log_unbind_logger(mod_raven_logger);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
