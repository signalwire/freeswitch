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

#include "mpf_jitter_buffer.h"

#if ENABLE_JB_TRACE
#define JB_TRACE printf
#else
static APR_INLINE void null_trace() {}
#define JB_TRACE null_trace
#endif

struct mpf_jitter_buffer_t {
	/* jitter buffer config */
	mpf_jb_config_t *config;
	/* codec to be used to dissect payload */
	mpf_codec_t     *codec;

	/* cyclic raw data */
	apr_byte_t      *raw_data;
	/* frames (out of raw data) */
	mpf_frame_t     *frames;
	/* number of frames */
	apr_size_t       frame_count;
	/* frame timestamp units (samples) */
	apr_uint32_t    frame_ts;
	/* frame size in bytes */
	apr_size_t       frame_size;

	/* playout delay in timetsamp units */
	apr_uint32_t     playout_delay_ts;

	/* write should be synchronized (offset calculated) */
	apr_byte_t       write_sync;
	/* write timestamp offset */
	int              write_ts_offset;
	
	/* write pointer in timestamp units */
	apr_uint32_t     write_ts;
	/* read pointer in timestamp units */
	apr_uint32_t     read_ts;

	/* timestamp event starts at */
	apr_uint32_t                   event_write_base_ts;
	/* the first (base) frame of the event */
	mpf_named_event_frame_t        event_write_base;
	/* the last received update for the event */
	const mpf_named_event_frame_t *event_write_update;
};


mpf_jitter_buffer_t* mpf_jitter_buffer_create(mpf_jb_config_t *jb_config, mpf_codec_descriptor_t *descriptor, mpf_codec_t *codec, apr_pool_t *pool)
{
	size_t i;
	mpf_frame_t *frame;
	mpf_jitter_buffer_t *jb = apr_palloc(pool,sizeof(mpf_jitter_buffer_t));
	if(!jb_config) {
		/* create default jb config */
		jb_config = apr_palloc(pool,sizeof(mpf_jb_config_t));
		mpf_jb_config_init(jb_config);
	}
	/* validate jb config */
	if(jb_config->initial_playout_delay == 0) {
		/* default configuration */
		jb_config->min_playout_delay = 10; /* ms */
		jb_config->initial_playout_delay = 50; /* ms */
		jb_config->max_playout_delay = 200; /* ms */
	}
	else {
		if(jb_config->min_playout_delay > jb_config->initial_playout_delay) {
			jb_config->min_playout_delay = jb_config->initial_playout_delay;
		}
		if(jb_config->max_playout_delay < jb_config->initial_playout_delay) {
			jb_config->max_playout_delay = 2 * jb_config->initial_playout_delay;
		}
	}
	jb->config = jb_config;
	jb->codec = codec;

	jb->frame_ts = (apr_uint32_t)mpf_codec_frame_samples_calculate(descriptor);
	jb->frame_size = mpf_codec_frame_size_calculate(descriptor,codec->attribs);
	jb->frame_count = jb->config->max_playout_delay / CODEC_FRAME_TIME_BASE;
	jb->raw_data = apr_palloc(pool,jb->frame_size*jb->frame_count);
	jb->frames = apr_palloc(pool,sizeof(mpf_frame_t)*jb->frame_count);
	for(i=0; i<jb->frame_count; i++) {
		frame = &jb->frames[i];
		frame->type = MEDIA_FRAME_TYPE_NONE;
		frame->marker = MPF_MARKER_NONE;
		frame->codec_frame.buffer = jb->raw_data + i*jb->frame_size;
	}

	jb->playout_delay_ts = (apr_uint32_t)(jb->config->initial_playout_delay *
		descriptor->channel_count * descriptor->sampling_rate / 1000);

	jb->write_sync = 1;
	jb->write_ts_offset = 0;
	jb->write_ts = jb->read_ts = 0;

	jb->event_write_base_ts = 0;
	memset(&jb->event_write_base,0,sizeof(mpf_named_event_frame_t));
	jb->event_write_update = NULL;

	return jb;
}

