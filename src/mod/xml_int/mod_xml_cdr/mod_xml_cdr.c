/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
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
#include <sys/stat.h>
#include <switch.h>
#include <curl/curl.h>

static struct {
	char *cred;
	char *url;
	char *log_dir;
	char *err_log_dir;
	uint32_t delay;
	uint32_t retries;
	uint32_t shutdown;
	uint32_t ignore_cacert_check;
	int encode;
	int log_b;
	int disable100continue;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_cdr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_cdr_shutdown);
SWITCH_MODULE_DEFINITION(mod_xml_cdr, mod_xml_cdr_load, mod_xml_cdr_shutdown, NULL);

/* this function would have access to the HTML returned by the webserver, we dont need it 
 * and the default curl activity is to print to stdout, something not as desirable
 * so we have a dummy function here
 */
static size_t httpCallBack(char *buffer, size_t size, size_t nitems, void *outstream)
{
	return size * nitems;
}

static switch_status_t my_on_hangup(switch_core_session_t *session)
{
	switch_xml_t cdr;
	char *xml_text = NULL;
	char *path = NULL;
	char *curl_xml_text = NULL;
	const char *logdir = NULL;
	char *xml_text_escaped = NULL;
	int fd = -1;
	uint32_t cur_try;
	long httpRes;
	CURL *curl_handle = NULL;
	struct curl_slist *headers = NULL;
	struct curl_slist *slist = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (globals.shutdown) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!globals.log_b && channel && switch_channel_get_originator_caller_profile(channel)) {
		return SWITCH_STATUS_SUCCESS;
	}

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

	if (!(logdir = switch_channel_get_variable(channel, "xml_cdr_base"))) {
		logdir = globals.log_dir;
	}

	if (!switch_strlen_zero(logdir)) {
		if ((path = switch_mprintf("%s%s%s.cdr.xml", logdir, SWITCH_PATH_SEPARATOR, switch_core_session_get_uuid(session)))) {
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
				int wrote;
				wrote = write(fd, xml_text, (unsigned) strlen(xml_text));
				close(fd);
				fd = -1;
			} else {
				char ebuf[512] = { 0 };
#ifdef WIN32
				strerror_s(ebuf, sizeof(ebuf), errno);
#else
				strerror_r(errno, ebuf, sizeof(ebuf));
#endif
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error writing [%s][%s]\n", path, ebuf);
			}
			switch_safe_free(path);
		}
	}

	/* try to post it to the web server */
	if (!switch_strlen_zero(globals.url)) {
		curl_handle = curl_easy_init();

		if (globals.encode) {
			switch_size_t need_bytes = strlen(xml_text) * 3;

			xml_text_escaped = malloc(need_bytes);
			switch_assert(xml_text_escaped);
			memset(xml_text_escaped, 0, need_bytes);
			if (globals.encode == 1) {
				headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
				switch_url_encode(xml_text, xml_text_escaped, need_bytes);
			} else {
				headers = curl_slist_append(headers, "Content-Type: application/x-www-form-base64-encoded");
				switch_b64_encode((unsigned char *) xml_text, need_bytes / 3, (unsigned char *) xml_text_escaped, need_bytes);
			}
			switch_safe_free(xml_text);
			xml_text = xml_text_escaped;
		} else {
			headers = curl_slist_append(headers, "Content-Type: application/x-www-form-plaintext");
		}

		if (!strncasecmp(globals.url, "https", 5)) {
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}

		if (!(curl_xml_text = switch_mprintf("cdr=%s", xml_text))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			goto error;
		}

		if (!switch_strlen_zero(globals.cred)) {
			curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
			curl_easy_setopt(curl_handle, CURLOPT_USERPWD, globals.cred);
		}

		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, curl_xml_text);
		curl_easy_setopt(curl_handle, CURLOPT_URL, globals.url);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-xml/1.0");
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, httpCallBack);

		if (globals.disable100continue) {
			slist = curl_slist_append(slist, "Expect:");
			curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, slist);
		}

		if (globals.ignore_cacert_check) {
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, FALSE);
		}

		/* these were used for testing, optionally they may be enabled if someone desires
		   curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 120); // tcp timeout
		   curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1); // 302 recursion level
		 */

		for (cur_try = 0; cur_try < globals.retries; cur_try++) {
			if (cur_try > 0) {
				switch_yield(globals.delay * 1000000);
			}
			curl_easy_perform(curl_handle);
			curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
			if (httpRes == 200) {
				goto success;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Got error [%ld] posting to web server [%s]\n", httpRes, globals.url);
			}
		}
		curl_easy_cleanup(curl_handle);
		curl_slist_free_all(headers);
		curl_slist_free_all(slist);
		slist = NULL;
		headers = NULL;
		curl_handle = NULL;

		/* if we are here the web post failed for some reason */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to post to web server, writing to file\n");

		if ((path = switch_mprintf("%s%s%s.cdr.xml", globals.err_log_dir, SWITCH_PATH_SEPARATOR, switch_core_session_get_uuid(session)))) {
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
				int wrote;
				wrote = write(fd, xml_text, (unsigned) strlen(xml_text));
				close(fd);
				fd = -1;
			} else {
				char ebuf[512] = { 0 };
#ifdef WIN32
				strerror_s(ebuf, sizeof(ebuf), errno);
#else
				strerror_r(errno, ebuf, sizeof(ebuf));
#endif
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error![%s]\n", ebuf);
			}
		}
	}

  success:
	status = SWITCH_STATUS_SUCCESS;

  error:
	if (curl_handle) {
		curl_easy_cleanup(curl_handle);
	}
	if (headers) {
		curl_slist_free_all(headers);
	}
	if (slist) {
		curl_slist_free_all(slist);
	}
	switch_safe_free(curl_xml_text);
	switch_safe_free(xml_text);
	switch_safe_free(path);
	switch_xml_free(cdr);

	return status;
}

