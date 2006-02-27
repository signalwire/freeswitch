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

	while (switch_channel_get_state(channel) == CS_EXECUTE) {
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
															  char *terminator)
{
	unsigned int i = 0, x =  (unsigned int) strlen(buf);
	switch_channel *channel;
	switch_status status = SWITCH_STATUS_SUCCESS;
	
	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	*terminator = '\0';

	for (i = 0 ; i < x; i++) {
		if (strchr(terminators, buf[i])) {
			*terminator = buf[i];
			return SWITCH_STATUS_SUCCESS;
		}
	}

	while (switch_channel_get_state(channel) == CS_EXECUTE) {
		switch_frame *read_frame;

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
		
		if ((status = switch_core_session_read_frame(session, &read_frame, -1, 0)) != SWITCH_STATUS_SUCCESS) {
			break;
		}
	}

	return status;
}



SWITCH_DECLARE(switch_status) switch_ivr_record_file(switch_core_session *session, 
													 char *file,
													 switch_dtmf_callback_function dtmf_callback,
													 void *buf,
													 unsigned int buflen)
{
	switch_channel *channel;
    char dtmf[128];
	switch_file_handle fh;
	switch_frame *read_frame;
	switch_codec codec, *read_codec;
	char *codec_name;
	switch_status status = SWITCH_STATUS_SUCCESS;
	
	memset(&fh, 0, sizeof(fh));

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	read_codec = switch_core_session_get_read_codec(session);
	assert(read_codec != NULL);

	fh.channels = read_codec->implementation->number_of_channels;
	fh.samplerate = read_codec->implementation->samples_per_second;


	if (switch_core_file_open(&fh,
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
							  codec_name, fh.samplerate, fh.channels, read_codec->implementation->microseconds_per_frame / 1000);
		switch_core_file_close(&fh);
		return SWITCH_STATUS_GENERR;
	}
	

	while (switch_channel_get_state(channel) == CS_EXECUTE) {
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

		len = (size_t) read_frame->datalen / 2;
		switch_core_file_write(&fh, read_frame->data, &len);
	}

	switch_core_session_set_read_codec(session, read_codec);
	switch_core_file_close(&fh);

	return status;
}

SWITCH_DECLARE(switch_status) switch_ivr_play_file(switch_core_session *session, 
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
	size_t len = 0, ilen = 0;
	switch_frame write_frame;
	switch_timer timer;
	switch_core_thread_session thread_session;
	switch_codec codec;
	switch_memory_pool *pool = switch_core_session_get_pool(session);
	switch_file_handle fh;
	char *codec_name;
	int x;
	int stream_id;
	switch_status status = SWITCH_STATUS_SUCCESS;

	memset(&fh, 0, sizeof(fh));

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_core_file_open(&fh,
							  file,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
							  switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel);
		return SWITCH_STATUS_GENERR;
	}

	switch_channel_answer(channel);

	write_frame.data = abuf;
	write_frame.buflen = sizeof(abuf);


	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OPEN FILE %s %dhz %d channels\n", file, fh.samplerate, fh.channels);

	interval = 20;
	samples = (fh.samplerate / 50) * fh.channels;
	len = samples * 2;

	codec_name = "L16";

	if (switch_core_codec_init(&codec,
							   codec_name,
							   fh.samplerate,
							   interval,
							   fh.channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, pool) == SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activated\n");
		write_frame.codec = &codec;
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activation Failed %s@%dhz %d channels %dms\n",
							  codec_name, fh.samplerate, fh.channels, interval);
		switch_core_file_close(&fh);
		return SWITCH_STATUS_GENERR;
	}

	if (timer_name) {
		if (switch_core_timer_init(&timer, timer_name, interval, samples, pool) != SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "setup timer failed!\n");
			switch_core_codec_destroy(&codec);
			switch_core_file_close(&fh);
			return SWITCH_STATUS_GENERR;
		}
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "setup timer success %d bytes per %d ms!\n", len, interval);
	}
	write_frame.rate = fh.samplerate;

	if (timer_name) {
		/* start a thread to absorb incoming audio */
		for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
			switch_core_service_session(session, &thread_session, stream_id);
		}
	}

	ilen = samples;
	while (switch_channel_get_state(channel) == CS_EXECUTE) {
		int done = 0;

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
		
		switch_core_file_read(&fh, abuf, &ilen);

		if (done || ilen <= 0) {
			break;
		}

		write_frame.datalen = ilen * 2;
		write_frame.samples = (int) ilen;
#ifdef SWAP_LINEAR
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
	switch_core_file_close(&fh);

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
	int interval = 0, samples = 0;
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
								rate,
								flags,
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
							   rate,
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
		if (switch_core_timer_init(&timer, timer_name, interval, samples, pool) != SWITCH_STATUS_SUCCESS) {
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
	write_frame.rate = rate;

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
	while (switch_channel_get_state(channel) == CS_EXECUTE) {

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
#ifdef SWAP_LINEAR
		switch_swap_linear(write_frame.data, (int) write_frame.datalen);
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
	switch_core_codec_destroy(&codec);
	flags = 0;
	switch_core_codec_destroy(&codec);

	if (timer_name) {
		/* End the audio absorbing thread */
		switch_core_thread_session_end(&thread_session);
		switch_core_timer_destroy(&timer);
	}

	return status;
}



