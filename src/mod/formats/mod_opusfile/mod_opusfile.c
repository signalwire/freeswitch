/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Dragos Oancea <dragos.oancea@nexmo.com> (mod_opusfile.c)
 *
 *
 * mod_opusfile.c -- Read and Write OGG/Opus files . Some parts inspired from mod_shout.c, libopusfile, libopusenc
 *
 */
#include <switch.h>

#include <opusfile.h>

#ifdef HAVE_OPUSFILE_ENCODE
#include <opus/opusenc.h>
#endif 

#define OPUSFILE_MAX 32*1024 
#define TC_BUFFER_SIZE 1024 * 256 /* max ammount of decoded audio we can have at a time (bytes)*/
#define DEFAULT_RATE 48000 /* default fullband */
#define OPUS_MAX_PCM 5760 /* opus recommended max output buf */

#define OPUSSTREAM_MAX 64*1024 
#define OGG_MIN_PAGE_SIZE 2400 // this much data buffered before trying to open the incoming stream
#define OGG_MAX_PAGE_SIZE 65307 // a bit less than 64k, standard ogg

#define PAGES_PER_SEC 4

//#define LIMIT_DROP

#ifdef LIMIT_DROP
#define MIN_OGG_PAYLOAD 40 // drop incoming frames smaller than this (decoder)
#endif 

//#undef HAVE_OPUSFILE_ENCODE  /*don't encode anything */

SWITCH_MODULE_LOAD_FUNCTION(mod_opusfile_load);
SWITCH_MODULE_DEFINITION(mod_opusfile, mod_opusfile_load, NULL, NULL);

struct opus_file_context {
	switch_file_t *fd;
	OggOpusFile *of;
	ogg_int64_t duration;
	int output_seekable;
	ogg_int64_t pcm_offset;
	ogg_int64_t pcm_print_offset;
	ogg_int64_t next_pcm_offset;
	ogg_int64_t nsamples;
	opus_int32  bitrate;
	int li;
	int prev_li;
	switch_mutex_t *audio_mutex;
	switch_buffer_t *audio_buffer;
	switch_mutex_t *ogg_mutex;
	switch_buffer_t *ogg_buffer;
	opus_int16 decode_buf[OPUS_MAX_PCM];
	switch_bool_t eof;
	switch_thread_rwlock_t *rwlock;
	switch_file_handle_t *handle;
	size_t samplerate;
	int frame_size;
	int channels;
	size_t buffer_seconds;
	size_t err;
	opus_int16 *opusbuf;
	switch_size_t opusbuflen;
	FILE *fp;
#ifdef HAVE_OPUSFILE_ENCODE
	OggOpusEnc *enc;
	OggOpusComments *comments;
	unsigned char encode_buf[OPUSFILE_MAX];
	int encoded_buflen;
	size_t samples_encode;
#endif
	switch_memory_pool_t *pool;
};

typedef struct opus_file_context opus_file_context;

struct opus_stream_context {
	switch_file_t *fd;
	OggOpusFile *of;
	ogg_int64_t duration;
	int output_seekable;
	ogg_int64_t pcm_offset;
	ogg_int64_t pcm_print_offset;
	ogg_int64_t next_pcm_offset;
	opus_int64 raw_offset;
	ogg_int64_t nsamples;
	opus_int32  bitrate;
	int li;
	int prev_li;
	switch_mutex_t *audio_mutex;
	switch_buffer_t *audio_buffer;
	switch_mutex_t *ogg_mutex;
	switch_buffer_t *ogg_buffer;
	unsigned char ogg_data[OGG_MAX_PAGE_SIZE * 2];
	unsigned int ogg_data_len;
	switch_bool_t read_stream;
	switch_bool_t dec_page_ready;
	opus_int16 decode_buf[OPUS_MAX_PCM];
	switch_bool_t eof;
	switch_thread_rwlock_t *rwlock;
	switch_file_handle_t *handle;
	size_t samplerate;
	int frame_size;
	int dec_channels;
	size_t err;
	opus_int16 *opusbuf;
	switch_size_t opusbuflen;
#ifdef HAVE_OPUSFILE_ENCODE
	OggOpusEnc *enc;
	OggOpusComments *comments;
	unsigned char encode_buf[OPUSSTREAM_MAX];
	int encoded_buflen;
	size_t samples_encode;
	int enc_channels;
	unsigned int enc_pagecount;
#endif
	unsigned int dec_count;
	switch_thread_t *read_stream_thread;
	switch_memory_pool_t *pool;
};

typedef struct opus_stream_context opus_stream_context_t;

static struct {
	int debug;
} globals;

