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
 * $Id: mrcp_state_machine.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MRCP_STATE_MACHINE_H
#define MRCP_STATE_MACHINE_H

/**
 * @file mrcp_state_machine.h
 * @brief MRCP State Machine
 */ 

#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** MRCP state machine declaration */
typedef struct mrcp_state_machine_t mrcp_state_machine_t;


/** MRCP state machine */
struct mrcp_state_machine_t {
	/** External object associated with state machine */
	void *obj;
	/** State either active or deactivating */
	apt_bool_t active;

	/** Virtual update */
	apt_bool_t (*update)(mrcp_state_machine_t *state_machine, mrcp_message_t *message);
	/** Deactivate */
	apt_bool_t (*deactivate)(mrcp_state_machine_t *state_machine);


	/** Message dispatcher */
	apt_bool_t (*on_dispatch)(mrcp_state_machine_t *state_machine, mrcp_message_t *message);
	/** Deactivated */
	apt_bool_t (*on_deactivate)(mrcp_state_machine_t *state_machine);
};

/** Initialize MRCP state machine */
static APR_INLINE void mrcp_state_machine_init(mrcp_state_machine_t *state_machine, void *obj)
{
	state_machine->obj = obj;
	state_machine->active = TRUE;
	state_machine->on_dispatch = NULL;
	state_machine->on_deactivate = NULL;
	state_machine->update = NULL;
	state_machine->deactivate = NULL;
}

/** Update MRCP state machine */
static APR_INLINE apt_bool_t mrcp_state_machine_update(mrcp_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(state_machine->update) {
		return state_machine->update(state_machine,message);
	}
	return FALSE;
}

/** Deactivate MRCP state machine */
static APR_INLINE apt_bool_t mrcp_state_machine_deactivate(mrcp_state_machine_t *state_machine)
{
	if(state_machine->deactivate) {
		state_machine->active = FALSE;
		return state_machine->deactivate(state_machine);
	}
	return FALSE;
}

APT_END_EXTERN_C

#endif /* MRCP_STATE_MACHINE_H */
