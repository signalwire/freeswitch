/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id: mrcp_sdp.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <stdlib.h>
#include <apr_general.h>
#include <sofia-sip/sdp.h>
#include "mrcp_sdp.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_control_descriptor.h"
#include "mpf_rtp_attribs.h"
#include "mpf_rtp_pt.h"
#include "apt_text_stream.h"
#include "apt_log.h"

#if _MSC_VER >= 1900
#pragma warning(disable: 4477)
// 'snprintf' : format string '%d' requires an argument of type 'int', but variadic argument 1 has type 'apr_size_t' 264
// 'snprintf' : format string '%d' requires an argument of type 'int', but variadic argument 1 has type 'const apr_size_t' 198
#endif

static apr_size_t sdp_rtp_media_generate(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, const mpf_rtp_media_descriptor_t *audio_descriptor);
static apr_size_t sdp_control_media_generate(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, const mrcp_control_descriptor_t *control_media, apt_bool_t offer);

static apt_bool_t mpf_rtp_media_generate(mpf_rtp_media_descriptor_t *rtp_media, const sdp_media_t *sdp_media, const apt_str_t *ip, apr_pool_t *pool);
static apt_bool_t mrcp_control_media_generate(mrcp_control_descriptor_t *mrcp_media, const sdp_media_t *sdp_media, const apt_str_t *ip, apr_pool_t *pool);

/** Generate SDP string by MRCP descriptor */
MRCP_DECLARE(apr_size_t) sdp_string_generate_by_mrcp_descriptor(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, apt_bool_t offer)
{
	apr_size_t i;
	apr_size_t count;
	apr_size_t audio_index = 0;
	mpf_rtp_media_descriptor_t *audio_media;
	apr_size_t video_index = 0;
	mpf_rtp_media_descriptor_t *video_media;
	apr_size_t control_index = 0;
	mrcp_control_descriptor_t *control_media;
	apr_size_t offset = 0;
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
			audio_index++;
			offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,audio_media);
			continue;
		}
		video_media = mrcp_session_video_media_get(descriptor,video_index);
		if(video_media && video_media->id == i) {
			/* generate video media */
			video_index++;
			offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,video_media);
			continue;
		}
		control_media = mrcp_session_control_media_get(descriptor,control_index);
		if(control_media && control_media->id == i) {
			/** generate mrcp control media */
			control_index++;
			offset += sdp_control_media_generate(buffer+offset,size-offset,descriptor,control_media,offer);
			continue;
		}
	}
	return offset;
}

/** Generate MRCP descriptor by SDP session */
MRCP_DECLARE(apt_bool_t) mrcp_descriptor_generate_by_sdp_session(mrcp_session_descriptor_t* descriptor, const sdp_session_t *sdp, const char *force_destination_ip, apr_pool_t *pool)
{
	sdp_media_t *sdp_media;

	if(!sdp) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid SDP Message");
		return FALSE;
	}
	
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
			case sdp_media_application:
			{
				mrcp_control_descriptor_t *control_media = mrcp_control_descriptor_create(pool);
				control_media->id = mrcp_session_control_media_add(descriptor,control_media);
				mrcp_control_media_generate(control_media,sdp_media,&descriptor->ip,pool);
				break;
			}
			default:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Not Supported SDP Media [%s]", sdp_media->m_type_name);
				break;
		}
	}
	return TRUE;
}

/** Generate SDP media by RTP media descriptor */
static apr_size_t sdp_rtp_media_generate(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, const mpf_rtp_media_descriptor_t *audio_media)
{
	apr_size_t offset = 0;
	if(audio_media->state == MPF_MEDIA_ENABLED) {
		int codec_count = 0;
		int i;
		mpf_codec_descriptor_t *codec_descriptor;
		apr_array_header_t *descriptor_arr = audio_media->codec_list.descriptor_arr;
		const apt_str_t *direction_str;
		if(!descriptor_arr) {
			return 0;
		}

		offset += snprintf(buffer+offset,size-offset,"m=audio %d RTP/AVP",audio_media->port);
		for(i=0; i<descriptor_arr->nelts; i++) {
			codec_descriptor = &APR_ARRAY_IDX(descriptor_arr,i,mpf_codec_descriptor_t);
			if(codec_descriptor->enabled == TRUE) {
				offset += snprintf(buffer+offset,size-offset," %d",codec_descriptor->payload_type);
				codec_count++;
			}
		}
		if(!codec_count){
			/* SDP m line should have at least one media format listed; use a reserved RTP payload type */
			offset += snprintf(buffer+offset,size-offset," %d",RTP_PT_RESERVED);
		}
		offset += snprintf(buffer+offset,size-offset,"\r\n");
		
		if(descriptor->ip.length && audio_media->ip.length && 
			apt_string_compare(&descriptor->ip,&audio_media->ip) != TRUE) {
			const char *media_ip = audio_media->ext_ip.buf ? audio_media->ext_ip.buf : audio_media->ip.buf;
			offset += snprintf(buffer+offset,size-offset,"c=IN IP4 %s\r\n",media_ip);
		}
		
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
		
		direction_str = mpf_rtp_direction_str_get(audio_media->direction);
		if(direction_str) {
			offset += snprintf(buffer+offset,size-offset,"a=%s\r\n",direction_str->buf);
		}
		
		if(audio_media->ptime) {
			offset += snprintf(buffer+offset,size-offset,"a=ptime:%hu\r\n",audio_media->ptime);
		}
	}
	else {
		offset += snprintf(buffer+offset,size-offset,"m=audio 0 RTP/AVP %d\r\n",RTP_PT_RESERVED);
	}

	offset += snprintf(buffer+offset,size-offset,"a=mid:%"APR_SIZE_T_FMT"\r\n",audio_media->mid);
	return offset;
}