void mpf_jitter_buffer_destroy(mpf_jitter_buffer_t *jb)
{
}

apt_bool_t mpf_jitter_buffer_restart(mpf_jitter_buffer_t *jb)
{
	jb->write_sync = 1;
	jb->write_ts_offset = 0;
	jb->write_ts = jb->read_ts;

	jb->event_write_base_ts = 0;
	memset(&jb->event_write_base,0,sizeof(mpf_named_event_frame_t));
	jb->event_write_update = NULL;

	return TRUE;
}

static APR_INLINE mpf_frame_t* mpf_jitter_buffer_frame_get(mpf_jitter_buffer_t *jb, apr_size_t ts)
{
	apr_size_t index = (ts / jb->frame_ts) % jb->frame_count;
	return &jb->frames[index];
}

static APR_INLINE jb_result_t mpf_jitter_buffer_write_prepare(mpf_jitter_buffer_t *jb, apr_uint32_t ts, apr_uint32_t *write_ts)
{
	if(jb->write_sync) {
		jb->write_ts_offset = ts - jb->write_ts;
		jb->write_sync = 0;
	}

	*write_ts = ts - jb->write_ts_offset + jb->playout_delay_ts;
	if(*write_ts % jb->frame_ts != 0) {
		/* not frame alligned */
		return JB_DISCARD_NOT_ALLIGNED;
	}
	return JB_OK;
}

jb_result_t mpf_jitter_buffer_write(mpf_jitter_buffer_t *jb, void *buffer, apr_size_t size, apr_uint32_t ts)
{
	mpf_frame_t *media_frame;
	apr_uint32_t write_ts;
	apr_size_t available_frame_count;
	jb_result_t result = mpf_jitter_buffer_write_prepare(jb,ts,&write_ts);
	if(result != JB_OK) {
		return result;
	}

	if(write_ts >= jb->write_ts) {
		if(write_ts - jb->write_ts > jb->frame_ts) {
			/* gap */
		}
		/* normal write */
	}
	else {
		if(write_ts >= jb->read_ts) {
			/* backward write */
		}
		else {
			/* too late */
			JB_TRACE("JB write ts=%"APR_SIZE_T_FMT" too late\n",write_ts);
			return JB_DISCARD_TOO_LATE;
		}
	}
	available_frame_count = jb->frame_count - (write_ts - jb->read_ts)/jb->frame_ts;
	if(available_frame_count <= 0) {
		/* too early */
		JB_TRACE("JB write ts=%"APR_SIZE_T_FMT" too early\n",write_ts);
		return JB_DISCARD_TOO_EARLY;
	}

	JB_TRACE("JB write ts=%"APR_SIZE_T_FMT" size=%"APR_SIZE_T_FMT"\n",write_ts,size);
	while(available_frame_count && size) {
		media_frame = mpf_jitter_buffer_frame_get(jb,write_ts);
		media_frame->codec_frame.size = jb->frame_size;
		if(mpf_codec_dissect(jb->codec,&buffer,&size,&media_frame->codec_frame) == FALSE) {
			break;
		}

		media_frame->type |= MEDIA_FRAME_TYPE_AUDIO;
		write_ts += jb->frame_ts;
		available_frame_count--;
	}

	if(size) {
		/* no frame available to write, but some data remains in buffer (partialy too early) */
	}

	if(write_ts > jb->write_ts) {
		jb->write_ts = write_ts;
	}
	return result;
}

