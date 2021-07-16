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

#include "apt_test_suite.h"
#include "apt_log.h"
/* common includes */
#include "mrcp_resource_loader.h"
#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
/* synthesizer includes */
#include "mrcp_synth_resource.h"
/* recognizer includes */
#include "mrcp_recog_resource.h"

#define SAMPLE_VOICE_AGE "28"
#define SAMPLE_CONTENT_TYPE "application/synthesis+ssml"
#define SAMPLE_CONTENT_ID "123456"
#define SAMPLE_CONTENT "SSML content goes here"
#define SAMPLE_PARAM_NAME "SampleParamName"
#define SAMPLE_PARAM_VALUE "SampleParamValue"

/* Create SPEAK request */
static mrcp_message_t* speak_request_create(mrcp_resource_factory_t *factory, apr_pool_t *pool)
{
	mrcp_message_t *message;
	mrcp_resource_t *resource = mrcp_resource_get(factory,MRCP_SYNTHESIZER_RESOURCE);
	if(!resource) {
		return NULL;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create SPEAK Request");
	message = mrcp_request_create(resource,MRCP_VERSION_2,SYNTHESIZER_SPEAK,pool);
	if(message) {
		/* set transparent header fields */
		apt_header_field_t *header_field;
		header_field = apt_header_field_create_c("Content-Type",SAMPLE_CONTENT_TYPE,message->pool);
		if(header_field) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set %s: %s",header_field->name.buf,header_field->value.buf);
			mrcp_message_header_field_add(message,header_field);
		}

		header_field = apt_header_field_create_c("Voice-Age",SAMPLE_VOICE_AGE,message->pool);
		if(header_field) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set %s: %s",header_field->name.buf,header_field->value.buf);
			mrcp_message_header_field_add(message,header_field);
		}

		/* set message body */
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Body: %s",SAMPLE_CONTENT);
		apt_string_assign(&message->body,SAMPLE_CONTENT,message->pool);
	}
	return message;
}

/* Test SPEAK request */
static apt_bool_t speak_request_test(mrcp_resource_factory_t *factory, mrcp_message_t *message)
{
	apt_bool_t res;
	apt_header_field_t *header_field;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Test SPEAK Request");
	res = FALSE;

	header_field = NULL;
	while( (header_field = mrcp_message_next_header_field_get(message,header_field)) != NULL ) {
		if(strncasecmp(header_field->name.buf,"Content-Type",header_field->name.length) == 0) {
			if(strncasecmp(header_field->value.buf,SAMPLE_CONTENT_TYPE,header_field->value.length) == 0) {
				/* OK */
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get %s: %s",header_field->name.buf,header_field->value.buf);
				res = TRUE;
			}
		}
		else if(strncasecmp(header_field->name.buf,"Voice-Age",header_field->name.length) == 0) {
			if(strncasecmp(header_field->value.buf,SAMPLE_VOICE_AGE,header_field->value.length) == 0) {
				/* OK */
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get %s: %s",header_field->name.buf,header_field->value.buf);
				res = TRUE;
			}
		}
	}
	if(res == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Test Header Fields");
		return FALSE;
	}

	if(strncasecmp(message->body.buf,SAMPLE_CONTENT,message->body.length) != 0) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Test Message Body");
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get Body: %s",message->body.buf);
	return TRUE;
}

/* Create SPEAK response */
static mrcp_message_t* speak_response_create(mrcp_resource_factory_t *factory, const mrcp_message_t *request)
{
	mrcp_message_t *response;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create SPEAK Response");
	response = mrcp_response_create(request,request->pool);
	if(response) {
		/* set IN-PROGRESS state */
		response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	}
	return response;
}

/* Create SPEAK-COMPLETE event */
static mrcp_message_t* speak_event_create(mrcp_resource_factory_t *factory, const mrcp_message_t *request)
{
	mrcp_message_t *event_message;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create SPEAK-COMPLETE Event");
	event_message = mrcp_event_create(request,SYNTHESIZER_SPEAK_COMPLETE,request->pool);
	if(event_message) {
		apt_header_field_t *header_field;
		header_field = apt_header_field_create_c("Completion-Cause","000 normal",event_message->pool);
		if(header_field) {
			mrcp_message_header_field_add(event_message,header_field);
		}

		/* set request state */
		event_message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;
	}
	return event_message;
}

