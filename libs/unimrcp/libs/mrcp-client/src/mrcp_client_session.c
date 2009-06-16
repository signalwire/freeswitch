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

#include "mrcp_client_session.h"
#include "mrcp_resource.h"
#include "mrcp_resource_factory.h"
#include "mrcp_sig_agent.h"
#include "mrcp_client_connection.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_control_descriptor.h"
#include "mrcp_message.h"
#include "mpf_termination.h"
#include "mpf_stream.h"
#include "mpf_engine.h"
#include "mpf_user.h"
#include "apt_consumer_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"


void mrcp_client_session_add(mrcp_client_t *client, mrcp_client_session_t *session);
void mrcp_client_session_remove(mrcp_client_t *client, mrcp_client_session_t *session);

static apt_bool_t mrcp_client_session_offer_send(mrcp_client_session_t *session);

static apt_bool_t mrcp_app_session_terminate_raise(mrcp_client_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t mrcp_app_sig_response_raise(mrcp_client_session_t *session, mrcp_sig_status_code_e status, apt_bool_t process_pending_requests);
static apt_bool_t mrcp_app_sig_event_raise(mrcp_client_session_t *session, mrcp_channel_t *channel);
static apt_bool_t mrcp_app_control_message_raise(mrcp_client_session_t *session, mrcp_channel_t *channel, mrcp_message_t *mrcp_message);
static apt_bool_t mrcp_app_request_dispatch(mrcp_client_session_t *session, const mrcp_app_message_t *app_message);

static apt_bool_t mrcp_client_resource_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_client_control_media_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_client_av_media_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor);

static mrcp_channel_t* mrcp_client_channel_find_by_name(mrcp_client_session_t *session, const apt_str_t *resource_name);

static apt_bool_t mrcp_client_mpf_request_send(
						mpf_engine_t *engine, 
						mpf_command_type_e command_id, 
						mpf_context_t *context, 
						mpf_termination_t *termination, 
						void *descriptor);

mrcp_client_session_t* mrcp_client_session_create(mrcp_application_t *application, void *obj)
{
	apr_pool_t *pool;
	mrcp_client_session_t *session = (mrcp_client_session_t*) mrcp_session_create(sizeof(mrcp_client_session_t)-sizeof(mrcp_session_t));
	pool = session->base.pool;
	session->application = application;
	session->codec_manager = NULL;
	session->app_obj = obj;
	session->profile = NULL;
	session->context = NULL;
	session->terminations = apr_array_make(pool,2,sizeof(rtp_termination_slot_t));
	session->channels = apr_array_make(pool,2,sizeof(mrcp_channel_t*));
	session->registered = FALSE;
	session->offer = NULL;
	session->answer = NULL;
	session->active_request = NULL;
	session->request_queue = apt_list_create(pool);
	session->offer_flag_count = 0;
	session->answer_flag_count = 0;
	session->terminate_flag_count = 0;
	return session;
}

mrcp_channel_t* mrcp_client_channel_create(
					mrcp_session_t *session, 
					mrcp_resource_id resource_id, 
					mpf_termination_t *termination, 
					mpf_rtp_termination_descriptor_t *rtp_descriptor, 
					void *obj)
{
	mrcp_channel_t *channel = apr_palloc(session->pool,sizeof(mrcp_channel_t));
	channel->pool = session->pool;
	channel->obj = obj;
	channel->session = session;
	channel->resource_id = resource_id;
	channel->resource_name = NULL;
	channel->control_channel = NULL;
	channel->termination = termination;
	channel->rtp_termination_slot = NULL;
	channel->resource = NULL;
	channel->waiting_for_channel = FALSE;
	channel->waiting_for_termination = FALSE;

	if(rtp_descriptor) {
		channel->rtp_termination_slot = apr_palloc(session->pool,sizeof(rtp_termination_slot_t));
		channel->rtp_termination_slot->descriptor = rtp_descriptor;
		channel->rtp_termination_slot->termination = NULL;
		channel->rtp_termination_slot->waiting = FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create Channel "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(session));
	return channel;
}

apt_bool_t mrcp_client_session_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_sig_status_code_e status_code = MRCP_SIG_STATUS_CODE_SUCCESS;
	if(!session->offer) {
		return FALSE;
	}
	if(!descriptor) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive Answer "APT_PTRSID_FMT" [null descriptor]",	MRCP_SESSION_PTRSID(&session->base));
		status_code = MRCP_SIG_STATUS_CODE_FAILURE;
		/* raise app response */
		return mrcp_app_sig_response_raise(session,status_code,TRUE);
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive Answer "APT_PTRSID_FMT" [c:%d a:%d v:%d]",
		MRCP_SESSION_PTRSID(&session->base),
		descriptor->control_media_arr->nelts,
		descriptor->audio_media_arr->nelts,
		descriptor->video_media_arr->nelts);

	if(session->base.signaling_agent->mrcp_version == MRCP_VERSION_1) {
		if(mrcp_client_resource_answer_process(session,descriptor) != TRUE) {
			status_code = MRCP_SIG_STATUS_CODE_FAILURE;
		}
	}
	else {
		mrcp_client_control_media_answer_process(session,descriptor);
		mrcp_client_av_media_answer_process(session,descriptor);
	}

	/* store received answer */
	session->answer = descriptor;

	if(!session->answer_flag_count) {
		/* raise app response */
		mrcp_app_sig_response_raise(session,status_code,TRUE);
	}

	return TRUE;
}

