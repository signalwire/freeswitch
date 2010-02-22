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

#include <stdlib.h>
#include <stdio.h>
#include <apr_uuid.h>
#include "apt_text_stream.h"

#define TOKEN_TRUE  "true"
#define TOKEN_FALSE "false"
#define TOKEN_TRUE_LENGTH  (sizeof(TOKEN_TRUE)-1)
#define TOKEN_FALSE_LENGTH (sizeof(TOKEN_FALSE)-1)


/** Navigate through the lines of the text stream (message) */
APT_DECLARE(apt_bool_t) apt_text_line_read(apt_text_stream_t *stream, apt_str_t *line)
{
	char *pos = stream->pos;
	apt_bool_t status = FALSE;
	line->length = 0;
	line->buf = pos;
	/* while not end of stream */
	while(pos < stream->end) {
		if(*pos == APT_TOKEN_CR) {
			/* end of line detected */
			line->length = pos - line->buf;
			pos++;
			if(pos < stream->end && *pos == APT_TOKEN_LF) {
				pos++;
			}
			status = TRUE;
			break;
		}
		else if(*pos == APT_TOKEN_LF) {
			/* end of line detected */
			line->length = pos - line->buf;
			pos++;
			status = TRUE;
			break;
		}
		pos++;
	}

	if(status == TRUE) {
		/* advance stream pos */
		stream->pos = pos;
	}
	else {
		/* end of stream is reached, do not advance stream pos, but set is_eos flag */
		stream->is_eos = TRUE;
	}
	return status;
}

/** Navigate through the headers (name:value pairs) of the text stream (message) 
	Valid headers are:
		name:value<CRLF>
		name: value<CRLF>
		name:    value<CRLF>
		name: value<LF>
		name:<CRLF>              (only name, no value)
		<CRLF>                   (empty header)
	Malformed headers are:
		name:value               (missing end of line <CRLF>)
		name<CRLF>               (missing separator ':')
*/
APT_DECLARE(apt_bool_t) apt_text_header_read(apt_text_stream_t *stream, apt_pair_t *pair)
{
	char *pos = stream->pos;
	apt_bool_t status = FALSE;
	apt_string_reset(&pair->name);
	apt_string_reset(&pair->value);
	/* while not end of stream */
	while(pos < stream->end) {
		if(*pos == APT_TOKEN_CR) {
			/* end of line detected */
			if(pair->value.buf) {
				/* set length of the value */
				pair->value.length = pos - pair->value.buf;
			}
			pos++;
			if(pos < stream->end && *pos == APT_TOKEN_LF) {
				pos++;
			}
			status = TRUE;
			break;
		}
		else if(*pos == APT_TOKEN_LF) {
			/* end of line detected */
			if(pair->value.buf) {
				/* set length of the value */
				pair->value.length = pos - pair->value.buf;
			}
			pos++;
			status = TRUE;
			break;
		}
		else if(!pair->name.length) {
			/* skip initial spaces and read name */
			if(!pair->name.buf && *pos != APT_TOKEN_SP) {
				pair->name.buf = pos;
			}
			if(*pos == ':') {
				/* set length of the name */
				pair->name.length = pos - pair->name.buf;
			}
		}
		else if(!pair->value.length) {
			/* skip initial spaces and read value */
			if(!pair->value.buf && *pos != APT_TOKEN_SP) {
				pair->value.buf = pos;
			}
		}
		pos++;
	}

	if(status == TRUE) {
		/* advance stream pos regardless it's a valid header or not */
		stream->pos = pos;
		
		/* if length == 0 && buf => header is malformed */
		if(!pair->name.length && pair->name.buf) {
			status = FALSE;
		}
	}
	else {
		/* end of stream is reached, do not advance stream pos, but set is_eos flag */
		stream->is_eos = TRUE;
	}

	return status;
}


/** Navigate through the fields of the line */
APT_DECLARE(apt_bool_t) apt_text_field_read(apt_text_stream_t *stream, char separator, apt_bool_t skip_spaces, apt_str_t *field)
{
	char *pos = stream->pos;
	if(skip_spaces == TRUE) {
		while(pos < stream->end && *pos == APT_TOKEN_SP) pos++;
	}

	field->buf = pos;
	field->length = 0;
	while(pos < stream->end && *pos != separator) pos++;

	field->length = pos - field->buf;
	if(pos < stream->end) {
		/* skip the separator */
		pos++;
	}

	stream->pos = pos;
	return field->length ? TRUE : FALSE;
}

