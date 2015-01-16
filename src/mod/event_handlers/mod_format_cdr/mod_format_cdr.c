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
 * Brian West <brian@freeswitch.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Justin Cassidy <xachenant@hotmail.com>
 *
 * mod_format_cdr.c -- XML CDR Module to files or curl
 *
 */
#include <switch.h>
#include <sys/stat.h>
#include <switch_curl.h>
#define MAX_URLS 20
#define MAX_ERR_DIRS 20

#define ENCODING_NONE 0
#define ENCODING_DEFAULT 1
#define ENCODING_BASE64 2
#define ENCODING_TEXTXML 3
#define ENCODING_APPLJSON 4

static struct {
	switch_hash_t *profile_hash;
	switch_memory_pool_t *pool;
	switch_event_node_t *node;
	switch_mutex_t *mutex;
	uint32_t shutdown;
} globals;

struct cdr_profile {
	char *name;
	char *format;
	char *cred;
	char *urls[MAX_URLS + 1];
	int url_count;
	int url_index;
	switch_thread_rwlock_t *log_path_lock;
	char *base_log_dir;
	char *base_err_log_dir[MAX_ERR_DIRS];
	char *log_dir;
	char *err_log_dir[MAX_ERR_DIRS];
	int err_dir_count;
	char *log_file;
	uint32_t delay;
	uint32_t retries;
	uint32_t enable_cacert_check;
	char *ssl_cert_file;
	char *ssl_key_file;
	char *ssl_key_password;
	char *ssl_version;
	char *ssl_cacert_file;
	uint32_t enable_ssl_verifyhost;
	int encode;
	int encode_values;
	int log_http_and_disk;
	int log_b;
	int prefix_a;
	int disable100continue;
	int rotate;
	long auth_scheme;
	int timeout;
	switch_memory_pool_t *pool;
};
typedef struct cdr_profile cdr_profile_t;

SWITCH_MODULE_LOAD_FUNCTION(mod_format_cdr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_format_cdr_shutdown);
SWITCH_MODULE_DEFINITION(mod_format_cdr, mod_format_cdr_load, mod_format_cdr_shutdown, NULL);

/* this function would have access to the HTML returned by the webserver, we don't need it 
 * and the default curl activity is to print to stdout, something not as desirable
 * so we have a dummy function here
 */
static size_t httpCallBack(char *buffer, size_t size, size_t nitems, void *outstream)
{
	return size * nitems;
}

