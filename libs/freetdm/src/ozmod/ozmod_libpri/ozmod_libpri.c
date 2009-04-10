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
#include "ozmod_libpri.h"

static ZIO_IO_UNLOAD_FUNCTION(zap_libpri_unload)
{
	return ZAP_SUCCESS;
}

static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(isdn_outgoing_call)
{
	zap_status_t status = ZAP_SUCCESS;
	zap_set_flag(zchan, ZAP_CHANNEL_OUTBOUND);
	zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DIALING);
	return status;
}

static ZIO_CHANNEL_REQUEST_FUNCTION(isdn_channel_request)
{
	return ZAP_FAIL;

}

#ifdef WIN32
static void s_pri_error(char *s)
#else
static void s_pri_error(struct pri *pri, char *s)
#endif
{
	zap_log(ZAP_LOG_ERROR, "%s", s);
}

#ifdef WIN32
static void s_pri_message(char *s)
#else
static void s_pri_message(struct pri *pri, char *s)
#endif
{
		zap_log(ZAP_LOG_DEBUG, "%s", s);
}

static uint32_t parse_opts(const char *in)
{
	uint32_t flags = 0;
	
	if (!in) {
		return 0;
	}
	
	if (strstr(in, "suggest_channel")) {
		flags |= OZMOD_LIBPRI_OPT_SUGGEST_CHANNEL;
	}
	
	if (strstr(in, "omit_display")) {
		flags |= OZMOD_LIBPRI_OPT_OMIT_DISPLAY_IE;
	}
	
	if (strstr(in, "omit_redirecting_number")) {
		flags |= OZMOD_LIBPRI_OPT_OMIT_REDIRECTING_NUMBER_IE;
	}

	return flags;
}


static int parse_debug(const char *in)
{
	int flags = 0;

	if (!in) {
		return 0;
	}

	if (strstr(in, "q921_raw")) {
		flags |= PRI_DEBUG_Q921_RAW;
	}

	if (strstr(in, "q921_dump")) {
		flags |= PRI_DEBUG_Q921_DUMP;
	}

	if (strstr(in, "q921_state")) {
		flags |= PRI_DEBUG_Q921_STATE;
	}

	if (strstr(in, "config")) {
		flags |= PRI_DEBUG_CONFIG;
	}

	if (strstr(in, "q931_dump")) {
		flags |= PRI_DEBUG_Q931_DUMP;
	}

	if (strstr(in, "q931_state")) {
		flags |= PRI_DEBUG_Q931_STATE;
	}

	if (strstr(in, "q931_anomaly")) {
		flags |= PRI_DEBUG_Q931_ANOMALY;
	}

	if (strstr(in, "apdu")) {
		flags |= PRI_DEBUG_APDU;
	}

	if (strstr(in, "aoc")) {
		flags |= PRI_DEBUG_AOC;
	}

	if (strstr(in, "all")) {
		flags |= PRI_DEBUG_ALL;
	}

	if (strstr(in, "none")) {
		flags = 0;
	}

	return flags;
}

static zap_io_interface_t zap_libpri_interface;

static zap_status_t zap_libpri_start(zap_span_t *span);

static ZIO_API_FUNCTION(zap_libpri_api)
{
	char *mycmd = NULL, *argv[10] = { 0 };
    int argc = 0;
	
	if (data) {
		mycmd = strdup(data);
		argc = zap_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc == 2) {
		if (!strcasecmp(argv[0], "kill")) {
			int span_id = atoi(argv[1]);
			zap_span_t *span = NULL;

			if (zap_span_find_by_name(argv[1], &span) == ZAP_SUCCESS || zap_span_find(span_id, &span) == ZAP_SUCCESS) {
				zap_libpri_data_t *isdn_data = span->signal_data;

				if (span->start != zap_libpri_start) {
					stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
					goto done;
				}

				zap_clear_flag((&isdn_data->spri), LPWRAP_PRI_READY);
				stream->write_function(stream, "%s: +OK killed.\n", __FILE__);
				goto done;
			} else {
				stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
				goto done;
			}
		}
	}

	if (argc > 2) {
		if (!strcasecmp(argv[0], "debug")) {
			zap_span_t *span = NULL;

			if (zap_span_find_by_name(argv[1], &span) == ZAP_SUCCESS) {
				zap_libpri_data_t *isdn_data = span->signal_data;
				if (span->start != zap_libpri_start) {
					stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
					goto done;
				}

				pri_set_debug(isdn_data->spri.pri, parse_debug(argv[2]));				
				stream->write_function(stream, "%s: +OK debug set.\n", __FILE__);
				goto done;
			} else {
				stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
				goto done;
			}
		}

	}

	stream->write_function(stream, "%s: -ERR invalid command.\n", __FILE__);
	
 done:

	zap_safe_free(mycmd);

	return ZAP_SUCCESS;
}