/** Scroll text stream */
APT_DECLARE(apt_bool_t) apt_text_stream_scroll(apt_text_stream_t *stream)
{
	apr_size_t remaining_length = stream->text.buf + stream->text.length - stream->pos;
	if(!remaining_length || remaining_length == stream->text.length) {
		stream->pos = stream->text.buf + remaining_length;
		return FALSE;
	}
	memmove(stream->text.buf,stream->pos,remaining_length);
	stream->pos = stream->text.buf + remaining_length;
	stream->text.length = remaining_length;
	*stream->pos = '\0';
	return TRUE;
}

/** Parse id@resource string */
APT_DECLARE(apt_bool_t) apt_id_resource_parse(const apt_str_t *str, char separator, apt_str_t *id, apt_str_t *resource, apr_pool_t *pool)
{
	apt_str_t field = *str;
	const char *pos = strchr(str->buf,separator);
	if(!pos) {
		return FALSE;
	}

	field.length = pos - field.buf;
	if(field.length >= str->length) {
		return FALSE;
	}
	apt_string_copy(id,&field,pool);
	field.buf += field.length + 1;
	field.length = str->length - (field.length + 1);
	apt_string_copy(resource,&field,pool);
	return TRUE;
}

/** Generate id@resource string */
APT_DECLARE(apt_bool_t) apt_id_resource_generate(const apt_str_t *id, const apt_str_t *resource, char separator, apt_str_t *str, apr_pool_t *pool)
{
	apr_size_t length = id->length+resource->length+1;
	char *buf = apr_palloc(pool,length+1);
	memcpy(buf,id->buf,id->length);
	buf[id->length] = separator;
	memcpy(buf+id->length+1,resource->buf,resource->length);
	buf[length] = '\0';
	str->buf = buf;
	str->length = length;
	return TRUE;
}

/** Generate only the name ("name":) of the header */
APT_DECLARE(apt_bool_t) apt_text_header_name_generate(const apt_str_t *name, apt_text_stream_t *stream)
{
	char *pos = stream->pos;
	memcpy(pos,name->buf,name->length);
	pos += name->length;
	*pos++ = ':';
	*pos++ = ' ';
	stream->pos = pos;
	return TRUE;
}

/** Parse name=value pair */
static apt_bool_t apt_pair_parse(apt_pair_t *pair, const apt_str_t *field, apr_pool_t *pool)
{
	apt_text_stream_t stream;
	apt_str_t item;
	stream.text = *field;
	apt_text_stream_reset(&stream);

	/* read name */
	if(apt_text_field_read(&stream,'=',TRUE,&item) == FALSE) {
		return FALSE;
	}
	apt_string_copy(&pair->name,&item,pool);

	/* read value */
	apt_text_field_read(&stream,';',TRUE,&item);
	apt_string_copy(&pair->value,&item,pool);
	return TRUE;
}

/** Parse array of name-value pairs */
APT_DECLARE(apt_bool_t) apt_pair_array_parse(apt_pair_arr_t *arr, const apt_str_t *value, apr_pool_t *pool)
{
	apt_str_t field;
	apt_pair_t *pair;
	apt_text_stream_t stream;
	if(!arr || !value) {
		return FALSE;
	}

	stream.text = *value;
	apt_text_stream_reset(&stream);
	/* read name-value pairs */
	while(apt_text_field_read(&stream,';',TRUE,&field) == TRUE) {
		pair = apr_array_push(arr);
		apt_pair_parse(pair,&field,pool);
	}
	return TRUE;
}

/** Generate array of name-value pairs */
APT_DECLARE(apt_bool_t) apt_pair_array_generate(apt_pair_arr_t *arr, apt_text_stream_t *stream)
{
	int i;
	apt_pair_t *pair;
	char *pos = stream->pos;
	if(!arr) {
		return FALSE;
	}

	for(i=0; i<arr->nelts; i++) {
		pair = (apt_pair_t*)arr->elts + i;
		if(i != 0) {
			*pos++ = ';';
		}
		if(pair->name.length) {
			memcpy(pos,pair->name.buf,pair->name.length);
			pos += pair->name.length;
			if(pair->value.length) {
				*pos++ = '=';
				memcpy(pos,pair->value.buf,pair->value.length);
				pos += pair->value.length;
			}
		}
	}
	stream->pos = pos;
	return TRUE;
}

