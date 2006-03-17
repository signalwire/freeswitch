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
 * switch_ivr_api.c -- IVR Library
 *
 */
#include <switch.h>
#include <switch_ivr.h>


/* TBD (Lots! there are not very many functions in here lol) */

SWITCH_DECLARE(switch_status) switch_ivr_collect_digits_callback(switch_core_session *session,
																 switch_dtmf_callback_function dtmf_callback,
																 void *buf,
																 unsigned int buflen)
{
	switch_channel *channel;
	switch_status status = SWITCH_STATUS_SUCCESS;
	
	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	if (!dtmf_callback) {
		return SWITCH_STATUS_GENERR;
	}

	while(switch_channel_ready(channel)) {
		switch_frame *read_frame;
		char dtmf[128];

		if (switch_channel_has_dtmf(channel)) {
			switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
			status = dtmf_callback(session, dtmf, buf, buflen);
		}

		if (status != SWITCH_STATUS_SUCCESS) {
			break;
		}

		if (switch_core_session_read_frame(session, &read_frame, -1, 0) != SWITCH_STATUS_SUCCESS) {
			break;
		}
	}

	return status;
}


SWITCH_DECLARE(switch_status) switch_ivr_collect_digits_count(switch_core_session *session,
															  char *buf,
															  unsigned int buflen,
															  unsigned int maxdigits,
															  const char *terminators,
															  char *terminator,
															  unsigned int timeout,
															  unsigned int poll_channel
															  )
{
	unsigned int i = 0, x =  (unsigned int) strlen(buf);
	switch_channel *channel;
	switch_status status = SWITCH_STATUS_SUCCESS;
	switch_time_t started = 0;
	unsigned int elapsed;

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	*terminator = '\0';

	if (terminators) {
		for (i = 0 ; i < x; i++) {
			if (strchr(terminators, buf[i])) {
				*terminator = buf[i];
				return SWITCH_STATUS_SUCCESS;
			}
		}
	}

	if (timeout) {
		started = switch_time_now();
	}

	while(switch_channel_ready(channel)) {
		switch_frame *read_frame;

		if (timeout) {
			elapsed = (unsigned int)((switch_time_now() - started) / 1000);
			if (elapsed >= timeout) {
				break;
			}
		}
		
		if (switch_channel_has_dtmf(channel)) {
			char dtmf[128];
			switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));

			for(i =0 ; i < (unsigned int) strlen(dtmf); i++) {

				if (strchr(terminators, dtmf[i])) {
					*terminator = dtmf[i];
					return SWITCH_STATUS_SUCCESS;
				}

				buf[x++] = dtmf[i];
				buf[x] = '\0';
				if (x >= buflen || x >= maxdigits) {
					return SWITCH_STATUS_SUCCESS;
				}
			}
		}
		if (poll_channel) {
			if ((status = switch_core_session_read_frame(session, &read_frame, -1, 0)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		} else {
			switch_yield(1000);
		}
	}

	return status;
}



