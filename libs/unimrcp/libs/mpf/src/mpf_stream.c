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
 * $Id: mpf_stream.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mpf_stream.h"

/** Create stream capabilities */
MPF_DECLARE(mpf_stream_capabilities_t*) mpf_stream_capabilities_create(mpf_stream_direction_e direction, apr_pool_t *pool)
{
	mpf_stream_capabilities_t *capabilities = (mpf_stream_capabilities_t*)apr_palloc(pool,sizeof(mpf_stream_capabilities_t));
	capabilities->direction = direction;
	mpf_codec_capabilities_init(&capabilities->codecs,1,pool);
	return capabilities;
}

/** Clone stream capabilities */
MPF_DECLARE(mpf_stream_capabilities_t*) mpf_stream_capabilities_clone(const mpf_stream_capabilities_t *src_capabilities, apr_pool_t *pool)
{
	mpf_stream_capabilities_t *capabilities = (mpf_stream_capabilities_t*)apr_palloc(pool,sizeof(mpf_stream_capabilities_t));
	capabilities->direction = src_capabilities->direction;
	mpf_codec_capabilities_clone(&capabilities->codecs,&src_capabilities->codecs,pool);
	return capabilities;
}

/** Merge stream capabilities */
MPF_DECLARE(apt_bool_t) mpf_stream_capabilities_merge(mpf_stream_capabilities_t *capabilities, const mpf_stream_capabilities_t *src_capabilities, apr_pool_t *pool)
{
	capabilities->direction |= src_capabilities->direction;
	return mpf_codec_capabilities_merge(&capabilities->codecs,&src_capabilities->codecs,pool);
}



/** Create audio stream */
MPF_DECLARE(mpf_audio_stream_t*) mpf_audio_stream_create(void *obj, const mpf_audio_stream_vtable_t *vtable, const mpf_stream_capabilities_t *capabilities, apr_pool_t *pool)
{
	mpf_audio_stream_t *stream;
	if(!vtable || !capabilities) {
		return NULL;
	}
	
	/* validate required fields */
	if(capabilities->direction & STREAM_DIRECTION_SEND) {
		/* validate sink */
		if(!vtable->write_frame) {
			return NULL;
		}
	}
	if(capabilities->direction & STREAM_DIRECTION_RECEIVE) {
		/* validate source */
		if(!vtable->read_frame) {
			return NULL;
		}
	}

	stream = (mpf_audio_stream_t*)apr_palloc(pool,sizeof(mpf_audio_stream_t));
	stream->obj = obj;
	stream->vtable = vtable;
	stream->termination = NULL;
	stream->capabilities = capabilities;
	stream->direction = capabilities->direction;
	stream->rx_descriptor = NULL;
	stream->rx_event_descriptor = NULL;
	stream->tx_descriptor = NULL;
	stream->tx_event_descriptor = NULL;
	return stream;
}

/** Validate audio stream receiver */
MPF_DECLARE(apt_bool_t) mpf_audio_stream_rx_validate(
									mpf_audio_stream_t *stream,
									const mpf_codec_descriptor_t *descriptor,
									const mpf_codec_descriptor_t *event_descriptor,
									apr_pool_t *pool)
{
	if(!stream->capabilities) {
		return FALSE;
	}

	if(!stream->rx_descriptor) {
		stream->rx_descriptor = mpf_codec_descriptor_create_by_capabilities(&stream->capabilities->codecs,descriptor,pool);
	}
	if(!stream->rx_event_descriptor) {
		if(stream->capabilities->codecs.allow_named_events == TRUE && event_descriptor) {
			stream->rx_event_descriptor = apr_palloc(pool,sizeof(mpf_codec_descriptor_t));
			*stream->rx_event_descriptor = *event_descriptor;
		}
	}

	return stream->rx_descriptor ? TRUE : FALSE;
}

/** Validate audio stream transmitter */
MPF_DECLARE(apt_bool_t) mpf_audio_stream_tx_validate(
									mpf_audio_stream_t *stream,
									const mpf_codec_descriptor_t *descriptor,
									const mpf_codec_descriptor_t *event_descriptor,
									apr_pool_t *pool)
{
	if(!stream->capabilities) {
		return FALSE;
	}

	if(!stream->tx_descriptor) {
		stream->tx_descriptor = mpf_codec_descriptor_create_by_capabilities(&stream->capabilities->codecs,descriptor,pool);
	}
	if(!stream->tx_event_descriptor) {
		if(stream->capabilities->codecs.allow_named_events == TRUE && event_descriptor) {
			stream->tx_event_descriptor = apr_palloc(pool,sizeof(mpf_codec_descriptor_t));
			*stream->tx_event_descriptor = *event_descriptor;
		}
	}
	return stream->tx_descriptor ? TRUE : FALSE;
}

/** Trace media path */
MPF_DECLARE(void) mpf_audio_stream_trace(mpf_audio_stream_t *stream, mpf_stream_direction_e direction, apt_text_stream_t *output)
{
	if(stream->vtable->trace) {
		stream->vtable->trace(stream,direction,output);
		return;
	}

	if(direction & STREAM_DIRECTION_SEND) {
		mpf_codec_descriptor_t *descriptor = stream->tx_descriptor;
		if(descriptor) {
			apr_size_t offset = output->pos - output->text.buf;
			output->pos += apr_snprintf(output->pos, output->text.length - offset,
				"[%s/%d/%d]->Sink",
				descriptor->name.buf,
				descriptor->sampling_rate,
				descriptor->channel_count);
		}
	}
	if(direction & STREAM_DIRECTION_RECEIVE) {
		mpf_codec_descriptor_t *descriptor = stream->rx_descriptor;
		if(descriptor) {
			apr_size_t offset = output->pos - output->text.buf;
			output->pos += apr_snprintf(output->pos, output->text.length - offset,
				"Source->[%s/%d/%d]",
				descriptor->name.buf,
				descriptor->sampling_rate,
				descriptor->channel_count);
		}
	}
}
