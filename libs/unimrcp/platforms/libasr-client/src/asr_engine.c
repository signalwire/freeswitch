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

#include <stdlib.h>

/* APR includes */
#include <apr_thread_cond.h>
#include <apr_thread_proc.h>

/* common includes */
#include "unimrcp_client.h"
#include "mrcp_application.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
/* recognizer includes */
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
/* APT includes */
#include "apt_nlsml_doc.h"
#include "apt_log.h"
#include "apt_pool.h"

#include "asr_engine.h"


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
	asr_engine_t       *engine;
	/** MRCP session */
	mrcp_session_t     *mrcp_session;
	/** MRCP channel */
	mrcp_channel_t     *mrcp_channel;
	/** RECOGNITION-COMPLETE message  */
	mrcp_message_t     *recog_complete;

	/** File to read audio stream from */
	FILE               *audio_in;
	/** Streaming is in-progress */
	apt_bool_t          streaming;

	/** Conditional wait object */
	apr_thread_cond_t  *wait_object;
	/** Mutex of the wait object */
	apr_thread_mutex_t *mutex;

	/** Message sent from client stack */
	const mrcp_app_message_t *app_message;
};


/** Declaration of recognizer audio stream methods */
static apt_bool_t asr_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	NULL,
	NULL,
	NULL,
	asr_stream_read,
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

	/* create APR pool */
	pool = apt_pool_create();
	if(!pool) {
		return NULL;
	}

	/* create the structure of default directories layout */
	dir_layout = apt_default_dir_layout_create(root_dir_path,pool);
	/* create singleton logger */
	apt_log_instance_create(log_output,log_priority,pool);

	if((log_output & APT_LOG_OUTPUT_FILE) == APT_LOG_OUTPUT_FILE) {
		/* open the log file */
		apt_log_file_open(dir_layout->log_dir_path,"unimrcpclient",MAX_LOG_FILE_SIZE,MAX_LOG_FILE_COUNT,pool);
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
	if(asr_session->mrcp_session) {
		mrcp_application_session_destroy(asr_session->mrcp_session);
		asr_session->mrcp_session = NULL;
	}
	free(asr_session);
	return TRUE;
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
	return TRUE;
}

/** Create DEFINE-GRAMMAR request */
static mrcp_message_t* define_grammar_message_create(asr_session_t *asr_session, const char *grammar_file)
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
		char *grammar_file_path = apt_datadir_filepath_get(dir_layout,grammar_file,pool);
		if(grammar_file_path) {
			char text[1024];
			apr_size_t size;
			FILE *grammar = fopen(grammar_file_path,"r");
			if(!grammar) {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot Open [%s]",grammar_file_path);
				return NULL;
			}

			size = fread(text,1,sizeof(text),grammar);
			apt_string_assign_n(&mrcp_message->body,text,size,mrcp_message->pool);
			fclose(grammar);
		}

		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			/* set generic header fields */
			if(mrcp_message->start_line.version == MRCP_VERSION_2) {
				apt_string_assign(&generic_header->content_type,"application/srgs+xml",mrcp_message->pool);
			}
			else {
				apt_string_assign(&generic_header->content_type,"application/grammar+xml",mrcp_message->pool);
			}
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
			apt_string_assign(&generic_header->content_id,"demo-grammar",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_ID);
		}
	}
	return mrcp_message;
}

