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
 *
 * mod_fsv -- FS Video File Format
 *
 */
#include <switch.h>


SWITCH_MODULE_LOAD_FUNCTION(mod_fsv_load);
SWITCH_MODULE_DEFINITION(mod_fsv, mod_fsv_load, NULL, NULL);

#define VID_BIT (1 << 31)
#define VERSION 4201

struct file_header {
	int32_t version;
	char video_codec_name[32];
	char video_fmtp[128];
	uint32_t audio_rate;
	uint32_t audio_ptime;
	switch_time_t created;
};

struct record_helper {
	switch_core_session_t *session;
	switch_mutex_t *mutex;
	int fd;
	int up;
};

static void *SWITCH_THREAD_FUNC record_video_thread(switch_thread_t *thread, void *obj)
{
	struct record_helper *eh = obj;
	switch_core_session_t *session = eh->session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	switch_frame_t *read_frame;
	int bytes;

	eh->up = 1;
	while (switch_channel_ready(channel)) {
		status = switch_core_session_read_video_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		bytes = read_frame->packetlen | VID_BIT;

		switch_mutex_lock(eh->mutex);

		if (write(eh->fd, &bytes, sizeof(bytes)) != (int) sizeof(bytes)) {
			switch_mutex_unlock(eh->mutex);
			break;
		}

		if (write(eh->fd, read_frame->packet, read_frame->packetlen) != (int) read_frame->packetlen) {
			switch_mutex_unlock(eh->mutex);
			break;
		}

		switch_mutex_unlock(eh->mutex);

		switch_core_session_write_video_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);
	}
	eh->up = 0;
	return NULL;
}

