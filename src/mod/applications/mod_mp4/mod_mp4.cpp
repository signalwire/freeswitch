/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Paulo Rogério Panhoto <paulo@voicetechnology.com.br>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * mod_mp4 -- MP4 File Format support for video apps.
 *
 */

#include <switch.h>
#include "mp4_helper.hpp"
#include "exception.hpp"


#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_mp4_load);
SWITCH_MODULE_DEFINITION(mod_mp4, mod_mp4_load, NULL, NULL);

#define VID_BIT (1 << 31)
#define VERSION 4201

/*
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
*/

struct AVParams {
	switch_core_session_t * session;
	switch_channel_t * channel;
	switch_timer_t * timer;
	switch_frame_t * frame;
	switch_mutex_t * mutex;
	bool video;
	switch_payload_t pt;
	MP4::Context * vc;
	bool done;
	bool * quit;
};

static void *SWITCH_THREAD_FUNC record_video_thread(switch_thread_t *thread, void *obj)
{
/*
	record_helper *eh = reinterpret_cast<record_helper *>(obj);
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
*/
	return NULL;
}

SWITCH_STANDARD_APP(record_mp4_function)
{
/*
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct record_helper eh = { 0 };
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	int fd;
	switch_mutex_t *mutex = NULL;
	switch_codec_t codec, *vid_codec;
	switch_codec_implementation_t read_impl = { };
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

/*
	if ((fd = open((char *) data, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IRUSR | S_IWUSR)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error opening file %s\n", (char *) data);
		return;
	}
**

	MP4::Context ctx(reinterpret_cast<char*>(data), true);

	if (switch_core_codec_init(&codec,
			"L16",
			NULL,
			read_impl.samples_per_second,
			read_impl.microseconds_per_packet / 1000,
			1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
			NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS)
	{
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
*/
}

static void *SWITCH_THREAD_FUNC play_video_function(switch_thread_t *thread, void *obj)
{
	AVParams * pt = reinterpret_cast<AVParams*>(obj);
	u_int next = 0, first = 0xffffffff;
	u_int64_t ts = 0, control = 0;

	bool ok;
	bool sent = true;
	pt->done = false;
	switch_time_t start = switch_time_now();
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pt->session), SWITCH_LOG_DEBUG, "Video thread Started\n");
	while (!*pt->quit && switch_channel_ready(pt->channel)) {
		if (pt->video) {
			if (sent) {
				switch_mutex_lock(pt->mutex);
				pt->frame->packetlen = pt->frame->buflen;
				ok = pt->vc->getVideoPacket(pt->frame->packet, pt->frame->packetlen, next);
				switch_mutex_unlock(pt->mutex);
				sent = false;
				if (ok) {
					switch_rtp_hdr_t *hdr = reinterpret_cast<switch_rtp_hdr_t *>(pt->frame->packet);
					if(first == 0xffffffff) first = next;
					next -= first;
					control = next * 90000LL / pt->vc->videoTrack().track.clock;
					control -= first;
					hdr->ts = htonl(control);
					control = control * 1000 / 90;
					if (pt->pt)
						hdr->pt = pt->pt;
				} else break;
			}

			ts = switch_time_now() - start;
			int64_t wait = control > ts ? (control - ts) : 0;

			if (wait > 0) {
				switch_cond_next();
				// wait the time for the next Video frame
				switch_sleep(wait);
			}

			if (switch_channel_test_flag(pt->channel, CF_VIDEO)) {
				switch_byte_t *data = (switch_byte_t *) pt->frame->packet;

				pt->frame->data = data + 12;
				pt->frame->datalen = pt->frame->packetlen - 12;
				switch_core_session_write_video_frame(pt->session, pt->frame, SWITCH_IO_FLAG_NONE, 0);
				sent = true;
			}

		} 
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pt->session), SWITCH_LOG_DEBUG, "Video thread ended\n");
	pt->done = true;
	return NULL;
}

static void *SWITCH_THREAD_FUNC play_audio_function(switch_thread_t *thread, void *obj)
{
	AVParams * pt = reinterpret_cast<AVParams*>(obj);
	u_int next = 0, first = 0xffffffff;
	u_int64_t ts = 0, control = 0;

	bool ok;
	bool sent = true;
	switch_dtmf_t dtmf = {0};
	pt->done = false;
	switch_frame_t * read_frame;
	
	while (!*pt->quit && switch_channel_ready(pt->channel)) {
		// event processing.
		// -- SEE switch_ivr_play_say.c:1231 && mod_dptools.c:1428 && mod_dptools.c:1919
		switch_core_session_read_frame(pt->session, &read_frame, SWITCH_IO_FLAG_SINGLE_READ, 0);

		if (switch_channel_test_flag(pt->channel, CF_BREAK)) {
			switch_channel_clear_flag(pt->channel, CF_BREAK);
			break;
		}

		switch_ivr_parse_all_events(pt->session);

		if (switch_channel_has_dtmf(pt->channel)) {
			switch_channel_dequeue_dtmf(pt->channel, &dtmf);
			const char * terminators = switch_channel_get_variable(pt->channel, SWITCH_PLAYBACK_TERMINATORS_VARIABLE);
			if (terminators && !strcasecmp(terminators, "none")) terminators = NULL;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pt->session), SWITCH_LOG_DEBUG, "Digit %c\n", dtmf.digit);
			if (terminators && strchr(terminators, dtmf.digit)) {
				std::string digit(&dtmf.digit, 0, 1);
				switch_channel_set_variable(pt->channel, SWITCH_PLAYBACK_TERMINATOR_USED, digit.c_str());
				break;
			}
		}
		
		switch_mutex_lock(pt->mutex);
		pt->frame->datalen = pt->frame->buflen;
		ok = pt->vc->getAudioPacket(pt->frame->data, pt->frame->datalen, next);
		switch_mutex_unlock(pt->mutex);

		if (ok) {
		  if (pt->frame->datalen > (int) pt->frame->buflen)
				pt->frame->datalen = pt->frame->buflen;

			switch_core_session_write_frame(pt->session, pt->frame, SWITCH_IO_FLAG_NONE, 0);
			switch_core_timer_next(pt->timer);
		}
		else break;
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pt->session), SWITCH_LOG_DEBUG, "Audio done\n");
	*pt->quit = pt->done = true;
	return NULL;
}

