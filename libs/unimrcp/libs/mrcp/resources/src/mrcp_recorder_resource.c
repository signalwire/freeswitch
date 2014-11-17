/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id: mrcp_recorder_resource.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mrcp_recorder_resource.h"
#include "mrcp_recorder_header.h"
#include "mrcp_resource.h"

/** String table of MRCP recorder methods (mrcp_recorder_method_id) */
static const apt_str_table_item_t recorder_method_string_table[] = {
	{{"SET-PARAMS",         10},10},
	{{"GET-PARAMS",         10},0},
	{{"RECORD",              6},0},
	{{"STOP",                4},2},
	{{"START-INPUT-TIMERS", 18},2}
};

/** String table of MRCP recorder events (mrcp_recorder_event_id) */
static const apt_str_table_item_t recorder_event_string_table[] = {
	{{"START-OF-INPUT",     14},0},
	{{"RECORD-COMPLETE",    15},0}
};

static APR_INLINE const apt_str_table_item_t* recorder_method_string_table_get(mrcp_version_e version)
{
	return recorder_method_string_table;
}

static APR_INLINE const apt_str_table_item_t* recorder_event_string_table_get(mrcp_version_e version)
{
	return recorder_event_string_table;
}

/** Create MRCP recorder resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_recorder_resource_create(apr_pool_t *pool)
{
	mrcp_resource_t *resource = mrcp_resource_create(pool);

	resource->method_count = RECORDER_METHOD_COUNT;
	resource->event_count = RECORDER_EVENT_COUNT;
	resource->get_method_str_table = recorder_method_string_table_get;
	resource->get_event_str_table = recorder_event_string_table_get;
	resource->get_resource_header_vtable = mrcp_recorder_header_vtable_get;
	return resource;
}