static switch_status_t set_format_cdr_log_dirs(cdr_profile_t *profile)
{
	switch_time_exp_t tm;
	char *path = NULL;
	char date[80] = "";
	switch_size_t retsize;
	switch_status_t status = SWITCH_STATUS_SUCCESS, dir_status;
	int err_dir_index;

	switch_time_exp_lt(&tm, switch_micro_time_now());
	switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d-%H-%M-%S", &tm);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Rotating log file paths\n");

	if (!zstr(profile->base_log_dir)) {
		if (profile->rotate) {
			if ((path = switch_mprintf("%s%s%s", profile->base_log_dir, SWITCH_PATH_SEPARATOR, date))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Rotating log file path to %s\n", path);

				dir_status = SWITCH_STATUS_SUCCESS;
				if (switch_directory_exists(path, profile->pool) != SWITCH_STATUS_SUCCESS) {
					dir_status = switch_dir_make(path, SWITCH_FPROT_OS_DEFAULT, profile->pool);
				}

				if (dir_status == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_wrlock(profile->log_path_lock);
					switch_safe_free(profile->log_dir);
					profile->log_dir = path;
					switch_thread_rwlock_unlock(profile->log_path_lock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new mod_format_cdr log_dir path\n");
					switch_safe_free(path);
					status = SWITCH_STATUS_FALSE;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to generate new mod_format_cdr log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Setting log file path to %s\n", profile->base_log_dir);
			if ((path = switch_safe_strdup(profile->base_log_dir))) {
				switch_thread_rwlock_wrlock(profile->log_path_lock);
				switch_safe_free(profile->log_dir);
				switch_dir_make_recursive(path, SWITCH_DEFAULT_DIR_PERMS, profile->pool);
				profile->log_dir = path;
				switch_thread_rwlock_unlock(profile->log_path_lock);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		}
	}

	for (err_dir_index = 0; err_dir_index < profile->err_dir_count; err_dir_index++) {
		if (profile->rotate) {
			if ((path = switch_mprintf("%s%s%s", profile->base_err_log_dir[err_dir_index], SWITCH_PATH_SEPARATOR, date))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Rotating err log file path to %s\n", path);

				dir_status = SWITCH_STATUS_SUCCESS;
				if (switch_directory_exists(path, profile->pool) != SWITCH_STATUS_SUCCESS) {
					dir_status = switch_dir_make(path, SWITCH_FPROT_OS_DEFAULT, profile->pool);
				}

				if (dir_status == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_wrlock(profile->log_path_lock);
					switch_safe_free(profile->err_log_dir[err_dir_index]);
					profile->err_log_dir[err_dir_index] = path;
					switch_thread_rwlock_unlock(profile->log_path_lock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new mod_format_cdr err_log_dir path\n");
					switch_safe_free(path);
					status = SWITCH_STATUS_FALSE;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to generate new mod_format_cdr err_log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Setting err log file path to %s\n", profile->base_err_log_dir[err_dir_index]);
			if ((path = switch_safe_strdup(profile->base_err_log_dir[err_dir_index]))) {
				switch_thread_rwlock_wrlock(profile->log_path_lock);
				switch_safe_free(profile->err_log_dir[err_dir_index]);
				switch_dir_make_recursive(path, SWITCH_DEFAULT_DIR_PERMS, profile->pool);
				profile->err_log_dir[err_dir_index] = path;
				switch_thread_rwlock_unlock(profile->log_path_lock);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set err_log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		}
	}

	return status;
}

static switch_status_t my_on_reporting_cb(switch_core_session_t *session, cdr_profile_t *profile)
{
	switch_xml_t xml_cdr = NULL;
	cJSON *json_cdr = NULL;
	char *cdr_text = NULL;
	char *lfile = NULL;
	char *dpath = NULL;
	char *path = NULL;
	char *curl_cdr_text = NULL;
	const char *logdir = NULL;
	char *cdr_text_escaped = NULL;
	int fd = -1;
	uint32_t cur_try;
	long httpRes;
	switch_CURL *curl_handle = NULL;
	switch_curl_slist_t *headers = NULL;
	switch_curl_slist_t *slist = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_FALSE;
	int is_b;
	const char *a_prefix = "";

	if (globals.shutdown) {
		return SWITCH_STATUS_SUCCESS;
	}

	is_b = channel && switch_channel_get_originator_caller_profile(channel);
	if (!profile->log_b && is_b) {
		const char *force_cdr = switch_channel_get_variable(channel, SWITCH_FORCE_PROCESS_CDR_VARIABLE);
		if (!switch_true(force_cdr)) {
			return SWITCH_STATUS_SUCCESS;
		}
	}
	if (!is_b && profile->prefix_a)
		a_prefix = "a_";

	if ( ! strcasecmp(profile->format, "json") ) {
		if (switch_ivr_generate_json_cdr(session, &json_cdr, profile->encode_values == ENCODING_DEFAULT) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Generating JSON Data!\n");
			return SWITCH_STATUS_FALSE;
		}

		/* build the JSON */
		cdr_text = cJSON_PrintUnformatted(json_cdr);

		if (!cdr_text) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error generating JSON!\n");
			goto error;
		}
	} else if ( ! strcasecmp(profile->format, "xml") ) {
		if (switch_ivr_generate_xml_cdr(session, &xml_cdr) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Generating XML Data!\n");
			return SWITCH_STATUS_FALSE;
		}

		/* build the XML */
		cdr_text = switch_xml_toxml(xml_cdr, SWITCH_TRUE);
		if (!cdr_text) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error generating XML!\n");
			goto error;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unhandled format for mod_format_cdr!\n");
		goto error;
	}

	switch_thread_rwlock_rdlock(profile->log_path_lock);

	if (!(logdir = switch_channel_get_variable(channel, "format_cdr_base"))) {
		logdir = profile->log_dir;
	}

	if (!zstr(logdir) && (profile->log_http_and_disk || !profile->url_count)) {
		if (profile->log_file) {
			lfile = switch_channel_expand_variables(channel, profile->log_file);
		} else {
			lfile = switch_mprintf("%s%s.cdr.%s", a_prefix, switch_core_session_get_uuid(session), profile->format);
		}
		dpath = switch_mprintf("%s%s%s", logdir, SWITCH_PATH_SEPARATOR, a_prefix);
		path = switch_mprintf("%s%s%s", logdir, SWITCH_PATH_SEPARATOR, lfile);
		if (lfile != profile->log_file) switch_safe_free(lfile);
		switch_thread_rwlock_unlock(profile->log_path_lock);
		if (path) {
			if (switch_directory_exists(dpath, profile->pool) != SWITCH_STATUS_SUCCESS) {
				switch_dir_make_recursive(dpath, SWITCH_FPROT_OS_DEFAULT, profile->pool);
			}
#ifdef _MSC_VER
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
#else
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) > -1) {
#endif
				int wrote;
				wrote = write(fd, cdr_text, (unsigned) strlen(cdr_text));
				if (!strcasecmp(profile->format, "json")) {
					wrote += write(fd, "\n", 1);
				}
				wrote++;
				close(fd);
				fd = -1;
			} else {
				char ebuf[512] = { 0 };
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error writing [%s][%s]\n",
						path, switch_strerror_r(errno, ebuf, sizeof(ebuf)));
			}
			switch_safe_free(path);
			switch_safe_free(dpath);
		}
	} else {
		switch_thread_rwlock_unlock(profile->log_path_lock);
	}

	/* try to post it to the web server */
	if (profile->url_count) {
		char *destUrl = NULL;
		curl_handle = switch_curl_easy_init();

		if (profile->encode == ENCODING_TEXTXML) {
			headers = switch_curl_slist_append(headers, "Content-Type: text/xml");
		} else if (profile->encode == ENCODING_APPLJSON) {
			headers = switch_curl_slist_append(headers, "Content-Type: application/json");
		} else if (profile->encode) {
			switch_size_t need_bytes = strlen(cdr_text) * 3 + 1;

			cdr_text_escaped = malloc(need_bytes);
			switch_assert(cdr_text_escaped);
			memset(cdr_text_escaped, 0, need_bytes);
			if (profile->encode == ENCODING_DEFAULT) {
				headers = switch_curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
				switch_url_encode(cdr_text, cdr_text_escaped, need_bytes);
			} else {
				headers = switch_curl_slist_append(headers, "Content-Type: application/x-www-form-base64-encoded");
				switch_b64_encode((unsigned char *) cdr_text, need_bytes / 3, (unsigned char *) cdr_text_escaped, need_bytes);
			}
			switch_safe_free(cdr_text);
			cdr_text = cdr_text_escaped;
		} else {
			headers = switch_curl_slist_append(headers, "Content-Type: application/x-www-form-plaintext");
		}

		if (profile->encode == ENCODING_TEXTXML) {
			curl_cdr_text = cdr_text;
		} else if (profile->encode == ENCODING_APPLJSON) {
			curl_cdr_text = cdr_text;
		} else if (!(curl_cdr_text = switch_mprintf("cdr=%s", cdr_text))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			goto error;
		}

		if (!zstr(profile->cred)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, profile->auth_scheme);
			switch_curl_easy_setopt(curl_handle, CURLOPT_USERPWD, profile->cred);
		}

		switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
		switch_curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, curl_cdr_text);
		switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-format-cdr/1.0");
		switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, httpCallBack);

		if (profile->disable100continue) {
			slist = switch_curl_slist_append(slist, "Expect:");
			switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, slist);
		}

		if (!zstr(profile->ssl_cert_file)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSLCERT, profile->ssl_cert_file);
		}

		if (!zstr(profile->ssl_key_file)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSLKEY, profile->ssl_key_file);
		}

		if (!zstr(profile->ssl_key_password)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSLKEYPASSWD, profile->ssl_key_password);
		}

		if (!zstr(profile->ssl_version)) {
			if (!strcasecmp(profile->ssl_version, "SSLv3")) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
			} else if (!strcasecmp(profile->ssl_version, "TLSv1")) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
			}
		}

		if (!zstr(profile->ssl_cacert_file)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_CAINFO, profile->ssl_cacert_file);
		}
		
		switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, profile->timeout);

		/* these were used for testing, optionally they may be enabled if someone desires
		   switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1); // 302 recursion level
		 */

		for (cur_try = 0; cur_try < profile->retries; cur_try++) {
			if (cur_try > 0) {
				switch_yield(profile->delay * 1000000);
			}

			destUrl = switch_mprintf("%s?uuid=%s%s", profile->urls[profile->url_index], a_prefix, switch_core_session_get_uuid(session));
			switch_curl_easy_setopt(curl_handle, CURLOPT_URL, destUrl);

			if (!strncasecmp(destUrl, "https", 5)) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
			}

			if (profile->enable_cacert_check) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, TRUE);
			}

			if (profile->enable_ssl_verifyhost) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2);
			}

			switch_curl_easy_perform(curl_handle);
			switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
			switch_safe_free(destUrl);
			if (httpRes >= 200 && httpRes <= 299) {
				goto success;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Got error [%ld] posting to web server [%s]\n",
								  httpRes, profile->urls[profile->url_index]);
				profile->url_index++;
				switch_assert(profile->url_count <= MAX_URLS);
				if (profile->url_index >= profile->url_count) {
					profile->url_index = 0;
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Retry will be with url [%s]\n", profile->urls[profile->url_index]);
			}
		}
		switch_curl_easy_cleanup(curl_handle);
		switch_curl_slist_free_all(headers);
		switch_curl_slist_free_all(slist);
		slist = NULL;
		headers = NULL;
		curl_handle = NULL;

		/* if we are here the web post failed for some reason */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to post to web server, writing to file\n");

		switch_thread_rwlock_rdlock(profile->log_path_lock);
		if (profile->log_file) {
			lfile = switch_channel_expand_variables(channel, profile->log_file);
		} else {
			lfile = switch_mprintf("%s%s.cdr.%s", a_prefix, switch_core_session_get_uuid(session), profile->format);
		}
		dpath = switch_mprintf("%s%s%s", logdir, SWITCH_PATH_SEPARATOR, a_prefix);
		path = switch_mprintf("%s%s%s", logdir, SWITCH_PATH_SEPARATOR, lfile);
		if (lfile != profile->log_file) switch_safe_free(lfile);
		switch_thread_rwlock_unlock(profile->log_path_lock);
		if (path) {
			if (switch_directory_exists(dpath, profile->pool) != SWITCH_STATUS_SUCCESS) {
				switch_dir_make_recursive(dpath, SWITCH_FPROT_OS_DEFAULT, profile->pool);
			}
#ifdef _MSC_VER
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
#else
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) > -1) {
#endif
				int wrote;
				wrote = write(fd, cdr_text, (unsigned) strlen(cdr_text));
				if (!strcasecmp(profile->format, "json")) {
					wrote += write(fd, "\n", 1);
				}
				wrote++;
				close(fd);
				fd = -1;
			} else {
				char ebuf[512] = { 0 };
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error![%s]\n",
						switch_strerror_r(errno, ebuf, sizeof(ebuf)));
			}
			switch_safe_free(path);
			switch_safe_free(dpath);
		}
	}

  success:
	status = SWITCH_STATUS_SUCCESS;

  error:
	if (curl_handle) {
		switch_curl_easy_cleanup(curl_handle);
	}
	if (headers) {
		switch_curl_slist_free_all(headers);
	}
	if (slist) {
		switch_curl_slist_free_all(slist);
	}
	if (curl_cdr_text != cdr_text) {
		switch_safe_free(curl_cdr_text);
	}
	switch_safe_free(cdr_text);
	switch_safe_free(path);
	switch_safe_free(dpath);
	if ( xml_cdr )
	{
		switch_xml_free(xml_cdr);
	}
	
	if ( json_cdr )
	{
		cJSON_Delete(json_cdr);
	}

	return status;
}