SWITCH_STANDARD_APP(play_mp4_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_frame_t write_frame = { 0 }, vid_frame = {0};
	switch_codec_t codec = { 0 }, vid_codec = {0}, *read_vid_codec;
	unsigned char *aud_buffer;
	unsigned char *vid_buffer;
	switch_timer_t timer = { 0 };
	switch_codec_implementation_t read_impl = {};
	bool done = false;

	try {
		MP4::Context vc((char *) data);

		switch_payload_t pt = 0;

		switch_core_session_get_read_impl(session, &read_impl);

		aud_buffer = (unsigned char *) switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);
		vid_buffer = (unsigned char *) switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);

		/*
		if (!vc.isOpen())
		{
			char msgbuf[1024];
			sprintf(msgbuf, "PLAYBACK ERROR (%s): FILE NOT FOUND.", (char*) data);
			switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, msgbuf);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error opening file %s\n", (char *) data);
			return;
		}
		
		if(!vc.isSupported())
		{
			char msgbuf[1024];
			sprintf(msgbuf, "PLAYBACK ERROR (%s): UNSUPPORTED FORMAT OR FILE NOT HINTED.", (char*) data);
			switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, msgbuf);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, 
				"Error reading track info. Maybe this file is not hinted.\n");
			throw 1;
		}
		*/

		switch_channel_set_variable(channel, "sip_force_video_fmtp", vc.videoTrack().fmtp.c_str());
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
			throw 2;
		}

		if (switch_core_codec_init(&codec,
									vc.audioTrack().codecName,
									NULL,
									vc.audioTrack().clock,
									vc.audioTrack().packetLength,
									1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
									NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Activation Success\n");
		} else {
			throw Exception("Audio Codec Activation Fail");
		}

		if (switch_core_codec_init(&vid_codec,
									vc.videoTrack().track.codecName,
									NULL,
									0,
									0,
									1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
									NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video Codec Activation Success\n");
		} else
		{
			throw Exception("Video Codec Activation Fail");
		}
		switch_core_session_set_read_codec(session, &codec);

		AVParams vpt;
		vpt.session = session;
		vpt.channel = channel;
		vpt.frame = &vid_frame;
		vpt.timer = &timer;
		vpt.video = true;
		vpt.pt = pt;
		vpt.vc = &vc;
		switch_mutex_init(&vpt.mutex, SWITCH_MUTEX_DEFAULT, switch_core_session_get_pool(session));
		vpt.quit = &done;
		
		switch_threadattr_t * thd_attr;
		switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_t *thread;
		switch_thread_create(&thread, thd_attr, play_video_function, (void*)&vpt, switch_core_session_get_pool(session));

		AVParams apt;
		apt.session = session;
		apt.channel = channel;
		apt.frame = &write_frame;
		apt.timer = &timer;
		apt.video = false;
		apt.vc = &vc;
		apt.mutex = vpt.mutex;
		apt.quit = &done;
		play_audio_function(NULL, &apt);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Waiting for video thread to join.\n");
		while (!vpt.done) {
			switch_cond_next();
		}

		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE PLAYED");
	} catch(const std::exception & e) 
	{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s\n", e.what());
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE,
			(std::string("PLAYBACK_FAILED - ") + e.what()).c_str());
	}catch(...)
	{
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "PLAYBACK_FAILED - See FS logs for detail.");
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Exception caught.\n");
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "All done.\n");
	if (timer.interval)  switch_core_timer_destroy(&timer);

	switch_core_session_set_read_codec(session, NULL);

	if (switch_core_codec_ready(&codec)) switch_core_codec_destroy(&codec);

	if (switch_core_codec_ready(&vid_codec)) switch_core_codec_destroy(&vid_codec);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_mp4_load)
{
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "play_mp4", "play an MP4 file", "play an MP4 file", play_mp4_function, "<file>", SAF_NONE);
	//SWITCH_ADD_APP(app_interface, "record_mp4", "record an MP4 file", "record an MP4 file", record_mp4_function, "<file>", SAF_NONE);

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
