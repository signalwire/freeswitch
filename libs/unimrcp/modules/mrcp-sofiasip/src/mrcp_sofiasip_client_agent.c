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
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/sofia_features.h>
#undef strcasecmp
#undef strncasecmp
#include <apr_general.h>

#include "mrcp_sofiasip_client_agent.h"
#include "mrcp_sofiasip_logger.h"
#include "mrcp_sofiasip_task.h"
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

	mrcp_sofia_task_t          *task;
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
static apt_bool_t mrcp_sofia_session_create(mrcp_session_t *session, const mrcp_sig_settings_t *settings, const mrcp_session_attribs_t *attribs);
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
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_sofiasip_client_agent_create(const char *id, mrcp_sofia_client_config_t *config, apr_pool_t *pool)
{
	apt_task_t *base;
	mrcp_sofia_agent_t *sofia_agent;

	sofia_agent = apr_palloc(pool,sizeof(mrcp_sofia_agent_t));
	sofia_agent->sig_agent = mrcp_signaling_agent_create(id,sofia_agent,pool);
	sofia_agent->sig_agent->create_client_session = mrcp_sofia_session_create;

	if(mrcp_sofia_config_validate(sofia_agent,config,pool) == FALSE) {
		return NULL;
	}

	apt_log(SIP_LOG_MARK,APT_PRIO_NOTICE,"Create SofiaSIP Agent [%s] ["SOFIA_SIP_VERSION"] %s",
			id,sofia_agent->sip_bind_str);
	sofia_agent->nua = NULL;
	sofia_agent->task = mrcp_sofia_task_create(mrcp_sofia_nua_create,sofia_agent,NULL,pool);
	if(!sofia_agent->task) {
		return NULL;
	}
	base = mrcp_sofia_task_base_get(sofia_agent->task);
	apt_task_name_set(base,id);
	sofia_agent->sig_agent->task = base;
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
	config->sip_timer_c = 0;

	config->tport_log = FALSE;
	config->tport_dump_file = NULL;

	return config;
}

static apt_bool_t mrcp_sofia_config_validate(mrcp_sofia_agent_t *sofia_agent, mrcp_sofia_client_config_t *config, apr_pool_t *pool)
{
	sofia_agent->config = config;
	if(!config || !config->local_ip) {
		return FALSE;
	}

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

static nua_t* mrcp_sofia_nua_create(void *obj, su_root_t *root)
{
	nua_t *nua;
	mrcp_sofia_agent_t *sofia_agent = obj;
	mrcp_sofia_client_config_t *sofia_config = sofia_agent->config;

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
		TAG_IF(sofia_config->sip_timer_c,NTATAG_TIMER_C(sofia_config->sip_timer_c)),
		SIPTAG_USER_AGENT_STR(sofia_config->user_agent_name),
		TAG_IF(sofia_config->tport_log == TRUE,TPTAG_LOG(1)), /* Print out SIP messages to the console */
		TAG_IF(sofia_config->tport_dump_file,TPTAG_DUMP(sofia_config->tport_dump_file)), /* Dump SIP messages to the file */
		TAG_END());                /* Last tag should always finish the sequence */
	sofia_agent->nua = nua;
	return nua;
}

static APR_INLINE mrcp_sofia_agent_t* mrcp_sofia_agent_get(mrcp_session_t *session)
{
	return session->signaling_agent->obj;
}

static const char* mrcp_sofia_feature_tag_append(const char *feature_tags, const char *resource_name, const char *key, const char *value, apr_pool_t *pool)
{
	if(feature_tags) {
		return apr_psprintf(pool,"%s;%s.%s=%s", feature_tags, resource_name, key, value);
	}

	return apr_psprintf(pool,"%s.%s=%s", resource_name, key, value);
}