SWITCH_DECLARE(switch_status) switch_ivr_record_file(switch_core_session *session, 
													 switch_file_handle *fh,
													 char *file,
													 switch_dtmf_callback_function dtmf_callback,
													 void *buf,
													 unsigned int buflen)
{
	switch_channel *channel;
    char dtmf[128];
	switch_file_handle lfh;
	switch_frame *read_frame;
	switch_codec codec, *read_codec;
	char *codec_name;
	switch_status status = SWITCH_STATUS_SUCCESS;

	if (!fh) {
		fh = &lfh;
	}
	memset(fh, 0, sizeof(*fh));

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	read_codec = switch_core_session_get_read_codec(session);
	assert(read_codec != NULL);

	fh->channels = read_codec->implementation->number_of_channels;
	fh->samplerate = read_codec->implementation->samples_per_second;


	if (switch_core_file_open(fh,
							  file,
							  SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT,
							  switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel);
		return SWITCH_STATUS_GENERR;
	}

	switch_channel_answer(channel);

	
	codec_name = "L16";
	if (switch_core_codec_init(&codec,
							   codec_name,
							   read_codec->implementation->samples_per_second,
							   read_codec->implementation->microseconds_per_frame / 1000,
							   read_codec->implementation->number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activated\n");
		switch_core_session_set_read_codec(session, &codec);		
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activation Failed %s@%dhz %d channels %dms\n",
							  codec_name, fh->samplerate, fh->channels, read_codec->implementation->microseconds_per_frame / 1000);
		switch_core_file_close(fh);
		return SWITCH_STATUS_GENERR;
	}
	

	while(switch_channel_ready(channel)) {
		size_t len;

		if (dtmf_callback || buf) {
			/*
			  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
			  if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
			*/
			if (switch_channel_has_dtmf(channel)) {
				switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
				if (dtmf_callback) {
					status = dtmf_callback(session, dtmf, buf, buflen);
				} else {
					switch_copy_string((char *)buf, dtmf, buflen);
					status = SWITCH_STATUS_BREAK;
				}
			}

			if (status != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
		
		if ((status = switch_core_session_read_frame(session, &read_frame, -1, 0)) != SWITCH_STATUS_SUCCESS) {
			break;
		}
		if (!switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
			len = (size_t) read_frame->datalen / 2;
			switch_core_file_write(fh, read_frame->data, &len);
		}
	}

	switch_core_session_set_read_codec(session, read_codec);
	switch_core_file_close(fh);

	return status;
}

SWITCH_DECLARE(switch_status) switch_ivr_play_file(switch_core_session *session, 
												   switch_file_handle *fh,
												   char *file,
												   char *timer_name,
												   switch_dtmf_callback_function dtmf_callback,
												   void *buf,
												   unsigned int buflen)
{
	switch_channel *channel;
	short abuf[960];
	char dtmf[128];
	int interval = 0, samples = 0;
	size_t len = 0, ilen = 0, olen = 0;
	switch_frame write_frame;
	switch_timer timer;
	switch_core_thread_session thread_session;
	switch_codec codec;
	switch_memory_pool *pool = switch_core_session_get_pool(session);
	char *codec_name;
	int x;
	int stream_id;
	switch_status status = SWITCH_STATUS_SUCCESS;
	switch_file_handle lfh;

	if (!fh) {
		fh = &lfh;
		memset(fh, 0, sizeof(lfh));
	}

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_core_file_open(fh,
							  file,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
							  switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel);
		return SWITCH_STATUS_GENERR;
	}


	write_frame.data = abuf;
	write_frame.buflen = sizeof(abuf);


	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OPEN FILE %s %dhz %d channels\n", file, fh->samplerate, fh->channels);

	interval = 20;
	samples = (fh->samplerate / 50) * fh->channels;
	len = samples * 2;

	codec_name = "L16";

	if (switch_core_codec_init(&codec,
							   codec_name,
							   fh->samplerate,
							   interval,
							   fh->channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, pool) == SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activated\n");
		write_frame.codec = &codec;
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activation Failed %s@%dhz %d channels %dms\n",
							  codec_name, fh->samplerate, fh->channels, interval);
		switch_core_file_close(fh);
		return SWITCH_STATUS_GENERR;
	}

	if (timer_name) {
		if (switch_core_timer_init(&timer, timer_name, interval, samples, pool) != SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "setup timer failed!\n");
			switch_core_codec_destroy(&codec);
			switch_core_file_close(fh);
			return SWITCH_STATUS_GENERR;
		}
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "setup timer success %d bytes per %d ms!\n", len, interval);
	}
	write_frame.rate = fh->samplerate;

	if (timer_name) {
		/* start a thread to absorb incoming audio */
		for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
			switch_core_service_session(session, &thread_session, stream_id);
		}
	}

	ilen = samples;
	while(switch_channel_ready(channel)) {
		int done = 0;
		int do_speed = 1;
		int last_speed = -1;

		if (dtmf_callback || buf) {
			/*
			  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
			  if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
			*/
			if (switch_channel_has_dtmf(channel)) {
				switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
				if (dtmf_callback) {
					status = dtmf_callback(session, dtmf, buf, buflen);
				} else {
					switch_copy_string((char *)buf, dtmf, buflen);
					status = SWITCH_STATUS_BREAK;
				}
			}
			
			if (status != SWITCH_STATUS_SUCCESS) {
				done = 1;
				break;
			}
		}
		
		if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
			memset(abuf, 0, ilen * 2);
			olen = ilen;
            do_speed = 0;
		} else if (fh->audio_buffer && (switch_buffer_inuse(fh->audio_buffer) > (ilen * 2))) {
			switch_buffer_read(fh->audio_buffer, abuf, ilen * 2);
			olen = ilen;
			do_speed = 0;
		} else {
			olen = ilen;
			switch_core_file_read(fh, abuf, &olen);
		}

		if (done || olen <= 0) {
			break;
		}

		if (fh->speed > 2) {
			fh->speed = 2;
		} else if(fh->speed < -2) {
			fh->speed = -2;
		}
		
		if (fh->audio_buffer && last_speed > -1 && last_speed != fh->speed) {
			switch_buffer_zero(fh->audio_buffer);
		}

		
		if (fh->speed && do_speed) {
			float factor = 0.25f * abs(fh->speed);
			size_t newlen, supplement, step;
			short *bp = write_frame.data;
			size_t wrote = 0;
			
			if (!fh->audio_buffer) {
				switch_buffer_create(fh->memory_pool, &fh->audio_buffer, SWITCH_RECCOMMENDED_BUFFER_SIZE);
			} 
			
			supplement = (int) (factor * olen);
			newlen = (fh->speed > 0) ? olen - supplement : olen + supplement;
			step = (fh->speed > 0) ? (newlen / supplement) : (olen / supplement);
			
			while ((wrote + step) < newlen) {
				switch_buffer_write(fh->audio_buffer, bp, step * 2);
				wrote += step;
				bp += step;
				if (fh->speed > 0) {
					bp++;
				} else {
					float f;
					short s;
					f = (float)(*bp + *(bp+1) + *(bp-1));
					f /= 3;
					s = (short) f;
					switch_buffer_write(fh->audio_buffer, &s, 2);
					wrote++;
				}
			}
			if (wrote < newlen) {
				size_t r = newlen - wrote;
				switch_buffer_write(fh->audio_buffer, bp, r*2);
				wrote += r;
			}
			last_speed = fh->speed;
			continue;
		} 

		write_frame.datalen = olen * 2;
		write_frame.samples = (int) olen;
