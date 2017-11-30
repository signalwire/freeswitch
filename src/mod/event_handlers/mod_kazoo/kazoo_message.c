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

/* deletes then add */
void kazoo_cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
	cJSON_DeleteItemFromObject(object, string);
	cJSON_AddItemToObject(object, string, item);
}

static int inline filter_compare(switch_event_t* evt, kazoo_filter_ptr filter)
{
	switch_event_header_t *header;
	int hasValue = 0, /*size, */n;
	char *value;

	switch(filter->compare) {

	case FILTER_COMPARE_EXISTS:
		hasValue = switch_event_get_header(evt, filter->name) != NULL ? 1 : 0;
		break;

	case FILTER_COMPARE_VALUE:
		value = switch_event_get_header_nil(evt, filter->name);
		hasValue = !strcmp(value, filter->value);
		break;

	case FILTER_COMPARE_PREFIX:
		for (header = evt->headers; header; header = header->next) {
			if(!strncmp(header->name, filter->value, strlen(filter->value))) {
				hasValue = 1;
				break;
			}
		}
		break;

	case FILTER_COMPARE_LIST:
		value = switch_event_get_header(evt, filter->name);
		if(value) {
			for(n = 0; n < filter->list.size; n++) {
				if(!strncmp(value, filter->list.value[n], strlen(filter->list.value[n]))) {
					hasValue = 1;
					break;
				}
			}
		}
		break;

	case FILTER_COMPARE_REGEX:
		break;

	default:
		break;
	}

	return hasValue;
}

static kazoo_filter_ptr inline filter_event(switch_event_t* evt, kazoo_filter_ptr filter)
{
	while(filter) {
		int hasValue = filter_compare(evt, filter);
		if(filter->type == FILTER_EXCLUDE) {
			if(hasValue)
				break;
		} else if(filter->type == FILTER_INCLUDE) {
			if(!hasValue)
				break;
		}
		filter = filter->next;
	}
	return filter;
}

static void kazoo_event_init_json_fields(switch_event_t *event, cJSON *json)
{
	switch_event_header_t *hp;
	for (hp = event->headers; hp; hp = hp->next) {
		if (hp->idx) {
			cJSON *a = cJSON_CreateArray();
			int i;

			for(i = 0; i < hp->idx; i++) {
				cJSON_AddItemToArray(a, cJSON_CreateString(hp->array[i]));
			}

			cJSON_AddItemToObject(json, hp->name, a);

		} else {
			cJSON_AddItemToObject(json, hp->name, cJSON_CreateString(hp->value));
		}
	}
}

static switch_status_t kazoo_event_init_json(kazoo_fields_ptr fields1, kazoo_fields_ptr fields2, switch_event_t* evt, cJSON** clone)
{
	switch_status_t status;
	if( (fields2 && fields2->verbose)
			|| (fields1 && fields1->verbose)
			|| ( (!fields2) &&  (!fields1)) ) {
		status = switch_event_serialize_json_obj(evt, clone);
	} else {
		status = SWITCH_STATUS_SUCCESS;
		*clone = cJSON_CreateObject();
		if((*clone) == NULL) {
			status = SWITCH_STATUS_GENERR;
		}
	}
	return status;
}

static cJSON * kazoo_event_json_value(kazoo_json_field_type type, const char *value) {
	cJSON *item = NULL;
	switch(type) {
	case JSON_STRING:
		item = cJSON_CreateString(value);
		break;

	case JSON_NUMBER:
		item = cJSON_CreateNumber(strtod(value, NULL));
		break;

	case JSON_BOOLEAN:
		item = cJSON_CreateBool(switch_true(value));
		break;

	case JSON_OBJECT:
		item = cJSON_CreateObject();
		break;

	case JSON_RAW:
		item = cJSON_CreateRaw(value);
		break;

	default:
		break;
	};

	return item;
}

static cJSON * kazoo_event_add_json_value(cJSON *dst, kazoo_field_ptr field, const char *as, const char *value) {
	cJSON *item = NULL;
	if(value || field->out_type == JSON_OBJECT) {
		if((item = kazoo_event_json_value(field->out_type, value)) != NULL) {
			kazoo_cJSON_AddItemToObject(dst, as, item);
		}
	}
	return item;
}

#define MAX_FIRST_OF 25

