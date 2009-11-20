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

/* 
 * Mandatory rules concerning plugin implementation.
 * 1. Each plugin MUST implement a plugin/engine creator function
 *    with the exact signature and name (the main entry point)
 *        MRCP_PLUGIN_DECLARE(mrcp_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
 * 2. Each plugin MUST declare its version number
 *        MRCP_PLUGIN_VERSION_DECLARE
 * 3. One and only one response MUST be sent back to the received request.
 * 4. Methods (callbacks) of the MRCP engine channel MUST not block.
 *   (asynchronous response can be sent from the context of other thread)
 * 5. Methods (callbacks) of the MPF engine stream MUST not block.
 */

#include <pocketsphinx.h>
#include <apr_thread_cond.h>
#include <apr_thread_proc.h>
#include <apr_tables.h>
#include <apr_file_io.h>
#include "mrcp_recog_engine.h"
#include "mpf_activity_detector.h"
#include "pocketsphinx_properties.h"
#include "apt_log.h"

#define POCKETSPHINX_CONFFILE_NAME "pocketsphinx.xml"

#define RECOGNIZER_SIDRES(recognizer) (recognizer)->channel->id.buf, "pocketsphinx"

typedef struct pocketsphinx_engine_t pocketsphinx_engine_t;
typedef struct pocketsphinx_recognizer_t pocketsphinx_recognizer_t;

/** Methods of recognition engine */
static apt_bool_t pocketsphinx_engine_destroy(mrcp_engine_t *engine);
static apt_bool_t pocketsphinx_engine_open(mrcp_engine_t *engine);
static apt_bool_t pocketsphinx_engine_close(mrcp_engine_t *engine);
static mrcp_engine_channel_t* pocketsphinx_engine_recognizer_create(mrcp_engine_t *engine, apr_pool_t *pool);

static const struct mrcp_engine_method_vtable_t engine_vtable = {
	pocketsphinx_engine_destroy,
	pocketsphinx_engine_open,
	pocketsphinx_engine_close,
	pocketsphinx_engine_recognizer_create
};


/** Methods of recognition channel (recognizer) */
static apt_bool_t pocketsphinx_recognizer_destroy(mrcp_engine_channel_t *channel);
static apt_bool_t pocketsphinx_recognizer_open(mrcp_engine_channel_t *channel);
static apt_bool_t pocketsphinx_recognizer_close(mrcp_engine_channel_t *channel);
static apt_bool_t pocketsphinx_recognizer_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request);

static const struct mrcp_engine_channel_method_vtable_t channel_vtable = {
	pocketsphinx_recognizer_destroy,
	pocketsphinx_recognizer_open,
	pocketsphinx_recognizer_close,
	pocketsphinx_recognizer_request_process
};

/** Methods of audio stream to recognize  */
static apt_bool_t pocketsphinx_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	NULL, /* destroy */
	NULL, /* open_rx */
	NULL, /* close_rx */
	NULL, /* read_frame */
	NULL, /* open_tx */
	NULL, /* close_tx */
	pocketsphinx_stream_write
};

/** Pocketsphinx engine (engine is an aggregation of recognizers) */
struct pocketsphinx_engine_t {
	/* Engine base */
	mrcp_engine_t   *base;
	/** Properties loaded from config file */
	pocketsphinx_properties_t properties;
};

/** Pocketsphinx channel (recognizer) */
struct pocketsphinx_recognizer_t {
	/** Engine channel base */
	mrcp_engine_channel_t    *channel;

	/** Actual recognizer object */
	ps_decoder_t             *decoder;
	/** Configuration */
	cmd_ln_t                 *config;
	/** Recognizer properties coppied from default engine properties */
	pocketsphinx_properties_t properties;
	/** Is input timer started */
	apt_bool_t                is_input_timer_on;
	/** Noinput timeout */
	apr_size_t                no_input_timeout;
	/** Recognition timeout */
	apr_size_t                recognition_timeout;
	/** Timeout elapsed since the last partial result checking */
	apr_size_t                partial_result_timeout;
	/** Last (partially) recognized result */
	const char               *last_result;
	/** Active grammar identifier (content-id) */
	const char               *grammar_id;
	/** Table of defined grammars (key=content-id, value=grammar-file-path) */
	apr_table_t              *grammar_table;
	/** File to write waveform to if save_waveform is on */
	apr_file_t               *waveform;

	/** Voice activity detector */
	mpf_activity_detector_t  *detector;

	/** Thread to run recognition in */
	apr_thread_t             *thread;
	/** Conditional wait object */
	apr_thread_cond_t        *wait_object;
	/** Mutex of the wait object */
	apr_thread_mutex_t       *mutex;

