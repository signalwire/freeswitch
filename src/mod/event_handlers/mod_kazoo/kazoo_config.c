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
* Based on mod_skel by
* Anthony Minessale II <anthm@freeswitch.org>
*
* Contributor(s):
*
* Daniel Bryars <danb@aeriandi.com>
* Tim Brown <tim.brown@aeriandi.com>
* Anthony Minessale II <anthm@freeswitch.org>
* William King <william.king@quentustech.com>
* Mike Jerris <mike@jerris.com>
*
* kazoo.c -- Sends FreeSWITCH events to an AMQP broker
*
*/

#include "mod_kazoo.h"

static const char *LOG_LEVEL_NAMES[] = {
		"SWITCH_LOG_DEBUG10",
		"SWITCH_LOG_DEBUG9",
		"SWITCH_LOG_DEBUG8",
		"SWITCH_LOG_DEBUG7",
		"SWITCH_LOG_DEBUG6",
		"SWITCH_LOG_DEBUG5",
		"SWITCH_LOG_DEBUG4",
		"SWITCH_LOG_DEBUG3",
		"SWITCH_LOG_DEBUG2",
		"SWITCH_LOG_DEBUG1",
		"SWITCH_LOG_DEBUG",
		"SWITCH_LOG_INFO",
		"SWITCH_LOG_NOTICE",
		"SWITCH_LOG_WARNING",
		"SWITCH_LOG_ERROR",
		"SWITCH_LOG_CRIT",
		"SWITCH_LOG_ALERT",
		"SWITCH_LOG_CONSOLE",
		"SWITCH_LOG_INVALID",
		"SWITCH_LOG_UNINIT",
		NULL
};

static const switch_log_level_t LOG_LEVEL_VALUES[] = {
		SWITCH_LOG_DEBUG10,
		SWITCH_LOG_DEBUG9,
		SWITCH_LOG_DEBUG8,
		SWITCH_LOG_DEBUG7,
		SWITCH_LOG_DEBUG6,
		SWITCH_LOG_DEBUG5,
		SWITCH_LOG_DEBUG4,
		SWITCH_LOG_DEBUG3,
		SWITCH_LOG_DEBUG2,
		SWITCH_LOG_DEBUG1,
		SWITCH_LOG_DEBUG,
		SWITCH_LOG_INFO,
		SWITCH_LOG_NOTICE,
		SWITCH_LOG_WARNING,
		SWITCH_LOG_ERROR,
		SWITCH_LOG_CRIT,
		SWITCH_LOG_ALERT,
		SWITCH_LOG_CONSOLE,
		SWITCH_LOG_INVALID,
		SWITCH_LOG_UNINIT
};

switch_log_level_t log_str2level(const char *str)
{
	int x = 0;
	switch_log_level_t level = SWITCH_LOG_INVALID;

	if (switch_is_number(str)) {
		x = atoi(str);

		if (x > SWITCH_LOG_INVALID) {
			return SWITCH_LOG_INVALID - 1;
		} else if (x < 0) {
			return 0;
		} else {
			return x;
		}
	}


	for (x = 0;; x++) {
		if (!LOG_LEVEL_NAMES[x]) {
			break;
		}

		if (!strcasecmp(LOG_LEVEL_NAMES[x], str)) {
			level = LOG_LEVEL_VALUES[x]; //(switch_log_level_t) x;
			break;
		}
	}

	return level;
}

