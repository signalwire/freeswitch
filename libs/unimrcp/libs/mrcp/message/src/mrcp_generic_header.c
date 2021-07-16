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

#include "mrcp_generic_header.h"
#include "mrcp_start_line.h"

/** String table of mrcp generic-header fields (mrcp_generic_header_id) */
static const apt_str_table_item_t generic_header_string_table[] = {
	{{"Active-Request-Id-List",    22},3},
	{{"Proxy-Sync-Id",             13},0},
	{{"Accept-Charset",            14},7},
	{{"Content-Type",              12},9},
	{{"Content-Id",                10},9},
	{{"Content-Base",              12},8},
	{{"Content-Encoding",          16},9},
	{{"Content-Location",          16},9},
	{{"Content-Length",            14},10},
	{{"Cache-Control",             13},1},
	{{"Logging-Tag",               11},0},
	{{"Vendor-Specific-Parameters",26},0},
	{{"Accept",                     6},6},
	{{"Fetch-Timeout",             13},0},
	{{"Set-Cookie",                10},10},
	{{"Set-Cookie2",               11},10}
};

/** Parse mrcp request-id list */
static apt_bool_t mrcp_request_id_list_parse(mrcp_request_id_list_t *request_id_list, const apt_str_t *value)
{
	apt_str_t field;
	apt_text_stream_t stream;
	stream.text = *value;
	apt_text_stream_reset(&stream);
	request_id_list->count = 0;
	while(request_id_list->count < MAX_ACTIVE_REQUEST_ID_COUNT) {
		if(apt_text_field_read(&stream,',',TRUE,&field) == FALSE) {
			break;
		}
		request_id_list->ids[request_id_list->count] = mrcp_request_id_parse(&field);
		request_id_list->count++;
	}
	return TRUE;
}

/** Generate mrcp request-id list */
static apt_bool_t mrcp_request_id_list_generate(const mrcp_request_id_list_t *request_id_list, apt_str_t *str, apr_pool_t *pool)
{
	apr_size_t i;
	int length;
	char *pos;

	/* compute estimated length, assuming request-ids consist of upto 10 digits */
	str->length = 10 * request_id_list->count;
	if(request_id_list->count > 1) {
		/* , */
		str->length += request_id_list->count - 1;
	}

	str->buf = apr_palloc(pool,str->length + 1);

	pos = str->buf;
	for(i=0; i<request_id_list->count; i++) {
		if(i != 0) {
			*pos++ = ',';
		}

		length = apr_snprintf(pos, str->length - (pos - str->buf), "%"MRCP_REQUEST_ID_FMT, request_id_list->ids[i]);
		if(length < 0)
			return FALSE;
		pos += length;
	}
	*pos = '\0';
	/* compute actual length */
	str->length = pos - str->buf;
	return TRUE;
}


/** Initialize generic-header */
static void mrcp_generic_header_init(mrcp_generic_header_t *generic_header)
{
	generic_header->active_request_id_list.count = 0;
	apt_string_reset(&generic_header->proxy_sync_id);
	apt_string_reset(&generic_header->accept_charset);
	apt_string_reset(&generic_header->content_type);
	apt_string_reset(&generic_header->content_id);
	apt_string_reset(&generic_header->content_base);
	apt_string_reset(&generic_header->content_encoding);
	apt_string_reset(&generic_header->content_location);
	generic_header->content_length = 0;
	apt_string_reset(&generic_header->cache_control);
	apt_string_reset(&generic_header->logging_tag);
	generic_header->vendor_specific_params = NULL;
	/* initializes additionnal MRCP v2 generic header fields */
	apt_string_reset(&generic_header->accept);
	generic_header->fetch_timeout = 0;
	apt_string_reset(&generic_header->set_cookie);
	apt_string_reset(&generic_header->set_cookie2);
}


/** Allocate generic-header */
static void* mrcp_generic_header_allocate(mrcp_header_accessor_t *accessor, apr_pool_t *pool)
{
	mrcp_generic_header_t *generic_header = apr_palloc(pool,sizeof(mrcp_generic_header_t));
	mrcp_generic_header_init(generic_header);
	accessor->data = generic_header;
	return accessor->data;
}

