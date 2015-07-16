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
 * mod_xml_cdr.c -- XML CDR Module to files or curl
 *
 */
#include <switch.h>
#include <sys/stat.h>
#include <switch_curl.h>
#define MAX_URLS 20

#define ENCODING_NONE 0
#define ENCODING_DEFAULT 1
#define ENCODING_BASE64 2
#define ENCODING_TEXTXML 3

static struct {
	char *cred;
	char *urls[MAX_URLS + 1];
	int url_count;
	int url_index;
	switch_thread_rwlock_t *log_path_lock;
	switch_mutex_t *url_index_mutex;
	char *base_log_dir;
	char *base_err_log_dir;
	char *log_dir;
	char *err_log_dir;
	uint32_t delay;
	uint32_t retries;
	uint32_t shutdown;
	uint32_t enable_cacert_check;
	char *ssl_cert_file;
	char *ssl_key_file;
	char *ssl_key_password;
	char *ssl_version;
	char *ssl_cacert_file;
	uint32_t enable_ssl_verifyhost;
	int encode;
	int log_http_and_disk;
	int log_b;
	int prefix_a;
	int disable100continue;
	int rotate;
	long auth_scheme;
	int timeout;
	switch_memory_pool_t *pool;
	switch_event_node_t *node;
	char *cookie_file;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_cdr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_cdr_shutdown);
SWITCH_MODULE_DEFINITION(mod_xml_cdr, mod_xml_cdr_load, mod_xml_cdr_shutdown, NULL);

/* this function would have access to the HTML returned by the webserver, we don't need it 
 * and the default curl activity is to print to stdout, something not as desirable
 * so we have a dummy function here
 */
static size_t httpCallBack(char *buffer, size_t size, size_t nitems, void *outstream)
{
	return size * nitems;
}

