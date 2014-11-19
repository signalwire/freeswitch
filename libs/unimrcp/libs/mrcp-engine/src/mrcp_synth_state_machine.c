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
 * $Id: mrcp_synth_state_machine.c 2228 2014-11-12 01:18:27Z achaloyan@gmail.com $
 */

#include "apt_obj_list.h"
#include "apt_log.h"
#include "mrcp_state_machine.h"
#include "mrcp_synth_state_machine.h"
#include "mrcp_synth_header.h"
#include "mrcp_generic_header.h"
#include "mrcp_synth_resource.h"
#include "mrcp_message.h"

/** MRCP synthesizer states */
typedef enum {
	SYNTHESIZER_STATE_IDLE,
	SYNTHESIZER_STATE_SPEAKING,
	SYNTHESIZER_STATE_PAUSED,

	SYNTHESIZER_STATE_COUNT
} mrcp_synth_state_e;

static const char * state_names[SYNTHESIZER_STATE_COUNT] = {
	"IDLE",
	"SPEAKING",
	"PAUSED"
};

typedef struct mrcp_synth_state_machine_t mrcp_synth_state_machine_t;
struct mrcp_synth_state_machine_t {
	/** state machine base */
	mrcp_state_machine_t   base;
	/** synthesizer state */
	mrcp_synth_state_e     state;
	/** indicate whether active_request was processed from pending request queue */
	apt_bool_t             is_pending;
	/** request sent to synthesizer engine and waiting for the response to be received */
	mrcp_message_t        *active_request;
	/** in-progress speak request */
	mrcp_message_t        *speaker;
	/** queue of pending speak requests */
	apt_obj_list_t        *queue;
	/** properties used in set/get params */
	mrcp_message_header_t *properties;
};

typedef apt_bool_t (*synth_method_f)(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message);

static APR_INLINE apt_bool_t synth_request_dispatch(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	state_machine->active_request = message;
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE apt_bool_t synth_response_dispatch(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	state_machine->active_request = NULL;
	if(state_machine->base.active == FALSE) {
		/* this is the response to deactivation (STOP) request */
		return state_machine->base.on_deactivate(&state_machine->base);
	}
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE apt_bool_t synth_event_dispatch(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->base.active == FALSE) {
		/* do nothing, state machine has already been deactivated */
		return FALSE;
	}
	return state_machine->base.on_dispatch(&state_machine->base,message);
}

static APR_INLINE void synth_state_change(mrcp_synth_state_machine_t *state_machine, mrcp_synth_state_e state, mrcp_message_t *message)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"State Transition %s -> %s "APT_SIDRES_FMT,
		state_names[state_machine->state],
		state_names[state],
		MRCP_MESSAGE_SIDRES(message));
	state_machine->state = state;
	if(state == SYNTHESIZER_STATE_IDLE) {
		state_machine->speaker = NULL;
	}
}


static apt_bool_t synth_request_set_params(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_header_fields_set(state_machine->properties,&message->header,message->pool);
	return synth_request_dispatch(state_machine,message);
}

static apt_bool_t synth_response_set_params(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return synth_response_dispatch(state_machine,message);
}

static apt_bool_t synth_request_get_params(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return synth_request_dispatch(state_machine,message);
}

static apt_bool_t synth_response_get_params(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_header_fields_get(&message->header,state_machine->properties,&state_machine->active_request->header,message->pool);
	return synth_response_dispatch(state_machine,message);
}

static apt_bool_t synth_request_speak(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_header_fields_inherit(&message->header,state_machine->properties,message->pool);
	if(state_machine->speaker) {
		mrcp_message_t *response;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Queue Up SPEAK Request "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		message->start_line.request_state = MRCP_REQUEST_STATE_PENDING;
		apt_list_push_back(state_machine->queue,message,message->pool);
		
		response = mrcp_response_create(message,message->pool);
		response->start_line.request_state = MRCP_REQUEST_STATE_PENDING;
		return synth_response_dispatch(state_machine,response);
	}

	return synth_request_dispatch(state_machine,message);
}

static apt_bool_t synth_response_speak(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(message->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS) {
		state_machine->speaker = state_machine->active_request;
		synth_state_change(state_machine,SYNTHESIZER_STATE_SPEAKING,message);
	}
	if(state_machine->is_pending == TRUE) {
		mrcp_message_t *event_message = mrcp_event_create(
							state_machine->active_request,
							SYNTHESIZER_SPEECH_MARKER,
							state_machine->active_request->pool);
		event_message->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
		state_machine->is_pending = FALSE;
		/* not to send the response for pending request, instead send SPEECH-MARKER event */
		return synth_event_dispatch(state_machine,event_message);
	}
	return synth_response_dispatch(state_machine,message);
}

