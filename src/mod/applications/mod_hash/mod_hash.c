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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Ken Rice <krice at suspicious dot org
 * Mathieu Rene <mathieu.rene@gmail.com>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Rupa Schomaker <rupa@rupa.com>
 *
 * mod_hash.c -- Hash api, hash backend for limit
 *
 */

#include <switch.h>

#define LIMIT_HASH_CLEANUP_INTERVAL 900

SWITCH_MODULE_LOAD_FUNCTION(mod_hash_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_hash_shutdown);
SWITCH_MODULE_DEFINITION(mod_hash, mod_hash_load, mod_hash_shutdown, NULL);

/* CORE STUFF */
static struct {
	switch_memory_pool_t *pool;
	switch_thread_rwlock_t *limit_hash_rwlock;
	switch_hash_t *limit_hash;
	switch_thread_rwlock_t *db_hash_rwlock;
	switch_hash_t *db_hash;
} globals;

typedef struct {
	uint32_t total_usage;
	uint32_t rate_usage;
	time_t last_check;
	uint32_t interval;
} limit_hash_item_t;

struct callback {
	char *buf;
	size_t len;
	int matches;
};

typedef struct callback callback_t;

/* HASH STUFF */
typedef struct {
	switch_hash_t *hash;
} limit_hash_private_t;

/* \brief Enforces limit_hash restrictions
 * \param session current session
 * \param realm limit realm
 * \param id limit id
 * \param max maximum count
 * \param interval interval for rate limiting
 * \return SWITCH_TRUE if the access is allowed, SWITCH_FALSE if it isnt
 */
SWITCH_LIMIT_INCR(limit_incr_hash)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *hashkey = NULL;
	switch_bool_t status = SWITCH_STATUS_SUCCESS;
	limit_hash_item_t *item = NULL;
	time_t now = switch_epoch_time_now(NULL);
	limit_hash_private_t *pvt = NULL;
	uint8_t increment = 1;

	hashkey = switch_core_session_sprintf(session, "%s_%s", realm, resource);

	switch_thread_rwlock_wrlock(globals.limit_hash_rwlock);
	/* Check if that realm+resource has ever been checked */
	if (!(item = (limit_hash_item_t *) switch_core_hash_find(globals.limit_hash, hashkey))) {
		/* No, create an empty structure and add it, then continue like as if it existed */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "Creating new limit structure: key: %s\n", hashkey);
		item = (limit_hash_item_t *) malloc(sizeof(limit_hash_item_t));
		switch_assert(item);
		memset(item, 0, sizeof(limit_hash_item_t));
		switch_core_hash_insert(globals.limit_hash, hashkey, item);
	}

	/* Did we already run on this channel before? */
	if ((pvt = switch_channel_get_private(channel, "limit_hash"))) {
		/* Yes, but check if we did that realm+resource
		   If we didnt, allow incrementing the counter.
		   If we did, dont touch it but do the validation anyways
		 */
		increment = !switch_core_hash_find(pvt->hash, hashkey);
	} else {
		/* This is the first limit check on this channel, create a hashtable, set our prviate data */
		pvt = (limit_hash_private_t *) switch_core_session_alloc(session, sizeof(limit_hash_private_t));
		memset(pvt, 0, sizeof(limit_hash_private_t));
		switch_core_hash_init(&pvt->hash, switch_core_session_get_pool(session));
		switch_channel_set_private(channel, "limit_hash", pvt);
	}

	if (interval > 0) {
		item->interval = interval;
		if (item->last_check <= (now - interval)) {
			item->rate_usage = 1;
			item->last_check = now;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "Usage for %s reset to 1\n",
							  hashkey);
		} else {
			/* Always increment rate when its checked as it doesnt depend on the channel */
			item->rate_usage++;

			if ((max >= 0) && (item->rate_usage > (uint32_t) max)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s exceeds maximum rate of %d/%ds, now at %d\n",
								  hashkey, max, interval, item->rate_usage);
				status = SWITCH_STATUS_GENERR;
				goto end;
			}
		}
	} else if ((max >= 0) && (item->total_usage + increment > (uint32_t) max)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s is already at max value (%d)\n", hashkey, item->total_usage);
		status = SWITCH_STATUS_GENERR;
		goto end;
	}

	if (increment) {
		item->total_usage++;

		switch_core_hash_insert(pvt->hash, hashkey, item);

		if (max == -1) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s is now %d\n", hashkey, item->total_usage);
		} else if (interval == 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s is now %d/%d\n", hashkey, item->total_usage, max);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s is now %d/%d for the last %d seconds\n", hashkey,
							  item->rate_usage, max, interval);
		}

		switch_limit_fire_event("hash", realm, resource, item->total_usage, item->rate_usage, max, max >= 0 ? (uint32_t) max : 0);
	}

	/* Save current usage & rate into channel variables so it can be used later in the dialplan, or added to CDR records */
	{
		const char *susage = switch_core_session_sprintf(session, "%d", item->total_usage);
		const char *srate = switch_core_session_sprintf(session, "%d", item->rate_usage);

		switch_channel_set_variable(channel, "limit_usage", susage);
		switch_channel_set_variable(channel, switch_core_session_sprintf(session, "limit_usage_%s", hashkey), susage);

		switch_channel_set_variable(channel, "limit_rate", srate);
		switch_channel_set_variable(channel, switch_core_session_sprintf(session, "limit_rate_%s", hashkey), srate);
	}

  end:
	switch_thread_rwlock_unlock(globals.limit_hash_rwlock);
	return status;
}

