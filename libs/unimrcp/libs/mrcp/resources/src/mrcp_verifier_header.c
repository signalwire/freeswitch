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

#include "mrcp_verifier_header.h"

/** String table of MRCP verifier header fields (mrcp_verifier_header_id) */
static const apt_str_table_item_t verifier_header_string_table[] = {
	{{"Repository-URI",              14},0},
	{{"Voiceprint-Identifier",       21},12},
	{{"Verification-Mode",           17},6},
	{{"Adapt-Model",                 11},1},
	{{"Abort-Model",                 11},11},
	{{"Min-Verification-Score",      22},1},
	{{"Num-Min-Verification-Phrases",28},6},
	{{"Num-Max-Verification-Phrases",28},5},
	{{"No-Input-Timeout",            16},2},
	{{"Save-Waveform",               13},4},
	{{"Media-Type",                  10},2},
	{{"Waveform-URI",                12},0},
	{{"Voiceprint-Exists",           17},11},
	{{"Ver-Buffer-Utterance",        20},4},
	{{"Input-Waveform-URI",          18},0},
	{{"Completion-Cause",            16},11},
	{{"Completion-Reason",           17},15},
	{{"Speech-Complete-Timeout",     23},1},
	{{"New-Audio-Channel",           17},2},
	{{"Abort-Verification",          18},6},
	{{"Start-Input-Timers",          18},1}
};

/** String table of MRCP verifier completion-cause fields (mrcp_verifier_completion_cause_e) */
static const apt_str_table_item_t completion_cause_string_table[] = {
	{{"success",                 7},2},
	{{"error",                   5},0},
	{{"no-input-timeout",       16},0},
	{{"too-much-speech-timeout",23},0},
	{{"speech-too-early",       16},9},
	{{"buffer-empty",           12},0},
	{{"out-of-sequence",        15},0},
	{{"repository-uri-failure", 22},15},
	{{"repository-uri-missing", 22},15},
	{{"voiceprint-id-missing",  21},14},
	{{"voiceprint-id-not-exist",23},14},
	{{"speech-not-usable",      17},7}
};


/** Initialize verifier header */
static void mrcp_verifier_header_init(mrcp_verifier_header_t *verifier_header)
{
	apt_string_reset(&verifier_header->repository_uri);
	apt_string_reset(&verifier_header->voiceprint_identifier);
	apt_string_reset(&verifier_header->verification_mode);
	verifier_header->adapt_model = FALSE;
	verifier_header->abort_model = FALSE;
	verifier_header->min_verification_score = 0.0;
	verifier_header->num_min_verification_phrases = 0;
	verifier_header->num_max_verification_phrases = 0;
	verifier_header->no_input_timeout = 0;
	verifier_header->save_waveform = FALSE;
	apt_string_reset(&verifier_header->media_type);
	apt_string_reset(&verifier_header->waveform_uri);
	verifier_header->voiceprint_exists = FALSE;
	verifier_header->ver_buffer_utterance = FALSE;
	apt_string_reset(&verifier_header->input_waveform_uri);
	verifier_header->completion_cause = VERIFIER_COMPLETION_CAUSE_UNKNOWN;
	apt_string_reset(&verifier_header->completion_reason);
	verifier_header->speech_complete_timeout = 0;
	verifier_header->new_audio_channel = FALSE;
	verifier_header->abort_verification = FALSE;
	verifier_header->start_input_timers = FALSE;
}

/** Allocate MRCP verifier header */
static void* mrcp_verifier_header_allocate(mrcp_header_accessor_t *accessor, apr_pool_t *pool)
{
	mrcp_verifier_header_t *verifier_header = apr_palloc(pool,sizeof(mrcp_verifier_header_t));
	mrcp_verifier_header_init(verifier_header);
	accessor->data = verifier_header;
	return accessor->data;
}

