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

#include "mrcp_recog_header.h"

/** String table of MRCPv1 recognizer headers (mrcp_recog_header_id) */
static const apt_str_table_item_t v1_recog_header_string_table[] = {
	{{"Confidence-Threshold",       20},7},
	{{"Sensitivity-Level",          17},3},
	{{"Speed-Vs-Accuracy",          17},4},
	{{"N-Best-List-Length",         18},1},
	{{"No-Input-Timeout",           16},2},
	{{"Recognition-Timeout",        19},16},
	{{"Waveform-Url",               12},0},
	{{"Completion-Cause",           16},16},
	{{"Recognizer-Context-Block",   24},12},
	{{"Recognizer-Start-Timers",    23},11},
	{{"Speech-Complete-Timeout",    23},7},
	{{"Speech-Incomplete-Timeout",  25},8},
	{{"DTMF-Interdigit-Timeout",    23},5},
	{{"DTMF-Term-Timeout",          17},14},
	{{"DTMF-Term-Char",             14},14},
	{{"Failed-Uri",                 10},10},
	{{"Failed-Uri-Cause",           16},16},
	{{"Save-Waveform",              13},5},
	{{"New-Audio-Channel",          17},2},
	{{"Speech-Language",            15},8},
	{{"Input-Type",                 10},10},
	{{"Input-Waveform-Uri",         18},6},
	{{"Completion-Reason",          17},15},
	{{"Media-Type",                 10},0},
	{{"Ver-Buffer-Utterance",       20},0},
	{{"Recognition-Mode",           16},14},
	{{"Cancel-If-Queue",            15},3},
	{{"Hotword-Max-Duration",       20},10},
	{{"Hotword-Min-Duration",       20},20},
	{{"Interpret-Text",             14},7},
	{{"DTMF-Buffer-Time",           16},5},
	{{"Clear-DTMF-Buffer",          17},1},
	{{"Early-No-Match",             14},0}
};

/** String table of MRCPv2 recognizer headers (mrcp_recog_header_id) */
static const apt_str_table_item_t v2_recog_header_string_table[] = {
	{{"Confidence-Threshold",       20},8},
	{{"Sensitivity-Level",          17},3},
	{{"Speed-Vs-Accuracy",          17},4},
	{{"N-Best-List-Length",         18},1},
	{{"No-Input-Timeout",           16},2},
	{{"Recognition-Timeout",        19},16},
	{{"Waveform-Uri",               12},0},
	{{"Completion-Cause",           16},16},
	{{"Recognizer-Context-Block",   24},7},
	{{"Start-Input-Timers",         18},2},
	{{"Speech-Complete-Timeout",    23},7},
	{{"Speech-Incomplete-Timeout",  25},8},
	{{"DTMF-Interdigit-Timeout",    23},5},
	{{"DTMF-Term-Timeout",          17},14},
	{{"DTMF-Term-Char",             14},14},
	{{"Failed-Uri",                 10},10},
	{{"Failed-Uri-Cause",           16},16},
	{{"Save-Waveform",              13},5},
	{{"New-Audio-Channel",          17},2},
	{{"Speech-Language",            15},8},
	{{"Input-Type",                 10},10},
	{{"Input-Waveform-Uri",         18},6},
	{{"Completion-Reason",          17},13},
	{{"Media-Type",                 10},0},
	{{"Ver-Buffer-Utterance",       20},0},
	{{"Recognition-Mode",           16},14},
	{{"Cancel-If-Queue",            15},3},
	{{"Hotword-Max-Duration",       20},10},
	{{"Hotword-Min-Duration",       20},20},
	{{"Interpret-Text",             14},7},
	{{"DTMF-Buffer-Time",           16},5},
	{{"Clear-DTMF-Buffer",          17},1},
	{{"Early-No-Match",             14},0}
};

