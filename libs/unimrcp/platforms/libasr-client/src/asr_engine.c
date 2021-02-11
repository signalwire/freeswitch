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

#include <stdlib.h>

/* APR includes */
#include <apr_thread_proc.h>
/* Common includes */
#include "unimrcp_client.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_client_session.h"
/* APT includes */
#include "apt_nlsml_doc.h"
#include "apt_pool.h"

#include "asr_engine.h"

#define LINE_BUFFER 1024
#define RIFF_CHUNK_LEN 12

const char *STANDARD_GRAMMAR_URI_SCHEMES = "http:,https:,file:,builtin:";
// udpate note - proprietary grammar URI schemes may be added as comma separated list
const char *PROPRIETARY_GRAMMAR_URI_SCHEMES = NULL; // ",xxxx:,yyyy:";
// udpate note - end proprietary grammar URI schemes

int startswith(const char *pre, const char *str) {
	return strncmp(pre,str,strlen(pre)) == 0;
}

int endsWith(const char *str, const char *suffix) {
	size_t lenstr, lensuffix;

	if (!str || !suffix)
		return 0;

	lenstr = strlen(str);
	lensuffix = strlen(suffix);
	if (lensuffix > lenstr)
		return 0;

	return strncmp(str+lenstr-lensuffix,suffix,lensuffix) == 0;
}

/** Declaration of recognizer audio stream methods */
static apt_bool_t asr_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	NULL,
	NULL,
	NULL,
	asr_stream_read,
	NULL,
	NULL,
	NULL,
	NULL
};

static apt_bool_t app_message_handler(const mrcp_app_message_t *app_message);


/** Create ASR engine */
ASR_CLIENT_DECLARE(asr_engine_t*) asr_engine_create(
									const char *root_dir_path,
									apt_log_priority_e log_priority,
									apt_log_output_e log_output)
{
	apr_pool_t *pool = NULL;
	apt_dir_layout_t *dir_layout;
	asr_engine_t *engine;
	mrcp_client_t *mrcp_client;
	mrcp_application_t *mrcp_app;
	const char *log_prefix = "unimrcpclient";

	/* create APR pool */
	pool = apt_pool_create();
	if(!pool) {
		return NULL;
	}

	/* create the structure of default directories layout */
	dir_layout = apt_default_dir_layout_create(root_dir_path,pool);
	/* create singleton logger */
	apt_log_instance_create(log_output,log_priority,pool);

	// udpate note - todo - this may be out of date from 1.6
	if(apt_log_output_mode_check(APT_LOG_OUTPUT_FILE) == TRUE) {
		/* open the log file */
		const char *log_dir_path = apt_dir_layout_path_get(dir_layout,APT_LAYOUT_LOG_DIR);
		const char *logfile_conf_path = apt_confdir_filepath_get(dir_layout,"logfile.xml",pool);
		apt_log_file_open_ex(log_dir_path,log_prefix,logfile_conf_path,pool);
	}

	if(apt_log_output_mode_check(APT_LOG_OUTPUT_SYSLOG) == TRUE) {
		/* open the syslog */
		const char *logfile_conf_path = apt_confdir_filepath_get(dir_layout,"syslog.xml",pool);
		apt_syslog_open(log_prefix,logfile_conf_path,pool);
	}

	engine = apr_palloc(pool,sizeof(asr_engine_t));
	engine->pool = pool;
	engine->mrcp_client = NULL;
	engine->mrcp_app = NULL;

	/* create UniMRCP client stack */
	mrcp_client = unimrcp_client_create(dir_layout);
	if(!mrcp_client) {
		apt_log_instance_destroy();
		apr_pool_destroy(pool);
		return NULL;
	}

	/* create an application */
	mrcp_app = mrcp_application_create(
								app_message_handler,
								engine,
								pool);
	if(!mrcp_app) {
		mrcp_client_destroy(mrcp_client);
		apt_log_instance_destroy();
		apr_pool_destroy(pool);
		return NULL;
	}

	/* register application in client stack */
	mrcp_client_application_register(mrcp_client,mrcp_app,"ASRAPP");

	/* start client stack */
	if(mrcp_client_start(mrcp_client) != TRUE) {
		mrcp_client_destroy(mrcp_client);
		apt_log_instance_destroy();
		apr_pool_destroy(pool);
		return NULL;
	}

	engine->mrcp_client = mrcp_client;
	engine->mrcp_app = mrcp_app;
	return engine;
}

/** Destroy ASR engine */
ASR_CLIENT_DECLARE(apt_bool_t) asr_engine_destroy(asr_engine_t *engine)
{
	if(engine->mrcp_client) {
		/* shutdown client stack */
		mrcp_client_shutdown(engine->mrcp_client);
		/* destroy client stack */
		mrcp_client_destroy(engine->mrcp_client);
		engine->mrcp_client = NULL;
		engine->mrcp_app = NULL;
	}

	/* destroy singleton logger */
	apt_log_instance_destroy();
	/* destroy APR pool */
	apr_pool_destroy(engine->pool);
	return TRUE;
}



/** Destroy ASR session */
static apt_bool_t asr_session_destroy_ex(asr_session_t *asr_session, apt_bool_t terminate)
{
	if(terminate == TRUE) {
		apr_thread_mutex_lock(asr_session->mutex);
		if(mrcp_application_session_terminate(asr_session->mrcp_session) == TRUE) {
			apr_thread_cond_wait(asr_session->wait_object,asr_session->mutex);
			/* the response must be checked to be the valid one */
		}
		apr_thread_mutex_unlock(asr_session->mutex);
	}

	if(asr_session->audio_in) {
		fclose(asr_session->audio_in);
		asr_session->audio_in = NULL;
	}

	if(asr_session->mutex) {
		apr_thread_mutex_destroy(asr_session->mutex);
		asr_session->mutex = NULL;
	}
	if(asr_session->wait_object) {
		apr_thread_cond_destroy(asr_session->wait_object);
		asr_session->wait_object = NULL;
	}
	if(asr_session->media_buffer) {
		mpf_frame_buffer_destroy(asr_session->media_buffer);
		asr_session->media_buffer = NULL;
	}

	return mrcp_application_session_destroy(asr_session->mrcp_session);
}

