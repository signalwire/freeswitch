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

/** Termination vtable declaration */
typedef struct mpf_termination_vtable_t mpf_termination_vtable_t;

/** Table of termination virtual methods */
struct mpf_termination_vtable_t {
	/** Virtual termination destroy method */
	apt_bool_t (*destroy)(mpf_termination_t *termination);

	/** Virtual termination add method */
	apt_bool_t (*add)(mpf_termination_t *termination, void *descriptor);
	/** Virtual termination modify method */
	apt_bool_t (*modify)(mpf_termination_t *termination, void *descriptor);
	/** Virtual termination subtract method */
	apt_bool_t (*subtract)(mpf_termination_t *termination);
};


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
	/** Timer manager */
	mpf_timer_manager_t            *timer_manager;
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
 * Add MPF termination.
 * @param termination the termination to add
 * @param descriptor the termination specific descriptor
 */
MPF_DECLARE(apt_bool_t) mpf_termination_add(mpf_termination_t *termination, void *descriptor);

/**
 * Modify MPF termination.
 * @param termination the termination to modify
 * @param descriptor the termination specific descriptor
 */
MPF_DECLARE(apt_bool_t) mpf_termination_modify(mpf_termination_t *termination, void *descriptor);

/**
 * Subtract MPF termination.
 * @param termination the termination to subtract
 */
MPF_DECLARE(apt_bool_t) mpf_termination_subtract(mpf_termination_t *termination);


APT_END_EXTERN_C

#endif /*__MPF_TERMINATION_H__*/