static apt_bool_t synth_pending_requests_remove(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *request_message, mrcp_message_t *response_message)
{
	apt_list_elem_t *elem;
	mrcp_message_t *pending_message;
	mrcp_request_id_list_t *request_id_list = NULL;
	mrcp_generic_header_t *generic_header = mrcp_generic_header_get(request_message);
	mrcp_generic_header_t *response_generic_header = mrcp_generic_header_prepare(response_message);
	if(generic_header && mrcp_generic_header_property_check(request_message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST) == TRUE) {
		if(generic_header->active_request_id_list.count) {
			/* selective STOP request */
			request_id_list = &generic_header->active_request_id_list;
		}
	}

	elem = apt_list_first_elem_get(state_machine->queue);
	while(elem) {
		pending_message = apt_list_elem_object_get(elem);
		if(!request_id_list || active_request_id_list_find(generic_header,pending_message->start_line.request_id) == TRUE) {
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove Pending SPEAK Request "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
				MRCP_MESSAGE_SIDRES(pending_message),
				pending_message->start_line.request_id);
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

static apt_bool_t synth_request_stop(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *response_message;
	if(state_machine->speaker) {
		mrcp_request_id_list_t *request_id_list = NULL;
		mrcp_generic_header_t *generic_header = mrcp_generic_header_get(message);
		if(generic_header && mrcp_generic_header_property_check(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST) == TRUE) {
			if(generic_header->active_request_id_list.count) {
				/* selective STOP request */
				request_id_list = &generic_header->active_request_id_list;
			}
		}

		if(!request_id_list || active_request_id_list_find(generic_header,state_machine->speaker->start_line.request_id) == TRUE) {
			/* found in-progress SPEAK request, stop it */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Found IN-PROGRESS SPEAK Request "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
				MRCP_MESSAGE_SIDRES(message),
				message->start_line.request_id);
			return synth_request_dispatch(state_machine,message);
		}
	}

	/* found no in-progress SPEAK request, sending immediate response */
	response_message = mrcp_response_create(message,message->pool);
	synth_pending_requests_remove(state_machine,message,response_message);
	return synth_response_dispatch(state_machine,response_message);
}

static apt_bool_t synth_response_stop(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *pending_request;
	mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
	/* append active id list */
	active_request_id_list_append(generic_header,state_machine->speaker->start_line.request_id);
	mrcp_generic_header_property_add(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
	synth_pending_requests_remove(state_machine,state_machine->active_request,message);
	synth_state_change(state_machine,SYNTHESIZER_STATE_IDLE,message);
	pending_request = apt_list_pop_front(state_machine->queue);
	synth_response_dispatch(state_machine,message);

	/* process pending SPEAK requests / if any */
	if(pending_request) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process Pending SPEAK Request "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
			MRCP_MESSAGE_SIDRES(message),
			pending_request->start_line.request_id);
		state_machine->is_pending = TRUE;
		synth_request_dispatch(state_machine,pending_request);
	}
	return TRUE;
}

static apt_bool_t synth_request_pause(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->speaker) {
		/* speaking or paused state */
		if(state_machine->state == SYNTHESIZER_STATE_SPEAKING) {
			synth_request_dispatch(state_machine,message);
		}
		else {
			/* paused state */
			mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
			synth_response_dispatch(state_machine,response_message);
		}
	}
	else {
		/* idle state */
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		synth_response_dispatch(state_machine,response_message);
	}
	return TRUE;
}

static apt_bool_t synth_response_pause(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(message->start_line.status_code == MRCP_STATUS_CODE_SUCCESS) {
		mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
		/* append active id list */
		active_request_id_list_append(generic_header,state_machine->speaker->start_line.request_id);
		mrcp_generic_header_property_add(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
		synth_state_change(state_machine,SYNTHESIZER_STATE_PAUSED,message);
	}
	synth_response_dispatch(state_machine,message);
	return TRUE;
}

static apt_bool_t synth_request_resume(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->speaker) {
		/* speaking or paused state */
		if(state_machine->state == SYNTHESIZER_STATE_PAUSED) {
			synth_request_dispatch(state_machine,message);
		}
		else {
			/* speaking state */
			mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
			synth_response_dispatch(state_machine,response_message);
		}
	}
	else {
		/* idle state */
		mrcp_message_t *response_message = mrcp_response_create(message,message->pool);
		response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
		synth_response_dispatch(state_machine,response_message);
	}
	return TRUE;
}

