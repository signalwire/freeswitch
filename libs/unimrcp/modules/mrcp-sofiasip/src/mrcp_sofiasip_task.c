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

typedef struct sofiasip_msg_container_t sofiasip_msg_container_t;
#define SU_MSG_ARG_T   sofiasip_msg_container_t

#include <sofia-sip/su.h>
#include <sofia-sip/nua.h>
#undef strcasecmp
#undef strncasecmp
#include <apr_general.h>

#include "mrcp_sofiasip_task.h"
#include "mrcp_sofiasip_logger.h"
#include "apt_log.h"

/** Sofia-SIP task */
struct mrcp_sofia_task_t {
	apr_pool_t   *pool;
	apt_task_t   *base;

	create_nua_f  nua_creator;
	void         *obj;

	su_root_t    *root;
	nua_t        *nua;
};

struct sofiasip_msg_container_t {
	apt_task_t     *task;
	apt_task_msg_t *msg;
};

static void mrcp_sofia_task_initialize(apt_task_t *task);
static void mrcp_sofia_task_deinitialize(apt_task_t *task);
static apt_bool_t mrcp_sofia_task_run(apt_task_t *task);
static apt_bool_t mrcp_sofia_task_on_terminate(apt_task_t *task);
static apt_bool_t mrcp_sofia_task_msg_signal(apt_task_t *task, apt_task_msg_t *msg);

/** Create Sofia-SIP task */
APT_DECLARE(mrcp_sofia_task_t*) mrcp_sofia_task_create(
										create_nua_f nua_creator,
										void *obj,
										apt_task_msg_pool_t *msg_pool,
										apr_pool_t *pool)
{
	apt_task_vtable_t *vtable;
	mrcp_sofia_task_t *task;

	if(!nua_creator) {
		return NULL;
	}

	task = apr_palloc(pool,sizeof(mrcp_sofia_task_t));
	task->pool = pool;
	task->nua_creator = nua_creator;
	task->obj = obj;

	task->base = apt_task_create(task,msg_pool,pool);
	if(!task->base) {
		return NULL;
	}

	vtable = apt_task_vtable_get(task->base);
	if(vtable) {
		vtable->on_pre_run = mrcp_sofia_task_initialize;
		vtable->on_post_run = mrcp_sofia_task_deinitialize;
		vtable->run = mrcp_sofia_task_run;
		vtable->terminate = mrcp_sofia_task_on_terminate;
		vtable->signal_msg = mrcp_sofia_task_msg_signal;
	}

	task->root = NULL;
	task->nua = NULL;

	return task;
}

/** Destroy Sofia-SIP task */
APT_DECLARE(apt_bool_t) mrcp_sofia_task_destroy(mrcp_sofia_task_t *task)
{
	return apt_task_destroy(task->base);
}

/** Start Sofia-SIP task */
APT_DECLARE(apt_bool_t) mrcp_sofia_task_start(mrcp_sofia_task_t *task)
{
	return apt_task_start(task->base);
}

/** Terminate Sofia-SIP task */
APT_DECLARE(apt_bool_t) mrcp_sofia_task_terminate(mrcp_sofia_task_t *task)
{
	return apt_task_terminate(task->base,TRUE);
}

/** Break main loop of Sofia-SIP task */
APT_DECLARE(void) mrcp_sofia_task_break(mrcp_sofia_task_t *task)
{
	if(task->root) {
		su_root_break(task->root);
	}
}

/** Get task base */
APT_DECLARE(apt_task_t*) mrcp_sofia_task_base_get(const mrcp_sofia_task_t *task)
{
	return task->base;
}

/** Get task vtable */
APT_DECLARE(apt_task_vtable_t*) mrcp_sofia_task_vtable_get(const mrcp_sofia_task_t *task)
{
	return apt_task_vtable_get(task->base);
}

/** Get external object */
APT_DECLARE(void*) mrcp_sofia_task_object_get(const mrcp_sofia_task_t *task)
{
	return task->obj;
}

/** Get su_root object */
APT_DECLARE(su_root_t*) mrcp_sofia_task_su_root_get(const mrcp_sofia_task_t *task)
{
	return task->root;
}

/** Get nua object */
APT_DECLARE(nua_t*) mrcp_sofia_task_nua_get(const mrcp_sofia_task_t *task)
{
	return task->nua;
}

static void mrcp_sofia_task_initialize(apt_task_t *base)
{
	mrcp_sofia_task_t *task = apt_task_object_get(base);

	apt_log(SIP_LOG_MARK,APT_PRIO_DEBUG,"Initialize Task [%s]", apt_task_name_get(base));

	/* Initialize Sofia-SIP library and create event loop */
	su_init();
	task->root = su_root_create(NULL);

	task->nua = task->nua_creator(task->obj,task->root);
	if(!task->nua) {
		apt_log(SIP_LOG_MARK,APT_PRIO_WARNING,"Failed to Create NUA [%s]", apt_task_name_get(base));
	}
}

static void mrcp_sofia_task_deinitialize(apt_task_t *base)
{
	mrcp_sofia_task_t *task = apt_task_object_get(base);

	apt_log(SIP_LOG_MARK,APT_PRIO_DEBUG,"Deinitialize Task [%s]", apt_task_name_get(base));

	if(task->nua) {
		nua_destroy(task->nua);
		task->nua = NULL;
	}

	if(task->root) {
		su_root_destroy(task->root);
		task->root = NULL;
	}
	su_deinit();
}

static apt_bool_t mrcp_sofia_task_run(apt_task_t *base)
{
	mrcp_sofia_task_t *task = apt_task_object_get(base);

	if(!task->root || !task->nua) {
		apt_log(SIP_LOG_MARK,APT_PRIO_WARNING,"Failed to Run Sofia-SIP Task");
		return FALSE;
	}

	/* Run event loop */
	su_root_run(task->root);

	apt_task_terminate_request_process(base);
	return TRUE;
}

static apt_bool_t mrcp_sofia_task_on_terminate(apt_task_t *base)
{
	mrcp_sofia_task_t *task = apt_task_object_get(base);
	if(!task->nua) {
		return FALSE;
	}

	apt_log(SIP_LOG_MARK,APT_PRIO_DEBUG,"Send Shutdown Signal to NUA [%s]", apt_task_name_get(base));
	nua_shutdown(task->nua);
	return TRUE;
}

static void mrcp_sofia_task_msg_process(void *obj, su_msg_r msg, sofiasip_msg_container_t *container)
{
	apt_log(SIP_LOG_MARK,APT_PRIO_DEBUG,"Receive Sofia-SIP Task Msg [%s]", apt_task_name_get(container->task));
	apt_task_msg_process(container->task, container->msg);
}

static apt_bool_t mrcp_sofia_task_msg_signal(apt_task_t *base, apt_task_msg_t *msg)
{
	mrcp_sofia_task_t *task = apt_task_object_get(base);

	sofiasip_msg_container_t *container;
	su_msg_r m = SU_MSG_R_INIT;

	su_msg_new(m, sizeof(sofiasip_msg_container_t));
	container = su_msg_data(m);
	container->task = base;
	container->msg = msg;

	if(su_msg_send_to(m, su_root_task(task->root), mrcp_sofia_task_msg_process) != 0)
		return FALSE;

	return TRUE;
}
