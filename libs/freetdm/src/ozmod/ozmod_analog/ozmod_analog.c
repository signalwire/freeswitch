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

#ifndef localtime_r
struct tm * localtime_r(const time_t *clock, struct tm *result);
#endif

static void *zap_analog_channel_run(zap_thread_t *me, void *obj);

static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(analog_fxo_outgoing_call)
{
	if (!zap_test_flag(zchan, ZAP_CHANNEL_OFFHOOK) && !zap_test_flag(zchan, ZAP_CHANNEL_INTHREAD)) {		
		zap_channel_clear_needed_tones(zchan);
		zap_channel_clear_detected_tones(zchan);

		zap_channel_command(zchan, ZAP_COMMAND_OFFHOOK, NULL);
		zap_channel_command(zchan, ZAP_COMMAND_ENABLE_PROGRESS_DETECT, NULL);
		zchan->needed_tones[ZAP_TONEMAP_DIAL] = 1;
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DIALING);
		zap_thread_create_detached(zap_analog_channel_run, zchan);
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(analog_fxs_outgoing_call)
{

	if (zap_test_flag(zchan, ZAP_CHANNEL_INTHREAD)) {
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_CALLWAITING);
	} else {
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_GENRING);
		zap_thread_create_detached(zap_analog_channel_run, zchan);
	}

	return ZAP_SUCCESS;
}


static zap_status_t zap_analog_start(zap_span_t *span)
{
	zap_analog_data_t *analog_data = span->signal_data;
	zap_set_flag(analog_data, ZAP_ANALOG_RUNNING);
	return zap_thread_create_detached(zap_analog_run, span);
}

static ZIO_SIG_CONFIGURE_FUNCTION(zap_analog_configure_span)
//zap_status_t zap_analog_configure_span(zap_span_t *span, char *tonemap, uint32_t digit_timeout, uint32_t max_dialstr, zio_signal_cb_t sig_cb)
{
	zap_analog_data_t *analog_data;
	char *tonemap = "us";
	uint32_t digit_timeout = 10;
	uint32_t max_dialstr = 11;
	char *var, *val;
	int *intval;

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

	analog_data = malloc(sizeof(*analog_data));
	memset(analog_data, 0, sizeof(*analog_data));
	assert(analog_data != NULL);

	while(var = va_arg(ap, char *)) {
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
		}
	}

	span->start = zap_analog_start;
	analog_data->digit_timeout = digit_timeout;
	analog_data->max_dialstr = max_dialstr;
	analog_data->sig_cb = sig_cb;
	span->signal_type = ZAP_SIGTYPE_ANALOG;
	span->signal_data = analog_data;
	span->outgoing_call = span->trunk_type == ZAP_TRUNK_FXS ? analog_fxs_outgoing_call : analog_fxo_outgoing_call;
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

static void send_caller_id(zap_channel_t *zchan)
{
	zap_fsk_data_state_t fsk_data;
	uint8_t databuf[1024] = "";
	char time_str[9];
	struct tm tm;
	time_t now;
	zap_mdmf_type_t mt = MDMF_INVALID;

	time(&now);
#ifdef WIN32
	_tzset();
	_localtime64_s(&tm, &now);
#else
	localtime_r(&now, &tm);
#endif
	strftime(time_str, sizeof(time_str), "%m%d%H%M", &tm);

	zap_fsk_data_init(&fsk_data, databuf, sizeof(databuf));
	zap_fsk_data_add_mdmf(&fsk_data, MDMF_DATETIME, (uint8_t *) time_str, 8);
					
	if (zap_strlen_zero(zchan->caller_data.cid_num.digits)) {
		mt = MDMF_NO_NUM;
		zap_set_string(zchan->caller_data.cid_num.digits, "O");
	} else if (!strcasecmp(zchan->caller_data.cid_num.digits, "P") || !strcasecmp(zchan->caller_data.cid_num.digits, "O")) {
		mt = MDMF_NO_NUM;
	} else {
		mt = MDMF_PHONE_NUM;
	}
	zap_fsk_data_add_mdmf(&fsk_data, mt, (uint8_t *) zchan->caller_data.cid_num.digits, (uint8_t)strlen(zchan->caller_data.cid_num.digits));

	if (zap_strlen_zero(zchan->caller_data.cid_name)) {
		mt = MDMF_NO_NAME;
		zap_set_string(zchan->caller_data.cid_name, "O");
	} else if (!strcasecmp(zchan->caller_data.cid_name, "P") || !strcasecmp(zchan->caller_data.cid_name, "O")) {
		mt = MDMF_NO_NAME;
	} else {
		mt = MDMF_PHONE_NAME;
	}
	zap_fsk_data_add_mdmf(&fsk_data, mt, (uint8_t *) zchan->caller_data.cid_name, (uint8_t)strlen(zchan->caller_data.cid_name));
					
	zap_fsk_data_add_checksum(&fsk_data);
	zap_channel_send_fsk_data(zchan, &fsk_data, -14);
}

