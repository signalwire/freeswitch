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

#ifndef MRCP_TYPES_H
#define MRCP_TYPES_H

/**
 * @file mrcp_types.h
 * @brief Basic MRCP Types
 */ 

#include "mrcp.h"

APT_BEGIN_EXTERN_C

/** Protocol version */
typedef enum {
	
	MRCP_VERSION_UNKNOWN = 0,  /**< Unknown version */
	MRCP_VERSION_1 = 1,        /**< MRCPv1 (RFC4463) */
	MRCP_VERSION_2 = 2         /**< MRCPv2 (draft-ietf-speechsc-mrcpv2-20) */
} mrcp_version_e;

/** Enumeration of MRCP resource types */
typedef enum {
	MRCP_SYNTHESIZER_RESOURCE, /**< Synthesizer resource */
	MRCP_RECOGNIZER_RESOURCE,  /**< Recognizer resource */
	MRCP_RECORDER_RESOURCE,    /**< Recorder resource */
	MRCP_VERIFIER_RESOURCE,    /**< Verifier resource */

	MRCP_RESOURCE_TYPE_COUNT   /**< Number of resources */
} mrcp_resource_type_e;

/* MRCPv2 specifies request-id as 32bit unsigned integer,
 * while MRCPv1 doesn't limit this value (1 * DIGIT).
 * Some MRCPv1 clients use too long request-id. 
 * To support them #define TOO_LONG_MRCP_REQUEST_ID
 */
#ifdef TOO_LONG_MRCP_REQUEST_ID
/** MRCP request identifier */
typedef apr_uint64_t  mrcp_request_id;
/** Format to log MRCP request identifier */
#define MRCP_REQUEST_ID_FMT    APR_UINT64_T_FMT
#else 
/** MRCP request identifier */
typedef apr_uint32_t  mrcp_request_id;
/** Format to log MRCP request identifier */
#define MRCP_REQUEST_ID_FMT    "d"
#endif


/** Method identifier associated with method name */
typedef apr_size_t mrcp_method_id;
/** Resource identifier associated with resource name */
typedef apr_size_t mrcp_resource_id;


/** Opaque MRCP message declaration */
typedef struct mrcp_message_t mrcp_message_t;
/** Opaque MRCP resource declaration */
typedef struct mrcp_resource_t mrcp_resource_t;
/** Opaque MRCP resource factory declaration */
typedef struct mrcp_resource_factory_t mrcp_resource_factory_t;


APT_END_EXTERN_C

#endif /* MRCP_TYPES_H */
