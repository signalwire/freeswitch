/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2016, Anthony Minessale II <anthm@freeswitch.org>
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
* Chris Rienzo <chris.rienzo@citrix.com>
*
* mod_hiredis.c -- Redis DB access module
*
*/

#include "mod_hiredis.h"

mod_hiredis_global_t mod_hiredis_globals;

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_hiredis_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_hiredis_load);
SWITCH_MODULE_DEFINITION(mod_hiredis, mod_hiredis_load, mod_hiredis_shutdown, NULL);

#define DECR_DEL_SCRIPT "local v=redis.call(\"decr\",KEYS[1]);if v <= 0 then redis.call(\"del\",KEYS[1]) end;return v;"

/**
 * Get exclusive access to limit_pvt, if it exists
 */
static hiredis_limit_pvt_t *get_limit_pvt(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	hiredis_limit_pvt_t *limit_pvt = switch_channel_get_private(channel, "hiredis_limit_pvt");
	if (limit_pvt) {
		/* pvt already exists, return it */
		switch_mutex_lock(limit_pvt->mutex);
		return limit_pvt;
	}
	return NULL;
}

/**
 * Add limit_pvt and get exclusive access to it
 */
static hiredis_limit_pvt_t *add_limit_pvt(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	hiredis_limit_pvt_t *limit_pvt = switch_channel_get_private(channel, "hiredis_limit_pvt");
	if (limit_pvt) {
		/* pvt already exists, return it */
		switch_mutex_lock(limit_pvt->mutex);
		return limit_pvt;
	}

	/* not created yet, add it - NOTE a channel mutex would be better here if we had access to it */
	switch_mutex_lock(mod_hiredis_globals.limit_pvt_mutex);
	limit_pvt = switch_channel_get_private(channel, "hiredis_limit_pvt");
	if (limit_pvt) {
		/* was just added by another thread */
		switch_mutex_unlock(mod_hiredis_globals.limit_pvt_mutex);
		switch_mutex_lock(limit_pvt->mutex);
		return limit_pvt;
	}

	/* still not created yet, add it */
	limit_pvt = switch_core_session_alloc(session, sizeof(*limit_pvt));
	switch_mutex_init(&limit_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	limit_pvt->first = NULL;
	switch_channel_set_private(channel, "hiredis_limit_pvt", limit_pvt);
	switch_mutex_unlock(mod_hiredis_globals.limit_pvt_mutex);
	switch_mutex_lock(limit_pvt->mutex);
	return limit_pvt;
}

/**
 * Release exclusive acess to limit_pvt
 */
static void release_limit_pvt(hiredis_limit_pvt_t *limit_pvt)
{
	if (limit_pvt) {
		switch_mutex_unlock(limit_pvt->mutex);
	}
}

SWITCH_STANDARD_APP(raw_app)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *response = NULL, *profile_name = NULL, *cmd = NULL;
	hiredis_profile_t *profile = NULL;

	if ( !zstr(data) ) {
		profile_name = strdup(data);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: invalid data! Use the format 'default set keyname value' \n");
		goto done;
	}

	if ( (cmd = strchr(profile_name, ' '))) {
		*cmd = '\0';
		cmd++;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: invalid data! Use the format 'default set keyname value' \n");
		goto done;
	}

	profile = switch_core_hash_find(mod_hiredis_globals.profiles, profile_name);

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: Unable to locate profile[%s]\n", profile_name);
		return;
	}

	if ( hiredis_profile_execute_sync(profile, session, &response, cmd) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: profile[%s] error executing [%s] because [%s]\n", profile_name, cmd, response ? response : "");
	}

	switch_channel_set_variable(channel, "hiredis_raw_response", response ? response : "");

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

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hiredis: debug: profile[%s] for command [%s]\n", input, data);

	profile = switch_core_hash_find(mod_hiredis_globals.profiles, input);

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: Unable to locate profile[%s]\n", input);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( hiredis_profile_execute_sync(profile, session, &response, data) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: profile[%s] error executing [%s] reason:[%s]\n", input, data, response ? response : "");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if (response) {
		stream->write_function(stream, response);
	}
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
	char *response = NULL, *limit_key = NULL;
	int64_t count = 0; /* Redis defines the incr action as to be performed on a 64 bit signed integer */
	time_t now = switch_epoch_time_now(NULL);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	hiredis_limit_pvt_t *limit_pvt = NULL;
	hiredis_limit_pvt_node_t *limit_pvt_node = NULL;
	switch_memory_pool_t *session_pool = switch_core_session_get_pool(session);

	if ( zstr(realm) ) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: realm must be defined\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( interval < 0 ) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: interval must be >= 0\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	profile = switch_core_hash_find(mod_hiredis_globals.profiles, realm);

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: Unable to locate profile[%s]\n", realm);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( interval ) {
		limit_key = switch_core_session_sprintf(session, "%s_%d", resource, now / interval);
	} else {
		limit_key = switch_core_session_sprintf(session, "%s", resource);
	}

	if ( (status = hiredis_profile_execute_pipeline_printf(profile, session, &response, "incr %s", limit_key) ) != SWITCH_STATUS_SUCCESS ) {
		if ( status == SWITCH_STATUS_SOCKERR && profile->ignore_connect_fail) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "hiredis: ignoring profile[%s] connection error incrementing [%s]\n", realm, limit_key);
			switch_goto_status(SWITCH_STATUS_SUCCESS, done);
		} else if ( profile->ignore_error ) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "hiredis: ignoring profile[%s] general error incrementing [%s]\n", realm, limit_key);
			switch_goto_status(SWITCH_STATUS_SUCCESS, done);
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: profile[%s] error incrementing [%s] because [%s]\n", realm, limit_key, response ? response : "");
		switch_channel_set_variable(channel, "hiredis_raw_response", response ? response : "");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	/* set expiration for interval on first increment */
	if ( interval && !strcmp("1", response ? response : "") ) {
		hiredis_profile_execute_pipeline_printf(profile, session, NULL, "expire %s %d", limit_key, interval);
	}

	switch_channel_set_variable(channel, "hiredis_raw_response", response ? response : "");

	count = atoll(response ? response : "");

	if ( switch_is_number(response ? response : "") && count <= 0 ) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "limit not positive after increment, resource = %s, val = %s\n", limit_key, response ? response : "");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "resource = %s, response = %s\n", limit_key, response ? response : "");
	}

	if ( !switch_is_number(response ? response : "") && !profile->ignore_error ) {
		/* got response error */
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	} else if ( max > 0 && count > 0 && count > max ) {
		switch_channel_set_variable(channel, "hiredis_limit_exceeded", "true");
		if ( !interval ) { /* don't need to decrement intervals if limit exceeded since the interval keys are named w/ timestamp */
			if ( profile->delete_when_zero ) {
				hiredis_profile_eval_pipeline(profile, session, NULL, DECR_DEL_SCRIPT, 1, limit_key);
			} else {
				hiredis_profile_execute_pipeline_printf(profile, session, NULL, "decr %s", limit_key);
			}
		}
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if ( !interval && count > 0 ) {
		/* only non-interval limits need to be released on session destroy */
		limit_pvt_node = switch_core_alloc(session_pool, sizeof(*limit_pvt_node));
		limit_pvt_node->realm = switch_core_strdup(session_pool, realm);
		limit_pvt_node->resource = switch_core_strdup(session_pool, resource);
		limit_pvt_node->limit_key = limit_key;
		limit_pvt_node->inc = 1;
		limit_pvt_node->interval = interval;
		limit_pvt = add_limit_pvt(session);
		limit_pvt_node->next = limit_pvt->first;
		limit_pvt->first = limit_pvt_node;
		release_limit_pvt(limit_pvt);
	}

 done:
	switch_safe_free(response);
	return status;
}