/** Open audio input file */
static apt_bool_t asr_input_file_open(asr_session_t *asr_session, const char *input_file)
{
	const apt_dir_layout_t *dir_layout = mrcp_application_dir_layout_get(asr_session->engine->mrcp_app);
	apr_pool_t *pool = mrcp_application_session_pool_get(asr_session->mrcp_session);
	char *input_file_path = apt_datadir_filepath_get(dir_layout,input_file,pool);

	if(!input_file_path) {
		return FALSE;
	}
	
	if(asr_session->audio_in) {
		fclose(asr_session->audio_in);
		asr_session->audio_in = NULL;
	}

	asr_session->audio_in = fopen(input_file_path,"rb");

	if(asr_session->audio_in != NULL) {
		char buf[RIFF_CHUNK_LEN+1] = "";
		if(fread(buf,1,RIFF_CHUNK_LEN,asr_session->audio_in) == RIFF_CHUNK_LEN &&
			strncmp(buf,"RIFF",4) == 0 &&
			strncmp(buf+8,"WAVE",4) == 0) {

			// advance to data chunk
			while(strncmp(buf,"data",4) != 0) {
				if(fread(buf,1,4,asr_session->audio_in) != 4) {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No data in [%s]",input_file_path);
					return FALSE;
				}
			}
			if(fread(buf,1,4,asr_session->audio_in) != 4) {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot seek in [%s]",input_file_path);
				return FALSE;
			}
		}
		else {
			rewind(asr_session->audio_in); // rewind if no wave header
		}
	}

	if(!asr_session->audio_in) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot Open [%s]",input_file_path);
		return FALSE;
	}

	return TRUE;
}

/** MPF callback to read audio frame */
static apt_bool_t asr_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	asr_session_t *asr_session = stream->obj;
	if(asr_session && asr_session->streaming == TRUE) {
		if(asr_session->input_mode == INPUT_MODE_FILE) {
			if(asr_session->audio_in) {
				if(fread(frame->codec_frame.buffer,1,frame->codec_frame.size,asr_session->audio_in) == frame->codec_frame.size) {
					/* normal read */
					frame->type |= MEDIA_FRAME_TYPE_AUDIO;
				}
				else {
					/* file is over */
					asr_session->streaming = FALSE;
				}
			}
		}
		if(asr_session->input_mode == INPUT_MODE_STREAM) {
			if(asr_session->media_buffer) {
				mpf_frame_buffer_read(asr_session->media_buffer,frame);
			}
		}
	}
	return TRUE;
}

/** Create DEFINE-GRAMMAR request */
static mrcp_message_t* define_grammar_message_create(asr_session_t *asr_session, const char *grammar_file_name, int grammar_id)
{
	/* create MRCP message */
	mrcp_message_t *mrcp_message = mrcp_application_message_create(
						asr_session->mrcp_session,
						asr_session->mrcp_channel,
						RECOGNIZER_DEFINE_GRAMMAR);
	if(mrcp_message) {
		mrcp_generic_header_t *generic_header;

		/* set message body */
		const apt_dir_layout_t *dir_layout = mrcp_application_dir_layout_get(asr_session->engine->mrcp_app);
		apr_pool_t *pool = mrcp_application_session_pool_get(asr_session->mrcp_session);

		char *last;
		char *allSchemes = apr_pstrcat(pool,STANDARD_GRAMMAR_URI_SCHEMES,PROPRIETARY_GRAMMAR_URI_SCHEMES,NULL);
		char *scheme = apr_strtok(allSchemes,",",&last);

		apt_bool_t foundScheme = FALSE;
		char *content_type = NULL;
		do {
			if(startswith(scheme,grammar_file_name)) {
				// grammar URI uses a suppported URI scheme
				apt_string_assign(&mrcp_message->body,grammar_file_name,pool);
				content_type = apr_pstrdup(pool,"text/uri-list");
				foundScheme = TRUE;
			}

		} while((scheme = apr_strtok(NULL,",",&last)) != 0);

		if(!foundScheme && startswith("<",grammar_file_name)) {
			// grammar is special weighted grammar format
			apt_string_assign(&mrcp_message->body,grammar_file_name,pool);
			content_type = apr_pstrdup(pool,"text/grammar-ref-list");
			foundScheme = TRUE;
		}
		else if(!foundScheme) {
			// grammar is not URI and not weighted URI, use local file as inline grammar
			char *grammar_file_path = apt_datadir_filepath_get(dir_layout,grammar_file_name,pool);

			if(grammar_file_path) {
				apr_finfo_t finfo;
				apr_file_t *grammar_file;
				apt_str_t *content = &mrcp_message->body;

				if(apr_file_open(&grammar_file,grammar_file_path,APR_FOPEN_READ | APR_FOPEN_BINARY,0,pool) != APR_SUCCESS) {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open Grammar File %s",grammar_file_path);
					return NULL;
				}

				if(apr_file_info_get(&finfo,APR_FINFO_SIZE,grammar_file) != APR_SUCCESS) {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Grammar File Info %s",grammar_file_path);
					apr_file_close(grammar_file);
					return NULL;
				}

				content->length = (apr_size_t)finfo.size;
				content->buf = (char*) apr_palloc(pool,content->length+1);
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Load Grammar File Content size [%"APR_SIZE_T_FMT" bytes] %s",
					content->length,grammar_file_path);
				if(apr_file_read(grammar_file,content->buf,&content->length) != APR_SUCCESS) {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Read Grammar File Content %s",grammar_file_path);
					apr_file_close(grammar_file);
					return NULL;
				}
				content->buf[content->length] = '\0';
				apr_file_close(grammar_file);

				if(endsWith(grammar_file_name,".grxml") || endsWith(grammar_file_name,".xml")) {
					content_type = apr_pstrdup(mrcp_message->pool,"application/srgs+xml");
				}
				// udpate note - propietary grammar content types may be added
				/*
				else if(endsWith(grammar_file_name,".xxx")) {
					content_type = apr_pstrdup(mrcp_message->pool,"application/vnd.xxx.xxx");
				}
				*/
				// udpate note - end proprietary grammar content types
				else {
					apt_log(APT_LOG_MARK,APT_PRIO_ERROR,"Unsupported grammar scheme. grammar_file_name:%s",grammar_file_name);
					return NULL;
				}
			}
		}

		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			char *content_id;
			/* set generic header fields */
			if(foundScheme) {
				apt_string_assign(&generic_header->content_type,content_type,mrcp_message->pool);
			}
			else if(mrcp_message->start_line.version == MRCP_VERSION_2) {
				apt_string_assign(&generic_header->content_type,content_type,mrcp_message->pool);
			}
			else {
				// udpate note - i'm not sure this is still backwards compatible with MRCP v1
				apt_string_assign(&generic_header->content_type,"application/grammar+xml",mrcp_message->pool);
			}

			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
			content_id = apr_psprintf(mrcp_message->pool,"demo-grammar-%d",grammar_id);
			apt_string_assign(&generic_header->content_id,content_id,mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_ID);
		}
	}
	return mrcp_message;
}

