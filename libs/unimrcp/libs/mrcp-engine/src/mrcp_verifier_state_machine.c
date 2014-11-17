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
 * $Id: mrcp_verifier_state_machine.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "apt_obj_list.h"
#include "apt_log.h"
#include "mrcp_verifier_state_machine.h"
#include "mrcp_generic_header.h"
#include "mrcp_verifier_header.h"
#include "mrcp_verifier_resource.h"
#include "mrcp_message.h"

/** MRCP verifier states */
typedef enum {
	VERIFIER_STATE_IDLE,
	VERIFIER_STATE_OPENED,
	VERIFIER_STATE_VERIFYING,

	VERIFIER_STATE_COUNT
} mrcp_verifier_state_e;

static const char * state_names[VERIFIER_STATE_COUNT] = {
	"IDLE",
	"OPENED",
	"VERIFYING"
};

typedef struct mrcp_verifier_state_machine_t mrcp_verifier_state_machine_t;

struct mrcp_verifier_state_machine_t {
	/** state machine base */
	mrcp_state_machine_t   base;
	/** verifier state */
	mrcp_verifier_state_e  state;
	/** request sent to verification engine and waiting for the response to be received */
	mrcp_message_t        *active_request;
	/** in-progress verify request */
	mrcp_message_t        *verify;
	/** properties used in set/get params */
	mrcp_message_header_t *properties;
};

typedef apt_bool_t (*verifier_method_f)(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message);

