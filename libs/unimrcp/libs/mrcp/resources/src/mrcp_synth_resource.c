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

#include "mrcp_synth_resource.h"
#include "mrcp_synth_header.h"
#include "mrcp_synth_state_machine.h"
#include "mrcp_resource.h"
#include "mrcp_message.h"

/** String table of MRCP synthesizer methods (mrcp_synthesizer_method_id) */
static const apt_str_table_item_t synth_method_string_table[] = {
	{{"SET-PARAMS",       10},10},
	{{"GET-PARAMS",       10},0},
	{{"SPEAK",             5},1},
	{{"STOP",              4},1},
	{{"PAUSE",             5},0},
	{{"RESUME",            6},0},
	{{"BARGE-IN-OCCURRED",17},0},
	{{"CONTROL",           7},0},
	{{"DEFINE-LEXICON",   14},0}
};

/** String table of MRCP synthesizer events (mrcp_synthesizer_event_id) */
static const apt_str_table_item_t synth_event_string_table[] = {
	{{"SPEECH-MARKER", 13},3},
	{{"SPEAK-COMPLETE",14},3}
};

/** Set resource specifica data */
static apt_bool_t synth_message_resourcify_by_id(mrcp_resource_t *resource, mrcp_message_t *message)
{
	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		const apt_str_t *name = apt_string_table_str_get(
			synth_method_string_table,
			SYNTHESIZER_METHOD_COUNT,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		const apt_str_t *name = apt_string_table_str_get(
			synth_event_string_table,
			SYNTHESIZER_EVENT_COUNT,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}

	message->header.resource_header_accessor.vtable = 
		mrcp_synth_header_vtable_get(message->start_line.version);
	return TRUE;
}

/** Set resource specifica data */
static apt_bool_t synth_message_resourcify_by_name(mrcp_resource_t *resource, mrcp_message_t *message)
{
	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		message->start_line.method_id = apt_string_table_id_find(
			synth_method_string_table,
			SYNTHESIZER_METHOD_COUNT,
			&message->start_line.method_name);
		if(message->start_line.method_id >= SYNTHESIZER_METHOD_COUNT) {
			return FALSE;
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		message->start_line.method_id = apt_string_table_id_find(
			synth_event_string_table,
			SYNTHESIZER_EVENT_COUNT,
			&message->start_line.method_name);
		if(message->start_line.method_id >= SYNTHESIZER_EVENT_COUNT) {
			return FALSE;
		}
	}

	message->header.resource_header_accessor.vtable = 
		mrcp_synth_header_vtable_get(message->start_line.version);
	return TRUE;
}


/** Create MRCP synthesizer resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_synth_resource_create(apr_pool_t *pool)
{
	mrcp_resource_t *resource = apr_palloc(pool,sizeof(mrcp_resource_t));
	mrcp_resource_init(resource);

	resource->resourcify_message_by_id = synth_message_resourcify_by_id;
	resource->resourcify_message_by_name = synth_message_resourcify_by_name;

	resource->create_client_state_machine = mrcp_synth_client_state_machine_create;
	resource->create_server_state_machine = mrcp_synth_server_state_machine_create;
	return resource;
}