apt_bool_t mrcp_client_session_terminate_response_process(mrcp_client_session_t *session)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive Terminate Response "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));

	if(session->terminate_flag_count) {
		session->terminate_flag_count--;
	}

	if(!session->terminate_flag_count) {
		mrcp_app_session_terminate_raise(session,MRCP_SIG_STATUS_CODE_SUCCESS);
	}
	return TRUE;
}

apt_bool_t mrcp_client_session_terminate_event_process(mrcp_client_session_t *session)
{
	if(session->active_request) {
		/* raise app response */
		mrcp_app_sig_response_raise(session,MRCP_SIG_STATUS_CODE_TERMINATE,FALSE);

		/* cancel remaing requests (if any) */
		do {
			session->active_request = apt_list_pop_front(session->request_queue);
			if(session->active_request) {
				mrcp_app_sig_response_raise(session,MRCP_SIG_STATUS_CODE_CANCEL,FALSE);
			}
		}
		while(session->active_request);
	}
	else {
		/* raise app event */
		mrcp_app_sig_event_raise(session,NULL);
	}
	return TRUE;
}

apt_bool_t mrcp_client_session_control_response_process(mrcp_client_session_t *session, mrcp_message_t *message)
{
	mrcp_channel_t *channel = mrcp_client_channel_find_by_name(session,&message->channel_id.resource_name);
	if(!channel) {
		return FALSE;
	}
	return mrcp_app_control_message_raise(session,channel,message);
}

apt_bool_t mrcp_client_session_discover_response_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive Resource Discovery Response "APT_PTR_FMT, MRCP_SESSION_PTR(&session->base));
	if(!session->active_request) {
		return FALSE;
	}

	if(!descriptor) {
		/* raise app response */
		return mrcp_app_sig_response_raise(session,MRCP_SIG_STATUS_CODE_FAILURE,TRUE);
	}

	if(session->base.signaling_agent->mrcp_version == MRCP_VERSION_1) {
		if(descriptor->resource_state == TRUE) {
			mrcp_control_descriptor_t *control_media;
			if(!session->answer) {
				session->answer = descriptor;
			}
			control_media = apr_palloc(session->base.pool,sizeof(mrcp_control_descriptor_t));
			mrcp_control_descriptor_init(control_media);
			control_media->id = mrcp_session_control_media_add(session->answer,control_media);
			control_media->resource_name = descriptor->resource_name;
		}
	}

	if(session->answer_flag_count) {
		session->answer_flag_count--;
	}

	if(!session->answer_flag_count) {
		mrcp_app_message_t *response;
		response = mrcp_client_app_response_create(session->active_request,MRCP_SIG_STATUS_CODE_SUCCESS,session->base.pool);
		response->descriptor = session->answer;
		session->answer = NULL;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Raise App Resource Discovery Response "APT_PTR_FMT, MRCP_SESSION_PTR(&session->base));
		session->application->handler(response);

		session->active_request = apt_list_pop_front(session->request_queue);
		if(session->active_request) {
			mrcp_app_request_dispatch(session,session->active_request);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_client_on_channel_add(mrcp_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)channel->session;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Control Channel Add "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
	if(!channel->waiting_for_channel) {
		return FALSE;
	}
	channel->waiting_for_channel = FALSE;
	if(session->offer_flag_count) {
		session->offer_flag_count--;
		if(!session->offer_flag_count) {
			/* send offer to server */
			mrcp_client_session_offer_send(session);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_client_on_channel_modify(mrcp_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)channel->session;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Control Channel Modify "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
	if(!channel->waiting_for_channel) {
		return FALSE;
	}
	channel->waiting_for_channel = FALSE;
	if(session->answer_flag_count) {
		session->answer_flag_count--;
		if(!session->answer_flag_count) {
			/* raise app response */
			mrcp_app_sig_response_raise(
				session,
				status == TRUE ? MRCP_SIG_STATUS_CODE_SUCCESS : MRCP_SIG_STATUS_CODE_FAILURE,
				TRUE);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_client_on_channel_remove(mrcp_channel_t *channel, apt_bool_t status)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)channel->session;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Control Channel Remove "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
	if(!channel->waiting_for_channel) {
		return FALSE;
	}
	channel->waiting_for_channel = FALSE;
	if(session->terminate_flag_count) {
		session->terminate_flag_count--;
		if(!session->terminate_flag_count) {
			mrcp_app_session_terminate_raise(
				session,
				status == TRUE ? MRCP_SIG_STATUS_CODE_SUCCESS : MRCP_SIG_STATUS_CODE_FAILURE);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_client_on_message_receive(mrcp_channel_t *channel, mrcp_message_t *message)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)channel->session;
	return mrcp_app_control_message_raise(session,channel,message);
}

mrcp_app_message_t* mrcp_client_app_signaling_request_create(mrcp_sig_command_e command_id, apr_pool_t *pool)
{
	mrcp_app_message_t *app_message = apr_palloc(pool,sizeof(mrcp_app_message_t));
	app_message->message_type = MRCP_APP_MESSAGE_TYPE_SIGNALING;
	app_message->sig_message.message_type = MRCP_SIG_MESSAGE_TYPE_REQUEST;
	app_message->sig_message.command_id = command_id;
	return app_message;
}

mrcp_app_message_t* mrcp_client_app_signaling_event_create(mrcp_sig_event_e event_id, apr_pool_t *pool)
{
	mrcp_app_message_t *app_message = apr_palloc(pool,sizeof(mrcp_app_message_t));
	app_message->message_type = MRCP_APP_MESSAGE_TYPE_SIGNALING;
	app_message->sig_message.message_type = MRCP_SIG_MESSAGE_TYPE_EVENT;
	app_message->sig_message.event_id = event_id;
	return app_message;
}

mrcp_app_message_t* mrcp_client_app_control_message_create(apr_pool_t *pool)
{
	mrcp_app_message_t *app_message = apr_palloc(pool,sizeof(mrcp_app_message_t));
	app_message->message_type = MRCP_APP_MESSAGE_TYPE_CONTROL;
	return app_message;
}

mrcp_app_message_t* mrcp_client_app_response_create(const mrcp_app_message_t *app_request, mrcp_sig_status_code_e status, apr_pool_t *pool)
{
	mrcp_app_message_t *app_response = apr_palloc(pool,sizeof(mrcp_app_message_t));
	*app_response = *app_request;
	app_response->sig_message.message_type = MRCP_SIG_MESSAGE_TYPE_RESPONSE;
	app_response->sig_message.status = status;
	return app_response;
}

apt_bool_t mrcp_client_app_message_process(mrcp_app_message_t *app_message)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)app_message->session;
	if(app_message->message_type == MRCP_APP_MESSAGE_TYPE_SIGNALING) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive App Request "APT_PTRSID_FMT" [%d]",
			MRCP_SESSION_PTRSID(&session->base),
			app_message->sig_message.command_id);
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive App MRCP Request "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
	}

	if(session->active_request) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Push Request to Queue "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
		apt_list_push_back(session->request_queue,app_message,session->base.pool);
		return TRUE;
	}

	session->active_request = app_message;
	mrcp_app_request_dispatch(session,app_message);
	return TRUE;
}

