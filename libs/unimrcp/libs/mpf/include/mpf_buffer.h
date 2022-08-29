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

#ifndef MPF_BUFFER_H
#define MPF_BUFFER_H

/**
 * @file mpf_buffer.h
 * @brief Buffer of Media Chunks
 */ 

#include "mpf_frame.h"

APT_BEGIN_EXTERN_C

/** Opaque media buffer declaration */
typedef struct mpf_buffer_t mpf_buffer_t;


/** Create buffer */
mpf_buffer_t* mpf_buffer_create(apr_pool_t *pool);

/** Destroy buffer */
void mpf_buffer_destroy(mpf_buffer_t *buffer);

/** Restart buffer */
apt_bool_t mpf_buffer_restart(mpf_buffer_t *buffer);

/** Write audio chunk to buffer */
apt_bool_t mpf_buffer_audio_write(mpf_buffer_t *buffer, void *data, apr_size_t size);

/** Write event to buffer */
apt_bool_t mpf_buffer_event_write(mpf_buffer_t *buffer, mpf_frame_type_e event_type);

/** Read media frame from buffer */
apt_bool_t mpf_buffer_frame_read(mpf_buffer_t *buffer, mpf_frame_t *media_frame);

/** Get size of buffer **/
apr_size_t mpf_buffer_get_size(const mpf_buffer_t *buffer);

APT_END_EXTERN_C

#endif /* MPF_BUFFER_H */