#if __BYTE_ORDER == __BIG_ENDIAN
		switch_swap_linear(write_frame.data, (int) write_frame.datalen / 2);
#endif
		
		for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
			if (switch_core_session_write_frame(session, &write_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Bad Write\n");
				done = 1;
				break;
			}
			if (done) {
				break;
			}
		}
		if (timer_name) {
			if ((x = switch_core_timer_next(&timer)) < 0) {
				break;
			}
		} else { /* time off the channel (if you must) */
			switch_frame *read_frame;
			if (switch_core_session_read_frame(session, &read_frame, -1, 0) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "done playing file\n");
	switch_core_file_close(fh);

	switch_core_codec_destroy(&codec);

	if (timer_name) {
		/* End the audio absorbing thread */
		switch_core_thread_session_end(&thread_session);
		switch_core_timer_destroy(&timer);
	}

	return status;
}




SWITCH_DECLARE(switch_status) switch_ivr_speak_text(switch_core_session *session, 
													char *tts_name,
													char *voice_name,
													char *timer_name,
													size_t rate,
													switch_dtmf_callback_function dtmf_callback,
													char *text,
													void *buf,
													unsigned int buflen)
{
	switch_channel *channel;
	short abuf[960];
	char dtmf[128];
	int interval = 0;
	size_t samples = 0;
	size_t len = 0;
	size_t ilen = 0;
	switch_frame write_frame;
	switch_timer timer;
	switch_core_thread_session thread_session;
	switch_codec codec;
	switch_memory_pool *pool = switch_core_session_get_pool(session);
	char *codec_name;
	int x;
	int stream_id;
	int done = 0;
	int lead_in_out = 10;

	switch_status status = SWITCH_STATUS_SUCCESS;
	switch_speech_handle sh;
	switch_speech_flag flags = SWITCH_SPEECH_FLAG_TTS;


	memset(&sh, 0, sizeof(sh));

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_core_speech_open(&sh,
								tts_name,
								voice_name,
								(unsigned int)rate,
								&flags,
								switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid TTS module!\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_channel_answer(channel);

	write_frame.data = abuf;
	write_frame.buflen = sizeof(abuf);


	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OPEN TTS %s\n", tts_name);
	
	interval = 20;
	samples = (rate / 50);
	len = samples * 2;

	codec_name = "L16";

	if (switch_core_codec_init(&codec,
							   codec_name,
							   (int)rate,
							   interval,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, pool) == SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activated\n");
		write_frame.codec = &codec;
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activation Failed %s@%dhz %d channels %dms\n",
							  codec_name, rate, 1, interval);
		flags = 0;
		switch_core_speech_close(&sh, &flags);
		return SWITCH_STATUS_GENERR;
	}

	if (timer_name) {
		if (switch_core_timer_init(&timer, timer_name, interval, (int)samples, pool) != SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "setup timer failed!\n");
			switch_core_codec_destroy(&codec);
			flags = 0;
			switch_core_speech_close(&sh, &flags);
			return SWITCH_STATUS_GENERR;
		}
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "setup timer success %d bytes per %d ms!\n", len, interval);
	}

	flags = 0;
	switch_core_speech_feed_tts(&sh, text, &flags);
	write_frame.rate = (int)rate;

	memset(write_frame.data, 0, len);
	write_frame.datalen = len;
	write_frame.samples = len / 2;
	
	for( x = 0; !done && x < lead_in_out; x++) {
		for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
			if (switch_core_session_write_frame(session, &write_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Bad Write\n");
				done = 1;
				break;
			}
		}
	}

	if (timer_name) {
		/* start a thread to absorb incoming audio */
		for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
			switch_core_service_session(session, &thread_session, stream_id);
		}
	}

	ilen = len;
	while(switch_channel_ready(channel)) {
		if (dtmf_callback || buf) {


			/*
			  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
			  if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
			*/
			if (switch_channel_has_dtmf(channel)) {
				switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
				if (dtmf_callback) {
					status = dtmf_callback(session, dtmf, buf, buflen);
				} else {
					switch_copy_string((char *)buf, dtmf, buflen);
					status = SWITCH_STATUS_BREAK;
				}
			}
			
			if (status != SWITCH_STATUS_SUCCESS) {
				done = 1;
				break;
			}
		}

		flags = SWITCH_SPEECH_FLAG_BLOCKING;
		status = switch_core_speech_read_tts(&sh,
											 abuf,
											 &ilen,
											 &rate,
											 &flags);

		if (status != SWITCH_STATUS_SUCCESS) {
			for( x = 0; !done && x < lead_in_out; x++) {
				for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
					if (switch_core_session_write_frame(session, &write_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
						switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Bad Write\n");
						done = 1;
						break;
					}
				}
			}
			done = 1;
		}
		
		if (done || ilen <= 0) {
			break;
		}

		write_frame.datalen = ilen;
		write_frame.samples = (int) ilen / 2;

		for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
			if (switch_core_session_write_frame(session, &write_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Bad Write\n");
				done = 1;
				break;
			}

			if (done) {
				break;
			}
		}
		if (timer_name) {
			if ((x = switch_core_timer_next(&timer)) < 0) {
				break;
			}
		} else { /* time off the channel (if you must) */
			switch_frame *read_frame;
			if (switch_core_session_read_frame(session, &read_frame, -1, 0) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "done speaking text\n");
	flags = 0;	
	switch_core_speech_close(&sh, &flags);
	switch_core_codec_destroy(&codec);

	if (timer_name) {
		/* End the audio absorbing thread */
		switch_core_thread_session_end(&thread_session);
		switch_core_timer_destroy(&timer);
	}

	return status;
}


/* Bridge Related Stuff*/
/*********************************************************************************/
struct audio_bridge_data {
	switch_core_session *session_a;
	switch_core_session *session_b;
	int running;
};

static void *audio_bridge_thread(switch_thread *thread, void *obj)
{
	struct switch_core_thread_session *data = obj;
	int *stream_id_p;
	int stream_id = 0, ans_a = 0, ans_b = 0;
	switch_dtmf_callback_function dtmf_callback;
	void *user_data;
	int *his_status;

	switch_channel *chan_a, *chan_b;
	switch_frame *read_frame;
	switch_core_session *session_a, *session_b;

	session_a = data->objs[0];
	session_b = data->objs[1];

	stream_id_p = data->objs[2];
	dtmf_callback = data->objs[3];
	user_data = data->objs[4];
	his_status = data->objs[5];

	if (stream_id_p) {
		stream_id = *stream_id_p;
	}

	chan_a = switch_core_session_get_channel(session_a);
	chan_b = switch_core_session_get_channel(session_b);


	ans_a = switch_channel_test_flag(chan_a, CF_ANSWERED);
	ans_b = switch_channel_test_flag(chan_b, CF_ANSWERED);


	while (data->running > 0 && *his_status > 0) {
		switch_channel_state b_state = switch_channel_get_state(chan_b);

		switch (b_state) {
		case CS_HANGUP:
			data->running = -1;
			continue;
			break;
		default:
			break;
		}

		/* If this call is running on early media and it answers for real, pass it along... */
		if (!ans_b && switch_channel_test_flag(chan_a, CF_ANSWERED)) {
			if (!switch_channel_test_flag(chan_b, CF_ANSWERED)) {
				switch_channel_answer(chan_b);
			}
			ans_b++;
		}

		if (!ans_a && switch_channel_test_flag(chan_b, CF_ANSWERED)) {
			if (!switch_channel_test_flag(chan_a, CF_ANSWERED)) {
				switch_channel_answer(chan_a);
			}
			ans_a++;
		}

		/* if 1 channel has DTMF pass it to the other */
		if (switch_channel_has_dtmf(chan_a)) {
			char dtmf[128];
			switch_channel_dequeue_dtmf(chan_a, dtmf, sizeof(dtmf));
			switch_core_session_send_dtmf(session_b, dtmf);

			if (dtmf_callback) {
				if (dtmf_callback(session_a, dtmf, user_data, 0) != SWITCH_STATUS_SUCCESS) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s ended call via DTMF\n", switch_channel_get_name(chan_a));
					data->running = -1;
					break;
				}
			}
		}

		/* read audio from 1 channel and write it to the other */
		if (switch_core_session_read_frame(session_a, &read_frame, -1, stream_id) == SWITCH_STATUS_SUCCESS
			&& read_frame->datalen) {
			if (switch_core_session_write_frame(session_b, read_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "write: %s Bad Frame.... Bubye!\n", switch_channel_get_name(chan_b));
				data->running = -1;
			}
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "read: %s Bad Frame.... Bubye!\n", switch_channel_get_name(chan_a));
			data->running = -1;
		}

		switch_yield(1000);
	}
	//switch_channel_hangup(chan_b);
	data->running = 0;

	return NULL;
}


