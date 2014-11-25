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
 * $Id: mrcp_sofiasip_client_agent.c 2253 2014-11-21 02:57:19Z achaloyan@gmail.com $
 */

typedef struct mrcp_sofia_agent_t mrcp_sofia_agent_t;
#define NUA_MAGIC_T mrcp_sofia_agent_t

typedef struct mrcp_sofia_session_t mrcp_sofia_session_t;
#define NUA_HMAGIC_T mrcp_sofia_session_t

#include <sofia-sip/su.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/sofia_features.h>
#undef strcasecmp
#undef strncasecmp
#include <apr_general.h>

#include "mrcp_sofiasip_client_agent.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_sdp.h"
#include "apt_log.h"

struct mrcp_sofia_agent_t {
	mrcp_sig_agent_t           *sig_agent;

	mrcp_sofia_client_config_t *config;
	char                       *sip_contact_str;
	char                       *sip_from_str;
	char                       *sip_bind_str;

	su_root_t                  *root;
	nua_t                      *nua;
};

struct mrcp_sofia_session_t {
	mrcp_session_t            *session;
	const mrcp_sig_settings_t *sip_settings;

	su_home_t                 *home;
	nua_handle_t              *nh;
	enum nua_callstate         nua_state;

	apt_bool_t                 terminate_requested;
	mrcp_session_descriptor_t *descriptor;
	apr_thread_mutex_t        *mutex;
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
static apt_bool_t mrcp_sofia_session_create(mrcp_session_t *session, const mrcp_sig_settings_t *settings);

static void mrcp_sofia_event_callback( nua_event_t           nua_event,
									   int                   status,
									   char const           *phrase,
									   nua_t                *nua,
									   mrcp_sofia_agent_t   *sofia_agent,
									   nua_handle_t         *nh,
									   mrcp_sofia_session_t *sofia_session,
									   sip_t const          *sip,
									   tagi_t                tags[]);

apt_bool_t mrcp_sofiasip_log_init(const char *name, const char *level_str, apt_bool_t redirect);

/** Create Sofia-SIP Signaling Agent */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_sofiasip_client_agent_create(const char *id, mrcp_sofia_client_config_t *config, apr_pool_t *pool)
{
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	mrcp_sofia_agent_t *sofia_agent;
	sofia_agent = apr_palloc(pool,sizeof(mrcp_sofia_agent_t));
	sofia_agent->sig_agent = mrcp_signaling_agent_create(id,sofia_agent,pool);
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
	apt_task_name_set(task,id);
	vtable = apt_task_vtable_get(task);
	if(vtable) {
		vtable->on_pre_run = mrcp_sofia_task_initialize;
		vtable->run = mrcp_sofia_task_run;
		vtable->terminate = mrcp_sofia_task_terminate;
	}
	sofia_agent->sig_agent->task = task;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create SofiaSIP Agent [%s] ["SOFIA_SIP_VERSION"] %s",
				id,sofia_agent->sip_bind_str);
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
	
	config->user_agent_name = NULL;
	config->origin = NULL;
	config->transport = NULL;

	config->sip_t1 = 0;
	config->sip_t2 = 0;
	config->sip_t4 = 0;
	config->sip_t1x64 = 0;

	config->tport_log = FALSE;
	config->tport_dump_file = NULL;

	return config;
}

MRCP_DECLARE(apt_bool_t) mrcp_sofiasip_client_logger_init(const char *name, const char *level_str, apt_bool_t redirect)
{
	return mrcp_sofiasip_log_init(name,level_str,redirect);
}

