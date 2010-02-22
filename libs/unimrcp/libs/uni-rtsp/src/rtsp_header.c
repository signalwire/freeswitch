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

#include "rtsp_header.h"
#include "apt_string_table.h"

/** String table of RTSP header fields (rtsp_header_field_id) */
static const apt_str_table_item_t rtsp_header_string_table[] = {
	{{"CSeq",           4},1},
	{{"Transport",      9},0},
	{{"Session",        7},0},
	{{"RTP-Info",       8},0},
	{{"Content-Type",  12},8},
	{{"Content-Length",14},8}
};

/** String table of RTSP content types (rtsp_content_type) */
static const apt_str_table_item_t rtsp_content_type_string_table[] = {
	{{"application/sdp", 15},12},
	{{"application/mrcp",16},12}
};

/** String table of RTSP transport protocols (rtsp_transport_e) */
static const apt_str_table_item_t rtsp_transport_string_table[] = {
	{{"RTP", 3},0}
};

/** String table of RTSP lower transport protocols (rtsp_lower_transport_e) */
static const apt_str_table_item_t rtsp_lower_transport_string_table[] = {
	{{"UDP", 3},0},
	{{"TCP", 3},0}
};

/** String table of RTSP transport profiles (rtsp_profile_e) */
static const apt_str_table_item_t rtsp_profile_string_table[] = {
	{{"AVP", 3},0},
	{{"SAVP",4},0}
};

/** String table of RTSP transport attributes (rtsp_transport_attrib_e) */
static const apt_str_table_item_t rtsp_transport_attrib_string_table[] = {
	{{"client_port", 11},0},
	{{"server_port", 11},2},
	{{"source",       6},2},
	{{"destination", 11},0},
	{{"unicast",      7},0},
	{{"multicast",    9},1},
	{{"mode",         4},2}
};

/** Parse RTSP transport port range */
static apt_bool_t rtsp_port_range_parse(rtsp_port_range_t *port_range, apt_text_stream_t *stream)
{
	apt_str_t value;
	/* read min value */
	if(apt_text_field_read(stream,'-',TRUE,&value) == FALSE) {
		return FALSE;
	}
	port_range->min = (apr_port_t)apt_size_value_parse(&value);
	
	/* read optional max value */
	if(apt_text_field_read(stream,';',TRUE,&value) == TRUE) {
		port_range->max = (apr_port_t)apt_size_value_parse(&value);
	}

	return TRUE;
}

/** Generate RTSP transport port range */
static apt_bool_t rtsp_port_range_generate(rtsp_transport_attrib_e attrib, const rtsp_port_range_t *port_range, apt_text_stream_t *text_stream)
{
	const apt_str_t *str;
	str = apt_string_table_str_get(rtsp_transport_attrib_string_table,RTSP_TRANSPORT_ATTRIB_COUNT,attrib);
	if(!str) {
		return FALSE;
	}
	apt_string_value_generate(str,text_stream);
	apt_text_char_insert(text_stream,'=');
	apt_size_value_generate(port_range->min,text_stream);
	if(port_range->max > port_range->min) {
		apt_text_char_insert(text_stream,'-');
		apt_size_value_generate(port_range->max,text_stream);
	}
	return TRUE;
}

/** Parse text value of RTSP transport attrib (source/destination, e.t.c) */
static apt_bool_t rtsp_transport_attrib_value_parse(apt_str_t *value, apt_text_stream_t *stream, apr_pool_t *pool)
{
	apt_str_t field;
	/* read value */
	if(apt_text_field_read(stream,';',TRUE,&field) == FALSE) {
		return FALSE;
	}
	apt_string_copy(value,&field,pool);
	return TRUE;
}