static apt_bool_t mrcp_client_session_offer_send(mrcp_client_session_t *session)
{
	mrcp_session_descriptor_t *descriptor = session->offer;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send Offer "APT_PTRSID_FMT" [c:%d a:%d v:%d]",
		MRCP_SESSION_PTRSID(&session->base),
		descriptor->control_media_arr->nelts,
		descriptor->audio_media_arr->nelts,
		descriptor->video_media_arr->nelts);
	return mrcp_session_offer(&session->base,descriptor);
}

static apt_bool_t mrcp_app_session_terminate_raise(mrcp_client_session_t *session, mrcp_sig_status_code_e status)
{
	int i;
	mrcp_channel_t *channel;
	for(i=0; i<session->channels->nelts; i++) {
		channel = ((mrcp_channel_t**)session->channels->elts)[i];
		if(!channel) continue;

		if(channel->control_channel) {
			mrcp_client_control_channel_destroy(channel->control_channel);
			channel->control_channel = NULL;
		}
	}

	mrcp_client_session_remove(session->application->client,session);
	/* raise app response */
	return mrcp_app_sig_response_raise(session,status,FALSE);
}

static apt_bool_t mrcp_app_sig_response_raise(mrcp_client_session_t *session, mrcp_sig_status_code_e status, apt_bool_t process_pending_requests)
{
	mrcp_app_message_t *response;
	const mrcp_app_message_t *request = session->active_request;
	if(!request) {
		return FALSE;
	}
	session->active_request = NULL;
	response = mrcp_client_app_response_create(request,status,session->base.pool);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Raise App Response "APT_PTRSID_FMT" [%d] %s [%d]", 
		MRCP_SESSION_PTRSID(&session->base),
		response->sig_message.command_id,
		status == MRCP_SIG_STATUS_CODE_SUCCESS ? "SUCCESS" : "FAILURE",
		status);
	session->application->handler(response);

	if(process_pending_requests) {
		session->active_request = apt_list_pop_front(session->request_queue);
		if(session->active_request) {
			mrcp_app_request_dispatch(session,session->active_request);
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_app_sig_event_raise(mrcp_client_session_t *session, mrcp_channel_t *channel)
{
	mrcp_app_message_t *app_event;
	if(!session) {
		return FALSE;
	}
	app_event = mrcp_client_app_signaling_event_create(MRCP_SIG_EVENT_TERMINATE,session->base.pool);
	app_event->application = session->application;
	app_event->session = &session->base;
	app_event->channel = channel;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Raise App Event "APT_PTRSID_FMT" [%d]", 
		MRCP_SESSION_PTRSID(&session->base),
		app_event->sig_message.event_id);
	return session->application->handler(app_event);
}

static apt_bool_t mrcp_app_control_message_raise(mrcp_client_session_t *session, mrcp_channel_t *channel, mrcp_message_t *mrcp_message)
{
	if(mrcp_message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		mrcp_app_message_t *response;
		mrcp_message_t *mrcp_request;
		if(!session->active_request || !session->active_request->control_message) {
			return FALSE;
		}
		response = mrcp_client_app_response_create(session->active_request,0,session->base.pool);
		mrcp_request = session->active_request->control_message;
		mrcp_message->start_line.method_id = mrcp_request->start_line.method_id;
		mrcp_message->start_line.method_name = mrcp_request->start_line.method_name;
		response->control_message = mrcp_message;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Raise App MRCP Response "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
		session->application->handler(response);

		session->active_request = apt_list_pop_front(session->request_queue);
		if(session->active_request) {
			mrcp_app_request_dispatch(session,session->active_request);
		}
	}
	else if(mrcp_message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		mrcp_app_message_t *app_message;
		app_message = mrcp_client_app_control_message_create(session->base.pool);
		app_message->control_message = mrcp_message;
		app_message->application = session->application;
		app_message->session = &session->base;
		app_message->channel = channel;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Raise App MRCP Event "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
		session->application->handler(app_message);
	}
	return TRUE;
}

static apt_bool_t mrcp_client_channel_find(mrcp_client_session_t *session, mrcp_channel_t *channel, int *index)
{
	int i;
	for(i=0; i<session->channels->nelts; i++) {
		mrcp_channel_t *existing_channel = ((mrcp_channel_t**)session->channels->elts)[i];
		if(existing_channel == channel) {
			if(index) {
				*index = i;
			}
			return TRUE;
		}
	}
	return FALSE;
}

static rtp_termination_slot_t* mrcp_client_rtp_termination_find(mrcp_client_session_t *session, mpf_termination_t *termination)
{
	int i;
	rtp_termination_slot_t *slot;
	for(i=0; i<session->terminations->nelts; i++) {
		slot = &((rtp_termination_slot_t*)session->terminations->elts)[i];
		if(slot && slot->termination == termination) {
			return slot;
		}
	}
	return NULL;
}

static int mrcp_client_audio_media_find_by_mid(const mrcp_session_descriptor_t *descriptor, apr_size_t mid)
{
	int i;
	mpf_rtp_media_descriptor_t *media;
	for(i=0; i<descriptor->audio_media_arr->nelts; i++) {
		media = ((mpf_rtp_media_descriptor_t**)descriptor->audio_media_arr->elts)[i];
		if(media->mid == mid) {
			return i;
		}
	}
	return -1;
}

static mrcp_channel_t* mrcp_client_channel_termination_find(mrcp_client_session_t *session, mpf_termination_t *termination)
{
	int i;
	mrcp_channel_t *channel;
	for(i=0; i<session->channels->nelts; i++) {
		channel = ((mrcp_channel_t**)session->channels->elts)[i];
		if(!channel) continue;

		if(channel->termination == termination) {
			return channel;
		}
	}
	return NULL;
}

static mrcp_channel_t* mrcp_client_channel_find_by_name(mrcp_client_session_t *session, const apt_str_t *resource_name)
{
	int i;
	mrcp_channel_t *channel;
	for(i=0; i<session->channels->nelts; i++) {
		channel = ((mrcp_channel_t**)session->channels->elts)[i];
		if(!channel) continue;

		if(apt_string_compare(channel->resource_name,resource_name) == TRUE) {
			return channel;
		}
	}
	return NULL;
}

static apt_bool_t mrcp_client_message_send(mrcp_client_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	if(!session->base.id.length) {
		mrcp_message_t *response = mrcp_response_create(message,message->pool);
		response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Raise App Failure MRCP Response "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
		mrcp_app_control_message_raise(session,channel,response);
		return TRUE;
	}

	message->channel_id.session_id = session->base.id;
	message->start_line.request_id = ++session->base.last_request_id;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send MRCP Request "APT_PTRSIDRES_FMT" [%d]",
					MRCP_SESSION_PTRSID(&session->base),
					channel->resource_name->buf,
					message->start_line.request_id);

	if(channel->control_channel) {
		/* MRCPv2 */
		mrcp_client_control_message_send(channel->control_channel,message);
	}
	else {
		/* MRCPv1 */
		mrcp_session_control_request(channel->session,message);
	}

	return TRUE;
}

