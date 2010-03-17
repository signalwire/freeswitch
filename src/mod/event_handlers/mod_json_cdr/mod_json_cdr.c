/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * mod_json_cdr.c -- JSON CDR Module to files or curl
 *
 */
#include <sys/stat.h>
#include <switch.h>
#include <curl/curl.h>
#include <json.h>

#define MAX_URLS 20

#define ENCODING_NONE 0
#define ENCODING_DEFAULT 1
#define ENCODING_BASE64 2

static struct {
	char *cred;
	char *urls[MAX_URLS + 1];
	int url_count;
	int url_index;
	switch_thread_rwlock_t *log_path_lock;
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
	int auth_scheme;
	switch_memory_pool_t *pool;
	switch_event_node_t *node;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_json_cdr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_json_cdr_shutdown);
SWITCH_MODULE_DEFINITION(mod_json_cdr, mod_json_cdr_load, mod_json_cdr_shutdown, NULL);

/* this function would have access to the HTML returned by the webserver, we don't need it 
 * and the default curl activity is to print to stdout, something not as desirable
 * so we have a dummy function here
 */
static size_t httpCallBack(char *buffer, size_t size, size_t nitems, void *outstream)
{
	return size * nitems;
}

static switch_status_t set_json_cdr_log_dirs()
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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new mod_json_cdr log_dir path\n");
					switch_safe_free(path);
					status = SWITCH_STATUS_FALSE;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to generate new mod_json_cdr log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Setting log file path to %s\n", globals.base_log_dir);
			if ((path = switch_safe_strdup(globals.base_log_dir))) {
				switch_thread_rwlock_wrlock(globals.log_path_lock);
				switch_safe_free(globals.log_dir);
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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new mod_json_cdr err_log_dir path\n");
					switch_safe_free(path);
					status = SWITCH_STATUS_FALSE;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to generate new mod_json_cdr err_log_dir path\n");
				status = SWITCH_STATUS_FALSE;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Setting err log file path to %s\n", globals.base_err_log_dir);
			if ((path = switch_safe_strdup(globals.base_err_log_dir))) {
				switch_thread_rwlock_wrlock(globals.log_path_lock);
				switch_safe_free(globals.err_log_dir);
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

#define JSON_ENSURE_SUCCESS(obj) if (is_error(obj)) { return; }
SWITCH_DECLARE(void) set_json_profile_data(struct json_object *json, switch_caller_profile_t *caller_profile)
{
	struct json_object *param = NULL;

	param = json_object_new_string((char *)caller_profile->username);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "username", param);

	param = json_object_new_string((char *)caller_profile->dialplan);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "dialplan", param);

	param = json_object_new_string((char *)caller_profile->caller_id_name);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "caller_id_name", param);

	param = json_object_new_string((char *)caller_profile->ani);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "ani", param);

	param = json_object_new_string((char *)caller_profile->aniii);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "aniii", param);

	param = json_object_new_string((char *)caller_profile->caller_id_number);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "caller_id_number", param);

	param = json_object_new_string((char *)caller_profile->network_addr);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "network_addr", param);

	param = json_object_new_string((char *)caller_profile->rdnis);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "rdnis", param);

	param = json_object_new_string(caller_profile->destination_number);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "destination_number", param);

	param = json_object_new_string(caller_profile->uuid);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "uuid", param);

	param = json_object_new_string((char *)caller_profile->source);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "source", param);

	param = json_object_new_string((char *)caller_profile->context);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "context", param);

	param = json_object_new_string(caller_profile->chan_name);
	JSON_ENSURE_SUCCESS(param);
	json_object_object_add(json, "chan_name", param);

}

