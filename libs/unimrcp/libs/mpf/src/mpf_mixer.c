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
 * $Id: mpf_mixer.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mpf_mixer.h"
#include "mpf_encoder.h"
#include "mpf_decoder.h"
#include "mpf_resampler.h"
#include "mpf_codec_manager.h"
#include "apt_log.h"

typedef struct mpf_mixer_t mpf_mixer_t;

/** MPF mixer derived from MPF object */
struct mpf_mixer_t {
	/** MPF mixer base */
	mpf_object_t         base;
	/** Array of audio sources */
	mpf_audio_stream_t **source_arr;
	/** Number of audio sources */
	apr_size_t           source_count;
	/** Audio sink */
	mpf_audio_stream_t  *sink;

	/** Frame to read from audio source */
	mpf_frame_t          frame;
	/** Mixed frame to write to audio sink */
	mpf_frame_t          mix_frame;
};

static apt_bool_t mpf_frames_mix(mpf_frame_t *mix_frame, const mpf_frame_t *frame)
{
	apr_size_t i;
	apr_int16_t *mix_buf = mix_frame->codec_frame.buffer;
	const apr_int16_t *buf = frame->codec_frame.buffer;
	apr_size_t samples = frame->codec_frame.size / sizeof(apr_int16_t);

	if(mix_frame->codec_frame.size != frame->codec_frame.size) {
		return FALSE;
	}

	if((frame->type & MEDIA_FRAME_TYPE_AUDIO) == MEDIA_FRAME_TYPE_AUDIO) {
		for(i=0; i<samples; i++) {
			/* overflow MUST be considered */
			mix_buf[i] = mix_buf[i] + buf[i];
		}
		mix_frame->type |= MEDIA_FRAME_TYPE_AUDIO;
	}

	return TRUE;
}

static apt_bool_t mpf_mixer_process(mpf_object_t *object)
{
	apr_size_t i;
	mpf_audio_stream_t *source;
	mpf_mixer_t *mixer = (mpf_mixer_t*) object;

	mixer->mix_frame.type = MEDIA_FRAME_TYPE_NONE;
	mixer->mix_frame.marker = MPF_MARKER_NONE;
	memset(mixer->mix_frame.codec_frame.buffer,0,mixer->mix_frame.codec_frame.size);
	for(i=0; i<mixer->source_count; i++) {
		source = mixer->source_arr[i];
		if(source) {
			mixer->frame.type = MEDIA_FRAME_TYPE_NONE;
			mixer->frame.marker = MPF_MARKER_NONE;
			source->vtable->read_frame(source,&mixer->frame);
			mpf_frames_mix(&mixer->mix_frame,&mixer->frame);
		}
	}
	mixer->sink->vtable->write_frame(mixer->sink,&mixer->mix_frame);
	return TRUE;
}

static apt_bool_t mpf_mixer_destroy(mpf_object_t *object)
{
	apr_size_t i;
	mpf_audio_stream_t *source;
	mpf_mixer_t *mixer = (mpf_mixer_t*) object;

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Destroy Mixer %s",object->name);
	for(i=0; i<mixer->source_count; i++)	{
		source = mixer->source_arr[i];
		if(source) {
			mpf_audio_stream_rx_close(source);
		}
	}
	mpf_audio_stream_tx_close(mixer->sink);
	return TRUE;
}

static void mpf_mixer_trace(mpf_object_t *object)
{
	mpf_mixer_t *mixer = (mpf_mixer_t*) object;
	apr_size_t i;
	mpf_audio_stream_t *source;
	char buf[2048];
	apr_size_t offset;

	apt_text_stream_t output;
	apt_text_stream_init(&output,buf,sizeof(buf)-1);

	for(i=0; i<mixer->source_count; i++)	{
		source = mixer->source_arr[i];
		if(source) {
			mpf_audio_stream_trace(source,STREAM_DIRECTION_RECEIVE,&output);
			apt_text_char_insert(&output,';');
		}
	}

	offset = output.pos - output.text.buf;
	output.pos += apr_snprintf(output.pos, output.text.length - offset,
		"->Mixer->");

	mpf_audio_stream_trace(mixer->sink,STREAM_DIRECTION_SEND,&output);

	*output.pos = '\0';
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Media Path %s %s",
		object->name,
		output.text.buf);
}

MPF_DECLARE(mpf_object_t*) mpf_mixer_create(
								mpf_audio_stream_t **source_arr, 
								apr_size_t source_count, 
								mpf_audio_stream_t *sink, 
								const mpf_codec_manager_t *codec_manager, 
								const char *name,
								apr_pool_t *pool)
{
	apr_size_t i;
	apr_size_t frame_size;
	mpf_codec_descriptor_t *descriptor;
	mpf_audio_stream_t *source;
	mpf_mixer_t *mixer;
	if(!source_arr || !source_count || !sink) {
		return NULL;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Create Mixer %s",name);
	mixer = apr_palloc(pool,sizeof(mpf_mixer_t));
	mixer->source_arr = NULL;
	mixer->source_count = 0;
	mixer->sink = NULL;
	mpf_object_init(&mixer->base,name);
	mixer->base.process = mpf_mixer_process;
	mixer->base.destroy = mpf_mixer_destroy;
	mixer->base.trace = mpf_mixer_trace;

	if(mpf_audio_stream_tx_validate(sink,NULL,NULL,pool) == FALSE) {
		return NULL;
	}

	descriptor = sink->tx_descriptor;
	if(descriptor && mpf_codec_lpcm_descriptor_match(descriptor) == FALSE) {
		mpf_codec_t *codec = mpf_codec_manager_codec_get(codec_manager,descriptor,pool);
		if(codec) {
			/* set encoder after mixer */
			mpf_audio_stream_t *encoder = mpf_encoder_create(sink,codec,pool);
			sink = encoder;
		}
	}
	mixer->sink = sink;
	mpf_audio_stream_tx_open(sink,NULL);

	for(i=0; i<source_count; i++)	{
		source = source_arr[i];
		if(!source) continue;

		if(mpf_audio_stream_rx_validate(source,NULL,NULL,pool) == FALSE) {
			continue;
		}

		descriptor = source->rx_descriptor;
		if(descriptor && mpf_codec_lpcm_descriptor_match(descriptor) == FALSE) {
			mpf_codec_t *codec = mpf_codec_manager_codec_get(codec_manager,descriptor,pool);
			if(codec) {
				/* set decoder before mixer */
				mpf_audio_stream_t *decoder = mpf_decoder_create(source,codec,pool);
				source = decoder;
			}
		}
		source_arr[i] = source;
		mpf_audio_stream_rx_open(source,NULL);
	}
	mixer->source_arr = source_arr;
	mixer->source_count = source_count;

	descriptor = sink->tx_descriptor;
	frame_size = mpf_codec_linear_frame_size_calculate(descriptor->sampling_rate,descriptor->channel_count);
	mixer->frame.codec_frame.size = frame_size;
	mixer->frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	mixer->mix_frame.codec_frame.size = frame_size;
	mixer->mix_frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	return &mixer->base;
}
