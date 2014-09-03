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
 * Kevin Morizur <kmorizur@avgs.ca> 
 * Mathieu Rene <mrene@avgs.ca>
 *
 * mod_redis.c -- Redis limit backend
 *
 */

#include <switch.h>
#include "credis.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_redis_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_redis_shutdown);
SWITCH_MODULE_DEFINITION(mod_redis, mod_redis_load, mod_redis_shutdown, NULL);

static struct{
	char *host;
	int port;
	int timeout;
} globals;

static switch_xml_config_item_t instructions[] = {
	/* parameter name        type                 reloadable   pointer                         default value     options structure */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("host", CONFIG_RELOAD, &globals.host, NULL, "localhost", "Hostname for redis server"),	
	SWITCH_CONFIG_ITEM("port", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &globals.port, (void *) 6379, NULL,NULL, NULL),
	SWITCH_CONFIG_ITEM("timeout", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &globals.timeout, (void *) 10000, NULL,NULL, NULL),
	SWITCH_CONFIG_ITEM_END()
};

/* HASH STUFF */
typedef struct {
	switch_hash_t *hash;
	switch_mutex_t *mutex;
} limit_redis_private_t;

static switch_status_t redis_factory(REDIS *redis) 
{
	if (!((*redis) = credis_connect(globals.host, globals.port, globals.timeout))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't connect to redis server at %s:%d timeout:%d\n", globals.host, globals.port, globals.timeout);		
		return SWITCH_STATUS_FALSE;
	}
	return SWITCH_STATUS_SUCCESS;
}

/* \brief Enforces limit_redis restrictions
 * \param session current session
 * \param realm limit realm
 * \param id limit id
 * \param max maximum count
 * \param interval interval for rate limiting
 * \return SWITCH_TRUE if the access is allowed, SWITCH_FALSE if it isnt
 */
SWITCH_LIMIT_INCR(limit_incr_redis)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	limit_redis_private_t *pvt = NULL;
	int val,uuid_val;
	char *rediskey = NULL;
	char *uuid_rediskey = NULL;
	uint8_t increment = 1;
	switch_status_t status = SWITCH_STATUS_SUCCESS;	
	REDIS redis;
	
	if (redis_factory(&redis) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}
	
	/* Get the keys for redis server */
	uuid_rediskey = switch_core_session_sprintf(session,"%s_%s_%s", switch_core_get_switchname(), realm, resource);
	rediskey = switch_core_session_sprintf(session, "%s_%s", realm, resource);

	if ((pvt = switch_channel_get_private(channel, "limit_redis"))) {
		increment = !switch_core_hash_find_locked(pvt->hash, rediskey, pvt->mutex);
	} else {
		/* This is the first limit check on this channel, create a hashtable, set our prviate data and add a state handler */
		pvt = (limit_redis_private_t *) switch_core_session_alloc(session, sizeof(limit_redis_private_t));
		switch_core_hash_init(&pvt->hash);
		switch_mutex_init(&pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		switch_channel_set_private(channel, "limit_redis", pvt);
	}
	
	if (!(switch_core_hash_find_locked(pvt->hash, rediskey, pvt->mutex))) {
		switch_core_hash_insert_locked(pvt->hash, rediskey, rediskey, pvt->mutex);
	}
	
   	if (increment) {
		if (credis_incr(redis, rediskey, &val) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't increment value corresponding to %s\n", rediskey);
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}

		if (max > 0) {
			if (val > max){
				if (credis_decr(redis, rediskey, &val) != 0) {
               		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Couldn't decrement value corresponding to %s\n", rediskey);
					switch_goto_status(SWITCH_STATUS_GENERR, end);
				} else {
           			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s exceeds maximum rate of %d\n", 
						rediskey, max);
					switch_goto_status(SWITCH_STATUS_FALSE, end);
				}
			} else {
				if (credis_incr(redis, uuid_rediskey, &uuid_val) != 0) {
       				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Couldn't increment value corresponding to %s\n", uuid_rediskey);
       				switch_goto_status(SWITCH_STATUS_FALSE, end);
				}
			}	
		} else  {
			if (credis_incr(redis, uuid_rediskey, &uuid_val) != 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Couldn't increment value corresponding to %s\n", uuid_rediskey);
			switch_goto_status(SWITCH_STATUS_FALSE, end);
			}
		}
    }
/*
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "Limit incr redis : rediskey : %s val : %d max : %d\n", rediskey, val, max);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "Limit incr redis : uuid_rediskey : %s uuid_val : %d max : %d\n", uuid_rediskey,uuid_val,max);
*/
end:
	if (redis) {
		credis_close(redis);
	}
	return status;
}
	