static void *zap_analog_channel_run(zap_thread_t *me, void *obj)
{
	zap_channel_t *zchan = (zap_channel_t *) obj;
	zap_buffer_t *dt_buffer = NULL;
	teletone_generation_session_t ts;
	uint8_t frame[1024];
	zap_size_t len, rlen;
	zap_tone_type_t tt = ZAP_TONE_DTMF;
	char dtmf[128] = "";
	zap_size_t dtmf_offset = 0;
	zap_analog_data_t *analog_data = zchan->span->signal_data;
	zap_channel_t *closed_chan;
	uint32_t state_counter = 0, elapsed = 0, collecting = 0, interval = 0, last_digit = 0, indicate = 0, dial_timeout = 30000;
	zap_sigmsg_t sig;
	zap_status_t status;
	
	zap_log(ZAP_LOG_DEBUG, "ANALOG CHANNEL thread starting.\n");

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
			case ZAP_CHANNEL_STATE_GET_CALLERID:
				{
					if (state_counter > 5000 || !zap_test_flag(zchan, ZAP_CHANNEL_CALLERID_DETECT)) {
						zap_channel_command(zchan, ZAP_COMMAND_DISABLE_CALLERID_DETECT, NULL);
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_IDLE);
					}
				}
				break;
			case ZAP_CHANNEL_STATE_DIALING:
				{
					if (state_counter > dial_timeout) {
						if (zchan->needed_tones[ZAP_TONEMAP_DIAL]) {
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_BUSY);
						} else {
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_UP);
						}
					} 
				}
				break;
			case ZAP_CHANNEL_STATE_GENRING:
				{
					if (state_counter > 60000) {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
					} else if (!zchan->fsk_buffer || !zap_buffer_inuse(zchan->fsk_buffer)) {
						zap_sleep(interval);
						continue;
					}
				}
				break;
			case ZAP_CHANNEL_STATE_DIALTONE:
				{
					if (!zap_test_flag(zchan, ZAP_CHANNEL_HOLD) && state_counter > 10000) {
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
						if (zap_test_flag(zchan, ZAP_CHANNEL_RINGING)) {
							zap_channel_command(zchan, ZAP_COMMAND_GENERATE_RING_OFF, NULL);
						}
						
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
			case ZAP_CHANNEL_STATE_CALLWAITING:
				{
					int done = 0;
					
					if (zchan->detected_tones[ZAP_TONEMAP_CALLWAITING_ACK] == 1) {
						send_caller_id(zchan);
						zchan->detected_tones[ZAP_TONEMAP_CALLWAITING_ACK]++;
					} else if (state_counter > 600 && !zchan->detected_tones[ZAP_TONEMAP_CALLWAITING_ACK]) {
						send_caller_id(zchan);
						zchan->detected_tones[ZAP_TONEMAP_CALLWAITING_ACK]++;
					} else if (state_counter > 1000 && !zchan->detected_tones[ZAP_TONEMAP_CALLWAITING_ACK]) {
						done = 1;
					} else if (state_counter > 10000) {
						if (zchan->fsk_buffer) {
							zap_buffer_zero(zchan->fsk_buffer);
						} else {
							zap_buffer_create(&zchan->fsk_buffer, 128, 128, 0);
						}
						
						ts.user_data = zchan->fsk_buffer;
						teletone_run(&ts, zchan->span->tone_map[ZAP_TONEMAP_CALLWAITING_SAS]);
						ts.user_data = dt_buffer;
						done = 1;
					}

					if (done) {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_UP);
						zap_clear_flag_locked(zchan, ZAP_CHANNEL_STATE_CHANGE);
						zap_clear_flag_locked(zchan->span, ZAP_SPAN_STATE_CHANGE);
						zchan->detected_tones[ZAP_TONEMAP_CALLWAITING_ACK] = 0;
					}
				}
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
						
					if (zchan->type == ZAP_CHAN_TYPE_FXO && !zap_test_flag(zchan, ZAP_CHANNEL_OFFHOOK)) {
						zap_channel_command(zchan, ZAP_COMMAND_OFFHOOK, NULL);
					}

					if (zchan->fsk_buffer && zap_buffer_inuse(zchan->fsk_buffer)) {
						zap_log(ZAP_LOG_DEBUG, "Cancel FSK transmit due to early answer.\n");
						zap_buffer_zero(zchan->fsk_buffer);
					}

					if (zchan->type == ZAP_CHAN_TYPE_FXS && zap_test_flag(zchan, ZAP_CHANNEL_RINGING)) {
						zap_channel_command(zchan, ZAP_COMMAND_GENERATE_RING_OFF, NULL);
					}

					if (zchan->token_count == 1) {
						zap_clear_flag(zchan, ZAP_CHANNEL_HOLD);
					}

					if (zap_test_flag(zchan, ZAP_CHANNEL_HOLD)) {
						zap_clear_flag(zchan, ZAP_CHANNEL_HOLD);
						sig.event_id = ZAP_SIGEVENT_ADD_CALL;
					} else {
						sig.event_id = ZAP_SIGEVENT_UP;
					}

					analog_data->sig_cb(&sig);
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
					sig.event_id = ZAP_SIGEVENT_START;

					if (zchan->type == ZAP_CHAN_TYPE_FXO) {
						zap_set_string(zchan->caller_data.dnis.digits, zchan->chan_number);
					} else {
						zap_set_string(zchan->caller_data.dnis.digits, dtmf);
					}

					analog_data->sig_cb(&sig);
					continue;
				}
				break;
			case ZAP_CHANNEL_STATE_DOWN:
				{
					sig.event_id = ZAP_SIGEVENT_STOP;
					analog_data->sig_cb(&sig);
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
				}
				break;
			case ZAP_CHANNEL_STATE_CALLWAITING:
				{
					zchan->detected_tones[ZAP_TONEMAP_CALLWAITING_ACK] = 0;
					if (zchan->fsk_buffer) {
						zap_buffer_zero(zchan->fsk_buffer);
					} else {
						zap_buffer_create(&zchan->fsk_buffer, 128, 128, 0);
					}
					
					ts.user_data = zchan->fsk_buffer;
					teletone_run(&ts, zchan->span->tone_map[ZAP_TONEMAP_CALLWAITING_SAS]);
					teletone_run(&ts, zchan->span->tone_map[ZAP_TONEMAP_CALLWAITING_CAS]);
					ts.user_data = dt_buffer;
				}
				break;
			case ZAP_CHANNEL_STATE_GENRING:
				{
					send_caller_id(zchan);
					zap_channel_command(zchan, ZAP_COMMAND_GENERATE_RING_ON, NULL);
				}
				break;
			case ZAP_CHANNEL_STATE_GET_CALLERID:
				{
					memset(&zchan->caller_data, 0, sizeof(zchan->caller_data));
					zap_channel_command(zchan, ZAP_COMMAND_ENABLE_CALLERID_DETECT, NULL);
					continue;
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
				if (analog_data->sig_cb(&sig) == ZAP_BREAK) {
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

		if (zchan->type == ZAP_CHAN_TYPE_FXO && zchan->detected_tones[0]) {
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
					if (analog_data->sig_cb) {
						analog_data->sig_cb(&sig);
					}
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
			} else if (zchan->detected_tones[ZAP_TONEMAP_DIAL]) {
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
			} else if (zchan->detected_tones[ZAP_TONEMAP_RING]) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_UP);
			}
			
			zap_channel_clear_detected_tones(zchan);
		}

		if ((zchan->dtmf_buffer && zap_buffer_inuse(zchan->dtmf_buffer)) || (zchan->fsk_buffer && zap_buffer_inuse(zchan->fsk_buffer))) {
			rlen = len;
			memset(frame, 0, len);
			zap_channel_write(zchan, frame, sizeof(frame), &rlen);
			continue;
		}
		
		if (!indicate) {
			continue;
		}

		if (zchan->type == ZAP_CHAN_TYPE_FXO && !zap_test_flag(zchan, ZAP_CHANNEL_OFFHOOK)) {
			zap_channel_command(zchan, ZAP_COMMAND_OFFHOOK, NULL);
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


	if (zchan->type == ZAP_CHAN_TYPE_FXO && zap_test_flag(zchan, ZAP_CHANNEL_OFFHOOK)) {
		zap_channel_command(zchan, ZAP_COMMAND_ONHOOK, NULL);
	}

	if (zchan->type == ZAP_CHAN_TYPE_FXS && zap_test_flag(zchan, ZAP_CHANNEL_RINGING)) {
		zap_channel_command(zchan, ZAP_COMMAND_GENERATE_RING_OFF, NULL);
	}

	
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

	zap_log(ZAP_LOG_DEBUG, "ANALOG CHANNEL thread ended.\n");

	return NULL;
}