static switch_status_t switch_opusfile_decode(opus_file_context *context, void *data, size_t max_bytes, int channels)
{
	int ret = 0;
	size_t buf_inuse;

	if (!context->of) {
		return SWITCH_STATUS_FALSE;
	}
	
	memset(context->decode_buf, 0, sizeof(context->decode_buf));
	switch_mutex_lock(context->audio_mutex);
	while (!(context->eof) && (buf_inuse = switch_buffer_inuse(context->audio_buffer)) <= max_bytes) {

		if (channels == 1) {
			ret = op_read(context->of, (opus_int16 *)context->decode_buf, OPUS_MAX_PCM, NULL);
		} else if (channels == 2) {
			ret = op_read_stereo(context->of, (opus_int16 *)context->decode_buf, OPUS_MAX_PCM);
		} else if (channels > 2) {
			ret = op_read(context->of, (opus_int16 *)context->decode_buf, OPUS_MAX_PCM, NULL);
		} else if ((channels > 255) || (channels < 1)) { 
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS File] Invalid number of channels");
				switch_mutex_unlock(context->audio_mutex);
				return SWITCH_STATUS_FALSE;
		}
		if (ret < 0) {
			switch(ret) {
			case OP_HOLE:	/* There was a hole in the data, and some samples may have been skipped. Call this function again to continue decoding past the hole.*/
			case OP_EREAD:	/*An underlying read operation failed. This may signal a truncation attack from an <https:> source.*/
			
			case OP_EFAULT: /*	An internal memory allocation failed. */

			case OP_EIMPL:	/*An unseekable stream encountered a new link that used a feature that is not implemented, such as an unsupported channel family.*/

			case OP_EINVAL:	/* The stream was only partially open. */

			case OP_ENOTFORMAT: /*	An unseekable stream encountered a new link that did not have any logical Opus streams in it. */

			case OP_EBADHEADER:	/*An unseekable stream encountered a new link with a required header packet that was not properly formatted, contained illegal values, or was missing altogether.*/

			case OP_EVERSION:	/*An unseekable stream encountered a new link with an ID header that contained an unrecognized version number.*/

			case OP_EBADPACKET: /*Failed to properly decode the next packet.*/

			case OP_EBADLINK:		/*We failed to find data we had seen before.*/

			case OP_EBADTIMESTAMP:		/*An unseekable stream encountered a new link with a starting timestamp that failed basic validity checks.*/

			default:
			    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS Decoder]: error decoding file: [%d]\n", ret);
				switch_mutex_unlock(context->audio_mutex);
				return SWITCH_STATUS_FALSE;
			}
		} else if (ret == 0) {
			/*The number of samples returned may be 0 if the buffer was too small to store even a single sample for both channels, or if end-of-file was reached*/
			if (globals.debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Decoder]: EOF reached [%d]\n", ret);
			}
			context->eof = SWITCH_TRUE;
			break;
		} else /* (ret > 0)*/ {
			/*The number of samples read per channel on success*/
			switch_buffer_write(context->audio_buffer, (opus_int16 *)context->decode_buf, ret * sizeof(opus_int16) * channels);

			if (globals.debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"[OGG/OPUS Decoder]: Read samples: %d. Wrote bytes to buffer: [%d] bytes in use: [%u]\n", ret, (int)(ret * sizeof(int16_t) * channels), (unsigned int)buf_inuse);
			}
		}
	}
	switch_mutex_unlock(context->audio_mutex);
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t switch_opusfile_open(switch_file_handle_t *handle, const char *path)
{
	opus_file_context *context;
	char *ext;
	int ret;

	if ((ext = strrchr(path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS File] Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	}
	ext++;

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(*context))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	context->pool = handle->memory_pool;

	switch_thread_rwlock_create(&(context->rwlock), context->pool);

	switch_thread_rwlock_rdlock(context->rwlock);

	switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->pool);

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		if (switch_buffer_create_dynamic(&context->audio_buffer, TC_BUFFER_SIZE, TC_BUFFER_SIZE * 2, 0) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			goto err;
		}
	}

	handle->samples = 0;
	handle->samplerate = context->samplerate = DEFAULT_RATE; /*open files at 48 khz always*/
	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 1;
	handle->speed = 0;
	handle->pos = 0;
	handle->private_info = context;
	context->handle = handle;
	memcpy(handle->file_path, path, strlen(path));

#ifdef HAVE_OPUSFILE_ENCODE
	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		int err; int mapping_family = 0;

		context->channels = handle->channels;
		context->samplerate = handle->samplerate;
		handle->seekable = 0;
		context->comments = ope_comments_create();
		ope_comments_add(context->comments, "METADATA", "Freeswitch/mod_opusfile");
		// opus_multistream_surround_encoder_get_size() in libopus will check these
		if ((context->channels > 2) && (context->channels <= 8)) {
			mapping_family = 1;
		} else if ((context->channels > 8) && (context->channels <= 255)) {
			mapping_family = 255;
		}
		context->enc = ope_encoder_create_file(handle->file_path, context->comments, context->samplerate, context->channels, mapping_family, &err);
		if (!context->enc) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open file for writing [%d] [%s]\n", err, ope_strerror(err));
			switch_thread_rwlock_unlock(context->rwlock);
			return SWITCH_STATUS_FALSE;
		}
		switch_thread_rwlock_unlock(context->rwlock);
		return SWITCH_STATUS_SUCCESS;
	}