static apt_bool_t synth_response_resume(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(message->start_line.status_code == MRCP_STATUS_CODE_SUCCESS) {
		mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
		/* append active id list */
		active_request_id_list_append(generic_header,state_machine->speaker->start_line.request_id);
		mrcp_generic_header_property_add(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
		synth_state_change(state_machine,SYNTHESIZER_STATE_SPEAKING,message);
	}
	synth_response_dispatch(state_machine,message);
	return TRUE;
}

static apt_bool_t synth_request_barge_in_occurred(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *response_message;
	if(state_machine->speaker) {
		apt_bool_t kill_on_barge_in = TRUE;
		mrcp_synth_header_t *synth_header = mrcp_resource_header_get(message);
		if(synth_header) {
			if(mrcp_resource_header_property_check(message,SYNTHESIZER_HEADER_KILL_ON_BARGE_IN) == TRUE) {
				kill_on_barge_in = synth_header->kill_on_barge_in;
			}
		}
	
		if(kill_on_barge_in == TRUE) {
			return synth_request_dispatch(state_machine,message);
		}
	}

	/* found no kill-on-bargein enabled in-progress SPEAK request, sending immediate response */
	response_message = mrcp_response_create(message,message->pool);
	return synth_response_dispatch(state_machine,response_message);
}

static apt_bool_t synth_response_barge_in_occurred(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
	/* append active id list */
	active_request_id_list_append(generic_header,state_machine->speaker->start_line.request_id);
	mrcp_generic_header_property_add(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
	synth_pending_requests_remove(state_machine,state_machine->active_request,message);
	synth_state_change(state_machine,SYNTHESIZER_STATE_IDLE,message);
	return synth_response_dispatch(state_machine,message);
}

static apt_bool_t synth_request_control(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *response_message;
	if(state_machine->state == SYNTHESIZER_STATE_SPEAKING) {
		return synth_request_dispatch(state_machine,message);
	}

	/* found no in-progress SPEAK request, sending immediate response */
	response_message = mrcp_response_create(message,message->pool);
	return synth_response_dispatch(state_machine,response_message);
}

static apt_bool_t synth_response_control(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
	/* append active id list */
	active_request_id_list_append(generic_header,state_machine->speaker->start_line.request_id);
	mrcp_generic_header_property_add(message,GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST);
	return synth_response_dispatch(state_machine,message);
}

static apt_bool_t synth_request_define_lexicon(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *response_message;
	if(state_machine->state == SYNTHESIZER_STATE_IDLE) {
		return synth_request_dispatch(state_machine,message);
	}

	/* sending failure response */
	response_message = mrcp_response_create(message,message->pool);
	response_message->start_line.status_code = MRCP_STATUS_CODE_METHOD_NOT_VALID;
	return synth_response_dispatch(state_machine,response_message);
}

static apt_bool_t synth_response_define_lexicon(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return synth_response_dispatch(state_machine,message);
}

static apt_bool_t synth_event_speech_marker(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->speaker) {
		/* unexpected event, no in-progress speak request */
		return FALSE;
	}

	if(state_machine->speaker->start_line.request_id != message->start_line.request_id) {
		/* unexpected event */
		return FALSE;
	}
	
	message->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	return synth_event_dispatch(state_machine,message);
}

static apt_bool_t synth_event_speak_complete(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_message_t *pending_request;
	if(!state_machine->speaker) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Unexpected SPEAK-COMPLETE Event "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		return FALSE;
	}

	if(state_machine->speaker->start_line.request_id != message->start_line.request_id) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Unexpected SPEAK-COMPLETE Event "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		return FALSE;
	}

	if(state_machine->active_request && state_machine->active_request->start_line.method_id == SYNTHESIZER_STOP) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Ignore SPEAK-COMPLETE Event "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]: waiting for STOP response",
			MRCP_MESSAGE_SIDRES(message),
			message->start_line.request_id);
		return FALSE;
	}

	if(mrcp_resource_header_property_check(message,SYNTHESIZER_HEADER_COMPLETION_CAUSE) != TRUE) {
		mrcp_synth_header_t *synth_header = mrcp_resource_header_prepare(message);
		synth_header->completion_cause = SYNTHESIZER_COMPLETION_CAUSE_NORMAL;
		mrcp_resource_header_property_add(message,SYNTHESIZER_HEADER_COMPLETION_CAUSE);
	}
	synth_state_change(state_machine,SYNTHESIZER_STATE_IDLE,message);
	synth_event_dispatch(state_machine,message);

	/* process pending SPEAK requests */
	pending_request = apt_list_pop_front(state_machine->queue);
	if(pending_request) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process Pending SPEAK Request "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
			MRCP_MESSAGE_SIDRES(pending_request),
			pending_request->start_line.request_id);
		state_machine->is_pending = TRUE;
		synth_request_dispatch(state_machine,pending_request);
	}
	return TRUE;
}

static synth_method_f synth_request_method_array[SYNTHESIZER_METHOD_COUNT] = {
	synth_request_set_params,
	synth_request_get_params,
	synth_request_speak,
	synth_request_stop,
	synth_request_pause,
	synth_request_resume,
	synth_request_barge_in_occurred,
	synth_request_control,
	synth_request_define_lexicon
};

