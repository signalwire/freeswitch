/*
 * Copyright (c) 2008-2012, Anthony Minessale II
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
 *
 * Contributor(s):
 *
 * John Wehle (john@feith.com)
 *
 */

#include "openzap.h"
#include "zap_analog_em.h"

#ifndef localtime_r
struct tm * localtime_r(const time_t *clock, struct tm *result);
#endif

static void *zap_analog_em_channel_run(zap_thread_t *me, void *obj);

/**
 * \brief Starts an EM channel thread (outgoing call)
 * \param zchan Channel to initiate call on
 * \return Success or failure
 *
 * Initialises state, starts tone progress detection and runs the channel in a new a thread.
 */
static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(analog_em_outgoing_call)
{
	if (!zap_test_flag(zchan, ZAP_CHANNEL_OFFHOOK) && !zap_test_flag(zchan, ZAP_CHANNEL_INTHREAD)) {		
		zap_channel_clear_needed_tones(zchan);
		zap_channel_clear_detected_tones(zchan);

		zap_set_flag(zchan, ZAP_CHANNEL_OUTBOUND);

		zap_channel_command(zchan, ZAP_COMMAND_OFFHOOK, NULL);
		zap_channel_command(zchan, ZAP_COMMAND_ENABLE_PROGRESS_DETECT, NULL);
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DIALING);
		zap_thread_create_detached(zap_analog_em_channel_run, zchan);
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

/**
 * \brief Starts an EM span thread (monitor)
 * \param span Span to monitor
 * \return Success or failure
 */
static zap_status_t zap_analog_em_start(zap_span_t *span)
{
	zap_analog_em_data_t *analog_data = span->signal_data;
	zap_set_flag(analog_data, ZAP_ANALOG_EM_RUNNING);
	return zap_thread_create_detached(zap_analog_em_run, span);
}

/**
 * \brief Initialises an EM span from configuration variables
 * \param span Span to configure
 * \param sig_cb Callback function for event signals
 * \param ap List of configuration variables
 * \return Success or failure
 */
static ZIO_SIG_CONFIGURE_FUNCTION(zap_analog_em_configure_span)
//zap_status_t zap_analog_em_configure_span(zap_span_t *span, char *tonemap, uint32_t digit_timeout, uint32_t max_dialstr, zio_signal_cb_t sig_cb)
{
	zap_analog_em_data_t *analog_data;
	const char *tonemap = "us";
	uint32_t digit_timeout = 10;
	uint32_t max_dialstr = 11;
	const char *var, *val;
	int *intval;

	assert(sig_cb != NULL);

	if (span->signal_type) {
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling.");
		return ZAP_FAIL;
	}
	
	analog_data = malloc(sizeof(*analog_data));
	assert(analog_data != NULL);
	memset(analog_data, 0, sizeof(*analog_data));

	while((var = va_arg(ap, char *))) {
		if (!strcasecmp(var, "tonemap")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			tonemap = val;
		} else if (!strcasecmp(var, "digit_timeout")) {
			if (!(intval = va_arg(ap, int *))) {
				break;
			}
			digit_timeout = *intval;
		} else if (!strcasecmp(var, "max_dialstr")) {
			if (!(intval = va_arg(ap, int *))) {
				break;
			}
			max_dialstr = *intval;
		} else {
			snprintf(span->last_error, sizeof(span->last_error), "Unknown parameter [%s]", var);
			return ZAP_FAIL;
		}
	}


	if (digit_timeout < 2000 || digit_timeout > 10000) {
		digit_timeout = 2000;
	}

	if (max_dialstr < 2 || max_dialstr > MAX_DIALSTRING) {
		zap_log(ZAP_LOG_ERROR, "Invalid max_dialstr, setting to %d\n", MAX_DIALSTRING);
		max_dialstr = MAX_DIALSTRING;
	}

	span->start = zap_analog_em_start;
	analog_data->digit_timeout = digit_timeout;
	analog_data->max_dialstr = max_dialstr;
	span->signal_cb = sig_cb;
	span->signal_type = ZAP_SIGTYPE_ANALOG;
	span->signal_data = analog_data;
	span->outgoing_call = analog_em_outgoing_call;
	zap_span_load_tones(span, tonemap);

	return ZAP_SUCCESS;

}

/**
 * \brief Retrieves tone generation output to be sent
 * \param ts Teletone generator
 * \param map Tone map
 * \return -1 on error, 0 on success
 */
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

/**
 * \brief Main thread function for EM channel (outgoing call)
 * \param me Current thread
 * \param obj Channel to run in this thread
 */
static void *zap_analog_em_channel_run(zap_thread_t *me, void *obj)
{
	zap_channel_t *zchan = (zap_channel_t *) obj;
	zap_buffer_t *dt_buffer = NULL;
	teletone_generation_session_t ts;
	uint8_t frame[1024];
	zap_size_t len, rlen;
	zap_tone_type_t tt = ZAP_TONE_DTMF;
	char dtmf[128] = "";
	zap_size_t dtmf_offset = 0;
	zap_analog_em_data_t *analog_data = zchan->span->signal_data;
	zap_channel_t *closed_chan;
	uint32_t state_counter = 0, elapsed = 0, collecting = 0, interval = 0, last_digit = 0, indicate = 0, dial_timeout = 30000;
	zap_sigmsg_t sig;
	zap_status_t status;
	
	zap_log(ZAP_LOG_DEBUG, "ANALOG EM CHANNEL thread starting.\n");

	ts.buffer = NULL;

	if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "OPEN ERROR [%s]\n", zchan->last_error);
		goto done;
	}

	if (zap_buffer_create(&dt_buffer, 1024, 3192, 0) != ZAP_SUCCESS) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "memory error!");
		zap_log(ZAP_LOG_ERROR, "MEM ERROR\n");
		goto done;
	}

	if (zap_channel_command(zchan, ZAP_COMMAND_ENABLE_DTMF_DETECT, &tt) != ZAP_SUCCESS) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "error initilizing tone detector!");
		zap_log(ZAP_LOG_ERROR, "TONE ERROR\n");
		goto done;
	}

	zap_set_flag_locked(zchan, ZAP_CHANNEL_INTHREAD);
	teletone_init_session(&ts, 0, teletone_handler, dt_buffer);
	ts.rate = 8000;