#endif 
	
	context->of = op_open_file(path, &ret);
	if (!context->of) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS File] Error opening %s\n", path);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_test_flag(handle, SWITCH_FILE_WRITE_APPEND)) {
		op_pcm_seek(context->of, 0); // overwrite
		handle->pos = 0;
	}

	context->prev_li = -1;
	context->nsamples = 0;

	handle->channels = context->channels = op_channel_count(context->of, -1);
	context->pcm_offset = op_pcm_tell(context->of);

	if(context->pcm_offset!=0){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS File] Non-zero starting PCM offset: [%li]\n", (long)context->pcm_offset);
	}

	context->eof = SWITCH_FALSE;
	context->pcm_print_offset = context->pcm_offset - DEFAULT_RATE;
	context->bitrate = 0;
	context->buffer_seconds = 1;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS File] Opening File [%s] %dhz\n", path, handle->samplerate);

	context->li = op_current_link(context->of);

	if (context->li != context->prev_li) {
		const OpusHead *head;
		const OpusTags *tags;
		head=op_head(context->of, context->li);
		if (head) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS File] Channels: %i\n", head->channel_count);
			if (head->input_sample_rate) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS File] Original sampling rate: %lu Hz\n", (unsigned long)head->input_sample_rate);
			}
		}
		if (op_seekable(context->of)) {
			ogg_int64_t duration;
			opus_int64  size;
			duration = op_pcm_total(context->of, context->li);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO , "[OGG/OPUS File] Duration (samples): %u\n", (unsigned int)duration);
			size = op_raw_total(context->of, context->li);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[OGG/OPUS File] Size (bytes): %u\n", (unsigned int)size);
		}
		tags = op_tags(context->of, context->li);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS File] Encoded by: %s\n", tags->vendor);
	}

	switch_thread_rwlock_unlock(context->rwlock);
	return SWITCH_STATUS_SUCCESS;

err:
	switch_thread_rwlock_unlock(context->rwlock);

	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_opusfile_close(switch_file_handle_t *handle)
{
	opus_file_context *context = handle->private_info;

	switch_thread_rwlock_rdlock(context->rwlock);
	if (context->of) {
		op_free(context->of);
	}
#ifdef HAVE_OPUSFILE_ENCODE
	if (context->enc) {
		ope_encoder_drain(context->enc);
		ope_encoder_destroy(context->enc);
	}
	if (context->comments) {
		ope_comments_destroy(context->comments);
	}
#endif 
	if (context->audio_buffer) {
		switch_buffer_destroy(&context->audio_buffer);
	}
	switch_thread_rwlock_unlock(context->rwlock);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opusfile_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	int ret;
	opus_file_context *context = handle->private_info;

	if (handle->handler || switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		return SWITCH_STATUS_FALSE;
	} else {
		if (whence == SWITCH_SEEK_CUR) {
			samples -= switch_buffer_inuse(context->audio_buffer) / sizeof(int16_t);
		}
		switch_buffer_zero(context->audio_buffer);
		ret = op_pcm_seek(context->of, samples);
		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[OGG/OPUS File] seek samples: [%u]\n", (unsigned int)samples);
		}
		if (ret == 0) {
			handle->pos = *cur_sample = samples;
			return SWITCH_STATUS_SUCCESS;
		}
	}
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_opusfile_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	opus_file_context *context = handle->private_info;
	size_t bytes = *len * sizeof(int16_t) * handle->real_channels;
	size_t rb = 0, newbytes;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (!handle->handler) {
		if (switch_opusfile_decode(context, data, bytes, handle->real_channels) == SWITCH_STATUS_FALSE) {
			context->eof = SWITCH_TRUE;
		}
	}
	switch_mutex_lock(context->audio_mutex);
	rb = switch_buffer_read(context->audio_buffer, data, bytes);
	switch_mutex_unlock(context->audio_mutex);

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS File] rb: [%"SWITCH_SIZE_T_FMT"] bytes: [%"SWITCH_SIZE_T_FMT"]\n", rb, bytes);
	}

	if (!rb && (context->eof)) {
		if (globals.debug) {
			// should be same as returned by op_pcm_total()
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS File] EOF. sample count: [%"SWITCH_SIZE_T_FMT"]\n", handle->sample_count);
		}
		*len = 0;
		return SWITCH_STATUS_FALSE;
	}
	if (rb) {
		*len = rb / sizeof(int16_t) / handle->real_channels;
		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS File] rb: [%"SWITCH_SIZE_T_FMT"] *len: [%"SWITCH_SIZE_T_FMT"]\n", rb, *len);
		}
	} else {
		newbytes = (2 * handle->samplerate * handle->real_channels) * context->buffer_seconds;
		if (newbytes < bytes) {
			bytes = newbytes;
		}
		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					"[OGG/OPUS File] Padding with empty audio. seconds: [%d] bytes: [%d] newbytes: [%d] real_channels: [%d]\n", 
					(int)context->buffer_seconds, (int)bytes, (int)newbytes, (int)handle->real_channels);
		}
		memset(data, 255, bytes);
		*len = bytes / sizeof(int16_t) / handle->real_channels;
	}

	handle->pos += *len;
	handle->sample_count += *len;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opusfile_write(switch_file_handle_t *handle, void *data, size_t *len)
{
#ifdef HAVE_OPUSFILE_ENCODE
	size_t nsamples = *len;
	int err;
	int mapping_family = 0;

	opus_file_context *context;

	if (!handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error no handle\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!(context = handle->private_info)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error no context\n");
		return SWITCH_STATUS_FALSE;
	}
	if (!context->comments) {
		context->comments = ope_comments_create();
		ope_comments_add(context->comments, "METADATA", "Freeswitch/mod_opusfile");
	}
	if (context->channels > 2) {
			mapping_family = 1;
	}
	if (!context->enc) {
		context->enc = ope_encoder_create_file(handle->file_path, context->comments, handle->samplerate, handle->channels, mapping_family, &err);
		if (!context->enc) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open file for writing. err: [%d] [%s]\n", err, ope_strerror(err));
			return SWITCH_STATUS_FALSE;
		}
	}

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[OGG/OPUS File] write nsamples: [%d]\n", (int)nsamples);
	}

	err = ope_encoder_write(context->enc, (opus_int16 *)data, nsamples);

	if (err != OPE_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS File] Can't encode. err: [%d] [%s]\n", err, ope_strerror(err));
		return SWITCH_STATUS_FALSE;
	}

	handle->sample_count += *len;
