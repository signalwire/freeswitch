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

#include "rtsp_start_line.h"
#include "apt_string_table.h"
#include "apt_log.h"

APT_LOG_SOURCE_IMPLEMENT(RTSP,rtsp_log_source,"RTSP")

/** Protocol name used in version string */
#define RTSP_NAME "RTSP"
#define RTSP_NAME_LENGTH        (sizeof(RTSP_NAME)-1)

/** Separators used in RTSP version string parse/generate */
#define RTSP_NAME_VERSION_SEPARATOR        '/'
#define RTSP_VERSION_MAJOR_MINOR_SEPARATOR '.'

/** String table of RTSP methods (rtsp_method_id) */
static const apt_str_table_item_t rtsp_method_string_table[] = {
	{{"SETUP",    5},0},
	{{"ANNOUNCE", 8},0},
	{{"TEARDOWN", 8},0},
	{{"DESCRIBE", 8},0},
	{{"OPTIONS",  7},0}
};

/** String table of RTSP reason phrases (rtsp_reason_phrase_e) */
static const apt_str_table_item_t rtsp_reason_string_table[] = {
	{{"OK",                     2},0},
	{{"Created",                7},0},
	{{"Bad Request",           11},0},
	{{"Unauthorized",          12},0},
	{{"Not Found",              9},4},
	{{"Method Not Allowed",    18},0},
	{{"Not Acceptable",        14},4},
	{{"Proxy Auth Required",   19},0},
	{{"Request Timeout",       15},0},
	{{"Session Not Found",     17},2},
	{{"Internal Server Error", 21},0},
	{{"Not Implemented",       15},5},
	{{"Service Unavailable",   19},2}
};

/** Parse RTSP URI */
static apt_bool_t rtsp_resource_uri_parse(const apt_str_t *field, rtsp_request_line_t *request_line, apr_pool_t *pool)
{
	char *str;
	apt_str_t *url = &request_line->url;
	if(!field->length || !field->buf) {
		return FALSE;
	}

	apt_string_copy(url,field,pool);
	if(url->buf[url->length-1] == '/') {
		url->length--;
		url->buf[url->length] = '\0';
	}

	str = strrchr(url->buf,'/');
	if(str) {
		str++;
	}
	request_line->resource_name = str;
	return TRUE;
}

/** Parse RTSP version */
static rtsp_version_e rtsp_version_parse(const apt_str_t *field)
{
	rtsp_version_e version = RTSP_VERSION_UNKNOWN;
	const char *pos;
	if(field->length <= RTSP_NAME_LENGTH || strncasecmp(field->buf,RTSP_NAME,RTSP_NAME_LENGTH) != 0) {
		/* unexpected protocol name */
		return version;
	}

	pos = field->buf + RTSP_NAME_LENGTH;
	if(*pos == RTSP_NAME_VERSION_SEPARATOR) {
		pos++;
		switch(*pos) {
			case '1': version = RTSP_VERSION_1; break;
			default: ;
		}
	}
	return version;
}

/** Generate RTSP version */
static apt_bool_t rtsp_version_generate(rtsp_version_e version, apt_text_stream_t *stream)
{
	if(stream->pos + RTSP_NAME_LENGTH + 1 >= stream->end) {
		return FALSE;
	}
	memcpy(stream->pos,RTSP_NAME,RTSP_NAME_LENGTH);
	stream->pos += RTSP_NAME_LENGTH;
	*stream->pos++ = RTSP_NAME_VERSION_SEPARATOR;

	if(apt_text_size_value_insert(stream,version) == FALSE) {
		return FALSE;
	}

	if(stream->pos + 2 >= stream->end) {
		return FALSE;
	}
	*stream->pos++ = RTSP_VERSION_MAJOR_MINOR_SEPARATOR;
	*stream->pos++ = '0';
	return TRUE;
}

/** Parse RTSP status-code */
static APR_INLINE rtsp_status_code_e rtsp_status_code_parse(const apt_str_t *field)
{
	return apt_size_value_parse(field);
}

/** Generate RTSP status-code */
static APR_INLINE apt_bool_t rtsp_status_code_generate(rtsp_status_code_e status_code, apt_text_stream_t *stream)
{
	return apt_text_size_value_insert(stream,status_code);
}

/** Generate RTSP request-line */
static apt_bool_t rtsp_request_line_generate(rtsp_request_line_t *start_line, apt_text_stream_t *stream)
{
	const apt_str_t *method_name = apt_string_table_str_get(rtsp_method_string_table,RTSP_METHOD_COUNT,start_line->method_id);
	if(!method_name) {
		return FALSE;
	}
	start_line->method_name = *method_name;
	if(apt_text_string_insert(stream,&start_line->method_name) == FALSE) {
		return FALSE;
	}
	if(apt_text_space_insert(stream) == FALSE) {
		return FALSE;
	}

	if(apt_text_string_insert(stream,&start_line->url) == FALSE) {
		return FALSE;
	}
	if(apt_text_space_insert(stream) == FALSE) {
		return FALSE;
	}

	return rtsp_version_generate(start_line->version,stream);
}

