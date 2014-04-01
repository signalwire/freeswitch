/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2014, Grasshopper
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
 * The Original Code is mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * rayo_cpa_detector.c -- Glue to normalize events from and to allow multiple instantiation of various detectors in FreeSWITCH
 */

#include "rayo_cpa_detector.h"

static struct {
	/** detectors supported by this module mapped by signal-type */
	switch_hash_t *detectors;
	/** synchronizes access to detectors */
	switch_mutex_t *detectors_mutex;
} globals;

struct rayo_cpa_detector;

/**
 * Detector definition
 */
struct rayo_cpa_detector {
	/** unique internal name of this detector */
	const char *name;
	/** detector ID */
	const char *uuid;
	/** start detection APP */
	const char *start_app;
	/** args to pass to start detection app */
	const char *start_app_args;
	/** stop detection APP */
	const char *stop_app;
	/** args to pass to stop detection app */
	const char *stop_app_args;
	/** (optional) name of header to get the signal type from */
	const char *signal_type_header;
	/** (optional) where to get the signal value from the event */
	const char *signal_value_header;
	/** (optional) where to get the signal duration from the event */
	const char *signal_duration_header;
	/** detector event to signal type mapping */
	switch_hash_t *signal_type_map;
};

/**
 * Detection state
 */
struct rayo_cpa_detector_state {
	/** reference count */
	int refs;
};

/**
 * Start detecting
 * @param call_uuid call to detect signal on
 * @param signal_ns namespace of signal to detect
 * @param error_detail on failure, describes the error
 * @return 1 if successful, 0 if failed
 */
int rayo_cpa_detector_start(const char *call_uuid, const char *signal_ns, const char **error_detail)
{
	struct rayo_cpa_detector *detector = switch_core_hash_find(globals.detectors, signal_ns);
	switch_core_session_t *session;
	if (detector) {
		if (zstr(detector->start_app)) {
			/* nothing to do */
			return 1;
		}
		session = switch_core_session_locate(call_uuid);
		if (session) {
			struct rayo_cpa_detector_state *detector_state = switch_channel_get_private(switch_core_session_get_channel(session), detector->uuid);
			if (detector_state) {
				/* detector is already running */
				detector_state->refs++;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Start detector %s, refs = %d\n", detector->name, detector_state->refs);
				switch_core_session_rwunlock(session);
				return 1;
			}
			detector_state = switch_core_session_alloc(session, sizeof(*detector_state));
			detector_state->refs = 1;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Starting detector %s, refs = 1\n", detector->name);
			switch_channel_set_private(switch_core_session_get_channel(session), detector->uuid, detector_state);
			switch_core_session_execute_application_async(session, detector->start_app, zstr(detector->start_app_args) ? NULL : detector->start_app_args);
			switch_core_session_rwunlock(session);
			return 1;
		} else {
			*error_detail = "session gone";
			return 0;
		}
	}
	*error_detail = "detector not supported";
	return 0;
}

/**
 * Stop detecting
 * @param call_uuid call to stop detecting signal on
 * @param signal_ns name of signal to stop detecting
 */
void rayo_cpa_detector_stop(const char *call_uuid, const char *signal_ns)
{
	struct rayo_cpa_detector *detector = switch_core_hash_find(globals.detectors, signal_ns);
	switch_core_session_t *session;
	if (detector) {
		if (zstr(detector->stop_app)) {
			/* nothing to do */
			return;
		}
		session = switch_core_session_locate(call_uuid);
		if (session) {
			struct rayo_cpa_detector_state *detector_state = switch_channel_get_private(switch_core_session_get_channel(session), detector->uuid);
			if (detector_state) {
				detector_state->refs--;
				if (detector_state->refs < 0) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Stop detector %s refs = %d\n", detector->name, detector_state->refs);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Stop detector %s refs = %d\n", detector->name, detector_state->refs);
				}
				if (detector_state->refs == 0) {
					/* nobody interested in detector events- shut it down */
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Stopping detector %s\n", detector->name);
					switch_core_session_execute_application_async(session, detector->stop_app, zstr(detector->stop_app_args) ? NULL : detector->stop_app_args);
					switch_channel_set_private(switch_core_session_get_channel(session), detector->uuid, NULL);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Detector %s is already stopped\n", detector->name);
			}
			switch_core_session_rwunlock(session);
		}
	}
}