#else
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[OGG/OPUS File] Encoding support not built-in, build the module with libopusenc!\n");
#endif 
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opusfile_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_opusfile_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	return SWITCH_STATUS_FALSE;
}


#define OPUSFILE_DEBUG_SYNTAX "<on|off>"
SWITCH_STANDARD_API(mod_opusfile_debug)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", OPUSFILE_DEBUG_SYNTAX);
	} else {
		if (!strcasecmp(cmd, "on")) {
			globals.debug = 1;
			stream->write_function(stream, "OPUSFILE Debug: on\n");
#ifdef HAVE_OPUSFILE_ENCODE
			stream->write_function(stream, "Library version (encoding): %s ABI: %s\n", ope_get_version_string(), ope_get_abi_version());
#endif 
		} else if (!strcasecmp(cmd, "off")) {
			globals.debug = 0;
			stream->write_function(stream, "OPUSFILE Debug: off\n");
		} else {
			stream->write_function(stream, "-USAGE: %s\n", OPUSFILE_DEBUG_SYNTAX);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opusstream_set_initial(opus_stream_context_t *context) 
{
	/* https://www.opus-codec.org/docs/opusfile_api-0.5/group__stream__info.html#ga9272a4a6ac9e01fbc549008f5ff58b4c */

	if (context->of) {
		int ret;
		/* docs: "Obtain the PCM offset of the next sample to be read. " */
		ret = op_pcm_tell(context->of);
		if (ret != OP_EINVAL) {
			context->pcm_offset = ret;
		}
		context->pcm_print_offset = context->pcm_offset - context->samplerate;

		/* docs: "Obtain the current value of the position indicator for _of." */
		ret = op_raw_tell(context->of);
		if (ret != OP_EINVAL) {
			context->raw_offset = ret;
		}

		/* docs: "Get the channel count of the given link in a (possibly-chained) Ogg Opus stream. " */
		context->dec_channels = op_channel_count(context->of, -1);
		if (context->dec_channels == 0) {
			context->dec_channels = 1;
		}

		context->samplerate = DEFAULT_RATE;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_opusstream_stream_info(opus_stream_context_t *context) 
{
	const OpusHead *head;
	const OpusTags *tags;
	opus_int32 bitrate;

	if (context->of) {

		/* docs: "Get the serial number of the given link in a (possibly-chained) Ogg Opus stream. "*/
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS Stream Decode] SerialNO: [%u]\n", op_serialno(context->of, -1));
		bitrate = op_bitrate_instant(context->of);
		if (bitrate > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS Stream Decode] Bitrate: [%d]\n", bitrate);
		}

		if(context->pcm_offset!=0){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] Non-zero starting PCM offset: [%li]\n", 
					(long)context->pcm_offset);
		}

		/* docs: "Retrieve the index of the current link." */
		context->li = op_current_link(context->of);

		/* docs: "Get the ID header information for the given link in a (possibly chained) Ogg Opus stream. " */
		head = op_head(context->of, context->li);
		if (head) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS Stream Decode] Channels: [%i]\n", head->channel_count);
			if (head->input_sample_rate) {
				context->samplerate = head->input_sample_rate;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS Stream Decode] Original sampling rate: [%lu] Hz\n", 
						(unsigned long)head->input_sample_rate);
			}
		}
		/*docs: "Returns whether or not the data source being read is seekable."*/
		if (op_seekable(context->of)) {
			opus_int64  size;
			context->duration = op_pcm_total(context->of, context->li); // page duration 
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO , "[OGG/OPUS Stream Decode] Duration (samples): [%u]\n", (unsigned int)context->duration);
			size = op_raw_total(context->of, context->li);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[OGG/OPUS Stream Decode] Size (bytes): [%u]\n", (unsigned int)size);
		}
		/* docs: "Get the comment header information for the given link in a (possibly chained) Ogg Opus stream." */
		tags = op_tags(context->of, context->li);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS Stream Decode] Encoded by: [%s]\n", tags->vendor);
		return SWITCH_STATUS_FALSE;
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opusstream_stream_decode(opus_stream_context_t *context, void *data, int channels)
{
	int ret;
	size_t buf_inuse;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!context->of) {
		return SWITCH_STATUS_FALSE;
	}
	memset(context->decode_buf, 0, sizeof(context->decode_buf));
	switch_mutex_lock(context->audio_mutex);
	while (!(context->eof)) {

		if (channels == 1) {
			ret = op_read(context->of, (opus_int16 *)context->decode_buf, OPUS_MAX_PCM, NULL);
		} else if (channels > 1) {
			ret = op_read_stereo(context->of, (opus_int16 *)context->decode_buf, OPUS_MAX_PCM);
		} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS Stream] Invalid number of channels!\n");
				switch_goto_status(SWITCH_STATUS_FALSE, end);
		}

		if (ret < 0) {
			switch(ret) {
			case OP_HOLE:	/* There was a hole in the data, and some samples may have been skipped. Call this function again to continue decoding past the hole.*/
				if (!context->dec_page_ready) {
					if (globals.debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Decoder]: incomplete ogg page, will retry\n");
					}
					switch_goto_status(SWITCH_STATUS_SUCCESS, end);
				}
			case OP_EREAD:	/*An underlying read operation failed. This may signal a truncation attack from an <https:> source.*/
			
			case OP_EFAULT: /*	An internal memory allocation failed. */

			case OP_EIMPL:	/*An unseekable stream encountered a new link that used a feature that is not implemented, such as an unsupported channel family.*/

			case OP_EINVAL:	/* The stream was only partially open. */

			case OP_ENOTFORMAT: /*	An unseekable stream encountered a new link that did not have any logical Opus streams in it. */

			case OP_EBADHEADER:	/*An unseekable stream encountered a new link with a required header packet that was not properly formatted, contained illegal values, or was missing altogether.*/

			case OP_EVERSION:	/*An unseekable stream encountered a new link with an ID header that contained an unrecognized version number.*/

			case OP_EBADPACKET: /*Failed to properly decode the next packet.*/

			case OP_EBADLINK:		/*We failed to find data we had seen before.*/

			case OP_EBADTIMESTAMP:		/*An unseekable stream encountered a new link with a starting timestamp that failed basic validity checks.*/

			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS Decoder]: error decoding stream: [%d]\n", ret);
				switch_goto_status(SWITCH_STATUS_FALSE, end);
			}
		} else if (ret == 0) {
			/*The number of samples returned may be 0 if the buffer was too small to store even a single sample for both channels, or if end-of-file was reached*/
			if (globals.debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Decoder]: EOF reached [%d]\n", ret);
			}

			context->eof = TRUE;
			break;
		} else /* (ret > 0)*/ {
			/*The number of samples read per channel on success*/
			switch_buffer_write(context->audio_buffer, (opus_int16 *)context->decode_buf, ret * sizeof(opus_int16) * channels);
			buf_inuse = switch_buffer_inuse(context->audio_buffer);

			if (globals.debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
						"[OGG/OPUS Decoder]: Read samples: %d. Wrote bytes to buffer: [%d] bytes in use: [%u] byte pos stream: [%lu]\n", 
						ret, (int)(ret * sizeof(int16_t) * channels), (unsigned int)buf_inuse, (long unsigned int)op_raw_tell(context->of));
			}
		}
	}

