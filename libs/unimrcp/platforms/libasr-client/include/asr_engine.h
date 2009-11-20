/*
 * Copyright 2009 Arsen Chaloyan
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

#ifndef __ASR_ENGINE_H__
#define __ASR_ENGINE_H__

/**
 * @file asr_engine.h
 * @brief Basic ASR engine on top of UniMRCP client library
 */ 

#include "apt_log.h"

/** Lib export/import defines (win32) */
#ifdef WIN32
#ifdef ASR_CLIENT_STATIC_LIB
#define ASR_CLIENT_DECLARE(type)   type __stdcall
#else
#ifdef ASR_CLIENT_LIB_EXPORT
#define ASR_CLIENT_DECLARE(type)   __declspec(dllexport) type __stdcall
#else
#define ASR_CLIENT_DECLARE(type)   __declspec(dllimport) type __stdcall
#endif
#endif
#else
#define ASR_CLIENT_DECLARE(type) type
#endif

APT_BEGIN_EXTERN_C

/** Opaque ASR engine */
typedef struct asr_engine_t asr_engine_t;

/** Opaque ASR session */
typedef struct asr_session_t asr_session_t;


/**
 * Create ASR engine.
 * @param root_dir_path the path to root directory
 * @param log_priority the log priority level
 * @param log_output the log output mode
 */
ASR_CLIENT_DECLARE(asr_engine_t*) asr_engine_create(
									const char *root_dir_path,
									apt_log_priority_e log_priority,
									apt_log_output_e log_output);

/**
 * Destroy ASR engine.
 * @param engine the engine to destroy
 */
ASR_CLIENT_DECLARE(apt_bool_t) asr_engine_destroy(asr_engine_t *engine);



/**
 * Create ASR session.
 * @param engine the engine session belongs to
 * @param profile the name of UniMRCP profile to use
 */
ASR_CLIENT_DECLARE(asr_session_t*) asr_session_create(asr_engine_t *engine, const char *profile);

/**
 * Initiate recognition.
 * @param session the session to run recognition in the scope of
 * @param grammar_file the name of the grammar file to use (path is relative to data dir)
 * @param input_file the name of the audio input file to use (path is relative to data dir)
 * @return the recognition result (input element of NLSML content)
 */
ASR_CLIENT_DECLARE(const char*) asr_session_recognize(asr_session_t *session, const char *grammar_file, const char *input_file);

/**
 * Destroy ASR session.
 * @param session the session to destroy
 */
ASR_CLIENT_DECLARE(apt_bool_t) asr_session_destroy(asr_session_t *session);


/**
 * Set log priority.
 * @param priority the priority to set
 */
ASR_CLIENT_DECLARE(apt_bool_t) asr_engine_log_priority_set(apt_log_priority_e log_priority);


APT_END_EXTERN_C

#endif /*__ASR_ENGINE_H__*/
