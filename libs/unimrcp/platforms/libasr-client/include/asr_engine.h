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

#ifndef ASR_ENGINE_H
#define ASR_ENGINE_H

/**
 * @file asr_engine.h
 * @brief Basic ASR engine on top of UniMRCP client library
 */ 

/* APT includes */
#include "apt_log.h"
/* APR includes */
#include <apr_thread_cond.h>
/* Common includes */
#include "unimrcp_client.h"
#include "mrcp_application.h"
/* Recognizer includes */
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
/* MPF includes */
#include <mpf_frame_buffer.h>

#include "asr_engine_common.h"

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

typedef enum {
	INPUT_MODE_NONE,
	INPUT_MODE_FILE,
	INPUT_MODE_STREAM
} input_mode_e;

#define MAX_URIS 10

/** ASR engine on top of UniMRCP client stack */
struct asr_engine_t {
	/** MRCP client stack */
	mrcp_client_t      *mrcp_client;
	/** MRCP client stack */
	mrcp_application_t *mrcp_app;
	/** Memory pool */
	apr_pool_t         *pool;
};

/** ASR session on top of UniMRCP session/channel */
struct asr_session_t {
	/** Back pointer to engine */
	asr_engine_t             *engine;
	/** MRCP session */
	mrcp_session_t           *mrcp_session;
	/** MRCP channel */
	mrcp_channel_t           *mrcp_channel;
	/** RECOGNITION-COMPLETE message  */
	mrcp_message_t           *recog_complete;

	/** Input mode (either file or stream) */
	input_mode_e              input_mode;
	/** File to read media frames from */
	FILE                     *audio_in;
	/* Buffer of media frames */
	mpf_frame_buffer_t       *media_buffer;
	/** Streaming is in-progress */
	apt_bool_t                streaming;

	/** Conditional wait object */
	apr_thread_cond_t        *wait_object;
	/** Mutex of the wait object */
	apr_thread_mutex_t       *mutex;

	/** Message sent from client stack */
	const mrcp_app_message_t *app_message;
};


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
 * Initiate recognition based on specified grammar and input file.
 * @param session the session to run recognition in the scope of
 * @param grammar_file the name of the grammar file to use (path is relative to data dir)
 * @param input_file the name of the audio input file to use (path is relative to data dir)
 * @param set_params_file the name of the parameters file to use (path is relative to data dir)
 * @param send_set_params whether or not to use a separate SET-PARAMS request
 * @return the recognition result (input element of NLSML content)
 */
ASR_CLIENT_DECLARE(const char*) asr_session_file_recognize(
									asr_session_t *session,
									const char *grammar_file,
									const char *input_file,
									const char *set_params_file,
									apt_bool_t send_set_params);

/**
 * Initiate recognition based on specified grammar and input stream.
 * @param session the session to run recognition in the scope of
 * @param grammar_file the name of the grammar file to use (path is relative to data dir)
 * @return the recognition result (input element of NLSML content)
 *
 * @remark Audio data should be streamed through asr_session_stream_write() function calls
 */
ASR_CLIENT_DECLARE(const char*) asr_session_stream_recognize(
									asr_session_t *session,
									const char *grammar_file);

/**
 * Write audio data to recognize.
 * @param session the session to write audio data for
 * @param data the audio data
 * @param size the size of data
 */
ASR_CLIENT_DECLARE(apt_bool_t) asr_session_stream_write(
									asr_session_t *session,
									char *data,
									int size);

/**
 * Send SET-PARAM request.
 * @param session the session to send SET-PARAM in the scope of
 * @param set_params_file the name of the parameters file to use (path is relative to data dir)
 * @param param_name the name of the individual parameter to set
 * @param param_value the value of the individual parameter to set
 *
 * @remark Either param_name/param_value or set_param_file is supposed to be specified
 */
ASR_CLIENT_DECLARE(apt_bool_t) asr_session_set_param(
									asr_session_t *session,
									const char *set_params_file,
									const char *param_name,
									const char *param_value);

/**
 * Send GET-PARAM request.
 * @param session the session to send GET-PARAM in the scope of
 */
ASR_CLIENT_DECLARE(ParameterSet*) asr_session_get_all_params(asr_session_t *session);

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

/**
 * Send DEFINE-GRAMMAR request.
 * @param session the session to send DEFINE-GRAMMAR in the scope of
 * @param grammar_uri the grammar URI to use
 * @param grammar_id the identifier of the grammar to use in Content-Id
 */
ASR_CLIENT_DECLARE(apt_bool_t) asr_session_define_grammar(
									asr_session_t *session,
									const char *grammar_uri,
									int grammar_id);

/**
 * Send RECOGNIZE request.
 * @param session the session to send DEFINE-GRAMMAR in the scope of
 * @param grammar_file the name of the grammar file to use (path is relative to data dir)
 * @param input_file the name of the audio input file to use (path is relative to data dir)
 * @param uri_count the number of grammar URIs to use
 * @param weights the array of grammar weights to use
 * @param set_params_file the name of the parameters file to use (path is relative to data dir)
 * @param send_set_params whether or not to use a separate SET-PARAMS request
 */
ASR_CLIENT_DECLARE(apt_bool_t) asr_session_file_recognize_send(
									asr_session_t *session,
									const char *grammar_file,
									const char *input_file,
									int uri_count,
									float weights[],
									const char *set_params_file,
									apt_bool_t send_set_params);

/**
 * Receive MRCP event.
 * @param session the session to receive an event in the scope of
 */
ASR_CLIENT_DECLARE(mrcp_recognizer_event_id) asr_session_file_recognize_receive(asr_session_t *session);

/**
 * Get NLSML instance.
 * @param message the message to retreive NSLML results from
 */
ASR_CLIENT_DECLARE(const char*) nlsml_result_get(mrcp_message_t *message);

APT_END_EXTERN_C

#endif /* ASR_ENGINE_H */
