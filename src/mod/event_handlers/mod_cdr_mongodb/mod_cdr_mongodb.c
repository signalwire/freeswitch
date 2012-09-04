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
 * Daniel Swarbrick <daniel.swarbrick@seventhsignal.de>
 * 
 * mod_cdr_mongodb.c -- MongoDB CDR Module
 *
 * Derived from:
 * mod_xml_cdr.c -- XML CDR Module to files or curl
 *
 */
#include <switch.h>
#include <mongo.h>

static struct {
	switch_memory_pool_t *pool;
	int shutdown;
	char *mongo_host;
	uint32_t mongo_port;
	char *mongo_namespace;
	mongo mongo_conn[1];
	switch_mutex_t *mongo_mutex;
	switch_bool_t log_b;
} globals;

static switch_xml_config_item_t config_settings[] = {
	/* key, flags, ptr, default_value, syntax, helptext */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("host", CONFIG_REQUIRED, &globals.mongo_host, "127.0.0.1", NULL, "MongoDB server host address"),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("namespace", CONFIG_REQUIRED, &globals.mongo_namespace, NULL, "database.collection", "MongoDB namespace"),

	/* key, type, flags, ptr, default_value, data, syntax, helptext */
	SWITCH_CONFIG_ITEM("port", SWITCH_CONFIG_INT, CONFIG_REQUIRED, &globals.mongo_port, 27017, NULL, NULL, "MongoDB server TCP port"),
	SWITCH_CONFIG_ITEM("log-b-leg", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE, &globals.log_b, SWITCH_TRUE, NULL, NULL, "Log B-leg in addition to A-leg"),

	SWITCH_CONFIG_ITEM_END()
};


SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_mongodb_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_mongodb_shutdown);
SWITCH_MODULE_DEFINITION(mod_cdr_mongodb, mod_cdr_mongodb_load, mod_cdr_mongodb_shutdown, NULL);

static void set_bson_profile_data(bson *b, switch_caller_profile_t *caller_profile)
{
	bson_append_string(b, "username", caller_profile->username);
	bson_append_string(b, "dialplan", caller_profile->dialplan);
	bson_append_string(b, "caller_id_name", caller_profile->caller_id_name);
	bson_append_string(b, "ani", caller_profile->ani);
	bson_append_string(b, "aniii", caller_profile->aniii);
	bson_append_string(b, "caller_id_number", caller_profile->caller_id_number);
	bson_append_string(b, "network_addr", caller_profile->network_addr);
	bson_append_string(b, "rdnis", caller_profile->rdnis);
	bson_append_string(b, "destination_number", caller_profile->destination_number);
	bson_append_string(b, "uuid", caller_profile->uuid);
	bson_append_string(b, "source", caller_profile->source);
	bson_append_string(b, "context", caller_profile->context);
	bson_append_string(b, "chan_name", caller_profile->chan_name);
}


