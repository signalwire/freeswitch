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

#include "private/ftdm_core.h"
#include "ftdm_analog_em.h"

#ifndef localtime_r
struct tm * localtime_r(const time_t *clock, struct tm *result);
#endif

static void *ftdm_analog_em_channel_run(ftdm_thread_t *me, void *obj);

/**
 * \brief Starts an EM channel thread (outgoing call)
 * \param ftdmchan Channel to initiate call on
 * \return Success or failure
 *
 * Initialises state, starts tone progress detection and runs the channel in a new a thread.
 */
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(analog_em_outgoing_call)
{
	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK) && !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INTHREAD)) {		
		ftdm_channel_clear_needed_tones(ftdmchan);
		ftdm_channel_clear_detected_tones(ftdmchan);

		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);

		ftdm_channel_command(ftdmchan, FTDM_COMMAND_OFFHOOK, NULL);
		ftdm_channel_command(ftdmchan, FTDM_COMMAND_ENABLE_PROGRESS_DETECT, NULL);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DIALING);
		ftdm_thread_create_detached(ftdm_analog_em_channel_run, ftdmchan);
		return FTDM_SUCCESS;
	}

	return FTDM_FAIL;
}

/**
 * \brief Starts an EM span thread (monitor)
 * \param span Span to monitor
 * \return Success or failure
 */
static ftdm_status_t ftdm_analog_em_start(ftdm_span_t *span)
{
	ftdm_analog_em_data_t *analog_data = span->signal_data;
	ftdm_set_flag(analog_data, FTDM_ANALOG_EM_RUNNING);
	return ftdm_thread_create_detached(ftdm_analog_em_run, span);
}

/**
 * \brief Returns the signalling status on a channel
 * \param ftdmchan Channel to get status on
 * \param status	Pointer to set signalling status
 * \return Success or failure
 */

static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(analog_em_get_channel_sig_status)
{
	ftdm_unused_arg(ftdmchan);
	*status = FTDM_SIG_STATE_UP;
	return FTDM_SUCCESS;
}

/**
 * \brief Returns the signalling status on a span
 * \param span Span to get status on
 * \param status	Pointer to set signalling status
 * \return Success or failure
 */

static FIO_SPAN_GET_SIG_STATUS_FUNCTION(analog_em_get_span_sig_status)
{
	ftdm_unused_arg(span);
	*status = FTDM_SIG_STATE_UP;
	return FTDM_SUCCESS;
}

/**
 * \brief Initialises an EM span from configuration variables
 * \param span Span to configure
 * \param sig_cb Callback function for event signals
 * \param ap List of configuration variables
 * \return Success or failure
 */
