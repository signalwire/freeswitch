/*
 * Copyright 2009 Arsen Chaloyan
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

#include "mpf_frame_buffer.h"

struct mpf_frame_buffer_t {
	apr_byte_t         *raw_data;
	mpf_frame_t        *frames;
	apr_size_t          frame_count;
	apr_size_t          frame_size;

	apr_size_t          write_pos;
	apr_size_t          read_pos;

	apr_thread_mutex_t *guard;
	apr_pool_t         *pool;
};


mpf_frame_buffer_t* mpf_frame_buffer_create(apr_size_t frame_size, apr_size_t frame_count, apr_pool_t *pool)
{
	apr_size_t i;
	mpf_frame_t *frame;
	mpf_frame_buffer_t *buffer = apr_palloc(pool,sizeof(mpf_frame_buffer_t));

	buffer->frame_size = frame_size;
	buffer->frame_count = frame_count;
	buffer->raw_data = apr_palloc(pool,buffer->frame_size*buffer->frame_count);
	buffer->frames = apr_palloc(pool,sizeof(mpf_frame_t)*buffer->frame_count);
	for(i=0; i<buffer->frame_count; i++) {
		frame = &buffer->frames[i];
		frame->type = MEDIA_FRAME_TYPE_NONE;
		frame->marker = MPF_MARKER_NONE;
		frame->codec_frame.buffer = buffer->raw_data + i*buffer->frame_size;
	}

	buffer->write_pos = buffer->read_pos = 0;
	apr_thread_mutex_create(&buffer->guard,APR_THREAD_MUTEX_UNNESTED,pool);
	return buffer;
}

void mpf_frame_buffer_destroy(mpf_frame_buffer_t *buffer)
{
	if(buffer->guard) {
		apr_thread_mutex_destroy(buffer->guard);
		buffer->guard = NULL;
	}
}

apt_bool_t mpf_frame_buffer_restart(mpf_frame_buffer_t *buffer)
{
	buffer->write_pos = buffer->read_pos;
	return TRUE;
}

static APR_INLINE mpf_frame_t* mpf_frame_buffer_frame_get(mpf_frame_buffer_t *buffer, apr_size_t pos)
{
	apr_size_t index = pos % buffer->frame_count;
	return &buffer->frames[index];
}

apt_bool_t mpf_frame_buffer_write(mpf_frame_buffer_t *buffer, const mpf_frame_t *frame)
{
	mpf_frame_t *write_frame;
	void *data = frame->codec_frame.buffer;
	apr_size_t size = frame->codec_frame.size;

	apr_thread_mutex_lock(buffer->guard);
	while(buffer->write_pos - buffer->read_pos < buffer->frame_count && size >= buffer->frame_size) {
		write_frame = mpf_frame_buffer_frame_get(buffer,buffer->write_pos);
		write_frame->type = frame->type;
		write_frame->codec_frame.size = buffer->frame_size;
		memcpy(
			write_frame->codec_frame.buffer,
			data,
			write_frame->codec_frame.size);

		data = (char*)data + buffer->frame_size;
		size -= buffer->frame_size;
		buffer->write_pos ++;
	}

	apr_thread_mutex_unlock(buffer->guard);
	/* if size != 0 => non frame alligned or buffer is full */
	return size == 0 ? TRUE : FALSE;
}

apt_bool_t mpf_frame_buffer_read(mpf_frame_buffer_t *buffer, mpf_frame_t *media_frame)
{
	apr_thread_mutex_lock(buffer->guard);
	if(buffer->write_pos > buffer->read_pos) {
		/* normal read */
		mpf_frame_t *src_media_frame = mpf_frame_buffer_frame_get(buffer,buffer->read_pos);
		media_frame->type = src_media_frame->type;
		media_frame->marker = src_media_frame->marker;
		if(media_frame->type & MEDIA_FRAME_TYPE_AUDIO) {
			media_frame->codec_frame.size = src_media_frame->codec_frame.size;
			memcpy(
				media_frame->codec_frame.buffer,
				src_media_frame->codec_frame.buffer,
				media_frame->codec_frame.size);
		}
		if(media_frame->type & MEDIA_FRAME_TYPE_EVENT) {
			media_frame->event_frame = src_media_frame->event_frame;
		}
		src_media_frame->type = MEDIA_FRAME_TYPE_NONE;
		src_media_frame->marker = MPF_MARKER_NONE;
		buffer->read_pos ++;
	}
	else {
		/* underflow */
		media_frame->type = MEDIA_FRAME_TYPE_NONE;
		media_frame->marker = MPF_MARKER_NONE;
	}
	apr_thread_mutex_unlock(buffer->guard);
	return TRUE;
}