/** String table of MRCPv1 recognizer completion-cause fields (mrcp_recog_completion_cause_e) */
static const apt_str_table_item_t v1_completion_cause_string_table[] = {
	{{"success",                     7},1},
	{{"no-match",                    8},8},
	{{"no-input-timeout",           16},3},
	{{"recognition-timeout",        19},0},
	{{"gram-load-failure",          17},7},
	{{"gram-comp-failure",          17},5},
	{{"error",                       5},0},
	{{"speech-too-early",           16},1},
	{{"too-much-speech-timeout",    23},0},
	{{"uri-failure",                11},0},
	{{"language-unsupported",       20},0},
	{{"cancelled",                   9},0},
	{{"semantics-failure",          17},2},
	{{"partial-match",              13},13},
	{{"partial-match-maxtime",      21},13},
	{{"no-match-maxtime",           16},9},
	{{"gram-definition-failure",    23},5}
};


/** String table of MRCPv2 recognizer completion-cause fields (mrcp_recog_completion_cause_e) */
static const apt_str_table_item_t v2_completion_cause_string_table[] = {
	{{"success",                     7},7},
	{{"no-match",                    8},4},
	{{"no-input-timeout",           16},3},
	{{"hotword-maxtime",            15},0},
	{{"grammar-load-failure",       20},8},
	{{"grammar-compilation-failure",27},8},
	{{"recognizer-error",           16},0},
	{{"speech-too-early",           16},1},
	{{"success-maxtime",            15},15},
	{{"uri-failure",                11},0},
	{{"language-unsupported",       20},0},
	{{"cancelled",                   9},0},
	{{"semantics-failure",          17},2},
	{{"partial-match",              13},13},
	{{"partial-match-maxtime",      21},13},
	{{"no-match-maxtime",           16},9},
	{{"grammar-definition-failure", 26},9}
};

/** Initialize recognizer header */
static void mrcp_recog_header_init(mrcp_recog_header_t *recog_header)
{
	recog_header->confidence_threshold = 0.0;
	recog_header->sensitivity_level = 0.0;
	recog_header->speed_vs_accuracy = 0.0;
	recog_header->n_best_list_length = 0;
	recog_header->no_input_timeout = 0;
	recog_header->recognition_timeout = 0;
	apt_string_reset(&recog_header->waveform_uri);
	recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_COUNT;
	apt_string_reset(&recog_header->recognizer_context_block);
	recog_header->start_input_timers = FALSE;
	recog_header->speech_complete_timeout = 0;
	recog_header->speech_incomplete_timeout = 0;
	recog_header->dtmf_interdigit_timeout = 0;
	recog_header->dtmf_term_timeout = 0;
	recog_header->dtmf_term_char = 0;
	apt_string_reset(&recog_header->failed_uri);
	apt_string_reset(&recog_header->failed_uri_cause);
	recog_header->save_waveform = FALSE;
	recog_header->new_audio_channel = FALSE;
	apt_string_reset(&recog_header->speech_language);
	/* initializes additionnal MRCPV2 recog headers */
	apt_string_reset(&recog_header->input_type);
	apt_string_reset(&recog_header->input_waveform_uri);
	apt_string_reset(&recog_header->completion_reason);
	apt_string_reset(&recog_header->media_type);
	recog_header->ver_buffer_utterance = FALSE;
	apt_string_reset(&recog_header->recognition_mode);
	recog_header->cancel_if_queue = FALSE;
	recog_header->hotword_max_duration = 0;
	recog_header->hotword_min_duration = 0;
	apt_string_reset(&recog_header->interpret_text);
	recog_header->dtmf_buffer_time = 0;
	recog_header->clear_dtmf_buffer = FALSE;
	recog_header->early_no_match = FALSE;
}

/** Allocate MRCP recognizer header */
static void* mrcp_recog_header_allocate(mrcp_header_accessor_t *accessor, apr_pool_t *pool)
{
	mrcp_recog_header_t *recog_header = apr_palloc(pool,sizeof(mrcp_recog_header_t));
	mrcp_recog_header_init(recog_header);
	accessor->data = recog_header;
	return accessor->data;
}