/** Generate SDP media by MRCP control media descriptor */
static apr_size_t sdp_control_media_generate(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, const mrcp_control_descriptor_t *control_media, apt_bool_t offer)
{
	int i;
	apr_size_t offset = 0;
	const apt_str_t *proto;
	const apt_str_t *setup_type;
	const apt_str_t *connection_type;
	proto = mrcp_proto_get(control_media->proto);
	setup_type = mrcp_setup_type_get(control_media->setup_type);
	connection_type = mrcp_connection_type_get(control_media->connection_type);
	if(offer == TRUE) { /* offer */
		if(control_media->port) {
			offset += snprintf(buffer+offset,size-offset,
				"m=application %d %s 1\r\n"
				"a=setup:%s\r\n"
				"a=connection:%s\r\n"
				"a=resource:%s\r\n",
				control_media->port,
				proto ? proto->buf : "",
				setup_type ? setup_type->buf : "",
				connection_type ? connection_type->buf : "",
				control_media->resource_name.buf);

		}
		else {
			offset += snprintf(buffer+offset,size-offset,
				"m=application %d %s 1\r\n"
				"a=resource:%s\r\n",
				control_media->port,
				proto ? proto->buf : "",
				control_media->resource_name.buf);
		}
	}
	else { /* answer */
		if(control_media->port) {
			offset += snprintf(buffer+offset,size-offset,
				"m=application %d %s 1\r\n"
				"a=setup:%s\r\n"
				"a=connection:%s\r\n"
				"a=channel:%s@%s\r\n",
				control_media->port,
				proto ? proto->buf : "",
				setup_type ? setup_type->buf : "",
				connection_type ? connection_type->buf : "",
				control_media->session_id.buf,
				control_media->resource_name.buf);
		}
		else {
			offset += snprintf(buffer+offset,size-offset,
				"m=application %d %s 1\r\n"
				"a=channel:%s@%s\r\n",
				control_media->port,
				proto ? proto->buf : "",
				control_media->session_id.buf,
				control_media->resource_name.buf);
		}
	}

	for(i=0; i<control_media->cmid_arr->nelts; i++) {
		offset += snprintf(buffer+offset,size-offset,
			"a=cmid:%"APR_SIZE_T_FMT"\r\n",
			APR_ARRAY_IDX(control_media->cmid_arr,i,apr_size_t));
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
			case RTP_ATTRIB_MID:
				rtp_media->mid = atoi(attrib->a_value);
				break;
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

/** Generate MRCP control media by SDP media */
static apt_bool_t mrcp_control_media_generate(mrcp_control_descriptor_t *control_media, const sdp_media_t *sdp_media, const apt_str_t *ip, apr_pool_t *pool)
{
	mrcp_attrib_e id;
	apt_str_t name;
	apt_str_t value;
	sdp_attribute_t *attrib = NULL;
	apt_string_set(&name,sdp_media->m_proto_name);
	control_media->proto = mrcp_proto_find(&name);
	if(control_media->proto != MRCP_PROTO_TCP) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Not supported SDP Proto [%s], expected [%s]",sdp_media->m_proto_name,mrcp_proto_get(MRCP_PROTO_TCP)->buf);
		return FALSE;
	}
	
	for(attrib = sdp_media->m_attributes; attrib; attrib=attrib->a_next) {
		apt_string_set(&name,attrib->a_name);
		id = mrcp_attrib_id_find(&name);
		switch(id) {
			case MRCP_ATTRIB_SETUP:
				apt_string_set(&value,attrib->a_value);
				control_media->setup_type = mrcp_setup_type_find(&value);
				break;
			case MRCP_ATTRIB_CONNECTION:
				apt_string_set(&value,attrib->a_value);
				control_media->connection_type = mrcp_connection_type_find(&value);
				break;
			case MRCP_ATTRIB_RESOURCE:
				apt_string_assign(&control_media->resource_name,attrib->a_value,pool);
				break;
			case MRCP_ATTRIB_CHANNEL:
				apt_string_set(&value,attrib->a_value);
				apt_id_resource_parse(&value,'@',&control_media->session_id,&control_media->resource_name,pool);
				break;
			case MRCP_ATTRIB_CMID:
				mrcp_cmid_add(control_media->cmid_arr,atoi(attrib->a_value));
				break;
			default:
				break;
		}
	}

	if(sdp_media->m_connections) {
		apt_string_assign(&control_media->ip,sdp_media->m_connections->c_address,pool);
	}
	else {
		control_media->ip = *ip;
	}
	control_media->port = (apr_port_t)sdp_media->m_port;
	return TRUE;
}

/** Generate SDP resource discovery string */
MRCP_DECLARE(apr_size_t) sdp_resource_discovery_string_generate(const char *ip, const char *origin, char *buffer, apr_size_t size)
{
	apr_size_t offset = 0;
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
			"m=application 0 TCP/MRCPv2 1\r\n"
			"a=resource:speechsynth\r\n"
			"a=resource:speechrecog\r\n"
			"m=audio 0 RTP/AVP 0 8 96 101\r\n"
			"a=rtpmap:0 PCMU/8000\r\n"
			"a=rtpmap:8 PCMA/8000\r\n"
			"a=rtpmap:96 L16/8000\r\n"
			"a=rtpmap:101 telephone-event/8000\r\n",
			origin,
			ip,
			ip);
	return offset;
}