/** Create RECOGNIZE request */
static mrcp_message_t* recognize_message_create(asr_session_t *asr_session, int uri_count, float weights[])
{
	mrcp_message_t *mrcp_message;
	if (weights == NULL) return NULL;

	/* create MRCP message */
	mrcp_message = mrcp_application_message_create(
										asr_session->mrcp_session,
										asr_session->mrcp_channel,
										RECOGNIZER_RECOGNIZE);
	if(mrcp_message) {
		mrcp_recog_header_t *recog_header = mrcp_resource_header_prepare(mrcp_message);
		mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			int i = 0;
			char *content_id = NULL;
			char *body = "";

			/* set generic header fields */
			apt_bool_t useWeights = FALSE;
			for(i = 0; i < uri_count; i++) {
				if(weights[i] != 1.0) {
					useWeights = TRUE;
					break;
				}
			}

			if(useWeights) {
				apt_string_assign(&generic_header->content_type,"text/grammar-ref-list",mrcp_message->pool);
			}
			else {
				apt_string_assign(&generic_header->content_type,"text/uri-list",mrcp_message->pool);
			}
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);

			for(i = 0; i < uri_count; i++) {
				/* set message body */
				if(useWeights) {
					content_id = apr_psprintf(mrcp_message->pool,"<session:demo-grammar-%d>;weight=\"%.2f\"\n",i,weights[i]);
				}
				else {
					content_id = apr_psprintf(mrcp_message->pool,"session:demo-grammar-%d\n",i);
				}
				if (content_id == NULL) {
					apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed create  content_id. i: %d",i);
					return NULL;
				}
				body = apr_pstrcat(mrcp_message->pool,body,content_id,NULL);
			}
			apt_string_assign(&mrcp_message->body,body,mrcp_message->pool);
		}
		/* get/allocate recognizer header */
		if(recog_header) {
			if(mrcp_message->start_line.version == MRCP_VERSION_2) {
				/* set recognizer header fields */
				recog_header->cancel_if_queue = FALSE;
				mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CANCEL_IF_QUEUE);
			}
			// Session Parameters are set in asr_session_set_param(). Use a parmeters file to override the ASR defaults
			recog_header->start_input_timers = TRUE;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_START_INPUT_TIMERS);
		}
	}
	return mrcp_message;
}

/** Get NLSML result, exported for usage with external tools. */
ASR_CLIENT_DECLARE(const char*) nlsml_result_get(mrcp_message_t *message)
{
	nlsml_interpretation_t *interpretation;
	nlsml_instance_t *instance;
	nlsml_result_t *result = nlsml_result_parse(message->body.buf, message->body.length, message->pool);
	if(!result) {
		return NULL;
	}

	/* get first interpretation */
	interpretation = nlsml_first_interpretation_get(result);
	if(!interpretation) {
		return NULL;
	}

	/* get first instance */
	instance = nlsml_interpretation_first_instance_get(interpretation);
	if(!instance) {
		return NULL;
	}

	nlsml_instance_swi_suppress(instance);
	return nlsml_instance_content_generate(instance, message->pool);
}


/** Application message handler */
static apt_bool_t app_message_handler(const mrcp_app_message_t *app_message)
{
	if((app_message->message_type == MRCP_APP_MESSAGE_TYPE_SIGNALING && 
		app_message->sig_message.message_type == MRCP_SIG_MESSAGE_TYPE_RESPONSE) ||
		app_message->message_type == MRCP_APP_MESSAGE_TYPE_CONTROL) {

		asr_session_t *asr_session = mrcp_application_session_object_get(app_message->session);
		if(asr_session) {
			apr_thread_mutex_lock(asr_session->mutex);
			asr_session->app_message = app_message;
			apr_thread_cond_signal(asr_session->wait_object);
			apr_thread_mutex_unlock(asr_session->mutex);
		}
	}
	return TRUE;
}

/** Check signaling response */
static apt_bool_t sig_response_check(const mrcp_app_message_t *app_message)
{
	if(!app_message || app_message->message_type != MRCP_APP_MESSAGE_TYPE_SIGNALING) {
		return FALSE;
	}

	return (app_message->sig_message.status == MRCP_SIG_STATUS_CODE_SUCCESS) ? TRUE : FALSE;
}

/** Check MRCP response */
static apt_bool_t mrcp_response_check(const mrcp_app_message_t *app_message, mrcp_request_state_e state)
{
	mrcp_message_t *mrcp_message = NULL;
	if(app_message && app_message->message_type == MRCP_APP_MESSAGE_TYPE_CONTROL) {
		mrcp_message = app_message->control_message;
	}

	if(!mrcp_message || mrcp_message->start_line.message_type != MRCP_MESSAGE_TYPE_RESPONSE ) {
		return FALSE;
	}

	if(mrcp_message->start_line.status_code != MRCP_STATUS_CODE_SUCCESS && 
		mrcp_message->start_line.status_code != MRCP_STATUS_CODE_SUCCESS_WITH_IGNORE) {
		return FALSE;
	}
	return (mrcp_message->start_line.request_state == state) ? TRUE : FALSE;
}

