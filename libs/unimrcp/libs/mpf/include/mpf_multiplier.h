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

#ifndef MPF_MULTIPLIER_H
#define MPF_MULTIPLIER_H

/**
 * @file mpf_multiplier.h
 * @brief MPF Stream Multiplier (1-source, n-sinks)
 */ 

#include "mpf_object.h"

APT_BEGIN_EXTERN_C

/**
 * Create audio stream multiplier.
 * @param source the audio source
 * @param sink_arr the array of audio sinks
 * @param sink_count the number of audio sinks
 * @param codec_manager the codec manager
 * @param name the informative name used for debugging
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_object_t*) mpf_multiplier_create(
								mpf_audio_stream_t *source,
								mpf_audio_stream_t **sink_arr,
								apr_size_t sink_count,
								const mpf_codec_manager_t *codec_manager,
								const char *name,
								apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* MPF_MULTIPLIER_H */