/* Create GET-PARAMS request */
static mrcp_message_t* get_params_request_create(mrcp_resource_factory_t *factory, apr_pool_t *pool)
{
	mrcp_message_t *message;
	mrcp_resource_t *resource = mrcp_resource_get(factory,MRCP_SYNTHESIZER_RESOURCE);
	if(!resource) {
		return NULL;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create GET-PARAMS Request");
	message = mrcp_request_create(resource,MRCP_VERSION_2,SYNTHESIZER_GET_PARAMS,pool);
	if(message) {
		apt_header_field_t *header_field;
		header_field = apt_header_field_create_c("Content-Id","",message->pool);
		if(header_field) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set %s:",header_field->name.buf);
			mrcp_message_header_field_add(message,header_field);
		}
		header_field = apt_header_field_create_c("Vendor-Specific-Params",SAMPLE_PARAM_NAME,message->pool);
		if(header_field) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set %s:",header_field->name.buf);
			mrcp_message_header_field_add(message,header_field);
		}
		header_field = apt_header_field_create_c("Voice-Age","",message->pool);
		if(header_field) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set %s:",header_field->name.buf);
			mrcp_message_header_field_add(message,header_field);
		}
	}
	return message;
}

/* Create GET-PARAMS response */
static mrcp_message_t* get_params_response_create(mrcp_resource_factory_t *factory, mrcp_message_t *request)
{
	mrcp_message_t *response;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create GET-PARAMS Response");
	response = mrcp_response_create(request,request->pool);
	if(response) {
		apt_header_field_t *header_field;
		header_field = apt_header_field_create_c("Content-Id",SAMPLE_CONTENT_ID,response->pool);
		if(header_field) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set %s: %s",header_field->name.buf,header_field->value.buf);
			mrcp_message_header_field_add(response,header_field);
		}
		header_field = apt_header_field_create_c("Vendor-Specific-Params",SAMPLE_PARAM_NAME"="SAMPLE_PARAM_VALUE,response->pool);
		if(header_field) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set %s: %s",header_field->name.buf,header_field->value.buf);
			mrcp_message_header_field_add(response,header_field);
		}
		header_field = apt_header_field_create_c("Voice-Age",SAMPLE_VOICE_AGE,response->pool);
		if(header_field) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set %s: %s",header_field->name.buf,header_field->value.buf);
			mrcp_message_header_field_add(response,header_field);
		}	
	}
	return response;
}


static apt_bool_t speak_test_run(apt_test_suite_t *suite, mrcp_resource_factory_t *factory)
{
	mrcp_message_t *message = speak_request_create(factory,suite->pool);
	if(!message) {
		return FALSE;
	}
	
	if(speak_request_test(factory,message) != TRUE) {
		return FALSE;
	}

	speak_response_create(factory,message);
	speak_event_create(factory,message);
	return TRUE;
}

static apt_bool_t get_params_test_run(apt_test_suite_t *suite, mrcp_resource_factory_t *factory)
{
	mrcp_message_t *message = get_params_request_create(factory,suite->pool);
	if(!message) {
		return FALSE;
	}

	get_params_response_create(factory,message);
	return TRUE;
}

static apt_bool_t set_get_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	mrcp_resource_factory_t *factory;
	mrcp_resource_loader_t *resource_loader;
	resource_loader = mrcp_resource_loader_create(TRUE,suite->pool);
	if(!resource_loader) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Resource Loader");
		return FALSE;
	}
	
	factory = mrcp_resource_factory_get(resource_loader);
	if(!factory) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Resource Factory");
		return FALSE;
	}

	speak_test_run(suite,factory);
	get_params_test_run(suite,factory);
	
	mrcp_resource_factory_destroy(factory);
	return TRUE;
}

apt_test_suite_t* transparent_set_get_test_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"trans-set-get",NULL,set_get_test_run);
	return suite;
}
