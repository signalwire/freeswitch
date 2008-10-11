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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Justin Cassidy <xachenant@hotmail.com>
 *
 * mod_xml_curl.c -- CURL XML Gateway
 *
 */
#include <switch.h>
#include <curl/curl.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_curl_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_curl_shutdown);
SWITCH_MODULE_DEFINITION(mod_xml_curl, mod_xml_curl_load, mod_xml_curl_shutdown, NULL);

struct xml_binding {
	char *url;
	char *bindings;
	char *cred;
	int disable100continue;
	uint32_t ignore_cacert_check;
	switch_hash_t *vars_map;
};

static int keep_files_around = 0;

typedef struct xml_binding xml_binding_t;

struct config_data {
	char *name;
	int fd;
};

typedef struct hash_node {
    switch_hash_t* hash;
    struct hash_node* next;
} hash_node_t;

static struct {
    switch_memory_pool_t* pool;
    hash_node_t* hash_root;
    hash_node_t* hash_tail;
} globals;

#define XML_CURL_SYNTAX "[debug_on|debug_off]"
SWITCH_STANDARD_API(xml_curl_function)
{
	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_strlen_zero(cmd)) {
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
	CURL *curl_handle = NULL;
	struct config_data config_data;
	switch_xml_t xml = NULL;
	char *data = NULL;
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	xml_binding_t *binding = (xml_binding_t *) user_data;
	char *file_url;
	struct curl_slist *slist = NULL;
	long httpRes = 0;
	struct curl_slist *headers = NULL;
	char hostname[256] = "";
	char basic_data[512];

	gethostname(hostname, sizeof(hostname));

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

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	switch_snprintf(filename, sizeof(filename), "%s%s.tmp.xml", SWITCH_GLOBAL_dirs.temp_dir, uuid_str);
	curl_handle = curl_easy_init();
	headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

	if (!strncasecmp(binding->url, "https", 5)) {
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}

	config_data.name = filename;
	if ((config_data.fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
		if (!switch_strlen_zero(binding->cred)) {
			curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
			curl_easy_setopt(curl_handle, CURLOPT_USERPWD, binding->cred);
		}
		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
		curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
		curl_easy_setopt(curl_handle, CURLOPT_URL, binding->url);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &config_data);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-xml/1.0");

		if (binding->disable100continue) {
			slist = curl_slist_append(slist, "Expect:");
			curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, slist);
		}

		if (binding->ignore_cacert_check) {
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, FALSE);
		}

		curl_easy_perform(curl_handle);
		curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
		curl_easy_cleanup(curl_handle);
		curl_slist_free_all(headers);
		curl_slist_free_all(slist);
		close(config_data.fd);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening temp file!\n");
	}

	if (httpRes == 200) {
		if (!(xml = switch_xml_parse_file(filename))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing Result!\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received HTTP error %ld trying to fetch %s\ndata: [%s]\n", httpRes, binding->url, data);
		xml = NULL;
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
		char *bind_cred = NULL;
		char *bind_mask = NULL;
		int disable100continue = 0;
		uint32_t ignore_cacert_check = 0;
		hash_node_t* hash_node;
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
			} else if (!strcasecmp(var, "disable-100-continue") && switch_true(val)) {
				disable100continue = 1;
			} else if (!strcasecmp(var, "ignore-cacert-check") && switch_true(val)) {
				ignore_cacert_check = 1;
			} else if (!strcasecmp(var, "enable-post-var")) {
				if (!vars_map && need_vars_map == 0) {
					if (switch_core_hash_init(&vars_map, globals.pool) != SWITCH_STATUS_SUCCESS) {
						need_vars_map = -1;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't init params hash!\n");
						continue;
					}
					need_vars_map = 1;
				}

				if (vars_map && val)
					if (switch_core_hash_insert(vars_map, val, ENABLE_PARAM_VALUE) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't add %s to params hash!\n", val);
					}
			}
		}

		if (!url) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Binding has no url!\n");
			if (vars_map)
			    switch_core_hash_destroy(&vars_map);
			continue;
		}

		if (!(binding = malloc(sizeof(*binding)))) {
			if (vars_map)
			    switch_core_hash_destroy(&vars_map);
			goto done;
		}
		memset(binding, 0, sizeof(*binding));

		binding->url = strdup(url);

		if (bind_mask) {
			binding->bindings = strdup(bind_mask);
		}

		if (bind_cred) {
			binding->cred = strdup(bind_cred);
		}

		binding->disable100continue = disable100continue;
		binding->ignore_cacert_check = ignore_cacert_check;

		binding->vars_map = vars_map;
		
		if (vars_map) {
		    switch_zmalloc(hash_node,sizeof(hash_node_t));
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
						  switch_strlen_zero(bname) ? "N/A" : bname, binding->url, binding->bindings ? binding->bindings : "all");
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

	SWITCH_ADD_API(xml_curl_api_interface, "xml_curl", "XML Curl", xml_curl_function, XML_CURL_SYNTAX);
	switch_console_set_complete("add xml_curl debug_on");
	switch_console_set_complete("add xml_curl debug_off");

	memset(&globals,0,sizeof(globals));
	globals.pool = pool;
	globals.hash_root = NULL;
	globals.hash_tail = NULL;

	if (do_config() == SWITCH_STATUS_SUCCESS) {
		curl_global_init(CURL_GLOBAL_ALL);
	} else {
		return SWITCH_STATUS_FALSE;
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_curl_shutdown)
{
	hash_node_t* ptr = NULL;

	while(globals.hash_root) {
	    ptr = globals.hash_root;
	    switch_core_hash_destroy(&ptr->hash);
	    globals.hash_root = ptr->next;
	    switch_safe_free(ptr);
	}
	
	switch_xml_unbind_search_function_ptr(xml_url_fetch);
	curl_global_cleanup();
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