static apt_bool_t mrcp_sofia_session_create(mrcp_session_t *session, const mrcp_sig_settings_t *settings, const mrcp_session_attribs_t *attribs)
{
	const char *sip_to_str = NULL;
	const char *user_name;
	const char *feature_tags;
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

	user_name = settings->user_name;
	feature_tags = settings->feature_tags;

	if(attribs && attribs->generic_attribs) {
		const apr_array_header_t *header = apr_table_elts(attribs->generic_attribs);
		apr_table_entry_t *entry = (apr_table_entry_t *)header->elts;
		int i;
		for(i=0; i<header->nelts; i++) {
			if(strcasecmp(entry[i].key,"sip.request.user-name") == 0) {
				user_name = entry[i].val;
			}
			else if(strcasecmp(entry[i].key,"sip.request.uri") == 0) {
				sip_to_str = entry[i].val;
			}
			else {
				feature_tags = mrcp_sofia_feature_tag_append(feature_tags,"generic",entry[i].key,entry[i].val,session->pool);
			}
		}
	}
	if(attribs && attribs->resource_attribs) {
		const apr_array_header_t *header;
		apr_table_entry_t *entry;
		int i;
		apr_table_t *table;
		const char *resource_name;
		apr_hash_index_t *it;
		const void *key;
		void *val;
		it = apr_hash_first(session->pool,attribs->resource_attribs);
		for(; it; it = apr_hash_next(it)) {
			apr_hash_this(it,&key,NULL,&val);
			resource_name = key;
			table = val;
			if(!table) continue;

			header = apr_table_elts(table);
			entry = (apr_table_entry_t *)header->elts;
			for(i=0; i<header->nelts; i++) {
				feature_tags = mrcp_sofia_feature_tag_append(feature_tags,resource_name,entry[i].key,entry[i].val,session->pool);
			}
		}
	}

	if(!sip_to_str) {
		if(user_name && *user_name != '\0') {
			sip_to_str = apr_psprintf(session->pool,"sip:%s@%s:%hu",
											user_name,
											settings->server_ip,
											settings->server_port);
		}
		else {
			sip_to_str = apr_psprintf(session->pool,"sip:%s:%hu",
											settings->server_ip,
											settings->server_port);
		}
	}

	sofia_session->nh = nua_handle(
				sofia_agent->nua,
				sofia_session,
				SIPTAG_TO_STR(sip_to_str),
				SIPTAG_FROM_STR(sofia_agent->sip_from_str),
				TAG_IF(sofia_agent->sip_contact_str,SIPTAG_CONTACT_STR(sofia_agent->sip_contact_str)),
				TAG_IF(feature_tags,SIPTAG_ACCEPT_CONTACT_STR(feature_tags)),
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
		apt_obj_log(SIP_LOG_MARK,APT_PRIO_INFO,session->log_obj,"Local SDP " APT_NAMESID_FMT "\n%s", 
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
		if (sofia_session->nua_state != nua_callstate_init) {
			sofia_session->terminate_requested = TRUE;
			nua_bye(sofia_session->nh, TAG_END());
		}
		else {
			mrcp_sofia_session_unref(sofia_session);
		}
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
		apt_obj_log(SIP_LOG_MARK,APT_PRIO_INFO,session->log_obj,"Remote SDP " APT_NAMESID_FMT "\n%s",
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

	apt_obj_log(SIP_LOG_MARK,APT_PRIO_INFO,session->log_obj,"Redirect " APT_NAMESID_FMT " to " URL_PRINT_FORMAT,
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
		apt_log(SIP_LOG_MARK,APT_PRIO_WARNING,"SIP Call State [%s]", nua_callstate_name(nua_state));
		return;
	}
	apt_obj_log(SIP_LOG_MARK,APT_PRIO_NOTICE,sofia_session->session->log_obj,"SIP Call State %s [%s]",
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
			apt_obj_log(SIP_LOG_MARK,APT_PRIO_INFO,session->obj,"Resource Discovery SDP %s\n%s", 
				session->name,
				remote_sdp_str);

			parser = sdp_parse(sofia_session->home,remote_sdp_str,(int)strlen(remote_sdp_str),0);
			sdp = sdp_session(parser);
			mrcp_resource_discovery_generate_by_sdp_session(descriptor,sdp,session->pool);
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
	apt_log(SIP_LOG_MARK,APT_PRIO_INFO,"Receive SIP Event [%s] Status %d %s [%s]",
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
				mrcp_sofia_task_break(sofia_agent->task);
			}
			break;
		default:
			break;
	}
}