/** Parse MRCP recognizer header */
static apt_bool_t mrcp_recog_header_parse(mrcp_recog_header_t *recog_header, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	apt_bool_t status = TRUE;
	switch(id) {
		case RECOGNIZER_HEADER_N_BEST_LIST_LENGTH:
			recog_header->n_best_list_length = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_NO_INPUT_TIMEOUT:
			recog_header->no_input_timeout = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_RECOGNITION_TIMEOUT:
			recog_header->recognition_timeout = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_WAVEFORM_URI:
			apt_string_copy(&recog_header->waveform_uri,value,pool);
			break;
		case RECOGNIZER_HEADER_COMPLETION_CAUSE:
			recog_header->completion_cause = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_RECOGNIZER_CONTEXT_BLOCK:
			apt_string_copy(&recog_header->recognizer_context_block,value,pool);
			break;
		case RECOGNIZER_HEADER_START_INPUT_TIMERS:
			apt_boolean_value_parse(value,&recog_header->start_input_timers);
			break;
		case RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT:
			recog_header->speech_complete_timeout = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_SPEECH_INCOMPLETE_TIMEOUT:
			recog_header->speech_incomplete_timeout = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT:
			recog_header->dtmf_interdigit_timeout = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_DTMF_TERM_TIMEOUT:
			recog_header->dtmf_term_timeout = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_DTMF_TERM_CHAR:
			recog_header->dtmf_term_char = *value->buf;
			break;
		case RECOGNIZER_HEADER_FAILED_URI:
			apt_string_copy(&recog_header->failed_uri,value,pool);
			break;
		case RECOGNIZER_HEADER_FAILED_URI_CAUSE:
			apt_string_copy(&recog_header->failed_uri_cause,value,pool);
			break;
		case RECOGNIZER_HEADER_SAVE_WAVEFORM:
			apt_boolean_value_parse(value,&recog_header->save_waveform);
			break;
		case RECOGNIZER_HEADER_NEW_AUDIO_CHANNEL:
			apt_boolean_value_parse(value,&recog_header->new_audio_channel);
			break;
		case RECOGNIZER_HEADER_SPEECH_LANGUAGE:
			apt_string_copy(&recog_header->speech_language,value,pool);
			break;
		case RECOGNIZER_HEADER_INPUT_TYPE:
			apt_string_copy(&recog_header->input_type,value,pool);
			break;
		case RECOGNIZER_HEADER_MEDIA_TYPE:
			apt_string_copy(&recog_header->media_type,value,pool);
			break;
		case RECOGNIZER_HEADER_INPUT_WAVEFORM_URI:
			apt_string_copy(&recog_header->input_waveform_uri,value,pool);
			break;
		case RECOGNIZER_HEADER_COMPLETION_REASON:
			apt_string_copy(&recog_header->completion_reason,value,pool);
			break;
		case RECOGNIZER_HEADER_VER_BUFFER_UTTERANCE:
			apt_boolean_value_parse(value,&recog_header->ver_buffer_utterance);
			break;
		case RECOGNIZER_HEADER_RECOGNITION_MODE:
			apt_string_copy(&recog_header->recognition_mode,value,pool);
			break;
		case RECOGNIZER_HEADER_CANCEL_IF_QUEUE:
			apt_boolean_value_parse(value,&recog_header->cancel_if_queue);
			break;
		case RECOGNIZER_HEADER_HOTWORD_MAX_DURATION:
			recog_header->hotword_max_duration = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_HOTWORD_MIN_DURATION:
			recog_header->hotword_min_duration = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_INTERPRET_TEXT:
			apt_string_copy(&recog_header->interpret_text,value,pool);
			break;
		case RECOGNIZER_HEADER_DTMF_BUFFER_TIME:
			recog_header->dtmf_buffer_time = apt_size_value_parse(value);
			break;
		case RECOGNIZER_HEADER_CLEAR_DTMF_BUFFER:
			apt_boolean_value_parse(value,&recog_header->clear_dtmf_buffer);
			break;
		case RECOGNIZER_HEADER_EARLY_NO_MATCH:
			apt_boolean_value_parse(value,&recog_header->early_no_match);
			break;
		default:
			status = FALSE;
	}
	return status;
}

static APR_INLINE float apt_size_value_parse_as_float(const apt_str_t *value)
{
	float f = (float)apt_size_value_parse(value);
	return f / 100;
}

