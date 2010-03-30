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

#include "freetdm.h"
#include "ftdm_analog.h"

#ifndef localtime_r
struct tm * localtime_r(const time_t *clock, struct tm *result);
#endif

static void *ftdm_analog_channel_run(ftdm_thread_t *me, void *obj);

/**
 * \brief Starts an FXO channel thread (outgoing call)
 * \param ftdmchan Channel to initiate call on
 * \return Success or failure
 *
 * Initialises state, starts tone progress detection and runs the channel in a new a thread.
 */
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(analog_fxo_outgoing_call)
{
	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK) && !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INTHREAD)) {		
		ftdm_channel_clear_needed_tones(ftdmchan);
		ftdm_channel_clear_detected_tones(ftdmchan);

		ftdm_channel_command(ftdmchan, FTDM_COMMAND_OFFHOOK, NULL);
		ftdm_channel_command(ftdmchan, FTDM_COMMAND_ENABLE_PROGRESS_DETECT, NULL);
		ftdmchan->needed_tones[FTDM_TONEMAP_DIAL] = 1;
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DIALING);
		ftdm_thread_create_detached(ftdm_analog_channel_run, ftdmchan);
		return FTDM_SUCCESS;
	}

	return FTDM_FAIL;
}

/**
 * \brief Starts an FXS channel thread (outgoing call)
 * \param ftdmchan Channel to initiate call on
 * \return Success or failure
 *
 * Indicates call waiting if channel is already in use, otherwise runs the channel in a new thread.
 */
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(analog_fxs_outgoing_call)
{

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INTHREAD)) {
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_CALLWAITING);
	} else {
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_GENRING);
		ftdm_thread_create_detached(ftdm_analog_channel_run, ftdmchan);
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Starts an analog span thread (monitor)
 * \param span Span to monitor
 * \return Success or failure
 */
static ftdm_status_t ftdm_analog_start(ftdm_span_t *span)
{
	ftdm_analog_data_t *analog_data = span->signal_data;
	ftdm_set_flag(analog_data, FTDM_ANALOG_RUNNING);
	return ftdm_thread_create_detached(ftdm_analog_run, span);
}

/**
 * \brief Initialises an analog span from configuration variables
 * \param span Span to configure
 * \param sig_cb Callback function for event signals
 * \param ap List of configuration variables
 * \return Success or failure
 */
static FIO_SIG_CONFIGURE_FUNCTION(ftdm_analog_configure_span)
//ftdm_status_t ftdm_analog_configure_span(ftdm_span_t *span, char *tonemap, uint32_t digit_timeout, uint32_t max_dialstr, fio_signal_cb_t sig_cb)
{
	ftdm_analog_data_t *analog_data;
	const char *tonemap = "us";
	const char *hotline = "";
	uint32_t digit_timeout = 10;
	uint32_t max_dialstr = MAX_DTMF;
	const char *var, *val;
	int *intval;
	uint32_t flags = FTDM_ANALOG_CALLERID;

	assert(sig_cb != NULL);

	if (span->signal_type) {
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling.");
		return FTDM_FAIL;
	}
	
	analog_data = ftdm_malloc(sizeof(*analog_data));
	assert(analog_data != NULL);
	memset(analog_data, 0, sizeof(*analog_data));

	while ((var = va_arg(ap, char *))) {
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
		} else if (!strcasecmp(var, "enable_callerid")) {
			if (!(val = va_arg(ap, char *))) {
                break;
            }
			
			if (ftdm_true(val)) {
				flags |= FTDM_ANALOG_CALLERID;
			} else {
				flags &= ~FTDM_ANALOG_CALLERID;
			}
		} else if (!strcasecmp(var, "max_dialstr")) {
			if (!(intval = va_arg(ap, int *))) {
				break;
			}
			max_dialstr = *intval;
		} else if (!strcasecmp(var, "hotline")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			hotline = val;
		} else {
			snprintf(span->last_error, sizeof(span->last_error), "Unknown parameter [%s]", var);
			return FTDM_FAIL;
		}			
	}


	if (digit_timeout < 2000 || digit_timeout > 10000) {
		digit_timeout = 2000;
	}

	if ((max_dialstr < 1 && !strlen(hotline)) || max_dialstr > MAX_DTMF) {
		max_dialstr = MAX_DTMF;
	}
	
	span->start = ftdm_analog_start;
	analog_data->flags = flags;
	analog_data->digit_timeout = digit_timeout;
	analog_data->max_dialstr = max_dialstr;
	span->signal_cb = sig_cb;
	strncpy(analog_data->hotline, hotline, sizeof(analog_data->hotline));
	span->signal_type = FTDM_SIGTYPE_ANALOG;
	span->signal_data = analog_data;
	span->outgoing_call = span->trunk_type == FTDM_TRUNK_FXS ? analog_fxs_outgoing_call : analog_fxo_outgoing_call;
	ftdm_span_load_tones(span, tonemap);

	return FTDM_SUCCESS;

}

