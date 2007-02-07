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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core.c -- Main Core Library
 *
 */

#include <switch.h>
#include <stdio.h>
#include <switch_version.h>

#ifdef HAVE_MLOCKALL
#include <sys/mman.h>
#endif

//#define DEBUG_ALLOC
#define DO_EVENTS

#ifdef CRASH_PROT
#define __CP "ENABLED"
#else
#define __CP "DISABLED"
#endif

#define SWITCH_EVENT_QUEUE_LEN 256
#define SWITCH_MESSAGE_QUEUE_LEN 256
#define SWITCH_SQL_QUEUE_LEN 2000

#define SWITCH_BUFFER_BLOCK_FRAMES 25
#define SWITCH_BUFFER_START_FRAMES 50

struct switch_media_bug {
	switch_buffer_t *raw_write_buffer;
	switch_buffer_t *raw_read_buffer;
    switch_frame_t *replace_frame_in;
    switch_frame_t *replace_frame_out;
	switch_media_bug_callback_t callback;
	switch_mutex_t *read_mutex;
	switch_mutex_t *write_mutex;
	switch_core_session_t *session;
	void *user_data;
	uint32_t flags;
    uint8_t ready;
	struct switch_media_bug *next;
};

typedef enum {
    SSF_NONE = 0,
    SSF_DESTROYED = (1 << 0)
} switch_session_flag_t;

struct switch_core_session {
	uint32_t id;
	char name[80];
    switch_session_flag_t flags;
	int thread_running;
	switch_memory_pool_t *pool;
	switch_channel_t *channel;
	switch_thread_t *thread;
	const switch_endpoint_interface_t *endpoint_interface;
	switch_io_event_hooks_t event_hooks;
	switch_codec_t *read_codec;
	switch_codec_t *write_codec;

