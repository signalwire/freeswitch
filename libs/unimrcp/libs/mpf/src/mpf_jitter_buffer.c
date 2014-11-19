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
 * $Id: mpf_jitter_buffer.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "mpf_jitter_buffer.h"
#include "mpf_trace.h"

#if ENABLE_JB_TRACE == 1
#define JB_TRACE printf
#elif ENABLE_JB_TRACE == 2
#define JB_TRACE mpf_debug_output_trace
#else
#define JB_TRACE mpf_null_trace
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
	apr_uint32_t     frame_ts;
	/* frame size in bytes */
	apr_size_t       frame_size;

	/* playout delay in timetsamp units */
	apr_uint32_t     playout_delay_ts;
	/* max playout delay in timetsamp units */
	apr_uint32_t     max_playout_delay_ts;

	/* write should be synchronized (offset calculated) */
	apr_byte_t       write_sync;
	/* write timestamp offset */
	apr_int32_t      write_ts_offset;
	
	/* write pointer in timestamp units */
	apr_uint32_t     write_ts;
	/* read pointer in timestamp units */
	apr_uint32_t     read_ts;

	/* min length of the buffer in timestamp units */
	apr_int32_t      min_length_ts;
	/* max length of the buffer in timestamp units */
	apr_int32_t      max_length_ts;
	/* number of statistical measurements made */
	apr_uint32_t     measurment_count;

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
	if(jb_config->min_playout_delay > jb_config->initial_playout_delay) {
		jb_config->min_playout_delay = jb_config->initial_playout_delay;
	}
	if(jb_config->max_playout_delay < jb_config->initial_playout_delay) {
		jb_config->max_playout_delay = 2 * jb_config->initial_playout_delay;
	}
	if(jb_config->max_playout_delay == 0) {
		jb_config->max_playout_delay = 600; /* ms */
	}
	
	jb->config = jb_config;
	jb->codec = codec;

	/* calculate and allocate frame related data */
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

	if(jb->config->initial_playout_delay % CODEC_FRAME_TIME_BASE != 0) {
		jb->config->initial_playout_delay += CODEC_FRAME_TIME_BASE - jb->config->initial_playout_delay % CODEC_FRAME_TIME_BASE;
	}

	/* calculate playout delay in timestamp units */
	jb->playout_delay_ts = jb->frame_ts * jb->config->initial_playout_delay / CODEC_FRAME_TIME_BASE;
	jb->max_playout_delay_ts = jb->frame_ts * jb->config->max_playout_delay / CODEC_FRAME_TIME_BASE;

	jb->write_sync = 1;
	jb->write_ts_offset = 0;
	jb->write_ts = jb->read_ts = 0;

	jb->min_length_ts = jb->max_length_ts = 0;
	jb->measurment_count = 0;

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

	if(jb->config->adaptive && jb->playout_delay_ts == jb->max_playout_delay_ts) {
		jb->playout_delay_ts = jb->frame_ts * jb->config->initial_playout_delay / CODEC_FRAME_TIME_BASE;
	}

	JB_TRACE("JB restart\n");
	return TRUE;
}

static APR_INLINE mpf_frame_t* mpf_jitter_buffer_frame_get(mpf_jitter_buffer_t *jb, apr_size_t ts)
{
	apr_size_t index = (ts / jb->frame_ts) % jb->frame_count;
	return &jb->frames[index];
}

static APR_INLINE void mpf_jitter_buffer_stat_update(mpf_jitter_buffer_t *jb)
{
	apr_int32_t length_ts;

	if(jb->measurment_count == 50) {
		/* start over after every N measurements */
		apr_int32_t mean_length_ts = jb->min_length_ts + (jb->max_length_ts - jb->min_length_ts) / 2;
		JB_TRACE("JB stat length [%d : %d] playout delay=%u\n",
			jb->min_length_ts,jb->max_length_ts,jb->playout_delay_ts);
		jb->min_length_ts = jb->max_length_ts = mean_length_ts;
		jb->measurment_count = 0;
	}
	
	/* calculate current length of the buffer */
	length_ts = jb->write_ts - jb->read_ts;
	if(length_ts > jb->max_length_ts) {
		/* update max length */
		jb->max_length_ts = length_ts;
	}
	else if(length_ts < jb->min_length_ts) {
		/* update min length */
		jb->min_length_ts = length_ts;
	}
	/* increment the counter after every stat update */
	jb->measurment_count++;
}

static APR_INLINE void mpf_jitter_buffer_frame_allign(mpf_jitter_buffer_t *jb, apr_uint32_t *ts)
{
	if(*ts % jb->frame_ts != 0) 
		*ts -= *ts % jb->frame_ts;
}

