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

#ifndef __MPF_RTP_PT_H__
#define __MPF_RTP_PT_H__

/**
 * @file mpf_rtp_pt.h
 * @brief RTP Payload Types (RFC3551)
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

/** RTP payload types */
typedef enum {
	RTP_PT_PCMU        =  0, /**< PCMU          Audio 8kHz 1  */
	RTP_PT_PCMA        =  8, /**< PCMA          Audio 8kHz 1  */

	RTP_PT_CN          =  13, /**< Comfort Noise Audio 8kHz 1  */

	RTP_PT_DYNAMIC     =  96, /**< Start of dynamic payload types */
	RTP_PT_DYNAMIC_MAX = 127, /**< End of dynamic payload types  */

	RTP_PT_UNKNOWN     = 128  /**< Unknown (invalid) payload type */
} mpf_rtp_pt_e;

APT_END_EXTERN_C

#endif /*__MPF_RTP_PT_H__*/