char * first_of(switch_event_t *src, char * in)
{
	switch_event_header_t *header;
	char *y1, *y2, *y3 = NULL;
	char *value[MAX_FIRST_OF];
	int n, size;

	y1 = strdup(in);
	if((y2 = (char *) switch_stristr("first-of", y1)) != NULL) {
		char tmp[2048] = "";
		y3 = switch_find_end_paren((const char *)y2 + 8, '(', ')');
		*++y3='\0';
		*y2 = '\0';
		size = switch_separate_string(y2 + 9, '|', value, MAX_FIRST_OF);
		for(n=0; n < size; n++) {
			header = switch_event_get_header_ptr(src, value[n]);
			if(header) {
				switch_snprintf(tmp, sizeof(tmp), "%s%s%s", y1, header->name, y3);
				free(y1);
				y2 = strdup(tmp);
				y3 = first_of(src, y2);
				if(y2 == y3) {
					return y2;
				} else {
					return y3;
				}
			}
		}
	}
	free(y1);
	return in;

}

cJSON * kazoo_event_add_field_to_json(cJSON *dst, switch_event_t *src, kazoo_field_ptr field)
{
	switch_event_header_t *header;
	char *expanded, *firstOf;
	uint i, n;
	cJSON *item = NULL;

	switch(field->in_type) {
		case FIELD_COPY:
			if((header = switch_event_get_header_ptr(src, field->name)) != NULL) {
				if (header->idx) {
					item = cJSON_CreateArray();
					for(i = 0; i < header->idx; i++) {
						cJSON_AddItemToArray(item, kazoo_event_json_value(field->out_type, header->array[i]));
					}
					kazoo_cJSON_AddItemToObject(dst, field->as ? field->as : field->name, item);
				} else {
					item = kazoo_event_add_json_value(dst, field, field->as ? field->as : field->name, header->value);
				}
			}
			break;

		case FIELD_EXPAND:
			firstOf = first_of(src, field->value);
			expanded = switch_event_expand_headers(src, firstOf);
			if(expanded != NULL && !zstr(expanded)) {
				item = kazoo_event_add_json_value(dst, field, field->as ? field->as : field->name, expanded);
			}
			if(expanded != firstOf) {
				free(expanded);
			}
			if(firstOf != field->value) {
				free(firstOf);
			}
			break;

		case FIELD_FIRST_OF:
			for(n = 0; n < field->list.size; n++) {
				if(*field->list.value[n] == '#') {
					item = kazoo_event_add_json_value(dst, field, field->as ? field->as : field->name, ++field->list.value[n]);
					break;
				} else {
					header = switch_event_get_header_ptr(src, field->list.value[n]);
					if(header) {
						if (header->idx) {
							item = cJSON_CreateArray();
							for(i = 0; i < header->idx; i++) {
								cJSON_AddItemToArray(item, kazoo_event_json_value(field->out_type, header->array[i]));
							}
							kazoo_cJSON_AddItemToObject(dst, field->as ? field->as : field->name, item);
						} else {
							item = kazoo_event_add_json_value(dst, field, field->as ? field->as : field->name, header->value);
						}
						break;
					}
				}
			}
			break;

		case FIELD_PREFIX:
			for (header = src->headers; header; header = header->next) {
				if(!strncmp(header->name, field->name, strlen(field->name))) {
					if (header->idx) {
						cJSON *array = cJSON_CreateArray();
						for(i = 0; i < header->idx; i++) {
							cJSON_AddItemToArray(array, kazoo_event_json_value(field->out_type, header->array[i]));
						}
						kazoo_cJSON_AddItemToObject(dst, field->exclude_prefix ? header->name+strlen(field->name) : header->name, array);
					} else {
						kazoo_event_add_json_value(dst, field, field->exclude_prefix ? header->name+strlen(field->name) : header->name, header->value);
					}
				}
			}
			break;

		case FIELD_STATIC:
			item = kazoo_event_add_json_value(dst, field, field->name, field->value);
			break;

		case FIELD_GROUP:
			item = dst;
			break;

		default:
			break;
	}

    return item;
}

