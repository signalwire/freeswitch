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

#include "mpf_audio_file_stream.h"
#include "mpf_termination.h"
#include "mpf_frame.h"
#include "mpf_codec_manager.h"
#include "apt_log.h"

/** Audio file stream */
typedef struct mpf_audio_file_stream_t mpf_audio_file_stream_t;
struct mpf_audio_file_stream_t {
	mpf_audio_stream_t *audio_stream;

	FILE               *read_handle;
	FILE               *write_handle;

	apt_bool_t          eof;
	apr_size_t          max_write_size;
	apr_size_t          cur_write_size;
};

static APR_INLINE void mpf_audio_file_event_raise(mpf_audio_stream_t *stream, int event_id, void *descriptor);


static apt_bool_t mpf_audio_file_destroy(mpf_audio_stream_t *stream)
{
	mpf_audio_file_stream_t *file_stream = stream->obj;
	if(file_stream->read_handle) {
		fclose(file_stream->read_handle);
		file_stream->read_handle = NULL;
	}
	if(file_stream->write_handle) {
		fclose(file_stream->write_handle);
		file_stream->write_handle = NULL;
	}
	return TRUE;
}

static apt_bool_t mpf_audio_file_reader_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	return TRUE;
}

static apt_bool_t mpf_audio_file_reader_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_audio_file_frame_read(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	mpf_audio_file_stream_t *file_stream = stream->obj;
	if(file_stream->read_handle && file_stream->eof == FALSE) {
		if(fread(frame->codec_frame.buffer,1,frame->codec_frame.size,file_stream->read_handle) == frame->codec_frame.size) {
			frame->type = MEDIA_FRAME_TYPE_AUDIO;
		}
		else {
			file_stream->eof = TRUE;
			mpf_audio_file_event_raise(stream,0,NULL);
		}
	}
	return TRUE;
}


static apt_bool_t mpf_audio_file_writer_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	return TRUE;
}

static apt_bool_t mpf_audio_file_writer_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_audio_file_frame_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	mpf_audio_file_stream_t *file_stream = stream->obj;
	if(file_stream->write_handle && 
		(!file_stream->max_write_size || file_stream->cur_write_size < file_stream->max_write_size)) {
		file_stream->cur_write_size += fwrite(
										frame->codec_frame.buffer,
										1,
										frame->codec_frame.size,
										file_stream->write_handle);
		if(file_stream->cur_write_size >= file_stream->max_write_size) {
			mpf_audio_file_event_raise(stream,0,NULL);
		}
	}
	return TRUE;
}

static const mpf_audio_stream_vtable_t vtable = {
	mpf_audio_file_destroy,
	mpf_audio_file_reader_open,
	mpf_audio_file_reader_close,
	mpf_audio_file_frame_read,
	mpf_audio_file_writer_open,
	mpf_audio_file_writer_close,
	mpf_audio_file_frame_write
};

MPF_DECLARE(mpf_audio_stream_t*) mpf_file_stream_create(mpf_termination_t *termination, apr_pool_t *pool)
{
	mpf_audio_file_stream_t *file_stream = apr_palloc(pool,sizeof(mpf_audio_file_stream_t));
	mpf_stream_capabilities_t *capabilities = mpf_stream_capabilities_create(STREAM_DIRECTION_DUPLEX,pool);
	mpf_audio_stream_t *audio_stream = mpf_audio_stream_create(file_stream,&vtable,capabilities,pool);
	if(!audio_stream) {
		return NULL;
	}
	audio_stream->termination = termination;

	file_stream->audio_stream = audio_stream;
	file_stream->write_handle = NULL;
	file_stream->read_handle = NULL;
	file_stream->eof = FALSE;
	file_stream->max_write_size = 0;
	file_stream->cur_write_size = 0;

	return audio_stream;
}

MPF_DECLARE(apt_bool_t) mpf_file_stream_modify(mpf_audio_stream_t *stream, mpf_audio_file_descriptor_t *descriptor)
{
	mpf_audio_file_stream_t *file_stream = stream->obj;
	if(descriptor->mask & FILE_READER) {
		if(file_stream->read_handle) {
			fclose(file_stream->read_handle);
		}
		file_stream->read_handle = descriptor->read_handle;
		file_stream->eof = FALSE;
		stream->direction |= FILE_READER;

		stream->rx_descriptor = descriptor->codec_descriptor;
	}
	if(descriptor->mask & FILE_WRITER) {
		if(file_stream->write_handle) {
			fclose(file_stream->write_handle);
		}
		file_stream->write_handle = descriptor->write_handle;
		file_stream->max_write_size = descriptor->max_write_size;
		file_stream->cur_write_size = 0;
		stream->direction |= FILE_WRITER;

		stream->tx_descriptor = descriptor->codec_descriptor;
	}
	return TRUE;
}

static APR_INLINE void mpf_audio_file_event_raise(mpf_audio_stream_t *stream, int event_id, void *descriptor)
{
	if(stream->termination->event_handler) {
		stream->termination->event_handler(stream->termination,event_id,descriptor);
	}
}
