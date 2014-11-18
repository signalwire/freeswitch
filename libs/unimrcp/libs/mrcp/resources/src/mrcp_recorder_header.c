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
 * $Id: mrcp_recorder_header.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mrcp_recorder_header.h"

/** String table of recorder header fields (mrcp_recorder_header_id) */
static const apt_str_table_item_t recorder_header_string_table[] = {
	{{"Sensitivity-Level",    17},3},
	{{"No-Input-Timeout",     16},2},
	{{"Completion-Cause",     16},16},
	{{"Completion-Reason",    17},11},
	{{"Failed-Uri",           10},10},
	{{"Failed-Uri-Cause",     16},16},
	{{"Record-Uri",           10},0},
	{{"Media-Type",           10},2},
	{{"Max-Time",              8},2},
	{{"Trim-Length",          11},0},
	{{"Final-Silence",        13},1},
	{{"Capture-On-Speech",    17},2},
	{{"Ver-Buffer-Utterance", 20},0},
	{{"Start-Input-Timers",   18},1},
	{{"New-Audio-Channel",    17},2}
};

/** String table of recorder completion-cause fields (mrcp_recorder_completion_cause_e) */
static const apt_str_table_item_t completion_cause_string_table[] = {
	{{"success-silence",  15},8},
	{{"success-maxtime",  15},8},
	{{"no-input-timeout", 16},0},
	{{"uri-failure",      11},0},
	{{"error",             5},0}
};


/** Initialize recorder header */
static void mrcp_recorder_header_init(mrcp_recorder_header_t *recorder_header)
{
	recorder_header->sensitivity_level = 0.0;
	recorder_header->no_input_timeout = 0;
	recorder_header->completion_cause = RECORDER_COMPLETION_CAUSE_COUNT;
	apt_string_reset(&recorder_header->completion_reason);
	apt_string_reset(&recorder_header->failed_uri);
	apt_string_reset(&recorder_header->failed_uri_cause);
	apt_string_reset(&recorder_header->record_uri);
	apt_string_reset(&recorder_header->media_type);
	recorder_header->max_time = 0;
	recorder_header->trim_length = 0;
	recorder_header->final_silence = 0;
	recorder_header->capture_on_speech = FALSE;
	recorder_header->ver_buffer_utterance = FALSE;
	recorder_header->start_input_timers = FALSE;
	recorder_header->new_audio_channel = FALSE;
}

/** Allocate MRCP recorder header */
static void* mrcp_recorder_header_allocate(mrcp_header_accessor_t *accessor, apr_pool_t *pool)
{
	mrcp_recorder_header_t *recorder_header = apr_palloc(pool,sizeof(mrcp_recorder_header_t));
	mrcp_recorder_header_init(recorder_header);
	accessor->data = recorder_header;
	return accessor->data;
}

/** Parse MRCP recorder header */
static apt_bool_t mrcp_recorder_header_parse(mrcp_header_accessor_t *accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	apt_bool_t status = TRUE;
	mrcp_recorder_header_t *recorder_header = accessor->data;
	switch(id) {
		case RECORDER_HEADER_SENSITIVITY_LEVEL:
			recorder_header->sensitivity_level = apt_float_value_parse(value);
			break;
		case RECORDER_HEADER_NO_INPUT_TIMEOUT:
			recorder_header->no_input_timeout = apt_size_value_parse(value);
			break;
		case RECORDER_HEADER_COMPLETION_CAUSE:
			recorder_header->completion_cause = apt_size_value_parse(value);
			break;
		case RECORDER_HEADER_COMPLETION_REASON:
			recorder_header->completion_reason = *value;
			break;
		case RECORDER_HEADER_FAILED_URI:
			recorder_header->failed_uri = *value;
			break;
		case RECORDER_HEADER_FAILED_URI_CAUSE:
			recorder_header->failed_uri_cause = *value;
			break;
		case RECORDER_HEADER_RECORD_URI:
			recorder_header->record_uri = *value;
			break;
		case RECORDER_HEADER_MEDIA_TYPE:
			recorder_header->media_type = *value;
			break;
		case RECORDER_HEADER_MAX_TIME:
			recorder_header->max_time = apt_size_value_parse(value);
			break;
		case RECORDER_HEADER_TRIM_LENGTH:
			recorder_header->trim_length = apt_size_value_parse(value);
			break;
		case RECORDER_HEADER_FINAL_SILENCE:
			recorder_header->final_silence = apt_size_value_parse(value);
			break;
		case RECORDER_HEADER_CAPTURE_ON_SPEECH:
			apt_boolean_value_parse(value,&recorder_header->capture_on_speech);
			break;
		case RECORDER_HEADER_VER_BUFFER_UTTERANCE:
			apt_boolean_value_parse(value,&recorder_header->ver_buffer_utterance);
			break;
		case RECORDER_HEADER_START_INPUT_TIMERS:
			apt_boolean_value_parse(value,&recorder_header->start_input_timers);
			break;
		case RECORDER_HEADER_NEW_AUDIO_CHANNEL:
			apt_boolean_value_parse(value,&recorder_header->new_audio_channel);
			break;
		default:
			status = FALSE;
	}
	return status;
}

