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

#ifndef MPF_RTP_ATTRIBS_H
#define MPF_RTP_ATTRIBS_H

/**
 * @file mpf_rtp_attribs.h
 * @brief RTP Attributes (SDP)
 */ 

#include "mpf_rtp_descriptor.h"

APT_BEGIN_EXTERN_C

/** RTP attributes */
typedef enum {
	RTP_ATTRIB_RTPMAP,
	RTP_ATTRIB_SENDONLY,
	RTP_ATTRIB_RECVONLY,
	RTP_ATTRIB_SENDRECV,
	RTP_ATTRIB_MID,
	RTP_ATTRIB_PTIME,

	RTP_ATTRIB_COUNT,
	RTP_ATTRIB_UNKNOWN = RTP_ATTRIB_COUNT
} mpf_rtp_attrib_e;


/** Get audio media attribute name by attribute identifier */
MPF_DECLARE(const apt_str_t*) mpf_rtp_attrib_str_get(mpf_rtp_attrib_e attrib_id);

/** Find audio media attribute identifier by attribute name */
MPF_DECLARE(mpf_rtp_attrib_e) mpf_rtp_attrib_id_find(const apt_str_t *attrib);

/** Get string by RTP direction (send/receive) */
MPF_DECLARE(const apt_str_t*) mpf_rtp_direction_str_get(mpf_stream_direction_e direction);

APT_END_EXTERN_C

#endif /* MPF_RTP_ATTRIBS_H */