SWITCH_DECLARE(void) set_json_chan_vars(struct json_object *json, switch_channel_t *channel)
{
	struct json_object *variable = NULL;
	switch_event_header_t *hi = switch_channel_variable_first(channel);

	if (!hi)
		return;

	for (; hi; hi = hi->next) {
		if (!zstr(hi->name) && !zstr(hi->value)) {
			char *data;
			switch_size_t dlen = strlen(hi->value) * 3;

			if ((data = malloc(dlen))) {
				memset(data, 0, dlen);
				switch_url_encode(hi->value, data, dlen);

				variable = json_object_new_string(data);
				if (!is_error(variable)) {
					json_object_object_add(json, hi->name, variable);
				}
				free(data);
			}
		}
	}
	switch_channel_variable_last(channel);

	return;
}



SWITCH_DECLARE(switch_status_t) generate_json_cdr(switch_core_session_t *session, struct json_object **json_cdr)
{

	struct json_object *cdr = json_object_new_object();
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	struct json_object *variables, *j_main_cp, *j_caller_profile, *j_caller_extension, *j_times, *time_tag,
		*j_application, *j_callflow, *j_inner_extension, *j_apps, *j_o, *j_channel_data, *j_field;
	switch_app_log_t *app_log;
	char tmp[512], *f;
	
	if (is_error(cdr)) {
		return SWITCH_STATUS_FALSE;
	}

	j_channel_data = json_object_new_object();
	if (is_error(j_channel_data)) {
		goto error;
	}
	json_object_object_add(cdr, "channel_data", j_channel_data);

	
	j_field = json_object_new_string((char *) switch_channel_state_name(switch_channel_get_state(channel)));
	json_object_object_add(j_channel_data, "state", j_field);

	j_field = json_object_new_string(switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND ? "outbound" : "inbound");

	json_object_object_add(j_channel_data, "direction", j_field);


	switch_snprintf(tmp, sizeof(tmp), "%d", switch_channel_get_state(channel));
	j_field = json_object_new_string((char *) tmp);
	json_object_object_add(j_channel_data, "state_number", j_field);
	

	if ((f = switch_channel_get_flag_string(channel))) {
		j_field = json_object_new_string((char *) f);
		json_object_object_add(j_channel_data, "flags", j_field);
		free(f);
	}

	if ((f = switch_channel_get_cap_string(channel))) {
		j_field = json_object_new_string((char *) f);
		json_object_object_add(j_channel_data, "caps", j_field);
		free(f);
	}


	variables = json_object_new_object();
	json_object_object_add(cdr, "variables", variables);

	if (is_error(variables)) {
		goto error;
	}

	set_json_chan_vars(variables, channel);


	if ((app_log = switch_core_session_get_app_log(session))) {
		switch_app_log_t *ap;

		j_apps = json_object_new_object();

		if (is_error(j_apps)) {
			goto error;
		}

		json_object_object_add(cdr, "app_log", j_apps);

		for (ap = app_log; ap; ap = ap->next) {
			j_application = json_object_new_object();

			if (is_error(j_application)) {
				goto error;
			}

			json_object_object_add(j_application, "app_name", json_object_new_string(ap->app));
			json_object_object_add(j_application, "app_data", json_object_new_string(ap->arg));

			json_object_object_add(j_apps, "application", j_application);
		}
	}


	caller_profile = switch_channel_get_caller_profile(channel);

	while (caller_profile) {

		j_callflow = json_object_new_object();

		if (is_error(j_callflow)) {
			goto error;
		}

		json_object_object_add(cdr, "callflow", j_callflow);

		if (!zstr(caller_profile->dialplan)) {
			json_object_object_add(j_callflow, "dialplan", json_object_new_string((char *)caller_profile->dialplan));
		}

		if (!zstr(caller_profile->profile_index)) {
			json_object_object_add(j_callflow, "profile_index", json_object_new_string((char *)caller_profile->profile_index));
		}

		if (caller_profile->caller_extension) {
			switch_caller_application_t *ap;

			j_caller_extension = json_object_new_object();

			if (is_error(j_caller_extension)) {
				goto error;
			}

			json_object_object_add(j_callflow, "extension", j_caller_extension);

			json_object_object_add(j_caller_extension, "name", json_object_new_string(caller_profile->caller_extension->extension_name));
			json_object_object_add(j_caller_extension, "number", json_object_new_string(caller_profile->caller_extension->extension_number));

			if (caller_profile->caller_extension->current_application) {
				json_object_object_add(j_caller_extension, "current_app", json_object_new_string(caller_profile->caller_extension->current_application->application_name));
			}

			for (ap = caller_profile->caller_extension->applications; ap; ap = ap->next) {
				j_application = json_object_new_object();

				if (is_error(j_application)) {
					goto error;
				}


				json_object_object_add(j_caller_extension, "application", j_application);

				if (ap == caller_profile->caller_extension->current_application) {
					json_object_object_add(j_application, "last_executed", json_object_new_string("true"));
				}
				json_object_object_add(j_application, "app_name", json_object_new_string(ap->application_name));
				json_object_object_add(j_application, "app_data", json_object_new_string(ap->application_data));
			}

			if (caller_profile->caller_extension->children) {
				switch_caller_profile_t *cp = NULL;
				for (cp = caller_profile->caller_extension->children; cp; cp = cp->next) {

					if (!cp->caller_extension) {
						continue;
					}

					j_inner_extension = json_object_new_object();
					if (is_error(j_inner_extension)) {
						goto error;
					}

					json_object_object_add(j_caller_extension, "sub_extensions", j_inner_extension);


					j_caller_extension = json_object_new_object();
					if (is_error(j_caller_extension)) {
						goto error;
					}

					json_object_object_add(j_inner_extension, "extension", j_caller_extension);

					json_object_object_add(j_caller_extension, "name", json_object_new_string(cp->caller_extension->extension_name));
					json_object_object_add(j_caller_extension, "number", json_object_new_string(cp->caller_extension->extension_number));

					json_object_object_add(j_caller_extension, "dialplan", json_object_new_string((char *)cp->dialplan));

					if (cp->caller_extension->current_application) {
						json_object_object_add(j_caller_extension, "current_app", json_object_new_string(cp->caller_extension->current_application->application_name));
					}

					for (ap = cp->caller_extension->applications; ap; ap = ap->next) {
						j_application = json_object_new_object();
						
						if (is_error(j_application)) {
							goto error;
						}
						json_object_object_add(j_caller_extension, "application", j_application);

						if (ap == cp->caller_extension->current_application) {
							json_object_object_add(j_application, "last_executed", json_object_new_string("true"));
						}
						json_object_object_add(j_application, "app_name", json_object_new_string(ap->application_name));
						json_object_object_add(j_application, "app_data", json_object_new_string(ap->application_data));
					}
				}
			}
		}

		j_main_cp = json_object_new_object();
		if (is_error(j_main_cp)) {
			goto error;
		}

		json_object_object_add(j_callflow, "caller_profile", j_main_cp);

		set_json_profile_data(j_main_cp, caller_profile);

		if (caller_profile->originator_caller_profile) {
			switch_caller_profile_t *cp = NULL;

			j_o = json_object_new_object();
			if (is_error(j_o)) {
				goto error;
			}
			
			json_object_object_add(j_main_cp, "originator", j_o);

			for (cp = caller_profile->originator_caller_profile; cp; cp = cp->next) {
				j_caller_profile = json_object_new_object();
				if (is_error(j_caller_profile)) {
					goto error;
				}

				json_object_object_add(j_o, "originator_caller_profile", j_caller_profile);

				set_json_profile_data(j_caller_profile, cp);
			}
		}

		if (caller_profile->originatee_caller_profile) {
			switch_caller_profile_t *cp = NULL;

			j_o = json_object_new_object();
			if (is_error(j_o)) {
				goto error;
			}

			json_object_object_add(j_main_cp, "originatee", j_o);

			for (cp = caller_profile->originatee_caller_profile; cp; cp = cp->next) {

				j_caller_profile = json_object_new_object();
				if (is_error(j_caller_profile)) {
					goto error;
				}
				
				json_object_object_add(j_o, "originatee_caller_profile", j_caller_profile);
				set_json_profile_data(j_caller_profile, cp);
			}
		}

		if (caller_profile->times) {

			j_times = json_object_new_object();
			if (is_error(j_times)) {
				goto error;
			}

			json_object_object_add(j_callflow, "times", j_times);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->created);
			time_tag = json_object_new_string(tmp);
			if (is_error(time_tag)) {
				goto error;
			}
			json_object_object_add(j_times, "created_time", time_tag);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->profile_created);
			time_tag = json_object_new_string(tmp);
			if (is_error(time_tag)) {
				goto error;
			}
			json_object_object_add(j_times, "profile_created_time", time_tag);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->progress);
			time_tag = json_object_new_string(tmp);
			if (is_error(time_tag)) {
				goto error;
			}
			json_object_object_add(j_times, "progress_time", time_tag);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->progress_media);
			time_tag = json_object_new_string(tmp);
			if (is_error(time_tag)) {
				goto error;
			}
			json_object_object_add(j_times, "progress_media_time", time_tag);


			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->answered);
			time_tag = json_object_new_string(tmp);
			if (is_error(time_tag)) {
				goto error;
			}
			json_object_object_add(j_times, "answered_time", time_tag);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->hungup);
			time_tag = json_object_new_string(tmp);
			if (is_error(time_tag)) {
				goto error;
			}
			json_object_object_add(j_times, "hangup_time", time_tag);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->resurrected);
			time_tag = json_object_new_string(tmp);
			if (is_error(time_tag)) {
				goto error;
			}
			json_object_object_add(j_times, "resurrect_time", time_tag);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->transferred);
			time_tag = json_object_new_string(tmp);
			if (is_error(time_tag)) {
				goto error;
			}
			json_object_object_add(j_times, "transfer_time", time_tag);

		}

		caller_profile = caller_profile->next;
	}

	*json_cdr = cdr;

	return SWITCH_STATUS_SUCCESS;
	
  error:

	if (cdr) {
		json_object_put(cdr);
	}

	return SWITCH_STATUS_FALSE;
}