static switch_status_t set_xml_cdr_log_dirs()
{
	switch_time_exp_t tm;
	char *path = NULL;
	char date[80] = "";
	switch_size_t retsize;
	switch_status_t status = SWITCH_STATUS_SUCCESS, dir_status;

	switch_time_exp_lt(&tm, switch_micro_time_now());
	switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d-%H-%M-%S", &tm);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Rotating log file paths\n");

	if (!zstr(globals.base_log_dir)) {
		if (globals.rotate) {
			if ((path = switch_mprintf("%s%s%s", globals.base_log_dir, SWITCH_PATH_SEPARATOR, date))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Rotating log file path to %s\n", path);

				dir_status = SWITCH_STATUS_SUCCESS;
				if (switch_directory_exists(path, globals.pool) != SWITCH_STATUS_SUCCESS) {
					dir_status = switch_dir_make(path, SWITCH_FPROT_OS_DEFAULT, globals.pool);
				}

				if (dir_status == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_wrlock(globals.log_path_lock);
					switch_safe_free(globals.log_dir);
					globals.log_dir = path;
					switch_thread_rwlock_unlock(globals.log_path_lock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new mod_xml_cdr log_dir path\n");
					switch_safe_free(path);
					status = SWITCH_STATUS_FALSE;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to generate new mod_xml_cdr log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Setting log file path to %s\n", globals.base_log_dir);
			if ((path = switch_safe_strdup(globals.base_log_dir))) {
				switch_thread_rwlock_wrlock(globals.log_path_lock);
				switch_safe_free(globals.log_dir);
				switch_dir_make_recursive(path, SWITCH_DEFAULT_DIR_PERMS, globals.pool);
				globals.log_dir = path;
				switch_thread_rwlock_unlock(globals.log_path_lock);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		}
	}

	if (!zstr(globals.base_err_log_dir)) {
		if (globals.rotate) {
			if ((path = switch_mprintf("%s%s%s", globals.base_err_log_dir, SWITCH_PATH_SEPARATOR, date))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Rotating err log file path to %s\n", path);

				dir_status = SWITCH_STATUS_SUCCESS;
				if (switch_directory_exists(path, globals.pool) != SWITCH_STATUS_SUCCESS) {
					dir_status = switch_dir_make(path, SWITCH_FPROT_OS_DEFAULT, globals.pool);
				}

				if (dir_status == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_wrlock(globals.log_path_lock);
					switch_safe_free(globals.err_log_dir);
					globals.err_log_dir = path;
					switch_thread_rwlock_unlock(globals.log_path_lock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new mod_xml_cdr err_log_dir path\n");
					switch_safe_free(path);
					status = SWITCH_STATUS_FALSE;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to generate new mod_xml_cdr err_log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Setting err log file path to %s\n", globals.base_err_log_dir);
			if ((path = switch_safe_strdup(globals.base_err_log_dir))) {
				switch_thread_rwlock_wrlock(globals.log_path_lock);
				switch_safe_free(globals.err_log_dir);
				switch_dir_make_recursive(path, SWITCH_DEFAULT_DIR_PERMS, globals.pool);
				globals.err_log_dir = path;
				switch_thread_rwlock_unlock(globals.log_path_lock);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set err_log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		}
	}

	return status;
}

static switch_status_t my_on_reporting(switch_core_session_t *session)
{
	switch_xml_t cdr = NULL;
	char *xml_text = NULL;
	char *path = NULL;
	char *curl_xml_text = NULL;
	const char *logdir = NULL;
	char *xml_text_escaped = NULL;
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
	char url_joiner = '?';

	if (globals.shutdown) {
		return SWITCH_STATUS_SUCCESS;
	}

	is_b = channel && switch_channel_get_originator_caller_profile(channel);
	if (!globals.log_b && is_b) {
		const char *force_cdr = switch_channel_get_variable(channel, SWITCH_FORCE_PROCESS_CDR_VARIABLE);
		if (!switch_true(force_cdr)) {
			return SWITCH_STATUS_SUCCESS;
		}
	}
	if (!is_b && globals.prefix_a)
		a_prefix = "a_";

	if (switch_ivr_generate_xml_cdr(session, &cdr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Generating Data!\n");
		return SWITCH_STATUS_FALSE;
	}

	/* build the XML */
	xml_text = switch_xml_toxml(cdr, SWITCH_TRUE);
	if (!xml_text) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		goto error;
	}

	switch_thread_rwlock_rdlock(globals.log_path_lock);

	if (!(logdir = switch_channel_get_variable(channel, "xml_cdr_base"))) {
		logdir = globals.log_dir;
	}

	if (!zstr(logdir) && (globals.log_http_and_disk || !globals.url_count)) {
		path = switch_mprintf("%s%s%s%s.cdr.xml", logdir, SWITCH_PATH_SEPARATOR, a_prefix, switch_core_session_get_uuid(session));
		switch_thread_rwlock_unlock(globals.log_path_lock);
		if (path) {
#ifdef _MSC_VER
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
#else
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) > -1) {
#endif
				int wrote;
				wrote = write(fd, xml_text, (unsigned) strlen(xml_text));
				wrote++;
				close(fd);
				fd = -1;
			} else {
				char ebuf[512] = { 0 };
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error writing [%s][%s]\n",
						path, switch_strerror_r(errno, ebuf, sizeof(ebuf)));
			}
			switch_safe_free(path);
		}
	} else {
		switch_thread_rwlock_unlock(globals.log_path_lock);
	}

	/* try to post it to the web server */
	if (globals.url_count) {
		char *destUrl = NULL;
		int g_url_index = -1;

		switch_mutex_lock(globals.url_index_mutex);
		g_url_index = globals.url_index;
		switch_mutex_unlock(globals.url_index_mutex);

		curl_handle = switch_curl_easy_init();

		if (globals.encode == ENCODING_TEXTXML) {
			headers = switch_curl_slist_append(headers, "Content-Type: text/xml");
		} else if (globals.encode) {
			switch_size_t need_bytes = strlen(xml_text) * 3 + 1;

			xml_text_escaped = malloc(need_bytes);
			switch_assert(xml_text_escaped);
			memset(xml_text_escaped, 0, need_bytes);
			if (globals.encode == ENCODING_DEFAULT) {
				headers = switch_curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
				switch_url_encode_opt(xml_text, xml_text_escaped, need_bytes, SWITCH_TRUE);
			} else {
				headers = switch_curl_slist_append(headers, "Content-Type: application/x-www-form-base64-encoded");
				switch_b64_encode((unsigned char *) xml_text, need_bytes / 3, (unsigned char *) xml_text_escaped, need_bytes);
			}
			switch_safe_free(xml_text);
			xml_text = xml_text_escaped;
		} else {
			headers = switch_curl_slist_append(headers, "Content-Type: application/x-www-form-plaintext");
		}

		if (globals.encode == ENCODING_TEXTXML) {
			curl_xml_text = xml_text;
		} else if (!(curl_xml_text = switch_mprintf("cdr=%s", xml_text))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			goto error;
		}

		if (!zstr(globals.cred)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, globals.auth_scheme);
			switch_curl_easy_setopt(curl_handle, CURLOPT_USERPWD, globals.cred);
		}

		switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
		switch_curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, curl_xml_text);
		switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-xml/1.0");
		switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, httpCallBack);

		if (globals.disable100continue) {
			slist = switch_curl_slist_append(slist, "Expect:");
			switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, slist);
		}

		if (globals.ssl_cert_file) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSLCERT, globals.ssl_cert_file);
		}

		if (globals.ssl_key_file) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSLKEY, globals.ssl_key_file);
		}

		if (globals.ssl_key_password) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSLKEYPASSWD, globals.ssl_key_password);
		}

		if (globals.ssl_version) {
			if (!strcasecmp(globals.ssl_version, "SSLv3")) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
			} else if (!strcasecmp(globals.ssl_version, "TLSv1")) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
			}
		}

		if (globals.ssl_cacert_file) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_CAINFO, globals.ssl_cacert_file);
		}
		
		if (globals.cookie_file) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_COOKIEJAR, globals.cookie_file);
			switch_curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, globals.cookie_file);
		}

		switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, globals.timeout);

		/* these were used for testing, optionally they may be enabled if someone desires
		   switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1); // 302 recursion level
		 */

		for (cur_try = 0; cur_try < globals.retries; cur_try++) {
			if (cur_try > 0) {
				switch_yield(globals.delay * 1000000);
			}

			if( strchr(globals.urls[g_url_index], '?') != NULL ) {
				url_joiner = '&';
			}
			destUrl = switch_mprintf("%s%cuuid=%s%s", globals.urls[g_url_index], url_joiner, a_prefix, switch_core_session_get_uuid(session));
			switch_curl_easy_setopt(curl_handle, CURLOPT_URL, destUrl);

			if (!strncasecmp(destUrl, "https", 5)) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
			}

			if (globals.enable_cacert_check) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, TRUE);
			}

			if (globals.enable_ssl_verifyhost) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2);
			}

			/* overrides default 300s timeout, could be usefull if the current web server is down to prevent long time waiting for nothing */
			/* connection_timeout = retry_timeout  */
			switch_curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, !globals.delay ? 5 : (long)globals.delay);
			switch_curl_easy_perform(curl_handle);
			switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
			switch_safe_free(destUrl);
			if (httpRes >= 200 && httpRes <= 299) {
				goto success;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Got error [%ld] posting to web server [%s]\n",
								  httpRes, globals.urls[g_url_index]);
				g_url_index++;
				switch_assert(globals.url_count <= MAX_URLS);
				if (g_url_index >= globals.url_count) {
					g_url_index = 0;
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Retry will be with url [%s]\n", globals.urls[g_url_index]);
				switch_mutex_lock(globals.url_index_mutex);
				if (globals.url_index != g_url_index) {
					globals.url_index = g_url_index;
				}
				switch_mutex_unlock(globals.url_index_mutex);
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

		switch_thread_rwlock_rdlock(globals.log_path_lock);
		path = switch_mprintf("%s%s%s%s.cdr.xml", globals.err_log_dir, SWITCH_PATH_SEPARATOR, a_prefix, switch_core_session_get_uuid(session));
		switch_thread_rwlock_unlock(globals.log_path_lock);
		if (path) {
#ifdef _MSC_VER
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
#else
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) > -1) {
#endif
				int wrote;
				wrote = write(fd, xml_text, (unsigned) strlen(xml_text));
				wrote++;
				close(fd);
				fd = -1;
			} else {
				char ebuf[512] = { 0 };
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error![%s]\n",
						switch_strerror_r(errno, ebuf, sizeof(ebuf)));
			}
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
	if (curl_xml_text != xml_text) {
		switch_safe_free(curl_xml_text);
	}
	switch_safe_free(xml_text);
	switch_safe_free(path);
	switch_xml_free(cdr);

	return status;
}

