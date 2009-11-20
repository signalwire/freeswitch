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
#include "mrcp_resource.h"

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

static APR_INLINE const apt_str_table_item_t* synth_method_string_table_get(mrcp_version_e version)
{
	return synth_method_string_table;
}

static APR_INLINE const apt_str_table_item_t* synth_event_string_table_get(mrcp_version_e version)
{
	return synth_event_string_table;
}

/** Create MRCP synthesizer resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_synth_resource_create(apr_pool_t *pool)
{
	mrcp_resource_t *resource = mrcp_resource_create(pool);

	resource->method_count = SYNTHESIZER_METHOD_COUNT;
	resource->event_count = SYNTHESIZER_EVENT_COUNT;
	resource->get_method_str_table = synth_method_string_table_get;
	resource->get_event_str_table = synth_event_string_table_get;
	resource->get_resource_header_vtable = mrcp_synth_header_vtable_get;
	return resource;
}