end:
	context->eof = FALSE; // for next page 

	switch_mutex_unlock(context->audio_mutex);

	return status;
}

static switch_status_t switch_opusstream_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct opus_stream_context *context = NULL;
	int encoding, decoding;
#ifdef HAVE_OPUSFILE_ENCODE
	int err;
#endif 

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct opus_stream_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {

		memset(context, 0, sizeof(struct opus_stream_context));
		codec->private_info = context;
		context->pool = codec->memory_pool;

		switch_thread_rwlock_create(&(context->rwlock), context->pool);

		switch_thread_rwlock_rdlock(context->rwlock);

		switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->pool);
		switch_mutex_init(&context->ogg_mutex, SWITCH_MUTEX_NESTED, context->pool);

		if (switch_buffer_create_dynamic(&context->audio_buffer, TC_BUFFER_SIZE, TC_BUFFER_SIZE * 2, 0) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			switch_thread_rwlock_unlock(context->rwlock);
			return SWITCH_STATUS_MEMERR;
		}

		if (switch_buffer_create_dynamic(&context->ogg_buffer, TC_BUFFER_SIZE, TC_BUFFER_SIZE * 2, 0) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			switch_thread_rwlock_unlock(context->rwlock);
			return SWITCH_STATUS_MEMERR;
		}

		context->samplerate = codec->implementation->actual_samples_per_second;
		context->frame_size = codec->implementation->actual_samples_per_second * (codec->implementation->microseconds_per_packet / 1000) / 1000;

		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream] frame_size: [%d]\n", (int)context->frame_size);
		}
#ifdef HAVE_OPUSFILE_ENCODE
		if (encoding) {
			if (!context->comments) {
				context->comments = ope_comments_create();
				ope_comments_add(context->comments, "METADATA", "Freeswitch/mod_opusfile");
			}
			if (!context->enc) {
				int mapping_family = 0;
				// opus_multistream_surround_encoder_get_size() in libopus will check these
				if ((context->enc_channels > 2) && (context->enc_channels <= 8)) {
					mapping_family = 1;
				} else if ((context->enc_channels > 8) && (context->enc_channels <= 255)) {
					// multichannel/multistream mapping family . https://people.xiph.org/~giles/2013/draft-ietf-codec-oggopus.html#rfc.section.5.1.1
					mapping_family = 255;
				}
				context->enc = ope_encoder_create_pull(context->comments, !context->samplerate?DEFAULT_RATE:context->samplerate, !context->enc_channels?1:context->enc_channels, mapping_family, &err);

				if (!context->enc) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS Stream Encode] Can't create stream. err: [%d] [%s]\n", err, ope_strerror(err));
					switch_thread_rwlock_unlock(context->rwlock);
					return SWITCH_STATUS_FALSE;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS Stream Encode] Stream opened for encoding\n"); 
				}
				ope_encoder_ctl(context->enc, OPUS_SET_COMPLEXITY_REQUEST, 5);
				ope_encoder_ctl(context->enc, OPUS_SET_APPLICATION_REQUEST, OPUS_APPLICATION_VOIP);
			}
		}
