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

#include "mpf_termination.h"
#include "mpf_file_termination_factory.h"
#include "mpf_audio_file_stream.h"

static apt_bool_t mpf_file_termination_destroy(mpf_termination_t *termination)
{
	return TRUE;
}

static apt_bool_t mpf_file_termination_add(mpf_termination_t *termination, void *descriptor)
{
	apt_bool_t status = TRUE;
	mpf_audio_stream_t *audio_stream = termination->audio_stream;
	if(!audio_stream) {
		audio_stream = mpf_file_stream_create(termination,termination->pool);
		if(!audio_stream) {
			return FALSE;
		}
		termination->audio_stream = audio_stream;
	}

	if(descriptor) {
		status = mpf_file_stream_modify(audio_stream,descriptor);
	}
	return status;
}

static apt_bool_t mpf_file_termination_modify(mpf_termination_t *termination, void *descriptor)
{
	apt_bool_t status = TRUE;
	mpf_audio_stream_t *audio_stream = termination->audio_stream;
	if(!audio_stream) {
		return FALSE;
	}

	if(descriptor) {
		status = mpf_file_stream_modify(audio_stream,descriptor);
	}
	return status;
}

static apt_bool_t mpf_file_termination_subtract(mpf_termination_t *termination)
{
	return TRUE;
}

static const mpf_termination_vtable_t file_vtable = {
	mpf_file_termination_destroy,
	mpf_file_termination_add,
	mpf_file_termination_modify,
	mpf_file_termination_subtract
};

static mpf_termination_t* mpf_file_termination_create(
									mpf_termination_factory_t *termination_factory,
									void *obj, 
									apr_pool_t *pool)
{
	return mpf_termination_base_create(termination_factory,obj,&file_vtable,NULL,NULL,pool);
}

MPF_DECLARE(mpf_termination_factory_t*) mpf_file_termination_factory_create(apr_pool_t *pool)
{
	mpf_termination_factory_t *file_termination_factory = apr_palloc(pool,sizeof(mpf_termination_factory_t));
	file_termination_factory->create_termination = mpf_file_termination_create;
	return file_termination_factory;
}
