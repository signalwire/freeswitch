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
 * $Id: apt_task_msg.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <stdlib.h>
#include "apt_task_msg.h"

/** Abstract pool of task messages to allocate task messages from */
struct apt_task_msg_pool_t {
	void (*destroy)(apt_task_msg_pool_t *pool);

	apt_task_msg_t* (*acquire_msg)(apt_task_msg_pool_t *pool);
	void (*release_msg)(apt_task_msg_t *task_msg);

	void       *obj;
	apr_pool_t *pool;
};


/** Dynamic allocation of messages (no actual pool exist)*/
typedef struct apt_msg_pool_dynamic_t apt_msg_pool_dynamic_t;

struct apt_msg_pool_dynamic_t {
	apr_size_t size;
};

static apt_task_msg_t* dynamic_pool_acquire_msg(apt_task_msg_pool_t *task_msg_pool)
{
	apt_msg_pool_dynamic_t *dynamic_pool = task_msg_pool->obj;
	apt_task_msg_t *task_msg = malloc(dynamic_pool->size);
	task_msg->msg_pool = task_msg_pool;
	task_msg->type = TASK_MSG_USER;
	task_msg->sub_type = 0;
	return task_msg;
}

static void dynamic_pool_release_msg(apt_task_msg_t *task_msg)
{
	if(task_msg) {
		free(task_msg);
	}
}

static void dynamic_pool_destroy(apt_task_msg_pool_t *task_msg_pool)
{
	/* nothing to do */
}

APT_DECLARE(apt_task_msg_pool_t*) apt_task_msg_pool_create_dynamic(apr_size_t msg_size, apr_pool_t *pool)
{
	apt_task_msg_pool_t *task_msg_pool = apr_palloc(pool,sizeof(apt_task_msg_pool_t));
	apt_msg_pool_dynamic_t *dynamic_pool = apr_palloc(pool,sizeof(apt_msg_pool_dynamic_t));
	dynamic_pool->size = msg_size + sizeof(apt_task_msg_t) - 1;

	task_msg_pool->pool = pool;
	task_msg_pool->obj = dynamic_pool;
	task_msg_pool->acquire_msg = dynamic_pool_acquire_msg;
	task_msg_pool->release_msg = dynamic_pool_release_msg;
	task_msg_pool->destroy = dynamic_pool_destroy;
	return task_msg_pool;
}


/** Static allocation of messages from message pool (not implemented yet) */
APT_DECLARE(apt_task_msg_pool_t*) apt_task_msg_pool_create_static(apr_size_t msg_size, apr_size_t pool_size, apr_pool_t *pool)
{
	return NULL;
}



APT_DECLARE(void) apt_task_msg_pool_destroy(apt_task_msg_pool_t *msg_pool)
{
	if(msg_pool->destroy) {
		msg_pool->destroy(msg_pool);
	}
}

APT_DECLARE(apt_task_msg_t*) apt_task_msg_acquire(apt_task_msg_pool_t *task_msg_pool)
{
	if(!task_msg_pool->acquire_msg)
		return NULL;
	return task_msg_pool->acquire_msg(task_msg_pool);
}

APT_DECLARE(void) apt_task_msg_release(apt_task_msg_t *task_msg)
{
	apt_task_msg_pool_t *task_msg_pool = task_msg->msg_pool;
	if(task_msg_pool->release_msg)
		task_msg_pool->release_msg(task_msg);
}
