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

#include "mpf_decoder.h"
#include "apt_log.h"

typedef struct mpf_decoder_t mpf_decoder_t;

struct mpf_decoder_t {
	mpf_audio_stream_t *base;
	mpf_audio_stream_t *source;
	mpf_frame_t         frame_in;
};


static apt_bool_t mpf_decoder_destroy(mpf_audio_stream_t *stream)
{
	mpf_decoder_t *decoder = stream->obj;
	return mpf_audio_stream_destroy(decoder->source);
}

static apt_bool_t mpf_decoder_open(mpf_audio_stream_t *stream)
{
	mpf_decoder_t *decoder = stream->obj;
	return mpf_audio_stream_rx_open(decoder->source);
}

static apt_bool_t mpf_decoder_close(mpf_audio_stream_t *stream)
{
	mpf_decoder_t *decoder = stream->obj;
	return mpf_audio_stream_rx_close(decoder->source);
}

static apt_bool_t mpf_decoder_process(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	mpf_decoder_t *decoder = stream->obj;
	if(mpf_audio_stream_frame_read(decoder->source,&decoder->frame_in) != TRUE) {
		return FALSE;
	}

	frame->type = decoder->frame_in.type;
	if((frame->type & MEDIA_FRAME_TYPE_EVENT) == MEDIA_FRAME_TYPE_EVENT) {
		frame->event_frame = decoder->frame_in.event_frame;
	}
	if((frame->type & MEDIA_FRAME_TYPE_AUDIO) == MEDIA_FRAME_TYPE_AUDIO) {
		mpf_codec_decode(decoder->source->rx_codec,&decoder->frame_in.codec_frame,&frame->codec_frame);
	}
	return TRUE;
}


static const mpf_audio_stream_vtable_t vtable = {
	mpf_decoder_destroy,
	mpf_decoder_open,
	mpf_decoder_close,
	mpf_decoder_process,
	NULL,
	NULL,
	NULL
};

MPF_DECLARE(mpf_audio_stream_t*) mpf_decoder_create(mpf_audio_stream_t *source, apr_pool_t *pool)
{
	apr_size_t frame_size;
	mpf_codec_t *codec;
	mpf_decoder_t *decoder;
	if(!source || !source->rx_codec) {
		return NULL;
	}
	decoder = apr_palloc(pool,sizeof(mpf_decoder_t));
	decoder->base = mpf_audio_stream_create(decoder,&vtable,STREAM_MODE_RECEIVE,pool);
	decoder->source = source;

	codec = source->rx_codec;
	frame_size = mpf_codec_frame_size_calculate(codec->descriptor,codec->attribs);
	decoder->base->rx_codec = codec;
	decoder->frame_in.codec_frame.size = frame_size;
	decoder->frame_in.codec_frame.buffer = apr_palloc(pool,frame_size);
	return decoder->base;
}
