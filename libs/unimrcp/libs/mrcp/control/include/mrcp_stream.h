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
 * $Id: mrcp_stream.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MRCP_STREAM_H
#define MRCP_STREAM_H

/**
 * @file mrcp_stream.h
 * @brief MRCP Stream Parser and Generator
 */ 

#include "apt_text_message.h"
#include "mrcp_types.h"

APT_BEGIN_EXTERN_C


/** Opaque MRCP parser declaration */
typedef struct mrcp_parser_t mrcp_parser_t;
/** Opaque MRCP generator declaration */
typedef struct mrcp_generator_t mrcp_generator_t;


/** Create MRCP stream parser */
MRCP_DECLARE(mrcp_parser_t*) mrcp_parser_create(const mrcp_resource_factory_t *resource_factory, apr_pool_t *pool);

/** Set resource by name to be used for parsing of MRCPv1 messages */
MRCP_DECLARE(void) mrcp_parser_resource_set(mrcp_parser_t *parser, const apt_str_t *resource_name);

/** Set verbose mode for the parser */
MRCP_DECLARE(void) mrcp_parser_verbose_set(mrcp_parser_t *parser, apt_bool_t verbose);

/** Parse MRCP stream */
MRCP_DECLARE(apt_message_status_e) mrcp_parser_run(mrcp_parser_t *parser, apt_text_stream_t *stream, mrcp_message_t **message);



/** Create MRCP stream generator */
MRCP_DECLARE(mrcp_generator_t*) mrcp_generator_create(const mrcp_resource_factory_t *resource_factory, apr_pool_t *pool);

/** Set verbose mode for the generator */
MRCP_DECLARE(void) mrcp_generator_verbose_set(mrcp_generator_t *generator, apt_bool_t verbose);

/** Generate MRCP stream */
MRCP_DECLARE(apt_message_status_e) mrcp_generator_run(mrcp_generator_t *generator, mrcp_message_t *message, apt_text_stream_t *stream);


/** Generate MRCP message (excluding message body) */
MRCP_DECLARE(apt_bool_t) mrcp_message_generate(const mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *stream);


APT_END_EXTERN_C

#endif /* MRCP_STREAM_H */
