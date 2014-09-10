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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Justin Cassidy <xachenant@hotmail.com>
 *
 * mod_xml_curl.c -- CURL XML Gateway
 *
 */
#include <switch.h>
#include <switch_curl.h>


SWITCH_MODULE_LOAD_FUNCTION(mod_xml_curl_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_curl_shutdown);
SWITCH_MODULE_DEFINITION(mod_xml_curl, mod_xml_curl_load, mod_xml_curl_shutdown, NULL);


struct xml_binding {
	char *method;
	char *url;
	char *bindings;
	char *cred;
	char *bind_local;
	int disable100continue;
	int use_get_style;
	uint32_t enable_cacert_check;
	char *ssl_cert_file;
	char *ssl_key_file;
	char *ssl_key_password;
	char *ssl_version;
	char *ssl_cacert_file;
	uint32_t enable_ssl_verifyhost;
	char *cookie_file;
	switch_hash_t *vars_map;
	int use_dynamic_url;
	long auth_scheme;
	int timeout;
};

static int keep_files_around = 0;

typedef struct xml_binding xml_binding_t;

#define XML_CURL_MAX_BYTES 1024 * 1024

struct config_data {
	char *name;
	int fd;
	switch_size_t bytes;
	switch_size_t max_bytes;
	int err;
};

typedef struct hash_node {
	switch_hash_t *hash;
	struct hash_node *next;
} hash_node_t;

static struct {
	switch_memory_pool_t *pool;
	hash_node_t *hash_root;
	hash_node_t *hash_tail;
} globals;

#define XML_CURL_SYNTAX "[debug_on|debug_off]"
SWITCH_STANDARD_API(xml_curl_function)
{
	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (zstr(cmd)) {
		goto usage;
	}

	if (!strcasecmp(cmd, "debug_on")) {
		keep_files_around = 1;
	} else if (!strcasecmp(cmd, "debug_off")) {
		keep_files_around = 0;
	} else {
		goto usage;
	}

	stream->write_function(stream, "OK\n");
	return SWITCH_STATUS_SUCCESS;

  usage:
	stream->write_function(stream, "USAGE: %s\n", XML_CURL_SYNTAX);
	return SWITCH_STATUS_SUCCESS;
}

static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	struct config_data *config_data = data;
	int x;

	config_data->bytes += realsize;

	if (config_data->bytes > config_data->max_bytes) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Oversized file detected [%d bytes]\n", (int) config_data->bytes);
		config_data->err = 1;
		return 0;
	}

	x = write(config_data->fd, ptr, realsize);
	if (x != (int) realsize) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Short write! %d out of %d\n", x, realsize);
	}
	return x;
}