/**
 * \brief Retrieves tone generation output to be sent
 * \param ts Teletone generator
 * \param map Tone map
 * \return -1 on error, 0 on success
 */
static int teletone_handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	ftdm_buffer_t *dt_buffer = ts->user_data;
	int wrote;

	if (!dt_buffer) {
		return -1;
	}
	wrote = teletone_mux_tones(ts, map);
	ftdm_buffer_write(dt_buffer, ts->buffer, wrote * 2);
	return 0;
}

/**
 * \brief Sends caller id on an analog channel (FSK coded)
 * \param ftdmchan Channel to send caller id on
 */
static void send_caller_id(ftdm_channel_t *ftdmchan)
{
	ftdm_fsk_data_state_t fsk_data;
	uint8_t databuf[1024] = "";
	char time_str[9];
	struct tm tm;
	time_t now;
	ftdm_mdmf_type_t mt = MDMF_INVALID;

	time(&now);
#ifdef WIN32
	_tzset();
	_localtime64_s(&tm, &now);
#else
	localtime_r(&now, &tm);
#endif
	strftime(time_str, sizeof(time_str), "%m%d%H%M", &tm);

	ftdm_fsk_data_init(&fsk_data, databuf, sizeof(databuf));
	ftdm_fsk_data_add_mdmf(&fsk_data, MDMF_DATETIME, (uint8_t *) time_str, 8);
					
	if (ftdm_strlen_zero(ftdmchan->caller_data.cid_num.digits)) {
		mt = MDMF_NO_NUM;
		ftdm_set_string(ftdmchan->caller_data.cid_num.digits, "O");
	} else if (!strcasecmp(ftdmchan->caller_data.cid_num.digits, "P") || !strcasecmp(ftdmchan->caller_data.cid_num.digits, "O")) {
		mt = MDMF_NO_NUM;
	} else {
		mt = MDMF_PHONE_NUM;
	}
	ftdm_fsk_data_add_mdmf(&fsk_data, mt, (uint8_t *) ftdmchan->caller_data.cid_num.digits, (uint8_t)strlen(ftdmchan->caller_data.cid_num.digits));

	if (ftdm_strlen_zero(ftdmchan->caller_data.cid_name)) {
		mt = MDMF_NO_NAME;
		ftdm_set_string(ftdmchan->caller_data.cid_name, "O");
	} else if (!strcasecmp(ftdmchan->caller_data.cid_name, "P") || !strcasecmp(ftdmchan->caller_data.cid_name, "O")) {
		mt = MDMF_NO_NAME;
	} else {
		mt = MDMF_PHONE_NAME;
	}
	ftdm_fsk_data_add_mdmf(&fsk_data, mt, (uint8_t *) ftdmchan->caller_data.cid_name, (uint8_t)strlen(ftdmchan->caller_data.cid_name));
					
	ftdm_fsk_data_add_checksum(&fsk_data);
	ftdm_channel_send_fsk_data(ftdmchan, &fsk_data, -14);
}

/**
 * \brief Main thread function for analog channel (outgoing call)
 * \param me Current thread
 * \param obj Channel to run in this thread
 */