/** Parse generic-header */
static apt_bool_t mrcp_generic_header_parse(mrcp_header_accessor_t *accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	apt_bool_t status = TRUE;
	mrcp_generic_header_t *generic_header = accessor->data;
	switch(id) {
		case GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST:
			mrcp_request_id_list_parse(&generic_header->active_request_id_list,value);
			break;
		case GENERIC_HEADER_PROXY_SYNC_ID:
			generic_header->proxy_sync_id = *value;
			break;
		case GENERIC_HEADER_ACCEPT_CHARSET:
			generic_header->accept_charset = *value;
			break;
		case GENERIC_HEADER_CONTENT_TYPE:
			generic_header->content_type =  *value;
			break;
		case GENERIC_HEADER_CONTENT_ID:
			generic_header->content_id = *value;
			break;
		case GENERIC_HEADER_CONTENT_BASE:
			generic_header->content_base = *value;
			break;
		case GENERIC_HEADER_CONTENT_ENCODING:
			generic_header->content_encoding = *value;
			break;
		case GENERIC_HEADER_CONTENT_LOCATION:
			generic_header->content_location = *value;
			break;
		case GENERIC_HEADER_CONTENT_LENGTH:
			generic_header->content_length = apt_size_value_parse(value);
			break;
		case GENERIC_HEADER_CACHE_CONTROL:
			generic_header->cache_control = *value;
			break;
		case GENERIC_HEADER_LOGGING_TAG:
			generic_header->logging_tag = *value;
			break;
		case GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS:
			if(!generic_header->vendor_specific_params) {
				generic_header->vendor_specific_params = apt_pair_array_create(1,pool);
			}
			apt_pair_array_parse(generic_header->vendor_specific_params,value,pool);
			break;
		case GENERIC_HEADER_ACCEPT:
			generic_header->accept = *value;
			break;
		case GENERIC_HEADER_FETCH_TIMEOUT:
			generic_header->fetch_timeout = apt_size_value_parse(value);
			break;
		case GENERIC_HEADER_SET_COOKIE:
			generic_header->set_cookie = *value;
			break;
		case GENERIC_HEADER_SET_COOKIE2:
			generic_header->set_cookie2 = *value;
			break;
		default:
			status = FALSE;
	}
	return status;
}

/** Generate generic-header */
static apt_bool_t mrcp_generic_header_generate(const mrcp_header_accessor_t *accessor, apr_size_t id, apt_str_t *value, apr_pool_t *pool)
{
	mrcp_generic_header_t *generic_header = accessor->data;
	switch(id) {
		case GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST:
			mrcp_request_id_list_generate(&generic_header->active_request_id_list,value,pool);
			break;
		case GENERIC_HEADER_PROXY_SYNC_ID:
			*value = generic_header->proxy_sync_id;
			break;
		case GENERIC_HEADER_ACCEPT_CHARSET:
			*value = generic_header->accept_charset;
			break;
		case GENERIC_HEADER_CONTENT_TYPE:
			*value = generic_header->content_type;
			break;
		case GENERIC_HEADER_CONTENT_ID:
			*value = generic_header->content_id;
			break;
		case GENERIC_HEADER_CONTENT_BASE:
			*value = generic_header->content_base;
			break;
		case GENERIC_HEADER_CONTENT_ENCODING:
			*value = generic_header->content_encoding;
			break;
		case GENERIC_HEADER_CONTENT_LOCATION:
			*value = generic_header->content_location;
			break;
		case GENERIC_HEADER_CONTENT_LENGTH:
			apt_size_value_generate(generic_header->content_length,value,pool);
			break;
		case GENERIC_HEADER_CACHE_CONTROL:
			*value = generic_header->cache_control;
			break;
		case GENERIC_HEADER_LOGGING_TAG:
			*value = generic_header->logging_tag;
			break;
		case GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS:
			apt_pair_array_generate(generic_header->vendor_specific_params,value,pool);
			break;
		case GENERIC_HEADER_ACCEPT:
			*value = generic_header->accept;
			break;
		case GENERIC_HEADER_FETCH_TIMEOUT:
			apt_size_value_generate(generic_header->fetch_timeout,value,pool);
			break;
		case GENERIC_HEADER_SET_COOKIE:
			*value = generic_header->set_cookie;
			break;
		case GENERIC_HEADER_SET_COOKIE2:
			*value = generic_header->set_cookie2;
			break;
		default:
			break;
	}
	return TRUE;
}