static synth_method_f synth_response_method_array[SYNTHESIZER_METHOD_COUNT] = {
	synth_response_set_params,
	synth_response_get_params,
	synth_response_speak,
	synth_response_stop,
	synth_response_pause,
	synth_response_resume,
	synth_response_barge_in_occurred,
	synth_response_control,
	synth_response_define_lexicon,
};

static synth_method_f synth_event_method_array[SYNTHESIZER_EVENT_COUNT] = {
	synth_event_speech_marker,
	synth_event_speak_complete
};

/** Update state according to received incoming request from MRCP client */
static apt_bool_t synth_request_state_update(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	synth_method_f method;
	if(message->start_line.method_id >= SYNTHESIZER_METHOD_COUNT) {
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process %s Request "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.method_name.buf,
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	method = synth_request_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return synth_request_dispatch(state_machine,message);
}

/** Update state according to received outgoing response from synthesizer engine */
static apt_bool_t synth_response_state_update(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	synth_method_f method;
	if(!state_machine->active_request) {
		/* unexpected response, no active request waiting for response */
		return FALSE;
	}
	if(state_machine->active_request->start_line.request_id != message->start_line.request_id) {
		/* unexpected response, request id doesn't match */
		return FALSE;
	}

	if(message->start_line.method_id >= SYNTHESIZER_METHOD_COUNT) {
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process %s Response "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.method_name.buf,
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	method = synth_response_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return synth_response_dispatch(state_machine,message);
}

/** Update state according to received outgoing event from synthesizer engine */
static apt_bool_t synth_event_state_update(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	synth_method_f method;
	if(message->start_line.method_id >= SYNTHESIZER_EVENT_COUNT) {
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Process %s Event "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
		message->start_line.method_name.buf,
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	method = synth_event_method_array[message->start_line.method_id];
	if(method) {
		return method(state_machine,message);
	}
	return synth_event_dispatch(state_machine,message);
}

/** Update state according to request received from MRCP client or response/event received from synthesizer engine */
static apt_bool_t synth_state_update(mrcp_state_machine_t *base, mrcp_message_t *message)
{
	mrcp_synth_state_machine_t *synth_state_machine = (mrcp_synth_state_machine_t*)base;
	apt_bool_t status = TRUE;
	switch(message->start_line.message_type) {
		case MRCP_MESSAGE_TYPE_REQUEST:
			status = synth_request_state_update(synth_state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_RESPONSE:
			status = synth_response_state_update(synth_state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_EVENT:
			status = synth_event_state_update(synth_state_machine,message);
			break;
		default:
			status = FALSE;
			break;
	}
	return status;
}

/** Deactivate state machine */
static apt_bool_t synth_state_deactivate(mrcp_state_machine_t *base)
{
	mrcp_synth_state_machine_t *state_machine = (mrcp_synth_state_machine_t*)base;
	mrcp_message_t *message;
	mrcp_message_t *source;
	if(!state_machine->speaker) {
		/* no in-progress SPEAK request to deactivate */
		return FALSE;
	}
	source = state_machine->speaker;

	/* create internal STOP request */
	message = mrcp_request_create(
						source->resource,
						source->start_line.version,
						SYNTHESIZER_STOP,
						source->pool);
	message->channel_id = source->channel_id;
	message->start_line.request_id = source->start_line.request_id + 1;
	apt_string_set(&message->start_line.method_name,"DEACTIVATE"); /* informative only */
	message->header = source->header;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create and Process STOP Request "APT_SIDRES_FMT" [%"MRCP_REQUEST_ID_FMT"]",
		MRCP_MESSAGE_SIDRES(message),
		message->start_line.request_id);
	return synth_request_dispatch(state_machine,message);
}

/** Create MRCP synthesizer state machine */
mrcp_state_machine_t* mrcp_synth_state_machine_create(void *obj, mrcp_version_e version, apr_pool_t *pool)
{
	mrcp_synth_state_machine_t *state_machine = apr_palloc(pool,sizeof(mrcp_synth_state_machine_t));
	mrcp_state_machine_init(&state_machine->base,obj);
	state_machine->base.update = synth_state_update;
	state_machine->base.deactivate = synth_state_deactivate;
	state_machine->state = SYNTHESIZER_STATE_IDLE;
	state_machine->is_pending = FALSE;
	state_machine->active_request = NULL;
	state_machine->speaker = NULL;
	state_machine->queue = apt_list_create(pool);
	state_machine->properties = mrcp_message_header_create(
			mrcp_generic_header_vtable_get(version),
			mrcp_synth_header_vtable_get(version),
			pool);
	return &state_machine->base;
}