/** Parse MRCP verifier header */
static apt_bool_t mrcp_verifier_header_parse(mrcp_header_accessor_t *accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	mrcp_verifier_header_t *verifier_header = accessor->data;
	apt_bool_t status = TRUE;
	switch(id) {
		case VERIFIER_HEADER_REPOSITORY_URI:
			verifier_header->repository_uri = *value;
			break;
		case VERIFIER_HEADER_VOICEPRINT_IDENTIFIER:
			verifier_header->voiceprint_identifier = *value;
			break;
		case VERIFIER_HEADER_VERIFICATION_MODE:
			verifier_header->verification_mode = *value;
			break;
		case VERIFIER_HEADER_ADAPT_MODEL:
			apt_boolean_value_parse(value,&verifier_header->adapt_model);
			break;
		case VERIFIER_HEADER_ABORT_MODEL:
			apt_boolean_value_parse(value,&verifier_header->abort_model);
			break;
		case VERIFIER_HEADER_MIN_VERIFICATION_SCORE:
			verifier_header->min_verification_score = apt_float_value_parse(value);
			break;
		case VERIFIER_HEADER_NUM_MIN_VERIFICATION_PHRASES:
			verifier_header->num_min_verification_phrases = apt_size_value_parse(value);
			break;
		case VERIFIER_HEADER_NUM_MAX_VERIFICATION_PHRASES:
			verifier_header->num_max_verification_phrases = apt_size_value_parse(value);
			break;
		case VERIFIER_HEADER_NO_INPUT_TIMEOUT:
			verifier_header->no_input_timeout = apt_size_value_parse(value);
			break;
		case VERIFIER_HEADER_SAVE_WAVEFORM:
			apt_boolean_value_parse(value,&verifier_header->save_waveform);
			break;
		case VERIFIER_HEADER_MEDIA_TYPE:
			verifier_header->media_type = *value;
			break;
		case VERIFIER_HEADER_WAVEFORM_URI:
			verifier_header->waveform_uri = *value;
			break;
		case VERIFIER_HEADER_VOICEPRINT_EXISTS:
			apt_boolean_value_parse(value,&verifier_header->voiceprint_exists);
			break;
		case VERIFIER_HEADER_VER_BUFFER_UTTERANCE:
			apt_boolean_value_parse(value,&verifier_header->ver_buffer_utterance);
			break;
		case VERIFIER_HEADER_INPUT_WAVEFORM_URI:
			verifier_header->input_waveform_uri = *value;
			break;
		case VERIFIER_HEADER_COMPLETION_CAUSE:
			verifier_header->completion_cause = apt_size_value_parse(value);
			break;
		case VERIFIER_HEADER_COMPLETION_REASON:
			verifier_header->completion_reason = *value;
			break;
		case VERIFIER_HEADER_SPEECH_COMPLETE_TIMEOUT:
			verifier_header->speech_complete_timeout = apt_size_value_parse(value);
			break;
		case VERIFIER_HEADER_NEW_AUDIO_CHANNEL:
			apt_boolean_value_parse(value,&verifier_header->new_audio_channel);
			break;
		case VERIFIER_HEADER_ABORT_VERIFICATION:
			apt_boolean_value_parse(value,&verifier_header->abort_verification);
			break;
		case VERIFIER_HEADER_START_INPUT_TIMERS:
			apt_boolean_value_parse(value,&verifier_header->start_input_timers);
			break;
		default:
			status = FALSE;
	}
	return status;
}

