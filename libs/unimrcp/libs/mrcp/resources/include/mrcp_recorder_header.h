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

#ifndef MRCP_RECORDER_HEADER_H
#define MRCP_RECORDER_HEADER_H

/**
 * @file mrcp_recorder_header.h
 * @brief MRCP Recorder Header
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C

/** MRCP recorder header fields */
typedef enum {
	RECORDER_HEADER_SENSITIVITY_LEVEL,
	RECORDER_HEADER_NO_INPUT_TIMEOUT,
	RECORDER_HEADER_COMPLETION_CAUSE,
	RECORDER_HEADER_COMPLETION_REASON,
	RECORDER_HEADER_FAILED_URI,
	RECORDER_HEADER_FAILED_URI_CAUSE,
	RECORDER_HEADER_RECORD_URI,
	RECORDER_HEADER_MEDIA_TYPE,
	RECORDER_HEADER_MAX_TIME,
	RECORDER_HEADER_TRIM_LENGTH,
	RECORDER_HEADER_FINAL_SILENCE,
	RECORDER_HEADER_CAPTURE_ON_SPEECH,
	RECORDER_HEADER_VER_BUFFER_UTTERANCE,
	RECORDER_HEADER_START_INPUT_TIMERS,
	RECORDER_HEADER_NEW_AUDIO_CHANNEL,

	RECORDER_HEADER_COUNT
} mrcp_recorder_header_id;


/** MRCP recorder completion-cause  */
typedef enum {
	RECORDER_COMPLETION_CAUSE_SUCCESS_SILENCE         = 0,
	RECORDER_COMPLETION_CAUSE_SUCCESS_MAXTIME         = 1,
	RECORDER_COMPLETION_CAUSE_NO_INPUT_TIMEOUT        = 2,
	RECORDER_COMPLETION_CAUSE_URI_FAILURE             = 3,
	RECORDER_COMPLETION_CAUSE_ERROR                   = 4,

	RECORDER_COMPLETION_CAUSE_COUNT                   = 5,
	RECORDER_COMPLETION_CAUSE_UNKNOWN                 = RECORDER_COMPLETION_CAUSE_COUNT
} mrcp_recorder_completion_cause_e;



/** MRCP recorder-header declaration */
typedef struct mrcp_recorder_header_t mrcp_recorder_header_t;

/** MRCP recorder-header */
struct mrcp_recorder_header_t {
	/** To filter out background noise and not mistake it for speech */
	float                            sensitivity_level;
	/** When recording is started and there is no speech detected for a
	certain period of time, the recorder can send a RECORD-COMPLETE event */
	apr_size_t                       no_input_timeout;
	/** MUST be part of a RECORD-COMPLETE event coming from the 
	recorder resource to the client */
	mrcp_recorder_completion_cause_e completion_cause;
	/** MAY be specified in a RECORD-COMPLETE event coming from
	the recorder resource to the client */
	apt_str_t                        completion_reason;
	/** When a recorder method needs to post the audio to a URI and access to
	the URI fails, the server MUST provide the failed URI in this header
	in the method response */
	apt_str_t                        failed_uri;
	/** When a recorder method needs to post the audio to a URI and access to
	the URI fails, the server MUST provide the URI specific or protocol
	specific response code through this header in the method response */
	apt_str_t                        failed_uri_cause;
	/** When a recorder method contains this header the server must capture
	the audio and store it */
	apt_str_t                        record_uri;
	/** A RECORD method MUST contain this header, which specifies to the
	server the Media Type of the captured audio or video */
	apt_str_t                        media_type;
	/** When recording is started this specifies the maximum length of the
	recording in milliseconds, calculated from the time the actual
	capture and store begins and is not necessarily the time the RECORD
	method is received */
	apr_size_t                       max_time;
	/** This header MAY be sent on a STOP method and specifies the length of
	audio to be trimmed from the end of the recording after the stop */
	apr_size_t                       trim_length;
	/**  When recorder is started and the actual capture begins, this header
	specifies the length of silence in the audio that is to be
	interpreted as the end of the recording*/
	apr_size_t                       final_silence;
	/** f false, the recorder MUST start capturing immediately when started.
	If true, the recorder MUST wait for the endpointing functionality to
	detect speech before it starts capturing */
	apt_bool_t                       capture_on_speech;
	/** Tells the server to buffer the utterance associated with this 
	recording request into the verification buffer */
	apt_bool_t                       ver_buffer_utterance;
	/** MAY be sent as part of the RECORD request. A value of false tells the 
	recorder resource to start the operation, but not to start the no-input 
	timer until the client sends a START-INPUT-TIMERS */
	apt_bool_t                       start_input_timers;
	/** MAY be specified in a RECORD request and allows the
	client to tell the server that, from this point on, further input
	audio comes from a different audio source */
	apt_bool_t                       new_audio_channel;
};


/** Get recorder header vtable */
const mrcp_header_vtable_t* mrcp_recorder_header_vtable_get(mrcp_version_e version);

/** Get recorder completion cause string */
MRCP_DECLARE(const apt_str_t*) mrcp_recorder_completion_cause_get(
									mrcp_recorder_completion_cause_e completion_cause, 
									mrcp_version_e version);

APT_END_EXTERN_C

#endif /* MRCP_RECORDER_HEADER_H */
