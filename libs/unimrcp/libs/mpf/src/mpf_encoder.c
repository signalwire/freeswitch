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
 * $Id: mpf_encoder.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mpf_encoder.h"
#include "apt_log.h"

typedef struct mpf_encoder_t mpf_encoder_t;

struct mpf_encoder_t {
	mpf_audio_stream_t *base;
	mpf_audio_stream_t *sink;
	mpf_codec_t        *codec;
	mpf_frame_t         frame_out;
};


static apt_bool_t mpf_encoder_destroy(mpf_audio_stream_t *stream)
{
	mpf_encoder_t *encoder = stream->obj;
	return mpf_audio_stream_destroy(encoder->sink);
}

static apt_bool_t mpf_encoder_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	mpf_encoder_t *encoder = stream->obj;
	mpf_codec_open(encoder->codec);
	return mpf_audio_stream_tx_open(encoder->sink,encoder->codec);
}

static apt_bool_t mpf_encoder_close(mpf_audio_stream_t *stream)
{
	mpf_encoder_t *encoder = stream->obj;
	mpf_codec_close(encoder->codec);
	return mpf_audio_stream_tx_close(encoder->sink);
}

static apt_bool_t mpf_encoder_process(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	mpf_encoder_t *encoder = stream->obj;

	encoder->frame_out.type = frame->type;
	encoder->frame_out.marker = frame->marker;
	if((frame->type & MEDIA_FRAME_TYPE_EVENT) == MEDIA_FRAME_TYPE_EVENT) {
		encoder->frame_out.event_frame = frame->event_frame;
	}
	if((frame->type & MEDIA_FRAME_TYPE_AUDIO) == MEDIA_FRAME_TYPE_AUDIO) {
		mpf_codec_encode(encoder->codec,&frame->codec_frame,&encoder->frame_out.codec_frame);
	}
	return mpf_audio_stream_frame_write(encoder->sink,&encoder->frame_out);
}

static void mpf_encoder_trace(mpf_audio_stream_t *stream, mpf_stream_direction_e direction, apt_text_stream_t *output)
{
	apr_size_t offset;
	mpf_codec_descriptor_t *descriptor;
	mpf_encoder_t *encoder = stream->obj;

	descriptor = encoder->base->tx_descriptor;
	if(descriptor) {
		offset = output->pos - output->text.buf;
		output->pos += apr_snprintf(output->pos, output->text.length - offset,
			"[%s/%d/%d]->Encoder->",
			descriptor->name.buf,
			descriptor->sampling_rate,
			descriptor->channel_count);
	}

	mpf_audio_stream_trace(encoder->sink,direction,output);
}


static const mpf_audio_stream_vtable_t vtable = {
	mpf_encoder_destroy,
	NULL,
	NULL,
	NULL,
	mpf_encoder_open,
	mpf_encoder_close,
	mpf_encoder_process,
	mpf_encoder_trace
};

MPF_DECLARE(mpf_audio_stream_t*) mpf_encoder_create(mpf_audio_stream_t *sink, mpf_codec_t *codec, apr_pool_t *pool)
{
	apr_size_t frame_size;
	mpf_encoder_t *encoder;
	mpf_stream_capabilities_t *capabilities;
	if(!sink || !codec) {
		return NULL;
	}
	encoder = apr_palloc(pool,sizeof(mpf_encoder_t));
	capabilities = mpf_stream_capabilities_create(STREAM_DIRECTION_SEND,pool);
	encoder->base = mpf_audio_stream_create(encoder,&vtable,capabilities,pool);
	if(!encoder->base) {
		return NULL;
	}
	encoder->base->tx_descriptor = mpf_codec_lpcm_descriptor_create(
		sink->tx_descriptor->sampling_rate,
		sink->tx_descriptor->channel_count,
		pool);
	encoder->base->tx_event_descriptor = sink->tx_event_descriptor;
	
	encoder->sink = sink;
	encoder->codec = codec;

	frame_size = mpf_codec_frame_size_calculate(sink->tx_descriptor,codec->attribs);
	encoder->frame_out.codec_frame.size = frame_size;
	encoder->frame_out.codec_frame.buffer = apr_palloc(pool,frame_size);
	return encoder->base;
}