/* !\brief Determines whether a given entry is ready to be removed. */
SWITCH_HASH_DELETE_FUNC(limit_hash_cleanup_delete_callback) {
	limit_hash_item_t *item = (limit_hash_item_t *) val;
	time_t now = switch_epoch_time_now(NULL);

	/* reset to 0 if window has passed so we can clean it up */
	if (item->rate_usage > 0 && (item->last_check <= (now - item->interval))) {
		item->rate_usage = 0;
	}

	if (item->total_usage == 0 && item->rate_usage == 0) {
		/* Noone is using this item anymore */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Freeing limit item: %s\n", (const char *) key);
		
		free(item);
		return SWITCH_TRUE;
	}
	
	return SWITCH_FALSE;
}

/* !\brief Periodically checks for unused limit entries and frees them */
SWITCH_STANDARD_SCHED_FUNC(limit_hash_cleanup_callback)
{
	switch_thread_rwlock_wrlock(globals.limit_hash_rwlock);
	switch_core_hash_delete_multi(globals.limit_hash, limit_hash_cleanup_delete_callback, NULL);
	switch_thread_rwlock_unlock(globals.limit_hash_rwlock);
	
	task->runtime = switch_epoch_time_now(NULL) + LIMIT_HASH_CLEANUP_INTERVAL;
}