static ZIO_IO_LOAD_FUNCTION(zap_libpri_io_init)
{
	assert(zio != NULL);
	memset(&zap_libpri_interface, 0, sizeof(zap_libpri_interface));

	zap_libpri_interface.name = "libpri";
	zap_libpri_interface.api = zap_libpri_api;

	*zio = &zap_libpri_interface;

	return ZAP_SUCCESS;
}

static ZIO_SIG_LOAD_FUNCTION(zap_libpri_init)
{
	pri_set_error(s_pri_error);
	pri_set_message(s_pri_message);
	return ZAP_SUCCESS;
}


static zap_state_map_t isdn_state_map = {
	{
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_ANY_STATE},
			{ZAP_CHANNEL_STATE_RESTART, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_RESTART, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
			{ZAP_CHANNEL_STATE_DIALING, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_DIALING, ZAP_END},
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_UP, ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_UP, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_CHANNEL_STATE_DOWN, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_UP, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END}
		},

		/****************************************/
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_ANY_STATE},
			{ZAP_CHANNEL_STATE_RESTART, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_RESTART, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
			{ZAP_CHANNEL_STATE_DIALTONE, ZAP_CHANNEL_STATE_RING, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_DIALTONE, ZAP_END},
			{ZAP_CHANNEL_STATE_RING, ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_RING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_UP, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_CHANNEL_STATE_DOWN, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, 
			 ZAP_CHANNEL_STATE_CANCEL, ZAP_CHANNEL_STATE_UP, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_UP, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
		},
		

	}
};





static __inline__ void state_advance(zap_channel_t *zchan)
{
	//Q931mes_Generic *gen = (Q931mes_Generic *) zchan->caller_data.raw_data;
	zap_libpri_data_t *isdn_data = zchan->span->signal_data;
	zap_status_t status;
	zap_sigmsg_t sig;
	q931_call *call = (q931_call *) zchan->call_data;
	
	
	zap_log(ZAP_LOG_DEBUG, "%d:%d STATE [%s]\n", 
			zchan->span_id, zchan->chan_id, zap_channel_state2str(zchan->state));


#if 0
	if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND) && !call) {
		zap_log(ZAP_LOG_WARNING, "NO CALL!!!!\n");
	}
