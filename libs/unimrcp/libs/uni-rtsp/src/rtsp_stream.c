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

/** Stage of RTSP stream processing (parse/generate) */
typedef enum {
	RTSP_STREAM_STAGE_NONE,
	RTSP_STREAM_STAGE_START_LINE,
	RTSP_STREAM_STAGE_HEADER,
	RTSP_STREAM_STAGE_BODY
} rtsp_stream_stage_e;

/** RTSP parser */
struct rtsp_parser_t {
	rtsp_stream_stage_e  stage;
	apt_bool_t           skip_lf;
	rtsp_message_t      *message;
	apr_pool_t          *pool;
};

/** RTSP generator */
struct rtsp_generator_t {
	rtsp_stream_status_e stage;
	rtsp_message_t      *message;
	apr_pool_t          *pool;
};

/** Read RTSP message-body */
static apt_bool_t rtsp_message_body_read(rtsp_message_t *message, apt_text_stream_t *stream)
{
	apt_bool_t status = TRUE;
	if(message->body.buf) {
		/* stream length available to read */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to read */
		apr_size_t required_length = message->header.content_length - message->body.length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* not complete */
			status = FALSE;
		}
		memcpy(message->body.buf+message->body.length,stream->pos,required_length);
		message->body.length += required_length;
		stream->pos += required_length;
		message->body.buf[message->body.length] = '\0';
	}

	return status;
}

/** Write RTSP message-body */
static apt_bool_t rtsp_message_body_write(rtsp_message_t *message, apt_text_stream_t *stream)
{
	apt_bool_t status = TRUE;
	if(message->body.length < message->header.content_length) {
		/* stream length available to write */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to write */
		apr_size_t required_length = message->header.content_length - message->body.length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* not complete */
			status = FALSE;
		}

		memcpy(stream->pos,message->body.buf+message->body.length,required_length);
		message->body.length += required_length;
		stream->pos += required_length;
	}

	return status;
}

/** Create RTSP parser */
RTSP_DECLARE(rtsp_parser_t*) rtsp_parser_create(apr_pool_t *pool)
{
	rtsp_parser_t *parser = apr_palloc(pool,sizeof(rtsp_parser_t));
	parser->stage = RTSP_STREAM_STAGE_NONE;
	parser->skip_lf = FALSE;
	parser->message = NULL;
	parser->pool = pool;
	return parser;
}

static rtsp_stream_status_e rtsp_parser_break(rtsp_parser_t *parser, apt_text_stream_t *stream)
{
	/* failed to parse message */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached */
		return RTSP_STREAM_STATUS_INCOMPLETE;
	}

	/* error case */
	parser->stage = RTSP_STREAM_STAGE_NONE;
	return RTSP_STREAM_STATUS_INVALID;
}

/** Parse RTSP stream */
RTSP_DECLARE(rtsp_stream_status_e) rtsp_parser_run(rtsp_parser_t *parser, apt_text_stream_t *stream)
{
	rtsp_message_t *message = parser->message;
	if(parser->stage == RTSP_STREAM_STAGE_NONE || !message) {
		/* create new RTSP message */
		message = rtsp_message_create(RTSP_MESSAGE_TYPE_UNKNOWN,parser->pool);
		parser->message = message;
		parser->stage = RTSP_STREAM_STAGE_START_LINE;
	}

	if(parser->stage == RTSP_STREAM_STAGE_START_LINE) {
		/* parse start-line */
		if(rtsp_start_line_parse(&message->start_line,stream,message->pool) == FALSE) {
			return rtsp_parser_break(parser,stream);
		}
		parser->stage = RTSP_STREAM_STAGE_HEADER;
	}

	if(parser->stage == RTSP_STREAM_STAGE_HEADER) {
		/* parse header */
		if(rtsp_header_parse(&message->header,stream,message->pool) == FALSE) {
			return rtsp_parser_break(parser,stream);
		}

		parser->stage = RTSP_STREAM_STAGE_NONE;
		if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE) {
			if(message->header.content_length) {
				apt_str_t *body = &message->body;
				body->buf = apr_palloc(message->pool,message->header.content_length+1);
				body->length = 0;
				parser->stage = RTSP_STREAM_STAGE_BODY;
			}
		}
	}

	if(parser->stage == RTSP_STREAM_STAGE_BODY) {
		if(rtsp_message_body_read(message,stream) == FALSE) {
			return rtsp_parser_break(parser,stream);
		}
		parser->stage = RTSP_STREAM_STAGE_NONE;
	}

	/* in the worst case message segmentation may occur between <CR> and <LF> 
	   of the final empty header */
	if(!message->body.length && *(stream->pos-1)== APT_TOKEN_CR) {
		/* if this is the case be prepared to skip <LF> */
		parser->skip_lf = TRUE;
	}

	return RTSP_STREAM_STATUS_COMPLETE;
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
	generator->stage = RTSP_STREAM_STAGE_NONE;
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

