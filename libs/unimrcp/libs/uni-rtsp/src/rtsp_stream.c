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

#include "rtsp_stream.h"
#include "apt_log.h"

/** RTSP parser */
struct rtsp_parser_t {
	rtsp_stream_result_e result;
	char                *pos;
	apt_bool_t           skip_lf;
	rtsp_message_t      *message;
	apr_pool_t          *pool;
};

/** RTSP generator */
struct rtsp_generator_t {
	rtsp_stream_result_e result;
	char                *pos;
	rtsp_message_t      *message;
	apr_pool_t          *pool;
};

/** Read RTSP message-body */
static rtsp_stream_result_e rtsp_message_body_read(rtsp_message_t *message, apt_text_stream_t *stream)
{
	rtsp_stream_result_e result = RTSP_STREAM_MESSAGE_COMPLETE;
	if(message->body.buf) {
		/* stream length available to read */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to read */
		apr_size_t required_length = message->header.content_length - message->body.length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* not complete */
			result = RTSP_STREAM_MESSAGE_TRUNCATED;
		}
		memcpy(message->body.buf+message->body.length,stream->pos,required_length);
		message->body.length += required_length;
		stream->pos += required_length;
		message->body.buf[message->body.length] = '\0';
	}

	return result;
}

/** Parse RTSP message-body */
static rtsp_stream_result_e rtsp_message_body_parse(rtsp_message_t *message, apt_text_stream_t *stream, apr_pool_t *pool)
{
	if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE) {
		if(message->header.content_length) {
			apt_str_t *body = &message->body;
			body->buf = apr_palloc(pool,message->header.content_length+1);
			body->length = 0;
			return rtsp_message_body_read(message,stream);
		}
	}
	return RTSP_STREAM_MESSAGE_COMPLETE;
}

/** Write RTSP message-body */
static rtsp_stream_result_e rtsp_message_body_write(rtsp_message_t *message, apt_text_stream_t *stream)
{
	rtsp_stream_result_e result = RTSP_STREAM_MESSAGE_COMPLETE;
	if(message->body.length < message->header.content_length) {
		/* stream length available to write */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to write */
		apr_size_t required_length = message->header.content_length - message->body.length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* not complete */
			result = RTSP_STREAM_MESSAGE_TRUNCATED;
		}

		memcpy(stream->pos,message->body.buf+message->body.length,required_length);
		message->body.length += required_length;
		stream->pos += required_length;
	}

	return result;
}

/** Generate RTSP message-body */
static rtsp_stream_result_e rtsp_message_body_generate(rtsp_message_t *message, apt_text_stream_t *stream, apr_pool_t *pool)
{
	if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE) {
		if(message->header.content_length) {
			apt_str_t *body = &message->body;
			body->length = 0;
			return rtsp_message_body_write(message,stream);
		}
	}
	return RTSP_STREAM_MESSAGE_COMPLETE;
}

/** Create RTSP parser */
RTSP_DECLARE(rtsp_parser_t*) rtsp_parser_create(apr_pool_t *pool)
{
	rtsp_parser_t *parser = apr_palloc(pool,sizeof(rtsp_parser_t));
	parser->result = RTSP_STREAM_MESSAGE_INVALID;
	parser->pos = NULL;
	parser->skip_lf = FALSE;
	parser->message = NULL;
	parser->pool = pool;
	return parser;
}

static rtsp_stream_result_e rtsp_parser_break(rtsp_parser_t *parser, apt_text_stream_t *stream)
{
	/* failed to parse either start-line or header */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached, rewind/restore stream */
		stream->pos = parser->pos;
		parser->result = RTSP_STREAM_MESSAGE_TRUNCATED;
		parser->message = NULL;
	}
	else {
		/* error case */
		parser->result = RTSP_STREAM_MESSAGE_INVALID;
	}
	return parser->result;
}

