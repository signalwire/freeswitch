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

#ifndef __MPF_FRAME_BUFFER_H__
#define __MPF_FRAME_BUFFER_H__

/**
 * @file mpf_frame_buffer.h
 * @brief Buffer of Media Frames
 */ 

#include "mpf_frame.h"

APT_BEGIN_EXTERN_C

/** Opaque frame buffer declaration */
typedef struct mpf_frame_buffer_t mpf_frame_buffer_t;


/** Create frame buffer */
mpf_frame_buffer_t* mpf_frame_buffer_create(apr_size_t frame_size, apr_size_t frame_count, apr_pool_t *pool);

/** Destroy frame buffer */
void mpf_frame_buffer_destroy(mpf_frame_buffer_t *buffer);

/** Restart frame buffer */
apt_bool_t mpf_frame_buffer_restart(mpf_frame_buffer_t *buffer);

/** Write frame to buffer */
apt_bool_t mpf_frame_buffer_write(mpf_frame_buffer_t *buffer, const mpf_frame_t *frame);

/** Read frame from buffer */
apt_bool_t mpf_frame_buffer_read(mpf_frame_buffer_t *buffer, mpf_frame_t *frame);

APT_END_EXTERN_C

#endif /*__MPF_FRAME_BUFFER_H__*/