static switch_status audio_bridge_on_hangup(switch_core_session *session)
{
	switch_core_session *other_session;
	switch_channel *channel = NULL, *other_channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	other_session = switch_channel_get_private(channel);
	assert(other_session != NULL);

	other_channel = switch_core_session_get_channel(other_session);
	assert(other_channel != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "CUSTOM HANGUP %s kill %s\n", switch_channel_get_name(channel),
						  switch_channel_get_name(other_channel));

	//switch_core_session_kill_channel(other_session, SWITCH_SIG_KILL);
	//switch_core_session_kill_channel(session, SWITCH_SIG_KILL);
	if (switch_channel_test_flag(channel, CF_ORIGINATOR) && !switch_channel_test_flag(other_channel, CF_TRANSFER)) {
		switch_core_session_kill_channel(other_session, SWITCH_SIG_KILL);
		switch_channel_hangup(other_channel);
	}
	
	switch_channel_clear_flag(channel, CF_ORIGINATOR);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status audio_bridge_on_ring(switch_core_session *session)
{
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "CUSTOM RING\n");

	/* put the channel in a passive state so we can loop audio to it */
	if (switch_channel_test_flag(channel, CF_OUTBOUND)) {
		switch_channel_set_state(channel, CS_TRANSMIT);
		return SWITCH_STATUS_FALSE;
	}


	return SWITCH_STATUS_SUCCESS;
}

