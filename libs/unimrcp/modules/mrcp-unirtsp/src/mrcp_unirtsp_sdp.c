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
#include <apr_general.h>
#include <sofia-sip/sdp.h>
#include "rtsp_message.h"
#include "mrcp_unirtsp_sdp.h"
#include "mpf_rtp_attribs.h"
#include "apt_text_stream.h"
#include "apt_log.h"


/** Generate SDP media by RTP media descriptor */
static apr_size_t sdp_rtp_media_generate(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, const mpf_rtp_media_descriptor_t *audio_media)
{
	apr_size_t offset = 0;
	int i;
	mpf_codec_descriptor_t *codec_descriptor;
	apr_array_header_t *descriptor_arr = audio_media->codec_list.descriptor_arr;
	if(!descriptor_arr) {
		return 0;
	}
	offset += snprintf(buffer+offset,size-offset,
		"m=audio %d RTP/AVP", 
		audio_media->state == MPF_MEDIA_ENABLED ? audio_media->port : 0);
	for(i=0; i<descriptor_arr->nelts; i++) {
		codec_descriptor = &APR_ARRAY_IDX(descriptor_arr,i,mpf_codec_descriptor_t);
		if(codec_descriptor->enabled == TRUE) {
			offset += snprintf(buffer+offset,size-offset," %d", codec_descriptor->payload_type);
		}
	}
	offset += snprintf(buffer+offset,size-offset,"\r\n");
	if(audio_media->state == MPF_MEDIA_ENABLED) {
		const apt_str_t *direction_str = mpf_rtp_direction_str_get(audio_media->direction);
		for(i=0; i<descriptor_arr->nelts; i++) {
			codec_descriptor = &APR_ARRAY_IDX(descriptor_arr,i,mpf_codec_descriptor_t);
			if(codec_descriptor->enabled == TRUE && codec_descriptor->name.buf) {
				offset += snprintf(buffer+offset,size-offset,"a=rtpmap:%d %s/%d\r\n",
					codec_descriptor->payload_type,
					codec_descriptor->name.buf,
					codec_descriptor->sampling_rate);
				if(codec_descriptor->format.buf) {
					offset += snprintf(buffer+offset,size-offset,"a=fmtp:%d %s\r\n",
						codec_descriptor->payload_type,
						codec_descriptor->format.buf);
				}
			}
		}
		if(direction_str) {
			offset += snprintf(buffer+offset,size-offset,"a=%s\r\n",direction_str->buf);
		}
		
		if(audio_media->ptime) {
			offset += snprintf(buffer+offset,size-offset,"a=ptime:%hu\r\n",
				audio_media->ptime);
		}
	}
	return offset;
}

/** Generate RTP media descriptor by SDP media */
static apt_bool_t mpf_rtp_media_generate(mpf_rtp_media_descriptor_t *rtp_media, const sdp_media_t *sdp_media, const apt_str_t *ip, apr_pool_t *pool)
{
	mpf_rtp_attrib_e id;
	apt_str_t name;
	sdp_attribute_t *attrib = NULL;
	sdp_rtpmap_t *map;
	mpf_codec_descriptor_t *codec;
	for(attrib = sdp_media->m_attributes; attrib; attrib=attrib->a_next) {
		apt_string_set(&name,attrib->a_name);
		id = mpf_rtp_attrib_id_find(&name);
		switch(id) {
			case RTP_ATTRIB_PTIME:
				rtp_media->ptime = (apr_uint16_t)atoi(attrib->a_value);
				break;
			default:
				break;
		}
	}

	mpf_codec_list_init(&rtp_media->codec_list,5,pool);
	for(map = sdp_media->m_rtpmaps; map; map = map->rm_next) {
		codec = mpf_codec_list_add(&rtp_media->codec_list);
		if(codec) {
			codec->payload_type = (apr_byte_t)map->rm_pt;
			apt_string_assign(&codec->name,map->rm_encoding,pool);
			codec->sampling_rate = (apr_uint16_t)map->rm_rate;
			codec->channel_count = 1;
		}
	}

	switch(sdp_media->m_mode) {
		case sdp_inactive:
			rtp_media->direction = STREAM_DIRECTION_NONE;
			break;
		case sdp_sendonly:
			rtp_media->direction = STREAM_DIRECTION_SEND;
			break;
		case sdp_recvonly:
			rtp_media->direction = STREAM_DIRECTION_RECEIVE;
			break;
		case sdp_sendrecv:
			rtp_media->direction = STREAM_DIRECTION_DUPLEX;
			break;
	}

	if(sdp_media->m_connections) {
		apt_string_assign(&rtp_media->ip,sdp_media->m_connections->c_address,pool);
	}
	else {
		rtp_media->ip = *ip;
	}
	if(sdp_media->m_port) {
		rtp_media->port = (apr_port_t)sdp_media->m_port;
		rtp_media->state = MPF_MEDIA_ENABLED;
	}
	else {
		rtp_media->state = MPF_MEDIA_DISABLED;
	}
	return TRUE;
}

