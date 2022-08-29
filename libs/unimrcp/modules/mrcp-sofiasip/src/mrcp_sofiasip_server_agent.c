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

typedef struct mrcp_sofia_agent_t mrcp_sofia_agent_t;
#define NUA_MAGIC_T mrcp_sofia_agent_t

typedef struct mrcp_sofia_session_t mrcp_sofia_session_t;
#define NUA_HMAGIC_T mrcp_sofia_session_t

#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/sofia_features.h>
#undef strcasecmp
#undef strncasecmp
#include <apr_general.h>

#include "mrcp_sofiasip_server_agent.h"
#include "mrcp_sofiasip_logger.h"
#include "mrcp_sofiasip_task.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_sdp.h"
#include "apt_text_stream.h"
#include "apt_log.h"

struct mrcp_sofia_agent_t {
	mrcp_sig_agent_t           *sig_agent;
	mrcp_sofia_server_config_t *config;
	char                       *sip_contact_str;
	char                       *sip_bind_str;

	mrcp_sofia_task_t          *task;
	apt_bool_t                  online;
};

struct mrcp_sofia_session_t {
	mrcp_session_t             *session;
	su_home_t                  *home;
	nua_handle_t               *nh;

	mrcp_session_attribs_t      attribs;
};

/* Task Interface */
static void mrcp_sofia_task_on_offline(apt_task_t *base);
static void mrcp_sofia_task_on_online(apt_task_t *base);

/* MRCP Signaling Interface */
static apt_bool_t mrcp_sofia_on_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_sofia_on_session_terminate(mrcp_session_t *session);

static const mrcp_session_response_vtable_t session_response_vtable = {
	mrcp_sofia_on_session_answer,
	mrcp_sofia_on_session_terminate,
	NULL /* mrcp_sofia_on_session_control */,
	NULL /* mrcp_sofia_on_session_discover */
};

static apt_bool_t mrcp_sofia_on_session_terminate_event(mrcp_session_t *session);

static const mrcp_session_event_vtable_t session_event_vtable = {
	mrcp_sofia_on_session_terminate_event
};

static apt_bool_t mrcp_sofia_config_validate(mrcp_sofia_agent_t *sofia_agent, mrcp_sofia_server_config_t *config, apr_pool_t *pool);
static nua_t* mrcp_sofia_nua_create(void *obj, su_root_t *root);

static void mrcp_sofia_event_callback(
					nua_event_t           nua_event,
					int                   status,
					char const           *phrase,
					nua_t                *nua,
					mrcp_sofia_agent_t   *sofia_agent,
					nua_handle_t         *nh,
					mrcp_sofia_session_t *sofia_session,
					sip_t const          *sip,
					tagi_t                tags[]);