/** Generate RTSP status-line */
static apt_bool_t rtsp_status_line_generate(rtsp_status_line_t *start_line, apt_text_stream_t *stream)
{
	if(rtsp_version_generate(start_line->version,stream) == FALSE) {
		return FALSE;
	}
	if(apt_text_space_insert(stream) == FALSE) {
		return FALSE;
	}

	if(rtsp_status_code_generate(start_line->status_code,stream) == FALSE) {
		return FALSE;
	}
	if(apt_text_space_insert(stream) == FALSE) {
		return FALSE;
	}

	return apt_text_string_insert(stream,&start_line->reason);
}

/** Parse RTSP start-line */
RTSP_DECLARE(apt_bool_t) rtsp_start_line_parse(rtsp_start_line_t *start_line, apt_str_t *str, apr_pool_t *pool)
{
	apt_text_stream_t line;
	apt_str_t field;

	apt_text_stream_init(&line,str->buf,str->length);
	if(apt_text_field_read(&line,APT_TOKEN_SP,TRUE,&field) == FALSE) {
		apt_log(RTSP_LOG_MARK,APT_PRIO_WARNING,"Cannot read the first field in start-line");
		return FALSE;
	}

	if(field.buf == strstr(field.buf,RTSP_NAME)) {
		/* parsing RTSP response */
		rtsp_status_line_t  *status_line = &start_line->common.status_line;
		start_line->message_type = RTSP_MESSAGE_TYPE_RESPONSE;
		rtsp_status_line_init(status_line);

		status_line->version = rtsp_version_parse(&field);

		if(apt_text_field_read(&line,APT_TOKEN_SP,TRUE,&field) == FALSE) {
			apt_log(RTSP_LOG_MARK,APT_PRIO_WARNING,"Cannot parse status-code in status-line");
			return FALSE;
		}
		status_line->status_code = rtsp_status_code_parse(&field);

		if(apt_text_field_read(&line,APT_TOKEN_SP,TRUE,&field) == FALSE) {
			apt_log(RTSP_LOG_MARK,APT_PRIO_WARNING,"Cannot parse reason phrase in status-line");
			return FALSE;
		}
		apt_string_copy(&status_line->reason,&field,pool);
	}
	else {
		/* parsing RTSP request */
		rtsp_request_line_t *request_line = &start_line->common.request_line;
		start_line->message_type = RTSP_MESSAGE_TYPE_REQUEST;
		rtsp_request_line_init(request_line);

		apt_string_copy(&request_line->method_name,&field,pool);
		request_line->method_id = apt_string_table_id_find(rtsp_method_string_table,RTSP_METHOD_COUNT,&field);

		if(apt_text_field_read(&line,APT_TOKEN_SP,TRUE,&field) == FALSE) {
			apt_log(RTSP_LOG_MARK,APT_PRIO_WARNING,"Cannot parse URL in request-line");
			return FALSE;
		}
		rtsp_resource_uri_parse(&field,request_line,pool);

		if(apt_text_field_read(&line,APT_TOKEN_SP,TRUE,&field) == FALSE) {
			apt_log(RTSP_LOG_MARK,APT_PRIO_WARNING,"Cannot parse version in request-line");
			return FALSE;
		}
		request_line->version = rtsp_version_parse(&field);
	}

	return TRUE;
}

/** Generate RTSP start-line */
RTSP_DECLARE(apt_bool_t) rtsp_start_line_generate(rtsp_start_line_t *start_line, apt_text_stream_t *stream)
{
	apt_bool_t status = FALSE;
	switch(start_line->message_type) {
		case RTSP_MESSAGE_TYPE_REQUEST:
			status = rtsp_request_line_generate(&start_line->common.request_line,stream);
			break;
		case RTSP_MESSAGE_TYPE_RESPONSE:
			status = rtsp_status_line_generate(&start_line->common.status_line,stream);
			break;
		default:
			break;
	}

	if(status == FALSE) {
		return FALSE;
	}
		
	return apt_text_eol_insert(stream);
}

/** Get reason phrase by status code */
RTSP_DECLARE(const apt_str_t*) rtsp_reason_phrase_get(rtsp_reason_phrase_e reason)
{
	return apt_string_table_str_get(rtsp_reason_string_table,RTSP_REASON_PHRASE_COUNT,reason);
}