/** Parse RTSP stream */
RTSP_DECLARE(rtsp_stream_result_e) rtsp_parser_run(rtsp_parser_t *parser, apt_text_stream_t *stream)
{
	rtsp_message_t *message = parser->message;
	if(message && parser->result == RTSP_STREAM_MESSAGE_TRUNCATED) {
		/* process continuation data */
		parser->result = rtsp_message_body_read(message,stream);
		return parser->result;
	}

	/* create new RTSP message */
	message = rtsp_message_create(RTSP_MESSAGE_TYPE_UNKNOWN,parser->pool);
	parser->message = message;
	/* store current position to be able to rewind/restore stream if needed */
	parser->pos = stream->pos;
	/* parse start-line */
	if(rtsp_start_line_parse(&message->start_line,stream,message->pool) == FALSE) {
		return rtsp_parser_break(parser,stream);
	}
	/* parse header */
	if(rtsp_header_parse(&message->header,stream,message->pool) == FALSE) {
		return rtsp_parser_break(parser,stream);
	}
	/* parse body */
	parser->result = rtsp_message_body_parse(message,stream,message->pool);
	
	/* in the worst case message segmentation may occur between <CR> and <LF> 
	   of the final empty header */
	if(!message->body.length && *(stream->pos-1)== APT_TOKEN_CR) {
		/* if this is the case be prepared to skip <LF> */
		parser->skip_lf = TRUE;
	}
	return parser->result;
}

/** Get parsed RTSP message */
RTSP_DECLARE(rtsp_message_t*) rtsp_parser_message_get(const rtsp_parser_t *parser)
{
	return parser->message;
}


/** Create RTSP stream generator */
RTSP_DECLARE(rtsp_generator_t*) rtsp_generator_create(apr_pool_t *pool)
{
	rtsp_generator_t *generator = apr_palloc(pool,sizeof(rtsp_generator_t));
	generator->result = RTSP_STREAM_MESSAGE_INVALID;
	generator->pos = NULL;
	generator->message = NULL;
	generator->pool = pool;
	return generator;
}

/** Set RTSP message to generate */
RTSP_DECLARE(apt_bool_t) rtsp_generator_message_set(rtsp_generator_t *generator, rtsp_message_t *message)
{
	if(!message) {
		return FALSE;
	}
	generator->message = message;
	return TRUE;
}

static rtsp_stream_result_e rtsp_generator_break(rtsp_generator_t *generator, apt_text_stream_t *stream)
{
	/* failed to generate either start-line or header */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached, rewind/restore stream */
		stream->pos = generator->pos;
		generator->result = RTSP_STREAM_MESSAGE_TRUNCATED;
	}
	else {
		/* error case */
		generator->result = RTSP_STREAM_MESSAGE_INVALID;
	}
	return generator->result;
}

/** Generate RTSP stream */
RTSP_DECLARE(rtsp_stream_result_e) rtsp_generator_run(rtsp_generator_t *generator, apt_text_stream_t *stream)
{
	rtsp_message_t *message = generator->message;
	if(!message) {
		return RTSP_STREAM_MESSAGE_INVALID;
	}

	if(message && generator->result == RTSP_STREAM_MESSAGE_TRUNCATED) {
		/* process continuation data */
		generator->result = rtsp_message_body_write(message,stream);
		return generator->result;
	}

	/* generate start-line */
	if(rtsp_start_line_generate(&message->start_line,stream) == FALSE) {
		return rtsp_generator_break(generator,stream);
	}

	/* generate header */
	if(rtsp_header_generate(&message->header,stream) == FALSE) {
		return rtsp_generator_break(generator,stream);
	}

	/* generate body */
	generator->result = rtsp_message_body_generate(message,stream,message->pool);
	return generator->result;
}


/** Walk through RTSP stream and invoke message handler for each parsed message */
RTSP_DECLARE(apt_bool_t) rtsp_stream_walk(rtsp_parser_t *parser, apt_text_stream_t *stream, rtsp_message_handler_f handler, void *obj)
{
	rtsp_stream_result_e result;
	if(parser->skip_lf == TRUE) {
		/* skip <LF> occurred as a result of message segmentation between <CR> and <LF> */
		apt_text_char_skip(stream,APT_TOKEN_LF);
		parser->skip_lf = FALSE;
	}
	do {
		result = rtsp_parser_run(parser,stream);
		if(result == RTSP_STREAM_MESSAGE_COMPLETE) {
			/* message is completely parsed */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Parsed RTSP Message [%lu]", stream->pos - stream->text.buf);
			/* invoke message handler */
			handler(obj,parser->message,result);
		}
		else if(result == RTSP_STREAM_MESSAGE_TRUNCATED) {
			/* message is partially parsed, to be continued */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Truncated RTSP Message [%lu]", stream->pos - stream->text.buf);
			/* prepare stream for further processing */
			if(apt_text_stream_scroll(stream) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Scroll RTSP Stream", stream->text.buf);
			}
			return TRUE;
		}
		else if(result == RTSP_STREAM_MESSAGE_INVALID){
			/* error case */
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse RTSP Message");
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
