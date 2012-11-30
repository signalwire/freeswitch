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
 * 
 * Csaba Zelei <csaba.zelei@eworldcom.net>
 *
 * Thanks for eWorld Com Ltd. for funding the module. 
 *
 * mod_spy.c -- UserSpy Module
 *
 */

#include <switch.h>

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_spy_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_spy_load);

SWITCH_MODULE_DEFINITION(mod_spy, mod_spy_load, mod_spy_shutdown, NULL);

struct mod_spy_globals {
	switch_memory_pool_t *pool;
	switch_event_node_t *node;
	switch_hash_t *spy_hash;
	switch_thread_rwlock_t *spy_hash_lock;
	uint32_t spy_count;
} globals;

typedef struct spy {
	const char *uuid;
	struct spy *next;
} spy_t;


static switch_status_t spy_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *data = switch_channel_get_private(channel, "_userspy_");
	const char *uuid = switch_core_session_get_uuid(session);
	spy_t *spy = NULL, *p = NULL, *prev = NULL;

	switch_thread_rwlock_wrlock(globals.spy_hash_lock);

	spy = switch_core_hash_find(globals.spy_hash, data);
	for (p = spy; p; p = p->next) {
		if (p->uuid == uuid) {
			if (prev) {
				prev->next = p->next;
			} else {
				spy = p->next;
			}
			globals.spy_count--;
			break;
		}
		prev = p;
	}

	switch_core_hash_insert(globals.spy_hash, data, spy);

	switch_thread_rwlock_unlock(globals.spy_hash_lock);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t spy_on_exchange_media(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *spy_uuid = switch_channel_get_variable(channel, "spy_uuid");

	if (spy_uuid) {
		if (switch_ivr_eavesdrop_session(session, spy_uuid, NULL, ED_DTMF) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't eavesdrop on uuid %s\n", spy_uuid);
		}
	}

	switch_channel_set_state(channel, CS_PARK);
	return SWITCH_STATUS_FALSE;
}

static switch_status_t spy_on_park(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *moh = switch_channel_get_hold_music(channel);

	while (switch_channel_ready(channel) && switch_channel_get_state(channel) == CS_PARK) {
		if (moh) {
			switch_status_t status = switch_ivr_play_file(session, NULL, moh, NULL);
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				break;
			}
		}
	}
	return SWITCH_STATUS_FALSE;
}


static const switch_state_handler_table_t spy_state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ spy_on_hangup,
	/*.on_exchange_media */ spy_on_exchange_media,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/* on_hibernate */ NULL,
	/* on reset */ NULL,
	/* on_park */ spy_on_park,
};

SWITCH_STANDARD_API(dump_hash)
{
	switch_hash_index_t *hi;
	const void *key;
	void *val;
	spy_t *spy;

	switch_thread_rwlock_rdlock(globals.spy_hash_lock);

	for (hi = switch_hash_first(NULL, globals.spy_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &key, NULL, &val);
		spy = (spy_t *) val;

		stream->write_function(stream, "%s :");
		while (spy) {
			stream->write_function(stream, " %s", spy->uuid);
			spy = spy->next;
		}
		stream->write_function(stream, "\n");
	}

	stream->write_function(stream, "\n%d total spy\n", globals.spy_count);
	switch_thread_rwlock_unlock(globals.spy_hash_lock);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t process_event(switch_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_core_session_t *session = NULL;
	char *username[3] = { NULL };
	char *domain[3] = { NULL };
	char key[512];
	char *uuid = NULL, *my_uuid = NULL;
	int i;


	switch_thread_rwlock_rdlock(globals.spy_hash_lock);

	if (!globals.spy_count) {
		goto done;
	}

	username[0] = switch_event_get_header(event, "Caller-Username");
	domain[0] = switch_event_get_header(event, "variable_domain_name");

	username[1] = switch_event_get_header(event, "variable_dialed_user");
	domain[1] = switch_event_get_header(event, "variable_dialed_domain");

	username[2] = switch_event_get_header(event, "variable_user_name");
	domain[2] = switch_event_get_header(event, "variable_domain_name");

	for (i = 0; i < 3; i++) {

		if (username[i] && domain[i]) {
			spy_t *spy = NULL;
			switch_snprintf(key, sizeof(key), "%s@%s", username[i], domain[i]);

			if ((spy = switch_core_hash_find(globals.spy_hash, key))) {
				while (spy) {
					if ((session = switch_core_session_locate(spy->uuid))) {
						switch_channel_t *channel = switch_core_session_get_channel(session);
						
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "UserSpy retrieved uuid %s for key %s, activating eavesdrop\n", uuid, key);
						my_uuid = switch_event_get_header(event, "Unique-ID");
						
						switch_channel_set_variable(channel, "spy_uuid", my_uuid);
						
						switch_channel_set_state(channel, CS_EXCHANGE_MEDIA);
						switch_channel_set_flag(channel, CF_BREAK);
						
						switch_core_session_rwunlock(session);
					}
					spy = spy->next;
				}
				break;
			} else {
				/* not found */
				status = SWITCH_STATUS_FALSE;
			}
		}
	}
	
	
 done:
	switch_thread_rwlock_unlock(globals.spy_hash_lock);
	return status;


}