static apt_bool_t mrcp_client_channel_modify(mrcp_client_session_t *session, mrcp_channel_t *channel, apt_bool_t enable)
{
	int index;
	if(!session->offer) {
		return FALSE;
	}
	if(!channel->resource_name) {
		return FALSE;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Modify Control Channel "APT_PTRSIDRES_FMT" [%d]",
					MRCP_SESSION_PTRSID(&session->base),
					channel->resource_name->buf,
					enable);
	if(mrcp_client_channel_find(session,channel,&index) == TRUE) {
		mrcp_control_descriptor_t *control_media = mrcp_session_control_media_get(session->offer,(apr_size_t)index);
		if(control_media) {
			control_media->port = (enable == TRUE) ? 9 : 0;
			if(channel->termination && channel->termination->audio_stream) {
				int i = mrcp_client_audio_media_find_by_mid(session->offer,control_media->cmid);
				if(i >= 0) {
					mpf_stream_mode_e mode = mpf_stream_mode_negotiate(channel->termination->audio_stream->mode);
					mpf_rtp_media_descriptor_t *audio_media = mrcp_session_audio_media_get(session->offer,(apr_size_t)i);
					if(audio_media) {
						if(enable == TRUE) {
							audio_media->mode |= mode;
						}
						else {
							audio_media->mode &= ~mode;
						}
						audio_media->base.state = (audio_media->mode != STREAM_MODE_NONE) ? MPF_MEDIA_ENABLED : MPF_MEDIA_DISABLED;
					}
				}
			}
		}
	}

	session->offer->resource_name = *channel->resource_name;
	session->offer->resource_state = enable;
	return mrcp_client_session_offer_send(session);
}

