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

struct mpf_jitter_buffer_t {
	mpf_jb_config_t *config;

	apr_byte_t      *raw_data;
	mpf_frame_t     *frames;
	apr_size_t       frame_count;
	apr_size_t       frame_ts;
	apr_size_t       frame_size;

	apr_size_t       playout_delay_ts;

	apr_byte_t       write_sync;
	int              write_ts_offset;
	
	apr_size_t       write_ts;
	apr_size_t       read_ts;

	apr_pool_t      *pool;
};


mpf_jitter_buffer_t* mpf_jitter_buffer_create(mpf_jb_config_t *jb_config, mpf_codec_t *codec, apr_pool_t *pool)
{
	size_t i;
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

	jb->frame_ts = mpf_codec_frame_samples_calculate(codec->descriptor);
	jb->frame_size = mpf_codec_frame_size_calculate(codec->descriptor,codec->attribs);
	jb->frame_count = jb->config->max_playout_delay / CODEC_FRAME_TIME_BASE;
	jb->raw_data = apr_palloc(pool,jb->frame_size*jb->frame_count);
	jb->frames = apr_palloc(pool,sizeof(mpf_frame_t)*jb->frame_count);
	for(i=0; i<jb->frame_count; i++) {
		jb->frames[i].type = MEDIA_FRAME_TYPE_NONE;
		jb->frames[i].codec_frame.buffer = jb->raw_data + i*jb->frame_size;
	}

	jb->playout_delay_ts = jb->config->initial_playout_delay *
		codec->descriptor->channel_count * codec->descriptor->sampling_rate / 1000;

	jb->write_sync = 1;
	jb->write_ts_offset = 0;
	jb->write_ts = jb->read_ts = 0;

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
	return TRUE;
}

static APR_INLINE mpf_frame_t* mpf_jitter_buffer_frame_get(mpf_jitter_buffer_t *jb, apr_size_t ts)
{
	apr_size_t index = (ts / jb->frame_ts) % jb->frame_count;
	return &jb->frames[index];
}

static APR_INLINE jb_result_t mpf_jitter_buffer_write_prepare(mpf_jitter_buffer_t *jb, apr_uint32_t ts, 
												apr_size_t *write_ts, apr_size_t *available_frame_count)
{
	jb_result_t result = JB_OK;
	if(jb->write_sync) {
		jb->write_ts_offset = ts - jb->write_ts;
		jb->write_sync = 0;
	}

	*write_ts = ts - jb->write_ts_offset + jb->playout_delay_ts;
	if(*write_ts % jb->frame_ts != 0) {
		/* not frame alligned */
		return JB_DISCARD_NOT_ALLIGNED;
	}

	if(*write_ts >= jb->write_ts) {
		if(*write_ts - jb->write_ts > jb->frame_ts) {
			/* gap */
		}
		/* normal write */
	}
	else {
		if(*write_ts >= jb->read_ts) {
			/* backward write */
		}
		else {
			/* too late */
			result = JB_DISCARD_TOO_LATE;
		}
	}
	*available_frame_count = jb->frame_count - (*write_ts - jb->read_ts)/jb->frame_ts;
	return result;
}

jb_result_t mpf_jitter_buffer_write(mpf_jitter_buffer_t *jb, mpf_codec_t *codec, void *buffer, apr_size_t size, apr_uint32_t ts)
{
	mpf_frame_t *media_frame;
	apr_size_t write_ts;
	apr_size_t available_frame_count = 0;
	jb_result_t result = mpf_jitter_buffer_write_prepare(jb,ts,&write_ts,&available_frame_count);
	if(result != JB_OK) {
		return result;
	}

	while(available_frame_count && size) {
		media_frame = mpf_jitter_buffer_frame_get(jb,write_ts);
		media_frame->codec_frame.size = jb->frame_size;
		if(mpf_codec_dissect(codec,&buffer,&size,&media_frame->codec_frame) == FALSE) {
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

jb_result_t mpf_jitter_buffer_write_named_event(mpf_jitter_buffer_t *jb, mpf_named_event_frame_t *named_event, apr_uint32_t ts)
{
	return JB_OK;
}

apt_bool_t mpf_jitter_buffer_read(mpf_jitter_buffer_t *jb, mpf_frame_t *media_frame)
{
	mpf_frame_t *src_media_frame = mpf_jitter_buffer_frame_get(jb,jb->read_ts);
	if(jb->write_ts > jb->read_ts) {
		/* normal read */
		media_frame->type = src_media_frame->type;
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
		media_frame->type = MEDIA_FRAME_TYPE_NONE;
		jb->write_ts += jb->frame_ts;
	}
	src_media_frame->type = MEDIA_FRAME_TYPE_NONE;
	jb->read_ts += jb->frame_ts;
	return TRUE;
}
