/*
 * Copyright (c) 2010, Moises Silva <moy@sangoma.com>
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

#include <libpri.h>
#include <poll.h>
#include "private/ftdm_core.h"

typedef enum {
	PRITAP_RUNNING = (1 << 0),
} pritap_flags_t;

typedef struct pritap {
	int32_t flags;
	struct pri *pri;
	int debug;
	ftdm_channel_t *dchan;
	ftdm_span_t *span;
	ftdm_span_t *peerspan;
	struct pritap *pritap;
} pritap_t;

static FIO_IO_UNLOAD_FUNCTION(ftdm_pritap_unload)
{
	return FTDM_SUCCESS;
}

static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(pritap_get_channel_sig_status)
{
	*status = FTDM_SIG_STATE_UP;
	return FTDM_SUCCESS;
}

static FIO_SPAN_GET_SIG_STATUS_FUNCTION(pritap_get_span_sig_status)
{
	*status = FTDM_SIG_STATE_UP;
	return FTDM_SUCCESS;
}


static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(pritap_outgoing_call)
{
	ftdm_log(FTDM_LOG_ERROR, "Cannot dial on PRI tapping line!\n");
	return FTDM_FAIL;
}

static void s_pri_error(struct pri *pri, char *s)
{
	ftdm_log(FTDM_LOG_ERROR, "%s", s);
}

static void s_pri_message(struct pri *pri, char *s)
{
	ftdm_log(FTDM_LOG_DEBUG, "%s", s);
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

static ftdm_io_interface_t ftdm_pritap_interface;

static ftdm_status_t ftdm_pritap_start(ftdm_span_t *span);

static FIO_API_FUNCTION(ftdm_pritap_api)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;
	
	if (data) {
		mycmd = ftdm_strdup(data);
		argc = ftdm_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc > 2) {
		if (!strcasecmp(argv[0], "debug")) {
			ftdm_span_t *span = NULL;

			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS) {
				pritap_t *pritap = span->signal_data;
				if (span->start != ftdm_pritap_start) {
					stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
					goto done;
				}

				pri_set_debug(pritap->pri, parse_debug(argv[2]));				
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

	ftdm_safe_free(mycmd);

	return FTDM_SUCCESS;
}

static FIO_IO_LOAD_FUNCTION(ftdm_pritap_io_init)
{
	memset(&ftdm_pritap_interface, 0, sizeof(ftdm_pritap_interface));

	ftdm_pritap_interface.name = "pritap";
	ftdm_pritap_interface.api = ftdm_pritap_api;

	*fio = &ftdm_pritap_interface;

	return FTDM_SUCCESS;
}

static FIO_SIG_LOAD_FUNCTION(ftdm_pritap_init)
{
	pri_set_error(s_pri_error);
	pri_set_message(s_pri_message);
	return FTDM_SUCCESS;
}

static ftdm_state_map_t pritap_state_map = {
	{
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
			{FTDM_CHANNEL_STATE_RING, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, 
			 FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_UP, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
		},
		

	}
};

static __inline__ void state_advance(ftdm_channel_t *ftdmchan)
{
	pritap_t *pritap = ftdmchan->span->signal_data;
	ftdm_status_t status;
	ftdm_sigmsg_t sig;
	q931_call *call = (q931_call *) ftdmchan->call_data;
	
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "processing state %s\n", ftdmchan->span_id, ftdmchan->chan_id, ftdm_channel_state2str(ftdmchan->state));

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;

	switch (ftdmchan->state) {
	case FTDM_CHANNEL_STATE_DOWN:
		{
			ftdmchan->call_data = NULL;
			ftdm_channel_done(ftdmchan);			
		}
		break;

	case FTDM_CHANNEL_STATE_PROGRESS:
		{
			pri_progress(pritap->pri, call, ftdmchan->chan_id, 1);
		}
		break;

	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			pri_proceeding(pritap->pri, call, ftdmchan->chan_id, 1);
		}
		break;

	case FTDM_CHANNEL_STATE_RING:
		{
			pri_acknowledge(pritap->pri, call, ftdmchan->chan_id, 0);
			sig.event_id = FTDM_SIGEVENT_START;
			if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_UP:
		{
			pri_answer(pritap->pri, call, 0, 1);
		}
		break;

	case FTDM_CHANNEL_STATE_HANGUP:
		{
			if (call) {
				pri_hangup(pritap->pri, call, ftdmchan->caller_data.hangup_cause);
				pri_destroycall(pritap->pri, call);
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			} else {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
		break;

	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			sig.event_id = FTDM_SIGEVENT_STOP;
			status = ftdm_span_send_signal(ftdmchan->span, &sig);
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);

		}

	default:
		{
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "ignoring state change from %s to %s\n", ftdm_channel_state2str(ftdmchan->last_state), ftdm_channel_state2str(ftdmchan->state));
		}
		break;
	}

	return;
}

static __inline__ void pritap_check_state(ftdm_span_t *span)
{
    if (ftdm_test_flag(span, FTDM_SPAN_STATE_CHANGE)) {
        uint32_t j;
        ftdm_clear_flag_locked(span, FTDM_SPAN_STATE_CHANGE);
        for(j = 1; j <= span->chan_count; j++) {
            if (ftdm_test_flag((span->channels[j]), FTDM_CHANNEL_STATE_CHANGE)) {
		ftdm_mutex_lock(span->channels[j]->mutex);
                ftdm_clear_flag((span->channels[j]), FTDM_CHANNEL_STATE_CHANGE);
                state_advance(span->channels[j]);
                ftdm_channel_complete_state(span->channels[j]);
		ftdm_mutex_unlock(span->channels[j]->mutex);
            }
        }
    }
}

static int pri_io_read(struct pri *pri, void *buf, int buflen)
{
	int res;
	ftdm_status_t zst;
	pritap_t *pritap = pri_get_userdata(pri);
	ftdm_size_t len = buflen;

	if ((zst = ftdm_channel_read(pritap->dchan, buf, &len)) != FTDM_SUCCESS) {
		if (zst == FTDM_FAIL) {
			ftdm_log(FTDM_LOG_CRIT, "span %d D channel read fail! [%s]\n", pritap->span->span_id, pritap->dchan->last_error);
		} else {
			ftdm_log(FTDM_LOG_CRIT, "span %d D channel read timeout!\n", pritap->span->span_id);
		}
		return -1;
	}

	res = (int)len;

	memset(&((unsigned char*)buf)[res],0,2);

	res += 2;

	return res;
}

static int pri_io_write(struct pri *pri, void *buf, int buflen)
{
	pritap_t *pritap = pri_get_userdata(pri);
	ftdm_size_t len = buflen - 2; 

	if (ftdm_channel_write(pritap->dchan, buf, buflen, &len) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "span %d D channel write failed! [%s]\n", pritap->span->span_id, pritap->dchan->last_error);
		return -1; 
	}   

	return (int)buflen;
}

static void handle_pri_passive_event(pritap_t *pritap, pri_event *e)
{
	ftdm_log(FTDM_LOG_NOTICE, "passive event %s on span %s\n", pri_event2str(e->gen.e), pritap->span->name);

	switch (e->e) {

	case PRI_EVENT_RING:
		break;

	case PRI_EVENT_PROGRESS:
		break;

	case PRI_EVENT_PROCEEDING:
		break;

	case PRI_EVENT_ANSWER:
		break;

	case PRI_EVENT_HANGUP:
		break;

	case PRI_EVENT_HANGUP_ACK:
		break;

	default:
		ftdm_log(FTDM_LOG_DEBUG, "Ignoring passive event %s on span %s\n", pri_event2str(e->gen.e), pritap->span->name);
		break;

	}
}

static void *ftdm_pritap_run(ftdm_thread_t *me, void *obj)
{
	ftdm_span_t *span = (ftdm_span_t *) obj;
	pritap_t *pritap = span->signal_data;
	pri_event *event = NULL;
	struct pollfd dpoll = { 0, 0, 0 };
	int rc = 0;

	ftdm_log(FTDM_LOG_DEBUG, "Tapping PRI thread started on span %d\n", span->span_id);
	
	pritap->span = span;

	ftdm_set_flag(span, FTDM_SPAN_IN_THREAD);
	
	if (ftdm_channel_open(span->span_id, pritap->dchan->chan_id, &pritap->dchan) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to open D-channel for span %s\n", span->name);
		goto done;
	}

	if ((pritap->pri = pri_new_cb(pritap->dchan->sockfd, PRI_NETWORK, PRI_SWITCH_NI2, pri_io_read, pri_io_write, pritap))){
		pri_set_debug(pritap->pri, pritap->debug);
	} else {
		ftdm_log(FTDM_LOG_CRIT, "Failed to create tapping PRI\n");
		goto done;
	}

	dpoll.fd = pritap->dchan->sockfd;

	while (ftdm_running() && !ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD)) {


		pritap_check_state(span);

		dpoll.revents = 0;
		dpoll.events = POLLIN;

		rc = poll(&dpoll, 1, 10);

		if (rc < 0) {
			if (errno == EINTR) {
				ftdm_log(FTDM_LOG_DEBUG, "D-channel waiting interrupted, continuing ...\n");
				continue;
			}
			ftdm_log(FTDM_LOG_ERROR, "poll failed: %s\n", strerror(errno));
			continue;
		}

		pri_schedule_run(pritap->pri);

		if (rc) {
			if (dpoll.revents & POLLIN) {
				event = pri_read_event(pritap->pri);
				if (event) {
					handle_pri_passive_event(pritap, event);
				}
			} else {
				ftdm_log(FTDM_LOG_WARNING, "nothing to read?\n");
			}
		}

		pritap_check_state(span);
	}

done:
	ftdm_log(FTDM_LOG_DEBUG, "Tapping PRI thread ended on span %d\n", span->span_id);

	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);
	ftdm_clear_flag(pritap, PRITAP_RUNNING);

	return NULL;
}

static ftdm_status_t ftdm_pritap_stop(ftdm_span_t *span)
{
	pritap_t *pritap = span->signal_data;

	if (!ftdm_test_flag(pritap, PRITAP_RUNNING)) {
		return FTDM_FAIL;
	}

	ftdm_set_flag(span, FTDM_SPAN_STOP_THREAD);

	while (ftdm_test_flag(span, FTDM_SPAN_IN_THREAD)) {
		ftdm_sleep(100);
	}

	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_pritap_start(ftdm_span_t *span)
{
	ftdm_status_t ret;
	pritap_t *pritap = span->signal_data;

	if (ftdm_test_flag(pritap, PRITAP_RUNNING)) {
		return FTDM_FAIL;
	}

	ftdm_clear_flag(span, FTDM_SPAN_STOP_THREAD);
	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

	ftdm_set_flag(pritap, PRITAP_RUNNING);
	ret = ftdm_thread_create_detached(ftdm_pritap_run, span);

	if (ret != FTDM_SUCCESS) {
		return ret;
	}

	return ret;
}

static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_pritap_configure_span)
{
	uint32_t i;
	const char *var, *val;
	const char *debug = NULL;
	ftdm_channel_t *dchan = NULL;
	pritap_t *pritap = NULL;
	ftdm_span_t *peerspan = NULL;
	unsigned paramindex = 0;

	if (span->trunk_type >= FTDM_TRUNK_NONE) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid trunk type '%s' defaulting to T1.\n", ftdm_trunk_type2str(span->trunk_type));
		span->trunk_type = FTDM_TRUNK_T1;
	}
	
	for (i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->type == FTDM_CHAN_TYPE_DQ921) {
			dchan = span->channels[i];
		}
	}

	if (!dchan) {
		ftdm_log(FTDM_LOG_ERROR, "No d-channel specified in freetdm.conf!\n", ftdm_trunk_type2str(span->trunk_type));
		return FTDM_FAIL;
	}
	
	for (paramindex = 0; ftdm_parameters[paramindex].var; paramindex++) {
		var = ftdm_parameters[paramindex].var;
		val = ftdm_parameters[paramindex].val;
		ftdm_log(FTDM_LOG_DEBUG, "Tapping PRI key=value, %s=%s\n", var, val);

		if (!strcasecmp(var, "debug")) {
			debug = val;
		} else if (!strcasecmp(var, "peerspan")) {
			if (ftdm_span_find_by_name(val, &peerspan) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid tapping peer span %s\n", val);
				break;
			}
		} else {
			ftdm_log(FTDM_LOG_ERROR,  "Unknown pri tapping parameter [%s]", var);
		}
	}

	if (!peerspan) {
		ftdm_log(FTDM_LOG_ERROR, "No valid peerspan was specified!\n");
		return FTDM_FAIL;
	}
    
	pritap = ftdm_calloc(1, sizeof(*pritap));
	if (!pritap) {
		return FTDM_FAIL;
	}

	pritap->debug = parse_debug(debug);
	pritap->dchan = dchan;
	pritap->peerspan = peerspan;

	span->start = ftdm_pritap_start;
	span->stop = ftdm_pritap_stop;
	span->signal_cb = sig_cb;
	
	span->signal_data = pritap;
	span->signal_type = FTDM_SIGTYPE_ISDN;
	span->outgoing_call = pritap_outgoing_call;

	span->get_channel_sig_status = pritap_get_channel_sig_status;
	span->get_span_sig_status = pritap_get_span_sig_status;
	
	span->state_map = &pritap_state_map;

	return FTDM_SUCCESS;
}

/**
 * \brief FreeTDM pritap signaling and IO module definition
 */
ftdm_module_t ftdm_module = { 
	"pritap",
	ftdm_pritap_io_init,
	ftdm_pritap_unload,
	ftdm_pritap_init,
	NULL,
	NULL,
	ftdm_pritap_configure_span,
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
