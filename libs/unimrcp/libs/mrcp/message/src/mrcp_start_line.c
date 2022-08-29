/*
 * Copyright 2008-2015 Arsen Chaloyan
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

#include <stdlib.h>
#include <stdio.h>
#include "mrcp_start_line.h"
#include "apt_string_table.h"
#include "apt_log.h"

/** Protocol name used in version string */
#define MRCP_NAME               "MRCP"
#define MRCP_NAME_LENGTH        (sizeof(MRCP_NAME)-1)

/** Separators used in MRCP version string parse/generate */
#define MRCP_NAME_VERSION_SEPARATOR        '/'
#define MRCP_VERSION_MAJOR_MINOR_SEPARATOR '.'

/** Max number of digits message length consists of */
#define MAX_DIGIT_COUNT 6


/** String table of MRCP request-states (mrcp_request_state_t) */
static const apt_str_table_item_t mrcp_request_state_string_table[] = {
	{{"COMPLETE",    8},0},
	{{"IN-PROGRESS",11},0},
	{{"PENDING",     7},0}
};


/** Parse MRCP version */
static mrcp_version_e mrcp_version_parse(const apt_str_t *field)
{
	mrcp_version_e version = MRCP_VERSION_UNKNOWN;
	const char *pos;
	if(field->length <= MRCP_NAME_LENGTH || strncasecmp(field->buf,MRCP_NAME,MRCP_NAME_LENGTH) != 0) {
		/* unexpected protocol name */
		return version;
	}

	pos = field->buf + MRCP_NAME_LENGTH;
	if(*pos == MRCP_NAME_VERSION_SEPARATOR) {
		pos++;
		switch(*pos) {
			case '1': version = MRCP_VERSION_1; break;
			case '2': version = MRCP_VERSION_2; break;
			default: ;
		}
	}
	return version;
}

/** Generate MRCP version */
static apt_bool_t mrcp_version_generate(mrcp_version_e version, apt_text_stream_t *stream)
{
	if(stream->pos + MRCP_NAME_LENGTH + 1 >= stream->end) {
		return FALSE;
	}
	memcpy(stream->pos,MRCP_NAME,MRCP_NAME_LENGTH);
	stream->pos += MRCP_NAME_LENGTH;
	*stream->pos++ = MRCP_NAME_VERSION_SEPARATOR;

	if(apt_text_size_value_insert(stream,version) == FALSE) {
		return FALSE;
	}

	if(stream->pos + 2 >= stream->end) {
		return FALSE;
	}
	*stream->pos++ = MRCP_VERSION_MAJOR_MINOR_SEPARATOR;
	*stream->pos++ = '0';
	return TRUE;
}

/** Parse MRCP request-state used in MRCP response and event */
static APR_INLINE mrcp_request_state_e mrcp_request_state_parse(const apt_str_t *request_state_str)
{
	return apt_string_table_id_find(mrcp_request_state_string_table,MRCP_REQUEST_STATE_COUNT,request_state_str);
}

/** Generate MRCP request-state used in MRCP response and event */
static apt_bool_t mrcp_request_state_generate(mrcp_request_state_e request_state, apt_text_stream_t *stream)
{
	const apt_str_t *name;
	name = apt_string_table_str_get(mrcp_request_state_string_table,MRCP_REQUEST_STATE_COUNT,request_state);
	if(request_state < MRCP_REQUEST_STATE_COUNT) {
		memcpy(stream->pos,name->buf,name->length);
		stream->pos += name->length;
	}
	return TRUE;
}


/** Parse MRCP status-code */
static APR_INLINE mrcp_status_code_e mrcp_status_code_parse(const apt_str_t *field)
{
	return apt_size_value_parse(field);
}

/** Generate MRCP status-code */
static APR_INLINE apr_size_t  mrcp_status_code_generate(mrcp_status_code_e status_code, apt_text_stream_t *stream)
{
	return apt_text_size_value_insert(stream,status_code);
}


