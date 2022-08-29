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

#include "mrcp_synth_header.h"

/** String table of MRCP synthesizer header fields (mrcp_synthesizer_header_id) */
static const apt_str_table_item_t synth_header_string_table[] = {
	{{"Jump-Size",            9},0},
	{{"Kill-On-Barge-In",    16},0},
	{{"Speaker-Profile",     15},8},
	{{"Completion-Cause",    16},16},
	{{"Completion-Reason",   17},13},
	{{"Voice-Gender",        12},6},
	{{"Voice-Age",            9},6},
	{{"Voice-Variant",       13},6},
	{{"Voice-Name",          10},8},
	{{"Prosody-Volume",      14},8},
	{{"Prosody-Rate",        12},12},
	{{"Speech-Marker",       13},7},
	{{"Speech-Language",     15},7},
	{{"Fetch-Hint",          10},2},
	{{"Audio-Fetch-Hint",    16},0},
	{{"Failed-Uri",          10},10},
	{{"Failed-Uri_Cause",    16},10},
	{{"Speak-Restart",       13},13},
	{{"Speak-Length",        12},6},
	{{"Load-Lexicon",        12},2},
	{{"Lexicon-Search-Order",20},2}
};

/** String table of MRCP speech-unit fields (mrcp_speech_unit_t) */
static const apt_str_table_item_t speech_unit_string_table[] = {
	{{"Second",   6},2},
	{{"Word",     4},0},
	{{"Sentence", 8},2},
	{{"Paragraph",9},0}
};

/** String table of MRCP voice-gender fields (mrcp_voice_gender_t) */
static const apt_str_table_item_t voice_gender_string_table[] = {
	{{"male",   4},0},
	{{"female", 6},0},
	{{"neutral",7},0}
};

/** String table of MRCP prosody-volume fields (mrcp_prosody_volume_t) */
static const apt_str_table_item_t prosody_volume_string_table[] = {
	{{"silent", 6},1},
	{{"x-soft", 6},2},
	{{"soft",   4},3},
	{{"medium", 6},0},
	{{"loud",   4},0},
	{{"x-loud", 6},5},
	{{"default",7},0} 
};

/** String table of MRCP prosody-rate fields (mrcp_prosody_rate_t) */
static const apt_str_table_item_t prosody_rate_string_table[] = {
	{{"x-slow", 6},3},
	{{"slow",   4},0},
	{{"medium", 6},0},
	{{"fast",   4},0},
	{{"x-fast", 6},4},
	{{"default",7},0}
};

/** String table of MRCP synthesizer completion-cause fields (mrcp_synthesizer_completion_cause_t) */
static const apt_str_table_item_t completion_cause_string_table[] = {
	{{"normal",               6},0},
	{{"barge-in",             8},0},
	{{"parse-failure",       13},0},
	{{"uri-failure",         11},0},
	{{"error",                5},0},
	{{"language-unsupported",20},4},
	{{"lexicon-load-failure",20},1},
	{{"cancelled",            9},0}
};


static APR_INLINE apr_size_t apt_string_table_value_parse(const apt_str_table_item_t *string_table, apr_size_t count, const apt_str_t *value)
{
	return apt_string_table_id_find(string_table,count,value);
}

static apt_bool_t apt_string_table_value_pgenerate(const apt_str_table_item_t *string_table, apr_size_t count, apr_size_t id, apt_str_t *str, apr_pool_t *pool)
{
	const apt_str_t *name = apt_string_table_str_get(string_table,count,id);
	if(!name) {
		return FALSE;
	}

	apt_string_copy(str,name,pool);
	return TRUE;
}