static void *ftdm_analog_channel_run(ftdm_thread_t *me, void *obj)
{
	ftdm_channel_t *ftdmchan = (ftdm_channel_t *) obj;
	ftdm_buffer_t *dt_buffer = NULL;
	teletone_generation_session_t ts;
	uint8_t frame[1024];
	ftdm_size_t len, rlen;
	ftdm_tone_type_t tt = FTDM_TONE_DTMF;
	char dtmf[MAX_DTMF+1] = "";
	ftdm_size_t dtmf_offset = 0;
	ftdm_analog_data_t *analog_data = ftdmchan->span->signal_data;
	ftdm_channel_t *closed_chan;
	uint32_t state_counter = 0, elapsed = 0, collecting = 0, interval = 0, last_digit = 0, indicate = 0, dial_timeout = 30000;
	ftdm_sigmsg_t sig;
	ftdm_status_t status;
	
	ftdm_log(FTDM_LOG_DEBUG, "ANALOG CHANNEL thread starting.\n");

	ts.buffer = NULL;

	if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "OPEN ERROR [%s]\n", ftdmchan->last_error);
		goto done;
	}

	if (ftdm_buffer_create(&dt_buffer, 1024, 3192, 0) != FTDM_SUCCESS) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "memory error!");
		ftdm_log(FTDM_LOG_ERROR, "MEM ERROR\n");
		goto done;
	}

	if (ftdm_channel_command(ftdmchan, FTDM_COMMAND_ENABLE_DTMF_DETECT, &tt) != FTDM_SUCCESS) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "error initilizing tone detector!");
		ftdm_log(FTDM_LOG_ERROR, "TONE ERROR\n");
		goto done;
	}

	ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_INTHREAD);
	teletone_init_session(&ts, 0, teletone_handler, dt_buffer);
	ts.rate = 8000;
#if 0
	ts.debug = 1;
	ts.debug_stream = stdout;