#endif 
		switch_thread_rwlock_unlock(context->rwlock);
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_opusstream_destroy(switch_codec_t *codec)
{
	struct opus_stream_context *context = codec->private_info;
	switch_status_t st;
	
	switch_thread_rwlock_rdlock(context->rwlock);
	
	if (context->read_stream_thread) {
		switch_thread_join(&st, context->read_stream_thread);
		if (st == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Encode/Decode] Joined decoding thread\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Encode/Decode] Can't join decoding thread\n");
		}
	}

	if (context->of) {
		op_free(context->of);
	}

#ifdef HAVE_OPUSFILE_ENCODE
	if (context->enc) {
		ope_encoder_destroy(context->enc);
	}
	if (context->comments) {
		ope_comments_destroy(context->comments);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Encode/Decode] Encoded pages: [%u]\n", context->enc_pagecount);
#endif 
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Encode/Decode] Decoded chunks: [%u]\n", context->dec_count);
	if (context->audio_buffer) {
		switch_buffer_destroy(&context->audio_buffer);
	}
	if (context->ogg_buffer) {
		switch_buffer_destroy(&context->ogg_buffer);
	}
	switch_thread_rwlock_unlock(context->rwlock);
	codec->private_info = NULL;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Encode/Decode] Stopped processing\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opusstream_encode(switch_codec_t *codec,
										switch_codec_t *other_codec,
										void *decoded_data,
										uint32_t decoded_data_len,
										uint32_t decoded_rate,
										void *encoded_data, 
										uint32_t *encoded_data_len,
										uint32_t *encoded_rate,
										unsigned int *flag)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
#ifdef HAVE_OPUSFILE_ENCODE
	struct opus_stream_context *context = codec->private_info;
	size_t nsamples = (int)decoded_data_len / sizeof(int16_t);
	int err, ret;
	int len = 0; int thres;
	unsigned char *decode_buf = decoded_data;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	globals.debug = 0;
	switch_thread_rwlock_rdlock(context->rwlock);

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
				"[OGG/OPUS Stream Encode] : switch_opusfile_stream_encode() decoded_data [%x][%x][%x][%x] nsamples: [%d]\n", 
				decode_buf[0], decode_buf[1], decode_buf[2], decode_buf[3], (int)nsamples);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Encode] stream write nsamples: [%d]\n", (int)nsamples);
	}
	if (context->enc_channels == 0) {
		context->enc_channels = 1;
	}
	if (!context->samplerate) {
		context->samplerate = DEFAULT_RATE;
	}

	if (context->enc) {
		// we reach here every 20 ms.
		// decoded_data - this can be an interleaved buffer, to do multistream. we’ll need the exact number of channels too.
		err = ope_encoder_write(context->enc, (opus_int16 *)decoded_data, nsamples / context->enc_channels);
		if (err != OPE_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS Stream Encode] can't encode, ret: [%d] [%s]\n", err, ope_strerror(err));
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
		context->samples_encode += nsamples;
	}

	thres = context->samplerate/PAGES_PER_SEC;

	if (!(context->samples_encode % thres) && context->samples_encode > context->samplerate) {
		if (context->enc) {
			unsigned char *vb = context->encode_buf;
			int req_flush = 1; 
			/* OPE_EXPORT int ope_encoder_get_page(OggOpusEnc *enc, unsigned char **page, opus_int32 *len, int flush); */
			ret = ope_encoder_get_page(context->enc, &vb, &len, req_flush);
			if (ret == 0) {
				/* ope_encoder_get_page(): ret is 1 if there is a page available, 0 if not. */
				if (globals.debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Encode] can't retrieve encoded page, page not ready. ret: [%d]\n", ret);
				}
				switch_goto_status(SWITCH_STATUS_SUCCESS, end);
			} else {
				if (globals.debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Encode] retrieved page from encoder. ret [%d] len: [%d] [%p]\n", 
							ret, len, context->encode_buf);
				}
				if (len > OGG_MAX_PAGE_SIZE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS Stream Encode] retrieved page bigger than ogg max size!\n");
					switch_goto_status(SWITCH_STATUS_FALSE, end);
				}
				memcpy(encoded_data, vb, len);
				*encoded_data_len = len;
				context->enc_pagecount++;
				switch_thread_rwlock_unlock(context->rwlock);
				return SWITCH_STATUS_SUCCESS;
			}
		} else {
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	}
end: 
	*encoded_data_len = 0;
	switch_thread_rwlock_unlock(context->rwlock);
#else
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[OGG/OPUS Stream Encode] Encoding support not built-in, build the module with libopusenc!\n");
#endif 
	return status;
}

// decode_stream_cb(): nbytes is OP_READ_SIZE (builtin limit - libopusfile).
// this is being called by op_read() or op_read_stereo() - we’re giving chunks of pages to be decoded. 
static int decode_stream_cb(void *dcontext, unsigned char *data, int nbytes) 
{
	opus_stream_context_t *context = (opus_stream_context_t *)dcontext;
	unsigned int ret = 0;
	
	if (!context) {
		return 0;
	}

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] decode CB called: context: %p data: %p packet_len: %d\n", 
				(void *)context, data, nbytes);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] decode_stream_cb(): switch_thread_self(): %lx\n", (unsigned long)(intptr_t)switch_thread_self());
	}

	switch_mutex_lock(context->ogg_mutex);
	ret = switch_buffer_read(context->ogg_buffer, context->ogg_data, nbytes);
	if (ret == 0) {
		data = NULL;
		switch_mutex_unlock(context->ogg_mutex);
		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] No data. Wanted: [%d] bytes\n", nbytes);
		}
		return ret;
	}
	context->dec_count++;
	memcpy(data, context->ogg_data, ret);

	if (switch_buffer_inuse(context->ogg_buffer)) {
		context->dec_page_ready = 0;
	} else {
		context->dec_page_ready = 1;
		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] buffer is empty, all pages passed to the decoder\n");
		}

	}
	switch_mutex_unlock(context->ogg_mutex);

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] decode_stream_cb(): ret: %u\n",  ret);
	}
	return ret;
}

