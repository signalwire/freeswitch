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

typedef struct mrcp_sofia_agent_t mrcp_sofia_agent_t;
#define NUA_MAGIC_T mrcp_sofia_agent_t

typedef struct mrcp_sofia_session_t mrcp_sofia_session_t;
#define NUA_HMAGIC_T mrcp_sofia_session_t

#include <sofia-sip/su.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/sofia_features.h>
#undef strcasecmp
#undef strncasecmp
#include <apr_general.h>

#include "mrcp_sofiasip_client_agent.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_sdp.h"
#include "apt_log.h"

#define SOFIA_TASK_NAME "SofiaSIP Agent"

struct mrcp_sofia_agent_t {
	mrcp_sig_agent_t           *sig_agent;

	mrcp_sofia_client_config_t *config;
	char                       *sip_contact_str;
	char                       *sip_to_str;
	char                       *sip_from_str;
	char                       *sip_bind_str;

	su_root_t                  *root;
	nua_t                      *nua;
};

struct mrcp_sofia_session_t {
	mrcp_session_t     *session;
	su_home_t          *home;
	nua_handle_t       *nh;

	apt_bool_t          terminate_requested;
	apr_thread_mutex_t *mutex;
};

/* Task Interface */
static void mrcp_sofia_task_initialize(apt_task_t *task);
static apt_bool_t mrcp_sofia_task_run(apt_task_t *task);
static apt_bool_t mrcp_sofia_task_terminate(apt_task_t *task);

/* MRCP Signaling Interface */
static apt_bool_t mrcp_sofia_session_offer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_sofia_session_terminate_request(mrcp_session_t *session);
static apt_bool_t mrcp_sofia_session_discover_request(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);

static const mrcp_session_request_vtable_t session_request_vtable = {
	mrcp_sofia_session_offer,
	mrcp_sofia_session_terminate_request,
	NULL,
	mrcp_sofia_session_discover_request
};

static apt_bool_t mrcp_sofia_config_validate(mrcp_sofia_agent_t *sofia_agent, mrcp_sofia_client_config_t *config, apr_pool_t *pool);
static apt_bool_t mrcp_sofia_session_create(mrcp_session_t *session);

static void mrcp_sofia_event_callback( nua_event_t           nua_event,
									   int                   status,
									   char const           *phrase,
									   nua_t                *nua,
									   mrcp_sofia_agent_t   *sofia_agent,
									   nua_handle_t         *nh,
									   mrcp_sofia_session_t *sofia_session,
									   sip_t const          *sip,
									   tagi_t                tags[]);


/** Create Sofia-SIP Signaling Agent */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_sofiasip_client_agent_create(mrcp_sofia_client_config_t *config, apr_pool_t *pool)
{
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	mrcp_sofia_agent_t *sofia_agent;
	sofia_agent = apr_palloc(pool,sizeof(mrcp_sofia_agent_t));
	sofia_agent->sig_agent = mrcp_signaling_agent_create(sofia_agent,MRCP_VERSION_2,pool);
	sofia_agent->sig_agent->create_client_session = mrcp_sofia_session_create;
	sofia_agent->root = NULL;
	sofia_agent->nua = NULL;

	if(mrcp_sofia_config_validate(sofia_agent,config,pool) == FALSE) {
		return NULL;
	}

	task = apt_task_create(sofia_agent,NULL,pool);
	if(!task) {
		return NULL;
	}
	apt_task_name_set(task,SOFIA_TASK_NAME);
	vtable = apt_task_vtable_get(task);
	if(vtable) {
		vtable->on_pre_run = mrcp_sofia_task_initialize;
		vtable->run = mrcp_sofia_task_run;
		vtable->terminate = mrcp_sofia_task_terminate;
	}
	sofia_agent->sig_agent->task = task;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create "SOFIA_TASK_NAME" ["SOFIA_SIP_VERSION"] %s:%hu -> %s:%hu %s",
			config->local_ip,config->local_port,
			config->remote_ip,config->remote_port,
			config->transport ? config->transport : "");
	return sofia_agent->sig_agent;
}

/** Allocate Sofia-SIP config */
MRCP_DECLARE(mrcp_sofia_client_config_t*) mrcp_sofiasip_client_config_alloc(apr_pool_t *pool)
{
	mrcp_sofia_client_config_t *config = apr_palloc(pool,sizeof(mrcp_sofia_client_config_t));
	config->local_ip = NULL;
	config->ext_ip = NULL;
	config->local_port = 0;
	config->local_user_name = NULL;
	config->remote_ip = NULL;
	config->remote_port = 0;
	config->remote_user_name = NULL;

	config->force_destination = FALSE;
	
	config->user_agent_name = NULL;
	config->origin = NULL;
	config->transport = NULL;
	return config;
}