/** Parse boolean-value */
APT_DECLARE(apt_bool_t) apt_boolean_value_parse(const apt_str_t *str, apt_bool_t *value)
{
	if(!str->buf) {
		return FALSE;
	}
	if(strncasecmp(str->buf,TOKEN_TRUE,TOKEN_TRUE_LENGTH) == 0) {
		*value = TRUE;
		return TRUE;
	}
	if(strncasecmp(str->buf,TOKEN_FALSE,TOKEN_FALSE_LENGTH) == 0) {
		*value = FALSE;
		return TRUE;
	}
	return FALSE;
}

/** Generate boolean-value */
APT_DECLARE(apt_bool_t) apt_boolean_value_generate(apt_bool_t value, apt_text_stream_t *stream)
{
	if(value == TRUE) {
		memcpy(stream->pos,TOKEN_TRUE,TOKEN_TRUE_LENGTH);
		stream->pos += TOKEN_TRUE_LENGTH;
	}
	else {
		memcpy(stream->pos,TOKEN_FALSE,TOKEN_FALSE_LENGTH);
		stream->pos += TOKEN_FALSE_LENGTH;
	}
	return TRUE;
}

/** Parse size_t value */
APT_DECLARE(apr_size_t) apt_size_value_parse(const apt_str_t *str)
{
	return str->buf ? atol(str->buf) : 0;
}

/** Generate apr_size_t value */
APT_DECLARE(apt_bool_t) apt_size_value_generate(apr_size_t value, apt_text_stream_t *stream)
{
	int length = sprintf(stream->pos, "%"APR_SIZE_T_FMT, value);
	if(length <= 0) {
		return FALSE;
	}
	stream->pos += length;
	return TRUE;
}

/** Parse float value */
APT_DECLARE(float) apt_float_value_parse(const apt_str_t *str)
{
	return str->buf ? (float)atof(str->buf) : 0;
}

/** Generate float value */
APT_DECLARE(apt_bool_t) apt_float_value_generate(float value, apt_text_stream_t *stream)
{
	char *end;
	int length = sprintf(stream->pos,"%f",value);
	if(length <= 0) {
		return FALSE;
	}

	/* remove trailing 0s (if any) */
	end = stream->pos + length - 1;
	while(*end == 0x30 && end != stream->pos) end--;

	stream->pos = end + 1;
	return TRUE;
}

/** Generate value plus the length (number of digits) of the value itself. */
APT_DECLARE(apt_bool_t) apt_var_length_value_generate(apr_size_t *value, apr_size_t max_count, apt_str_t *str)
{
	/* (N >= (10^M-M)) ? N+M+1 : N+M */
	apr_size_t temp;
	apr_size_t count; /* M */
	apr_size_t bounds; /* 10^M */
	int length;

	/* calculate count */
	temp = *value;
	count = 0;
	do{count++; temp /= 10;} while(temp);

	/* calculate bounds */
	temp = count;
	bounds = 1;
	do{bounds *= 10; temp--;} while(temp);

	if(*value >= bounds - count) {
		count++;
	}

	*value += count;
	if(count > max_count) {
		return FALSE;
	}

	str->length = 0;
	length = sprintf(str->buf, "%"APR_SIZE_T_FMT, *value);
	if(length <= 0) {
		return FALSE;
	}
	str->length = length;
	return TRUE;
}


/** Generate unique identifier (hex string) */
APT_DECLARE(apt_bool_t) apt_unique_id_generate(apt_str_t *id, apr_size_t length, apr_pool_t *pool)
{
	char *hex_str;
	apr_size_t i;
	apr_size_t count;
	apr_uuid_t uuid;
	apr_uuid_get(&uuid);
	
	hex_str = apr_palloc(pool,length+1);
	
	count = length / 2;
	if(count > sizeof(uuid)) {
		count = sizeof(uuid);
	}
	for(i=0; i<count; i++) {
		sprintf(hex_str+i*2,"%02x",uuid.data[i]);
	}
	hex_str[length] = '\0';

	id->buf = hex_str;
	id->length = length;
	return TRUE;
}