#if 0
	ts.debug = 1;
	ts.debug_stream = stdout;
#endif
	zap_channel_command(zchan, ZAP_COMMAND_GET_INTERVAL, &interval);
	zap_buffer_set_loops(dt_buffer, -1);
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = zchan->chan_id;
	sig.span_id = zchan->span_id;
	sig.channel = zchan;
	
	assert(interval != 0);

	while (zap_running() && zap_test_flag(zchan, ZAP_CHANNEL_INTHREAD)) {
		zap_wait_flag_t flags = ZAP_READ;
		zap_size_t dlen = 0;
		
		len = sizeof(frame);
		
		elapsed += interval;
		state_counter += interval;
		
		if (!zap_test_flag(zchan, ZAP_CHANNEL_STATE_CHANGE)) {
			switch(zchan->state) {
			case ZAP_CHANNEL_STATE_DIALING:
				{
					if (! zchan->needed_tones[ZAP_TONEMAP_RING]
						&& zap_test_flag(zchan, ZAP_CHANNEL_WINK)) {
						if (zap_strlen_zero(zchan->caller_data.ani.digits)) {
							zap_log(ZAP_LOG_ERROR, "No Digits to send!\n");
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_BUSY);
						} else {
							if (zap_channel_command(zchan, ZAP_COMMAND_SEND_DTMF, zchan->caller_data.ani.digits) != ZAP_SUCCESS) {
								zap_log(ZAP_LOG_ERROR, "Send Digits Failed [%s]\n", zchan->last_error);
								zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_BUSY);
							} else {
								state_counter = 0;
								zchan->needed_tones[ZAP_TONEMAP_RING] = 1;
								zchan->needed_tones[ZAP_TONEMAP_BUSY] = 1;
								zchan->needed_tones[ZAP_TONEMAP_FAIL1] = 1;
								zchan->needed_tones[ZAP_TONEMAP_FAIL2] = 1;
								zchan->needed_tones[ZAP_TONEMAP_FAIL3] = 1;
								dial_timeout = ((zchan->dtmf_on + zchan->dtmf_off) * strlen(zchan->caller_data.ani.digits)) + 2000;
							}
						}
						break;
					}
					if (state_counter > dial_timeout) {
						if (!zap_test_flag(zchan, ZAP_CHANNEL_WINK)) {
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_BUSY);
						} else {
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_UP);
						}
					} 
				}
				break;
			case ZAP_CHANNEL_STATE_DIALTONE:
				{
					if (state_counter > 10000) {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_BUSY);
					}
				}
				break;
			case ZAP_CHANNEL_STATE_BUSY:
				{
					if (state_counter > 20000) {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_ATTN);
					}
				}
				break;
			case ZAP_CHANNEL_STATE_ATTN:
				{
					if (state_counter > 20000) {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
					}
				}
				break;
			case ZAP_CHANNEL_STATE_HANGUP:
				{
					if (state_counter > 500) {
						if (zap_test_flag(zchan, ZAP_CHANNEL_OFFHOOK) && 
							(zchan->last_state == ZAP_CHANNEL_STATE_RING || zchan->last_state == ZAP_CHANNEL_STATE_DIALTONE 
							 || zchan->last_state >= ZAP_CHANNEL_STATE_IDLE)) {
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_BUSY);
						} else {
							zchan->caller_data.hangup_cause = ZAP_CAUSE_NORMAL_CLEARING;
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
						}
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
			case ZAP_CHANNEL_STATE_DOWN:
				{
					goto done;
				}
				break;
			default:
				break;
			}
		} else {
			zap_clear_flag_locked(zchan, ZAP_CHANNEL_STATE_CHANGE);
			zap_clear_flag_locked(zchan->span, ZAP_SPAN_STATE_CHANGE);
			zap_channel_complete_state(zchan);
			indicate = 0;
			state_counter = 0;

			zap_log(ZAP_LOG_DEBUG, "Executing state handler on %d:%d for %s\n", 
					zchan->span_id, zchan->chan_id,
					zap_channel_state2str(zchan->state));
			switch(zchan->state) {
			case ZAP_CHANNEL_STATE_UP:
				{
					zap_channel_use(zchan);
					zap_channel_clear_needed_tones(zchan);
					zap_channel_flush_dtmf(zchan);

					if (!zap_test_flag(zchan, ZAP_CHANNEL_OFFHOOK)) {
						zap_channel_command(zchan, ZAP_COMMAND_OFFHOOK, NULL);
					}

					sig.event_id = ZAP_SIGEVENT_UP;

					zap_span_send_signal(zchan->span, &sig);
					continue;
				}
				break;
			case ZAP_CHANNEL_STATE_DIALING:
				{
					zap_channel_use(zchan);
				}
				break;
			case ZAP_CHANNEL_STATE_IDLE:
				{
					zap_channel_use(zchan);

					if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
						zap_set_string(zchan->caller_data.dnis.digits, zchan->chan_number);
					} else {
						zap_set_string(zchan->caller_data.dnis.digits, dtmf);
					}

					sig.event_id = ZAP_SIGEVENT_START;

					zap_span_send_signal(zchan->span, &sig);
					continue;
				}
				break;
			case ZAP_CHANNEL_STATE_DOWN:
				{
					sig.event_id = ZAP_SIGEVENT_STOP;
					zap_span_send_signal(zchan->span, &sig);
					goto done;
				}
				break;
			case ZAP_CHANNEL_STATE_DIALTONE:
				{
					memset(&zchan->caller_data, 0, sizeof(zchan->caller_data));
					*dtmf = '\0';
					dtmf_offset = 0;
					zap_buffer_zero(dt_buffer);
					teletone_run(&ts, zchan->span->tone_map[ZAP_TONEMAP_DIAL]);
					indicate = 1;

					zap_channel_command(zchan, ZAP_COMMAND_WINK, NULL);
				}
				break;
			case ZAP_CHANNEL_STATE_RING:
				{
					zap_buffer_zero(dt_buffer);
					teletone_run(&ts, zchan->span->tone_map[ZAP_TONEMAP_RING]);
					indicate = 1;
				}
				break;
			case ZAP_CHANNEL_STATE_BUSY:
				{
					zchan->caller_data.hangup_cause = ZAP_CAUSE_NORMAL_CIRCUIT_CONGESTION;
					if (zap_test_flag(zchan, ZAP_CHANNEL_OFFHOOK) && !zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
						zap_buffer_zero(dt_buffer);
						teletone_run(&ts, zchan->span->tone_map[ZAP_TONEMAP_BUSY]);
						indicate = 1;
					} else {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
					}
				}
				break;
			case ZAP_CHANNEL_STATE_ATTN:
				{
					if (zap_test_flag(zchan, ZAP_CHANNEL_OFFHOOK) && !zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
						zap_buffer_zero(dt_buffer);
						teletone_run(&ts, zchan->span->tone_map[ZAP_TONEMAP_ATTN]);
						indicate = 1;
					} else {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
					}
				}
				break;
			default:
				break;
			}
		}


		if (zchan->state == ZAP_CHANNEL_STATE_DIALTONE || zchan->state == ZAP_CHANNEL_STATE_COLLECT) {
			if ((dlen = zap_channel_dequeue_dtmf(zchan, dtmf + dtmf_offset, sizeof(dtmf) - strlen(dtmf)))) {

				if (zchan->state == ZAP_CHANNEL_STATE_DIALTONE) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_COLLECT);
					collecting = 1;
				}
				dtmf_offset = strlen(dtmf);
				last_digit = elapsed;
				sig.event_id = ZAP_SIGEVENT_COLLECTED_DIGIT;
				sig.raw_data = dtmf;
				if (zap_span_send_signal(zchan->span, &sig) == ZAP_BREAK) {
					collecting = 0;
				}
			}
		}


		if (last_digit && (!collecting || ((elapsed - last_digit > analog_data->digit_timeout) || strlen(dtmf) > analog_data->max_dialstr))) {
			zap_log(ZAP_LOG_DEBUG, "Number obtained [%s]\n", dtmf);
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_IDLE);
			last_digit = 0;
			collecting = 0;
		}

		if (zap_channel_wait(zchan, &flags, interval * 2) != ZAP_SUCCESS) {
			continue;
		}

		if (!(flags & ZAP_READ)) {
			continue;
		}

		if (zap_channel_read(zchan, frame, &len) != ZAP_SUCCESS) {
			zap_log(ZAP_LOG_ERROR, "READ ERROR [%s]\n", zchan->last_error);
			goto done;
		}

		if (zchan->detected_tones[0]) {
			zap_sigmsg_t sig;
			int i;
			memset(&sig, 0, sizeof(sig));
			sig.chan_id = zchan->chan_id;
			sig.span_id = zchan->span_id;
			sig.channel = zchan;
			sig.event_id = ZAP_SIGEVENT_TONE_DETECTED;
			
			for (i = 1; i < ZAP_TONEMAP_INVALID; i++) {
				if (zchan->detected_tones[i]) {
					zap_log(ZAP_LOG_DEBUG, "Detected tone %s on %d:%d\n", zap_tonemap2str(i), zchan->span_id, zchan->chan_id);
					sig.raw_data = &i;
					zap_span_send_signal(zchan->span, &sig);
				}
			}
			
			if (zchan->detected_tones[ZAP_TONEMAP_BUSY] || 
				zchan->detected_tones[ZAP_TONEMAP_FAIL1] ||
				zchan->detected_tones[ZAP_TONEMAP_FAIL2] ||
				zchan->detected_tones[ZAP_TONEMAP_FAIL3] ||
				zchan->detected_tones[ZAP_TONEMAP_ATTN]
				) {
				zap_log(ZAP_LOG_ERROR, "Failure indication detected!\n");
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_BUSY);
			} else if (zchan->detected_tones[ZAP_TONEMAP_RING]) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_UP);
			}
			
			zap_channel_clear_detected_tones(zchan);
		}

		if ((zchan->dtmf_buffer && zap_buffer_inuse(zchan->dtmf_buffer))) {
			rlen = len;
			memset(frame, 0, len);
			zap_channel_write(zchan, frame, sizeof(frame), &rlen);
			continue;
		}
		
		if (!indicate) {
			continue;
		}

		if (zchan->effective_codec != ZAP_CODEC_SLIN) {
			len *= 2;
		}

		rlen = zap_buffer_read_loop(dt_buffer, frame, len);

		if (zchan->effective_codec != ZAP_CODEC_SLIN) {
			zio_codec_t codec_func = NULL;

			if (zchan->native_codec == ZAP_CODEC_ULAW) {
				codec_func = zio_slin2ulaw;
			} else if (zchan->native_codec == ZAP_CODEC_ALAW) {
				codec_func = zio_slin2alaw;
			}

			if (codec_func) {
				status = codec_func(frame, sizeof(frame), &rlen);
			} else {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "codec error!");
				goto done;
			}
		}

		zap_channel_write(zchan, frame, sizeof(frame), &rlen);
	}

 done:

	zap_channel_command(zchan, ZAP_COMMAND_ONHOOK, NULL);
	
	closed_chan = zchan;
	zap_channel_close(&zchan);

	zap_channel_command(closed_chan, ZAP_COMMAND_SET_NATIVE_CODEC, NULL);

	if (ts.buffer) {
		teletone_destroy_session(&ts);
	}

	if (dt_buffer) {
		zap_buffer_destroy(&dt_buffer);
	}

	zap_clear_flag(closed_chan, ZAP_CHANNEL_INTHREAD);

	zap_log(ZAP_LOG_DEBUG, "ANALOG EM CHANNEL thread ended.\n");

	return NULL;
}

