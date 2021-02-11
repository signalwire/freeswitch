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

#include "mrcp_connection.h"
#include "apt_pool.h"

mrcp_connection_t* mrcp_connection_create(void)
{
	mrcp_connection_t *connection;
	apr_pool_t *pool = apt_pool_create();
	if(!pool) {
		return NULL;
	}
	
	connection = apr_palloc(pool,sizeof(mrcp_connection_t));
	connection->pool = pool;
	apt_string_reset(&connection->remote_ip);
	connection->l_sockaddr = NULL;
	connection->r_sockaddr = NULL;
	connection->sock = NULL;
	connection->id = NULL;
	connection->verbose = TRUE;
	connection->access_count = 0;
	connection->use_count = 0;
	APR_RING_ELEM_INIT(connection,link);
	connection->channel_table = apr_hash_make(pool);
	connection->parser = NULL;
	connection->generator = NULL;
	connection->rx_buffer = NULL;
	connection->rx_buffer_size = 0;
	connection->tx_buffer = NULL;
	connection->tx_buffer_size = 0;
	connection->inactivity_timer = NULL;
	connection->termination_timer = NULL;

	return connection;
}

void mrcp_connection_destroy(mrcp_connection_t *connection)
{
	if(connection && connection->pool) {
		apr_pool_destroy(connection->pool);
	}
}

apt_bool_t mrcp_connection_channel_add(mrcp_connection_t *connection, mrcp_control_channel_t *channel)
{
	if(!connection || !channel) {
		return FALSE;
	}
	apr_hash_set(connection->channel_table,channel->identifier.buf,channel->identifier.length,channel);
	channel->connection = connection;
	connection->access_count++;
	connection->use_count++;
	return TRUE;
}

mrcp_control_channel_t* mrcp_connection_channel_find(const mrcp_connection_t *connection, const apt_str_t *identifier)
{
	if(!connection || !identifier) {
		return NULL;
	}
	return apr_hash_get(connection->channel_table,identifier->buf,identifier->length);
}

apt_bool_t mrcp_connection_channel_remove(mrcp_connection_t *connection, mrcp_control_channel_t *channel)
{
	if(!connection || !channel) {
		return FALSE;
	}
	apr_hash_set(connection->channel_table,channel->identifier.buf,channel->identifier.length,NULL);
	channel->connection = NULL;
	connection->access_count--;
	return TRUE;
}

apt_bool_t mrcp_connection_disconnect_raise(mrcp_connection_t *connection, const mrcp_connection_event_vtable_t *vtable)
{
	if(vtable && vtable->on_disconnect) {
		mrcp_control_channel_t *channel;
		void *val;
		apr_hash_index_t *it = apr_hash_first(connection->pool,connection->channel_table);
		/* walk through the list of channels and raise disconnect event for them */
		for(; it; it = apr_hash_next(it)) {
			apr_hash_this(it,NULL,NULL,&val);
			channel = val;
			if(channel) {
				vtable->on_disconnect(channel);
			}
		}
	}
	return TRUE;
}
