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
 *
 * mod_snom.c -- SNOM Specific Features
 *
 */
#include <switch.h>
#include <switch_curl.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_snom_load);
SWITCH_MODULE_DEFINITION(mod_snom, mod_snom_load, NULL, NULL);

static switch_bool_t snom_bind_key(const char *key,
								   const char *light,
								   const char *label, const char *user, const char *host, const char *profile, const char *action_name, const char *action)
{
	switch_event_t *event;


	if (user && host && profile) {
		if (switch_event_create(&event, SWITCH_EVENT_SEND_MESSAGE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "user", user);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "host", host);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "profile", profile);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "content-type", "application/x-buttons");
			if (action && action_name) {
				switch_event_add_body(event, "k=%s\nc=%s\nl=%s\nn=%s\na=%s\n", key, light, label, action, action_name);
			} else {
				switch_event_add_body(event, "k=%s\nc=%s\nl=%s\n\n", key, light, label);
			}

			switch_event_fire(&event);
		}
		return SWITCH_TRUE;
	}

	return SWITCH_FALSE;
}


#define URL_SYNTAX ""
SWITCH_STANDARD_API(snom_url_api_function)
{
#if 0
	char *tmp;
	switch_event_serialize(stream->param_event, &tmp, SWITCH_TRUE);
	printf("W00t\n%s\n", tmp);
	free(tmp);
#endif

	return SWITCH_STATUS_SUCCESS;

}

#define KEY_BIND_SYNTAX "<key> <light> <label> <user> <host> <profile> <action_name> <action>"
SWITCH_STANDARD_API(snom_bind_key_api_function)
{
	int argc;
	char *mydata = NULL, *argv[8];

	mydata = strdup(cmd);
	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 6) {
		goto err;
	}

	if (snom_bind_key(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7])) {
		stream->write_function(stream, "+OK %s\n", cmd);
		goto end;
	}

  err:

	stream->write_function(stream, "-Error %s\n", KEY_BIND_SYNTAX);

  end:

	free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

static size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	return realsize;
}

#define COMMAND_SYNTAX "<ip|user> <command> <type> <username> <password>"
SWITCH_STANDARD_API(snom_command_api_function)
{
	int argc;
	long httpRes = 0;
	char *url = NULL;
	char *argv[5] = { 0 };
	char *argdata = NULL;
	char *userpwd = NULL;
	char *apiresp = NULL;
	ip_t  ip;
	switch_CURL *curl_handle = NULL;

	if (zstr(cmd) || !(argdata = strdup(cmd))) {
		goto end;
	}

	argc = switch_separate_string(argdata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 3 || (argc > 3 && argc < 5)) {
		stream->write_function(stream, "-ERR %s\n", COMMAND_SYNTAX);
		goto end;
	}

	if (strcasecmp(argv[1],"key")) {
		stream->write_function(stream, "-ERR only KEY command allowed at the moment\n");
		goto end;
	}

	if (switch_inet_pton(AF_INET, argv[0], &ip)) {
		url = switch_mprintf("http://%s/command.htm?%s=%s",argv[0],argv[1],argv[2]);
	} else {
		char *sql = NULL;
		char buf[32];
		char *ret = NULL;
		switch_cache_db_handle_t *db = NULL;
		switch_stream_handle_t apistream = { 0 };
		switch_status_t status;

		SWITCH_STANDARD_STREAM(apistream);
		if ((status = switch_api_execute("sofia_contact", argv[0], NULL, &apistream)) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "-ERR error executing sofia_contact\n");
			goto end;
		}
		apiresp = (char*) apistream.data;

		if (!zstr(apiresp)) {
			if (!strcasecmp(apistream.data,"error/user_not_registered")) {
				stream->write_function(stream, "-ERR user '%s' not registered\n",argv[0]);
				goto end;
			}
		} else {
			goto end;
		}

		if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "%s", "-ERR Database Error!\n");
			goto end;
		}

		sql = switch_mprintf("select network_ip from registrations where url = '%s'", apiresp);

		ret = switch_cache_db_execute_sql2str(db, sql, buf, sizeof(buf), NULL);
		switch_safe_free(sql);
		switch_cache_db_release_db_handle(&db);

		if (!ret) {
			stream->write_function(stream, "%s", "-ERR Query '%s' failed!\n", sql);
			goto end;
		}

		url = switch_mprintf("http://%s/command.htm?%s=%s",buf,argv[1],argv[2]);
	}

	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_callback);
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-curl/1.0");
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 15);
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);

	if (argc == 5) {
		userpwd = switch_mprintf("%s:%s",argv[3],argv[4]);
		curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, (long)CURLAUTH_ANY);
		curl_easy_setopt(curl_handle, CURLOPT_USERPWD, userpwd);
	}
	curl_easy_perform(curl_handle);
	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	curl_easy_cleanup(curl_handle);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "curl url %s , result %ld\n", url,httpRes);

	if (httpRes != 200)
		stream->write_function(stream, "-ERR %s [HTTP:%ld]\n", cmd, httpRes);
	else
		stream->write_function(stream, "+OK %s\n", cmd);

end:
	switch_safe_free(apiresp);
	switch_safe_free(userpwd);
	switch_safe_free(argdata);
	switch_safe_free(url);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_snom_load)
{

	switch_api_interface_t *commands_api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);


	SWITCH_ADD_API(commands_api_interface, "snom_bind_key", "Bind a key", snom_bind_key_api_function, KEY_BIND_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "snom_url", "url", snom_url_api_function, URL_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "snom_command", "Sends Command over HTTP Request", snom_command_api_function, COMMAND_SYNTAX);


	/* indicate that the module should continue to be loaded */
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