static switch_xml_t xml_url_fetch(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
								  void *user_data)
{
	char filename[512] = "";
	switch_CURL *curl_handle = NULL;
	switch_CURLcode cc;
	struct config_data config_data;
	switch_xml_t xml = NULL;
	char *data = NULL;
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	xml_binding_t *binding = (xml_binding_t *) user_data;
	char *file_url;
	switch_curl_slist_t *slist = NULL;
	long httpRes = 0;
	switch_curl_slist_t *headers = NULL;
	char hostname[256] = "";
	char basic_data[512];
	char *uri = NULL;
	char *dynamic_url = NULL;

    strncpy(hostname, switch_core_get_switchname(), sizeof(hostname) - 1);

	if (!binding) {
		return NULL;
	}

	if ((file_url = strstr(binding->url, "file:"))) {
		file_url += 5;

		if (!(xml = switch_xml_parse_file(file_url))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing Result!\n");
		}

		return xml;
	}

	switch_snprintf(basic_data, sizeof(basic_data), "hostname=%s&section=%s&tag_name=%s&key_name=%s&key_value=%s",
					hostname, section, switch_str_nil(tag_name), switch_str_nil(key_name), switch_str_nil(key_value));

	data = switch_event_build_param_string(params, basic_data, binding->vars_map);
	switch_assert(data);

	if (binding->use_dynamic_url) {
		if (!params) {
			switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
			switch_assert(params);
		}

		switch_event_add_header_string(params, SWITCH_STACK_TOP, "hostname", hostname);
		switch_event_add_header_string(params, SWITCH_STACK_TOP, "section", switch_str_nil(section));
		switch_event_add_header_string(params, SWITCH_STACK_TOP, "tag_name", switch_str_nil(tag_name));
		switch_event_add_header_string(params, SWITCH_STACK_TOP, "key_name", switch_str_nil(key_name));
		switch_event_add_header_string(params, SWITCH_STACK_TOP, "key_value", switch_str_nil(key_value));
		dynamic_url = switch_event_expand_headers(params, binding->url);
		switch_assert(dynamic_url);
	} else {
		dynamic_url = binding->url;
	}

	if (binding->use_get_style == 1) {
		uri = malloc(strlen(data) + strlen(dynamic_url) + 16);
		switch_assert(uri);
		sprintf(uri, "%s%c%s", dynamic_url, strchr(dynamic_url, '?') != NULL ? '&' : '?', data);
	}

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	switch_snprintf(filename, sizeof(filename), "%s%s%s.tmp.xml", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, uuid_str);
	curl_handle = switch_curl_easy_init();
	headers = switch_curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

	if (!strncasecmp(binding->url, "https", 5)) {
		switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}

	memset(&config_data, 0, sizeof(config_data));

	config_data.name = filename;
	config_data.max_bytes = XML_CURL_MAX_BYTES;

	if ((config_data.fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
		if (!zstr(binding->cred)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, binding->auth_scheme);
			switch_curl_easy_setopt(curl_handle, CURLOPT_USERPWD, binding->cred);
		}
		switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
		if (binding->method != NULL)
			switch_curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, binding->method);
		switch_curl_easy_setopt(curl_handle, CURLOPT_POST, !binding->use_get_style);
		switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
		if (!binding->use_get_style)
			switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
		switch_curl_easy_setopt(curl_handle, CURLOPT_URL, binding->use_get_style ? uri : dynamic_url);
		switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
		switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &config_data);
		switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-xml/1.0");
		switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);

		if (binding->timeout) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, binding->timeout);
		}

		if (binding->disable100continue) {
			slist = switch_curl_slist_append(slist, "Expect:");
			switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, slist);
		}

		if (binding->enable_cacert_check) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, TRUE);
		}

		if (binding->ssl_cert_file) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSLCERT, binding->ssl_cert_file);
		}

		if (binding->ssl_key_file) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSLKEY, binding->ssl_key_file);
		}

		if (binding->ssl_key_password) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSLKEYPASSWD, binding->ssl_key_password);
		}

		if (binding->ssl_version) {
			if (!strcasecmp(binding->ssl_version, "SSLv3")) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
			} else if (!strcasecmp(binding->ssl_version, "TLSv1")) {
				switch_curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
			}
		}

		if (binding->ssl_cacert_file) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_CAINFO, binding->ssl_cacert_file);
		}

		if (binding->enable_ssl_verifyhost) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2);
		}

		if (binding->cookie_file) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_COOKIEJAR, binding->cookie_file);
			switch_curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, binding->cookie_file);
		}

		if (binding->bind_local) {
			curl_easy_setopt(curl_handle, CURLOPT_INTERFACE, binding->bind_local);
		}

		cc = switch_curl_easy_perform(curl_handle);
		if (cc && cc != CURLE_WRITE_ERROR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "CURL returned error:[%d] %s\n", cc, switch_curl_easy_strerror(cc));
		}

		switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
		switch_curl_easy_cleanup(curl_handle);
		switch_curl_slist_free_all(headers);
		switch_curl_slist_free_all(slist);
		close(config_data.fd);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening temp file!\n");
	}

	if (config_data.err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error encountered! [%s]\ndata: [%s]\n", binding->url, data);
		xml = NULL;
	} else {
		if (httpRes == 200) {
			if (!(xml = switch_xml_parse_file(filename))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing Result! [%s]\ndata: [%s]\n", binding->url, data);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received HTTP error %ld trying to fetch %s\ndata: [%s]\n", httpRes, binding->url,
							  data);
			xml = NULL;
		}
	}

	/* Debug by leaving the file behind for review */
	if (keep_files_around) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "XML response is in %s\n", filename);
	} else {
		if (unlink(filename) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "XML response file [%s] delete failed\n", filename);
		}
	}

	switch_safe_free(data);
	if (binding->use_get_style == 1)
		switch_safe_free(uri);
	if (binding->use_dynamic_url && dynamic_url != binding->url)
		switch_safe_free(dynamic_url);
	return xml;
}

