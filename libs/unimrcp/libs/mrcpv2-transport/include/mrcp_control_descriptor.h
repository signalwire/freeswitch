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

#ifndef __MRCP_CONTROL_DESCRIPTOR_H__
#define __MRCP_CONTROL_DESCRIPTOR_H__

/**
 * @file mrcp_control_descriptor.h
 * @brief MRCPv2 Control Descriptor
 */ 

#include "apt_string.h"
#include "mrcp_connection_types.h"

APT_BEGIN_EXTERN_C

/** MRCPv2 proto transport */
typedef enum {
	MRCP_PROTO_TCP,
	MRCP_PROTO_TLS,

	MRCP_PROTO_COUNT,
	MRCP_PROTO_UNKNOWN = MRCP_PROTO_COUNT
}mrcp_proto_type_e;


/** MRCPv2 attributes */
typedef enum {
	MRCP_ATTRIB_SETUP,
	MRCP_ATTRIB_CONNECTION,
	MRCP_ATTRIB_RESOURCE,
	MRCP_ATTRIB_CHANNEL,
	MRCP_ATTRIB_CMID,

	MRCP_ATTRIB_COUNT,
	MRCP_ATTRIB_UNKNOWN = MRCP_ATTRIB_COUNT
}mrcp_attrib_e;


/** MRCPv2 setup attributes */
typedef enum {
	MRCP_SETUP_TYPE_ACTIVE,
	MRCP_SETUP_TYPE_PASSIVE,

	MRCP_SETUP_TYPE_COUNT,
	MRCP_SETUP_TYPE_UNKNOWN = MRCP_SETUP_TYPE_COUNT
} mrcp_setup_type_e;

/** MRCPv2 connection attributes */
typedef enum {
	MRCP_CONNECTION_TYPE_NEW,
	MRCP_CONNECTION_TYPE_EXISTING,

	MRCP_CONNECTION_TYPE_COUNT,
	MRCP_CONNECTION_TYPE_UNKNOWN = MRCP_CONNECTION_TYPE_COUNT
} mrcp_connection_type_e;


/** MRCPv2 control descriptor */
struct mrcp_control_descriptor_t {
	/** IP address */
	apt_str_t              ip;
	/** Port */
	apr_port_t             port;
	/** Protocol type */
	mrcp_proto_type_e      proto;
	/** Setup type */
	mrcp_setup_type_e      setup_type;
	/** Connection type */
	mrcp_connection_type_e connection_type;
	/** Resource name */
	apt_str_t              resource_name;
	/** Session identifier */
	apt_str_t              session_id;
	/** Control media identifier */
	apr_size_t             cmid;
	/** Base identifier */
	apr_size_t             id;
};

/** Initialize MRCP control descriptor */
static APR_INLINE void mrcp_control_descriptor_init(mrcp_control_descriptor_t *descriptor)
{
	apt_string_reset(&descriptor->ip);
	descriptor->port = 0;
	descriptor->proto = MRCP_PROTO_UNKNOWN;
	descriptor->setup_type = MRCP_SETUP_TYPE_UNKNOWN;
	descriptor->connection_type = MRCP_CONNECTION_TYPE_UNKNOWN;
	apt_string_reset(&descriptor->resource_name);
	apt_string_reset(&descriptor->session_id);
	descriptor->cmid = 0;
	descriptor->id = 0;
}

/** Create MRCP control offer */
MRCP_DECLARE(mrcp_control_descriptor_t*) mrcp_control_offer_create(apr_pool_t *pool);

/** Create MRCP control answer */
MRCP_DECLARE(mrcp_control_descriptor_t*) mrcp_control_answer_create(mrcp_control_descriptor_t *offer, apr_pool_t *pool);

/** Get MRCP protocol transport name by identifier */
MRCP_DECLARE(const apt_str_t*) mrcp_proto_get(mrcp_proto_type_e proto);

/** Find MRCP protocol transport identifier by name */
MRCP_DECLARE(mrcp_proto_type_e) mrcp_proto_find(const apt_str_t *attrib);


/** Get MRCP attribute name by identifier */
MRCP_DECLARE(const apt_str_t*) mrcp_attrib_str_get(mrcp_attrib_e attrib_id);

/** Find MRCP attribute identifier by name */
MRCP_DECLARE(mrcp_attrib_e) mrcp_attrib_id_find(const apt_str_t *attrib);


/** Get MRCP setup type name by identifier */
MRCP_DECLARE(const apt_str_t*) mrcp_setup_type_get(mrcp_setup_type_e setup_type);

/** Find MRCP setup type identifier by name */
MRCP_DECLARE(mrcp_setup_type_e) mrcp_setup_type_find(const apt_str_t *attrib);


/** Get MRCP connection type name by identifier */
MRCP_DECLARE(const apt_str_t*) mrcp_connection_type_get(mrcp_connection_type_e connection_type);

/** Find MRCP connection type identifier by name */
MRCP_DECLARE(mrcp_connection_type_e) mrcp_connection_type_find(const apt_str_t *attrib);


APT_END_EXTERN_C

#endif /*__MRCP_CONTROL_DESCRIPTOR_H__*/