/** Parse MRCP request-line */
static apt_bool_t mrcp_request_line_parse(mrcp_start_line_t *start_line, apt_text_stream_t *stream)
{
	apt_str_t field;
	if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse request-id in request-line");
		return FALSE;
	}
	start_line->request_id = mrcp_request_id_parse(&field);

	if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse mrcp-version in request-line");
		return FALSE;
	}

	start_line->request_state = mrcp_request_state_parse(&field);
	if(start_line->request_state == MRCP_REQUEST_STATE_UNKNOWN) {
		/* request-line */
		start_line->message_type = MRCP_MESSAGE_TYPE_REQUEST;
	}
	else {
		/* event line */
		start_line->message_type = MRCP_MESSAGE_TYPE_EVENT;

		if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse mrcp-version in request-line");
			return FALSE;
		}
	}

	start_line->version = mrcp_version_parse(&field);
	if(start_line->version == MRCP_VERSION_UNKNOWN) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown mrcp-version");
		return FALSE;
	}
	return TRUE;
}

/** Generate MRCP request-line */
static apt_bool_t mrcp_request_line_generate(mrcp_start_line_t *start_line, apt_text_stream_t *stream)
{
	memcpy(stream->pos,start_line->method_name.buf,start_line->method_name.length);
	stream->pos += start_line->method_name.length;
	*stream->pos++ = APT_TOKEN_SP;

	mrcp_request_id_generate(start_line->request_id,stream);
	*stream->pos++ = APT_TOKEN_SP;

	if(start_line->message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		if(start_line->status_code != MRCP_STATUS_CODE_UNKNOWN) {
			mrcp_status_code_generate(start_line->status_code,stream);
			*stream->pos++ = APT_TOKEN_SP;
		}
	}
	else if(start_line->message_type == MRCP_MESSAGE_TYPE_EVENT) {
		mrcp_request_state_generate(start_line->request_state,stream);
		*stream->pos++ = APT_TOKEN_SP;
	}

	mrcp_version_generate(start_line->version,stream);
	return TRUE;
}

/** Parse MRCP response-line */
static apt_bool_t mrcp_response_line_parse(mrcp_start_line_t *start_line, apt_text_stream_t *stream)
{
	apt_str_t field;
	start_line->length = 0;
	if(start_line->version == MRCP_VERSION_2) {
		if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse message-length in response-line");
			return FALSE;
		}
		start_line->length = apt_size_value_parse(&field);
	}

	if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse request-id in response-line");
		return FALSE;
	}
	start_line->request_id = mrcp_request_id_parse(&field);

	if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse status-code in response-line");
		return FALSE;
	}
	start_line->status_code = mrcp_status_code_parse(&field);

	if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse request-state in response-line");
		return FALSE;
	}
	start_line->request_state = mrcp_request_state_parse(&field);
	return TRUE;
}

/** Generate MRCP response-line */
static apt_bool_t mrcp_response_line_generate(mrcp_start_line_t *start_line, apt_text_stream_t *stream)
{
	mrcp_version_generate(start_line->version,stream);
	*stream->pos++ = APT_TOKEN_SP;

	mrcp_request_id_generate(start_line->request_id,stream);
	*stream->pos++ = APT_TOKEN_SP;

	mrcp_status_code_generate(start_line->status_code,stream);
	*stream->pos++ = APT_TOKEN_SP;

	mrcp_request_state_generate(start_line->request_state,stream);
	return TRUE;
}