/** Parse MRCP prosody-rate value */
static apt_bool_t mrcp_prosody_param_rate_parse(mrcp_prosody_rate_t *prosody_rate, const apt_str_t *value, apr_pool_t *pool)
{
	if(!value->length) {
		return FALSE;
	}

	/** For the rate attribute, relative changes are a number. (not preceded by a "+" or "-")(w3c ssml)*/
	if('0'<=value->buf[0] && value->buf[0]<='9') {
		prosody_rate->type = PROSODY_RATE_TYPE_RELATIVE_CHANGE;
	}
	else {
		prosody_rate->type = PROSODY_RATE_TYPE_LABEL;
	}

	if(prosody_rate->type == PROSODY_RATE_TYPE_RELATIVE_CHANGE) {
		prosody_rate->value.relative = apt_float_value_parse(value);
	}
	else {
		prosody_rate->value.label = apt_string_table_value_parse(prosody_rate_string_table,PROSODY_RATE_COUNT,value);
	}

	return TRUE;
}

/** Generate MRCP prosody-rate value */
static apt_bool_t mrcp_prosody_rate_generate(mrcp_prosody_rate_t *prosody_rate, apt_str_t *str, apr_pool_t *pool)
{
	if(prosody_rate->type == PROSODY_RATE_TYPE_LABEL) {
		apt_string_table_value_pgenerate(
			prosody_rate_string_table,
			PROSODY_RATE_COUNT,
			prosody_rate->value.label,
			str,
			pool);
	}
	else {
		apt_float_value_generate(prosody_rate->value.relative,str,pool);
	}

	return TRUE;
}

/** Parse MRCP prosody-volume value */
static apt_bool_t mrcp_prosody_param_volume_parse(mrcp_prosody_volume_t *prosody_volume, const apt_str_t *value, apr_pool_t *pool)
{
	if(!value->length) {
		return FALSE;
	}

	/** For the volume attribute, relative changes are a number preceded by "+" or "-" (w3c ssml)*/
	if(value->buf[0]=='+' || value->buf[0]=='-') {
		prosody_volume->type = PROSODY_VOLUME_TYPE_RELATIVE_CHANGE;
	}
	else if('0'<=value->buf[0] && value->buf[0]<='9') {
		prosody_volume->type = PROSODY_VOLUME_TYPE_NUMERIC;
	}
	else {
		prosody_volume->type = PROSODY_VOLUME_TYPE_LABEL;
	}

	if(prosody_volume->type == PROSODY_VOLUME_TYPE_RELATIVE_CHANGE) {
		prosody_volume->value.relative = apt_float_value_parse(value);
	}
	else if(prosody_volume->type == PROSODY_VOLUME_TYPE_NUMERIC) {
		prosody_volume->value.numeric = apt_float_value_parse(value);
	}
	else {
		prosody_volume->value.label = apt_string_table_value_parse(prosody_volume_string_table,PROSODY_VOLUME_COUNT,value);
	}

	return TRUE;
}

/** Generate MRCP prosody-volume value */
static apt_bool_t mrcp_prosody_volume_generate(mrcp_prosody_volume_t *prosody_volume, apt_str_t *str, apr_pool_t *pool)
{
	if(prosody_volume->type == PROSODY_VOLUME_TYPE_LABEL) {
		apt_string_table_value_pgenerate(
			prosody_volume_string_table,
			PROSODY_VOLUME_COUNT,
			prosody_volume->value.label,
			str,
			pool);
	}
	else if (prosody_volume->type == PROSODY_VOLUME_TYPE_NUMERIC) {
		apt_float_value_generate(prosody_volume->value.numeric,str,pool);
	}
	else {
		apt_float_value_generate(prosody_volume->value.relative,str,pool);
	}

	return TRUE;
}