/**
 * \brief Processes EM events coming from zaptel/dahdi
 * \param span Span on which the event was fired
 * \param event Event to be treated
 * \return Success or failure
 */
static __inline__ zap_status_t process_event(zap_span_t *span, zap_event_t *event)
{
	zap_sigmsg_t sig;
	int locked = 0;
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = event->channel->chan_id;
	sig.span_id = event->channel->span_id;
	sig.channel = event->channel;


	zap_log(ZAP_LOG_DEBUG, "EVENT [%s][%d:%d] STATE [%s]\n", 
			zap_oob_event2str(event->enum_id), event->channel->span_id, event->channel->chan_id, zap_channel_state2str(event->channel->state));

	zap_mutex_lock(event->channel->mutex);
	locked++;

	switch(event->enum_id) {
	case ZAP_OOB_ONHOOK:
		{
			if (event->channel->state != ZAP_CHANNEL_STATE_DOWN) {
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_DOWN);
			}

		}
		break;
	case ZAP_OOB_OFFHOOK:
		{
			if (zap_test_flag(event->channel, ZAP_CHANNEL_INTHREAD)) {
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_UP);
			} else {
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_DIALTONE);
				zap_mutex_unlock(event->channel->mutex);
				locked = 0;
				zap_thread_create_detached(zap_analog_em_channel_run, event->channel);
			}
		break;
		}
	case ZAP_OOB_WINK:
		{
			if (event->channel->state != ZAP_CHANNEL_STATE_DIALING) {
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_DOWN);
			} else {
				zap_set_flag_locked(event->channel, ZAP_CHANNEL_WINK);
			}

		}
		break;
	}
	if (locked) {
		zap_mutex_unlock(event->channel->mutex);
	}
	return ZAP_SUCCESS;
}