/** Generate MRCP descriptor by SDP session */
static mrcp_session_descriptor_t* mrcp_descriptor_generate_by_sdp_session(mrcp_session_descriptor_t *descriptor, const sdp_session_t *sdp, const char *force_destination_ip, apr_pool_t *pool)
{
	sdp_media_t *sdp_media;

	if(force_destination_ip) {
		apt_string_assign(&descriptor->ip,force_destination_ip,pool);
	}
	else if(sdp->sdp_connection) {
		apt_string_assign(&descriptor->ip,sdp->sdp_connection->c_address,pool);
	}

	for(sdp_media=sdp->sdp_media; sdp_media; sdp_media=sdp_media->m_next) {
		switch(sdp_media->m_type) {
			case sdp_media_audio:
			{
				mpf_rtp_media_descriptor_t *media = apr_palloc(pool,sizeof(mpf_rtp_media_descriptor_t));
				mpf_rtp_media_descriptor_init(media);
				media->id = mrcp_session_audio_media_add(descriptor,media);
				mpf_rtp_media_generate(media,sdp_media,&descriptor->ip,pool);
				break;
			}
			case sdp_media_video:
			{
				mpf_rtp_media_descriptor_t *media = apr_palloc(pool,sizeof(mpf_rtp_media_descriptor_t));
				mpf_rtp_media_descriptor_init(media);
				media->id = mrcp_session_video_media_add(descriptor,media);
				mpf_rtp_media_generate(media,sdp_media,&descriptor->ip,pool);
				break;
			}
			default:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Not Supported SDP Media [%s]", sdp_media->m_type_name);
				break;
		}
	}
	return descriptor;
}


