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
 * $Id: mrcp_recorder_resource.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MRCP_RECORDER_RESOURCE_H
#define MRCP_RECORDER_RESOURCE_H

/**
 * @file mrcp_recorder_resource.h
 * @brief MRCP Recorder Resource
 */ 

#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** MRCP recorder methods */
typedef enum {
	RECORDER_SET_PARAMS,
	RECORDER_GET_PARAMS,
	RECORDER_RECORD,
	RECORDER_STOP,
	RECORDER_START_INPUT_TIMERS,

	RECORDER_METHOD_COUNT
} mrcp_recorder_method_id;

/** MRCP recorder events */
typedef enum {
	RECORDER_START_OF_INPUT,
	RECORDER_RECORD_COMPLETE,

	RECORDER_EVENT_COUNT
} mrcp_recorder_event_id;

/** Create MRCP recorder resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_recorder_resource_create(apr_pool_t *pool);

APT_END_EXTERN_C

#endif /* MRCP_RECORDER_RESOURCE_H */