	switch_buffer_t *raw_write_buffer;
	switch_frame_t raw_write_frame;
	switch_frame_t enc_write_frame;
	uint8_t raw_write_buf[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	uint8_t enc_write_buf[SWITCH_RECCOMMENDED_BUFFER_SIZE];

	switch_buffer_t *raw_read_buffer;
	switch_frame_t raw_read_frame;
	switch_frame_t enc_read_frame;
	uint8_t raw_read_buf[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	uint8_t enc_read_buf[SWITCH_RECCOMMENDED_BUFFER_SIZE];


	switch_audio_resampler_t *read_resampler;
	switch_audio_resampler_t *write_resampler;

	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;

	switch_thread_rwlock_t *rwlock;

	void *streams[SWITCH_MAX_STREAMS];
	int stream_count;

	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	void *private_info;
	switch_queue_t *event_queue;
	switch_queue_t *message_queue;
	switch_queue_t *private_event_queue;
	switch_thread_rwlock_t *bug_rwlock;
	switch_media_bug_t *bugs;
};

SWITCH_DECLARE_DATA switch_directories SWITCH_GLOBAL_dirs;

struct switch_core_runtime {
	switch_time_t initiated;
	uint32_t session_id;
	apr_pool_t *memory_pool;
	switch_hash_t *session_table;
	switch_hash_t *global_vars;
	switch_mutex_t *session_table_mutex;
#ifdef CRASH_PROT
	switch_hash_t *stack_table;
#endif
	switch_core_db_t *db;
	switch_core_db_t *event_db;
	const switch_state_handler_table_t *state_handlers[SWITCH_MAX_STATE_HANDLERS];
    int state_handler_index;
	FILE *console;
	uint32_t session_count;
	uint32_t session_limit;
	switch_queue_t *sql_queue;
	uint32_t no_new_sessions;
	uint32_t shutting_down;
	uint8_t running;
};

/* Prototypes */
static void *SWITCH_THREAD_FUNC switch_core_session_thread(switch_thread_t *thread, void *obj);
static void switch_core_standard_on_init(switch_core_session_t *session);
static void switch_core_standard_on_hangup(switch_core_session_t *session);
static void switch_core_standard_on_ring(switch_core_session_t *session);
static void switch_core_standard_on_execute(switch_core_session_t *session);
static void switch_core_standard_on_loopback(switch_core_session_t *session);
static void switch_core_standard_on_transmit(switch_core_session_t *session);
static void switch_core_standard_on_hold(switch_core_session_t *session);


/* The main runtime obj we keep this hidden for ourselves */
static struct switch_core_runtime runtime;


static void db_pick_path(char *dbname, char *buf, switch_size_t size)
{

	memset(buf, 0, size);
	if (strchr(dbname, '/')) {
		strncpy(buf, dbname, size);
	} else {
		snprintf(buf, size, "%s%s%s.db", SWITCH_GLOBAL_dirs.db_dir, SWITCH_PATH_SEPARATOR, dbname);
	}
}

static void switch_core_media_bug_destroy(switch_media_bug_t *bug)
{
	switch_buffer_destroy(&bug->raw_read_buffer);
	switch_buffer_destroy(&bug->raw_write_buffer);
}


SWITCH_DECLARE(switch_frame_t *) switch_core_media_bug_get_replace_frame(switch_media_bug_t *bug)
{
    return bug->replace_frame_in;
}

SWITCH_DECLARE(void) switch_core_media_bug_set_replace_frame(switch_media_bug_t *bug, switch_frame_t *frame)
{
    bug->replace_frame_out = frame;
}

SWITCH_DECLARE(void *) switch_core_media_bug_get_user_data(switch_media_bug_t *bug)
{
	return bug->user_data;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_read(switch_media_bug_t *bug, switch_frame_t *frame)
{
	uint32_t bytes = 0;
	uint8_t data[SWITCH_RECCOMMENDED_BUFFER_SIZE] = {0};
	uint32_t datalen = 0;
	int16_t *dp, *fp;
	uint32_t x;
	size_t rlen = 0;
	size_t wlen = 0;
	uint32_t blen;
	size_t rdlen = 0;
	uint32_t maxlen;


	if (bug->raw_read_buffer) {
		rlen = switch_buffer_inuse(bug->raw_read_buffer);
	}

	if (bug->raw_write_buffer) {
		wlen = switch_buffer_inuse(bug->raw_write_buffer);
	}

	if ((bug->raw_read_buffer && bug->raw_write_buffer) && (!rlen && !wlen)) {
		return SWITCH_STATUS_FALSE;
	}


	maxlen = sizeof(data) > frame->buflen ? frame->buflen :  sizeof(data);
	if ((rdlen = rlen > wlen ? wlen : rlen) > maxlen) {
		rdlen = maxlen;
	}

	if (!rdlen) {
		rdlen = maxlen;
	}

	frame->datalen = 0;

	if (rlen) {
		switch_mutex_lock(bug->read_mutex);
		
		frame->datalen = (uint32_t) switch_buffer_read(bug->raw_read_buffer,
													   frame->data,
													   rdlen);
		switch_mutex_unlock(bug->read_mutex);
	}

	if (wlen) {
		switch_mutex_lock(bug->write_mutex);
		datalen = (uint32_t) switch_buffer_read(bug->raw_write_buffer,
												data,
												rdlen);
		switch_mutex_unlock(bug->write_mutex);
	}


	bytes = (datalen > frame->datalen) ? datalen : frame->datalen;

	if (bytes) {
		dp = (int16_t *) data;
		fp = (int16_t *) frame->data;
		
		rlen = frame->datalen / 2;
		wlen = datalen / 2;
		blen = bytes / 2;
		
		for(x = 0; x < blen; x++) {
			int32_t z = 0;

			if (x < rlen) {
				z += (int32_t) *(fp+x);
			}
			if (x < wlen) {
				z += (int32_t)*(dp+x);
			}
			switch_normalize_to_16bit(z);
			*(fp+x) = (int16_t) z;
		}

		frame->datalen = bytes;


		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

#define MAX_BUG_BUFFER 1024 * 512
SWITCH_DECLARE(switch_status_t) switch_core_media_bug_add(switch_core_session_t *session,
														  switch_media_bug_callback_t callback,
														  void *user_data,
														  switch_media_bug_flag_t flags,
														  switch_media_bug_t **new_bug)

{
	switch_media_bug_t *bug, *bp;
	switch_size_t bytes;

    if (flags & SMBF_WRITE_REPLACE) {
        switch_thread_rwlock_wrlock(session->bug_rwlock);
        for (bp = session->bugs; bp; bp = bp->next) {
            if (switch_test_flag(bp, SMBF_WRITE_REPLACE)) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Only one bug of this type allowed!\n");
                switch_thread_rwlock_unlock(session->bug_rwlock);
                return SWITCH_STATUS_GENERR;
            }
        }
        switch_thread_rwlock_unlock(session->bug_rwlock);
    }

	if (!(bug = switch_core_session_alloc(session, sizeof(*bug)))) {
		return SWITCH_STATUS_MEMERR;
	}

	bug->callback = callback;
	bug->user_data = user_data;
	bug->session = session;
	bug->flags = flags;
    bug->ready = 1;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Attaching BUG to %s\n", switch_channel_get_name(session->channel));
	bytes = session->read_codec->implementation->bytes_per_frame;

	if (!bug->flags) {
		bug->flags = (SMBF_READ_STREAM | SMBF_WRITE_STREAM);
	}

	if (switch_test_flag(bug, SMBF_READ_STREAM)) {
		switch_buffer_create_dynamic(&bug->raw_read_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, MAX_BUG_BUFFER);
		switch_mutex_init(&bug->read_mutex, SWITCH_MUTEX_NESTED, session->pool);
	}

	bytes = session->write_codec->implementation->bytes_per_frame;

	if (switch_test_flag(bug, SMBF_WRITE_STREAM)) {
		switch_buffer_create_dynamic(&bug->raw_write_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, MAX_BUG_BUFFER);
		switch_mutex_init(&bug->write_mutex, SWITCH_MUTEX_NESTED, session->pool);
	}

	switch_thread_rwlock_wrlock(session->bug_rwlock);
	bug->next = session->bugs;
	session->bugs = bug;
	switch_thread_rwlock_unlock(session->bug_rwlock);
	*new_bug = bug;

	if (bug->callback) {
		bug->callback(bug, bug->user_data, SWITCH_ABC_TYPE_INIT);
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove_all(switch_core_session_t *session)
{
	switch_media_bug_t *bp;

	if (session->bugs) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if (bp->callback) {
				bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_CLOSE);
			}
			switch_core_media_bug_destroy(bp);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing BUG from %s\n", switch_channel_get_name(session->channel));
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
		session->bugs = NULL;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove(switch_core_session_t *session, switch_media_bug_t **bug)
{
	switch_media_bug_t *bp = NULL, *last = NULL;

	if (session->bugs) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
            if (!bp->ready) {
                continue;
            }
			if (bp == *bug) {
				if (last) {
					last->next = bp->next;
				} else {
					session->bugs = bp->next;
				}
				break;
			}
			last = bp;
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);

		if (bp) {
			if (bp->callback) {
				bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_CLOSE);
                bp->ready = 0;
			}
			switch_core_media_bug_destroy(bp);
			*bug = NULL;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing BUG from %s\n", switch_channel_get_name(session->channel));
			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
}

struct switch_core_port_allocator {
	switch_port_t start;
	switch_port_t end;
	switch_port_t next;
	uint8_t inc;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
};

SWITCH_DECLARE(switch_status_t) switch_core_port_allocator_new(switch_port_t start,
															   switch_port_t end,
															   uint8_t inc,
															   switch_core_port_allocator_t **new_allocator)
{
	switch_status_t status;
	switch_memory_pool_t *pool;
	switch_core_port_allocator_t *alloc;

	if ((status = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if (!(alloc = switch_core_alloc(pool, sizeof(*alloc)))) {
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_MEMERR;
	}
	
	alloc->start = start;
	alloc->next = start;
	alloc->end = end;
	if (!(alloc->inc = inc)) {
		alloc->inc = 2;
	}
	switch_mutex_init(&alloc->mutex, SWITCH_MUTEX_NESTED, pool);
	alloc->pool = pool;
	*new_allocator = alloc;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_port_t) switch_core_port_allocator_request_port(switch_core_port_allocator_t *alloc)
{
	switch_port_t port;

	switch_mutex_lock(alloc->mutex);
	port = alloc->next;
	alloc->next = alloc->next + alloc->inc;
	if (alloc->next > alloc->end) {
		alloc->next = alloc->start;
	}
	switch_mutex_unlock(alloc->mutex);
	return port;
}

SWITCH_DECLARE(void) switch_core_port_allocator_destroy(switch_core_port_allocator_t **alloc)
{
	switch_memory_pool_t *pool = (*alloc)->pool;
	switch_core_destroy_memory_pool(&pool);
	*alloc = NULL;
}

SWITCH_DECLARE(switch_core_db_t *) switch_core_db_open_file(char *filename)
{
	switch_core_db_t *db;
	char path[1024];

	db_pick_path(filename, path, sizeof(path));
	if (switch_core_db_open(path, &db)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", switch_core_db_errmsg(db));
		switch_core_db_close(db);
		db = NULL;
	}
	return db;
}


SWITCH_DECLARE(void) switch_core_db_test_reactive(switch_core_db_t *db, char *test_sql, char *reactive_sql) 
{
    char *errmsg;

    if(db) {
        if(test_sql) {
            switch_core_db_exec(
                         db,
                         test_sql,
                         NULL,
                         NULL,
                         &errmsg
                         );

            if (errmsg) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n[%s]\nAuto Generating Table!\n", errmsg, test_sql);
                switch_core_db_free(errmsg);
                errmsg = NULL;
                switch_core_db_exec(
									db,
                             reactive_sql,
                             NULL,
                             NULL,
                             &errmsg
                             );
                if (errmsg) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n[%s]\n", errmsg, reactive_sql);
                    switch_core_db_free(errmsg);
                    errmsg = NULL;
                }
            }
        }
    }

}



SWITCH_DECLARE(switch_status_t) switch_core_set_console(char *console)
{
	if ((runtime.console = fopen(console, "a")) == 0) {
		fprintf(stderr, "Cannot open output file %s.\n", console);
		return SWITCH_STATUS_FALSE;
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(FILE *) switch_core_get_console(void)
{
	return runtime.console;
}

SWITCH_DECLARE(FILE *) switch_core_data_channel(switch_text_channel_t channel)
{
	FILE *handle = stdout;

	switch (channel) {
	case SWITCH_CHANNEL_ID_LOG:
	case SWITCH_CHANNEL_ID_LOG_CLEAN:
		handle = runtime.console;
		break;
	default:
		handle = runtime.console;
		break;
	}

	return handle;
}

SWITCH_DECLARE(int) switch_core_add_state_handler(const switch_state_handler_table_t *state_handler)
{
	int index = runtime.state_handler_index++;

	if (runtime.state_handler_index >= SWITCH_MAX_STATE_HANDLERS) {
		return -1;
	}

	runtime.state_handlers[index] = state_handler;
	return index;
}

SWITCH_DECLARE(const switch_state_handler_table_t *) switch_core_get_state_handler(int index)
{

	if (index > SWITCH_MAX_STATE_HANDLERS || index > runtime.state_handler_index) {
		return NULL;
	}

	return runtime.state_handlers[index];
}

#ifdef SWITCH_DEBUG_RWLOCKS
SWITCH_DECLARE(switch_status_t) switch_core_session_perform_read_lock(switch_core_session_t *session,
                                                                      const char *file,
                                                                      const char *func,
                                                                      int line)
#else
SWITCH_DECLARE(switch_status_t) switch_core_session_read_lock(switch_core_session_t *session)
#endif
{
	switch_status_t status = SWITCH_STATUS_FALSE;

    if (session->rwlock) {
        if (switch_test_flag(session, SSF_DESTROYED)) {
            status = SWITCH_STATUS_FALSE;
#ifdef SWITCH_DEBUG_RWLOCKS
            switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_DEBUG, "%s Read lock FAIL\n", 
                              switch_channel_get_name(session->channel));
#endif
        } else {
            status = (switch_status_t) switch_thread_rwlock_tryrdlock(session->rwlock);
#ifdef SWITCH_DEBUG_RWLOCKS
            switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_DEBUG, "%s Read lock AQUIRED\n", 
                              switch_channel_get_name(session->channel));
#endif
        }	
    }

	return status;
}

#ifdef SWITCH_DEBUG_RWLOCKS
SWITCH_DECLARE(void) switch_core_session_perform_write_lock(switch_core_session_t *session,
                                                            const char *file,
                                                            const char *func,
                                                            int line)
{

    switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_DEBUG, "%s Write lock AQUIRED\n", 
                      switch_channel_get_name(session->channel));
#else
SWITCH_DECLARE(void) switch_core_session_write_lock(switch_core_session_t *session)
{
#endif
	switch_thread_rwlock_wrlock(session->rwlock);
}

#ifdef SWITCH_DEBUG_RWLOCKS
SWITCH_DECLARE(void) switch_core_session_perform_rwunlock(switch_core_session_t *session,
                                                  const char *file,
                                                  const char *func,
                                                  int line)
{
    switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_DEBUG, "%s Read/Write lock CLEARED\n", 
                      switch_channel_get_name(session->channel));
#else
SWITCH_DECLARE(void) switch_core_session_rwunlock(switch_core_session_t *session)
{
#endif
	switch_thread_rwlock_unlock(session->rwlock);
    
}

SWITCH_DECLARE(char *) switch_core_get_variable(char *varname)
{
	return (char *) switch_core_hash_find(runtime.global_vars, varname);
}

SWITCH_DECLARE(void) switch_core_set_variable(char *varname, char *value)
{
    switch_core_hash_insert(runtime.global_vars, switch_core_strdup(runtime.memory_pool, varname), switch_core_strdup(runtime.memory_pool, value));
}


#ifdef SWITCH_DEBUG_RWLOCKS
SWITCH_DECLARE(switch_core_session_t *) switch_core_session_perform_locate(char *uuid_str,
                                                                           const char *file,
                                                                           const char *func,
                                                                           int line)
#else
SWITCH_DECLARE(switch_core_session_t *) switch_core_session_locate(char *uuid_str)
#endif
{
	switch_core_session_t *session = NULL;

	if (uuid_str) {
		switch_mutex_lock(runtime.session_table_mutex);
		if ((session = switch_core_hash_find(runtime.session_table, uuid_str))) {
			/* Acquire a read lock on the session */
#ifdef SWITCH_DEBUG_RWLOCKS
			if (switch_core_session_perform_read_lock(session, file, func, line) != SWITCH_STATUS_SUCCESS) {
#else
            if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
#endif
				/* not available, forget it */
				session = NULL;
			}
		}
		switch_mutex_unlock(runtime.session_table_mutex);
	}

    /* if its not NULL, now it's up to you to rwunlock this */
    return session;
}

SWITCH_DECLARE(void) switch_core_session_hupall(switch_call_cause_t cause)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;
	switch_channel_t *channel;
    uint32_t loops = 0;

	switch_mutex_lock(runtime.session_table_mutex);
	for (hi = switch_hash_first(runtime.memory_pool, runtime.session_table); hi; hi = switch_hash_next(hi)) {
        switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			session = (switch_core_session_t *) val;
			channel = switch_core_session_get_channel(session);
			switch_channel_hangup(channel, cause);
		}
	}
	switch_mutex_unlock(runtime.session_table_mutex);

	while(runtime.session_count > 0) {
		switch_yield(100000);
        if (++loops == 100) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Giving up with %d session%s remaining\n", 
                              runtime.session_count, runtime.session_count == 1 ? "" : "s");
            break;
        }
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_session_message_send(char *uuid_str, switch_core_session_message_t *message)
{
	switch_core_session_t *session = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(runtime.session_table_mutex);
	if ((session = switch_core_hash_find(runtime.session_table, uuid_str)) != 0) {
		/* Acquire a read lock on the session or forget it the channel is dead */
		if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
			if (switch_channel_get_state(session->channel) < CS_HANGUP) {
				status = switch_core_session_receive_message(session, message);
			}
            switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.session_table_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_event_send(char *uuid_str, switch_event_t **event)
{
	switch_core_session_t *session = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(runtime.session_table_mutex);
	if ((session = switch_core_hash_find(runtime.session_table, uuid_str)) != 0) {
		/* Acquire a read lock on the session or forget it the channel is dead */
		if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
			if (switch_channel_get_state(session->channel) < CS_HANGUP) {
				status = switch_core_session_queue_event(session, event);
			}
            switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.session_table_mutex);

	return status;
}

SWITCH_DECLARE(char *) switch_core_session_get_uuid(switch_core_session_t *session)
{
	return session->uuid_str;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_read_codec(switch_core_session_t *session, switch_codec_t *codec)
{
	switch_event_t *event;

	assert(session != NULL);

	if (switch_event_create(&event, SWITCH_EVENT_CODEC) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(session->channel, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-read-codec-name", codec->implementation->iananame);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-read-codec-rate", "%d", codec->implementation->samples_per_second);
		switch_event_fire(&event);
	}
				
	session->read_codec = codec;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_read_codec(switch_core_session_t *session)
{
	return session->read_codec;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_write_codec(switch_core_session_t *session, switch_codec_t *codec)
{
	switch_event_t *event;
	assert(session != NULL);

	if (switch_event_create(&event, SWITCH_EVENT_CODEC) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(session->channel, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-write-codec-name", codec->implementation->iananame);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-write-codec-rate", "%d", codec->implementation->samples_per_second);
		switch_event_fire(&event);
	}

	session->write_codec = codec;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_codec_t *) switch_core_session_get_write_codec(switch_core_session_t *session)
{
	return session->write_codec;
}

SWITCH_DECLARE(switch_status_t) switch_core_codec_init(switch_codec_t *codec, char *codec_name, char *fmtp, uint32_t rate, int ms,
													 int channels, uint32_t flags,
													 const switch_codec_settings_t *codec_settings,
													 switch_memory_pool_t *pool)
{
	const switch_codec_interface_t *codec_interface;
	const switch_codec_implementation_t *iptr, *implementation = NULL;

	assert(codec != NULL);
	assert(codec_name != NULL);

	memset(codec, 0, sizeof(*codec));

	if ((codec_interface = switch_loadable_module_get_codec_interface(codec_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid codec %s!\n", codec_name);
		return SWITCH_STATUS_GENERR;
	}

    /* If no specific codec interval is requested opt for 20ms above all else because lots of stuff assumes it */
    if (!ms) {
        for (iptr = codec_interface->implementations; iptr; iptr = iptr->next) {
            if ((!rate || rate == iptr->samples_per_second) &&
                (20 == (iptr->microseconds_per_frame / 1000)) &&
                (!channels || channels == iptr->number_of_channels)) {
                implementation = iptr;
                goto found;
            }
        }
    }

    /* Either looking for a specific interval or there was no interval specified and there wasn't one @20ms available*/
	for (iptr = codec_interface->implementations; iptr; iptr = iptr->next) {
		if ((!rate || rate == iptr->samples_per_second) &&
			(!ms || ms == (iptr->microseconds_per_frame / 1000)) &&
			(!channels || channels == iptr->number_of_channels)) {
			implementation = iptr;
			break;
		}
	}

 found:

	if (implementation) {
		switch_status_t status;
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

		if (fmtp) {
			codec->fmtp_in = switch_core_strdup(codec->memory_pool, fmtp);
		}

		implementation->init(codec, flags, codec_settings);

		return SWITCH_STATUS_SUCCESS;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Codec %s Exists but not at the desired implementation. %dhz %dms\n",
						  codec_name,
						  rate,
						  ms);
	}

	return SWITCH_STATUS_NOTIMPL;

}

SWITCH_DECLARE(switch_status_t) switch_core_codec_encode(switch_codec_t *codec,
													   switch_codec_t *other_codec,
													   void *decoded_data,
													   uint32_t decoded_data_len,
													   uint32_t decoded_rate,
													   void *encoded_data,
													   uint32_t *encoded_data_len, uint32_t *encoded_rate, unsigned int *flag)
{
	assert(codec != NULL);
	assert(encoded_data != NULL);
	assert(decoded_data != NULL);

	if (!codec->implementation) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!switch_test_flag(codec, SWITCH_CODEC_FLAG_ENCODE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec's encoder is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}


	return codec->implementation->encode(codec,
										 other_codec,
										 decoded_data,
										 decoded_data_len,
										 decoded_rate, encoded_data, encoded_data_len, encoded_rate, flag);

}

SWITCH_DECLARE(switch_status_t) switch_core_codec_decode(switch_codec_t *codec,
													   switch_codec_t *other_codec,
													   void *encoded_data,
													   uint32_t encoded_data_len,
													   uint32_t encoded_rate,
													   void *decoded_data,
													   uint32_t *decoded_data_len, 
													   uint32_t *decoded_rate, 
													   unsigned int *flag)
{

	assert(codec != NULL);
	assert(encoded_data != NULL);
	assert(decoded_data != NULL);



	if (!codec->implementation) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!switch_test_flag(codec, SWITCH_CODEC_FLAG_DECODE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec's decoder is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}


	return codec->implementation->decode(codec,
										 other_codec,
										 encoded_data,
										 encoded_data_len,
										 encoded_rate, decoded_data, decoded_data_len, decoded_rate, flag);

}

SWITCH_DECLARE(switch_status_t) switch_core_codec_destroy(switch_codec_t *codec)
{
	assert(codec != NULL);

	if (!codec->implementation) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	codec->implementation->destroy(codec);

	if (switch_test_flag(codec, SWITCH_CODEC_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&codec->memory_pool);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_open(switch_file_handle_t *fh, char *file_path, unsigned int flags,
													switch_memory_pool_t *pool)
{
	char *ext;
	switch_status_t status;

	if ((ext = strrchr(file_path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
		return SWITCH_STATUS_FALSE;
	}
	ext++;

	if ((fh->file_interface = switch_loadable_module_get_file_interface(ext)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid file format [%s]!\n", ext);
		return SWITCH_STATUS_GENERR;
	}

	fh->flags = flags;
	if (pool) {
		fh->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&fh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		switch_set_flag(fh, SWITCH_FILE_FLAG_FREE_POOL);
	}

	return fh->file_interface->file_open(fh, file_path);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_read(switch_file_handle_t *fh, void *data, switch_size_t *len)
{
	assert(fh != NULL);

	return fh->file_interface->file_read(fh, data, len);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_write(switch_file_handle_t *fh, void *data, switch_size_t *len)
{
	assert(fh != NULL);

	return fh->file_interface->file_write(fh, data, len);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_seek(switch_file_handle_t *fh, unsigned int *cur_pos, int64_t samples,
													int whence)
{
	assert(fh != NULL);
	return fh->file_interface->file_seek(fh, cur_pos, samples, whence);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_set_string(switch_file_handle_t *fh, switch_audio_col_t col, const char *string)
{
	assert(fh != NULL);

	return fh->file_interface->file_set_string(fh, col, string);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_get_string(switch_file_handle_t *fh, switch_audio_col_t col, const char **string)
{
	assert(fh != NULL);
	
	return fh->file_interface->file_get_string(fh, col, string);

}


SWITCH_DECLARE(switch_status_t) switch_core_file_close(switch_file_handle_t *fh)
{
	return fh->file_interface->file_close(fh);
}

SWITCH_DECLARE(switch_status_t) switch_core_directory_open(switch_directory_handle_t *dh, 
														 char *module_name, 
														 char *source,
														 char *dsn,
														 char *passwd,
														 switch_memory_pool_t *pool)
{
	switch_status_t status;

	if ((dh->directory_interface = switch_loadable_module_get_directory_interface(module_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid directory module [%s]!\n", module_name);
		return SWITCH_STATUS_GENERR;
	}

	if (pool) {
		dh->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&dh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		switch_set_flag(dh, SWITCH_DIRECTORY_FLAG_FREE_POOL);
	}

	return dh->directory_interface->directory_open(dh, source, dsn, passwd);
}

SWITCH_DECLARE(switch_status_t) switch_core_directory_query(switch_directory_handle_t *dh, char *base, char *query)
{
	return dh->directory_interface->directory_query(dh, base, query);
}

SWITCH_DECLARE(switch_status_t) switch_core_directory_next(switch_directory_handle_t *dh)
{
	return dh->directory_interface->directory_next(dh);
}

SWITCH_DECLARE(switch_status_t) switch_core_directory_next_pair(switch_directory_handle_t *dh, char **var, char **val)
{
	return dh->directory_interface->directory_next_pair(dh, var, val);
}

SWITCH_DECLARE(switch_status_t) switch_core_directory_close(switch_directory_handle_t *dh)
{
	return dh->directory_interface->directory_close(dh);
}

SWITCH_DECLARE(switch_status_t) switch_core_speech_open(switch_speech_handle_t *sh, 
														char *module_name, 
														char *voice_name,
														unsigned int rate,
														switch_speech_flag_t *flags,
														switch_memory_pool_t *pool)
{
	switch_status_t status;

	if ((sh->speech_interface = switch_loadable_module_get_speech_interface(module_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid speech module [%s]!\n", module_name);
		return SWITCH_STATUS_GENERR;
	}

	switch_copy_string(sh->engine, module_name, sizeof(sh->engine));
	sh->flags = *flags;
	if (pool) {
		sh->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&sh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		switch_set_flag(sh, SWITCH_SPEECH_FLAG_FREE_POOL);
	}
	sh->rate = rate;
	sh->name = switch_core_strdup(pool, module_name);
	return sh->speech_interface->speech_open(sh, voice_name, rate, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_open(switch_asr_handle_t *ah,
													 char *module_name,
													 char *codec,
													 int rate,
													 char *dest,
													 switch_asr_flag_t *flags,
													 switch_memory_pool_t *pool)
{
	switch_status_t status;

	assert(ah != NULL);

	if ((ah->asr_interface = switch_loadable_module_get_asr_interface(module_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid asr module [%s]!\n", module_name);
		return SWITCH_STATUS_GENERR;
	}
	
	ah->flags = *flags;

	if (pool) {
		ah->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&ah->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		switch_set_flag(ah, SWITCH_ASR_FLAG_FREE_POOL);
	}

	ah->rate = rate;
	ah->name = switch_core_strdup(ah->memory_pool, module_name);

	return ah->asr_interface->asr_open(ah, codec, rate, dest, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_load_grammar(switch_asr_handle_t *ah, char *grammar, char *path)
{
	char *epath = NULL;
	switch_status_t status;

	assert(ah != NULL);

	if (*path != '/') {
		epath = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.grammar_dir, SWITCH_PATH_SEPARATOR, path);
		path = epath;
	}

	status = ah->asr_interface->asr_load_grammar(ah, grammar, path);
	switch_safe_free(epath);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_unload_grammar(switch_asr_handle_t *ah, char *grammar)
{
	switch_status_t status;

	assert(ah != NULL);
	status = ah->asr_interface->asr_unload_grammar(ah, grammar);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_pause(switch_asr_handle_t *ah)
{
	assert(ah != NULL);

	return ah->asr_interface->asr_pause(ah);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_resume(switch_asr_handle_t *ah)
{
	assert(ah != NULL);

	return ah->asr_interface->asr_resume(ah);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	assert(ah != NULL);

	return ah->asr_interface->asr_close(ah, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags)
{
	assert(ah != NULL);
	
	return ah->asr_interface->asr_feed(ah, data, len, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	assert(ah != NULL);
	
	return ah->asr_interface->asr_check_results(ah, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags)
{
	assert(ah != NULL);
	
	return ah->asr_interface->asr_get_results(ah, xmlstr, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags)
{
	assert(sh != NULL);

	return sh->speech_interface->speech_feed_tts(sh, text, flags);
}

SWITCH_DECLARE(void) switch_core_speech_flush_tts(switch_speech_handle_t *sh)
{
	assert(sh != NULL);

	if (sh->speech_interface->speech_flush_tts) {
		sh->speech_interface->speech_flush_tts(sh);
	}
}

SWITCH_DECLARE(void) switch_core_speech_text_param_tts(switch_speech_handle_t *sh, char *param, char *val)
{
	assert(sh != NULL);

	if (sh->speech_interface->speech_text_param_tts) {
		sh->speech_interface->speech_text_param_tts(sh, param, val);
	}
}

SWITCH_DECLARE(void) switch_core_speech_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val)
{
	assert(sh != NULL);

	if (sh->speech_interface->speech_numeric_param_tts) {
		sh->speech_interface->speech_numeric_param_tts(sh, param, val);
	}
}

SWITCH_DECLARE(void) switch_core_speech_float_param_tts(switch_speech_handle_t *sh, char *param, double val)
{
	assert(sh != NULL);

	if (sh->speech_interface->speech_float_param_tts) {
		sh->speech_interface->speech_float_param_tts(sh, param, val);
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_speech_read_tts(switch_speech_handle_t *sh, 
														  void *data,
														  switch_size_t *datalen,
														  uint32_t *rate,
														  switch_speech_flag_t *flags)
{
	assert(sh != NULL);

	return sh->speech_interface->speech_read_tts(sh, data, datalen, rate, flags);
}


SWITCH_DECLARE(switch_status_t) switch_core_speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags)
{
	return sh->speech_interface->speech_close(sh, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_timer_init(switch_timer_t *timer, char *timer_name, int interval, int samples,
													 switch_memory_pool_t *pool)
{
	switch_timer_interface_t *timer_interface;
	switch_status_t status;
	memset(timer, 0, sizeof(*timer));
	if ((timer_interface = switch_loadable_module_get_timer_interface(timer_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid timer %s!\n", timer_name);
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

SWITCH_DECLARE(int) switch_core_timer_next(switch_timer_t *timer)
{
	if (!timer->timer_interface) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (timer->timer_interface->timer_next(timer) == SWITCH_STATUS_SUCCESS) {
		return timer->samplecount;
	} else {
		return -1;
	}

}

SWITCH_DECLARE(switch_status_t) switch_core_timer_step(switch_timer_t *timer)
{
	if (!timer->timer_interface) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	return timer->timer_interface->timer_step(timer);
}

SWITCH_DECLARE(switch_status_t) switch_core_timer_check(switch_timer_t *timer, uint32_t *diff)
{
	if (!timer->timer_interface) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	return timer->timer_interface->timer_check(timer, diff);
}


SWITCH_DECLARE(switch_status_t) switch_core_timer_destroy(switch_timer_t *timer)
{
	if (!timer->timer_interface) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer is not initilized!\n");
		return SWITCH_STATUS_GENERR;
	}

	timer->timer_interface->timer_destroy(timer);

	if (switch_test_flag(timer, SWITCH_TIMER_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&timer->memory_pool);
	}

	return SWITCH_STATUS_SUCCESS;
}

static void *switch_core_service_thread(switch_thread_t *thread, void *obj)
{
	switch_core_thread_session_t *data = obj;
	switch_core_session_t *session = data->objs[0];
	int *stream_id_p = data->objs[1];
	switch_channel_t *channel;
	switch_frame_t *read_frame;
	int stream_id = *stream_id_p;

	assert(thread != NULL);
	assert(session != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_channel_set_flag(channel, CF_SERVICE);
	while (data->running > 0) {
		switch (switch_core_session_read_frame(session, &read_frame, -1, stream_id)) {
		case SWITCH_STATUS_SUCCESS:
		case SWITCH_STATUS_TIMEOUT:
		case SWITCH_STATUS_BREAK:
			break;
		default:
			data->running = -1;
			continue;
		}
	}

	switch_channel_clear_flag(channel, CF_SERVICE);
	data->running = 0;
	return NULL;
}

/* Either add a timeout here or make damn sure the thread cannot get hung somehow (my preference) */
SWITCH_DECLARE(void) switch_core_thread_session_end(switch_core_thread_session_t *thread_session)
{
	if (thread_session->running > 0) {
		thread_session->running = -1;

		while (thread_session->running) {
			switch_yield(1000);
		}
	}
}

SWITCH_DECLARE(void) switch_core_service_session(switch_core_session_t *session,
												 switch_core_thread_session_t *thread_session, int stream_id)
{
	thread_session->running = 1;
	thread_session->objs[0] = session;
	thread_session->objs[1] = &stream_id;
	switch_core_session_launch_thread(session, switch_core_service_thread, thread_session);
}

SWITCH_DECLARE(switch_memory_pool_t *) switch_core_session_get_pool(switch_core_session_t *session)
{
	return session->pool;
}

/* **ONLY** alloc things with this function that **WILL NOT** outlive
   the session itself or expect an earth shattering KABOOM!*/
SWITCH_DECLARE(void *) switch_core_session_alloc(switch_core_session_t *session, switch_size_t memory)
{
	void *ptr = NULL;
	assert(session != NULL);
	assert(session->pool != NULL);

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocate %d\n", memory);
#endif


	if ((ptr = apr_palloc(session->pool, memory)) != 0) {
		memset(ptr, 0, memory);
	}
	return ptr;
}

/* **ONLY** alloc things with these functions that **WILL NOT** need
   to be freed *EVER* ie this is for *PERMANENT* memory allocation */

SWITCH_DECLARE(void *) switch_core_permanent_alloc(switch_size_t memory)
{
	void *ptr = NULL;
	assert(runtime.memory_pool != NULL);

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Perm Allocate %d\n", memory);
#endif

	if ((ptr = apr_palloc(runtime.memory_pool, memory)) != 0) {
		memset(ptr, 0, memory);
	}
	return ptr;
}

SWITCH_DECLARE(char *) switch_core_permanent_strdup(char *todup)
{
	char *duped = NULL;
	switch_size_t len;

	assert(runtime.memory_pool != NULL);

	if (!todup)
		return NULL;

	len = strlen(todup) + 1;

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Perm Allocate %d\n", len);
#endif

	if (todup && (duped = apr_palloc(runtime.memory_pool, len)) != 0) {
		strncpy(duped, todup, len);
	}
	return duped;
}


SWITCH_DECLARE(char *) switch_core_session_strdup(switch_core_session_t *session, char *todup)
{
	char *duped = NULL;
	switch_size_t len;
	assert(session != NULL);
	assert(session->pool != NULL);

	if (!todup) {
		return NULL;
	}
	len = strlen(todup) + 1;

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocate %d\n", len);
#endif

	if (todup && (duped = apr_palloc(session->pool, len)) != 0) {
		strncpy(duped, todup, len);
	}
	return duped;
}


SWITCH_DECLARE(char *) switch_core_strdup(switch_memory_pool_t *pool, char *todup)
{
	char *duped = NULL;
	switch_size_t len;
	assert(pool != NULL);

	if (!todup) {
		return NULL;
	}
	
	len = strlen(todup) + 1;

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocate %d\n", len);
#endif

	if (todup && (duped = apr_palloc(pool, len)) != 0) {
		strncpy(duped, todup, len);
	}
	return duped;
}

SWITCH_DECLARE(void *) switch_core_session_get_private(switch_core_session_t *session)
{
	assert(session != NULL);
	return session->private_info;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_set_private(switch_core_session_t *session, void *private_info)
{
	assert(session != NULL);
	session->private_info = private_info;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) switch_core_session_add_stream(switch_core_session_t *session, void *private_info)
{
	session->streams[session->stream_count++] = private_info;
	return session->stream_count - 1;
}

SWITCH_DECLARE(void *) switch_core_session_get_stream(switch_core_session_t *session, int index)
{
	return session->streams[index];
}


SWITCH_DECLARE(int) switch_core_session_get_stream_count(switch_core_session_t *session)
{
	return session->stream_count;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_outgoing_channel(switch_core_session_t *session,
																   char *endpoint_name,
																   switch_caller_profile_t *caller_profile,
																   switch_core_session_t **new_session,
																   switch_memory_pool_t *pool)
{
	switch_io_event_hook_outgoing_channel_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	const switch_endpoint_interface_t *endpoint_interface;
	switch_channel_t *channel = NULL;
	switch_caller_profile_t *outgoing_profile = caller_profile;

	if ((endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate channel type %s\n", endpoint_name);
		return SWITCH_STATUS_FALSE;
	}

	if (endpoint_interface->io_routines->outgoing_channel) {
		if (session) {
			channel = switch_core_session_get_channel(session);
			if (caller_profile) {			
				char *ecaller_id_name = NULL, *ecaller_id_number = NULL;

				ecaller_id_name = switch_channel_get_variable(channel, "effective_caller_id_name");
				ecaller_id_number = switch_channel_get_variable(channel, "effective_caller_id_number");

				if (ecaller_id_name || ecaller_id_number) {
                    if (!ecaller_id_name) {
                        ecaller_id_name = caller_profile->caller_id_name;
                    }
                    if (!ecaller_id_number) {
                        ecaller_id_number = caller_profile->caller_id_number;
                    }
					outgoing_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																 caller_profile->username,
																 caller_profile->dialplan,
																 ecaller_id_name,
																 ecaller_id_number,
																 caller_profile->network_addr,
																 caller_profile->ani,
																 caller_profile->aniii, 
																 caller_profile->rdnis,
																 caller_profile->source,
																 caller_profile->context,
																 caller_profile->destination_number);
					outgoing_profile->flags = caller_profile->flags;
				} 
			}
			if (!outgoing_profile) {
				outgoing_profile = switch_channel_get_caller_profile(channel);
			}
		}
		
		if ((status = endpoint_interface->io_routines->outgoing_channel(session,
																		outgoing_profile,
																		new_session,
																		pool)) == SWITCH_STATUS_SUCCESS) {
			if (session) {
				for (ptr = session->event_hooks.outgoing_channel; ptr; ptr = ptr->next) {
					if ((status = ptr->outgoing_channel(session, caller_profile, *new_session)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				}
			}
		} else {
			return status;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate outgoing channel interface for %s\n", endpoint_name);
		return SWITCH_STATUS_FALSE;
	}

	if (*new_session) {
		switch_caller_profile_t *profile = NULL, *peer_profile = NULL, *cloned_profile = NULL;
		switch_event_t *event;
		switch_channel_t *peer_channel = switch_core_session_get_channel(*new_session);

		
		if (session && channel) {
			profile = switch_channel_get_caller_profile(channel);
		}
		if (peer_channel) {
			peer_profile = switch_channel_get_caller_profile(peer_channel);
		}

		if (channel && peer_channel) {
			char *export_vars, *val;
            switch_codec_t *read_codec = switch_core_session_get_read_codec(session);

            if (read_codec) {
                char tmp[80];
                switch_codec2str(read_codec, tmp, sizeof(tmp));
                switch_channel_set_variable(peer_channel, SWITCH_ORIGINATOR_CODEC_VARIABLE, tmp);
            }

			switch_channel_set_variable(peer_channel, SWITCH_ORIGINATOR_VARIABLE, switch_core_session_get_uuid(session));            

            /* A comma (,) separated list of variable names that should ne propagated from originator to originatee */
			if ((export_vars = switch_channel_get_variable(channel, "export_vars"))) {
				char *cptmp = switch_core_session_strdup(session, export_vars);
				int argc;
				char *argv[256];

				if ((argc = switch_separate_string(cptmp, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
					int x;

					for (x = 0; x < argc; x++) {
						char *val;
						if ((val = switch_channel_get_variable(channel, argv[x]))) {
							switch_channel_set_variable(peer_channel, argv[x], val);
						}
					}
				}
			}

			if ((val = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE))) {
				switch_channel_set_variable(peer_channel, SWITCH_B_SDP_VARIABLE, val);
			}

			if ((val = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE))) {
				switch_channel_set_variable(peer_channel, SWITCH_MAX_FORWARDS_VARIABLE, val);
			}
			
			if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
				switch_channel_set_flag(peer_channel, CF_NOMEDIA);
			}

			if (profile) {
				if ((cloned_profile = switch_caller_profile_clone(*new_session, profile)) != 0) {
					switch_channel_set_originator_caller_profile(peer_channel, cloned_profile);
				}
			}

			if (peer_profile) {
				if (session && (cloned_profile = switch_caller_profile_clone(session, peer_profile)) != 0) {
					switch_channel_set_originatee_caller_profile(channel, cloned_profile);
				}
			}
		}

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_OUTGOING) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(peer_channel, event);
			switch_event_fire(&event);
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_answer_channel(switch_core_session_t *session)
{
	switch_io_event_hook_answer_channel_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	assert(session != NULL);
	if (session->endpoint_interface->io_routines->answer_channel) {
		if ((status = session->endpoint_interface->io_routines->answer_channel(session)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.answer_channel; ptr; ptr = ptr->next) {
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

SWITCH_DECLARE(switch_status_t) switch_core_session_receive_message(switch_core_session_t *session, switch_core_session_message_t *message)
{
	switch_io_event_hook_receive_message_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	assert(session != NULL);
	if (session->endpoint_interface->io_routines->receive_message) {
		if ((status =
			 session->endpoint_interface->io_routines->receive_message(session, message)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.receive_message; ptr; ptr = ptr->next) {
				if ((status = ptr->receive_message(session, message)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_message(switch_core_session_t *session, switch_core_session_message_t *message)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	assert(session != NULL);

	if (!session->message_queue) {
		switch_queue_create(&session->message_queue, SWITCH_MESSAGE_QUEUE_LEN, session->pool);
	}

	if (session->message_queue) {
		if (switch_queue_trypush(session->message_queue, message) == SWITCH_STATUS_SUCCESS) {
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_message(switch_core_session_t *session, switch_core_session_message_t **message)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;

	assert(session != NULL);
	
	if (session->message_queue) {
		if ((status = (switch_status_t) switch_queue_trypop(session->message_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*message = (switch_core_session_message_t *) pop;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_receive_event(switch_core_session_t *session, switch_event_t **event)
	 
{
	switch_io_event_hook_receive_event_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	assert(session != NULL);

	/* Acquire a read lock on the session or forget it the channel is dead */
	if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
		if (switch_channel_get_state(session->channel) < CS_HANGUP) {
			if (session->endpoint_interface->io_routines->receive_event) {
				status = session->endpoint_interface->io_routines->receive_event(session, *event);
			} 
	
			if (status == SWITCH_STATUS_SUCCESS) {
				for (ptr = session->event_hooks.receive_event; ptr; ptr = ptr->next) {
					if ((status = ptr->receive_event(session, *event)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				}
			}

			if (status == SWITCH_STATUS_BREAK) {
				status = SWITCH_STATUS_SUCCESS;
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				switch_event_destroy(event);
			}
		}
        switch_core_session_rwunlock(session);
	}
	
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	assert(session != NULL);

	if (!session->event_queue) {
		switch_queue_create(&session->event_queue, SWITCH_EVENT_QUEUE_LEN, session->pool);
	}

	if (session->event_queue) {
		if (switch_queue_trypush(session->event_queue, *event) == SWITCH_STATUS_SUCCESS) {
			*event = NULL;
			status = SWITCH_STATUS_SUCCESS;
		}
	} 

	return status;
}

SWITCH_DECLARE(int32_t) switch_core_session_event_count(switch_core_session_t *session)
{
	if (session->event_queue) {
		return (int32_t) switch_queue_size(session->event_queue);
	}

	return -1;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_event(switch_core_session_t *session, switch_event_t **event)
	 
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;

	assert(session != NULL);
	
	if (session->event_queue) {
		if ((status = (switch_status_t) switch_queue_trypop(session->event_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*event = (switch_event_t *) pop;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_private_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	assert(session != NULL);
	
	if (!session->private_event_queue) {
		switch_queue_create(&session->private_event_queue, SWITCH_EVENT_QUEUE_LEN, session->pool);
	}

	if (session->private_event_queue) {
		if (switch_queue_trypush(session->private_event_queue, *event) == SWITCH_STATUS_SUCCESS) {
			*event = NULL;
			status = SWITCH_STATUS_SUCCESS;
		}
	} 

	return status;
}

SWITCH_DECLARE(int32_t) switch_core_session_private_event_count(switch_core_session_t *session)
{
	if (session->private_event_queue) {
		return (int32_t) switch_queue_size(session->private_event_queue);
	}

	return -1;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_private_event(switch_core_session_t *session, switch_event_t **event)
	 
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;
	switch_channel_t *channel;

	assert(session != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	if (switch_channel_test_flag(channel, CF_EVENT_PARSE)) {
		return status;
	}

	
	if (session->private_event_queue) {
		if ((status = (switch_status_t) switch_queue_trypop(session->private_event_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*event = (switch_event_t *) pop;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_read_frame(switch_core_session_t *session, switch_frame_t **frame,
															 int timeout, int stream_id)
{
	switch_io_event_hook_read_frame_t *ptr;
	switch_status_t status;
	int need_codec, perfect, do_bugs = 0;
	unsigned int flag = 0;
 top:
	
	status = SWITCH_STATUS_FALSE;
	need_codec = perfect = 0;
	
	assert(session != NULL);
	*frame = NULL;

	if (switch_channel_test_flag(session->channel, CF_HOLD)) {
		status = SWITCH_STATUS_BREAK;
		goto done;
	}

	if (session->endpoint_interface->io_routines->read_frame) {
		if ((status = session->endpoint_interface->io_routines->read_frame(session,
																		   frame,
																		   timeout,
																		   SWITCH_IO_FLAG_NOOP,
																		   stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.read_frame; ptr; ptr = ptr->next) {
				if ((status =
					 ptr->read_frame(session, frame, timeout, SWITCH_IO_FLAG_NOOP,
									 stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		goto done;
	}

	if (!(*frame)) {
		goto done;
	}

	assert(session != NULL);
	assert(*frame != NULL);
	
	if (switch_test_flag(*frame, SFF_CNG)) {
		status = SWITCH_STATUS_SUCCESS;
		goto done;
	}

	if ((session->read_codec && (*frame)->codec && session->read_codec->implementation != (*frame)->codec->implementation)) {
		need_codec = TRUE;
	}

	if (session->read_codec && !(*frame)->codec) {
		need_codec = TRUE;
	}

	if (!session->read_codec && (*frame)->codec) {
		need_codec = TRUE;
	}

	if (session->bugs && !need_codec) {
		do_bugs = 1;
		need_codec = 1;
	}

	if (status == SWITCH_STATUS_SUCCESS && need_codec) {
		switch_frame_t *enc_frame, *read_frame = *frame;

		if (read_frame->codec) {
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
										   read_frame->codec->implementation->bytes_per_frame * 20,
										   session->read_codec->implementation->samples_per_second,
										   session->read_codec->implementation->bytes_per_frame * 20, session->pool);
				}
			case SWITCH_STATUS_SUCCESS:
				read_frame = &session->raw_read_frame;
				break;
			case SWITCH_STATUS_NOOP:
				status = SWITCH_STATUS_SUCCESS;
				break;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s decoder error!\n",
								  session->read_codec->codec_interface->interface_name);
				goto done;
			}
		}
		if (session->read_resampler) {
			short *data = read_frame->data;

			session->read_resampler->from_len =
				switch_short_to_float(data, session->read_resampler->from, (int) read_frame->datalen / 2);
			session->read_resampler->to_len =
				switch_resample_process(session->read_resampler, session->read_resampler->from,
										session->read_resampler->from_len, session->read_resampler->to,
										session->read_resampler->to_size, 0);
			switch_float_to_short(session->read_resampler->to, data, read_frame->datalen);
			read_frame->samples = session->read_resampler->to_len;
			read_frame->datalen = session->read_resampler->to_len * 2;
			read_frame->rate = session->read_resampler->to_rate;
		}

		if (session->bugs) {
			switch_media_bug_t *bp;
			switch_thread_rwlock_rdlock(session->bug_rwlock);
			for (bp = session->bugs; bp; bp = bp->next) {
				if (bp->ready && switch_test_flag(bp, SMBF_READ_STREAM)) {
					switch_mutex_lock(bp->read_mutex);
					switch_buffer_write(bp->raw_read_buffer, read_frame->data, read_frame->datalen);
					if (bp->callback) {
						bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_READ);
					}
					switch_mutex_unlock(bp->read_mutex);
				}
			}
			switch_thread_rwlock_unlock(session->bug_rwlock);
		}

		if (do_bugs) {
			goto done;
		}

		if (session->read_codec) {
			if ((*frame)->datalen == session->read_codec->implementation->bytes_per_frame) {
				perfect = TRUE;
			} else {
				if (!session->raw_read_buffer) {
					switch_size_t bytes = session->read_codec->implementation->bytes_per_frame;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Engaging Read Buffer at %u bytes\n", bytes);
					switch_buffer_create_dynamic(&session->raw_read_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, 0);
				}
				if (!switch_buffer_write(session->raw_read_buffer, read_frame->data, read_frame->datalen)) {
					status = SWITCH_STATUS_MEMERR;
					goto done;
				}
			}

			if (perfect || switch_buffer_inuse(session->raw_read_buffer) >= session->read_codec->implementation->bytes_per_frame) {
				if (perfect) {
					enc_frame = *frame;
					session->raw_read_frame.rate = (*frame)->rate;
				} else {
					session->raw_read_frame.datalen = (uint32_t)switch_buffer_read(session->raw_read_buffer,
																				   session->raw_read_frame.data,
																				   session->read_codec->implementation->bytes_per_frame);
					
					session->raw_read_frame.rate = session->read_codec->implementation->samples_per_second;
					enc_frame = &session->raw_read_frame;
				}
				session->enc_read_frame.datalen = session->enc_read_frame.buflen;
				assert(session->read_codec != NULL);				
				assert(enc_frame != NULL);
				assert(enc_frame->data != NULL);
				
				status = switch_core_codec_encode(session->read_codec,
												  enc_frame->codec,
												  enc_frame->data,
												  enc_frame->datalen,
												  session->read_codec->implementation->samples_per_second,
												  session->enc_read_frame.data,
												  &session->enc_read_frame.datalen,
												  &session->enc_read_frame.rate, 
												  &flag);


				switch (status) {
				case SWITCH_STATUS_RESAMPLE:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "fixme 1\n");
				case SWITCH_STATUS_SUCCESS:
					*frame = &session->enc_read_frame;
					break;
				case SWITCH_STATUS_NOOP:
					*frame = &session->raw_read_frame;
					status = SWITCH_STATUS_SUCCESS;
					break;
				default:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s encoder error!\n",
										  session->read_codec->codec_interface->interface_name);
					*frame = NULL;
					status = SWITCH_STATUS_GENERR;
					break;
				}
			} else {
				goto top;
			}
		}
	}

 done:
	if (!(*frame)) {
		status = SWITCH_STATUS_FALSE;
	} else {
		if (flag & SFF_CNG) {
			switch_set_flag((*frame), SFF_CNG);
		}
	}

	return status;
}

static switch_status_t perform_write(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id)
{
	switch_io_event_hook_write_frame_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->write_frame) {

		if ((status =
			 session->endpoint_interface->io_routines->write_frame(session, frame, timeout, flags,
																   stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.write_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->write_frame(session, frame, timeout, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}
	return status;
}

SWITCH_DECLARE(void) switch_core_session_reset(switch_core_session_t *session)
{
	/* sweep theese under the rug, they wont be leaked they will be reclaimed
	   when the session ends.
	*/

	session->read_resampler = NULL;
	session->write_resampler = NULL;

	/* wipe theese, they will be recreated if need be */
	switch_buffer_destroy(&session->raw_read_buffer);
	switch_buffer_destroy(&session->raw_write_buffer);
}

SWITCH_DECLARE(switch_status_t) switch_core_session_write_frame(switch_core_session_t *session, switch_frame_t *frame,
															  int timeout, int stream_id)
{

	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_frame_t *enc_frame = NULL, *write_frame = frame;
	unsigned int flag = 0, need_codec = 0, perfect = 0, do_bugs = 0, do_write = 0;
	switch_io_flag_t io_flag = SWITCH_IO_FLAG_NOOP;

	assert(session != NULL);
	assert(frame != NULL);


	if (switch_channel_test_flag(session->channel, CF_HOLD)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_test_flag(frame, SFF_CNG)) {

		if (switch_channel_test_flag(session->channel, CF_ACCEPT_CNG)) { 
			return perform_write(session, frame, timeout, flag, stream_id);
		}

		return SWITCH_STATUS_SUCCESS;
	}

	assert(frame->codec != NULL);

	if ((session->write_codec && frame->codec && session->write_codec->implementation != frame->codec->implementation)) {
		need_codec = TRUE;
	}

	if (session->write_codec && !frame->codec) {
		need_codec = TRUE;
	}

	if (!session->write_codec && frame->codec) {
		need_codec = TRUE;
	}

	if (session->bugs && !need_codec) {
		do_bugs = 1;
		need_codec = 1;
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
											  &session->raw_write_frame.datalen, &session->raw_write_frame.rate, &flag);
			
			
			switch (status) {
			case SWITCH_STATUS_RESAMPLE:
				write_frame = &session->raw_write_frame;
				if (!session->write_resampler) {
					status = switch_resample_create(&session->write_resampler,
													frame->codec->implementation->samples_per_second,
													frame->codec->implementation->bytes_per_frame * 20,
													session->write_codec->implementation->samples_per_second,
													session->write_codec->implementation->bytes_per_frame * 20,
													session->pool);
				}
				break;
			case SWITCH_STATUS_SUCCESS:
				write_frame = &session->raw_write_frame;
				break;
			case SWITCH_STATUS_BREAK:
				return SWITCH_STATUS_SUCCESS;
			case SWITCH_STATUS_NOOP:
				write_frame = frame;
				status = SWITCH_STATUS_SUCCESS;
				break;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s decoder error!\n",
									  frame->codec->codec_interface->interface_name);
				return status;
			}
		}
		if (session->write_resampler) {
			short *data = write_frame->data;

			session->write_resampler->from_len = write_frame->datalen / 2;
			switch_short_to_float(data, session->write_resampler->from, session->write_resampler->from_len);



			session->write_resampler->to_len = (uint32_t)
				switch_resample_process(session->write_resampler, session->write_resampler->from,
										session->write_resampler->from_len, session->write_resampler->to,
										session->write_resampler->to_size, 0);
			

			switch_float_to_short(session->write_resampler->to, data, session->write_resampler->to_len);

			write_frame->samples = session->write_resampler->to_len;
			write_frame->datalen = write_frame->samples * 2;
			write_frame->rate = session->write_resampler->to_rate;
		}

		if (session->bugs) {
			switch_media_bug_t *bp;
			switch_thread_rwlock_rdlock(session->bug_rwlock);
			for (bp = session->bugs; bp; bp = bp->next) {
                if (!bp->ready) {
                    continue;
                }
				if (switch_test_flag(bp, SMBF_WRITE_STREAM)) {
					switch_mutex_lock(bp->write_mutex);
					switch_buffer_write(bp->raw_write_buffer, write_frame->data, write_frame->datalen);
					switch_mutex_unlock(bp->write_mutex);
					if (bp->callback) {
						bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_WRITE);
					}
				} else if (switch_test_flag(bp, SMBF_WRITE_REPLACE)) {
                    do_bugs = 0;
					if (bp->callback) {
                        bp->replace_frame_in = frame;
                        bp->replace_frame_out = NULL;
						bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_WRITE_REPLACE);
                        write_frame = bp->replace_frame_out;
					}
				}
			}
			switch_thread_rwlock_unlock(session->bug_rwlock);
		}

		if (do_bugs) {
            do_write = 1;
            write_frame = frame;
            goto done;
		}

		if (session->write_codec) {
			if (write_frame->datalen == session->write_codec->implementation->bytes_per_frame) {
				perfect = TRUE;
			} else {
				if (!session->raw_write_buffer) {
					switch_size_t bytes = session->write_codec->implementation->bytes_per_frame;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
										  "Engaging Write Buffer at %u bytes to accomodate %u->%u\n",
										  bytes,
										  write_frame->datalen, session->write_codec->implementation->bytes_per_frame);
					if ((status =switch_buffer_create_dynamic(&session->raw_write_buffer,
															  bytes * SWITCH_BUFFER_BLOCK_FRAMES,
															  bytes * SWITCH_BUFFER_START_FRAMES,
															  0)) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Write Buffer Failed!\n");
						return status;
					}
				}

				if (!(switch_buffer_write(session->raw_write_buffer, write_frame->data, write_frame->datalen))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Write Buffer %u bytes Failed!\n", write_frame->datalen);
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
												  &session->enc_write_frame.rate, &flag);
				
				switch (status) {
				case SWITCH_STATUS_RESAMPLE:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "fixme 2\n");
				case SWITCH_STATUS_SUCCESS:
					write_frame = &session->enc_write_frame;
					break;
				case SWITCH_STATUS_NOOP:
					write_frame = enc_frame;
					status = SWITCH_STATUS_SUCCESS;
					break;
				default:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s encoder error!\n",
										  session->read_codec->codec_interface->interface_name);
					write_frame = NULL;
					return status;
				}
				if (flag & SFF_CNG) {
					switch_set_flag(write_frame, SFF_CNG);
				}
				status = perform_write(session, write_frame, timeout, io_flag, stream_id);
				return status;
			} else {
				switch_size_t used = switch_buffer_inuse(session->raw_write_buffer);
				uint32_t bytes = session->write_codec->implementation->bytes_per_frame;
				switch_size_t frames = (used / bytes);
				
				status = SWITCH_STATUS_SUCCESS;
				if (!frames) {
					return status;
				} else {
					switch_size_t x;
					for (x = 0; x < frames; x++) {
						if ((session->raw_write_frame.datalen = (uint32_t)
							 switch_buffer_read(session->raw_write_buffer, session->raw_write_frame.data, bytes)) != 0) {
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
															  &session->enc_write_frame.rate, &flag);

							
							switch (status) {
							case SWITCH_STATUS_RESAMPLE:
								write_frame = &session->enc_write_frame;
								if (!session->read_resampler) {
									status = switch_resample_create(&session->read_resampler,
																	frame->codec->implementation->samples_per_second,
																	frame->codec->implementation->bytes_per_frame * 20,
																	session->write_codec->implementation->
																	samples_per_second,
																	session->write_codec->implementation->
																	bytes_per_frame * 20, session->pool);
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
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s encoder error %d!\n",
													  session->read_codec->codec_interface->interface_name, status);
								write_frame = NULL;
								return status;
							}

							if (session->read_resampler) {
								short *data = write_frame->data;

								session->read_resampler->from_len = switch_short_to_float(data,
																						  session->read_resampler->from,
																						  (int) write_frame->datalen /
																						  2);
								session->read_resampler->to_len = (uint32_t)
									switch_resample_process(session->read_resampler, session->read_resampler->from,
															session->read_resampler->from_len,
															session->read_resampler->to,
															session->read_resampler->to_size, 0);
								switch_float_to_short(session->read_resampler->to, data, write_frame->datalen * 2);
								write_frame->samples = session->read_resampler->to_len;
								write_frame->datalen = session->read_resampler->to_len * 2;
								write_frame->rate = session->read_resampler->to_rate;
							}
							if (flag & SFF_CNG) {
								switch_set_flag(write_frame, SFF_CNG);
							}
							if ((status = perform_write(session, write_frame, timeout, io_flag, stream_id)) != SWITCH_STATUS_SUCCESS) {
								break;
							}
						}
					}
					return status;
				}
			}
		}
	} else {
		do_write = 1;
	}

 done:
	if (do_write) {
		return perform_write(session, frame, timeout, io_flag, stream_id);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_perform_kill_channel(switch_core_session_t *session, 
                                                                         const char *file, 
                                                                         const char *func, 
                                                                         int line, 
                                                                         switch_signal_t sig)
{
	switch_io_event_hook_kill_channel_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, (char *) file, func, line, SWITCH_LOG_INFO, "Kill %s [%d]\n", switch_channel_get_name(session->channel), sig);

	if (session->endpoint_interface->io_routines->kill_channel) {
		if ((status = session->endpoint_interface->io_routines->kill_channel(session, sig)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.kill_channel; ptr; ptr = ptr->next) {
				if ((status = ptr->kill_channel(session, sig)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_waitfor_read(switch_core_session_t *session, int timeout, int stream_id)
{
	switch_io_event_hook_waitfor_read_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->waitfor_read) {
		if ((status =
			 session->endpoint_interface->io_routines->waitfor_read(session, timeout,
																	stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.waitfor_read; ptr; ptr = ptr->next) {
				if ((status = ptr->waitfor_read(session, timeout, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_waitfor_write(switch_core_session_t *session, int timeout,
																int stream_id)
{
	switch_io_event_hook_waitfor_write_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->waitfor_write) {
		if ((status =
			 session->endpoint_interface->io_routines->waitfor_write(session, timeout,
																	 stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.waitfor_write; ptr; ptr = ptr->next) {
				if ((status = ptr->waitfor_write(session, timeout, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_send_dtmf(switch_core_session_t *session, char *dtmf)
{
	switch_io_event_hook_send_dtmf_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session->endpoint_interface->io_routines->send_dtmf) {
        if (strchr(dtmf, 'w') || strchr(dtmf, 'W')) {
            char *d;
            for (d = dtmf; d && *d; d++) {
                char digit[2] = "";
                
                if (*d == 'w') {
                    switch_yield(500000);
                    continue;
                } else if (*d == 'W') {
                    switch_yield(1000000);
                    continue;
                }

                digit[0] = *d;
                if ((status = session->endpoint_interface->io_routines->send_dtmf(session, digit)) != SWITCH_STATUS_SUCCESS) {
                    return status;
                }
            }
        } else {
            status = session->endpoint_interface->io_routines->send_dtmf(session, dtmf);
        }

        if (status == SWITCH_STATUS_SUCCESS) {
            for (ptr = session->event_hooks.send_dtmf; ptr; ptr = ptr->next) {
                if ((status = ptr->send_dtmf(session, dtmf)) != SWITCH_STATUS_SUCCESS) {
                    break;
                }
            }
        }
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_outgoing(switch_core_session_t *session,
																		  switch_outgoing_channel_hook_t outgoing_channel)
{
	switch_io_event_hook_outgoing_channel_t *hook, *ptr;

	assert(outgoing_channel != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->outgoing_channel = outgoing_channel;
		if (!session->event_hooks.outgoing_channel) {
			session->event_hooks.outgoing_channel = hook;
		} else {
			for (ptr = session->event_hooks.outgoing_channel; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_answer_channel(switch_core_session_t *session,
																				switch_answer_channel_hook_t
																				answer_channel)
{
	switch_io_event_hook_answer_channel_t *hook, *ptr;

	assert(answer_channel != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->answer_channel = answer_channel;
		if (!session->event_hooks.answer_channel) {
			session->event_hooks.answer_channel = hook;
		} else {
			for (ptr = session->event_hooks.answer_channel; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_read_frame(switch_core_session_t *session,
																			switch_read_frame_hook_t read_frame)
{
	switch_io_event_hook_read_frame_t *hook, *ptr;

	assert(read_frame != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->read_frame = read_frame;
		if (!session->event_hooks.read_frame) {
			session->event_hooks.read_frame = hook;
		} else {
			for (ptr = session->event_hooks.read_frame; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_write_frame(switch_core_session_t *session,
																			 switch_write_frame_hook_t write_frame)
{
	switch_io_event_hook_write_frame_t *hook, *ptr;

	assert(write_frame != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->write_frame = write_frame;
		if (!session->event_hooks.write_frame) {
			session->event_hooks.write_frame = hook;
		} else {
			for (ptr = session->event_hooks.write_frame; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_kill_channel(switch_core_session_t *session,
																			  switch_kill_channel_hook_t kill_channel)
{
	switch_io_event_hook_kill_channel_t *hook, *ptr;

	assert(kill_channel != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->kill_channel = kill_channel;
		if (!session->event_hooks.kill_channel) {
			session->event_hooks.kill_channel = hook;
		} else {
			for (ptr = session->event_hooks.kill_channel; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_waitfor_read(switch_core_session_t *session,
																			  switch_waitfor_read_hook_t waitfor_read)
{
	switch_io_event_hook_waitfor_read_t *hook, *ptr;

	assert(waitfor_read != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->waitfor_read = waitfor_read;
		if (!session->event_hooks.waitfor_read) {
			session->event_hooks.waitfor_read = hook;
		} else {
			for (ptr = session->event_hooks.waitfor_read; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_waitfor_write(switch_core_session_t *session,
																			   switch_waitfor_write_hook_t waitfor_write)
{
	switch_io_event_hook_waitfor_write_t *hook, *ptr;

	assert(waitfor_write != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->waitfor_write = waitfor_write;
		if (!session->event_hooks.waitfor_write) {
			session->event_hooks.waitfor_write = hook;
		} else {
			for (ptr = session->event_hooks.waitfor_write; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}


SWITCH_DECLARE(switch_status_t) switch_core_session_add_event_hook_send_dtmf(switch_core_session_t *session,
																		   switch_send_dtmf_hook_t send_dtmf)
{
	switch_io_event_hook_send_dtmf_t *hook, *ptr;

	assert(send_dtmf != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->send_dtmf = send_dtmf;
		if (!session->event_hooks.send_dtmf) {
			session->event_hooks.send_dtmf = hook;
		} else {
			for (ptr = session->event_hooks.send_dtmf; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}


SWITCH_DECLARE(switch_status_t) switch_core_new_memory_pool(switch_memory_pool_t **pool)
{

	assert(runtime.memory_pool != NULL);

	if ((apr_pool_create(pool, NULL)) != SWITCH_STATUS_SUCCESS) {
		*pool = NULL;
		return SWITCH_STATUS_MEMERR;
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_destroy_memory_pool(switch_memory_pool_t **pool)
{
	apr_pool_destroy(*pool);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_channel_t *) switch_core_session_get_channel(switch_core_session_t *session)
{
	return session->channel;
}

static void switch_core_standard_on_init(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard INIT %s\n", switch_channel_get_name(session->channel));
}

static void switch_core_standard_on_hangup(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard HANGUP %s, cause: %s\n", switch_channel_get_name(session->channel),switch_channel_cause2str(switch_channel_get_cause(session->channel)));

}

static void switch_core_standard_on_ring(switch_core_session_t *session)
{
	switch_dialplan_interface_t *dialplan_interface = NULL;
	switch_caller_profile_t *caller_profile;
	switch_caller_extension_t *extension = NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard RING %s\n", switch_channel_get_name(session->channel));

	if ((caller_profile = switch_channel_get_caller_profile(session->channel)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't get profile!\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
        return;
	} else {
		char *dp[25];
        char *dpstr;
		int argc, x, count = 0;

		if (!switch_strlen_zero(caller_profile->dialplan)) {
            if ((dpstr = switch_core_session_strdup(session, caller_profile->dialplan))) {
                argc = switch_separate_string(dpstr, ',', dp, (sizeof(dp) / sizeof(dp[0]))); 
                for (x = 0; x < argc; x++) {
                    char *dpname = dp[x];
                    char *dparg = NULL;

                    if (dpname) {
                        if ((dparg = strchr(dpname, ':'))) {
                            *dparg++ = '\0';
                        }
                    }
                    if (!(dialplan_interface = switch_loadable_module_get_dialplan_interface(dpname))) {
                        continue;
                    }

                    count++;

                    if ((extension = dialplan_interface->hunt_function(session, dparg)) != 0) {
                        switch_channel_set_caller_extension(session->channel, extension);
                        return;
                    }
                }
            }
        }

		if (!count) {
			if (switch_channel_test_flag(session->channel, CF_OUTBOUND)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No Dialplan, changing state to HOLD\n");
				switch_channel_set_state(session->channel, CS_HOLD);
				return;
			} 
		}
	}

	if (!extension) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No Route, Aborting\n");
        switch_channel_hangup(session->channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
    }
}

static void switch_core_standard_on_execute(switch_core_session_t *session)
{
	switch_caller_extension_t *extension;
	switch_event_t *event;
	const switch_application_interface_t *application_interface;


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard EXECUTE\n");
	if ((extension = switch_channel_get_caller_extension(session->channel)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Extension!\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return;
	}

	while (switch_channel_get_state(session->channel) == CS_EXECUTE && extension->current_application) {
		char *expanded = NULL;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Execute %s(%s)\n",
							  extension->current_application->application_name,
							  extension->current_application->application_data);
		if (
			(application_interface =
			 switch_loadable_module_get_application_interface(extension->current_application->application_name)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Application %s\n",
								  extension->current_application->application_name);
			switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		}

		if (!application_interface->application_function) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Function for %s\n",
								  extension->current_application->application_name);
			switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		}
		
		if ((expanded = switch_channel_expand_variables(session->channel, extension->current_application->application_data)) != 
			extension->current_application->application_data) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Expanded String %s(%s)\n",
							  extension->current_application->application_name,
							  expanded);
		}

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(session->channel, event);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application", "%s", extension->current_application->application_name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data-Orig", "%s", extension->current_application->application_data);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data", "%s", expanded);
			switch_event_fire(&event);
		}
		

		if (switch_channel_get_variable(session->channel, "presence_id")) {
			char *arg = switch_mprintf("%s(%s)", extension->current_application->application_name, expanded);
			if (arg) {
				switch_channel_presence(session->channel, "unknown", arg);
				switch_safe_free(arg);
			}
		}

		application_interface->application_function(session, expanded);

		if (expanded != extension->current_application->application_data) {
			switch_safe_free(expanded);
		}
		extension->current_application = extension->current_application->next;
	}
	
	if (switch_channel_get_state(session->channel) == CS_EXECUTE) {
		switch_channel_hangup(session->channel, SWITCH_CAUSE_NORMAL_CLEARING);
	}
}

static void switch_core_standard_on_loopback(switch_core_session_t *session)
{
	switch_frame_t *frame;
	int stream_id;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard LOOPBACK\n");

	while (switch_channel_get_state(session->channel) == CS_LOOPBACK) {
		for (stream_id = 0; stream_id < session->stream_count; stream_id++) {
			if (switch_core_session_read_frame(session, &frame, -1, stream_id) == SWITCH_STATUS_SUCCESS) {
				switch_core_session_write_frame(session, frame, -1, stream_id);
			}
		}
	}
}

static void switch_core_standard_on_transmit(switch_core_session_t *session)
{
	assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard TRANSMIT\n");
}

static void switch_core_standard_on_hold(switch_core_session_t *session)
{
	assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard HOLD\n");
}

static void switch_core_standard_on_hibernate(switch_core_session_t *session)
{
	assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard HIBERNATE\n");
}

SWITCH_DECLARE(void) switch_core_session_signal_state_change(switch_core_session_t *session)
{

	/* If trylock fails the signal is already awake so we needn't bother */
	if (switch_mutex_trylock(session->mutex) == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_signal(session->cond);
		switch_mutex_unlock(session->mutex);
	} 
	
}

SWITCH_DECLARE(unsigned int) switch_core_session_running(switch_core_session_t *session)
{
	return session->thread_running;
}
#ifdef CRASH_PROT
#if defined (__GNUC__) && defined (LINUX)
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#define STACK_LEN 10

/* Obtain a backtrace and print it to stdout. */
static void print_trace (void)
{
	void *array[STACK_LEN];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace (array, STACK_LEN);
	strings = backtrace_symbols (array, size);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Obtained %zd stack frames.\n", size);
	
	for (i = 0; i < size; i++) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CRIT, "%s\n", strings[i]);
	}

	free (strings);
}
#else
static void print_trace (void)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Trace not avaliable =(\n");
}
#endif


static void handle_fatality(int sig)
{
	switch_thread_id_t thread_id;
	jmp_buf *env;

	if (sig && (thread_id = switch_thread_self()) && (env = (jmp_buf *) apr_hash_get(runtime.stack_table, &thread_id, sizeof(thread_id)))) {
		print_trace();
		longjmp(*env, sig);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Caught signal %d for unmapped thread!", sig);
		abort();
	}
}
#endif

SWITCH_DECLARE(void) switch_core_session_run(switch_core_session_t *session)
{
	switch_channel_state_t state = CS_NEW, laststate = CS_HANGUP, midstate = CS_DONE, endstate;
	const switch_endpoint_interface_t *endpoint_interface;
	const switch_state_handler_table_t *driver_state_handler = NULL;
	const switch_state_handler_table_t *application_state_handler = NULL;
#ifdef CRASH_PROT
	switch_thread_id_t thread_id = switch_thread_self();
	jmp_buf env;
	int sig;

	signal(SIGSEGV, handle_fatality);
	signal(SIGFPE, handle_fatality);
#ifndef WIN32
	signal(SIGBUS, handle_fatality);
#endif

	if ((sig = setjmp(env)) != 0) {
		switch_event_t *event;

		if (switch_event_create(&event, SWITCH_EVENT_SESSION_CRASH) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(session->channel, event);
			switch_event_fire(&event);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Thread has crashed for channel %s\n", switch_channel_get_name(session->channel));
		switch_channel_hangup(session->channel, SWITCH_CAUSE_CRASH);
	} else {
		apr_hash_set(runtime.stack_table, &thread_id, sizeof(thread_id), &env);
	}
#endif
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
	
	session->thread_running = 1;
	endpoint_interface = session->endpoint_interface;
	assert(endpoint_interface != NULL);

	driver_state_handler = endpoint_interface->state_handler;
	assert(driver_state_handler != NULL);

	switch_mutex_lock(session->mutex);

	while ((state = switch_channel_get_state(session->channel)) != CS_DONE) {
		if (state != laststate) {
			int index = 0;
			int proceed = 1;
			midstate = state;

			switch (state) {
			case CS_NEW:		/* Just created, Waiting for first instructions */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State NEW\n", switch_channel_get_name(session->channel));
				break;
			case CS_DONE:
				goto done;
			case CS_HANGUP:	/* Deactivate and end the thread */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State HANGUP\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_hangup ||
					(driver_state_handler->on_hangup &&
					 driver_state_handler->on_hangup(session) == SWITCH_STATUS_SUCCESS &&
					 midstate == switch_channel_get_state(session->channel))) {
					while((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hangup ||
							(application_state_handler->on_hangup &&
							 application_state_handler->on_hangup(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while(proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hangup ||
							(application_state_handler->on_hangup &&
							 application_state_handler->on_hangup(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}

					if (proceed) {
						switch_core_standard_on_hangup(session);
					}
				}
				goto done;
			case CS_INIT:		/* Basic setup tasks */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State INIT\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_init ||
					(driver_state_handler->on_init &&
					 driver_state_handler->on_init(session) == SWITCH_STATUS_SUCCESS &&
					 midstate == switch_channel_get_state(session->channel))) {
					while((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_init ||
							(application_state_handler->on_init &&
							 application_state_handler->on_init(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while(proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_init ||
							(application_state_handler->on_init &&
							 application_state_handler->on_init(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_init(session);
					}
				}
				break;
			case CS_RING:		/* Look for a dialplan and find something to do */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State RING\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_ring ||
					(driver_state_handler->on_ring &&
					 driver_state_handler->on_ring(session) == SWITCH_STATUS_SUCCESS &&
					 midstate == switch_channel_get_state(session->channel))) {
					while((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_ring ||
							(application_state_handler->on_ring &&
							 application_state_handler->on_ring(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while(proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_ring ||
							(application_state_handler->on_ring &&
							 application_state_handler->on_ring(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_ring(session);
					}
				}
				break;
			case CS_EXECUTE:	/* Execute an Operation */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State EXECUTE\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_execute ||
					(driver_state_handler->on_execute &&
					 driver_state_handler->on_execute(session) == SWITCH_STATUS_SUCCESS &&
					 midstate == switch_channel_get_state(session->channel))) {
					while((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_execute ||
							(application_state_handler->on_execute &&
							 application_state_handler->on_execute(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while(proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_execute ||
							(application_state_handler->on_execute &&
							 application_state_handler->on_execute(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_execute(session);
					}
				}
				break;
			case CS_LOOPBACK:	/* loop all data back to source */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State LOOPBACK\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_loopback ||
					(driver_state_handler->on_loopback &&
					 driver_state_handler->on_loopback(session) == SWITCH_STATUS_SUCCESS &&
					 midstate == switch_channel_get_state(session->channel))) {
					while((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_loopback ||
							(application_state_handler->on_loopback &&
							 application_state_handler->on_loopback(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while(proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_loopback ||
							(application_state_handler->on_loopback &&
							 application_state_handler->on_loopback(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_loopback(session);
					}
				}
				break;
			case CS_TRANSMIT:	/* send/recieve data to/from another channel */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State TRANSMIT\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_transmit ||
					(driver_state_handler->on_transmit &&
					 driver_state_handler->on_transmit(session) == SWITCH_STATUS_SUCCESS &&
					 midstate == switch_channel_get_state(session->channel))) {

					while((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_transmit ||
							(application_state_handler->on_transmit &&
							 application_state_handler->on_transmit(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while(proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_transmit ||
							(application_state_handler->on_transmit &&
							 application_state_handler->on_transmit(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_transmit(session);
					}
				}
				break;
			case CS_HOLD:	/* wait in limbo */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State HOLD\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_hold ||
					(driver_state_handler->on_hold &&
					 driver_state_handler->on_hold(session) == SWITCH_STATUS_SUCCESS &&
					 midstate == switch_channel_get_state(session->channel))) {

					while((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hold ||
							(application_state_handler->on_hold &&
							 application_state_handler->on_hold(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while(proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hold ||
							(application_state_handler->on_hold &&
							 application_state_handler->on_hold(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_hold(session);
					}
				}
				break;
			case CS_HIBERNATE:	/* wait in limbo */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State HIBERNATE\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_hibernate ||
					(driver_state_handler->on_hibernate &&
					 driver_state_handler->on_hibernate(session) == SWITCH_STATUS_SUCCESS &&
					 midstate == switch_channel_get_state(session->channel))) {

					while((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hibernate ||
							(application_state_handler->on_hibernate &&
							 application_state_handler->on_hibernate(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while(proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hibernate ||
							(application_state_handler->on_hibernate &&
							 application_state_handler->on_hibernate(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_hibernate(session);
					}
				}
				break;
			}

			if (midstate == CS_DONE) {
				break;
			}

			laststate = midstate;
		}
		

		endstate = switch_channel_get_state(session->channel);


		if (midstate == endstate) {
			switch_thread_cond_wait(session->cond, session->mutex);
		}

	}
 done:
	switch_mutex_unlock(session->mutex);

#ifdef CRASH_PROT
	apr_hash_set(runtime.stack_table, &thread_id, sizeof(thread_id), NULL);
#endif
	session->thread_running = 0;

}

SWITCH_DECLARE(void) switch_core_session_destroy(switch_core_session_t **session)
{
	switch_memory_pool_t *pool;
	switch_event_t *event;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Close Channel %s\n", switch_channel_get_name((*session)->channel));

	switch_mutex_lock(runtime.session_table_mutex);
	switch_core_hash_delete(runtime.session_table, (*session)->uuid_str);
	if (runtime.session_count) {
		runtime.session_count--;
	}
	switch_mutex_unlock(runtime.session_table_mutex);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DESTROY) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data((*session)->channel, event);
		switch_event_fire(&event);
	}

	switch_core_media_bug_remove_all(*session);
	switch_buffer_destroy(&(*session)->raw_read_buffer);
	switch_buffer_destroy(&(*session)->raw_write_buffer);
	switch_channel_uninit((*session)->channel);

	pool = (*session)->pool;
	*session = NULL;
	apr_pool_destroy(pool);
	pool = NULL;

}

SWITCH_DECLARE(switch_status_t) switch_core_hash_init(switch_hash_t **hash, switch_memory_pool_t *pool)
{
	assert(pool != NULL);

	if ((*hash = apr_hash_make(pool)) != 0) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_destroy(switch_hash_t *hash)
{
	assert(hash != NULL);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert_dup(switch_hash_t *hash, char *key, void *data)
{
	apr_hash_set(hash, switch_core_strdup(apr_hash_pool_get(hash), key), APR_HASH_KEY_STRING, data);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_insert(switch_hash_t *hash, char *key, void *data)
{
	apr_hash_set(hash, key, APR_HASH_KEY_STRING, data);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_hash_delete(switch_hash_t *hash, char *key)
{
	apr_hash_set(hash, key, APR_HASH_KEY_STRING, NULL);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void *) switch_core_hash_find(switch_hash_t *hash, char *key)
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

SWITCH_DECLARE(void) switch_core_launch_thread(switch_thread_start_t func, void *obj, switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_core_thread_session_t *ts;
	int mypool;

	mypool = pool ? 0 : 1;

	if (!pool && switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate memory pool\n");
		return;
	}

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);

	if ((ts = switch_core_alloc(pool, sizeof(*ts))) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate memory\n");
	} else {
		if (mypool) {
			ts->pool = pool;
		}
		ts->objs[0] = obj;
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, func, ts, pool);
	}

}

static void *SWITCH_THREAD_FUNC switch_core_session_thread(switch_thread_t *thread, void *obj)
{
	switch_core_session_t *session = obj;
	session->thread = thread;



	switch_core_session_run(session);
	switch_core_media_bug_remove_all(session);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Session %u (%s) Locked, Waiting on external entities\n", session->id, switch_channel_get_name(session->channel));
	switch_core_session_write_lock(session);
    switch_set_flag(session, SSF_DESTROYED);
	switch_core_session_rwunlock(session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Session %u (%s) Ended\n", session->id, switch_channel_get_name(session->channel));
	switch_core_session_destroy(&session);
	return NULL;
}


SWITCH_DECLARE(void) switch_core_session_thread_launch(switch_core_session_t *session)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr;;
	switch_threadattr_create(&thd_attr, session->pool);
	switch_threadattr_detach_set(thd_attr, 1);

	if (! session->thread_running) {
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		if (switch_thread_create(&thread, thd_attr, switch_core_session_thread, session, session->pool) != SWITCH_STATUS_SUCCESS) {
			switch_core_session_destroy(&session);
		}
	}
}


SWITCH_DECLARE(void) switch_core_session_launch_thread(switch_core_session_t *session, switch_thread_start_t func,
													   void *obj)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_threadattr_create(&thd_attr, session->pool);
	switch_threadattr_detach_set(thd_attr, 1);

	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, func, obj, session->pool);

}


SWITCH_DECLARE(void *) switch_core_alloc(switch_memory_pool_t *pool, switch_size_t memory)
{
	void *ptr = NULL;
	assert(pool != NULL);

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocate %d\n", memory);
	//assert(memory < 600000);
#endif

	if ((ptr = apr_palloc(pool, memory)) != 0) {
		memset(ptr, 0, memory);
	}
	return ptr;
}

SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request(const switch_endpoint_interface_t *endpoint_interface,
																  switch_memory_pool_t *pool)
{
	switch_memory_pool_t *usepool;
	switch_core_session_t *session;
	switch_uuid_t uuid;
	uint32_t count = 0;

	assert(endpoint_interface != NULL);

	switch_mutex_lock(runtime.session_table_mutex);
	count = runtime.session_count;
	switch_mutex_unlock(runtime.session_table_mutex);

	if ((count + 1) > runtime.session_limit) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Over Session Limit!\n");
		return NULL;
	}

	if (runtime.no_new_sessions || runtime.shutting_down) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read my lips: no new sessions!\n");
		return NULL;
	}

	if (pool) {
		usepool = pool;
	} else if (switch_core_new_memory_pool(&usepool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate memory pool\n");
		return NULL;
	}

	if ((session = switch_core_alloc(usepool, sizeof(switch_core_session_t))) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate session\n");
		apr_pool_destroy(usepool);
		return NULL;
	}

	if (switch_channel_alloc(&session->channel, usepool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate channel structure\n");
		apr_pool_destroy(usepool);
		return NULL;
	}

	switch_channel_init(session->channel, session, CS_NEW, 0);

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

	switch_mutex_init(&session->mutex, SWITCH_MUTEX_NESTED, session->pool);
	switch_thread_rwlock_create(&session->bug_rwlock, session->pool);
	switch_thread_cond_create(&session->cond, session->pool);
	switch_thread_rwlock_create(&session->rwlock, session->pool);

	snprintf(session->name, sizeof(session->name), "%u", session->id);
	switch_mutex_lock(runtime.session_table_mutex);
	session->id = runtime.session_id++;
	switch_core_hash_insert(runtime.session_table, session->uuid_str, session);
	runtime.session_count++;
	switch_mutex_unlock(runtime.session_table_mutex);

	return session;
}

SWITCH_DECLARE(uint32_t) switch_core_session_count(void)
{
	return runtime.session_count;
}

SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request_by_name(char *endpoint_name, switch_memory_pool_t *pool)
{
	const switch_endpoint_interface_t *endpoint_interface;

	if ((endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate channel type %s\n", endpoint_name);
		return NULL;
	}

	return switch_core_session_request(endpoint_interface, pool);
}

SWITCH_DECLARE(switch_status_t) switch_core_db_persistant_execute(switch_core_db_t *db, char *sql, uint32_t retries)
{
	char *errmsg;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint8_t forever = 0;

	if (!retries) {
		forever = 1;
		retries = 1000;
	}

	while(retries > 0) {
		switch_core_db_exec(
							db,
							sql,
							NULL,
							NULL,
							&errmsg
							);		
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", errmsg);
			switch_core_db_free(errmsg);
			switch_yield(100000);
			retries--;
			if (retries == 0 && forever) {
				retries = 1000;
				continue;
			}
		} else {
			status = SWITCH_STATUS_SUCCESS;
			break;
		}
	}

	return status;
}

#ifdef DO_EVENTS
#define SQLLEN 1024 * 64
static void *SWITCH_THREAD_FUNC switch_core_sql_thread(switch_thread_t *thread, void *obj)
{
	void *pop;
	uint32_t itterations = 0;
	uint8_t trans = 0, nothing_in_queue = 0;
	uint32_t freq = 1000, target = 1000;
	uint32_t len = 0;
	uint32_t sql_len = SQLLEN;
	char *sqlbuf = (char *) malloc(sql_len);
	
	if (!runtime.event_db) {
		runtime.event_db = switch_core_db_handle();
	}
	switch_queue_create(&runtime.sql_queue, SWITCH_SQL_QUEUE_LEN, runtime.memory_pool);

	for(;;) {
		if (switch_queue_trypop(runtime.sql_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			char *sql = (char *) pop;
			uint32_t newlen;

			if (sql) {
				if (itterations == 0) {
					char *isql = "begin transaction CORE1;\n";
					switch_core_db_persistant_execute(runtime.event_db, isql, 0);
					trans = 1;
					
				}

				itterations++;
				newlen = (uint32_t)strlen(sql) + 2;
				if (len + newlen > sql_len) {
					sql_len = len + SQLLEN;
					if (!(sqlbuf = realloc(sqlbuf, sql_len))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL thread ending on mem err\n");
						break;
					}
				}
				snprintf(sqlbuf + len, sql_len - len, "%s;\n", sql); 
				len += newlen;
				switch_core_db_free(sql);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "SQL thread ending\n");
				break;
			}
		} else {
			nothing_in_queue = 1;
		}


		if (trans && ((itterations == target) || nothing_in_queue)) {
			char *isql = "end transaction CORE1";

			switch_core_db_persistant_execute(runtime.event_db, sqlbuf, 0);
			switch_core_db_persistant_execute(runtime.event_db, isql, 0);
			itterations = 0;
			trans = 0;
			nothing_in_queue = 0;
			len = 0;
			*sqlbuf = '\0';
		}
		
		if (nothing_in_queue) {
			switch_yield(freq);
		} 
	}


	free(sqlbuf);
	return NULL;
}


static void switch_core_sql_thread_launch(void)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr;;
	
	assert(runtime.memory_pool != NULL);

	switch_threadattr_create(&thd_attr, runtime.memory_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, switch_core_sql_thread, NULL, runtime.memory_pool);
	
}

static void core_event_handler(switch_event_t *event)
{
	char *sql = NULL;

	switch (event->event_id) {
	case SWITCH_EVENT_CHANNEL_DESTROY:
		sql = switch_mprintf("delete from channels where uuid='%s'", switch_event_get_header(event, "unique-id"));
		break;
	case SWITCH_EVENT_CHANNEL_CREATE:
		sql = switch_mprintf("insert into channels (uuid,created,name,state) values('%q','%q','%q','%q')",
									 switch_event_get_header(event, "unique-id"),
									 switch_event_get_header(event, "event-date-local"),
									 switch_event_get_header(event, "channel-name"),
									 switch_event_get_header(event, "channel-state")
									 );
		break;
	case SWITCH_EVENT_CODEC:
		sql = switch_mprintf("update channels set read_codec='%q',read_rate='%q',write_codec='%q',write_rate='%q' where uuid='%q'",
									 switch_event_get_header(event, "channel-read-codec-name"),
									 switch_event_get_header(event, "channel-read-codec-rate"),
									 switch_event_get_header(event, "channel-write-codec-name"),
									 switch_event_get_header(event, "channel-write-codec-rate"),
									 switch_event_get_header(event, "unique-id"));
		break;
	case SWITCH_EVENT_CHANNEL_EXECUTE:
		sql = switch_mprintf("update channels set application='%q',application_data='%q' where uuid='%q'",
									 switch_event_get_header(event, "application"),
									 switch_event_get_header(event, "application-data"),
									 switch_event_get_header(event, "unique-id")
									 );
		break;
	case SWITCH_EVENT_CHANNEL_STATE:
		if (event) {
			char *state = switch_event_get_header(event, "channel-state-number");
			switch_channel_state_t state_i = atoi(state);

			switch(state_i) {
			case CS_HANGUP:
			case CS_DONE:
				break;
			case CS_RING:
				sql = switch_mprintf("update channels set state='%s',cid_name='%q',cid_num='%q',ip_addr='%s',dest='%q'"
											 "where uuid='%s'",
											 switch_event_get_header(event, "channel-state"),
											 switch_event_get_header(event, "caller-caller-id-name"),
											 switch_event_get_header(event, "caller-caller-id-number"),
											 switch_event_get_header(event, "caller-network-addr"),
											 switch_event_get_header(event, "caller-destination-number"),
											 switch_event_get_header(event, "unique-id")
											 );
				break;
			default:
				sql = switch_mprintf("update channels set state='%s' where uuid='%s'", 
											 switch_event_get_header(event, "channel-state"),
											 switch_event_get_header(event, "unique-id")
											 );
				break;
			}
		
		}
		break;
	case SWITCH_EVENT_CHANNEL_BRIDGE:
		sql = switch_mprintf("insert into calls values ('%s','%q','%q','%q','%q','%s','%q','%q','%q','%q','%s')",
									 switch_event_get_header(event, "event-calling-function"),
									 switch_event_get_header(event, "caller-caller-id-name"),
									 switch_event_get_header(event, "caller-caller-id-number"),
									 switch_event_get_header(event, "caller-destination-number"),
									 switch_event_get_header(event, "caller-channel-name"),
									 switch_event_get_header(event, "caller-unique-id"),
									 switch_event_get_header(event, "originatee-caller-id-name"),
									 switch_event_get_header(event, "originatee-caller-id-number"),
									 switch_event_get_header(event, "originatee-destination-number"),
									 switch_event_get_header(event, "originatee-channel-name"),
									 switch_event_get_header(event, "originatee-unique-id")
									 );
		break;
	case SWITCH_EVENT_CHANNEL_UNBRIDGE:
		sql = switch_mprintf("delete from calls where caller_uuid='%s'", switch_event_get_header(event, "caller-unique-id"));
		break;
	case SWITCH_EVENT_SHUTDOWN:
		sql = switch_mprintf("delete from channels;delete from interfaces;delete from calls");
		break;
	case SWITCH_EVENT_LOG:
		return;
	case SWITCH_EVENT_MODULE_LOAD:
		sql = switch_mprintf("insert into interfaces (type,name,description,syntax) values('%q','%q','%q','%q')",
									 switch_event_get_header(event, "type"),
									 switch_event_get_header(event, "name"),
									 switch_event_get_header(event, "description"),
									 switch_event_get_header(event, "syntax")
									 );
		break;
	default:
		break;
	}

	if (sql) {
		switch_queue_push(runtime.sql_queue, sql);
		sql = NULL;
	}
}
#endif

SWITCH_DECLARE(void) switch_core_set_globals(void)
{
#ifdef WIN32
#define BUFSIZE 1024
    char lpPathBuffer[BUFSIZE];
	DWORD dwBufSize=BUFSIZE;
	char exePath[1024];
	char *lastbacklash;
	GetModuleFileName( NULL, exePath, BUFSIZE );
	lastbacklash = strrchr( exePath, '\\');
	exePath[(lastbacklash - exePath + 1)] = '\0';
	if (!SWITCH_GLOBAL_dirs.base_dir && (SWITCH_GLOBAL_dirs.base_dir = (char *) malloc(BUFSIZE))) {
		snprintf(SWITCH_GLOBAL_dirs.base_dir, BUFSIZE, "%s", exePath);
	}
	if (!SWITCH_GLOBAL_dirs.mod_dir && (SWITCH_GLOBAL_dirs.mod_dir = (char *) malloc(BUFSIZE))) {
		snprintf(SWITCH_GLOBAL_dirs.mod_dir, BUFSIZE, "%smod", exePath);
	}
	if (!SWITCH_GLOBAL_dirs.conf_dir && (SWITCH_GLOBAL_dirs.conf_dir = (char *) malloc(BUFSIZE))) {
		snprintf(SWITCH_GLOBAL_dirs.conf_dir, BUFSIZE, "%sconf", exePath);
	}
	if (!SWITCH_GLOBAL_dirs.log_dir && (SWITCH_GLOBAL_dirs.log_dir = (char *) malloc(BUFSIZE))) {
		snprintf(SWITCH_GLOBAL_dirs.log_dir, BUFSIZE, "%slog", exePath);
	}
	if (!SWITCH_GLOBAL_dirs.db_dir && (SWITCH_GLOBAL_dirs.db_dir = (char *) malloc(BUFSIZE))) {
		snprintf(SWITCH_GLOBAL_dirs.db_dir, BUFSIZE, "%sdb", exePath);
	}
	if (!SWITCH_GLOBAL_dirs.script_dir && (SWITCH_GLOBAL_dirs.script_dir = (char *) malloc(BUFSIZE))) {
		snprintf(SWITCH_GLOBAL_dirs.script_dir, BUFSIZE, "%sscripts", exePath);
	}
	if (!SWITCH_GLOBAL_dirs.htdocs_dir && (SWITCH_GLOBAL_dirs.htdocs_dir = (char *) malloc(BUFSIZE))) {
		snprintf(SWITCH_GLOBAL_dirs.htdocs_dir, BUFSIZE, "%shtdocs", exePath);
	}
	if (!SWITCH_GLOBAL_dirs.htdocs_dir && (SWITCH_GLOBAL_dirs.grammar_dir = (char *) malloc(BUFSIZE))) {
		snprintf(SWITCH_GLOBAL_dirs.grammar_dir, BUFSIZE, "%sgrammar", exePath);
	}
#else
	SWITCH_GLOBAL_dirs.base_dir = SWITCH_PREFIX_DIR;
	SWITCH_GLOBAL_dirs.mod_dir = SWITCH_MOD_DIR;
	if(!SWITCH_GLOBAL_dirs.conf_dir) SWITCH_GLOBAL_dirs.conf_dir = SWITCH_CONF_DIR;
	if(!SWITCH_GLOBAL_dirs.log_dir) SWITCH_GLOBAL_dirs.log_dir = SWITCH_LOG_DIR;
	if(!SWITCH_GLOBAL_dirs.db_dir) SWITCH_GLOBAL_dirs.db_dir = SWITCH_DB_DIR;
	SWITCH_GLOBAL_dirs.script_dir = SWITCH_SCRIPT_DIR;
	SWITCH_GLOBAL_dirs.htdocs_dir = SWITCH_HTDOCS_DIR;
	SWITCH_GLOBAL_dirs.grammar_dir = SWITCH_GRAMMAR_DIR;
#endif
#ifdef SWITCH_TEMP_DIR
	SWITCH_GLOBAL_dirs.temp_dir = SWITCH_TEMP_DIR;
#else
#ifdef WIN32
	GetTempPath(dwBufSize, lpPathBuffer);
	if (!SWITCH_GLOBAL_dirs.htdocs_dir && (SWITCH_GLOBAL_dirs.htdocs_dir = (char *) malloc(BUFSIZE))) {
		snprintf(SWITCH_GLOBAL_dirs.htdocs_dir, BUFSIZE, "%s", lpPathBuffer);
	}
#else
	SWITCH_GLOBAL_dirs.temp_dir = "/tmp/";
#endif
#endif
}


SWITCH_DECLARE(uint32_t) switch_core_session_limit(uint32_t new_limit)
{
	if (new_limit) {
		runtime.session_limit = new_limit;
	}
	
	return runtime.session_limit;
}


SWITCH_DECLARE(int32_t) set_high_priority(void)
{
#ifdef __linux__
	struct sched_param sched = {0};
	sched.sched_priority = 1;
	if (sched_setscheduler(0, SCHED_RR, &sched)) {
        sched.sched_priority = 0;
        if (sched_setscheduler(0, SCHED_OTHER, &sched)) {
			return -1;
		}
	}
#endif

#ifdef WIN32
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#else
	nice(-10);
#endif

#define USE_MLOCKALL
#ifdef HAVE_MLOCKALL
#ifdef USE_MLOCKALL
	mlockall(MCL_CURRENT|MCL_FUTURE);
#endif
#endif
	return 0;
}

SWITCH_DECLARE(void) switch_core_runtime_loop(int bg)
{
#ifdef WIN32
	HANDLE shutdown_event;
	char path[256] = "";
#endif
	if (bg) {
		bg = 0;
#ifdef WIN32
		snprintf(path, sizeof(path), "Global\\Freeswitch.%d", getpid());
		shutdown_event = CreateEvent(NULL, FALSE, FALSE, path);		
		WaitForSingleObject(shutdown_event, INFINITE);
#else
		runtime.running = 1;
		while(runtime.running) {
			switch_yield(1000000);
		}
#endif
	}  else {
		/* wait for console input */
		switch_console_loop();
	}
}


SWITCH_DECLARE(switch_status_t) switch_core_init(char *console, const char **err)
{
	switch_xml_t xml = NULL, cfg = NULL;
	memset(&runtime, 0, sizeof(runtime));
	runtime.session_limit = 1000;

	switch_core_set_globals();

	/* INIT APR and Create the pool context */
	if (apr_initialize() != SWITCH_STATUS_SUCCESS) {
		apr_terminate();
		*err = "FATAL ERROR! Could not initilize APR\n";
		return SWITCH_STATUS_MEMERR;
	}

	if (apr_pool_create(&runtime.memory_pool, NULL) != SWITCH_STATUS_SUCCESS) {
		apr_terminate();
		*err = "FATAL ERROR! Could not allocate memory pool\n";
		return SWITCH_STATUS_MEMERR;
	}

	switch_core_hash_init(&runtime.global_vars, runtime.memory_pool);

	if (switch_xml_init(runtime.memory_pool, err) != SWITCH_STATUS_SUCCESS) {
		apr_terminate();
		return SWITCH_STATUS_MEMERR;
	}


	if ((xml = switch_xml_open_cfg("switch.conf", &cfg, NULL))) {
		switch_xml_t settings, param;
		
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				
				if (!strcasecmp(var, "max-sessions")) {
					runtime.session_limit = atoi(val);
				}
			}
		}

		if ((settings = switch_xml_child(cfg, "variables"))) {
			for (param = switch_xml_child(settings, "variable"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				char *varr = NULL, *vall = NULL;

				varr = switch_core_strdup(runtime.memory_pool, var);
				vall = switch_core_strdup(runtime.memory_pool, val);
				switch_core_hash_insert(runtime.global_vars, varr, vall);
			}
		}
		switch_xml_free(xml);
	}

	*err = NULL;

	if(console) {
		if (*console != '/') {
			char path[265];
			snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, console);
			console = path;
		}
		if (switch_core_set_console(console) != SWITCH_STATUS_SUCCESS) {
			*err = "FATAL ERROR! Could not open console\n";
			apr_terminate();
			return SWITCH_STATUS_GENERR;
		}
	} else {
		runtime.console = stdout;
	}

	assert(runtime.memory_pool != NULL);
	switch_log_init(runtime.memory_pool);

#ifdef DO_EVENTS
	switch_core_sql_thread_launch();
#endif

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocated memory pool. Sessions are %u bytes\n", sizeof(switch_core_session_t));
	switch_event_init(runtime.memory_pool);
	switch_rtp_init(runtime.memory_pool);

	/* Activate SQL database */
	if ((runtime.db = switch_core_db_handle()) == 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");
	} else {
		char create_channels_sql[] =
			"CREATE TABLE channels (\n"
			"   uuid  VARCHAR(255),\n"
			"   created  VARCHAR(255),\n"
			"   name  VARCHAR(255),\n"
			"   state  VARCHAR(255),\n"
			"   cid_name  VARCHAR(255),\n"
			"   cid_num  VARCHAR(255),\n"
			"   ip_addr  VARCHAR(255),\n"
			"   dest  VARCHAR(255),\n"
			"   application  VARCHAR(255),\n"
			"   application_data  VARCHAR(255),\n"
			"   read_codec  VARCHAR(255),\n"
			"   read_rate  VARCHAR(255),\n"
			"   write_codec  VARCHAR(255),\n"
			"   write_rate  VARCHAR(255)\n"
			");\n";
		char create_calls_sql[] =
			"CREATE TABLE calls (\n"
			"   function  VARCHAR(255),\n"
			"   caller_cid_name  VARCHAR(255),\n"
			"   caller_cid_num   VARCHAR(255),\n"
			"   caller_dest_num  VARCHAR(255),\n"
			"   caller_chan_name VARCHAR(255),\n"
			"   caller_uuid      VARCHAR(255),\n"
			"   callee_cid_name  VARCHAR(255),\n"
			"   callee_cid_num   VARCHAR(255),\n"
			"   callee_dest_num  VARCHAR(255),\n"
			"   callee_chan_name VARCHAR(255),\n"
			"   callee_uuid      VARCHAR(255)\n"
			");\n";
		char create_interfaces_sql[] =
			"CREATE TABLE interfaces (\n"
			"   type             VARCHAR(255),\n"
			"   name             VARCHAR(255),\n"
			"   description      VARCHAR(255),\n"
			"   syntax           VARCHAR(255)\n"
			");\n";

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening DB\n");
		switch_core_db_exec(runtime.db, "drop table channels", NULL, NULL, NULL);
		switch_core_db_exec(runtime.db, "drop table calls", NULL, NULL, NULL);
		switch_core_db_exec(runtime.db, "drop table interfaces", NULL, NULL, NULL);
		switch_core_db_exec(runtime.db, create_channels_sql, NULL, NULL, NULL);
		switch_core_db_exec(runtime.db, create_calls_sql, NULL, NULL, NULL);
		switch_core_db_exec(runtime.db, create_interfaces_sql, NULL, NULL, NULL);
#ifdef DO_EVENTS		
		if (switch_event_bind("core_db", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL) !=
			SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event handler!\n");
		}
#endif
	}

	runtime.session_id = 1;
	runtime.running = 1;
	switch_core_hash_init(&runtime.session_table, runtime.memory_pool);
	switch_mutex_init(&runtime.session_table_mutex, SWITCH_MUTEX_NESTED, runtime.memory_pool);
#ifdef CRASH_PROT
	switch_core_hash_init(&runtime.stack_table, runtime.memory_pool);
#endif
	runtime.initiated = switch_time_now();
	return SWITCH_STATUS_SUCCESS;
}

#ifdef SIGPIPE
static void handle_SIGPIPE(int sig)
{
	if(sig);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig Pipe!\n");
	return;
}
#endif

#ifdef SIGPOLL
static void handle_SIGPOLL(int sig)
{
	if(sig);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig Poll!\n");
	return;
}
#endif

#ifdef SIGIO
static void handle_SIGIO(int sig)
{
	if(sig);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig I/O!\n");
	return;
}
#endif

#ifdef TRAP_BUS
static void handle_SIGBUS(int sig)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig BUS!\n");
	return;
}
#endif

/* no ctl-c mofo */
static void handle_SIGINT(int sig)
{
	if (sig);
	return;
}
SWITCH_DECLARE(switch_status_t) switch_core_init_and_modload(char *console, const char **err)
{
	switch_event_t *event;
	if (switch_core_init(console, err) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	/* set signal handlers */
	signal(SIGINT, handle_SIGINT);
#ifdef SIGPIPE
	signal(SIGPIPE, handle_SIGPIPE);
#endif
#ifdef SIGPOLL
	signal(SIGPIPE, handle_SIGPOLL);
#endif
#ifdef SIGIO
	signal(SIGPIPE, handle_SIGIO);
#endif
#ifdef TRAP_BUS
	signal(SIGBUS, handle_SIGBUS);
#endif
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Bringing up environment.\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Loading Modules.\n");
	if (switch_loadable_module_init() != SWITCH_STATUS_SUCCESS) {
		*err = "Cannot load modules";
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Error: %s", err);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_create(&event, SWITCH_EVENT_STARTUP) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Info", "System Ready");
		switch_event_fire(&event);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\nFreeSWITCH Version %s Started.\nCrash Protection [%s]\nMax Sessions[%u]\n\n", SWITCH_VERSION_FULL, __CP, switch_core_session_limit(0));
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(void) switch_core_measure_time(switch_time_t total_ms, switch_core_time_duration_t *duration)
{
    switch_time_t temp = total_ms / 1000;
	memset(duration, 0, sizeof(*duration));
	duration->mms = (uint32_t)(total_ms % 1000);
	duration->ms = (uint32_t)(temp % 1000);
    temp = temp / 1000;
    duration->sec = (uint32_t)(temp % 60);
    temp = temp / 60;
    duration->min = (uint32_t)(temp % 60);
	temp = temp / 60;
	duration->hr = (uint32_t)(temp % 24);
	temp = temp / 24;
	duration->day = (uint32_t)(temp % 365);
	duration->yr = (uint32_t)(temp / 365);
}

SWITCH_DECLARE(switch_time_t) switch_core_uptime(void)
{
	return switch_time_now() - runtime.initiated;
}

SWITCH_DECLARE(int32_t) switch_core_session_ctl(switch_session_ctl_t cmd, uint32_t *val)
{

	if (runtime.shutting_down) {
		return -1;
	}

	switch (cmd) {
	case SCSC_PAUSE_INBOUND:
		runtime.no_new_sessions = *val;
		break;
	case SCSC_HUPALL:
		switch_core_session_hupall(SWITCH_CAUSE_MANAGER_REQUEST);
		break;
	case SCSC_SHUTDOWN:
		runtime.running = 0;
		break;
	case SCSC_CHECK_RUNNING:
		*val = runtime.running;
		break;
	}

	return 0;
}

SWITCH_DECLARE(switch_status_t) switch_core_destroy(int vg)
{
	switch_event_t *event;
	if (switch_event_create(&event, SWITCH_EVENT_SHUTDOWN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Info", "System Shutting Down");
		switch_event_fire(&event);
	}
	runtime.shutting_down = 1;
	runtime.no_new_sessions = 1;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "End existing sessions\n");
	switch_core_session_hupall(SWITCH_CAUSE_SYSTEM_SHUTDOWN);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Clean up modules.\n");
	switch_loadable_module_shutdown();

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Closing Event Engine.\n");
	switch_event_shutdown();
	
	switch_queue_push(runtime.sql_queue, NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Waiting for unfinished SQL transactions\n");
	while (switch_queue_size(runtime.sql_queue) > 0) {
		switch_yield(10000);
	}
	switch_core_db_close(runtime.db);
	switch_core_db_close(runtime.event_db);
	switch_xml_destroy();
	if (vg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Full destruction of the core disabled for memory debugging purposes.\n");
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Finalizing Shutdown.\n");
	switch_log_shutdown();
	
	if(runtime.console != stdout && runtime.console != stderr) {
		fclose(runtime.console);
		runtime.console = NULL;
	}

    switch_yield(1000000);

#ifdef WIN32
	free(SWITCH_GLOBAL_dirs.base_dir);
	free(SWITCH_GLOBAL_dirs.mod_dir);
	free(SWITCH_GLOBAL_dirs.conf_dir);
	free(SWITCH_GLOBAL_dirs.log_dir);
	free(SWITCH_GLOBAL_dirs.db_dir);
	free(SWITCH_GLOBAL_dirs.script_dir);
	free(SWITCH_GLOBAL_dirs.htdocs_dir);
	free(SWITCH_GLOBAL_dirs.grammar_dir);
	free(SWITCH_GLOBAL_dirs.temp_dir);
#endif

	if (runtime.memory_pool) {
		apr_pool_destroy(runtime.memory_pool);
		if (!vg) {
			apr_terminate();
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