/** Duplicate generic-header */
static apt_bool_t mrcp_generic_header_duplicate(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	mrcp_generic_header_t *generic_header = accessor->data;
	const mrcp_generic_header_t *src_generic_header = src->data;
	apt_bool_t status = TRUE;

	if(!generic_header || !src_generic_header) {
		return FALSE;
	}

	switch(id) {
		case GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST:
			break;
		case GENERIC_HEADER_PROXY_SYNC_ID:
			generic_header->proxy_sync_id = *value;
			break;
		case GENERIC_HEADER_ACCEPT_CHARSET:
			generic_header->accept_charset = *value;
			break;
		case GENERIC_HEADER_CONTENT_TYPE:
			generic_header->content_type = *value;
			break;
		case GENERIC_HEADER_CONTENT_ID:
			generic_header->content_id = *value;
			break;
		case GENERIC_HEADER_CONTENT_BASE:
			generic_header->content_base = *value;
			break;
		case GENERIC_HEADER_CONTENT_ENCODING:
			generic_header->content_encoding = *value;
			break;
		case GENERIC_HEADER_CONTENT_LOCATION:
			generic_header->content_location = *value;
			break;
		case GENERIC_HEADER_CONTENT_LENGTH:
			generic_header->content_length = src_generic_header->content_length;
			break;
		case GENERIC_HEADER_CACHE_CONTROL:
			generic_header->cache_control = *value;
			break;
		case GENERIC_HEADER_LOGGING_TAG:
			generic_header->logging_tag = *value;
			break;
		case GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS:
			generic_header->vendor_specific_params = apt_pair_array_copy(src_generic_header->vendor_specific_params,pool);
			break;
		case GENERIC_HEADER_ACCEPT:
			generic_header->accept = *value;
			break;
		case GENERIC_HEADER_FETCH_TIMEOUT:
			generic_header->fetch_timeout = src_generic_header->fetch_timeout;
			break;
		case GENERIC_HEADER_SET_COOKIE:
			generic_header->set_cookie = *value;
			break;
		case GENERIC_HEADER_SET_COOKIE2:
			generic_header->set_cookie2 = *value;
			break;
		default:
			status = FALSE;
	}
	return status;
}

static const mrcp_header_vtable_t vtable = {
	mrcp_generic_header_allocate,
	NULL, /* nothing to destroy */
	mrcp_generic_header_parse,
	mrcp_generic_header_generate,
	mrcp_generic_header_duplicate,
	generic_header_string_table,
	GENERIC_HEADER_COUNT
};


MRCP_DECLARE(const mrcp_header_vtable_t*) mrcp_generic_header_vtable_get(mrcp_version_e version)
{
	return &vtable;
}


/** Append active request id list */
MRCP_DECLARE(apt_bool_t) active_request_id_list_append(mrcp_generic_header_t *generic_header, mrcp_request_id request_id)
{
	mrcp_request_id_list_t *request_id_list = &generic_header->active_request_id_list;
	if(request_id_list->count >= MAX_ACTIVE_REQUEST_ID_COUNT) {
		return FALSE;
	}
	request_id_list->ids[request_id_list->count++] = request_id;
	return TRUE;
}

/** Find request id in active request id list */
MRCP_DECLARE(apt_bool_t) active_request_id_list_find(const mrcp_generic_header_t *generic_header, mrcp_request_id request_id)
{
	apr_size_t i;
	const mrcp_request_id_list_t *request_id_list = &generic_header->active_request_id_list;
	for(i=0; i<request_id_list->count; i++) {
		if(request_id_list->ids[i] == request_id) {
			return TRUE;
		}
	}
	return FALSE;
}