/** Create Sofia-SIP Signaling Agent */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_sofiasip_server_agent_create(const char *id, mrcp_sofia_server_config_t *config, apr_pool_t *pool)
{
	apt_task_t *base;
	apt_task_vtable_t *vtable;
	mrcp_sofia_agent_t *sofia_agent;
	
	sofia_agent = apr_palloc(pool,sizeof(mrcp_sofia_agent_t));
	sofia_agent->sig_agent = mrcp_signaling_agent_create(id,sofia_agent,pool);
	sofia_agent->config = config;

	if(mrcp_sofia_config_validate(sofia_agent,config,pool) == FALSE) {
		return NULL;
	}

	apt_log(SIP_LOG_MARK,APT_PRIO_NOTICE,"Create SofiaSIP Agent [%s] ["SOFIA_SIP_VERSION"] %s",
				id,sofia_agent->sip_bind_str);
	sofia_agent->task = mrcp_sofia_task_create(mrcp_sofia_nua_create,sofia_agent,NULL,pool);
	if(!sofia_agent->task) {
		return NULL;
	}
	sofia_agent->online = TRUE;
	base = mrcp_sofia_task_base_get(sofia_agent->task);
	apt_task_name_set(base,id);
	vtable = apt_task_vtable_get(base);
	if(vtable) {
		vtable->on_offline_complete = mrcp_sofia_task_on_offline;
		vtable->on_online_complete = mrcp_sofia_task_on_online;
	}
	sofia_agent->sig_agent->task = base;
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
	config->disable_soa = FALSE;
	config->extract_feature_tags = TRUE;
	config->extract_call_id = FALSE;
	config->extract_user_name = FALSE;
	config->sip_t1 = 0;
	config->sip_t2 = 0;
	config->sip_t4 = 0;
	config->sip_t1x64 = 0;
	config->session_expires = 600; /* sec */
	config->min_session_expires = 120; /* sec */

	config->tport_log = FALSE;
	config->tport_dump_file = NULL;

	return config;
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

static void mrcp_sofia_task_on_offline(apt_task_t *base)
{
	mrcp_sofia_task_t *task = apt_task_object_get(base);
	mrcp_sofia_agent_t *agent = mrcp_sofia_task_object_get(task);

	agent->online = FALSE;
}

static void mrcp_sofia_task_on_online(apt_task_t *base)
{
	mrcp_sofia_task_t *task = apt_task_object_get(base);
	mrcp_sofia_agent_t *agent = mrcp_sofia_task_object_get(task);

	agent->online = TRUE;
}

static nua_t* mrcp_sofia_nua_create(void *obj, su_root_t *root)
{
	nua_t *nua;
	mrcp_sofia_agent_t *sofia_agent = obj;
	mrcp_sofia_server_config_t *sofia_config = sofia_agent->config;

	/* Create a user agent instance. The stack will call the 'event_callback()' 
	 * callback when events such as succesful registration to network, 
	 * an incoming call, etc, occur. 
	 */
	nua = nua_create(
		root,                      /* Event loop */
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

	return nua;
}

static mrcp_sofia_session_t* mrcp_sofia_session_create(mrcp_sofia_agent_t *sofia_agent, nua_handle_t *nh)
{
	mrcp_sofia_session_t *sofia_session;
	mrcp_session_t* session = sofia_agent->sig_agent->create_server_session(sofia_agent->sig_agent);
	if(!session) {
		return NULL;
	}
	session->response_vtable = &session_response_vtable;
	session->event_vtable = &session_event_vtable;

	sofia_session = apr_palloc(session->pool,sizeof(mrcp_sofia_session_t));
	sofia_session->home = su_home_new(sizeof(*sofia_session->home));
	sofia_session->session = session;
	session->obj = sofia_session;
	
	nua_handle_bind(nh, sofia_session);
	sofia_session->nh = nh;

	mrcp_session_attribs_init(&sofia_session->attribs);
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
		apt_log(SIP_LOG_MARK,APT_PRIO_INFO,"Local SDP " APT_NAMESID_FMT "\n%s", 
			session->name,
			MRCP_SESSION_SID(session), 
			local_sdp_str);
	}

	nua_respond(sofia_session->nh, SIP_200_OK,
				TAG_IF(sofia_agent->sip_contact_str,SIPTAG_CONTACT_STR(sofia_agent->sip_contact_str)),
				TAG_IF(sofia_agent->config->disable_soa && local_sdp_str,SIPTAG_CONTENT_TYPE_STR("application/sdp")),
				TAG_IF(sofia_agent->config->disable_soa && local_sdp_str,SIPTAG_PAYLOAD_STR(local_sdp_str)),
				TAG_IF(!sofia_agent->config->disable_soa && local_sdp_str,SOATAG_USER_SDP_STR(local_sdp_str)),
				SOATAG_AUDIO_AUX("telephone-event"),
				NUTAG_AUTOANSWER(0),
				TAG_IF(sofia_agent->config->disable_soa,NUTAG_MEDIA_ENABLE(0)),
				NUTAG_SESSION_TIMER(sofia_agent->config->session_expires),
				NUTAG_MIN_SE(sofia_agent->config->min_session_expires),
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

	apt_log(SIP_LOG_MARK,APT_PRIO_NOTICE,"Destroy Session " APT_SID_FMT, MRCP_SESSION_SID(session));
	mrcp_session_destroy(session);
	return TRUE;
}

static apt_bool_t mrcp_sofia_on_session_terminate_event(mrcp_session_t *session)
{
	mrcp_sofia_session_t *sofia_session = session->obj;
	if(sofia_session) {
		if(sofia_session->nh) {
			apt_log(SIP_LOG_MARK,APT_PRIO_NOTICE,"Initiate Session Termination " APT_SID_FMT, MRCP_SESSION_SID(session));
			nua_bye(sofia_session->nh, TAG_END());
		}
	}

	return TRUE;
}

static void mrcp_sofia_on_call_receive(
							mrcp_sofia_agent_t   *sofia_agent,
							nua_handle_t         *nh,
							mrcp_sofia_session_t *sofia_session,
							sip_t const          *sip,
							tagi_t                tags[])
{
	apt_bool_t status = FALSE;
	const char *remote_sdp_str = NULL;
	mrcp_session_descriptor_t *descriptor;

	if(sofia_agent->online == FALSE) {
		apt_log(SIP_LOG_MARK,APT_PRIO_WARNING,"Cannot Establish SIP Session in Offline Mode");
		nua_respond(nh, SIP_503_SERVICE_UNAVAILABLE, TAG_END());
		return;
	}

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
		apt_log(SIP_LOG_MARK,APT_PRIO_INFO,"Remote SDP " APT_NAMESID_FMT "\n%s",
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

	descriptor->attribs = sofia_session->attribs;

	mrcp_session_offer(sofia_session->session,descriptor);
}

static void mrcp_sofia_on_call_terminate(
							mrcp_sofia_agent_t          *sofia_agent,
							nua_handle_t                *nh,
							mrcp_sofia_session_t        *sofia_session,
							sip_t const                 *sip,
							tagi_t                       tags[])
{
	if(sofia_session) {
		mrcp_session_terminate_request(sofia_session->session);
	}
}

static apt_bool_t mrcp_sofia_feature_tag_extract(const apt_str_t *feature_tag, apt_str_t *resource_name, apt_str_t *field, apt_str_t *value)
{
	apt_text_stream_t stream;
	apt_text_stream_init(&stream,feature_tag->buf,feature_tag->length);
	if(apt_text_field_read(&stream,'.',TRUE,resource_name) == FALSE) {
		return FALSE;
	}

	if(apt_text_field_read(&stream,'=',TRUE,field) == FALSE) {
		return FALSE;
	}

	if(apt_text_field_read(&stream,';',TRUE,value) == TRUE) {
		/* check and remove double quotes, if present */
		if(value->buf[0] == '"' && value->buf[value->length - 1] == '"') {
			value->buf++;
			value->length -= 2;
		}
	}
	return TRUE;
}

static void mrcp_sofia_feature_tags_extract(mrcp_sofia_session_t *sofia_session, sip_accept_contact_t *sip_accept_contact)
{
	msg_param_t const *param = sip_accept_contact->cp_params;
	while(param && *param) {
		apt_str_t resource_name;
		apt_str_t field;
		apt_str_t value;
		apt_str_t feature_tag;

		apt_string_set(&feature_tag, *param);
		apt_log(SIP_LOG_MARK, APT_PRIO_INFO, "Extract Feature Tag [%s] " APT_NAMESID_FMT, 
				feature_tag.buf,
				sofia_session->session->name,
				MRCP_SESSION_SID(sofia_session->session));

		/* extract resource name, field and value from feature tag */
		if(mrcp_sofia_feature_tag_extract(&feature_tag,&resource_name,&field,&value) == TRUE) {
			if(strncasecmp(resource_name.buf,"generic",resource_name.length) == 0) {
				/* set session generic attribute */
				mrcp_session_generic_attrib_set(&sofia_session->attribs,&field,&value,sofia_session->session->pool);
			}
			else {
				/* set session resource-specific attribute */
				mrcp_session_resource_attrib_set(&sofia_session->attribs,&resource_name,&field,&value,sofia_session->session->pool);
			}
		}
		else {
			apt_log(SIP_LOG_MARK, APT_PRIO_WARNING, "Failed to Extract Feature Tag [%s] " APT_NAMESID_FMT, 
					feature_tag.buf,
					sofia_session->session->name,
					MRCP_SESSION_SID(sofia_session->session));
		}

		param += 1;
	}
}

static void mrcp_sofia_sip_call_id_extract(mrcp_sofia_session_t *sofia_session, sip_call_id_t *sip_call_id)
{
	apt_str_t field;
	apt_str_t value;
	apt_string_set(&field,"sip.header.call-id");
	apt_string_set(&value,sip_call_id->i_id);
	/* set session generic attribute */
	mrcp_session_generic_attrib_set(&sofia_session->attribs,&field,&value,sofia_session->session->pool);
}

static void mrcp_sofia_sip_user_name_extract(mrcp_sofia_session_t *sofia_session, sip_request_t *sip_request)
{
	apt_str_t field;
	apt_str_t value;
	apt_string_set(&field,"sip.request.user-name");
	if(sip_request->rq_url) {
		apt_string_set(&value,sip_request->rq_url->url_user);
	}
	/* set session generic attribute */
	mrcp_session_generic_attrib_set(&sofia_session->attribs,&field,&value,sofia_session->session->pool);
}

static void mrcp_sofia_on_invite(
							mrcp_sofia_agent_t   *sofia_agent,
							nua_handle_t         *nh,
							mrcp_sofia_session_t *sofia_session,
							sip_t const          *sip,
							tagi_t                tags[])
{
	if(!sofia_session && sip) {
		if((sofia_agent->config->extract_feature_tags == TRUE && sip->sip_accept_contact) ||
			(sofia_agent->config->extract_call_id == TRUE && sip->sip_call_id) ||
			(sofia_agent->config->extract_user_name == TRUE && sip->sip_request)) {
			/* create sofia session */
			sofia_session = mrcp_sofia_session_create(sofia_agent,nh);
			if(!sofia_session) {
				nua_respond(nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
				return;
			}
		}

		if(sofia_agent->config->extract_feature_tags == TRUE && sip->sip_accept_contact) {
			mrcp_sofia_feature_tags_extract(sofia_session,sip->sip_accept_contact);
		}
		if(sofia_agent->config->extract_call_id == TRUE && sip->sip_call_id) {
			mrcp_sofia_sip_call_id_extract(sofia_session,sip->sip_call_id);
		}
		if(sofia_agent->config->extract_user_name == TRUE && sip->sip_request) {
			mrcp_sofia_sip_user_name_extract(sofia_session,sip->sip_request);
		}
	}
}

static void mrcp_sofia_on_state_change(
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
	
	apt_log(SIP_LOG_MARK,APT_PRIO_NOTICE,"SIP Call State %s [%s]",
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

static void mrcp_sofia_on_resource_discover(
							mrcp_sofia_agent_t   *sofia_agent,
							nua_handle_t         *nh,
							mrcp_sofia_session_t *sofia_session,
							sip_t const          *sip,
							tagi_t                tags[])
{
	char sdp_str[2048];
	const char *local_sdp_str = NULL;

	const char *ip = sofia_agent->config->ext_ip ? 
		sofia_agent->config->ext_ip : sofia_agent->config->local_ip;
	nua_t *nua = mrcp_sofia_task_nua_get(sofia_agent->task);

	if (sofia_agent->online == FALSE) {
		apt_log(SIP_LOG_MARK, APT_PRIO_WARNING, "Cannot do Resource Discovery in Offline Mode");
		nua_respond(nh, SIP_503_SERVICE_UNAVAILABLE, TAG_END());
		return;
	}

	if(sdp_resource_discovery_string_generate(ip,sofia_agent->config->origin,sdp_str,sizeof(sdp_str)) > 0) {
		local_sdp_str = sdp_str;
		apt_log(SIP_LOG_MARK,APT_PRIO_INFO,"Resource Discovery SDP\n[%s]\n", 
				local_sdp_str);
	}

	nua_respond(nh, SIP_200_OK, 
				NUTAG_WITH_CURRENT(nua),
				TAG_IF(sofia_agent->sip_contact_str,SIPTAG_CONTACT_STR(sofia_agent->sip_contact_str)),
				TAG_IF(sofia_agent->config->disable_soa && local_sdp_str,SIPTAG_CONTENT_TYPE_STR("application/sdp")),
				TAG_IF(sofia_agent->config->disable_soa && local_sdp_str,SIPTAG_PAYLOAD_STR(local_sdp_str)),
				TAG_IF(!sofia_agent->config->disable_soa && local_sdp_str,SOATAG_USER_SDP_STR(local_sdp_str)),
				SOATAG_AUDIO_AUX("telephone-event"),
				TAG_IF(sofia_agent->config->disable_soa,NUTAG_MEDIA_ENABLE(0)),
				TAG_END());

	nua_handle_destroy(nh);
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
	apt_log(SIP_LOG_MARK,APT_PRIO_INFO,"Receive SIP Event [%s] Status %d %s [%s]",
		nua_event_name(nua_event),
		status,
		phrase,
		sofia_agent->sig_agent->id);

	switch(nua_event) {
		case nua_i_invite:
			mrcp_sofia_on_invite(sofia_agent,nh,sofia_session,sip,tags);
			break;
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
				mrcp_sofia_task_break(sofia_agent->task);
			}
			break;
		default:
			break;
	}
}