switch_status_t kazoo_config_loglevels(switch_memory_pool_t *pool, switch_xml_t cfg, kazoo_loglevels_ptr *ptr)
{
	switch_xml_t xml_level, xml_logging;
	kazoo_loglevels_ptr loglevels = (kazoo_loglevels_ptr) switch_core_alloc(pool, sizeof(kazoo_loglevels_t));

	loglevels->failed_log_level = SWITCH_LOG_ALERT;
	loglevels->filtered_event_log_level = SWITCH_LOG_DEBUG1;
	loglevels->filtered_field_log_level = SWITCH_LOG_DEBUG1;
	loglevels->info_log_level = SWITCH_LOG_INFO;
	loglevels->warn_log_level = SWITCH_LOG_WARNING;
	loglevels->success_log_level = SWITCH_LOG_DEBUG;
	loglevels->time_log_level = SWITCH_LOG_DEBUG1;

	if ((xml_logging = switch_xml_child(cfg, "logging")) != NULL) {
		for (xml_level = switch_xml_child(xml_logging, "log"); xml_level; xml_level = xml_level->next) {
			char *var = (char *) switch_xml_attr_soft(xml_level, "name");
			char *val = (char *) switch_xml_attr_soft(xml_level, "value");

			if (!var) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "logging param missing 'name' attribute\n");
				continue;
			}

			if (!val) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "logging param[%s] missing 'value' attribute\n", var);
				continue;
			}

			if (!strncmp(var, "success", 7)) {
				loglevels->success_log_level = log_str2level(val);
			} else if (!strncmp(var, "failed", 6)) {
				loglevels->failed_log_level = log_str2level(val);
			} else if (!strncmp(var, "info", 4)) {
				loglevels->info_log_level = log_str2level(val);
			} else if (!strncmp(var, "warn", 4)) {
				loglevels->warn_log_level = log_str2level(val);
			} else if (!strncmp(var, "time", 4)) {
				loglevels->time_log_level = log_str2level(val);
			} else if (!strncmp(var, "filtered-event", 14)) {
				loglevels->filtered_event_log_level = log_str2level(val);
			} else if (!strncmp(var, "filtered-field", 14)) {
				loglevels->filtered_field_log_level = log_str2level(val);
			}
		} /* xml_level for loop */
	}

	*ptr = loglevels;
	return SWITCH_STATUS_SUCCESS;

}