static rtsp_stream_status_e rtsp_generator_break(rtsp_generator_t *generator, apt_text_stream_t *stream)
{
	/* failed to generate message */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached */
		return RTSP_STREAM_STATUS_INCOMPLETE;
	}

	/* error case */
	generator->stage = RTSP_STREAM_STAGE_NONE;
	return RTSP_STREAM_STATUS_INVALID;
}

/** Generate RTSP stream */
RTSP_DECLARE(rtsp_stream_status_e) rtsp_generator_run(rtsp_generator_t *generator, apt_text_stream_t *stream)
{
	rtsp_message_t *message = generator->message;
	if(!message) {
		return RTSP_STREAM_STATUS_INVALID;
	}

	if(generator->stage == RTSP_STREAM_STAGE_NONE) {
		/* generate start-line */
		if(rtsp_start_line_generate(&message->start_line,stream) == FALSE) {
			return rtsp_generator_break(generator,stream);
		}

		/* generate header */
		if(rtsp_header_generate(&message->header,stream) == FALSE) {
			return rtsp_generator_break(generator,stream);
		}

		generator->stage = RTSP_STREAM_STAGE_NONE;
		if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE) {
			if(message->header.content_length) {
				apt_str_t *body = &message->body;
				body->length = 0;
				generator->stage = RTSP_STREAM_STAGE_BODY;
			}
		}
	}

	if(generator->stage == RTSP_STREAM_STAGE_BODY) {
		if(rtsp_message_body_write(message,stream) == FALSE) {
			return rtsp_generator_break(generator,stream);
		}
		
		generator->stage = RTSP_STREAM_STAGE_NONE;
	}

	return RTSP_STREAM_STATUS_COMPLETE;
}


/** Walk through RTSP stream and invoke message handler for each parsed message */
RTSP_DECLARE(apt_bool_t) rtsp_stream_walk(rtsp_parser_t *parser, apt_text_stream_t *stream, rtsp_message_handler_f handler, void *obj)
{
	rtsp_stream_status_e status;
	if(parser->skip_lf == TRUE) {
		/* skip <LF> occurred as a result of message segmentation between <CR> and <LF> */
		apt_text_char_skip(stream,APT_TOKEN_LF);
		parser->skip_lf = FALSE;
	}
	do {
		status = rtsp_parser_run(parser,stream);
		if(status == RTSP_STREAM_STATUS_COMPLETE) {
			/* message is completely parsed */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Parsed RTSP Message [%lu]", stream->pos - stream->text.buf);
			/* connection has already been destroyed, if handler return FALSE  */
			if(handler(obj,parser->message,status) == FALSE) {
				return TRUE;
			}
		}
		else if(status == RTSP_STREAM_STATUS_INCOMPLETE) {
			/* message is partially parsed, to be continued */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Truncated RTSP Message [%lu]", stream->pos - stream->text.buf);
			/* prepare stream for further processing */
			if(apt_text_stream_scroll(stream) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Scroll RTSP Stream", stream->text.buf);
			}
			return TRUE;
		}
		else if(status == RTSP_STREAM_STATUS_INVALID){
			/* error case */
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse RTSP Message");
			/* invoke message handler */
			if(handler(obj,parser->message,status) == TRUE) {
				/* reset stream pos */
				stream->pos = stream->text.buf;
			}
			return FALSE;
		}
	}
	while(apt_text_is_eos(stream) == FALSE);

	/* reset stream pos */
	apt_text_stream_reset(stream);
	return TRUE;
}
