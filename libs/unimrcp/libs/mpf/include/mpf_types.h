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

#ifndef MPF_TYPES_H
#define MPF_TYPES_H

/**
 * @file mpf_types.h
 * @brief MPF Types Declarations
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

/** Opaque MPF engine declaration */
typedef struct mpf_engine_t mpf_engine_t;

/** Opaque MPF engine factory declaration */
typedef struct mpf_engine_factory_t mpf_engine_factory_t;

/** Opaque MPF scheduler declaration */
typedef struct mpf_scheduler_t mpf_scheduler_t;

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


APT_END_EXTERN_C

#endif /* MPF_TYPES_H */
