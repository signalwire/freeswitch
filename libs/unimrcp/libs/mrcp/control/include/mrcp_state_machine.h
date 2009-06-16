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

#ifndef __MRCP_STATE_MACHINE_H__
#define __MRCP_STATE_MACHINE_H__

/**
 * @file mrcp_state_machine.h
 * @brief MRCP State Machine
 */ 

#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** MRCP state machine declaration */
typedef struct mrcp_state_machine_t mrcp_state_machine_t;

/** MRCP message dispatcher */
typedef apt_bool_t (*mrcp_message_dispatcher_f)(mrcp_state_machine_t *state_machine, mrcp_message_t *message);


/** MRCP state machine */
struct mrcp_state_machine_t {
	/** External object associated with state machine */
	void *obj;
	/** Message dispatcher */
	mrcp_message_dispatcher_f dispatcher;

	/** Virtual update */
	apt_bool_t (*update)(mrcp_state_machine_t *state_machine, mrcp_message_t *message);
};

/** Initialize MRCP state machine */
static APR_INLINE void mrcp_state_machine_init(mrcp_state_machine_t *state_machine, void *obj, mrcp_message_dispatcher_f dispatcher)
{
	state_machine->obj = obj;
	state_machine->dispatcher = dispatcher;
}

/** Update MRCP state machine */
static APR_INLINE apt_bool_t mrcp_state_machine_update(mrcp_state_machine_t *state_machine, mrcp_message_t *message)
{
	return state_machine->update(state_machine,message);
}

APT_END_EXTERN_C

#endif /*__MRCP_STATE_MACHINE_H__*/
