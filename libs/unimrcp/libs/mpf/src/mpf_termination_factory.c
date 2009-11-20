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

#include "mpf_termination_factory.h"
#include "mpf_termination.h"

/** Create MPF termination from termination factory */
MPF_DECLARE(mpf_termination_t*) mpf_termination_create(
										mpf_termination_factory_t *termination_factory,
										void *obj,
										apr_pool_t *pool)
{
	if(termination_factory && termination_factory->create_termination) {
		return termination_factory->create_termination(termination_factory,obj,pool);
	}
	return NULL;
}

/** Create raw MPF termination. */
MPF_DECLARE(mpf_termination_t*) mpf_raw_termination_create(
										void *obj,
										mpf_audio_stream_t *audio_stream,
										mpf_video_stream_t *video_stream,
										apr_pool_t *pool)
{
	return mpf_termination_base_create(NULL,obj,NULL,audio_stream,video_stream,pool);
}

MPF_DECLARE(apt_bool_t) mpf_termination_destroy(mpf_termination_t *termination)
{
	if(termination->vtable && termination->vtable->destroy) {
		termination->vtable->destroy(termination);
	}
	return TRUE;
}

/** Get associated object. */
MPF_DECLARE(void*) mpf_termination_object_get(mpf_termination_t *termination)
{
	return termination->obj;
}

/** Get audio stream. */
MPF_DECLARE(mpf_audio_stream_t*) mpf_termination_audio_stream_get(mpf_termination_t *termination)
{
	return termination->audio_stream;
}

/** Get video stream. */
MPF_DECLARE(mpf_video_stream_t*) mpf_termination_video_stream_get(mpf_termination_t *termination)
{
	return termination->video_stream;
}
