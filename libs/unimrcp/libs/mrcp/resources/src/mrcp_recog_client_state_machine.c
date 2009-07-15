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

#include "mrcp_state_machine.h"
#include "mrcp_recog_state_machine.h"
#include "mrcp_message.h"


/** Update state according to request received from user level or response/event received from MRCP server */
static apt_bool_t recog_state_update(mrcp_state_machine_t *state_machine, mrcp_message_t *message)
{
	/* no actual state machine processing yet, dispatch whatever received */
	return state_machine->on_dispatch(state_machine,message);
}

/** Create MRCP recognizer client state machine */
mrcp_state_machine_t* mrcp_recog_client_state_machine_create(void *obj, mrcp_version_e version, apr_pool_t *pool)
{
	mrcp_state_machine_t *state_machine = apr_palloc(pool,sizeof(mrcp_state_machine_t));
	mrcp_state_machine_init(state_machine,obj);
	state_machine->update = recog_state_update;
	return state_machine;
}
