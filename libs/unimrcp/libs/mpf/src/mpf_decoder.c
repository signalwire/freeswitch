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
 * $Id: mpf_decoder.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mpf_decoder.h"
#include "apt_log.h"

typedef struct mpf_decoder_t mpf_decoder_t;

struct mpf_decoder_t {
	mpf_audio_stream_t *base;
	mpf_audio_stream_t *source;
	mpf_codec_t        *codec;
	mpf_frame_t         frame_in;
};


static apt_bool_t mpf_decoder_destroy(mpf_audio_stream_t *stream)
{
	mpf_decoder_t *decoder = stream->obj;
	return mpf_audio_stream_destroy(decoder->source);
}

static apt_bool_t mpf_decoder_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	mpf_decoder_t *decoder = stream->obj;
	mpf_codec_open(decoder->codec);
	return mpf_audio_stream_rx_open(decoder->source,decoder->codec);
}

static apt_bool_t mpf_decoder_close(mpf_audio_stream_t *stream)
{
	mpf_decoder_t *decoder = stream->obj;
	mpf_codec_close(decoder->codec);
	return mpf_audio_stream_rx_close(decoder->source);
}

static apt_bool_t mpf_decoder_process(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	mpf_decoder_t *decoder = stream->obj;
	decoder->frame_in.type = MEDIA_FRAME_TYPE_NONE;
	decoder->frame_in.marker = MPF_MARKER_NONE;
	if(mpf_audio_stream_frame_read(decoder->source,&decoder->frame_in) != TRUE) {
		return FALSE;
	}

	frame->type = decoder->frame_in.type;
	frame->marker = decoder->frame_in.marker;
	if((frame->type & MEDIA_FRAME_TYPE_EVENT) == MEDIA_FRAME_TYPE_EVENT) {
		frame->event_frame = decoder->frame_in.event_frame;
	}
	if((frame->type & MEDIA_FRAME_TYPE_AUDIO) == MEDIA_FRAME_TYPE_AUDIO) {
		mpf_codec_decode(decoder->codec,&decoder->frame_in.codec_frame,&frame->codec_frame);
	}
	return TRUE;
}

static void mpf_decoder_trace(mpf_audio_stream_t *stream, mpf_stream_direction_e direction, apt_text_stream_t *output)
{
	apr_size_t offset;
	mpf_codec_descriptor_t *descriptor;
	mpf_decoder_t *decoder = stream->obj;

	mpf_audio_stream_trace(decoder->source,direction,output);

	descriptor = decoder->base->rx_descriptor;
	if(descriptor) {
		offset = output->pos - output->text.buf;
		output->pos += apr_snprintf(output->pos, output->text.length - offset,
			"->Decoder->[%s/%d/%d]",
			descriptor->name.buf,
			descriptor->sampling_rate,
			descriptor->channel_count);
	}
}

static const mpf_audio_stream_vtable_t vtable = {
	mpf_decoder_destroy,
	mpf_decoder_open,
	mpf_decoder_close,
	mpf_decoder_process,
	NULL,
	NULL,
	NULL,
	mpf_decoder_trace
};

MPF_DECLARE(mpf_audio_stream_t*) mpf_decoder_create(mpf_audio_stream_t *source, mpf_codec_t *codec, apr_pool_t *pool)
{
	apr_size_t frame_size;
	mpf_decoder_t *decoder;
	mpf_stream_capabilities_t *capabilities;
	if(!source || !codec) {
		return NULL;
	}
	decoder = apr_palloc(pool,sizeof(mpf_decoder_t));
	capabilities = mpf_stream_capabilities_create(STREAM_DIRECTION_RECEIVE,pool);
	decoder->base = mpf_audio_stream_create(decoder,&vtable,capabilities,pool);
	if(!decoder->base) {
		return NULL;
	}
	decoder->base->rx_descriptor = mpf_codec_lpcm_descriptor_create(
		source->rx_descriptor->sampling_rate,
		source->rx_descriptor->channel_count,
		pool);
	decoder->base->rx_event_descriptor = source->rx_event_descriptor;

	decoder->source = source;
	decoder->codec = codec;

	frame_size = mpf_codec_frame_size_calculate(source->rx_descriptor,codec->attribs);
	decoder->frame_in.codec_frame.size = frame_size;
	decoder->frame_in.codec_frame.buffer = apr_palloc(pool,frame_size);
	return decoder->base;
}
