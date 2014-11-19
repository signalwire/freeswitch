/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id: mpf_encoder.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MPF_ENCODER_H
#define MPF_ENCODER_H

/**
 * @file mpf_encoder.h
 * @brief MPF Stream Encoder
 */ 

#include "mpf_stream.h"

APT_BEGIN_EXTERN_C

/**
 * Create audio stream encoder.
 * @param sink the sink to write encoded stream to
 * @param codec the codec to use for encode
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_audio_stream_t*) mpf_encoder_create(mpf_audio_stream_t *sink, mpf_codec_t *codec, apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* MPF_ENCODER_H */
