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

#include "apt_obj_list.h"
#include "apt_log.h"
#include "mrcp_recorder_state_machine.h"
#include "mrcp_generic_header.h"
#include "mrcp_recorder_header.h"
#include "mrcp_recorder_resource.h"
#include "mrcp_message.h"

/** MRCP recorder states */
typedef enum {
	RECORDER_STATE_IDLE,
	RECORDER_STATE_RECORDING,

	RECORDER_STATE_COUNT
} mrcp_recorder_state_e;

static const char * state_names[RECORDER_STATE_COUNT] = {
	"IDLE",
	"RECORDING",
};

typedef struct mrcp_recorder_state_machine_t mrcp_recorder_state_machine_t;

struct mrcp_recorder_state_machine_t {
	/** state machine base */
	mrcp_state_machine_t   base;
	/** recorder state */
	mrcp_recorder_state_e  state;
	/** request sent to recorder engine and waiting for the response to be received */
	mrcp_message_t        *active_request;
	/** in-progress record request */
	mrcp_message_t        *record;
	/** properties used in set/get params */
	mrcp_message_header_t *properties;
};

typedef apt_bool_t (*recorder_method_f)(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message);

static APR_INLINE apt_bool_t recorder_request_dispatch(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	state_machine->active_request = message;
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE apt_bool_t recorder_response_dispatch(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	state_machine->active_request = NULL;
	if(state_machine->base.active == FALSE) {
		/* this is the response to deactivation (STOP) request */
		return state_machine->base.on_deactivate(&state_machine->base);
	}
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE apt_bool_t recorder_event_dispatch(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->base.active == FALSE) {
		/* do nothing, state machine has already been deactivated */
		return FALSE;
	}
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE void recorder_state_change(mrcp_recorder_state_machine_t *state_machine, mrcp_recorder_state_e state, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"State Transition %s -> %s " APT_SIDRES_FMT,
		state_names[state_machine->state],
		state_names[state],
		MRCP_MESSAGE_SIDRES(message));
	state_machine->state = state;
	if(state == RECORDER_STATE_IDLE) {
		state_machine->record = NULL;
	}
}


static apt_bool_t recorder_request_set_params(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_header_fields_set(state_machine->properties,&message->header,message->pool);
	return recorder_request_dispatch(state_machine,message);
}

static apt_bool_t recorder_response_set_params(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	return recorder_response_dispatch(state_machine,message);
}

static apt_bool_t recorder_request_get_params(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	return recorder_request_dispatch(state_machine,message);
}

static apt_bool_t recorder_response_get_params(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_header_fields_get(&message->header,state_machine->properties,&state_machine->active_request->header,message->pool);
	return recorder_response_dispatch(state_machine,message);
}

static apt_bool_t recorder_request_record(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_header_fields_inherit(&message->header,state_machine->properties,message->pool);
	if(state_machine->state == RECORDER_STATE_RECORDING) {
		mrcp_message_t *response;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Reject RECORD Request " APT_SIDRES_FMT " [%" MRCP_REQUEST_ID_FMT "]",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		
		/* there is in-progress request, reject this one */
		response = mrcp_response_create(message,message->pool);
		response->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return recorder_response_dispatch(state_machine,response);
	}

	return recorder_request_dispatch(state_machine,message);
}

static apt_bool_t recorder_response_record(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(message->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS) {
		state_machine->record = state_machine->active_request;
		recorder_state_change(state_machine,RECORDER_STATE_RECORDING,message);
	}
	return recorder_response_dispatch(state_machine,message);
}

static apt_bool_t recorder_request_stop(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *response;
	if(state_machine->state == RECORDER_STATE_RECORDING) {
		/* found in-progress RECORDER request, stop it */
		return recorder_request_dispatch(state_machine,message);
	}

	/* found no in-progress RECORDER request, sending immediate response */
	response = mrcp_response_create(message,message->pool);
	return recorder_response_dispatch(state_machine,response);
}

static apt_bool_t recorder_response_stop(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
	/* append active id list */
	active_request_id_list_append(generic_header,state_machine->record->start_line.request_id);
	mrcp_generic_header_property_add(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
	recorder_state_change(state_machine,RECORDER_STATE_IDLE,message);
	return recorder_response_dispatch(state_machine,message);
}

static apt_bool_t recorder_request_start_timers(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *response;
	if(state_machine->state == RECORDER_STATE_RECORDING) {
		/* found in-progress request */
		return recorder_request_dispatch(state_machine,message);
	}

	/* found no in-progress request */
	response = mrcp_response_create(message,message->pool);
	response->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
	return recorder_response_dispatch(state_machine,response);
}

static apt_bool_t recorder_response_start_timers(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	return recorder_response_dispatch(state_machine,message);
}

static apt_bool_t recorder_event_start_of_input(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->record) {
		/* unexpected event, no in-progress record request */
		return FALSE;
	}

	if(state_machine->record->start_line.request_id != message->start_line.request_id) {
		/* unexpected event */
		return FALSE;
	}
	
	message->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	return recorder_event_dispatch(state_machine,message);
}

static apt_bool_t recorder_event_record_complete(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->record) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Unexpected RECORD-COMPLETE Event " APT_SIDRES_FMT " [%" MRCP_REQUEST_ID_FMT "]",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		return FALSE;
	}

	if(state_machine->record->start_line.request_id != message->start_line.request_id) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Unexpected RECORD-COMPLETE Event " APT_SIDRES_FMT " [%" MRCP_REQUEST_ID_FMT "]",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		return FALSE;
	}

	if(state_machine->active_request && state_machine->active_request->start_line.method_id == RECORDER_STOP) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Ignore RECORD-COMPLETE Event " APT_SIDRES_FMT " [%" MRCP_REQUEST_ID_FMT "]: waiting for STOP response",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		return FALSE;
	}

	if(mrcp_resource_header_property_check(message,RECORDER_HEADER_COMPLETION_CAUSE) != TRUE) {
		mrcp_recorder_header_t *recorder_header = mrcp_resource_header_prepare(message);
		recorder_header->completion_cause = RECORDER_COMPLETION_CAUSE_SUCCESS_SILENCE;
		mrcp_resource_header_property_add(message,RECORDER_HEADER_COMPLETION_CAUSE);
	}
	recorder_state_change(state_machine,RECORDER_STATE_IDLE,message);
	return recorder_event_dispatch(state_machine,message);
}

