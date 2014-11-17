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
 * $Id: rtsp_stream.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "rtsp_stream.h"
#include "apt_log.h"

/** RTSP parser */
struct rtsp_parser_t {
	apt_message_parser_t    *base;
};

/** RTSP generator */
struct rtsp_generator_t {
	apt_message_generator_t *base;
};

/** Create message and read start line */
static apt_bool_t rtsp_parser_on_start(apt_message_parser_t *parser, apt_message_context_t *context, apt_text_stream_t *stream, apr_pool_t *pool);
/** Header section handler */
static apt_bool_t rtsp_parser_on_header_complete(apt_message_parser_t *parser, apt_message_context_t *context);

static const apt_message_parser_vtable_t parser_vtable = {
	rtsp_parser_on_start,
	rtsp_parser_on_header_complete,
	NULL
};


/** Initialize by generating message start line and return header section and body */
apt_bool_t rtsp_generator_on_start(apt_message_generator_t *generator, apt_message_context_t *context, apt_text_stream_t *stream);

static const apt_message_generator_vtable_t generator_vtable = {
	rtsp_generator_on_start,
	NULL,
	NULL
};


/** Create RTSP parser */
RTSP_DECLARE(rtsp_parser_t*) rtsp_parser_create(apr_pool_t *pool)
{
	rtsp_parser_t *parser = apr_palloc(pool,sizeof(rtsp_parser_t));
	parser->base = apt_message_parser_create(parser,&parser_vtable,pool);
	return parser;
}

/** Parse RTSP stream */
RTSP_DECLARE(apt_message_status_e) rtsp_parser_run(rtsp_parser_t *parser, apt_text_stream_t *stream, rtsp_message_t **message)
{
	return apt_message_parser_run(parser->base,stream,(void**)message);
}

/** Create message and read start line */
static apt_bool_t rtsp_parser_on_start(apt_message_parser_t *parser, apt_message_context_t *context, apt_text_stream_t *stream, apr_pool_t *pool)
{
	rtsp_message_t *message;
	apt_str_t start_line;
	/* read start line */
	if(apt_text_line_read(stream,&start_line) == FALSE) {
		return FALSE;
	}
	
	message = rtsp_message_create(RTSP_MESSAGE_TYPE_UNKNOWN,pool);
	if(rtsp_start_line_parse(&message->start_line,&start_line,message->pool) == FALSE) {
		return FALSE;
	}
	
	context->message = message;
	context->header = &message->header.header_section;
	context->body = &message->body;
	return TRUE;
}

/** Header section handler */
static apt_bool_t rtsp_parser_on_header_complete(apt_message_parser_t *parser, apt_message_context_t *context)
{
	rtsp_message_t *rtsp_message = context->message;
	rtsp_header_fields_parse(&rtsp_message->header,rtsp_message->pool);

	if(context->body && rtsp_header_property_check(&rtsp_message->header,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE) {
		context->body->length = rtsp_message->header.content_length;
	}

	return TRUE;
}

/** Create RTSP stream generator */
RTSP_DECLARE(rtsp_generator_t*) rtsp_generator_create(apr_pool_t *pool)
{
	rtsp_generator_t *generator = apr_palloc(pool,sizeof(rtsp_generator_t));
	generator->base = apt_message_generator_create(generator,&generator_vtable,pool);
	return generator;
}


/** Generate RTSP stream */
RTSP_DECLARE(apt_message_status_e) rtsp_generator_run(rtsp_generator_t *generator, rtsp_message_t *message, apt_text_stream_t *stream)
{
	return apt_message_generator_run(generator->base,message,stream);
}

/** Initialize by generating message start line and return header section and body */
apt_bool_t rtsp_generator_on_start(apt_message_generator_t *generator, apt_message_context_t *context, apt_text_stream_t *stream)
{
	rtsp_message_t *rtsp_message = context->message;
	context->header = &rtsp_message->header.header_section;
	context->body = &rtsp_message->body;
	return rtsp_start_line_generate(&rtsp_message->start_line,stream);
}