/** Parse MRCP speech-length value */
static apt_bool_t mrcp_speech_length_value_parse(mrcp_speech_length_value_t *speech_length, const apt_str_t *value, apr_pool_t *pool)
{
	if(!value->length) {
		return FALSE;
	}

	switch(*value->buf) {
		case '+': speech_length->type = SPEECH_LENGTH_TYPE_NUMERIC_POSITIVE; break;
		case '-': speech_length->type = SPEECH_LENGTH_TYPE_NUMERIC_NEGATIVE; break;
		default : speech_length->type = SPEECH_LENGTH_TYPE_TEXT;
	}

	if(speech_length->type == SPEECH_LENGTH_TYPE_TEXT) {
		apt_string_copy(&speech_length->value.tag,value,pool);
	}
	else {
		mrcp_numeric_speech_length_t *numeric = &speech_length->value.numeric;
		apt_str_t str;
		apt_text_stream_t stream;
		stream.text = *value;
		apt_text_stream_reset(&stream);
		stream.pos++;
		if(apt_text_field_read(&stream,APT_TOKEN_SP,TRUE,&str) == FALSE) {
			return FALSE;
		}
		numeric->length = apt_size_value_parse(&str);

		if(apt_text_field_read(&stream,APT_TOKEN_SP,TRUE,&str) == FALSE) {
			return FALSE;
		}
		numeric->unit = apt_string_table_value_parse(speech_unit_string_table,SPEECH_UNIT_COUNT,&str);
	}
	return TRUE;
}

/** Generate MRCP speech-length value */
static apt_bool_t mrcp_speech_length_generate(mrcp_speech_length_value_t *speech_length, apt_str_t *str, apr_pool_t *pool)
{
	if(speech_length->type == SPEECH_LENGTH_TYPE_TEXT) {
		apt_str_t *tag = &speech_length->value.tag;
		if(tag->length) {
			apt_string_copy(str,tag,pool);
		}
	}
	else {
		const apt_str_t *unit_name = apt_string_table_str_get(
										speech_unit_string_table,
										SPEECH_UNIT_COUNT,
										speech_length->value.numeric.unit);
		if(!unit_name) {
			return FALSE;
		}

		str->buf = apr_psprintf(pool, "%c%"APR_SIZE_T_FMT" %s",
						speech_length->type == SPEECH_LENGTH_TYPE_NUMERIC_POSITIVE ? '+' : '-',
						speech_length->value.numeric.length,
						unit_name->buf);
		str->length = strlen(str->buf);
	}
	return TRUE;
}

/** Initialize synthesizer header */
static void mrcp_synth_header_init(mrcp_synth_header_t *synth_header)
{
	synth_header->jump_size.type = SPEECH_LENGTH_TYPE_UNKNOWN;
	synth_header->kill_on_barge_in = FALSE;
	apt_string_reset(&synth_header->speaker_profile);
	synth_header->completion_cause = SYNTHESIZER_COMPLETION_CAUSE_UNKNOWN;
	apt_string_reset(&synth_header->completion_reason);
	synth_header->voice_param.gender = VOICE_GENDER_UNKNOWN;
	synth_header->voice_param.age = 0;
	synth_header->voice_param.variant = 0;
	apt_string_reset(&synth_header->voice_param.name);
	synth_header->prosody_param.volume.type = PROSODY_VOLUME_TYPE_UNKNOWN;
	synth_header->prosody_param.rate.type = PROSODY_RATE_TYPE_UNKNOWN;
	apt_string_reset(&synth_header->speech_marker);
	apt_string_reset(&synth_header->speech_language);
	apt_string_reset(&synth_header->fetch_hint);
	apt_string_reset(&synth_header->audio_fetch_hint);
	apt_string_reset(&synth_header->failed_uri);
	apt_string_reset(&synth_header->failed_uri_cause);
	synth_header->speak_restart = FALSE;
	synth_header->speak_length.type = SPEECH_LENGTH_TYPE_UNKNOWN;
	synth_header->load_lexicon = FALSE;
	apt_string_reset(&synth_header->lexicon_search_order);
}


