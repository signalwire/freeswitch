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
 * $Id: mrcp_synth_header.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MRCP_SYNTH_HEADER_H
#define MRCP_SYNTH_HEADER_H

/**
 * @file mrcp_synth_header.h
 * @brief MRCP Synthesizer Header
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C

/** MRCP synthesizer header fields */
typedef enum {
	SYNTHESIZER_HEADER_JUMP_SIZE,
	SYNTHESIZER_HEADER_KILL_ON_BARGE_IN,
	SYNTHESIZER_HEADER_SPEAKER_PROFILE,
	SYNTHESIZER_HEADER_COMPLETION_CAUSE,
	SYNTHESIZER_HEADER_COMPLETION_REASON,
	SYNTHESIZER_HEADER_VOICE_GENDER,
	SYNTHESIZER_HEADER_VOICE_AGE,
	SYNTHESIZER_HEADER_VOICE_VARIANT,
	SYNTHESIZER_HEADER_VOICE_NAME,
	SYNTHESIZER_HEADER_PROSODY_VOLUME,
	SYNTHESIZER_HEADER_PROSODY_RATE,
	SYNTHESIZER_HEADER_SPEECH_MARKER,
	SYNTHESIZER_HEADER_SPEECH_LANGUAGE,
	SYNTHESIZER_HEADER_FETCH_HINT,
	SYNTHESIZER_HEADER_AUDIO_FETCH_HINT,
	SYNTHESIZER_HEADER_FAILED_URI,
	SYNTHESIZER_HEADER_FAILED_URI_CAUSE,
	SYNTHESIZER_HEADER_SPEAK_RESTART,
	SYNTHESIZER_HEADER_SPEAK_LENGTH,
	SYNTHESIZER_HEADER_LOAD_LEXICON,
	SYNTHESIZER_HEADER_LEXICON_SEARCH_ORDER,

	SYNTHESIZER_HEADER_COUNT
} mrcp_synthesizer_header_id;


/** Speech-units */
typedef enum {
	SPEECH_UNIT_SECOND,
	SPEECH_UNIT_WORD,
	SPEECH_UNIT_SENTENCE,
	SPEECH_UNIT_PARAGRAPH,

	SPEECH_UNIT_COUNT
} mrcp_speech_unit_e;

/** Speech-length types */
typedef enum {
	SPEECH_LENGTH_TYPE_TEXT,
	SPEECH_LENGTH_TYPE_NUMERIC_POSITIVE,
	SPEECH_LENGTH_TYPE_NUMERIC_NEGATIVE,

	SPEECH_LENGTH_TYPE_UNKNOWN
} mrcp_speech_length_type_e;

/** MRCP voice-gender */
typedef enum {
	VOICE_GENDER_MALE,
	VOICE_GENDER_FEMALE,
	VOICE_GENDER_NEUTRAL,
	
	VOICE_GENDER_COUNT,
	VOICE_GENDER_UNKNOWN = VOICE_GENDER_COUNT
} mrcp_voice_gender_e;

/** Prosody-volume type */
typedef enum {
	PROSODY_VOLUME_TYPE_LABEL,
	PROSODY_VOLUME_TYPE_NUMERIC,
	PROSODY_VOLUME_TYPE_RELATIVE_CHANGE,
	
	PROSODY_VOLUME_TYPE_UNKNOWN
} mrcp_prosody_volume_type_e;

/** Prosody-rate type */
typedef enum {
	PROSODY_RATE_TYPE_LABEL,
	PROSODY_RATE_TYPE_RELATIVE_CHANGE,

	PROSODY_RATE_TYPE_UNKNOWN
} mrcp_prosody_rate_type_e;

/** Prosody-volume */
typedef enum {
	PROSODY_VOLUME_SILENT,
	PROSODY_VOLUME_XSOFT,
	PROSODY_VOLUME_SOFT,
	PROSODY_VOLUME_MEDIUM,
	PROSODY_VOLUME_LOUD,
	PROSODY_VOLUME_XLOUD,
	PROSODY_VOLUME_DEFAULT,

	PROSODY_VOLUME_COUNT,
	PROSODY_VOLUME_UNKNOWN = PROSODY_VOLUME_COUNT
} mrcp_prosody_volume_label_e;