/** Generate MRCP descriptor by RTSP request */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_descriptor_generate_by_rtsp_request(
											const rtsp_message_t *request,
											const char *force_destination_ip,
											const apr_table_t *resource_map,
											apr_pool_t *pool,
											su_home_t *home)
{
	mrcp_session_descriptor_t *descriptor = NULL;
	const char *resource_name = mrcp_name_get_by_rtsp_name(
		resource_map,
		request->start_line.common.request_line.resource_name);
	if(!resource_name) {
		return NULL;
	}
	
	if(request->start_line.common.request_line.method_id == RTSP_METHOD_SETUP) {
		if(rtsp_header_property_check(&request->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE) == TRUE &&
			rtsp_header_property_check(&request->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE &&
			request->body.buf) {
			
			sdp_parser_t *parser;
			sdp_session_t *sdp;

			parser = sdp_parse(home,request->body.buf,request->body.length,0);
			sdp = sdp_session(parser);
			if(sdp) {
				descriptor = mrcp_session_descriptor_create(pool);
				mrcp_descriptor_generate_by_sdp_session(descriptor,sdp,force_destination_ip,pool);
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse SDP Message");
			}
			sdp_parser_free(parser);
		}
		else {
			/* create default descriptor in case RTSP SETUP contains no SDP */
			mpf_rtp_media_descriptor_t *media;
			descriptor = mrcp_session_descriptor_create(pool);
			media = apr_palloc(pool,sizeof(mpf_rtp_media_descriptor_t));
			mpf_rtp_media_descriptor_init(media);
			media->state = MPF_MEDIA_ENABLED;
			media->id = mrcp_session_audio_media_add(descriptor,media);
			if(rtsp_header_property_check(&request->header.property_set,RTSP_HEADER_FIELD_TRANSPORT) == TRUE) {
				media->port = request->header.transport.client_port_range.min;
				media->ip = request->header.transport.destination;
			}
		}

		if(descriptor) {
			apt_string_assign(&descriptor->resource_name,resource_name,pool);
			descriptor->resource_state = TRUE;
		}
	}
	else if(request->start_line.common.request_line.method_id == RTSP_METHOD_TEARDOWN) {
		descriptor = mrcp_session_descriptor_create(pool);
		apt_string_assign(&descriptor->resource_name,resource_name,pool);
		descriptor->resource_state = FALSE;
	}
	return descriptor;
}

/** Generate MRCP descriptor by RTSP response */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_descriptor_generate_by_rtsp_response(
											const rtsp_message_t *request, 
											const rtsp_message_t *response, 
											const char *force_destination_ip,
											const apr_table_t *resource_map, 
											apr_pool_t *pool, 
											su_home_t *home)
{
	mrcp_session_descriptor_t *descriptor = NULL;
	const char *resource_name = mrcp_name_get_by_rtsp_name(
		resource_map,
		request->start_line.common.request_line.resource_name);
	if(!resource_name) {
		return NULL;
	}
	
	if(request->start_line.common.request_line.method_id == RTSP_METHOD_SETUP) {
		if(rtsp_header_property_check(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE) == TRUE &&
			rtsp_header_property_check(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE &&
			response->body.buf) {
			
			sdp_parser_t *parser;
			sdp_session_t *sdp;

			parser = sdp_parse(home,response->body.buf,response->body.length,0);
			sdp = sdp_session(parser);
			if(sdp) {
				descriptor = mrcp_session_descriptor_create(pool);
				mrcp_descriptor_generate_by_sdp_session(descriptor,sdp,force_destination_ip,pool);

				apt_string_assign(&descriptor->resource_name,resource_name,pool);
				descriptor->resource_state = TRUE;
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse SDP Message");
			}
			
			sdp_parser_free(parser);
		}
		else {
			descriptor = mrcp_session_descriptor_create(pool);
			apt_string_assign(&descriptor->resource_name,resource_name,pool);
			descriptor->resource_state = FALSE;
		}
	}
	else if(request->start_line.common.request_line.method_id == RTSP_METHOD_TEARDOWN) {
		descriptor = mrcp_session_descriptor_create(pool);
		apt_string_assign(&descriptor->resource_name,resource_name,pool);
		descriptor->resource_state = FALSE;
	}
	return descriptor;
}

/** Generate RTSP request by MRCP descriptor */
MRCP_DECLARE(rtsp_message_t*) rtsp_request_generate_by_mrcp_descriptor(const mrcp_session_descriptor_t *descriptor, const apr_table_t *resource_map, apr_pool_t *pool)
{
	apr_size_t i;
	apr_size_t count;
	apr_size_t audio_index = 0;
	mpf_rtp_media_descriptor_t *audio_media;
	apr_size_t video_index = 0;
	mpf_rtp_media_descriptor_t *video_media;
	apr_size_t offset = 0;
	char buffer[2048];
	apr_size_t size = sizeof(buffer);
	rtsp_message_t *request;
	const char *ip = descriptor->ext_ip.buf ? descriptor->ext_ip.buf : (descriptor->ip.buf ? descriptor->ip.buf : "0.0.0.0");

	request = rtsp_request_create(pool);
	request->start_line.common.request_line.resource_name = rtsp_name_get_by_mrcp_name(
		resource_map,
		descriptor->resource_name.buf);
	if(descriptor->resource_state != TRUE) {
		request->start_line.common.request_line.method_id = RTSP_METHOD_TEARDOWN;
		return request;
	}

	request->start_line.common.request_line.method_id = RTSP_METHOD_SETUP;

	buffer[0] = '\0';
	offset += snprintf(buffer+offset,size-offset,
			"v=0\r\n"
			"o=%s 0 0 IN IP4 %s\r\n"
			"s=-\r\n"
			"c=IN IP4 %s\r\n"
			"t=0 0\r\n",
			descriptor->origin.buf ? descriptor->origin.buf : "-",
			ip,
			ip);
	count = mrcp_session_media_count_get(descriptor);
	for(i=0; i<count; i++) {
		audio_media = mrcp_session_audio_media_get(descriptor,audio_index);
		if(audio_media && audio_media->id == i) {
			/* generate audio media */
			audio_index++;
			offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,audio_media);
			request->header.transport.client_port_range.min = audio_media->port;
			request->header.transport.client_port_range.max = audio_media->port+1;
			continue;
		}
		video_media = mrcp_session_video_media_get(descriptor,video_index);
		if(video_media && video_media->id == i) {
			/* generate video media */
			video_index++;
			offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,video_media);
			continue;
		}
	}

	request->header.transport.protocol = RTSP_TRANSPORT_RTP;
	request->header.transport.profile = RTSP_PROFILE_AVP;
	request->header.transport.delivery = RTSP_DELIVERY_UNICAST;
	rtsp_header_property_add(&request->header.property_set,RTSP_HEADER_FIELD_TRANSPORT);

	if(offset) {
		apt_string_assign_n(&request->body,buffer,offset,pool);
		request->header.content_type = RTSP_CONTENT_TYPE_SDP;
		rtsp_header_property_add(&request->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE);
		request->header.content_length = offset;
		rtsp_header_property_add(&request->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH);
	}
	return request;
}

