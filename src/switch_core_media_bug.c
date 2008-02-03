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
 * switch_core_media_bug.c -- Main Core Library (Media Bugs)
 *
 */

#include "switch.h"
#include "private/switch_core_pvt.h"

static void switch_core_media_bug_destroy(switch_media_bug_t *bug)
{
	switch_buffer_destroy(&bug->raw_read_buffer);
	switch_buffer_destroy(&bug->raw_write_buffer);
}

SWITCH_DECLARE(uint32_t) switch_core_media_bug_test_flag(switch_media_bug_t *bug, uint32_t flag)
{
	return switch_test_flag(bug, flag);
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

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_read(switch_media_bug_t *bug, switch_frame_t *frame)
{
	uint32_t bytes = 0;
	uint32_t datalen = 0;
	int16_t *dp, *fp;
	uint32_t x;
	size_t rlen = 0;
	size_t wlen = 0;
	uint32_t blen;
	size_t rdlen = 0;
	uint32_t maxlen;
	switch_codec_t *read_codec = switch_core_session_get_read_codec(bug->session);

	if (bug->raw_read_buffer) {
		rlen = switch_buffer_inuse(bug->raw_read_buffer);
	}

	if (bug->raw_write_buffer) {
		wlen = switch_buffer_inuse(bug->raw_write_buffer);
	}

	if ((bug->raw_read_buffer && bug->raw_write_buffer) && (!rlen && !wlen)) {
		return SWITCH_STATUS_FALSE;
	}
	
	maxlen = SWITCH_RECOMMENDED_BUFFER_SIZE > frame->buflen ? frame->buflen : SWITCH_RECOMMENDED_BUFFER_SIZE;

	if ((rdlen = rlen > wlen ? wlen : rlen) > maxlen) {
		rdlen = maxlen;
	}

	if (!rdlen) {
		rdlen = maxlen;
	}

	frame->datalen = 0;

	if (rlen) {
		switch_mutex_lock(bug->read_mutex);

		frame->datalen = (uint32_t) switch_buffer_read(bug->raw_read_buffer, frame->data, rdlen);
		switch_mutex_unlock(bug->read_mutex);
	}

	if (wlen) {
		switch_mutex_lock(bug->write_mutex);
		datalen = (uint32_t) switch_buffer_read(bug->raw_write_buffer, bug->data, rdlen);
		switch_mutex_unlock(bug->write_mutex);
	}


	bytes = (datalen > frame->datalen) ? datalen : frame->datalen;
	switch_assert( bytes <= maxlen );
	
	if (bytes) {
		int16_t *tp = bug->tmp;
		
		dp = (int16_t *) bug->data;
		fp = (int16_t *) frame->data;
		rlen = frame->datalen / 2;
		wlen = datalen / 2;
		blen = bytes / 2;

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
					z += (int32_t) *(fp + x);
				}
				if (x < wlen) {
					z += (int32_t) *(dp + x);
				}
				switch_normalize_to_16bit(z);
				*(fp + x) = (int16_t) z;
			}
		}

		frame->datalen = bytes;
		frame->samples = bytes / sizeof(int16_t);
		frame->rate = read_codec->implementation->actual_samples_per_second;
		
		return SWITCH_STATUS_SUCCESS;
	}
	
	return SWITCH_STATUS_FALSE;
}

#define MAX_BUG_BUFFER 1024 * 512
SWITCH_DECLARE(switch_status_t) switch_core_media_bug_add(switch_core_session_t *session,
														  switch_media_bug_callback_t callback,
														  void *user_data, time_t stop_time, switch_media_bug_flag_t flags, switch_media_bug_t **new_bug)
{
	switch_media_bug_t *bug;//, *bp;
	switch_size_t bytes;

	if (flags & SMBF_WRITE_REPLACE) {
#if 0
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if (switch_test_flag(bp, SMBF_WRITE_REPLACE)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Only one bug of this type allowed!\n");
				switch_thread_rwlock_unlock(session->bug_rwlock);
				return SWITCH_STATUS_GENERR;
			}
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
#endif
	}

	if (flags & SMBF_READ_REPLACE) {
#if 0
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if (switch_test_flag(bp, SMBF_READ_REPLACE)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Only one bug of this type allowed!\n");
				switch_thread_rwlock_unlock(session->bug_rwlock);
				return SWITCH_STATUS_GENERR;
			}
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
#endif
	}

	if (!(bug = switch_core_session_alloc(session, sizeof(*bug)))) {
		return SWITCH_STATUS_MEMERR;
	}

	bug->callback = callback;
	bug->user_data = user_data;
	bug->session = session;
	bug->flags = flags;

	bug->stop_time = stop_time;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Attaching BUG to %s\n", switch_channel_get_name(session->channel));
	bytes = session->read_codec->implementation->bytes_per_frame;

	if (!bug->flags) {
		bug->flags = (SMBF_READ_STREAM | SMBF_WRITE_STREAM);
	}

	if (switch_test_flag(bug, SMBF_READ_STREAM) || switch_test_flag(bug, SMBF_READ_PING)) {
		switch_buffer_create_dynamic(&bug->raw_read_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, MAX_BUG_BUFFER);
		switch_mutex_init(&bug->read_mutex, SWITCH_MUTEX_NESTED, session->pool);
	}

	bytes = session->write_codec->implementation->bytes_per_frame;

	if (switch_test_flag(bug, SMBF_WRITE_STREAM)) {
		switch_buffer_create_dynamic(&bug->raw_write_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, MAX_BUG_BUFFER);
		switch_mutex_init(&bug->write_mutex, SWITCH_MUTEX_NESTED, session->pool);
	}

	if ((bug->flags & SMBF_THREAD_LOCK)) {
		bug->thread_id = switch_thread_self();
	}

	bug->ready = 1;
	switch_thread_rwlock_wrlock(session->bug_rwlock);
	bug->next = session->bugs;
	session->bugs = bug;
	switch_thread_rwlock_unlock(session->bug_rwlock);
	*new_bug = bug;

	if (bug->callback) {
		switch_bool_t result = bug->callback(bug, bug->user_data, SWITCH_ABC_TYPE_INIT);
		if (result == SWITCH_FALSE) {
			return switch_core_media_bug_remove(session, new_bug);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove_all(switch_core_session_t *session)
{
	switch_media_bug_t *bp;

	if (session->bugs) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if (bp->thread_id && bp->thread_id != switch_thread_self()) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "BUG is thread locked skipping.\n");
				continue;
			}

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

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_close(switch_media_bug_t **bug)
{
	switch_media_bug_t *bp = *bug;
	if (bp) {
		if (bp->thread_id && bp->thread_id != switch_thread_self()) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "BUG is thread locked skipping.\n");
			return SWITCH_STATUS_FALSE;
		}

		if (bp->callback) {
			bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_CLOSE);
			bp->ready = 0;
		}
		switch_core_media_bug_destroy(bp);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing BUG from %s\n", switch_channel_get_name(bp->session->channel));
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
			if (bp->thread_id && bp->thread_id != switch_thread_self()) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "BUG is thread locked skipping.\n");
				continue;
			}

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
		status = switch_core_media_bug_close(&bp);
	}
	
	return status;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
