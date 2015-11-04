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
* William King <william.king@quentustech.com>
*
* mod_hiredis.c -- Redis DB access module
*
*/

#include "mod_hiredis.h"

mod_hiredis_global_t mod_hiredis_globals;

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_hiredis_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_hiredis_load);
SWITCH_MODULE_DEFINITION(mod_hiredis, mod_hiredis_load, mod_hiredis_shutdown, NULL);

SWITCH_STANDARD_APP(raw_app)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *response = NULL, *profile_name = NULL, *cmd = NULL;
	hiredis_profile_t *profile = NULL;

	if ( !zstr(data) ) {
		profile_name = strdup(data);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: invalid data! Use the format 'default set keyname value' \n");
		goto done;
	}

	if ( (cmd = strchr(profile_name, ' '))) {
		*cmd = '\0';
		cmd++;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: invalid data! Use the format 'default set keyname value' \n");
		goto done;
	}

	profile = switch_core_hash_find(mod_hiredis_globals.profiles, profile_name);

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: Unable to locate profile[%s]\n", profile_name);
		return;
	}

	if ( hiredis_profile_execute_sync(profile, cmd, &response) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: profile[%s] error executing [%s] because [%s]\n", profile_name, cmd, response);
	}

	switch_channel_set_variable(channel, "hiredis_raw_response", response);

 done:
	switch_safe_free(profile_name);
	switch_safe_free(response);
	return;
}

SWITCH_STANDARD_API(raw_api)
{
	hiredis_profile_t *profile = NULL;
	char *data = NULL, *input = NULL, *response = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if ( !zstr(cmd) ) {
		input = strdup(cmd);
	} else {
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( (data = strchr(input, ' '))) {
		*data = '\0';
		data++;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "hiredis: debug: profile[%s] for command [%s]\n", input, data);

	profile = switch_core_hash_find(mod_hiredis_globals.profiles, input);

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: Unable to locate profile[%s]\n", input);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( hiredis_profile_execute_sync(profile, data, &response) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: profile[%s] error executing [%s] reason:[%s]\n", input, data, response);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	stream->write_function(stream, response);
 done:
	switch_safe_free(input);
	switch_safe_free(response);
	return status;
}

/*
SWITCH_LIMIT_INCR(name) static switch_status_t name (switch_core_session_t *session, const char *realm, const char *resource,
                                                     const int max, const int interval)
*/
SWITCH_LIMIT_INCR(hiredis_limit_incr)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	hiredis_profile_t *profile = NULL;
	char *hashkey = NULL, *response = NULL, *limit_key = NULL;
	int64_t count = 0; /* Redis defines the incr action as to be performed on a 64 bit signed integer */
	time_t now = switch_epoch_time_now(NULL);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	hiredis_limit_pvt_t *limit_pvt = NULL;
	switch_memory_pool_t *session_pool = switch_core_session_get_pool(session);

	if ( zstr(realm) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: realm must be defined\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	profile = switch_core_hash_find(mod_hiredis_globals.profiles, realm);

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: Unable to locate profile[%s]\n", realm);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( interval ) {
		limit_key = switch_mprintf("%s_%d", resource, now / interval);
	} else {
		limit_key = switch_mprintf("%s", resource);
	}

	hashkey = switch_mprintf("incr %s", limit_key);

	if ( hiredis_profile_execute_sync(profile, hashkey, &response) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: profile[%s] error executing [%s] because [%s]\n", realm, hashkey, response);
		switch_channel_set_variable(channel, "hiredis_raw_response", response);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	switch_channel_set_variable(channel, "hiredis_raw_response", response);

	limit_pvt = switch_core_alloc(session_pool, sizeof(hiredis_limit_pvt_t));
	limit_pvt->next = switch_channel_get_private(channel, "hiredis_limit_pvt");
	limit_pvt->realm = switch_core_strdup(session_pool, realm);
	limit_pvt->resource = switch_core_strdup(session_pool, resource);
	limit_pvt->limit_key = switch_core_strdup(session_pool, limit_key);
	limit_pvt->inc = 1;
	limit_pvt->interval = interval;
	switch_channel_set_private(channel, "hiredis_limit_pvt", limit_pvt);

	count = atoll(response);

	if ( !count || count > max ) {
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

 done:
	switch_safe_free(limit_key);
	switch_safe_free(response);
	switch_safe_free(hashkey);
	return status;
}

/*
  SWITCH_LIMIT_RELEASE(name) static switch_status_t name (switch_core_session_t *session, const char *realm, const char *resource)
*/
SWITCH_LIMIT_RELEASE(hiredis_limit_release)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	hiredis_profile_t *profile = NULL;
	char *hashkey = NULL, *response = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	hiredis_limit_pvt_t *limit_pvt = switch_channel_get_private(channel, "hiredis_limit_pvt");

	/* If realm and resource are NULL, then clear all of the limits */
	if ( !realm && !resource ) {
		hiredis_limit_pvt_t *tmp = limit_pvt;

		while (tmp) {
			profile = switch_core_hash_find(mod_hiredis_globals.profiles, limit_pvt->realm);
			hashkey = switch_mprintf("decr %s", tmp->limit_key);
			limit_pvt = tmp->next;

			if ( limit_pvt && (limit_pvt->interval > 0) && (hiredis_profile_execute_sync(profile, hashkey, &response) != SWITCH_STATUS_SUCCESS)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: profile[%s] error executing [%s] because [%s]\n",
								  tmp->realm, hashkey, response);
			}

			tmp = limit_pvt;
			switch_safe_free(response);
			switch_safe_free(hashkey);
		}
	} else {
		profile = switch_core_hash_find(mod_hiredis_globals.profiles, limit_pvt->realm);

		hashkey = switch_mprintf("decr %s", limit_pvt->limit_key);

		if ( hiredis_profile_execute_sync(profile, hashkey, &response) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: profile[%s] error executing [%s] because [%s]\n", realm, hashkey, response);
			switch_channel_set_variable(channel, "hiredis_raw_response", response);
			switch_goto_status(SWITCH_STATUS_GENERR, done);
		}

		switch_channel_set_variable(channel, "hiredis_raw_response", response);
	}

 done:
	switch_safe_free(response);
	switch_safe_free(hashkey);
	return status;
}

