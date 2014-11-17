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
 * $Id: apt_text_message.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef APT_TEXT_MESSAGE_H
#define APT_TEXT_MESSAGE_H

/**
 * @file apt_text_message.h
 * @brief Text Message Interface (RFC5322)
 */ 

#include "apt_header_field.h"
#include "apt_text_stream.h"

APT_BEGIN_EXTERN_C

/** Status of text message processing (parsing/generation) */
typedef enum {
	APT_MESSAGE_STATUS_COMPLETE,
	APT_MESSAGE_STATUS_INCOMPLETE,
	APT_MESSAGE_STATUS_INVALID
} apt_message_status_e;


/** Opaque text message parser */
typedef struct apt_message_parser_t apt_message_parser_t;
/** Vtable of text message parser */
typedef struct apt_message_parser_vtable_t apt_message_parser_vtable_t;

/** Opaque text message generator */
typedef struct apt_message_generator_t apt_message_generator_t;
/** Vtable of text message generator */
typedef struct apt_message_generator_vtable_t apt_message_generator_vtable_t;

/** Temporary context associated with message and used for its parsing or generation */
typedef struct apt_message_context_t apt_message_context_t;

/** Create message parser */
APT_DECLARE(apt_message_parser_t*) apt_message_parser_create(void *obj, const apt_message_parser_vtable_t *vtable, apr_pool_t *pool);

/** Parse message by raising corresponding event handlers */
APT_DECLARE(apt_message_status_e) apt_message_parser_run(apt_message_parser_t *parser, apt_text_stream_t *stream, void **message);

/** Get external object associated with parser */
APT_DECLARE(void*) apt_message_parser_object_get(apt_message_parser_t *parser);

/** Set verbose mode for the parser */
APT_DECLARE(void) apt_message_parser_verbose_set(apt_message_parser_t *parser, apt_bool_t verbose);


/** Create message generator */
APT_DECLARE(apt_message_generator_t*) apt_message_generator_create(void *obj, const apt_message_generator_vtable_t *vtable, apr_pool_t *pool);

/** Generate message */
APT_DECLARE(apt_message_status_e) apt_message_generator_run(apt_message_generator_t *generator, void *message, apt_text_stream_t *stream);

/** Get external object associated with generator */
APT_DECLARE(void*) apt_message_generator_object_get(apt_message_generator_t *generator);

/** Set verbose mode for the parser */
APT_DECLARE(void) apt_message_generator_verbose_set(apt_message_generator_t *generator, apt_bool_t verbose);


/** Parse individual header field (name-value pair) */
APT_DECLARE(apt_header_field_t*) apt_header_field_parse(apt_text_stream_t *stream, apr_pool_t *pool);

/** Generate individual header field (name-value pair) */
APT_DECLARE(apt_bool_t) apt_header_field_generate(const apt_header_field_t *header_field, apt_text_stream_t *stream);

/** Parse header section */
APT_DECLARE(apt_bool_t) apt_header_section_parse(apt_header_section_t *header, apt_text_stream_t *stream, apr_pool_t *pool);

/** Generate header section */
APT_DECLARE(apt_bool_t) apt_header_section_generate(const apt_header_section_t *header, apt_text_stream_t *stream);


/** Temporary context associated with message and used for its parsing or generation */
struct apt_message_context_t {
	/** Context or ptotocol specific message */
	void                 *message;
	/** Header section of the message */
	apt_header_section_t *header;
	/** Body or content of the message */
	apt_str_t            *body;
};

/** Vtable of text message parser */
struct apt_message_parser_vtable_t {
	/** Start new message parsing by associating corresponding context and reading its start-line if applicable */
	apt_bool_t (*on_start)(apt_message_parser_t *parser, apt_message_context_t *context, apt_text_stream_t *stream, apr_pool_t *pool);
	/** Header section handler is invoked when entire header section has been read and parsed into header fields */
	apt_bool_t (*on_header_complete)(apt_message_parser_t *parser, apt_message_context_t *context);
	/** Body handler is invoked when entire body has been read */
	apt_bool_t (*on_body_complete)(apt_message_parser_t *parser, apt_message_context_t *context);
};

/** Vtable of text message generator */
struct apt_message_generator_vtable_t {
	/** Start message generation by associating corresponding context and generating message start-line if applicable */
	apt_bool_t (*on_start)(apt_message_generator_t *generator, apt_message_context_t *context, apt_text_stream_t *stream);
	/** Header section handler is invoked to notify header section has been generated */
	apt_bool_t (*on_header_complete)(apt_message_generator_t *generator, apt_message_context_t *context, apt_text_stream_t *stream);
	/** Body handler is invoked to notify body has been generated */
	apt_bool_t (*on_body_complete)(apt_message_generator_t *generator, apt_message_context_t *context, apt_text_stream_t *stream);
};


APT_END_EXTERN_C

#endif /* APT_TEXT_MESSAGE_H */
