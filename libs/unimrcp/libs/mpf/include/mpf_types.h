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

#ifndef __MPF_TYPES_H__
#define __MPF_TYPES_H__

/**
 * @file mpf_types.h
 * @brief MPF Types Declarations
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

/** Opaque MPF engine declaration */
typedef struct mpf_engine_t mpf_engine_t;

/** Opaque codec manager declaration */
typedef struct mpf_codec_manager_t mpf_codec_manager_t;

/** Opaque MPF context declaration */
typedef struct mpf_context_t mpf_context_t;

/** Opaque MPF termination declaration */
typedef struct mpf_termination_t mpf_termination_t;

/** Opaque MPF termination factory declaration */
typedef struct mpf_termination_factory_t mpf_termination_factory_t;

/** Opaque MPF audio stream declaration */
typedef struct mpf_audio_stream_t mpf_audio_stream_t;

/** Opaque MPF video stream declaration */
typedef struct mpf_video_stream_t mpf_video_stream_t;

/** Termination vtable declaration */
typedef struct mpf_termination_vtable_t mpf_termination_vtable_t;

/** Table of termination virtual methods */
struct mpf_termination_vtable_t {
	/** Virtual termination destroy method */
	apt_bool_t (*destroy)(mpf_termination_t *termination);
	/** Virtual termination modify method */
	apt_bool_t (*modify)(mpf_termination_t *termination, void *descriptor);
};

APT_END_EXTERN_C

#endif /*__MPF_TYPES_H__*/