/**
 * Handle event from detector
 */
static void rayo_cpa_detector_event(switch_event_t *event)
{
	struct rayo_cpa_detector *detector = (struct rayo_cpa_detector *)event->bind_user_data;
	if (detector) {
		const char *signal_type = "rayo_default";
		if (!zstr(detector->signal_type_header)) {
			signal_type = switch_event_get_header(event, detector->signal_type_header);
		}
		if (!zstr(signal_type)) {
			signal_type = switch_core_hash_find(detector->signal_type_map, signal_type);
		}
		if (!zstr(signal_type)) {
			switch_event_t *cpa_event;
			const char *uuid = switch_event_get_header(event, "Unique-ID");
			if (zstr(uuid)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Detector %s %s event is missing call UUID!\n", detector->name, signal_type);
				return;
			}
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Got Rayo CPA event %s\n", signal_type);
			if (switch_event_create_subclass(&cpa_event, SWITCH_EVENT_CUSTOM, "rayo::cpa") == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(cpa_event, SWITCH_STACK_BOTTOM, "Unique-ID", uuid);
				switch_event_add_header_string(cpa_event, SWITCH_STACK_BOTTOM, "detector-name", detector->name);
				switch_event_add_header_string(cpa_event, SWITCH_STACK_BOTTOM, "detector-uuid", detector->uuid);
				switch_event_add_header(cpa_event, SWITCH_STACK_BOTTOM, "signal-type", "%s%s:%s", RAYO_CPA_BASE, signal_type, RAYO_VERSION);
				if (!zstr(detector->signal_value_header)) {
					const char *value = switch_event_get_header(event, detector->signal_value_header);
					if (!zstr(value)) {
						switch_event_add_header_string(cpa_event, SWITCH_STACK_BOTTOM, "value", value);
					}
				}
				if (!zstr(detector->signal_duration_header)) {
					const char *duration = switch_event_get_header(event, detector->signal_duration_header);
					if (!zstr(duration)) {
						switch_event_add_header_string(cpa_event, SWITCH_STACK_BOTTOM, "duration", duration);
					}
				}
				switch_event_fire(&cpa_event);
			}
		} else {
			/* couldn't map event to Rayo signal-type */
			const char *event_name = switch_event_get_header(event, "Event-Name");
			const char *event_subclass = switch_event_get_header(event, "Event-Subclass");
			if (zstr(event_subclass)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Failed to find Rayo signal-type for event %s\n", event_name);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Failed to find Rayo signal-type for event %s %s\n", event_name, event_subclass);
			}
		}
	}
}

#define RAYO_CPA_DETECTOR_SYNTAX  "rayo_cpa <uuid> <signal-type> <start|stop>"
SWITCH_STANDARD_API(rayo_cpa_detector_api)
{
	char *cmd_dup = NULL;
	char *argv[4] = { 0 };
	int argc = 0;

	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR: USAGE %s\n", RAYO_CPA_DETECTOR_SYNTAX);
		goto done;
	}

	cmd_dup = strdup(cmd);
	argc = switch_separate_string(cmd_dup, ' ', argv, sizeof(argv) / sizeof(argv[0]));
	
	if (argc != 3) {
		stream->write_function(stream, "-ERR: USAGE %s\n", RAYO_CPA_DETECTOR_SYNTAX);
	} else {
		const char *err_reason = NULL;
		if (!strcmp(argv[2], "stop")) {
			rayo_cpa_detector_stop(argv[0], argv[1]);
			stream->write_function(stream, "+OK\n");
		} else if (!strcmp(argv[2], "start")) {
			if (!rayo_cpa_detector_start(argv[0], argv[1], &err_reason)) {
				if (err_reason) {
					stream->write_function(stream, "-ERR: %s\n", err_reason);
				} else {
					stream->write_function(stream, "-ERR\n");
				}
			} else {
				stream->write_function(stream, "+OK\n");
			}
		} else {
			stream->write_function(stream, "-ERR: USAGE %s\n", RAYO_CPA_DETECTOR_SYNTAX);
		}
	}