/** Parse RTSP transport */
static apt_bool_t rtsp_transport_attrib_parse(rtsp_transport_t *transport, const apt_str_t *field, apr_pool_t *pool)
{
	rtsp_transport_attrib_e attrib;
	apt_str_t name;
	apt_text_stream_t stream;

	stream.text = *field;
	apt_text_stream_reset(&stream);

	/* read attrib name */
	if(apt_text_field_read(&stream,'=',TRUE,&name) == FALSE) {
		return FALSE;
	}

	attrib = apt_string_table_id_find(rtsp_transport_attrib_string_table,RTSP_TRANSPORT_ATTRIB_COUNT,&name);
	switch(attrib) {
		case RTSP_TRANSPORT_ATTRIB_CLIENT_PORT:
			rtsp_port_range_parse(&transport->client_port_range,&stream);
			break;
		case RTSP_TRANSPORT_ATTRIB_SERVER_PORT:
			rtsp_port_range_parse(&transport->client_port_range,&stream);
			break;
		case RTSP_TRANSPORT_ATTRIB_SOURCE:
			rtsp_transport_attrib_value_parse(&transport->source,&stream,pool);
			break;
		case RTSP_TRANSPORT_ATTRIB_DESTINATION:
			rtsp_transport_attrib_value_parse(&transport->destination,&stream,pool);
			break;
		case RTSP_TRANSPORT_ATTRIB_UNICAST:
			transport->delivery = RTSP_DELIVERY_UNICAST;
			break;
		case RTSP_TRANSPORT_ATTRIB_MULTICAST:
			transport->delivery = RTSP_DELIVERY_MULTICAST;
			break;
		case RTSP_TRANSPORT_ATTRIB_MODE:
			rtsp_transport_attrib_value_parse(&transport->mode,&stream,pool);
			break;
		default:
			break;
	}
	return TRUE;
}

/** Parse RTSP transport protocol (RTP/AVP[/UDP]) */
static apt_bool_t rtsp_transport_protocol_parse(rtsp_transport_t *transport, const apt_str_t *value)
{
	apt_str_t field;
	apt_text_stream_t stream;

	stream.text = *value;
	apt_text_stream_reset(&stream);

	/* set the defaults */
	transport->protocol = RTSP_TRANSPORT_RTP;
	transport->profile = RTSP_PROFILE_AVP;
	transport->lower_protocol = RTSP_LOWER_TRANSPORT_UDP;

	/* read transport protocol (RTP) */
	if(apt_text_field_read(&stream,'/',TRUE,&field) == FALSE) {
		return FALSE;
	}
	transport->protocol = apt_string_table_id_find(rtsp_transport_string_table,RTSP_TRANSPORT_COUNT,&field);
	if(transport->protocol >= RTSP_TRANSPORT_COUNT) {
		return FALSE;
	}

	/* read transport profile (AVP) */
	if(apt_text_field_read(&stream,'/',TRUE,&field) == FALSE) {
		return FALSE;
	}
	transport->profile = apt_string_table_id_find(rtsp_profile_string_table,RTSP_PROFILE_COUNT,&field);
	if(transport->profile >= RTSP_PROFILE_COUNT) {
		return FALSE;
	}

	/* read optional lower transport protocol (UDP) */
	if(apt_text_field_read(&stream,'/',TRUE,&field) == TRUE) {
		transport->lower_protocol = apt_string_table_id_find(rtsp_lower_transport_string_table,RTSP_LOWER_TRANSPORT_COUNT,&field);
		if(transport->lower_protocol >= RTSP_LOWER_TRANSPORT_COUNT) {
			return FALSE;
		}
	}

	return TRUE;
}

/** Parse RTSP transport */
static apt_bool_t rtsp_transport_parse(rtsp_transport_t *transport, const apt_str_t *line, apr_pool_t *pool)
{
	apt_str_t field;
	apt_text_stream_t stream;

	stream.text = *line;
	apt_text_stream_reset(&stream);
	/* read transport protocol (RTP/AVP[/UDP]) */
	if(apt_text_field_read(&stream,';',TRUE,&field) == FALSE) {
		return FALSE;
	}
	
	/* parse transport protocol (RTP/AVP[/UDP]) */
	if(rtsp_transport_protocol_parse(transport,&field) == FALSE) {
		return FALSE;
	}

	/* read transport attributes */
	while(apt_text_field_read(&stream,';',TRUE,&field) == TRUE) {
		rtsp_transport_attrib_parse(transport,&field,pool);
	}

	return TRUE;
}

