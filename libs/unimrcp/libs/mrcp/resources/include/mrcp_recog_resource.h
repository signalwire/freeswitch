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

#ifndef MRCP_RECOG_RESOURCE_H
#define MRCP_RECOG_RESOURCE_H

/**
 * @file mrcp_recog_resource.h
 * @brief MRCP Recognizer Resource
 */ 

#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** MRCP recognizer methods */
typedef enum {
	RECOGNIZER_SET_PARAMS,
	RECOGNIZER_GET_PARAMS,
	RECOGNIZER_DEFINE_GRAMMAR,
	RECOGNIZER_RECOGNIZE,
	RECOGNIZER_INTERPRET,
	RECOGNIZER_GET_RESULT,
	RECOGNIZER_START_INPUT_TIMERS,
	RECOGNIZER_STOP,
	RECOGNIZER_START_PHRASE_ENROLLMENT,
	RECOGNIZER_ENROLLMENT_ROLLBACK,
	RECOGNIZER_END_PHRASE_ENROLLMENT,
	RECOGNIZER_MODIFY_PHRASE,
	RECOGNIZER_DELETE_PHRASE,

	RECOGNIZER_METHOD_COUNT
} mrcp_recognizer_method_id;

/** MRCP recognizer events */
typedef enum {
	RECOGNIZER_START_OF_INPUT,
	RECOGNIZER_RECOGNITION_COMPLETE,
	RECOGNIZER_INTERPRETATION_COMPLETE,

	RECOGNIZER_EVENT_COUNT
} mrcp_recognizer_event_id;

/** Create MRCP recognizer resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_recog_resource_create(apr_pool_t *pool);

APT_END_EXTERN_C

#endif /* MRCP_RECOG_RESOURCE_H */
