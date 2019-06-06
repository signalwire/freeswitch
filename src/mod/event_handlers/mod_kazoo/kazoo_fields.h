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

#ifndef KAZOO_FIELDS_H
#define KAZOO_FIELDS_H

#include <switch.h>

#define MAX_LIST_FIELDS 25

typedef struct kazoo_log_levels kazoo_loglevels_t;
typedef kazoo_loglevels_t *kazoo_loglevels_ptr;

struct kazoo_log_levels
{
	switch_log_level_t success_log_level;
	switch_log_level_t failed_log_level;
	switch_log_level_t warn_log_level;
	switch_log_level_t info_log_level;
	switch_log_level_t time_log_level;
	switch_log_level_t filtered_event_log_level;
	switch_log_level_t filtered_field_log_level;

};

typedef struct kazoo_logging kazoo_logging_t;
typedef kazoo_logging_t *kazoo_logging_ptr;

struct kazoo_logging
{
	kazoo_loglevels_ptr levels;
	const char *profile_name;
	const char *event_name;
};

typedef struct kazoo_list_s {
  char *value[MAX_LIST_FIELDS];
  int size;
} kazoo_list_t;

typedef enum {
	FILTER_COMPARE_REGEX,
	FILTER_COMPARE_LIST,
	FILTER_COMPARE_VALUE,
	FILTER_COMPARE_PREFIX,
	FILTER_COMPARE_EXISTS,
	FILTER_COMPARE_FIELD

} kazoo_filter_compare_type;

typedef enum {
	FILTER_EXCLUDE,
	FILTER_INCLUDE,
	FILTER_ENSURE
} kazoo_filter_type;

typedef struct kazoo_filter_t {
	kazoo_filter_type type;
	kazoo_filter_compare_type compare;
	char* name;
	char* value;
	kazoo_list_t list;
	struct kazoo_filter_t* next;
} kazoo_filter, *kazoo_filter_ptr;


typedef enum {
	JSON_NONE,
	JSON_STRING,
	JSON_NUMBER,
	JSON_BOOLEAN,
	JSON_OBJECT,
	JSON_RAW
} kazoo_json_field_type;

typedef enum {
	FIELD_NONE,
	FIELD_COPY,
	FIELD_STATIC,
	FIELD_FIRST_OF,
	FIELD_EXPAND,
	FIELD_PREFIX,
	FIELD_OBJECT,
	FIELD_GROUP,
	FIELD_REFERENCE

} kazoo_field_type;

typedef struct kazoo_field_t kazoo_field;
typedef kazoo_field *kazoo_field_ptr;

typedef struct kazoo_fields_t kazoo_fields;
typedef kazoo_fields *kazoo_fields_ptr;

typedef struct kazoo_definition_t kazoo_definition;
typedef kazoo_definition *kazoo_definition_ptr;

struct kazoo_field_t {
	char* name;
	char* value;
	char* as;
	kazoo_list_t list;
	switch_bool_t exclude_prefix;
	kazoo_field_type in_type;
	kazoo_json_field_type out_type;
	kazoo_filter_ptr filter;

	kazoo_definition_ptr ref;
	kazoo_field_ptr next;
	kazoo_fields_ptr children;
};

struct kazoo_fields_t {
	kazoo_field_ptr head;
	int verbose;
};


struct kazoo_definition_t {
	char* name;
	kazoo_field_ptr head;
	kazoo_filter_ptr filter;
};

struct kazoo_event {
	kazoo_event_profile_ptr profile;
	char *name;
	kazoo_fields_ptr fields;
	kazoo_filter_ptr filter;

	kazoo_event_t* next;
};

struct kazoo_event_profile {
	char *name;
	kazoo_config_ptr root;
	switch_bool_t running;
	switch_memory_pool_t *pool;
	kazoo_filter_ptr filter;
	kazoo_fields_ptr fields;
	kazoo_event_ptr events;

	kazoo_loglevels_ptr logging;
};

struct kazoo_fetch_profile {
	char *name;
	kazoo_config_ptr root;
	switch_bool_t running;
	switch_memory_pool_t *pool;
	kazoo_fields_ptr fields;
	int fetch_timeout;
	switch_mutex_t *fetch_reply_mutex;
	switch_hash_t *fetch_reply_hash;
	switch_xml_binding_t *fetch_binding;
	switch_xml_section_t section;

	kazoo_loglevels_ptr logging;
};

#endif /* KAZOO_FIELDS_H */