/** Generate RTSP transport */
static apt_bool_t rtsp_transport_generate(rtsp_transport_t *transport, apt_text_stream_t *text_stream)
{
	const apt_str_t *protocol = apt_string_table_str_get(rtsp_transport_string_table,RTSP_TRANSPORT_COUNT,transport->protocol);
	const apt_str_t *profile = apt_string_table_str_get(rtsp_profile_string_table,RTSP_PROFILE_COUNT,transport->profile);
	if(!protocol || !profile) {
		return FALSE;
	}
	apt_string_value_generate(protocol,text_stream);
	apt_text_char_insert(text_stream,'/');
	apt_string_value_generate(profile,text_stream);

	if(transport->delivery != RTSP_DELIVERY_NONE) {
		const apt_str_t *delivery = NULL;
		rtsp_transport_attrib_e attrib = RTSP_TRANSPORT_ATTRIB_NONE;
		if(transport->delivery == RTSP_DELIVERY_UNICAST) {
			attrib = RTSP_TRANSPORT_ATTRIB_UNICAST;
		}
		else if(transport->delivery == RTSP_DELIVERY_MULTICAST) {
			attrib = RTSP_TRANSPORT_ATTRIB_MULTICAST;
		}
		delivery = apt_string_table_str_get(rtsp_transport_attrib_string_table,RTSP_TRANSPORT_ATTRIB_COUNT,attrib);
		if(!delivery) {
			return FALSE;
		}
	
		apt_text_char_insert(text_stream,';');
		apt_string_value_generate(delivery,text_stream);
	}

	if(rtsp_port_range_is_valid(&transport->client_port_range) == TRUE) {
		apt_text_char_insert(text_stream,';');
		rtsp_port_range_generate(RTSP_TRANSPORT_ATTRIB_CLIENT_PORT,&transport->client_port_range,text_stream);
	}
	if(rtsp_port_range_is_valid(&transport->server_port_range) == TRUE) {
		apt_text_char_insert(text_stream,';');
		rtsp_port_range_generate(RTSP_TRANSPORT_ATTRIB_SERVER_PORT,&transport->server_port_range,text_stream);
	}

	if(transport->mode.length) {
		const apt_str_t *str;
		str = apt_string_table_str_get(rtsp_transport_attrib_string_table,RTSP_TRANSPORT_ATTRIB_COUNT,RTSP_TRANSPORT_ATTRIB_MODE);
		if(str) {
			apt_text_char_insert(text_stream,';');
			apt_string_value_generate(str,text_stream);
			apt_text_char_insert(text_stream,'=');
			apt_string_value_generate(&transport->mode,text_stream);
		}
	}
	return TRUE;
}

/** Parse RTSP transport */
static apt_bool_t rtsp_session_id_parse(apt_str_t *session_id, const apt_str_t *value, apr_pool_t *pool)
{
	char *sep;
	if(!value->buf) {
		return FALSE;
	}
	apt_string_copy(session_id,value,pool);
	sep = strchr(session_id->buf,';');
	if(sep) {
		session_id->length = sep - session_id->buf;
		*sep = '\0';
	}
	return TRUE;
}

