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

#ifndef __MPF_TERMINATION_H__
#define __MPF_TERMINATION_H__

/**
 * @file mpf_termination.h
 * @brief MPF Termination
 */ 

#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/** Prototype of termination event handler */
typedef apt_bool_t (*mpf_termination_event_handler_f)(mpf_termination_t *termination, int event_id, void *descriptor);

/** MPF Termination */
struct mpf_termination_t {
	/** Pool to allocate memory from */
	apr_pool_t                     *pool;
	/** External object */
	void                           *obj;
	/** Object to send events to */
	void                           *event_handler_obj;
	/** Event handler */
	mpf_termination_event_handler_f event_handler;
	/** Codec manager */
	const mpf_codec_manager_t      *codec_manager;
	/** Termination factory entire termination created by */
	mpf_termination_factory_t      *termination_factory;
	/** Table of virtual methods */
	const mpf_termination_vtable_t *vtable;
	/** Slot in context */
	apr_size_t                      slot;

	/** Audio stream */
	mpf_audio_stream_t             *audio_stream;
	/** Video stream */
	mpf_video_stream_t             *video_stream;
};

/** MPF termination factory */
struct mpf_termination_factory_t {
	/** Virtual create */
	mpf_termination_t* (*create_termination)(mpf_termination_factory_t *factory, void *obj, apr_pool_t *pool);

	/* more to add */
};


/**
 * Create MPF termination base.
 * @param termination_factory the termination factory
 * @param obj the external object associated with termination
 * @param vtable the table of virtual functions of termination
 * @param audio_stream the audio stream
 * @param video_stream the video stream
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_termination_t*) mpf_termination_base_create(
										mpf_termination_factory_t *termination_factory,
										void *obj,
										const mpf_termination_vtable_t *vtable,
										mpf_audio_stream_t *audio_stream, 
										mpf_video_stream_t *video_stream, 
										apr_pool_t *pool);

/**
 * Modify MPF termination.
 * @param termination the termination to modify
 * @param descriptor the termination specific descriptor
 */
MPF_DECLARE(apt_bool_t) mpf_termination_modify(mpf_termination_t *termination, void *descriptor);

/**
 * Validate MPF termination.
 * @param termination the termination to validate
 */
MPF_DECLARE(apt_bool_t) mpf_termination_validate(mpf_termination_t *termination);

/**
 * Destroy MPF termination.
 * @param termination the termination to destroy
 */
MPF_DECLARE(apt_bool_t) mpf_termination_destroy(mpf_termination_t *termination);

/**
 * Get associated object.
 * @param termination the termination to get object from
 */
MPF_DECLARE(void*) mpf_termination_object_get(mpf_termination_t *termination);


/**
 * Create MPF termination by termination factory.
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

APT_END_EXTERN_C

#endif /*__MPF_TERMINATION_H__*/
