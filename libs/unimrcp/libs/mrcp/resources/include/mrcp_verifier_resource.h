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

#ifndef MRCP_VERIFIER_RESOURCE_H
#define MRCP_VERIFIER_RESOURCE_H

/**
 * @file mrcp_verifier_resource.h
 * @brief MRCP Verifier Resource
 */ 

#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** MRCP verifier methods */
typedef enum {
	VERIFIER_SET_PARAMS,
	VERIFIER_GET_PARAMS,
	VERIFIER_START_SESSION,
	VERIFIER_END_SESSION,
	VERIFIER_QUERY_VOICEPRINT,
	VERIFIER_DELETE_VOICEPRINT,
	VERIFIER_VERIFY,
	VERIFIER_VERIFY_FROM_BUFFER,
	VERIFIER_VERIFY_ROLLBACK,
	VERIFIER_STOP,
	VERIFIER_CLEAR_BUFFER,
	VERIFIER_START_INPUT_TIMERS,
	VERIFIER_GET_INTERMIDIATE_RESULT,

	VERIFIER_METHOD_COUNT
} mrcp_verifier_method_id;

/** MRCP verifier events */
typedef enum {
	VERIFIER_START_OF_INPUT,
	VERIFIER_VERIFICATION_COMPLETE,

	VERIFIER_EVENT_COUNT
} mrcp_verifier_event_id;

/** Create MRCP verifier resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_verifier_resource_create(apr_pool_t *pool);

APT_END_EXTERN_C

#endif /* MRCP_VERIFIER_RESOURCE_H */
