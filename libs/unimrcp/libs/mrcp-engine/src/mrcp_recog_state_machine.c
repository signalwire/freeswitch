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

#include "apt_obj_list.h"
#include "apt_log.h"
#include "mrcp_state_machine.h"
#include "mrcp_recog_state_machine.h"
#include "mrcp_recog_header.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_resource.h"
#include "mrcp_message.h"

/** MRCP recognizer states */
typedef enum {
	RECOGNIZER_STATE_IDLE,
	RECOGNIZER_STATE_RECOGNIZING,
	RECOGNIZER_STATE_RECOGNIZED,

	RECOGNIZER_STATE_COUNT
} mrcp_recog_state_e;

static const char * state_names[RECOGNIZER_STATE_COUNT] = {
	"IDLE",
	"RECOGNIZING",
	"RECOGNIZED"
};

typedef struct mrcp_recog_state_machine_t mrcp_recog_state_machine_t;
struct mrcp_recog_state_machine_t {
	/** state machine base */
	mrcp_state_machine_t  base;
	/** recognizer state */
	mrcp_recog_state_e    state;
	/** indicate whether active_request was processed from pending request queue */
	apt_bool_t            is_pending;
	/** request sent to recognition engine and waiting for the response to be received */
	mrcp_message_t       *active_request;
	/** in-progress recognize request */
	mrcp_message_t       *recog;
	/** queue of pending recognition requests */
	apt_obj_list_t       *queue;
	/** properties used in set/get params */
	mrcp_message_header_t properties;
};

typedef apt_bool_t (*recog_method_f)(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message);

static APR_INLINE apt_bool_t recog_request_dispatch(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	state_machine->active_request = message;
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE apt_bool_t recog_response_dispatch(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	state_machine->active_request = NULL;
	if(state_machine->base.active == FALSE) {
		/* this is the response to deactivation (STOP) request */
		return state_machine->base.on_deactivate(&state_machine->base);
	}
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE apt_bool_t recog_event_dispatch(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->base.active == FALSE) {
		/* do nothing, state machine has already been deactivated */
		return FALSE;
	}
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE void recog_state_change(mrcp_recog_state_machine_t *state_machine, mrcp_recog_state_e state)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"State Transition %s -> %s",state_names[state_machine->state],state_names[state]);
	state_machine->state = state;
	if(state == RECOGNIZER_STATE_IDLE) {
		state_machine->recog = NULL;
	}
}