static switch_status_t my_on_reporting(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_header_t *hi;
	switch_caller_profile_t *caller_profile;
	switch_app_log_t *app_log;
	bson cdr;
	int is_b;
	char *tmp;

	if (globals.shutdown) {
		return SWITCH_STATUS_SUCCESS;
	}

	is_b = channel && switch_channel_get_originator_caller_profile(channel);
	if (!globals.log_b && is_b) {
		const char *force_cdr = switch_channel_get_variable(channel, SWITCH_FORCE_PROCESS_CDR_VARIABLE);
		if (!switch_true(force_cdr)) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	bson_init(&cdr);

	/* Channel data */
	bson_append_start_object(&cdr, "channel_data");
	bson_append_string(&cdr, "state", switch_channel_state_name(switch_channel_get_state(channel)));
	bson_append_string(&cdr, "direction", switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND ? "outbound" : "inbound");
	bson_append_int(&cdr, "state_number", switch_channel_get_state(channel));

	if ((tmp = switch_channel_get_flag_string(channel))) {
		bson_append_string(&cdr, "flags", tmp);
		free(tmp);
	}

	if ((tmp = switch_channel_get_cap_string(channel))) {
		bson_append_string(&cdr, "caps", tmp);
		free(tmp);
	}
	bson_append_finish_object(&cdr);				/* channel_data */


	/* Channel variables */
	bson_append_start_object(&cdr, "variables");

	if ((hi = switch_channel_variable_first(channel))) {
		for (; hi; hi = hi->next) {
			if (!zstr(hi->name) && !zstr(hi->value)) {
				bson_append_string(&cdr, hi->name, hi->value);
			}
		}
		switch_channel_variable_last(channel);
	}

	bson_append_finish_object(&cdr);				/* variables */


	/* App log */
	if ((app_log = switch_core_session_get_app_log(session))) {
		switch_app_log_t *ap;

		bson_append_start_object(&cdr, "app_log");
		for (ap = app_log; ap; ap = ap->next) {
			bson_append_start_object(&cdr, "application");
			bson_append_string(&cdr, "app_name", ap->app);
			bson_append_string(&cdr, "app_data", switch_str_nil(ap->arg));
			bson_append_long(&cdr, "app_stamp", ap->stamp);
			bson_append_finish_object(&cdr);		/* application */
		}

		bson_append_finish_object(&cdr);			/* app_log */
	}


	/* Callflow */
	caller_profile = switch_channel_get_caller_profile(channel);

	while (caller_profile) {
		bson_append_start_object(&cdr, "callflow");

		if (!zstr(caller_profile->dialplan)) {
			bson_append_string(&cdr, "dialplan", caller_profile->dialplan);
		}

		if (!zstr(caller_profile->profile_index)) {
			bson_append_string(&cdr, "profile_index", caller_profile->profile_index);
		}

		if (caller_profile->caller_extension) {
			switch_caller_application_t *ap;

			bson_append_start_object(&cdr, "extension");

			bson_append_string(&cdr, "name", caller_profile->caller_extension->extension_name);
			bson_append_string(&cdr, "number", caller_profile->caller_extension->extension_number);

			if (caller_profile->caller_extension->current_application) {
				bson_append_string(&cdr, "current_app", caller_profile->caller_extension->current_application->application_name);
			}

			for (ap = caller_profile->caller_extension->applications; ap; ap = ap->next) {
				bson_append_start_object(&cdr, "application");
				if (ap == caller_profile->caller_extension->current_application) {
					bson_append_bool(&cdr, "last_executed", 1);
				}
				bson_append_string(&cdr, "app_name", ap->application_name);
				bson_append_string(&cdr, "app_data", switch_str_nil(ap->application_data));
				bson_append_finish_object(&cdr);
			}

			if (caller_profile->caller_extension->children) {
				switch_caller_profile_t *cp = NULL;

				for (cp = caller_profile->caller_extension->children; cp; cp = cp->next) {

					if (!cp->caller_extension) {
						continue;
					}

					bson_append_start_object(&cdr, "sub_extensions");
					bson_append_start_object(&cdr, "extension");

					bson_append_string(&cdr, "name", cp->caller_extension->extension_name);
					bson_append_string(&cdr, "number", cp->caller_extension->extension_number);
					bson_append_string(&cdr, "dialplan", cp->dialplan);
					if (cp->caller_extension->current_application) {
						bson_append_string(&cdr, "current_app", cp->caller_extension->current_application->application_name);
					}

					for (ap = cp->caller_extension->applications; ap; ap = ap->next) {
						bson_append_start_object(&cdr, "application");
						if (ap == cp->caller_extension->current_application) {
							bson_append_bool(&cdr, "last_executed", 1);
						}
						bson_append_string(&cdr, "app_name", ap->application_name);
						bson_append_string(&cdr, "app_data", switch_str_nil(ap->application_data));
						bson_append_finish_object(&cdr);
					}

					bson_append_finish_object(&cdr);	/* extension */
					bson_append_finish_object(&cdr);	/* sub_extensions */
				}
			}

			bson_append_finish_object(&cdr);			/* extension */
		}

		bson_append_start_object(&cdr, "caller_profile");
		set_bson_profile_data(&cdr, caller_profile);

		if (caller_profile->origination_caller_profile) {
			switch_caller_profile_t *cp = NULL;

			bson_append_start_object(&cdr, "origination");
			for (cp = caller_profile->origination_caller_profile; cp; cp = cp->next) {
				bson_append_start_object(&cdr, "origination_caller_profile");
				set_bson_profile_data(&cdr, cp);
				bson_append_finish_object(&cdr);
			}
			bson_append_finish_object(&cdr);			/* origination */
		}

		if (caller_profile->originator_caller_profile) {
			switch_caller_profile_t *cp = NULL;

			bson_append_start_object(&cdr, "originator");
			for (cp = caller_profile->originator_caller_profile; cp; cp = cp->next) {
				bson_append_start_object(&cdr, "originator_caller_profile");
				set_bson_profile_data(&cdr, cp);
				bson_append_finish_object(&cdr);
			}
			bson_append_finish_object(&cdr);			/* originator */
		}

		if (caller_profile->originatee_caller_profile) {
			switch_caller_profile_t *cp = NULL;

			bson_append_start_object(&cdr, "originatee");
			for (cp = caller_profile->originatee_caller_profile; cp; cp = cp->next) {
				bson_append_start_object(&cdr, "originatee_caller_profile");
				set_bson_profile_data(&cdr, cp);
				bson_append_finish_object(&cdr);
			}
			bson_append_finish_object(&cdr);			/* originatee */
		}

		bson_append_finish_object(&cdr);				/* caller_profile */

		/* Timestamps */
		if (caller_profile->times) {
			bson_append_start_object(&cdr, "times");

			/* Insert timestamps as long ints (microseconds) to preserve accuracy */
			bson_append_long(&cdr, "created_time", caller_profile->times->created);
			bson_append_long(&cdr, "profile_created_time", caller_profile->times->profile_created);
			bson_append_long(&cdr, "progress_time", caller_profile->times->progress);
			bson_append_long(&cdr, "progress_media_time", caller_profile->times->progress_media);
			bson_append_long(&cdr, "answered_time", caller_profile->times->answered);
			bson_append_long(&cdr, "bridged_time", caller_profile->times->bridged);
			bson_append_long(&cdr, "last_hold_time", caller_profile->times->last_hold);
			bson_append_long(&cdr, "hold_accum_time", caller_profile->times->hold_accum);
			bson_append_long(&cdr, "hangup_time", caller_profile->times->hungup);
			bson_append_long(&cdr, "resurrect_time", caller_profile->times->resurrected);
			bson_append_long(&cdr, "transfer_time", caller_profile->times->transferred);
			bson_append_finish_object(&cdr);			/* times */
		}

		bson_append_finish_object(&cdr);				/* callflow */
		caller_profile = caller_profile->next;
	}

	bson_finish(&cdr);

	switch_mutex_lock(globals.mongo_mutex);

	if (mongo_insert(globals.mongo_conn, globals.mongo_namespace, &cdr) != MONGO_OK) {
		if (globals.mongo_conn->err == MONGO_IO_ERROR) {
			mongo_error_t db_status;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "MongoDB connection failed; attempting reconnect...\n");
			db_status = mongo_reconnect(globals.mongo_conn);

			if (db_status != MONGO_OK) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MongoDB reconnect failed with error code %d\n", db_status);
				status = SWITCH_STATUS_FALSE;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "MongoDB connection re-established.\n");
				if (mongo_insert(globals.mongo_conn, globals.mongo_namespace, &cdr) != MONGO_OK) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mongo_insert: error code %d\n", globals.mongo_conn->err);
					status = SWITCH_STATUS_FALSE;
				}
			}

		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mongo_insert: error code %d\n", globals.mongo_conn->err);
			status = SWITCH_STATUS_FALSE;
		}
	}

	switch_mutex_unlock(globals.mongo_mutex);
	bson_destroy(&cdr);

	return status;
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


