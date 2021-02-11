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
#include "mrcp_generic_header.h"
/* synthesizer includes */
#include "mrcp_synth_header.h"
#include "mrcp_synth_resource.h"
/* recognizer includes */
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"

#define SAMPLE_VOICE_AGE 28
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
		mrcp_generic_header_t *generic_header;
		mrcp_synth_header_t *synth_header;
		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(message);
		if(generic_header) {
			/* set generic header fields */
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Content-Type: %s",SAMPLE_CONTENT_TYPE);
			apt_string_assign(&generic_header->content_type,SAMPLE_CONTENT_TYPE,message->pool);
			mrcp_generic_header_property_add(message,GENERIC_HEADER_CONTENT_TYPE);
		}
		/* get/allocate synthesizer header */
		synth_header = mrcp_resource_header_prepare(message);
		if(synth_header) {
			/* set synthesizer header fields */
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Voice-Age: %d",SAMPLE_VOICE_AGE);
			synth_header->voice_param.age = SAMPLE_VOICE_AGE;
			mrcp_resource_header_property_add(message,SYNTHESIZER_HEADER_VOICE_AGE);
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
	mrcp_generic_header_t *generic_header;
	mrcp_synth_header_t *synth_header;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Test SPEAK Request");
	res = FALSE;
	/* get generic header */
	generic_header = mrcp_generic_header_get(message);
	if(generic_header) {
		/* test content type header */
		if(mrcp_generic_header_property_check(message,GENERIC_HEADER_CONTENT_TYPE) == TRUE) {
			if(strncasecmp(generic_header->content_type.buf,SAMPLE_CONTENT_TYPE,generic_header->content_type.length) == 0) {
				/* OK */
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get Content-Type: %s",generic_header->content_type.buf);
				res = TRUE;
			}
		}
	}
	if(res == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Test Generic Header");
		return FALSE;
	}

	res = FALSE;
	/* get synthesizer header */
	synth_header = mrcp_resource_header_get(message);
	if(synth_header) {
		/* test voice age header */
		if(mrcp_resource_header_property_check(message,SYNTHESIZER_HEADER_VOICE_AGE) == TRUE) {
			if(synth_header->voice_param.age == SAMPLE_VOICE_AGE) {
				/* OK */
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get Voice-Age: %"APR_SIZE_T_FMT,synth_header->voice_param.age);
				res = TRUE;
			}
		}
	}
	if(res == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Test Synthesizer Header");
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
		/* get/allocate synthesizer header */
		mrcp_synth_header_t *synth_header = mrcp_resource_header_prepare(event_message);
		if(synth_header) {
			/* set completion cause */
			synth_header->completion_cause = SYNTHESIZER_COMPLETION_CAUSE_NORMAL;
			mrcp_resource_header_property_add(event_message,SYNTHESIZER_HEADER_COMPLETION_CAUSE);
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
		apt_str_t param_name;
		apt_str_t param_value;
		mrcp_generic_header_t *generic_header;
		mrcp_synth_header_t *synth_header;
		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(message);
		if(generic_header) {
			/* set content id empty header */
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Content-ID: <empty>");
			mrcp_generic_header_name_property_add(message,GENERIC_HEADER_CONTENT_ID);
			
			/* set vendor specific params header */
			generic_header->vendor_specific_params = apt_pair_array_create(1,pool);
			apt_string_set(&param_name,SAMPLE_PARAM_NAME);
			apt_string_reset(&param_value);
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Vendor-Specific-Params: %s",param_name.buf);
			apt_pair_array_append(generic_header->vendor_specific_params,&param_name,&param_value,pool);
			mrcp_generic_header_property_add(message,GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS);
		}
		/* get/allocate synthesizer header */
		synth_header = mrcp_resource_header_prepare(message);
		if(synth_header) {
			/* set voice age empty header */
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Voice-Age: <empty>");
			mrcp_resource_header_name_property_add(message,SYNTHESIZER_HEADER_VOICE_AGE);
		}
	}
	return message;
}

/* Create GET-PARAMS response */
static mrcp_message_t* get_params_response_create(mrcp_resource_factory_t *factory, mrcp_message_t *request)
{
	apt_bool_t res;
	mrcp_message_t *response;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create GET-PARAMS Response");
	response = mrcp_response_create(request,request->pool);
	if(response) {
		mrcp_generic_header_t *generic_header;
		mrcp_synth_header_t *synth_header;
		res = FALSE;
		/* get generic header */
		generic_header = mrcp_generic_header_get(request);
		if(generic_header) {
			mrcp_generic_header_t *res_generic_header = mrcp_generic_header_prepare(response);
			/* test content id header */
			if(mrcp_generic_header_property_check(request,GENERIC_HEADER_CONTENT_ID) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Content-ID: %s",SAMPLE_CONTENT_ID);
				apt_string_assign(&res_generic_header->content_id,SAMPLE_CONTENT_ID,response->pool);
				mrcp_generic_header_property_add(response,GENERIC_HEADER_CONTENT_ID);
				res = TRUE;
			}
			/* test vendor specific header */
			if(mrcp_generic_header_property_check(request,GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS) == TRUE) {
				apt_str_t name;
				const apt_pair_t *pair;
				res_generic_header->vendor_specific_params = apt_pair_array_create(1,response->pool);
				apt_string_set(&name,SAMPLE_PARAM_NAME);
				pair = apt_pair_array_find(generic_header->vendor_specific_params,&name);
				if(pair) {
					apt_str_t value;
					apt_string_set(&value,SAMPLE_PARAM_VALUE);
					apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Vendor-Specific-Params: %s=%s",name.buf,value.buf);
					apt_pair_array_append(res_generic_header->vendor_specific_params,&name,&value,response->pool);
				}
				mrcp_generic_header_property_add(response,GENERIC_HEADER_VENDOR_SPECIFIC_PARAMS);
				res = TRUE;
			}
		}

		if(res == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Test Generic Header");
			return NULL;
		}	

		res = FALSE;
		/* get synthesizer header */
		synth_header = mrcp_resource_header_get(request);
		if(synth_header) {
			mrcp_synth_header_t *res_synth_header = mrcp_resource_header_prepare(response);
			/* test voice age header */
			if(mrcp_resource_header_property_check(request,SYNTHESIZER_HEADER_VOICE_AGE) == TRUE) {
				res_synth_header->voice_param.age = SAMPLE_VOICE_AGE;
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set Voice-Age: %"APR_SIZE_T_FMT,res_synth_header->voice_param.age);
				mrcp_resource_header_property_add(response,SYNTHESIZER_HEADER_VOICE_AGE);
				res = TRUE;
			}
		}
		if(res == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Test Synthesizer Header");
			return NULL;
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

apt_test_suite_t* set_get_test_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"set-get",NULL,set_get_test_run);
	return suite;
}