static switch_status_t my_on_reporting(switch_core_session_t *session)
{
	struct json_object *json_cdr = NULL;
	const char *json_text = NULL;
	char *path = NULL;
	char *curl_json_text = NULL;
	const char *logdir = NULL;
	char *json_text_escaped = NULL;
	int fd = -1;
	uint32_t cur_try;
	long httpRes;
	CURL *curl_handle = NULL;
	struct curl_slist *headers = NULL;
	struct curl_slist *slist = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_FALSE;
	int is_b;
	const char *a_prefix = "";

	if (globals.shutdown) {
		return SWITCH_STATUS_SUCCESS;
	}

	is_b = channel && switch_channel_get_originator_caller_profile(channel);
	if (!globals.log_b && is_b) {
		return SWITCH_STATUS_SUCCESS;
	}
	if (!is_b && globals.prefix_a)
		a_prefix = "a_";


	if (generate_json_cdr(session, &json_cdr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Generating Data!\n");
		return SWITCH_STATUS_FALSE;
	}
	
	json_text = json_object_to_json_string(json_cdr);

	if (!json_text) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		goto error;
	}

	switch_thread_rwlock_rdlock(globals.log_path_lock);

	if (!(logdir = switch_channel_get_variable(channel, "json_cdr_base"))) {
		logdir = globals.log_dir;
	}

	if (!zstr(logdir) && (globals.log_http_and_disk || !globals.url_count)) {
		path = switch_mprintf("%s%s%s%s.cdr.json", logdir, SWITCH_PATH_SEPARATOR, a_prefix, switch_core_session_get_uuid(session));
		switch_thread_rwlock_unlock(globals.log_path_lock);
		if (path) {
#ifdef _MSC_VER
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
#else
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) > -1) {
#endif
				int wrote;
				wrote = write(fd, json_text, (unsigned) strlen(json_text));
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
	} else {
		switch_thread_rwlock_unlock(globals.log_path_lock);
	}

	/* try to post it to the web server */
	if (globals.url_count) {
		char *destUrl = NULL;
		curl_handle = curl_easy_init();

		if (globals.encode) {
			switch_size_t need_bytes = strlen(json_text) * 3;

			json_text_escaped = malloc(need_bytes);
			switch_assert(json_text_escaped);
			memset(json_text_escaped, 0, need_bytes);
			if (globals.encode == ENCODING_DEFAULT) {
				headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
				switch_url_encode(json_text, json_text_escaped, need_bytes);
			} else {
				headers = curl_slist_append(headers, "Content-Type: application/x-www-form-base64-encoded");
				switch_b64_encode((unsigned char *) json_text, need_bytes / 3, (unsigned char *) json_text_escaped, need_bytes);
			}

			json_text = json_text_escaped;

			if (!(curl_json_text = switch_mprintf("cdr=%s", json_text))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
				goto error;
			}

		} else {
			headers = curl_slist_append(headers, "Content-Type: application/x-www-form-plaintext");
			curl_json_text = (char *)json_text;
		}


		if (!zstr(globals.cred)) {
			curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, globals.auth_scheme);
			curl_easy_setopt(curl_handle, CURLOPT_USERPWD, globals.cred);
		}

		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, curl_json_text);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-json/1.0");
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, httpCallBack);

		if (globals.disable100continue) {
			slist = curl_slist_append(slist, "Expect:");
			curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, slist);
		}

		if (globals.ssl_cert_file) {
			curl_easy_setopt(curl_handle, CURLOPT_SSLCERT, globals.ssl_cert_file);
		}

		if (globals.ssl_key_file) {
			curl_easy_setopt(curl_handle, CURLOPT_SSLKEY, globals.ssl_key_file);
		}

		if (globals.ssl_key_password) {
			curl_easy_setopt(curl_handle, CURLOPT_SSLKEYPASSWD, globals.ssl_key_password);
		}

		if (globals.ssl_version) {
			if (!strcasecmp(globals.ssl_version, "SSLv3")) {
				curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
			} else if (!strcasecmp(globals.ssl_version, "TLSv1")) {
				curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
			}
		}

		if (globals.ssl_cacert_file) {
			curl_easy_setopt(curl_handle, CURLOPT_CAINFO, globals.ssl_cacert_file);
		}

		/* these were used for testing, optionally they may be enabled if someone desires
		   curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 120); // tcp timeout
		   curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1); // 302 recursion level
		 */

		for (cur_try = 0; cur_try < globals.retries; cur_try++) {
			if (cur_try > 0) {
				switch_yield(globals.delay * 1000000);
			}

			destUrl = switch_mprintf("%s?uuid=%s", globals.urls[globals.url_index], switch_core_session_get_uuid(session));
			curl_easy_setopt(curl_handle, CURLOPT_URL, destUrl);

			if (!strncasecmp(destUrl, "https", 5)) {
				curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
				curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
			}

			if (globals.enable_cacert_check) {
				curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, TRUE);
			}

			if (globals.enable_ssl_verifyhost) {
				curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2);
			}

			curl_easy_perform(curl_handle);
			curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
			switch_safe_free(destUrl);
			if (httpRes == 200) {
				goto success;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Got error [%ld] posting to web server [%s]\n",
								  httpRes, globals.urls[globals.url_index]);
				globals.url_index++;
				switch_assert(globals.url_count <= MAX_URLS);
				if (globals.url_index >= globals.url_count) {
					globals.url_index = 0;
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Retry will be with url [%s]\n", globals.urls[globals.url_index]);
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

		switch_thread_rwlock_rdlock(globals.log_path_lock);
		path = switch_mprintf("%s%s%s%s.cdr.json", globals.err_log_dir, SWITCH_PATH_SEPARATOR, a_prefix, switch_core_session_get_uuid(session));
		switch_thread_rwlock_unlock(globals.log_path_lock);
		if (path) {
#ifdef _MSC_VER
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
#else
			if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) > -1) {
#endif
				int wrote;
				wrote = write(fd, json_text, (unsigned) strlen(json_text));
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
	if (curl_json_text != json_text) {
		switch_safe_free(curl_json_text);
	}
	
	json_object_put(json_cdr);
	switch_safe_free(json_text_escaped);

	switch_safe_free(path);

	return status;
}