static apt_bool_t mrcp_client_channel_add(mrcp_client_session_t *session, mrcp_channel_t *channel)
{
	mrcp_channel_t **channel_slot;
	mrcp_control_descriptor_t *control_media;
	mpf_rtp_termination_descriptor_t *rtp_descriptor = NULL;
	rtp_termination_slot_t *termination_slot;
	apr_pool_t *pool = session->base.pool;
	mrcp_profile_t *profile = session->profile;
	if(mrcp_client_channel_find(session,channel,NULL) == TRUE) {
		/* update */
		return mrcp_client_channel_modify(session,channel,TRUE);
	}

	if(!session->offer) {
		session->offer = mrcp_session_descriptor_create(pool);
		session->context = mpf_context_create(session,5,pool);
	}
	if(!channel->resource) {
		channel->resource = mrcp_resource_get(profile->resource_factory,channel->resource_id);
		if(!channel->resource) {
			return FALSE;
		}
		channel->resource_name = mrcp_resource_name_get(profile->resource_factory,channel->resource_id);
		if(!channel->resource_name) {
			return FALSE;
		}
	}
	if(session->base.signaling_agent->mrcp_version == MRCP_VERSION_1) {
		session->offer->resource_name = *channel->resource_name;
		session->offer->resource_state = TRUE;
	}
	else {
		if(!channel->control_channel) {
			channel->control_channel = mrcp_client_control_channel_create(profile->connection_agent,channel,pool);
		}
		control_media = mrcp_control_offer_create(pool);
		control_media->id = mrcp_session_control_media_add(session->offer,control_media);
		control_media->cmid = session->offer->control_media_arr->nelts;
		control_media->resource_name = *channel->resource_name;
		if(mrcp_client_control_channel_add(channel->control_channel,control_media) == TRUE) {
			channel->waiting_for_channel = TRUE;
			session->offer_flag_count++;
		}
	}

	/* add to channel array */
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Add Control Channel "APT_PTRSIDRES_FMT,
					MRCP_SESSION_PTRSID(&session->base),
					channel->resource_name->buf);
	channel_slot = apr_array_push(session->channels);
	*channel_slot = channel;

	if(channel->termination) {
		if(mrcp_client_mpf_request_send(profile->media_engine,MPF_COMMAND_ADD,session->context,channel->termination,NULL) == TRUE) {
			channel->waiting_for_termination = TRUE;
			session->offer_flag_count++;
		}
	}

	if(channel->rtp_termination_slot) {
		rtp_descriptor = channel->rtp_termination_slot->descriptor;
	}	
	/* add to rtp termination array */
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Add RTP Termination "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
	termination_slot = apr_array_push(session->terminations);
	termination_slot->waiting = FALSE;
	termination_slot->termination = NULL;
	termination_slot->descriptor = NULL;
	if(rtp_descriptor) {
		if(rtp_descriptor->audio.local) {
			session->offer->ip = rtp_descriptor->audio.local->base.ip;
			session->offer->ext_ip = rtp_descriptor->audio.local->base.ext_ip;
			rtp_descriptor->audio.local->base.id = mrcp_session_audio_media_add(session->offer,rtp_descriptor->audio.local);
			rtp_descriptor->audio.local->mid = session->offer->audio_media_arr->nelts;
		}
	}
	else {
		/* create rtp termination */
		mpf_termination_t *termination = mpf_termination_create(profile->rtp_termination_factory,session,session->base.pool);
		termination_slot->termination = termination;

		/* initialize rtp descriptor */
		rtp_descriptor = apr_palloc(pool,sizeof(mpf_rtp_termination_descriptor_t));
		mpf_rtp_termination_descriptor_init(rtp_descriptor);
		if(channel->termination && channel->termination->audio_stream) {
			mpf_rtp_media_descriptor_t *media;
			media = apr_palloc(pool,sizeof(mpf_rtp_media_descriptor_t));
			mpf_rtp_media_descriptor_init(media);
			media->base.state = MPF_MEDIA_ENABLED;
			media->mode = mpf_stream_mode_negotiate(channel->termination->audio_stream->mode);
			rtp_descriptor->audio.local = media;
		}
		/* send add termination request (add to media context) */
		if(mrcp_client_mpf_request_send(profile->media_engine,MPF_COMMAND_ADD,session->context,termination,rtp_descriptor) == TRUE) {
			termination_slot->waiting = TRUE;
			session->offer_flag_count++;
		}
	}
	termination_slot->descriptor = rtp_descriptor;
	channel->rtp_termination_slot = termination_slot;

	if(!session->offer_flag_count) {
		/* send offer to server */
		mrcp_client_session_offer_send(session);
	}
	return TRUE;
}