switch_status_t kazoo_config_filters(switch_memory_pool_t *pool, switch_xml_t cfg, kazoo_filter_ptr *ptr)
{
	switch_xml_t filters, filter;
//	char *routing_key = NULL;
	kazoo_filter_ptr root = NULL, prv = NULL, cur = NULL;


	if ((filters = switch_xml_child(cfg, "filters")) != NULL) {
		for (filter = switch_xml_child(filters, "filter"); filter; filter = filter->next) {
			const char *var = switch_xml_attr(filter, "name");
			const char *val = switch_xml_attr(filter, "value");
			const char *type = switch_xml_attr(filter, "type");
			const char *compare = switch_xml_attr(filter, "compare");
			cur = (kazoo_filter_ptr) switch_core_alloc(pool, sizeof(kazoo_filter));
			memset(cur, 0, sizeof(kazoo_filter));
			if(prv == NULL) {
				root = prv = cur;
			} else {
				prv->next = cur;
				prv = cur;
			}
			cur->type = FILTER_EXCLUDE;
			cur->compare = FILTER_COMPARE_VALUE;

			if(var)
				cur->name = switch_core_strdup(pool, var);

			if(val)
				cur->value = switch_core_strdup(pool, val);

			if(type) {
				if (!strncmp(type, "exclude", 7)) {
					cur->type = FILTER_EXCLUDE;
				} else if (!strncmp(type, "include", 7)) {
						cur->type = FILTER_INCLUDE;
				}
			}

			if(compare) {
				if (!strncmp(compare, "value", 7)) {
					cur->compare = FILTER_COMPARE_VALUE;
				} else if (!strncmp(compare, "prefix", 6)) {
						cur->compare = FILTER_COMPARE_PREFIX;
				} else if (!strncmp(compare, "list", 4)) {
						cur->compare = FILTER_COMPARE_LIST;
				} else if (!strncmp(compare, "exists", 6)) {
						cur->compare = FILTER_COMPARE_EXISTS;
				} else if (!strncmp(compare, "regex", 5)) {
						cur->compare = FILTER_COMPARE_REGEX;
				} else if (!strncmp(compare, "field", 5)) {
						cur->compare = FILTER_COMPARE_FIELD;
				}
			}

			if(cur->value == NULL)
				cur->compare = FILTER_COMPARE_EXISTS;

			if(cur->compare == FILTER_COMPARE_LIST) {
				cur->list.size = switch_separate_string(cur->value, '|', cur->list.value, MAX_LIST_FIELDS);
			}

		}

	}

	*ptr = root;

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t kazoo_config_field(kazoo_config_ptr definitions, switch_memory_pool_t *pool, switch_xml_t cfg, kazoo_field_ptr *ptr)
{
	const char *var = switch_xml_attr(cfg, "name");
	const char *val = switch_xml_attr(cfg, "value");
	const char *as = switch_xml_attr(cfg, "as");
	const char *type = switch_xml_attr(cfg, "type");
	const char *exclude_prefix = switch_xml_attr(cfg, "exclude-prefix");
	const char *serialize_as = switch_xml_attr(cfg, "serialize-as");
	kazoo_field_ptr cur = (kazoo_field_ptr) switch_core_alloc(pool, sizeof(kazoo_field));
	cur->in_type = FIELD_NONE;
	cur->out_type = JSON_NONE;

	if(var)
		cur->name = switch_core_strdup(pool, var);

	if(val)
		cur->value = switch_core_strdup(pool, val);

	if(as)
		cur->as = switch_core_strdup(pool, as);

	if(type) {
		if (!strncmp(type, "copy", 4)) {
			cur->in_type = FIELD_COPY;
		} else if (!strncmp(type, "static", 6)) {
			cur->in_type = FIELD_STATIC;
		} else if (!strncmp(type, "first-of", 8)) {
			cur->in_type = FIELD_FIRST_OF;
		} else if (!strncmp(type, "expand", 6)) {
			cur->in_type = FIELD_EXPAND;
		} else if (!strncmp(type, "prefix", 10)) {
			cur->in_type = FIELD_PREFIX;
		} else if (!strncmp(type, "group", 5)) {
			cur->in_type = FIELD_GROUP;
		} else if (!strncmp(type, "reference", 9)) {
			cur->in_type = FIELD_REFERENCE;
		}
	}

	if(serialize_as) {
		if (!strncmp(serialize_as, "string", 5)) {
			cur->out_type = JSON_STRING;
		} else if (!strncmp(serialize_as, "number", 6)) {
			cur->out_type = JSON_NUMBER;
		} else if (!strncmp(serialize_as, "boolean", 7)) {
			cur->out_type = JSON_BOOLEAN;
		} else if (!strncmp(serialize_as, "object", 6)) {
			cur->out_type = JSON_OBJECT;
		} else if (!strncmp(serialize_as, "raw", 6)) {
			cur->out_type = JSON_RAW;
		}
	}

	if(exclude_prefix)
		cur->exclude_prefix = switch_true(exclude_prefix);

	kazoo_config_filters(pool, cfg, &cur->filter);
	kazoo_config_fields(definitions, pool, cfg, &cur->children);

	if(cur->children != NULL
			&& (cur->in_type == FIELD_STATIC)
			&& (cur->out_type == JSON_NONE)
			) {
		cur->out_type = JSON_OBJECT;
	}
	if(cur->in_type == FIELD_NONE) {
		cur->in_type = FIELD_COPY;
	}

	if(cur->out_type == JSON_NONE) {
		cur->out_type = JSON_STRING;
	}

	if(cur->in_type == FIELD_FIRST_OF) {
		cur->list.size = switch_separate_string(cur->value, '|', cur->list.value, MAX_LIST_FIELDS);
	}

	if(cur->in_type == FIELD_REFERENCE) {
		cur->ref = (kazoo_definition_ptr)switch_core_hash_find(definitions->hash, cur->name);
		if(cur->ref == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "referenced field %s not found\n", cur->name);
		}
	}

	*ptr = cur;

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t kazoo_config_fields_loop(kazoo_config_ptr definitions, switch_memory_pool_t *pool, switch_xml_t cfg, kazoo_field_ptr *ptr)
{
	switch_xml_t field;
	kazoo_field_ptr root = NULL, prv = NULL;


	for (field = switch_xml_child(cfg, "field"); field; field = field->next) {
		kazoo_field_ptr cur = NULL;
		kazoo_config_field(definitions, pool, field, &cur);
		if(root == NULL) {
			root = prv = cur;
		} else {
			prv->next = cur;
			prv = cur;
		}
	}

	*ptr = root;

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t kazoo_config_fields(kazoo_config_ptr definitions, switch_memory_pool_t *pool, switch_xml_t cfg, kazoo_fields_ptr *ptr)
{
	switch_xml_t fields;
	kazoo_fields_ptr root = NULL;


	if ((fields = switch_xml_child(cfg, "fields")) != NULL) {
		const char *verbose = switch_xml_attr(fields, "verbose");
		root = (kazoo_fields_ptr) switch_core_alloc(pool, sizeof(kazoo_fields));
		root->verbose = SWITCH_TRUE;
		if(verbose) {
			root->verbose = switch_true(verbose);
		}

		kazoo_config_fields_loop(definitions, pool, fields, &root->head);

	}

	*ptr = root;

	return SWITCH_STATUS_SUCCESS;

}

kazoo_config_ptr kazoo_config_event_handlers(kazoo_config_ptr definitions, switch_xml_t cfg)
{
	switch_xml_t xml_profiles = NULL, xml_profile = NULL;
	kazoo_config_ptr profiles = NULL;
	switch_memory_pool_t *pool = NULL;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "error creating memory pool for producers\n");
		return NULL;
	}

	profiles = switch_core_alloc(pool, sizeof(kazoo_config));
	profiles->pool = pool;
	switch_core_hash_init(&profiles->hash);

	if ((xml_profiles = switch_xml_child(cfg, "event-handlers"))) {
		if ((xml_profile = switch_xml_child(xml_profiles, "profile"))) {
			for (; xml_profile; xml_profile = xml_profile->next) {
				const char *name = switch_xml_attr(xml_profile, "name");
				if(name == NULL) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "missing attr name\n" );
					continue;
				}
				kazoo_config_event_handler(definitions, profiles, xml_profile, NULL);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unable to locate a event-handler profile for kazoo\n" );
		}
	} else {
		destroy_config(&profiles);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "unable to locate event-handlers section for kazoo, using default\n" );
	}

	return profiles;

}

