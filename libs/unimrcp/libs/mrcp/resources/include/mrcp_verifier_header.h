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

#ifndef MRCP_VERIFIER_HEADER_H
#define MRCP_VERIFIER_HEADER_H

/**
 * @file mrcp_verifier_header.h
 * @brief MRCP Verifier Header
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C

/** MRCP verifier header fields */
typedef enum {
	VERIFIER_HEADER_REPOSITORY_URI,
	VERIFIER_HEADER_VOICEPRINT_IDENTIFIER,
	VERIFIER_HEADER_VERIFICATION_MODE,
	VERIFIER_HEADER_ADAPT_MODEL,
	VERIFIER_HEADER_ABORT_MODEL,
	VERIFIER_HEADER_MIN_VERIFICATION_SCORE,
	VERIFIER_HEADER_NUM_MIN_VERIFICATION_PHRASES,
	VERIFIER_HEADER_NUM_MAX_VERIFICATION_PHRASES,
	VERIFIER_HEADER_NO_INPUT_TIMEOUT,
	VERIFIER_HEADER_SAVE_WAVEFORM,
	VERIFIER_HEADER_MEDIA_TYPE,
	VERIFIER_HEADER_WAVEFORM_URI,
	VERIFIER_HEADER_VOICEPRINT_EXISTS,
	VERIFIER_HEADER_VER_BUFFER_UTTERANCE,
	VERIFIER_HEADER_INPUT_WAVEFORM_URI,
	VERIFIER_HEADER_COMPLETION_CAUSE,
	VERIFIER_HEADER_COMPLETION_REASON,
	VERIFIER_HEADER_SPEECH_COMPLETE_TIMEOUT,
	VERIFIER_HEADER_NEW_AUDIO_CHANNEL,
	VERIFIER_HEADER_ABORT_VERIFICATION,
	VERIFIER_HEADER_START_INPUT_TIMERS,

	VERIFIER_HEADER_COUNT
} mrcp_verifier_header_id;


/** MRCP verifier completion-cause  */
typedef enum {
	VERIFIER_COMPLETION_CAUSE_SUCCESS                 = 0,
	VERIFIER_COMPLETION_CAUSE_ERROR                   = 1,
	VERIFIER_COMPLETION_CAUSE_NO_INPUT_TIMEOUT        = 2,
	VERIFIER_COMPLETION_CAUSE_TOO_MUCH_SPEECH_TIMEOUT = 3,
	VERIFIER_COMPLETION_CAUSE_SPEECH_TOO_EARLY        = 4,
	VERIFIER_COMPLETION_CAUSE_BUFFER_EMPTY            = 5,
	VERIFIER_COMPLETION_CAUSE_OUT_OF_SEQUENCE         = 6,
	VERIFIER_COMPLETION_CAUSE_REPOSITORY_URI_FAILURE  = 7,
	VERIFIER_COMPLETION_CAUSE_REPOSITORY_URI_MISSING  = 8,
	VERIFIER_COMPLETION_CAUSE_VOICEPRINT_ID_MISSING   = 9,
	VERIFIER_COMPLETION_CAUSE_VOICEPRINT_ID_NOT_EXIST = 10,
	VERIFIER_COMPLETION_CAUSE_SPEECH_NOT_USABLE       = 11,

	VERIFIER_COMPLETION_CAUSE_COUNT                   = 12,
	VERIFIER_COMPLETION_CAUSE_UNKNOWN                 = VERIFIER_COMPLETION_CAUSE_COUNT
} mrcp_verifier_completion_cause_e;



/** MRCP verifier-header declaration */
typedef struct mrcp_verifier_header_t mrcp_verifier_header_t;

/** MRCP verifier-header */
struct mrcp_verifier_header_t {
	/** Specifies the voiceprint repository to be used or referenced during 
	speaker verification or identification operations */
	apt_str_t                     repository_uri;
	/** Specifies the claimed identity for verification applications */
	apt_str_t                     voiceprint_identifier;
	/** Specifies the mode of the verification resource */
	apt_str_t                     verification_mode;
	/** Indicates the desired behavior of the verification resource
	after a successful verification operation */
	apt_bool_t                    adapt_model;
	/** Indicates the desired behavior of the verification resource
	upon session termination */
	apt_bool_t                    abort_model;
	/** Determines the minimum verification score for which a verification 
	decision of "accepted" may be declared by the server */
	float                         min_verification_score;
	/** Specifies the minimum number of valid utterances 
	before a positive decision is given for verification */
	apr_size_t                    num_min_verification_phrases;
	/** Specifies the number of valid utterances required 
	before a decision is forced for verification */
	apr_size_t                    num_max_verification_phrases;
	/** Sets the length of time from the start of the verification timers 
	(see START-INPUT-TIMERS) until the declaration of a no-input event 
	in the VERIFICATION-COMPLETE server event message */
	apr_size_t                    no_input_timeout;
	/** Allows the client to request the verification resource to save
	the audio stream that was used for verification/identification */
	apt_bool_t                    save_waveform;
	/** Tells the server resource the Media Type of the captured audio or video 
	such as the one captured and returned by the Waveform-URI header field */
	apt_str_t                     media_type;
	/** If the Save-Waveform header field is set to true, the verification resource
	MUST attempt to record the incoming audio stream of the verification into 
	a file and provide a URI for the client to access it */
	apt_str_t                     waveform_uri;
	/** Shows the status of the voiceprint specified 
	in the QUERY-VOICEPRINT method */
	apt_bool_t                    voiceprint_exists;
	/** Indicates that this utterance could be 
	later considered for Speaker Verification */
	apt_bool_t                    ver_buffer_utterance;
	/** Specifies stored audio content that the client requests the server 
	to fetch and process according to the current verification mode, 
	either to train the voiceprint or verify a claimed identity */
	apt_str_t                     input_waveform_uri;
	/** Indicates the cause of VERIFY or VERIFY-FROM-BUFFER method completion */
	mrcp_verifier_completion_cause_e completion_cause;
	/** MAY be specified in a VERIFICATION-COMPLETE event 
	coming from the verifier resource to the client */
	apt_str_t                     completion_reason;
	/** Specifies the length of silence required following user
    speech before the speech verifier finalizes a result */
	apr_size_t                    speech_complete_timeout;
	/** MAY be specified in a VERIFIER request and allows the
    client to tell the server that, from this point on, further input
    audio comes from a different audio source */
	apt_bool_t                    new_audio_channel;
	/** MUST be sent in a STOP request to indicate 
	whether or not to abort a VERIFY method in progress */
	apt_bool_t                    abort_verification;
	/** MAY be sent as part of a VERIFY request. A value of false 
	tells the verification resource to start the VERIFY operation, 
	but not to start the no-input timer yet */
	apt_bool_t                    start_input_timers;
};


/** Get verifier header vtable */
const mrcp_header_vtable_t* mrcp_verifier_header_vtable_get(mrcp_version_e version);

/** Get verifier completion cause string */
MRCP_DECLARE(const apt_str_t*) mrcp_verifier_completion_cause_get(mrcp_verifier_completion_cause_e completion_cause, mrcp_version_e version);

APT_END_EXTERN_C

#endif /* MRCP_VERIFIER_HEADER_H */
