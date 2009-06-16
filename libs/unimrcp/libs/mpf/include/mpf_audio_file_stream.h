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

#ifndef __MPF_AUDIO_FILE_STREAM_H__
#define __MPF_AUDIO_FILE_STREAM_H__

/**
 * @file mpf_audio_file_stream.h
 * @brief MPF Audio FIle Stream
 */ 

#include "mpf_stream.h"
#include "mpf_audio_file_descriptor.h"

APT_BEGIN_EXTERN_C

/**
 * Create file stream.
 * @param termination the back pointer to hold
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_audio_stream_t*) mpf_file_stream_create(mpf_termination_t *termination, apr_pool_t *pool);

/**
 * Modify file stream.
 * @param stream file stream to modify
 * @param descriptor the descriptor to modify stream according
 */
MPF_DECLARE(apt_bool_t) mpf_file_stream_modify(mpf_audio_stream_t *stream, mpf_audio_file_descriptor_t *descriptor);

APT_END_EXTERN_C

#endif /*__MPF_AUDIO_FILE_STREAM_H__*/