static apt_bool_t mrcp_sofia_config_validate(mrcp_sofia_agent_t *sofia_agent, mrcp_sofia_client_config_t *config, apr_pool_t *pool)
{
	const char *local_ip = config->ext_ip ? config->ext_ip : config->local_ip;
	if(!config->local_ip || !config->remote_ip) {
		return FALSE;
	}

	sofia_agent->config = config;
	sofia_agent->sip_contact_str = apr_psprintf(pool,"sip:%s:%d", local_ip, config->local_port);
	sofia_agent->sip_from_str = apr_psprintf(pool,"sip:%s:%d", local_ip, config->local_port);

	if(config->remote_user_name && config->remote_user_name != '\0') {
		sofia_agent->sip_to_str = apr_psprintf(pool,"sip:%s@%s:%d",
										config->remote_user_name,
										config->remote_ip,
										config->remote_port);
	}
	else {
		sofia_agent->sip_to_str = apr_psprintf(pool,"sip:%s:%d",
										config->remote_ip,
										config->remote_port);
	}
	if(config->transport) {
		sofia_agent->sip_bind_str = apr_psprintf(pool,"sip:%s:%d;transport=%s",
										config->local_ip,
										config->local_port,
										config->transport);
	}
	else {
		sofia_agent->sip_bind_str = apr_psprintf(pool,"sip:%s:%d",
										config->local_ip,
										config->local_port);
	}
	return TRUE;
}

static void mrcp_sofia_task_initialize(apt_task_t *task)
{
	mrcp_sofia_agent_t *sofia_agent = apt_task_object_get(task);

	/* Initialize Sofia-SIP library and create event loop */
	su_init();
	sofia_agent->root = su_root_create(NULL);

	/* Create a user agent instance. The stack will call the 'event_callback()' 
	 * callback when events such as succesful registration to network, 
	 * an incoming call, etc, occur. 
	 */
	sofia_agent->nua = nua_create(
					sofia_agent->root,         /* Event loop */
					mrcp_sofia_event_callback, /* Callback for processing events */
					sofia_agent,               /* Additional data to pass to callback */
					NUTAG_URL(sofia_agent->sip_bind_str), /* Address to bind to */
					TAG_END());                /* Last tag should always finish the sequence */
	if(sofia_agent->nua) {
		nua_set_params(
					sofia_agent->nua,
					NUTAG_AUTOANSWER(0),
					NUTAG_APPL_METHOD("OPTIONS"),
					SIPTAG_USER_AGENT_STR(sofia_agent->config->user_agent_name),
					TAG_END());
	}
}

static apt_bool_t mrcp_sofia_task_run(apt_task_t *task)
{
	mrcp_sofia_agent_t *sofia_agent = apt_task_object_get(task);

	if(sofia_agent->nua) {
		/* Run event loop */
		su_root_run(sofia_agent->root);
		
		/* Destroy allocated resources */
		nua_destroy(sofia_agent->nua);
		sofia_agent->nua = NULL;
	}
	su_root_destroy(sofia_agent->root);
	sofia_agent->root = NULL;
	su_deinit();

	apt_task_child_terminate(task);
	return TRUE;
}

static apt_bool_t mrcp_sofia_task_terminate(apt_task_t *task)
{
	mrcp_sofia_agent_t *sofia_agent = apt_task_object_get(task);
	if(sofia_agent->nua) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Send Shutdown Signal to NUA");
		nua_shutdown(sofia_agent->nua);
	}
	return TRUE;
}

static APR_INLINE mrcp_sofia_agent_t* mrcp_sofia_agent_get(mrcp_session_t *session)
{
	return session->signaling_agent->obj;
}

static apt_bool_t mrcp_sofia_session_create(mrcp_session_t *session)
{
	mrcp_sofia_agent_t *sofia_agent = mrcp_sofia_agent_get(session);
	mrcp_sofia_session_t *sofia_session;
	session->request_vtable = &session_request_vtable;

	sofia_session = apr_palloc(session->pool,sizeof(mrcp_sofia_session_t));
	sofia_session->mutex = NULL;
	sofia_session->home = su_home_new(sizeof(*sofia_session->home));
	sofia_session->session = session;
	sofia_session->terminate_requested = FALSE;
	session->obj = sofia_session;
	
	sofia_session->nh = nua_handle(
				sofia_agent->nua,
				sofia_session,
				SIPTAG_TO_STR(sofia_agent->sip_to_str),
				SIPTAG_FROM_STR(sofia_agent->sip_from_str),
				SIPTAG_CONTACT_STR(sofia_agent->sip_contact_str),
				TAG_END());

	apr_thread_mutex_create(&sofia_session->mutex,APR_THREAD_MUTEX_DEFAULT,session->pool);
	return TRUE;
}

