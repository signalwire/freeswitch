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
 * $Id: mrcp_stream.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mrcp_stream.h"
#include "mrcp_message.h"
#include "mrcp_resource_factory.h"
#include "mrcp_resource.h"
#include "apt_log.h"


/** MRCP parser */
struct mrcp_parser_t {
	apt_message_parser_t          *base;
	const mrcp_resource_factory_t *resource_factory;
	mrcp_resource_t               *resource;
};

/** MRCP generator */
struct mrcp_generator_t {
	apt_message_generator_t       *base;
	const mrcp_resource_factory_t *resource_factory;
};

/** Create message and read start line */
static apt_bool_t mrcp_parser_on_start(apt_message_parser_t *parser, apt_message_context_t *context, apt_text_stream_t *stream, apr_pool_t *pool);
/** Header section handler */
static apt_bool_t mrcp_parser_on_header_complete(apt_message_parser_t *parser, apt_message_context_t *context);

static const apt_message_parser_vtable_t parser_vtable = {
	mrcp_parser_on_start,
	mrcp_parser_on_header_complete,
	NULL
};

/** Start message generation  */
apt_bool_t mrcp_generator_on_start(apt_message_generator_t *generator, apt_message_context_t *context, apt_text_stream_t *stream);
/** Finalize by setting overall message length in start line */
apt_bool_t mrcp_generator_on_header_complete(apt_message_generator_t *generator, apt_message_context_t *context, apt_text_stream_t *stream);

static const apt_message_generator_vtable_t generator_vtable = {
	mrcp_generator_on_start,
	mrcp_generator_on_header_complete,
	NULL
};


/** Create MRCP stream parser */
MRCP_DECLARE(mrcp_parser_t*) mrcp_parser_create(const mrcp_resource_factory_t *resource_factory, apr_pool_t *pool)
{
	mrcp_parser_t *parser = apr_palloc(pool,sizeof(mrcp_parser_t));
	parser->base = apt_message_parser_create(parser,&parser_vtable,pool);
	parser->resource_factory = resource_factory;
	parser->resource = NULL;
	return parser;
}

/** Set resource by name to be used for parsing of MRCPv1 messages */
MRCP_DECLARE(void) mrcp_parser_resource_set(mrcp_parser_t *parser, const apt_str_t *resource_name)
{
	if(resource_name) {
		parser->resource = mrcp_resource_find(parser->resource_factory,resource_name);
	}
}

/** Set verbose mode for the parser */
MRCP_DECLARE(void) mrcp_parser_verbose_set(mrcp_parser_t *parser, apt_bool_t verbose)
{
	apt_message_parser_verbose_set(parser->base,verbose);
}

/** Parse MRCP stream */
MRCP_DECLARE(apt_message_status_e) mrcp_parser_run(mrcp_parser_t *parser, apt_text_stream_t *stream, mrcp_message_t **message)
{
	return apt_message_parser_run(parser->base,stream,(void**)message);
}

/** Create message and read start line */
static apt_bool_t mrcp_parser_on_start(apt_message_parser_t *parser, apt_message_context_t *context, apt_text_stream_t *stream, apr_pool_t *pool)
{
	mrcp_message_t *mrcp_message;
	apt_str_t start_line;
	/* read start line */
	if(apt_text_line_read(stream,&start_line) == FALSE) {
		return FALSE;
	}

	/* create new MRCP message */
	mrcp_message = mrcp_message_create(pool);
	/* parse start-line */
	if(mrcp_start_line_parse(&mrcp_message->start_line,&start_line,mrcp_message->pool) == FALSE) {
		return FALSE;
	}

	if(mrcp_message->start_line.version == MRCP_VERSION_1) {
		mrcp_parser_t *mrcp_parser = apt_message_parser_object_get(parser);
		if(!mrcp_parser->resource) {
			return FALSE;
		}
		apt_string_copy(
			&mrcp_message->channel_id.resource_name,
			&mrcp_parser->resource->name,
			pool);

		if(mrcp_message_resource_set(mrcp_message,mrcp_parser->resource) == FALSE) {
			return FALSE;
		}
	}

	context->message = mrcp_message;
	context->header = &mrcp_message->header.header_section;
	context->body = &mrcp_message->body;
	return TRUE;
}