/** Generate RTSP response by MRCP descriptor */
MRCP_DECLARE(rtsp_message_t*) rtsp_response_generate_by_mrcp_descriptor(const rtsp_message_t *request, const mrcp_session_descriptor_t *descriptor, const apr_table_t *resource_map, apr_pool_t *pool)
{
	rtsp_message_t *response = NULL;

	switch(descriptor->status) {
		case MRCP_SESSION_STATUS_OK:
			response = rtsp_response_create(request,RTSP_STATUS_CODE_OK,RTSP_REASON_PHRASE_OK,pool);
			break;
		case MRCP_SESSION_STATUS_NO_SUCH_RESOURCE:
			response = rtsp_response_create(request,RTSP_STATUS_CODE_NOT_FOUND,RTSP_REASON_PHRASE_NOT_FOUND,pool);
			break;
		case MRCP_SESSION_STATUS_UNACCEPTABLE_RESOURCE:
		case MRCP_SESSION_STATUS_UNAVAILABLE_RESOURCE:
			response = rtsp_response_create(request,RTSP_STATUS_CODE_NOT_ACCEPTABLE,RTSP_REASON_PHRASE_NOT_ACCEPTABLE,pool);
			break;
		case MRCP_SESSION_STATUS_ERROR:
			response = rtsp_response_create(request,RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR,RTSP_REASON_PHRASE_INTERNAL_SERVER_ERROR,pool);
			break;
	}

	if(!response) {
		return NULL;
	}

	if(descriptor->status == MRCP_SESSION_STATUS_OK) {
		apr_size_t i;
		apr_size_t count;
		apr_size_t audio_index = 0;
		mpf_rtp_media_descriptor_t *audio_media;
		apr_size_t video_index = 0;
		mpf_rtp_media_descriptor_t *video_media;
		apr_size_t offset = 0;
		char buffer[2048];
		apr_size_t size = sizeof(buffer);
		const char *ip = descriptor->ext_ip.buf ? descriptor->ext_ip.buf : (descriptor->ip.buf ? descriptor->ip.buf : "0.0.0.0");

		buffer[0] = '\0';
		offset += snprintf(buffer+offset,size-offset,
				"v=0\r\n"
				"o=%s 0 0 IN IP4 %s\r\n"
				"s=-\r\n"
				"c=IN IP4 %s\r\n"
				"t=0 0\r\n",
				descriptor->origin.buf ? descriptor->origin.buf : "-",
				ip,
				ip);
		count = mrcp_session_media_count_get(descriptor);
		for(i=0; i<count; i++) {
			audio_media = mrcp_session_audio_media_get(descriptor,audio_index);
			if(audio_media && audio_media->id == i) {
				/* generate audio media */
				rtsp_transport_t *transport;
				audio_index++;
				offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,audio_media);
				transport = &response->header.transport;
				transport->server_port_range.min = audio_media->port;
				transport->server_port_range.max = audio_media->port+1;
				transport->client_port_range = request->header.transport.client_port_range;
				continue;
			}
			video_media = mrcp_session_video_media_get(descriptor,video_index);
			if(video_media && video_media->id == i) {
				/* generate video media */
				video_index++;
				offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,video_media);
				continue;
			}
		}

		/* ok */
		response->header.transport.protocol = RTSP_TRANSPORT_RTP;
		response->header.transport.profile = RTSP_PROFILE_AVP;
		response->header.transport.delivery = RTSP_DELIVERY_UNICAST;
		rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_TRANSPORT);

		if(offset) {
			apt_string_assign_n(&response->body,buffer,offset,pool);
			response->header.content_type = RTSP_CONTENT_TYPE_SDP;
			rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE);
			response->header.content_length = offset;
			rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH);
		}
	}
	return response;
}

