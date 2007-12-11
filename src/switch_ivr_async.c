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
 * Bret McDanel <bret AT 0xdecafbad dot com>
 *
 * switch_ivr_async.c -- IVR Library (async operations)
 *
 */
#include <switch.h>

struct echo_helper {
	switch_core_session_t *session;
	int up;
};

static void *SWITCH_THREAD_FUNC echo_video_thread(switch_thread_t *thread, void *obj)
{
	struct echo_helper *eh = obj;
	switch_core_session_t *session = eh->session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	switch_frame_t *read_frame;

	eh->up = 1;	
	while(switch_channel_ready(channel) && switch_channel_get_state(channel) == CS_LOOPBACK) {
		status = switch_core_session_read_video_frame(session, &read_frame, -1, 0);
		
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}
		
		switch_core_session_write_video_frame(session, read_frame, -1, 0);
		
	}
	eh->up = 0;
	return NULL;
}

SWITCH_DECLARE(void) switch_ivr_session_echo(switch_core_session_t *session)
{
	switch_status_t status;
	switch_frame_t *read_frame;
	struct echo_helper eh = {0};
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	

	switch_channel_pre_answer(channel);

	if (switch_channel_test_flag(channel, CF_VIDEO)) {
		eh.session = session;
		switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, echo_video_thread, &eh, switch_core_session_get_pool(session));
	}

	while(switch_channel_ready(channel) && switch_channel_get_state(channel) == CS_LOOPBACK) {
		status = switch_core_session_read_frame(session, &read_frame, -1, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}
		switch_core_session_write_frame(session, read_frame, -1, 0);
	}

	if (eh.up) {
		while(eh.up) {
			switch_yield(1000);
		}
	}
}


typedef struct {
	switch_file_handle_t fh;
	int mux;
} displace_helper_t;