const OpusFileCallbacks cb={decode_stream_cb, NULL, NULL, NULL};

static void *SWITCH_THREAD_FUNC read_stream_thread(switch_thread_t *thread, void *obj)
{
	opus_stream_context_t *context = (opus_stream_context_t *) obj;
	int err = 0;
	OggOpusFile *temp_of = NULL;
	int buffered_ogg_bytes;

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] read_stream_thread(): switch_thread_self(): 0x%lx\n", (unsigned long)(intptr_t)switch_thread_self());
	}
	switch_thread_rwlock_rdlock(context->rwlock);
	switch_mutex_lock(context->ogg_mutex);

	if ((buffered_ogg_bytes = switch_buffer_inuse(context->ogg_buffer))) {
		if (buffered_ogg_bytes <= OGG_MAX_PAGE_SIZE) {
			switch_buffer_peek(context->ogg_buffer, context->ogg_data, buffered_ogg_bytes);
			context->ogg_data_len = buffered_ogg_bytes;
		}
	} 

	/* https://mf4.xiph.org/jenkins/view/opus/job/opusfile-unix/ws/doc/html/group__stream__open__close.html#gad183ecf5fbec5add3a5ccf1e3b1d2593  */
	/* docs: "Open a stream using the given set of callbacks to access it." */
	temp_of = op_open_callbacks(context, &cb, (const unsigned char *)context->ogg_data, context->ogg_data_len, &err);
	if (temp_of && (err == 0)) {
		context->dec_page_ready = 1; 
		context->of = temp_of;
		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[OGG/OPUS Stream Decode] Opened stream, installed decoding callback!\n");
		}
		switch_opusstream_set_initial(context);
		switch_opusstream_stream_info(context);
	} else if (err != 0) {
		switch (err) {
			case OP_EREAD:
			   //  An underlying read, seek, or tell operation failed when it should have succeeded, or we failed to find data in the stream we had seen before. 
			case OP_EFAULT:
				//    There was a memory allocation failure, or an internal library error. 
			case OP_EIMPL:
				// The stream used a feature that is not implemented, such as an unsupported channel family. 
			case OP_EINVAL:
				// seek() was implemented and succeeded on this source, but tell() did not, or the starting position indicator was not equal to _initial_bytes. 
			case OP_ENOTFORMAT:
				// The stream contained a link that did not have any logical Opus streams in it. 
			case OP_EBADHEADER:
				// A required header packet was not properly formatted, contained illegal values, or was missing altogether. 
			case OP_EVERSION:
				// An ID header contained an unrecognized version number. 
			case OP_EBADLINK:
				// We failed to find data we had seen before after seeking. 
			case OP_EBADTIMESTAMP:
				// The first or last timestamp in a link failed basic validity checks
			default:
				context->dec_page_ready = 0;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS Stream Decode] error opening stream: [%d]\n", err);
		}
	}

	switch_mutex_unlock(context->ogg_mutex);
	switch_thread_rwlock_unlock(context->rwlock);
	return NULL;
}

static void launch_read_stream_thread(opus_stream_context_t *context)
{
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, context->pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&context->read_stream_thread, thd_attr, read_stream_thread, context, context->pool);
}

static switch_status_t switch_opusstream_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, 
										  void *decoded_data, 
										  uint32_t *decoded_data_len, 
										  uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct opus_stream_context *context = codec->private_info;
	size_t bytes = 0; 
	int ogg_bytes = OGG_MIN_PAGE_SIZE; // min page size before trying to open the incoming stream 
	size_t rb = 0;
	unsigned char *encode_buf = encoded_data;
	size_t buffered_ogg_bytes = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}
	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
				"[OGG/OPUS Stream Decode] : switch_opusstream_decode() encoded_data [%x][%x][%x][%x] encoded_data_len: [%u]\n", 
				encode_buf[0], encode_buf[1], encode_buf[2], encode_buf[3], encoded_data_len);
	}
#ifdef LIMIT_DROP
	if ((encoded_data_len <=  MIN_OGG_PAYLOAD) && (encoded_data_len > 0)) {
		*decoded_data_len = 0;
		if (globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] switch_opusstream_decode(): drop [%u]", (unsigned int)encoded_data_len);
		}
		return SWITCH_STATUS_SUCCESS;
	}
