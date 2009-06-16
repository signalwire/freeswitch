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

#ifndef __MPF_STREAM_MODE_H__
#define __MPF_STREAM_MODE_H__

/**
 * @file mpf_stream_mode.h
 * @brief MPF Stream Mode (Send/Receive)
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

/** Enumeration of stream modes */
typedef enum {
	STREAM_MODE_NONE    = 0x0, /**< none */
	STREAM_MODE_SEND    = 0x1, /**< send */
	STREAM_MODE_RECEIVE = 0x2, /**< receive */

	STREAM_MODE_SEND_RECEIVE = STREAM_MODE_SEND | STREAM_MODE_RECEIVE /**< send and receive */
} mpf_stream_mode_e; 

static APR_INLINE mpf_stream_mode_e mpf_stream_mode_negotiate(mpf_stream_mode_e remote_mode)
{
	mpf_stream_mode_e local_mode = remote_mode;
	if(local_mode == STREAM_MODE_SEND) {
		local_mode = STREAM_MODE_RECEIVE;
	}
	else if(local_mode == STREAM_MODE_RECEIVE) {
		local_mode = STREAM_MODE_SEND;
	}
	return local_mode;
}


APT_END_EXTERN_C

#endif /*__MPF_STREAM_MODE_H__*/
