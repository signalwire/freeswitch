/*
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "openzap.h"
#include "zap_analog.h"

zap_status_t zap_analog_configure_span(zap_span_t *span, char *tonemap, zio_signal_cb_t sig_cb)
{

	if (span->signal_type) {
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling.");
		return ZAP_FAIL;
	}
	
	span->analog_data = malloc(sizeof(*span->analog_data));
	assert(span->analog_data != NULL);
	memset(span->analog_data, 0, sizeof(*span->analog_data));
	span->signal_type = ZAP_SIGTYPE_ANALOG;
	zap_span_load_tones(span, tonemap);
	
	return ZAP_SUCCESS;

}

static int teletone_handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	zap_buffer_t *dt_buffer = ts->user_data;
	int wrote;

	if (!dt_buffer) {
		return -1;
	}
	wrote = teletone_mux_tones(ts, map);
	zap_buffer_write(dt_buffer, ts->buffer, wrote * 2);
	return 0;
}

static void *zap_analog_channel_run(zap_thread_t *me, void *obj)
{
	zap_channel_t *chan = (zap_channel_t *) obj;
	zap_buffer_t *dt_buffer = NULL;
	teletone_generation_session_t ts;
	uint8_t frame[1024];
	zap_size_t len, rlen;
	zap_codec_t codec = ZAP_CODEC_SLIN, old_codec;
	char *tones[4] = {0};
	time_t start;
	int isbz = 0;
	int wtime = 10;
	zap_tone_type_t tt = ZAP_TONE_DTMF;
	int play_tones = 1;
	

	tones[0] = chan->span->tone_map[ZAP_TONEMAP_DIAL];
	tones[1] = chan->span->tone_map[ZAP_TONEMAP_BUSY];
	tones[2] = chan->span->tone_map[ZAP_TONEMAP_ATTN];
	
	zap_log(ZAP_LOG_DEBUG, "ANALOG CHANNEL thread starting.\n");

	if (zap_channel_open_chan(chan) != ZAP_SUCCESS) {
		goto done;
	}

	if (zap_buffer_create(&dt_buffer, 1024, 3192, 0) != ZAP_SUCCESS) {
		goto done;
	}

	if (zap_channel_command(chan, ZAP_COMMAND_ENABLE_TONE_DETECT, &tt) != ZAP_SUCCESS) {
		goto done;
	}

	zap_set_flag_locked(chan, ZAP_CHANNEL_INTHREAD);
	zap_set_state_locked(chan, ZAP_CHANNEL_STATE_DIALTONE);

	teletone_init_session(&ts, 0, teletone_handler, dt_buffer);
#if 0
	ts.debug = 1;
	ts.debug_stream = stdout;
#endif
	ts.rate = 8000;
	teletone_run(&ts, tones[isbz++]);
	zap_channel_command(chan, ZAP_COMMAND_GET_CODEC, &old_codec);
	zap_channel_command(chan, ZAP_COMMAND_SET_CODEC, &codec);
	zap_buffer_set_loops(dt_buffer, -1);

	time(&start);

	while (chan->state >= ZAP_CHANNEL_STATE_DIALTONE && zap_test_flag(chan, ZAP_CHANNEL_INTHREAD)) {
		zap_wait_flag_t flags = ZAP_READ;
		char dtmf[128];
		zap_size_t dlen = 0;

		len = sizeof(frame);

		if (play_tones && tones[isbz] && time(NULL) - start > wtime) {
			zap_buffer_zero(dt_buffer);
			teletone_run(&ts, tones[isbz++]);
			time(&start);
			wtime *= 2;
		}

		if ((dlen = zap_channel_dequeue_dtmf(chan, dtmf, sizeof(dtmf)))) {
			printf("DTMF %s\n", dtmf);
			play_tones = 0;
		}

		if (zap_channel_wait(chan, &flags, -1) == ZAP_FAIL) {
			goto done;
		}

		if (flags & ZAP_READ) {
			if (zap_channel_read(chan, frame, &len) == ZAP_SUCCESS) {
				rlen = zap_buffer_read_loop(dt_buffer, frame, len);
				if (play_tones) {
					zap_channel_write(chan, frame, &rlen);
				}
			} else {
				goto done;
			}
		}
	}
	
 done:
	zap_channel_command(chan, ZAP_COMMAND_SET_CODEC, &old_codec);
	if (ts.buffer) {
		teletone_destroy_session(&ts);
	}
	if (dt_buffer) {
		zap_buffer_destroy(&dt_buffer);
	}

	zap_clear_flag(chan, ZAP_CHANNEL_INTHREAD);
	zap_channel_close(&chan);
	zap_log(ZAP_LOG_DEBUG, "ANALOG CHANNEL thread ended. %d\n", old_codec);
	return NULL;
}

static zap_status_t process_event(zap_span_t *span, zap_event_t *event)
{
	zap_log(ZAP_LOG_DEBUG, "EVENT [%s][%d:%d]\n", zap_oob_event2str(event->enum_id), event->channel->span_id, event->channel->chan_id);

	switch(event->enum_id) {
	case ZAP_OOB_ONHOOK:
		{
			zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_DOWN);
		}
		break;
	case ZAP_OOB_OFFHOOK:
		{
			if (!zap_test_flag(event->channel, ZAP_CHANNEL_INTHREAD)) {
				zap_thread_create_detached(zap_analog_channel_run, event->channel);
			}
		}
	}


	return ZAP_SUCCESS;
}

static void *zap_analog_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	zap_analog_data_t *data = span->analog_data;

	zap_log(ZAP_LOG_DEBUG, "ANALOG thread starting.\n");

	while(zap_test_flag(data, ZAP_ANALOG_RUNNING)) {
		int waitms = 10;
		zap_status_t status;

		status = zap_span_poll_event(span, waitms);
		
		switch(status) {
		case ZAP_SUCCESS:
			{
				zap_event_t *event;
				while (zap_span_next_event(span, &event) == ZAP_SUCCESS) {
					if (process_event(span, event) != ZAP_SUCCESS) {
						goto end;
					}
				}
			}
			break;
		case ZAP_FAIL:
			{
				zap_log(ZAP_LOG_DEBUG, "Failure!\n");
				goto end;
			}
			break;
		default:
			break;
		}

	}

 end:

	zap_clear_flag(data, ZAP_ANALOG_RUNNING);
	
	zap_log(ZAP_LOG_DEBUG, "ANALOG thread ending.\n");

	return NULL;
}



zap_status_t zap_analog_start(zap_span_t *span)
{
	zap_set_flag(span->analog_data, ZAP_ANALOG_RUNNING);
	return zap_thread_create_detached(zap_analog_run, span);
}