#endif
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_INTERVAL, &interval);
	ftdm_buffer_set_loops(dt_buffer, -1);
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;
	
	assert(interval != 0);

	while (ftdm_running() && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INTHREAD)) {
		ftdm_wait_flag_t flags = FTDM_READ;
		ftdm_size_t dlen = 0;
		
		len = sizeof(frame);
		
		elapsed += interval;
		state_counter += interval;
		
		if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
			switch(ftdmchan->state) {
			case FTDM_CHANNEL_STATE_GET_CALLERID:
				{
					if (state_counter > 5000 || !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_CALLERID_DETECT)) {
						ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_CALLERID_DETECT, NULL);
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_IDLE);
					}
				}
				break;
			case FTDM_CHANNEL_STATE_DIALING:
				{
					if (state_counter > dial_timeout) {
						if (ftdmchan->needed_tones[FTDM_TONEMAP_DIAL]) {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
						} else {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);
						}
					} 
				}
				break;
			case FTDM_CHANNEL_STATE_GENRING:
				{
					if (state_counter > 60000) {
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
					} else if (!ftdmchan->fsk_buffer || !ftdm_buffer_inuse(ftdmchan->fsk_buffer)) {
						ftdm_sleep(interval);
						continue;
					}
				}
				break;
			case FTDM_CHANNEL_STATE_DIALTONE:
				{
					if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_HOLD) && state_counter > 10000) {
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
					}
				}
				break;
			case FTDM_CHANNEL_STATE_BUSY:
				{
					if (state_counter > 20000) {
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_ATTN);
					}
				}
				break;
			case FTDM_CHANNEL_STATE_ATTN:
				{
					if (state_counter > 20000) {
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
					}
				}
				break;
			case FTDM_CHANNEL_STATE_HANGUP:
				{
					if (state_counter > 500) {
						if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_RINGING)) {
							ftdm_channel_command(ftdmchan, FTDM_COMMAND_GENERATE_RING_OFF, NULL);
						}
						
						if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK) && 
							(ftdmchan->last_state == FTDM_CHANNEL_STATE_RING || ftdmchan->last_state == FTDM_CHANNEL_STATE_DIALTONE 
							 || ftdmchan->last_state >= FTDM_CHANNEL_STATE_IDLE)) {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
						} else {
							ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_CLEARING;
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
						}
					}
				}
				break;
			case FTDM_CHANNEL_STATE_CALLWAITING:
				{
					int done = 0;
					
					if (ftdmchan->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK] == 1) {
						send_caller_id(ftdmchan);
						ftdmchan->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK]++;
					} else if (state_counter > 600 && !ftdmchan->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK]) {
						send_caller_id(ftdmchan);
						ftdmchan->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK]++;
					} else if (state_counter > 1000 && !ftdmchan->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK]) {
						done = 1;
					} else if (state_counter > 10000) {
						if (ftdmchan->fsk_buffer) {
							ftdm_buffer_zero(ftdmchan->fsk_buffer);
						} else {
							ftdm_buffer_create(&ftdmchan->fsk_buffer, 128, 128, 0);
						}
						
						ts.user_data = ftdmchan->fsk_buffer;
						teletone_run(&ts, ftdmchan->span->tone_map[FTDM_TONEMAP_CALLWAITING_SAS]);
						ts.user_data = dt_buffer;
						done = 1;
					}

					if (done) {
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);
						ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);
						ftdm_clear_flag_locked(ftdmchan->span, FTDM_SPAN_STATE_CHANGE);
						ftdmchan->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK] = 0;
					}
				}
			case FTDM_CHANNEL_STATE_UP:
			case FTDM_CHANNEL_STATE_IDLE:
				{
					ftdm_sleep(interval);
					continue;
				}
				break;
			case FTDM_CHANNEL_STATE_DOWN:
				{
					goto done;
				}
				break;
			default:
				break;
			}
		} else {
			ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);
			ftdm_clear_flag_locked(ftdmchan->span, FTDM_SPAN_STATE_CHANGE);
			ftdm_channel_complete_state(ftdmchan);
			indicate = 0;
			state_counter = 0;

			ftdm_log(FTDM_LOG_DEBUG, "Executing state handler on %d:%d for %s\n", 
					ftdmchan->span_id, ftdmchan->chan_id,
					ftdm_channel_state2str(ftdmchan->state));
			switch(ftdmchan->state) {
			case FTDM_CHANNEL_STATE_UP:
				{
					ftdm_channel_use(ftdmchan);
					ftdm_channel_clear_needed_tones(ftdmchan);
					ftdm_channel_flush_dtmf(ftdmchan);
						
					if (ftdmchan->type == FTDM_CHAN_TYPE_FXO && !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK)) {
						ftdm_channel_command(ftdmchan, FTDM_COMMAND_OFFHOOK, NULL);
					}

					if (ftdmchan->fsk_buffer && ftdm_buffer_inuse(ftdmchan->fsk_buffer)) {
						ftdm_log(FTDM_LOG_DEBUG, "Cancel FSK transmit due to early answer.\n");
						ftdm_buffer_zero(ftdmchan->fsk_buffer);
					}

					if (ftdmchan->type == FTDM_CHAN_TYPE_FXS && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_RINGING)) {
						ftdm_channel_command(ftdmchan, FTDM_COMMAND_GENERATE_RING_OFF, NULL);
					}

					if (ftdmchan->token_count == 1) {
						ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_HOLD);
					}

					if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_HOLD)) {
						ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_HOLD);
						sig.event_id = FTDM_SIGEVENT_ADD_CALL;
					} else {
						sig.event_id = FTDM_SIGEVENT_UP;
					}

					ftdm_span_send_signal(ftdmchan->span, &sig);
					continue;
				}
				break;
			case FTDM_CHANNEL_STATE_DIALING:
				{
					ftdm_channel_use(ftdmchan);
				}
				break;
			case FTDM_CHANNEL_STATE_IDLE:
				{
					ftdm_channel_use(ftdmchan);
					sig.event_id = FTDM_SIGEVENT_START;

					if (ftdmchan->type == FTDM_CHAN_TYPE_FXO) {
						ftdm_set_string(ftdmchan->caller_data.dnis.digits, ftdmchan->chan_number);
					} else {
						ftdm_set_string(ftdmchan->caller_data.dnis.digits, dtmf);
					}

					ftdm_span_send_signal(ftdmchan->span, &sig);
					continue;
				}
				break;
			case FTDM_CHANNEL_STATE_DOWN:
				{
					sig.event_id = FTDM_SIGEVENT_STOP;
					ftdm_span_send_signal(ftdmchan->span, &sig);
					goto done;
				}
				break;
			case FTDM_CHANNEL_STATE_DIALTONE:
				{
					memset(&ftdmchan->caller_data, 0, sizeof(ftdmchan->caller_data));
					*dtmf = '\0';
					dtmf_offset = 0;
					ftdm_buffer_zero(dt_buffer);
					teletone_run(&ts, ftdmchan->span->tone_map[FTDM_TONEMAP_DIAL]);
					indicate = 1;
				}
				break;
			case FTDM_CHANNEL_STATE_CALLWAITING:
				{
					ftdmchan->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK] = 0;
					if (ftdmchan->fsk_buffer) {
						ftdm_buffer_zero(ftdmchan->fsk_buffer);
					} else {
						ftdm_buffer_create(&ftdmchan->fsk_buffer, 128, 128, 0);
					}
					
					ts.user_data = ftdmchan->fsk_buffer;
					teletone_run(&ts, ftdmchan->span->tone_map[FTDM_TONEMAP_CALLWAITING_SAS]);
					teletone_run(&ts, ftdmchan->span->tone_map[FTDM_TONEMAP_CALLWAITING_CAS]);
					ts.user_data = dt_buffer;
				}
				break;
			case FTDM_CHANNEL_STATE_GENRING:
				{
					ftdm_sigmsg_t sig;

					send_caller_id(ftdmchan);
					ftdm_channel_command(ftdmchan, FTDM_COMMAND_GENERATE_RING_ON, NULL);

					memset(&sig, 0, sizeof(sig));
					sig.chan_id = ftdmchan->chan_id;
					sig.span_id = ftdmchan->span_id;
					sig.channel = ftdmchan;
					sig.event_id = FTDM_SIGEVENT_PROGRESS;
					ftdm_span_send_signal(ftdmchan->span, &sig);
					
				}
				break;
			case FTDM_CHANNEL_STATE_GET_CALLERID:
				{
					memset(&ftdmchan->caller_data, 0, sizeof(ftdmchan->caller_data));
					ftdm_channel_command(ftdmchan, FTDM_COMMAND_ENABLE_CALLERID_DETECT, NULL);
					continue;
				}
				break;
			case FTDM_CHANNEL_STATE_RING:
				{
					ftdm_buffer_zero(dt_buffer);
					teletone_run(&ts, ftdmchan->span->tone_map[FTDM_TONEMAP_RING]);
					indicate = 1;
					
				}
				break;
			case FTDM_CHANNEL_STATE_BUSY:
				{
					ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_CIRCUIT_CONGESTION;
					if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK) && !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
						ftdm_buffer_zero(dt_buffer);
						teletone_run(&ts, ftdmchan->span->tone_map[FTDM_TONEMAP_BUSY]);
						indicate = 1;
					} else {
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
					}
				}
				break;
			case FTDM_CHANNEL_STATE_ATTN:
				{
					if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK) && !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
						ftdm_buffer_zero(dt_buffer);
						teletone_run(&ts, ftdmchan->span->tone_map[FTDM_TONEMAP_ATTN]);
						indicate = 1;
					} else {
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
					}
				}
				break;
			default:
				break;
			}
		}


		if (ftdmchan->state == FTDM_CHANNEL_STATE_DIALTONE || ftdmchan->state == FTDM_CHANNEL_STATE_COLLECT) {
			if ((dlen = ftdm_channel_dequeue_dtmf(ftdmchan, dtmf + dtmf_offset, sizeof(dtmf) - strlen(dtmf)))) {

				if (ftdmchan->state == FTDM_CHANNEL_STATE_DIALTONE) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_COLLECT);
					collecting = 1;
				}
				dtmf_offset = strlen(dtmf);
				last_digit = elapsed;
				sig.event_id = FTDM_SIGEVENT_COLLECTED_DIGIT;
				sig.raw_data = dtmf;
				if (ftdm_span_send_signal(ftdmchan->span, &sig) == FTDM_BREAK) {
					collecting = 0;
				}
			}
			else if(!analog_data->max_dialstr)
			{
				last_digit = elapsed;
				collecting = 0;
				strcpy(dtmf, analog_data->hotline);
			}
		}


		if (last_digit && (!collecting || ((elapsed - last_digit > analog_data->digit_timeout) || strlen(dtmf) >= analog_data->max_dialstr))) {
			ftdm_log(FTDM_LOG_DEBUG, "Number obtained [%s]\n", dtmf);
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_IDLE);
			last_digit = 0;
			collecting = 0;
		}

		if (ftdm_channel_wait(ftdmchan, &flags, interval * 2) != FTDM_SUCCESS) {
			continue;
		}

		if (!(flags & FTDM_READ)) {
			continue;
		}

		if (ftdm_channel_read(ftdmchan, frame, &len) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "READ ERROR [%s]\n", ftdmchan->last_error);
			goto done;
		}

		if (ftdmchan->type == FTDM_CHAN_TYPE_FXO && ftdmchan->detected_tones[0]) {
			ftdm_sigmsg_t sig;
			int i;
			memset(&sig, 0, sizeof(sig));
			sig.chan_id = ftdmchan->chan_id;
			sig.span_id = ftdmchan->span_id;
			sig.channel = ftdmchan;
			sig.event_id = FTDM_SIGEVENT_TONE_DETECTED;
			
			for (i = 1; i < FTDM_TONEMAP_INVALID; i++) {
				if (ftdmchan->detected_tones[i]) {
					ftdm_log(FTDM_LOG_DEBUG, "Detected tone %s on %d:%d\n", ftdm_tonemap2str(i), ftdmchan->span_id, ftdmchan->chan_id);
					sig.raw_data = &i;
					ftdm_span_send_signal(ftdmchan->span, &sig);
				}
			}
			
			if (ftdmchan->detected_tones[FTDM_TONEMAP_BUSY] || 
				ftdmchan->detected_tones[FTDM_TONEMAP_FAIL1] ||
				ftdmchan->detected_tones[FTDM_TONEMAP_FAIL2] ||
				ftdmchan->detected_tones[FTDM_TONEMAP_FAIL3] ||
				ftdmchan->detected_tones[FTDM_TONEMAP_ATTN]
				) {
				ftdm_log(FTDM_LOG_ERROR, "Failure indication detected!\n");
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
			} else if (ftdmchan->detected_tones[FTDM_TONEMAP_DIAL]) {
				if (ftdm_strlen_zero(ftdmchan->caller_data.dnis.digits)) {
					ftdm_log(FTDM_LOG_ERROR, "No Digits to send!\n");
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
				} else {
					if (ftdm_channel_command(ftdmchan, FTDM_COMMAND_SEND_DTMF, ftdmchan->caller_data.dnis.digits) != FTDM_SUCCESS) {
						ftdm_log(FTDM_LOG_ERROR, "Send Digits Failed [%s]\n", ftdmchan->last_error);
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
					} else {
						state_counter = 0;
						ftdmchan->needed_tones[FTDM_TONEMAP_RING] = 1;
						ftdmchan->needed_tones[FTDM_TONEMAP_BUSY] = 1;
						ftdmchan->needed_tones[FTDM_TONEMAP_FAIL1] = 1;
						ftdmchan->needed_tones[FTDM_TONEMAP_FAIL2] = 1;
						ftdmchan->needed_tones[FTDM_TONEMAP_FAIL3] = 1;
						dial_timeout = ((ftdmchan->dtmf_on + ftdmchan->dtmf_off) * strlen(ftdmchan->caller_data.dnis.digits)) + 2000;
					}
				}
			} else if (ftdmchan->detected_tones[FTDM_TONEMAP_RING]) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);
			}
			
			ftdm_channel_clear_detected_tones(ftdmchan);
		}

		if ((ftdmchan->dtmf_buffer && ftdm_buffer_inuse(ftdmchan->dtmf_buffer)) || (ftdmchan->fsk_buffer && ftdm_buffer_inuse(ftdmchan->fsk_buffer))) {
			//rlen = len;
			//memset(frame, 0, len);
			//ftdm_channel_write(ftdmchan, frame, sizeof(frame), &rlen);
			continue;
		}
		
		if (!indicate) {
			continue;
		}

		if (ftdmchan->type == FTDM_CHAN_TYPE_FXO && !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK)) {
			ftdm_channel_command(ftdmchan, FTDM_COMMAND_OFFHOOK, NULL);
		}

		if (ftdmchan->effective_codec != FTDM_CODEC_SLIN) {
			len *= 2;
		}

		rlen = ftdm_buffer_read_loop(dt_buffer, frame, len);					
		
		if (ftdmchan->effective_codec != FTDM_CODEC_SLIN) {
			fio_codec_t codec_func = NULL;

			if (ftdmchan->native_codec == FTDM_CODEC_ULAW) {
				codec_func = fio_slin2ulaw;
			} else if (ftdmchan->native_codec == FTDM_CODEC_ALAW) {
				codec_func = fio_slin2alaw;
			}

			if (codec_func) {
				status = codec_func(frame, sizeof(frame), &rlen);
			} else {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "codec error!");
				goto done;
			}
		}

		ftdm_channel_write(ftdmchan, frame, sizeof(frame), &rlen);
	}

 done:


	if (ftdmchan->type == FTDM_CHAN_TYPE_FXO && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK)) {
		ftdm_channel_command(ftdmchan, FTDM_COMMAND_ONHOOK, NULL);
	}

	if (ftdmchan->type == FTDM_CHAN_TYPE_FXS && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_RINGING)) {
		ftdm_channel_command(ftdmchan, FTDM_COMMAND_GENERATE_RING_OFF, NULL);
	}

	
	closed_chan = ftdmchan;
	ftdm_channel_close(&ftdmchan);

	ftdm_channel_command(closed_chan, FTDM_COMMAND_SET_NATIVE_CODEC, NULL);

	if (ts.buffer) {
		teletone_destroy_session(&ts);
	}

	if (dt_buffer) {
		ftdm_buffer_destroy(&dt_buffer);
	}

	if (closed_chan->state != FTDM_CHANNEL_STATE_DOWN) {
		ftdm_set_state_locked(closed_chan, FTDM_CHANNEL_STATE_DOWN);
	}

	ftdm_log(FTDM_LOG_DEBUG, "ANALOG CHANNEL %d:%d thread ended.\n", closed_chan->span_id, closed_chan->chan_id);
	ftdm_clear_flag(closed_chan, FTDM_CHANNEL_INTHREAD);

	return NULL;
}

