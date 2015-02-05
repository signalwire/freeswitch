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
	if ((flag & SMBF_PRUNE)) {
		switch_clear_flag(bug, SMBF_LOCK);
	}
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

SWITCH_DECLARE(switch_frame_t *) switch_core_media_bug_get_native_read_frame(switch_media_bug_t *bug)
{
	return bug->native_read_frame;
}

SWITCH_DECLARE(switch_frame_t *) switch_core_media_bug_get_native_write_frame(switch_media_bug_t *bug)
{
	return bug->native_write_frame;
}

SWITCH_DECLARE(void) switch_core_media_bug_set_read_replace_frame(switch_media_bug_t *bug, switch_frame_t *frame)
{
	bug->read_replace_frame_out = frame;
}

SWITCH_DECLARE(void) switch_core_media_bug_set_read_demux_frame(switch_media_bug_t *bug, switch_frame_t *frame)
{
	bug->read_demux_frame = frame;
}

SWITCH_DECLARE(void *) switch_core_media_bug_get_user_data(switch_media_bug_t *bug)
{
	return bug->user_data;
}

SWITCH_DECLARE(void) switch_core_media_bug_flush(switch_media_bug_t *bug)
{

	bug->record_pre_buffer_count = 0;

	if (bug->raw_read_buffer) {
		switch_mutex_lock(bug->read_mutex);
		switch_buffer_zero(bug->raw_read_buffer);
		switch_mutex_unlock(bug->read_mutex);
	}

	if (bug->raw_write_buffer) {
		switch_mutex_lock(bug->write_mutex);
		switch_buffer_zero(bug->raw_write_buffer);
		switch_mutex_unlock(bug->write_mutex);
	}

	bug->record_frame_size = 0;
	bug->record_pre_buffer_count = 0;
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

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_set_pre_buffer_framecount(switch_media_bug_t *bug, uint32_t framecount)
{
	bug->record_pre_buffer_max = framecount;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_read(switch_media_bug_t *bug, switch_frame_t *frame, switch_bool_t fill)
{
	switch_size_t bytes = 0, datalen = 0;
	int16_t *dp, *fp;
	uint32_t x;
	size_t rlen = 0;
	size_t wlen = 0;
	uint32_t blen;
	switch_codec_implementation_t read_impl = { 0 };
	int16_t *tp;
	switch_size_t do_read = 0, do_write = 0, has_read = 0, has_write = 0, fill_read = 0, fill_write = 0;

	switch_core_session_get_read_impl(bug->session, &read_impl);

	bytes = read_impl.decoded_bytes_per_packet;

	if (frame->buflen < bytes) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_ERROR, "%s frame buffer too small!\n",
						  switch_channel_get_name(bug->session->channel));
		return SWITCH_STATUS_FALSE;
	}

	if ((!bug->raw_read_buffer && (!bug->raw_write_buffer || !switch_test_flag(bug, SMBF_WRITE_STREAM)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_ERROR, 
				"%s Buffer Error (raw_read_buffer=%p, raw_write_buffer=%p, read=%s, write=%s)\n",
			        switch_channel_get_name(bug->session->channel),
				(void *)bug->raw_read_buffer, (void *)bug->raw_write_buffer, 
				switch_test_flag(bug, SMBF_READ_STREAM) ? "yes" : "no",
				switch_test_flag(bug, SMBF_WRITE_STREAM) ? "yes" : "no");
		return SWITCH_STATUS_FALSE;
	}

	frame->flags = 0;
	frame->datalen = 0;

	if (switch_test_flag(bug, SMBF_READ_STREAM)) {
		has_read = 1;
		switch_mutex_lock(bug->read_mutex);
		do_read = switch_buffer_inuse(bug->raw_read_buffer);
		switch_mutex_unlock(bug->read_mutex);
	}

	if (switch_test_flag(bug, SMBF_WRITE_STREAM)) {
		has_write = 1;
		switch_mutex_lock(bug->write_mutex);
		do_write = switch_buffer_inuse(bug->raw_write_buffer);
		switch_mutex_unlock(bug->write_mutex);
	}


	if (bug->record_frame_size && bug->record_pre_buffer_max && (do_read || do_write) && bug->record_pre_buffer_count < bug->record_pre_buffer_max) {
		bug->record_pre_buffer_count++;
		return SWITCH_STATUS_FALSE;
	} else {
		uint32_t frame_size;
		switch_codec_implementation_t read_impl = { 0 };
		
		switch_core_session_get_read_impl(bug->session, &read_impl);
		frame_size = read_impl.decoded_bytes_per_packet;
		bug->record_frame_size = frame_size;
	}

	if (bug->record_frame_size && do_write > do_read && do_write > (bug->record_frame_size * 2)) {
		switch_mutex_lock(bug->write_mutex);
		switch_buffer_toss(bug->raw_write_buffer, bug->record_frame_size);
		do_write = switch_buffer_inuse(bug->raw_write_buffer);
		switch_mutex_unlock(bug->write_mutex);
	}



	if ((has_read && !do_read)) {
		fill_read = 1;
	}

	if ((has_write && !do_write)) {
		fill_write = 1;
	}


	if (bug->record_frame_size) {
		if ((do_read && do_read < bug->record_frame_size) || (do_write && do_write < bug->record_frame_size)) {
			return SWITCH_STATUS_FALSE;
		}

		if (do_read && do_read > bug->record_frame_size) {
			do_read = bug->record_frame_size;
		}

		if (do_write && do_write > bug->record_frame_size) {
			do_write = bug->record_frame_size;
		}
	}

	if ((fill_read && fill_write) || (fill && (fill_read || fill_write))) {
		return SWITCH_STATUS_FALSE;
	}

	if (do_read && do_read > SWITCH_RECOMMENDED_BUFFER_SIZE) {
		do_read = 1280;
	}

	if (do_write && do_write > SWITCH_RECOMMENDED_BUFFER_SIZE) {
		do_write = 1280;
	}
	
	if (do_read) {
		switch_mutex_lock(bug->read_mutex);
		frame->datalen = (uint32_t) switch_buffer_read(bug->raw_read_buffer, frame->data, do_read);
		if (frame->datalen != do_read) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_ERROR, "Framing Error Reading!\n");
			switch_core_media_bug_flush(bug);
			switch_mutex_unlock(bug->read_mutex);
			return SWITCH_STATUS_FALSE;
		}
		switch_mutex_unlock(bug->read_mutex);
	} else if (fill_read) {
		frame->datalen = (uint32_t)bytes;
		memset(frame->data, 255, frame->datalen);
	}

	if (do_write) {
		switch_assert(bug->raw_write_buffer);
		switch_mutex_lock(bug->write_mutex);
		datalen = (uint32_t) switch_buffer_read(bug->raw_write_buffer, bug->data, do_write);
		if (datalen != do_write) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_ERROR, "Framing Error Writing!\n");
			switch_core_media_bug_flush(bug);
			switch_mutex_unlock(bug->write_mutex);
			return SWITCH_STATUS_FALSE;
		}
		switch_mutex_unlock(bug->write_mutex);
	} else if (fill_write) {
		datalen = bytes;
		memset(bug->data, 255, datalen);
	}

	tp = bug->tmp;
	dp = (int16_t *) bug->data;
	fp = (int16_t *) frame->data;
	rlen = frame->datalen / 2;
	wlen = datalen / 2;
	blen = (uint32_t)(bytes / 2);

	if (switch_test_flag(bug, SMBF_STEREO)) {
		int16_t *left, *right;
		size_t left_len, right_len;
		if (switch_test_flag(bug, SMBF_STEREO_SWAP)) {
			left = dp; /* write stream */
			left_len = wlen;
			right = fp; /* read stream */
			right_len = rlen;
		} else {
			left = fp; /* read stream */
			left_len = rlen;
			right = dp; /* write stream */
			right_len = wlen;
		}
		for (x = 0; x < blen; x++) {
			if (x < left_len) {
				*(tp++) = *(left + x);
			} else {
				*(tp++) = 0;
			}
			if (x < right_len) {
				*(tp++) = *(right + x);
			} else {
				*(tp++) = 0;
			}
		}
		memcpy(frame->data, bug->tmp, bytes * 2);
	} else {
		for (x = 0; x < blen; x++) {
			int32_t w = 0, r = 0, z = 0;
			
			if (x < rlen) {
				r = (int32_t) * (fp + x);
			}

			if (x < wlen) {
				w = (int32_t) * (dp + x);
			}
			
			z = w + r;

			if (z > SWITCH_SMAX || z < SWITCH_SMIN) {
				if (r) z += (r/2);
				if (w) z += (w/2);
			}

			switch_normalize_to_16bit(z);

			*(fp + x) = (int16_t) z;
		}
	}

	frame->datalen = (uint32_t)bytes;
	frame->samples = (uint32_t)(bytes / sizeof(int16_t) / read_impl.number_of_channels);
	frame->rate = read_impl.actual_samples_per_second;
	frame->codec = NULL;

	if (switch_test_flag(bug, SMBF_STEREO)) {
		frame->datalen *= 2;
		frame->channels = 2;
	} else {
		frame->channels = read_impl.number_of_channels;
	}

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
	switch_media_bug_t *bug, *bp;
	switch_size_t bytes;
	switch_event_t *event;
	int tap_only = 1, punt = 0;

	const char *p;

	if (!zstr(function)) {
		if ((flags & SMBF_ONE_ONLY)) {
			switch_thread_rwlock_wrlock(session->bug_rwlock);
			for (bp = session->bugs; bp; bp = bp->next) {
				if (!zstr(bp->function) && !strcasecmp(function, bp->function)) {
					punt = 1;
					break;
				} 
			}
			switch_thread_rwlock_unlock(session->bug_rwlock);
		}
	}
	
	if (punt) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Only one bug of this type allowed!\n");
		return SWITCH_STATUS_GENERR;
	}


	if (!switch_channel_media_ready(session->channel)) {
		if (switch_channel_pre_answer(session->channel) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}



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

	switch_core_session_get_read_impl(session, &bug->read_impl);
	switch_core_session_get_write_impl(session, &bug->write_impl);

	if (function) {
		bug->function = switch_core_session_strdup(session, function);
	}

	if (target) {
		bug->target = switch_core_session_strdup(session, target);
	}
	
	bug->stop_time = stop_time;
	bytes = bug->read_impl.decoded_bytes_per_packet;

	if (!bug->flags) {
		bug->flags = (SMBF_READ_STREAM | SMBF_WRITE_STREAM);
	}

	if (switch_test_flag(bug, SMBF_READ_STREAM) || switch_test_flag(bug, SMBF_READ_PING)) {
		switch_buffer_create_dynamic(&bug->raw_read_buffer, bytes * SWITCH_BUFFER_BLOCK_FRAMES, bytes * SWITCH_BUFFER_START_FRAMES, MAX_BUG_BUFFER);
		switch_mutex_init(&bug->read_mutex, SWITCH_MUTEX_NESTED, session->pool);
	}

	bytes = bug->write_impl.decoded_bytes_per_packet;

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

	for(bp = session->bugs; bp; bp = bp->next) {
		if (bp->ready && !switch_test_flag(bp, SMBF_TAP_NATIVE_READ) && !switch_test_flag(bp, SMBF_TAP_NATIVE_WRITE)) {
			tap_only = 0;
		}	
	}

	switch_thread_rwlock_unlock(session->bug_rwlock);
	*new_bug = bug;


	if (tap_only) {
		switch_set_flag(session, SSF_MEDIA_BUG_TAP_ONLY);
	} else {
		switch_clear_flag(session, SSF_MEDIA_BUG_TAP_ONLY);
	}

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

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_transfer_recordings(switch_core_session_t *orig_session, switch_core_session_t *new_session)
{
	switch_media_bug_t *bp;
	char *list[100] = { 0 };
	int stop_times[100] = { 0 };
	int i = 0, x = 0;

	if (orig_session->bugs) {
		switch_channel_t *new_channel = switch_core_session_get_channel(new_session);
		switch_channel_t *orig_channel = switch_core_session_get_channel(orig_session);
		const char *save_append = switch_channel_get_variable(new_channel, "record_append");
		const char *save_stereo = switch_channel_get_variable(new_channel, "record_stereo");
		const char *orig_stereo = switch_channel_get_variable(orig_channel, "record_stereo");
		const char *new_stereo = orig_stereo;
		
		switch_thread_rwlock_wrlock(orig_session->bug_rwlock);
		switch_channel_set_variable(new_channel, "RECORD_MIN_SEC", "0");
		switch_channel_set_variable(new_channel, "record_append", "true");
		switch_channel_set_variable(new_channel, "record_stereo", new_stereo);

		for (bp = orig_session->bugs; bp; bp = bp->next) {
			if (!strcmp(bp->function, "session_record")) {
				list[x] = switch_core_session_strdup(new_session, bp->target);
				if (bp->stop_time > 0) {
					stop_times[x] = (int)(bp->stop_time - switch_epoch_time_now(NULL));
				}
				x++;
			}
		}

		switch_thread_rwlock_unlock(orig_session->bug_rwlock);

		for(i = 0; i < x; i++) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(orig_session), SWITCH_LOG_DEBUG, "Transfering %s from %s to %s\n", list[i],
							  switch_core_session_get_name(orig_session), switch_core_session_get_name(new_session));
			switch_ivr_stop_record_session(orig_session, list[i]);
			switch_ivr_record_session(new_session, list[i], stop_times[i], NULL);
		}

		switch_channel_set_variable(new_channel, "record_append", save_append);
		switch_channel_set_variable(new_channel, "record_stereo", save_stereo);

	}

	return x ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_core_media_bug_transfer_callback(switch_core_session_t *orig_session, switch_core_session_t *new_session, 
																		switch_media_bug_callback_t callback, void * (*user_data_dup_func) (switch_core_session_t *, void *))
{
	switch_media_bug_t *new_bug = NULL, *cur = NULL, *bp = NULL, *last = NULL;
	int total = 0;

	switch_thread_rwlock_wrlock(orig_session->bug_rwlock);
	bp = orig_session->bugs;
	while (bp) {
		cur = bp;
		bp = bp->next;
		
		if (cur->callback == callback) {
			if (last) {
				last->next = cur->next;
			} else {
				orig_session->bugs = cur->next;
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(orig_session), SWITCH_LOG_DEBUG, "Transfering %s from %s to %s\n", cur->target,
							  switch_core_session_get_name(orig_session), switch_core_session_get_name(new_session));

			switch_core_media_bug_add(new_session, cur->function, cur->target, cur->callback,
									  user_data_dup_func(new_session, cur->user_data),
									  cur->stop_time, cur->flags, &new_bug);
			switch_core_media_bug_destroy(cur);
			total++;
		} else {
			last = cur;
		}
	}

	if (!orig_session->bugs && switch_core_codec_ready(&orig_session->bug_codec)) {
		switch_core_codec_destroy(&orig_session->bug_codec);
	}

	switch_thread_rwlock_unlock(orig_session->bug_rwlock);


	return total ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_core_media_bug_pop(switch_core_session_t *orig_session, const char *function, switch_media_bug_t **pop)
{
	switch_media_bug_t *bp;

	if (orig_session->bugs) {
		switch_thread_rwlock_wrlock(orig_session->bug_rwlock);
		for (bp = orig_session->bugs; bp; bp = bp->next) {
			if (!strcmp(bp->function, function)) {
				switch_set_flag(bp, SMBF_LOCK);
				break;
			}
		}
		switch_thread_rwlock_unlock(orig_session->bug_rwlock);

		if (bp) {
			*pop = bp;
			return SWITCH_STATUS_SUCCESS;
		} else {
			*pop = NULL;
		}
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(uint32_t) switch_core_media_bug_count(switch_core_session_t *orig_session, const char *function)
{
	switch_media_bug_t *bp;
	uint32_t x = 0;

	if (orig_session->bugs) {
		switch_thread_rwlock_rdlock(orig_session->bug_rwlock);
		for (bp = orig_session->bugs; bp; bp = bp->next) {
			if (!switch_test_flag(bp, SMBF_PRUNE) && !switch_test_flag(bp, SMBF_LOCK) && !strcmp(bp->function, function)) {
				x++;
			}
		}
		switch_thread_rwlock_unlock(orig_session->bug_rwlock);
	}

	return x;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_exec_all(switch_core_session_t *orig_session, 
															   const char *function, switch_media_bug_exec_cb_t cb, void *user_data)
{
	switch_media_bug_t *bp;
	int x = 0;

	switch_assert(cb);

	if (orig_session->bugs) {
		switch_thread_rwlock_wrlock(orig_session->bug_rwlock);
		for (bp = orig_session->bugs; bp; bp = bp->next) {
			if (!switch_test_flag(bp, SMBF_PRUNE) && !switch_test_flag(bp, SMBF_LOCK) && !strcmp(bp->function, function)) {
				cb(bp, user_data);
				x++;
			}
		}
		switch_thread_rwlock_unlock(orig_session->bug_rwlock);
	}

	return x ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_enumerate(switch_core_session_t *session, switch_stream_handle_t *stream)
{
	switch_media_bug_t *bp;

	stream->write_function(stream, "<media-bugs>\n");

	if (session->bugs) {
        switch_thread_rwlock_rdlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			int thread_locked = (bp->thread_id && bp->thread_id == switch_thread_self());
			stream->write_function(stream, 
								   " <media-bug>\n"
								   "  <function>%s</function>\n"
								   "  <target>%s</target>\n"
								   "  <thread-locked>%d</thread-locked>\n"
								   " </media-bug>\n", 
								   bp->function, bp->target, thread_locked);

		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
	}

	stream->write_function(stream, "</media-bugs>\n");
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove_all_function(switch_core_session_t *session, const char *function)
{
	switch_media_bug_t *bp;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (session->bugs) {
		switch_thread_rwlock_wrlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			if ((bp->thread_id && bp->thread_id != switch_thread_self()) || switch_test_flag(bp, SMBF_LOCK)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "BUG is thread locked skipping.\n");
				continue;
			}
			
			if (!zstr(function) && strcmp(bp->function, function)) {
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
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_bug_close(switch_media_bug_t **bug)
{
	switch_media_bug_t *bp = *bug;
	if (bp) {
		if ((bp->thread_id && bp->thread_id != switch_thread_self()) || switch_test_flag(bp, SMBF_LOCK)) {
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
	switch_media_bug_t *bp = NULL, *bp2 = NULL, *last = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int tap_only = 0;

	if (switch_core_media_bug_test_flag(*bug, SMBF_LOCK)) {
		return status;
	}

	switch_thread_rwlock_wrlock(session->bug_rwlock);
	if (session->bugs) {
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
	}

	if (!session->bugs && switch_core_codec_ready(&session->bug_codec)) {
		switch_core_codec_destroy(&session->bug_codec);
	}

	if (session->bugs) {
		for(bp2 = session->bugs; bp2; bp2 = bp2->next) {
			if (bp2->ready && !switch_test_flag(bp2, SMBF_TAP_NATIVE_READ) && !switch_test_flag(bp2, SMBF_TAP_NATIVE_WRITE)) {
				tap_only = 0;
			}	
		}
	}
	
	if (tap_only) {
		switch_set_flag(session, SSF_MEDIA_BUG_TAP_ONLY);
	} else {
		switch_clear_flag(session, SSF_MEDIA_BUG_TAP_ONLY);
	}

	switch_thread_rwlock_unlock(session->bug_rwlock);

	if (bp) {
		status = switch_core_media_bug_close(&bp);
	}
	
	return status;
}


SWITCH_DECLARE(uint32_t) switch_core_media_bug_prune(switch_core_session_t *session)
{
	switch_media_bug_t *bp = NULL, *last = NULL;
	int ttl = 0;


  top:

	switch_thread_rwlock_wrlock(session->bug_rwlock);
	if (session->bugs) {
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
	}

	if (!session->bugs && switch_core_codec_ready(&session->bug_codec)) {
		switch_core_codec_destroy(&session->bug_codec);
	}

	switch_thread_rwlock_unlock(session->bug_rwlock);

	if (bp) {
		switch_clear_flag(bp, SMBF_LOCK);
		bp->thread_id = 0;
		switch_core_media_bug_close(&bp);
		ttl++;
		goto top;
	}
	
	return ttl;
}


SWITCH_DECLARE(switch_status_t) switch_core_media_bug_remove_callback(switch_core_session_t *session, switch_media_bug_callback_t callback)
{
	switch_media_bug_t *cur = NULL, *bp = NULL, *last = NULL;
	int total = 0;

	switch_thread_rwlock_wrlock(session->bug_rwlock);
	if (session->bugs) {
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
	}

	if (!session->bugs && switch_core_codec_ready(&session->bug_codec)) {
		switch_core_codec_destroy(&session->bug_codec);
	}

	switch_thread_rwlock_unlock(session->bug_rwlock);


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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