#endif

	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = zchan->chan_id;
	sig.span_id = zchan->span_id;
	sig.channel = zchan;


	switch (zchan->state) {
	case ZAP_CHANNEL_STATE_DOWN:
		{
			zchan->call_data = NULL;
			zap_channel_done(zchan);			
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS;
				if ((status = isdn_data->sig_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				pri_progress(isdn_data->spri.pri, call, zchan->chan_id, 1);
			} else {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS_MEDIA;
				if ((status = isdn_data->sig_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				pri_proceeding(isdn_data->spri.pri, call, zchan->chan_id, 1);
			} else {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RING:
		{
			if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				if (call) {
					pri_acknowledge(isdn_data->spri.pri, call, zchan->chan_id, 0);
					sig.event_id = ZAP_SIGEVENT_START;
					if ((status = isdn_data->sig_cb(&sig) != ZAP_SUCCESS)) {
						zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
					}
				} else {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
				}
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RESTART:
		{
			zchan->caller_data.hangup_cause = ZAP_CAUSE_NORMAL_UNSPECIFIED;
			sig.event_id = ZAP_SIGEVENT_RESTART;
			status = isdn_data->sig_cb(&sig);
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
		}
		break;
	case ZAP_CHANNEL_STATE_UP:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_UP;
				if ((status = isdn_data->sig_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else if (call) {
				pri_answer(isdn_data->spri.pri, call, 0, 1);
			} else {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_DIALING:
		if (isdn_data) {
			struct pri_sr *sr;
			int dp;

			if (!(call = pri_new_call(isdn_data->spri.pri))) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
				return;
			}

			
			dp = zchan->caller_data.ani.type;
			switch(dp) {
			case ZAP_TON_NATIONAL:
				dp = PRI_NATIONAL_ISDN;
				break;
			case ZAP_TON_INTERNATIONAL:
				dp = PRI_INTERNATIONAL_ISDN;
				break;
			case ZAP_TON_SUBSCRIBER_NUMBER:
				dp = PRI_LOCAL_ISDN;
				break;
			default:
				dp = isdn_data->dp;
			}

			zchan->call_data = call;
			sr = pri_sr_new();
			assert(sr);
			pri_sr_set_channel(sr, zchan->chan_id, 0, 0);
			pri_sr_set_bearer(sr, 0, isdn_data->l1);
			pri_sr_set_called(sr, zchan->caller_data.ani.digits, dp, 1);
			pri_sr_set_caller(sr, zchan->caller_data.cid_num.digits, (isdn_data->opts & OZMOD_LIBPRI_OPT_OMIT_DISPLAY_IE ? NULL : zchan->caller_data.cid_name),
						dp, PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN);

			if (!(isdn_data->opts & OZMOD_LIBPRI_OPT_OMIT_REDIRECTING_NUMBER_IE)) {
				pri_sr_set_redirecting(sr, zchan->caller_data.cid_num.digits, dp, PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN, PRI_REDIR_UNCONDITIONAL);
			}

			if (pri_setup(isdn_data->spri.pri, call, sr)) {
				zchan->caller_data.hangup_cause = ZAP_CAUSE_DESTINATION_OUT_OF_ORDER;				
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
			}

			pri_sr_free(sr);
		}

		break;
	case ZAP_CHANNEL_STATE_HANGUP:
		{
			if (call) {
				pri_hangup(isdn_data->spri.pri, call, zchan->caller_data.hangup_cause);
				pri_destroycall(isdn_data->spri.pri, call);
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
			} else {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_HANGUP_COMPLETE:
		break;
	case ZAP_CHANNEL_STATE_TERMINATING:
		{
			sig.event_id = ZAP_SIGEVENT_STOP;
			status = isdn_data->sig_cb(&sig);
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);

		}
	default:
		break;
	}



	return;
}

static __inline__ void check_state(zap_span_t *span)
{
    if (zap_test_flag(span, ZAP_SPAN_STATE_CHANGE)) {
        uint32_t j;
        zap_clear_flag_locked(span, ZAP_SPAN_STATE_CHANGE);
        for(j = 1; j <= span->chan_count; j++) {
            if (zap_test_flag((span->channels[j]), ZAP_CHANNEL_STATE_CHANGE)) {
				zap_mutex_lock(span->channels[j]->mutex);
                zap_clear_flag((span->channels[j]), ZAP_CHANNEL_STATE_CHANGE);
                state_advance(span->channels[j]);
                zap_channel_complete_state(span->channels[j]);
				zap_mutex_unlock(span->channels[j]->mutex);
            }
        }
    }
}



static int on_info(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{

	zap_log(ZAP_LOG_DEBUG, "number is: %s\n", pevent->ring.callednum);
	if (strlen(pevent->ring.callednum) > 3) {
		zap_log(ZAP_LOG_DEBUG, "final number is: %s\n", pevent->ring.callednum);
		pri_answer(spri->pri, pevent->ring.call, 0, 1);
	}
	return 0;
}

static int on_hangup(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	zap_span_t *span = spri->private_info;
	zap_channel_t *zchan = NULL;
	q931_call *call = NULL;
	zchan = span->channels[pevent->hangup.channel];
	
	if (zchan) {
		call = (q931_call *) zchan->call_data;
		zap_log(ZAP_LOG_DEBUG, "-- Hangup on channel %d:%d\n", spri->span->span_id, pevent->hangup.channel);
		zchan->caller_data.hangup_cause = pevent->hangup.cause;
		pri_release(spri->pri, call, 0);
		pri_destroycall(spri->pri, call);
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_TERMINATING);
	} else {
		zap_log(ZAP_LOG_DEBUG, "-- Hangup on channel %d:%d %s but it's not in use?\n", spri->span->span_id,
				pevent->hangup.channel, zchan->chan_id);
	}

	return 0;
}

static int on_answer(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	zap_span_t *span = spri->private_info;
	zap_channel_t *zchan = NULL;

	zchan = span->channels[pevent->answer.channel];
	
	if (zchan) {
		zap_log(ZAP_LOG_DEBUG, "-- Answer on channel %d:%d\n", spri->span->span_id, pevent->answer.channel);
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_UP);
	} else {
		zap_log(ZAP_LOG_DEBUG, "-- Answer on channel %d:%d %s but it's not in use?\n", spri->span->span_id, pevent->answer.channel, zchan->chan_id);
				
	}

	return 0;
}


static int on_proceed(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	zap_span_t *span = spri->private_info;
	zap_channel_t *zchan = NULL;
	
	zchan = span->channels[pevent->proceeding.channel];
	
	if (zchan) {
		zap_log(ZAP_LOG_DEBUG, "-- Proceeding on channel %d:%d\n", spri->span->span_id, pevent->proceeding.channel);
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_PROGRESS_MEDIA);
	} else {
		zap_log(ZAP_LOG_DEBUG, "-- Proceeding on channel %d:%d %s but it's not in use?\n", spri->span->span_id,
						  pevent->proceeding.channel, zchan->chan_id);
	}

	return 0;
}


static int on_ringing(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	zap_span_t *span = spri->private_info;
	zap_channel_t *zchan = NULL;

	zchan = span->channels[pevent->ringing.channel];
	
	if (zchan) {
		zap_log(ZAP_LOG_DEBUG, "-- Ringing on channel %d:%d\n", spri->span->span_id, pevent->ringing.channel);
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_PROGRESS);
	} else {
		zap_log(ZAP_LOG_DEBUG, "-- Ringing on channel %d:%d %s but it's not in use?\n", spri->span->span_id,
						  pevent->ringing.channel, zchan->chan_id);
	}

	return 0;
}


static int on_ring(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	zap_span_t *span = spri->private_info;
	zap_channel_t *zchan = NULL;
	int ret = 0;

	//switch_mutex_lock(globals.channel_mutex);
	
	zchan = span->channels[pevent->ring.channel];
	if (!zchan || zchan->state != ZAP_CHANNEL_STATE_DOWN || zap_test_flag(zchan, ZAP_CHANNEL_INUSE)) {
		zap_log(ZAP_LOG_WARNING, "--Duplicate Ring on channel %d:%d (ignored)\n", spri->span->span_id, pevent->ring.channel);
		ret = 0;
		goto done;
	}

	if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_WARNING, "--Failure opening channel %d:%d (ignored)\n", spri->span->span_id, pevent->ring.channel);
		ret = 0;
		goto done;
	}
	

	zap_log(ZAP_LOG_NOTICE, "-- Ring on channel %d:%d (from %s to %s)\n", spri->span->span_id, pevent->ring.channel,
					  pevent->ring.callingnum, pevent->ring.callednum);
	
	memset(&zchan->caller_data, 0, sizeof(zchan->caller_data));
	
	zap_set_string(zchan->caller_data.cid_num.digits, (char *)pevent->ring.callingnum);
	if (!zap_strlen_zero((char *)pevent->ring.callingname)) {
		zap_set_string(zchan->caller_data.cid_name, (char *)pevent->ring.callingname);
	} else {
		zap_set_string(zchan->caller_data.cid_name, (char *)pevent->ring.callingnum);
	}
	zap_set_string(zchan->caller_data.ani.digits, (char *)pevent->ring.callingani);
	zap_set_string(zchan->caller_data.dnis.digits, (char *)pevent->ring.callednum);
	
	if (pevent->ring.ani2 >= 0) {
		snprintf(zchan->caller_data.aniII, 5, "%.2d", pevent->ring.ani2);
	}
	
	// scary to trust this pointer, you'd think they would give you a copy of the call data so you own it......
	zchan->call_data = pevent->ring.call;
	
	zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RING);
	
 done:
	//switch_mutex_unlock(globals.channel_mutex);

	return ret;
}

static int check_flags(lpwrap_pri_t *spri)
{
	zap_span_t *span = spri->private_info;

	check_state(span);

	if (!zap_running() || zap_test_flag(span, ZAP_SPAN_STOP_THREAD)) {
		return -1;
	}

	return 0;
}

static int on_restart(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	zap_span_t *span = spri->private_info;
	zap_channel_t *zchan;

	zap_log(ZAP_LOG_NOTICE, "-- Restarting %d:%d\n", spri->span->span_id, pevent->restart.channel);

	zchan = span->channels[pevent->restart.channel];

	if (!zchan) {
		return 0;
	}

	if (pevent->restart.channel < 1) {
		zap_set_state_all(zchan->span, ZAP_CHANNEL_STATE_RESTART);
	} else {
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
	}

	return 0;
}

static int on_dchan_up(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{
	
	if (!zap_test_flag(spri, LPWRAP_PRI_READY)) {
		zap_log(ZAP_LOG_INFO, "Span %d D-Chan UP!\n", spri->span->span_id);
		zap_set_flag(spri, LPWRAP_PRI_READY);
	}

	return 0;
}

static int on_dchan_down(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{

	if (zap_test_flag(spri, LPWRAP_PRI_READY)) {
		zap_log(ZAP_LOG_INFO, "Span %d D-Chan DOWN!\n", spri->span->span_id);
		zap_clear_flag(spri, LPWRAP_PRI_READY);
	}

	return 0;
}

static int on_anything(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{

	zap_log(ZAP_LOG_DEBUG, "Caught Event span %d %u (%s)\n", spri->span->span_id, event_type, lpwrap_pri_event_str(event_type));
	return 0;
}



static int on_io_fail(lpwrap_pri_t *spri, lpwrap_pri_event_t event_type, pri_event *pevent)
{

	zap_log(ZAP_LOG_DEBUG, "Caught Event span %d %u (%s)\n", spri->span->span_id, event_type, lpwrap_pri_event_str(event_type));
	return 0;
}


static void *zap_libpri_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	zap_libpri_data_t *isdn_data = span->signal_data;
	int x, i;
	int down = 0;
	
	zap_set_flag(span, ZAP_SPAN_IN_THREAD);
	
	while(zap_running() && !zap_test_flag(span, ZAP_SPAN_STOP_THREAD)) {
		x = 0;

		for(i = 1; i <= span->chan_count; i++) {
			if (span->channels[i]->type == ZAP_CHAN_TYPE_DQ921) {
				if (zap_channel_open(span->span_id, i, &isdn_data->dchan) == ZAP_SUCCESS) {
					zap_log(ZAP_LOG_DEBUG, "opening d-channel #%d %d:%d\n", x, isdn_data->dchan->span_id, isdn_data->dchan->chan_id);
					isdn_data->dchan->state = ZAP_CHANNEL_STATE_UP;
					x++;
					break;
				}
			}
		}

		
		if (!x || lpwrap_init_pri(&isdn_data->spri,
								  span,  // span
								  isdn_data->dchan, // dchan
								  isdn_data->pswitch,
								  isdn_data->node,
								  isdn_data->debug) < 0) {
			snprintf(span->last_error, sizeof(span->last_error), "PRI init FAIL!");
		} else {

			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_ANY, on_anything);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RING, on_ring);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RINGING, on_ringing);
			//LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_SETUP_ACK, on_proceed);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_PROCEEDING, on_proceed);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_ANSWER, on_answer);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_DCHAN_UP, on_dchan_up);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_DCHAN_DOWN, on_dchan_down);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_HANGUP_REQ, on_hangup);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_HANGUP, on_hangup);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_INFO_RECEIVED, on_info);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_RESTART, on_restart);
			LPWRAP_MAP_PRI_EVENT(isdn_data->spri, LPWRAP_PRI_EVENT_IO_FAIL, on_io_fail);
			
			if (down) {
				zap_log(ZAP_LOG_INFO, "PRI back up on span %d\n", isdn_data->spri.span->span_id);
				zap_set_state_all(span, ZAP_CHANNEL_STATE_RESTART);
				down = 0;
			}

			isdn_data->spri.on_loop = check_flags;
			isdn_data->spri.private_info = span;
			
			lpwrap_run_pri(&isdn_data->spri);
			zap_channel_close(&isdn_data->dchan);
		}
		
		if (!zap_running() || zap_test_flag(span, ZAP_SPAN_STOP_THREAD)) {
			break;
		}

		zap_log(ZAP_LOG_CRIT, "PRI down on span %d\n", isdn_data->spri.span->span_id);
		zap_set_state_all(span, ZAP_CHANNEL_STATE_RESTART);
		check_state(span);
		check_state(span);
		down++;
		zap_sleep(5000);
	}

	zap_log(ZAP_LOG_DEBUG, "PRI thread ended on span %d\n", isdn_data->spri.span->span_id);

	zap_clear_flag(span, ZAP_SPAN_IN_THREAD);
	zap_clear_flag(isdn_data, OZMOD_LIBPRI_RUNNING);

	return NULL;
}


static zap_status_t zap_libpri_stop(zap_span_t *span)
{
	zap_libpri_data_t *isdn_data = span->signal_data;

	if (!zap_test_flag(isdn_data, OZMOD_LIBPRI_RUNNING)) {
		return ZAP_FAIL;
	}

	zap_set_state_all(span, ZAP_CHANNEL_STATE_RESTART);
	check_state(span);
	zap_set_flag(span, ZAP_SPAN_STOP_THREAD);
	while(zap_test_flag(span, ZAP_SPAN_IN_THREAD)) {
		zap_sleep(100);
	}
	check_state(span);

	return ZAP_SUCCESS;
}

static zap_status_t zap_libpri_start(zap_span_t *span)
{
	zap_status_t ret;
	zap_libpri_data_t *isdn_data = span->signal_data;

	if (zap_test_flag(isdn_data, OZMOD_LIBPRI_RUNNING)) {
		return ZAP_FAIL;
	}

	zap_clear_flag(span, ZAP_SPAN_STOP_THREAD);
	zap_clear_flag(span, ZAP_SPAN_IN_THREAD);

	zap_set_flag(isdn_data, OZMOD_LIBPRI_RUNNING);
	ret = zap_thread_create_detached(zap_libpri_run, span);

	if (ret != ZAP_SUCCESS) {
		return ret;
	}

	return ret;
}


static int str2node(char *node)
{
	if (!strcasecmp(node, "cpe") || !strcasecmp(node, "user"))
		return PRI_CPE;
	if (!strcasecmp(node, "network") || !strcasecmp(node, "net"))
		return PRI_NETWORK;
	return -1;
}

static int str2switch(char *swtype)
{
	if (!strcasecmp(swtype, "ni1"))
		return PRI_SWITCH_NI1;
	if (!strcasecmp(swtype, "ni2"))
		return PRI_SWITCH_NI2;
	if (!strcasecmp(swtype, "dms100"))
		return PRI_SWITCH_DMS100;
	if (!strcasecmp(swtype, "lucent5e") || !strcasecmp(swtype, "5ess"))
		return PRI_SWITCH_LUCENT5E;
	if (!strcasecmp(swtype, "att4ess") || !strcasecmp(swtype, "4ess"))
		return PRI_SWITCH_ATT4ESS;
	if (!strcasecmp(swtype, "euroisdn"))
		return PRI_SWITCH_EUROISDN_E1;
	if (!strcasecmp(swtype, "gr303eoc"))
		return PRI_SWITCH_GR303_EOC;
	if (!strcasecmp(swtype, "gr303tmc"))
		return PRI_SWITCH_GR303_TMC;
	return PRI_SWITCH_DMS100;
}


static int str2l1(char *l1)
{
	if (!strcasecmp(l1, "alaw"))
		return PRI_LAYER_1_ALAW;
	
	return PRI_LAYER_1_ULAW;
}

static int str2dp(char *dp)
{
	if (!strcasecmp(dp, "international"))
		return PRI_INTERNATIONAL_ISDN;
	if (!strcasecmp(dp, "national"))
		return PRI_NATIONAL_ISDN;
	if (!strcasecmp(dp, "local"))
		return PRI_LOCAL_ISDN;
	if (!strcasecmp(dp, "private"))
		return PRI_PRIVATE;
	if (!strcasecmp(dp, "unknown"))
		return PRI_UNKNOWN;

	return PRI_UNKNOWN;
}

static ZIO_SIG_CONFIGURE_FUNCTION(zap_libpri_configure_span)
{
	uint32_t i, x = 0;
	//zap_channel_t *dchans[2] = {0};
	zap_libpri_data_t *isdn_data;
	char *var, *val;
	char *debug = NULL;

	if (span->trunk_type >= ZAP_TRUNK_NONE) {
		zap_log(ZAP_LOG_WARNING, "Invalid trunk type '%s' defaulting to T1.\n", zap_trunk_type2str(span->trunk_type));
		span->trunk_type = ZAP_TRUNK_T1;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->type == ZAP_CHAN_TYPE_DQ921) {
			if (x > 1) {
				snprintf(span->last_error, sizeof(span->last_error), "Span has more than 2 D-Channels!");
				return ZAP_FAIL;
			} else {
#if 0
				if (zap_channel_open(span->span_id, i, &dchans[x]) == ZAP_SUCCESS) {
					zap_log(ZAP_LOG_DEBUG, "opening d-channel #%d %d:%d\n", x, dchans[x]->span_id, dchans[x]->chan_id);
					dchans[x]->state = ZAP_CHANNEL_STATE_UP;
					x++;
				}
#endif
			}
		}
	}
	
#if 0
	if (!x) {
		snprintf(span->last_error, sizeof(span->last_error), "Span has no D-Channels!");
		return ZAP_FAIL;
	}
#endif

	isdn_data = malloc(sizeof(*isdn_data));
	assert(isdn_data != NULL);
	memset(isdn_data, 0, sizeof(*isdn_data));
	
	while((var = va_arg(ap, char *))) {
		if (!strcasecmp(var, "node")) {
			int node;
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			node = str2node(val);
			if (-1 == node) {
				zap_log(ZAP_LOG_ERROR, "Unknown node type %s, defaulting to CPE mode\n", val);
				node = PRI_CPE;
			}
			isdn_data->node = node;
		} else if (!strcasecmp(var, "switch")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			isdn_data->pswitch = str2switch(val);
		} else if (!strcasecmp(var, "opts")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			isdn_data->opts = parse_opts(val);
		} else if (!strcasecmp(var, "dp")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			isdn_data->dp = str2dp(val);
		} else if (!strcasecmp(var, "l1")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			isdn_data->l1 = str2l1(val);
		} else if (!strcasecmp(var, "debug")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			debug = val;
		} else {
			snprintf(span->last_error, sizeof(span->last_error), "Unknown parameter [%s]", var);
			return ZAP_FAIL;
		}
	}
	

	span->start = zap_libpri_start;
	span->stop = zap_libpri_stop;
	isdn_data->sig_cb = sig_cb;
	//isdn_data->dchans[0] = dchans[0];
	//isdn_data->dchans[1] = dchans[1];
	//isdn_data->dchan = isdn_data->dchans[0];
	
	isdn_data->debug = parse_debug(debug);
		

	span->signal_data = isdn_data;
	span->signal_type = ZAP_SIGTYPE_ISDN;
	span->outgoing_call = isdn_outgoing_call;

	if ((isdn_data->opts & OZMOD_LIBPRI_OPT_SUGGEST_CHANNEL)) {
		span->channel_request = isdn_channel_request;
		span->suggest_chan_id = 1;
	}

	span->state_map = &isdn_state_map;

	return ZAP_SUCCESS;
}

zap_module_t zap_module = { 
	"libpri",
	zap_libpri_io_init,
	zap_libpri_unload,
	zap_libpri_init,
	zap_libpri_configure_span,
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