static APR_INLINE apt_bool_t apt_size_value_generate_from_float(float value, apt_text_stream_t *stream)
{
	apr_size_t s = (apr_size_t)((value + 0.001f) * 100);
	return apt_size_value_generate(s,stream);
}

/** Parse MRCPv1 recognizer header */
static apt_bool_t mrcp_v1_recog_header_parse(mrcp_header_accessor_t *accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	mrcp_recog_header_t *recog_header = accessor->data;
	if(id == RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD) {
		recog_header->confidence_threshold = apt_size_value_parse_as_float(value);
		return TRUE;
	}
	else if(id == RECOGNIZER_HEADER_SENSITIVITY_LEVEL) {
		recog_header->sensitivity_level = apt_size_value_parse_as_float(value);
		return TRUE;
	}
	else if(id == RECOGNIZER_HEADER_SPEED_VS_ACCURACY) {
		recog_header->speed_vs_accuracy = apt_size_value_parse_as_float(value);
		return TRUE;
	}
	return mrcp_recog_header_parse(recog_header,id,value,pool);
}

/** Parse MRCPv2 recognizer header */
static apt_bool_t mrcp_v2_recog_header_parse(mrcp_header_accessor_t *accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	mrcp_recog_header_t *recog_header = accessor->data;
	if(id == RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD) {
		recog_header->confidence_threshold = apt_float_value_parse(value);
		return TRUE;
	}
	else if(id == RECOGNIZER_HEADER_SENSITIVITY_LEVEL) {
		recog_header->sensitivity_level = apt_float_value_parse(value);
		return TRUE;
	}
	else if(id == RECOGNIZER_HEADER_SPEED_VS_ACCURACY) {
		recog_header->speed_vs_accuracy = apt_float_value_parse(value);
		return TRUE;
	}
	return mrcp_recog_header_parse(recog_header,id,value,pool);
}

