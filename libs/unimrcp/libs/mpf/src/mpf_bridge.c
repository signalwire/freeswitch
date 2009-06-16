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

#include "mpf_bridge.h"
#include "mpf_stream.h"
#include "apt_log.h"

static apt_bool_t mpf_bridge_process(mpf_object_t *object)
{
	object->frame.type = MEDIA_FRAME_TYPE_NONE;
	object->source->vtable->read_frame(object->source,&object->frame);
	
	if((object->frame.type & MEDIA_FRAME_TYPE_AUDIO) == 0) {
		memset(	object->frame.codec_frame.buffer,
				0,
				object->frame.codec_frame.size);
	}

	object->sink->vtable->write_frame(object->sink,&object->frame);
	return TRUE;
}

static apt_bool_t mpf_null_bridge_process(mpf_object_t *object)
{
	object->frame.type = MEDIA_FRAME_TYPE_NONE;
	object->source->vtable->read_frame(object->source,&object->frame);
	object->sink->vtable->write_frame(object->sink,&object->frame);
	return TRUE;
}


static apt_bool_t mpf_bridge_destroy(mpf_object_t *object)
{
	mpf_object_t *bridge = object;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Destroy Audio Bridge");
	mpf_audio_stream_rx_close(bridge->source);
	mpf_audio_stream_tx_close(bridge->sink);
	return TRUE;
}

static mpf_object_t* mpf_bridge_base_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	mpf_object_t *bridge;
	if(!source || !sink) {
		return NULL;
	}

	bridge = apr_palloc(pool,sizeof(mpf_object_t));
	bridge->source = source;
	bridge->sink = sink;
	bridge->process = mpf_bridge_process;
	bridge->destroy = mpf_bridge_destroy;

	if(mpf_audio_stream_rx_open(source) == FALSE) {
		return NULL;
	}
	if(mpf_audio_stream_tx_open(sink) == FALSE) {
		mpf_audio_stream_rx_close(source);
		return NULL;
	}
	return bridge;
}

MPF_DECLARE(mpf_object_t*) mpf_bridge_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor;
	apr_size_t frame_size;
	mpf_object_t *bridge;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Create Audio Bridge");
	bridge = mpf_bridge_base_create(source,sink,pool);
	if(!bridge) {
		return NULL;
	}

	descriptor = source->rx_codec->descriptor;
	frame_size = mpf_codec_linear_frame_size_calculate(descriptor->sampling_rate,descriptor->channel_count);
	bridge->frame.codec_frame.size = frame_size;
	bridge->frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	return bridge;
}

MPF_DECLARE(mpf_object_t*) mpf_null_bridge_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	mpf_codec_t *codec;
	apr_size_t frame_size;
	mpf_object_t *bridge;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Create Audio Null Bridge");
	bridge = mpf_bridge_base_create(source,sink,pool);
	if(!bridge) {
		return NULL;
	}
	bridge->process = mpf_null_bridge_process;

	codec = source->rx_codec;
	frame_size = mpf_codec_frame_size_calculate(codec->descriptor,codec->attribs);
	bridge->frame.codec_frame.size = frame_size;
	bridge->frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	return bridge;
}