static apt_bool_t recog_request_set_params(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process SET-PARAMS Request [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	mrcp_message_header_set(&state_machine->properties,&message->header,message->pool);
	return recog_request_dispatch(state_machine,message);
}

static apt_bool_t recog_response_set_params(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process SET-PARAMS Response [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	return recog_response_dispatch(state_machine,message);
}

static apt_bool_t recog_request_get_params(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process GET-PARAMS Request [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	return recog_request_dispatch(state_machine,message);
}

static apt_bool_t recog_response_get_params(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process GET-PARAMS Response [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	mrcp_message_header_set(&message->header,&state_machine->active_request->header,message->pool);
	mrcp_message_header_get(&message->header,&state_machine->properties,message->pool);
	return recog_response_dispatch(state_machine,message);
}

static apt_bool_t recog_request_define_grammar(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->state == RECOGNIZER_STATE_RECOGNIZING) {
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		return recog_response_dispatch(state_machine,response_message);
	}
	else if(state_machine->state == RECOGNIZER_STATE_RECOGNIZED) {
		recog_state_change(state_machine,RECOGNIZER_STATE_IDLE);
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process DEFINE-GRAMMAR Request [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	return recog_request_dispatch(state_machine,message);
}

static apt_bool_t recog_response_define_grammar(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process DEFINE-GRAMMAR Response [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	if(mrcp_resource_header_property_check(message,RECOGNIZER_HEADER_COMPLETION_CAUSE) != TRUE) {
		mrcp_recog_header_t *recog_header = mrcp_resource_header_prepare(message);
		recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_SUCCESS;
		mrcp_resource_header_property_add(message,RECOGNIZER_HEADER_COMPLETION_CAUSE);
	}
	return recog_response_dispatch(state_machine,message);
}

static apt_bool_t recog_request_recognize(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_header_inherit(&message->header,&state_machine->properties,message->pool);
	if(state_machine->state == RECOGNIZER_STATE_RECOGNIZING) {
		mrcp_message_t *response;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Queue Up RECOGNIZE Request [%"MRCP_REQUEST_ID_FMT"]",
			message->start_line.request_id);
		message->start_line.request_state = MRCP_REQUEST_STATE_PENDING;
		apt_list_push_back(state_machine->queue,message,message->pool);
		
		response = mrcp_response_create(message,message->pool);
		response->start_line.request_state = MRCP_REQUEST_STATE_PENDING;
		return recog_response_dispatch(state_machine,response);
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process RECOGNIZE Request [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	return recog_request_dispatch(state_machine,message);
}

static apt_bool_t recog_response_recognize(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process RECOGNIZE Response [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	if(message->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS) {
		state_machine->recog = state_machine->active_request;
		recog_state_change(state_machine,RECOGNIZER_STATE_RECOGNIZING);
	}
	if(state_machine->is_pending == TRUE) {
		state_machine->is_pending = FALSE;
		/* not to send the response for pending request */
		return TRUE;
	}
	return recog_response_dispatch(state_machine,message);
}

static apt_bool_t recog_request_get_result(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *response_message;
	if(state_machine->state == RECOGNIZER_STATE_RECOGNIZED) {
		/* found recognized request */
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process GET-RESULT Request [%"MRCP_REQUEST_ID_FMT"]",
			message->start_line.request_id);
		return recog_request_dispatch(state_machine,message);
	}

	/* found no recognized request */
	response_message = mrcp_response_create(message,message->pool);
	response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
	return recog_response_dispatch(state_machine,response_message);
}

static apt_bool_t recog_response_get_result(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process GET-RESULT Response [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	return recog_response_dispatch(state_machine,message);
}

static apt_bool_t recog_request_recognition_start_timers(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *response_message;
	if(state_machine->state == RECOGNIZER_STATE_RECOGNIZING) {
		/* found in-progress request */
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process START-INPUT-TIMERS Request [%"MRCP_REQUEST_ID_FMT"]",
			message->start_line.request_id);
		return recog_request_dispatch(state_machine,message);
	}

	/* found no in-progress request */
	response_message = mrcp_response_create(message,message->pool);
	response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
	return recog_response_dispatch(state_machine,response_message);
}

static apt_bool_t recog_response_recognition_start_timers(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process START-INPUT-TIMERS Response [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	return recog_response_dispatch(state_machine,message);
}

static apt_bool_t recog_pending_requests_remove(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *request_message, mrcp_message_t *response_message)
{
	apt_list_elem_t *elem;
	mrcp_message_t *pending_message;
	mrcp_request_id_list_t *request_id_list = NULL;
	mrcp_generic_header_t *generic_header = mrcp_generic_header_get(request_message);
	mrcp_generic_header_t *response_generic_header = mrcp_generic_header_prepare(response_message);
	if(generic_header && mrcp_generic_header_property_check(request_message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST) == TRUE) {
		if(generic_header->active_request_id_list.ids && generic_header->active_request_id_list.count) {
			/* selective STOP request */
			request_id_list = &generic_header->active_request_id_list;
		}
	}

	elem = apt_list_first_elem_get(state_machine->queue);
	while(elem) {
		pending_message = apt_list_elem_object_get(elem);
		if(!request_id_list || active_request_id_list_find(generic_header,pending_message->start_line.request_id) == TRUE) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove Pending RECOGNIZE Request [%d]",pending_message->start_line.request_id);
			elem = apt_list_elem_remove(state_machine->queue,elem);
			/* append active id list */
			active_request_id_list_append(response_generic_header,pending_message->start_line.request_id);
		}
		else {
			/* speak request remains in the queue, just proceed to the next one */
			elem = apt_list_next_elem_get(state_machine->queue,elem);
		}
	}
	if(response_generic_header->active_request_id_list.count) {
		mrcp_generic_header_property_add(response_message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
	}
	return TRUE;
}

static apt_bool_t recog_request_stop(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *response_message;
	if(state_machine->state == RECOGNIZER_STATE_RECOGNIZING) {
		mrcp_request_id_list_t *request_id_list = NULL;
		mrcp_generic_header_t *generic_header = mrcp_generic_header_get(message);
		if(generic_header && mrcp_generic_header_property_check(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST) == TRUE) {
			if(generic_header->active_request_id_list.ids && generic_header->active_request_id_list.count) {
				/* selective STOP request */
				request_id_list = &generic_header->active_request_id_list;
			}
		}

		if(!request_id_list || active_request_id_list_find(generic_header,state_machine->recog->start_line.request_id) == TRUE) {
			/* found in-progress RECOGNIZE request, stop it */
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process STOP Request [%"MRCP_REQUEST_ID_FMT"]",
				message->start_line.request_id);
			return recog_request_dispatch(state_machine,message);
		}
	}
	else if(state_machine->state == RECOGNIZER_STATE_RECOGNIZED) {
		recog_state_change(state_machine,RECOGNIZER_STATE_IDLE);
	}

	/* found no in-progress RECOGNIZE request, sending immediate response */
	response_message = mrcp_response_create(message,message->pool);
	recog_pending_requests_remove(state_machine,message,response_message);
	return recog_response_dispatch(state_machine,response_message);
}

static apt_bool_t recog_response_stop(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *pending_request;
	mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process STOP Response [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	/* append active id list */
	active_request_id_list_append(generic_header,state_machine->recog->start_line.request_id);
	mrcp_generic_header_property_add(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
	recog_pending_requests_remove(state_machine,state_machine->active_request,message);
	recog_state_change(state_machine,RECOGNIZER_STATE_IDLE);
	pending_request = apt_list_pop_front(state_machine->queue);
	recog_response_dispatch(state_machine,message);

	/* process pending RECOGNIZE requests / if any */
	if(pending_request) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process Pending RECOGNIZE Request [%d]",pending_request->start_line.request_id);
		state_machine->is_pending = TRUE;
		recog_request_dispatch(state_machine,pending_request);
	}
	return TRUE;
}

static apt_bool_t recog_event_start_of_input(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->recog) {
		/* unexpected event, no in-progress recognition request */
		return FALSE;
	}

	if(state_machine->recog->start_line.request_id != message->start_line.request_id) {
		/* unexpected event */
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process START-OF-INPUT Event [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	message->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	return recog_event_dispatch(state_machine,message);
}

static apt_bool_t recog_event_recognition_complete(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *pending_request;
	if(!state_machine->recog) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Unexpected RECOGNITION-COMPLETE Event [%"MRCP_REQUEST_ID_FMT"]",
			message->start_line.request_id);
		return FALSE;
	}

	if(state_machine->recog->start_line.request_id != message->start_line.request_id) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Unexpected RECOGNITION-COMPLETE Event [%"MRCP_REQUEST_ID_FMT"]",
			message->start_line.request_id);
		return FALSE;
	}

	if(state_machine->active_request && state_machine->active_request->start_line.method_id == RECOGNIZER_STOP) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Ignore RECOGNITION-COMPLETE Event [%d]: waiting for STOP response",message->start_line.request_id);
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process RECOGNITION-COMPLETE Event [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	if(mrcp_resource_header_property_check(message,RECOGNIZER_HEADER_COMPLETION_CAUSE) != TRUE) {
		mrcp_recog_header_t *recog_header = mrcp_resource_header_prepare(message);
		recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_SUCCESS;
		mrcp_resource_header_property_add(message,RECOGNIZER_HEADER_COMPLETION_CAUSE);
	}
	recog_state_change(state_machine,RECOGNIZER_STATE_RECOGNIZED);
	recog_event_dispatch(state_machine,message);

	/* process pending RECOGNIZE requests */
	pending_request = apt_list_pop_front(state_machine->queue);
	if(pending_request) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process Pending RECOGNIZE Request [%d]",pending_request->start_line.request_id);
		state_machine->is_pending = TRUE;
		recog_request_dispatch(state_machine,pending_request);
	}
	return TRUE;
}

static recog_method_f recog_request_method_array[RECOGNIZER_METHOD_COUNT] = {
	recog_request_set_params,
	recog_request_get_params,
	recog_request_define_grammar,
	recog_request_recognize,
	recog_request_get_result,
	recog_request_recognition_start_timers,
	recog_request_stop
};

static recog_method_f recog_response_method_array[RECOGNIZER_METHOD_COUNT] = {
	recog_response_set_params,
	recog_response_get_params,
	recog_response_define_grammar,
	recog_response_recognize,
	recog_response_get_result,
	recog_response_recognition_start_timers,
	recog_response_stop
};

static recog_method_f recog_event_method_array[RECOGNIZER_EVENT_COUNT] = {
	recog_event_start_of_input,
	recog_event_recognition_complete
};

/** Update state according to received incoming request from MRCP client */
static apt_bool_t recog_request_state_update(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	recog_method_f method;
	if(message->start_line.method_id >= RECOGNIZER_METHOD_COUNT) {
		return FALSE;
	}
	
	method = recog_request_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return recog_request_dispatch(state_machine,message);
}

/** Update state according to received outgoing response from recognition engine */
static apt_bool_t recog_response_state_update(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	recog_method_f method;
	if(!state_machine->active_request) {
		/* unexpected response, no active request waiting for response */
		return FALSE;
	}
	if(state_machine->active_request->start_line.request_id != message->start_line.request_id) {
		/* unexpected response, request id doesn't match */
		return FALSE;
	}

	if(message->start_line.method_id >= RECOGNIZER_METHOD_COUNT) {
		return FALSE;
	}
	
	method = recog_response_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return recog_response_dispatch(state_machine,message);
}

/** Update state according to received outgoing event from recognition engine */
static apt_bool_t recog_event_state_update(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	recog_method_f method;
	if(message->start_line.method_id >= RECOGNIZER_EVENT_COUNT) {
		return FALSE;
	}
	
	method = recog_event_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return recog_event_dispatch(state_machine,message);
}

/** Update state according to request received from MRCP client or response/event received from recognition engine */
static apt_bool_t recog_state_update(mrcp_state_machine_t *base, mrcp_message_t *message)
{
	mrcp_recog_state_machine_t *state_machine = (mrcp_recog_state_machine_t*)base;
	apt_bool_t status = TRUE;
	switch(message->start_line.message_type) {
		case MRCP_MESSAGE_TYPE_REQUEST:
			status = recog_request_state_update(state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_RESPONSE:
			status = recog_response_state_update(state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_EVENT:
			status = recog_event_state_update(state_machine,message);
			break;
		default:
			status = FALSE;
			break;
	}
	return status;
}

/** Deactivate state machine */
static apt_bool_t recog_state_deactivate(mrcp_state_machine_t *base)
{
	mrcp_recog_state_machine_t *state_machine = (mrcp_recog_state_machine_t*)base;
	mrcp_message_t *message;
	mrcp_message_t *source;
	if(state_machine->state != RECOGNIZER_STATE_RECOGNIZING) {
		/* no in-progress RECOGNIZE request to deactivate */
		return FALSE;
	}
	source = state_machine->recog;
	if(!source) {
		return FALSE;
	}

	/* create internal STOP request */
	message = mrcp_request_create(
						source->resource,
						source->start_line.version,
						RECOGNIZER_STOP,
						source->pool);
	message->channel_id = source->channel_id;
	message->start_line.request_id = source->start_line.request_id + 1;
	apt_string_set(&message->start_line.method_name,"DEACTIVATE"); /* informative only */
	message->header = source->header;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create and Process STOP Request [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.request_id);
	return recog_request_dispatch(state_machine,message);
}

/** Create MRCP recognizer state machine */
mrcp_state_machine_t* mrcp_recog_state_machine_create(void *obj, mrcp_version_e version, apr_pool_t *pool)
{
	mrcp_message_header_t *properties;
	mrcp_recog_state_machine_t *state_machine = apr_palloc(pool,sizeof(mrcp_recog_state_machine_t));
	mrcp_state_machine_init(&state_machine->base,obj);
	state_machine->base.update = recog_state_update;
	state_machine->base.deactivate = recog_state_deactivate;
	state_machine->state = RECOGNIZER_STATE_IDLE;
	state_machine->is_pending = FALSE;
	state_machine->active_request = NULL;
	state_machine->recog = NULL;
	state_machine->queue = apt_list_create(pool);
	properties = &state_machine->properties;
	mrcp_message_header_init(properties);
	properties->generic_header_accessor.vtable = mrcp_generic_header_vtable_get(version);
	properties->resource_header_accessor.vtable = mrcp_recog_header_vtable_get(version);
	return &state_machine->base;
}
