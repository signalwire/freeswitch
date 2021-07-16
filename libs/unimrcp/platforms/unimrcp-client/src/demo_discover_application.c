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

/* 
 * Demo resource discovery.
 * C -> S: SIP OPTIONS or RTPS DESCRIBE
 * S -> C: SIP OK or RTPS OK
 */

#include "demo_application.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_control_descriptor.h"
#include "apt_log.h"


/** Declaration of synthesizer application methods */
static apt_bool_t discover_application_run(demo_application_t *demo_application, const char *profile);
static apt_bool_t discover_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message);

/** Declaration of application message handlers */
static apt_bool_t discover_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t discover_application_on_resource_discover(mrcp_application_t *application, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor, mrcp_sig_status_code_e status);

static const mrcp_app_message_dispatcher_t discover_application_dispatcher = {
	NULL,
	discover_application_on_session_terminate,
	NULL,
	NULL,
	NULL,
	NULL,
	discover_application_on_resource_discover
};

/** Create demo resource discover application */
demo_application_t* demo_discover_application_create(apr_pool_t *pool)
{
	demo_application_t *discover_application = apr_palloc(pool,sizeof(demo_application_t));
	discover_application->application = NULL;
	discover_application->framework = NULL;
	discover_application->handler = discover_application_handler;
	discover_application->run = discover_application_run;
	return discover_application;
}

/** Run demo resource discover scenario */
static apt_bool_t discover_application_run(demo_application_t *demo_application, const char *profile)
{
	/* create session */
	mrcp_session_t *session = mrcp_application_session_create(demo_application->application,profile,NULL);
	if(!session) {
		return FALSE;
	}
	
	/* send resource discover request */
	if(mrcp_application_resource_discover(session) != TRUE) {
		mrcp_application_session_destroy(session);
		return FALSE;
	}

	return TRUE;
}

/** Handle the messages sent from the MRCP client stack */
static apt_bool_t discover_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message)
{
	/* app_message should be dispatched now,
	*  the default dispatcher is used in demo. */
	return mrcp_application_message_dispatch(&discover_application_dispatcher,app_message);
}

/** Handle the responses sent to session terminate requests */
static apt_bool_t discover_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	/* received response to session termination request,
	now it's safe to destroy no more referenced session */
	mrcp_application_session_destroy(session);
	return TRUE;
}

/** Handle the responses sent to resource discover requests */
static apt_bool_t discover_application_on_resource_discover(mrcp_application_t *application, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor, mrcp_sig_status_code_e status)
{
	if(descriptor && status == MRCP_SIG_STATUS_CODE_SUCCESS) {
		int i;
		int count = descriptor->control_media_arr->nelts;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"On Resource Discover [%d]", count);

		for(i = 0; i < count; i++) {
			mrcp_control_descriptor_t *control_media = mrcp_session_control_media_get(descriptor,i);
			if(control_media) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"[%d] - %s", i,control_media->resource_name.buf);
			}
		}
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Failed to Discover Resources");
	}
	
	mrcp_application_session_terminate(session);
	return TRUE;
}