/* !\brief Releases usage of a limit_hash-controlled ressource  */
SWITCH_LIMIT_RELEASE(limit_release_hash)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	limit_hash_private_t *pvt = switch_channel_get_private(channel, "limit_hash");
	limit_hash_item_t *item = NULL;
	switch_hash_index_t *hi;
	char *hashkey = NULL;

	if (!pvt || !pvt->hash) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_thread_rwlock_wrlock(globals.limit_hash_rwlock);

	/* clear for uuid */
	if (realm == NULL && resource == NULL) {
		/* Loop through the channel's hashtable which contains mapping to all the limit_hash_item_t referenced by that channel */
		while ((hi = switch_hash_first(NULL, pvt->hash))) {
			void *val = NULL;
			const void *key;
			switch_ssize_t keylen;
			limit_hash_item_t *item = NULL;

			switch_hash_this(hi, &key, &keylen, &val);

			item = (limit_hash_item_t *) val;
			item->total_usage--;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s is now %d\n", (const char *) key, item->total_usage);

			if (item->total_usage == 0 && item->rate_usage == 0) {
				/* Noone is using this item anymore */
				switch_core_hash_delete(globals.limit_hash, (const char *) key);
				free(item);
			}

			switch_core_hash_delete(pvt->hash, (const char *) key);
		}
	} else {
		hashkey = switch_core_session_sprintf(session, "%s_%s", realm, resource);

		if ((item = (limit_hash_item_t *) switch_core_hash_find(pvt->hash, hashkey))) {
			item->total_usage--;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s is now %d\n", (const char *) hashkey, item->total_usage);

			switch_core_hash_delete(pvt->hash, hashkey);

			if (item->total_usage == 0 && item->rate_usage == 0) {
				/* Noone is using this item anymore */
				switch_core_hash_delete(globals.limit_hash, (const char *) hashkey);
				free(item);
			}
		}
	}

	switch_thread_rwlock_unlock(globals.limit_hash_rwlock);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_LIMIT_USAGE(limit_usage_hash)
{
	char *hash_key = NULL;
	limit_hash_item_t *item = NULL;
	int count = 0;

	switch_thread_rwlock_rdlock(globals.limit_hash_rwlock);

	hash_key = switch_mprintf("%s_%s", realm, resource);

	if ((item = switch_core_hash_find(globals.limit_hash, hash_key))) {
		count = item->total_usage;
		*rcount = item->rate_usage;
	}

 	switch_safe_free(hash_key);
	switch_thread_rwlock_unlock(globals.limit_hash_rwlock);

	return count;
}

SWITCH_LIMIT_RESET(limit_reset_hash)
{
	return SWITCH_STATUS_GENERR;
}

SWITCH_LIMIT_STATUS(limit_status_hash)
{
	/*
	switch_hash_index_t *hi = NULL;
	int count = 0;
	char *ret = NULL;
	
	switch_thread_rwlock_rdlock(globals.limit_hash_rwlock);
	
	for (hi = switch_hash_first(NULL, globals.limit_hash); hi; switch_hash_next(hi)) {
		count++;
	}
	
	switch_thread_rwlock_unlock(globals.limit_hash_rwlock);
	
	ret = switch_mprintf("There are %d elements being tracked.", count);
	return ret;
	*/
	return strdup("-ERR not supported yet (locking problems).");
}

/* APP/API STUFF */

/* CORE HASH STUFF */

#define HASH_USAGE "[insert|delete]/<realm>/<key>/<val>"
#define HASH_DESC "save data"

SWITCH_STANDARD_APP(hash_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *hash_key = NULL;
	char *value = NULL;

	switch_thread_rwlock_wrlock(globals.db_hash_rwlock);

	if (!zstr(data)) {
		mydata = strdup(data);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, '/', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 3 || !argv[0]) {
		goto usage;
	}

	hash_key = switch_mprintf("%s_%s", argv[1], argv[2]);

	if (!strcasecmp(argv[0], "insert")) {
		if (argc < 4) {
			goto usage;
		}
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			free(value);
			switch_core_hash_delete(globals.db_hash, hash_key);
		}
		value = strdup(argv[3]);
		switch_assert(value);
		switch_core_hash_insert(globals.db_hash, hash_key, value);
	} else if (!strcasecmp(argv[0], "delete")) {
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			switch_safe_free(value);
			switch_core_hash_delete(globals.db_hash, hash_key);
		}
	} else {
		goto usage;
	}

	goto done;

  usage:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "USAGE: hash %s\n", HASH_USAGE);

  done:
	switch_thread_rwlock_unlock(globals.db_hash_rwlock);
	switch_safe_free(mydata);
	switch_safe_free(hash_key);
}

