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

#ifndef APT_TEXT_STREAM_H
#define APT_TEXT_STREAM_H

/**
 * @file apt_text_stream.h
 * @brief Text Stream Parse/Generate Routine
 */ 

#include "apt_string_table.h"
#include "apt_pair.h"

APT_BEGIN_EXTERN_C

/** Space */
#define APT_TOKEN_SP 0x20
/** Horizontal tab */
#define APT_TOKEN_HTAB 0x09
/** Carrige return */
#define APT_TOKEN_CR 0x0D
/** Line feed */
#define APT_TOKEN_LF 0x0A

/** Text stream declaration */
typedef struct apt_text_stream_t apt_text_stream_t;

/** Text stream is used for message parsing and generation */
struct apt_text_stream_t {
	/** Text stream */
	apt_str_t   text;

	/** Current position in the stream */
	char       *pos;
	/** End of stream pointer */
	const char *end;
	/** Is end of stream reached */
	apt_bool_t  is_eos;
};


/**
 * Read entire line of the text stream.
 * @param stream the text stream to navigate on
 * @param line the read line to return
 * @return TRUE if the line is successfully read, otherwise FALSE
 * @remark To be used to navigate through the lines of the text stream (message).
 */
APT_DECLARE(apt_bool_t) apt_text_line_read(apt_text_stream_t *stream, apt_str_t *line);

/**
 * Read header field (name-value pair) of the text stream by scanning entire line.
 * @param stream the text stream to navigate
 * @param pair the read pair to return
 * @return TRUE if the header is successfully read, otherwise FALSE
 * @remark To be used to navigate through the lines and read header fields 
 * (name:value pairs) of the text stream (message).
 */
APT_DECLARE(apt_bool_t) apt_text_header_read(apt_text_stream_t *stream, apt_pair_t *pair);

/**
 * Read the field terminated with specified separator.
 * @param stream the text stream to navigate
 * @param separator the field separator
 * @param skip_spaces whether to skip spaces or not
 * @param field the read field to return
 * @return TRUE if the read field isn't empty, otherwise FALSE
 * @remark To be used to navigate through the fields of the text stream (message).
 */
APT_DECLARE(apt_bool_t) apt_text_field_read(apt_text_stream_t *stream, char separator, apt_bool_t skip_spaces, apt_str_t *field);

/** Generate name-value pair line */
APT_DECLARE(apt_bool_t) apt_text_name_value_insert(apt_text_stream_t *stream, const apt_str_t *name, const apt_str_t *value);

/** Generate only the name ("name:") of the header field */
APT_DECLARE(apt_bool_t) apt_text_header_name_insert(apt_text_stream_t *stream, const apt_str_t *name);

/** Parse array of name-value pairs */
APT_DECLARE(apt_bool_t) apt_pair_array_parse(apt_pair_arr_t *arr, const apt_str_t *value, apr_pool_t *pool);
/** Generate array of name-value pairs */
APT_DECLARE(apt_bool_t) apt_pair_array_generate(const apt_pair_arr_t *arr, apt_str_t *str, apr_pool_t *pool);


/** Parse boolean-value */
APT_DECLARE(apt_bool_t) apt_boolean_value_parse(const apt_str_t *str, apt_bool_t *value);
/** Generate apr_size_t value from pool (buffer is allocated from pool) */
APT_DECLARE(apt_bool_t) apt_boolean_value_generate(apt_bool_t value, apt_str_t *str, apr_pool_t *pool);

/** Parse apr_size_t value */
APT_DECLARE(apr_size_t) apt_size_value_parse(const apt_str_t *str);
/** Generate apr_size_t value from pool (buffer is allocated from pool) */
APT_DECLARE(apt_bool_t) apt_size_value_generate(apr_size_t value, apt_str_t *str, apr_pool_t *pool);

/** Insert apr_size_t value */
APT_DECLARE(apt_bool_t) apt_text_size_value_insert(apt_text_stream_t *stream, apr_size_t value);

/** Parse float value */
APT_DECLARE(float) apt_float_value_parse(const apt_str_t *str);
/** Generate float value (buffer is allocated from pool) */
APT_DECLARE(apt_bool_t) apt_float_value_generate(float value, apt_str_t *str, apr_pool_t *pool);

/** Insert float value */
APT_DECLARE(apt_bool_t) apt_text_float_value_insert(apt_text_stream_t *stream, float value);
/** Insert string value */
APT_DECLARE(apt_bool_t) apt_text_string_insert(apt_text_stream_t *stream, const apt_str_t *str);