static __inline__ zap_status_t process_event(zap_span_t *span, zap_event_t *event)
{
	zap_sigmsg_t sig;
	zap_analog_data_t *analog_data = event->channel->span->signal_data;
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
	case ZAP_OOB_RING_START:
		{
			if (!event->channel->ring_count && (event->channel->state == ZAP_CHANNEL_STATE_DOWN && !zap_test_flag(event->channel, ZAP_CHANNEL_INTHREAD))) {
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_GET_CALLERID);
				event->channel->ring_count = 1;
				zap_mutex_unlock(event->channel->mutex);
				locked = 0;
				zap_thread_create_detached(zap_analog_channel_run, event->channel);
			} else {
				event->channel->ring_count++;
			}
		}
		break;
	case ZAP_OOB_ONHOOK:
		{
			if (zap_test_flag(event->channel, ZAP_CHANNEL_RINGING)) {
				zap_channel_command(event->channel, ZAP_COMMAND_GENERATE_RING_OFF, NULL);
			}

			if (event->channel->state != ZAP_CHANNEL_STATE_DOWN) {
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_DOWN);
			}

		}
		break;
	case ZAP_OOB_FLASH:
		{
			if (event->channel->state == ZAP_CHANNEL_STATE_CALLWAITING) {
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_UP);
				zap_clear_flag_locked(event->channel, ZAP_CHANNEL_STATE_CHANGE);
				zap_clear_flag_locked(event->channel->span, ZAP_SPAN_STATE_CHANGE);
				event->channel->detected_tones[ZAP_TONEMAP_CALLWAITING_ACK] = 0;
			} 

			zap_channel_rotate_tokens(event->channel);
			
			if (zap_test_flag(event->channel, ZAP_CHANNEL_HOLD) && event->channel->token_count != 1) {
				zap_set_state_locked(event->channel,  ZAP_CHANNEL_STATE_UP);
			} else {
				sig.event_id = ZAP_SIGEVENT_FLASH;
				analog_data->sig_cb(&sig);
			}
		}
		break;
	case ZAP_OOB_OFFHOOK:
		{
			if (event->channel->type == ZAP_CHAN_TYPE_FXS) {
				if (zap_test_flag(event->channel, ZAP_CHANNEL_INTHREAD)) {
					if (zap_test_flag(event->channel, ZAP_CHANNEL_RINGING)) {
						zap_channel_command(event->channel, ZAP_COMMAND_GENERATE_RING_OFF, NULL);
					}
					zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_UP);
				} else {
					zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_DIALTONE);
					zap_mutex_unlock(event->channel->mutex);
					locked = 0;
					zap_thread_create_detached(zap_analog_channel_run, event->channel);
				}
			} else {
				if (!zap_test_flag(event->channel, ZAP_CHANNEL_INTHREAD)) {
					if (zap_test_flag(event->channel, ZAP_CHANNEL_OFFHOOK)) {
						zap_channel_command(event->channel, ZAP_COMMAND_ONHOOK, NULL);
					}
				}
				zap_set_state_locked(event->channel, ZAP_CHANNEL_STATE_DOWN);
			}
		}
	}
	if (locked) {
		zap_mutex_unlock(event->channel->mutex);
	}
	return ZAP_SUCCESS;
}

static void *zap_analog_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	zap_analog_data_t *analog_data = span->signal_data;

	zap_log(ZAP_LOG_DEBUG, "ANALOG thread starting.\n");

	while(zap_running() && zap_test_flag(analog_data, ZAP_ANALOG_RUNNING)) {
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

	zap_clear_flag(analog_data, ZAP_ANALOG_RUNNING);
	
	zap_log(ZAP_LOG_DEBUG, "ANALOG thread ending.\n");

	return NULL;
}


static ZIO_SIG_LOAD_FUNCTION(zap_analog_init)
{
	return ZAP_SUCCESS;
}

zap_module_t zap_module = { 
	"analog",
	NULL,
	NULL,
	zap_analog_init,
	zap_analog_configure_span,
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
