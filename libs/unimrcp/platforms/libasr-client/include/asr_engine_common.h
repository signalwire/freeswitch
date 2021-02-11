/*
 * Copyright 2009-2015 Arsen Chaloyan
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

#ifndef ASR_ENGINE_COMMON_H
#define ASR_ENGINE_COMMON_H

#include <apt.h>

#include "mrcp_recog_resource.h"

/**
 * @file asr_engine_common.h
 * @brief Put into spearate include file to ease access through SWIG or
 * other interop wrappers.
 */

typedef struct {
	float   confidence_threshold;
	float   sensitivity_level;
	float   speed_vs_accuracy;
	long    n_best_list_length;
	long    no_input_timeout;
	long    recognition_timeout;
	char   *recognizer_context_block;
	long    speech_complete_timeout;
	long    speech_incomplete_timeout;
	long    dtmf_interdigit_timeout;
	long    dtmf_term_timeout;
	char    dtmf_term_char;
	int     save_waveform;
	char   *speech_language;
	char   *media_type;
	char   *recognition_mode;
	long    hotword_max_duration;
	long    hotword_min_duration;
	long    dtmf_buffer_time;
	int     early_no_match;
} ParameterSet;

typedef struct {
	mrcp_recognizer_event_id   event_id;
	int                        completion_cause_code;
	const char                *completion_cause_name;
	apt_bool_t                 is_recognizing;
	apt_bool_t                 completion_success;
	const char                *result;
	const char                *body;
} RecognitionResult;

#endif /* ASR_ENGINE_COMMON_H */