/** Generate MRCP recorder header */
static apt_bool_t mrcp_recorder_header_generate(const mrcp_header_accessor_t *accessor, apr_size_t id, apt_str_t *value, apr_pool_t *pool)
{
	mrcp_recorder_header_t *recorder_header = accessor->data;
	switch(id) {
		case RECORDER_HEADER_SENSITIVITY_LEVEL:
			apt_float_value_generate(recorder_header->sensitivity_level,value,pool);
			break;
		case RECORDER_HEADER_NO_INPUT_TIMEOUT:
			apt_size_value_generate(recorder_header->no_input_timeout,value,pool);
			break;
		case RECORDER_HEADER_COMPLETION_CAUSE:
		{
			apt_completion_cause_generate(
				completion_cause_string_table,
				RECORDER_COMPLETION_CAUSE_COUNT,
				recorder_header->completion_cause,
				value,
				pool);
			break;
		}
		case RECORDER_HEADER_COMPLETION_REASON:
			*value = recorder_header->completion_reason;
			break;
		case RECORDER_HEADER_FAILED_URI:
			*value = recorder_header->failed_uri;
			break;
		case RECORDER_HEADER_FAILED_URI_CAUSE:
			*value = recorder_header->failed_uri_cause;
			break;
		case RECORDER_HEADER_RECORD_URI:
			*value = recorder_header->record_uri;
			break;
		case RECORDER_HEADER_MEDIA_TYPE:
			*value = recorder_header->media_type;
			break;
		case RECORDER_HEADER_MAX_TIME:
			apt_size_value_generate(recorder_header->max_time,value,pool);
			break;
		case RECORDER_HEADER_TRIM_LENGTH:
			apt_size_value_generate(recorder_header->trim_length,value,pool);
			break;
		case RECORDER_HEADER_FINAL_SILENCE:
			apt_size_value_generate(recorder_header->final_silence,value,pool);
			break;
		case RECORDER_HEADER_CAPTURE_ON_SPEECH:
			apt_boolean_value_generate(recorder_header->capture_on_speech,value,pool);
			break;
		case RECORDER_HEADER_VER_BUFFER_UTTERANCE:
			apt_boolean_value_generate(recorder_header->ver_buffer_utterance,value,pool);
			break;
		case RECORDER_HEADER_START_INPUT_TIMERS:
			apt_boolean_value_generate(recorder_header->start_input_timers,value,pool);
			break;
		case RECORDER_HEADER_NEW_AUDIO_CHANNEL:
			apt_boolean_value_generate(recorder_header->new_audio_channel,value,pool);
			break;
		default:
			break;
	}
	return TRUE;
}

/** Duplicate MRCP recorder header */
static apt_bool_t mrcp_recorder_header_duplicate(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	mrcp_recorder_header_t *recorder_header = accessor->data;
	const mrcp_recorder_header_t *src_recorder_header = src->data;
	apt_bool_t status = TRUE;

	if(!recorder_header || !src_recorder_header) {
		return FALSE;
	}
	
	switch(id) {
		case RECORDER_HEADER_SENSITIVITY_LEVEL:
			recorder_header->sensitivity_level = src_recorder_header->sensitivity_level;
			break;
		case RECORDER_HEADER_NO_INPUT_TIMEOUT:
			recorder_header->no_input_timeout = src_recorder_header->no_input_timeout;
			break;
		case RECORDER_HEADER_COMPLETION_CAUSE:
			recorder_header->completion_cause = src_recorder_header->completion_cause;
			break;
		case RECORDER_HEADER_COMPLETION_REASON:
			recorder_header->completion_reason = *value;
			break;
		case RECORDER_HEADER_FAILED_URI:
			recorder_header->failed_uri = *value;
			break;
		case RECORDER_HEADER_FAILED_URI_CAUSE:
			recorder_header->failed_uri_cause = *value;
			break;
		case RECORDER_HEADER_RECORD_URI:
			recorder_header->record_uri = *value;
			break;
		case RECORDER_HEADER_MEDIA_TYPE:
			recorder_header->media_type = *value;
			break;
		case RECORDER_HEADER_MAX_TIME:
			recorder_header->max_time = src_recorder_header->max_time;
			break;
		case RECORDER_HEADER_TRIM_LENGTH:
			recorder_header->trim_length = src_recorder_header->trim_length;
			break;
		case RECORDER_HEADER_FINAL_SILENCE:
			recorder_header->final_silence = src_recorder_header->final_silence;
			break;
		case RECORDER_HEADER_CAPTURE_ON_SPEECH:
			recorder_header->capture_on_speech = src_recorder_header->capture_on_speech;
			break;
		case RECORDER_HEADER_VER_BUFFER_UTTERANCE:
			recorder_header->ver_buffer_utterance = src_recorder_header->ver_buffer_utterance;
			break;
		case RECORDER_HEADER_START_INPUT_TIMERS:
			recorder_header->start_input_timers = src_recorder_header->start_input_timers;
			break;
		case RECORDER_HEADER_NEW_AUDIO_CHANNEL:
			recorder_header->new_audio_channel = src_recorder_header->new_audio_channel;
			break;
		default:
			status = FALSE;
	}
	return status;
}

static const mrcp_header_vtable_t vtable = {
	mrcp_recorder_header_allocate,
	NULL, /* nothing to destroy */
	mrcp_recorder_header_parse,
	mrcp_recorder_header_generate,
	mrcp_recorder_header_duplicate,
	recorder_header_string_table,
	RECORDER_HEADER_COUNT
};

const mrcp_header_vtable_t* mrcp_recorder_header_vtable_get(mrcp_version_e version)
{
	return &vtable;
}

MRCP_DECLARE(const apt_str_t*) mrcp_recorder_completion_cause_get(
									mrcp_recorder_completion_cause_e completion_cause, 
									mrcp_version_e version)
{
	return apt_string_table_str_get(completion_cause_string_table,RECORDER_COMPLETION_CAUSE_COUNT,completion_cause);
}