/** Allocate MRCP synthesizer header */
static void* mrcp_synth_header_allocate(mrcp_header_accessor_t *accessor, apr_pool_t *pool)
{
	mrcp_synth_header_t *synth_header = apr_palloc(pool,sizeof(mrcp_synth_header_t));
	mrcp_synth_header_init(synth_header);
	accessor->data = synth_header;
	return accessor->data;
}

/** Parse MRCP synthesizer header */
static apt_bool_t mrcp_synth_header_parse(mrcp_header_accessor_t *accessor, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	apt_bool_t status = TRUE;
	mrcp_synth_header_t *synth_header = accessor->data;
	switch(id) {
		case SYNTHESIZER_HEADER_JUMP_SIZE:
			mrcp_speech_length_value_parse(&synth_header->jump_size,value,pool);
			break;
		case SYNTHESIZER_HEADER_KILL_ON_BARGE_IN:
			apt_boolean_value_parse(value,&synth_header->kill_on_barge_in);
			break;
		case SYNTHESIZER_HEADER_SPEAKER_PROFILE:
			synth_header->speaker_profile = *value;
			break;
		case SYNTHESIZER_HEADER_COMPLETION_CAUSE:
			synth_header->completion_cause = apt_size_value_parse(value);
			break;
		case SYNTHESIZER_HEADER_COMPLETION_REASON:
			synth_header->completion_reason = *value;
			break;
		case SYNTHESIZER_HEADER_VOICE_GENDER:
			synth_header->voice_param.gender = apt_string_table_value_parse(voice_gender_string_table,VOICE_GENDER_COUNT,value);
			break;
		case SYNTHESIZER_HEADER_VOICE_AGE:
			synth_header->voice_param.age = apt_size_value_parse(value);
			break;
		case SYNTHESIZER_HEADER_VOICE_VARIANT:
			synth_header->voice_param.variant = apt_size_value_parse(value);
			break;
		case SYNTHESIZER_HEADER_VOICE_NAME:
			synth_header->voice_param.name = *value;
			break;
		case SYNTHESIZER_HEADER_PROSODY_VOLUME:
			mrcp_prosody_param_volume_parse(&synth_header->prosody_param.volume,value,pool);
			break;
		case SYNTHESIZER_HEADER_PROSODY_RATE:
			mrcp_prosody_param_rate_parse(&synth_header->prosody_param.rate,value,pool);
			break;
		case SYNTHESIZER_HEADER_SPEECH_MARKER:
			synth_header->speech_marker = *value;
			break;
		case SYNTHESIZER_HEADER_SPEECH_LANGUAGE:
			synth_header->speech_language = *value;
			break;
		case SYNTHESIZER_HEADER_FETCH_HINT:
			synth_header->fetch_hint = *value;
			break;
		case SYNTHESIZER_HEADER_AUDIO_FETCH_HINT:
			synth_header->audio_fetch_hint = *value;
			break;
		case SYNTHESIZER_HEADER_FAILED_URI:
			synth_header->failed_uri = *value;
			break;
		case SYNTHESIZER_HEADER_FAILED_URI_CAUSE:
			synth_header->failed_uri_cause = *value;
			break;
		case SYNTHESIZER_HEADER_SPEAK_RESTART:
			apt_boolean_value_parse(value,&synth_header->speak_restart);
			break;
		case SYNTHESIZER_HEADER_SPEAK_LENGTH:
			mrcp_speech_length_value_parse(&synth_header->speak_length,value,pool);
			break;
		case SYNTHESIZER_HEADER_LOAD_LEXICON:
			apt_boolean_value_parse(value,&synth_header->load_lexicon);
			break;
		case SYNTHESIZER_HEADER_LEXICON_SEARCH_ORDER:
			synth_header->lexicon_search_order = *value;
			break;
		default:
			status = FALSE;
	}
	return status;
}