/** Generate MRCP verifier header */
static apt_bool_t mrcp_verifier_header_generate(const mrcp_header_accessor_t *accessor, apr_size_t id, apt_str_t *value, apr_pool_t *pool)
{
	mrcp_verifier_header_t *verifier_header = accessor->data;
	switch(id) {
		case VERIFIER_HEADER_REPOSITORY_URI:
			*value = verifier_header->repository_uri;
			break;
		case VERIFIER_HEADER_VOICEPRINT_IDENTIFIER:
			*value = verifier_header->voiceprint_identifier;
			break;
		case VERIFIER_HEADER_VERIFICATION_MODE:
			*value = verifier_header->verification_mode;
			break;
		case VERIFIER_HEADER_ADAPT_MODEL:
			apt_boolean_value_generate(verifier_header->adapt_model,value,pool);
			break;
		case VERIFIER_HEADER_ABORT_MODEL:
			apt_boolean_value_generate(verifier_header->abort_model,value,pool);
			break;
		case VERIFIER_HEADER_MIN_VERIFICATION_SCORE:
			apt_float_value_generate(verifier_header->min_verification_score,value,pool);
			break;
		case VERIFIER_HEADER_NUM_MIN_VERIFICATION_PHRASES:
			apt_size_value_generate(verifier_header->num_min_verification_phrases,value,pool);
			break;
		case VERIFIER_HEADER_NUM_MAX_VERIFICATION_PHRASES:
			apt_size_value_generate(verifier_header->num_max_verification_phrases,value,pool);
			break;
		case VERIFIER_HEADER_NO_INPUT_TIMEOUT:
			apt_size_value_generate(verifier_header->no_input_timeout,value,pool);
			break;
		case VERIFIER_HEADER_SAVE_WAVEFORM:
			apt_boolean_value_generate(verifier_header->save_waveform,value,pool);
			break;
		case VERIFIER_HEADER_MEDIA_TYPE:
			*value = verifier_header->media_type;
			break;
		case VERIFIER_HEADER_WAVEFORM_URI:
			*value = verifier_header->waveform_uri;
			break;
		case VERIFIER_HEADER_VOICEPRINT_EXISTS:
			apt_boolean_value_generate(verifier_header->voiceprint_exists,value,pool);
			break;
		case VERIFIER_HEADER_VER_BUFFER_UTTERANCE:
			apt_boolean_value_generate(verifier_header->ver_buffer_utterance,value,pool);
			break;
		case VERIFIER_HEADER_INPUT_WAVEFORM_URI:
			*value = verifier_header->input_waveform_uri;
			break;
		case VERIFIER_HEADER_COMPLETION_CAUSE:
			apt_completion_cause_generate(
				completion_cause_string_table,
				VERIFIER_COMPLETION_CAUSE_COUNT,
				verifier_header->completion_cause,
				value,
				pool);
			break;
		case VERIFIER_HEADER_COMPLETION_REASON:
			*value = verifier_header->completion_reason;
			break;
		case VERIFIER_HEADER_SPEECH_COMPLETE_TIMEOUT:
			apt_size_value_generate(verifier_header->speech_complete_timeout,value,pool);
			break;
		case VERIFIER_HEADER_NEW_AUDIO_CHANNEL:
			apt_boolean_value_generate(verifier_header->new_audio_channel,value,pool);
			break;
		case VERIFIER_HEADER_ABORT_VERIFICATION:
			apt_boolean_value_generate(verifier_header->abort_verification,value,pool);
			break;
		case VERIFIER_HEADER_START_INPUT_TIMERS:
			apt_boolean_value_generate(verifier_header->start_input_timers,value,pool);
			break;
		default:
			break;
	}
	return TRUE;
}