static switch_bool_t displace_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	displace_helper_t *dh = (displace_helper_t *) user_data;
	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = { 0 };

	frame.data = data;
	frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		if (dh) {
			switch_core_file_close(&dh->fh);
		}
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
		{
			switch_frame_t *frame = switch_core_media_bug_get_read_replace_frame(bug);
			if (dh && !dh->mux) {
				memset(frame->data, 255, frame->datalen);
			}
			switch_core_media_bug_set_read_replace_frame(bug, frame);
		}
		break;
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		if (dh) {
			switch_frame_t *frame = NULL;
			switch_size_t len;
			switch_status_t st;

			frame = switch_core_media_bug_get_write_replace_frame(bug);
			len = frame->samples;

			if (dh->mux) {
				int16_t buf[1024];
				int16_t *fp = frame->data;
				uint32_t x;

				st = switch_core_file_read(&dh->fh, buf, &len);
				
				for(x = 0; x < (uint32_t) len; x++) {
					int32_t mixed = fp[x] + buf[x];
					switch_normalize_to_16bit(mixed);
					fp[x] = (int16_t) mixed;
				}
			} else {
				st = switch_core_file_read(&dh->fh, frame->data, &len);
				frame->samples = (uint32_t) len;
				frame->datalen = frame->samples * 2;
			}

			if (st != SWITCH_STATUS_SUCCESS || len == 0) {
				return SWITCH_FALSE;
			}

			switch_core_media_bug_set_write_replace_frame(bug, frame);
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_stop_displace_session(switch_core_session_t *session, const char *file)
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_assert(channel != NULL);
	if ((bug = switch_channel_get_private(channel, file))) {
		switch_channel_set_private(channel, file, NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_displace_session(switch_core_session_t *session, char *file, uint32_t limit, const char *flags)
{
	switch_channel_t *channel;
	switch_codec_t *read_codec;
	switch_media_bug_t *bug;
	switch_status_t status;
	time_t to = 0;
	displace_helper_t *dh;


	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	if ((bug = switch_channel_get_private(channel, file))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Only 1 of the same file per channel please!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!(dh = switch_core_session_alloc(session, sizeof(*dh)))) {
		return SWITCH_STATUS_MEMERR;
	}



	read_codec = switch_core_session_get_read_codec(session);
	switch_assert(read_codec != NULL);

	dh->fh.channels = read_codec->implementation->number_of_channels;
	dh->fh.samplerate = read_codec->implementation->actual_samples_per_second;


	if (switch_core_file_open(&dh->fh,
							  file,
							  read_codec->implementation->number_of_channels,
							  read_codec->implementation->actual_samples_per_second,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
							  switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_core_session_reset(session);
		return SWITCH_STATUS_GENERR;
	}

	switch_channel_pre_answer(channel);

	if (limit) {
		to = time(NULL) + limit;
	}

	if (flags && strchr(flags, 'm')) {
		dh->mux++;
	}

	if ((status = switch_core_media_bug_add(session, displace_callback, dh, to, SMBF_WRITE_REPLACE | SMBF_READ_REPLACE, &bug)) != SWITCH_STATUS_SUCCESS) {
		switch_core_file_close(&dh->fh);
		return status;
	}

	switch_channel_set_private(channel, file, bug);

	return SWITCH_STATUS_SUCCESS;
}


static switch_bool_t record_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_file_handle_t *fh = (switch_file_handle_t *) user_data;
	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = { 0 };

	frame.data = data;
	frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		if (fh) {
			switch_core_file_close(fh);
		}
		break;
	case SWITCH_ABC_TYPE_READ:
		if (fh) {
			switch_size_t len;
			switch_core_session_t *session;
			switch_channel_t *channel;
			
			session = switch_core_media_bug_get_session(bug);
			switch_assert(session != NULL);
			channel = switch_core_session_get_channel(session);
			switch_assert(channel != NULL);
			
			if (switch_core_media_bug_read(bug, &frame) == SWITCH_STATUS_SUCCESS) {
				int doit = 1;
				if (!switch_channel_test_flag(channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bug, SMBF_RECORD_ANSWER_REQ)) {
					doit = 0;
				}

				if (doit) {
					len = (switch_size_t) frame.datalen / 2;
					switch_core_file_write(fh, frame.data, &len);
				}
			}
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_stop_record_session(switch_core_session_t *session, const char *file)
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_assert(channel != NULL);
	if ((bug = switch_channel_get_private(channel, file))) {
		switch_channel_set_private(channel, file, NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}


struct eavesdrop_pvt {
	switch_buffer_t *buffer;
	switch_mutex_t *mutex;
	switch_buffer_t *r_buffer;
	switch_mutex_t *r_mutex;
	switch_buffer_t *w_buffer;
	switch_mutex_t *w_mutex;
	uint32_t flags;
};


static switch_bool_t eavesdrop_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	struct eavesdrop_pvt *ep = (struct eavesdrop_pvt *) user_data;
	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = { 0 };
	
	frame.data = data;
	frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		break;
	case SWITCH_ABC_TYPE_WRITE:
		break;
	case SWITCH_ABC_TYPE_READ_PING:
		if (ep->buffer) {
			if (switch_core_media_bug_read(bug, &frame) == SWITCH_STATUS_SUCCESS) {
                switch_buffer_lock(ep->buffer);
                switch_buffer_zwrite(ep->buffer, frame.data, frame.datalen);
                switch_buffer_unlock(ep->buffer);
			}
		} else {
            return SWITCH_FALSE;
        }
		break;
	case SWITCH_ABC_TYPE_READ:
		break;

	case SWITCH_ABC_TYPE_READ_REPLACE:
		{
			if (switch_test_flag(ep, ED_MUX_READ)) {
				switch_frame_t *frame = switch_core_media_bug_get_read_replace_frame(bug);
			
				if (switch_buffer_inuse(ep->r_buffer) >= frame->datalen) {
					uint32_t bytes;
					switch_buffer_lock(ep->r_buffer);
					bytes = (uint32_t) switch_buffer_read(ep->r_buffer, data, frame->datalen);
			
					frame->datalen = switch_merge_sln(frame->data, frame->samples, (int16_t *)data, bytes / 2) * 2;
					frame->samples = frame->datalen / 2;
				
					switch_buffer_unlock(ep->r_buffer);
					switch_core_media_bug_set_read_replace_frame(bug, frame);
				}
			}
		}
		break;

	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		{
			if (switch_test_flag(ep, ED_MUX_WRITE)) {
				switch_frame_t *frame = switch_core_media_bug_get_write_replace_frame(bug);
			
				if (switch_buffer_inuse(ep->w_buffer) >= frame->datalen) {
					uint32_t bytes;
					switch_buffer_lock(ep->w_buffer);
					bytes = (uint32_t) switch_buffer_read(ep->w_buffer, data, frame->datalen);
			
					frame->datalen = switch_merge_sln(frame->data, frame->samples, (int16_t *)data, bytes / 2) * 2;
					frame->samples = frame->datalen / 2;
				
					switch_buffer_unlock(ep->w_buffer);
					switch_core_media_bug_set_write_replace_frame(bug, frame);
				}
			}
		}
		break;

	default:
		break;
	}

	return SWITCH_TRUE;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_eavesdrop_session(switch_core_session_t *session, const char *uuid, switch_eavesdrop_flag_t flags)
{
	switch_core_session_t *tsession;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_codec_t *read_codec = switch_core_session_get_read_codec(session);
	
	if ((tsession = switch_core_session_locate(uuid))) {
		struct eavesdrop_pvt ep = { 0 };
		switch_media_bug_t *bug = NULL;
		switch_channel_t *tchannel = switch_core_session_get_channel(tsession);
		switch_frame_t *read_frame, write_frame = { 0 };
		switch_codec_t codec = {0};
		int16_t buf[1024];
		switch_codec_t *tread_codec = switch_core_session_get_read_codec(tsession);
		
		
		switch_channel_pre_answer(channel);
		
		if (switch_core_codec_init(&codec,
								   "L16",
								   NULL, 
								   tread_codec->implementation->actual_samples_per_second,
								   tread_codec->implementation->microseconds_per_frame / 1000,
								   tread_codec->implementation->number_of_channels, 
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, 
								   NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot init codec\n");
			return status;
		}
		
		switch_core_session_set_read_codec(session, &codec);
		write_frame.codec = &codec;
		write_frame.data = buf;
		write_frame.buflen = sizeof(buf);
		write_frame.rate = read_codec->implementation->actual_samples_per_second;
		
		ep.flags = flags;
		switch_mutex_init(&ep.mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(tsession));
		switch_buffer_create_dynamic(&ep.buffer, 1024, 2048, 2048);
		switch_buffer_add_mutex(ep.buffer, ep.mutex);

		switch_mutex_init(&ep.w_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(tsession));
		switch_buffer_create_dynamic(&ep.w_buffer, 1024, 2048, 2048);
		switch_buffer_add_mutex(ep.w_buffer, ep.w_mutex);

		switch_mutex_init(&ep.r_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(tsession));
		switch_buffer_create_dynamic(&ep.r_buffer, 1024, 2048, 2048);
		switch_buffer_add_mutex(ep.r_buffer, ep.r_mutex);

		
		if (switch_core_media_bug_add(tsession, eavesdrop_callback, &ep, 0, 
									  SMBF_READ_STREAM | SMBF_WRITE_STREAM | SMBF_READ_REPLACE | SMBF_WRITE_REPLACE | SMBF_READ_PING, 
									  &bug) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot attach bug\n");
			goto end;
		}
		
        while(switch_channel_ready(tchannel) && switch_channel_ready(channel)) {
			uint32_t len = sizeof(buf);
			switch_event_t *event = NULL;
			char *fcommand = NULL;
			
			status = switch_core_session_read_frame(session, &read_frame, 1000, 0);
			
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				goto end;
			}

			if (switch_core_session_dequeue_event(session, &event) == SWITCH_STATUS_SUCCESS) {
				char *command = switch_event_get_header(event, "eavesdrop-command");
				if (command) {
					fcommand = command;
				}
				switch_event_destroy(&event);
			}

			if ((flags & ED_DTMF) && switch_channel_has_dtmf(channel)) {
				char dtmf[128] = "";
				switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
				fcommand = dtmf;
			}

			if (fcommand) {
				char *d;
				for(d = fcommand; *d; d++) {
					int z = 1;
					
					switch (*d) {
					case '1':
						switch_set_flag((&ep), ED_MUX_READ);
						switch_clear_flag((&ep), ED_MUX_WRITE);
						break;
					case '2':
						switch_set_flag((&ep), ED_MUX_WRITE);
						switch_clear_flag((&ep), ED_MUX_READ);
						break;
					case '3':
						switch_set_flag((&ep), ED_MUX_READ);
						switch_set_flag((&ep), ED_MUX_WRITE);
						break;
					case '0':
						switch_clear_flag((&ep), ED_MUX_READ);
						switch_clear_flag((&ep), ED_MUX_WRITE);
						break;
					default:
						z = 0;
						break;

					}
					
					if (z) {
						switch_buffer_lock(ep.r_buffer);
						switch_buffer_zero(ep.r_buffer);
						switch_buffer_unlock(ep.r_buffer);
						
						switch_buffer_lock(ep.w_buffer);
						switch_buffer_zero(ep.w_buffer);
						switch_buffer_unlock(ep.w_buffer);
					}
				}
			}

			if (!switch_test_flag(read_frame, SFF_CNG)) {
				switch_buffer_lock(ep.r_buffer);
				switch_buffer_zwrite(ep.r_buffer, read_frame->data, read_frame->datalen);
				switch_buffer_unlock(ep.r_buffer);

				switch_buffer_lock(ep.w_buffer);
				switch_buffer_zwrite(ep.w_buffer, read_frame->data, read_frame->datalen);
				switch_buffer_unlock(ep.w_buffer);
			}


			if (len > tread_codec->implementation->samples_per_frame * 2) {
				len = tread_codec->implementation->samples_per_frame * 2;
			}
			
            if (switch_buffer_inuse(ep.buffer) >= len) {
                switch_buffer_lock(ep.buffer);				
                write_frame.datalen = (uint32_t)switch_buffer_read(ep.buffer, buf, len);
				write_frame.samples = write_frame.datalen / 2;
				if (switch_core_session_write_frame(session, &write_frame, 1000, 0) != SWITCH_STATUS_SUCCESS) {
					goto end;
				}
                switch_buffer_unlock(ep.buffer);
            }
        }


    end:

		switch_core_codec_destroy(&codec);

        if (bug) {
            switch_core_media_bug_remove(tsession, &bug);
        }

        if (ep.buffer) {
            switch_buffer_destroy(&ep.buffer);
        }

        if (ep.r_buffer) {
            switch_buffer_destroy(&ep.r_buffer);
        }

        if (ep.w_buffer) {
            switch_buffer_destroy(&ep.w_buffer);
        }

        switch_core_session_rwunlock(tsession);
		status = SWITCH_STATUS_SUCCESS;

		switch_core_session_set_read_codec(session, read_codec);
		switch_core_session_reset(session);
    }
	
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_record_session(switch_core_session_t *session, char *file, uint32_t limit, switch_file_handle_t *fh)
{
	switch_channel_t *channel;
	switch_codec_t *read_codec;
	const char *p;
	const char *vval;
	switch_media_bug_t *bug;
	switch_status_t status;
	time_t to = 0;
	switch_media_bug_flag_t flags = SMBF_READ_STREAM | SMBF_WRITE_STREAM;
	uint8_t channels;
	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	if ((bug = switch_channel_get_private(channel, file))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Only 1 of the same file per channel please!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!fh) {
		if (!(fh = switch_core_session_alloc(session, sizeof(*fh)))) {
			return SWITCH_STATUS_MEMERR;
		}
	}

	read_codec = switch_core_session_get_read_codec(session);
	switch_assert(read_codec != NULL);

	channels = read_codec->implementation->number_of_channels;

	if ((p = switch_channel_get_variable(channel, "RECORD_STEREO")) && switch_true(p)) {
		flags |= SMBF_STEREO;
		channels = 2;
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_ANSWER_REQ")) && switch_true(p)) {
		flags |= SMBF_RECORD_ANSWER_REQ;
	}
	
	fh->channels = channels;
	fh->samplerate = read_codec->implementation->actual_samples_per_second;


	if (switch_core_file_open(fh,
							  file,
							  channels,
							  read_codec->implementation->actual_samples_per_second,
							  SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", file);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_core_session_reset(session);
		return SWITCH_STATUS_GENERR;
	}

	switch_channel_pre_answer(channel);

	if ((p = switch_channel_get_variable(channel, "RECORD_TITLE"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_TITLE, vval);
		switch_channel_set_variable(channel, "RECORD_TITLE", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_COPYRIGHT"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_COPYRIGHT, vval);
		switch_channel_set_variable(channel, "RECORD_COPYRIGHT", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_SOFTWARE"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_SOFTWARE, vval);
		switch_channel_set_variable(channel, "RECORD_SOFTWARE", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_ARTIST"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_ARTIST, vval);
		switch_channel_set_variable(channel, "RECORD_ARTIST", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_COMMENT"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_COMMENT, vval);
		switch_channel_set_variable(channel, "RECORD_COMMENT", NULL);
	}

	if ((p = switch_channel_get_variable(channel, "RECORD_DATE"))) {
		vval = (const char *) switch_core_session_strdup(session, p);
		switch_core_file_set_string(fh, SWITCH_AUDIO_COL_STR_DATE, vval);
		switch_channel_set_variable(channel, "RECORD_DATE", NULL);
	}

	if (limit) {
		to = time(NULL) + limit;
	}
	
	if ((status = switch_core_media_bug_add(session, record_callback, fh, to, flags, &bug)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error adding media bug for file %s\n", file);
		switch_core_file_close(fh);
		return status;
	}

	switch_channel_set_private(channel, file, bug);

	return SWITCH_STATUS_SUCCESS;
}

typedef struct {
	switch_core_session_t *session;
	teletone_dtmf_detect_state_t dtmf_detect;
} switch_inband_dtmf_t;

static switch_bool_t inband_dtmf_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_inband_dtmf_t *pvt = (switch_inband_dtmf_t *) user_data;
	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = { 0 };
	char digit_str[80];
	switch_channel_t *channel = switch_core_session_get_channel(pvt->session);

	switch_assert(channel != NULL);
	frame.data = data;
	frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		break;
	case SWITCH_ABC_TYPE_READ:
		if (switch_core_media_bug_read(bug, &frame) == SWITCH_STATUS_SUCCESS) {
			teletone_dtmf_detect(&pvt->dtmf_detect, frame.data, frame.samples);
			teletone_dtmf_get(&pvt->dtmf_detect, digit_str, sizeof(digit_str));
			if (digit_str[0]) {
				switch_channel_queue_dtmf(channel, digit_str);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "DTMF DETECTED: [%s]\n", digit_str);
			}
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_stop_inband_dtmf_session(switch_core_session_t *session)
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_assert(channel != NULL);
	if ((bug = switch_channel_get_private(channel, "dtmf"))) {
		switch_channel_set_private(channel, "dtmf", NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_inband_dtmf_session(switch_core_session_t *session)
{
	switch_channel_t *channel;
	switch_codec_t *read_codec;
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_inband_dtmf_t *pvt;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	read_codec = switch_core_session_get_read_codec(session);
	switch_assert(read_codec != NULL);

	if (!(pvt = switch_core_session_alloc(session, sizeof(*pvt)))) {
		return SWITCH_STATUS_MEMERR;
	}

	teletone_dtmf_detect_init(&pvt->dtmf_detect, read_codec->implementation->actual_samples_per_second);

	pvt->session = session;

	switch_channel_pre_answer(channel);

	if ((status = switch_core_media_bug_add(session, inband_dtmf_callback, pvt, 0, SMBF_READ_STREAM, &bug)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	switch_channel_set_private(channel, "dtmf", bug);

	return SWITCH_STATUS_SUCCESS;
}


typedef struct {
	switch_core_session_t *session;
	teletone_generation_session_t ts;
    switch_buffer_t *audio_buffer;
	switch_mutex_t *mutex;
	int read;
} switch_inband_dtmf_generate_t;

static int teletone_dtmf_generate_handler(teletone_generation_session_t * ts, teletone_tone_map_t * map)
{
    switch_buffer_t *audio_buffer = ts->user_data;
    int wrote;

    if (!audio_buffer) {
        return -1;
    }

    wrote = teletone_mux_tones(ts, map);
    switch_buffer_write(audio_buffer, ts->buffer, wrote * 2);

    return 0;
}


static switch_status_t generate_on_dtmf(switch_core_session_t *session, const char *dtmf)
{

    switch_media_bug_t *bug;
    switch_channel_t *channel = switch_core_session_get_channel(session);
    
    if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "dtmf_generate"))) {
        switch_inband_dtmf_generate_t *pvt = (switch_inband_dtmf_generate_t *) switch_core_media_bug_get_user_data(bug);
        
        if (pvt) {
			switch_mutex_lock(pvt->mutex);
			teletone_run(&pvt->ts, (char *)dtmf);
			switch_mutex_unlock(pvt->mutex);
			return SWITCH_STATUS_FALSE;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_bool_t inband_dtmf_generate_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_inband_dtmf_generate_t *pvt = (switch_inband_dtmf_generate_t *) user_data;
	switch_frame_t *frame;
	switch_codec_t *read_codec;

	read_codec = switch_core_session_get_read_codec(pvt->session);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		{
			switch_buffer_create_dynamic(&pvt->audio_buffer, 512, 1024, 0);
			teletone_init_session(&pvt->ts, 0, teletone_dtmf_generate_handler, pvt->audio_buffer);
			pvt->ts.rate = read_codec->implementation->actual_samples_per_second;
			pvt->ts.channels = 1;
			switch_mutex_init(&pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(pvt->session));
			switch_core_event_hook_add_recv_dtmf(pvt->session, generate_on_dtmf);
		}
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		{
			switch_buffer_destroy(&pvt->audio_buffer);
			teletone_destroy_session(&pvt->ts);
			switch_core_event_hook_remove_recv_dtmf(pvt->session, generate_on_dtmf);
		}
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		{
			switch_size_t bytes;
			switch_mutex_lock(pvt->mutex);
			if (pvt->read) {
				frame = switch_core_media_bug_get_read_replace_frame(bug);
			} else {
				frame = switch_core_media_bug_get_write_replace_frame(bug);
			}
			if (switch_buffer_inuse(pvt->audio_buffer) && (bytes = switch_buffer_read(pvt->audio_buffer, frame->data, frame->datalen))) {
				if (bytes < frame->datalen) {
					switch_byte_t *dp = frame->data;
					memset(dp + bytes, 0, frame->datalen - bytes);
				}
			}
			if (pvt->read) {
				switch_core_media_bug_set_read_replace_frame(bug, frame);
			} else {
				switch_core_media_bug_set_write_replace_frame(bug, frame);
			}
			switch_mutex_unlock(pvt->mutex);
		}
		break;
	default:
		break;
	}

	return SWITCH_TRUE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_stop_inband_dtmf_generate_session(switch_core_session_t *session)
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_assert(channel != NULL);
	if ((bug = switch_channel_get_private(channel, "dtmf_generate"))) {
		switch_channel_set_private(channel, "dtmf_generate", NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_inband_dtmf_generate_session(switch_core_session_t *session, switch_bool_t read_stream)
{
	switch_channel_t *channel;
	switch_codec_t *read_codec;
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_inband_dtmf_generate_t *pvt;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	read_codec = switch_core_session_get_read_codec(session);
	switch_assert(read_codec != NULL);

	if (!(pvt = switch_core_session_alloc(session, sizeof(*pvt)))) {
		return SWITCH_STATUS_MEMERR;
	}

	pvt->session = session;
	pvt->read = !!read_stream;
	switch_channel_pre_answer(channel);

	if ((status = switch_core_media_bug_add(session, inband_dtmf_generate_callback, pvt, 0, 
											pvt->read ? SMBF_READ_REPLACE : SMBF_WRITE_REPLACE, &bug)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	switch_channel_set_private(channel, "dtmf_generate", bug);

	return SWITCH_STATUS_SUCCESS;
}


#define MAX_TONES 16
typedef struct {
	teletone_multi_tone_t mt;
	char *app;
	char *data;
	char *key;
	teletone_tone_map_t map;
	int up;
} switch_tone_detect_t;


typedef struct {
	switch_tone_detect_t list[MAX_TONES+1];
	int index;
	switch_media_bug_t *bug;
	switch_core_session_t *session;
} switch_tone_container_t;

static switch_bool_t tone_detect_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_tone_container_t *cont = (switch_tone_container_t *) user_data;
	switch_frame_t *frame = NULL;
	int i = 0;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
		frame = switch_core_media_bug_get_read_replace_frame(bug);
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		{

			if (!frame) {
				frame = switch_core_media_bug_get_write_replace_frame(bug);
			}
			
			for (i = 0 ; i < cont->index; cont++) {
				if (cont->list[i].up && teletone_multi_tone_detect(&cont->list[i].mt, frame->data, frame->samples)) {
					switch_event_t *event;
					
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "TONE %s DETECTED\n", cont->list[i].key);
					cont->list[i].up = 0;

					if (cont->list[i].app) {
						if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "call-command", "execute");
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-name", "%s", cont->list[i].app);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-arg", "%s", cont->list[i].data);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "lead-frames", "%d", 5);
							switch_core_session_queue_private_event(cont->session, &event);
						}
					}

					if (switch_event_create(&event, SWITCH_EVENT_DETECTED_TONE) == SWITCH_STATUS_SUCCESS) {
						switch_event_t *dup;
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detected-Tone", "%s", cont->list[i].key);
						
						if (switch_event_dup(&dup, event) == SWITCH_STATUS_SUCCESS) {
							switch_event_fire(&dup);
						}
	    
						if (switch_core_session_queue_event(cont->session, &event) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Event queue failed!\n");
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
							switch_event_fire(&event);
						}
					}
				}
			}
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}
	return SWITCH_TRUE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_stop_tone_detect_session(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_tone_container_t *cont;
	
	switch_assert(channel != NULL);
	if ((cont = switch_channel_get_private(channel, "_tone_detect_"))) {
		switch_channel_set_private(channel, "_tone_detect_", NULL);
		switch_core_media_bug_remove(session, &cont->bug);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
	
}

SWITCH_DECLARE(switch_status_t) switch_ivr_tone_detect_session(switch_core_session_t *session, 
															   const char *key, const char *tone_spec,
															   const char *flags, time_t timeout,
															   const char *app, const char *data)
{
  switch_channel_t *channel;
  switch_codec_t *read_codec;
  switch_status_t status;
  switch_tone_container_t *cont = NULL;
  char *p, *next;
  int i = 0, ok = 0;

  switch_media_bug_flag_t bflags = 0;

  channel = switch_core_session_get_channel(session);
  switch_assert(channel != NULL);

  read_codec = switch_core_session_get_read_codec(session);
  switch_assert(read_codec != NULL);
  
  if (switch_strlen_zero(key)) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Key Specified!\n");
	  return SWITCH_STATUS_FALSE;
  }
  
  if ((cont = switch_channel_get_private(channel, "_tone_detect_"))) {
	  if (cont->index >= MAX_TONES) {
		  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max Tones Reached!\n");
		  return SWITCH_STATUS_FALSE;
	  }

	  for(i = 0; i < cont->index; i++) {
		  if (!switch_strlen_zero(cont->list[cont->index].key) && !strcasecmp(key, cont->list[cont->index].key)) {
			  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Renabling %s\n", key);
			  cont->list[cont->index].up = 1;
			  teletone_multi_tone_init(&cont->list[i].mt, &cont->list[i].map);
			  return SWITCH_STATUS_SUCCESS;
		  }
	  }
  }

  if (switch_strlen_zero(tone_spec)) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Spec Specified!\n");
	  return SWITCH_STATUS_FALSE;
  }

  if (!cont && !(cont = switch_core_session_alloc(session, sizeof(*cont)))) {
	  return SWITCH_STATUS_MEMERR;
  }

  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding tone spec %s index %d\n", tone_spec, cont->index);

  i = 0;
  p = (char *) tone_spec;

  do {
	  teletone_process_t this;
	  next = strchr(p, ',');
	  while(*p == ' ') p++;
	  if ((this = (teletone_process_t) atof(p))) {
		  ok++;
		  cont->list[cont->index].map.freqs[i++] = this;
	  }
	  if (next) {
		  p = next + 1;
	  }
  } while (next);
  cont->list[cont->index].map.freqs[i++] = 0;

  if (!ok) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid tone spec!\n");
	  return SWITCH_STATUS_FALSE;

  }

  cont->list[cont->index].key = switch_core_session_strdup(session, key);

  if (app) {
	  cont->list[cont->index].app = switch_core_session_strdup(session, app);
  }
  
  if (data) {
	  cont->list[cont->index].data = switch_core_session_strdup(session, data);
  }

  cont->list[cont->index].up = 1;
  cont->list[cont->index].mt.sample_rate = read_codec->implementation->actual_samples_per_second;
  teletone_multi_tone_init(&cont->list[cont->index].mt, &cont->list[cont->index].map);
  cont->session = session;
  
  switch_channel_pre_answer(channel);
  
  if (switch_strlen_zero(flags)) {
	  bflags = SMBF_READ_REPLACE;
  } else {
	  if (strchr(flags, 'r')) {
		  bflags |= SMBF_READ_REPLACE;
	  } else if (strchr(flags, 'w')) {
		  bflags |= SMBF_WRITE_REPLACE;
	  }
  }


  if ((status = switch_core_media_bug_add(session, tone_detect_callback, cont, timeout, bflags, &cont->bug)) != SWITCH_STATUS_SUCCESS) {
	  return status;
  }

  switch_channel_set_private(channel, "_tone_detect_", cont);
  cont->index++;
  
  return SWITCH_STATUS_SUCCESS;
}


struct speech_thread_handle {
	switch_core_session_t *session;
	switch_asr_handle_t *ah;
	switch_media_bug_t *bug;
	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC speech_thread(switch_thread_t * thread, void *obj)
{
	struct speech_thread_handle *sth = (struct speech_thread_handle *) obj;
	switch_channel_t *channel = switch_core_session_get_channel(sth->session);
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
	switch_status_t status;

	switch_thread_cond_create(&sth->cond, sth->pool);
	switch_mutex_init(&sth->mutex, SWITCH_MUTEX_NESTED, sth->pool);


	switch_core_session_read_lock(sth->session);
	switch_mutex_lock(sth->mutex);

	while (switch_channel_ready(channel) && !switch_test_flag(sth->ah, SWITCH_ASR_FLAG_CLOSED)) {
		char *xmlstr = NULL;

		switch_thread_cond_wait(sth->cond, sth->mutex);
		if (switch_core_asr_check_results(sth->ah, &flags) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *event;

			status = switch_core_asr_get_results(sth->ah, &xmlstr, &flags);

			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				goto done;
			}


			if (switch_event_create(&event, SWITCH_EVENT_DETECTED_SPEECH) == SWITCH_STATUS_SUCCESS) {
				if (status == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Speech-Type", "detected-speech");
					switch_event_add_body(event, "%s", xmlstr);
				} else {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Speech-Type", "begin-speaking");
				}

				if (switch_test_flag(sth->ah, SWITCH_ASR_FLAG_FIRE_EVENTS)) {
					switch_event_t *dup;

					if (switch_event_dup(&dup, event) == SWITCH_STATUS_SUCCESS) {
						switch_event_fire(&dup);
					}

				}

				if (switch_core_session_queue_event(sth->session, &event) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Event queue failed!\n");
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
					switch_event_fire(&event);
				}
			}

			switch_safe_free(xmlstr);
		}
	}
  done:

	switch_mutex_unlock(sth->mutex);
	switch_core_session_rwunlock(sth->session);

	return NULL;
}

static switch_bool_t speech_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	struct speech_thread_handle *sth = (struct speech_thread_handle *) user_data;
	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = { 0 };
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;

	frame.data = data;
	frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:{
			switch_thread_t *thread;
			switch_threadattr_t *thd_attr = NULL;

			switch_threadattr_create(&thd_attr, sth->pool);
			switch_threadattr_detach_set(thd_attr, 1);
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
			switch_thread_create(&thread, thd_attr, speech_thread, sth, sth->pool);

		}
		break;
	case SWITCH_ABC_TYPE_CLOSE:{
			switch_core_asr_close(sth->ah, &flags);
			switch_mutex_lock(sth->mutex);
			switch_thread_cond_signal(sth->cond);
			switch_mutex_unlock(sth->mutex);
		}
		break;
	case SWITCH_ABC_TYPE_READ:
		if (sth->ah) {
			if (switch_core_media_bug_read(bug, &frame) == SWITCH_STATUS_SUCCESS) {
				if (switch_core_asr_feed(sth->ah, frame.data, frame.datalen, &flags) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error Feeding Data\n");
					return SWITCH_FALSE;
				}
				if (switch_core_asr_check_results(sth->ah, &flags) == SWITCH_STATUS_SUCCESS) {
					switch_mutex_lock(sth->mutex);
					switch_thread_cond_signal(sth->cond);
					switch_mutex_unlock(sth->mutex);
				}
			}
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_stop_detect_speech(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct speech_thread_handle *sth;

	switch_assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		switch_channel_set_private(channel, SWITCH_SPEECH_KEY, NULL);
		switch_core_media_bug_remove(session, &sth->bug);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}



SWITCH_DECLARE(switch_status_t) switch_ivr_pause_detect_speech(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct speech_thread_handle *sth;

	switch_assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		switch_core_asr_pause(sth->ah);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_resume_detect_speech(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct speech_thread_handle *sth;

	switch_assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		switch_core_asr_resume(sth->ah);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}


SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_load_grammar(switch_core_session_t *session, char *grammar, char *path)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
	struct speech_thread_handle *sth;

	switch_assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		if (switch_core_asr_load_grammar(sth->ah, grammar, path) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error loading Grammar\n");
			switch_core_asr_close(sth->ah, &flags);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_FALSE;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech_unload_grammar(switch_core_session_t *session, const char *grammar)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
	struct speech_thread_handle *sth;

	switch_assert(channel != NULL);
	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		if (switch_core_asr_unload_grammar(sth->ah, grammar) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error unloading Grammar\n");
			switch_core_asr_close(sth->ah, &flags);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_FALSE;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_detect_speech(switch_core_session_t *session,
														 const char *mod_name,
														 const char *grammar,
														 const char *path,
														 const char *dest,
														 switch_asr_handle_t *ah)
{
	switch_channel_t *channel;
	switch_codec_t *read_codec;
	switch_status_t status;
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
	struct speech_thread_handle *sth;
	const char *val;

	if (!ah) {
		if (!(ah = switch_core_session_alloc(session, sizeof(*ah)))) {
			return SWITCH_STATUS_MEMERR;
		}
	}

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	read_codec = switch_core_session_get_read_codec(session);
	switch_assert(read_codec != NULL);


	if ((val = switch_channel_get_variable(channel, "fire_asr_events"))) {
		switch_set_flag(ah, SWITCH_ASR_FLAG_FIRE_EVENTS);
	}

	if ((sth = switch_channel_get_private(channel, SWITCH_SPEECH_KEY))) {
		if (switch_core_asr_load_grammar(sth->ah, grammar, path) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error loading Grammar\n");
			switch_core_asr_close(sth->ah, &flags);
			return SWITCH_STATUS_FALSE;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_core_asr_open(ah,
							 mod_name,
							 "L16",
							 read_codec->implementation->actual_samples_per_second, dest, &flags,
							 switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {

		if (switch_core_asr_load_grammar(ah, grammar, path) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error loading Grammar\n");
			switch_core_asr_close(ah, &flags);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}


	sth = switch_core_session_alloc(session, sizeof(*sth));
	sth->pool = switch_core_session_get_pool(session);
	sth->session = session;
	sth->ah = ah;

	if ((status = switch_core_media_bug_add(session, speech_callback, sth, 0, SMBF_READ_STREAM, &sth->bug)) != SWITCH_STATUS_SUCCESS) {
		switch_core_asr_close(ah, &flags);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return status;
	}

	switch_channel_set_private(channel, SWITCH_SPEECH_KEY, sth);

	return SWITCH_STATUS_SUCCESS;
}


struct hangup_helper {
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_bool_t bleg;
	switch_call_cause_t cause;
};

SWITCH_STANDARD_SCHED_FUNC(sch_hangup_callback)
{
	struct hangup_helper *helper;
	switch_core_session_t *session, *other_session;
	const char *other_uuid;

	switch_assert(task);

	helper = (struct hangup_helper *) task->cmd_arg;

	if ((session = switch_core_session_locate(helper->uuid_str))) {
		switch_channel_t *channel = switch_core_session_get_channel(session);

		if (helper->bleg) {
			if ((other_uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE)) && (other_session = switch_core_session_locate(other_uuid))) {
				switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
				switch_channel_hangup(other_channel, helper->cause);
				switch_core_session_rwunlock(other_session);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No channel to hangup\n");
			}
		} else {
			switch_channel_hangup(channel, helper->cause);
		}

		switch_core_session_rwunlock(session);
	}
}

SWITCH_DECLARE(uint32_t) switch_ivr_schedule_hangup(time_t runtime, const char *uuid, switch_call_cause_t cause, switch_bool_t bleg)
{
	struct hangup_helper *helper;
	size_t len = sizeof(*helper);

	switch_zmalloc(helper, len);

	switch_copy_string(helper->uuid_str, uuid, sizeof(helper->uuid_str));
	helper->cause = cause;
	helper->bleg = bleg;

	return switch_scheduler_add_task(runtime, sch_hangup_callback, (char *) __SWITCH_FUNC__, uuid, 0, helper, SSHF_FREE_ARG);
}

struct transfer_helper {
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *extension;
	char *dialplan;
	char *context;
};

SWITCH_STANDARD_SCHED_FUNC(sch_transfer_callback)
{
	struct transfer_helper *helper;
	switch_core_session_t *session;

	switch_assert(task);

	helper = (struct transfer_helper *) task->cmd_arg;

	if ((session = switch_core_session_locate(helper->uuid_str))) {
		switch_ivr_session_transfer(session, helper->extension, helper->dialplan, helper->context);
		switch_core_session_rwunlock(session);
	}

}

SWITCH_DECLARE(uint32_t) switch_ivr_schedule_transfer(time_t runtime, const char *uuid, char *extension, char *dialplan, char *context)
{
	struct transfer_helper *helper;
	size_t len = sizeof(*helper);
	char *cur = NULL;

	if (extension) {
		len += strlen(extension) + 1;
	}

	if (dialplan) {
		len += strlen(dialplan) + 1;
	}

	if (context) {
		len += strlen(context) + 1;
	}

	switch_zmalloc(cur, len);
	helper = (struct transfer_helper *)cur;

	switch_copy_string(helper->uuid_str, uuid, sizeof(helper->uuid_str));

	cur += sizeof(*helper);

	if (extension) {
		switch_copy_string(cur, extension, strlen(extension) + 1);
		helper->extension = cur;
		cur += strlen(helper->extension) + 1;
	}

	if (dialplan) {
		switch_copy_string(cur, dialplan, strlen(dialplan) + 1);
		helper->dialplan = cur;
		cur += strlen(helper->dialplan) + 1;
	}

	if (context) {
		switch_copy_string(cur, context, strlen(context) + 1);
		helper->context = cur;
	}

	return switch_scheduler_add_task(runtime, sch_transfer_callback, (char *) __SWITCH_FUNC__, uuid, 0, helper, SSHF_FREE_ARG);
}


struct broadcast_helper {
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *path;
	switch_media_flag_t flags;
};

SWITCH_STANDARD_SCHED_FUNC(sch_broadcast_callback)
{
	struct broadcast_helper *helper;
	switch_assert(task);

	helper = (struct broadcast_helper *) task->cmd_arg;
	switch_ivr_broadcast(helper->uuid_str, helper->path, helper->flags);
}

SWITCH_DECLARE(uint32_t) switch_ivr_schedule_broadcast(time_t runtime, char *uuid, char *path, switch_media_flag_t flags)
{
	struct broadcast_helper *helper;
	size_t len = sizeof(*helper) + strlen(path) + 1;
	char *cur = NULL;

	switch_zmalloc(cur, len);
	helper = (struct broadcast_helper *)cur;
	
	cur += sizeof(*helper);
	switch_copy_string(helper->uuid_str, uuid, sizeof(helper->uuid_str));
	helper->flags = flags;
	
	switch_copy_string(cur, path, len - sizeof(helper));
	helper->path = cur;

	return switch_scheduler_add_task(runtime, sch_broadcast_callback, (char *) __SWITCH_FUNC__, uuid, 0, helper, SSHF_FREE_ARG);
}

SWITCH_DECLARE(switch_status_t) switch_ivr_broadcast(const char *uuid, const char *path, switch_media_flag_t flags)
{
	switch_channel_t *channel;
	int nomedia;
	switch_core_session_t *session, *master;
	switch_event_t *event;
	switch_core_session_t *other_session = NULL;
	const char *other_uuid = NULL;
	char *app = "playback";

	switch_assert(path);

	if ((session = switch_core_session_locate(uuid))) {
		char *cause = NULL;
		char *mypath;
		char *p;

		master = session;

		channel = switch_core_session_get_channel(session);
		switch_assert(channel != NULL);

		if ((switch_channel_test_flag(channel, CF_EVENT_PARSE))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Channel [%s] already broadcasting...broadcast aborted\n", 
							  switch_channel_get_name(channel));
			switch_core_session_rwunlock(session);
			return SWITCH_STATUS_FALSE;
		}

		mypath = strdup(path);


		if ((nomedia = switch_channel_test_flag(channel, CF_BYPASS_MEDIA))) {
			switch_ivr_media(uuid, SMF_REBRIDGE);
		}
		
		if ((p = strchr(mypath, ':')) && *(p+1) == ':') {
			app = mypath;
			*p++ = '\0';
			*p++ = '\0';
			path = p;
		}

		if ((cause = strchr(app, '!'))) {
			*cause++ = '\0';
			if (!cause) {
				cause = "normal_clearing";
			}
		}

		if ((flags & SMF_ECHO_BLEG) && (other_uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE))
			&& (other_session = switch_core_session_locate(other_uuid))) {
			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "call-command", "execute");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-name", "%s", app);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-arg", "%s", path);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "lead-frames", "%d", 5);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event-lock", "%s", "true");
				if ((flags & SMF_LOOP)) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "loops", "%d", -1);
				}
				
				switch_core_session_queue_private_event(other_session, &event);
			}
			
			switch_core_session_rwunlock(other_session);
			master = other_session;
			other_session = NULL;
		}

		if ((flags & SMF_ECHO_ALEG)) {
			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "call-command", "execute");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-name", "%s", app);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-arg", "%s", path);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "lead-frames", "%d", 5);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event-lock", "%s", "true");
				if ((flags & SMF_LOOP)) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "loops", "%d", -1);
				}
				switch_core_session_queue_private_event(session, &event);
			}
			master = session;
		}

		if (nomedia) {
			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "call-command", "nomedia");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "nomedia-uuid", "%s", uuid);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event-lock", "%s", "true");
				switch_core_session_queue_private_event(master, &event);
			}
		}

		if (cause) {
			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "call-command", "execute");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-name", "hangup");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "execute-app-arg", "%s", cause);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event-lock", "%s", "true");
				switch_core_session_queue_private_event(session, &event);
			}
		}

		switch_core_session_rwunlock(session);
		switch_safe_free(mypath);
	}


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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