static recorder_method_f recorder_request_method_array[RECORDER_METHOD_COUNT] = {
	recorder_request_set_params,
	recorder_request_get_params,
	recorder_request_record,
	recorder_request_stop,
	recorder_request_start_timers
};

static recorder_method_f recorder_response_method_array[RECORDER_METHOD_COUNT] = {
	recorder_response_set_params,
	recorder_response_get_params,
	recorder_response_record,
	recorder_response_stop,
	recorder_response_start_timers
};

static recorder_method_f recorder_event_method_array[RECORDER_EVENT_COUNT] = {
	recorder_event_start_of_input,
	recorder_event_record_complete
};

/** Update state according to received incoming request from MRCP client */
static apt_bool_t recorder_request_state_update(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	recorder_method_f method;
	if(message->start_line.method_id >= RECORDER_METHOD_COUNT) {
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process %s Request " APT_SIDRES_FMT " [%" MRCP_REQUEST_ID_FMT "]",
		message->start_line.method_name.buf,
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	method = recorder_request_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return recorder_request_dispatch(state_machine,message);
}

/** Update state according to received outgoing response from recorder engine */
static apt_bool_t recorder_response_state_update(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	recorder_method_f method;
	if(!state_machine->active_request) {
		/* unexpected response, no active request waiting for response */
		return FALSE;
	}
	if(state_machine->active_request->start_line.request_id != message->start_line.request_id) {
		/* unexpected response, request id doesn't match */
		return FALSE;
	}

	if(message->start_line.method_id >= RECORDER_METHOD_COUNT) {
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process %s Response " APT_SIDRES_FMT " [%" MRCP_REQUEST_ID_FMT "]",
		message->start_line.method_name.buf,
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	method = recorder_response_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return recorder_response_dispatch(state_machine,message);
}

/** Update state according to received outgoing event from recorder engine */
static apt_bool_t recorder_event_state_update(mrcp_recorder_state_machine_t *state_machine, mrcp_message_t *message)
{
	recorder_method_f method;
	if(message->start_line.method_id >= RECORDER_EVENT_COUNT) {
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process %s Event " APT_SIDRES_FMT " [%" MRCP_REQUEST_ID_FMT "]",
		message->start_line.method_name.buf,
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	method = recorder_event_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return recorder_event_dispatch(state_machine,message);
}

/** Update state according to request received from MRCP client or response/event received from recorder engine */
static apt_bool_t recorder_state_update(mrcp_state_machine_t *base, mrcp_message_t *message)
{
	mrcp_recorder_state_machine_t *state_machine = (mrcp_recorder_state_machine_t*)base;
	apt_bool_t status = TRUE;
	switch(message->start_line.message_type) {
		case MRCP_MESSAGE_TYPE_REQUEST:
			status = recorder_request_state_update(state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_RESPONSE:
			status = recorder_response_state_update(state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_EVENT:
			status = recorder_event_state_update(state_machine,message);
			break;
		default:
			status = FALSE;
			break;
	}
	return status;
}

/** Deactivate state machine */
static apt_bool_t recorder_state_deactivate(mrcp_state_machine_t *base)
{
	mrcp_recorder_state_machine_t *state_machine = (mrcp_recorder_state_machine_t*)base;
	mrcp_message_t *message;
	mrcp_message_t *source;
	if(state_machine->state != RECORDER_STATE_RECORDING) {
		/* no in-progress RECORD request to deactivate */
		return FALSE;
	}
	source = state_machine->record;
	if(!source) {
		return FALSE;
	}

	/* create internal STOP request */
	message = mrcp_request_create(
						source->resource,
						source->start_line.version,
						RECORDER_STOP,
						source->pool);
	message->channel_id = source->channel_id;
	message->start_line.request_id = source->start_line.request_id + 1;
	apt_string_set(&message->start_line.method_name,"DEACTIVATE"); /* informative only */
	message->header = source->header;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create and Process STOP Request " APT_SIDRES_FMT " [%" MRCP_REQUEST_ID_FMT "]",
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	return recorder_request_dispatch(state_machine,message);
}

/** Create MRCP recorder state machine */
mrcp_state_machine_t* mrcp_recorder_state_machine_create(void *obj, mrcp_version_e version, apr_pool_t *pool)
{
	mrcp_recorder_state_machine_t *state_machine = apr_palloc(pool,sizeof(mrcp_recorder_state_machine_t));
	mrcp_state_machine_init(&state_machine->base,obj);
	state_machine->base.update = recorder_state_update;
	state_machine->base.deactivate = recorder_state_deactivate;
	state_machine->state = RECORDER_STATE_IDLE;
	state_machine->active_request = NULL;
	state_machine->record = NULL;
	state_machine->properties = mrcp_message_header_create(
			mrcp_generic_header_vtable_get(version),
			mrcp_recorder_header_vtable_get(version),
			pool);
	return &state_machine->base;
}
