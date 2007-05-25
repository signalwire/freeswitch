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

zap_status_t zap_analog_configure_span(zap_span_t *span, char *tonemap, uint32_t digit_timeout, uint32_t max_dialstr, zio_signal_cb_t sig_cb)
{

	assert(sig_cb != NULL);

	if (span->signal_type) {
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling.");
		return ZAP_FAIL;
	}
	
	if (digit_timeout < 2000 || digit_timeout > 10000) {
		digit_timeout = 2000;
	}

	if (max_dialstr < 2 || max_dialstr > 20) {
		max_dialstr = 11;
	}

	span->analog_data = malloc(sizeof(*span->analog_data));
	memset(span->analog_data, 0, sizeof(*span->analog_data));
	assert(span->analog_data != NULL);

	span->analog_data->digit_timeout = digit_timeout;
	span->analog_data->max_dialstr = max_dialstr;
	span->analog_data->sig_cb = sig_cb;
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
	zap_tone_type_t tt = ZAP_TONE_DTMF;
	char dtmf[128];
	int dtmf_offset = 0;
	zap_analog_data_t *data = chan->span->analog_data;
	zap_channel_t *closed_chan;
	uint32_t state_counter = 0, elapsed = 0, interval = 0, last_digit = 0, indicate = 0;
	zap_sigmsg_t sig;
	
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
	teletone_init_session(&ts, 0, teletone_handler, dt_buffer);
	ts.rate = 8000;

	zap_channel_command(chan, ZAP_COMMAND_GET_INTERVAL, &interval);
	zap_buffer_set_loops(dt_buffer, -1);

	while (zap_test_flag(chan, ZAP_CHANNEL_INTHREAD)) {
		zap_wait_flag_t flags = ZAP_READ;
		zap_size_t dlen = 0;
		
		len = sizeof(frame);
		
		elapsed += interval;
		state_counter += interval;

		if (!zap_test_flag(chan, ZAP_CHANNEL_STATE_CHANGE)) {
			switch(chan->state) {
			case ZAP_CHANNEL_STATE_DIALTONE:
				{
					if (state_counter > 10000) {
						zap_set_state_locked(chan, ZAP_CHANNEL_STATE_BUSY);
					}
				}
				break;
			case ZAP_CHANNEL_STATE_BUSY:
				{
					if (state_counter > 20000) {
						zap_set_state_locked(chan, ZAP_CHANNEL_STATE_ATTN);
					}
				}
				break;
			case ZAP_CHANNEL_STATE_UP:
			case ZAP_CHANNEL_STATE_IDLE:
				{

					zap_sleep(interval);
					continue;
				}
				break;
			default:
				break;

			}
		} else {

			zap_clear_flag_locked(chan, ZAP_CHANNEL_STATE_CHANGE);
			switch(chan->state) {
			case ZAP_CHANNEL_STATE_UP:
				{
					sig.event_id = ZAP_SIGEVENT_UP;
					data->sig_cb(&sig);
					
					continue;
				}
				break;
			case ZAP_CHANNEL_STATE_IDLE:
				{
					memset(&sig, 0, sizeof(sig));
					sig.event_id = ZAP_SIGEVENT_START;
					sig.chan_id = chan->chan_id;
					sig.span_id = chan->span_id;
					sig.channel = chan;
					sig.span = chan->span;
					zap_copy_string(sig.dnis, dtmf, sizeof(sig.dnis));
					data->sig_cb(&sig);
					continue;
				}
				break;
			case ZAP_CHANNEL_STATE_DOWN:
				{
					sig.event_id = ZAP_SIGEVENT_STOP;
					data->sig_cb(&sig);
					goto done;
				}
				break;
			case ZAP_CHANNEL_STATE_DIALTONE:
				{
					zap_buffer_zero(dt_buffer);
					teletone_run(&ts, chan->span->tone_map[ZAP_TONEMAP_DIAL]);
					indicate = 1;
				}
				break;

			case ZAP_CHANNEL_STATE_RING:
				{
					zap_buffer_zero(dt_buffer);
					teletone_run(&ts, chan->span->tone_map[ZAP_TONEMAP_RING]);
					indicate = 1;
				}
				break;
				
			case ZAP_CHANNEL_STATE_BUSY:
				{
					zap_buffer_zero(dt_buffer);
					teletone_run(&ts, chan->span->tone_map[ZAP_TONEMAP_BUSY]);
					indicate = 1;
				}
				break;

			case ZAP_CHANNEL_STATE_ATTN:
				{
					zap_buffer_zero(dt_buffer);
					teletone_run(&ts, chan->span->tone_map[ZAP_TONEMAP_ATTN]);
					indicate = 1;
				}
				break;
			default:
				indicate = 0;
				break;
			}

			state_counter = 0;
		}
		

		if ((dlen = zap_channel_dequeue_dtmf(chan, dtmf + dtmf_offset, sizeof(dtmf) - strlen(dtmf)))) {
			if (chan->state == ZAP_CHANNEL_STATE_DIALTONE || chan->state == ZAP_CHANNEL_STATE_COLLECT) {
				zap_log(ZAP_LOG_DEBUG, "DTMF %s\n", dtmf + dtmf_offset);
				if (chan->state == ZAP_CHANNEL_STATE_DIALTONE) {
					zap_set_state_locked(chan, ZAP_CHANNEL_STATE_COLLECT);
				}
				dtmf_offset = strlen(dtmf);
				last_digit = elapsed;
			}
		}

		if (last_digit && ((elapsed - last_digit > data->digit_timeout) || strlen(dtmf) > data->max_dialstr)) {
			zap_log(ZAP_LOG_DEBUG, "Number obtained [%s]\n", dtmf);
			zap_set_state_locked(chan, ZAP_CHANNEL_STATE_IDLE);
			last_digit = 0;
		}
		
		if (zap_channel_wait(chan, &flags, -1) == ZAP_FAIL) {
			goto done;
		}
		
		if (flags & ZAP_READ) {
			if (zap_channel_read(chan, frame, &len) == ZAP_SUCCESS) {
				if (chan->effective_codec != ZAP_CODEC_SLIN) {
					len *= 2;
				}
				rlen = zap_buffer_read_loop(dt_buffer, frame, len);
				
				if (indicate) {		
					zio_codec_t codec_func = NULL;
					zap_status_t status;
					
					if (chan->effective_codec != ZAP_CODEC_SLIN) {
						if (chan->native_codec == ZAP_CODEC_ULAW) {
							codec_func = zio_slin2ulaw;
						} else if (chan->native_codec == ZAP_CODEC_ALAW) {
							codec_func = zio_slin2alaw;
						}
						
						if (codec_func) {
							status = codec_func(frame, sizeof(frame), &rlen);
						} else {
							snprintf(chan->last_error, sizeof(chan->last_error), "codec error!");
							status = ZAP_FAIL;
						}
					}

					zap_channel_write(chan, frame, &rlen);
				}
			} else {
				goto done;
			}
		}
	}
	
 done:


	closed_chan = chan;
	zap_channel_close(&chan);

	zap_channel_command(closed_chan, ZAP_COMMAND_SET_NATIVE_CODEC, NULL);

	if (ts.buffer) {
		teletone_destroy_session(&ts);
	}
	if (dt_buffer) {
		zap_buffer_destroy(&dt_buffer);
	}

	zap_clear_flag(closed_chan, ZAP_CHANNEL_INTHREAD);
	
	zap_log(ZAP_LOG_DEBUG, "ANALOG CHANNEL thread ended.\n");
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
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_DIALTONE);
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