/** Generate MRCP synthesizer header */
static apt_bool_t mrcp_synth_header_generate(const mrcp_header_accessor_t *accessor, apr_size_t id, apt_str_t *value, apr_pool_t *pool)
{
	mrcp_synth_header_t *synth_header = accessor->data;
	switch(id) {
		case SYNTHESIZER_HEADER_JUMP_SIZE:
			mrcp_speech_length_generate(&synth_header->jump_size,value,pool);
			break;
		case SYNTHESIZER_HEADER_KILL_ON_BARGE_IN:
			apt_boolean_value_generate(synth_header->kill_on_barge_in,value,pool);
			break;
		case SYNTHESIZER_HEADER_SPEAKER_PROFILE:
			*value = synth_header->speaker_profile;
			break;
		case SYNTHESIZER_HEADER_COMPLETION_CAUSE:
			apt_completion_cause_generate(
				completion_cause_string_table,
				SYNTHESIZER_COMPLETION_CAUSE_COUNT,
				synth_header->completion_cause,
				value,
				pool);
			break;
		case SYNTHESIZER_HEADER_COMPLETION_REASON:
			*value = synth_header->completion_reason;
			break;
		case SYNTHESIZER_HEADER_VOICE_GENDER:
			apt_string_table_value_pgenerate(
				voice_gender_string_table,
				VOICE_GENDER_COUNT,
				synth_header->voice_param.gender,
				value,
				pool);
			break;
		case SYNTHESIZER_HEADER_VOICE_AGE:
			apt_size_value_generate(synth_header->voice_param.age,value,pool);
			break;
		case SYNTHESIZER_HEADER_VOICE_VARIANT:
			apt_size_value_generate(synth_header->voice_param.variant,value,pool);
			break;
		case SYNTHESIZER_HEADER_VOICE_NAME:
			*value = synth_header->voice_param.name;
			break;
		case SYNTHESIZER_HEADER_PROSODY_VOLUME:
			mrcp_prosody_volume_generate(&synth_header->prosody_param.volume,value,pool);
			break;
		case SYNTHESIZER_HEADER_PROSODY_RATE:
			mrcp_prosody_rate_generate(&synth_header->prosody_param.rate,value,pool);
			break;
		case SYNTHESIZER_HEADER_SPEECH_MARKER:
			*value = synth_header->speech_marker;
			break;
		case SYNTHESIZER_HEADER_SPEECH_LANGUAGE:
			*value = synth_header->speech_language;
			break;
		case SYNTHESIZER_HEADER_FETCH_HINT:
			*value = synth_header->fetch_hint;
			break;
		case SYNTHESIZER_HEADER_AUDIO_FETCH_HINT:
			*value = synth_header->audio_fetch_hint;
			break;
		case SYNTHESIZER_HEADER_FAILED_URI:
			*value = synth_header->failed_uri;
			break;
		case SYNTHESIZER_HEADER_FAILED_URI_CAUSE:
			*value = synth_header->failed_uri_cause;
			break;
		case SYNTHESIZER_HEADER_SPEAK_RESTART:
			apt_boolean_value_generate(synth_header->speak_restart,value,pool);
			break;
		case SYNTHESIZER_HEADER_SPEAK_LENGTH:
			mrcp_speech_length_generate(&synth_header->speak_length,value,pool);
			break;
		case SYNTHESIZER_HEADER_LOAD_LEXICON:
			apt_boolean_value_generate(synth_header->load_lexicon,value,pool);
			break;
		case SYNTHESIZER_HEADER_LEXICON_SEARCH_ORDER:
			*value = synth_header->lexicon_search_order;
			break;
		default:
			break;
	}
	return TRUE;
}

