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
 * $Id: mrcp_sofiasip_server_agent.c 2250 2014-11-19 05:41:12Z achaloyan@gmail.com $
 */

typedef struct mrcp_sofia_agent_t mrcp_sofia_agent_t;
#define NUA_MAGIC_T mrcp_sofia_agent_t

typedef struct mrcp_sofia_session_t mrcp_sofia_session_t;
#define NUA_HMAGIC_T mrcp_sofia_session_t

#include <sofia-sip/su.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/sofia_features.h>
#undef strcasecmp
#undef strncasecmp
#include <apr_general.h>

#include "mrcp_sofiasip_server_agent.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_sdp.h"
#include "apt_log.h"

struct mrcp_sofia_agent_t {
	mrcp_sig_agent_t           *sig_agent;

	mrcp_sofia_server_config_t *config;
	char                       *sip_contact_str;
	char                       *sip_bind_str;

	su_root_t                  *root;
	nua_t                      *nua;
};

struct mrcp_sofia_session_t {
	mrcp_session_t *session;
	su_home_t      *home;
	nua_handle_t   *nh;
};

/* Task Interface */
static void mrcp_sofia_task_initialize(apt_task_t *task);
static apt_bool_t mrcp_sofia_task_run(apt_task_t *task);
static apt_bool_t mrcp_sofia_task_terminate(apt_task_t *task);

/* MRCP Signaling Interface */
static apt_bool_t mrcp_sofia_on_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_sofia_on_session_terminate(mrcp_session_t *session);

static const mrcp_session_response_vtable_t session_response_vtable = {
	mrcp_sofia_on_session_answer,
	mrcp_sofia_on_session_terminate,
	NULL /* mrcp_sofia_on_session_control */,
	NULL /* mrcp_sofia_on_session_discover */
};