	/** Pending request from client stack to recognizer */
	mrcp_message_t           *request;
	/** Pending event from mpf layer to recognizer */
	mrcp_message_t           *complete_event;
	/** In-progress RECOGNIZE request */
	mrcp_message_t           *inprogress_recog;
	/** Pending STOP response */
	mrcp_message_t           *stop_response;
	/** Is recognition channel being closed */
	apt_bool_t                close_requested;
	/** Flag to prevent race condition when checking if a message is present */
	apt_bool_t                message_waiting;
};

static void* APR_THREAD_FUNC pocketsphinx_recognizer_run(apr_thread_t *thread, void *data);

/** Declare this macro to set plugin version */
MRCP_PLUGIN_VERSION_DECLARE

/** Declare this macro to use log routine of the server, plugin is loaded from */
MRCP_PLUGIN_LOGGER_IMPLEMENT

/** Create pocketsphinx engine (engine is an aggregation of recognizers) */
MRCP_PLUGIN_DECLARE(mrcp_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
{
	pocketsphinx_engine_t *engine = apr_palloc(pool,sizeof(pocketsphinx_engine_t));
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create PocketSphinx Engine");
	
	/* create engine base */
	engine->base = mrcp_engine_create(
					MRCP_RECOGNIZER_RESOURCE,  /* MRCP resource identifier */
					engine,                    /* object to associate */
					&engine_vtable,            /* virtual methods table of engine */
					pool);                     /* pool to allocate memory from */
	return engine->base;
}

/** Destroy pocketsphinx engine */
static apt_bool_t pocketsphinx_engine_destroy(mrcp_engine_t *engine_base)
{
	return TRUE;
}

/** Open pocketsphinx engine */
static apt_bool_t pocketsphinx_engine_open(mrcp_engine_t *engine_base)
{
	pocketsphinx_engine_t *engine = engine_base->obj;
	const apt_dir_layout_t *dir_layout = engine_base->dir_layout;

	char *file_path = NULL;
	apr_filepath_merge(&file_path,dir_layout->conf_dir_path,POCKETSPHINX_CONFFILE_NAME,0,engine_base->pool);

	/* load properties */
	pocketsphinx_properties_load(&engine->properties,file_path,dir_layout,engine_base->pool);
	return TRUE;
}

/** Close pocketsphinx engine */
static apt_bool_t pocketsphinx_engine_close(mrcp_engine_t *engine_base)
{
	return TRUE;
}

/** Create pocketsphinx recognizer */
static mrcp_engine_channel_t* pocketsphinx_engine_recognizer_create(mrcp_engine_t *engine_base, apr_pool_t *pool)
{
	mpf_stream_capabilities_t *capabilities;
	mpf_termination_t *termination; 
	mrcp_engine_channel_t *channel;
	pocketsphinx_engine_t *engine = engine_base->obj;

	/* create pocketsphinx recognizer */
	pocketsphinx_recognizer_t *recognizer = apr_palloc(pool,sizeof(pocketsphinx_recognizer_t));
	recognizer->decoder = NULL;
	recognizer->config = NULL;
	recognizer->is_input_timer_on = FALSE;
	recognizer->no_input_timeout = 0;
	recognizer->recognition_timeout = 0;
	recognizer->partial_result_timeout = 0;
	recognizer->last_result = NULL;
	recognizer->detector = NULL;
	recognizer->thread = NULL;
	recognizer->wait_object = NULL;
	recognizer->mutex = NULL;
	recognizer->request = NULL;
	recognizer->complete_event = NULL;
	recognizer->inprogress_recog = FALSE;
	recognizer->stop_response = NULL;
	recognizer->close_requested = FALSE;
	recognizer->grammar_id = NULL;
	recognizer->grammar_table = apr_table_make(pool,1);
	recognizer->waveform = NULL;
	recognizer->message_waiting = FALSE;

	/* copy default properties loaded from config */
	recognizer->properties = engine->properties;

	capabilities = mpf_sink_stream_capabilities_create(pool);
	mpf_codec_capabilities_add(
			&capabilities->codecs,
			MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000,
			"LPCM");

	/* create media termination */
	termination = mrcp_engine_audio_termination_create(
			recognizer,           /* object to associate */
			&audio_stream_vtable, /* virtual methods table of audio stream */
			capabilities,         /* stream capabilities */
			pool);                /* pool to allocate memory from */

	/* create engine channel base */
	channel = mrcp_engine_channel_create(
			engine_base,          /* engine */
			&channel_vtable,      /* virtual methods table of engine channel */
			recognizer,           /* object to associate */
			termination,          /* associated media termination */
			pool);                /* pool to allocate memory from */

	apr_thread_mutex_create(&recognizer->mutex,APR_THREAD_MUTEX_DEFAULT,channel->pool);
	apr_thread_cond_create(&recognizer->wait_object,channel->pool);

	recognizer->channel = channel;
	return channel;
}

/** Destroy pocketsphinx recognizer */
static apt_bool_t pocketsphinx_recognizer_destroy(mrcp_engine_channel_t *channel)
{
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;
	if(recognizer->mutex) {
		apr_thread_mutex_destroy(recognizer->mutex);
		recognizer->mutex = NULL;
	}
	if(recognizer->wait_object) {
		apr_thread_cond_destroy(recognizer->wait_object);
		recognizer->wait_object = NULL;
	}
	return TRUE;
}

/** Open pocketsphinx recognizer (asynchronous response MUST be sent) */
static apt_bool_t pocketsphinx_recognizer_open(mrcp_engine_channel_t *channel)
{
	apr_status_t rv;
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open Channel "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));

	/* Launch a thread to run recognition in */
	rv = apr_thread_create(&recognizer->thread,NULL,pocketsphinx_recognizer_run,recognizer,channel->pool);
	if(rv != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Failed to Launch Thread "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
		return mrcp_engine_channel_open_respond(channel,FALSE);
	}

	return TRUE;
}

/** Close pocketsphinx recognizer (asynchronous response MUST be sent)*/
static apt_bool_t pocketsphinx_recognizer_close(mrcp_engine_channel_t *channel)
{
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close Channel "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
	if(!recognizer->thread) {
		return mrcp_engine_channel_close_respond(channel);
	}

	/* Signal recognition thread to terminate */
	apr_thread_mutex_lock(recognizer->mutex);
	recognizer->close_requested = TRUE;
	recognizer->message_waiting = TRUE;
	apr_thread_cond_signal(recognizer->wait_object);
	apr_thread_mutex_unlock(recognizer->mutex);
	return TRUE;
}

/** Process MRCP request (asynchronous response MUST be sent)*/
static apt_bool_t pocketsphinx_recognizer_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;

	/* Store request and signal recognition thread to process the request */
	apr_thread_mutex_lock(recognizer->mutex);
	recognizer->request = request;
	recognizer->message_waiting = TRUE;
	apr_thread_cond_signal(recognizer->wait_object);
	apr_thread_mutex_unlock(recognizer->mutex);
	return TRUE;
}

