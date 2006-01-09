/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_core.c -- Main Core Library
 *
 */
#include <switch.h>

#ifdef EMBED_PERL
#include <EXTERN.h>
#include <perl.h>

static char *embedding[] = { "", "-e", ""};
EXTERN_C void xs_init (pTHX);
#endif


#ifndef SWITCH_DB_DIR
#ifdef WIN32
#define SWITCH_DB_DIR ".\\db"
#else
#define SWITCH_DB_DIR "/usr/local/freeswitch/db"
#endif
#endif

struct switch_core_session {
	unsigned long id;
	char name[80];
	switch_memory_pool *pool;
	switch_channel *channel;
	switch_thread *thread;
	const switch_endpoint_interface *endpoint_interface;
	struct switch_io_event_hooks event_hooks;
	switch_codec *read_codec;
	switch_codec *write_codec;

	switch_buffer *raw_write_buffer;
	switch_frame raw_write_frame;
	switch_frame enc_write_frame;
	unsigned char *raw_write_buf[3200];
	unsigned char *enc_write_buf[3200];

	switch_buffer *raw_read_buffer;
	switch_frame raw_read_frame;
	switch_frame enc_read_frame;
	unsigned char *raw_read_buf[3200];
	unsigned char *enc_read_buf[3200];


	switch_audio_resampler *read_resampler;
	switch_audio_resampler *write_resampler;

	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;

	void *streams[SWITCH_MAX_STREAMS];
	int stream_count;

	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH+1];
	void *private;
};

struct switch_core_runtime {
	time_t initiated;
	unsigned long session_id;
	apr_pool_t *memory_pool;
	switch_hash *session_table;
	switch_core_db *db;
#ifdef EMBED_PERL
	PerlInterpreter *my_perl;
#endif
	FILE *console;
};

/* Prototypes */
static int handle_SIGINT(int sig);
static int handle_SIGPIPE(int sig);
static void * SWITCH_THREAD_FUNC switch_core_session_thread(switch_thread *thread, void *obj);
static void switch_core_standard_on_init(switch_core_session *session);
static void switch_core_standard_on_hangup(switch_core_session *session);
static void switch_core_standard_on_ring(switch_core_session *session);
static void switch_core_standard_on_execute(switch_core_session *session);
static void switch_core_standard_on_loopback(switch_core_session *session);
static void switch_core_standard_on_transmit(switch_core_session *session);


/* The main runtime obj we keep this hidden for ourselves */
static struct switch_core_runtime runtime;


static int handle_SIGPIPE(int sig)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Sig Pipe!\n");
	return 0;
}

#ifdef TRAP_BUS
static int handle_SIGBUS(int sig)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Sig BUS!\n");
	return 0;
}
#endif

/* no ctl-c mofo */
static int handle_SIGINT(int sig)
{
	return 0;
}


static void db_pick_path(char *dbname, char *buf, size_t size) 
{

	memset(buf, 0, size);
	if (strchr(dbname, '/')) {
		strncpy(buf, dbname, size);
	} else {
		snprintf(buf, size, "%s/%s.db", SWITCH_DB_DIR, dbname);
	}
}

SWITCH_DECLARE(switch_core_db *) switch_core_db_open_file(char *filename) 
{
	switch_core_db *db;
	char path[1024];

	db_pick_path(filename, path, sizeof(path));
	if (switch_core_db_open(path, &db)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "SQL ERR [%s]\n", switch_core_db_errmsg(db));
		switch_core_db_close(db);
		db=NULL;
	}
	return db;
}

SWITCH_DECLARE(FILE *) switch_core_data_channel(switch_text_channel channel)
{
	FILE *handle = stdout;

	switch (channel) {
	case SWITCH_CHANNEL_ID_CONSOLE:
	case SWITCH_CHANNEL_ID_CONSOLE_CLEAN:
		handle = runtime.console;
		break;
	default:
		handle = stdout;
		break;
	}

	return handle;
}

#ifdef EMBED_PERL
/* test frontend to the perl interpreter */
SWITCH_DECLARE(switch_status) switch_core_do_perl(char *txt)
{
	PerlInterpreter *my_perl = runtime.my_perl;
	eval_pv(txt, TRUE);
	return SWITCH_STATUS_SUCCESS;
}
#endif


SWITCH_DECLARE(char *) switch_core_session_get_uuid(switch_core_session *session)
{
	return session->uuid_str;
}