static apt_bool_t mrcp_sofia_config_validate(mrcp_sofia_agent_t *sofia_agent, mrcp_sofia_server_config_t *config, apr_pool_t *pool);

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
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_sofiasip_server_agent_create(const char *id, mrcp_sofia_server_config_t *config, apr_pool_t *pool)
{
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	mrcp_sofia_agent_t *sofia_agent;
	sofia_agent = apr_palloc(pool,sizeof(mrcp_sofia_agent_t));
	sofia_agent->sig_agent = mrcp_signaling_agent_create(id,sofia_agent,pool);
	sofia_agent->config = config;
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
MRCP_DECLARE(mrcp_sofia_server_config_t*) mrcp_sofiasip_server_config_alloc(apr_pool_t *pool)
{
	mrcp_sofia_server_config_t *config = apr_palloc(pool,sizeof(mrcp_sofia_server_config_t));
	config->local_ip = NULL;
	config->ext_ip = NULL;
	config->local_port = 0;
	config->user_name = NULL;
	config->user_agent_name = NULL;
	config->origin = NULL;
	config->transport = NULL;
	config->force_destination = FALSE;
	config->sip_t1 = 0;
	config->sip_t2 = 0;
	config->sip_t4 = 0;
	config->sip_t1x64 = 0;

	config->tport_log = FALSE;
	config->tport_dump_file = NULL;

	return config;
}

MRCP_DECLARE(apt_bool_t) mrcp_sofiasip_server_logger_init(const char *name, const char *level_str, apt_bool_t redirect)
{
	return mrcp_sofiasip_log_init(name,level_str,redirect);
}

static apt_bool_t mrcp_sofia_config_validate(mrcp_sofia_agent_t *sofia_agent, mrcp_sofia_server_config_t *config, apr_pool_t *pool)
{
	sofia_agent->config = config;
	sofia_agent->sip_contact_str = NULL; /* Let Sofia-SIP implicitly set Contact header by default */
	if(config->ext_ip) {
		/* Use external IP address in Contact header, if behind NAT */
		sofia_agent->sip_contact_str = apr_psprintf(pool,"sip:%s:%hu",config->ext_ip,config->local_port);
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
	mrcp_sofia_server_config_t *sofia_config = sofia_agent->config;

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

static mrcp_sofia_session_t* mrcp_sofia_session_create(mrcp_sofia_agent_t *sofia_agent, nua_handle_t *nh)
{
	mrcp_sofia_session_t *sofia_session;
	mrcp_session_t* session = sofia_agent->sig_agent->create_server_session(sofia_agent->sig_agent);
	if(!session) {
		return NULL;
	}
	session->response_vtable = &session_response_vtable;
	session->event_vtable = NULL;

	sofia_session = apr_palloc(session->pool,sizeof(mrcp_sofia_session_t));
	sofia_session->home = su_home_new(sizeof(*sofia_session->home));
	sofia_session->session = session;
	session->obj = sofia_session;
	
	nua_handle_bind(nh, sofia_session);
	sofia_session->nh = nh;
	return sofia_session;
}

static int sip_status_get(mrcp_session_status_e status)
{
	switch (status) {
		case MRCP_SESSION_STATUS_OK:
			return 200;
		case MRCP_SESSION_STATUS_NO_SUCH_RESOURCE:
			return 404;
		case MRCP_SESSION_STATUS_UNACCEPTABLE_RESOURCE:
			return 406;
		case MRCP_SESSION_STATUS_UNAVAILABLE_RESOURCE:
			return 480;
		case MRCP_SESSION_STATUS_ERROR:
			return 500;
	}
	return 200;
}

static apt_bool_t mrcp_sofia_on_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_sofia_session_t *sofia_session = session->obj;
	mrcp_sofia_agent_t *sofia_agent = session->signaling_agent->obj;
	const char *local_sdp_str = NULL;
	char sdp_str[2048];

	if(!sofia_agent || !sofia_session || !sofia_session->nh) {
		return FALSE;
	}

	if(descriptor->status != MRCP_SESSION_STATUS_OK) {
		int status = sip_status_get(descriptor->status);
		nua_respond(sofia_session->nh, status, sip_status_phrase(status),
					TAG_IF(sofia_agent->sip_contact_str,SIPTAG_CONTACT_STR(sofia_agent->sip_contact_str)),
					TAG_END());
		return TRUE;
	}

	if(sofia_agent->config->origin) {
		apt_string_set(&descriptor->origin,sofia_agent->config->origin);
	}

	if(sdp_string_generate_by_mrcp_descriptor(sdp_str,sizeof(sdp_str),descriptor,FALSE) > 0) {
		local_sdp_str = sdp_str;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Local SDP "APT_NAMESID_FMT"\n%s", 
			session->name,
			MRCP_SESSION_SID(session), 
			local_sdp_str);
	}

	nua_respond(sofia_session->nh, SIP_200_OK, 
				TAG_IF(sofia_agent->sip_contact_str,SIPTAG_CONTACT_STR(sofia_agent->sip_contact_str)),
				TAG_IF(local_sdp_str,SOATAG_USER_SDP_STR(local_sdp_str)),
				SOATAG_AUDIO_AUX("telephone-event"),
				NUTAG_AUTOANSWER(0),
				TAG_END());
	
	return TRUE;
}

static apt_bool_t mrcp_sofia_on_session_terminate(mrcp_session_t *session)
{
	mrcp_sofia_session_t *sofia_session = session->obj;
	if(sofia_session) {
		if(sofia_session->nh) {
			nua_handle_bind(sofia_session->nh, NULL);
			nua_handle_destroy(sofia_session->nh);
		}
		if(sofia_session->home) {
			su_home_unref(sofia_session->home);
			sofia_session->home = NULL;
		}
		sofia_session->session = NULL;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy Session "APT_SID_FMT, MRCP_SESSION_SID(session));
	mrcp_session_destroy(session);
	return TRUE;
}

static void mrcp_sofia_on_call_receive(mrcp_sofia_agent_t   *sofia_agent,
									   nua_handle_t         *nh,
									   mrcp_sofia_session_t *sofia_session,
									   sip_t const          *sip,
									   tagi_t                tags[])
{
	apt_bool_t status = FALSE;
	const char *remote_sdp_str = NULL;
	mrcp_session_descriptor_t *descriptor;

	if(!sofia_session) {
		sofia_session = mrcp_sofia_session_create(sofia_agent,nh);
		if(!sofia_session) {
			nua_respond(nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
			return;
		}
	}

	descriptor = mrcp_session_descriptor_create(sofia_session->session->pool);

	tl_gets(tags, 
			SOATAG_REMOTE_SDP_STR_REF(remote_sdp_str),
			TAG_END());

	if(remote_sdp_str) {
		sdp_parser_t *parser = NULL;
		sdp_session_t *sdp = NULL;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remote SDP "APT_NAMESID_FMT"\n%s",
			sofia_session->session->name,
			MRCP_SESSION_SID(sofia_session->session),
			remote_sdp_str);

		parser = sdp_parse(sofia_session->home,remote_sdp_str,(int)strlen(remote_sdp_str),0);
		sdp = sdp_session(parser);
		status = mrcp_descriptor_generate_by_sdp_session(descriptor,sdp,NULL,sofia_session->session->pool);
		sdp_parser_free(parser);
	}

	if(status == FALSE) {
		nua_respond(nh, SIP_400_BAD_REQUEST, TAG_END());
		return;
	}

	mrcp_session_offer(sofia_session->session,descriptor);
}

static void mrcp_sofia_on_call_terminate(mrcp_sofia_agent_t          *sofia_agent,
									     nua_handle_t                *nh,
									     mrcp_sofia_session_t        *sofia_session,
									     sip_t const                 *sip,
									     tagi_t                       tags[])
{
	if(sofia_session) {
		mrcp_session_terminate_request(sofia_session->session);
	}
}

static void mrcp_sofia_on_state_change(mrcp_sofia_agent_t   *sofia_agent,
									   nua_handle_t         *nh,
									   mrcp_sofia_session_t *sofia_session,
									   sip_t const          *sip,
									   tagi_t                tags[])
{
	int nua_state = nua_callstate_init;
	tl_gets(tags, 
			NUTAG_CALLSTATE_REF(nua_state),
			TAG_END()); 
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"SIP Call State %s [%s]",
		sofia_session ? sofia_session->session->name : "",
		nua_callstate_name(nua_state));

	switch(nua_state) {
		case nua_callstate_received:
			mrcp_sofia_on_call_receive(sofia_agent,nh,sofia_session,sip,tags);
			break;
		case nua_callstate_terminated:
			mrcp_sofia_on_call_terminate(sofia_agent,nh,sofia_session,sip,tags);
			break;
	}
}

static void mrcp_sofia_on_resource_discover(mrcp_sofia_agent_t   *sofia_agent,
									        nua_handle_t         *nh,
									        mrcp_sofia_session_t *sofia_session,
									        sip_t const          *sip,
									        tagi_t                tags[])
{
	char sdp_str[2048];
	const char *local_sdp_str = NULL;

	const char *ip = sofia_agent->config->ext_ip ? 
		sofia_agent->config->ext_ip : sofia_agent->config->local_ip;

	if(sdp_resource_discovery_string_generate(ip,sofia_agent->config->origin,sdp_str,sizeof(sdp_str)) > 0) {
		local_sdp_str = sdp_str;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Resource Discovery SDP\n[%s]\n", 
				local_sdp_str);
	}

	nua_respond(nh, SIP_200_OK, 
				NUTAG_WITH_CURRENT(sofia_agent->nua),
				TAG_IF(sofia_agent->sip_contact_str,SIPTAG_CONTACT_STR(sofia_agent->sip_contact_str)),
				TAG_IF(local_sdp_str,SOATAG_USER_SDP_STR(local_sdp_str)),
				SOATAG_AUDIO_AUX("telephone-event"),
				TAG_END());
}

/** This callback will be called by SIP stack to process incoming events */
static void mrcp_sofia_event_callback( nua_event_t           nua_event,
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
			mrcp_sofia_on_state_change(sofia_agent,nh,sofia_session,sip,tags);
			break;
		case nua_i_options:
			mrcp_sofia_on_resource_discover(sofia_agent,nh,sofia_session,sip,tags);
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