#endif

	switch_thread_rwlock_rdlock(context->rwlock);
	switch_mutex_lock(context->ogg_mutex);
	memset(context->ogg_data, 0, sizeof(context->ogg_data)); 
	if (encoded_data_len <= SWITCH_RECOMMENDED_BUFFER_SIZE) {
		switch_buffer_write(context->ogg_buffer, encode_buf, encoded_data_len);
 
		if ((buffered_ogg_bytes = switch_buffer_inuse(context->ogg_buffer)) >= ogg_bytes) {
			if (globals.debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						"[OGG/OPUS Stream Decode] switch_opusstream_decode() encoded_data [%x][%x][%x][%x] encoded_data_len: %u buffered_ogg_bytes: [%u]\n", 
						encode_buf[0], encode_buf[1], encode_buf[2], encode_buf[3], encoded_data_len, (unsigned int)buffered_ogg_bytes);
			}
			if (buffered_ogg_bytes <= OGG_MAX_PAGE_SIZE) {
				switch_buffer_peek(context->ogg_buffer, context->ogg_data, buffered_ogg_bytes);
				context->ogg_data_len = buffered_ogg_bytes;
			}	else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS Stream Decode] buffered ogg data bigger than max OGG page size, will flush\n");
				*decoded_data_len = 0;
				switch_buffer_zero(context->ogg_buffer);
				switch_goto_status(SWITCH_STATUS_SUCCESS, end);
			}
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[OGG/OPUS Stream Decode] too much data to buffer, flushing buffer!\n");
		*decoded_data_len = 0;
		switch_buffer_zero(context->ogg_buffer);
		switch_goto_status(SWITCH_STATUS_SUCCESS, end);
	}

	if ((buffered_ogg_bytes >= ogg_bytes) && encoded_data_len) {

		if (!(op_test(NULL, context->ogg_data, buffered_ogg_bytes))) {
			if (!context->read_stream && buffered_ogg_bytes > OGG_MIN_PAGE_SIZE) {
				if (globals.debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] launching decoding thread\n");
				}
				launch_read_stream_thread(context);
				context->read_stream = 1; // mark thread started
			}
		} 
	}
	if (context->of) {
		if (switch_opusstream_stream_decode(context, context->ogg_data, context->dec_channels) == SWITCH_STATUS_FALSE) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[OGG/OPUS Stream Decode] Cannot decode stream\n");
			*decoded_data_len = 0;
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	}
	switch_mutex_lock(context->audio_mutex);
	bytes = switch_buffer_inuse(context->audio_buffer);
	rb = switch_buffer_read(context->audio_buffer, decoded_data, context->frame_size * sizeof(int16_t));
	switch_mutex_unlock(context->audio_mutex);

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[OGG/OPUS Stream Decode] rb (read from audio_buffer): [%d] bytes in audio buffer: [%d]\n", (int)rb, (int)bytes);
	}

	*decoded_data_len = rb ; // bytes
end:

	switch_thread_rwlock_unlock(context->rwlock);
	switch_mutex_unlock(context->ogg_mutex);
	return status;
}

/* Registration */

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_opusfile_load)
{
	switch_file_interface_t *file_interface;
	switch_api_interface_t *commands_api_interface;
	switch_codec_interface_t *codec_interface;
	int mpf = 10000, spf = 80, bpf = 160, count = 2;
	int RATES[] = {8000, 16000, 24000, 48000};
	int i;

	supported_formats[0] = "opus";

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(commands_api_interface, "opusfile_debug", "Set OPUSFILE Debug", mod_opusfile_debug, OPUSFILE_DEBUG_SYNTAX);

	switch_console_set_complete("add opusfile_debug on");
	switch_console_set_complete("add opusfile_debug off");

	globals.debug = 0;

	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = switch_opusfile_open;
	file_interface->file_close = switch_opusfile_close;
	file_interface->file_read = switch_opusfile_read;
	file_interface->file_write = switch_opusfile_write;
	file_interface->file_seek = switch_opusfile_seek;
	file_interface->file_set_string = switch_opusfile_set_string;
	file_interface->file_get_string = switch_opusfile_get_string;

	SWITCH_ADD_CODEC(codec_interface, "OPUSSTREAM");

	for (i = 0; i < sizeof(RATES) / sizeof(RATES[0]); i++) {
// mono
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,
											 98, 	 /* the IANA code number */ // does not matter
											 "OPUSSTREAM",  /* the IANA code name */ // we just say OPUSSTREAM is an ogg/opus stream
											 NULL,   /* default fmtp to send (can be overridden by the init function) */
											 RATES[i], /* samples transferred per second */ // 48000 !
											 RATES[i], /* actual samples transferred per second */
											 16 * RATES[i] / 8000, /* bits transferred per second */
											 mpf * count,  /* number of microseconds per frame */
											 spf * RATES[i] / 8000, /* number of samples per frame */
											 bpf * RATES[i] / 8000, /* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1, /* number of channels represented */
											 1,	/* number of frames per network packet */
											switch_opusstream_init, switch_opusstream_encode, switch_opusstream_decode, switch_opusstream_destroy);
// stereo
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,
											 98, 	 /* the IANA code number */ // does not matter
											 "OPUSSTREAM",  /* the IANA code name */ // we just say OPUSSTREAM is an ogg/opus stream
											 NULL,   /* default fmtp to send (can be overridden by the init function) */
											 RATES[i], /* samples transferred per second */
											 RATES[i], /* actual samples transferred per second */
											 16 * RATES[i] / 8000 * 2, /* bits transferred per second */
											 mpf * count,  /* number of microseconds per frame */
											 spf * RATES[i] / 8000 * 2, /* number of samples per frame */
											 bpf * RATES[i] / 8000 * 2, /* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 2, /* number of channels represented */
											 1,	/* number of frames per network packet */
											switch_opusstream_init, switch_opusstream_encode, switch_opusstream_decode, switch_opusstream_destroy);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "mod_opusfile loaded\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

