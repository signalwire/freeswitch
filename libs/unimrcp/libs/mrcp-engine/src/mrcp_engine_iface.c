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

#include "mrcp_engine_iface.h"
#include "apt_log.h"

/** Destroy engine */
apt_bool_t mrcp_engine_virtual_destroy(mrcp_engine_t *engine)
{
	return engine->method_vtable->destroy(engine);
}

/** Open engine */
apt_bool_t mrcp_engine_virtual_open(mrcp_engine_t *engine)
{
	if(engine->is_open == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open Engine [%s]",engine->id);
		return engine->method_vtable->open(engine);
	}
	return FALSE;
}

/** Response to open engine request */
void mrcp_engine_on_open(mrcp_engine_t *engine, apt_bool_t status)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Engine Opened [%s] status [%s]",
		engine->id,
		status == TRUE ? "success" : "failure");
	engine->is_open = status;
}

/** Close engine */
apt_bool_t mrcp_engine_virtual_close(mrcp_engine_t *engine)
{
	if(engine->is_open == TRUE) {
		engine->is_open = FALSE;
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close Engine [%s]",engine->id);
		return engine->method_vtable->close(engine);
	}
	return FALSE;
}

/** Response to close engine request */
void mrcp_engine_on_close(mrcp_engine_t *engine)
{
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Engine Closed [%s]",engine->id);
	engine->is_open = FALSE;
}

/** Create engine channel */
mrcp_engine_channel_t* mrcp_engine_channel_virtual_create(mrcp_engine_t *engine, apr_table_t *attribs, mrcp_version_e mrcp_version, apr_pool_t *pool)
{
	mrcp_engine_channel_t *channel;
	if(engine->is_open != TRUE) {
		return NULL;
	}
	if(engine->config->max_channel_count && engine->cur_channel_count >= engine->config->max_channel_count) {
		apt_log(APT_LOG_MARK, APT_PRIO_NOTICE, "Maximum channel count %"APR_SIZE_T_FMT" exceeded for engine [%s]",
			engine->config->max_channel_count, engine->id);
		return NULL;
	}
	channel = engine->method_vtable->create_channel(engine,pool);
	if(channel) {
		channel->mrcp_version = mrcp_version;
		channel->attribs = attribs;
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
	config->max_channel_count = 0;
	config->params = NULL;
	return config;
}

/** Allocate engine profile settings */
mrcp_engine_settings_t* mrcp_engine_settings_alloc(apr_pool_t *pool)
{
	mrcp_engine_settings_t *settings = apr_palloc(pool,sizeof(mrcp_engine_settings_t));
	settings->resource_id = NULL;
	settings->engine_id = NULL;
	settings->attribs = NULL;
	settings->engine = NULL;
	return settings;
}