/* !\brief Releases usage of a limit_redis-controlled ressource  */
SWITCH_LIMIT_RELEASE(limit_release_redis)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	limit_redis_private_t *pvt = switch_channel_get_private(channel, "limit_redis");
	int val, uuid_val;
	char *rediskey = NULL;
	char *uuid_rediskey = NULL;
	int status = SWITCH_STATUS_SUCCESS;
	REDIS redis;
	
	if (!pvt || !pvt->hash) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No hashtable for channel %s\n", switch_channel_get_name(channel));
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (redis_factory(&redis) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(pvt->mutex);

	/* clear for uuid */
	if (realm == NULL && resource == NULL) {
		switch_hash_index_t *hi = NULL;
		/* Loop through the channel's hashtable which contains mapping to all the limit_redis_item_t referenced by that channel */
		while ((hi = switch_core_hash_first_iter(pvt->hash, hi))) {
			void *p_val = NULL;
			const void *p_key;
			char *p_uuid_key = NULL;
			switch_ssize_t keylen;
			
			switch_core_hash_this(hi, &p_key, &keylen, &p_val);
			
			if (credis_decr(redis, (const char*)p_key, &val) != 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Couldn't decrement value corresponding to %s\n", (char *)p_key);
				switch_goto_status(SWITCH_STATUS_FALSE, end);
			}
	   		p_uuid_key = switch_core_session_sprintf(session, "%s_%s", switch_core_get_switchname(), (char *)p_key);
			if (credis_decr(redis,p_uuid_key,&uuid_val) != 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Couldn't decrement value corresponding to %s\n", p_uuid_key);
				switch_goto_status(SWITCH_STATUS_FALSE, end);
			}
			switch_core_hash_delete(pvt->hash, (const char *) p_key);
			/*
        	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "Limit release redis : rediskey : %s val : %d\n", (char *)p_val,val);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "Limit incr redis : uuid_rediskey : %s uuid_val : %d\n",
					 p_uuid_key, uuid_val);*/
		}
	
	} else {	
	   	rediskey = switch_core_session_sprintf(session, "%s_%s", realm, resource);
		uuid_rediskey = switch_core_session_sprintf(session, "%s_%s_%s", switch_core_get_switchname(), realm, resource);
		switch_core_hash_delete(pvt->hash, (const char *) rediskey);

		if (credis_decr(redis, rediskey, &val) != 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Couldn't decrement value corresponding to %s\n", rediskey);
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
		if (credis_decr(redis, uuid_rediskey, &uuid_val) != 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Couldn't decrement value corresponding to %s\n", uuid_rediskey);
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
		
/*
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Limit release redis : rediskey : %s val : %d\n", rediskey,val);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Limit incr redis : uuid_rediskey : %s uuid_val : %d\n", uuid_rediskey,uuid_val);
*/
	}
end:
	switch_mutex_unlock(pvt->mutex);
	if (redis) {
		credis_close(redis);
	}
	return status;
}

SWITCH_LIMIT_USAGE(limit_usage_redis)
{
	char *redis_key;
	char *str;
	REDIS redis;
	int usage;
	
	if (redis_factory(&redis) != SWITCH_STATUS_SUCCESS) {
		return 0;
	}

	redis_key = switch_mprintf("%s_%s", realm, resource);
  
	if (credis_get(redis, redis_key, &str) != 0){
		usage = 0;
	} else {
		usage = atoi(str);		
	}
	
	if (redis) {
		credis_close(redis);
	}
	
	switch_safe_free(redis_key);
	return usage;
}

SWITCH_LIMIT_RESET(limit_reset_redis)
{
	REDIS redis;
	if (redis_factory(&redis) == SWITCH_STATUS_SUCCESS) {
		char *rediskey = switch_mprintf("%s_*", switch_core_get_switchname());
		int dec = 0, val = 0, keyc;
		char *uuids[2000];
	
		if ((keyc = credis_keys(redis, rediskey, uuids, switch_arraylen(uuids))) > 0) {
			int i = 0;
			int hostnamelen = (int)strlen(switch_core_get_switchname())+1;
			
			for (i = 0; i < keyc && uuids[i]; i++){
				const char *key = uuids[i] + hostnamelen;
				char *value;
			
				if ((int)strlen(uuids[i]) <= hostnamelen) {
					continue; /* Sanity check */
				}
			
				credis_get(redis, key, &value);
				dec = atoi(value);
				credis_decrby(redis, key, dec, &val);
			
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "DECR %s by %d. value is now %d\n", key, dec, val);
			}
		}
		switch_safe_free(rediskey);
		credis_close(redis);
		return SWITCH_STATUS_SUCCESS;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't check/clear old redis entries\n");
		return SWITCH_STATUS_FALSE;
	}
}

SWITCH_LIMIT_STATUS(limit_status_redis)
{
	char *ret = switch_mprintf("This function is not yet available for Redis DB");
	return ret;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_redis_load)
{
	switch_limit_interface_t *limit_interface = NULL;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	if (switch_xml_config_parse_module_settings("redis.conf", SWITCH_FALSE, instructions) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	/* If FreeSWITCH was restarted and we still have active calls, decrement them so our global count stays valid */
	limit_reset_redis();
	
	SWITCH_ADD_LIMIT(limit_interface, "redis", limit_incr_redis, limit_release_redis, limit_usage_redis, limit_reset_redis, limit_status_redis, NULL);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_redis_shutdown)
{

	switch_xml_config_cleanup(instructions);

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
