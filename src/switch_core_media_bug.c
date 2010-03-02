/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_media_bug.c -- Main Core Library (Media Bugs)
 *
 */

#include "switch.h"
#include "private/switch_core_pvt.h"

static void switch_core_media_bug_destroy(switch_media_bug_t *bug)
{
	switch_event_t *event = NULL;

	if (bug->raw_read_buffer) {
		switch_buffer_destroy(&bug->raw_read_buffer);
	}

	if (bug->raw_write_buffer) {
		switch_buffer_destroy(&bug->raw_write_buffer);
	}

	if (switch_event_create(&event, SWITCH_EVENT_MEDIA_BUG_STOP) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Media-Bug-Function", "%s", bug->function);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Media-Bug-Target", "%s", bug->target);
		if (bug->session) switch_channel_event_set_data(bug->session->channel, event);
		switch_event_fire(&event);
	}
}

SWITCH_DECLARE(void) switch_core_media_bug_pause(switch_core_session_t *session)
{
	switch_channel_set_flag(session->channel, CF_PAUSE_BUGS);
}

SWITCH_DECLARE(void) switch_core_media_bug_resume(switch_core_session_t *session)
{
	switch_channel_clear_flag(session->channel, CF_PAUSE_BUGS);
}

SWITCH_DECLARE(uint32_t) switch_core_media_bug_test_flag(switch_media_bug_t *bug, uint32_t flag)
{
	return switch_test_flag(bug, flag);
}

SWITCH_DECLARE(uint32_t) switch_core_media_bug_set_flag(switch_media_bug_t *bug, uint32_t flag)
{
	return switch_set_flag(bug, flag);
}

SWITCH_DECLARE(uint32_t) switch_core_media_bug_clear_flag(switch_media_bug_t *bug, uint32_t flag)
{
	return switch_clear_flag(bug, flag);
}

SWITCH_DECLARE(switch_core_session_t *) switch_core_media_bug_get_session(switch_media_bug_t *bug)
{
	return bug->session;
}

SWITCH_DECLARE(switch_frame_t *) switch_core_media_bug_get_write_replace_frame(switch_media_bug_t *bug)
{
	return bug->write_replace_frame_in;
}

SWITCH_DECLARE(void) switch_core_media_bug_set_write_replace_frame(switch_media_bug_t *bug, switch_frame_t *frame)
{
	bug->write_replace_frame_out = frame;
}

SWITCH_DECLARE(switch_frame_t *) switch_core_media_bug_get_read_replace_frame(switch_media_bug_t *bug)
{
	return bug->read_replace_frame_in;
}

SWITCH_DECLARE(void) switch_core_media_bug_set_read_replace_frame(switch_media_bug_t *bug, switch_frame_t *frame)
{
	bug->read_replace_frame_out = frame;
}

SWITCH_DECLARE(void *) switch_core_media_bug_get_user_data(switch_media_bug_t *bug)
{
	return bug->user_data;
}

SWITCH_DECLARE(void) switch_core_media_bug_flush(switch_media_bug_t *bug)
{
	if (bug->raw_read_buffer) {
		switch_buffer_zero(bug->raw_read_buffer);
	}

	if (bug->raw_write_buffer) {
		switch_buffer_zero(bug->raw_write_buffer);
	}
}