/** Initialize pocketsphinx decoder [RECOG] */
static apt_bool_t pocketsphinx_decoder_init(pocketsphinx_recognizer_t *recognizer, const char *grammar)
{
	const mpf_codec_descriptor_t *descriptor = mrcp_engine_sink_stream_codec_get(recognizer->channel);
	const char *model = recognizer->properties.model_8k;
	const char *rate = "8000";
	if(descriptor && descriptor->sampling_rate == 16000) {
		model = recognizer->properties.model_16k;
		rate = "16000";
	}

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Init Config rate [%s] dictionary [%s] "APT_SIDRES_FMT,
		rate,
		recognizer->properties.dictionary,
		RECOGNIZER_SIDRES(recognizer));
	recognizer->config = cmd_ln_init(recognizer->config, ps_args(), FALSE,
							 "-samprate", rate,
							 "-hmm", model,
							 "-jsgf", grammar,
							 "-dict", recognizer->properties.dictionary,
							 "-frate", "50",
							 "-silprob", "0.005",
							 NULL);
	if(!recognizer->config) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Init Config "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Init Decoder "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
	if(recognizer->decoder) {
		if(ps_reinit(recognizer->decoder,recognizer->config) < 0) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Reinit Decoder "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
			return FALSE;
		}
	}
	else {
		recognizer->decoder = ps_init(recognizer->config);
		if(!recognizer->decoder) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Init Decoder "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
			return FALSE;
		}
	}

	if(!recognizer->detector) {
		recognizer->detector = mpf_activity_detector_create(recognizer->channel->pool);
		mpf_activity_detector_level_set(recognizer->detector,50);
	}
	return TRUE;
}

/** Build pocketsphinx recognized result [RECOG] */
static apt_bool_t pocketsphinx_result_build(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *message)
{
	apt_str_t *body = &message->body;
	if(!recognizer->last_result || !recognizer->grammar_id) {
		return FALSE;
	}

	body->buf = apr_psprintf(message->pool,
		"<?xml version=\"1.0\"?>\n"
		"<result grammar=\"%s\">\n"
		"  <interpretation grammar=\"%s\" confidence=\"%d\">\n"
		"    <input mode=\"speech\">%s</input>\n"
		"  </interpretation>\n"
		"</result>\n",
		recognizer->grammar_id,
		recognizer->grammar_id,
		99,
		recognizer->last_result);
	if(body->buf) {
		mrcp_generic_header_t *generic_header;
		generic_header = mrcp_generic_header_prepare(message);
		if(generic_header) {
			/* set content type */
			apt_string_assign(&generic_header->content_type,"application/x-nlsml",message->pool);
			mrcp_generic_header_property_add(message,GENERIC_HEADER_CONTENT_TYPE);
		}
		
		body->length = strlen(body->buf);
	}
	return TRUE;
}

