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

#ifndef __MPF_DECODER_H__
#define __MPF_DECODER_H__

/**
 * @file mpf_decoder.h
 * @brief MPF Stream Decoder
 */ 

#include "mpf_stream.h"

APT_BEGIN_EXTERN_C

/**
 * Create audio stream decoder.
 * @param source the source to get encoded stream from
 * @param codec the codec to use for decode
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_audio_stream_t*) mpf_decoder_create(mpf_audio_stream_t *source, mpf_codec_t *codec, apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__MPF_ENCODER_H__*/
