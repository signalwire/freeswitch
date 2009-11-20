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

#include "mrcp_stream.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_resource_factory.h"
#include "apt_log.h"

/** MRCP parser */
struct mrcp_parser_t {
	mrcp_resource_factory_t *resource_factory;
	apt_str_t                resource_name;
	mrcp_stream_result_e     result;
	char                    *pos;
	apt_bool_t               skip_lf;
	mrcp_message_t          *message;
	apr_pool_t              *pool;
};

/** MRCP generator */
struct mrcp_generator_t {
	mrcp_resource_factory_t *resource_factory;
	mrcp_stream_result_e     result;
	char                    *pos;
	mrcp_message_t          *message;
	apr_pool_t              *pool;
};


/** Read MRCP message-body */
static mrcp_stream_result_e mrcp_message_body_read(mrcp_message_t *message, apt_text_stream_t *stream)
{
	mrcp_stream_result_e result = MRCP_STREAM_MESSAGE_COMPLETE;
	if(message->body.buf) {
		mrcp_generic_header_t *generic_header = mrcp_generic_header_get(message);
		/* stream length available to read */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to read */
		apr_size_t required_length = generic_header->content_length - message->body.length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* not complete */
			result = MRCP_STREAM_MESSAGE_TRUNCATED;
		}
		memcpy(message->body.buf+message->body.length,stream->pos,required_length);
		message->body.length += required_length;
		stream->pos += required_length;
		message->body.buf[message->body.length] = '\0';
	}

	return result;
}

/** Parse MRCP message-body */
static mrcp_stream_result_e mrcp_message_body_parse(mrcp_message_t *message, apt_text_stream_t *stream, apr_pool_t *pool)
{
	if(mrcp_generic_header_property_check(message,GENERIC_HEADER_CONTENT_LENGTH) == TRUE) {
		mrcp_generic_header_t *generic_header = mrcp_generic_header_get(message);
		if(generic_header && generic_header->content_length) {
			apt_str_t *body = &message->body;
			body->buf = apr_palloc(pool,generic_header->content_length+1);
			body->length = 0;
			return mrcp_message_body_read(message,stream);
		}
	}
	return MRCP_STREAM_MESSAGE_COMPLETE;
}

/** Write MRCP message-body */
static mrcp_stream_result_e mrcp_message_body_write(mrcp_message_t *message, apt_text_stream_t *stream)
{
	mrcp_stream_result_e result = MRCP_STREAM_MESSAGE_COMPLETE;
	mrcp_generic_header_t *generic_header = mrcp_generic_header_get(message);
	if(generic_header && message->body.length < generic_header->content_length) {
		/* stream length available to write */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to write */
		apr_size_t required_length = generic_header->content_length - message->body.length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* not complete */
			result = MRCP_STREAM_MESSAGE_TRUNCATED;
		}

		memcpy(stream->pos,message->body.buf+message->body.length,required_length);
		message->body.length += required_length;
		stream->pos += required_length;
	}

	return result;
}

/** Generate MRCP message-body */
static mrcp_stream_result_e mrcp_message_body_generate(mrcp_message_t *message, apt_text_stream_t *stream)
{
	if(mrcp_generic_header_property_check(message,GENERIC_HEADER_CONTENT_LENGTH) == TRUE) {
		mrcp_generic_header_t *generic_header = mrcp_generic_header_get(message);
		if(generic_header && generic_header->content_length) {
			apt_str_t *body = &message->body;
			body->length = 0;
			return mrcp_message_body_write(message,stream);
		}
	}
	return MRCP_STREAM_MESSAGE_COMPLETE;
}

/** Parse MRCP message (excluding message body) */
MRCP_DECLARE(apt_bool_t) mrcp_message_parse(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *stream)
{
	mrcp_resource_t *resource;

	/* parse start-line */
	if(mrcp_start_line_parse(&message->start_line,stream,message->pool) == FALSE) {
		return FALSE;
	}

	if(message->start_line.version == MRCP_VERSION_2) {
		mrcp_channel_id_parse(&message->channel_id,stream,message->pool);
	}

	/* find resource */
	resource = mrcp_resource_find(resource_factory,&message->channel_id.resource_name);
	if(!resource) {
		return FALSE;
	}

	if(mrcp_message_resource_set(message,resource) == FALSE) {
		return FALSE;
	}

	/* parse header */
	if(mrcp_message_header_parse(&message->header,stream,message->pool) == FALSE) {
		return FALSE;
	}

	return TRUE;
}

/** Generate MRCP message (excluding message body) */
MRCP_DECLARE(apt_bool_t) mrcp_message_generate(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *stream)
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

	/* generate header */
	if(mrcp_message_header_generate(&message->header,stream) == FALSE) {
		return FALSE;
	}

	/* finalize start-line generation */
	if(mrcp_start_line_finalize(&message->start_line,message->body.length,stream) == FALSE) {
		return FALSE;
	}

	return TRUE;
}


/** Create MRCP stream parser */
MRCP_DECLARE(mrcp_parser_t*) mrcp_parser_create(mrcp_resource_factory_t *resource_factory, apr_pool_t *pool)
{
	mrcp_parser_t *parser = apr_palloc(pool,sizeof(mrcp_parser_t));
	parser->resource_factory = resource_factory;
	apt_string_reset(&parser->resource_name);
	parser->result = MRCP_STREAM_MESSAGE_INVALID;
	parser->pos = NULL;
	parser->skip_lf = FALSE;
	parser->message = NULL;
	parser->pool = pool;
	return parser;
}

/** Set resource name to be used while parsing (MRCPv1 only) */
MRCP_DECLARE(void) mrcp_parser_resource_name_set(mrcp_parser_t *parser, const apt_str_t *resource_name)
{
	if(resource_name) {
		apt_string_copy(&parser->resource_name,resource_name,parser->pool);
	}
}