/** Clear pocketsphinx grammars [RECOG] */
static apt_bool_t pocketsphinx_grammars_clear(pocketsphinx_recognizer_t *recognizer)
{
	const apr_array_header_t *tarr = apr_table_elts(recognizer->grammar_table);
	const apr_table_entry_t *telts = (const apr_table_entry_t*)tarr->elts;
	int i;
	for(i = 0; i < tarr->nelts; i++) {
		const char *grammar_file_path = telts[i].val;
		if(grammar_file_path) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove Grammar File [%s] "APT_SIDRES_FMT,
				grammar_file_path,RECOGNIZER_SIDRES(recognizer));
			apr_file_remove(grammar_file_path,recognizer->channel->pool);
		}
	}
	apr_table_clear(recognizer->grammar_table);
	return TRUE;
}

/** Load pocketsphinx grammar [RECOG] */
static mrcp_status_code_e pocketsphinx_grammar_load(pocketsphinx_recognizer_t *recognizer, const char *content_id, const char *content_type, const apt_str_t *content)
{
	/* load grammar */
	mrcp_engine_channel_t *channel = recognizer->channel;
	const apt_dir_layout_t *dir_layout = channel->engine->dir_layout;
	const char *grammar_file_path = NULL;
	const char *grammar_file_name = NULL;
	apr_file_t *fd = NULL;
	apr_status_t rv;
	apr_size_t size;

	/* only JSGF grammar is supported */
	if(strstr(content_type,"jsgf") == NULL) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Not Supported Content-Type [%s] "APT_SIDRES_FMT,
			content_type,RECOGNIZER_SIDRES(recognizer));
		return MRCP_STATUS_CODE_UNSUPPORTED_PARAM_VALUE;
	}

	grammar_file_name = apr_psprintf(channel->pool,"%s-%s.gram",channel->id.buf,content_id);
	grammar_file_path = apt_datadir_filepath_get(dir_layout,grammar_file_name,channel->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create Grammar File [%s] "APT_SIDRES_FMT,
		grammar_file_path,RECOGNIZER_SIDRES(recognizer));
	rv = apr_file_open(&fd,grammar_file_path,APR_CREATE|APR_TRUNCATE|APR_WRITE|APR_BINARY,
		APR_OS_DEFAULT,channel->pool);
	if(rv != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot Open Grammar File to Write [%s] "APT_SIDRES_FMT,
			grammar_file_path,RECOGNIZER_SIDRES(recognizer));
		return MRCP_STATUS_CODE_METHOD_FAILED;
	}

	size = content->length;
	apr_file_write(fd,content->buf,&size);
	apr_file_close(fd);

	/* init pocketsphinx decoder */
	if(pocketsphinx_decoder_init(recognizer,grammar_file_path) != TRUE) {
		apr_file_remove(grammar_file_path,channel->pool);
		return MRCP_STATUS_CODE_METHOD_FAILED;
	}
	recognizer->grammar_id = content_id;
	apr_table_setn(recognizer->grammar_table,content_id,grammar_file_path);
	return MRCP_STATUS_CODE_SUCCESS;
}

/** Unload pocketsphinx grammar [RECOG] */
static mrcp_status_code_e pocketsphinx_grammar_unload(pocketsphinx_recognizer_t *recognizer, const char *content_id)
{
	/* unload grammar */
	const char *grammar_file_path = apr_table_get(recognizer->grammar_table,content_id);
	if(!grammar_file_path) {
		return MRCP_STATUS_CODE_ILLEGAL_PARAM_VALUE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove Grammar File [%s] "APT_SIDRES_FMT,
		grammar_file_path,RECOGNIZER_SIDRES(recognizer));
	apr_file_remove(grammar_file_path,recognizer->channel->pool);
	apr_table_unset(recognizer->grammar_table,content_id);
	return MRCP_STATUS_CODE_SUCCESS;
}