#define ENABLE_PARAM_VALUE "enabled"
static switch_status_t do_config(void)
{
	char *cf = "xml_curl.conf";
	switch_xml_t cfg, xml, bindings_tag, binding_tag, param;
	xml_binding_t *binding = NULL;
	int x = 0;
	int need_vars_map = 0;
	switch_hash_t *vars_map = NULL;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if (!(bindings_tag = switch_xml_child(cfg, "bindings"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing <bindings> tag!\n");
		goto done;
	}

	for (binding_tag = switch_xml_child(bindings_tag, "binding"); binding_tag; binding_tag = binding_tag->next) {
		char *bname = (char *) switch_xml_attr_soft(binding_tag, "name");
		char *url = NULL;
		char *bind_local = NULL;
		char *bind_cred = NULL;
		char *bind_mask = NULL;
		char *method = NULL;
		int disable100continue = 1;
		int use_dynamic_url = 0, timeout = 0;
		uint32_t enable_cacert_check = 0;
		char *ssl_cert_file = NULL;
		char *ssl_key_file = NULL;
		char *ssl_key_password = NULL;
		char *ssl_version = NULL;
		char *ssl_cacert_file = NULL;
		uint32_t enable_ssl_verifyhost = 0;
		char *cookie_file = NULL;
		hash_node_t *hash_node;
		long auth_scheme = CURLAUTH_BASIC;
		need_vars_map = 0;
		vars_map = NULL;


		for (param = switch_xml_child(binding_tag, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "gateway-url")) {
				bind_mask = (char *) switch_xml_attr_soft(param, "bindings");
				if (val) {
					url = val;
				}
			} else if (!strcasecmp(var, "gateway-credentials")) {
				bind_cred = val;
			} else if (!strcasecmp(var, "auth-scheme")) {
				if (*val == '=') {
					auth_scheme = 0;
					val++;
				}

				if (!strcasecmp(val, "basic")) {
					auth_scheme |= CURLAUTH_BASIC;
				} else if (!strcasecmp(val, "digest")) {
					auth_scheme |= CURLAUTH_DIGEST;
				} else if (!strcasecmp(val, "NTLM")) {
					auth_scheme |= CURLAUTH_NTLM;
				} else if (!strcasecmp(val, "GSS-NEGOTIATE")) {
					auth_scheme |= CURLAUTH_GSSNEGOTIATE;
				} else if (!strcasecmp(val, "any")) {
					auth_scheme = (long)CURLAUTH_ANY;
				}
			} else if (!strcasecmp(var, "disable-100-continue") && !switch_true(val)) {
				disable100continue = 0;
			} else if (!strcasecmp(var, "method")) {
				method = val;
			} else if (!strcasecmp(var, "timeout")) {
				int tmp = atoi(val);
				if (tmp >= 0) {
					timeout = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set a negative timeout!\n");
				}
			} else if (!strcasecmp(var, "enable-cacert-check") && switch_true(val)) {
				enable_cacert_check = 1;
			} else if (!strcasecmp(var, "ssl-cert-path")) {
				ssl_cert_file = val;
			} else if (!strcasecmp(var, "ssl-key-path")) {
				ssl_key_file = val;
			} else if (!strcasecmp(var, "ssl-key-password")) {
				ssl_key_password = val;
			} else if (!strcasecmp(var, "ssl-version")) {
				ssl_version = val;
			} else if (!strcasecmp(var, "ssl-cacert-file")) {
				ssl_cacert_file = val;
			} else if (!strcasecmp(var, "enable-ssl-verifyhost") && switch_true(val)) {
				enable_ssl_verifyhost = 1;
			} else if (!strcasecmp(var, "cookie-file")) {
				cookie_file = val;
			} else if (!strcasecmp(var, "use-dynamic-url") && switch_true(val)) {
				use_dynamic_url = 1;
			} else if (!strcasecmp(var, "enable-post-var")) {
				if (!vars_map && need_vars_map == 0) {
					if (switch_core_hash_init(&vars_map) != SWITCH_STATUS_SUCCESS) {
						need_vars_map = -1;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't init params hash!\n");
						continue;
					}
					need_vars_map = 1;
				}

				if (vars_map && val) {
					if (switch_core_hash_insert(vars_map, val, ENABLE_PARAM_VALUE) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't add %s to params hash!\n", val);
					}
				}
			} else if (!strcasecmp(var, "bind-local")) {
				bind_local = val;
			}
		}

		if (!url) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Binding has no url!\n");
			if (vars_map)
				switch_core_hash_destroy(&vars_map);
			continue;
		}

		if (!(binding = switch_core_alloc(globals.pool, sizeof(*binding)))) {
			if (vars_map)
				switch_core_hash_destroy(&vars_map);
			goto done;
		}
		memset(binding, 0, sizeof(*binding));

		binding->auth_scheme = auth_scheme;
		binding->timeout = timeout;
		binding->url = switch_core_strdup(globals.pool, url);
		switch_assert(binding->url);

		if (bind_local != NULL) {
			binding->bind_local = switch_core_strdup(globals.pool, bind_local);
		}
		if (method != NULL) {
			binding->method = switch_core_strdup(globals.pool, method);
		} else {
			binding->method = NULL;
		}
		if (bind_mask) {
			binding->bindings = switch_core_strdup(globals.pool, bind_mask);
		}

		if (bind_cred) {
			binding->cred = switch_core_strdup(globals.pool, bind_cred);
		}

		binding->disable100continue = disable100continue;
		binding->use_get_style = method != NULL && strcasecmp(method, "post") != 0;
		binding->use_dynamic_url = use_dynamic_url;
		binding->enable_cacert_check = enable_cacert_check;

		if (ssl_cert_file) {
			binding->ssl_cert_file = switch_core_strdup(globals.pool, ssl_cert_file);
		}

		if (ssl_key_file) {
			binding->ssl_key_file = switch_core_strdup(globals.pool, ssl_key_file);
		}

		if (ssl_key_password) {
			binding->ssl_key_password = switch_core_strdup(globals.pool, ssl_key_password);
		}

		if (ssl_version) {
			binding->ssl_version = switch_core_strdup(globals.pool, ssl_version);
		}

		if (ssl_cacert_file) {
			binding->ssl_cacert_file = switch_core_strdup(globals.pool, ssl_cacert_file);
		}

		binding->enable_ssl_verifyhost = enable_ssl_verifyhost;

		if (cookie_file) {
			binding->cookie_file = switch_core_strdup(globals.pool, cookie_file);
		}

		binding->vars_map = vars_map;

		if (vars_map) {
			switch_zmalloc(hash_node, sizeof(hash_node_t));
			hash_node->hash = vars_map;
			hash_node->next = NULL;

			if (!globals.hash_root) {
				globals.hash_root = hash_node;
				globals.hash_tail = globals.hash_root;
			}

			else {
				globals.hash_tail->next = hash_node;
				globals.hash_tail = globals.hash_tail->next;
			}

		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Binding [%s] XML Fetch Function [%s] [%s]\n",
						  zstr(bname) ? "N/A" : bname, binding->url, binding->bindings ? binding->bindings : "all");
		switch_xml_bind_search_function(xml_url_fetch, switch_xml_parse_section_string(binding->bindings), binding);
		x++;
		binding = NULL;
	}

  done:
	switch_xml_free(xml);

	return x ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_curl_load)
{
	switch_api_interface_t *xml_curl_api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;
	globals.hash_root = NULL;
	globals.hash_tail = NULL;

	if (do_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	SWITCH_ADD_API(xml_curl_api_interface, "xml_curl", "XML Curl", xml_curl_function, XML_CURL_SYNTAX);
	switch_console_set_complete("add xml_curl debug_on");
	switch_console_set_complete("add xml_curl debug_off");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_curl_shutdown)
{
	hash_node_t *ptr = NULL;

	while (globals.hash_root) {
		ptr = globals.hash_root;
		switch_core_hash_destroy(&ptr->hash);
		globals.hash_root = ptr->next;
		switch_safe_free(ptr);
	}

	switch_xml_unbind_search_function_ptr(xml_url_fetch);

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
