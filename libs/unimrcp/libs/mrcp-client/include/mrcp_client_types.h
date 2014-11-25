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
 * $Id: mrcp_client_types.h 2251 2014-11-21 02:36:44Z achaloyan@gmail.com $
 */

#ifndef MRCP_CLIENT_TYPES_H
#define MRCP_CLIENT_TYPES_H

/**
 * @file mrcp_client_types.h
 * @brief MRCP Client Types
 */ 

#include "mrcp_sig_types.h"
#include "mrcp_connection_types.h"
#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/** Opaque MRCP client declaration */
typedef struct mrcp_client_t mrcp_client_t;

/** Opaque MRCP client profile declaration */
typedef struct mrcp_client_profile_t mrcp_client_profile_t;
/** Backward compatible declaration of MRCP profile */
typedef mrcp_client_profile_t mrcp_profile_t;

/** Opaque MRCP application declaration */
typedef struct mrcp_application_t mrcp_application_t;

/** Opaque MRCP channel declaration */
typedef struct mrcp_channel_t mrcp_channel_t;


APT_END_EXTERN_C

#endif /* MRCP_CLIENT_TYPES_H */
