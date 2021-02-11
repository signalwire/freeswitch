/*
 * Copyright 2008-2015 Arsen Chaloyan
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
#include "mrcp_resource.h"

/** String table of MRCP recognizer methods (mrcp_recognizer_method_id) */
static const apt_str_table_item_t v1_recog_method_string_table[] = {
	{{"SET-PARAMS",               10},10},
	{{"GET-PARAMS",               10},10},
	{{"DEFINE-GRAMMAR",           14},2},
	{{"RECOGNIZE",                 9},7},
	{{"INTERPRET",                 9},0},
	{{"GET-RESULT",               10},6},
	{{"RECOGNITION-START-TIMERS", 24},7},
	{{"STOP",                      4},2},
	{{"START-PHRASE-ENROLLMENT",  23},2},
	{{"ENROLLMENT-ROLLBACK",      19},2},
	{{"END-PHRASE-ENROLLMENT",    21},5},
	{{"MODIFY-PHRASE",            13},0},
	{{"DELETE-PHRASE",            13},2}
};

/** String table of MRCPv2 recognizer methods (mrcp_recognizer_method_id) */
static const apt_str_table_item_t v2_recog_method_string_table[] = {
	{{"SET-PARAMS",               10},10},
	{{"GET-PARAMS",               10},10},
	{{"DEFINE-GRAMMAR",           14},2},
	{{"RECOGNIZE",                 9},0},
	{{"INTERPRET",                 9},0},
	{{"GET-RESULT",               10},6},
	{{"START-INPUT-TIMERS",       18},7},
	{{"STOP",                      4},2},
	{{"START-PHRASE-ENROLLMENT",  23},6},
	{{"ENROLLMENT-ROLLBACK",      19},2},
	{{"END-PHRASE-ENROLLMENT",    21},5},
	{{"MODIFY-PHRASE",            13},0},
	{{"DELETE-PHRASE",            13},2}
};

/** String table of MRCP recognizer events (mrcp_recognizer_event_id) */
static const apt_str_table_item_t v1_recog_event_string_table[] = {
	{{"START-OF-SPEECH",          15},0},
	{{"RECOGNITION-COMPLETE",     20},0},
	{{"INTERPRETATION-COMPLETE",  23},0}
};

/** String table of MRCPv2 recognizer events (mrcp_recognizer_event_id) */
static const apt_str_table_item_t v2_recog_event_string_table[] = {
	{{"START-OF-INPUT",           14},0},
	{{"RECOGNITION-COMPLETE",     20},0},
	{{"INTERPRETATION-COMPLETE",  23},0}
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

/** Create MRCP recognizer resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_recog_resource_create(apr_pool_t *pool)
{
	mrcp_resource_t *resource = mrcp_resource_create(pool);

	resource->method_count = RECOGNIZER_METHOD_COUNT;
	resource->event_count = RECOGNIZER_EVENT_COUNT;
	resource->get_method_str_table = recog_method_string_table_get;
	resource->get_event_str_table = recog_event_string_table_get;
	resource->get_resource_header_vtable = mrcp_recog_header_vtable_get;
	return resource;
}