/** Prosody-rate */
typedef enum {
	PROSODY_RATE_XSLOW,
	PROSODY_RATE_SLOW,
	PROSODY_RATE_MEDIUM,
	PROSODY_RATE_FAST,
	PROSODY_RATE_XFAST,
	PROSODY_RATE_DEFAULT,

	PROSODY_RATE_COUNT,
	PROSODY_RATE_UNKNOWN = PROSODY_RATE_COUNT
} mrcp_prosody_rate_label_e;

/** Synthesizer completion-cause specified in SPEAK-COMPLETE event */
typedef enum {
	SYNTHESIZER_COMPLETION_CAUSE_NORMAL               = 0,
	SYNTHESIZER_COMPLETION_CAUSE_BARGE_IN             = 1,
	SYNTHESIZER_COMPLETION_CAUSE_PARSE_FAILURE        = 2,
	SYNTHESIZER_COMPLETION_CAUSE_URI_FAILURE          = 3,
	SYNTHESIZER_COMPLETION_CAUSE_ERROR                = 4,
	SYNTHESIZER_COMPLETION_CAUSE_LANGUAGE_UNSUPPORTED = 5,
	SYNTHESIZER_COMPLETION_CAUSE_LEXICON_LOAD_FAILURE = 6,
	SYNTHESIZER_COMPLETION_CAUSE_CANCELLED            = 7,

	SYNTHESIZER_COMPLETION_CAUSE_COUNT                = 8,
	SYNTHESIZER_COMPLETION_CAUSE_UNKNOWN              = SYNTHESIZER_COMPLETION_CAUSE_COUNT
} mrcp_synth_completion_cause_e;


/** Speech-length value declaration */
typedef struct mrcp_speech_length_value_t mrcp_speech_length_value_t;
/** Numeric speech-length declaration */
typedef struct mrcp_numeric_speech_length_t mrcp_numeric_speech_length_t;
/** Prosody-param declaration */
typedef struct mrcp_prosody_param_t mrcp_prosody_param_t;
/** Voice-param declaration */
typedef struct mrcp_voice_param_t mrcp_voice_param_t;
/**Prosody-rate declaration*/
typedef struct mrcp_prosody_rate_t mrcp_prosody_rate_t;
/**Prosody-volume declaration*/
typedef struct mrcp_prosody_volume_t mrcp_prosody_volume_t;
/** MRCP synthesizer-header declaration */
typedef struct mrcp_synth_header_t mrcp_synth_header_t;

/** Numeric speech-length */
struct mrcp_numeric_speech_length_t {
	/** The length */
	apr_size_t         length;
	/** The unit (second/word/sentence/paragraph) */
	mrcp_speech_unit_e unit;
};

/** Definition of speech-length value */
struct mrcp_speech_length_value_t {
	/** Speech-length type (numeric/text)*/
	mrcp_speech_length_type_e type;
	/** Speech-length value (either numeric or text) */
	union {
		/** Text speech-length */
		apt_str_t                    tag;
		/** Numeric speech-length */
		mrcp_numeric_speech_length_t numeric;
	} value;
};

/** MRCP voice-param */
struct mrcp_voice_param_t {
	/** Voice gender (male/femaile/neutral)*/
	mrcp_voice_gender_e gender;
	/** Voice age */
	apr_size_t          age;
	/** Voice variant */
	apr_size_t          variant;
	/** Voice name */
	apt_str_t           name;
};

/** MRCP prosody-volume */
struct mrcp_prosody_volume_t {
	/** prosody-volume type (one of label,numeric,relative change) */
	mrcp_prosody_volume_type_e type;

	/** prosody-volume value */
	union {
		/** one of "silent", "x-soft", ... */ 
		mrcp_prosody_volume_label_e label;
		/** numeric value */
		float                       numeric;
		/** relative change */
		float                       relative;
	} value;
};