SWITCH_DECLARE(switch_status) switch_core_session_set_read_codec(switch_core_session *session, switch_codec *codec)
{
	assert(session != NULL);

	session->read_codec = codec;
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status) switch_core_session_set_write_codec(switch_core_session *session, switch_codec *codec)
{
	assert(session != NULL);

	session->write_codec = codec;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_core_codec_init(switch_codec *codec, char *codec_name, int rate, int ms, int channels, switch_codec_flag flags, const switch_codec_settings *codec_settings, switch_memory_pool *pool)
{
	const switch_codec_interface *codec_interface;
	const switch_codec_implementation *iptr, *implementation = NULL;

	assert(codec != NULL);
	assert(codec_name != NULL);

	memset(codec, 0, sizeof(*codec));

	if (!(codec_interface = switch_loadable_module_get_codec_interface(codec_name))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "invalid codec %s!\n", codec_name);
		return SWITCH_STATUS_GENERR;
	}

	for(iptr = codec_interface->implementations; iptr; iptr = iptr->next) {
		if ((!rate || rate == iptr->samples_per_second) && 
			(!ms || ms == (iptr->microseconds_per_frame / 1000)) && 
			(!channels || channels == iptr->number_of_channels)) {
			implementation = iptr;
			break;
		}
	}

	if (implementation) {
		switch_status status;
		codec->codec_interface = codec_interface;
		codec->implementation = implementation;
		codec->flags = flags;

		if (pool) {
			codec->memory_pool = pool;
		} else {
			if ((status = switch_core_new_memory_pool(&codec->memory_pool)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			switch_set_flag(codec, SWITCH_CODEC_FLAG_FREE_POOL);
		}
		implementation->init(codec, flags, codec_settings);

		return SWITCH_STATUS_SUCCESS;
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec %s Exists but not then desired implementation.\n", codec_name);	
	}

	return SWITCH_STATUS_NOTIMPL;

}

SWITCH_DECLARE(switch_status) switch_core_codec_encode(switch_codec *codec,
													   switch_codec *other_codec,
													   void *decoded_data,
													   size_t decoded_data_len,
													   int decoded_rate,
													   void *encoded_data,
													   size_t *encoded_data_len,
													   int *encoded_rate,
													   unsigned int *flag)
{
	assert(codec != NULL);
	assert(encoded_data != NULL);
	assert(decoded_data != NULL);

	if (!codec->implementation) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!switch_test_flag(codec, SWITCH_CODEC_FLAG_ENCODE)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec's encoder is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	*encoded_data_len = decoded_data_len;
	return codec->implementation->encode(codec,
										 other_codec,
										 decoded_data,
										 decoded_data_len,
										 decoded_rate,
										 encoded_data,
										 encoded_data_len,
										 encoded_rate, 
										 flag);

}

SWITCH_DECLARE(switch_status) switch_core_codec_decode(switch_codec *codec,
													   switch_codec *other_codec,
													   void *encoded_data,
													   size_t encoded_data_len,
													   int encoded_rate,
													   void *decoded_data,
													   size_t *decoded_data_len,
													   int *decoded_rate,
													   unsigned int *flag)
{
	assert(codec != NULL);
	assert(encoded_data != NULL);
	assert(decoded_data != NULL);

	if (!codec->implementation) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!switch_test_flag(codec, SWITCH_CODEC_FLAG_DECODE)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec's decoder is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	*decoded_data_len = encoded_data_len;

	return codec->implementation->decode(codec,
										 other_codec,
										 encoded_data,
										 encoded_data_len,
										 encoded_rate,
										 decoded_data,
										 decoded_data_len,
										 decoded_rate,
										 flag);

}

SWITCH_DECLARE(switch_status) switch_core_codec_destroy(switch_codec *codec)
{
	assert(codec != NULL);

	if (!codec->implementation) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	codec->implementation->destroy(codec);

	if (switch_test_flag(codec, SWITCH_CODEC_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&codec->memory_pool);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_core_file_open(switch_file_handle *fh, char *file_path, unsigned int flags, switch_memory_pool *pool)
{
	char *ext;
	switch_status status;

	memset(fh, 0, sizeof(*fh));

	if (!(ext = strrchr(file_path, '.'))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid Format\n");
		return SWITCH_STATUS_FALSE;
	}
	ext++;

	if (!(fh->file_interface = switch_loadable_module_get_file_interface(ext))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "invalid file format [%s]!\n", ext);
		return SWITCH_STATUS_GENERR;
	}

	fh->flags = flags;
	if (pool) {
		fh->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&fh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		switch_set_flag(fh, SWITCH_TIMER_FLAG_FREE_POOL);
	}

	return fh->file_interface->file_open(fh, file_path);
}

SWITCH_DECLARE(switch_status) switch_core_file_read(switch_file_handle *fh, void *data, size_t *len)
{
	assert(fh != NULL);

	return fh->file_interface->file_read(fh, data, (unsigned int *)len);
}

SWITCH_DECLARE(switch_status) switch_core_file_write(switch_file_handle *fh, void *data, size_t *len)
{
	assert(fh != NULL);

	return fh->file_interface->file_write(fh, data, (unsigned int *)len);
}

SWITCH_DECLARE(switch_status) switch_core_file_seek(switch_file_handle *fh, unsigned int *cur_pos, unsigned int samples, int whence)
{
	return fh->file_interface->file_seek(fh, cur_pos, samples, whence);
}

SWITCH_DECLARE(switch_status) switch_core_file_close(switch_file_handle *fh)
{
	return fh->file_interface->file_close(fh);
}


SWITCH_DECLARE(switch_status) switch_core_timer_init(switch_timer *timer, char *timer_name, int interval, int samples, switch_memory_pool *pool)
{
	switch_timer_interface *timer_interface;
	switch_status status;
	memset(timer, 0, sizeof(*timer));
	if (!(timer_interface = switch_loadable_module_get_timer_interface(timer_name))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "invalid timer %s!\n", timer_name);
		return SWITCH_STATUS_GENERR;
	}

	timer->interval = interval;
	timer->samples = samples;
	timer->samplecount = 0;
	timer->timer_interface = timer_interface;

	if (pool) {
		timer->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&timer->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		switch_set_flag(timer, SWITCH_TIMER_FLAG_FREE_POOL);
	}

	timer->timer_interface->timer_init(timer);
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(int) switch_core_timer_next(switch_timer *timer)
{
	if (!timer->timer_interface) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Timer is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (timer->timer_interface->timer_next(timer) == SWITCH_STATUS_SUCCESS) {
		return timer->samplecount;
	} else {
		return -1;
	}

}


SWITCH_DECLARE(switch_status) switch_core_timer_destroy(switch_timer *timer)
{
	if (!timer->timer_interface) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Timer is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	timer->timer_interface->timer_destroy(timer);

	if (switch_test_flag(timer, SWITCH_TIMER_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&timer->memory_pool);
	}

	return SWITCH_STATUS_SUCCESS;
}

static void *switch_core_service_thread(switch_thread *thread, void *obj)
{
	switch_core_thread_session *data = obj;
	switch_core_session *session = data->objs[0];
	int *stream_id = data->objs[1];
	switch_channel *channel;
	switch_frame *read_frame;

	assert(session != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);


	while(data->running > 0) {
		switch(switch_core_session_read_frame(session, &read_frame, -1, *stream_id)) {
		case SWITCH_STATUS_SUCCESS:
			break;
		case SWITCH_STATUS_TIMEOUT:
			break;
		default:
			data->running = -1;
			continue;
			break;
		}

		switch_yield(100);
	}

	data->running = 0;
	return NULL;
}

/* Either add a timeout here or make damn sure the thread cannot get hung somehow (my preference) */
SWITCH_DECLARE(void) switch_core_thread_session_end(switch_core_thread_session *thread_session)
{
	switch_core_session *session = thread_session->objs[0];

	switch_core_session_kill_channel(session, SWITCH_SIG_KILL);

	if (thread_session->running > 0) {
		thread_session->running = -1;

		while(thread_session->running) {
			switch_yield(1000);
		}
	}
}

SWITCH_DECLARE(void) switch_core_service_session(switch_core_session *session, switch_core_thread_session *thread_session, int stream_id)
{
	thread_session->running = 1;
	thread_session->objs[0] = session;
	thread_session->objs[1] = &stream_id;
	switch_core_session_launch_thread(session, switch_core_service_thread, thread_session);
}

SWITCH_DECLARE(switch_memory_pool *) switch_core_session_get_pool(switch_core_session *session)
{
	return session->pool;
}

/* **ONLY** alloc things with this function that **WILL NOT** outlive
   the session itself or expect an earth shattering KABOOM!*/
SWITCH_DECLARE(void *)switch_core_session_alloc(switch_core_session *session, size_t memory)
{
	void *ptr = NULL;
	assert(session != NULL);
	assert(session->pool != NULL);

	if ((ptr = apr_palloc(session->pool, memory))) {
		memset(ptr, 0, memory);
	}
	return ptr;
}

/* **ONLY** alloc things with these functions that **WILL NOT** need
   to be freed *EVER* ie this is for *PERMENANT* memory allocation */

SWITCH_DECLARE(void *) switch_core_permenant_alloc(size_t memory)
{
	void *ptr = NULL;
	assert(runtime.memory_pool != NULL);

	if ((ptr = apr_palloc(runtime.memory_pool, memory))) {
		memset(ptr, 0, memory);
	}
	return ptr;
}

SWITCH_DECLARE(char *) switch_core_permenant_strdup(char *todup)
{
	char *duped = NULL;
	size_t len;

	assert(runtime.memory_pool != NULL);

	if (!todup) return NULL;

	len = strlen(todup) + 1;
	if (todup && (duped = apr_palloc(runtime.memory_pool, len))) {
		strncpy(duped, todup, len);
	}
	return duped;
}


SWITCH_DECLARE(char *) switch_core_session_strdup(switch_core_session *session, char *todup)
{
	char *duped = NULL;
	size_t len;
	assert(session != NULL);
	assert(session->pool != NULL);

	if (!todup) return NULL;

	len = strlen(todup) + 1;

	if (todup && (duped = apr_palloc(session->pool, len))) {
		strncpy(duped, todup, len);
	}
	return duped;
}


SWITCH_DECLARE(char *) switch_core_strdup(switch_memory_pool *pool, char *todup)
{
	char *duped = NULL;
	size_t len;
	assert(pool != NULL);
	assert(todup != NULL);

	if (!todup) return NULL;
	len = strlen(todup) + 1;

	if (todup && (duped = apr_palloc(pool, len))) {
		strncpy(duped, todup, len);
	}
	return duped;
}

SWITCH_DECLARE(void *) switch_core_session_get_private(switch_core_session *session)
{
	assert(session != NULL);
	return session->private;
}


SWITCH_DECLARE(switch_status) switch_core_session_set_private(switch_core_session *session, void *private)
{
	assert(session != NULL);
	session->private = private;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) switch_core_session_add_stream(switch_core_session *session, void *private)
{
	session->streams[session->stream_count++] = private;
	return session->stream_count - 1;
}

SWITCH_DECLARE(void *) switch_core_session_get_stream(switch_core_session *session, int index)
{
	return session->streams[index];
}


SWITCH_DECLARE(int) switch_core_session_get_stream_count(switch_core_session *session)
{
	return session->stream_count;
}

SWITCH_DECLARE(switch_status) switch_core_session_outgoing_channel(switch_core_session *session,
																   char *endpoint_name,
																   switch_caller_profile *caller_profile,
																   switch_core_session **new_session)
{
	struct switch_io_event_hook_outgoing_channel *ptr;
	switch_status status = SWITCH_STATUS_FALSE;
	const switch_endpoint_interface *endpoint_interface;

	if (!(endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not locate channel type %s\n", endpoint_name);
		return SWITCH_STATUS_FALSE;
	}

	if (endpoint_interface->io_routines->outgoing_channel) {
		if ((status = endpoint_interface->io_routines->outgoing_channel(session, caller_profile, new_session)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.outgoing_channel; ptr ; ptr = ptr->next) {
				if ((status = ptr->outgoing_channel(session, caller_profile, *new_session)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	if (*new_session) {
		switch_caller_profile *profile = NULL, *peer_profile = NULL, *cloned_profile = NULL;
		switch_channel *channel = NULL, *peer_channel = NULL;

		if ((channel = switch_core_session_get_channel(session))) {
			profile = switch_channel_get_caller_profile(channel);
		}
		if ((peer_channel = switch_core_session_get_channel(*new_session))) {
			peer_profile = switch_channel_get_caller_profile(peer_channel);
		}

		if (channel && peer_channel) {
			if (profile) {
				if ((cloned_profile = switch_caller_profile_clone(*new_session, profile))) {
					switch_channel_set_originator_caller_profile(peer_channel, cloned_profile);
				}
			}
			if (peer_profile) {
				if ((cloned_profile = switch_caller_profile_clone(session, peer_profile))) {
					switch_channel_set_originatee_caller_profile(channel, cloned_profile);
				}
			}
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status) switch_core_session_answer_channel(switch_core_session *session)
{
	struct switch_io_event_hook_answer_channel *ptr;
	switch_status status = SWITCH_STATUS_FALSE;

	assert(session != NULL);
	if (session->endpoint_interface->io_routines->answer_channel) {
		if ((status = session->endpoint_interface->io_routines->answer_channel(session)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.answer_channel; ptr ; ptr = ptr->next) {
				if ((status = ptr->answer_channel(session)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	} else {
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

SWITCH_DECLARE(switch_status) switch_core_session_read_frame(switch_core_session *session, switch_frame **frame, int timeout, int stream_id)
{
	struct switch_io_event_hook_read_frame *ptr;
	switch_status status = SWITCH_STATUS_FALSE;
	int need_codec = 0, perfect = 0;


	if (session->endpoint_interface->io_routines->read_frame) {
		if ((status = session->endpoint_interface->io_routines->read_frame(session,
																		   frame,
																		   timeout,
																		   SWITCH_IO_FLAG_NOOP,
																		   stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.read_frame; ptr ; ptr = ptr->next) {
				if ((status = ptr->read_frame(session, frame, timeout, SWITCH_IO_FLAG_NOOP, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	if (status != SWITCH_STATUS_SUCCESS || !(*frame)) {
		return status;
	}

	/* if you think this code is redundant.... too bad! I like to understand what I'm doing */
	if ((session->read_codec && (*frame)->codec && session->read_codec->implementation != (*frame)->codec->implementation)) {
		need_codec = TRUE;
	}

	if (session->read_codec && !(*frame)->codec) {
		need_codec = TRUE;
	}

	if (!session->read_codec && (*frame)->codec) {
		need_codec = TRUE;
	}

	if (status == SWITCH_STATUS_SUCCESS && need_codec) {
		switch_frame *enc_frame, *read_frame = *frame;

		if (read_frame->codec) {
			unsigned int flag = 0;
			session->raw_read_frame.datalen = session->raw_read_frame.buflen;
			status = switch_core_codec_decode(read_frame->codec,
											  session->read_codec,
											  read_frame->data,
											  read_frame->datalen,
											  session->read_codec->implementation->samples_per_second,
											  session->raw_read_frame.data,
											  &session->raw_read_frame.datalen,
											  &session->raw_read_frame.rate,
											  &flag);

			switch (status) {
			case SWITCH_STATUS_RESAMPLE:
				if (!session->read_resampler) {
					switch_resample_create(&session->read_resampler,
										   read_frame->codec->implementation->samples_per_second,
										   read_frame->codec->implementation->bytes_per_frame * 10,
										   session->read_codec->implementation->samples_per_second,
										   session->read_codec->implementation->bytes_per_frame * 10,
										   session->pool);
				}
			case SWITCH_STATUS_SUCCESS:
				read_frame = &session->raw_read_frame;
				break;
			case SWITCH_STATUS_NOOP:
				status = SWITCH_STATUS_SUCCESS;
				break;
			default:
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec %s decoder error!\n", session->read_codec->codec_interface->interface_name);
				return status;
				break;
			}
		}
		if (session->read_resampler) {
			short *data = read_frame->data;

			session->read_resampler->from_len = switch_short_to_float(data, session->read_resampler->from, (int)read_frame->datalen / 2 );
			session->read_resampler->to_len = switch_resample_process(session->read_resampler,
																	  session->read_resampler->from,
																	  session->read_resampler->from_len, 
																	  session->read_resampler->to, 
																	  (int)session->read_resampler->to_size, 
																	  0);
			switch_float_to_short(session->read_resampler->to, data, read_frame->datalen);
			read_frame->samples = session->read_resampler->to_len;
			read_frame->datalen = session->read_resampler->to_len * 2;
			read_frame->rate = session->read_resampler->to_rate;
		}

		if (session->read_codec) {
			if ((*frame)->datalen == session->read_codec->implementation->bytes_per_frame) {
				perfect = TRUE;
			} else {
				if (! session->raw_read_buffer) {
					int bytes = session->read_codec->implementation->bytes_per_frame * 10;
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Engaging Read Buffer at %d bytes\n", bytes);
					switch_buffer_create(session->pool, &session->raw_read_buffer, bytes);
				}
				if (!switch_buffer_write(session->raw_read_buffer, read_frame->data, read_frame->datalen)) {
					return SWITCH_STATUS_MEMERR;
				}
			}

			if (switch_buffer_inuse(session->raw_read_buffer) >= session->read_codec->implementation->bytes_per_frame) {
				unsigned int flag = 0;

				if (perfect) {
					enc_frame = *frame;
					session->raw_read_frame.rate = (*frame)->rate;
				} else {
					session->raw_read_frame.datalen = switch_buffer_read(session->raw_read_buffer,
																		 session->raw_read_frame.data,
																		 session->read_codec->implementation->bytes_per_frame);
					session->raw_read_frame.rate = session->read_codec->implementation->samples_per_second;
					enc_frame = &session->raw_read_frame;
				}
				session->enc_read_frame.datalen = session->enc_read_frame.buflen;
				status = switch_core_codec_encode(session->read_codec,
												  (*frame)->codec,
												  session->raw_read_frame.data,
												  session->raw_read_frame.datalen,
												  (*frame)->codec->implementation->samples_per_second,
												  session->enc_read_frame.data,
												  &session->enc_read_frame.datalen,
												  &session->enc_read_frame.rate,
												  &flag);


				switch (status) {
				case SWITCH_STATUS_RESAMPLE:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "fixme 1\n");
				case SWITCH_STATUS_SUCCESS:
					*frame = &session->enc_read_frame;
					break;
				case SWITCH_STATUS_NOOP:
					*frame = &session->raw_read_frame;
					status = SWITCH_STATUS_SUCCESS;
					break;
				default:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec %s encoder error!\n", session->read_codec->codec_interface->interface_name);
					*frame = NULL;
					status = SWITCH_STATUS_GENERR;
					break;
				}
			}
		}
	}

	return status;
}

static switch_status perform_write(switch_core_session *session, switch_frame *frame, int timeout, switch_io_flag flags, int stream_id) {
	struct switch_io_event_hook_write_frame *ptr;
	switch_status status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->write_frame) {
		if ((status = session->endpoint_interface->io_routines->write_frame(session, frame, timeout, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.write_frame; ptr ; ptr = ptr->next) {
				if ((status = ptr->write_frame(session, frame, timeout, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}
	return status;
}

SWITCH_DECLARE(switch_status) switch_core_session_write_frame(switch_core_session *session, switch_frame *frame, int timeout, int stream_id)
{

	switch_status status = SWITCH_STATUS_FALSE;
	switch_frame *enc_frame, *write_frame = frame;
	unsigned int flag = 0, need_codec = 0, perfect = 0;
	switch_io_flag io_flag = SWITCH_IO_FLAG_NOOP;

	/* if you think this code is redundant.... too bad! I like to understand what I'm doing */
	if ((session->write_codec && frame->codec && session->write_codec->implementation != frame->codec->implementation)) {
		need_codec = TRUE;
	}

	if (session->write_codec && !frame->codec) {
		need_codec = TRUE;
	}

	if (!session->write_codec && frame->codec) {
		need_codec = TRUE;
	}

	if (need_codec) {
		if (frame->codec) {
			session->raw_write_frame.datalen = session->raw_write_frame.buflen;
			status = switch_core_codec_decode(frame->codec,
											  session->write_codec,
											  frame->data,
											  frame->datalen,
											  session->write_codec->implementation->samples_per_second,
											  session->raw_write_frame.data,
											  &session->raw_write_frame.datalen,
											  &session->raw_write_frame.rate,
											  &flag);

			switch (status) {
			case SWITCH_STATUS_RESAMPLE:
				write_frame = &session->raw_write_frame;
				if (!session->write_resampler) {
					status = switch_resample_create(&session->write_resampler,
													frame->codec->implementation->samples_per_second,
													frame->codec->implementation->bytes_per_frame * 10,
													session->write_codec->implementation->samples_per_second,
													session->write_codec->implementation->bytes_per_frame * 10,
													session->pool);
				}
				break;
			case SWITCH_STATUS_SUCCESS:
				write_frame = &session->raw_write_frame;
				break;
			case SWITCH_STATUS_NOOP:
				write_frame = frame;
				status = SWITCH_STATUS_SUCCESS;
				break;
			default:
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec %s decoder error!\n", frame->codec->codec_interface->interface_name);
				return status;
				break;
			}
		} 
		if (session->write_resampler) {
			short *data = write_frame->data;

			session->write_resampler->from_len = switch_short_to_float(data, session->write_resampler->from, (int)write_frame->datalen / 2);
			session->write_resampler->to_len = switch_resample_process(session->write_resampler,
																	   session->write_resampler->from,
																	   session->write_resampler->from_len, 
																	   session->write_resampler->to, 
																	   (int)session->write_resampler->to_size, 
																	   0);
			switch_float_to_short(session->write_resampler->to, data, write_frame->datalen * 2);
			write_frame->samples = session->write_resampler->to_len;
			write_frame->datalen = session->write_resampler->to_len * 2;
			write_frame->rate = session->write_resampler->to_rate;
		}
		if (session->write_codec) {
			if (write_frame->datalen == session->write_codec->implementation->bytes_per_frame) {
				perfect = TRUE;
			} else {
				if (!session->raw_write_buffer) {
					int bytes = session->write_codec->implementation->bytes_per_frame * 10;
					switch_console_printf(SWITCH_CHANNEL_CONSOLE,
										  "Engaging Write Buffer at %d bytes to accomidate %d->%d\n",
										  bytes,
										  write_frame->datalen,
										  session->write_codec->implementation->bytes_per_frame);
					if ((status = switch_buffer_create(session->pool, &session->raw_write_buffer, bytes)) != SWITCH_STATUS_SUCCESS) {
						switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Write Buffer Failed!\n");
						return status;
					}
				}
				if (!(switch_buffer_write(session->raw_write_buffer, write_frame->data, write_frame->datalen))) {
					return SWITCH_STATUS_MEMERR;
				}
			}

			if (perfect) {
				enc_frame = write_frame;
				session->enc_write_frame.datalen = session->enc_write_frame.buflen;

				status = switch_core_codec_encode(session->write_codec,
												  frame->codec,
												  enc_frame->data,
												  enc_frame->datalen,
												  session->write_codec->implementation->samples_per_second,
												  session->enc_write_frame.data,
												  &session->enc_write_frame.datalen,
												  &session->enc_write_frame.rate,
												  &flag);

				switch (status) {
				case SWITCH_STATUS_RESAMPLE:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "fixme 2\n");
				case SWITCH_STATUS_SUCCESS:
					write_frame = &session->enc_write_frame;
					break;
				case SWITCH_STATUS_NOOP:
					write_frame = enc_frame;
					status = SWITCH_STATUS_SUCCESS;
					break;
				default:
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec %s encoder error!\n", session->read_codec->codec_interface->interface_name);
					write_frame = NULL;
					return status;
					break;
				}

				status = perform_write(session, write_frame, timeout, io_flag, stream_id);
				return status;
			} else {
				int used = switch_buffer_inuse(session->raw_write_buffer);
				int bytes = session->write_codec->implementation->bytes_per_frame;
				int frames = (used / bytes);


				if (frames) {
					int x;
					for (x = 0; x < frames; x++) {
						if ((session->raw_write_frame.datalen =
							 switch_buffer_read(session->raw_write_buffer,
												session->raw_write_frame.data,
												bytes))) {

							enc_frame = &session->raw_write_frame;
							session->raw_write_frame.rate = session->write_codec->implementation->samples_per_second;
							session->enc_write_frame.datalen = session->enc_write_frame.buflen;
							status = switch_core_codec_encode(session->write_codec,
															  frame->codec,
															  enc_frame->data,
															  enc_frame->datalen,
															  frame->codec->implementation->samples_per_second,
															  session->enc_write_frame.data,
															  &session->enc_write_frame.datalen,
															  &session->enc_write_frame.rate,
															  &flag);



							switch (status) {
							case SWITCH_STATUS_RESAMPLE:
								write_frame = &session->enc_write_frame;
								if (!session->read_resampler) {
									status = switch_resample_create(&session->read_resampler,
																	frame->codec->implementation->samples_per_second,
																	frame->codec->implementation->bytes_per_frame * 10,
																	session->write_codec->implementation->samples_per_second,
																	session->write_codec->implementation->bytes_per_frame * 10,
																	session->pool);
								}
								break;
							case SWITCH_STATUS_SUCCESS:
								write_frame = &session->enc_write_frame;
								break;
							case SWITCH_STATUS_NOOP:
								write_frame = enc_frame;
								status = SWITCH_STATUS_SUCCESS;
								break;
							default:
								switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Codec %s encoder error!\n", session->read_codec->codec_interface->interface_name);
								write_frame = NULL;
								return status;
								break;
							}

							if (session->read_resampler) {
								short *data = write_frame->data;
								
								session->read_resampler->from_len = switch_short_to_float(data,
																						  session->read_resampler->from, 
																						  (int)write_frame->datalen / 2);
								session->read_resampler->to_len = switch_resample_process(session->read_resampler,
																						  session->read_resampler->from,
																						  session->read_resampler->from_len, 
																						  session->read_resampler->to, 
																						  (int)session->read_resampler->to_size, 
																						  0);
								switch_float_to_short(session->read_resampler->to, data, write_frame->datalen * 2);
								write_frame->samples = session->read_resampler->to_len;
								write_frame->datalen = session->read_resampler->to_len * 2;
								write_frame->rate = session->read_resampler->to_rate;
							}
							status = perform_write(session, write_frame, timeout, io_flag, stream_id);
						}
					}
					return status;
				}
			}
		}
	} else {
		status = perform_write(session, frame, timeout, io_flag, stream_id);
	}
	return status;
}

SWITCH_DECLARE(switch_status) switch_core_session_kill_channel(switch_core_session *session, switch_signal sig)
{
	struct switch_io_event_hook_kill_channel *ptr;
	switch_status status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->kill_channel) {
		if ((status = session->endpoint_interface->io_routines->kill_channel(session, sig)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.kill_channel; ptr ; ptr = ptr->next) {
				if ((status = ptr->kill_channel(session, sig)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;

}

SWITCH_DECLARE(switch_status) switch_core_session_waitfor_read(switch_core_session *session, int timeout, int stream_id)
{
	struct switch_io_event_hook_waitfor_read *ptr;
	switch_status status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->waitfor_read) {
		if ((status = session->endpoint_interface->io_routines->waitfor_read(session, timeout, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.waitfor_read; ptr ; ptr = ptr->next) {
				if ((status = ptr->waitfor_read(session, timeout, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;

}

SWITCH_DECLARE(switch_status) switch_core_session_waitfor_write(switch_core_session *session, int timeout, int stream_id)
{
	struct switch_io_event_hook_waitfor_write *ptr;
	switch_status status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->waitfor_write) {
		if ((status = session->endpoint_interface->io_routines->waitfor_write(session, timeout, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.waitfor_write; ptr ; ptr = ptr->next) {
				if ((status = ptr->waitfor_write(session, timeout, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;
}


SWITCH_DECLARE(switch_status) switch_core_session_send_dtmf(switch_core_session *session, char *dtmf) 
{
	struct switch_io_event_hook_send_dtmf *ptr;
	switch_status status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->send_dtmf) {
		if ((status = session->endpoint_interface->io_routines->send_dtmf(session, dtmf)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.send_dtmf; ptr ; ptr = ptr->next) {
				if ((status = ptr->send_dtmf(session, dtmf)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_outgoing(switch_core_session *session, switch_outgoing_channel_hook outgoing_channel)
{
	switch_io_event_hook_outgoing_channel *hook, *ptr;

	assert(outgoing_channel != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook)))) {
		hook->outgoing_channel = outgoing_channel;
		if (!session->event_hooks.outgoing_channel) {
			session->event_hooks.outgoing_channel = hook;
		} else {
			for(ptr = session->event_hooks.outgoing_channel ; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_answer_channel(switch_core_session *session, switch_answer_channel_hook answer_channel)
{
	switch_io_event_hook_answer_channel *hook, *ptr;

	assert(answer_channel != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook)))) {
		hook->answer_channel = answer_channel;
		if (!session->event_hooks.answer_channel) {
			session->event_hooks.answer_channel = hook;
		} else {
			for(ptr = session->event_hooks.answer_channel ; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_read_frame(switch_core_session *session, switch_read_frame_hook read_frame)
{
	switch_io_event_hook_read_frame *hook, *ptr;

	assert(read_frame != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook)))) {
		hook->read_frame = read_frame;
		if (!session->event_hooks.read_frame) {
			session->event_hooks.read_frame = hook;
		} else {
			for(ptr = session->event_hooks.read_frame ; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_write_frame(switch_core_session *session, switch_write_frame_hook write_frame)
{
	switch_io_event_hook_write_frame *hook, *ptr;

	assert(write_frame != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook)))) {
		hook->write_frame = write_frame;
		if (!session->event_hooks.write_frame) {
			session->event_hooks.write_frame = hook;
		} else {
			for(ptr = session->event_hooks.write_frame ; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_kill_channel(switch_core_session *session, switch_kill_channel_hook kill_channel)
{
	switch_io_event_hook_kill_channel *hook, *ptr;

	assert(kill_channel != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook)))) {
		hook->kill_channel = kill_channel;
		if (!session->event_hooks.kill_channel) {
			session->event_hooks.kill_channel = hook;
		} else {
			for(ptr = session->event_hooks.kill_channel ; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_waitfor_read(switch_core_session *session, switch_waitfor_read_hook waitfor_read)
{
	switch_io_event_hook_waitfor_read *hook, *ptr;

	assert(waitfor_read != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook)))) {
		hook->waitfor_read = waitfor_read;
		if (!session->event_hooks.waitfor_read) {
			session->event_hooks.waitfor_read = hook;
		} else {
			for(ptr = session->event_hooks.waitfor_read ; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_waitfor_write(switch_core_session *session, switch_waitfor_write_hook waitfor_write)
{
	switch_io_event_hook_waitfor_write *hook, *ptr;

	assert(waitfor_write != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook)))) {
		hook->waitfor_write = waitfor_write;
		if (!session->event_hooks.waitfor_write) {
			session->event_hooks.waitfor_write = hook;
		} else {
			for(ptr = session->event_hooks.waitfor_write ; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}


SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_send_dtmf(switch_core_session *session, switch_send_dtmf_hook send_dtmf)
{
	switch_io_event_hook_send_dtmf *hook, *ptr;

	assert(send_dtmf != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook)))) {
		hook->send_dtmf = send_dtmf;
		if (!session->event_hooks.send_dtmf) {
			session->event_hooks.send_dtmf = hook;
		} else {
			for(ptr = session->event_hooks.send_dtmf ; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}


SWITCH_DECLARE(switch_status) switch_core_new_memory_pool(switch_memory_pool **pool)
{

	if (runtime.memory_pool == NULL) {
		return SWITCH_STATUS_MEMERR;
	}

	if ((apr_pool_create(pool, runtime.memory_pool)) != APR_SUCCESS) {
		*pool = NULL;
		return SWITCH_STATUS_MEMERR;
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_core_destroy_memory_pool(switch_memory_pool **pool)
{
	apr_pool_destroy(*pool);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_channel *) switch_core_session_get_channel(switch_core_session *session)
{
	return session->channel;
}

static void switch_core_standard_on_init(switch_core_session *session)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Standard INIT %s\n", switch_channel_get_name(session->channel));
}

static void switch_core_standard_on_hangup(switch_core_session *session)
{

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Standard HANGUP %s\n", switch_channel_get_name(session->channel));

}

static void switch_core_standard_on_ring(switch_core_session *session)
{
	switch_dialplan_interface *dialplan_interface;
	switch_caller_profile *caller_profile;
	switch_caller_extension *extension;

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Standard RING %s\n", switch_channel_get_name(session->channel));

	if (!(caller_profile = switch_channel_get_caller_profile(session->channel))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't get profile!\n");
		switch_channel_set_state(session->channel, CS_HANGUP);
	} else {
		if (!(dialplan_interface = switch_loadable_module_get_dialplan_interface(caller_profile->dialplan))) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't get dialplan [%s]!\n", caller_profile->dialplan);
			switch_channel_set_state(session->channel, CS_HANGUP);
		} else {
			if ((extension = dialplan_interface->hunt_function(session))) {
				switch_channel_set_caller_extension(session->channel, extension);
			}
		}
	}
}

static void switch_core_standard_on_execute(switch_core_session *session)
{
	switch_caller_extension *extension;
	const switch_application_interface *application_interface;


	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Standard EXECUTE\n");
	if (!(extension = switch_channel_get_caller_extension(session->channel))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "No Extension!\n");
		switch_channel_set_state(session->channel, CS_HANGUP);
		return;
	}

	while (switch_channel_get_state(session->channel) == CS_EXECUTE && extension->current_application) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Execute %s(%s)\n", extension->current_application->application_name,
							  extension->current_application->application_data);
		if (!(application_interface = switch_loadable_module_get_application_interface(extension->current_application->application_name))) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid Application %s\n", extension->current_application->application_name);
			switch_channel_set_state(session->channel, CS_HANGUP);
			return;
		}

		if (!application_interface->application_function) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "No Function for %s\n", extension->current_application->application_name);
			switch_channel_set_state(session->channel, CS_HANGUP);
			return;
		}

		application_interface->application_function(session, extension->current_application->application_data);
		extension->current_application = extension->current_application->next;
	}

	switch_channel_set_state(session->channel, CS_HANGUP);
}

static void switch_core_standard_on_loopback(switch_core_session *session)
{
	switch_channel_state state;
	switch_frame *frame;
	int stream_id;

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Standard LOOPBACK\n");

	while ((state = switch_channel_get_state(session->channel)) == CS_LOOPBACK) {
		for(stream_id = 0; stream_id < session->stream_count; stream_id++) {
			if (switch_core_session_read_frame(session, &frame, -1, stream_id) == SWITCH_STATUS_SUCCESS) {
				switch_core_session_write_frame(session, frame, -1, stream_id);
			}
		}
	}
}

static void switch_core_standard_on_transmit(switch_core_session *session)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Standard TRANSMIT\n");
}


SWITCH_DECLARE(void) switch_core_session_signal_state_change(switch_core_session *session)
{
	switch_thread_cond_signal(session->cond);
}

SWITCH_DECLARE(void) switch_core_session_run(switch_core_session *session)
{
	switch_channel_state state = CS_NEW, laststate = CS_HANGUP, midstate = CS_DONE;
	const switch_endpoint_interface *endpoint_interface;
	const switch_event_handler_table *driver_event_handlers = NULL;
	const switch_event_handler_table *application_event_handlers = NULL;

	/*
	  Life of the channel. you have channel and pool in your session
	  everywhere you go you use the session to malloc with
	  switch_core_session_alloc(session, <size>)

	  The enpoint module gets the first crack at implementing the state
	  if it wants to, it can cancel the default behaviour by returning SWITCH_STATUS_FALSE

	  Next comes the channel's event handler table that can be set by an application
	  which also can veto the next behaviour in line by returning SWITCH_STATUS_FALSE

	  Finally the default state behaviour is called.


	*/
	assert(session != NULL);
	application_event_handlers = switch_channel_get_event_handlers(session->channel);

	endpoint_interface = session->endpoint_interface;
	assert(endpoint_interface != NULL);

	driver_event_handlers = endpoint_interface->event_handlers;
	assert(driver_event_handlers != NULL);

	switch_mutex_lock(session->mutex);

	while ((state = switch_channel_get_state(session->channel)) != CS_DONE) {
		switch_event *event;

		if (state != laststate) {
			midstate = state;

			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_STATE) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(session->channel, event);
				switch_event_fire(&event);
			}

			switch ( state ) {
			case CS_NEW: /* Just created, Waiting for first instructions */
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "State NEW\n");
				break;
			case CS_DONE:
				continue;
				break;
			case CS_HANGUP: /* Deactivate and end the thread */
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "State HANGUP\n");
				if (!driver_event_handlers->on_hangup ||
					(driver_event_handlers->on_hangup &&
					 driver_event_handlers->on_hangup(session) == SWITCH_STATUS_SUCCESS && 
					 midstate == switch_channel_get_state(session->channel))) {
					if (!application_event_handlers || !application_event_handlers->on_hangup ||
						(application_event_handlers->on_hangup &&
						 application_event_handlers->on_hangup(session) == SWITCH_STATUS_SUCCESS && 
						 midstate == switch_channel_get_state(session->channel))) {
						switch_core_standard_on_hangup(session);
					}
				}
				switch_channel_set_state(session->channel, CS_DONE);
				break;
			case CS_INIT: /* Basic setup tasks */
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "State INIT\n");
				if (!driver_event_handlers->on_init ||
					(driver_event_handlers->on_init &&
					 driver_event_handlers->on_init(session) == SWITCH_STATUS_SUCCESS && 
					 midstate == switch_channel_get_state(session->channel))) {
					if (!application_event_handlers || !application_event_handlers->on_init ||
						(application_event_handlers->on_init &&
						 application_event_handlers->on_init(session) == SWITCH_STATUS_SUCCESS && 
						 midstate == switch_channel_get_state(session->channel))) {
						switch_core_standard_on_init(session);
					}
				}
				break;
			case CS_RING: /* Look for a dialplan and find something to do */
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "State RING\n");
				if (!driver_event_handlers->on_ring ||
					(driver_event_handlers->on_ring &&
					 driver_event_handlers->on_ring(session) == SWITCH_STATUS_SUCCESS && 
					 midstate == switch_channel_get_state(session->channel))) {
					if (!application_event_handlers || !application_event_handlers->on_ring ||
						(application_event_handlers->on_ring &&
						 application_event_handlers->on_ring(session) == SWITCH_STATUS_SUCCESS && 
						 midstate == switch_channel_get_state(session->channel))) {
						switch_core_standard_on_ring(session);
					}
				}
				break;
			case CS_EXECUTE: /* Execute an Operation*/
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "State EXECUTE\n");
				if (!driver_event_handlers->on_execute ||
					(driver_event_handlers->on_execute &&
					 driver_event_handlers->on_execute(session) == SWITCH_STATUS_SUCCESS && 
					 midstate == switch_channel_get_state(session->channel))) {
					if (!application_event_handlers || !application_event_handlers->on_execute ||
						(application_event_handlers->on_execute &&
						 application_event_handlers->on_execute(session) == SWITCH_STATUS_SUCCESS && 
						 midstate == switch_channel_get_state(session->channel))) {
						switch_core_standard_on_execute(session);
					}
				}
				break;
			case CS_LOOPBACK: /* loop all data back to source */
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "State LOOPBACK\n");
				if (!driver_event_handlers->on_loopback ||
					(driver_event_handlers->on_loopback &&
					 driver_event_handlers->on_loopback(session) == SWITCH_STATUS_SUCCESS && 
					 midstate == switch_channel_get_state(session->channel))) {
					if (!application_event_handlers || !application_event_handlers->on_loopback ||
						(application_event_handlers->on_loopback &&
						 application_event_handlers->on_loopback(session) == SWITCH_STATUS_SUCCESS && 
						 midstate == switch_channel_get_state(session->channel))) {
						switch_core_standard_on_loopback(session);
					}
				}
				break;
			case CS_TRANSMIT: /* send/recieve data to/from another channel */
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "State TRANSMIT\n");
				if (!driver_event_handlers->on_transmit ||
					(driver_event_handlers->on_transmit &&
					 driver_event_handlers->on_transmit(session) == SWITCH_STATUS_SUCCESS && 
					 midstate == switch_channel_get_state(session->channel))) {
					if (!application_event_handlers || !application_event_handlers->on_transmit ||
						(application_event_handlers->on_transmit &&
						 application_event_handlers->on_transmit(session) == SWITCH_STATUS_SUCCESS && 
						 midstate == switch_channel_get_state(session->channel))) {
						switch_core_standard_on_transmit(session);
					}
				}
				break;
			}

			laststate = midstate;
		}

		if (state < CS_DONE && midstate == switch_channel_get_state(session->channel)) {
			switch_thread_cond_wait(session->cond, session->mutex);
		}
	}
}

SWITCH_DECLARE(void) switch_core_session_destroy(switch_core_session **session)
{
	switch_memory_pool *pool;

	pool = (*session)->pool;
	*session = NULL;
	apr_pool_destroy(pool);
	pool = NULL;

}

SWITCH_DECLARE(switch_status) switch_core_hash_init(switch_hash **hash, switch_memory_pool *pool)
{
	if ((*hash = apr_hash_make(pool))) {
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_GENERR;
}

SWITCH_DECLARE(switch_status) switch_core_hash_destroy(switch_hash *hash)
{
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_core_hash_insert_dup(switch_hash *hash, char *key, void *data)
{
	apr_hash_set(hash, switch_core_strdup(apr_hash_pool_get(hash), key), APR_HASH_KEY_STRING, data);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_core_hash_insert(switch_hash *hash, char *key, void *data)
{
	apr_hash_set(hash, key, APR_HASH_KEY_STRING, data);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_core_hash_delete(switch_hash *hash, char *key)
{
	apr_hash_set(hash, key, APR_HASH_KEY_STRING, NULL);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void *) switch_core_hash_find(switch_hash *hash, char *key)
{
	return apr_hash_get(hash, key, APR_HASH_KEY_STRING);
}

/* This function abstracts the thread creation for modules by allowing you to pass a function ptr and
   a void object and trust that that the function will be run in a thread with arg  This lets
   you request and activate a thread without giving up any knowledge about what is in the thread
   neither the core nor the calling module know anything about each other.

   This thread is expected to never exit until the application exits so the func is responsible
   to make sure that is the case.

   The typical use for this is so switch_loadable_module.c can start up a thread for each module
   passing the table of module methods as a session obj into the core without actually allowing
   the core to have any clue and keeping switch_loadable_module.c from needing any thread code.

*/

SWITCH_DECLARE(void) switch_core_launch_thread(switch_thread_start_t func, void *obj, switch_memory_pool *pool)
{
	switch_thread *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_core_thread_session *ts;
	int mypool;

	mypool = pool ? 0 : 1;

	if (!pool && switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate memory pool\n");
		return;
	}

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);

	if (!(ts = switch_core_alloc(pool, sizeof(*ts)))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate memory\n");
	} else {
		if (mypool) {
			ts->pool = pool;
		}
		ts->objs[0] = obj;

		switch_thread_create(&thread,
							 thd_attr,
							 func,
							 ts,
							 pool
							 );
	}

}

static void * SWITCH_THREAD_FUNC switch_core_session_thread(switch_thread *thread, void *obj)
{
	switch_core_session *session = obj;


	session->thread = thread;

	session->id = runtime.session_id++;
	if(runtime.session_id >= sizeof(unsigned long))
		runtime.session_id = 1;

	snprintf(session->name, sizeof(session->name), "%ld", session->id);

	switch_core_hash_insert(runtime.session_table, session->name, session);
	switch_core_session_run(session);
	switch_core_session_destroy(&session);


	return NULL;
}


SWITCH_DECLARE(void) switch_core_session_thread_launch(switch_core_session *session)
{
	switch_thread *thread;
	switch_threadattr_t *thd_attr;;
	switch_threadattr_create(&thd_attr, session->pool);
	switch_threadattr_detach_set(thd_attr, 1);

	if (switch_thread_create(&thread,
							 thd_attr,
							 switch_core_session_thread,
							 session,
							 session->pool
							 ) != APR_SUCCESS) {
		switch_core_session_destroy(&session);
	}

}

SWITCH_DECLARE(void) switch_core_session_launch_thread(switch_core_session *session, switch_thread_start_t func, void *obj)
{
	switch_thread *thread;
	switch_threadattr_t *thd_attr;;
	switch_threadattr_create(&thd_attr, session->pool);
	switch_threadattr_detach_set(thd_attr, 1);

	switch_thread_create(&thread,
						 thd_attr,
						 func,
						 obj,
						 session->pool
						 );

}


SWITCH_DECLARE(void *) switch_core_alloc(switch_memory_pool *pool, size_t memory)
{
	void *ptr = NULL;
	assert(pool != NULL);

	if ((ptr = apr_palloc(pool, memory))) {
		memset(ptr, 0, memory);
	}
	return ptr;
}

SWITCH_DECLARE(switch_core_session *) switch_core_session_request(const switch_endpoint_interface *endpoint_interface, switch_memory_pool *pool)
{
	switch_memory_pool *usepool;
	switch_core_session *session;
	switch_uuid_t uuid;

	assert(endpoint_interface != NULL);

	if (pool) {
		usepool = pool;
	} else if (switch_core_new_memory_pool(&usepool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate memory pool\n");
		return NULL;
	}

	if (!(session = switch_core_alloc(usepool, sizeof(switch_core_session)))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate session\n");
		apr_pool_destroy(usepool);
		return NULL;
	}

	if (switch_channel_alloc(&session->channel, usepool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate channel structure\n");
		apr_pool_destroy(usepool);
		return NULL;
	}

	switch_channel_init(session->channel, session, CS_NEW, CF_SEND_AUDIO | CF_RECV_AUDIO);

	/* The session *IS* the pool you may not alter it because you have no idea how
	   its all private it will be passed to the thread run function */

	switch_uuid_get(&uuid);
	switch_uuid_format(session->uuid_str, &uuid);

	session->pool = usepool;
	session->endpoint_interface = endpoint_interface;

	session->raw_write_frame.data = session->raw_write_buf;
	session->raw_write_frame.buflen = sizeof(session->raw_write_buf);
	session->raw_read_frame.data = session->raw_read_buf;
	session->raw_read_frame.buflen = sizeof(session->raw_read_buf);


	session->enc_write_frame.data = session->enc_write_buf;
	session->enc_write_frame.buflen = sizeof(session->enc_write_buf);
	session->enc_read_frame.data = session->enc_read_buf;
	session->enc_read_frame.buflen = sizeof(session->enc_read_buf);

	switch_mutex_init(&session->mutex, SWITCH_MUTEX_NESTED ,session->pool);
	switch_thread_cond_create(&session->cond, session->pool);

	return session;
}

SWITCH_DECLARE(switch_core_session *) switch_core_session_request_by_name(char *endpoint_name, switch_memory_pool *pool)
{
	const switch_endpoint_interface *endpoint_interface;

	if (!(endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not locate channel type %s\n", endpoint_name);
		return NULL;
	}

	return switch_core_session_request(endpoint_interface, pool);
}


static void core_event_handler (switch_event *event)
{
	char buf[1024];

	switch(event->event_id) {
	case SWITCH_EVENT_LOG:
		return;
		break;
	default:
		buf[0] = '\0';
		//switch_event_serialize(event, buf, sizeof(buf), NULL);
		//switch_console_printf(SWITCH_CHANNEL_CONSOLE, "\nCORE EVENT\n--------------------------------\n%s\n", buf);
		break;
	}
}

SWITCH_DECLARE(switch_status) switch_core_init(void)
{
#ifdef EMBED_PERL
	PerlInterpreter *my_perl;
#endif

	runtime.console = stdout;

	/* INIT APR and Create the pool context */
	if (apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return SWITCH_STATUS_MEMERR;
	}

	if (apr_pool_create(&runtime.memory_pool, NULL) != APR_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate memory pool\n");
		switch_core_destroy();
		return SWITCH_STATUS_MEMERR;
	}



	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Allocated memory pool.\n");
	switch_event_init(runtime.memory_pool);

	assert(runtime.memory_pool != NULL);

	/* Activate SQL database */
	if (!(runtime.db = switch_core_db_handle())) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Error Opening DB!\n");
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Opening DB\n");
		if (switch_event_bind("core_db", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Couldn't bind event handler!\n");
		}
	}

#ifdef EMBED_PERL
	if (! (my_perl = perl_alloc())) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate perl intrepreter\n");
		switch_core_destroy();
		return SWITCH_STATUS_MEMERR;
	}
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Allocated perl intrepreter.\n");

	PERL_SET_CONTEXT(my_perl);
	perl_construct(my_perl);
	perl_parse(my_perl, xs_init, 3, embedding, NULL);
	perl_run(my_perl);
	runtime.my_perl = my_perl;
#endif

	runtime.session_id = 1;

	switch_core_hash_init(&runtime.session_table, runtime.memory_pool);

	/* set signal handlers and startup time */
	(void) signal(SIGINT,(void *) handle_SIGINT);
#ifdef SIGPIPE
	(void) signal(SIGPIPE,(void *) handle_SIGPIPE);
#endif
#ifdef TRAP_BUS
	(void) signal(SIGBUS,(void *) handle_SIGBUS);
#endif
	time(&runtime.initiated);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status) switch_core_destroy(void)
{

#ifdef EMBED_PERL
	if (runtime.my_perl) {
		perl_destruct(runtime.my_perl);
		perl_free(runtime.my_perl);
		runtime.my_perl = NULL;
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Unallocated perl interpreter.\n");
	}
#endif

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Closing Event Engine.\n");
	switch_event_shutdown();


	switch_core_db_close(runtime.db);

	if (runtime.memory_pool) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Unallocating memory pool.\n");
		apr_pool_destroy(runtime.memory_pool);
		apr_terminate();
	}

	return SWITCH_STATUS_SUCCESS;
}

