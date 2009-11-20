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

#ifndef __MPF_RTP_STREAM_H__
#define __MPF_RTP_STREAM_H__

/**
 * @file mpf_rtp_stream.h
 * @brief MPF RTP Stream
 */ 

#include "mpf_stream.h"
#include "mpf_rtp_descriptor.h"

APT_BEGIN_EXTERN_C

/**
 * Create RTP stream.
 * @param termination the back pointer to hold
 * @param config the configuration to use
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_audio_stream_t*) mpf_rtp_stream_create(mpf_termination_t *termination, mpf_rtp_config_t *config, apr_pool_t *pool);

/**
 * Add/enable RTP stream.
 * @param stream RTP stream to add
 */
MPF_DECLARE(apt_bool_t) mpf_rtp_stream_add(mpf_audio_stream_t *stream);

/**
 * Subtract/disable RTP stream.
 * @param stream RTP stream to subtract
 */
MPF_DECLARE(apt_bool_t) mpf_rtp_stream_remove(mpf_audio_stream_t *stream);

/**
 * Modify RTP stream.
 * @param stream RTP stream to modify
 * @param descriptor the descriptor to modify stream according
 */
MPF_DECLARE(apt_bool_t) mpf_rtp_stream_modify(mpf_audio_stream_t *stream, mpf_rtp_stream_descriptor_t *descriptor);

APT_END_EXTERN_C

#endif /*__MPF_RTP_STREAM_H__*/
