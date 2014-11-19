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
 * $Id: demo_framework.c 2233 2014-11-12 01:34:59Z achaloyan@gmail.com $
 */

#include <apr_hash.h>
#include "demo_framework.h"
#include "demo_application.h"
#include "unimrcp_client.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

#define FRAMEWORK_TASK_NAME "Framework Agent"

#define MAX_APP_NAME_LENGTH     16
#define MAX_PROFILE_NAME_LENGTH 16

/** Demo framework */
struct demo_framework_t {
	/** MRCP client stack instance */
	mrcp_client_t       *client;
	/** Message processing task */
	apt_consumer_task_t *task;
	/** Table of demo applications */
	apr_hash_t          *application_table;
	/** Memory to allocate memory from */
	apr_pool_t          *pool;
};

typedef struct framework_task_data_t framework_task_data_t;
struct framework_task_data_t {
	char                      app_name[MAX_APP_NAME_LENGTH];
	char                      profile_name[MAX_PROFILE_NAME_LENGTH];
	demo_application_t       *demo_application;
	const mrcp_app_message_t *app_message;
};

typedef enum {
	DEMO_APPLICATION_MSG_ID,
	DEMO_CONSOLE_MSG_ID
} framework_msg_type_e;

static apt_bool_t demo_framework_message_handler(const mrcp_app_message_t *app_message);
static apt_bool_t demo_framework_consumer_task_create(demo_framework_t *framework);
static apt_bool_t demo_framework_app_register(demo_framework_t *framework, demo_application_t *demo_application, const char *name);

/** Create demo framework */
demo_framework_t* demo_framework_create(apt_dir_layout_t *dir_layout)
{
	demo_framework_t *framework = NULL;
	mrcp_client_t *client = unimrcp_client_create(dir_layout);
	if(client) {
		demo_application_t *demo_application;
		apr_pool_t *pool = mrcp_client_memory_pool_get(client);
		/* create demo framework */
		framework = apr_palloc(pool,sizeof(demo_framework_t));
		framework->pool = pool;
		framework->client = client;
		framework->application_table = apr_hash_make(pool);

		/* create demo synthesizer application */
		demo_application = demo_synth_application_create(framework->pool);
		if(demo_application) {
			demo_framework_app_register(framework,demo_application,"synth");
		}

		/* create demo recognizer application */
		demo_application = demo_recog_application_create(framework->pool);
		if(demo_application) {
			demo_framework_app_register(framework,demo_application,"recog");
		}

		/* create demo bypass media application */
		demo_application = demo_bypass_application_create(framework->pool);
		if(demo_application) {
			demo_framework_app_register(framework,demo_application,"bypass");
		}

		/* create demo resource discover application */
		demo_application = demo_discover_application_create(framework->pool);
		if(demo_application) {
			demo_framework_app_register(framework,demo_application,"discover");
		}

		demo_framework_consumer_task_create(framework);

		if(framework->task) {
			apt_task_t *task = apt_consumer_task_base_get(framework->task);
			apt_task_start(task);
		}
		
		/* start client stack */
		mrcp_client_start(client);
	}

	return framework;
}

/** Run demo application */
apt_bool_t demo_framework_app_run(demo_framework_t *framework, const char *app_name, const char *profile_name)
{
	apt_task_t *task = apt_consumer_task_base_get(framework->task);
	apt_task_msg_t *task_msg = apt_task_msg_get(task);
	if(task_msg) {
		framework_task_data_t *framework_task_data = (framework_task_data_t*)task_msg->data;
		task_msg->type = TASK_MSG_USER;
		task_msg->sub_type = DEMO_CONSOLE_MSG_ID;
		strncpy(framework_task_data->app_name,app_name,sizeof(framework_task_data->app_name)-1);
		strncpy(framework_task_data->profile_name,profile_name,sizeof(framework_task_data->profile_name)-1);
		framework_task_data->app_message = NULL;
		framework_task_data->demo_application = NULL;
		apt_task_msg_signal(task,task_msg);
	}
	return TRUE;
}

