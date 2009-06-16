/*
 * Copyright 2008 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mrcp_recog_resource.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_state_machine.h"
#include "mrcp_resource.h"
#include "mrcp_message.h"

/** String table of MRCP recognizer methods (mrcp_recognizer_method_id) */
static const apt_str_table_item_t v1_recog_method_string_table[] = {
	{{"SET-PARAMS",               10},10},
	{{"GET-PARAMS",               10},10},
	{{"DEFINE-GRAMMAR",           14},0},
	{{"RECOGNIZE",                 9},7},
	{{"GET-RESULT",               10},4},
	{{"RECOGNITION-START-TIMERS", 24},7},
	{{"STOP",                      4},1}
};

/** String table of mrcpv2 recognizer methods (mrcp_recognizer_method_id) */
static const apt_str_table_item_t v2_recog_method_string_table[] = {
	{{"SET-PARAMS",               10},10},
	{{"GET-PARAMS",               10},10},
	{{"DEFINE-GRAMMAR",           14},0},
	{{"RECOGNIZE",                 9},7},
	{{"GET-RESULT",               10},4},
	{{"START-INPUT-TIMERS",       18},2},
	{{"STOP",                      4},2}
};

/** String table of MRCP recognizer events (mrcp_recognizer_event_id) */
static const apt_str_table_item_t v1_recog_event_string_table[] = {
	{{"START-OF-SPEECH",          15},0},
	{{"RECOGNITION-COMPLETE",     20},0}
};

/** String table of mrcpv2 recognizer events (mrcp_recognizer_event_id) */
static const apt_str_table_item_t v2_recog_event_string_table[] = {
	{{"START-OF-INPUT",           14},0},
	{{"RECOGNITION-COMPLETE",     20},0}
};


static APR_INLINE const apt_str_table_item_t* recog_method_string_table_get(mrcp_version_e version)
{
	if(version == MRCP_VERSION_1) {
		return v1_recog_method_string_table;
	}
	return v2_recog_method_string_table;
}

static APR_INLINE const apt_str_table_item_t* recog_event_string_table_get(mrcp_version_e version)
{
	if(version == MRCP_VERSION_1) {
		return v1_recog_event_string_table;
	}
	return v2_recog_event_string_table;
}

/** Set resource specifica data */
static apt_bool_t recog_message_resourcify_by_id(mrcp_resource_t *resource, mrcp_message_t *message)
{
	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		const apt_str_t *name = apt_string_table_str_get(
			recog_method_string_table_get(message->start_line.version),
			RECOGNIZER_METHOD_COUNT,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		const apt_str_t *name = apt_string_table_str_get(
			recog_event_string_table_get(message->start_line.version),
			RECOGNIZER_EVENT_COUNT,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}

	message->header.resource_header_accessor.vtable = mrcp_recog_header_vtable_get(message->start_line.version);
	return TRUE;
}

/** Set resource specifica data */
static apt_bool_t recog_message_resourcify_by_name(mrcp_resource_t *resource, mrcp_message_t *message)
{
	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		message->start_line.method_id = apt_string_table_id_find(
			recog_method_string_table_get(message->start_line.version),
			RECOGNIZER_METHOD_COUNT,
			&message->start_line.method_name);
		if(message->start_line.method_id >= RECOGNIZER_METHOD_COUNT) {
			return FALSE;
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		message->start_line.method_id = apt_string_table_id_find(
			recog_event_string_table_get(message->start_line.version),
			RECOGNIZER_EVENT_COUNT,
			&message->start_line.method_name);
		if(message->start_line.method_id >= RECOGNIZER_EVENT_COUNT) {
			return FALSE;
		}
	}

	message->header.resource_header_accessor.vtable = mrcp_recog_header_vtable_get(message->start_line.version);
	return TRUE;
}


/** Create MRCP recognizer resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_recog_resource_create(apr_pool_t *pool)
{
	mrcp_resource_t *resource = apr_palloc(pool,sizeof(mrcp_resource_t));
	mrcp_resource_init(resource);

	resource->resourcify_message_by_id = recog_message_resourcify_by_id;
	resource->resourcify_message_by_name = recog_message_resourcify_by_name;

	resource->create_client_state_machine = mrcp_recog_client_state_machine_create;
	resource->create_server_state_machine = mrcp_recog_server_state_machine_create;
	return resource;
}
