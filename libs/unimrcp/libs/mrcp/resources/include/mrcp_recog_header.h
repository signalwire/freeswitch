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

#ifndef __MRCP_RECOG_HEADER_H__
#define __MRCP_RECOG_HEADER_H__

/**
 * @file mrcp_recog_header.h
 * @brief MRCP Recognizer Header
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C

/** MRCP recognizer headers */
typedef enum {
	RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD,
	RECOGNIZER_HEADER_SENSITIVITY_LEVEL,
	RECOGNIZER_HEADER_SPEED_VS_ACCURACY,
	RECOGNIZER_HEADER_N_BEST_LIST_LENGTH,
	RECOGNIZER_HEADER_NO_INPUT_TIMEOUT,
	RECOGNIZER_HEADER_RECOGNITION_TIMEOUT,
	RECOGNIZER_HEADER_WAVEFORM_URI,
	RECOGNIZER_HEADER_COMPLETION_CAUSE,
	RECOGNIZER_HEADER_RECOGNIZER_CONTEXT_BLOCK,
	RECOGNIZER_HEADER_START_INPUT_TIMERS,
	RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT,
	RECOGNIZER_HEADER_SPEECH_INCOMPLETE_TIMEOUT,
	RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT,
	RECOGNIZER_HEADER_DTMF_TERM_TIMEOUT,
	RECOGNIZER_HEADER_DTMF_TERM_CHAR,
	RECOGNIZER_HEADER_FAILED_URI,
	RECOGNIZER_HEADER_FAILED_URI_CAUSE,
	RECOGNIZER_HEADER_SAVE_WAVEFORM,
	RECOGNIZER_HEADER_NEW_AUDIO_CHANNEL,
	RECOGNIZER_HEADER_SPEECH_LANGUAGE,

	/** Additional headers for MRCP v2 */
	RECOGNIZER_HEADER_INPUT_TYPE,
	RECOGNIZER_HEADER_INPUT_WAVEFORM_URI,
	RECOGNIZER_HEADER_COMPLETION_REASON,
	RECOGNIZER_HEADER_MEDIA_TYPE,
	RECOGNIZER_HEADER_VER_BUFFER_UTTERANCE,
	RECOGNIZER_HEADER_RECOGNITION_MODE,
	RECOGNIZER_HEADER_CANCEL_IF_QUEUE,
	RECOGNIZER_HEADER_HOTWORD_MAX_DURATION,
	RECOGNIZER_HEADER_HOTWORD_MIN_DURATION,
	RECOGNIZER_HEADER_INTERPRET_TEXT,
	RECOGNIZER_HEADER_DTMF_BUFFER_TIME,
	RECOGNIZER_HEADER_CLEAR_DTMF_BUFFER,
	RECOGNIZER_HEADER_EARLY_NO_MATCH,

	RECOGNIZER_HEADER_COUNT
} mrcp_recognizer_header_id;


/** MRCP recognizer completion-cause  */
typedef enum {
	RECOGNIZER_COMPLETION_CAUSE_SUCCESS                 = 0,
	RECOGNIZER_COMPLETION_CAUSE_NO_MATCH                = 1,
	RECOGNIZER_COMPLETION_CAUSE_NO_INPUT_TIMEOUT        = 2,
	RECOGNIZER_COMPLETION_CAUSE_RECOGNITION_TIMEOUT     = 3,
	RECOGNIZER_COMPLETION_CAUSE_GRAM_LOAD_FAILURE       = 4,
	RECOGNIZER_COMPLETION_CAUSE_GRAM_COMP_FAILURE       = 5,
	RECOGNIZER_COMPLETION_CAUSE_ERROR                   = 6,
	RECOGNIZER_COMPLETION_CAUSE_SPEECH_TOO_EARLY        = 7,
	RECOGNIZER_COMPLETION_CAUSE_TOO_MUCH_SPEECH_TIMEOUT = 8,
	RECOGNIZER_COMPLETION_CAUSE_URI_FAILURE             = 9,
	RECOGNIZER_COMPLETION_CAUSE_LANGUAGE_UNSUPPORTED    = 10,

	/** Additional completion-cause for MRCP v2 */
	RECOGNIZER_COMPLETION_CAUSE_CANCELLED               = 11,
	RECOGNIZER_COMPLETION_CAUSE_SEMANTICS_FAILURE       = 12,
	RECOGNIZER_COMPLETION_CAUSE_PARTIAL_MATCH           = 13,
	RECOGNIZER_COMPLETION_CAUSE_PARTIAL_MATCH_MAXTIME   = 14,
	RECOGNIZER_COMPLETION_CAUSE_NO_MATCH_MAXTIME        = 15,
	RECOGNIZER_COMPLETION_CAUSE_GRAM_DEFINITION_FAILURE = 16,

	RECOGNIZER_COMPLETION_CAUSE_COUNT                   = 17,
	RECOGNIZER_COMPLETION_CAUSE_UNKNOWN                 = RECOGNIZER_COMPLETION_CAUSE_COUNT
} mrcp_recog_completion_cause_e;



/** MRCP recognizer-header declaration */
typedef struct mrcp_recog_header_t mrcp_recog_header_t;