static mrcp_stream_result_e mrcp_parser_break(mrcp_parser_t *parser, apt_text_stream_t *stream)
{
	/* failed to parse either start-line or header */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached, rewind/restore stream */
		stream->pos = parser->pos;
		parser->result = MRCP_STREAM_MESSAGE_TRUNCATED;
		parser->message = NULL;
	}
	else {
		/* error case */
		parser->result = MRCP_STREAM_MESSAGE_INVALID;
	}
	return parser->result;
}

/** Parse MRCP stream */
MRCP_DECLARE(mrcp_stream_result_e) mrcp_parser_run(mrcp_parser_t *parser, apt_text_stream_t *stream)
{
	mrcp_message_t *message = parser->message;
	if(message && parser->result == MRCP_STREAM_MESSAGE_TRUNCATED) {
		/* process continuation data */
		parser->result = mrcp_message_body_read(message,stream);
		return parser->result;
	}
	
	/* create new MRCP message */
	message = mrcp_message_create(parser->pool);
	message->channel_id.resource_name = parser->resource_name;
	parser->message = message;
	/* store current position to be able to rewind/restore stream if needed */
	parser->pos = stream->pos;

	/* parse start-line and header */
	if(mrcp_message_parse(parser->resource_factory,message,stream) == FALSE) {
		return mrcp_parser_break(parser,stream);
	}

	/* parse body */
	parser->result = mrcp_message_body_parse(message,stream,message->pool);

	/* in the worst case message segmentation may occur between <CR> and <LF> 
	   of the final empty header */
	if(!message->body.length && *(stream->pos-1)== APT_TOKEN_CR) {
		/* if this is the case be prepared to skip <LF> */
		parser->skip_lf = TRUE;
	}
	return parser->result;
}

/** Get parsed MRCP message */
MRCP_DECLARE(mrcp_message_t*) mrcp_parser_message_get(const mrcp_parser_t *parser)
{
	return parser->message;
}


/** Create MRCP stream generator */
MRCP_DECLARE(mrcp_generator_t*) mrcp_generator_create(mrcp_resource_factory_t *resource_factory, apr_pool_t *pool)
{
	mrcp_generator_t *generator = apr_palloc(pool,sizeof(mrcp_generator_t));
	generator->resource_factory = resource_factory;
	generator->result = MRCP_STREAM_MESSAGE_INVALID;
	generator->pos = NULL;
	generator->message = NULL;
	generator->pool = pool;
	return generator;
}

/** Set MRCP message to generate */
MRCP_DECLARE(apt_bool_t) mrcp_generator_message_set(mrcp_generator_t *generator, mrcp_message_t *message)
{
	if(!message) {
		return FALSE;
	}
	generator->message = message;
	return TRUE;
}

static mrcp_stream_result_e mrcp_generator_break(mrcp_generator_t *generator, apt_text_stream_t *stream)
{
	/* failed to generate either start-line or header */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached, rewind/restore stream */
		stream->pos = generator->pos;
		generator->result = MRCP_STREAM_MESSAGE_TRUNCATED;
	}
	else {
		/* error case */
		generator->result = MRCP_STREAM_MESSAGE_INVALID;
	}
	return generator->result;
}

/** Generate MRCP stream */
MRCP_DECLARE(mrcp_stream_result_e) mrcp_generator_run(mrcp_generator_t *generator, apt_text_stream_t *stream)
{
	mrcp_message_t *message = generator->message;
	if(!message) {
		return MRCP_STREAM_MESSAGE_INVALID;
	}

	if(message && generator->result == MRCP_STREAM_MESSAGE_TRUNCATED) {
		/* process continuation data */
		generator->result = mrcp_message_body_write(message,stream);
		return generator->result;
	}

	/* generate start-line and header */
	if(mrcp_message_generate(generator->resource_factory,message,stream) == FALSE) {
		return mrcp_generator_break(generator,stream);
	}

	/* generate body */
	generator->result = mrcp_message_body_generate(message,stream);
	return generator->result;
}


/** Walk through MRCP stream and invoke message handler for each parsed message */
MRCP_DECLARE(apt_bool_t) mrcp_stream_walk(mrcp_parser_t *parser, apt_text_stream_t *stream, mrcp_message_handler_f handler, void *obj)
{
	mrcp_stream_result_e result;
	if(parser->skip_lf == TRUE) {
		/* skip <LF> occurred as a result of message segmentation between <CR> and <LF> */
		apt_text_char_skip(stream,APT_TOKEN_LF);
		parser->skip_lf = FALSE;
	}
	do {
		result = mrcp_parser_run(parser,stream);
		if(result == MRCP_STREAM_MESSAGE_COMPLETE) {
			/* message is completely parsed */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Parsed MRCP Message [%lu]", stream->pos - stream->text.buf);
			/* invoke message handler */
			handler(obj,parser->message,result);
		}
		else if(result == MRCP_STREAM_MESSAGE_TRUNCATED) {
			/* message is partially parsed, to be continued */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Truncated MRCP Message [%lu]", stream->pos - stream->text.buf);
			/* prepare stream for further processing */
			if(apt_text_stream_scroll(stream) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Scroll MRCP Stream", stream->text.buf);
			}
			return TRUE;
		}
		else if(result == MRCP_STREAM_MESSAGE_INVALID){
			/* error case */
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse MRCP Message");
			/* invoke message handler */
			handler(obj,parser->message,result);
			/* reset stream pos */
			stream->pos = stream->text.buf;
			return FALSE;
		}
	}
	while(apt_text_is_eos(stream) == FALSE);

	/* reset stream pos */
	stream->pos = stream->text.buf;
	return TRUE;
}
