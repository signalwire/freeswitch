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

#ifndef MPF_STREAM_DESCRIPTOR_H
#define MPF_STREAM_DESCRIPTOR_H

/**
 * @file mpf_stream_descriptor.h
 * @brief MPF Stream Descriptor
 */ 

#include "mpf_codec_descriptor.h"

APT_BEGIN_EXTERN_C

/** Stream capabilities declaration */
typedef struct mpf_stream_capabilities_t mpf_stream_capabilities_t;

/** Stream directions (none, send, receive, duplex) */
typedef enum {
	STREAM_DIRECTION_NONE    = 0x0, /**< none */
	STREAM_DIRECTION_SEND    = 0x1, /**< send (sink) */
	STREAM_DIRECTION_RECEIVE = 0x2, /**< receive (source) */

	STREAM_DIRECTION_DUPLEX  = STREAM_DIRECTION_SEND | STREAM_DIRECTION_RECEIVE /**< duplex */
} mpf_stream_direction_e; 


/** Stream capabilities */
struct mpf_stream_capabilities_t {
	/** Supported directions either send, receive or bidirectional stream (bitmask of mpf_stream_direction_e) */
	mpf_stream_direction_e   direction;
	/** Codec capabilities (supported codecs and named events) */
	mpf_codec_capabilities_t codecs;
};

/** Create stream capabilities */
MPF_DECLARE(mpf_stream_capabilities_t*) mpf_stream_capabilities_create(mpf_stream_direction_e directions, apr_pool_t *pool);

/** Create source stream capabilities */
static APR_INLINE mpf_stream_capabilities_t* mpf_source_stream_capabilities_create(apr_pool_t *pool)
{
	return mpf_stream_capabilities_create(STREAM_DIRECTION_RECEIVE,pool);
}

/** Create sink stream capabilities */
static APR_INLINE mpf_stream_capabilities_t* mpf_sink_stream_capabilities_create(apr_pool_t *pool)
{
	return mpf_stream_capabilities_create(STREAM_DIRECTION_SEND,pool);
}

/** Clone stream capabilities */
MPF_DECLARE(mpf_stream_capabilities_t*) mpf_stream_capabilities_clone(const mpf_stream_capabilities_t *src_capabilities, apr_pool_t *pool);

/** Merge stream capabilities */
MPF_DECLARE(apt_bool_t) mpf_stream_capabilities_merge(mpf_stream_capabilities_t *capabilities, const mpf_stream_capabilities_t *src_capabilities, apr_pool_t *pool);


/** Get reverse direction */
static APR_INLINE mpf_stream_direction_e mpf_stream_reverse_direction_get(mpf_stream_direction_e direction)
{
	mpf_stream_direction_e rev_direction = direction;
	if(rev_direction == STREAM_DIRECTION_SEND) {
		rev_direction = STREAM_DIRECTION_RECEIVE;
	}
	else if(rev_direction == STREAM_DIRECTION_RECEIVE) {
		rev_direction = STREAM_DIRECTION_SEND;
	}
	return rev_direction;
}


APT_END_EXTERN_C

#endif /* MPF_STREAM_DESCRIPTOR_H */