static const switch_state_handler_table audio_bridge_peer_state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ audio_bridge_on_ring,
	/*.on_execute */ NULL,
	/*.on_hangup */ audio_bridge_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};

static const switch_state_handler_table audio_bridge_caller_state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ audio_bridge_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};

SWITCH_DECLARE(switch_status) switch_ivr_multi_threaded_bridge(switch_core_session *session, 
															   switch_core_session *peer_session,
															   unsigned int timelimit,
															   switch_dtmf_callback_function dtmf_callback,
															   void *session_data,
															   void *peer_session_data)
															   

															   
{
	struct switch_core_thread_session this_audio_thread, other_audio_thread;
	switch_channel *caller_channel, *peer_channel;
	time_t start;
	int stream_id = 0;
	switch_frame *read_frame;

	

	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);

	switch_channel_set_flag(caller_channel, CF_ORIGINATOR);

	peer_channel = switch_core_session_get_channel(peer_session);
	assert(peer_channel != NULL);

	memset(&other_audio_thread, 0, sizeof(other_audio_thread));
	memset(&this_audio_thread, 0, sizeof(this_audio_thread));
	other_audio_thread.objs[0] = session;
	other_audio_thread.objs[1] = peer_session;
	other_audio_thread.objs[2] = &stream_id;
	other_audio_thread.objs[3] = dtmf_callback;
	other_audio_thread.objs[4] = session_data;
	other_audio_thread.objs[5] = &this_audio_thread.running;
	other_audio_thread.running = 5;

	this_audio_thread.objs[0] = peer_session;
	this_audio_thread.objs[1] = session;
	this_audio_thread.objs[2] = &stream_id;
	this_audio_thread.objs[3] = dtmf_callback;
	this_audio_thread.objs[4] = peer_session_data;
	this_audio_thread.objs[5] = &other_audio_thread.running;
	this_audio_thread.running = 2;


	switch_channel_set_private(caller_channel, peer_session);
	switch_channel_set_private(peer_channel, session);
	switch_channel_add_state_handler(caller_channel, &audio_bridge_caller_state_handlers);
	switch_channel_add_state_handler(peer_channel, &audio_bridge_peer_state_handlers);
	switch_core_session_thread_launch(peer_session);
	time(&start);

	for (;;) {
		int state = switch_channel_get_state(peer_channel);
		if (state > CS_RING) {
			break;
		}
		
		if (!switch_channel_ready(caller_channel)) {
			break;
		}
		
		if ((time(NULL) - start) > timelimit) {
			break;
		}
		switch_yield(1000);
	}

	switch_channel_pre_answer(caller_channel);
	

	while (switch_channel_ready(caller_channel) &&
		   switch_channel_ready(peer_channel) &&
		   !switch_channel_test_flag(peer_channel, CF_ANSWERED) &&
		   !switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA) &&
		   ((time(NULL) - start) < timelimit)) {



		/* read from the channel while we wait if the audio is up on it */
		if (switch_channel_test_flag(caller_channel, CF_ANSWERED) || switch_channel_test_flag(caller_channel, CF_EARLY_MEDIA)) {
			if (switch_core_session_read_frame(session, &read_frame, 1000, 0) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		} else {
			switch_yield(1000);
		}
	}

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
		switch_channel_answer(caller_channel);
	}

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) || 
		switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) {

		switch_core_session_launch_thread(session, audio_bridge_thread, (void *) &other_audio_thread);
		audio_bridge_thread(NULL, (void *) &this_audio_thread);

		//switch_channel_hangup(peer_channel);
		if (other_audio_thread.running > 0) {
			other_audio_thread.running = -1;
			/* wait for the other audio thread */
			while (other_audio_thread.running) {
				switch_yield(1000);
			}
		}
	}
	return SWITCH_STATUS_SUCCESS;
}