static apt_bool_t mrcp_client_session_update(mrcp_client_session_t *session)
{
	if(!session->offer) {
		return FALSE;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Update Session "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
	return mrcp_client_session_offer_send(session);
}

static apt_bool_t mrcp_client_session_terminate(mrcp_client_session_t *session)
{
	mrcp_profile_t *profile;
	mrcp_channel_t *channel;
	rtp_termination_slot_t *slot;
	int i;
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Terminate Session "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
	profile = session->profile;
	/* remove existing control channels */
	for(i=0; i<session->channels->nelts; i++) {
		/* get existing channel */
		channel = *((mrcp_channel_t**)session->channels->elts + i);
		if(!channel) continue;

		if(channel->control_channel) {
			/* remove channel */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Remove Control Channel "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
			if(mrcp_client_control_channel_remove(channel->control_channel) == TRUE) {
				channel->waiting_for_channel = TRUE;
				session->terminate_flag_count++;
			}
		}

		if(channel->termination) {		
			/* send subtract termination request */
			if(channel->termination) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Subtract Channel Termination "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
				if(mrcp_client_mpf_request_send(profile->media_engine,MPF_COMMAND_SUBTRACT,session->context,channel->termination,NULL) == TRUE) {
					channel->waiting_for_termination = TRUE;
					session->terminate_flag_count++;
				}
			}
		}
	}

	/* subtract existing terminations */
	for(i=0; i<session->terminations->nelts; i++) {
		/* get existing termination */
		slot = &((rtp_termination_slot_t*)session->terminations->elts)[i];
		if(!slot || !slot->termination) continue;

		/* send subtract termination request */
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Subtract Termination "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
		if(mrcp_client_mpf_request_send(profile->media_engine,MPF_COMMAND_SUBTRACT,session->context,slot->termination,NULL) == TRUE) {
			slot->waiting = TRUE;
			session->terminate_flag_count++;
		}
	}

	session->terminate_flag_count++;
	mrcp_session_terminate_request(&session->base);
	return TRUE;
}

static apt_bool_t mrcp_client_resource_discover(mrcp_client_session_t *session)
{
	mrcp_session_descriptor_t *descriptor = NULL;
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send Resource Discovery Request "APT_PTR_FMT, MRCP_SESSION_PTR(&session->base));
	session->answer = NULL;
	if(session->base.signaling_agent->mrcp_version == MRCP_VERSION_1) {
		const apt_str_t *resource_name;
		mrcp_resource_id i;

		for(i=0; i<MRCP_RESOURCE_TYPE_COUNT; i++) {
			resource_name = mrcp_resource_name_get(session->profile->resource_factory,i);
			if(!resource_name) continue;
		
			descriptor = mrcp_session_descriptor_create(session->base.pool);
			apt_string_copy(&descriptor->resource_name,resource_name,session->base.pool);
			if(mrcp_session_discover_request(&session->base,descriptor) == TRUE) {
				session->answer_flag_count++;
			}
		}
	}
	else {
		if(mrcp_session_discover_request(&session->base,descriptor) == TRUE) {
			session->answer_flag_count++;
		}
	}

	if(session->answer_flag_count == 0) {
		mrcp_app_sig_response_raise(session,MRCP_SIG_STATUS_CODE_FAILURE,TRUE);
	}
	return TRUE;
}

