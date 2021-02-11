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

#ifndef APT_TASK_MSG_H
#define APT_TASK_MSG_H

/**
 * @file apt_task_msg.h
 * @brief Task Message Base Definition
 */ 

#include "apt.h"

APT_BEGIN_EXTERN_C

/** Enumeration of task message types */
typedef enum {
	TASK_MSG_CORE,                      /**< core task message type */
	TASK_MSG_USER                       /**< user defined task messages start from here */
} apt_task_msg_type_e;

/** Enumeration of core task messages */
typedef enum {
	CORE_TASK_MSG_NONE,                 /**< indefinite message */
	CORE_TASK_MSG_START_COMPLETE,       /**< start-complete message */
	CORE_TASK_MSG_TERMINATE_REQUEST,    /**< terminate-request message */
	CORE_TASK_MSG_TERMINATE_COMPLETE,   /**< terminate-complete message */
	CORE_TASK_MSG_TAKEOFFLINE_REQUEST,  /**< take-offline-request message */
	CORE_TASK_MSG_TAKEOFFLINE_COMPLETE, /**< take-offline-complete message */
	CORE_TASK_MSG_BRINGONLINE_REQUEST,  /**< bring-online-request message */
	CORE_TASK_MSG_BRINGONLINE_COMPLETE, /**< bring-online-complete message */
} apt_core_task_msg_type_e;

/** Opaque task message declaration */
typedef struct apt_task_msg_t apt_task_msg_t;
/** Opaque task message pool declaration */
typedef struct apt_task_msg_pool_t apt_task_msg_pool_t;

/** Task message is used for inter task communication */
struct apt_task_msg_t {
	/** Message pool the task message is allocated from */
	apt_task_msg_pool_t *msg_pool;
	/** Task msg type */
	int                  type;
	/** Task msg sub type */
	int                  sub_type;
	/** Context specific data */
	char                 data[1];
};


/** Create pool of task messages with dynamic allocation of messages (no actual pool is created) */
APT_DECLARE(apt_task_msg_pool_t*) apt_task_msg_pool_create_dynamic(apr_size_t msg_size, apr_pool_t *pool);

/** Create pool of task messages with static allocation of messages */
APT_DECLARE(apt_task_msg_pool_t*) apt_task_msg_pool_create_static(apr_size_t msg_size, apr_size_t msg_pool_size, apr_pool_t *pool);

/** Destroy pool of task messages */
APT_DECLARE(void) apt_task_msg_pool_destroy(apt_task_msg_pool_t *msg_pool);


/** Acquire task message from task message pool */
APT_DECLARE(apt_task_msg_t*) apt_task_msg_acquire(apt_task_msg_pool_t *task_msg_pool);

/** Realese task message */
APT_DECLARE(void) apt_task_msg_release(apt_task_msg_t *task_msg);


APT_END_EXTERN_C

#endif /* APT_TASK_MSG_H */