done:
	switch_safe_free(cmd_dup);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Configure CPA
 */
static switch_status_t do_config(switch_memory_pool_t *pool, const char *config_file)
{
	switch_xml_t cfg, xml, cpa_xml;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_hash_t *bound_events;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Configuring CPA\n");
	if (!(xml = switch_xml_open_cfg(config_file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", config_file);
		return SWITCH_STATUS_TERM;
	}

	switch_core_hash_init(&bound_events);

	cpa_xml = switch_xml_child(cfg, "cpa");
	if (cpa_xml) {
		switch_xml_t detector_xml;

		for (detector_xml = switch_xml_child(cpa_xml, "detector"); detector_xml; detector_xml = detector_xml->next) {
			switch_xml_t start_xml, stop_xml, event_xml;
			struct rayo_cpa_detector *detector;
			char id[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
			const char *name = switch_xml_attr_soft(detector_xml, "name");
			if (zstr(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing name of CPA detector!\n");
				status = SWITCH_STATUS_TERM;
				goto done;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CPA detector: %s\n", name);
			detector = switch_core_alloc(pool, sizeof(*detector));
			switch_core_hash_init(&detector->signal_type_map);
			detector->name = switch_core_strdup(pool, name);
			switch_uuid_str(id, sizeof(id));
			detector->uuid = switch_core_strdup(pool, id);

			start_xml = switch_xml_child(detector_xml, "start");
			if (start_xml) {
				detector->start_app = switch_core_strdup(pool, switch_xml_attr_soft(start_xml, "application"));
				detector->start_app_args = switch_core_strdup(pool, switch_xml_attr_soft(start_xml, "data"));
			}

			stop_xml = switch_xml_child(detector_xml, "stop");
			if (stop_xml) {
				detector->stop_app = switch_core_strdup(pool, switch_xml_attr_soft(stop_xml, "application"));
				detector->stop_app_args = switch_core_strdup(pool, switch_xml_attr_soft(stop_xml, "data"));
			}

			event_xml = switch_xml_child(detector_xml, "event");
			if (event_xml) {
				int event_ok = 0;
				switch_xml_t signal_type_xml;
				const char *event_class = switch_xml_attr_soft(event_xml, "class");
				const char *event_subclass = switch_xml_attr_soft(event_xml, "subclass");
				switch_event_types_t event_type;
				if (zstr(event_class)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing event class for CPA detector: %s\n", detector->name);
					status = SWITCH_STATUS_TERM;
					goto done;
				}

				if (switch_name_event(event_class, &event_type) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid event class %s for CPA detector: %s\n", event_class, detector->name);
					status = SWITCH_STATUS_TERM;
					goto done;
				}

				/* bind detector to event if not already done... */
				{
					struct rayo_cpa_detector *bound_detector;
					const char *event_name = switch_core_sprintf(pool, "%s %s", event_class, event_subclass);
					if (!(bound_detector = switch_core_hash_find(bound_events, event_name))) {
						/* not yet bound */
						if (zstr(event_subclass)) {
							event_subclass = NULL;
						}
						switch_event_bind("rayo_cpa_detector", event_type, event_subclass, rayo_cpa_detector_event, detector);
						switch_core_hash_insert(bound_events, event_name, detector); /* mark as bound */
					} else if (bound_detector != detector) {
						/* can't have multiple detectors generating the same event! */
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Detector %s attempted to bind to event %s that is already bound by %s\n", detector->name, event_name, bound_detector->name);
						status = SWITCH_STATUS_TERM;
						goto done;
					}
				}

				/* configure native event -> rayo CPA event mapping */
				detector->signal_type_header = switch_core_strdup(pool, switch_xml_attr_soft(event_xml, "type-header"));
				detector->signal_value_header = switch_core_strdup(pool, switch_xml_attr_soft(event_xml, "value-header"));
				detector->signal_duration_header = switch_core_strdup(pool, switch_xml_attr_soft(event_xml, "duration-header"));

				/* configure native event type -> rayo CPA signal type mapping */
				for (signal_type_xml = switch_xml_child(event_xml, "signal-type"); signal_type_xml; signal_type_xml = signal_type_xml->next) {
					const char *header_value = switch_core_strdup(pool, switch_xml_attr_soft(signal_type_xml, "header-value"));
					const char *signal_type = switch_core_strdup(pool, switch_xml_attr_soft(signal_type_xml, "value"));
					if (zstr(signal_type)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Detector %s missing signal-type value!\n", detector->name);
						status = SWITCH_STATUS_TERM;
						goto done;
					} else {
						/* add signal-type to detector mapping */
						const char *signal_type_ns = switch_core_sprintf(pool, "%s%s:%s", RAYO_CPA_BASE, signal_type, RAYO_VERSION);
						event_ok = 1;
						switch_core_hash_insert(globals.detectors, signal_type_ns, detector);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding CPA %s => %s\n", signal_type_ns, detector->name);
					}

					/* map event value to signal-type */
					if (zstr(header_value)) {
						switch_core_hash_insert(detector->signal_type_map, "rayo_default", signal_type);
					} else {
						switch_core_hash_insert(detector->signal_type_map, header_value, signal_type);
					}
				}

				if (!event_ok) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Detector %s is missing Rayo signal-type for event\n", detector->name);
					status = SWITCH_STATUS_TERM;
					goto done;
				}
			}
		}
	}

done:
	switch_core_hash_destroy(&bound_events);
	switch_xml_free(xml);

	return status;
}

/**
 * Console auto-completion for signal types
 */
static switch_status_t rayo_cpa_detector_signal_types(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;

	switch_mutex_lock(globals.detectors_mutex);
	for (hi = switch_core_hash_first(globals.detectors); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &vvar, NULL, &val);
		switch_console_push_match(&my_matches, (const char *) vvar);
	}
	switch_mutex_unlock(globals.detectors_mutex);

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

/**
 * Load CPA signal detection features
 * @param module_interface
 * @param pool memory pool
 * @param config_file
 * @return SWITCH_STATUS_SUCCESS if successfully loaded
 */
switch_status_t rayo_cpa_detector_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file)
{
	switch_api_interface_t *api_interface;

	switch_core_hash_init(&globals.detectors);
	switch_mutex_init(&globals.detectors_mutex, SWITCH_MUTEX_NESTED, pool);

	if (do_config(pool, config_file) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	SWITCH_ADD_API(api_interface, "rayo_cpa", "Query rayo status", rayo_cpa_detector_api, RAYO_CPA_DETECTOR_SYNTAX);

	switch_console_set_complete("add rayo_cpa ::console::list_uuid ::rayo_cpa::list_signal_types start");
	switch_console_set_complete("add rayo_cpa ::console::list_uuid ::rayo_cpa::list_signal_types stop");
	switch_console_add_complete_func("::rayo_cpa::list_signal_types", rayo_cpa_detector_signal_types);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Disable CPA signal detection features
 */
void rayo_cpa_detector_shutdown(void)
{
	switch_console_set_complete("del rayo_cpa");
	switch_console_del_complete_func("::rayo_cpa::list_signal_types");
	switch_core_hash_destroy(&globals.detectors);
	switch_event_unbind_callback(rayo_cpa_detector_event);
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