static void event_handler(switch_event_t *event)
{
	if (process_event(event) != SWITCH_STATUS_SUCCESS) {
		const char *peer_uuid = switch_event_get_header(event, "variable_signal_bond");
		switch_core_session_t *peer_session = NULL;
		switch_channel_t *peer_channel = NULL;
		switch_event_t *peer_event = NULL;
		
		if (!peer_uuid) {
			return;
		}

		if (!(peer_session = switch_core_session_locate(peer_uuid))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Can't locate peer session for uuid %s\n", peer_uuid);
			return;
		}

		peer_channel = switch_core_session_get_channel(peer_session);
		
		if (switch_event_create(&peer_event, SWITCH_EVENT_CHANNEL_BRIDGE) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't create bridge event for peer channel %s\n", peer_uuid);
			goto end;
		}

		switch_channel_event_set_data(peer_channel, peer_event);

	end:
		switch_core_session_rwunlock(peer_session);

		if (peer_event) {
			process_event(peer_event);
			switch_event_destroy(&peer_event);
		}
	}
}

#define USERSPY_SYNTAX "<user@domain> [uuid]"
SWITCH_STANDARD_APP(userspy_function)
{
	int argc = 0;
	char *argv[2] = { 0 };
	char *params = NULL;

	if (!zstr(data) && (params = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(params, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			char *uuid = switch_core_session_get_uuid(session);
			switch_status_t status;
			spy_t *spy = NULL;

			spy = switch_core_session_alloc(session, sizeof(spy_t));
			switch_assert(spy != NULL);
			spy->uuid = uuid;

			switch_thread_rwlock_wrlock(globals.spy_hash_lock);

			spy->next = (spy_t *) switch_core_hash_find(globals.spy_hash, argv[0]);

			status = switch_core_hash_insert(globals.spy_hash, argv[0], (void *) spy);

			if ((status != SWITCH_STATUS_SUCCESS)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't insert to spy hash\n");
				switch_channel_hangup(channel, SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED);
				switch_thread_rwlock_unlock(globals.spy_hash_lock);
				return;
			}

			globals.spy_count++;
			switch_thread_rwlock_unlock(globals.spy_hash_lock);

			switch_channel_set_private(channel, "_userspy_", (void *) argv[0]);
			switch_channel_add_state_handler(channel, &spy_state_handlers);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "UserSpy activated on %s \n", argv[0]);

			if (argv[1]) {
				switch_channel_set_variable(channel, "spy_uuid", argv[1]);
				switch_channel_set_state(channel, CS_EXCHANGE_MEDIA);
				return;
			}

			switch_channel_set_state(channel, CS_PARK);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", USERSPY_SYNTAX);
	}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_spy_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	globals.pool = pool;

	switch_core_hash_init(&globals.spy_hash, pool);
	switch_thread_rwlock_create(&globals.spy_hash_lock, pool);
	globals.spy_count = 0;

	switch_event_bind_removable(modname, SWITCH_EVENT_CHANNEL_BRIDGE, NULL, event_handler, NULL, &globals.node);

	SWITCH_ADD_APP(app_interface, "userspy", "Spy on a user constantly", "Spy on a user constantly", userspy_function, USERSPY_SYNTAX, SAF_NONE);
	SWITCH_ADD_API(api_interface, "userspy_show", "Show current spies", dump_hash, "userspy_show");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_spy_shutdown)
{
	int sanity = 0;

	while (globals.spy_count) {
		switch_cond_next();
		if (++sanity >= 60000) {
			break;
		}
	}

	switch_event_unbind(&globals.node);
	switch_core_hash_destroy(&globals.spy_hash);
	switch_thread_rwlock_destroy(globals.spy_hash_lock);

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