/** Generate RTSP resource discovery request */
MRCP_DECLARE(rtsp_message_t*) rtsp_resource_discovery_request_generate(
											const char *resource_name,
											const apr_table_t *resource_map,
											apr_pool_t *pool)
{
	rtsp_message_t *request = rtsp_request_create(pool);
	request->start_line.common.request_line.resource_name = rtsp_name_get_by_mrcp_name(
		resource_map,
		resource_name);
	
	request->start_line.common.request_line.method_id = RTSP_METHOD_DESCRIBE;
	return request;
}

/** Generate resource discovery descriptor by RTSP response */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_resource_discovery_response_generate(
											const rtsp_message_t *request, 
											const rtsp_message_t *response,
											const apr_table_t *resource_map,
											apr_pool_t *pool,
											su_home_t *home)
{
	mrcp_session_descriptor_t *descriptor = NULL;
	const char *resource_name = mrcp_name_get_by_rtsp_name(
					resource_map,
					request->start_line.common.request_line.resource_name);
	if(!resource_name) {
		return NULL;
	}
	
	descriptor = mrcp_session_descriptor_create(pool);
	apt_string_assign(&descriptor->resource_name,resource_name,pool);
	
	if(rtsp_header_property_check(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE) == TRUE &&
		rtsp_header_property_check(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE &&
		response->body.buf) {
			
		sdp_parser_t *parser;
		sdp_session_t *sdp;

		parser = sdp_parse(home,response->body.buf,response->body.length,0);
		sdp = sdp_session(parser);
		if(sdp) {
			mrcp_descriptor_generate_by_sdp_session(descriptor,sdp,0,pool);
			descriptor->resource_state = TRUE;
		}
		else {
			apt_string_assign(&descriptor->resource_name,resource_name,pool);
			descriptor->resource_state = TRUE;
		}

		sdp_parser_free(parser);
	}
	else {
		descriptor->resource_state = FALSE;
	}
	return descriptor;
}

/** Generate RTSP resource discovery response */
MRCP_DECLARE(rtsp_message_t*) rtsp_resource_discovery_response_generate(
											const rtsp_message_t *request, 
											const char *ip,
											const char *origin,
											apr_pool_t *pool)
{
	rtsp_message_t *response = rtsp_response_create(request,RTSP_STATUS_CODE_OK,RTSP_REASON_PHRASE_OK,pool);
	if(response) {
		apr_size_t offset = 0;
		char buffer[2048];
		apr_size_t size = sizeof(buffer);
		
		if(!ip) {
			ip = "0.0.0.0";
		}
		if(!origin) {
			origin = "-";
		}
		
		buffer[0] = '\0';
		offset += snprintf(buffer+offset,size-offset,
			"v=0\r\n"
			"o=%s 0 0 IN IP4 %s\r\n"
			"s=-\r\n"
			"c=IN IP4 %s\r\n"
			"t=0 0\r\n"
			"m=audio 0 RTP/AVP 0 8 96 101\r\n"
			"a=rtpmap:0 PCMU/8000\r\n"
			"a=rtpmap:8 PCMA/8000\r\n"
			"a=rtpmap:96 L16/8000\r\n"
			"a=rtpmap:101 telephone-event/8000\r\n",
			origin,
			ip,
			ip);
		
		if(offset) {
			apt_string_assign_n(&response->body,buffer,offset,pool);
			response->header.content_type = RTSP_CONTENT_TYPE_SDP;
			rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE);
			response->header.content_length = offset;
			rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH);
		}
	}

	return response;
}

/** Get MRCP resource name by RTSP resource name */
MRCP_DECLARE(const char*) mrcp_name_get_by_rtsp_name(const apr_table_t *resource_map, const char *rtsp_name)
{
	const apr_array_header_t *header = apr_table_elts(resource_map);
	apr_table_entry_t *entry = (apr_table_entry_t *)header->elts;
	int i;
	
	for(i=0; i<header->nelts; i++) {
		if(entry[i].val && rtsp_name) {
			if(apr_strnatcasecmp(entry[i].val,rtsp_name) == 0) {
				return entry[i].key;
			}
		}
	}
	return rtsp_name;
}

/** Get RTSP resource name by MRCP resource name */
MRCP_DECLARE(const char*) rtsp_name_get_by_mrcp_name(const apr_table_t *resource_map, const char *mrcp_name)
{
	const char *rtsp_name = apr_table_get(resource_map,mrcp_name);
	if(rtsp_name) {
		return rtsp_name;
	}
	return mrcp_name;
}