static switch_status_t load_config(switch_memory_pool_t *pool)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_xml_config_parse_module_settings("cdr_mongodb.conf", SWITCH_FALSE, config_settings) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	return status;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_mongodb_load)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mongo_error_t db_status;

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;
	if (load_config(pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to load or parse config!\n");
		return SWITCH_STATUS_FALSE;
	}

	db_status = mongo_connect(globals.mongo_conn, globals.mongo_host, globals.mongo_port);

	if (db_status != MONGO_OK) {
		switch (globals.mongo_conn->err) {
			case MONGO_CONN_NO_SOCKET:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mongo_connect: no socket\n");
				break;
			case MONGO_CONN_FAIL:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mongo_connect: connection failed\n");
				break;
			case MONGO_CONN_NOT_MASTER:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mongo_connect: not master\n");
				break;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mongo_connect: unknown error %d\n", db_status);
		}
		return SWITCH_STATUS_FALSE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connected to MongoDB server %s:%d\n", globals.mongo_host, globals.mongo_port);
	}

	switch_mutex_init(&globals.mongo_mutex, SWITCH_MUTEX_NESTED, pool);

	switch_core_add_state_handler(&state_handlers);
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	return status;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_mongodb_shutdown)
{
	globals.shutdown = 1;
	switch_core_remove_state_handler(&state_handlers);
	switch_mutex_destroy(globals.mongo_mutex);

	mongo_destroy(globals.mongo_conn);

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