static apt_bool_t mrcp_sofia_session_destroy(mrcp_sofia_session_t *sofia_session)
{
	if(sofia_session->mutex) {
		apr_thread_mutex_destroy(sofia_session->mutex);
		sofia_session->mutex = NULL;
	}
	if(sofia_session->home) {
		su_home_unref(sofia_session->home);
		sofia_session->home = NULL;
	}
	return TRUE;
}

static apt_bool_t mrcp_sofia_session_unref(mrcp_sofia_session_t *sofia_session)
{
	if(sofia_session->nh) {
		nua_handle_bind(sofia_session->nh, NULL);
		nua_handle_destroy(sofia_session->nh);
		sofia_session->nh = NULL;
	}
	sofia_session->session = NULL;
	return TRUE;
}

static apt_bool_t mrcp_sofia_session_offer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	char sdp_str[2048];
	char *local_sdp_str = NULL;
	apt_bool_t res = FALSE;
	mrcp_sofia_session_t *sofia_session = session->obj;
	if(!sofia_session) {
		return FALSE;
	}

	if(session->signaling_agent) {
		mrcp_sofia_agent_t *sofia_agent = mrcp_sofia_agent_get(session);
		if(sofia_agent) {
			if(sofia_agent->config->origin) {
				apt_string_set(&descriptor->origin,sofia_agent->config->origin);
			}
		}
	}
	if(sdp_string_generate_by_mrcp_descriptor(sdp_str,sizeof(sdp_str),descriptor,TRUE) > 0) {
		local_sdp_str = sdp_str;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Local SDP "APT_PTRSID_FMT"\n%s", 
			MRCP_SESSION_PTRSID(session), 
			local_sdp_str);
	}

	apr_thread_mutex_lock(sofia_session->mutex);

	if(sofia_session->nh) {
		res = TRUE;
		nua_invite(sofia_session->nh,
				TAG_IF(local_sdp_str,SOATAG_USER_SDP_STR(local_sdp_str)),
				TAG_END());
	}

	apr_thread_mutex_unlock(sofia_session->mutex);
	return res;
}

static apt_bool_t mrcp_sofia_session_terminate_request(mrcp_session_t *session)
{
	mrcp_sofia_session_t *sofia_session = session->obj;
	if(!sofia_session) {
		return FALSE;
	}

	sofia_session->terminate_requested = FALSE;
	apr_thread_mutex_lock(sofia_session->mutex);
	if(sofia_session->nh) {
		sofia_session->terminate_requested = TRUE;
		nua_bye(sofia_session->nh,TAG_END());
	}
	apr_thread_mutex_unlock(sofia_session->mutex);

	if(sofia_session->terminate_requested == FALSE) {
		mrcp_sofia_session_destroy(sofia_session);
		mrcp_session_terminate_response(session);
	}
	return TRUE;
}

static apt_bool_t mrcp_sofia_session_discover_request(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	apt_bool_t res = FALSE;
	mrcp_sofia_session_t *sofia_session = session->obj;
	if(!sofia_session) {
		return FALSE;
	}
	
	apr_thread_mutex_lock(sofia_session->mutex);
	if(sofia_session->nh) {
		res = TRUE;
		nua_options(sofia_session->nh,TAG_END());
	}
	apr_thread_mutex_unlock(sofia_session->mutex);
	return res;
}

static void mrcp_sofia_on_session_ready(
						int                   status,
						mrcp_sofia_agent_t   *sofia_agent,
						nua_handle_t         *nh,
						mrcp_sofia_session_t *sofia_session,
						sip_t const          *sip,
						tagi_t                tags[])
{
	mrcp_session_t *session = sofia_session->session;
	if(session) {
		const char *local_sdp_str = NULL, *remote_sdp_str = NULL;
		mrcp_session_descriptor_t *descriptor = NULL;

		tl_gets(tags, 
				SOATAG_LOCAL_SDP_STR_REF(local_sdp_str),
				SOATAG_REMOTE_SDP_STR_REF(remote_sdp_str),
				TAG_END());

		if(remote_sdp_str) {
			sdp_parser_t *parser = NULL;
			sdp_session_t *sdp = NULL;
			const char *force_destination_ip = NULL;
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remote SDP "APT_PTRSID_FMT"\n%s",
				MRCP_SESSION_PTRSID(session),
				remote_sdp_str);

			parser = sdp_parse(sofia_session->home,remote_sdp_str,(int)strlen(remote_sdp_str),0);
			sdp = sdp_session(parser);
			if(sofia_agent && sofia_agent->config->force_destination == TRUE) {
				force_destination_ip = sofia_agent->config->remote_ip;
			}
			descriptor = mrcp_descriptor_generate_by_sdp_session(sdp,force_destination_ip,session->pool);
			sdp_parser_free(parser);
		}

		mrcp_session_answer(session,descriptor);
	}
}

