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

#ifndef __RTSP_STREAM_H__
#define __RTSP_STREAM_H__

/**
 * @file rtsp_stream.h
 * @brief RTSP Stream Parser and Generator
 */ 

#include "rtsp_message.h"

APT_BEGIN_EXTERN_C

/** Status of RTSP stream processing (parse/generate) */
typedef enum {
	RTSP_STREAM_STATUS_COMPLETE,
	RTSP_STREAM_STATUS_INCOMPLETE,
	RTSP_STREAM_STATUS_INVALID
} rtsp_stream_status_e;

/** Opaque RTSP parser declaration */
typedef struct rtsp_parser_t rtsp_parser_t;
/** Opaque RTSP generator declaration */
typedef struct rtsp_generator_t rtsp_generator_t;

/** RTSP message handler */
typedef apt_bool_t (*rtsp_message_handler_f)(void *obj, rtsp_message_t *message, rtsp_stream_status_e status);

/** Create RTSP stream parser */
RTSP_DECLARE(rtsp_parser_t*) rtsp_parser_create(apr_pool_t *pool);

/** Parse RTSP stream */
RTSP_DECLARE(rtsp_stream_status_e) rtsp_parser_run(rtsp_parser_t *parser, apt_text_stream_t *stream);

/** Get parsed RTSP message */
RTSP_DECLARE(rtsp_message_t*) rtsp_parser_message_get(const rtsp_parser_t *parser);


/** Create RTSP stream generator */
RTSP_DECLARE(rtsp_generator_t*) rtsp_generator_create(apr_pool_t *pool);

/** Set RTSP message to generate */
RTSP_DECLARE(apt_bool_t) rtsp_generator_message_set(rtsp_generator_t *generator, rtsp_message_t *message);

/** Generate RTSP stream */
RTSP_DECLARE(rtsp_stream_status_e) rtsp_generator_run(rtsp_generator_t *generator, apt_text_stream_t *stream);


/** Walk through RTSP stream and call message handler for each parsed message */
RTSP_DECLARE(apt_bool_t) rtsp_stream_walk(rtsp_parser_t *parser, apt_text_stream_t *stream, rtsp_message_handler_f handler, void *obj);

APT_END_EXTERN_C

#endif /*__RTSP_STREAM_H__*/