#define HASH_API_USAGE "insert|select|delete/realm/key[/value]"
SWITCH_STANDARD_API(hash_api_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *value = NULL;
	char *hash_key = NULL;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, '/', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 3 || !argv[0]) {
		goto usage;
	}

	hash_key = switch_mprintf("%s_%s", argv[1], argv[2]);

	if (!strcasecmp(argv[0], "insert")) {
		if (argc < 4) {
			goto usage;
		}
		switch_thread_rwlock_wrlock(globals.db_hash_rwlock);
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			switch_safe_free(value);
			switch_core_hash_delete(globals.db_hash, hash_key);
		}
		value = strdup(argv[3]);
		switch_assert(value);
		switch_core_hash_insert(globals.db_hash, hash_key, value);
		stream->write_function(stream, "+OK\n");
		switch_thread_rwlock_unlock(globals.db_hash_rwlock);
	} else if (!strcasecmp(argv[0], "delete")) {
		switch_thread_rwlock_wrlock(globals.db_hash_rwlock);
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			switch_safe_free(value);
			switch_core_hash_delete(globals.db_hash, hash_key);
			stream->write_function(stream, "+OK\n");
		} else {
			stream->write_function(stream, "-ERR Not found\n");
		}
		switch_thread_rwlock_unlock(globals.db_hash_rwlock);
	} else if (!strcasecmp(argv[0], "select")) {
		switch_thread_rwlock_rdlock(globals.db_hash_rwlock);
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			stream->write_function(stream, "%s", value);
		}
		switch_thread_rwlock_unlock(globals.db_hash_rwlock);
	} else {
		goto usage;
	}

	goto done;

  usage:
	stream->write_function(stream, "-ERR Usage: hash %s\n", HASH_API_USAGE);

  done:

	switch_safe_free(mydata);
	switch_safe_free(hash_key);

	return SWITCH_STATUS_SUCCESS;
}

/* INIT/DEINIT STUFF */

SWITCH_MODULE_LOAD_FUNCTION(mod_hash_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *commands_api_interface;
	switch_limit_interface_t *limit_interface;
	switch_status_t status;

	memset(&globals, 0, sizeof(&globals));
	globals.pool = pool;

	status = switch_event_reserve_subclass(LIMIT_EVENT_USAGE);
	if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_INUSE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register event subclass \"%s\" (%d)\n", LIMIT_EVENT_USAGE, status);
		return SWITCH_STATUS_FALSE;
	}

	switch_thread_rwlock_create(&globals.limit_hash_rwlock, globals.pool);
	switch_thread_rwlock_create(&globals.db_hash_rwlock, globals.pool);
	switch_core_hash_init(&globals.limit_hash, pool);
	switch_core_hash_init(&globals.db_hash, pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* register limit interfaces */
	SWITCH_ADD_LIMIT(limit_interface, "hash", limit_incr_hash, limit_release_hash, limit_usage_hash, limit_reset_hash, limit_status_hash);

	switch_scheduler_add_task(switch_epoch_time_now(NULL) + LIMIT_HASH_CLEANUP_INTERVAL, limit_hash_cleanup_callback, "limit_hash_cleanup", "mod_hash", 0, NULL,
						  SSHF_NONE);
	
	SWITCH_ADD_APP(app_interface, "hash", "Insert into the hashtable", HASH_DESC, hash_function, HASH_USAGE, SAF_SUPPORT_NOMEDIA)
	SWITCH_ADD_API(commands_api_interface, "hash", "hash get/set", hash_api_function, "[insert|delete|select]/<realm>/<key>/<value>");
	switch_console_set_complete("add hash insert");
	switch_console_set_complete("add hash delete");
	switch_console_set_complete("add hash select");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
	
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_hash_shutdown)
{
	switch_scheduler_del_task_group("mod_hash");

	switch_thread_rwlock_destroy(globals.db_hash_rwlock);
	switch_thread_rwlock_destroy(globals.limit_hash_rwlock);

	switch_core_hash_destroy(&globals.limit_hash);
	switch_core_hash_destroy(&globals.db_hash);

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