static FIO_SIG_CONFIGURE_FUNCTION(ftdm_analog_em_configure_span)
//ftdm_status_t ftdm_analog_em_configure_span(ftdm_span_t *span, char *tonemap, uint32_t digit_timeout, uint32_t max_dialstr, fio_signal_cb_t sig_cb)
{
	ftdm_analog_em_data_t *analog_data;
	const char *tonemap = "us";
	uint32_t digit_timeout = 2000;
	uint32_t max_dialstr = 11;
	uint32_t dial_timeout = 0;
	ftdm_bool_t answer_supervision = FTDM_FALSE;
	const char *var, *val;
	int *intval;

	assert(sig_cb != NULL);

	if (span->signal_type) {
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling.");
		return FTDM_FAIL;
	}
	
	analog_data = ftdm_malloc(sizeof(*analog_data));
	assert(analog_data != NULL);
	memset(analog_data, 0, sizeof(*analog_data));

	while((var = va_arg(ap, char *))) {
		ftdm_log(FTDM_LOG_DEBUG, "Parsing analog em parameter '%s'\n", var);
		if (!strcasecmp(var, "tonemap")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			tonemap = val;
		} else if (!strcasecmp(var, "answer_supervision")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			answer_supervision = ftdm_true(val);
		} else if (!strcasecmp(var, "dial_timeout")) {
			if (!(intval = va_arg(ap, int *))) {
				break;
			}
			dial_timeout = *intval;
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
			ftdm_log(FTDM_LOG_ERROR, "Invalid parameter for analog em span: '%s'\n", var);
			return FTDM_FAIL;
		}
	}


	if (digit_timeout < 2000 || digit_timeout > 10000) {
		digit_timeout = 2000;
	}

	if (max_dialstr < 2 || max_dialstr > MAX_DIALSTRING) {
		ftdm_log(FTDM_LOG_ERROR, "Invalid max_dialstr, setting to %d\n", MAX_DIALSTRING);
		max_dialstr = MAX_DIALSTRING;
	}

	span->start = ftdm_analog_em_start;
	analog_data->digit_timeout = digit_timeout;
	analog_data->max_dialstr = max_dialstr;
	analog_data->dial_timeout = dial_timeout;
	analog_data->answer_supervision = answer_supervision;
	span->signal_cb = sig_cb;
	span->signal_type = FTDM_SIGTYPE_ANALOG;
	span->signal_data = analog_data;
	span->outgoing_call = analog_em_outgoing_call;
	span->get_channel_sig_status = analog_em_get_channel_sig_status;
	span->get_span_sig_status = analog_em_get_span_sig_status;
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
 * \brief Main thread function for EM channel (outgoing call)
 * \param me Current thread
 * \param obj Channel to run in this thread
 */
static void *ftdm_analog_em_channel_run(ftdm_thread_t *me, void *obj)
{
	ftdm_channel_t *ftdmchan = (ftdm_channel_t *) obj;
	ftdm_buffer_t *dt_buffer = NULL;
	teletone_generation_session_t ts;
	uint8_t frame[1024];
	ftdm_size_t len, rlen;
	ftdm_tone_type_t tt = FTDM_TONE_DTMF;
	char dtmf[128] = "";
	ftdm_size_t dtmf_offset = 0;
	ftdm_analog_em_data_t *analog_data = ftdmchan->span->signal_data;
	ftdm_channel_t *closed_chan;
	uint32_t state_counter = 0, elapsed = 0, collecting = 0, interval = 0, last_digit = 0, indicate = 0, dial_timeout = 30000;
	ftdm_sigmsg_t sig;
	int cas_bits = 0;
	uint32_t cas_answer = 0;
	int cas_answer_ms = 500;
	ftdm_bool_t digits_sent = FTDM_FALSE;

	ftdm_unused_arg(me);
	ftdm_log(FTDM_LOG_DEBUG, "ANALOG EM CHANNEL thread starting.\n");

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
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "IO Interval: %u\n", interval);

	while (ftdm_running() && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INTHREAD)) {
		ftdm_wait_flag_t flags = FTDM_READ;
		ftdm_size_t dlen = 0;
		
		elapsed += interval;
		state_counter += interval;

		if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
			switch(ftdmchan->state) {
			case FTDM_CHANNEL_STATE_DIALING:
				{
					if (! ftdmchan->needed_tones[FTDM_TONEMAP_RING]
						&& ftdm_test_flag(ftdmchan, FTDM_CHANNEL_WINK)
						&& !digits_sent) {
						if (ftdm_strlen_zero(ftdmchan->caller_data.dnis.digits)) {
							ftdm_log(FTDM_LOG_ERROR, "No Digits to send!\n");
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
						} else {
							if (ftdm_channel_command(ftdmchan, FTDM_COMMAND_SEND_DTMF, ftdmchan->caller_data.dnis.digits) != FTDM_SUCCESS) {
								ftdm_log(FTDM_LOG_ERROR, "Send Digits Failed [%s]\n", ftdmchan->last_error);
								ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
							} else {
								state_counter = 0;
								digits_sent = FTDM_TRUE;
								ftdmchan->needed_tones[FTDM_TONEMAP_RING] = 1;
								ftdmchan->needed_tones[FTDM_TONEMAP_BUSY] = 1;
								ftdmchan->needed_tones[FTDM_TONEMAP_FAIL1] = 1;
								ftdmchan->needed_tones[FTDM_TONEMAP_FAIL2] = 1;
								ftdmchan->needed_tones[FTDM_TONEMAP_FAIL3] = 1;
								dial_timeout = ((ftdmchan->dtmf_on + ftdmchan->dtmf_off) * strlen(ftdmchan->caller_data.dnis.digits)) + 2000;
								if (analog_data->dial_timeout) {
									dial_timeout += analog_data->dial_timeout;
								}
								ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Outbound dialing timeout: %dms\n", dial_timeout);
								ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Outbound CAS answer timeout: %dms\n", cas_answer_ms);
							}
						}
						break;
					}
					if (state_counter > dial_timeout) {
						if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_WINK)) {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
						} else if (!analog_data->answer_supervision) {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);
						}
					}
					cas_bits = 0;
					ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_CAS_BITS, &cas_bits);
					if (!(state_counter % 1000)) {
						ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "CAS bits: 0x%X\n", cas_bits);
					}
					if (cas_bits == 0xF) {
						cas_answer += interval;
						if (cas_answer >= cas_answer_ms) {
							ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Answering on CAS answer signal persistence!\n");
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);
						}
					} else if (cas_answer) {
						ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Resetting cas answer to 0: 0x%X!\n", cas_bits);
						cas_answer = 0;
					}
				}
				break;
			case FTDM_CHANNEL_STATE_DIALTONE:
				{
					if (state_counter > 10000) {
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
						if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK) && 
							(ftdmchan->last_state == FTDM_CHANNEL_STATE_RINGING || ftdmchan->last_state == FTDM_CHANNEL_STATE_DIALTONE 
							 || ftdmchan->last_state == FTDM_CHANNEL_STATE_RING)) {
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_BUSY);
						} else {
							ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_CLEARING;
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
						}
					}
				}
				break;
			case FTDM_CHANNEL_STATE_UP:
			case FTDM_CHANNEL_STATE_RING:
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

					if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OFFHOOK)) {
						ftdm_channel_command(ftdmchan, FTDM_COMMAND_OFFHOOK, NULL);
					}

					sig.event_id = FTDM_SIGEVENT_UP;

					ftdm_span_send_signal(ftdmchan->span, &sig);
					continue;
				}
				break;
			case FTDM_CHANNEL_STATE_DIALING:
				{
					ftdm_channel_use(ftdmchan);
				}
				break;
			case FTDM_CHANNEL_STATE_RING:
				{
					ftdm_channel_use(ftdmchan);

					if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
						ftdm_set_string(ftdmchan->caller_data.dnis.digits, ftdmchan->chan_number);
					} else {
						ftdm_set_string(ftdmchan->caller_data.dnis.digits, dtmf);
					}

					sig.event_id = FTDM_SIGEVENT_START;

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

					ftdm_channel_command(ftdmchan, FTDM_COMMAND_WINK, NULL);
				}
				break;
			case FTDM_CHANNEL_STATE_RINGING:
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
				ftdm_set_string(sig.ev_data.collected.digits, dtmf);
				if (ftdm_span_send_signal(ftdmchan->span, &sig) == FTDM_BREAK) {
					collecting = 0;
				}
			}
		}


		if (last_digit && (!collecting || ((elapsed - last_digit > analog_data->digit_timeout) || strlen(dtmf) > analog_data->max_dialstr))) {
			ftdm_log(FTDM_LOG_DEBUG, "Number obtained [%s]\n", dtmf);
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RING);
			last_digit = 0;
			collecting = 0;
		}

		if (ftdm_channel_wait(ftdmchan, &flags, interval * 2) != FTDM_SUCCESS) {
			continue;
		}

		if (!(flags & FTDM_READ)) {
			continue;
		}

		len = sizeof(frame);
		if (ftdm_channel_read(ftdmchan, frame, &len) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "READ ERROR [%s]\n", ftdmchan->last_error);
			goto done;
		}

		if (0 == len) {
			ftdm_log(FTDM_LOG_DEBUG, "Nothing read\n");
			continue;
		}

		if (ftdmchan->detected_tones[0]) {
			int i;
			
			for (i = 1; i < FTDM_TONEMAP_INVALID; i++) {
				if (ftdmchan->detected_tones[i]) {
					ftdm_log(FTDM_LOG_DEBUG, "Detected tone %s on %d:%d\n", ftdm_tonemap2str(i), ftdmchan->span_id, ftdmchan->chan_id);
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
			} else if (ftdmchan->detected_tones[FTDM_TONEMAP_RING]) {
				if (!analog_data->answer_supervision) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);
				} else {
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Ringing, but not answering since answer supervision is enabled\n");
				}
			}
			
			ftdm_channel_clear_detected_tones(ftdmchan);
		}

		if ((ftdmchan->dtmf_buffer && ftdm_buffer_inuse(ftdmchan->dtmf_buffer))) {
			rlen = len;
			memset(frame, 0, len);
			ftdm_channel_write(ftdmchan, frame, sizeof(frame), &rlen);
			continue;
		}
		
		if (!indicate) {
			continue;
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
				codec_func(frame, sizeof(frame), &rlen);
			} else {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "codec error!");
				goto done;
			}
		}

		ftdm_channel_write(ftdmchan, frame, sizeof(frame), &rlen);
	}

 done:

	ftdm_channel_command(ftdmchan, FTDM_COMMAND_ONHOOK, NULL);
	
	closed_chan = ftdmchan;
	ftdm_channel_close(&ftdmchan);

	ftdm_channel_command(closed_chan, FTDM_COMMAND_SET_NATIVE_CODEC, NULL);

	if (ts.buffer) {
		teletone_destroy_session(&ts);
	}

	if (dt_buffer) {
		ftdm_buffer_destroy(&dt_buffer);
	}

	ftdm_clear_flag(closed_chan, FTDM_CHANNEL_INTHREAD);

	ftdm_log(FTDM_LOG_DEBUG, "ANALOG EM CHANNEL thread ended.\n");

	return NULL;
}