/** Generate MRCP recognizer header */
static apt_bool_t mrcp_recog_header_generate(mrcp_recog_header_t *recog_header, apr_size_t id, apt_text_stream_t *value)
{
	switch(id) {
		case RECOGNIZER_HEADER_N_BEST_LIST_LENGTH:
			apt_size_value_generate(recog_header->n_best_list_length,value);
			break;
		case RECOGNIZER_HEADER_NO_INPUT_TIMEOUT:
			apt_size_value_generate(recog_header->no_input_timeout,value);
			break;
		case RECOGNIZER_HEADER_RECOGNITION_TIMEOUT:
			apt_size_value_generate(recog_header->recognition_timeout,value);
			break;
		case RECOGNIZER_HEADER_WAVEFORM_URI:
			apt_string_value_generate(&recog_header->waveform_uri,value);
			break;
		case RECOGNIZER_HEADER_RECOGNIZER_CONTEXT_BLOCK:
			apt_string_value_generate(&recog_header->recognizer_context_block,value);
			break;
		case RECOGNIZER_HEADER_START_INPUT_TIMERS:
			apt_boolean_value_generate(recog_header->start_input_timers,value);
			break;
		case RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT:
			apt_size_value_generate(recog_header->speech_complete_timeout,value);
			break;
		case RECOGNIZER_HEADER_SPEECH_INCOMPLETE_TIMEOUT:
			apt_size_value_generate(recog_header->speech_incomplete_timeout,value);
			break;
		case RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT:
			apt_size_value_generate(recog_header->dtmf_interdigit_timeout,value);
			break;
		case RECOGNIZER_HEADER_DTMF_TERM_TIMEOUT:
			apt_size_value_generate(recog_header->dtmf_term_timeout,value);
			break;
		case RECOGNIZER_HEADER_DTMF_TERM_CHAR:
			*value->pos++ = recog_header->dtmf_term_char;
			break;
		case RECOGNIZER_HEADER_FAILED_URI:
			apt_string_value_generate(&recog_header->failed_uri,value);
			break;
		case RECOGNIZER_HEADER_FAILED_URI_CAUSE:
			apt_string_value_generate(&recog_header->failed_uri_cause,value);
			break;
		case RECOGNIZER_HEADER_SAVE_WAVEFORM:
			apt_boolean_value_generate(recog_header->save_waveform,value);
			break;
		case RECOGNIZER_HEADER_NEW_AUDIO_CHANNEL:
			apt_boolean_value_generate(recog_header->new_audio_channel,value);
			break;
		case RECOGNIZER_HEADER_SPEECH_LANGUAGE:
			apt_string_value_generate(&recog_header->speech_language,value);
			break;
		case RECOGNIZER_HEADER_INPUT_TYPE:
			apt_string_value_generate(&recog_header->input_type,value);
			break;
		case RECOGNIZER_HEADER_INPUT_WAVEFORM_URI:
			apt_string_value_generate(&recog_header->input_waveform_uri,value);
			break;
		case RECOGNIZER_HEADER_COMPLETION_REASON:
			apt_string_value_generate(&recog_header->completion_reason,value);
			break;
		case RECOGNIZER_HEADER_MEDIA_TYPE:
			apt_string_value_generate(&recog_header->media_type,value);
			break;
		case RECOGNIZER_HEADER_VER_BUFFER_UTTERANCE:
			apt_boolean_value_generate(recog_header->ver_buffer_utterance,value);
			break;
		case RECOGNIZER_HEADER_RECOGNITION_MODE:
			apt_string_value_generate(&recog_header->recognition_mode,value);
			break;
		case RECOGNIZER_HEADER_CANCEL_IF_QUEUE:
			apt_boolean_value_generate(recog_header->cancel_if_queue,value);
			break;
		case RECOGNIZER_HEADER_HOTWORD_MAX_DURATION:
			apt_size_value_generate(recog_header->hotword_max_duration,value);
			break;
		case RECOGNIZER_HEADER_HOTWORD_MIN_DURATION:
			apt_size_value_generate(recog_header->hotword_min_duration,value);
			break;
		case RECOGNIZER_HEADER_INTERPRET_TEXT:
			apt_string_value_generate(&recog_header->interpret_text,value);
			break;
		case RECOGNIZER_HEADER_DTMF_BUFFER_TIME:
			apt_size_value_generate(recog_header->dtmf_buffer_time,value);
			break;
		case RECOGNIZER_HEADER_CLEAR_DTMF_BUFFER:
			apt_boolean_value_generate(recog_header->clear_dtmf_buffer,value);
			break;
		case RECOGNIZER_HEADER_EARLY_NO_MATCH:
			apt_boolean_value_generate(recog_header->early_no_match,value);
			break;
		default:
			break;
	}
	return TRUE;
}

/** Generate MRCPv1 recognizer header */
static apt_bool_t mrcp_v1_recog_header_generate(mrcp_header_accessor_t *accessor, apr_size_t id, apt_text_stream_t *value)
{
	mrcp_recog_header_t *recog_header = accessor->data;
	if(id == RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD) {
		return apt_size_value_generate_from_float(recog_header->confidence_threshold,value);
	}
	else if(id == RECOGNIZER_HEADER_SENSITIVITY_LEVEL) {
		return apt_size_value_generate_from_float(recog_header->sensitivity_level,value);
	}
	else if(id == RECOGNIZER_HEADER_SPEED_VS_ACCURACY) {
		return apt_size_value_generate_from_float(recog_header->speed_vs_accuracy,value);
	}
	else if(id == RECOGNIZER_HEADER_COMPLETION_CAUSE) {
		return mrcp_completion_cause_generate(
			v1_completion_cause_string_table,
			RECOGNIZER_COMPLETION_CAUSE_COUNT,
			recog_header->completion_cause,
			value);
	}
	return mrcp_recog_header_generate(recog_header,id,value);
}

/** Generate MRCPv2 recognizer header */
static apt_bool_t mrcp_v2_recog_header_generate(mrcp_header_accessor_t *accessor, apr_size_t id, apt_text_stream_t *value)
{
	mrcp_recog_header_t *recog_header = accessor->data;
	if(id == RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD) {
		return apt_float_value_generate(recog_header->confidence_threshold,value);
	}
	else if(id == RECOGNIZER_HEADER_SENSITIVITY_LEVEL) {
		return apt_float_value_generate(recog_header->sensitivity_level,value);
	}
	else if(id == RECOGNIZER_HEADER_SPEED_VS_ACCURACY) {
		return apt_float_value_generate(recog_header->speed_vs_accuracy,value);
	}
	else if(id == RECOGNIZER_HEADER_COMPLETION_CAUSE) {
		return mrcp_completion_cause_generate(
			v2_completion_cause_string_table,
			RECOGNIZER_COMPLETION_CAUSE_COUNT,
			recog_header->completion_cause,
			value);
	}
	return mrcp_recog_header_generate(recog_header,id,value);
}