/** Create RECOGNIZE request */
static mrcp_message_t* recognize_message_create(asr_session_t *asr_session)
{
	/* create MRCP message */
	mrcp_message_t *mrcp_message = mrcp_application_message_create(
										asr_session->mrcp_session,
										asr_session->mrcp_channel,
										RECOGNIZER_RECOGNIZE);
	if(mrcp_message) {
		mrcp_recog_header_t *recog_header;
		mrcp_generic_header_t *generic_header;
		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			/* set generic header fields */
			apt_string_assign(&generic_header->content_type,"text/uri-list",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
			/* set message body */
			apt_string_assign(&mrcp_message->body,"session:demo-grammar",mrcp_message->pool);
		}
		/* get/allocate recognizer header */
		recog_header = mrcp_resource_header_prepare(mrcp_message);
		if(recog_header) {
			if(mrcp_message->start_line.version == MRCP_VERSION_2) {
				/* set recognizer header fields */
				recog_header->cancel_if_queue = FALSE;
				mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CANCEL_IF_QUEUE);
			}
			recog_header->no_input_timeout = 5000;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT);
			recog_header->recognition_timeout = 20000;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT);
			recog_header->speech_complete_timeout = 400;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT);
			recog_header->dtmf_term_timeout = 3000;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_DTMF_TERM_TIMEOUT);
			recog_header->dtmf_interdigit_timeout = 3000;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT);
			recog_header->confidence_threshold = 0.5f;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD);
			recog_header->start_input_timers = TRUE;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_START_INPUT_TIMERS);
		}
	}
	return mrcp_message;
}

/** Get NLSML input result */
static const char* nlsml_input_get(mrcp_message_t *message)
{
	apr_xml_elem *interpret;
	apr_xml_elem *instance;
	apr_xml_elem *input;
	apr_xml_doc *doc = nlsml_doc_load(&message->body,message->pool);
	if(!doc) {
		return NULL;
	}
	
	/* get interpreted result */
	interpret = nlsml_first_interpret_get(doc);
	if(!interpret) {
		return NULL;
	}
	/* get instance and input */
	nlsml_interpret_results_get(interpret,&instance,&input);
	if(!input || !input->first_cdata.first) {
		return NULL;
	}
	
	/* return input */
	return input->first_cdata.first->text;
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

	asr_session_t *asr_session = malloc(sizeof(asr_session_t));

	/* create session */
	session = mrcp_application_session_create(engine->mrcp_app,profile,asr_session);
	if(!session) {
		free(asr_session);
		return NULL;
	}
	pool = mrcp_application_session_pool_get(session);
	
	termination = mrcp_application_source_termination_create(
			session,                   /* session, termination belongs to */
			&audio_stream_vtable,      /* virtual methods table of audio stream */
			NULL,                      /* codec descriptor of audio stream (NULL by default) */
			asr_session);              /* object to associate */
	
	channel = mrcp_application_channel_create(
			session,                   /* session, channel belongs to */
			MRCP_RECOGNIZER_RESOURCE,  /* MRCP resource identifier */
			termination,               /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			asr_session);              /* object to associate */

	if(!channel) {
		mrcp_application_session_destroy(session);
		free(asr_session);
		return NULL;
	}
	
	asr_session->engine = engine;
	asr_session->mrcp_session = session;
	asr_session->mrcp_channel = channel;
	asr_session->recog_complete = NULL;
	asr_session->streaming = FALSE;
	asr_session->audio_in = NULL;
	asr_session->mutex = NULL;
	asr_session->wait_object = NULL;
	asr_session->app_message = NULL;

	/* Create cond wait object and mutex */
	apr_thread_mutex_create(&asr_session->mutex,APR_THREAD_MUTEX_DEFAULT,pool);
	apr_thread_cond_create(&asr_session->wait_object,pool);

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

/** Initiate recognition */
ASR_CLIENT_DECLARE(const char*) asr_session_recognize(asr_session_t *asr_session, const char *grammar_file, const char *input_file)
{
	const mrcp_app_message_t *app_message;
	mrcp_message_t *mrcp_message;

	app_message = NULL;
	mrcp_message = define_grammar_message_create(asr_session,grammar_file);
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
	mrcp_message = recognize_message_create(asr_session);
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
	
	/* Open input file and start streaming */
	if(asr_input_file_open(asr_session,input_file) == FALSE) {
		return NULL;
	}
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
	return nlsml_input_get(asr_session->recog_complete);
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
