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

#ifndef __MRCP_SYNTH_RESOURCE_H__
#define __MRCP_SYNTH_RESOURCE_H__

/**
 * @file mrcp_synth_resource.h
 * @brief MRCP Synthesizer Resource
 */ 

#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** MRCP synthesizer methods */
typedef enum {
	SYNTHESIZER_SET_PARAMS,
	SYNTHESIZER_GET_PARAMS,
	SYNTHESIZER_SPEAK,
	SYNTHESIZER_STOP,
	SYNTHESIZER_PAUSE,
	SYNTHESIZER_RESUME,
	SYNTHESIZER_BARGE_IN_OCCURRED,
	SYNTHESIZER_CONTROL,
	SYNTHESIZER_DEFINE_LEXICON,

	SYNTHESIZER_METHOD_COUNT
} mrcp_synthesizer_method_id;

/** MRCP synthesizer events */
typedef enum {
	SYNTHESIZER_SPEECH_MARKER,
	SYNTHESIZER_SPEAK_COMPLETE,

	SYNTHESIZER_EVENT_COUNT
} mrcp_synthesizer_event_id;


/** Create MRCP synthesizer resource */
MRCP_DECLARE(mrcp_resource_t*) mrcp_synth_resource_create(apr_pool_t *pool);

APT_END_EXTERN_C

#endif /*__MRCP_SYNTH_RESOURCE_H__*/