/** Duplicate MRCP recognizer header */
static apt_bool_t mrcp_recog_header_duplicate(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, apr_size_t id, apr_pool_t *pool)
{
	mrcp_recog_header_t *recog_header = accessor->data;
	const mrcp_recog_header_t *src_recog_header = src->data;
	apt_bool_t status = TRUE;

	if(!recog_header || !src_recog_header) {
		return FALSE;
	}
	
	switch(id) {
		case RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD:
			recog_header->confidence_threshold = src_recog_header->confidence_threshold;
			break;
		case RECOGNIZER_HEADER_SENSITIVITY_LEVEL:
			recog_header->sensitivity_level = src_recog_header->sensitivity_level;
			break;
		case RECOGNIZER_HEADER_SPEED_VS_ACCURACY:
			recog_header->speed_vs_accuracy = src_recog_header->speed_vs_accuracy;
			break;
		case RECOGNIZER_HEADER_N_BEST_LIST_LENGTH:
			recog_header->n_best_list_length = src_recog_header->n_best_list_length;
			break;
		case RECOGNIZER_HEADER_NO_INPUT_TIMEOUT:
			recog_header->no_input_timeout = src_recog_header->no_input_timeout;
			break;
		case RECOGNIZER_HEADER_RECOGNITION_TIMEOUT:
			recog_header->recognition_timeout = src_recog_header->recognition_timeout;
			break;
		case RECOGNIZER_HEADER_WAVEFORM_URI:
			apt_string_copy(&recog_header->waveform_uri,&src_recog_header->waveform_uri,pool);
			break;
		case RECOGNIZER_HEADER_COMPLETION_CAUSE:
			recog_header->completion_cause = src_recog_header->completion_cause;
			break;
		case RECOGNIZER_HEADER_RECOGNIZER_CONTEXT_BLOCK:
			apt_string_copy(&recog_header->recognizer_context_block,&src_recog_header->recognizer_context_block,pool);
			break;
		case RECOGNIZER_HEADER_START_INPUT_TIMERS:
			recog_header->start_input_timers = src_recog_header->start_input_timers;
			break;
		case RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT:
			recog_header->speech_complete_timeout = src_recog_header->speech_complete_timeout;
			break;
		case RECOGNIZER_HEADER_SPEECH_INCOMPLETE_TIMEOUT:
			recog_header->speech_incomplete_timeout = src_recog_header->speech_incomplete_timeout;
			break;
		case RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT:
			recog_header->dtmf_interdigit_timeout = src_recog_header->dtmf_interdigit_timeout;
			break;
		case RECOGNIZER_HEADER_DTMF_TERM_TIMEOUT:
			recog_header->dtmf_term_timeout = src_recog_header->dtmf_term_timeout;
			break;
		case RECOGNIZER_HEADER_DTMF_TERM_CHAR:
			recog_header->dtmf_term_char = src_recog_header->dtmf_term_char;
			break;
		case RECOGNIZER_HEADER_FAILED_URI:
			apt_string_copy(&recog_header->failed_uri,&src_recog_header->failed_uri,pool);
			break;
		case RECOGNIZER_HEADER_FAILED_URI_CAUSE:
			apt_string_copy(&recog_header->failed_uri_cause,&src_recog_header->failed_uri_cause,pool);
			break;
		case RECOGNIZER_HEADER_SAVE_WAVEFORM:
			recog_header->save_waveform = src_recog_header->save_waveform;
			break;
		case RECOGNIZER_HEADER_NEW_AUDIO_CHANNEL:
			recog_header->new_audio_channel = src_recog_header->new_audio_channel;
			break;
		case RECOGNIZER_HEADER_SPEECH_LANGUAGE:
			apt_string_copy(&recog_header->speech_language,&src_recog_header->speech_language,pool);
			break;
		case RECOGNIZER_HEADER_INPUT_TYPE:
			apt_string_copy(&recog_header->input_type,&src_recog_header->input_type,pool);
			break;
		case RECOGNIZER_HEADER_INPUT_WAVEFORM_URI:
			apt_string_copy(&recog_header->input_waveform_uri,&src_recog_header->input_waveform_uri,pool);
			break;
		case RECOGNIZER_HEADER_COMPLETION_REASON:
			apt_string_copy(&recog_header->completion_reason,&src_recog_header->completion_reason,pool);
			break;
		case RECOGNIZER_HEADER_MEDIA_TYPE:
			apt_string_copy(&recog_header->media_type,&src_recog_header->media_type,pool);
			break;
		case RECOGNIZER_HEADER_VER_BUFFER_UTTERANCE:
			recog_header->ver_buffer_utterance = src_recog_header->ver_buffer_utterance;
			break;
		case RECOGNIZER_HEADER_RECOGNITION_MODE:
			apt_string_copy(&recog_header->recognition_mode,&src_recog_header->recognition_mode,pool);
			break;
		case RECOGNIZER_HEADER_CANCEL_IF_QUEUE:
			recog_header->cancel_if_queue = src_recog_header->cancel_if_queue;
			break;
		case RECOGNIZER_HEADER_HOTWORD_MAX_DURATION:
			recog_header->hotword_max_duration = src_recog_header->hotword_max_duration;
			break;
		case RECOGNIZER_HEADER_HOTWORD_MIN_DURATION:
			recog_header->hotword_min_duration = src_recog_header->hotword_min_duration;
			break;
		case RECOGNIZER_HEADER_INTERPRET_TEXT:
			apt_string_copy(&recog_header->interpret_text,&src_recog_header->interpret_text,pool);
			break;
		case RECOGNIZER_HEADER_DTMF_BUFFER_TIME:
			recog_header->dtmf_buffer_time = src_recog_header->dtmf_buffer_time;
			break;
		case RECOGNIZER_HEADER_CLEAR_DTMF_BUFFER:
			recog_header->clear_dtmf_buffer = src_recog_header->clear_dtmf_buffer;
			break;
		case RECOGNIZER_HEADER_EARLY_NO_MATCH:
			recog_header->early_no_match = src_recog_header->early_no_match;
			break;
		default:
			status = FALSE;
	}
	return status;
}