static APR_INLINE jb_result_t mpf_jitter_buffer_write_prepare(mpf_jitter_buffer_t *jb, apr_uint32_t ts, apr_uint32_t *write_ts)
{
	if(jb->write_sync) {
		JB_TRACE("JB write sync playout delay=%u\n",jb->playout_delay_ts);
		/* calculate the offset */
		jb->write_ts_offset = ts - jb->read_ts;
		jb->write_sync = 0;
	
		if(jb->config->time_skew_detection) {
			/* reset the statistics */
			jb->min_length_ts = jb->max_length_ts = jb->playout_delay_ts;
			jb->measurment_count = 0;
		}
	}

	/* calculate the write pos taking into account current offset and playout delay */
	*write_ts = ts - jb->write_ts_offset + jb->playout_delay_ts;
	if(*write_ts % jb->frame_ts != 0) {
		/* allign with frame_ts */
		apr_uint32_t delta_ts = *write_ts % jb->frame_ts;
		JB_TRACE("JB write allign ts=%u delta_ts=-%u\n",*write_ts,delta_ts);
		*write_ts -= delta_ts;
	}
	return JB_OK;
}

jb_result_t mpf_jitter_buffer_write(mpf_jitter_buffer_t *jb, void *buffer, apr_size_t size, apr_uint32_t ts, apr_byte_t marker)
{
	mpf_frame_t *media_frame;
	apr_uint32_t write_ts;
	apr_size_t available_frame_count;
	jb_result_t result;

	if(marker) {
		JB_TRACE("JB marker\n");
		/* new talkspurt detected => test whether the buffer is empty */
		if(jb->write_ts <= jb->read_ts) {
			/* resync */
			jb->write_sync = 1;
		}
	}

	/* calculate write_ts */
	result = mpf_jitter_buffer_write_prepare(jb,ts,&write_ts);
	if(result != JB_OK) {
		return result;
	}

	if(write_ts >= jb->read_ts) {
		if(write_ts >= jb->write_ts) {
			/* normal order */
		}
		else {
			/* out of order */
		}
	}
	else {
		apr_uint32_t delta_ts;
		/* packet arrived too late */
		if(write_ts < jb->write_ts) {
			/* out of order => discard */
			JB_TRACE("JB write ts=%u out of order, too late => discard\n",write_ts);
			return JB_DISCARD_TOO_LATE;
		}

		/* calculate a minimal adjustment needed in order to place the packet into the buffer */
		delta_ts = jb->read_ts - write_ts;

		if(jb->config->time_skew_detection) {
			JB_TRACE("JB stat length [%d : %d] playout delay=%u delta=%u\n",
				jb->min_length_ts,jb->max_length_ts,jb->playout_delay_ts,delta_ts);
			
			if((apr_uint32_t)(jb->max_length_ts - jb->min_length_ts) > jb->playout_delay_ts + delta_ts) {
				/* update the adjustment based on the collected statistics */
				delta_ts = (apr_uint32_t)(jb->max_length_ts - jb->min_length_ts) - jb->playout_delay_ts;
				mpf_jitter_buffer_frame_allign(jb,&delta_ts);
			}

			/* determine if there might be a time skew or not */
			if(jb->max_length_ts > 0 && (apr_uint32_t)jb->max_length_ts < jb->playout_delay_ts) {
				/* calculate the time skew */
				apr_uint32_t skew_ts = jb->playout_delay_ts - jb->max_length_ts;
				mpf_jitter_buffer_frame_allign(jb,&skew_ts);
				JB_TRACE("JB time skew detected offset=%u\n",skew_ts);

				/* adjust the offset and write pos */
				jb->write_ts_offset -= skew_ts;
				write_ts = ts - jb->write_ts_offset + jb->playout_delay_ts;

				/* adjust the statistics */
				jb->min_length_ts += skew_ts;
				jb->max_length_ts += skew_ts;

				if(skew_ts < delta_ts) {
					delta_ts -= skew_ts;
				}
				else {
					delta_ts = 0;
				}
			}
		}

		if(delta_ts) {
			if(jb->config->adaptive == 0) {
				/* jitter buffer is not adaptive => discard the packet */
				JB_TRACE("JB write ts=%u too late => discard\n",write_ts);
				return JB_DISCARD_TOO_LATE;
			}

			if(jb->playout_delay_ts + delta_ts > jb->max_playout_delay_ts) {
				/* max playout delay will be reached => discard the packet */
				JB_TRACE("JB write ts=%u max playout delay reached => discard\n",write_ts);
				return JB_DISCARD_TOO_LATE;
			}

			/* adjust the playout delay */
			jb->playout_delay_ts += delta_ts;
			write_ts += delta_ts;
			JB_TRACE("JB adjust playout delay=%u delta=%u\n",jb->playout_delay_ts,delta_ts);

			if(jb->config->time_skew_detection) {
				/* adjust the statistics */
				jb->min_length_ts += delta_ts;
				jb->max_length_ts += delta_ts;
			}
		}
	}

	/* get number of frames available to write */
	available_frame_count = jb->frame_count - (write_ts - jb->read_ts)/jb->frame_ts;
	if(available_frame_count <= 0) {
		/* too early */
		JB_TRACE("JB write ts=%u too early => discard\n",write_ts);
		return JB_DISCARD_TOO_EARLY;
	}

	JB_TRACE("JB write ts=%u size=%"APR_SIZE_T_FMT"\n",write_ts,size);
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
		/* advance write pos */
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
		if(named_event->duration < jb->event_write_update->duration) {
			/* ignore this update, it's something from the past, which makes no sense now */
			return JB_OK;
		}
		else if(named_event->duration == jb->event_write_update->duration) {
			/* this should be a retransmission, let it go through only if it contains new data */
			if(jb->event_write_update->edge == 1 || jb->event_write_update->edge == named_event->edge) {
				/* ignore this update since either the end of event marker has already been set,
				or the current event provides no updates */
				return JB_OK;
			}
		}

		/* calculate position in jitter buffer considering the last received event (update) */
		write_ts += jb->event_write_update->duration;
	}

	if(write_ts < jb->read_ts) {
		/* too late */
		apr_uint32_t delta_ts;
		if(jb->config->adaptive == 0) {
			/* jitter buffer is not adaptive => discard the packet */
			JB_TRACE("JB write ts=%u event=%d duration=%d too late => discard\n",
				write_ts,named_event->event_id,named_event->duration);
			return JB_DISCARD_TOO_LATE;
		}

		/* calculate a minimal adjustment needed in order to place the packet into the buffer */
		delta_ts = jb->read_ts - write_ts;

		if(jb->playout_delay_ts + delta_ts > jb->max_playout_delay_ts) {
			/* max playout delay will be reached => discard the packet */
			JB_TRACE("JB write ts=%u event=%d duration=%d max playout delay reached => discard\n",
				write_ts,named_event->event_id,named_event->duration);
			return JB_DISCARD_TOO_LATE;
		}

		/* adjust the playout delay */
		jb->playout_delay_ts += delta_ts;
		write_ts += delta_ts;
		if(marker) {
			jb->event_write_base_ts = write_ts;
		}
		JB_TRACE("JB adjust playout delay=%u delta=%u\n",jb->playout_delay_ts,delta_ts);
	}
	else if( (write_ts - jb->read_ts)/jb->frame_ts >= jb->frame_count) {
		/* too early */
		JB_TRACE("JB write ts=%u event=%d duration=%d too early => discard\n",
			write_ts,named_event->event_id,named_event->duration);
		return JB_DISCARD_TOO_EARLY;
	}

	media_frame = mpf_jitter_buffer_frame_get(jb,write_ts);
	media_frame->event_frame = *named_event;
	media_frame->type |= MEDIA_FRAME_TYPE_EVENT;
	if(marker) {
		media_frame->marker = MPF_MARKER_START_OF_EVENT;
	}
	else if(named_event->edge == 1) {
		media_frame->marker = MPF_MARKER_END_OF_EVENT;
	}
	JB_TRACE("JB write ts=%u event=%d duration=%d marker=%d\n",
		write_ts,named_event->event_id,named_event->duration,media_frame->marker);
	jb->event_write_update = &media_frame->event_frame;

	write_ts += jb->frame_ts;
	if(write_ts > jb->write_ts) {
		/* advance write pos */
		jb->write_ts = write_ts;
	}
	return result;
}

