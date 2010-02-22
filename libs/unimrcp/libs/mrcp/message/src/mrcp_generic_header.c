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
static apt_bool_t mrcp_request_id_list_generate(mrcp_request_id_list_t *request_id_list, apt_text_stream_t *stream)
{
	size_t i;
	for(i=0; i<request_id_list->count; i++) {
		mrcp_request_id_generate(request_id_list->ids[i],stream);
		if(i < request_id_list->count-1) {
			*stream->pos++ = ',';
		}
	}
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
	/* initializes additionnal MRCP V2 generic headers */
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
static apt_bool_t mrcp_generic_header_parse(mrcp_header_accessor_t *accessor, size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	apt_bool_t status = TRUE;
	mrcp_generic_header_t *generic_header = accessor->data;
	switch(id) {
		case GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST:
			mrcp_request_id_list_parse(&generic_header->active_request_id_list,value);
			break;
		case GENERIC_HEADER_PROXY_SYNC_ID:
			apt_string_copy(&generic_header->proxy_sync_id,value,pool);
			break;
		case GENERIC_HEADER_ACCEPT_CHARSET:
			apt_string_copy(&generic_header->accept_charset,value,pool);
			break;
		case GENERIC_HEADER_CONTENT_TYPE:
			apt_string_copy(&generic_header->content_type,value,pool);
			break;
		case GENERIC_HEADER_CONTENT_ID:
			apt_string_copy(&generic_header->content_id,value,pool);
			break;
		case GENERIC_HEADER_CONTENT_BASE:
			apt_string_copy(&generic_header->content_base,value,pool);
			break;
		case GENERIC_HEADER_CONTENT_ENCODING:
			apt_string_copy(&generic_header->content_encoding,value,pool);
			break;
		case GENERIC_HEADER_CONTENT_LOCATION:
			apt_string_copy(&generic_header->content_location,value,pool);
			break;
		case GENERIC_HEADER_CONTENT_LENGTH:
			generic_header->content_length = apt_size_value_parse(value);
			break;
		case GENERIC_HEADER_CACHE_CONTROL:
			apt_string_copy(&generic_header->cache_control,value,pool);
			break;
		case GENERIC_HEADER_LOGGING_TAG:
			apt_string_copy(&generic_header->logging_tag,value,pool);
			break;
		case GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS:
			if(!generic_header->vendor_specific_params) {
				generic_header->vendor_specific_params = apt_pair_array_create(1,pool);
			}
			apt_pair_array_parse(generic_header->vendor_specific_params,value,pool);
			break;
		case GENERIC_HEADER_ACCEPT:
			apt_string_copy(&generic_header->accept,value,pool);
			break;
		case GENERIC_HEADER_FETCH_TIMEOUT:
			generic_header->fetch_timeout = apt_size_value_parse(value);
			break;
		case GENERIC_HEADER_SET_COOKIE:
			apt_string_copy(&generic_header->set_cookie,value,pool);
			break;
		case GENERIC_HEADER_SET_COOKIE2:
			apt_string_copy(&generic_header->set_cookie2,value,pool);
			break;
		default:
			status = FALSE;
	}
	return status;
}

/** Generate generic-header */
static apt_bool_t mrcp_generic_header_generate(mrcp_header_accessor_t *accessor, size_t id, apt_text_stream_t *value)
{
	mrcp_generic_header_t *generic_header = accessor->data;
	switch(id) {
		case GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST:
			mrcp_request_id_list_generate(&generic_header->active_request_id_list,value);
			break;
		case GENERIC_HEADER_PROXY_SYNC_ID:
			apt_string_value_generate(&generic_header->proxy_sync_id,value);
			break;
		case GENERIC_HEADER_ACCEPT_CHARSET:
			apt_string_value_generate(&generic_header->accept_charset,value);
			break;
		case GENERIC_HEADER_CONTENT_TYPE:
			apt_string_value_generate(&generic_header->content_type,value);
			break;
		case GENERIC_HEADER_CONTENT_ID:
			apt_string_value_generate(&generic_header->content_id,value);
			break;
		case GENERIC_HEADER_CONTENT_BASE:
			apt_string_value_generate(&generic_header->content_base,value);
			break;
		case GENERIC_HEADER_CONTENT_ENCODING:
			apt_string_value_generate(&generic_header->content_encoding,value);
			break;
		case GENERIC_HEADER_CONTENT_LOCATION:
			apt_string_value_generate(&generic_header->content_location,value);
			break;
		case GENERIC_HEADER_CONTENT_LENGTH:
			apt_size_value_generate(generic_header->content_length,value);
			break;
		case GENERIC_HEADER_CACHE_CONTROL:
			apt_string_value_generate(&generic_header->cache_control,value);
			break;
		case GENERIC_HEADER_LOGGING_TAG:
			apt_string_value_generate(&generic_header->logging_tag,value);
			break;
		case GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS:
			apt_pair_array_generate(generic_header->vendor_specific_params,value);
			break;
		case GENERIC_HEADER_ACCEPT:
			apt_string_value_generate(&generic_header->accept,value);
			break;
		case GENERIC_HEADER_FETCH_TIMEOUT:
			apt_size_value_generate(generic_header->fetch_timeout,value);
			break;
		case GENERIC_HEADER_SET_COOKIE:
			apt_string_value_generate(&generic_header->set_cookie,value);
			break;
		case GENERIC_HEADER_SET_COOKIE2:
			apt_string_value_generate(&generic_header->set_cookie2,value);
			break;
		default:
			break;
	}
	return TRUE;
}

/** Duplicate generic-header */
static apt_bool_t mrcp_generic_header_duplicate(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, size_t id, apr_pool_t *pool)
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
			apt_string_copy(&generic_header->proxy_sync_id,&src_generic_header->proxy_sync_id,pool);
			break;
		case GENERIC_HEADER_ACCEPT_CHARSET:
			apt_string_copy(&generic_header->accept_charset,&src_generic_header->accept_charset,pool);
			break;
		case GENERIC_HEADER_CONTENT_TYPE:
			apt_string_copy(&generic_header->content_type,&src_generic_header->content_type,pool);
			break;
		case GENERIC_HEADER_CONTENT_ID:
			apt_string_copy(&generic_header->content_id,&src_generic_header->content_id,pool);
			break;
		case GENERIC_HEADER_CONTENT_BASE:
			apt_string_copy(&generic_header->content_base,&src_generic_header->content_base,pool);
			break;
		case GENERIC_HEADER_CONTENT_ENCODING:
			apt_string_copy(&generic_header->content_encoding,&src_generic_header->content_encoding,pool);
			break;
		case GENERIC_HEADER_CONTENT_LOCATION:
			apt_string_copy(&generic_header->content_location,&src_generic_header->content_location,pool);
			break;
		case GENERIC_HEADER_CONTENT_LENGTH:
			generic_header->content_length = src_generic_header->content_length;
			break;
		case GENERIC_HEADER_CACHE_CONTROL:
			apt_string_copy(&generic_header->cache_control,&src_generic_header->cache_control,pool);
			break;
		case GENERIC_HEADER_LOGGING_TAG:
			apt_string_copy(&generic_header->logging_tag,&src_generic_header->logging_tag,pool);
			break;
		case GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS:
			generic_header->vendor_specific_params = apt_pair_array_copy(src_generic_header->vendor_specific_params,pool);
			break;
		case GENERIC_HEADER_ACCEPT:
			apt_string_copy(&generic_header->accept,&src_generic_header->accept,pool);
			break;
		case GENERIC_HEADER_FETCH_TIMEOUT:
			generic_header->fetch_timeout = src_generic_header->fetch_timeout;
			break;
		case GENERIC_HEADER_SET_COOKIE:
			apt_string_copy(&generic_header->set_cookie,&src_generic_header->set_cookie,pool);
			break;
		case GENERIC_HEADER_SET_COOKIE2:
			apt_string_copy(&generic_header->set_cookie2,&src_generic_header->set_cookie2,pool);
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
MRCP_DECLARE(apt_bool_t) active_request_id_list_find(mrcp_generic_header_t *generic_header, mrcp_request_id request_id)
{
	size_t i;
	mrcp_request_id_list_t *request_id_list = &generic_header->active_request_id_list;
	for(i=0; i<request_id_list->count; i++) {
		if(request_id_list->ids[i] == request_id) {
			return TRUE;
		}
	}
	return FALSE;
}