static apt_bool_t mrcp_sofia_config_validate(mrcp_sofia_agent_t *sofia_agent, mrcp_sofia_client_config_t *config, apr_pool_t *pool)
{
	if(!config->local_ip) {
		return FALSE;
	}

	sofia_agent->config = config;
	if(config->ext_ip) {
		/* Use external IP address in Contact and From headers, if behind NAT */
		sofia_agent->sip_contact_str = apr_psprintf(pool,"sip:%s:%hu", config->ext_ip, config->local_port);
		sofia_agent->sip_from_str = apr_psprintf(pool,"sip:%s:%hu", config->ext_ip, config->local_port);
	}
	else {
		sofia_agent->sip_contact_str = NULL; /* Let Sofia-SIP implicitly set Contact header by default */
		sofia_agent->sip_from_str = apr_psprintf(pool,"sip:%s:%hu", config->local_ip, config->local_port);
	}

	if(config->transport) {
		sofia_agent->sip_bind_str = apr_psprintf(pool,"sip:%s:%hu;transport=%s",
										config->local_ip,
										config->local_port,
										config->transport);
	}
	else {
		sofia_agent->sip_bind_str = apr_psprintf(pool,"sip:%s:%hu",
										config->local_ip,
										config->local_port);
	}
	return TRUE;
}

static void mrcp_sofia_task_initialize(apt_task_t *task)
{
	mrcp_sofia_agent_t *sofia_agent = apt_task_object_get(task);
	mrcp_sofia_client_config_t *sofia_config = sofia_agent->config;

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
		NUTAG_AUTOANSWER(0),
		NUTAG_APPL_METHOD("OPTIONS"),
		TAG_IF(sofia_config->sip_t1,NTATAG_SIP_T1(sofia_config->sip_t1)),
		TAG_IF(sofia_config->sip_t2,NTATAG_SIP_T2(sofia_config->sip_t2)),
		TAG_IF(sofia_config->sip_t4,NTATAG_SIP_T4(sofia_config->sip_t4)),
		TAG_IF(sofia_config->sip_t1x64,NTATAG_SIP_T1X64(sofia_config->sip_t1x64)),
		SIPTAG_USER_AGENT_STR(sofia_config->user_agent_name),
		TAG_IF(sofia_config->tport_log == TRUE,TPTAG_LOG(1)), /* Print out SIP messages to the console */
		TAG_IF(sofia_config->tport_dump_file,TPTAG_DUMP(sofia_config->tport_dump_file)), /* Dump SIP messages to the file */
		TAG_END());                /* Last tag should always finish the sequence */
	if(!sofia_agent->nua) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create NUA [%s] %s",
					apt_task_name_get(task),
					sofia_agent->sip_bind_str);
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

	apt_task_terminate_request_process(task);
	return TRUE;
}

static apt_bool_t mrcp_sofia_task_terminate(apt_task_t *task)
{
	mrcp_sofia_agent_t *sofia_agent = apt_task_object_get(task);
	if(sofia_agent->nua) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Send Shutdown Signal to NUA [%s]",
				apt_task_name_get(task));
		nua_shutdown(sofia_agent->nua);
	}
	return TRUE;
}

static APR_INLINE mrcp_sofia_agent_t* mrcp_sofia_agent_get(mrcp_session_t *session)
{
	return session->signaling_agent->obj;
}

static apt_bool_t mrcp_sofia_session_create(mrcp_session_t *session, const mrcp_sig_settings_t *settings)
{
	const char *sip_to_str;
	mrcp_sofia_agent_t *sofia_agent = mrcp_sofia_agent_get(session);
	mrcp_sofia_session_t *sofia_session;
	session->request_vtable = &session_request_vtable;

	if(!sofia_agent->nua) {
		return FALSE;
	}

	sofia_session = apr_palloc(session->pool,sizeof(mrcp_sofia_session_t));
	sofia_session->mutex = NULL;
	sofia_session->home = su_home_new(sizeof(*sofia_session->home));
	sofia_session->session = session;
	sofia_session->sip_settings = settings;
	sofia_session->terminate_requested = FALSE;
	sofia_session->descriptor = NULL;
	session->obj = sofia_session;

	if(settings->user_name && *settings->user_name != '\0') {
		sip_to_str = apr_psprintf(session->pool,"sip:%s@%s:%hu",
										settings->user_name,
										settings->server_ip,
										settings->server_port);
	}
	else {
		sip_to_str = apr_psprintf(session->pool,"sip:%s:%hu",
										settings->server_ip,
										settings->server_port);
	}

	sofia_session->nh = nua_handle(
				sofia_agent->nua,
				sofia_session,
				SIPTAG_TO_STR(sip_to_str),
				SIPTAG_FROM_STR(sofia_agent->sip_from_str),
				TAG_IF(sofia_agent->sip_contact_str,SIPTAG_CONTACT_STR(sofia_agent->sip_contact_str)),
				TAG_IF(settings->feature_tags,SIPTAG_ACCEPT_CONTACT_STR(settings->feature_tags)),
				TAG_END());
	sofia_session->nua_state = nua_callstate_init;

	apr_thread_mutex_create(&sofia_session->mutex,APR_THREAD_MUTEX_DEFAULT,session->pool);
	return TRUE;
}