static void event_handler(switch_event_t *event)
{
	const char *sig = switch_event_get_header(event, "Trapped-Signal");

	if (sig && !strcmp(sig, "HUP")) {
		if (globals.rotate) {
			set_xml_cdr_log_dirs();
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

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_cdr_load)
{
	char *cf = "xml_cdr.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	/* test global state handlers */
	switch_core_add_state_handler(&state_handlers);

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));

	if (switch_event_bind_removable(modname, SWITCH_EVENT_TRAP, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	globals.log_http_and_disk = 0;
	globals.log_b = 1;
	globals.disable100continue = 0;
	globals.pool = pool;
	globals.auth_scheme = CURLAUTH_BASIC;

	switch_thread_rwlock_create(&globals.log_path_lock, pool);
	switch_mutex_init(&globals.url_index_mutex, SWITCH_MUTEX_NESTED, globals.pool);

	/* parse the config */
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_FALSE;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "cred") && !zstr(val)) {
				globals.cred = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "url") && !zstr(val)) {
				if (globals.url_count >= MAX_URLS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "maximum urls configured!\n");
				} else {
					globals.urls[globals.url_count++] = switch_core_strdup(globals.pool, val);
				}
			} else if (!strcasecmp(var, "log-http-and-disk")) {
				globals.log_http_and_disk = switch_true(val);
			} else if (!strcasecmp(var, "timeout")) {
				int tmp = atoi(val);
				if (tmp >= 0) {
					globals.timeout = tmp;
				} else {
					globals.timeout = 0;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set a negative timeout!\n");
				}
			} else if (!strcasecmp(var, "delay") && !zstr(val)) {
				globals.delay = switch_atoui(val);
			} else if (!strcasecmp(var, "log-b-leg")) {
				globals.log_b = switch_true(val);
			} else if (!strcasecmp(var, "prefix-a-leg")) {
				globals.prefix_a = switch_true(val);
			} else if (!strcasecmp(var, "disable-100-continue") && switch_true(val)) {
				globals.disable100continue = 1;
			} else if (!strcasecmp(var, "encode") && !zstr(val)) {
				if (!strcasecmp(val, "base64")) {
					globals.encode = ENCODING_BASE64;
				} else if (!strcasecmp(val, "textxml")) {
					globals.encode = ENCODING_TEXTXML;
				} else {
					globals.encode = switch_true(val) ? ENCODING_DEFAULT : ENCODING_NONE;
				}
			} else if (!strcasecmp(var, "retries") && !zstr(val)) {
				globals.retries = switch_atoui(val);
			} else if (!strcasecmp(var, "rotate") && !zstr(val)) {
				globals.rotate = switch_true(val);
			} else if (!strcasecmp(var, "log-dir")) {
				if (zstr(val)) {
					globals.base_log_dir = switch_core_sprintf(globals.pool, "%s%sxml_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
				} else {
					if (switch_is_file_path(val)) {
						globals.base_log_dir = switch_core_strdup(globals.pool, val);
					} else {
						globals.base_log_dir = switch_core_sprintf(globals.pool, "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, val);
					}
				}
			} else if (!strcasecmp(var, "err-log-dir")) {
				if (zstr(val)) {
					globals.base_err_log_dir = switch_core_sprintf(globals.pool, "%s%sxml_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
				} else {
					if (switch_is_file_path(val)) {
						globals.base_err_log_dir = switch_core_strdup(globals.pool, val);
					} else {
						globals.base_err_log_dir = switch_core_sprintf(globals.pool, "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, val);
					}
				}
			} else if (!strcasecmp(var, "enable-cacert-check") && switch_true(val)) {
				globals.enable_cacert_check = 1;
			} else if (!strcasecmp(var, "ssl-cert-path")) {
				globals.ssl_cert_file = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "ssl-key-path")) {
				globals.ssl_key_file = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "ssl-key-password")) {
				globals.ssl_key_password = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "ssl-version")) {
				globals.ssl_version = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "ssl-cacert-file")) {
				globals.ssl_cacert_file = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "enable-ssl-verifyhost") && switch_true(val)) {
				globals.enable_ssl_verifyhost = 1;
			} else if (!strcasecmp(var, "auth-scheme")) {
				if (*val == '=') {
					globals.auth_scheme = 0;
					val++;
				}

				if (!strcasecmp(val, "basic")) {
					globals.auth_scheme |= CURLAUTH_BASIC;
				} else if (!strcasecmp(val, "digest")) {
					globals.auth_scheme |= CURLAUTH_DIGEST;
				} else if (!strcasecmp(val, "NTLM")) {
					globals.auth_scheme |= CURLAUTH_NTLM;
				} else if (!strcasecmp(val, "GSS-NEGOTIATE")) {
					globals.auth_scheme |= CURLAUTH_GSSNEGOTIATE;
				} else if (!strcasecmp(val, "any")) {
					globals.auth_scheme = (long)CURLAUTH_ANY;
				}
			} else if (!strcasecmp(var, "cookie-file")) {
				globals.cookie_file = switch_core_strdup(globals.pool, val);
			}
		}
		
		if (zstr(globals.base_err_log_dir)) {
			if (!zstr(globals.base_log_dir)) {
				globals.base_err_log_dir = switch_core_strdup(globals.pool, globals.base_log_dir);
			} else {
				globals.base_err_log_dir = switch_core_sprintf(globals.pool, "%s%sxml_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
			}
		}
		
	}

	if (globals.retries && globals.delay == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Retries set but delay 0 setting to 5 seconds\n");
		globals.delay = 5;
	}

	globals.retries++;

	set_xml_cdr_log_dirs();

	switch_xml_free(xml);

	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_cdr_shutdown)
{

	globals.shutdown = 1;

	switch_safe_free(globals.log_dir);
	switch_safe_free(globals.err_log_dir);

	switch_event_unbind(&globals.node);
	switch_core_remove_state_handler(&state_handlers);

	switch_thread_rwlock_destroy(globals.log_path_lock);

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