/** Duplicate MRCP verifier header */
static apt_bool_t mrcp_verifier_header_duplicate(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	mrcp_verifier_header_t *verifier_header = accessor->data;
	const mrcp_verifier_header_t *src_verifier_header = src->data;
	apt_bool_t status = TRUE;

	if(!verifier_header || !src_verifier_header) {
		return FALSE;
	}
	
	switch(id) {
		case VERIFIER_HEADER_REPOSITORY_URI:
			verifier_header->repository_uri = *value;
			break;
		case VERIFIER_HEADER_VOICEPRINT_IDENTIFIER:
			verifier_header->voiceprint_identifier = *value;
			break;
		case VERIFIER_HEADER_VERIFICATION_MODE:
			verifier_header->verification_mode = *value;
			break;
		case VERIFIER_HEADER_ADAPT_MODEL:
			verifier_header->adapt_model = src_verifier_header->adapt_model;
			break;
		case VERIFIER_HEADER_ABORT_MODEL:
			verifier_header->abort_model = src_verifier_header->abort_model;
			break;
		case VERIFIER_HEADER_MIN_VERIFICATION_SCORE:
			verifier_header->min_verification_score = src_verifier_header->min_verification_score;
			break;
		case VERIFIER_HEADER_NUM_MIN_VERIFICATION_PHRASES:
			verifier_header->num_min_verification_phrases = src_verifier_header->num_min_verification_phrases;
			break;
		case VERIFIER_HEADER_NUM_MAX_VERIFICATION_PHRASES:
			verifier_header->num_max_verification_phrases = src_verifier_header->num_max_verification_phrases;
			break;
		case VERIFIER_HEADER_NO_INPUT_TIMEOUT:
			verifier_header->no_input_timeout = src_verifier_header->no_input_timeout;
			break;
		case VERIFIER_HEADER_SAVE_WAVEFORM:
			verifier_header->save_waveform = src_verifier_header->save_waveform;
			break;
		case VERIFIER_HEADER_MEDIA_TYPE:
			verifier_header->media_type = *value;
			break;
		case VERIFIER_HEADER_WAVEFORM_URI:
			verifier_header->waveform_uri = *value;
			break;
		case VERIFIER_HEADER_VOICEPRINT_EXISTS:
			verifier_header->voiceprint_exists = src_verifier_header->voiceprint_exists;
			break;
		case VERIFIER_HEADER_VER_BUFFER_UTTERANCE:
			verifier_header->ver_buffer_utterance = src_verifier_header->ver_buffer_utterance;
			break;
		case VERIFIER_HEADER_INPUT_WAVEFORM_URI:
			verifier_header->input_waveform_uri = *value;
			break;
		case VERIFIER_HEADER_COMPLETION_CAUSE:
			verifier_header->completion_cause = src_verifier_header->completion_cause;
			break;
		case VERIFIER_HEADER_COMPLETION_REASON:
			verifier_header->completion_reason = *value;
			break;
		case VERIFIER_HEADER_SPEECH_COMPLETE_TIMEOUT:
			verifier_header->speech_complete_timeout = src_verifier_header->speech_complete_timeout;
			break;
		case VERIFIER_HEADER_NEW_AUDIO_CHANNEL:
			verifier_header->new_audio_channel = src_verifier_header->new_audio_channel;
			break;
		case VERIFIER_HEADER_ABORT_VERIFICATION:
			verifier_header->abort_verification = src_verifier_header->abort_verification;
			break;
		case VERIFIER_HEADER_START_INPUT_TIMERS:
			verifier_header->start_input_timers = src_verifier_header->start_input_timers;
			break;
		default:
			status = FALSE;
	}
	return status;
}

static const mrcp_header_vtable_t header_vtable = {
	mrcp_verifier_header_allocate,
	NULL, /* nothing to destroy */
	mrcp_verifier_header_parse,
	mrcp_verifier_header_generate,
	mrcp_verifier_header_duplicate,
	verifier_header_string_table,
	VERIFIER_HEADER_COUNT
};

const mrcp_header_vtable_t* mrcp_verifier_header_vtable_get(mrcp_version_e version)
{
	return &header_vtable;
}

MRCP_DECLARE(const apt_str_t*) mrcp_verifier_completion_cause_get(mrcp_verifier_completion_cause_e completion_cause, mrcp_version_e version)
{
	const apt_str_table_item_t *table = completion_cause_string_table;
	return apt_string_table_str_get(table,VERIFIER_COMPLETION_CAUSE_COUNT,completion_cause);
}