SWITCH_STANDARD_APP(record_fsv_function)
{
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct record_helper eh = { 0 };
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	int fd;
	switch_mutex_t *mutex = NULL;
	switch_codec_t codec, *vid_codec;
	switch_codec_implementation_t read_impl = { 0 };
	int count = 0, sanity = 30;

	switch_core_session_get_read_impl(session, &read_impl);
	switch_channel_answer(channel);


	while (switch_channel_up(channel) && !switch_channel_test_flag(channel, CF_VIDEO)) {
		switch_yield(10000);

		if (count) count--;

		if (count == 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s waiting for video.\n", switch_channel_get_name(channel));
			count = 100;
			if (!--sanity) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s timeout waiting for video.\n", 
								  switch_channel_get_name(channel));
				return;
			}
		}
	}
	
	if (!switch_channel_ready(channel)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "%s not ready.\n", switch_channel_get_name(channel));
		return;
	}

	if ((fd = open((char *) data, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IRUSR | S_IWUSR)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error opening file %s\n", (char *) data);
		return;
	}

	if (switch_core_codec_init(&codec,
							   "L16",
							   NULL,
							   read_impl.samples_per_second,
							   read_impl.microseconds_per_packet / 1000,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Activation Success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Audio Codec Activation Fail\n");
		goto end;
	}

	switch_core_session_set_read_codec(session, &codec);

	if (switch_channel_test_flag(channel, CF_VIDEO)) {
		struct file_header h;
		memset(&h, 0, sizeof(h));
		vid_codec = switch_core_session_get_video_read_codec(session);

		h.version = VERSION;
		h.created = switch_micro_time_now();
		switch_set_string(h.video_codec_name, vid_codec->implementation->iananame);
		if (vid_codec->fmtp_in) {
			switch_set_string(h.video_fmtp, vid_codec->fmtp_in);
		}
		h.audio_rate = read_impl.samples_per_second;
		h.audio_ptime = read_impl.microseconds_per_packet / 1000;

		if (write(fd, &h, sizeof(h)) != sizeof(h)) {
			goto end;
		}

		switch_mutex_init(&mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		eh.mutex = mutex;
		eh.fd = fd;
		eh.session = session;
		switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, record_video_thread, &eh, switch_core_session_get_pool(session));
	}


	while (switch_channel_ready(channel)) {

		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		if (mutex) {
			switch_mutex_lock(mutex);
		}

		if (write(fd, &read_frame->datalen, sizeof(read_frame->datalen)) != sizeof(read_frame->datalen)) {
			if (mutex) {
				switch_mutex_unlock(mutex);
			}
			break;
		}

		if (write(fd, read_frame->data, read_frame->datalen) != (int) read_frame->datalen) {
			if (mutex) {
				switch_mutex_unlock(mutex);
			}
			break;
		}

		if (mutex) {
			switch_mutex_unlock(mutex);
		}
	}


  end:

	if (eh.up) {
		while (eh.up) {
			switch_cond_next();
		}
	}

	switch_core_session_set_read_codec(session, NULL);
	switch_core_codec_destroy(&codec);

}

SWITCH_STANDARD_APP(play_fsv_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_frame_t write_frame = { 0 }, vid_frame = {
	0};
	int fd = -1;
	int bytes;
	switch_codec_t codec = { 0 }, vid_codec = {
	0}, *read_vid_codec;
	unsigned char *aud_buffer;
	unsigned char *vid_buffer;
	struct file_header h;
	uint32_t ts = 0, last = 0;
	switch_timer_t timer = { 0 };
	switch_payload_t pt = 0;
	switch_codec_implementation_t read_impl = { 0 };
	switch_core_session_get_read_impl(session, &read_impl);

	aud_buffer = switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);
	vid_buffer = switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);

	if ((fd = open((char *) data, O_RDONLY | O_BINARY)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error opening file %s\n", (char *) data);
		return;
	}

	if (read(fd, &h, sizeof(h)) != sizeof(h)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error reading file header\n");
		goto end;
	}

	if (h.version != VERSION) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "File version does not match!\n");
		goto end;
	}

	switch_channel_set_variable(channel, "sip_force_video_fmtp", h.video_fmtp);
	switch_channel_answer(channel);

	if ((read_vid_codec = switch_core_session_get_video_read_codec(session))) {
		pt = read_vid_codec->agreed_pt;
	}

	write_frame.codec = &codec;
	write_frame.data = aud_buffer;
	write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	vid_frame.codec = &vid_codec;
	vid_frame.packet = vid_buffer;
	vid_frame.data = vid_buffer + 12;
	vid_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE - 12;
	switch_set_flag((&vid_frame), SFF_RAW_RTP);
	switch_set_flag((&vid_frame), SFF_PROXY_PACKET);

	if (switch_core_timer_init(&timer, "soft", read_impl.microseconds_per_packet / 1000,
							   read_impl.samples_per_packet, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Timer Activation Fail\n");
		goto end;
	}

	if (switch_core_codec_init(&codec,
							   "L16",
							   NULL,
							   h.audio_rate,
							   h.audio_ptime,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Activation Success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Audio Codec Activation Fail\n");
		goto end;
	}

	if (switch_core_codec_init(&vid_codec,
							   h.video_codec_name,
							   NULL,
							   0,
							   0,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video Codec Activation Success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Video Codec Activation Fail\n");
		goto end;
	}
	switch_core_session_set_read_codec(session, &codec);

	while (switch_channel_ready(channel)) {

		if (read(fd, &bytes, sizeof(bytes)) != sizeof(bytes)) {
			break;
		}

		if (bytes & VID_BIT) {
			switch_rtp_hdr_t *hdr = vid_frame.packet;
			bytes &= ~VID_BIT;

			if ((vid_frame.packetlen = read(fd, vid_frame.packet, bytes)) != (uint32_t) bytes) {
				break;
			}

			ts = ntohl(hdr->ts);
			if (pt) {
				hdr->pt = pt;
			}
			if (switch_channel_test_flag(channel, CF_VIDEO)) {
				switch_byte_t *data = (switch_byte_t *) vid_frame.packet;

				vid_frame.data = data + 12;
				vid_frame.datalen = vid_frame.packetlen - 12;
				switch_core_session_write_video_frame(session, &vid_frame, SWITCH_IO_FLAG_NONE, 0);
			}
			if (ts && last && last != ts) {
				switch_cond_next();
			}
			last = ts;
		} else {
			if (bytes > (int) write_frame.buflen) {
				bytes = write_frame.buflen;
			}
			if ((write_frame.datalen = read(fd, write_frame.data, bytes)) <= 0) {
				break;
			}
			switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
			switch_core_timer_next(&timer);
		}

	}

  end:

	if (timer.interval) {
		switch_core_timer_destroy(&timer);
	}


	switch_core_session_set_read_codec(session, NULL);


	if (switch_core_codec_ready(&codec)) {
		switch_core_codec_destroy(&codec);
	}

	if (switch_core_codec_ready(&vid_codec)) {
		switch_core_codec_destroy(&vid_codec);
	}

	if (fd > -1) {
		close(fd);
	}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_fsv_load)
{
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "play_fsv", "play an fsv file", "play an fsv file", play_fsv_function, "<file>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "record_fsv", "record an fsv file", "record an fsv file", record_fsv_function, "<file>", SAF_NONE);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