/**
 * \brief Main thread function for EM span (monitor)
 * \param me Current thread
 * \param obj Span to run in this thread
 */
static void *zap_analog_em_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	zap_analog_em_data_t *analog_data = span->signal_data;

	zap_log(ZAP_LOG_DEBUG, "ANALOG EM thread starting.\n");

	while(zap_running() && zap_test_flag(analog_data, ZAP_ANALOG_EM_RUNNING)) {
		int waitms = 10;
		zap_status_t status;

		status = zap_span_poll_event(span, waitms);
		
		switch(status) {
		case ZAP_SUCCESS:
			{
				zap_event_t *event;
				while (zap_span_next_event(span, &event) == ZAP_SUCCESS) {
					if (event->enum_id == ZAP_OOB_NOOP) {
						continue;
					}
					if (process_event(span, event) != ZAP_SUCCESS) {
						goto end;
					}
				}
			}
			break;
		case ZAP_FAIL:
			{
				zap_log(ZAP_LOG_ERROR, "Failure Polling event! [%s]\n", span->last_error);
			}
			break;
		default:
			break;
		}

	}

 end:

	zap_clear_flag(analog_data, ZAP_ANALOG_EM_RUNNING);
	
	zap_log(ZAP_LOG_DEBUG, "ANALOG EM thread ending.\n");

	return NULL;
}

/**
 * \brief Openzap analog EM module initialisation
 * \return Success
 */
static ZIO_SIG_LOAD_FUNCTION(zap_analog_em_init)
{
	return ZAP_SUCCESS;
}

/**
 * \brief Openzap analog EM module definition
 */
EX_DECLARE_DATA zap_module_t zap_module = {
	"analog_em",
	NULL,
	NULL,
	zap_analog_em_init,
	zap_analog_em_configure_span,
	NULL
};


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
