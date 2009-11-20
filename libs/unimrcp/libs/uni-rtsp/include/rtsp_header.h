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

#ifndef __RTSP_HEADER_H__
#define __RTSP_HEADER_H__

/**
 * @file rtsp_header.h
 * @brief RTSP Header
 */ 

#include "rtsp.h"
#include "apt_text_stream.h"

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


/** Bit field masks are used to define property set */
typedef int rtsp_header_property_t;


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

	/** Property set */
	rtsp_header_property_t property_set;
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
static APR_INLINE void rtsp_header_init(rtsp_header_t *header)
{
	header->cseq = 0;
	rtsp_transport_init(&header->transport);
	apt_string_reset(&header->session_id);
	apt_string_reset(&header->rtp_info);
	header->content_type = RTSP_CONTENT_TYPE_NONE;
	header->content_length = 0;
	header->property_set = 0;
}

/** Parse RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_header_parse(rtsp_header_t *header, apt_text_stream_t *text_stream, apr_pool_t *pool);
/** Generate RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_header_generate(rtsp_header_t *header, apt_text_stream_t *text_stream);



/** Add property to property set */
static APR_INLINE void rtsp_header_property_add(rtsp_header_property_t *property_set, apr_size_t id)
{
	int mask = 1 << id;
	*property_set |= mask;
}

/** Remove property from property set */
static APR_INLINE void rtsp_header_property_remove(rtsp_header_property_t *property_set, apr_size_t id)
{
	int mask = 1 << id;
	*property_set &= ~mask;
}

/** Check property in property set */
static APR_INLINE apt_bool_t rtsp_header_property_check(const rtsp_header_property_t *property_set, apr_size_t id)
{
	int mask = 1 << id;
	return ((*property_set & mask) == mask) ? TRUE : FALSE;
}

APT_END_EXTERN_C

#endif /*__RTSP_HEADER_H__*/