static switch_status_t kazoo_event_add_fields_to_json(kazoo_logging_ptr logging, cJSON *dst, switch_event_t *src, kazoo_field_ptr field) {

	kazoo_filter_ptr filter;
	cJSON *item = NULL;
	while(field) {
		if(field->in_type == FIELD_REFERENCE) {
			if(field->ref) {
				if((filter = filter_event(src, field->ref->filter)) != NULL) {
					switch_log_printf(SWITCH_CHANNEL_LOG, logging->levels->filtered_field_log_level, "profile[%s] event %s, referenced field %s filtered by settings %s : %s\n", logging->profile_name, logging->event_name, field->ref->name, filter->name, filter->value);
				} else {
					kazoo_event_add_fields_to_json(logging, dst, src, field->ref->head);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "profile[%s] event %s, referenced field %s not found\n", logging->profile_name, logging->event_name, field->name);
			}
		} else {
			if((filter = filter_event(src, field->filter)) != NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, logging->levels->filtered_field_log_level, "profile[%s] event %s, field %s filtered by settings %s : %s\n", logging->profile_name, logging->event_name, field->name, filter->name, filter->value);
			} else {
				item = kazoo_event_add_field_to_json(dst, src, field);
				if(field->children && item != NULL) {
					if(field->children->verbose && field->out_type == JSON_OBJECT) {
						kazoo_event_init_json_fields(src, item);
					}
					kazoo_event_add_fields_to_json(logging, field->out_type == JSON_OBJECT ? item : dst, src, field->children->head);
				}
			}
		}

		field = field->next;
	}

    return SWITCH_STATUS_SUCCESS;
}


kazoo_message_ptr kazoo_message_create_event(switch_event_t* evt, kazoo_event_ptr event, kazoo_event_profile_ptr profile)
{
	kazoo_message_ptr message;
	cJSON *JObj = NULL;
	kazoo_filter_ptr filtered;
	kazoo_logging_t logging;

	logging.levels = profile->logging;
	logging.event_name = switch_event_get_header_nil(evt, "Event-Name");
	logging.profile_name = profile->name;

	switch_event_add_header(evt, SWITCH_STACK_BOTTOM, "Switch-Nodename", "%s@%s", "freeswitch", switch_event_get_header(evt, "FreeSWITCH-Hostname"));


	message = malloc(sizeof(kazoo_message_t));
	if(message == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error allocating memory for serializing event to json\n");
		return NULL;
	}
	memset(message, 0, sizeof(kazoo_message_t));

	if(profile->filter) {
		// filtering
		if((filtered = filter_event(evt, profile->filter)) != NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, logging.levels->filtered_event_log_level, "profile[%s] event %s filtered by profile settings %s : %s\n", logging.profile_name, logging.event_name, filtered->name, filtered->value);
			kazoo_message_destroy(&message);
			return NULL;
		}
	}

	if(event && event->filter) {
		if((filtered = filter_event(evt, event->filter)) != NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, logging.levels->filtered_event_log_level, "profile[%s] event %s filtered by event settings %s : %s\n", logging.profile_name, logging.event_name, filtered->name, filtered->value);
			kazoo_message_destroy(&message);
			return NULL;
		}
	}

	kazoo_event_init_json(profile->fields, event ? event->fields : NULL, evt, &JObj);

	if(profile->fields)
		kazoo_event_add_fields_to_json(&logging, JObj, evt, profile->fields->head);

	if(event && event->fields)
		kazoo_event_add_fields_to_json(&logging, JObj, evt, event->fields->head);

	message->JObj = JObj;


	return message;


}

kazoo_message_ptr kazoo_message_create_fetch(switch_event_t* evt, kazoo_fetch_profile_ptr profile)
{
	kazoo_message_ptr message;
	cJSON *JObj = NULL;
	kazoo_logging_t logging;

	logging.levels = profile->logging;
	logging.event_name = switch_event_get_header_nil(evt, "Event-Name");
	logging.profile_name = profile->name;

	switch_event_add_header(evt, SWITCH_STACK_BOTTOM, "Switch-Nodename", "%s@%s", "freeswitch", switch_event_get_header(evt, "FreeSWITCH-Hostname"));

	message = malloc(sizeof(kazoo_message_t));
	if(message == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error allocating memory for serializing event to json\n");
		return NULL;
	}
	memset(message, 0, sizeof(kazoo_message_t));


	kazoo_event_init_json(profile->fields, NULL, evt, &JObj);

	if(profile->fields)
		kazoo_event_add_fields_to_json(&logging, JObj, evt, profile->fields->head);

	message->JObj = JObj;


	return message;


}


void kazoo_message_destroy(kazoo_message_ptr *msg)
{
	if (!msg || !*msg) return;
	if((*msg)->JObj != NULL)
		cJSON_Delete((*msg)->JObj);
	switch_safe_free(*msg);

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
