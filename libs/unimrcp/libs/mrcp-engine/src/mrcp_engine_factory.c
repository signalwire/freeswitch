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
 * $Id: mrcp_engine_factory.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <apr_hash.h>
#include "mrcp_engine_factory.h"
#include "mrcp_synth_state_machine.h"
#include "mrcp_recog_state_machine.h"
#include "mrcp_recorder_state_machine.h"
#include "mrcp_verifier_state_machine.h"
#include "apt_log.h"

/** Engine factory declaration */
struct mrcp_engine_factory_t {
	apr_hash_t *engines;
	apr_pool_t *pool;
};


MRCP_DECLARE(mrcp_engine_factory_t*) mrcp_engine_factory_create(apr_pool_t *pool)
{
	mrcp_engine_factory_t *factory = apr_palloc(pool,sizeof(mrcp_engine_factory_t));
	factory->pool = pool;
	factory->engines = apr_hash_make(pool);
	return factory;
}

/** Destroy registered engines and the factory */
MRCP_DECLARE(apt_bool_t) mrcp_engine_factory_destroy(mrcp_engine_factory_t *factory)
{
	mrcp_engine_t *engine;
	apr_hash_index_t *it;
	void *val;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Destroy MRCP Engines");
	it=apr_hash_first(factory->pool,factory->engines);
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		engine = val;
		if(engine) {
			mrcp_engine_virtual_destroy(engine);
		}
	}
	apr_hash_clear(factory->engines);
	return TRUE;
}

/** Open registered engines */
MRCP_DECLARE(apt_bool_t) mrcp_engine_factory_open(mrcp_engine_factory_t *factory)
{
	mrcp_engine_t *engine;
	apr_hash_index_t *it;
	void *val;
	it = apr_hash_first(factory->pool,factory->engines);
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		engine = val;
		if(engine) {
			mrcp_engine_virtual_open(engine);
		}
	}
	return TRUE;
}

/** Close registered engines */
MRCP_DECLARE(apt_bool_t) mrcp_engine_factory_close(mrcp_engine_factory_t *factory)
{
	mrcp_engine_t *engine;
	apr_hash_index_t *it;
	void *val;
	it=apr_hash_first(factory->pool,factory->engines);
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		engine = val;
		if(engine) {
			mrcp_engine_virtual_close(engine);
		}
	}
	return TRUE;
}

/** Register new engine */
MRCP_DECLARE(apt_bool_t) mrcp_engine_factory_engine_register(mrcp_engine_factory_t *factory, mrcp_engine_t *engine)
{
	if(!engine || !engine->id) {
		return FALSE;
	}

	switch(engine->resource_id) {
		case MRCP_SYNTHESIZER_RESOURCE:
			engine->create_state_machine = mrcp_synth_state_machine_create;
			break;
		case MRCP_RECOGNIZER_RESOURCE:
			engine->create_state_machine = mrcp_recog_state_machine_create;
			break;
		case MRCP_RECORDER_RESOURCE:
			engine->create_state_machine = mrcp_recorder_state_machine_create;
			break;
		case MRCP_VERIFIER_RESOURCE:
			engine->create_state_machine = mrcp_verifier_state_machine_create;
			break;
		default:
			break;
	}

	if(!engine->create_state_machine) {
		return FALSE;
	}

	apr_hash_set(factory->engines,engine->id,APR_HASH_KEY_STRING,engine);
	return TRUE;
}

/** Get engine by name */
MRCP_DECLARE(mrcp_engine_t*) mrcp_engine_factory_engine_get(const mrcp_engine_factory_t *factory, const char *name)
{
	if(!name) {
		return NULL;
	}
	return apr_hash_get(factory->engines,name,APR_HASH_KEY_STRING);
}

/** Find engine by resource identifier */
MRCP_DECLARE(mrcp_engine_t*) mrcp_engine_factory_engine_find(const mrcp_engine_factory_t *factory, mrcp_resource_id resource_id)
{
	mrcp_engine_t *engine;
	void *val;
	apr_hash_index_t *it = apr_hash_first(factory->pool,factory->engines);
	/* walk through the engines */
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		engine = val;
		if(engine && engine->resource_id == resource_id) {
			return engine;
		}
	}
	return NULL;
}

/** Start iterating over the engines in a factory */
MRCP_DECLARE(apr_hash_index_t*) mrcp_engine_factory_engine_first(const mrcp_engine_factory_t *factory)
{
	return apr_hash_first(factory->pool,factory->engines);
}