/**
 * \brief Processes freetdm event
 * \param span Span on which the event was fired
 * \param event Event to be treated
 * \return Success or failure
 */
static __inline__ ftdm_status_t process_event(ftdm_span_t *span, ftdm_event_t *event)
{
	ftdm_sigmsg_t sig;
	ftdm_analog_data_t *analog_data = event->channel->span->signal_data;
	int locked = 0;
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = event->channel->chan_id;
	sig.span_id = event->channel->span_id;
	sig.channel = event->channel;


	ftdm_log(FTDM_LOG_DEBUG, "EVENT [%s][%d:%d] STATE [%s]\n", 
			ftdm_oob_event2str(event->enum_id), event->channel->span_id, event->channel->chan_id, ftdm_channel_state2str(event->channel->state));

	ftdm_mutex_lock(event->channel->mutex);
	locked++;

	switch(event->enum_id) {
	case FTDM_OOB_RING_START:
		{
			if (event->channel->type != FTDM_CHAN_TYPE_FXO) {
				ftdm_log(FTDM_LOG_ERROR, "Cannot get a RING_START event on a non-fxo channel, please check your config.\n");
				ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_DOWN);
				goto end;
			}
			if (!event->channel->ring_count && (event->channel->state == FTDM_CHANNEL_STATE_DOWN && !ftdm_test_flag(event->channel, FTDM_CHANNEL_INTHREAD))) {
				if (ftdm_test_flag(analog_data, FTDM_ANALOG_CALLERID)) {
					ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_GET_CALLERID);
				} else {
					ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_IDLE);
				}
				event->channel->ring_count = 1;
				ftdm_mutex_unlock(event->channel->mutex);
				locked = 0;
				ftdm_thread_create_detached(ftdm_analog_channel_run, event->channel);
			} else {
				event->channel->ring_count++;
			}
		}
		break;
	case FTDM_OOB_ONHOOK:
		{
			if (ftdm_test_flag(event->channel, FTDM_CHANNEL_RINGING)) {
				ftdm_channel_command(event->channel, FTDM_COMMAND_GENERATE_RING_OFF, NULL);
			}

			if (event->channel->state != FTDM_CHANNEL_STATE_DOWN) {
				ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_DOWN);
			}

		}
		break;
	case FTDM_OOB_FLASH:
		{
			if (event->channel->state == FTDM_CHANNEL_STATE_CALLWAITING) {
				ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_UP);
				ftdm_clear_flag_locked(event->channel, FTDM_CHANNEL_STATE_CHANGE);
				ftdm_clear_flag_locked(event->channel->span, FTDM_SPAN_STATE_CHANGE);
				event->channel->detected_tones[FTDM_TONEMAP_CALLWAITING_ACK] = 0;
			} 

			ftdm_channel_rotate_tokens(event->channel);
			
			if (ftdm_test_flag(event->channel, FTDM_CHANNEL_HOLD) && event->channel->token_count != 1) {
				ftdm_set_state_locked(event->channel,  FTDM_CHANNEL_STATE_UP);
			} else {
				sig.event_id = FTDM_SIGEVENT_FLASH;
				ftdm_span_send_signal(span, &sig);
			}
		}
		break;
	case FTDM_OOB_OFFHOOK:
		{
			if (event->channel->type == FTDM_CHAN_TYPE_FXS) {
				if (ftdm_test_flag(event->channel, FTDM_CHANNEL_INTHREAD)) {
					if (ftdm_test_flag(event->channel, FTDM_CHANNEL_RINGING)) {
						ftdm_channel_command(event->channel, FTDM_COMMAND_GENERATE_RING_OFF, NULL);
					}
					ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_UP);
				} else {
					if(!analog_data->max_dialstr) {
						ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_COLLECT);
					} else {
						ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_DIALTONE);
					}						
					ftdm_mutex_unlock(event->channel->mutex);
					locked = 0;
					ftdm_thread_create_detached(ftdm_analog_channel_run, event->channel);
				}
			} else {
				if (!ftdm_test_flag(event->channel, FTDM_CHANNEL_INTHREAD)) {
					if (ftdm_test_flag(event->channel, FTDM_CHANNEL_OFFHOOK)) {
						ftdm_channel_command(event->channel, FTDM_COMMAND_ONHOOK, NULL);
					}
				}
				ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_DOWN);
			}
		}
	}

 end:

	if (locked) {
		ftdm_mutex_unlock(event->channel->mutex);
	}
	return FTDM_SUCCESS;
}