static void event_handler(switch_event_t *event)
{
	const char *sig = switch_event_get_header(event, "Trapped-Signal");

	if (sig && !strcmp(sig, "HUP")) {
		if (globals.rotate) {
			set_json_cdr_log_dirs();
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

SWITCH_MODULE_LOAD_FUNCTION(mod_json_cdr_load)
{
	char *cf = "json_cdr.conf";
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

	/* parse the config */
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
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
			} else if (!strcasecmp(var, "delay") && !zstr(val)) {
				globals.delay = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "log-b-leg")) {
				globals.log_b = switch_true(val);
			} else if (!strcasecmp(var, "prefix-a-leg")) {
				globals.prefix_a = switch_true(val);
			} else if (!strcasecmp(var, "disable-100-continue") && switch_true(val)) {
				globals.disable100continue = 1;
			} else if (!strcasecmp(var, "encode") && !zstr(val)) {
				if (!strcasecmp(val, "base64")) {
					globals.encode = ENCODING_BASE64;
				} else {
					globals.encode = switch_true(val) ? ENCODING_DEFAULT : ENCODING_NONE;
				}
			} else if (!strcasecmp(var, "retries") && !zstr(val)) {
				globals.retries = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "rotate") && !zstr(val)) {
				globals.rotate = switch_true(val);
			} else if (!strcasecmp(var, "log-dir")) {
				if (zstr(val)) {
					globals.base_log_dir = switch_core_sprintf(globals.pool, "%s%sjson_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
				} else {
					if (switch_is_file_path(val)) {
						globals.base_log_dir = switch_core_strdup(globals.pool, val);
					} else {
						globals.base_log_dir = switch_core_sprintf(globals.pool, "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, val);
					}
				}
			} else if (!strcasecmp(var, "err-log-dir")) {
				if (zstr(val)) {
					globals.base_err_log_dir = switch_core_sprintf(globals.pool, "%s%sjson_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
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
				globals.ssl_cert_file = val;
			} else if (!strcasecmp(var, "ssl-key-path")) {
				globals.ssl_key_file = val;
			} else if (!strcasecmp(var, "ssl-key-password")) {
				globals.ssl_key_password = val;
			} else if (!strcasecmp(var, "ssl-version")) {
				globals.ssl_version = val;
			} else if (!strcasecmp(var, "ssl-cacert-file")) {
				globals.ssl_cacert_file = val;
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
					globals.auth_scheme = CURLAUTH_ANY;
				}
			}

		}

		if (zstr(globals.base_err_log_dir)) {
			if (!zstr(globals.base_log_dir)) {
				globals.base_err_log_dir = switch_core_strdup(globals.pool, globals.base_log_dir);
			} else {
				globals.base_err_log_dir = switch_core_sprintf(globals.pool, "%s%sjson_cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
			}
		}
	}

	if (globals.retries < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Retries is negative, setting to 0\n");
		globals.retries = 0;
	}

	if (globals.retries && globals.delay <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Retries set but delay 0 setting to 5000ms\n");
		globals.delay = 5000;
	}

	globals.retries++;

	set_json_cdr_log_dirs();

	switch_xml_free(xml);
	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_json_cdr_shutdown)
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