jb_result_t mpf_jitter_buffer_event_write(mpf_jitter_buffer_t *jb, const mpf_named_event_frame_t *named_event, apr_uint32_t ts, apr_byte_t marker)
{
	mpf_frame_t *media_frame;
	apr_uint32_t write_ts;
	jb_result_t result = mpf_jitter_buffer_write_prepare(jb,ts,&write_ts);
	if(result != JB_OK) {
		return result;
	}

	/* new event detection */
	if(!marker) {
		if(jb->event_write_base.event_id != named_event->event_id || !jb->event_write_update) {
			/* new event detected, marker is missing though */
			marker = 1;
		}
		else if(jb->event_write_base_ts != write_ts) {
			/* detect whether this is a new segment of the same event or new event with missing marker
			assuming a threshold which equals to 4 frames */
			if(write_ts > jb->event_write_base_ts + jb->event_write_update->duration + 4*jb->frame_ts) {
				/* new event detected, marker is missing though */
				marker = 1;
			}
			else {
				/* new segment of the same long-lasting event detected */
				jb->event_write_base = *named_event;
				jb->event_write_update = &jb->event_write_base;
				jb->event_write_base_ts = write_ts;
			}
		}
	}
	if(marker) {
		/* new event */
		jb->event_write_base = *named_event;
		jb->event_write_update = &jb->event_write_base;
		jb->event_write_base_ts = write_ts;
	}
	else {
		/* an update */
		if(named_event->duration <= jb->event_write_update->duration) {
			/* ignore this update, it's either a retransmission or
			something from the past, which makes no sense now */
			return JB_OK;
		}
		/* calculate position in jitter buffer considering the last received event (update) */
		write_ts += jb->event_write_update->duration;
	}

	if(write_ts < jb->read_ts) {
		/* too late */
		JB_TRACE("JB write ts=%"APR_SIZE_T_FMT" event=%d duration=%d too late\n",
			write_ts,named_event->event_id,named_event->duration);
		return JB_DISCARD_TOO_LATE;
	}
	else if( (write_ts - jb->read_ts)/jb->frame_ts >= jb->frame_count) {
		/* too early */
		JB_TRACE("JB write ts=%"APR_SIZE_T_FMT" event=%d duration=%d too early\n",
			write_ts,named_event->event_id,named_event->duration);
		return JB_DISCARD_TOO_EARLY;
	}

	JB_TRACE("JB write ts=%"APR_SIZE_T_FMT" event=%d duration=%d\n",
		write_ts,named_event->event_id,named_event->duration);
	media_frame = mpf_jitter_buffer_frame_get(jb,write_ts);
	media_frame->event_frame = *named_event;
	media_frame->type |= MEDIA_FRAME_TYPE_EVENT;
	if(marker) {
		media_frame->marker = MPF_MARKER_START_OF_EVENT;
	}
	else if(named_event->edge == 1) {
		media_frame->marker = MPF_MARKER_END_OF_EVENT;
	}
	jb->event_write_update = &media_frame->event_frame;

	write_ts += jb->frame_ts;
	if(write_ts > jb->write_ts) {
		jb->write_ts = write_ts;
	}
	return result;
}

apt_bool_t mpf_jitter_buffer_read(mpf_jitter_buffer_t *jb, mpf_frame_t *media_frame)
{
	mpf_frame_t *src_media_frame = mpf_jitter_buffer_frame_get(jb,jb->read_ts);
	if(jb->write_ts > jb->read_ts) {
		/* normal read */
		JB_TRACE("JB read ts=%"APR_SIZE_T_FMT"\n",	jb->read_ts);
		media_frame->type = src_media_frame->type;
		media_frame->marker = src_media_frame->marker;
		if(media_frame->type & MEDIA_FRAME_TYPE_AUDIO) {
			media_frame->codec_frame.size = src_media_frame->codec_frame.size;
			memcpy(media_frame->codec_frame.buffer,src_media_frame->codec_frame.buffer,media_frame->codec_frame.size);
		}
		if(media_frame->type & MEDIA_FRAME_TYPE_EVENT) {
			media_frame->event_frame = src_media_frame->event_frame;
		}
	}
	else {
		/* underflow */
		JB_TRACE("JB read ts=%"APR_SIZE_T_FMT" underflow\n", jb->read_ts);
		media_frame->type = MEDIA_FRAME_TYPE_NONE;
		media_frame->marker = MPF_MARKER_NONE;
		jb->write_ts += jb->frame_ts;
	}
	src_media_frame->type = MEDIA_FRAME_TYPE_NONE;
	src_media_frame->marker = MPF_MARKER_NONE;
	jb->read_ts += jb->frame_ts;
	return TRUE;
}