/** Parse MRCP v2 start-line */
static apt_bool_t mrcp_v2_start_line_parse(mrcp_start_line_t *start_line, apt_text_stream_t *stream, apr_pool_t *pool)
{
	apt_str_t field;
	if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse message-length in v2 start-line");
		return FALSE;
	}
	start_line->length = apt_size_value_parse(&field);

	if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse request-id in v2 start-line");
		return FALSE;
	}
	start_line->request_id = mrcp_request_id_parse(&field);
	if(start_line->request_id == 0 && *field.buf != '0') {
		/* parsing MRCP v2 request or event */
		start_line->message_type = MRCP_MESSAGE_TYPE_REQUEST;
		apt_string_copy(&start_line->method_name,&field,pool);

		if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse request-id in v2 start-line");
			return FALSE;
		}
		start_line->request_id = mrcp_request_id_parse(&field);

		if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == TRUE) {
			/* parsing MRCP v2 event */
			start_line->request_state = mrcp_request_state_parse(&field);
			start_line->message_type = MRCP_MESSAGE_TYPE_EVENT;
		}
	}
	else {
		/* parsing MRCP v2 response */
		start_line->message_type = MRCP_MESSAGE_TYPE_RESPONSE;

		if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse status-code in v2 start-line");
			return FALSE;
		}
		start_line->status_code = mrcp_status_code_parse(&field);

		if(apt_text_field_read(stream,APT_TOKEN_SP,TRUE,&field) == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot parse request-state in v2 start-line");
			return FALSE;
		}
		start_line->request_state = mrcp_request_state_parse(&field);
	}

	return TRUE;
}

/** Generate MRCP v2 start-line */
static apt_bool_t mrcp_v2_start_line_generate(mrcp_start_line_t *start_line, apt_text_stream_t *stream)
{
	char *pos = stream->pos;
	mrcp_version_generate(start_line->version,stream);
	*stream->pos++ = APT_TOKEN_SP;

	start_line->length = stream->pos - pos; /* length is temporary used to store offset */
	/* reserving MAX_DIGIT_COUNT space for start_line->length */
	memset(stream->pos,APT_TOKEN_SP,MAX_DIGIT_COUNT+1);
	stream->pos += MAX_DIGIT_COUNT+1;

	if(start_line->message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		mrcp_request_id_generate(start_line->request_id,stream);
		*stream->pos++ = APT_TOKEN_SP;

		mrcp_status_code_generate(start_line->status_code,stream);
		*stream->pos++ = APT_TOKEN_SP;

		mrcp_request_state_generate(start_line->request_state,stream);
	}
	else {
		memcpy(stream->pos,start_line->method_name.buf,start_line->method_name.length);
		stream->pos += start_line->method_name.length;
		*stream->pos++ = APT_TOKEN_SP;

		mrcp_request_id_generate(start_line->request_id,stream);
		if(start_line->message_type == MRCP_MESSAGE_TYPE_EVENT) {
			*stream->pos++ = APT_TOKEN_SP;
			mrcp_request_state_generate(start_line->request_state,stream);
		}
	}
	return TRUE;
}

/** Initialize MRCP start-line */
MRCP_DECLARE(void) mrcp_start_line_init(mrcp_start_line_t *start_line)
{
	start_line->message_type = MRCP_MESSAGE_TYPE_UNKNOWN;
	start_line->version = MRCP_VERSION_UNKNOWN;
	start_line->length = 0;
	start_line->request_id = 0;
	apt_string_reset(&start_line->method_name);
	start_line->status_code = MRCP_STATUS_CODE_UNKNOWN;
	start_line->request_state = MRCP_REQUEST_STATE_UNKNOWN;
}