static apt_bool_t mrcp_sofia_session_cleanup(mrcp_sofia_session_t *sofia_session)
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
	const char *local_sdp_str = NULL;
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
		sofia_session->descriptor = descriptor;
		apt_obj_log(APT_LOG_MARK,APT_PRIO_INFO,session->log_obj,"Local SDP "APT_NAMESID_FMT"\n%s", 
			session->name,
			MRCP_SESSION_SID(session), 
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
		mrcp_sofia_session_cleanup(sofia_session);
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
	const char *remote_sdp_str = NULL;
	mrcp_session_descriptor_t *descriptor = mrcp_session_descriptor_create(session->pool);
	descriptor->response_code = status;

	tl_gets(tags, 
			SOATAG_REMOTE_SDP_STR_REF(remote_sdp_str),
			TAG_END());

	if(remote_sdp_str) {
		sdp_parser_t *parser = NULL;
		sdp_session_t *sdp = NULL;
		const char *force_destination_ip = NULL;
		apt_obj_log(APT_LOG_MARK,APT_PRIO_INFO,session->log_obj,"Remote SDP "APT_NAMESID_FMT"\n%s",
			session->name,
			MRCP_SESSION_SID(session),
			remote_sdp_str);

		parser = sdp_parse(sofia_session->home,remote_sdp_str,(int)strlen(remote_sdp_str),0);
		sdp = sdp_session(parser);
		if(sofia_session->sip_settings->force_destination == TRUE) {
			force_destination_ip = sofia_session->sip_settings->server_ip;
		}

		mrcp_descriptor_generate_by_sdp_session(descriptor,sdp,force_destination_ip,session->pool);
		sdp_parser_free(parser);
	}

	mrcp_session_answer(session,descriptor);
}

static void mrcp_sofia_on_session_redirect(
						int                   status,
						mrcp_sofia_agent_t   *sofia_agent,
						nua_handle_t         *nh,
						mrcp_sofia_session_t *sofia_session,
						sip_t const          *sip,
						tagi_t                tags[])
{
	mrcp_session_t *session = sofia_session->session;
	sip_to_t *sip_to;
	sip_contact_t *sip_contact;
	if(!sip || !sip->sip_contact) {
		return;
	}
	sip_contact = sip->sip_contact;
	
	apr_thread_mutex_lock(sofia_session->mutex);

	sip_to = sip_to_create(sofia_session->home, (const url_string_t *) sip_contact->m_url); 

	apt_obj_log(APT_LOG_MARK,APT_PRIO_INFO,session->log_obj,"Redirect "APT_NAMESID_FMT" to "URL_PRINT_FORMAT,
		session->name,
		MRCP_SESSION_SID(session),
		URL_PRINT_ARGS(sip_to->a_url));

	if(sofia_session->nh) {
		nua_handle_bind(sofia_session->nh, NULL);
		nua_handle_destroy(sofia_session->nh);
		sofia_session->nh = NULL;
	}

	sofia_session->nh = nua_handle(
				sofia_agent->nua,
				sofia_session,
				SIPTAG_TO(sip_to),
				SIPTAG_FROM_STR(sofia_agent->sip_from_str),
				TAG_IF(sofia_agent->sip_contact_str,SIPTAG_CONTACT_STR(sofia_agent->sip_contact_str)),
				TAG_END());

	apr_thread_mutex_unlock(sofia_session->mutex);

	mrcp_sofia_session_offer(sofia_session->session,sofia_session->descriptor);
}