/** Get MRCP event */
static mrcp_message_t* mrcp_event_get(const mrcp_app_message_t *app_message)
{
	mrcp_message_t *mrcp_message = NULL;
	if(app_message && app_message->message_type == MRCP_APP_MESSAGE_TYPE_CONTROL) {
		mrcp_message = app_message->control_message;
	}

	if(!mrcp_message || mrcp_message->start_line.message_type != MRCP_MESSAGE_TYPE_EVENT) {
		return NULL;
	}
	return mrcp_message;
}

/** Create ASR session */
ASR_CLIENT_DECLARE(asr_session_t*) asr_session_create(asr_engine_t *engine, const char *profile)
{
	mpf_termination_t *termination;
	mrcp_channel_t *channel;
	mrcp_session_t *session;
	const mrcp_app_message_t *app_message;
	apr_pool_t *pool;
	asr_session_t *asr_session;
	mpf_stream_capabilities_t *capabilities;

	/* create session */
	session = mrcp_application_session_create(engine->mrcp_app,profile,NULL);
	if(!session) {
		return NULL;
	}
	pool = mrcp_application_session_pool_get(session);

	asr_session = apr_palloc(pool,sizeof(asr_session_t));
	mrcp_application_session_object_set(session,asr_session);
	
	/* create source stream capabilities */
	capabilities = mpf_source_stream_capabilities_create(pool);
	/* add codec capabilities (Linear PCM) */
	mpf_codec_capabilities_add(
			&capabilities->codecs,
			MPF_SAMPLE_RATE_8000,
			"LPCM");

	termination = mrcp_application_audio_termination_create(
			session,                   /* session, termination belongs to */
			&audio_stream_vtable,      /* virtual methods table of audio stream */
			capabilities,              /* capabilities of audio stream */
			asr_session);            /* object to associate */

	channel = mrcp_application_channel_create(
			session,                   /* session, channel belongs to */
			MRCP_RECOGNIZER_RESOURCE,  /* MRCP resource identifier */
			termination,               /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			asr_session);              /* object to associate */

	if(!channel) {
		mrcp_application_session_destroy(session);
		return NULL;
	}

	asr_session->engine = engine;
	asr_session->mrcp_session = session;
	asr_session->mrcp_channel = channel;
	asr_session->recog_complete = NULL;
	asr_session->input_mode = INPUT_MODE_NONE;
	asr_session->streaming = FALSE;
	asr_session->audio_in = NULL;
	asr_session->media_buffer = NULL;
	asr_session->mutex = NULL;
	asr_session->wait_object = NULL;
	asr_session->app_message = NULL;

	/* Create cond wait object and mutex */
	apr_thread_mutex_create(&asr_session->mutex,APR_THREAD_MUTEX_DEFAULT,pool);
	apr_thread_cond_create(&asr_session->wait_object,pool);

	/* Create media buffer */
	asr_session->media_buffer = mpf_frame_buffer_create(160,20,pool);

	/* Send add channel request and wait for the response */
	apr_thread_mutex_lock(asr_session->mutex);
	app_message = NULL;
	if(mrcp_application_channel_add(asr_session->mrcp_session,asr_session->mrcp_channel) == TRUE) {
		apr_thread_cond_wait(asr_session->wait_object,asr_session->mutex);
		app_message = asr_session->app_message;
		asr_session->app_message = NULL;
	}
	apr_thread_mutex_unlock(asr_session->mutex);

	if(sig_response_check(app_message) == FALSE) {
		asr_session_destroy_ex(asr_session,TRUE);
		return NULL;
	}
	return asr_session;
}

// udpate note - break up original asr_session_file_recognize()
// into:
//   asr_session_file_recognize()
//   asr_session_define_grammar() - lets the function be called multiple times
//   asr_session_file_recognize_send() - lets us test some stuff in beteween send and receieve
//   asr_session_file_recognize_receive() - lets us test some stuff in beteween send and receieve

/** Initiate recognition based on specified grammar and input file */
ASR_CLIENT_DECLARE(const char*) asr_session_file_recognize(
									asr_session_t *asr_session,
									const char *grammar_file,
									const char *input_file,
									const char *set_params_file,
									apt_bool_t send_set_params)
{
	int uri_count = 0;
	float weights[MAX_URIS];
	int i = 0;
	char *temp_grammar_uri_list = apr_pstrdup(asr_session->mrcp_session->pool,grammar_file);
	char *last;
	char *grammar_uri;
	char *grammar_uri_with_params = apr_strtok(temp_grammar_uri_list,",",&last);
	char *weight;
	char *lastWeight;
	char *lastGrammar;

	for(i = 0; i < MAX_URIS; i++)
		weights[i] = 1.0f;

	do {
		if(grammar_uri_with_params[0] == '<') {
			grammar_uri = apr_strtok(grammar_uri_with_params,"<>",&lastGrammar);
			weight = apr_strtok(lastGrammar,"\"",&lastWeight);
			if(weight == NULL) {
				weights[uri_count] = 1.0f;
			}
			else {
				lastWeight[strlen(lastWeight)-1] = '\0';
				weights[uri_count] = (float) atof(lastWeight);
			}
		}
		else {
			grammar_uri = grammar_uri_with_params;
		}

		if(asr_session_define_grammar(asr_session,grammar_uri,uri_count)) {
			uri_count++;
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_ERROR,"Define grammar failed for %s.",grammar_uri);
			uri_count++;
		}

		if (uri_count == MAX_URIS) {
			apt_log(APT_LOG_MARK,APT_PRIO_ERROR,"URI list has too many URIs.");
			return NULL;
		}
	} while((grammar_uri_with_params = apr_strtok(NULL,",",&last)) != 0);


	asr_session_file_recognize_send(asr_session,grammar_file,input_file,uri_count,weights,set_params_file,send_set_params);
	do {
		mrcp_recognizer_event_id event_id = asr_session_file_recognize_receive(asr_session);

		if(event_id == RECOGNIZER_START_OF_INPUT) {
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Receieved Start-of-Input");
		}
		else if(event_id == RECOGNIZER_RECOGNITION_COMPLETE) {
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Receieved Recognition-Complete");
		}
	} while(!asr_session->recog_complete);
	/* Get results */
	return nlsml_result_get(asr_session->recog_complete);
}

