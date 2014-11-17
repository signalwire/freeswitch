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
 * $Id: mpf_named_event.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MPF_NAMED_EVENT_H
#define MPF_NAMED_EVENT_H

/**
 * @file mpf_named_event.h
 * @brief MPF Named Events (RFC4733/RFC2833)
 */ 

#include "mpf_codec_descriptor.h"

APT_BEGIN_EXTERN_C

/** Named event declaration */
typedef struct mpf_named_event_frame_t mpf_named_event_frame_t;


/** Named event (RFC4733/RFC2833, out-of-band DTMF) */
struct mpf_named_event_frame_t {
	/** event (DTMF, tone) identifier */
	apr_uint32_t event_id: 8;
#if (APR_IS_BIGENDIAN == 1)
	/** end of event */
	apr_uint32_t edge:     1;
	/** reserved */
	apr_uint32_t reserved: 1;
	/** tone volume */
	apr_uint32_t volume:   6;
#else
	/** tone volume */
	apr_uint32_t volume:   6;
	/** reserved */
	apr_uint32_t reserved: 1;
	/** end of event */
	apr_uint32_t edge:     1;
#endif
	/** event duration */
	apr_uint32_t duration: 16;
};

/** Create named event descriptor */
MPF_DECLARE(mpf_codec_descriptor_t*) mpf_event_descriptor_create(apr_uint16_t sampling_rate, apr_pool_t *pool);

/** Check whether the specified descriptor is named event one */
MPF_DECLARE(apt_bool_t) mpf_event_descriptor_check(const mpf_codec_descriptor_t *descriptor);

/** Convert DTMF character to event identifier */
MPF_DECLARE(apr_uint32_t) mpf_dtmf_char_to_event_id(const char dtmf_char);

/** Convert event identifier to DTMF character */
MPF_DECLARE(char) mpf_event_id_to_dtmf_char(const apr_uint32_t event_id);


APT_END_EXTERN_C

#endif /* MPF_NAMED_EVENT_H */