/** Duplicate MRCP synthesizer header */
static apt_bool_t mrcp_synth_header_duplicate(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, apr_size_t id, const apt_str_t *value, apr_pool_t *pool)
{
	mrcp_synth_header_t *synth_header = accessor->data;
	const mrcp_synth_header_t *src_synth_header = src->data;
	apt_bool_t status = TRUE;

	if(!synth_header || !src_synth_header) {
		return FALSE;
	}

	switch(id) {
		case SYNTHESIZER_HEADER_JUMP_SIZE:
			synth_header->jump_size = src_synth_header->jump_size;
			break;
		case SYNTHESIZER_HEADER_KILL_ON_BARGE_IN:
			synth_header->kill_on_barge_in = src_synth_header->kill_on_barge_in;
			break;
		case SYNTHESIZER_HEADER_SPEAKER_PROFILE:
			synth_header->speaker_profile = *value;
			break;
		case SYNTHESIZER_HEADER_COMPLETION_CAUSE:
			synth_header->completion_cause = src_synth_header->completion_cause;
			break;
		case SYNTHESIZER_HEADER_COMPLETION_REASON:
			synth_header->completion_reason = *value;
			break;
		case SYNTHESIZER_HEADER_VOICE_GENDER:
			synth_header->voice_param.gender = src_synth_header->voice_param.gender;
			break;
		case SYNTHESIZER_HEADER_VOICE_AGE:
			synth_header->voice_param.age = src_synth_header->voice_param.age;
			break;
		case SYNTHESIZER_HEADER_VOICE_VARIANT:
			synth_header->voice_param.variant = src_synth_header->voice_param.variant;
			break;
		case SYNTHESIZER_HEADER_VOICE_NAME:
			synth_header->voice_param.name = *value;
			break;
		case SYNTHESIZER_HEADER_PROSODY_VOLUME:
			synth_header->prosody_param.volume = src_synth_header->prosody_param.volume;
			break;
		case SYNTHESIZER_HEADER_PROSODY_RATE:
			synth_header->prosody_param.rate = src_synth_header->prosody_param.rate;
			break;
		case SYNTHESIZER_HEADER_SPEECH_MARKER:
			synth_header->speech_marker = *value;
			break;
		case SYNTHESIZER_HEADER_SPEECH_LANGUAGE:
			synth_header->speech_language = *value;
			break;
		case SYNTHESIZER_HEADER_FETCH_HINT:
			synth_header->fetch_hint = *value;
			break;
		case SYNTHESIZER_HEADER_AUDIO_FETCH_HINT:
			synth_header->audio_fetch_hint = *value;
			break;
		case SYNTHESIZER_HEADER_FAILED_URI:
			synth_header->failed_uri = *value;
			break;
		case SYNTHESIZER_HEADER_FAILED_URI_CAUSE:
			synth_header->failed_uri_cause = *value;
			break;
		case SYNTHESIZER_HEADER_SPEAK_RESTART:
			synth_header->speak_restart = src_synth_header->speak_restart;
			break;
		case SYNTHESIZER_HEADER_SPEAK_LENGTH:
			synth_header->speak_length = src_synth_header->speak_length;
			break;
		case SYNTHESIZER_HEADER_LOAD_LEXICON:
			synth_header->load_lexicon = src_synth_header->load_lexicon;
			break;
		case SYNTHESIZER_HEADER_LEXICON_SEARCH_ORDER:
			synth_header->lexicon_search_order = *value;
			break;
		default:
			status = FALSE;
	}
	return status;
}

static const mrcp_header_vtable_t vtable = {
	mrcp_synth_header_allocate,
	NULL, /* nothing to destroy */
	mrcp_synth_header_parse,
	mrcp_synth_header_generate,
	mrcp_synth_header_duplicate,
	synth_header_string_table,
	SYNTHESIZER_HEADER_COUNT
};

const mrcp_header_vtable_t* mrcp_synth_header_vtable_get(mrcp_version_e version)
{
	return &vtable;
}

MRCP_DECLARE(const apt_str_t*) mrcp_synth_completion_cause_get(mrcp_synth_completion_cause_e completion_cause, mrcp_version_e version)
{
	return apt_string_table_str_get(completion_cause_string_table,SYNTHESIZER_COMPLETION_CAUSE_COUNT,completion_cause);
}