/** Process DEFINE-GRAMMAR request [RECOG] */
static apt_bool_t pocketsphinx_define_grammar(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	const char *content_type = NULL;
	const char *content_id = NULL;
	mrcp_engine_channel_t *channel = recognizer->channel;

	mrcp_generic_header_t *generic_header = mrcp_generic_header_get(request);
	mrcp_recog_header_t *recog_header = mrcp_resource_header_prepare(response);
	if(!generic_header || !recog_header) {
		return FALSE;
	}

	recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_SUCCESS;
	mrcp_resource_header_property_add(response,RECOGNIZER_HEADER_COMPLETION_CAUSE);

	/* content-id must be specified */
	if(mrcp_generic_header_property_check(request,GENERIC_HEADER_CONTENT_ID) == TRUE) {
		content_id = generic_header->content_id.buf;
	}
	if(!content_id) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Missing Content-Id "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
		response->start_line.status_code = MRCP_STATUS_CODE_MISSING_PARAM;
		recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_GRAM_LOAD_FAILURE;
		return FALSE;
	}

	if(mrcp_generic_header_property_check(request,GENERIC_HEADER_CONTENT_LENGTH) == TRUE &&
		generic_header->content_length) {
		/* content-type must be specified */
		if(mrcp_generic_header_property_check(request,GENERIC_HEADER_CONTENT_TYPE) == TRUE) {
			content_type = generic_header->content_type.buf;
		}

		if(!content_type) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Missing Content-Type "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
			response->start_line.status_code = MRCP_STATUS_CODE_MISSING_PARAM;
			recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_GRAM_LOAD_FAILURE;
			return FALSE;
		}

		response->start_line.status_code = pocketsphinx_grammar_load(recognizer,content_id,content_type,&request->body);
		if(response->start_line.status_code != MRCP_STATUS_CODE_SUCCESS) {
			recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_GRAM_LOAD_FAILURE;
			return FALSE;
		}
	}
	else {
		response->start_line.status_code = pocketsphinx_grammar_unload(recognizer,content_id);
		if(response->start_line.status_code != MRCP_STATUS_CODE_SUCCESS) {
			recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_GRAM_LOAD_FAILURE;
			return FALSE;
		}
	}

	/* send asynchronous response */
	mrcp_engine_channel_message_send(channel,response);
	return TRUE;
}

/** Process RECOGNIZE request [RECOG] */
static apt_bool_t pocketsphinx_recognize(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	const char *content_type = NULL;
	mrcp_engine_channel_t *channel = recognizer->channel;
	mrcp_recog_header_t *request_recog_header;
	mrcp_recog_header_t *response_recog_header = mrcp_resource_header_prepare(response);
	mrcp_generic_header_t *generic_header = mrcp_generic_header_get(request);
	if(!generic_header || !response_recog_header) {
		return FALSE;
	}

	/* content-type must be specified */
	if(mrcp_generic_header_property_check(request,GENERIC_HEADER_CONTENT_TYPE) == TRUE) {
		content_type = generic_header->content_type.buf;
	}
	if(!content_type) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Missing Content-Type "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
		response->start_line.status_code = MRCP_STATUS_CODE_MISSING_PARAM;
		response_recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_GRAM_LOAD_FAILURE;
		return FALSE;
	}

	if(strcmp(content_type,"text/uri-list") == 0) {
		/* assume the uri-list contains last defined (active) grammar for now */
	}
	else {
		const char *content_id = NULL;
		/* content-id must be specified */
		if(mrcp_generic_header_property_check(request,GENERIC_HEADER_CONTENT_ID) == TRUE) {
			content_id = generic_header->content_id.buf;
		}
		if(!content_id) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Missing Content-Id "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
			response->start_line.status_code = MRCP_STATUS_CODE_MISSING_PARAM;
			response_recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_GRAM_LOAD_FAILURE;
			return FALSE;
		}

		response->start_line.status_code = pocketsphinx_grammar_load(recognizer,content_id,content_type,&request->body);
		if(response->start_line.status_code != MRCP_STATUS_CODE_SUCCESS) {
			response_recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_GRAM_LOAD_FAILURE;
			return FALSE;
		}
	}

	if(!recognizer->decoder || ps_start_utt(recognizer->decoder, NULL) < 0) {
		response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
		response_recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_ERROR;
		mrcp_resource_header_property_add(response,RECOGNIZER_HEADER_COMPLETION_CAUSE);
		return FALSE;
	}

	recognizer->is_input_timer_on = TRUE;
	/* get recognizer header */
	request_recog_header = mrcp_resource_header_get(request);
	if(request_recog_header) {
		if(mrcp_resource_header_property_check(request,RECOGNIZER_HEADER_START_INPUT_TIMERS) == TRUE) {
			recognizer->is_input_timer_on = request_recog_header->start_input_timers;
		}
		if(mrcp_resource_header_property_check(request,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT) == TRUE) {
			recognizer->properties.no_input_timeout = request_recog_header->no_input_timeout;
		}
		if(mrcp_resource_header_property_check(request,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT) == TRUE) {
			recognizer->properties.recognition_timeout = request_recog_header->recognition_timeout;
		}
		if(mrcp_resource_header_property_check(request,RECOGNIZER_HEADER_SAVE_WAVEFORM) == TRUE) {
			recognizer->properties.save_waveform = request_recog_header->save_waveform;
		}
	}

	/* check if waveform (utterance) should be saved */
	if(recognizer->properties.save_waveform == TRUE) {
		apr_status_t rv;
		const char *waveform_file_name = apr_psprintf(channel->pool,"utter-%s-%"MRCP_REQUEST_ID_FMT".pcm",
			channel->id.buf,
			request->start_line.request_id);
		char *waveform_file_path = NULL;
		apr_filepath_merge(&waveform_file_path,recognizer->properties.save_waveform_dir,
			waveform_file_name,0,channel->pool);

		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open Waveform File [%s] "APT_SIDRES_FMT,
			waveform_file_path,RECOGNIZER_SIDRES(recognizer));
		rv = apr_file_open(&recognizer->waveform,waveform_file_path,APR_CREATE|APR_TRUNCATE|APR_WRITE|APR_BINARY,
			APR_OS_DEFAULT,channel->pool);
		if(rv != APR_SUCCESS) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot Open Waveform File to Write [%s] "APT_SIDRES_FMT,
				waveform_file_path,RECOGNIZER_SIDRES(recognizer));
		}
	}

	response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	/* send asynchronous response */
	mrcp_engine_channel_message_send(channel,response);

	/* reset */
	mpf_activity_detector_reset(recognizer->detector);
	recognizer->no_input_timeout = 0;
	recognizer->recognition_timeout = 0;
	recognizer->partial_result_timeout = 0;
	recognizer->last_result = NULL;
	recognizer->complete_event = NULL;
	
	recognizer->inprogress_recog = request;
	return TRUE;
}

