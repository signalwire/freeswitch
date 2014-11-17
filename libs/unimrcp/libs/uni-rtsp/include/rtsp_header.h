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
 * $Id: rtsp_header.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef RTSP_HEADER_H
#define RTSP_HEADER_H

/**
 * @file rtsp_header.h
 * @brief RTSP Header
 */ 

#include "rtsp.h"
#include "apt_header_field.h"

APT_BEGIN_EXTERN_C

/** RTSP transport protocol */
typedef enum{
	RTSP_TRANSPORT_RTP,

	RTSP_TRANSPORT_COUNT,
	RTSP_TRANSPORT_NONE = RTSP_TRANSPORT_COUNT
} rtsp_transport_e;

/** RTSP transport profile */
typedef enum{
	RTSP_PROFILE_AVP,
	RTSP_PROFILE_SAVP,

	RTSP_PROFILE_COUNT,
	RTSP_PROFILE_NONE = RTSP_PROFILE_COUNT
} rtsp_profile_e;

/** RTSP lower-transport */
typedef enum{
	RTSP_LOWER_TRANSPORT_UDP,
	RTSP_LOWER_TRANSPORT_TCP,

	RTSP_LOWER_TRANSPORT_COUNT,
	RTSP_LOWER_TRANSPORT_NONE = RTSP_LOWER_TRANSPORT_COUNT
} rtsp_lower_transport_e;

/** RTSP transport attributes */
typedef enum{
	RTSP_TRANSPORT_ATTRIB_CLIENT_PORT,
	RTSP_TRANSPORT_ATTRIB_SERVER_PORT,
	RTSP_TRANSPORT_ATTRIB_SOURCE,
	RTSP_TRANSPORT_ATTRIB_DESTINATION,
	RTSP_TRANSPORT_ATTRIB_UNICAST,
	RTSP_TRANSPORT_ATTRIB_MULTICAST,
	RTSP_TRANSPORT_ATTRIB_MODE,

	RTSP_TRANSPORT_ATTRIB_COUNT,
	RTSP_TRANSPORT_ATTRIB_NONE = RTSP_TRANSPORT_ATTRIB_COUNT
} rtsp_transport_attrib_e;

/** RTSP delivery */
typedef enum{
	RTSP_DELIVERY_UNICAST,
	RTSP_DELIVERY_MULTICAST,

	RTSP_DELIVERY_COUNT,
	RTSP_DELIVERY_NONE = RTSP_DELIVERY_COUNT
} rtsp_delivery_e;

/** RTSP header fields */
typedef enum{
	RTSP_HEADER_FIELD_CSEQ,
	RTSP_HEADER_FIELD_TRANSPORT,
	RTSP_HEADER_FIELD_SESSION_ID,
	RTSP_HEADER_FIELD_RTP_INFO,
	RTSP_HEADER_FIELD_CONTENT_TYPE,
	RTSP_HEADER_FIELD_CONTENT_LENGTH,

	RTSP_HEADER_FIELD_COUNT,
	RTSP_HEADER_FIELD_UNKNOWN = RTSP_HEADER_FIELD_COUNT
} rtsp_header_field_id;

/** RTSP content types */
typedef enum {
	RTSP_CONTENT_TYPE_SDP,
	RTSP_CONTENT_TYPE_MRCP,

	RTSP_CONTENT_TYPE_COUNT,
	RTSP_CONTENT_TYPE_NONE = RTSP_CONTENT_TYPE_COUNT
} rtsp_content_type_e;



/** RTSP/RTP port range declaration */
typedef struct rtsp_port_range_t rtsp_port_range_t;
/** RTSP transport declaration */
typedef struct rtsp_transport_t rtsp_transport_t;
/** RTSP header declaration */
typedef struct rtsp_header_t rtsp_header_t;

/** RTSP/RTP port range */
struct rtsp_port_range_t {
	/** Min (low) port */
	apr_port_t min;
	/** Max (high) port */
	apr_port_t max;
};