/* Exported for usage with external tools. */
ASR_CLIENT_DECLARE(apt_bool_t) asr_session_define_grammar(
								asr_session_t *asr_session,
								const char *grammar_uri,
								int grammar_id)
{
	const mrcp_app_message_t *app_message = NULL;
	mrcp_message_t *mrcp_message;
	mrcp_status_code_e status_code;

	mrcp_channel_t *client_channel = (mrcp_channel_t*) asr_session->mrcp_channel;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Begin asr_session_define_grammar. session: %s. grammar_uri: %s. grammar_id: %d",client_channel->session->id.buf,grammar_uri,grammar_id);

	mrcp_message = define_grammar_message_create(asr_session,grammar_uri,grammar_id);
	if(!mrcp_message) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create DEFINE-GRAMMAR Request");
		return FALSE;
	}

	/* Send DEFINE-GRAMMAR request and wait for the response */
	apr_thread_mutex_lock(asr_session->mutex);
	if(mrcp_application_message_send(asr_session->mrcp_session,asr_session->mrcp_channel,mrcp_message) == TRUE) {
		apr_thread_cond_wait(asr_session->wait_object,asr_session->mutex);
		app_message = asr_session->app_message;
		asr_session->app_message = NULL;
	}
	apr_thread_mutex_unlock(asr_session->mutex);

	if(mrcp_response_check(app_message,MRCP_REQUEST_STATE_COMPLETE) == FALSE) {
		return FALSE;
	}

	status_code = app_message->control_message->start_line.status_code;
	return (MRCP_STATUS_CODE_SUCCESS == status_code || MRCP_STATUS_CODE_SUCCESS_WITH_IGNORE == status_code);
}

static void *set_individual_param(mrcp_message_t *mrcp_message, mrcp_recog_header_t *recog_header, const char *pname, const char *pvalue)
{
	int no_input_timeout, n_best_list_length, recognition_timeout;
	float confidence_threshold, sensitivity_level, speed_vs_accuracy;
	int speech_complete_timeout, speech_incomplete_timeout, dtmf_interdigit_timeout, dtmf_term_timeout;
	int hotword_max_duration, hotword_min_duration;

	if(apr_strnatcasecmp(pname,"Confidence-Threshold") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD);
		}
		else {
			confidence_threshold = (float) atof(pvalue);
			recog_header->confidence_threshold = confidence_threshold;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD);
		}
	}
	if(apr_strnatcasecmp(pname,"Sensitivity-Level") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_SENSITIVITY_LEVEL);
		}
		else {
			sensitivity_level = (float) atof(pvalue);
			recog_header->sensitivity_level = sensitivity_level;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_SENSITIVITY_LEVEL);
		}
	}
	if(apr_strnatcasecmp(pname,"Speed-Vs-Accuracy") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_SPEED_VS_ACCURACY);
		}
		else {
			speed_vs_accuracy = (float) atof(pvalue);
			recog_header->speed_vs_accuracy = speed_vs_accuracy;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_SPEED_VS_ACCURACY);
		}
	}
	if(apr_strnatcasecmp(pname,"N-Best-List-Length") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_N_BEST_LIST_LENGTH);
		}
		else {
			n_best_list_length = atoi(pvalue);
			recog_header->n_best_list_length = n_best_list_length;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_N_BEST_LIST_LENGTH);
		}
	}
	if(apr_strnatcasecmp(pname,"New-Audio-Channel") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_NEW_AUDIO_CHANNEL);
		}
		else {
			if(apr_strnatcasecmp(pvalue,"true") == 0) {
				recog_header->new_audio_channel = TRUE;
			}
			else {
				recog_header->new_audio_channel = FALSE;
			}
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_NEW_AUDIO_CHANNEL);
		}
	}
	if(apr_strnatcasecmp(pname,"No-Input-Timeout") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT);
		}
		else {
			no_input_timeout = atoi(pvalue);
			recog_header->no_input_timeout = no_input_timeout;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT);
		}
	}
	if(apr_strnatcasecmp(pname,"Recognition-Timeout") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT);
		}
		else {
			recognition_timeout = atoi(pvalue);
			recog_header->recognition_timeout = recognition_timeout;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT);
		}
	}
	if(apr_strnatcasecmp(pname,"Speech-Complete-Timeout") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT);
		}
		else {
			speech_complete_timeout = atoi(pvalue);
			recog_header->speech_complete_timeout = speech_complete_timeout;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT);
		}
	}
	if(apr_strnatcasecmp(pname,"Speech-Incomplete-Timeout") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_SPEECH_INCOMPLETE_TIMEOUT);
		}
		else {
			speech_incomplete_timeout = atoi(pvalue);
			recog_header->speech_incomplete_timeout = speech_incomplete_timeout;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_SPEECH_INCOMPLETE_TIMEOUT);
		}
	}
	if(apr_strnatcasecmp(pname,"DTMF-Interdigit-Timeout") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT);
		}
		else {
			dtmf_interdigit_timeout = atoi(pvalue);
			recog_header->dtmf_interdigit_timeout = dtmf_interdigit_timeout;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT);
		}
	}
	if(apr_strnatcasecmp(pname,"DTMF-Term-Timeout") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_DTMF_TERM_TIMEOUT);
		}
		else {
			dtmf_term_timeout = atoi(pvalue);
			recog_header->dtmf_term_timeout = dtmf_term_timeout;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_DTMF_TERM_TIMEOUT);
		}
	}
	if(apr_strnatcasecmp(pname,"DTMF-Term-Char") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_DTMF_TERM_CHAR);
		}
		else {
			recog_header->dtmf_term_char = pvalue[0];
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_DTMF_TERM_CHAR);
		}
	}
	if(apr_strnatcasecmp(pname,"Speech-Language") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_SPEECH_LANGUAGE);
		}
		else {
			apt_string_assign(&recog_header->speech_language,pvalue,mrcp_message->pool);
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_SPEECH_LANGUAGE);
		}
	}

	if(mrcp_message->start_line.version == MRCP_VERSION_2 && apr_strnatcasecmp(pname,"Recognition-Mode") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_RECOGNITION_MODE);
		}
		else {
			apt_string_assign(&recog_header->recognition_mode,pvalue,mrcp_message->pool);
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_RECOGNITION_MODE);
		}
	}
	if(mrcp_message->start_line.version == MRCP_VERSION_2 && apr_strnatcasecmp(pname,"Hotword-Max-Duration") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_HOTWORD_MAX_DURATION);
		}
		else {
			hotword_max_duration = atoi(pvalue);
			recog_header->hotword_max_duration = hotword_max_duration;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_HOTWORD_MAX_DURATION);
		}
	}
	if(mrcp_message->start_line.version == MRCP_VERSION_2 && apr_strnatcasecmp(pname, "Hotword-Min-Duration") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_resource_header_name_property_add(mrcp_message,RECOGNIZER_HEADER_HOTWORD_MIN_DURATION);
		}
		else {
			hotword_min_duration = atoi(pvalue);
			recog_header->hotword_min_duration = hotword_min_duration;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_HOTWORD_MIN_DURATION);
		}
	}
	if(apr_strnatcasecmp(pname,"Logging-Tag") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_LOGGING_TAG);
		}
		else {
			mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(mrcp_message);
			apt_string_assign(&generic_header->logging_tag,pvalue,mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_LOGGING_TAG);
		}
	}
	if(apr_strnatcasecmp(pname,"Vendor-Specific-Parameters") == 0) {
		if(0 == strlen(pvalue)) {
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS);
		}
		else {
			char *val;
			char *last;
			char *newValue = apr_pstrdup(mrcp_message->pool,pvalue);

			mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(mrcp_message);
			generic_header->vendor_specific_params = apt_pair_array_create(1,mrcp_message->pool);

			val = apr_strtok(newValue,";",&last);
			if (val != NULL) {
				do {
					apt_str_t aprVspName;
					apt_str_t aprVspValue;
					char *vspName;
					char *vspValue;
					char *vspLast;
					apr_collapse_spaces(val,val);
					vspName = apr_strtok(val,"=",&vspLast);
					vspValue = apr_strtok(NULL,"=",&vspLast);

					apt_string_set(&aprVspName,vspName);
					apt_string_set(&aprVspValue,vspValue);
					apt_pair_array_append(generic_header->vendor_specific_params,&aprVspName,&aprVspValue,mrcp_message->pool);
				} while ((val = apr_strtok(NULL,";",&last)) != 0);
			}

			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS);
		}
	}
	return NULL;
}

