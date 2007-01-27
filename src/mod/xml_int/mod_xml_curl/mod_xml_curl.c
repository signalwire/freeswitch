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
 *
 * mod_xml_curl.c -- CURL XML Gateway
 *
 */
#include <switch.h>
#include <curl/curl.h>

static const char modname[] = "mod_xml_curl";

struct xml_binding {
	char *url;
	char *bindings;
    char *cred;
};

typedef struct xml_binding xml_binding_t;

struct config_data {
	char *name;
	int fd;
};

static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int)(size * nmemb);
	struct config_data *config_data = data;

	write(config_data->fd, ptr, realsize);
	return realsize;
}


static switch_xml_t xml_url_fetch(char *section,
								  char *tag_name,
								  char *key_name,
								  char *key_value,
								  char *params,
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

    if (!binding) {
        return NULL;
    }

    if (file_url = strstr(binding->url, "file:")) {
        file_url += 5;
     
        if (!(xml = switch_xml_parse_file(file_url))) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing Result!\n");
        }
   
        return xml;
    }

    if (!(data = switch_mprintf("section=%s&tag_name=%s&key_name=%s&key_value=%s%s%s", 
                                section,
                                tag_name ? tag_name : "",
                                key_name ? key_name : "",
                                key_value ? key_value : "",
                                params ? strchr(params,'=') ? "&" : "&params=" : "", params ? params : ""))) {

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
        return NULL;
    }

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	snprintf(filename, sizeof(filename), "%s%s.tmp.xml", SWITCH_GLOBAL_dirs.temp_dir, uuid_str);
	curl_handle = curl_easy_init();
	if (!strncasecmp(binding->url, "https", 5)) {
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}
		
	config_data.name = filename;
	if ((config_data.fd = open(filename, O_CREAT | O_RDWR | O_TRUNC)) > -1) {
        if (!switch_strlen_zero(binding->cred)) {
            curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
            curl_easy_setopt(curl_handle, CURLOPT_USERPWD, binding->cred);
        }
        curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
		curl_easy_setopt(curl_handle, CURLOPT_URL, binding->url);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&config_data);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-xml/1.0");
		curl_easy_perform(curl_handle);
		curl_easy_cleanup(curl_handle);
		close(config_data.fd);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
	}

    switch_safe_free(data);

	if (!(xml = switch_xml_parse_file(filename))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing Result!\n");
	}

	unlink(filename);
	
	return xml;
}


static switch_loadable_module_interface_t xml_curl_module_interface = {
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

static switch_status_t do_config(void) 
{
	char *cf = "xml_curl.conf";
	switch_xml_t cfg, xml, bindings_tag, binding_tag, param;
    xml_binding_t *binding = NULL;
    int x = 0;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if (!(bindings_tag = switch_xml_child(cfg, "bindings"))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing <bindings> tag!\n");
        return SWITCH_STATUS_FALSE;
    }

	for (binding_tag = switch_xml_child(bindings_tag, "binding"); binding_tag; binding_tag = binding_tag->next) {
        char *bname = (char *) switch_xml_attr_soft(binding_tag, "name");
        char *url = NULL;
        char *bind_cred = NULL;
        char *bind_mask = NULL;

		for (param = switch_xml_child(binding_tag, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
            if (!strcasecmp(var, "gateway-url")) {
				bind_mask = (char *) switch_xml_attr_soft(param, "bindings");
                if (val) {
                    url = val;
                }
			} else if (!strcasecmp(var, "gateway-credentials")) {
                bind_cred = var;
            }
		}
        
        if (!url) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Binding has no url!\n");
            continue;
        }

        if (!(binding = malloc(sizeof(*binding)))) {
            return SWITCH_STATUS_FALSE;
        }
        memset(binding, 0, sizeof(*binding));

        binding->url = strdup(url);

        if (bind_mask) {
            binding->bindings = strdup(bind_mask);
        }

        if (bind_cred) {
            binding->cred = strdup(bind_cred);
        }
        
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Binding [%s] XML Fetch Function [%s] [%s]\n", 
                          switch_strlen_zero(bname) ? "N/A" : bname,
                          binding->url,
                          binding->bindings ? binding->bindings : "all");
		switch_xml_bind_search_function(xml_url_fetch, switch_xml_parse_section_string(binding->bindings), binding);
        x++;
        binding = NULL;
	}

	switch_xml_free(xml);

	return x ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}


SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &xml_curl_module_interface;

	if (do_config() == SWITCH_STATUS_SUCCESS) {
        curl_global_init(CURL_GLOBAL_ALL);
    } else {
        return SWITCH_STATUS_FALSE;
    }

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{
	curl_global_cleanup();
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
