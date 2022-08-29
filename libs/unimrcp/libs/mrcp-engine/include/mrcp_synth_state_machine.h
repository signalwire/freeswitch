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

#ifndef MRCP_SYNTH_STATE_MACHINE_H
#define MRCP_SYNTH_STATE_MACHINE_H

/**
 * @file mrcp_synth_state_machine.h
 * @brief MRCP Synthesizer State Machine
 */ 

#include "mrcp_state_machine.h"

APT_BEGIN_EXTERN_C

/** Create MRCP synthesizer state machine */
mrcp_state_machine_t* mrcp_synth_state_machine_create(void *obj, mrcp_version_e version, apr_pool_t *pool);

APT_END_EXTERN_C

#endif /* MRCP_SYNTH_STATE_MACHINE_H */
