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
 * $Id: 
 */

#include "mrcp_verifier_resource.h"
#include "mrcp_verifier_header.h"
#include "mrcp_resource.h"

/** String table of MRCP verifier methods (mrcp_verifier_method_id) */
static const apt_str_table_item_t verifier_method_string_table[] = {
	{{"SET-PARAMS",             10},10},
	{{"GET-PARAMS",             10},10},
	{{"START-SESSION",          13},8},
	{{"END-SESSION",            11},0},
	{{"QUERY-VOICEPRINT",       16},0},
	{{"DELETE-VOICEPRINT",      17},0},
	{{"VERIFY",                  6},6},
	{{"VERIFY-FROM-BUFFER",     18},7},
	{{"VERIFY-ROLLBACK",        15},7},
	{{"STOP",                    4},2},
	{{"CLEAR-BUFFER",           12},0},
	{{"START-INPUT-TIMERS",     18},6},
	{{"GET-INTERMEDIATE-RESULT",23},4},
};

/** String table of MRCP verifier events (mrcp_verifier_event_id) */
static const apt_str_table_item_t verifier_event_string_table[] = {
	{{"START-OF-INPUT",       14},0},
	{{"VERIFICATION-COMPLETE",21},0},
};

static APR_INLINE const apt_str_table_item_t* verifier_method_string_table_get(mrcp_version_e version)
{
	return verifier_method_string_table;
}

static APR_INLINE const apt_str_table_item_t* verifier_event_string_table_get(mrcp_version_e version)
{
	return verifier_event_string_table;
}


/** Create MRCP verifier resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_verifier_resource_create(apr_pool_t *pool)
{
	mrcp_resource_t *resource = mrcp_resource_create(pool);

	resource->method_count = VERIFIER_METHOD_COUNT;
	resource->event_count = VERIFIER_EVENT_COUNT;
	resource->get_method_str_table = verifier_method_string_table_get;
	resource->get_event_str_table = verifier_event_string_table_get;
	resource->get_resource_header_vtable = mrcp_verifier_header_vtable_get;
	return resource;
}