/** Parse MRCP start-line */
MRCP_DECLARE(apt_bool_t) mrcp_start_line_parse(mrcp_start_line_t *start_line, apt_str_t *str, apr_pool_t *pool)
{
	apt_text_stream_t line;
	apt_str_t field;
	apt_bool_t status = TRUE;

	start_line->message_type = MRCP_MESSAGE_TYPE_UNKNOWN;
	apt_text_stream_init(&line,str->buf,str->length);
	if(apt_text_field_read(&line,APT_TOKEN_SP,TRUE,&field) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot read the first field in start-line");
		return FALSE;
	}

	if(field.buf == strstr(field.buf,MRCP_NAME)) {
		start_line->version = mrcp_version_parse(&field);

		if(start_line->version == MRCP_VERSION_1) {
			/* parsing MRCP v1 response */
			start_line->message_type = MRCP_MESSAGE_TYPE_RESPONSE;
			status = mrcp_response_line_parse(start_line,&line);
		}
		else if(start_line->version == MRCP_VERSION_2) {
			/* parsing MRCP v2 start-line (request/response/event) */
			status = mrcp_v2_start_line_parse(start_line,&line,pool);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown MRCP version");
			return FALSE;
		}
	}
	else {
		/* parsing MRCP v1 request or event */
		apt_string_copy(&start_line->method_name,&field,pool);
		status = mrcp_request_line_parse(start_line,&line);
	}
	return status;
}

/** Generate MRCP start-line */
MRCP_DECLARE(apt_bool_t) mrcp_start_line_generate(mrcp_start_line_t *start_line, apt_text_stream_t *text_stream)
{
	apt_bool_t status = FALSE;
	if(start_line->version == MRCP_VERSION_1) {
		switch(start_line->message_type) {
			case MRCP_MESSAGE_TYPE_REQUEST:
				status = mrcp_request_line_generate(start_line,text_stream);
				break;
			case MRCP_MESSAGE_TYPE_RESPONSE:
				status = mrcp_response_line_generate(start_line,text_stream);
				break;
			case MRCP_MESSAGE_TYPE_EVENT:
				status = mrcp_request_line_generate(start_line,text_stream);
				break;
			default:
				break;
		}
	}
	else if(start_line->version == MRCP_VERSION_2) {
		status = mrcp_v2_start_line_generate(start_line,text_stream);
	}

	if(status == FALSE) {
		return FALSE;
	}

	return apt_text_eol_insert(text_stream);
}

/** Finalize MRCP start-line generation */
MRCP_DECLARE(apt_bool_t) mrcp_start_line_finalize(mrcp_start_line_t *start_line, apr_size_t content_length, apt_text_stream_t *text_stream)
{
	apr_size_t length = text_stream->pos - text_stream->text.buf + content_length;
	if(start_line->version == MRCP_VERSION_2) {
		/* message-length includes the number of bytes that specify the message-length in the header */
		/* too comlex to generate!!! see the discussion */
		/* http://www1.ietf.org/mail-archive/web/speechsc/current/msg01734.html */
		apt_str_t field;
		field.buf = text_stream->text.buf + start_line->length; /* length is temporary used to store offset */
		length -= MAX_DIGIT_COUNT;
		if(apt_var_length_value_generate(&length,MAX_DIGIT_COUNT,&field) == FALSE) {
			return FALSE;
		}
		field.buf[field.length] = APT_TOKEN_SP;
		start_line->length += field.length;

		field.length = MAX_DIGIT_COUNT - field.length;
		if(field.length) {
			memmove(text_stream->text.buf+field.length,text_stream->text.buf,start_line->length);
			text_stream->text.buf += field.length;
			text_stream->text.length -= field.length;
		}
	}

	start_line->length = length;
	return TRUE;
}

/** Parse MRCP request-id */
MRCP_DECLARE(mrcp_request_id) mrcp_request_id_parse(const apt_str_t *field)
{
	if(field->buf) {
#ifdef TOO_LONG_MRCP_REQUEST_ID
		return apr_atoi64(field->buf);
#else
		return atol(field->buf);
#endif
	}
	return 0;
}

/** Generate MRCP request-id */
MRCP_DECLARE(apt_bool_t) mrcp_request_id_generate(mrcp_request_id request_id, apt_text_stream_t *stream)
{
	int length = apr_snprintf(stream->pos, stream->end - stream->pos, "%"MRCP_REQUEST_ID_FMT, request_id);
	if(length <= 0) {
		return FALSE;
	}
	stream->pos += length;
	return TRUE;
}
