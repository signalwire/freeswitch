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

#ifndef __MPF_JITTER_BUFFER_H__
#define __MPF_JITTER_BUFFER_H__

/**
 * @file mpf_jitter_buffer.h
 * @brief Jitter Buffer
 */ 

#include "mpf_frame.h"
#include "mpf_codec.h"
#include "mpf_rtp_descriptor.h"

APT_BEGIN_EXTERN_C

/** Jitter buffer write result */
typedef enum {
	JB_OK,                   /**< successful write */
	JB_DISCARD_NOT_ALLIGNED, /**< discarded write (frame isn't alligned to CODEC_FRAME_TIME_BASE) */
	JB_DISCARD_TOO_LATE,     /**< discarded write (frame is arrived too late) */
	JB_DISCARD_TOO_EARLY,    /**< discarded write (frame is arrived too early, buffer is full) */
} jb_result_t;

/** Opaque jitter buffer declaration */
typedef struct mpf_jitter_buffer_t mpf_jitter_buffer_t;


/** Create jitter buffer */
mpf_jitter_buffer_t* mpf_jitter_buffer_create(mpf_jb_config_t *jb_config, mpf_codec_descriptor_t *descriptor, mpf_codec_t *codec, apr_pool_t *pool);

/** Destroy jitter buffer */
void mpf_jitter_buffer_destroy(mpf_jitter_buffer_t *jb);

/** Restart jitter buffer */
apt_bool_t mpf_jitter_buffer_restart(mpf_jitter_buffer_t *jb);

/** Write audio data to jitter buffer */
jb_result_t mpf_jitter_buffer_write(mpf_jitter_buffer_t *jb, void *buffer, apr_size_t size, apr_uint32_t ts);

/** Write named event to jitter buffer */
jb_result_t mpf_jitter_buffer_event_write(mpf_jitter_buffer_t *jb, const mpf_named_event_frame_t *named_event, apr_uint32_t ts, apr_byte_t marker);

/** Read media frame from jitter buffer */
apt_bool_t mpf_jitter_buffer_read(mpf_jitter_buffer_t *jb, mpf_frame_t *media_frame);

APT_END_EXTERN_C

#endif /*__MPF_JITTER_BUFFER_H__*/
