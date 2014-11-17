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
 * $Id: mpf_rtp_header.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MPF_RTP_HEADER_H
#define MPF_RTP_HEADER_H

/**
 * @file mpf_rtp_header.h
 * @brief RTP Header Definition
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

/** Protocol version */
#define RTP_VERSION 2

/** RTP header declaration */
typedef struct rtp_header_t rtp_header_t;
/** RTP extension header declaration */
typedef struct rtp_extension_header_t rtp_extension_header_t;


/** RTP header */
struct rtp_header_t {
#if (APR_IS_BIGENDIAN == 1)
	/** protocol version */
	apr_uint32_t version:   2;
	/** padding flag */
	apr_uint32_t padding:   1;
	/** header extension flag */
	apr_uint32_t extension: 1;
	/** CSRC count */
	apr_uint32_t count:     4;
	/** marker bit */
	apr_uint32_t marker:    1;
	/** payload type */
	apr_uint32_t type:      7;
#else
	/** CSRC count */
	apr_uint32_t count:     4;
	/** header extension flag */
	apr_uint32_t extension: 1;
	/** padding flag */
	apr_uint32_t padding:   1;
	/** protocol version */
	apr_uint32_t version:   2;
	/** payload type */
	apr_uint32_t type:      7;
	/** marker bit */
	apr_uint32_t marker:    1;
#endif	
	
	/** sequence number */
	apr_uint32_t sequence:  16;
	/** timestamp */
	apr_uint32_t timestamp;
	/** synchronization source */
	apr_uint32_t ssrc;
};

/** RTP extension header */
struct rtp_extension_header_t {
	/** profile */
	apr_uint16_t profile;
	/** length */
	apr_uint16_t length;
};

APT_END_EXTERN_C

#endif /* MPF_RTP_HEADER_H */