/**
 * \brief Main thread function for analog span (monitor)
 * \param me Current thread
 * \param obj Span to run in this thread
 */
static void *ftdm_analog_run(ftdm_thread_t *me, void *obj)
{
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_analog_data_t *analog_data = span->signal_data;
	int errs = 0;
	
	ftdm_log(FTDM_LOG_DEBUG, "ANALOG thread starting.\n");

	while(ftdm_running() && ftdm_test_flag(analog_data, FTDM_ANALOG_RUNNING)) {
		int waitms = 1000;
		ftdm_status_t status;

		if ((status = ftdm_span_poll_event(span, waitms)) != FTDM_FAIL) {
			errs = 0;
		}
		
		switch(status) {
		case FTDM_SUCCESS:
			{
				ftdm_event_t *event;
				while (ftdm_span_next_event(span, &event) == FTDM_SUCCESS) {
					if (event->enum_id == FTDM_OOB_NOOP) {
						continue;
					}
					if (process_event(span, event) != FTDM_SUCCESS) {
						goto end;
					}
				}
			}
			break;
		case FTDM_FAIL:
			{
				ftdm_log(FTDM_LOG_ERROR, "Failure Polling event! [%s]\n", span->last_error);
				if (++errs > 300) {
					ftdm_log(FTDM_LOG_CRIT, "Too Many Errors!\n");
					goto end;
				}
			}
			break;
		default:
			break;
		}

	}

 end:

	ftdm_clear_flag(analog_data, FTDM_ANALOG_RUNNING);
	
	ftdm_log(FTDM_LOG_DEBUG, "ANALOG thread ending.\n");

	return NULL;
}

/**
 * \brief FreeTDM analog signaling module initialisation
 * \return Success
 */
static FIO_SIG_LOAD_FUNCTION(ftdm_analog_init)
{
	return FTDM_SUCCESS;
}

/**
 * \brief FreeTDM analog signaling module definition
 */
EX_DECLARE_DATA ftdm_module_t ftdm_module = {
	"analog",
	NULL,
	NULL,
	ftdm_analog_init,
	ftdm_analog_configure_span,
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
