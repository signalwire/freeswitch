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
 * $Id: mrcp_control_descriptor.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "apt_string_table.h"
#include "mrcp_control_descriptor.h"

/** String table of mrcp proto types (mrcp_proto_type_e) */
static const apt_str_table_item_t mrcp_proto_type_table[] = {
	{{"TCP/MRCPv2",    10},4},
	{{"TCP/TLS/MRCPv2",14},4}
};

/** String table of mrcp attributes (mrcp_attrib_e) */
static const apt_str_table_item_t mrcp_attrib_table[] = {
	{{"setup",      5},0},
	{{"connection",10},1},
	{{"resource",   8},0},
	{{"channel",    7},1},
	{{"cmid",       4},1}
};

/** String table of mrcp setup attribute values (mrcp_setup_type_e) */
static const apt_str_table_item_t mrcp_setup_value_table[] = {
	{{"active",      6},0},
	{{"passive",     7},0}
};

/** String table of mrcp connection attribute values (mrcp_connection_type_e) */
static const apt_str_table_item_t mrcp_connection_value_table[] = {
	{{"new",         3},0},
	{{"existing",    8},0}
};


MRCP_DECLARE(const apt_str_t*) mrcp_proto_get(mrcp_proto_type_e proto)
{
	return apt_string_table_str_get(mrcp_proto_type_table,MRCP_PROTO_COUNT,proto);
}

MRCP_DECLARE(mrcp_proto_type_e) mrcp_proto_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mrcp_proto_type_table,MRCP_PROTO_COUNT,attrib);
}

MRCP_DECLARE(const apt_str_t*) mrcp_attrib_str_get(mrcp_attrib_e attrib_id)
{
	return apt_string_table_str_get(mrcp_attrib_table,MRCP_ATTRIB_COUNT,attrib_id);
}

MRCP_DECLARE(mrcp_attrib_e) mrcp_attrib_id_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mrcp_attrib_table,MRCP_ATTRIB_COUNT,attrib);
}

MRCP_DECLARE(const apt_str_t*) mrcp_setup_type_get(mrcp_setup_type_e setup_type)
{
	return apt_string_table_str_get(mrcp_setup_value_table,MRCP_SETUP_TYPE_COUNT,setup_type);
}

MRCP_DECLARE(mrcp_setup_type_e) mrcp_setup_type_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mrcp_setup_value_table,MRCP_SETUP_TYPE_COUNT,attrib);
}

MRCP_DECLARE(const apt_str_t*) mrcp_connection_type_get(mrcp_connection_type_e connection_type)
{
	return apt_string_table_str_get(mrcp_connection_value_table,MRCP_CONNECTION_TYPE_COUNT,connection_type);
}

MRCP_DECLARE(mrcp_connection_type_e) mrcp_connection_type_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mrcp_connection_value_table,MRCP_CONNECTION_TYPE_COUNT,attrib);
}

/** Create MRCP control descriptor */
MRCP_DECLARE(mrcp_control_descriptor_t*) mrcp_control_descriptor_create(apr_pool_t *pool)
{
	mrcp_control_descriptor_t *descriptor;
	descriptor = apr_palloc(pool,sizeof(mrcp_control_descriptor_t));

	apt_string_reset(&descriptor->ip);
	descriptor->port = 0;
	descriptor->proto = MRCP_PROTO_UNKNOWN;
	descriptor->setup_type = MRCP_SETUP_TYPE_UNKNOWN;
	descriptor->connection_type = MRCP_CONNECTION_TYPE_UNKNOWN;
	apt_string_reset(&descriptor->resource_name);
	apt_string_reset(&descriptor->session_id);
	descriptor->cmid_arr = apr_array_make(pool,1,sizeof(apr_size_t));
	descriptor->id = 0;
	return descriptor;
}

/** Create MRCP control offer */
MRCP_DECLARE(mrcp_control_descriptor_t*) mrcp_control_offer_create(apr_pool_t *pool)
{
	mrcp_control_descriptor_t *offer = mrcp_control_descriptor_create(pool);
	offer->proto = MRCP_PROTO_TCP;
	offer->port = TCP_DISCARD_PORT;
	offer->setup_type = MRCP_SETUP_TYPE_ACTIVE;
	offer->connection_type = MRCP_CONNECTION_TYPE_EXISTING;
	return offer;
}

/** Create MRCP control answer */
MRCP_DECLARE(mrcp_control_descriptor_t*) mrcp_control_answer_create(mrcp_control_descriptor_t *offer, apr_pool_t *pool)
{
	mrcp_control_descriptor_t *answer = mrcp_control_descriptor_create(pool);
	if(offer) {
		*answer = *offer;
		answer->cmid_arr = apr_array_copy(pool,offer->cmid_arr);
	}
	answer->setup_type = MRCP_SETUP_TYPE_PASSIVE;
	return answer;
}

/** Add cmid to cmid_arr */
MRCP_DECLARE(void) mrcp_cmid_add(apr_array_header_t *cmid_arr, apr_size_t cmid)
{
	APR_ARRAY_PUSH(cmid_arr, apr_size_t) = cmid;
}

/** Find cmid in cmid_arr */
MRCP_DECLARE(apt_bool_t) mrcp_cmid_find(const apr_array_header_t *cmid_arr, apr_size_t cmid)
{
	int i;
	for(i=0; i<cmid_arr->nelts; i++) {
		if(APR_ARRAY_IDX(cmid_arr,i,apr_size_t) == cmid) {
			return TRUE;
		}
	}
	return FALSE;
}