static apt_bool_t set_param_from_file(
								asr_session_t *asr_session,
								const char *set_params_file,
								mrcp_message_t *mrcp_message,
								mrcp_recog_header_t *recog_header)
{
	apr_pool_t * pool = asr_session->mrcp_session->pool;
	apr_status_t rv;
	apr_file_t *fp;
	char *str;

	const apt_dir_layout_t *dir_layout = mrcp_application_dir_layout_get(asr_session->engine->mrcp_app);
	char *param_file_path = apt_datadir_filepath_get(dir_layout,set_params_file,pool);
	if(set_params_file != NULL) {
		if((rv = apr_file_open(&fp,param_file_path,APR_FOPEN_READ,APR_OS_DEFAULT,asr_session->engine->pool)) != APR_SUCCESS) {
			apt_log(APT_LOG_MARK,APT_PRIO_ERROR,"Failed to open file for SET-PARAMS. file: %s",set_params_file);
			return FALSE;
		}

		str = apr_palloc(pool,LINE_BUFFER);
		do {
			char *val;
			char *last;

			rv = apr_file_gets(str,LINE_BUFFER,fp);
			if(str != NULL) {
				val = apr_strtok(str,":",&last);
				if (val != NULL) {
					const char *pname = NULL;
					const char *pvalue = NULL;

					apr_collapse_spaces(val,val);
					pname = val;

					val = apr_strtok(NULL,":",&last);
					if(val != NULL) {
						apr_collapse_spaces(val,val);
						pvalue = val;
						set_individual_param(mrcp_message,recog_header,pname,pvalue);
					}
				}
			}
		} while(rv != APR_EOF);
		apr_file_close(fp);

	}
	return TRUE;
}

/* Exported for usage with external tools. */
ASR_CLIENT_DECLARE(apt_bool_t) asr_session_file_recognize_send(
								asr_session_t *asr_session,
								const char *grammar_file,
								const char *input_file,
								int uri_count,
								float weights[],
								const char *set_params_file,
								apt_bool_t send_set_params)
{
	const mrcp_app_message_t *app_message = NULL;
	mrcp_message_t *mrcp_message;

	mrcp_channel_t *client_channel = (mrcp_channel_t*) asr_session->mrcp_channel;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Begin asr_session_file_recognize_send. session: %s. input_file: %s,grammar_file: %s",client_channel->session->id.buf, input_file,grammar_file == NULL ? "(null)" : grammar_file);

	/* Reset prev recog result (if any) */
	asr_session->recog_complete = NULL;

	mrcp_message = recognize_message_create(asr_session,uri_count,weights);
	if(!mrcp_message) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create RECOGNIZE Request");
		return FALSE;
	}
	// TODO - add in headers if Set-Params is not used.
	if(!send_set_params && set_params_file != NULL) {
		mrcp_recog_header_t *recog_header = mrcp_resource_header_prepare(mrcp_message);
		set_param_from_file(asr_session,set_params_file,mrcp_message,recog_header);
	}

	/* Send RECOGNIZE request */
	apr_thread_mutex_lock(asr_session->mutex);
	if(mrcp_application_message_send(asr_session->mrcp_session,asr_session->mrcp_channel,mrcp_message) == TRUE) {
		apr_thread_cond_wait(asr_session->wait_object,asr_session->mutex);
		app_message = asr_session->app_message;
		asr_session->app_message = NULL;
	}
	apr_thread_mutex_unlock(asr_session->mutex);

	if(mrcp_response_check(app_message,MRCP_REQUEST_STATE_INPROGRESS) == FALSE) {
		return FALSE;
	}

	/* Open input file and start streaming */
	asr_session->input_mode = INPUT_MODE_FILE;
	if(asr_input_file_open(asr_session,input_file) == FALSE) {
		return FALSE;
	}
	asr_session->streaming = TRUE;

	return TRUE;
}