static void mrcp_sofia_on_session_terminate(
						int                   status,
						mrcp_sofia_agent_t   *sofia_agent,
						nua_handle_t         *nh,
						mrcp_sofia_session_t *sofia_session,
						sip_t const          *sip,
						tagi_t                tags[])
{
	mrcp_session_t *session = sofia_session->session;
	if(session) {
		apt_bool_t terminate_requested;
	
		apr_thread_mutex_lock(sofia_session->mutex);
		terminate_requested = sofia_session->terminate_requested;
		session = sofia_session->session;
		mrcp_sofia_session_unref(sofia_session);
		apr_thread_mutex_unlock(sofia_session->mutex);

		if(terminate_requested == TRUE) {
			mrcp_sofia_session_destroy(sofia_session);
			mrcp_session_terminate_response(session);
		}
		else {
			mrcp_session_terminate_event(session);
		}
	}
}

static void mrcp_sofia_on_state_change(
						int                   status,
						mrcp_sofia_agent_t   *sofia_agent,
						nua_handle_t         *nh,
						mrcp_sofia_session_t *sofia_session,
						sip_t const          *sip,
						tagi_t                tags[])
{
	int ss_state = nua_callstate_init;
	tl_gets(tags,
			NUTAG_CALLSTATE_REF(ss_state),
			TAG_END());
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"SIP Call State "APT_PTR_FMT" [%s]",
		sofia_session ? MRCP_SESSION_PTR(sofia_session->session) : NULL,
		nua_callstate_name(ss_state));

	switch(ss_state) {
		case nua_callstate_ready:
			mrcp_sofia_on_session_ready(status,sofia_agent,nh,sofia_session,sip,tags);
			break;
		case nua_callstate_terminated:
			mrcp_sofia_on_session_terminate(status,sofia_agent,nh,sofia_session,sip,tags);
			break;
	}
}

static void mrcp_sofia_on_resource_discover(
						int                   status,
						mrcp_sofia_agent_t   *sofia_agent,
						nua_handle_t         *nh,
						mrcp_sofia_session_t *sofia_session,
						sip_t const          *sip,
						tagi_t                tags[])
{
	mrcp_session_t *session = sofia_session->session;
	if(session) {
		const char *remote_sdp_str = NULL;
		mrcp_session_descriptor_t *descriptor = NULL;

		tl_gets(tags, 
				SOATAG_REMOTE_SDP_STR_REF(remote_sdp_str),
				TAG_END());

		if(remote_sdp_str) {
			sdp_parser_t *parser = NULL;
			sdp_session_t *sdp = NULL;
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Resource Discovery SDP "APT_PTR_FMT"\n%s", 
				MRCP_SESSION_PTR(session),
				remote_sdp_str);

			parser = sdp_parse(sofia_session->home,remote_sdp_str,(int)strlen(remote_sdp_str),0);
			sdp = sdp_session(parser);
			descriptor = mrcp_descriptor_generate_by_sdp_session(sdp,NULL,session->pool);
			sdp_parser_free(parser);
		}

		mrcp_session_discover_response(session,descriptor);
	}
}

/** This callback will be called by SIP stack to process incoming events */
static void mrcp_sofia_event_callback( 
						nua_event_t           nua_event,
						int                   status,
						char const           *phrase,
						nua_t                *nua,
						mrcp_sofia_agent_t   *sofia_agent,
						nua_handle_t         *nh,
						mrcp_sofia_session_t *sofia_session,
						sip_t const          *sip,
						tagi_t                tags[])
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive SIP Event [%s] Status %d %s",
		nua_event_name(nua_event),status,phrase);

	switch(nua_event) {
		case nua_i_state:
			mrcp_sofia_on_state_change(status,sofia_agent,nh,sofia_session,sip,tags);
			break;
		case nua_r_options:
			mrcp_sofia_on_resource_discover(status,sofia_agent,nh,sofia_session,sip,tags);
			break;
		case nua_r_shutdown:
			/* if status < 200, shutdown still in progress */
			if(status >= 200) {
				/* break main loop of sofia thread */
				su_root_break(sofia_agent->root);
			}
			break;
		default: 
			break;
	}
}