/**
 * \brief Processes EM events coming from ftdmtel/dahdi
 * \param span Span on which the event was fired
 * \param event Event to be treated
 * \return Success or failure
 */
static __inline__ ftdm_status_t process_event(ftdm_span_t *span, ftdm_event_t *event)
{
	ftdm_sigmsg_t sig;
	int locked = 0;
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = event->channel->chan_id;
	sig.span_id = event->channel->span_id;
	sig.channel = event->channel;

	ftdm_unused_arg(span);
	ftdm_log(FTDM_LOG_DEBUG, "EVENT [%s][%d:%d] STATE [%s]\n", 
			ftdm_oob_event2str(event->enum_id), event->channel->span_id, event->channel->chan_id, ftdm_channel_state2str(event->channel->state));

	ftdm_mutex_lock(event->channel->mutex);
	locked++;

	switch(event->enum_id) {
	case FTDM_OOB_ONHOOK:
		{
			if (event->channel->state != FTDM_CHANNEL_STATE_DOWN) {
				ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_DOWN);
			}

		}
		break;
	case FTDM_OOB_OFFHOOK:
		{
			if (ftdm_test_flag(event->channel, FTDM_CHANNEL_INTHREAD)) {
				if (event->channel->state < FTDM_CHANNEL_STATE_UP) {
					ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_UP);
				}
			} else {
				ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_DIALTONE);
				ftdm_mutex_unlock(event->channel->mutex);
				locked = 0;
				ftdm_thread_create_detached(ftdm_analog_em_channel_run, event->channel);
			}
		break;
		}
	case FTDM_OOB_WINK:
		{
			if (event->channel->state != FTDM_CHANNEL_STATE_DIALING) {
				ftdm_set_state_locked(event->channel, FTDM_CHANNEL_STATE_DOWN);
			} else {
				ftdm_set_flag_locked(event->channel, FTDM_CHANNEL_WINK);
			}

		}
		break;
	}
	if (locked) {
		ftdm_mutex_unlock(event->channel->mutex);
	}
	return FTDM_SUCCESS;
}