/** Process GET-RESULT request [RECOG] */
static apt_bool_t pocketsphinx_get_result(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	if(pocketsphinx_result_build(recognizer,response) != TRUE) {
		response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
	}

	/* send asynchronous response */
	mrcp_engine_channel_message_send(recognizer->channel,response);
	return TRUE;
}

/** Process START-INPUT-TIMERS request [RECOG] */
static apt_bool_t pocketsphinx_start_input_timers(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	recognizer->is_input_timer_on = TRUE;
	
	/* send asynchronous response */
	mrcp_engine_channel_message_send(recognizer->channel,response);
	return TRUE;
}


/** Process STOP request [RECOG] */
static apt_bool_t pocketsphinx_stop(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	if(recognizer->inprogress_recog) {
		/* store pending STOP response for further processing */
		recognizer->stop_response = response;
		return TRUE;
	}

	/* send asynchronous response */
	mrcp_engine_channel_message_send(recognizer->channel,response);
	return TRUE;
}

/** Process RECOGNITION-COMPLETE event [RECOG] */
static apt_bool_t pocketsphinx_recognition_complete(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *complete_event)
{
	mrcp_recog_header_t *recog_header;
	if(!recognizer->inprogress_recog) {
		/* false event */
		return FALSE;
	}

	recognizer->inprogress_recog = NULL;
	ps_end_utt(recognizer->decoder);

	if(recognizer->waveform) {
		apr_file_close(recognizer->waveform);
		recognizer->waveform = NULL;
	}

	if(recognizer->stop_response) {
		/* recognition has been stopped, send STOP response instead */
		mrcp_message_t *response = recognizer->stop_response;
		recognizer->stop_response = NULL;
		if(recognizer->close_requested == FALSE) {
			mrcp_engine_channel_message_send(recognizer->channel,response);
		}
		return TRUE;
	}
	
	recog_header = mrcp_resource_header_get(complete_event);
	if(recog_header->completion_cause == RECOGNIZER_COMPLETION_CAUSE_SUCCESS || 
		recog_header->completion_cause == RECOGNIZER_COMPLETION_CAUSE_RECOGNITION_TIMEOUT) {
		int32 score;
		char const *hyp;
		char const *uttid;

		hyp = ps_get_hyp(recognizer->decoder, &score, &uttid);
		if(hyp && strlen(hyp) > 0) {
			int32 prob;
			recognizer->last_result = apr_pstrdup(recognizer->channel->pool,hyp);
			prob = ps_get_prob(recognizer->decoder, &uttid); 
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get Recognition Final Result [%s] Prob [%d] Score [%d] "APT_SIDRES_FMT,
				hyp,prob,score,RECOGNIZER_SIDRES(recognizer));
			if(pocketsphinx_result_build(recognizer,complete_event) != TRUE) {
				recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_ERROR;
			}
		}
		else {
			recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_NO_MATCH;
		}
	}

	/* send asynchronous event */
	mrcp_engine_channel_message_send(recognizer->channel,complete_event);
	return TRUE;
}

