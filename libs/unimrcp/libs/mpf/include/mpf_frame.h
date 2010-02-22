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

#ifndef __MPF_FRAME_H__
#define __MPF_FRAME_H__

/**
 * @file mpf_frame.h
 * @brief MPF Audio/Video/Named-event Frame
 */ 

#include "mpf_codec_descriptor.h"
#include "mpf_named_event.h"

APT_BEGIN_EXTERN_C

/** Media frame types */
typedef enum {
	MEDIA_FRAME_TYPE_NONE  = 0x0, /**< none */
	MEDIA_FRAME_TYPE_AUDIO = 0x1, /**< audio frame */
	MEDIA_FRAME_TYPE_VIDEO = 0x2, /**< video frame */
	MEDIA_FRAME_TYPE_EVENT = 0x4  /**< named event frame (RFC4733/RFC2833) */
} mpf_frame_type_e;

/** Media frame marker */
typedef enum {
	MPF_MARKER_NONE,           /**< none */
	MPF_MARKER_START_OF_EVENT, /**< start of event */
	MPF_MARKER_END_OF_EVENT,   /**< end of event */
	MPF_MARKER_NEW_SEGMENT     /**< start of new segment (long-lasting events) */
} mpf_frame_marker_e;

/** Media frame declaration */
typedef struct mpf_frame_t mpf_frame_t;

/** Media frame */
struct mpf_frame_t {
	/** frame type (audio/video/named-event) mpf_frame_type_e */
	int                     type;
	/** frame marker (start-of-event,end-of-event) mpf_frame_marker_e */
	int                     marker;
	/** codec frame */
	mpf_codec_frame_t       codec_frame;
	/** named-event frame */
	mpf_named_event_frame_t event_frame;
};


APT_END_EXTERN_C

#endif /*__MPF_FRAME_H__*/