/** Parse RTSP header field */
static apt_bool_t rtsp_header_field_parse(rtsp_header_t *header, rtsp_header_field_id id, const apt_str_t *value, apr_pool_t *pool)
{
	apt_bool_t status = TRUE;
	switch(id) {
		case RTSP_HEADER_FIELD_CSEQ:
			header->cseq = apt_size_value_parse(value);
			break;
		case RTSP_HEADER_FIELD_TRANSPORT:
			status = rtsp_transport_parse(&header->transport,value,pool);
			break;
		case RTSP_HEADER_FIELD_SESSION_ID:
			status = rtsp_session_id_parse(&header->session_id,value,pool);
			break;
		case RTSP_HEADER_FIELD_RTP_INFO:
			apt_string_copy(&header->rtp_info,value,pool);
			break;
		case RTSP_HEADER_FIELD_CONTENT_TYPE:
			header->content_type = apt_string_table_id_find(rtsp_content_type_string_table,RTSP_CONTENT_TYPE_COUNT,value);
			break;
		case RTSP_HEADER_FIELD_CONTENT_LENGTH:
			header->content_length = apt_size_value_parse(value);
			break;
		default:
			status = FALSE;
	}
	return status;
}

/** Generate RTSP header field */
static apr_size_t rtsp_header_field_generate(rtsp_header_t *header, apr_size_t id, apt_text_stream_t *value)
{
	switch(id) {
		case RTSP_HEADER_FIELD_CSEQ:
			apt_size_value_generate(header->cseq,value);
			break;
		case RTSP_HEADER_FIELD_TRANSPORT:
			rtsp_transport_generate(&header->transport,value);
			break;
		case RTSP_HEADER_FIELD_SESSION_ID:
			apt_string_value_generate(&header->session_id,value);
			break;
		case RTSP_HEADER_FIELD_RTP_INFO:
			apt_string_value_generate(&header->rtp_info,value);
			break;
		case RTSP_HEADER_FIELD_CONTENT_TYPE:
		{
			const apt_str_t *name = apt_string_table_str_get(rtsp_content_type_string_table,RTSP_CONTENT_TYPE_COUNT,header->content_type);
			if(name) {
				apt_string_value_generate(name,value);
			}
			break;
		}
		case RTSP_HEADER_FIELD_CONTENT_LENGTH:
			apt_size_value_generate(header->content_length,value);
			break;
		default:
			break;
	}
	return TRUE;
}

/** Parse RTSP header */
RTSP_DECLARE(apt_bool_t) rtsp_header_parse(rtsp_header_t *header, apt_text_stream_t *text_stream, apr_pool_t *pool)
{
	apt_pair_t pair;
	apt_bool_t result = FALSE;

	do {
		if(apt_text_header_read(text_stream,&pair) == TRUE) {
			if(pair.name.length) {
				/* parse header_field (name/value) */
				rtsp_header_field_id id = apt_string_table_id_find(rtsp_header_string_table,RTSP_HEADER_FIELD_COUNT,&pair.name);
				if(id < RTSP_HEADER_FIELD_COUNT) {
					if(rtsp_header_field_parse(header,id,&pair.value,pool) == TRUE) {
						rtsp_header_property_add(&header->property_set,id);
					}
				}
			}
			else {
				/* empty header -> exit */
				result = TRUE;
				break;
			}
		}
	}
	while(apt_text_is_eos(text_stream) == FALSE);

	return result;
}

/** Generate RTSP header */
RTSP_DECLARE(apt_bool_t) rtsp_header_generate(rtsp_header_t *header, apt_text_stream_t *text_stream)
{
	const apt_str_t *name;
	apr_size_t i;
	rtsp_header_property_t property_set;

	property_set = header->property_set;
	for(i=0; i<RTSP_HEADER_FIELD_COUNT && property_set != 0; i++) {
		if(rtsp_header_property_check(&property_set,i) == TRUE) {
			name = apt_string_table_str_get(rtsp_header_string_table,RTSP_HEADER_FIELD_COUNT,i);
			if(!name) {
				continue;
			}
			
			apt_text_header_name_generate(name,text_stream);
			rtsp_header_field_generate(header,i,text_stream);
			apt_text_eol_insert(text_stream);
			
			rtsp_header_property_remove(&property_set,i);
		}
	}

	apt_text_eol_insert(text_stream);
	return TRUE;
}