/** RTSP transport */
struct rtsp_transport_t {
	/** Transport profile */
	rtsp_transport_e         protocol;
	/** Transport profile */
	rtsp_profile_e           profile;
	/** Lower transport */
	rtsp_lower_transport_e   lower_protocol;
	/** Delivery method */
	rtsp_delivery_e          delivery;
	/** Client port range */
	rtsp_port_range_t        client_port_range;
	/** Server port range */
	rtsp_port_range_t        server_port_range;
	/** Source IP address */
	apt_str_t                source;
	/** Destination IP address */
	apt_str_t                destination;
	/** Mode indicates the method to support (either PLAY or RECORD) */
	apt_str_t                mode;
};

/** RTSP header */
struct rtsp_header_t {
	/** Sequence number */
	apr_size_t             cseq;
	/** Transport */
	rtsp_transport_t       transport; 
	/** Session identifier */
	apt_str_t              session_id;
	/** RTP-info */
	apt_str_t              rtp_info;

	/** Content type */
	rtsp_content_type_e    content_type;
	/** Content length */
	apr_size_t             content_length;

	/** Header section (collection of header fields)*/
	apt_header_section_t   header_section;
};


/** Initialize port range */
static APR_INLINE void rtsp_port_range_init(rtsp_port_range_t *port_range)
{
	port_range->min = 0;
	port_range->max = 0;
}

/** Initialize port range */
static APR_INLINE apt_bool_t rtsp_port_range_is_valid(const rtsp_port_range_t *port_range)
{
	return (port_range->min == 0 && port_range->max == 0) == FALSE;
}

/** Initialize transport */
static APR_INLINE void rtsp_transport_init(rtsp_transport_t *transport)
{
	transport->protocol = RTSP_TRANSPORT_RTP;
	transport->profile = RTSP_PROFILE_NONE;
	transport->lower_protocol = RTSP_LOWER_TRANSPORT_NONE;
	rtsp_port_range_init(&transport->client_port_range);
	rtsp_port_range_init(&transport->server_port_range);
	apt_string_reset(&transport->source);
	apt_string_reset(&transport->destination);
	apt_string_reset(&transport->mode);
}

/** Initialize header */
static APR_INLINE void rtsp_header_init(rtsp_header_t *header, apr_pool_t *pool)
{
	header->cseq = 0;
	rtsp_transport_init(&header->transport);
	apt_string_reset(&header->session_id);
	apt_string_reset(&header->rtp_info);
	header->content_type = RTSP_CONTENT_TYPE_NONE;
	header->content_length = 0;

	apt_header_section_init(&header->header_section);
	apt_header_section_array_alloc(&header->header_section,RTSP_HEADER_FIELD_COUNT,pool);
}


/** Add RTSP header field */
RTSP_DECLARE(apt_bool_t) rtsp_header_field_add(rtsp_header_t *header, apt_header_field_t *header_field, apr_pool_t *pool);

/** Parse RTSP header fields */
RTSP_DECLARE(apt_bool_t) rtsp_header_fields_parse(rtsp_header_t *header, apr_pool_t *pool);

/** Add RTSP header field property */
RTSP_DECLARE(apt_bool_t) rtsp_header_property_add(rtsp_header_t *header, rtsp_header_field_id id, apr_pool_t *pool);

/** Remove RTSP header field property */
static APR_INLINE apt_bool_t rtsp_header_property_remove(rtsp_header_t *header, rtsp_header_field_id id)
{
	apt_header_field_t *header_field = apt_header_section_field_get(&header->header_section,id);
	if(header_field) {
		return apt_header_section_field_remove(&header->header_section,header_field);
	}
	return FALSE;
}

/** Check RTSP header field property */
static APR_INLINE apt_bool_t rtsp_header_property_check(const rtsp_header_t *header, rtsp_header_field_id id)
{
	return apt_header_section_field_check(&header->header_section,id);
}


APT_END_EXTERN_C

#endif /* RTSP_HEADER_H */