static APR_INLINE const apt_str_table_item_t* recog_header_string_table_get(mrcp_version_e version)
{
	if(version == MRCP_VERSION_1) {
		return v1_recog_header_string_table;
	}
	return v2_recog_header_string_table;
}

static const mrcp_header_vtable_t v1_vtable = {
	mrcp_recog_header_allocate,
	NULL, /* nothing to destroy */
	mrcp_v1_recog_header_parse,
	mrcp_v1_recog_header_generate,
	mrcp_recog_header_duplicate,
	v1_recog_header_string_table,
	RECOGNIZER_HEADER_COUNT
};

static const mrcp_header_vtable_t v2_vtable = {
	mrcp_recog_header_allocate,
	NULL, /* nothing to destroy */
	mrcp_v2_recog_header_parse,
	mrcp_v2_recog_header_generate,
	mrcp_recog_header_duplicate,
	v2_recog_header_string_table,
	RECOGNIZER_HEADER_COUNT
};

const mrcp_header_vtable_t* mrcp_recog_header_vtable_get(mrcp_version_e version)
{
	if(version == MRCP_VERSION_1) {
		return &v1_vtable;
	}
	return &v2_vtable;
}

MRCP_DECLARE(const apt_str_t*) mrcp_recog_completion_cause_get(mrcp_recog_completion_cause_e completion_cause, mrcp_version_e version)
{
	const apt_str_table_item_t *table = v2_completion_cause_string_table;
	if(version == MRCP_VERSION_1) {
		table = v1_completion_cause_string_table;
	}

	return apt_string_table_str_get(table,RECOGNIZER_COMPLETION_CAUSE_COUNT,completion_cause);
}
