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
 * $Id: mpf_multiplier.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mpf_multiplier.h"
#include "mpf_encoder.h"
#include "mpf_decoder.h"
#include "mpf_resampler.h"
#include "mpf_codec_manager.h"
#include "apt_log.h"

typedef struct mpf_multiplier_t mpf_multiplier_t;

/** MPF multiplier derived from MPF object */
struct mpf_multiplier_t {
	/** MPF multiplier base */
	mpf_object_t         base;
	/** Audio source */
	mpf_audio_stream_t  *source;
	/** Array of audio sinks */
	mpf_audio_stream_t **sink_arr;
	/** Number of audio sinks */
	apr_size_t           sink_count;

	/** Media frame used to read data from source and write it to sinks */
	mpf_frame_t          frame;
};

static apt_bool_t mpf_multiplier_process(mpf_object_t *object)
{
	apr_size_t i;
	mpf_audio_stream_t *sink;
	mpf_multiplier_t *multiplier = (mpf_multiplier_t*) object;

	multiplier->frame.type = MEDIA_FRAME_TYPE_NONE;
	multiplier->frame.marker = MPF_MARKER_NONE;
	multiplier->source->vtable->read_frame(multiplier->source,&multiplier->frame);
	
	if((multiplier->frame.type & MEDIA_FRAME_TYPE_AUDIO) == 0) {
		memset(	multiplier->frame.codec_frame.buffer,
				0,
				multiplier->frame.codec_frame.size);
	}

	for(i=0; i<multiplier->sink_count; i++)	{
		sink = multiplier->sink_arr[i];
		if(sink) {
			sink->vtable->write_frame(sink,&multiplier->frame);
		}
	}
	return TRUE;
}

static apt_bool_t mpf_multiplier_destroy(mpf_object_t *object)
{
	apr_size_t i;
	mpf_audio_stream_t *sink;
	mpf_multiplier_t *multiplier = (mpf_multiplier_t*) object;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Destroy Multiplier %s",object->name);
	mpf_audio_stream_rx_close(multiplier->source);
	for(i=0; i<multiplier->sink_count; i++)	{
		sink = multiplier->sink_arr[i];
		if(sink) {
			mpf_audio_stream_tx_close(sink);
		}
	}
	return TRUE;
}

static void mpf_multiplier_trace(mpf_object_t *object)
{
	mpf_multiplier_t *multiplier = (mpf_multiplier_t*) object;
	apr_size_t i;
	mpf_audio_stream_t *sink;
	char buf[2048];
	apr_size_t offset;

	apt_text_stream_t output;
	apt_text_stream_init(&output,buf,sizeof(buf)-1);

	mpf_audio_stream_trace(multiplier->source,STREAM_DIRECTION_RECEIVE,&output);
	
	offset = output.pos - output.text.buf;
	output.pos += apr_snprintf(output.pos, output.text.length - offset,
		"->Multiplier->");

	for(i=0; i<multiplier->sink_count; i++)	{
		sink = multiplier->sink_arr[i];
		if(sink) {
			mpf_audio_stream_trace(sink,STREAM_DIRECTION_SEND,&output);
			apt_text_char_insert(&output,';');
		}
	}

	*output.pos = '\0';
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Media Path %s %s",
		object->name,
		output.text.buf);
}

MPF_DECLARE(mpf_object_t*) mpf_multiplier_create(
								mpf_audio_stream_t *source,
								mpf_audio_stream_t **sink_arr,
								apr_size_t sink_count,
								const mpf_codec_manager_t *codec_manager,
								const char *name,
								apr_pool_t *pool)
{
	apr_size_t i;
	apr_size_t frame_size;
	mpf_codec_descriptor_t *descriptor;
	mpf_audio_stream_t *sink;
	mpf_multiplier_t *multiplier;
	if(!source || !sink_arr || !sink_count) {
		return NULL;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Create Multiplier %s",name);
	multiplier = apr_palloc(pool,sizeof(mpf_multiplier_t));
	multiplier->source = NULL;
	multiplier->sink_arr = NULL;
	multiplier->sink_count = 0;
	mpf_object_init(&multiplier->base,name);
	multiplier->base.process = mpf_multiplier_process;
	multiplier->base.destroy = mpf_multiplier_destroy;
	multiplier->base.trace = mpf_multiplier_trace;

	if(mpf_audio_stream_rx_validate(source,NULL,NULL,pool) == FALSE) {
		return NULL;
	}

	descriptor = source->rx_descriptor;
	if(descriptor && mpf_codec_lpcm_descriptor_match(descriptor) == FALSE) {
		mpf_codec_t *codec = mpf_codec_manager_codec_get(codec_manager,descriptor,pool);
		if(codec) {
			/* set decoder before bridge */
			mpf_audio_stream_t *decoder = mpf_decoder_create(source,codec,pool);
			source = decoder;
		}
	}
	multiplier->source = source;
	mpf_audio_stream_rx_open(source,NULL);
	
	for(i=0; i<sink_count; i++)	{
		sink = sink_arr[i];
		if(!sink) continue;

		if(mpf_audio_stream_tx_validate(sink,NULL,NULL,pool) == FALSE) {
			continue;
		}

		descriptor = sink->tx_descriptor;
		if(descriptor && mpf_codec_lpcm_descriptor_match(descriptor) == FALSE) {
			mpf_codec_t *codec = mpf_codec_manager_codec_get(codec_manager,descriptor,pool);
			if(codec) {
				/* set encoder after bridge */
				mpf_audio_stream_t *encoder = mpf_encoder_create(sink,codec,pool);
				sink = encoder;
			}
		}
		sink_arr[i] = sink;
		mpf_audio_stream_tx_open(sink,NULL);
	}
	multiplier->sink_arr = sink_arr;
	multiplier->sink_count = sink_count;
	
	descriptor = source->rx_descriptor;
	frame_size = mpf_codec_linear_frame_size_calculate(descriptor->sampling_rate,descriptor->channel_count);
	multiplier->frame.codec_frame.size = frame_size;
	multiplier->frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	return &multiplier->base;
}
