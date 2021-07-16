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

#ifndef MPF_TERMINATION_FACTORY_H
#define MPF_TERMINATION_FACTORY_H

/**
 * @file mpf_termination_factory.h
 * @brief MPF Termination Factory
 */ 

#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/** MPF termination factory */
struct mpf_termination_factory_t {
	/** Virtual create */
	mpf_termination_t* (*create_termination)(mpf_termination_factory_t *factory, void *obj, apr_pool_t *pool);
	/** Virtual assign engine */
	apt_bool_t (*assign_engine)(mpf_termination_factory_t *factory, mpf_engine_t *media_engine);
};

/**
 * Assign media engine to termination factory.
 * @param termination_factory the termination factory to assign media engine to
 * @param media_engine the media engine to assign
 */
MPF_DECLARE(apt_bool_t) mpf_termination_factory_engine_assign(
										mpf_termination_factory_t *termination_factory,
										mpf_engine_t *media_engine);

/**
 * Create MPF termination from termination factory.
 * @param termination_factory the termination factory to create termination from
 * @param obj the external object associated with termination
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_termination_t*) mpf_termination_create(
										mpf_termination_factory_t *termination_factory,
										void *obj,
										apr_pool_t *pool);

/**
 * Create raw MPF termination.
 * @param obj the external object associated with termination
 * @param audio_stream the audio stream of the termination
 * @param video_stream the video stream of the termination
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_termination_t*) mpf_raw_termination_create(
										void *obj,
										mpf_audio_stream_t *audio_stream, 
										mpf_video_stream_t *video_stream, 
										apr_pool_t *pool);

/**
 * Destroy MPF termination.
 * @param termination the termination to destroy
 */
MPF_DECLARE(apt_bool_t) mpf_termination_destroy(mpf_termination_t *termination);

/**
 * Get termination name.
 * @param termination the termination to get name of
 */
MPF_DECLARE(const char*) mpf_termination_name_get(const mpf_termination_t *termination);

/**
 * Get associated object.
 * @param termination the termination to get object from
 */
MPF_DECLARE(void*) mpf_termination_object_get(const mpf_termination_t *termination);

/**
 * Get audio stream.
 * @param termination the termination to get audio stream from
 */
MPF_DECLARE(mpf_audio_stream_t*) mpf_termination_audio_stream_get(const mpf_termination_t *termination);

/**
 * Get video stream.
 * @param termination the termination to get video stream from
 */
MPF_DECLARE(mpf_video_stream_t*) mpf_termination_video_stream_get(const mpf_termination_t *termination);


APT_END_EXTERN_C

#endif /* MPF_TERMINATION_FACTORY_H */