/**
 * \brief Main thread function for EM span (monitor)
 * \param me Current thread
 * \param obj Span to run in this thread
 */
static void *ftdm_analog_em_run(ftdm_thread_t *me, void *obj)
{
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_analog_em_data_t *analog_data = span->signal_data;

	ftdm_unused_arg(me);
	ftdm_log(FTDM_LOG_DEBUG, "ANALOG EM thread starting.\n");

	while(ftdm_running() && ftdm_test_flag(analog_data, FTDM_ANALOG_EM_RUNNING)) {
		int waitms = 10;
		ftdm_status_t status;

		status = ftdm_span_poll_event(span, waitms, NULL);
		
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
			}
			break;
		default:
			break;
		}

	}

 end:

	ftdm_clear_flag(analog_data, FTDM_ANALOG_EM_RUNNING);
	
	ftdm_log(FTDM_LOG_DEBUG, "ANALOG EM thread ending.\n");

	return NULL;
}

/**
 * \brief FreeTDM analog EM module initialisation
 * \return Success
 */
static FIO_SIG_LOAD_FUNCTION(ftdm_analog_em_init)
{
	return FTDM_SUCCESS;
}

/**
 * \brief FreeTDM analog EM module definition
 */
EX_DECLARE_DATA ftdm_module_t ftdm_module = {
	"analog_em",
	NULL,
	NULL,
	ftdm_analog_em_init,
	ftdm_analog_em_configure_span,
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
