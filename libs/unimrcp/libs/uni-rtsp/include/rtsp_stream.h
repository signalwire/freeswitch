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
 * $Id: rtsp_stream.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef RTSP_STREAM_H
#define RTSP_STREAM_H

/**
 * @file rtsp_stream.h
 * @brief RTSP Stream Parser and Generator
 */ 

#include "rtsp_message.h"
#include "apt_text_message.h"

APT_BEGIN_EXTERN_C

/** Opaque RTSP parser declaration */
typedef struct rtsp_parser_t rtsp_parser_t;
/** Opaque RTSP generator declaration */
typedef struct rtsp_generator_t rtsp_generator_t;


/** Create RTSP stream parser */
RTSP_DECLARE(rtsp_parser_t*) rtsp_parser_create(apr_pool_t *pool);

/** Parse RTSP stream */
RTSP_DECLARE(apt_message_status_e) rtsp_parser_run(rtsp_parser_t *parser, apt_text_stream_t *stream, rtsp_message_t **message);


/** Create RTSP stream generator */
RTSP_DECLARE(rtsp_generator_t*) rtsp_generator_create(apr_pool_t *pool);

/** Generate RTSP stream */
RTSP_DECLARE(apt_message_status_e) rtsp_generator_run(rtsp_generator_t *generator, rtsp_message_t *message, apt_text_stream_t *stream);


APT_END_EXTERN_C

#endif /* RTSP_STREAM_H */