/** MRCP recognizer-header */
struct mrcp_recog_header_t {
	/** Tells the recognizer resource what confidence level the client considers a
    successful match */
	float                         confidence_threshold;
	/** To filter out background noise and not mistake it for speech */
	float                         sensitivity_level;
	/** Tunable towards Performance or Accuracy */
	float                         speed_vs_accuracy;
	/** The client, by setting this header, can ask the recognition resource 
	to send it more  than 1 alternative */
	apr_size_t                    n_best_list_length;
	/** The client can use the no-input-timeout header to set this timeout */
	apr_size_t                    no_input_timeout;
	/** The client can use the recognition-timeout header to set this timeout */
	apr_size_t                    recognition_timeout;
	/** MUST be present in the RECOGNITION-COMPLETE event if the Save-Waveform
	header was set to true */
	apt_str_t                     waveform_uri;
	/** MUST be part of a RECOGNITION-COMPLETE, event coming from
    the recognizer resource to the client */
	mrcp_recog_completion_cause_e completion_cause;
	/** MAY be sent as part of the SET-PARAMS or GET-PARAMS request */
	apt_str_t                     recognizer_context_block;
	/** MAY be sent as part of the RECOGNIZE request. A value of false tells
	the recognizer to start recognition, but not to start the no-input timer yet */
	apt_bool_t                    start_input_timers;
	/** Specifies the length of silence required following user
    speech before the speech recognizer finalizes a result */
	apr_size_t                    speech_complete_timeout;
	/** Specifies the required length of silence following user
    speech after which a recognizer finalizes a result */
	apr_size_t                    speech_incomplete_timeout;
	/** Specifies the inter-digit timeout value to use when
    recognizing DTMF input */
	apr_size_t                    dtmf_interdigit_timeout;
	/** Specifies the terminating timeout to use when 
	recognizing DTMF input*/
	apr_size_t                    dtmf_term_timeout;
	/** Specifies the terminating DTMF character for DTMF input
    recognition */
	char                          dtmf_term_char;
	/** When a recognizer needs to fetch or access a URI and the access fails
    the server SHOULD provide the failed URI in this header in the method response*/
	apt_str_t                     failed_uri;
	/** When a recognizer method needs a recognizer to fetch or access a URI
    and the access fails the server MUST provide the URI specific or
    protocol specific response code for the URI in the Failed-URI header */
	apt_str_t                     failed_uri_cause;
	/** Allows the client to request the recognizer resource to
    save the audio input to the recognizer */
	apt_bool_t                    save_waveform;
	/** MAY be specified in a RECOGNIZE request and allows the
    client to tell the server that, from this point on, further input
    audio comes from a different audio source */
	apt_bool_t                    new_audio_channel;
	/** Specifies the language of recognition grammar data within
    a session or request, if it is not specified within the data */
	apt_str_t                     speech_language;

	/** Additional headers for MRCP v2 */
	/** Specifies if the input that caused a barge-in was DTMF or speech */
	apt_str_t                     input_type;
	/** Optional header specifies a URI pointing to audio content to be
    processed by the RECOGNIZE operation */
	apt_str_t                     input_waveform_uri;
	/** MAY be specified in a RECOGNITION-COMPLETE event coming from
    the recognizer resource to the client */
	apt_str_t                     completion_reason;
	/** tells the server resource the Media Type in which to store captured 
	audio such as the one captured and returned by the Waveform-URI header */
	apt_str_t                     media_type;
	/** lets the client request the server to buffer the
    utterance associated with this recognition request into a buffer
    available to a co-resident verification resource */
	apt_bool_t                    ver_buffer_utterance;
	/** Specifies what mode the RECOGNIZE method will operate in */
	apt_str_t                     recognition_mode;
	/** Specifies what will happen if the client attempts to
    invoke another RECOGNIZE method when this RECOGNIZE request is
    already in progress for the resource*/
	apt_bool_t                    cancel_if_queue;
	/** Specifies the maximum length of an utterance (in seconds) that will
    be considered for Hotword recognition */
	apr_size_t                    hotword_max_duration;
	/** Specifies the minimum length of an utterance (in seconds) that will
    be considered for Hotword recognition */
	apr_size_t                    hotword_min_duration;
	/** Provides a pointer to the text for which a natural language interpretation is desired */
	apt_str_t                     interpret_text;
	/** MAY be specified in a GET-PARAMS or SET-PARAMS method and
    is used to specify the size in time, in milliseconds, of the
    typeahead buffer for the recognizer */
	apr_size_t                    dtmf_buffer_time;
	/** MAY be specified in a RECOGNIZE method and is used to
    tell the recognizer to clear the DTMF type-ahead buffer before
    starting the recognize */
	apt_bool_t                    clear_dtmf_buffer;
	/** MAY be specified in a RECOGNIZE method and is used to
    tell the recognizer that it MUST not wait for the end of speech
    before processing the collected speech to match active grammars */
	apt_bool_t                    early_no_match;
};


/** Get recognizer header vtable */
const mrcp_header_vtable_t* mrcp_recog_header_vtable_get(mrcp_version_e version);

/** Get recognizer completion cause string */
MRCP_DECLARE(const apt_str_t*) mrcp_recog_completion_cause_get(mrcp_recog_completion_cause_e completion_cause, mrcp_version_e version);

APT_END_EXTERN_C

#endif /*__MRCP_RECOG_HEADER_H__*/