/* Exported for usage with external tools. */
ASR_CLIENT_DECLARE(mrcp_recognizer_event_id) asr_session_file_recognize_receive(asr_session_t *asr_session)
{
	const mrcp_app_message_t *app_message = NULL;
	mrcp_message_t *mrcp_message;

	apr_thread_mutex_lock(asr_session->mutex);
	if(apr_thread_cond_timedwait(asr_session->wait_object,asr_session->mutex,60 * 1000000) != APR_SUCCESS) {
		apr_thread_mutex_unlock(asr_session->mutex);
		return RECOGNIZER_EVENT_COUNT;
	}
	app_message = asr_session->app_message;

	asr_session->app_message = NULL;
	apr_thread_mutex_unlock(asr_session->mutex);

	mrcp_message = mrcp_event_get(app_message);

	if(mrcp_message && mrcp_message->start_line.method_id == RECOGNIZER_START_OF_INPUT) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"START-OF-INPUT received");
	}
	if(mrcp_message && mrcp_message->start_line.method_id == RECOGNIZER_RECOGNITION_COMPLETE) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"RECOGNTION-COMPLETE received");
		asr_session->recog_complete = mrcp_message;
	}

	return mrcp_message->start_line.method_id;
}

// udpate note - is this ever used? Should it be removed?
/** Initiate recognition based on specified grammar and input stream */
ASR_CLIENT_DECLARE(const char*) asr_session_stream_recognize(
									asr_session_t *asr_session,
									const char *grammar_file)
{
	const mrcp_app_message_t *app_message = NULL;
	mrcp_message_t *mrcp_message = define_grammar_message_create(asr_session,grammar_file,1);
	if(!mrcp_message) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create DEFINE-GRAMMAR Request");
		return NULL;
	}

	/* Send DEFINE-GRAMMAR request and wait for the response */
	apr_thread_mutex_lock(asr_session->mutex);
	if(mrcp_application_message_send(asr_session->mrcp_session,asr_session->mrcp_channel,mrcp_message) == TRUE) {
		apr_thread_cond_wait(asr_session->wait_object,asr_session->mutex);
		app_message = asr_session->app_message;
		asr_session->app_message = NULL;
	}
	apr_thread_mutex_unlock(asr_session->mutex);

	if(mrcp_response_check(app_message,MRCP_REQUEST_STATE_COMPLETE) == FALSE) {
		return NULL;
	}

	/* Reset prev recog result (if any) */
	asr_session->recog_complete = NULL;

	app_message = NULL;
	mrcp_message = recognize_message_create(asr_session,1,NULL);
	if(!mrcp_message) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create RECOGNIZE Request");
		return NULL;
	}

	/* Send RECOGNIZE request and wait for the response */
	apr_thread_mutex_lock(asr_session->mutex);
	if(mrcp_application_message_send(asr_session->mrcp_session,asr_session->mrcp_channel,mrcp_message) == TRUE) {
		apr_thread_cond_wait(asr_session->wait_object,asr_session->mutex);
		app_message = asr_session->app_message;
		asr_session->app_message = NULL;
	}
	apr_thread_mutex_unlock(asr_session->mutex);

	if(mrcp_response_check(app_message,MRCP_REQUEST_STATE_INPROGRESS) == FALSE) {
		return NULL;
	}

	/* Reset media buffer */
	mpf_frame_buffer_restart(asr_session->media_buffer);

	/* Set input mode and start streaming */
	asr_session->input_mode = INPUT_MODE_STREAM;
	asr_session->streaming = TRUE;

	/* Wait for events either START-OF-INPUT or RECOGNITION-COMPLETE */
	do {
		apr_thread_mutex_lock(asr_session->mutex);
		app_message = NULL;
		if(apr_thread_cond_timedwait(asr_session->wait_object,asr_session->mutex, 60 * 1000000) != APR_SUCCESS) {
			apr_thread_mutex_unlock(asr_session->mutex);
			return NULL;
		}
		app_message = asr_session->app_message;
		asr_session->app_message = NULL;
		apr_thread_mutex_unlock(asr_session->mutex);

		mrcp_message = mrcp_event_get(app_message);
		if(mrcp_message && mrcp_message->start_line.method_id == RECOGNIZER_RECOGNITION_COMPLETE) {
			asr_session->recog_complete = mrcp_message;
		}
	}
	while(!asr_session->recog_complete);

	/* Get results */
	return nlsml_result_get(asr_session->recog_complete);
}

// udpate note - is this ever used? Should it be removed?
/** Write audio frame to recognize */
ASR_CLIENT_DECLARE(apt_bool_t) asr_session_stream_write(
									asr_session_t *asr_session,
									char *data,
									int size)
{
	mpf_frame_t frame;
	frame.type = MEDIA_FRAME_TYPE_AUDIO;
	frame.marker = MPF_MARKER_NONE;
	frame.codec_frame.buffer = data;
	frame.codec_frame.size = size;

	if(mpf_frame_buffer_write(asr_session->media_buffer,&frame) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Write Audio [%d]",size);
		return FALSE;
	}
	return TRUE;
}