SWITCH_DECLARE(void) switch_core_media_bug_inuse(switch_media_bug_t *bug, switch_size_t *readp, switch_size_t *writep)
{
	if (switch_test_flag(bug, SMBF_READ_STREAM)) {
		switch_mutex_lock(bug->read_mutex);
		*readp = bug->raw_read_buffer ? switch_buffer_inuse(bug->raw_read_buffer) : 0;
		switch_mutex_unlock(bug->read_mutex);
	} else {
		*readp = 0;
	}

	if (switch_test_flag(bug, SMBF_WRITE_STREAM)) {
		switch_mutex_lock(bug->write_mutex);
		*writep = bug->raw_write_buffer ? switch_buffer_inuse(bug->raw_write_buffer) : 0;
		switch_mutex_unlock(bug->write_mutex);
	} else {
		*writep = 0;
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_read(switch_media_bug_t *bug, switch_frame_t *frame, switch_bool_t fill)
{
	switch_size_t bytes = 0, datalen = 0, ttl = 0;
	int16_t *dp, *fp;
	uint32_t x;
	size_t rlen = 0;
	size_t wlen = 0;
	uint32_t blen;
	switch_codec_implementation_t read_impl = { 0 };
	int16_t *tp;

	switch_core_session_get_read_impl(bug->session, &read_impl);

	bytes = read_impl.decoded_bytes_per_packet;

	if (frame->buflen < bytes) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_ERROR, "%s frame buffer too small!\n",
						  switch_channel_get_name(bug->session->channel));
		return SWITCH_STATUS_FALSE;
	}

	if (!(bug->raw_read_buffer && (bug->raw_write_buffer || !switch_test_flag(bug, SMBF_WRITE_STREAM)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_ERROR, "%s Buffer Error\n",
						  switch_channel_get_name(bug->session->channel));
		return SWITCH_STATUS_FALSE;
	}

	frame->flags = 0;
	frame->datalen = 0;

	if (!switch_buffer_inuse(bug->raw_read_buffer)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(bug->read_mutex);
	frame->datalen = (uint32_t) switch_buffer_read(bug->raw_read_buffer, frame->data, bytes);
	ttl += frame->datalen;
	switch_mutex_unlock(bug->read_mutex);

	if (switch_test_flag(bug, SMBF_WRITE_STREAM)) {
		switch_assert(bug->raw_write_buffer);
		switch_mutex_lock(bug->write_mutex);
		datalen = (uint32_t) switch_buffer_read(bug->raw_write_buffer, bug->data, bytes);
		ttl += datalen;
		if (fill && datalen < bytes) {
			memset(((unsigned char *) bug->data) + datalen, 0, bytes - datalen);
			datalen = bytes;
		}
		switch_mutex_unlock(bug->write_mutex);
	}

	tp = bug->tmp;
	dp = (int16_t *) bug->data;
	fp = (int16_t *) frame->data;
	rlen = frame->datalen / 2;
	wlen = datalen / 2;
	blen = bytes / 2;

	if (!fill && rlen == 0 && wlen == 0) {
		frame->datalen = 0;
		frame->samples = 0;
		frame->rate = read_impl.actual_samples_per_second;
		frame->codec = NULL;
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(bug, SMBF_STEREO)) {
		for (x = 0; x < blen; x++) {
			if (x < rlen) {
				*(tp++) = *(fp + x);
			} else {
				*(tp++) = 0;
			}
			if (x < wlen) {
				*(tp++) = *(dp + x);
			} else {
				*(tp++) = 0;
			}
		}
		memcpy(frame->data, bug->tmp, bytes * 2);
	} else {
		for (x = 0; x < blen; x++) {
			int32_t z = 0;

			if (x < rlen) {
				z += (int32_t) * (fp + x);
			}
			if (x < wlen) {
				z += (int32_t) * (dp + x);
			}
			switch_normalize_to_16bit(z);
			*(fp + x) = (int16_t) z / 2;
		}
	}

	if (!ttl) {
		switch_set_flag(frame, SFF_CNG);
	}

	frame->datalen = bytes;
	frame->samples = bytes / sizeof(int16_t);
	frame->rate = read_impl.actual_samples_per_second;
	frame->codec = NULL;

	return SWITCH_STATUS_SUCCESS;
}

#define MAX_BUG_BUFFER 1024 * 512
SWITCH_DECLARE(switch_status_t) switch_core_media_bug_add(switch_core_session_t *session,
														  const char *function,
														  const char *target,
														  switch_media_bug_callback_t callback,
														  void *user_data, time_t stop_time, 
														  switch_media_bug_flag_t flags, 
														  switch_media_bug_t **new_bug)
{
	switch_media_bug_t *bug;	//, *bp;
	switch_size_t bytes;
	switch_codec_implementation_t read_impl = { 0 };
	switch_codec_implementation_t write_impl = { 0 };
	switch_event_t *event;

	const char *p;

	if (!switch_channel_media_ready(session->channel)) {
		if (switch_channel_pre_answer(session->channel) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}

	switch_core_session_get_read_impl(session, &read_impl);
	switch_core_session_get_write_impl(session, &write_impl);

	*new_bug = NULL;


	if ((p = switch_channel_get_variable(session->channel, "media_bug_answer_req")) && switch_true(p)) {
		flags |= SMBF_ANSWER_REQ;
	}
#if 0
	if (flags & SMBF_WRITE_REPLACE) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if (switch_test_flag(bp, SMBF_WRITE_REPLACE)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Only one bug of this type allowed!\n");
				switch_thread_rwlock_unlock(session->bug_rwlock);
				return SWITCH_STATUS_GENERR;
			}
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
	}

	if (flags & SMBF_READ_REPLACE) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if (switch_test_flag(bp, SMBF_READ_REPLACE)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Only one bug of this type allowed!\n");
				switch_thread_rwlock_unlock(session->bug_rwlock);
				return SWITCH_STATUS_GENERR;
			}
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
	}
#endif

	if (!(bug = switch_core_session_alloc(session, sizeof(*bug)))) {
		return SWITCH_STATUS_MEMERR;
	}

	bug->callback = callback;
	bug->user_data = user_data;
	bug->session = session;
	bug->flags = flags;
	bug->function = "N/A";
	bug->target = "N/A";

	if (function) {
		bug->function = switch_core_session_strdup(session, function);
	}

	if (target) {
		bug->target = switch_core_session_strdup(session, target);
	}
	
	bug->stop_time = stop_time;
	bytes = read_impl.decoded_bytes_per_packet;

	if (!bug->flags) {
		bug->flags = (SMBF_READ_STREAM | SMBF_WRITE_STREAM);
	}

	if (switch_test_flag(bug, SMBF_READ_STREAM) || switch_test_flag(bug, SMBF_READ_PING)) {
		switch_buffer_create_dynamic(&bug->raw_read_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, MAX_BUG_BUFFER);
		switch_mutex_init(&bug->read_mutex, SWITCH_MUTEX_NESTED, session->pool);
	}

	bytes = write_impl.decoded_bytes_per_packet;

	if (switch_test_flag(bug, SMBF_WRITE_STREAM)) {
		switch_buffer_create_dynamic(&bug->raw_write_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, MAX_BUG_BUFFER);
		switch_mutex_init(&bug->write_mutex, SWITCH_MUTEX_NESTED, session->pool);
	}

	if ((bug->flags & SMBF_THREAD_LOCK)) {
		bug->thread_id = switch_thread_self();
	}

	if (bug->callback) {
		switch_bool_t result = bug->callback(bug, bug->user_data, SWITCH_ABC_TYPE_INIT);
		if (result == SWITCH_FALSE) {
			switch_core_media_bug_destroy(bug);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error attaching BUG to %s\n",
							  switch_channel_get_name(session->channel));
			return SWITCH_STATUS_GENERR;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Attaching BUG to %s\n", switch_channel_get_name(session->channel));
	bug->ready = 1;
	switch_thread_rwlock_wrlock(session->bug_rwlock);
	bug->next = session->bugs;
	session->bugs = bug;
	switch_thread_rwlock_unlock(session->bug_rwlock);
	*new_bug = bug;

	if (switch_event_create(&event, SWITCH_EVENT_MEDIA_BUG_START) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Media-Bug-Function", "%s", bug->function);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Media-Bug-Target", "%s", bug->target);
		switch_channel_event_set_data(session->channel, event);
		switch_event_fire(&event);
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_core_media_bug_flush_all(switch_core_session_t *session)
{
	switch_media_bug_t *bp;

	if (session->bugs) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			switch_core_media_bug_flush(bp);
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove_all(switch_core_session_t *session)
{
	switch_media_bug_t *bp;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session->bugs) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if (bp->thread_id && bp->thread_id != switch_thread_self()) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "BUG is thread locked skipping.\n");
				continue;
			}

			if (bp->callback) {
				bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_CLOSE);
			}
			switch_core_media_bug_destroy(bp);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Removing BUG from %s\n", switch_channel_get_name(session->channel));
		}
		session->bugs = NULL;
		switch_thread_rwlock_unlock(session->bug_rwlock);
		status = SWITCH_STATUS_SUCCESS;
	}

	if (switch_core_codec_ready(&session->bug_codec)) {
		switch_core_codec_destroy(&session->bug_codec);
		memset(&session->bug_codec, 0, sizeof(session->bug_codec));
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_close(switch_media_bug_t **bug)
{
	switch_media_bug_t *bp = *bug;
	if (bp) {
		if (bp->thread_id && bp->thread_id != switch_thread_self()) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(*bug)), SWITCH_LOG_DEBUG, "BUG is thread locked skipping.\n");
			return SWITCH_STATUS_FALSE;
		}

		if (bp->callback) {
			bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_CLOSE);
			bp->ready = 0;
		}
		switch_core_media_bug_destroy(bp);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(*bug)), SWITCH_LOG_DEBUG, "Removing BUG from %s\n",
						  switch_channel_get_name(bp->session->channel));
		*bug = NULL;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove(switch_core_session_t *session, switch_media_bug_t **bug)
{
	switch_media_bug_t *bp = NULL, *last = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session->bugs) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if ((!bp->thread_id || bp->thread_id == switch_thread_self()) && bp->ready && bp == *bug) {
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
			status = switch_core_media_bug_close(&bp);
		}
	}

	if (!session->bugs && switch_core_codec_ready(&session->bug_codec)) {
		switch_core_codec_destroy(&session->bug_codec);
		memset(&session->bug_codec, 0, sizeof(session->bug_codec));
	}

	return status;
}