/** Dispatch MRCP request [RECOG] */
static apt_bool_t pocketsphinx_request_dispatch(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request)
{
	apt_bool_t processed = FALSE;
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Dispatch Request %s "APT_SIDRES_FMT,
		request->start_line.method_name.buf,
		RECOGNIZER_SIDRES(recognizer));
	switch(request->start_line.method_id) {
		case RECOGNIZER_SET_PARAMS:
			break;
		case RECOGNIZER_GET_PARAMS:
			break;
		case RECOGNIZER_DEFINE_GRAMMAR:
			processed = pocketsphinx_define_grammar(recognizer,request,response);
			break;
		case RECOGNIZER_RECOGNIZE:
			processed = pocketsphinx_recognize(recognizer,request,response);
			break;
		case RECOGNIZER_GET_RESULT:
			processed = pocketsphinx_get_result(recognizer,request,response);
			break;
		case RECOGNIZER_START_INPUT_TIMERS:
			processed = pocketsphinx_start_input_timers(recognizer,request,response);
			break;
		case RECOGNIZER_STOP:
			processed = pocketsphinx_stop(recognizer,request,response);
			break;
		default:
			break;
	}
	if(processed == FALSE) {
		/* send asynchronous response for non handled request */
		mrcp_engine_channel_message_send(recognizer->channel,response);
	}
	return TRUE;
}


/** Recognition thread [RECOG] */
static void* APR_THREAD_FUNC pocketsphinx_recognizer_run(apr_thread_t *thread, void *data)
{
	pocketsphinx_recognizer_t *recognizer = data;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Run Recognition Thread "APT_SIDRES_FMT, RECOGNIZER_SIDRES(recognizer));
	/** Send response to channel_open request */
	mrcp_engine_channel_open_respond(recognizer->channel,TRUE);

	do {
		apr_thread_mutex_lock(recognizer->mutex);
		/** Wait for MRCP requests */
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Wait for incoming messages "APT_SIDRES_FMT, RECOGNIZER_SIDRES(recognizer));
		if (!recognizer->message_waiting) {
			apr_thread_cond_wait(recognizer->wait_object,recognizer->mutex);
		}
		recognizer->message_waiting = FALSE;

		if(recognizer->request) {
			/* store request message and further dispatch it */
			mrcp_message_t *request = recognizer->request;
			recognizer->request = NULL;
			pocketsphinx_request_dispatch(recognizer,request);
		}
		if(recognizer->complete_event) {
			/* end of input detected, get recognition result and raise recognition complete event */
			pocketsphinx_recognition_complete(recognizer,recognizer->complete_event);
		}
		apr_thread_mutex_unlock(recognizer->mutex);
	}
	while(recognizer->close_requested == FALSE);

	/** Clear all the defined grammars */
	pocketsphinx_grammars_clear(recognizer);
	
	if(recognizer->decoder) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Free Decoder "APT_SIDRES_FMT, RECOGNIZER_SIDRES(recognizer));
		/** Free pocketsphinx decoder */
		ps_free(recognizer->decoder);
		recognizer->decoder = NULL;
	}

	recognizer->thread = NULL;
	/** Finally send response to channel_close request */
	mrcp_engine_channel_close_respond(recognizer->channel);

	/** Exit thread */
	apr_thread_exit(thread,APR_SUCCESS);
	return NULL;
}



/* Start of input (utterance) [MPF]  */
static apt_bool_t pocketsphinx_start_of_input(pocketsphinx_recognizer_t *recognizer)
{
	/* create START-OF-INPUT event */
	mrcp_message_t *message = mrcp_event_create(
						recognizer->inprogress_recog,
						RECOGNIZER_START_OF_INPUT,
						recognizer->inprogress_recog->pool);
	if(!message) {
		return FALSE;
	}

	/* set request state */
	message->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	/* send asynchronous event */
	return mrcp_engine_channel_message_send(recognizer->channel,message);
}

