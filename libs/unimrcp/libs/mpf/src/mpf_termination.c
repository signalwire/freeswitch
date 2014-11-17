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
 * $Id: mpf_termination.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
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
	termination->name = "media-tm";
	termination->obj = obj;
	termination->media_engine = NULL;
	termination->event_handler = NULL;
	termination->codec_manager = NULL;
	termination->timer_queue = NULL;
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

MPF_DECLARE(apt_bool_t) mpf_termination_add(mpf_termination_t *termination, void *descriptor)
{
	if(termination->vtable && termination->vtable->add) {
		termination->vtable->add(termination,descriptor);
	}
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_termination_modify(mpf_termination_t *termination, void *descriptor)
{
	if(termination->vtable && termination->vtable->modify) {
		termination->vtable->modify(termination,descriptor);
	}
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_termination_subtract(mpf_termination_t *termination)
{
	if(termination->vtable && termination->vtable->subtract) {
		termination->vtable->subtract(termination);
	}
	return TRUE;
}