/*
  SWITCH_LIMIT_RELEASE(name) static switch_status_t name (switch_core_session_t *session, const char *realm, const char *resource)
*/
SWITCH_LIMIT_RELEASE(hiredis_limit_release)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	hiredis_profile_t *profile = NULL;
	char *response = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	hiredis_limit_pvt_t *limit_pvt = get_limit_pvt(session);

	if (!limit_pvt) {
		/* nothing to release */
		return SWITCH_STATUS_SUCCESS;
	}

	/* If realm and resource are NULL, then clear all of the limits */
	if ( zstr(realm) && zstr(resource) ) {
		hiredis_limit_pvt_node_t *cur = NULL;

		for ( cur = limit_pvt->first; cur; cur = cur->next ) {
			/* Rate limited resources are not auto-decremented, they will expire. */
			if ( !cur->interval && cur->inc ) {
				switch_status_t result;
				cur->inc = 0; /* mark as released */
				profile = switch_core_hash_find(mod_hiredis_globals.profiles, cur->realm);
				if ( profile->delete_when_zero ) {
					result = hiredis_profile_eval_pipeline(profile, session, &response, DECR_DEL_SCRIPT, 1, cur->limit_key);
				} else {
					result = hiredis_profile_execute_pipeline_printf(profile, session, &response, "decr %s", cur->limit_key);
				}
				if ( result != SWITCH_STATUS_SUCCESS ) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: profile[%s] error decrementing [%s] because [%s]\n",
									  cur->realm, cur->limit_key, response ? response : "");
				}
				switch_safe_free(response);
				response = NULL;
			}
		}
	} else if (!zstr(resource) ) {
		/* clear single non-interval resource */
		hiredis_limit_pvt_node_t *cur = NULL;
		for (cur = limit_pvt->first; cur; cur = cur->next ) {
			if ( !cur->interval && cur->inc && !strcmp(cur->resource, resource) && (zstr(realm) || !strcmp(cur->realm, realm)) ) {
				/* found the resource to clear */
				cur->inc = 0; /* mark as released */
				profile = switch_core_hash_find(mod_hiredis_globals.profiles, cur->realm);
				if (profile) {
					if ( profile->delete_when_zero ) {
						status = hiredis_profile_eval_pipeline(profile, session, &response, DECR_DEL_SCRIPT, 1, cur->limit_key);
					} else {
						status = hiredis_profile_execute_pipeline_printf(profile, session, &response, "decr %s", cur->limit_key);
					}
					if ( status != SWITCH_STATUS_SUCCESS ) {
						if ( status == SWITCH_STATUS_SOCKERR && profile->ignore_connect_fail ) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "hiredis: ignoring profile[%s] connection error decrementing [%s]\n", cur->realm, cur->limit_key);
							switch_goto_status(SWITCH_STATUS_SUCCESS, done);
						} else if ( profile->ignore_error ) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "hiredis: ignoring profile[%s] general error decrementing [%s]\n", realm, cur->limit_key);
							switch_goto_status(SWITCH_STATUS_SUCCESS, done);
						}
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "hiredis: profile[%s] error decrementing [%s] because [%s]\n", realm, cur->limit_key, response ? response : "");
						switch_channel_set_variable(channel, "hiredis_raw_response", response ? response : "");
						switch_goto_status(SWITCH_STATUS_GENERR, done);
					}

					switch_channel_set_variable(channel, "hiredis_raw_response", response ? response : "");
				}
				break;
			}
		}
	}

 done:
	release_limit_pvt(limit_pvt);
	switch_safe_free(response);
	return status;
}