/** Destroy ASR session */
ASR_CLIENT_DECLARE(apt_bool_t) asr_session_destroy(asr_session_t *asr_session)
{
	return asr_session_destroy_ex(asr_session,TRUE);
}

/** Set log priority */
ASR_CLIENT_DECLARE(apt_bool_t) asr_engine_log_priority_set(apt_log_priority_e log_priority)
{
	return apt_log_priority_set(log_priority);
}


/** Handle MRCP headers (Parameters) **/

/** Create SET-PARAM request */
static mrcp_message_t* set_param_message_create(
							asr_session_t *asr_session,
							const char *set_params_file,
							const char* param_name,
							const char *param_value)
{
	mrcp_message_t *mrcp_message = NULL;

	if(set_params_file == NULL && param_name == NULL) {
		return NULL;
	}

	/* create MRCP message */
	mrcp_message = mrcp_application_message_create(asr_session->mrcp_session,asr_session->mrcp_channel,RECOGNIZER_SET_PARAMS);
	if(mrcp_message) {
		mrcp_recog_header_t *recog_header = mrcp_resource_header_prepare(mrcp_message);
		if(recog_header) {
			if(set_params_file != NULL) {
				set_param_from_file(asr_session,set_params_file,mrcp_message,recog_header);
			}
			else {
				set_individual_param(mrcp_message,recog_header,param_name,param_value);
			}
		}
	}
	return mrcp_message;
}

/** Create GET-PARAM request */
static mrcp_message_t* get_param_message_create(asr_session_t *asr_session)
{
	return mrcp_application_message_create(asr_session->mrcp_session,asr_session->mrcp_channel,RECOGNIZER_GET_PARAMS);
}

ASR_CLIENT_DECLARE(apt_bool_t) asr_session_set_param(
							asr_session_t *asr_session,
							const char *set_params_file,
							const char *param_name,
							const char *param_value)
{
	const mrcp_app_message_t *app_message = NULL;
	mrcp_message_t *mrcp_message = set_param_message_create(asr_session,set_params_file,param_name,param_value);
	mrcp_status_code_e status_code;

	if(!mrcp_message) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create SET-PARAMS Request");
		return FALSE;
	}

	/* Send SET-PARAMS request and wait for the response */
	apr_thread_mutex_lock(asr_session->mutex);
	if(mrcp_application_message_send(asr_session->mrcp_session,asr_session->mrcp_channel,mrcp_message) == TRUE) {
		apr_thread_cond_wait(asr_session->wait_object,asr_session->mutex);
		app_message = asr_session->app_message;
		asr_session->app_message = NULL;
	}
	apr_thread_mutex_unlock(asr_session->mutex);

	if(mrcp_response_check(app_message,MRCP_REQUEST_STATE_COMPLETE) == FALSE) {
		return FALSE;
	}

	status_code = app_message->control_message->start_line.status_code;
	return (MRCP_STATUS_CODE_SUCCESS == status_code || MRCP_STATUS_CODE_SUCCESS_WITH_IGNORE == status_code);
}

static void initialize_parameter_set(asr_session_t *asr_session, ParameterSet *p)
{
	if(asr_session->app_message->message_type == MRCP_APP_MESSAGE_TYPE_CONTROL) {
		mrcp_recog_header_t *recog_header = mrcp_resource_header_get(asr_session->app_message->control_message);

		p->confidence_threshold = recog_header->confidence_threshold;
		p->sensitivity_level = recog_header->sensitivity_level;
		p->speed_vs_accuracy = recog_header->speed_vs_accuracy;
		p->n_best_list_length = (long)recog_header->n_best_list_length;
		p->no_input_timeout = (long)recog_header->no_input_timeout;
		p->recognition_timeout = (long)recog_header->recognition_timeout;

		p->recognizer_context_block = recog_header->recognizer_context_block.buf;

		p->speech_complete_timeout = (long) recog_header->speech_complete_timeout;
		p->speech_incomplete_timeout = (long) recog_header->speech_incomplete_timeout;
		p->dtmf_interdigit_timeout = (long) recog_header->dtmf_interdigit_timeout;
		p->dtmf_term_timeout = (long) recog_header->dtmf_term_timeout;
		p->dtmf_term_char = recog_header->dtmf_term_char;
		p->save_waveform = recog_header->save_waveform;

		p->speech_language = recog_header->speech_language.buf;
		p->media_type = recog_header->media_type.buf;
		p->recognition_mode = recog_header->recognition_mode.buf;

		p->hotword_max_duration = (long) recog_header->hotword_max_duration;
		p->hotword_min_duration = (long) recog_header->hotword_min_duration;
		p->dtmf_buffer_time = (long) recog_header->dtmf_buffer_time;
		p->early_no_match = recog_header->early_no_match;
	}
}

ASR_CLIENT_DECLARE(ParameterSet*) asr_session_get_all_params(asr_session_t *asr_session)
{
	ParameterSet *p = NULL;
	const mrcp_app_message_t *app_message = NULL;
	apr_pool_t *pool = mrcp_application_session_pool_get(asr_session->mrcp_session);
	mrcp_message_t *mrcp_message = get_param_message_create(asr_session);
	if(!mrcp_message) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create GET-PARAMS Request");
		return NULL;
	}

	apr_thread_mutex_lock(asr_session->mutex);
	if(mrcp_application_message_send(asr_session->mrcp_session,asr_session->mrcp_channel,mrcp_message) == TRUE) {
		apr_thread_cond_wait(asr_session->wait_object,asr_session->mutex);
		app_message = asr_session->app_message;

	}
	apr_thread_mutex_unlock(asr_session->mutex);

	if(mrcp_response_check(app_message,MRCP_REQUEST_STATE_COMPLETE) == FALSE) {
		return NULL;
	}

	p = apr_palloc(pool, sizeof(ParameterSet));
	initialize_parameter_set(asr_session, p);
	asr_session->app_message = NULL;

	return p;
}