/** Destroy demo framework */
apt_bool_t demo_framework_destroy(demo_framework_t *framework)
{
	if(!framework) {
		return FALSE;
	}

	if(framework->task) {
		apt_task_t *task = apt_consumer_task_base_get(framework->task);
		apt_task_terminate(task,TRUE);
		apt_task_destroy(task);
		framework->task = NULL;
	}

	mrcp_client_shutdown(framework->client);
	return mrcp_client_destroy(framework->client);
}

static apt_bool_t demo_framework_app_register(demo_framework_t *framework, demo_application_t *demo_application, const char *name)
{
	apr_hash_set(framework->application_table,name,APR_HASH_KEY_STRING,demo_application);
	demo_application->framework = framework;
	demo_application->application = mrcp_application_create(
										demo_framework_message_handler,
										demo_application,
										framework->pool);
	return mrcp_client_application_register(framework->client,demo_application->application,name);
}

static void demo_framework_on_start_complete(apt_task_t *task)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Run Demo Framework");
}

static apt_bool_t demo_framework_console_msg_process(demo_framework_t *framework, const char *app_name, const char *profile_name)
{
	demo_application_t *demo_application = apr_hash_get(framework->application_table,app_name,APR_HASH_KEY_STRING);
	if(!demo_application) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No Such Demo Application [%s]",app_name);
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Run Demo Application Scenario [%s]",app_name);
	return demo_application->run(demo_application,profile_name);
}

static apt_bool_t demo_framework_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	if(msg->type == TASK_MSG_USER) {
		framework_task_data_t *data = (framework_task_data_t*)msg->data;
		switch(msg->sub_type) {
			case DEMO_APPLICATION_MSG_ID:
			{
				data->demo_application->handler(data->demo_application,data->app_message);
				break;
			}
			case DEMO_CONSOLE_MSG_ID:
			{
				apt_consumer_task_t *consumer_task = apt_task_object_get(task);
				demo_framework_t *framework = apt_consumer_task_object_get(consumer_task);
				demo_framework_console_msg_process(framework,data->app_name,data->profile_name);
				break;
			}
		}
	}
	return TRUE;
}

static apt_bool_t demo_framework_consumer_task_create(demo_framework_t *framework)
{
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(framework_task_data_t),framework->pool);
	framework->task = apt_consumer_task_create(framework,msg_pool,framework->pool);
	if(!framework->task) {
		return FALSE;
	}
	task = apt_consumer_task_base_get(framework->task);
	apt_task_name_set(task,FRAMEWORK_TASK_NAME);
	vtable = apt_consumer_task_vtable_get(framework->task);
	if(vtable) {
		vtable->process_msg = demo_framework_msg_process;
		vtable->on_start_complete = demo_framework_on_start_complete;
	}

	return TRUE;
}

/** Callback is called from MRCP client stack (task) context.
 *  Signal app_message to the main consumer task of the demo framework
 *  for further processing (see demo_framework_msg_process).
 */
static apt_bool_t demo_framework_message_handler(const mrcp_app_message_t *app_message)
{
	demo_application_t *demo_application;
	if(!app_message->application) {
		return FALSE;
	}
	demo_application = mrcp_application_object_get(app_message->application);
	if(demo_application && demo_application->framework) {
		demo_framework_t *framework = demo_application->framework;
		apt_task_t *task = apt_consumer_task_base_get(framework->task);
		apt_task_msg_t *task_msg = apt_task_msg_get(task);
		if(task_msg) {
			framework_task_data_t *framework_task_data = (framework_task_data_t*)task_msg->data;
			task_msg->type = TASK_MSG_USER;
			task_msg->sub_type = DEMO_APPLICATION_MSG_ID;
			framework_task_data->app_message = app_message;
			framework_task_data->demo_application = demo_application;
			apt_task_msg_signal(task,task_msg);
		}
	}
	return TRUE;
}