static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL
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
	globals.log_b = 1;
	globals.disable100continue = 0;

	/* parse the config */
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "cred") && !switch_strlen_zero(val)) {
				globals.cred = strdup(val);
			} else if (!strcasecmp(var, "url") && !switch_strlen_zero(val)) {
				globals.url = strdup(val);
			} else if (!strcasecmp(var, "delay") && !switch_strlen_zero(val)) {
				globals.delay = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "log-b-leg")) {
				globals.log_b = switch_true(val);
			} else if (!strcasecmp(var, "disable-100-continue") && switch_true(val)) {
				globals.disable100continue = 1;
			} else if (!strcasecmp(var, "encode") && !switch_strlen_zero(val)) {
				if (!strcasecmp(val, "base64")) {
					globals.encode = 2;
				} else {
					globals.encode = switch_true(val) ? 1 : 0;
				}
			} else if (!strcasecmp(var, "retries") && !switch_strlen_zero(val)) {
				globals.retries = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "log-dir")) {
				if (switch_strlen_zero(val)) {
					globals.log_dir = switch_mprintf("%s%sxml_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
				} else {
					if (switch_is_file_path(val)) {
						globals.log_dir = strdup(val);
					} else {
						globals.log_dir = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, val);
					}
				}
			} else if (!strcasecmp(var, "err-log-dir")) {
				if (switch_strlen_zero(val)) {
					globals.err_log_dir = switch_mprintf("%s%sxml_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
				} else {
					if (switch_is_file_path(val)) {
						globals.err_log_dir = strdup(val);
					} else {
						globals.err_log_dir = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, val);
					}
				}
			} else if (!strcasecmp(var, "ignore-cacert-check") && switch_true(val)) {
				globals.ignore_cacert_check = 1;
			}

			if (switch_strlen_zero(globals.err_log_dir)) {
				if (!switch_strlen_zero(globals.log_dir)) {
					globals.err_log_dir = strdup(globals.log_dir);
				} else {
					globals.err_log_dir = switch_mprintf("%s%sxml_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
				}
			}
		}
	}
	if (globals.retries < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "retries is negative, setting to 0\n");
		globals.retries = 0;
	}

	if (globals.retries && globals.delay <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "retries set but delay 0 setting to 5000ms\n");
		globals.delay = 5000;
	}

	globals.retries++;

	switch_xml_free(xml);
	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_cdr_shutdown)
{

	globals.shutdown = 1;
	
	switch_core_remove_state_handler(&state_handlers);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