/*
SWITCH_LIMIT_USAGE(name) static int name (const char *realm, const char *resource, uint32_t *rcount)
 */
SWITCH_LIMIT_USAGE(hiredis_limit_usage)
{
	hiredis_profile_t *profile = switch_core_hash_find(mod_hiredis_globals.profiles, realm);
	int64_t count = 0; /* Redis defines the incr action as to be performed on a 64 bit signed integer */
	char *hashkey = NULL, *response = NULL;

	if ( !zstr(realm) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: realm must be defined\n");
		goto err;
	}

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: Unable to locate profile[%s]\n", realm);
		goto err;
	}

	hashkey = switch_mprintf("get %s", resource);

	if ( hiredis_profile_execute_sync(profile, hashkey, &response) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: profile[%s] error executing [%s] because [%s]\n", realm, hashkey, response);
		goto err;
	}

	count = atoll(response);

	switch_safe_free(response);
	switch_safe_free(hashkey);
	return count;

 err:
	switch_safe_free(response);
	switch_safe_free(hashkey);
	return -1;
}

/*
SWITCH_LIMIT_RESET(name) static switch_status_t name (void)
 */
SWITCH_LIMIT_RESET(hiredis_limit_reset)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
					  "hiredis: unable to globally reset hiredis limit resources. Use 'hiredis_raw set resource_name 0'\n");

	return SWITCH_STATUS_GENERR;
}

/*
  SWITCH_LIMIT_INTERVAL_RESET(name) static switch_status_t name (const char *realm, const char *resource)
*/
SWITCH_LIMIT_INTERVAL_RESET(hiredis_limit_interval_reset)
{
	hiredis_profile_t *profile = switch_core_hash_find(mod_hiredis_globals.profiles, realm);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *hashkey = NULL, *response = NULL;

	if ( !zstr(realm) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: realm must be defined\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: Unable to locate profile[%s]\n", realm);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	hashkey = switch_mprintf("set %s 0", resource);

	if ( hiredis_profile_execute_sync(profile, hashkey, &response) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: profile[%s] error executing [%s] because [%s]\n", realm, hashkey, response);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

 done:
	switch_safe_free(response);
	switch_safe_free(hashkey);
	return status;
}

/*
SWITCH_LIMIT_STATUS(name) static char * name (void)
 */
SWITCH_LIMIT_STATUS(hiredis_limit_status)
{
	return strdup("-ERR not supported");
}

SWITCH_MODULE_LOAD_FUNCTION(mod_hiredis_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;
	switch_limit_interface_t *limit_interface;

	memset(&mod_hiredis_globals, 0, sizeof(mod_hiredis_global_t));
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	mod_hiredis_globals.pool = pool;

	switch_core_hash_init(&(mod_hiredis_globals.profiles));

	if ( mod_hiredis_do_config() != SWITCH_STATUS_SUCCESS ) {
		return SWITCH_STATUS_GENERR;
	}

	SWITCH_ADD_LIMIT(limit_interface, "hiredis", hiredis_limit_incr, hiredis_limit_release, hiredis_limit_usage,
					 hiredis_limit_reset, hiredis_limit_status, hiredis_limit_interval_reset);
	SWITCH_ADD_APP(app_interface, "hiredis_raw", "hiredis_raw", "hiredis_raw", raw_app, "", SAF_NONE);
	SWITCH_ADD_API(api_interface, "hiredis_raw", "hiredis_raw", raw_api, "");

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_hiredis_shutdown)
{
	switch_hash_index_t *hi;
	hiredis_profile_t *profile = NULL;
	/* loop through profiles, and destroy them */

	while ((hi = switch_core_hash_first(mod_hiredis_globals.profiles))) {
		switch_core_hash_this(hi, NULL, NULL, (void **)&profile);
		hiredis_profile_destroy(&profile);
		switch_safe_free(hi);
	}

	switch_core_hash_destroy(&(mod_hiredis_globals.profiles));
	
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