static void mrcp_sofia_on_session_terminate(
						int                   status,
						mrcp_sofia_agent_t   *sofia_agent,
						nua_handle_t         *nh,
						mrcp_sofia_session_t *sofia_session,
						sip_t const          *sip,
						tagi_t                tags[])
{
	mrcp_session_t *session;
	apt_bool_t terminate_requested;

	apr_thread_mutex_lock(sofia_session->mutex);
	terminate_requested = sofia_session->terminate_requested;
	session = sofia_session->session;
	mrcp_sofia_session_unref(sofia_session);
	apr_thread_mutex_unlock(sofia_session->mutex);

	if(terminate_requested == TRUE) {
		sofia_session->nua_state = nua_callstate_terminated;
		mrcp_sofia_session_cleanup(sofia_session);
		mrcp_session_terminate_response(session);
		return;
	}

	if(sofia_session->nua_state == nua_callstate_ready) {
		mrcp_session_terminate_event(session);
	}
	else {
		mrcp_session_descriptor_t *descriptor = mrcp_session_descriptor_create(session->pool);
		descriptor->response_code = status;
		mrcp_session_answer(session,descriptor);
	}
	sofia_session->nua_state = nua_callstate_terminated;
}

static void mrcp_sofia_on_state_change(
						int                   status,
						mrcp_sofia_agent_t   *sofia_agent,
						nua_handle_t         *nh,
						mrcp_sofia_session_t *sofia_session,
						sip_t const          *sip,
						tagi_t                tags[])
{
	int nua_state = nua_callstate_init;
	tl_gets(tags,
			NUTAG_CALLSTATE_REF(nua_state),
			TAG_END());
	
	if(!sofia_session || !sofia_session->session) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"SIP Call State [%s]", nua_callstate_name(nua_state));
		return;
	}
	apt_obj_log(APT_LOG_MARK,APT_PRIO_NOTICE,sofia_session->session->log_obj,"SIP Call State %s [%s]",
		sofia_session->session->name,
		nua_callstate_name(nua_state));

	if(nua_state == nua_callstate_terminated) {
		mrcp_sofia_on_session_terminate(status,sofia_agent,nh,sofia_session,sip,tags);
		return;
	}

	if(nua_state == nua_callstate_ready) {
		mrcp_sofia_on_session_ready(status,sofia_agent,nh,sofia_session,sip,tags);
	}
	sofia_session->nua_state = nua_state;
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
		mrcp_session_descriptor_t *descriptor = mrcp_session_descriptor_create(session->pool);
		descriptor->response_code = status;

		if(sip->sip_payload) {
			remote_sdp_str = sip->sip_payload->pl_data;
		}
		
		if(remote_sdp_str) {
			sdp_parser_t *parser = NULL;
			sdp_session_t *sdp = NULL;
			apt_obj_log(APT_LOG_MARK,APT_PRIO_INFO,session->obj,"Resource Discovery SDP %s\n%s", 
				session->name,
				remote_sdp_str);

			parser = sdp_parse(sofia_session->home,remote_sdp_str,(int)strlen(remote_sdp_str),0);
			sdp = sdp_session(parser);
			mrcp_descriptor_generate_by_sdp_session(descriptor,sdp,NULL,session->pool);
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
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive SIP Event [%s] Status %d %s [%s]",
		nua_event_name(nua_event),
		status,
		phrase,
		sofia_agent->sig_agent->id);

	switch(nua_event) {
		case nua_i_state:
			mrcp_sofia_on_state_change(status,sofia_agent,nh,sofia_session,sip,tags);
			break;
		case nua_r_invite:
			if(status >= 300 && status < 400) {
				mrcp_sofia_on_session_redirect(status,sofia_agent,nh,sofia_session,sip,tags);
			}
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