/** Reset navigation related data of the text stream */
static APR_INLINE void apt_text_stream_reset(apt_text_stream_t *stream)
{
	stream->pos = stream->text.buf;
	stream->end = stream->text.buf + stream->text.length;
	stream->is_eos = FALSE;
}

/** Initialize text stream */
static APR_INLINE void apt_text_stream_init(apt_text_stream_t *stream, char *buffer, apr_size_t size)
{
	stream->text.buf = buffer;
	stream->text.length = size;
	apt_text_stream_reset(stream);
}

/** Insert end of the line symbol(s) */
static APR_INLINE apt_bool_t apt_text_eol_insert(apt_text_stream_t *stream)
{
	if(stream->pos + 2 < stream->end) {
		*stream->pos++ = APT_TOKEN_CR;
		*stream->pos++ = APT_TOKEN_LF;
		return TRUE;
	}
	return FALSE;
}

/** Insert character */
static APR_INLINE apt_bool_t apt_text_char_insert(apt_text_stream_t *stream, char ch)
{
	if(stream->pos + 1 < stream->end) {
		*stream->pos++ = ch;
		return TRUE;
	}
	return FALSE;
}

/** Insert space */
static APR_INLINE apt_bool_t apt_text_space_insert(apt_text_stream_t *stream)
{
	return apt_text_char_insert(stream,APT_TOKEN_SP);
}

/** Insert space */
static APR_INLINE apt_bool_t apt_text_htab_insert(apt_text_stream_t *stream)
{
	return apt_text_char_insert(stream,APT_TOKEN_HTAB);
}

/** Check whether specified character is a white space (WSP = SP / HTAB) */
static APR_INLINE apt_bool_t apt_text_is_wsp(char ch)
{
	return (ch == APT_TOKEN_SP || ch == APT_TOKEN_HTAB) ? TRUE : FALSE;
}

/** Skip sequence of spaces */
static APR_INLINE void apt_text_spaces_skip(apt_text_stream_t *stream)
{
	while(stream->pos < stream->end && *stream->pos == APT_TOKEN_SP) 
		stream->pos++;
}

/** Skip sequence of white spaces (WSP = SP / HTAB) */
static APR_INLINE void apt_text_white_spaces_skip(apt_text_stream_t *stream)
{
	while(stream->pos < stream->end && apt_text_is_wsp(*stream->pos) == TRUE)
		stream->pos++;
}

/** Skip specified character */
static APR_INLINE void apt_text_char_skip(apt_text_stream_t *stream, char ch)
{
	if(stream->pos < stream->end && *stream->pos == ch) stream->pos++;
}

/** Skip sequence of specified characters */
static APR_INLINE void apt_text_chars_skip(apt_text_stream_t *stream, char ch)
{
	while(stream->pos < stream->end && *stream->pos == ch) stream->pos++;
}

/** Skip to specified character */
static APR_INLINE void apt_text_skip_to_char(apt_text_stream_t *stream, char ch)
{
	while(stream->pos < stream->end && *stream->pos != ch) stream->pos++;
}

/** Check whether end of stream is reached */
static APR_INLINE apt_bool_t apt_text_is_eos(const apt_text_stream_t *stream)
{
	return (stream->pos >= stream->end || stream->is_eos == TRUE) ? TRUE : FALSE;
}

/** Scroll text stream */
APT_DECLARE(apt_bool_t) apt_text_stream_scroll(apt_text_stream_t *stream);

/** Parse id at resource string */
APT_DECLARE(apt_bool_t) apt_id_resource_parse(const apt_str_t *str, char separator, apt_str_t *id, apt_str_t *resource, apr_pool_t *pool);
/** Generate id at resource string */
APT_DECLARE(apt_bool_t) apt_id_resource_generate(const apt_str_t *id, const apt_str_t *resource, char separator, apt_str_t *str, apr_pool_t *pool);

/** Generate value plus the length (number of digits) of the value itself */
APT_DECLARE(apt_bool_t) apt_var_length_value_generate(apr_size_t *value, apr_size_t max_count, apt_str_t *str);

/** Generate completion-cause */
APT_DECLARE(apt_bool_t) apt_completion_cause_generate(const apt_str_table_item_t table[], apr_size_t size, apr_size_t cause, apt_str_t *str, apr_pool_t *pool);

/**
 * Generate unique identifier (hex string)
 * @param id the id to generate
 * @param length the length of hex string to generate
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_bool_t) apt_unique_id_generate(apt_str_t *id, apr_size_t length, apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* APT_TEXT_STREAM_H */
