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
#include "mpf_stream.h"
#include "mpf_codec_manager.h"

MPF_DECLARE(mpf_termination_t*) mpf_termination_base_create(
										mpf_termination_factory_t *termination_factory,
										void *obj,
										const mpf_termination_vtable_t *vtable,
										mpf_audio_stream_t *audio_stream,
										mpf_video_stream_t *video_stream,
										apr_pool_t *pool)
{
	mpf_termination_t *termination = apr_palloc(pool,sizeof(mpf_termination_t));
	termination->pool = pool;
	termination->obj = obj;
	termination->event_handler_obj = NULL;
	termination->event_handler = NULL;
	termination->codec_manager = NULL;
	termination->termination_factory = termination_factory;
	termination->vtable = vtable;
	termination->slot = 0;
	if(audio_stream) {
		audio_stream->termination = termination;
	}
	if(video_stream) {
		video_stream->termination = termination;
	}
	termination->audio_stream = audio_stream;
	termination->video_stream = video_stream;
	return termination;
}

MPF_DECLARE(apt_bool_t) mpf_termination_destroy(mpf_termination_t *termination)
{
	if(termination->vtable && termination->vtable->destroy) {
		termination->vtable->destroy(termination);
	}
	return TRUE;
}

MPF_DECLARE(void*) mpf_termination_object_get(mpf_termination_t *termination)
{
	return termination->obj;
}

MPF_DECLARE(apt_bool_t) mpf_termination_modify(mpf_termination_t *termination, void *descriptor)
{
	if(termination->vtable && termination->vtable->modify) {
		termination->vtable->modify(termination,descriptor);
	}
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_termination_validate(mpf_termination_t *termination)
{
	mpf_audio_stream_t *audio_stream;
	if(!termination) {
		return FALSE;
	}
	audio_stream = termination->audio_stream;
	if(audio_stream) {
		if(!audio_stream->vtable) {
			return FALSE;
		}
		if((audio_stream->mode & STREAM_MODE_RECEIVE) == STREAM_MODE_RECEIVE) {
			if(!audio_stream->rx_codec) {
				audio_stream->rx_codec = mpf_codec_manager_default_codec_get(
											termination->codec_manager,
											termination->pool);
			}
		}
		if((audio_stream->mode & STREAM_MODE_SEND) == STREAM_MODE_SEND) {
			if(!audio_stream->tx_codec) {
				audio_stream->tx_codec = mpf_codec_manager_default_codec_get(
											termination->codec_manager,
											termination->pool);
			}
		}
	}
	return TRUE;
}


/** Create MPF termination by termination factory */
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