static switch_status_t my_on_reporting(switch_core_session_t *session)
{
	switch_hash_index_t *hi;
	void *val;
	switch_status_t status, tmpstatus;

	status = SWITCH_STATUS_SUCCESS;

	for (hi = switch_core_hash_first(globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
		cdr_profile_t *profile;
		switch_core_hash_this(hi, NULL, NULL, &val);
		profile = (cdr_profile_t *) val;

		tmpstatus = my_on_reporting_cb(session, profile);
		if ( tmpstatus != SWITCH_STATUS_SUCCESS ) {
			status = tmpstatus;
		}
	}

	return status;
}


static void event_handler(switch_event_t *event)
{
	switch_hash_index_t *hi;
	void *val;

	const char *sig = switch_event_get_header(event, "Trapped-Signal");

	if (sig && !strcmp(sig, "HUP")) {
		for (hi = switch_core_hash_first(globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
			cdr_profile_t *profile;
			switch_core_hash_this(hi, NULL, NULL, &val);
			profile = (cdr_profile_t *) val;

			if (profile->rotate) {
				set_format_cdr_log_dirs(profile);
			}
		}
	}
}

static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ my_on_reporting
};

switch_status_t mod_format_cdr_load_profile_xml(switch_xml_t xprofile)
{
	switch_memory_pool_t *pool = NULL;
	cdr_profile_t *profile = NULL;
	switch_xml_t settings, param;
	char *profile_name = (char *) switch_xml_attr_soft(xprofile, "name");

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	profile = switch_core_alloc(pool, sizeof(cdr_profile_t));
	memset(profile, 0, sizeof(cdr_profile_t));

	profile->pool = pool;
	profile->name = switch_core_strdup(profile->pool, profile_name);

	profile->log_http_and_disk = 0;
	profile->log_b = 1;
	profile->disable100continue = 0;
	profile->auth_scheme = CURLAUTH_BASIC;

	switch_thread_rwlock_create(&profile->log_path_lock, pool);

	if ((settings = switch_xml_child(xprofile, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "cred") && !zstr(val)) {
				profile->cred = switch_core_strdup(profile->pool, val);
			} else if (!strcasecmp(var, "format") && !zstr(val)) {
				profile->format = switch_core_strdup(profile->pool, val);
			} else if (!strcasecmp(var, "url") && !zstr(val)) {
				if (profile->url_count >= MAX_URLS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "maximum urls configured!\n");
				} else {
					profile->urls[profile->url_count++] = switch_core_strdup(profile->pool, val);
				}
			} else if (!strcasecmp(var, "log-http-and-disk")) {
				profile->log_http_and_disk = switch_true(val);
			} else if (!strcasecmp(var, "timeout")) {
				int tmp = atoi(val);
				if (tmp >= 0) {
					profile->timeout = tmp;
				} else {
					profile->timeout = 0;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set a negative timeout!\n");
				}
			} else if (!strcasecmp(var, "delay") && !zstr(val)) {
				profile->delay = switch_atoui(val);
			} else if (!strcasecmp(var, "log-b-leg")) {
				profile->log_b = switch_true(val);
			} else if (!strcasecmp(var, "prefix-a-leg")) {
				profile->prefix_a = switch_true(val);
			} else if (!strcasecmp(var, "disable-100-continue") && switch_true(val)) {
				profile->disable100continue = 1;
			} else if (!strcasecmp(var, "encode") && !zstr(val)) {
				if (!strcasecmp(val, "base64")) {
					profile->encode = ENCODING_BASE64;
				} else if (!strcasecmp(val, "textxml")) {
					profile->encode = ENCODING_TEXTXML;
				} else if (!strcasecmp(val, "appljson")) {
					profile->encode = ENCODING_APPLJSON;
				} else {
					profile->encode = switch_true(val) ? ENCODING_DEFAULT : ENCODING_NONE;
				}
			} else if (!strcasecmp(var, "retries") && !zstr(val)) {
				profile->retries = switch_atoui(val);
			} else if (!strcasecmp(var, "rotate") && !zstr(val)) {
				profile->rotate = switch_true(val);
			} else if (!strcasecmp(var, "log-file") && !zstr(val)) {
				profile->log_file = switch_core_strdup(profile->pool, val);
			} else if (!strcasecmp(var, "log-dir")) {
				if (zstr(val)) {
					profile->base_log_dir = switch_core_sprintf(profile->pool, "%s%sformat_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
				} else {
					if (switch_is_file_path(val)) {
						profile->base_log_dir = switch_core_strdup(profile->pool, val);
					} else {
						profile->base_log_dir = switch_core_sprintf(profile->pool, "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, val);
					}
				}
			} else if (!strcasecmp(var, "err-log-dir")) {
				if (profile->err_dir_count >= MAX_ERR_DIRS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "maximum error directories configured!\n");
				} else {

					if (zstr(val)) {
						profile->base_err_log_dir[profile->err_dir_count++] = switch_core_sprintf(profile->pool, "%s%sformat_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
					} else {
						if (switch_is_file_path(val)) {
							profile->base_err_log_dir[profile->err_dir_count++] = switch_core_strdup(profile->pool, val);
						} else {
							profile->base_err_log_dir[profile->err_dir_count++] = switch_core_sprintf(profile->pool, "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, val);
						}
					}
					
				}
			} else if (!strcasecmp(var, "enable-cacert-check") && switch_true(val)) {
				profile->enable_cacert_check = 1;
			} else if (!strcasecmp(var, "ssl-cert-path")) {
				profile->ssl_cert_file = switch_core_strdup(profile->pool, val);
			} else if (!strcasecmp(var, "ssl-key-path")) {
				profile->ssl_key_file = switch_core_strdup(profile->pool, val);
			} else if (!strcasecmp(var, "ssl-key-password")) {
				profile->ssl_key_password = switch_core_strdup(profile->pool, val);
			} else if (!strcasecmp(var, "ssl-version")) {
				profile->ssl_version = switch_core_strdup(profile->pool, val);
			} else if (!strcasecmp(var, "ssl-cacert-file")) {
				profile->ssl_cacert_file = switch_core_strdup(profile->pool, val);
			} else if (!strcasecmp(var, "enable-ssl-verifyhost") && switch_true(val)) {
				profile->enable_ssl_verifyhost = 1;
			} else if (!strcasecmp(var, "auth-scheme")) {
				if (*val == '=') {
					profile->auth_scheme = 0;
					val++;
				}

				if (!strcasecmp(val, "basic")) {
					profile->auth_scheme |= CURLAUTH_BASIC;
				} else if (!strcasecmp(val, "digest")) {
					profile->auth_scheme |= CURLAUTH_DIGEST;
				} else if (!strcasecmp(val, "NTLM")) {
					profile->auth_scheme |= CURLAUTH_NTLM;
				} else if (!strcasecmp(val, "GSS-NEGOTIATE")) {
					profile->auth_scheme |= CURLAUTH_GSSNEGOTIATE;
				} else if (!strcasecmp(val, "any")) {
					profile->auth_scheme = (long)CURLAUTH_ANY;
				}
			} else if (!strcasecmp(var, "encode-values") && !zstr(val)) {
				profile->encode_values = switch_true(val) ? ENCODING_DEFAULT : ENCODING_NONE;
			}
		}
		
		if (!profile->err_dir_count) {
			if (!zstr(profile->base_log_dir)) {
				profile->base_err_log_dir[profile->err_dir_count++] = switch_core_strdup(profile->pool, profile->base_log_dir);
			} else {
				profile->base_err_log_dir[profile->err_dir_count++] = switch_core_sprintf(profile->pool, "%s%sformat_cdr", 
					SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
			}
		}
	}

	if (profile->retries && profile->delay == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Retries set but delay 0 setting to 5 seconds\n");
		profile->delay = 5;
	}

	if ( ! profile->format || (strcasecmp(profile->format,"json") && strcasecmp(profile->format,"xml")) )
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No valid format_cdr format specified, defaulting to xml.\n");
		profile->format = switch_core_strdup(profile->pool,"xml");
	}

	profile->retries++;

	switch_mutex_lock(globals.mutex);
    switch_core_hash_insert(globals.profile_hash, profile->name, profile);
    switch_mutex_unlock(globals.mutex);

	set_format_cdr_log_dirs(profile);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_format_cdr_load)
{
	char *cf = "format_cdr.conf";
	switch_xml_t cfg, xml, xprofiles, xprofile;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	/* test global state handlers */
	switch_core_add_state_handler(&state_handlers);

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));

	if (switch_event_bind_removable(modname, SWITCH_EVENT_TRAP, SWITCH_EVENT_SUBCLASS_ANY, 
			event_handler, NULL, &globals.node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	globals.pool = pool;

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);
    switch_core_hash_init(&globals.profile_hash);

	/* parse the config */
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_lock(globals.mutex);
	if ((xprofiles = switch_xml_child(cfg, "profiles"))) {
        for (xprofile = switch_xml_child(xprofiles, "profile"); xprofile; xprofile = xprofile->next) {
            char *profile_name = (char *) switch_xml_attr_soft(xprofile, "name");

            if (zstr(profile_name)) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                        "<profile> is missing name attribute\n");
                continue;
            }

			mod_format_cdr_load_profile_xml(xprofile);
		}
	}
	switch_xml_free(xml);
	switch_mutex_unlock(globals.mutex);

	return status;
}

void mod_format_cdr_profile_shutdown(cdr_profile_t *profile)
{
	int err_dir_index = 0;

	for (err_dir_index = 0; err_dir_index < profile->err_dir_count; err_dir_index++) {
		switch_safe_free(profile->err_log_dir[err_dir_index]);
	}

	switch_safe_free(profile->log_dir);
	
	switch_thread_rwlock_destroy(profile->log_path_lock);

	switch_core_destroy_memory_pool(&profile->pool);
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_format_cdr_shutdown)
{
	switch_hash_index_t *hi;
	void *val;

	globals.shutdown = 1;

	switch_event_unbind(&globals.node);
	switch_core_remove_state_handler(&state_handlers);

	for (hi = switch_core_hash_first(globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
		cdr_profile_t *profile;
		switch_core_hash_this(hi, NULL, NULL, &val);
		profile = (cdr_profile_t *) val;

		if ( profile ) {
			mod_format_cdr_profile_shutdown(profile);
		}
	}

	switch_core_hash_destroy(&globals.profile_hash);

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