/** MRCP prosody-rate */
struct mrcp_prosody_rate_t {
	/** prosody-rate type (one of label, relative change) */
	mrcp_prosody_rate_type_e type;

	/** prosody-rate value */
	union {
		/** one of "x-slow", "slow", ... */ 
		mrcp_prosody_rate_label_e label;
		/** relative change */
		float                     relative;
	} value;
};

/** MRCP prosody-param */
struct mrcp_prosody_param_t {
	/** Prosofy volume */
	mrcp_prosody_volume_t volume;
	/** Prosofy rate */
	mrcp_prosody_rate_t   rate;
};

/** MRCP synthesizer-header */
struct mrcp_synth_header_t {
	/** MAY be specified in a CONTROL method and controls the
    amount to jump forward or backward in an active "SPEAK" request */
	mrcp_speech_length_value_t    jump_size;
	/** MAY be sent as part of the "SPEAK" method to enable kill-
    on-barge-in support */
	apt_bool_t                    kill_on_barge_in;
	/** MAY be part of the "SET-PARAMS"/"GET-PARAMS" or "SPEAK"
    request from the client to the server and specifies a URI which
    references the profile of the speaker */
	apt_str_t                     speaker_profile;
	/** MUST be specified in a "SPEAK-COMPLETE" event coming from
    the synthesizer resource to the client */
	mrcp_synth_completion_cause_e completion_cause;
	/** MAY be specified in a "SPEAK-COMPLETE" event coming from
    the synthesizer resource to the client */
	apt_str_t                     completion_reason;
	/** This set of header fields defines the voice of the speaker */
	mrcp_voice_param_t            voice_param;
	/** This set of header fields defines the prosody of the speech */
	mrcp_prosody_param_t          prosody_param;
	/** Contains timestamp information in a "timestamp" field */
	apt_str_t                     speech_marker;
	/** specifies the default language of the speech data if the
    language is not specified in the markup */
	apt_str_t                     speech_language;
	/** When the synthesizer needs to fetch documents or other resources like
    speech markup or audio files, this header controls the corresponding
    URI access properties */
	apt_str_t                     fetch_hint;
	/** When the synthesizer needs to fetch documents or other resources like
    speech audio files, this header controls the corresponding URI access
    properties */
	apt_str_t                     audio_fetch_hint;
	/** When a synthesizer method needs a synthesizer to fetch or access a
    URI and the access fails, the server SHOULD provide the failed URI in
    this header in the method response */
	apt_str_t                     failed_uri;
	/** When a synthesizer method needs a synthesizer to fetch or access a
    URI and the access fails the server MUST provide the URI specific or
    protocol specific response code for the URI in the Failed-URI header
    in the method response through this header */
	apt_str_t                     failed_uri_cause;
	/** When a CONTROL request to jump backward is issued to a currently
    speaking synthesizer resource, and the target jump point is before
    the start of the current "SPEAK" request, the current "SPEAK" request
    MUST restart */
	apt_bool_t                    speak_restart;
	/** MAY be specified in a CONTROL method to control the
    length of speech to speak, relative to the current speaking point in
    the currently active "SPEAK" request */
	mrcp_speech_length_value_t    speak_length;
	/** Used to indicate whether a lexicon has to be loaded or unloaded */
	apt_bool_t                    load_lexicon;
	/** Used to specify a list of active Lexicon URIs and the
    search order among the active lexicons */
	apt_str_t                     lexicon_search_order;
};

/** Get synthesizer header vtable */
const mrcp_header_vtable_t* mrcp_synth_header_vtable_get(mrcp_version_e version);

/** Get synthesizer completion cause string */
MRCP_DECLARE(const apt_str_t*) mrcp_synth_completion_cause_get(mrcp_synth_completion_cause_e completion_cause, mrcp_version_e version);


APT_END_EXTERN_C

#endif /* MRCP_SYNTH_HEADER_H */
