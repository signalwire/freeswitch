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
 * $Id: mpf_engine_factory.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <apr_tables.h>
#include "mpf_engine_factory.h"
#include "mpf_termination_factory.h"

/** Factory of media engines */
struct mpf_engine_factory_t {
	/** Array of pointers to media engines */
	apr_array_header_t   *engines_arr;
	/** Index of the current engine */
	int                   index;
};

/** Create factory of media engines. */
MPF_DECLARE(mpf_engine_factory_t*) mpf_engine_factory_create(apr_pool_t *pool)
{
	mpf_engine_factory_t *mpf_factory = apr_palloc(pool,sizeof(mpf_engine_factory_t));
	mpf_factory->engines_arr = apr_array_make(pool,1,sizeof(mpf_engine_t*));
	mpf_factory->index = 0;
	return mpf_factory;
}

/** Add media engine to factory. */
MPF_DECLARE(apt_bool_t) mpf_engine_factory_engine_add(mpf_engine_factory_t *mpf_factory, mpf_engine_t *media_engine)
{
	mpf_engine_t **slot;
	if(!media_engine)
		return FALSE;

	slot = apr_array_push(mpf_factory->engines_arr);
	*slot = media_engine;
	return TRUE;
}

/** Determine whether factory is empty. */
MPF_DECLARE(apt_bool_t) mpf_engine_factory_is_empty(const mpf_engine_factory_t *mpf_factory)
{
	return apr_is_empty_array(mpf_factory->engines_arr);
}

/** Select next available media engine. */
MPF_DECLARE(mpf_engine_t*) mpf_engine_factory_engine_select(mpf_engine_factory_t *mpf_factory)
{
	mpf_engine_t *media_engine = APR_ARRAY_IDX(mpf_factory->engines_arr, mpf_factory->index, mpf_engine_t*);
	if(++mpf_factory->index == mpf_factory->engines_arr->nelts) {
		mpf_factory->index = 0;
	}
	return media_engine;
}

/** Associate media engines with RTP termination factory. */
MPF_DECLARE(apt_bool_t) mpf_engine_factory_rtp_factory_assign(mpf_engine_factory_t *mpf_factory, mpf_termination_factory_t *rtp_factory)
{
	int i;
	mpf_engine_t *media_engine;
	for(i=0; i<mpf_factory->engines_arr->nelts; i++) {
		media_engine = APR_ARRAY_IDX(mpf_factory->engines_arr, i, mpf_engine_t*);
		mpf_termination_factory_engine_assign(rtp_factory,media_engine);
	}
	return TRUE;
}