SWITCH_DECLARE(uint32_t) switch_core_media_bug_prune(switch_core_session_t *session)
{
	switch_media_bug_t *bp = NULL, *last = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int ttl = 0;


  top:

	if (session->bugs) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if (switch_core_media_bug_test_flag(bp, SMBF_PRUNE)) {
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
			status = switch_core_media_bug_close(&bp);
			ttl++;
			goto top;
		}
	}

	if (!session->bugs && switch_core_codec_ready(&session->bug_codec)) {
		switch_core_codec_destroy(&session->bug_codec);
		memset(&session->bug_codec, 0, sizeof(session->bug_codec));
	}

	return ttl;
}


SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove_callback(switch_core_session_t *session, switch_media_bug_callback_t callback)
{
	switch_media_bug_t *cur = NULL, *bp = NULL, *last = NULL;
	int total = 0;

	if (session->bugs) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);

		bp = session->bugs;
		while (bp) {
			cur = bp;
			bp = bp->next;

			if ((!cur->thread_id || cur->thread_id == switch_thread_self()) && cur->ready && cur->callback == callback) {
				if (last) {
					last->next = cur->next;
				} else {
					session->bugs = cur->next;
				}
				if (switch_core_media_bug_close(&cur) == SWITCH_STATUS_SUCCESS) {
					total++;
				}
			} else {
				last = cur;
			}
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
	}

	if (!session->bugs && switch_core_codec_ready(&session->bug_codec)) {
		switch_core_codec_destroy(&session->bug_codec);
		memset(&session->bug_codec, 0, sizeof(session->bug_codec));
	}

	return total ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
