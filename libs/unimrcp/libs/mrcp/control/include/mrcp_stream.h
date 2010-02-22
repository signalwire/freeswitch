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

#ifndef __MRCP_STREAM_H__
#define __MRCP_STREAM_H__

/**
 * @file mrcp_stream.h
 * @brief MRCP Stream Parser and Generator
 */ 

#include "apt_text_stream.h"
#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** Status of MRCP stream processing (parse/generate) */
typedef enum {
	MRCP_STREAM_STATUS_COMPLETE,
	MRCP_STREAM_STATUS_INCOMPLETE,
	MRCP_STREAM_STATUS_INVALID
} mrcp_stream_status_e;

/** Opaque MRCP parser declaration */
typedef struct mrcp_parser_t mrcp_parser_t;
/** Opaque MRCP generator declaration */
typedef struct mrcp_generator_t mrcp_generator_t;

/** MRCP message handler */
typedef apt_bool_t (*mrcp_message_handler_f)(void *obj, mrcp_message_t *message, mrcp_stream_status_e status);

/** Parse MRCP message (excluding message body) */
MRCP_DECLARE(apt_bool_t) mrcp_message_parse(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *stream);

/** Generate MRCP message (excluding message body) */
MRCP_DECLARE(apt_bool_t) mrcp_message_generate(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *stream);


/** Create MRCP stream parser */
MRCP_DECLARE(mrcp_parser_t*) mrcp_parser_create(mrcp_resource_factory_t *resource_factory, apr_pool_t *pool);

/** Set resource name to be used while parsing (MRCPv1 only) */
MRCP_DECLARE(void) mrcp_parser_resource_name_set(mrcp_parser_t *parser, const apt_str_t *resource_name);

/** Parse MRCP stream */
MRCP_DECLARE(mrcp_stream_status_e) mrcp_parser_run(mrcp_parser_t *parser, apt_text_stream_t *stream);

/** Get parsed MRCP message */
MRCP_DECLARE(mrcp_message_t*) mrcp_parser_message_get(const mrcp_parser_t *parser);


/** Create MRCP stream generator */
MRCP_DECLARE(mrcp_generator_t*) mrcp_generator_create(mrcp_resource_factory_t *resource_factory, apr_pool_t *pool);

/** Set MRCP message to generate */
MRCP_DECLARE(apt_bool_t) mrcp_generator_message_set(mrcp_generator_t *generator, mrcp_message_t *message);

/** Generate MRCP stream */
MRCP_DECLARE(mrcp_stream_status_e) mrcp_generator_run(mrcp_generator_t *generator, apt_text_stream_t *stream);

/** Walk through MRCP stream and call message handler for each parsed message */
MRCP_DECLARE(apt_bool_t) mrcp_stream_walk(mrcp_parser_t *parser, apt_text_stream_t *stream, mrcp_message_handler_f handler, void *obj);

APT_END_EXTERN_C

#endif /*__MRCP_STREAM_H__*/
