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
 * $Id: mpf_buffer.c 2181 2014-09-14 04:29:38Z achaloyan@gmail.com $
 */

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h>
#include "mpf_buffer.h"

typedef struct mpf_chunk_t mpf_chunk_t;

struct mpf_chunk_t {
	APR_RING_ENTRY(mpf_chunk_t) link;
	mpf_frame_t                 frame;
};

struct mpf_buffer_t {
	APR_RING_HEAD(mpf_chunk_head_t, mpf_chunk_t) head;
	mpf_chunk_t                                 *cur_chunk;
	apr_size_t                                   remaining_chunk_size;
	apr_thread_mutex_t                          *guard;
	apr_pool_t                                  *pool;
	apr_size_t                                   size; /* total size */
};

mpf_buffer_t* mpf_buffer_create(apr_pool_t *pool)
{
	mpf_buffer_t *buffer = apr_palloc(pool,sizeof(mpf_buffer_t));
	buffer->pool = pool;
	buffer->cur_chunk = NULL;
	buffer->remaining_chunk_size = 0;
	buffer->size = 0;
	APR_RING_INIT(&buffer->head, mpf_chunk_t, link);
	apr_thread_mutex_create(&buffer->guard,APR_THREAD_MUTEX_UNNESTED,pool);
	return buffer;
}

void mpf_buffer_destroy(mpf_buffer_t *buffer)
{
	if(buffer->guard) {
		apr_thread_mutex_destroy(buffer->guard);
		buffer->guard = NULL;
	}
}

apt_bool_t mpf_buffer_restart(mpf_buffer_t *buffer)
{
	apr_thread_mutex_lock(buffer->guard);
	APR_RING_INIT(&buffer->head, mpf_chunk_t, link);
	apr_thread_mutex_unlock(buffer->guard);
	return TRUE;
}

static APR_INLINE apt_bool_t mpf_buffer_chunk_write(mpf_buffer_t *buffer, mpf_chunk_t *chunk)
{
	APR_RING_INSERT_TAIL(&buffer->head,chunk,mpf_chunk_t,link);
	return TRUE;
}

static APR_INLINE mpf_chunk_t* mpf_buffer_chunk_read(mpf_buffer_t *buffer)
{
	mpf_chunk_t *chunk = NULL;
	if(!APR_RING_EMPTY(&buffer->head,mpf_chunk_t,link)) {
		chunk = APR_RING_FIRST(&buffer->head);
		APR_RING_REMOVE(chunk,link);
	}
	return chunk;
}

apt_bool_t mpf_buffer_audio_write(mpf_buffer_t *buffer, void *data, apr_size_t size)
{
	mpf_chunk_t *chunk;
	apt_bool_t status;
	apr_thread_mutex_lock(buffer->guard);

	chunk = apr_palloc(buffer->pool,sizeof(mpf_chunk_t));
	APR_RING_ELEM_INIT(chunk,link);
	chunk->frame.codec_frame.buffer = apr_palloc(buffer->pool,size);
	memcpy(chunk->frame.codec_frame.buffer,data,size);
	chunk->frame.codec_frame.size = size;
	chunk->frame.type = MEDIA_FRAME_TYPE_AUDIO;
	status = mpf_buffer_chunk_write(buffer,chunk);
	
	buffer->size += size;
	apr_thread_mutex_unlock(buffer->guard);
	return status;
}

apt_bool_t mpf_buffer_event_write(mpf_buffer_t *buffer, mpf_frame_type_e event_type)
{
	mpf_chunk_t *chunk;
	apt_bool_t status;
	apr_thread_mutex_lock(buffer->guard);

	chunk = apr_palloc(buffer->pool,sizeof(mpf_chunk_t));
	APR_RING_ELEM_INIT(chunk,link);
	chunk->frame.codec_frame.buffer = NULL;
	chunk->frame.codec_frame.size = 0;
	chunk->frame.type = event_type;
	status = mpf_buffer_chunk_write(buffer,chunk);
	
	apr_thread_mutex_unlock(buffer->guard);
	return status;
}

apt_bool_t mpf_buffer_frame_read(mpf_buffer_t *buffer, mpf_frame_t *media_frame)
{
	mpf_codec_frame_t *dest;
	mpf_codec_frame_t *src;
	apr_size_t remaining_frame_size = media_frame->codec_frame.size;
	apr_thread_mutex_lock(buffer->guard);
	do {
		if(!buffer->cur_chunk) {
			buffer->cur_chunk = mpf_buffer_chunk_read(buffer);
			if(!buffer->cur_chunk) {
				/* buffer is empty */
				break;
			}
			buffer->remaining_chunk_size = buffer->cur_chunk->frame.codec_frame.size;
		}

		dest = &media_frame->codec_frame;
		src = &buffer->cur_chunk->frame.codec_frame;
		media_frame->type |= buffer->cur_chunk->frame.type;
		if(remaining_frame_size < buffer->remaining_chunk_size) {
			/* copy remaining_frame_size */
			memcpy(
				(char*)dest->buffer + dest->size - remaining_frame_size,
				(char*)src->buffer + src->size - buffer->remaining_chunk_size,
				remaining_frame_size);
			buffer->remaining_chunk_size -= remaining_frame_size;
			buffer->size -= remaining_frame_size;
			remaining_frame_size = 0;
		}
		else {
			/* copy remaining_chunk_size and proceed to the next chunk */
			memcpy(
				(char*)dest->buffer + dest->size - remaining_frame_size,
				(char*)src->buffer + src->size - buffer->remaining_chunk_size,
				buffer->remaining_chunk_size);
			remaining_frame_size -= buffer->remaining_chunk_size;
			buffer->size -= buffer->remaining_chunk_size;
			buffer->remaining_chunk_size = 0;
			buffer->cur_chunk = NULL;
		}
	}
	while(remaining_frame_size);

	if(remaining_frame_size) {
		apr_size_t offset = media_frame->codec_frame.size - remaining_frame_size;
		memset((char*)media_frame->codec_frame.buffer + offset, 0, remaining_frame_size);
	}
	apr_thread_mutex_unlock(buffer->guard);
	return TRUE;
}

apr_size_t mpf_buffer_get_size(const mpf_buffer_t *buffer)
{
	return buffer->size;
}
