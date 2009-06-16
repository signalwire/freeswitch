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

#ifndef __MRCP_SERVER_TYPES_H__
#define __MRCP_SERVER_TYPES_H__

/**
 * @file mrcp_server_types.h
 * @brief MRCP Server Types
 */ 

#include "mrcp_sig_types.h"
#include "mrcp_connection_types.h"
#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/** Opaque MRCP server declaration */
typedef struct mrcp_server_t mrcp_server_t;

/** Opaque MRCP profile declaration */
typedef struct mrcp_profile_t mrcp_profile_t;


APT_END_EXTERN_C

#endif /*__MRCP_SERVER_TYPES_H__*/
