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

#ifndef MRCP_SIG_TYPES_H
#define MRCP_SIG_TYPES_H

/**
 * @file mrcp_sig_types.h
 * @brief MRCP Signaling Types Declaration
 */ 

#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** Opaque MRCP signaling settings declaration */
typedef struct mrcp_sig_settings_t mrcp_sig_settings_t;

/** Opaque MRCP signaling agent declaration */
typedef struct mrcp_sig_agent_t mrcp_sig_agent_t;

/** Opaque MRCP signaling agent factory declaration */
typedef struct mrcp_sa_factory_t mrcp_sa_factory_t;

/** Opaque MRCP session declaration */
typedef struct mrcp_session_t mrcp_session_t;

/** Opaque MRCP session attributes declaration */
typedef struct mrcp_session_attribs_t mrcp_session_attribs_t;

/** Opaque MRCP session descriptor declaration */
typedef struct mrcp_session_descriptor_t mrcp_session_descriptor_t;

APT_END_EXTERN_C

#endif /* MRCP_SIG_TYPES_H */