kazoo_config_ptr kazoo_config_fetch_handlers(kazoo_config_ptr definitions, switch_xml_t cfg)
{
	switch_xml_t xml_profiles = NULL, xml_profile = NULL;
	kazoo_config_ptr profiles = NULL;
	switch_memory_pool_t *pool = NULL;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "error creating memory pool for producers\n");
		return NULL;
	}

	profiles = switch_core_alloc(pool, sizeof(kazoo_config));
	profiles->pool = pool;
	switch_core_hash_init(&profiles->hash);

	if ((xml_profiles = switch_xml_child(cfg, "fetch-handlers"))) {
		if ((xml_profile = switch_xml_child(xml_profiles, "profile"))) {
			for (; xml_profile; xml_profile = xml_profile->next) {
				const char *name = switch_xml_attr(xml_profile, "name");
				if(name == NULL) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "missing attr name\n" );
					continue;
				}
				kazoo_config_fetch_handler(definitions, profiles, xml_profile, NULL);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unable to locate a fetch-handler profile for kazoo\n" );
		}
	} else {
		destroy_config(&profiles);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "unable to locate fetch-handlers section for kazoo, using default\n" );
	}

	return profiles;

}


switch_status_t kazoo_config_definition(kazoo_config_ptr root, switch_xml_t cfg)
{
	kazoo_definition_ptr definition = NULL;
	char *name = (char *) switch_xml_attr_soft(cfg, "name");

	if (zstr(name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to load kazoo profile, check definition missing name attr\n");
		return SWITCH_STATUS_GENERR;
	}

	definition = switch_core_alloc(root->pool, sizeof(kazoo_definition));
	definition->name = switch_core_strdup(root->pool, name);

	kazoo_config_filters(root->pool, cfg, &definition->filter);
	kazoo_config_fields_loop(root, root->pool, cfg, &definition->head);

	if ( switch_core_hash_insert(root->hash, name, (void *) definition) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to insert new definition [%s] into kazoo definitions hash\n", name);
		return SWITCH_STATUS_GENERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "definition[%s] successfully configured\n", definition->name);
	return SWITCH_STATUS_SUCCESS;
}

kazoo_config_ptr kazoo_config_definitions(switch_xml_t cfg)
{
	switch_xml_t xml_definitions = NULL, xml_definition = NULL;
	kazoo_config_ptr definitions = NULL;
	switch_memory_pool_t *pool = NULL;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "error creating memory pool for definitions\n");
		return NULL;
	}

	definitions = switch_core_alloc(pool, sizeof(kazoo_config));
	definitions->pool = pool;
	switch_core_hash_init(&definitions->hash);

	if ((xml_definitions = switch_xml_child(cfg, "definitions"))) {
		if ((xml_definition = switch_xml_child(xml_definitions, "definition"))) {
			for (; xml_definition; xml_definition = xml_definition->next)	{
				kazoo_config_definition(definitions, xml_definition);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "no definitions for kazoo\n" );
		}
	} else {
		destroy_config(&definitions);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "unable to locate definitions section for kazoo, using default\n" );
	}

	return definitions;
}

void destroy_config(kazoo_config_ptr *ptr)
{
	kazoo_config_ptr config = NULL;
	switch_memory_pool_t *pool;

	if (!ptr || !*ptr) {
		return;
	}
	config = *ptr;
	pool = config->pool;

	switch_core_hash_destroy(&(config->hash));
	switch_core_destroy_memory_pool(&pool);

	*ptr = NULL;
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