apt_bool_t mpf_jitter_buffer_read(mpf_jitter_buffer_t *jb, mpf_frame_t *media_frame)
{
	mpf_frame_t *src_media_frame = mpf_jitter_buffer_frame_get(jb,jb->read_ts);
	if(jb->write_ts > jb->read_ts) {
		/* normal read */
		JB_TRACE("JB read ts=%u\n",	jb->read_ts);
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
		JB_TRACE("JB read ts=%u underflow\n", jb->read_ts);
		media_frame->type = MEDIA_FRAME_TYPE_NONE;
		media_frame->marker = MPF_MARKER_NONE;
	}
	src_media_frame->type = MEDIA_FRAME_TYPE_NONE;
	src_media_frame->marker = MPF_MARKER_NONE;
	/* advance read pos */
	jb->read_ts += jb->frame_ts;
	
	if(jb->config->time_skew_detection) {
		/* update statistics after every read */
		mpf_jitter_buffer_stat_update(jb);
	}
	return TRUE;
}

apr_uint32_t mpf_jitter_buffer_playout_delay_get(const mpf_jitter_buffer_t *jb)
{
	if(jb->config->adaptive == 0) {
		return jb->config->initial_playout_delay;
	}

	return jb->playout_delay_ts * CODEC_FRAME_TIME_BASE / jb->frame_ts;
}