/** Header section handler */
static apt_bool_t mrcp_parser_on_header_complete(apt_message_parser_t *parser, apt_message_context_t *context)
{
	mrcp_message_t *mrcp_message = context->message;
	if(mrcp_message->start_line.version == MRCP_VERSION_2) {
		mrcp_resource_t *resource;
		mrcp_parser_t *mrcp_parser;
		if(mrcp_channel_id_parse(&mrcp_message->channel_id,&mrcp_message->header,mrcp_message->pool) == FALSE) {
			return FALSE;
		}
		mrcp_parser = apt_message_parser_object_get(parser);
		/* find resource */
		resource = mrcp_resource_find(mrcp_parser->resource_factory,&mrcp_message->channel_id.resource_name);
		if(!resource) {
			return FALSE;
		}

		if(mrcp_message_resource_set(mrcp_message,resource) == FALSE) {
			return FALSE;
		}
	}

	if(mrcp_header_fields_parse(&mrcp_message->header,mrcp_message->pool) == FALSE) {
		return FALSE;
	}

	if(context->body && mrcp_generic_header_property_check(mrcp_message,GENERIC_HEADER_CONTENT_LENGTH) == TRUE) {
		mrcp_generic_header_t *generic_header = mrcp_generic_header_get(mrcp_message);
		if(generic_header && generic_header->content_length) {
			context->body->length = generic_header->content_length;
		}
	}
	return TRUE;
}


/** Create MRCP stream generator */
MRCP_DECLARE(mrcp_generator_t*) mrcp_generator_create(const mrcp_resource_factory_t *resource_factory, apr_pool_t *pool)
{
	mrcp_generator_t *generator = apr_palloc(pool,sizeof(mrcp_generator_t));
	generator->base = apt_message_generator_create(generator,&generator_vtable,pool);
	generator->resource_factory = resource_factory;
	return generator;
}

/** Set verbose mode for the generator */
MRCP_DECLARE(void) mrcp_generator_verbose_set(mrcp_generator_t *generator, apt_bool_t verbose)
{
	apt_message_generator_verbose_set(generator->base,verbose);
}

/** Generate MRCP stream */
MRCP_DECLARE(apt_message_status_e) mrcp_generator_run(mrcp_generator_t *generator, mrcp_message_t *message, apt_text_stream_t *stream)
{
	return apt_message_generator_run(generator->base,message,stream);
}

/** Initialize by generating message start line and return header section and body */
apt_bool_t mrcp_generator_on_start(apt_message_generator_t *generator, apt_message_context_t *context, apt_text_stream_t *stream)
{
	mrcp_message_t *mrcp_message = context->message;
	/* validate message */
	if(mrcp_message_validate(mrcp_message) == FALSE) {
		return FALSE;
	}
	/* generate start-line */
	if(mrcp_start_line_generate(&mrcp_message->start_line,stream) == FALSE) {
		return FALSE;
	}
		
	if(mrcp_message->start_line.version == MRCP_VERSION_2) {
		mrcp_channel_id_generate(&mrcp_message->channel_id,stream);
	}

	context->header = &mrcp_message->header.header_section;
	context->body = &mrcp_message->body;
	return TRUE;
}

/** Finalize by setting overall message length in start line */
apt_bool_t mrcp_generator_on_header_complete(apt_message_generator_t *generator, apt_message_context_t *context, apt_text_stream_t *stream)
{
	mrcp_message_t *mrcp_message = context->message;
	/* finalize start-line generation */
	return mrcp_start_line_finalize(&mrcp_message->start_line,mrcp_message->body.length,stream);
}

/** Generate MRCP message (excluding message body) */
MRCP_DECLARE(apt_bool_t) mrcp_message_generate(const mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *stream)
{
	/* validate message */
	if(mrcp_message_validate(message) == FALSE) {
		return FALSE;
	}
	
	/* generate start-line */
	if(mrcp_start_line_generate(&message->start_line,stream) == FALSE) {
		return FALSE;
	}

	if(message->start_line.version == MRCP_VERSION_2) {
		mrcp_channel_id_generate(&message->channel_id,stream);
	}

	/* generate header section */
	if(apt_header_section_generate(&message->header.header_section,stream) == FALSE) {
		return FALSE;
	}

	/* finalize start-line generation */
	if(mrcp_start_line_finalize(&message->start_line,message->body.length,stream) == FALSE) {
		return FALSE;
	}

	return TRUE;
}
