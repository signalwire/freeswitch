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

#include "mrcp_engine_iface.h"

/** Create engine channel */
mrcp_engine_channel_t* mrcp_engine_channel_virtual_create(mrcp_engine_t *engine, mrcp_version_e mrcp_version, apr_pool_t *pool)
{
	mrcp_engine_channel_t *channel;
	if(engine->is_open != TRUE) {
		return NULL;
	}
	if(engine->config->max_channel_count && engine->cur_channel_count >= engine->config->max_channel_count) {
		return NULL;
	}
	channel = engine->method_vtable->create_channel(engine,pool);
	if(channel) {
		channel->mrcp_version = mrcp_version;
		engine->cur_channel_count++;
	}
	return channel;
}

/** Destroy engine channel */
apt_bool_t mrcp_engine_channel_virtual_destroy(mrcp_engine_channel_t *channel)
{
	mrcp_engine_t *engine = channel->engine;
	if(engine->cur_channel_count) {
		engine->cur_channel_count--;
	}
	return channel->method_vtable->destroy(channel);
}

/** Allocate engine config */
mrcp_engine_config_t* mrcp_engine_config_alloc(apr_pool_t *pool)
{
	mrcp_engine_config_t *config = apr_palloc(pool,sizeof(mrcp_engine_config_t));
	config->name = NULL;
	config->max_channel_count = 0;
	config->params = NULL;
	return config;
}