static APR_INLINE apt_bool_t verifier_request_dispatch(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	state_machine->active_request = message;
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE apt_bool_t verifier_response_dispatch(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	state_machine->active_request = NULL;
	if(state_machine->base.active == FALSE) {
		/* this is the response to deactivation (STOP) request */
		return state_machine->base.on_deactivate(&state_machine->base);
	}
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE apt_bool_t verifier_event_dispatch(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->base.active == FALSE) {
		/* do nothing, state machine has already been deactivated */
		return FALSE;
	}
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE void verifier_state_change(mrcp_verifier_state_machine_t *state_machine, mrcp_verifier_state_e state, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"State Transition %s -> %s "APT_SIDRES_FMT,
		state_names[state_machine->state],
		state_names[state],
		MRCP_MESSAGE_SIDRES(message));
	state_machine->state = state;
	if(state == VERIFIER_STATE_IDLE) {
		state_machine->verify = NULL;
	}
}


static apt_bool_t verifier_request_set_params(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_header_fields_set(state_machine->properties,&message->header,message->pool);
	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_set_params(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_get_params(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_get_params(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_header_fields_get(&message->header,state_machine->properties,&state_machine->active_request->header,message->pool);
	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_start_session(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state == VERIFIER_STATE_VERIFYING) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return verifier_response_dispatch(state_machine,response_message);
	}

	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_start_session(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	verifier_state_change(state_machine,VERIFIER_STATE_OPENED,message);

	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_end_session(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state == VERIFIER_STATE_IDLE) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return verifier_response_dispatch(state_machine,response_message);
	}

	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_end_session(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	verifier_state_change(state_machine,VERIFIER_STATE_IDLE,message);

	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_query_voiceprint(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state == VERIFIER_STATE_IDLE) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return verifier_response_dispatch(state_machine,response_message);
	}

	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_query_voiceprint(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{

	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_delete_voiceprint(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state == VERIFIER_STATE_IDLE) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return verifier_response_dispatch(state_machine,response_message);
	}

	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_delete_voiceprint(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_verify(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state == VERIFIER_STATE_IDLE || state_machine->state == VERIFIER_STATE_VERIFYING) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return verifier_response_dispatch(state_machine,response_message);
	}

	state_machine->verify = message;
	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_verify(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	verifier_state_change(state_machine,VERIFIER_STATE_VERIFYING,message);

	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_verify_from_buffer(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state == VERIFIER_STATE_IDLE || state_machine->state == VERIFIER_STATE_VERIFYING) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return verifier_response_dispatch(state_machine,response_message);
	}

	state_machine->verify = message;
	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_verify_from_buffer(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	verifier_state_change(state_machine,VERIFIER_STATE_VERIFYING,message);

	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_verify_rollback(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_verify_rollback(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_stop(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state == VERIFIER_STATE_IDLE) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return verifier_response_dispatch(state_machine,response_message);
	}

	if(state_machine->state == VERIFIER_STATE_OPENED) {
		/* no in-progress VERIFY request, sending immediate response */
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		return verifier_response_dispatch(state_machine,response_message);
	}

	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_stop(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
	/* append active id list */
	active_request_id_list_append(generic_header,state_machine->verify->start_line.request_id);
	mrcp_generic_header_property_add(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
	verifier_state_change(state_machine,VERIFIER_STATE_OPENED,message);
	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_clear_buffer(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_clear_buffer(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_start_input_timers(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state == VERIFIER_STATE_IDLE || state_machine->state == VERIFIER_STATE_VERIFYING) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return verifier_response_dispatch(state_machine,response_message);
	}

	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_start_input_timers(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	return verifier_response_dispatch(state_machine,message);
}

static apt_bool_t verifier_request_get_intermidiate_result(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state != VERIFIER_STATE_VERIFYING) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return verifier_response_dispatch(state_machine,response_message);
	}
	return verifier_request_dispatch(state_machine,message);
}

static apt_bool_t verifier_response_get_intermidiate_result(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	return verifier_response_dispatch(state_machine,message);
}


static apt_bool_t verifier_event_start_of_input(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->verify) {
		/* unexpected event, no in-progress verify request */
		return FALSE;
	}

	if(state_machine->verify->start_line.request_id != message->start_line.request_id) {
		/* unexpected event */
		return FALSE;
	}
	
	message->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	return verifier_event_dispatch(state_machine,message);
}

static apt_bool_t verifier_event_verification_complete(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->verify) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Unexpected VERIFICATION-COMPLETE Event "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		return FALSE;
	}

	if(state_machine->verify->start_line.request_id != message->start_line.request_id) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Unexpected VERIFICATION-COMPLETE Event "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		return FALSE;
	}

	if(state_machine->active_request && state_machine->active_request->start_line.method_id == VERIFIER_STOP) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Ignore VERIFICATION-COMPLETE Event "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]: waiting for STOP response",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		return FALSE;
	}

	if(mrcp_resource_header_property_check(message,VERIFIER_HEADER_COMPLETION_CAUSE) != TRUE) {
		mrcp_verifier_header_t *verifier_header = mrcp_resource_header_prepare(message);
		verifier_header->completion_cause = VERIFIER_COMPLETION_CAUSE_SUCCESS;
		mrcp_resource_header_property_add(message,VERIFIER_HEADER_COMPLETION_CAUSE);
	}
	verifier_state_change(state_machine,VERIFIER_STATE_OPENED,message);
	return verifier_event_dispatch(state_machine,message);
}

static verifier_method_f verifier_request_method_array[VERIFIER_METHOD_COUNT] = {
	verifier_request_set_params,
	verifier_request_get_params,
	verifier_request_start_session,
	verifier_request_end_session,
	verifier_request_query_voiceprint,
	verifier_request_delete_voiceprint,
	verifier_request_verify,
	verifier_request_verify_from_buffer,
	verifier_request_verify_rollback,
	verifier_request_stop,
	verifier_request_clear_buffer,
	verifier_request_start_input_timers,
	verifier_request_get_intermidiate_result
};

static verifier_method_f verifier_response_method_array[VERIFIER_METHOD_COUNT] = {
	verifier_response_set_params,
	verifier_response_get_params,
	verifier_response_start_session,
	verifier_response_end_session,
	verifier_response_query_voiceprint,
	verifier_response_delete_voiceprint,
	verifier_response_verify,
	verifier_response_verify_from_buffer,
	verifier_response_verify_rollback,
	verifier_response_stop,
	verifier_response_clear_buffer,
	verifier_response_start_input_timers,
	verifier_response_get_intermidiate_result
};

static verifier_method_f verifier_event_method_array[VERIFIER_EVENT_COUNT] = {
	verifier_event_start_of_input,
	verifier_event_verification_complete
};

/** Update state according to received incoming request from MRCP client */
static apt_bool_t verifier_request_state_update(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	verifier_method_f method;
	if(message->start_line.method_id >= VERIFIER_METHOD_COUNT) {
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process %s Request "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.method_name.buf,
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	method = verifier_request_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return verifier_request_dispatch(state_machine,message);
}

/** Update state according to received outgoing response from verification engine */
static apt_bool_t verifier_response_state_update(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	verifier_method_f method;
	if(!state_machine->active_request) {
		/* unexpected response, no active request waiting for response */
		return FALSE;
	}
	if(state_machine->active_request->start_line.request_id != message->start_line.request_id) {
		/* unexpected response, request id doesn't match */
		return FALSE;
	}

	if(message->start_line.method_id >= VERIFIER_METHOD_COUNT) {
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process %s Response "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.method_name.buf,
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	method = verifier_response_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return verifier_response_dispatch(state_machine,message);
}

/** Update state according to received outgoing event from verification engine */
static apt_bool_t verifier_event_state_update(mrcp_verifier_state_machine_t *state_machine, mrcp_message_t *message)
{
	verifier_method_f method;
	if(message->start_line.method_id >= VERIFIER_EVENT_COUNT) {
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process %s Event "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.method_name.buf,
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	method = verifier_event_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return verifier_event_dispatch(state_machine,message);
}

/** Update state according to request received from MRCP client or response/event received from verification engine */
static apt_bool_t verifier_state_update(mrcp_state_machine_t *base, mrcp_message_t *message)
{
	mrcp_verifier_state_machine_t *state_machine = (mrcp_verifier_state_machine_t*)base;
	apt_bool_t status = TRUE;
	switch(message->start_line.message_type) {
		case MRCP_MESSAGE_TYPE_REQUEST:
			status = verifier_request_state_update(state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_RESPONSE:
			status = verifier_response_state_update(state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_EVENT:
			status = verifier_event_state_update(state_machine,message);
			break;
		default:
			status = FALSE;
			break;
	}
	return status;
}

/** Deactivate state machine */
static apt_bool_t verifier_state_deactivate(mrcp_state_machine_t *base)
{
	mrcp_verifier_state_machine_t *state_machine = (mrcp_verifier_state_machine_t*)base;
	mrcp_message_t *message;
	mrcp_message_t *source;
	if(state_machine->state != VERIFIER_STATE_VERIFYING) {
		/* no in-progress VERIFY request to deactivate */
		return FALSE;
	}
	source = state_machine->verify;
	if(!source) {
		return FALSE;
	}

	/* create internal STOP request */
	message = mrcp_request_create(
						source->resource,
						source->start_line.version,
						VERIFIER_STOP,
						source->pool);
	message->channel_id = source->channel_id;
	message->start_line.request_id = source->start_line.request_id + 1;
	apt_string_set(&message->start_line.method_name,"DEACTIVATE"); /* informative only */
	message->header = source->header;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create and Process STOP Request "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	return verifier_request_dispatch(state_machine,message);
}

/** Create MRCP verification state machine */
mrcp_state_machine_t* mrcp_verifier_state_machine_create(void *obj, mrcp_version_e version, apr_pool_t *pool)
{
	mrcp_verifier_state_machine_t *state_machine = apr_palloc(pool,sizeof(mrcp_verifier_state_machine_t));
	mrcp_state_machine_init(&state_machine->base,obj);
	state_machine->base.update = verifier_state_update;
	state_machine->base.deactivate = verifier_state_deactivate;
	state_machine->state = VERIFIER_STATE_IDLE;
	state_machine->active_request = NULL;
	state_machine->verify = NULL;
	state_machine->properties = mrcp_message_header_create(
			mrcp_generic_header_vtable_get(version),
			mrcp_verifier_header_vtable_get(version),
			pool);
	return &state_machine->base;
}