static apt_bool_t mrcp_client_on_termination_add(mrcp_client_session_t *session, mpf_message_t *mpf_message)
{
	rtp_termination_slot_t *termination_slot;
	if(!session) {
		return FALSE;
	}
	termination_slot = mrcp_client_rtp_termination_find(session,mpf_message->termination);
	if(termination_slot) {
		/* rtp termination */
		mpf_rtp_termination_descriptor_t *rtp_descriptor;
		if(termination_slot->waiting == FALSE) {
			return FALSE;
		}
		termination_slot->waiting = FALSE;
		rtp_descriptor = mpf_message->descriptor;
		if(rtp_descriptor->audio.local) {
			session->offer->ip = rtp_descriptor->audio.local->base.ip;
			session->offer->ext_ip = rtp_descriptor->audio.local->base.ext_ip;
			rtp_descriptor->audio.local->base.id = mrcp_session_audio_media_add(session->offer,rtp_descriptor->audio.local);
			rtp_descriptor->audio.local->mid = session->offer->audio_media_arr->nelts;
		}
		if(session->offer_flag_count) {
			session->offer_flag_count--;
			if(!session->offer_flag_count) {
				/* send offer to server */
				mrcp_client_session_offer_send(session);
			}
		}
	}
	else {
		/* channel termination */
		mrcp_channel_t *channel = mrcp_client_channel_termination_find(session,mpf_message->termination);
		if(channel && channel->waiting_for_termination == TRUE) {
			channel->waiting_for_termination = FALSE;
			if(session->offer_flag_count) {
				session->offer_flag_count--;
				if(!session->offer_flag_count) {
					/* send offer to server */
					mrcp_client_session_offer_send(session);
				}
			}
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_on_termination_modify(mrcp_client_session_t *session, mpf_message_t *mpf_message)
{
	rtp_termination_slot_t *termination_slot;
	if(!session) {
		return FALSE;
	}
	termination_slot = mrcp_client_rtp_termination_find(session,mpf_message->termination);
	if(termination_slot) {
		/* rtp termination */
		if(termination_slot->waiting == FALSE) {
			return FALSE;
		}
		termination_slot->waiting = FALSE;
		termination_slot->descriptor = mpf_message->descriptor;;

		if(session->offer_flag_count) {
			session->offer_flag_count--;
			if(!session->offer_flag_count) {
				/* send offer to server */
				mrcp_client_session_offer_send(session);
			}
		}
		if(session->answer_flag_count) {
			session->answer_flag_count--;
			if(!session->answer_flag_count) {
				/* raise app response */
				mrcp_app_sig_response_raise(session,MRCP_SIG_STATUS_CODE_SUCCESS,TRUE);
			}
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_on_termination_subtract(mrcp_client_session_t *session, mpf_message_t *mpf_message)
{
	rtp_termination_slot_t *termination_slot;
	if(!session) {
		return FALSE;
	}
	termination_slot = mrcp_client_rtp_termination_find(session,mpf_message->termination);
	if(termination_slot) {
		/* rtp termination */
		if(termination_slot->waiting == FALSE) {
			return FALSE;
		}
		termination_slot->waiting = FALSE;
		if(session->terminate_flag_count) {
			session->terminate_flag_count--;
			if(!session->terminate_flag_count) {
				mrcp_app_session_terminate_raise(session,MRCP_SIG_STATUS_CODE_SUCCESS);
			}
		}
	}
	else {
		/* channel termination */
		mrcp_channel_t *channel = mrcp_client_channel_termination_find(session,mpf_message->termination);
		if(channel && channel->waiting_for_termination == TRUE) {
			channel->waiting_for_termination = FALSE;
			if(session->terminate_flag_count) {
				session->terminate_flag_count--;
				if(!session->terminate_flag_count) {
					/* raise app response */
					mrcp_app_sig_response_raise(session,MRCP_SIG_STATUS_CODE_SUCCESS,TRUE);
				}
			}
		}
	}
	return TRUE;
}

apt_bool_t mrcp_client_mpf_message_process(mpf_message_t *mpf_message)
{
	mrcp_client_session_t *session = NULL;
	if(mpf_message->context) {
		session = mpf_context_object_get(mpf_message->context);
	}
	if(!session) {
		return FALSE;
	}
	if(mpf_message->message_type == MPF_MESSAGE_TYPE_RESPONSE) {
		switch(mpf_message->command_id) {
			case MPF_COMMAND_ADD:
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Termination Add "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
				mrcp_client_on_termination_add(session,mpf_message);
				break;
			case MPF_COMMAND_MODIFY:
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Termination Modify "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
				mrcp_client_on_termination_modify(session,mpf_message);
				break;
			case MPF_COMMAND_SUBTRACT:
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"On Termination Subtract "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
				mrcp_client_on_termination_subtract(session,mpf_message);
				break;
			default:
				break;
		}
	}
	else if(mpf_message->message_type == MPF_MESSAGE_TYPE_EVENT) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process MPF Event "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
	}
	return TRUE;
}

static apt_bool_t mrcp_client_resource_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	apt_bool_t status = TRUE;
	if(session->offer->resource_state == TRUE) {
		if(descriptor->resource_state == TRUE) {
			mrcp_client_av_media_answer_process(session,descriptor);
		}
		else {
			status = FALSE;
		}
	}
	return status;
}

static apt_bool_t mrcp_client_control_media_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_channel_t *channel;
	mrcp_control_descriptor_t *control_descriptor;
	int i;
	int count = session->channels->nelts;
	if(count != descriptor->control_media_arr->nelts) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Number of control channels [%d] != Number of control media in answer [%d]",
			count,descriptor->control_media_arr->nelts);
		count = descriptor->control_media_arr->nelts;
	}

	if(!session->base.id.length) {
		/* initial answer received, store session id and add to session's table */
		control_descriptor = mrcp_session_control_media_get(descriptor,0);
		if(control_descriptor) {
			session->base.id = control_descriptor->session_id;
		}
	}

	/* update existing control channels */
	for(i=0; i<count; i++) {
		/* get existing channel */
		channel = *((mrcp_channel_t**)session->channels->elts + i);
		if(!channel) continue;

		/* get control descriptor */
		control_descriptor = mrcp_session_control_media_get(descriptor,i);
		/* modify channel */
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Modify Control Channel "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
		if(mrcp_client_control_channel_modify(channel->control_channel,control_descriptor) == TRUE) {
			channel->waiting_for_channel = TRUE;
			session->answer_flag_count++;
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_av_media_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	rtp_termination_slot_t *slot;
	int i;
	int count = session->terminations->nelts;
	if(count != descriptor->audio_media_arr->nelts) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Number of terminations [%d] != Number of audio media in answer [%d]",
			count,descriptor->audio_media_arr->nelts);
		count = descriptor->audio_media_arr->nelts;
	}
	
	/* update existing terminations */
	for(i=0; i<count; i++) {
		mpf_rtp_media_descriptor_t *remote_media;
		mpf_rtp_termination_descriptor_t *rtp_descriptor;
		/* get existing termination */
		slot = &((rtp_termination_slot_t*)session->terminations->elts)[i];
		if(!slot) continue;

		remote_media = mrcp_session_audio_media_get(descriptor,i);
		if(slot->descriptor) {
			slot->descriptor->audio.remote = remote_media;
		}
		if(slot->termination) {
			/* construct termination descriptor */
			rtp_descriptor = apr_palloc(session->base.pool,sizeof(mpf_rtp_termination_descriptor_t));
			mpf_rtp_termination_descriptor_init(rtp_descriptor);
			rtp_descriptor->audio.local = NULL;
			rtp_descriptor->audio.remote = remote_media;

			/* send modify termination request */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Modify Termination "APT_PTRSID_FMT, MRCP_SESSION_PTRSID(&session->base));
			if(mrcp_client_mpf_request_send(session->profile->media_engine,MPF_COMMAND_MODIFY,session->context,slot->termination,rtp_descriptor) == TRUE) {
				slot->waiting = TRUE;
				session->answer_flag_count++;
			}
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_app_request_dispatch(mrcp_client_session_t *session, const mrcp_app_message_t *app_message)
{
	if(session->registered == FALSE) {
		session->base.signaling_agent = session->profile->signaling_agent;
		session->base.signaling_agent->create_client_session(&session->base);

		mrcp_client_session_add(session->application->client,session);
		session->registered = TRUE;
	}
	switch(app_message->message_type) {
		case MRCP_APP_MESSAGE_TYPE_SIGNALING:
		{
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Dispatch Application Request "APT_PTRSID_FMT" [%d]",
				MRCP_SESSION_PTRSID(&session->base),
				app_message->sig_message.command_id);
			switch(app_message->sig_message.command_id) {
				case MRCP_SIG_COMMAND_SESSION_UPDATE:
					mrcp_client_session_update(session);
					break;
				case MRCP_SIG_COMMAND_SESSION_TERMINATE:
					mrcp_client_session_terminate(session);
					break;
				case MRCP_SIG_COMMAND_CHANNEL_ADD:
					mrcp_client_channel_add(session,app_message->channel);
					break;
				case MRCP_SIG_COMMAND_CHANNEL_REMOVE:
					mrcp_client_channel_modify(session,app_message->channel,FALSE);
					break;
				case MRCP_SIG_COMMAND_RESOURCE_DISCOVER:
					mrcp_client_resource_discover(session);
					break;
				default:
					break;
			}
			break;
		}
		case MRCP_APP_MESSAGE_TYPE_CONTROL:
		{
			mrcp_client_message_send(session,app_message->channel,app_message->control_message);
			break;
		}
	}
	return TRUE;
}

/** Dispatch application message */
MRCP_DECLARE(apt_bool_t) mrcp_application_message_dispatch(const mrcp_app_message_dispatcher_t *dispatcher, const mrcp_app_message_t *app_message)
{
	apt_bool_t status = FALSE;
	switch(app_message->message_type) {
		case MRCP_APP_MESSAGE_TYPE_SIGNALING:
		{
			if(app_message->sig_message.message_type == MRCP_SIG_MESSAGE_TYPE_RESPONSE) {
				switch(app_message->sig_message.command_id) {
					case MRCP_SIG_COMMAND_SESSION_UPDATE:
						if(dispatcher->on_session_update) {
							status = dispatcher->on_session_update(
										app_message->application,
										app_message->session,
										app_message->sig_message.status);
						}
						break;
					case MRCP_SIG_COMMAND_SESSION_TERMINATE:
						if(dispatcher->on_session_terminate) {
							status = dispatcher->on_session_terminate(
										app_message->application,
										app_message->session,
										app_message->sig_message.status);
						}
						break;
					case MRCP_SIG_COMMAND_CHANNEL_ADD:
						if(dispatcher->on_channel_add) {
							status = dispatcher->on_channel_add(
										app_message->application,
										app_message->session,
										app_message->channel,
										app_message->sig_message.status);
						}
						break;
					case MRCP_SIG_COMMAND_CHANNEL_REMOVE:
						if(dispatcher->on_channel_remove) {
							status = dispatcher->on_channel_remove(
										app_message->application,
										app_message->session,
										app_message->channel,
										app_message->sig_message.status);
						}
						break;
					case MRCP_SIG_COMMAND_RESOURCE_DISCOVER:
						if(dispatcher->on_resource_discover) {
							status = dispatcher->on_resource_discover(
										app_message->application,
										app_message->session,
										app_message->descriptor,
										app_message->sig_message.status);
						}
						break;
					default:
						break;
				}
			}
			else if(app_message->sig_message.message_type == MRCP_SIG_MESSAGE_TYPE_EVENT) {
				switch(app_message->sig_message.event_id) {
					case MRCP_SIG_EVENT_READY:
						if(dispatcher->on_ready) {
							status = dispatcher->on_ready(
										app_message->application,
										app_message->sig_message.status);
						}
						break;
					case MRCP_SIG_EVENT_TERMINATE:
						if(dispatcher->on_terminate_event) {
							status = dispatcher->on_terminate_event(
										app_message->application,
										app_message->session,
										app_message->channel);
						}
						break;
					default:
						break;
				}
			}
			break;
		}
		case MRCP_APP_MESSAGE_TYPE_CONTROL:
		{
			if(dispatcher->on_message_receive) {
				status = dispatcher->on_message_receive(
										app_message->application,
										app_message->session,
										app_message->channel,
										app_message->control_message);
			}
			break;
		}
	}
	return status;
}

static apt_bool_t mrcp_client_mpf_request_send(
						mpf_engine_t *engine, 
						mpf_command_type_e command_id,
						mpf_context_t *context, 
						mpf_termination_t *termination, 
						void *descriptor)
{
	apt_task_t *media_task;
	apt_task_msg_t *msg;
	mpf_message_t *mpf_message;
	if(!engine) {
		return FALSE;
	}
	media_task = mpf_task_get(engine);
	msg = apt_task_msg_get(media_task);
	msg->type = TASK_MSG_USER;
	mpf_message = (mpf_message_t*) msg->data;

	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->command_id = command_id;
	mpf_message->context = context;
	mpf_message->termination = termination;
	mpf_message->descriptor = descriptor;
	return apt_task_msg_signal(media_task,msg);
}