/*
SWITCH_LIMIT_USAGE(name) static int name (const char *realm, const char *resource, uint32_t *rcount)
 */
SWITCH_LIMIT_USAGE(hiredis_limit_usage)
{
	hiredis_profile_t *profile = switch_core_hash_find(mod_hiredis_globals.profiles, realm);
	int64_t count = 0; /* Redis defines the incr action as to be performed on a 64 bit signed integer */
	char *response = NULL;

	if ( zstr(realm) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: realm must be defined\n");
		goto err;
	}

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: Unable to locate profile[%s]\n", realm);
		goto err;
	}

	if ( hiredis_profile_execute_pipeline_printf(profile, NULL, &response, "get %s", resource) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: profile[%s] error querying [%s] because [%s]\n", realm, resource, response ? response : "");
		goto err;
	}

	count = atoll(response ? response : "");

	switch_safe_free(response);
	return count;

 err:
	switch_safe_free(response);
	return -1;
}

/*
SWITCH_LIMIT_RESET(name) static switch_status_t name (void)
 */
SWITCH_LIMIT_RESET(hiredis_limit_reset)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: unable to globally reset hiredis limit resources. Use 'hiredis_raw set resource_name 0'\n");
	return SWITCH_STATUS_NOTIMPL;
}

/*
  SWITCH_LIMIT_INTERVAL_RESET(name) static switch_status_t name (const char *realm, const char *resource)
*/
SWITCH_LIMIT_INTERVAL_RESET(hiredis_limit_interval_reset)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hiredis: unable to reset hiredis interval limit resources.\n");
	return SWITCH_STATUS_NOTIMPL;
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

	memset(&mod_hiredis_globals, 0, sizeof(mod_hiredis_globals));
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	mod_hiredis_globals.pool = pool;
	switch_mutex_init(&mod_hiredis_globals.limit_pvt_mutex, SWITCH_MUTEX_NESTED, pool);

	switch_core_hash_init(&(mod_hiredis_globals.profiles));

	if ( mod_hiredis_do_config() != SWITCH_STATUS_SUCCESS ) {
		return SWITCH_STATUS_GENERR;
	}

	SWITCH_ADD_LIMIT(limit_interface, "hiredis", hiredis_limit_incr, hiredis_limit_release, hiredis_limit_usage,
					 hiredis_limit_reset, hiredis_limit_status, hiredis_limit_interval_reset);
	SWITCH_ADD_APP(app_interface, "hiredis_raw", "hiredis_raw", "hiredis_raw", raw_app, "", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
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