/* End of input (utterance) [MPF] */
static apt_bool_t pocketsphinx_end_of_input(pocketsphinx_recognizer_t *recognizer, mrcp_recog_completion_cause_e cause)
{
	mrcp_recog_header_t *recog_header;
	/* create RECOGNITION-COMPLETE event */
	mrcp_message_t *message = mrcp_event_create(
						recognizer->inprogress_recog,
						RECOGNIZER_RECOGNITION_COMPLETE,
						recognizer->inprogress_recog->pool);
	if(!message) {
		return FALSE;
	}

	/* get/allocate recognizer header */
	recog_header = mrcp_resource_header_prepare(message);
	if(recog_header) {
		/* set completion cause */
		recog_header->completion_cause = cause;
		mrcp_resource_header_property_add(message,RECOGNIZER_HEADER_COMPLETION_CAUSE);
	}
	/* set request state */
	message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;

	/* signal recognition thread first */
	apr_thread_mutex_lock(recognizer->mutex);
	recognizer->complete_event = message;
	recognizer->message_waiting = TRUE;
	apr_thread_cond_signal(recognizer->wait_object);
	apr_thread_mutex_unlock(recognizer->mutex);
	return TRUE;
}

/* Process audio frame [MPF] */
static apt_bool_t pocketsphinx_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	pocketsphinx_recognizer_t *recognizer = stream->obj;

	/* check whether recognition has been started and not completed yet */
	if(recognizer->inprogress_recog && !recognizer->complete_event) {
		mpf_detector_event_e det_event;

		/* first check if STOP has been requested */
		if(recognizer->stop_response) {
			/* recognition has been stopped -> acknowledge with complete-event */
			pocketsphinx_end_of_input(recognizer,RECOGNIZER_COMPLETION_CAUSE_SUCCESS);
			return TRUE;
		}

		if(recognizer->waveform) {
			/* write utterance to file */
			apr_size_t size = frame->codec_frame.size;
			apr_file_write(recognizer->waveform,frame->codec_frame.buffer,&size);
		}

		if(ps_process_raw(
					recognizer->decoder, 
					(const int16 *)frame->codec_frame.buffer, 
					frame->codec_frame.size / sizeof(int16),
					FALSE, 
					FALSE) < 0) {

			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Process Raw Data "APT_SIDRES_FMT,
				RECOGNIZER_SIDRES(recognizer));
		}

		recognizer->partial_result_timeout += CODEC_FRAME_TIME_BASE;
		if(recognizer->partial_result_timeout == recognizer->properties.partial_result_timeout) {
			int32 score;
			char const *hyp;
			char const *uttid;

			recognizer->partial_result_timeout = 0;
			hyp = ps_get_hyp(recognizer->decoder, &score, &uttid);
			if(hyp && strlen(hyp) > 0) {
				if(recognizer->last_result == NULL || 0 != strcmp(recognizer->last_result, hyp)) {
					recognizer->last_result = apr_pstrdup(recognizer->channel->pool,hyp);
					apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get Recognition Partial Result [%s] Score [%d] "APT_SIDRES_FMT,
						hyp,score,RECOGNIZER_SIDRES(recognizer));

					/* reset input timer as we have partial match now */
					if(score != 0 && recognizer->is_input_timer_on) {
						recognizer->is_input_timer_on = FALSE;
					}
				}
			}
		}

		if(recognizer->is_input_timer_on) {
			recognizer->no_input_timeout += CODEC_FRAME_TIME_BASE;
			if(recognizer->no_input_timeout == recognizer->properties.no_input_timeout) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Noinput Timeout Elapsed "APT_SIDRES_FMT,
						RECOGNIZER_SIDRES(recognizer));
				pocketsphinx_end_of_input(recognizer,RECOGNIZER_COMPLETION_CAUSE_NO_INPUT_TIMEOUT);
				return TRUE;
			}
		}

		recognizer->recognition_timeout += CODEC_FRAME_TIME_BASE;
		if(recognizer->recognition_timeout == recognizer->properties.recognition_timeout) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Recognition Timeout Elapsed "APT_SIDRES_FMT,
					RECOGNIZER_SIDRES(recognizer));
			pocketsphinx_end_of_input(recognizer,RECOGNIZER_COMPLETION_CAUSE_RECOGNITION_TIMEOUT);
			return TRUE;
		}

		det_event = mpf_activity_detector_process(recognizer->detector,frame);
		switch(det_event) {
			case MPF_DETECTOR_EVENT_ACTIVITY:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Detected Voice Activity "APT_SIDRES_FMT,
					RECOGNIZER_SIDRES(recognizer));
				pocketsphinx_start_of_input(recognizer);
				break;
			case MPF_DETECTOR_EVENT_INACTIVITY:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Detected Voice Inactivity "APT_SIDRES_FMT,
					RECOGNIZER_SIDRES(recognizer));
				pocketsphinx_end_of_input(recognizer,RECOGNIZER_COMPLETION_CAUSE_SUCCESS);
				break;
			default:
				break;
		}
	}

	return TRUE;
}
