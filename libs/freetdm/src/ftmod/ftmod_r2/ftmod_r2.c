/*
 * Copyright (c) 2009, Moises Silva <moy@sangoma.com>
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

#include <stdio.h>
#include <openr2.h>
#include "freetdm.h"

/* debug thread count for r2 legs */
static ftdm_mutex_t* g_thread_count_mutex;
static int32_t g_thread_count = 0;

/* when the users kills a span we clear this flag to kill the signaling thread */
/* FIXME: what about the calls that are already up-and-running? */
typedef enum {
	FTDM_R2_RUNNING = (1 << 0),
} ftdm_r2_flag_t;

/* private call information stored in ftdmchan->call_data void* ptr */
#define R2CALL(ftdmchan) ((ftdm_r2_call_t*)((ftdmchan)->call_data))
typedef struct ftdm_r2_call_t {
    openr2_chan_t *r2chan;
	int accepted:1;
	int answer_pending:1;
	int state_ack_pending:1;
	int disconnect_rcvd:1;
	int ftdm_started:1;
	ftdm_channel_state_t chanstate;
	ftdm_size_t dnis_index;
	ftdm_size_t ani_index;
	char name[10];
} ftdm_r2_call_t;

/* this is just used as place holder in the stack when configuring the span to avoid using bunch of locals */
typedef struct ft_r2_conf_s {
	/* openr2 types */
	openr2_variant_t variant;
	openr2_calling_party_category_t category;
	openr2_log_level_t loglevel;

	/* strings */
	char *logdir;
	char *advanced_protocol_file;

	/* ints */
	int32_t max_ani;
	int32_t max_dnis;
	int32_t mfback_timeout; 
	int32_t metering_pulse_timeout;

	/* booleans */
	int immediate_accept;
	int skip_category;
	int get_ani_first;
	int call_files;
	int double_answer;
	int charge_calls;
	int forced_release;
	int allow_collect_calls;
} ft_r2_conf_t;

/* r2 configuration stored in span->signal_data */
typedef struct ftdm_r2_data_s {
	/* span flags */
	ftdm_r2_flag_t flags;
	/* openr2 handle for the R2 variant context */
	openr2_context_t *r2context;
	/* category to use when making calls */
	openr2_calling_party_category_t category;
	/* whether to use OR2_CALL_WITH_CHARGE or OR2_CALL_NO_CHARGE when accepting a call */
	int charge_calls:1;
	/* allow or reject collect calls */
	int allow_collect_calls:1;
	/* whether to use forced release when hanging up */
	int forced_release:1;
	/* whether accept the call when offered, or wait until the user decides to accept */
	int accept_on_offer:1;
} ftdm_r2_data_t;

/* one element per span will be stored in g_mod_data_hash global var to keep track of them
   and destroy them on module unload */
typedef struct ftdm_r2_span_pvt_s {
	openr2_context_t *r2context; /* r2 context allocated for this span */
	ftdm_hash_t *r2calls; /* hash table of allocated call data per channel for this span */
} ftdm_r2_span_pvt_t;

/* span monitor thread */
static void *ftdm_r2_run(ftdm_thread_t *me, void *obj);

/* channel monitor thread */
static void *ftdm_r2_channel_run(ftdm_thread_t *me, void *obj);

/* hash of all the private span allocations
   we need to keep track of them to destroy them when unloading the module 
   since freetdm does not notify signaling modules when destroying a span
   span -> ftdm_r2_mod_allocs_t */
static ftdm_hash_t *g_mod_data_hash;

/* IO interface for the command API */
static ftdm_io_interface_t g_ftdm_r2_interface;

static void ft_r2_clean_call(ftdm_r2_call_t *call)
{
    openr2_chan_t *r2chan = call->r2chan;
    memset(call, 0, sizeof(*call));
    call->r2chan = r2chan;
}

static void ft_r2_accept_call(ftdm_channel_t *ftdmchan)
{
	openr2_chan_t *r2chan = R2CALL(ftdmchan)->r2chan;
	// FIXME: not always accept as no charge, let the user decide that
	// also we should check the return code from openr2_chan_accept_call and handle error condition
	// hanging up the call with protocol error as the reason, this openr2 API will fail only when there something
	// wrong at the I/O layer or the library itself
	openr2_chan_accept_call(r2chan, OR2_CALL_NO_CHARGE);
	R2CALL(ftdmchan)->accepted = 1;
}

static void ft_r2_answer_call(ftdm_channel_t *ftdmchan)
{
	openr2_chan_t *r2chan = R2CALL(ftdmchan)->r2chan;
	// FIXME
	// 1. check openr2_chan_answer_call return code
	// 2. The openr2_chan_answer_call_with_mode should be used depending on user settings
	// openr2_chan_answer_call_with_mode(r2chan, OR2_ANSWER_SIMPLE);
	openr2_chan_answer_call(r2chan);
	R2CALL(ftdmchan)->answer_pending = 0;
}

static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(r2_outgoing_call)
{
	ftdm_status_t status;
	ftdm_mutex_lock(ftdmchan->mutex);

	/* the channel may be down but the thread not quite done */
	ftdm_wait_for_flag_cleared(ftdmchan, FTDM_CHANNEL_INTHREAD, 200);

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INTHREAD)) {
		ftdm_log(FTDM_LOG_ERROR, "%d:%d Yay! R2 outgoing call in channel that is already in thread.\n", 
				ftdmchan->span_id, ftdmchan->chan_id);
		ftdm_mutex_unlock(ftdmchan->mutex);
		return FTDM_FAIL;
	}

	ft_r2_clean_call(ftdmchan->call_data);
	R2CALL(ftdmchan)->chanstate = FTDM_CHANNEL_STATE_DOWN;
	ftdm_channel_set_state(ftdmchan, FTDM_CHANNEL_STATE_DIALING, 0);
	ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);
	R2CALL(ftdmchan)->ftdm_started = 1;
	ftdm_mutex_unlock(ftdmchan->mutex);

	status = ftdm_thread_create_detached(ftdm_r2_channel_run, ftdmchan);
	if (status == FTDM_FAIL) {
		ftdm_log(FTDM_LOG_ERROR, "%d:%d Cannot handle request to start call in channel, failed to create thread!\n", 
				ftdmchan->span_id, ftdmchan->chan_id);
		ftdm_channel_done(ftdmchan);
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_r2_start(ftdm_span_t *span)
{
	ftdm_r2_data_t *r2_data = span->signal_data;
	ftdm_set_flag(r2_data, FTDM_R2_RUNNING);
	return ftdm_thread_create_detached(ftdm_r2_run, span);
}

/* always called from the monitor thread */
static void ftdm_r2_on_call_init(openr2_chan_t *r2chan)
{
	ftdm_channel_t *ftdmchan = openr2_chan_get_client_data(r2chan);
	ftdm_status_t status;
	ftdm_log(FTDM_LOG_NOTICE, "Received request to start call on chan %d\n", openr2_chan_get_number(r2chan));

	ftdm_mutex_lock(ftdmchan->mutex);

	if (ftdmchan->state != FTDM_CHANNEL_STATE_DOWN) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot handle request to start call in channel %d, invalid state (%d)\n", 
				openr2_chan_get_number(r2chan), ftdmchan->state);
		ftdm_mutex_unlock(ftdmchan->mutex);
		return;
	}

	/* the channel may be down but the thread not quite done */
	ftdm_wait_for_flag_cleared(ftdmchan, FTDM_CHANNEL_INTHREAD, 200);

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INTHREAD)) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot handle request to start call in channel %d, already in thread!\n", 
				openr2_chan_get_number(r2chan));
		ftdm_mutex_unlock(ftdmchan->mutex);
		return;
	}
	ft_r2_clean_call(ftdmchan->call_data);
	R2CALL(ftdmchan)->chanstate = FTDM_CHANNEL_STATE_DOWN;
	ftdm_channel_set_state(ftdmchan, FTDM_CHANNEL_STATE_COLLECT, 0);
	ftdm_mutex_unlock(ftdmchan->mutex);

	status = ftdm_thread_create_detached(ftdm_r2_channel_run, ftdmchan);
	if (status == FTDM_FAIL) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot handle request to start call in channel %d, failed to create thread!\n", 
				openr2_chan_get_number(r2chan));
	}
}

/* only called for incoming calls when the ANI, DNIS etc is complete and the user has to decide either to accept or reject the call */
static void ftdm_r2_on_call_offered(openr2_chan_t *r2chan, const char *ani, const char *dnis, openr2_calling_party_category_t category)
{
	ftdm_sigmsg_t sigev;
	ftdm_channel_t *ftdmchan = openr2_chan_get_client_data(r2chan);

	ftdm_log(FTDM_LOG_NOTICE, "Call offered on chan %d, ANI = %s, DNIS = %s, Category = %s\n", openr2_chan_get_number(r2chan), 
			ani, dnis, openr2_proto_get_category_string(category));

	/* notify the user about the new call */
	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = ftdmchan->chan_id;
	sigev.span_id = ftdmchan->span_id;
	sigev.channel = ftdmchan;
	sigev.event_id = FTDM_SIGEVENT_START;

	if (ftdm_span_send_signal(ftdmchan->span, &sigev) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_NOTICE, "Failed to handle call offered on chan %d\n", openr2_chan_get_number(r2chan));
		openr2_chan_disconnect_call(r2chan, OR2_CAUSE_OUT_OF_ORDER);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);
		return; 
	}
	ftdm_channel_use(ftdmchan);
	R2CALL(ftdmchan)->ftdm_started = 1; 
}

static void ftdm_r2_on_call_accepted(openr2_chan_t *r2chan, openr2_call_mode_t mode)
{
	ftdm_channel_t *ftdmchan = openr2_chan_get_client_data(r2chan);
	ftdm_log(FTDM_LOG_NOTICE, "Call accepted on chan %d\n", openr2_chan_get_number(r2chan));
	/* at this point the MF signaling has ended and there is no point on keep reading */
	openr2_chan_disable_read(r2chan);
	if (OR2_DIR_BACKWARD == openr2_chan_get_direction(r2chan)) {
		R2CALL(ftdmchan)->state_ack_pending = 1;
		if (R2CALL(ftdmchan)->answer_pending) {
			ftdm_log(FTDM_LOG_DEBUG, "Answer was pending on chan %d, answering now.\n", openr2_chan_get_number(r2chan));
			ft_r2_answer_call(ftdmchan);
			return;
		}
	} else {
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
	}
}

static void ftdm_r2_on_call_answered(openr2_chan_t *r2chan)
{
	ftdm_channel_t *ftdmchan = openr2_chan_get_client_data(r2chan);
	ftdm_log(FTDM_LOG_NOTICE, "Call answered on chan %d\n", openr2_chan_get_number(r2chan));
	/* notify the upper layer of progress in the outbound call */
	if (OR2_DIR_FORWARD == openr2_chan_get_direction(r2chan)) {
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);
	}
}

/* may be called in the signaling or media thread depending on whether the hangup is product of MF or CAS signaling */
static void ftdm_r2_on_call_disconnect(openr2_chan_t *r2chan, openr2_call_disconnect_cause_t cause)
{
	ftdm_sigmsg_t sigev;
	ftdm_r2_data_t *r2data;
	ftdm_channel_t *ftdmchan = openr2_chan_get_client_data(r2chan);
	ftdm_log(FTDM_LOG_NOTICE, "Call disconnected on chan %d\n", openr2_chan_get_number(r2chan));

	ftdm_log(FTDM_LOG_DEBUG, "Got openr2 disconnection, clearing call on channel %d\n", ftdmchan->physical_chan_id);

	R2CALL(ftdmchan)->disconnect_rcvd = 1;

	/* acknowledge the hangup, cause will be ignored. From here to -> HANGUP once the freetdm side hangs up as well */
	openr2_chan_disconnect_call(r2chan, OR2_CAUSE_NORMAL_CLEARING);

	/* if the call has not been started yet we must go to HANGUP right here */ 
	if (!R2CALL(ftdmchan)->ftdm_started) {
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
		return;
	}

	/* FIXME: use the cause received from openr2 and map it to ftdm cause  */
	ftdmchan->caller_data.hangup_cause  = FTDM_CAUSE_NORMAL_CLEARING; 

	/* notify the user of the call terminating */
	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = ftdmchan->chan_id;
	sigev.span_id = ftdmchan->span_id;
	sigev.channel = ftdmchan;
	sigev.event_id = FTDM_SIGEVENT_STOP;
	r2data = ftdmchan->span->signal_data;

	ftdm_span_send_signal(ftdmchan->span, &sigev);
}

static void ftdm_r2_on_call_end(openr2_chan_t *r2chan)
{
	ftdm_channel_t *ftdmchan = openr2_chan_get_client_data(r2chan);
	ftdm_log(FTDM_LOG_NOTICE, "Call finished on chan %d\n", openr2_chan_get_number(r2chan));
	/* this means the freetdm side disconnected the call, therefore we must move to DOWN here */
	if (!R2CALL(ftdmchan)->disconnect_rcvd) {
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		return;
	}
}

static void ftdm_r2_on_call_read(openr2_chan_t *r2chan, const unsigned char *buf, int buflen)
{
	ftdm_log(FTDM_LOG_NOTICE, "Call read data on chan %d\n", openr2_chan_get_number(r2chan));
}

static void ftdm_r2_on_hardware_alarm(openr2_chan_t *r2chan, int alarm)
{
	ftdm_log(FTDM_LOG_NOTICE, "Alarm on chan %d (%d)\n", openr2_chan_get_number(r2chan), alarm);
}

static void ftdm_r2_on_os_error(openr2_chan_t *r2chan, int errorcode)
{
	ftdm_log(FTDM_LOG_ERROR, "OS error on chan %d: %s\n", openr2_chan_get_number(r2chan), strerror(errorcode));
}

static void ftdm_r2_on_protocol_error(openr2_chan_t *r2chan, openr2_protocol_error_t reason)
{
	ftdm_sigmsg_t sigev;
	ftdm_r2_data_t *r2data;
	ftdm_channel_t *ftdmchan = openr2_chan_get_client_data(r2chan);

	ftdm_log(FTDM_LOG_ERROR, "Protocol error on chan %d\n", openr2_chan_get_number(r2chan));

	R2CALL(ftdmchan)->disconnect_rcvd = 1;

	if (!R2CALL(ftdmchan)->ftdm_started) {
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
		return;
	}

	ftdmchan->caller_data.hangup_cause  = FTDM_CAUSE_PROTOCOL_ERROR; 

	/* notify the user of the call terminating */
	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = ftdmchan->chan_id;
	sigev.span_id = ftdmchan->span_id;
	sigev.channel = ftdmchan;
	sigev.event_id = FTDM_SIGEVENT_STOP;
	r2data = ftdmchan->span->signal_data;

	ftdm_span_send_signal(ftdmchan->span, &sigev);
}

static void ftdm_r2_on_line_blocked(openr2_chan_t *r2chan)
{
	ftdm_log(FTDM_LOG_NOTICE, "Far end blocked on chan %d\n", openr2_chan_get_number(r2chan));
}

static void ftdm_r2_on_line_idle(openr2_chan_t *r2chan)
{
	ftdm_log(FTDM_LOG_NOTICE, "Far end unblocked on chan %d\n", openr2_chan_get_number(r2chan));
}

static void ftdm_r2_write_log(openr2_log_level_t level, const char *message)
{
	switch (level) {
		case OR2_LOG_NOTICE:
			ftdm_log(FTDM_LOG_NOTICE, "%s", message);
			break;
		case OR2_LOG_WARNING:
			ftdm_log(FTDM_LOG_WARNING, "%s", message);
			break;
		case OR2_LOG_ERROR:
			ftdm_log(FTDM_LOG_ERROR, "%s", message);
			break;
		case OR2_LOG_STACK_TRACE:
		case OR2_LOG_MF_TRACE:
		case OR2_LOG_CAS_TRACE:
		case OR2_LOG_DEBUG:
		case OR2_LOG_EX_DEBUG:
			ftdm_log(FTDM_LOG_DEBUG, "%s", message);
			break;
		default:
			ftdm_log(FTDM_LOG_WARNING, "We should handle logging level %d here.\n", level);
			ftdm_log(FTDM_LOG_DEBUG, "%s", message);
			break;
	}
}

static void ftdm_r2_on_context_log(openr2_context_t *r2context, openr2_log_level_t level, const char *fmt, va_list ap)
{
#define CONTEXT_TAG "Context -"
	char logmsg[256];
	char completemsg[sizeof(logmsg) + sizeof(CONTEXT_TAG) - 1];
	vsnprintf(logmsg, sizeof(logmsg), fmt, ap);
	snprintf(completemsg, sizeof(completemsg), CONTEXT_TAG "%s", logmsg);
	ftdm_r2_write_log(level, completemsg);
#undef CONTEXT_TAG
}

static void ftdm_r2_on_chan_log(openr2_chan_t *r2chan, openr2_log_level_t level, const char *fmt, va_list ap)
{
#define CHAN_TAG "Chan "
	char logmsg[256];
	char completemsg[sizeof(logmsg) + sizeof(CHAN_TAG) - 1];
	vsnprintf(logmsg, sizeof(logmsg), fmt, ap);
	snprintf(completemsg, sizeof(completemsg), CHAN_TAG "%d: %s", openr2_chan_get_number(r2chan), logmsg);
	ftdm_r2_write_log(level, completemsg);
#undef CHAN_TAG
}

static int ftdm_r2_on_dnis_digit_received(openr2_chan_t *r2chan, char digit)
{
	ftdm_sigmsg_t sigev;
	ftdm_r2_data_t *r2data;
	ftdm_channel_t *ftdmchan = openr2_chan_get_client_data(r2chan);
	ftdm_size_t collected_len = R2CALL(ftdmchan)->dnis_index;

	ftdm_log(FTDM_LOG_DEBUG, "DNIS digit %d received chan %d\n", digit, openr2_chan_get_number(r2chan));

	/* save the digit we just received */ 
	ftdmchan->caller_data.dnis.digits[collected_len] = digit;
	collected_len++;
	ftdmchan->caller_data.dnis.digits[collected_len] = '\0';
	R2CALL(ftdmchan)->dnis_index = collected_len;

	/* notify the user about the new digit and check if we should stop requesting more DNIS */
	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = ftdmchan->chan_id;
	sigev.span_id = ftdmchan->span_id;
	sigev.channel = ftdmchan;
	sigev.event_id = FTDM_SIGEVENT_COLLECTED_DIGIT;
	r2data = ftdmchan->span->signal_data;
	if (ftdm_span_send_signal(ftdmchan->span, &sigev) == FTDM_BREAK) {
		ftdm_log(FTDM_LOG_NOTICE, "Requested to stop getting DNIS. Current DNIS = %s on chan %d\n", ftdmchan->caller_data.dnis.digits, openr2_chan_get_number(r2chan));
		return OR2_STOP_DNIS_REQUEST; 
	}

	/* the only other reason to stop requesting DNIS is that there is no more room to save it */
	if (collected_len == (sizeof(ftdmchan->caller_data.dnis.digits) - 1)) {
		ftdm_log(FTDM_LOG_NOTICE, "No more room for DNIS. Current DNIS = %s on chan %d\n", ftdmchan->caller_data.dnis.digits, openr2_chan_get_number(r2chan));
		return OR2_STOP_DNIS_REQUEST;
	}

	return OR2_CONTINUE_DNIS_REQUEST; 
}

static void ftdm_r2_on_ani_digit_received(openr2_chan_t *r2chan, char digit)
{
	ftdm_channel_t *ftdmchan = openr2_chan_get_client_data(r2chan);
	ftdm_size_t collected_len = R2CALL(ftdmchan)->ani_index;

	/* check if we should drop ANI */
	if (collected_len == (sizeof(ftdmchan->caller_data.ani.digits) - 1)) {
		ftdm_log(FTDM_LOG_NOTICE, "No more room for ANI %c on chan %d, digit dropped.\n", digit, openr2_chan_get_number(r2chan));
		return;
	}
	ftdm_log(FTDM_LOG_DEBUG, "ANI digit %c received chan %d\n", digit, openr2_chan_get_number(r2chan));

	/* save the digit we just received */ 
	ftdmchan->caller_data.ani.digits[collected_len++] = digit;
	ftdmchan->caller_data.ani.digits[collected_len] = '\0';
}

static openr2_event_interface_t ftdm_r2_event_iface = {
	.on_call_init = ftdm_r2_on_call_init,
	.on_call_offered = ftdm_r2_on_call_offered,
	.on_call_accepted = ftdm_r2_on_call_accepted,
	.on_call_answered = ftdm_r2_on_call_answered,
	.on_call_disconnect = ftdm_r2_on_call_disconnect,
	.on_call_end = ftdm_r2_on_call_end,
	.on_call_read = ftdm_r2_on_call_read,
	.on_hardware_alarm = ftdm_r2_on_hardware_alarm,
	.on_os_error = ftdm_r2_on_os_error,
	.on_protocol_error = ftdm_r2_on_protocol_error,
	.on_line_blocked = ftdm_r2_on_line_blocked,
	.on_line_idle = ftdm_r2_on_line_idle,
	/* cast seems to be needed to get rid of the annoying warning regarding format attribute  */
	.on_context_log = (openr2_handle_context_logging_func)ftdm_r2_on_context_log,
	.on_dnis_digit_received = ftdm_r2_on_dnis_digit_received,
	.on_ani_digit_received = ftdm_r2_on_ani_digit_received,
	/* so far we do nothing with billing pulses */
	.on_billing_pulse_received = NULL
};

static int ftdm_r2_io_set_cas(openr2_chan_t *r2chan, int cas)
{
	ftdm_channel_t *ftdm_chan = openr2_chan_get_fd(r2chan);
	ftdm_status_t status = ftdm_channel_command(ftdm_chan, FTDM_COMMAND_SET_CAS_BITS, &cas);
	if (FTDM_FAIL == status) {
		return -1;
	}
	return 0;
}

static int ftdm_r2_io_get_cas(openr2_chan_t *r2chan, int *cas)
{
	ftdm_channel_t *ftdm_chan = openr2_chan_get_fd(r2chan);
	ftdm_status_t status = ftdm_channel_command(ftdm_chan, FTDM_COMMAND_GET_CAS_BITS, cas);
	if (FTDM_FAIL == status) {
		return -1;
	}
	return 0;
}

static int ftdm_r2_io_flush_write_buffers(openr2_chan_t *r2chan)
{
	ftdm_channel_t *ftdm_chan = openr2_chan_get_fd(r2chan);
	ftdm_status_t status = ftdm_channel_command(ftdm_chan, FTDM_COMMAND_FLUSH_TX_BUFFERS, NULL);
	if (FTDM_FAIL == status) {
		return -1;
	}
	return 0;
}

static int ftdm_r2_io_write(openr2_chan_t *r2chan, const void *buf, int size)
{
	ftdm_channel_t *ftdm_chan = openr2_chan_get_fd(r2chan);
	ftdm_size_t outsize = size;
	ftdm_status_t status = ftdm_channel_write(ftdm_chan, (void *)buf, size, &outsize);
	if (FTDM_FAIL == status) {
		return -1;
	}
	return outsize;
}

static int ftdm_r2_io_read(openr2_chan_t *r2chan, const void *buf, int size)
{
	ftdm_channel_t *ftdm_chan = openr2_chan_get_fd(r2chan);
	ftdm_size_t outsize = size;
	ftdm_status_t status = ftdm_channel_read(ftdm_chan, (void *)buf, &outsize);
	if (FTDM_FAIL == status) {
		return -1;
	}
	return outsize;
}

static int ftdm_r2_io_wait(openr2_chan_t *r2chan, int *flags, int block)
{
	ftdm_status_t status;
	ftdm_wait_flag_t ftdmflags = 0;

	ftdm_channel_t *ftdm_chan = openr2_chan_get_fd(r2chan);
	int32_t timeout = block ? -1 : 0;

	if (*flags & OR2_IO_READ) {
		ftdmflags |= FTDM_READ;
	}
	if (*flags & OR2_IO_WRITE) {
		ftdmflags |= FTDM_WRITE;
	}
	if (*flags & OR2_IO_OOB_EVENT) {
		ftdmflags |= FTDM_EVENTS;
	}

	status = ftdm_channel_wait(ftdm_chan, &ftdmflags, timeout);

	if (FTDM_SUCCESS != status) {
		return -1;
	}

	*flags = 0;
	if (ftdmflags & FTDM_READ) {
		*flags |= OR2_IO_READ;
	}
	if (ftdmflags & FTDM_WRITE) {
		*flags |= OR2_IO_WRITE;
	}
	if (ftdmflags & FTDM_EVENTS) {
		*flags |= OR2_IO_OOB_EVENT;
	}

	return 0;
}

/* The following openr2 hooks never get called, read on for reasoning ... */
/* since freetdm takes care of opening the file descriptor and using openr2_chan_new_from_fd, openr2 should never call this hook */
static openr2_io_fd_t ftdm_r2_io_open(openr2_context_t *r2context, int channo)
{
	ftdm_log(FTDM_LOG_ERROR, "I should not be called (I/O open)!!\n");
	return NULL;
}

/* since freetdm takes care of closing the file descriptor and uses openr2_chan_new_from_fd, openr2 should never call this hook */
static int ftdm_r2_io_close(openr2_chan_t *r2chan)
{
	ftdm_log(FTDM_LOG_ERROR, "I should not be called (I/O close)!!\n");
	return 0;
}

/* since freetdm takes care of opening the file descriptor and using openr2_chan_new_from_fd, openr2 should never call this hook */
static int ftdm_r2_io_setup(openr2_chan_t *r2chan)
{
	ftdm_log(FTDM_LOG_ERROR, "I should not be called (I/O Setup)!!\n");
	return 0;
}

/* since the signaling thread calls openr2_chan_process_cas_signaling directly, openr2 should never call this hook */
static int ftdm_r2_io_get_oob_event(openr2_chan_t *r2chan, openr2_oob_event_t *event)
{
	*event = 0;
	ftdm_log(FTDM_LOG_ERROR, "I should not be called (I/O get oob event)!!\n");
	return 0;
}

static openr2_io_interface_t ftdm_r2_io_iface = {
	.open = ftdm_r2_io_open, /* never called */
	.close = ftdm_r2_io_close, /* never called */
	.set_cas = ftdm_r2_io_set_cas,
	.get_cas = ftdm_r2_io_get_cas,
	.flush_write_buffers = ftdm_r2_io_flush_write_buffers,
	.write = ftdm_r2_io_write,
	.read = ftdm_r2_io_read,
	.setup = ftdm_r2_io_setup, /* never called */
	.wait = ftdm_r2_io_wait,
	.get_oob_event = ftdm_r2_io_get_oob_event /* never called */
};

static FIO_SIG_CONFIGURE_FUNCTION(ftdm_r2_configure_span)
	//ftdm_status_t (ftdm_span_t *span, fio_signal_cb_t sig_cb, va_list ap)
{
	int i = 0;
	int conf_failure = 0;
	char *var = NULL;
	char *val = NULL;
	ftdm_r2_data_t *r2data = NULL;
	ftdm_r2_span_pvt_t *spanpvt = NULL;
	ftdm_r2_call_t *r2call = NULL;
	openr2_chan_t *r2chan = NULL;

	assert(sig_cb != NULL);

	ft_r2_conf_t r2conf = 
	{
		.variant = OR2_VAR_ITU,
		.category = OR2_CALLING_PARTY_CATEGORY_NATIONAL_SUBSCRIBER,
		.loglevel = OR2_LOG_ERROR | OR2_LOG_WARNING,
		.max_ani = 10,
		.max_dnis = 4,
		.mfback_timeout = -1,
		.metering_pulse_timeout = -1,
		.allow_collect_calls = -1,
		.immediate_accept = -1,
		.skip_category = -1,
		.forced_release = -1,
		.charge_calls = -1,
		.get_ani_first = -1,
		.call_files = -1,
		.logdir = NULL,
		.advanced_protocol_file = NULL 
	};


	if (span->signal_type) {
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling.");
		return FTDM_FAIL;
	}

	while ((var = va_arg(ap, char *))) {
		ftdm_log(FTDM_LOG_DEBUG, "Reading R2 parameter %s for span %d\n", var, span->span_id);
		if (!strcasecmp(var, "variant")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (ftdm_strlen_zero_buf(val)) {
				ftdm_log(FTDM_LOG_NOTICE, "Ignoring empty R2 variant parameter\n");
				continue;
			}
			r2conf.variant = openr2_proto_get_variant(val);
			if (r2conf.variant == OR2_VAR_UNKNOWN) {
				ftdm_log(FTDM_LOG_ERROR, "Unknown R2 variant %s\n", val);
				conf_failure = 1;
				break;
			}
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d for variant %s\n", span->span_id, val);
		} else if (!strcasecmp(var, "category")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (ftdm_strlen_zero_buf(val)) {
				ftdm_log(FTDM_LOG_NOTICE, "Ignoring empty R2 category parameter\n");
				continue;
			}
			r2conf.category = openr2_proto_get_category(val);
			if (r2conf.category == OR2_CALLING_PARTY_CATEGORY_UNKNOWN) {
				ftdm_log(FTDM_LOG_ERROR, "Unknown R2 caller category %s\n", val);
				conf_failure = 1;
				break;
			}
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with default category %s\n", span->span_id, val);
		} else if (!strcasecmp(var, "logdir")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (ftdm_strlen_zero_buf(val)) {
				ftdm_log(FTDM_LOG_NOTICE, "Ignoring empty R2 logdir parameter\n");
				continue;
			}
			r2conf.logdir = val;
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with logdir %s\n", span->span_id, val);
		} else if (!strcasecmp(var, "logging")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (ftdm_strlen_zero_buf(val)) {
				ftdm_log(FTDM_LOG_NOTICE, "Ignoring empty R2 logging parameter\n");
				continue;
			}
			openr2_log_level_t tmplevel;
			char *clevel;
			char *logval = ftdm_malloc(strlen(val)+1); /* alloca man page scared me, so better to use good ol' malloc  */
			if (!logval) {
				ftdm_log(FTDM_LOG_WARNING, "Ignoring R2 logging parameter: '%s', failed to alloc memory\n", val);
				continue;
			}
			strcpy(logval, val);
			while (logval) {
				clevel = strsep(&logval, ",");
				if (-1 == (tmplevel = openr2_log_get_level(clevel))) {
					ftdm_log(FTDM_LOG_WARNING, "Ignoring invalid R2 logging level: '%s'\n", clevel);
					continue;
				}
				r2conf.loglevel |= tmplevel;
				ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with loglevel %s\n", span->span_id, clevel);
			}
			ftdm_safe_free(logval);
		} else if (!strcasecmp(var, "advanced_protocol_file")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (ftdm_strlen_zero_buf(val)) {
				ftdm_log(FTDM_LOG_NOTICE, "Ignoring empty R2 advanced_protocol_file parameter\n");
				continue;
			}
			r2conf.advanced_protocol_file = val;
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with advanced protocol file %s\n", span->span_id, val);
		} else if (!strcasecmp(var, "allow_collect_calls")) {
			r2conf.allow_collect_calls = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with allow collect calls max ani = %d\n", span->span_id, r2conf.allow_collect_calls);
		} else if (!strcasecmp(var, "double_answer")) {
			r2conf.double_answer = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with double answer = %d\n", span->span_id, r2conf.double_answer);
		} else if (!strcasecmp(var, "immediate_accept")) {
			r2conf.immediate_accept = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with immediate accept = %d\n", span->span_id, r2conf.immediate_accept);
		} else if (!strcasecmp(var, "skip_category")) {
			r2conf.skip_category = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with skip category = %d\n", span->span_id, r2conf.skip_category);
		} else if (!strcasecmp(var, "forced_release")) {
			r2conf.forced_release = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with forced release = %d\n", span->span_id, r2conf.forced_release);
		} else if (!strcasecmp(var, "charge_calls")) {
			r2conf.charge_calls = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with charge calls = %d\n", span->span_id, r2conf.charge_calls);
		} else if (!strcasecmp(var, "get_ani_first")) {
			r2conf.get_ani_first = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with get ani first = %d\n", span->span_id, r2conf.get_ani_first);
		} else if (!strcasecmp(var, "call_files")) {
			r2conf.call_files = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with call files = %d\n", span->span_id, r2conf.call_files);
		} else if (!strcasecmp(var, "mfback_timeout")) {
			r2conf.mfback_timeout = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with MF backward timeout = %dms\n", span->span_id, r2conf.mfback_timeout);
		} else if (!strcasecmp(var, "metering_pulse_timeout")) {
			r2conf.metering_pulse_timeout = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with metering pulse timeout = %dms\n", span->span_id, r2conf.metering_pulse_timeout);
		} else if (!strcasecmp(var, "max_ani")) {
			r2conf.max_ani = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with max ani = %d\n", span->span_id, r2conf.max_ani);
		} else if (!strcasecmp(var, "max_dnis")) {
			r2conf.max_dnis = va_arg(ap, int);
			ftdm_log(FTDM_LOG_DEBUG, "Configuring R2 span %d with max dnis = %d\n", span->span_id, r2conf.max_dnis);
		} else {
			snprintf(span->last_error, sizeof(span->last_error), "Unknown R2 parameter [%s]", var);
			return FTDM_FAIL;
		}
	}

	if (conf_failure) {
		snprintf(span->last_error, sizeof(span->last_error), "R2 configuration error");
		return FTDM_FAIL;
	}

	r2data = ftdm_malloc(sizeof(*r2data));
	if (!r2data) {
		snprintf(span->last_error, sizeof(span->last_error), "Failed to allocate R2 data.");
		return FTDM_FAIL;
	}
	memset(r2data, 0, sizeof(*r2data));

	spanpvt = ftdm_malloc(sizeof(*spanpvt));
	if (!spanpvt) {
		snprintf(span->last_error, sizeof(span->last_error), "Failed to allocate private span data container.");
		goto fail;
	}
	memset(spanpvt, 0, sizeof(*spanpvt));

	r2data->r2context = openr2_context_new(r2conf.variant, &ftdm_r2_event_iface, r2conf.max_ani, r2conf.max_dnis);
	if (!r2data->r2context) {
		snprintf(span->last_error, sizeof(span->last_error), "Cannot create openr2 context for span.");
		goto fail;
	}
	openr2_context_set_io_type(r2data->r2context, OR2_IO_CUSTOM, &ftdm_r2_io_iface);
	openr2_context_set_log_level(r2data->r2context, r2conf.loglevel);
	openr2_context_set_ani_first(r2data->r2context, r2conf.get_ani_first);
	openr2_context_set_skip_category_request(r2data->r2context, r2conf.skip_category);
	openr2_context_set_mf_back_timeout(r2data->r2context, r2conf.mfback_timeout);
	openr2_context_set_metering_pulse_timeout(r2data->r2context, r2conf.metering_pulse_timeout);
	openr2_context_set_double_answer(r2data->r2context, r2conf.double_answer);
	openr2_context_set_immediate_accept(r2data->r2context, r2conf.immediate_accept);
	if (r2conf.logdir) {
		openr2_context_set_log_directory(r2data->r2context, r2conf.logdir);
	}
	if (r2conf.advanced_protocol_file) {
		openr2_context_configure_from_advanced_file(r2data->r2context, r2conf.advanced_protocol_file);
	}

	spanpvt->r2calls = create_hashtable(FTDM_MAX_CHANNELS_SPAN, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	if (!spanpvt->r2calls) {
		snprintf(span->last_error, sizeof(span->last_error), "Cannot create channel calls hash for span.");
		goto fail;
	}

	for (i = 1; (i <= span->chan_count) && (i <= FTDM_MAX_CHANNELS_SPAN); i++) {
		r2chan = openr2_chan_new_from_fd(r2data->r2context, span->channels[i], span->channels[i]->physical_chan_id);
		if (!r2chan) {
			snprintf(span->last_error, sizeof(span->last_error), "Cannot create all openr2 channels for span.");
			goto fail;
		}
		if (r2conf.call_files) {
			openr2_chan_enable_call_files(r2chan);
			openr2_chan_set_log_level(r2chan, r2conf.loglevel);
		}

		r2call = ftdm_malloc(sizeof(*r2call));
		if (!r2call) {
			snprintf(span->last_error, sizeof(span->last_error), "Cannot create all R2 call data structures for the span.");
			ftdm_safe_free(r2chan);
			goto fail;
		}
		memset(r2call, 0, sizeof(*r2call));
		openr2_chan_set_logging_func(r2chan, ftdm_r2_on_chan_log);
		openr2_chan_set_client_data(r2chan, span->channels[i]);
        r2call->r2chan = r2chan;
		span->channels[i]->call_data = r2call;
		/* value and key are the same so just free one of them */
		snprintf(r2call->name, sizeof(r2call->name), "chancall%d", i);
		hashtable_insert(spanpvt->r2calls, (void *)r2call->name, r2call, HASHTABLE_FLAG_FREE_VALUE);

	}
	spanpvt->r2context = r2data->r2context;

	/* just the value must be freed by the hash */
	hashtable_insert(g_mod_data_hash, (void *)span->name, spanpvt, HASHTABLE_FLAG_FREE_VALUE);

	span->start = ftdm_r2_start;
	r2data->flags = 0;
	span->signal_cb = sig_cb;
	span->signal_type = FTDM_SIGTYPE_R2;
	span->signal_data = r2data;
	span->outgoing_call = r2_outgoing_call;

	return FTDM_SUCCESS;

fail:

	if (r2data && r2data->r2context) {
		openr2_context_delete(r2data->r2context);
	}
	if (spanpvt && spanpvt->r2calls) {
		hashtable_destroy(spanpvt->r2calls);
	}
	ftdm_safe_free(r2data);
	ftdm_safe_free(spanpvt);
	return FTDM_FAIL;

}

static void *ftdm_r2_channel_run(ftdm_thread_t *me, void *obj)
{
	ftdm_channel_t *closed_chan;
	uint32_t interval = 0;
	ftdm_sigmsg_t sigev;
	ftdm_channel_t *ftdmchan = (ftdm_channel_t *)obj;
	openr2_chan_t *r2chan = R2CALL(ftdmchan)->r2chan;

	ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_INTHREAD);

	ftdm_mutex_lock(g_thread_count_mutex);
	g_thread_count++;
	ftdm_mutex_unlock(g_thread_count_mutex);

	ftdm_log(FTDM_LOG_DEBUG, "R2 CHANNEL thread starting on %d in state %s.\n", 
			ftdmchan->physical_chan_id,
			ftdm_channel_state2str(ftdmchan->state));

	if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "OPEN ERROR [%s]\n", ftdmchan->last_error);
		goto endthread;
	}

	ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_INTERVAL, &interval);

	assert(interval != 0);
	ftdm_log(FTDM_LOG_DEBUG, "Got %d interval for chan %d\n", interval, ftdmchan->physical_chan_id);

	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
		/* FIXME: is this needed? */
		memset(ftdmchan->caller_data.dnis.digits, 0, sizeof(ftdmchan->caller_data.collected));
		memset(ftdmchan->caller_data.ani.digits, 0, sizeof(ftdmchan->caller_data.collected));
	}

	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = ftdmchan->chan_id;
	sigev.span_id = ftdmchan->span_id;
	sigev.channel = ftdmchan;

	while (ftdm_running()) {
		int32_t read_enabled = openr2_chan_get_read_enabled(r2chan);
		ftdm_wait_flag_t flags = read_enabled ? ( FTDM_READ | FTDM_WRITE ) : 0;

		if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE) && (R2CALL(ftdmchan)->chanstate != ftdmchan->state)) {

			ftdm_log(FTDM_LOG_DEBUG, "Executing state handler on %d:%d for %s\n", ftdmchan->span_id, ftdmchan->chan_id, ftdm_channel_state2str(ftdmchan->state));
			R2CALL(ftdmchan)->chanstate = ftdmchan->state;

			if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND) && !R2CALL(ftdmchan)->accepted &&
					(ftdmchan->state == FTDM_CHANNEL_STATE_PROGRESS ||
					 ftdmchan->state == FTDM_CHANNEL_STATE_PROGRESS_MEDIA ||
					 ftdmchan->state == FTDM_CHANNEL_STATE_UP) ) {
				/* if an accept ack will be required we should not acknowledge the state change just yet, 
				   it will be done below after processing the MF signals, otherwise we have a race condition between freetdm calling
				   openr2_chan_answer_call and openr2 accepting the call first, if freetdm calls openr2_chan_answer_call before the accept cycle
				   completes, openr2 will fail to answer the call */
				ftdm_log(FTDM_LOG_DEBUG, "State ack in chan %d:%d for state %s will have to wait a bit\n", ftdmchan->span_id, ftdmchan->chan_id, ftdm_channel_state2str(ftdmchan->state));
			} else if (ftdmchan->state != FTDM_CHANNEL_STATE_DOWN){
				/* the down state will be completed in ftdm_channel_done below */
				ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);
				ftdm_channel_complete_state(ftdmchan);
			}

			switch (ftdmchan->state) {

				/* starting an incoming call */
				case FTDM_CHANNEL_STATE_COLLECT: 
					{
						ftdm_log(FTDM_LOG_DEBUG, "COLLECT: Starting processing of incoming call in channel %d with interval %d\n", ftdmchan->physical_chan_id, interval);
					}
					break;

					/* starting an outgoing call */
				case FTDM_CHANNEL_STATE_DIALING:
					{
						// FIXME: use user defined calling party
						ftdm_channel_use(ftdmchan);
						ftdm_log(FTDM_LOG_DEBUG, "DIALING: Starting processing of outgoing call in channel %d with interval %d\n", ftdmchan->physical_chan_id, interval);
						if (openr2_chan_make_call(r2chan, ftdmchan->caller_data.cid_num.digits, ftdmchan->caller_data.dnis.digits, OR2_CALLING_PARTY_CATEGORY_NATIONAL_SUBSCRIBER)) {
							ftdm_log(FTDM_LOG_ERROR, "%d:%d Failed to make call in R2 channel, openr2_chan_make_call failed\n", ftdmchan->span_id, ftdmchan->chan_id);
							ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
						}
					}
					break;

					/* the call is ringing */
				case FTDM_CHANNEL_STATE_PROGRESS:
				case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
					{
						if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
							if (!R2CALL(ftdmchan)->accepted) {
								ftdm_log(FTDM_LOG_DEBUG, "PROGRESS: Accepting call on channel %d\n", ftdmchan->physical_chan_id);
								ft_r2_accept_call(ftdmchan);
							} 
						} else {
							ftdm_log(FTDM_LOG_DEBUG, "PROGRESS: Notifying progress in channel %d\n", ftdmchan->physical_chan_id);
							sigev.event_id = FTDM_SIGEVENT_PROGRESS;
							if (ftdm_span_send_signal(ftdmchan->span, &sigev) != FTDM_SUCCESS) {
								ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
							}
						}
					}
					break;

					/* the call was answered */
				case FTDM_CHANNEL_STATE_UP:
					{
						ftdm_log(FTDM_LOG_DEBUG, "UP: Call was answered on channel %d\n", ftdmchan->physical_chan_id);
						if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
							if (!R2CALL(ftdmchan)->accepted) {
								ftdm_log(FTDM_LOG_DEBUG, "UP: Call has not been accepted, need to accept first\n");
								// the answering will be done in the on_call_accepted handler
								ft_r2_accept_call(ftdmchan);
								R2CALL(ftdmchan)->answer_pending = 1;
							} else {
								ft_r2_answer_call(ftdmchan);
							}
						} else {
							ftdm_log(FTDM_LOG_DEBUG, "UP: Notifying of call answered in channel %d\n", ftdmchan->physical_chan_id);
							sigev.event_id = FTDM_SIGEVENT_UP;
							if (ftdm_span_send_signal(ftdmchan->span, &sigev) != FTDM_SUCCESS) {
								ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
							}
						}
					}
					break;

					/* just got hangup */
				case FTDM_CHANNEL_STATE_HANGUP: 
					{
						/* FIXME: the cause should be retrieved from ftdmchan->caller_data.hangup_cause and translated from Q931 to R2 cause */
						ftdm_log(FTDM_LOG_DEBUG, "HANGUP: Clearing call on channel %d\n", ftdmchan->physical_chan_id);
						if (!R2CALL(ftdmchan)->disconnect_rcvd) {
							/* this will disconnect the call, but need to wait for the call end before moving to DOWN */
							openr2_chan_disconnect_call(r2chan, OR2_CAUSE_NORMAL_CLEARING);
						} else {
							/* at this point on_call_end possibly was already called, 
							 * but we needed to wait for the freetdm confirmation before moving to DOWN */
							ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
						}
					}
					break;

					/* just got hangup from the freetdm side due to abnormal failure */
				case FTDM_CHANNEL_STATE_CANCEL:
					{
						ftdm_log(FTDM_LOG_DEBUG, "CANCEL: Unable to receive call on channel %d\n", ftdmchan->physical_chan_id);
						openr2_chan_disconnect_call(r2chan, OR2_CAUSE_OUT_OF_ORDER);
					}
					break;

					/* finished call for good */
				case FTDM_CHANNEL_STATE_DOWN: 
					{
						ftdm_log(FTDM_LOG_DEBUG, "DOWN: Placing channel %d back to the pool of available channels\n", ftdmchan->physical_chan_id);
						ftdm_channel_done(ftdmchan);
						goto endthread;
					}
					break;

				default:
					{
						ftdm_log(FTDM_LOG_ERROR, "%s: Unhandled channel state change in channel %d\n", ftdm_channel_state2str(ftdmchan->state), ftdmchan->physical_chan_id);
					}
					break;

			}
		}

		if (flags) {
			if (ftdm_channel_wait(ftdmchan, &flags, interval * 2) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_DEBUG, "ftdm_channel_wait did not return FTDM_SUCCESS\n");
				continue;
			}

			/* handle timeout events first if any */
			openr2_chan_run_schedule(r2chan);

			/* openr2 will now try to detect MF tones, make sense out of them, reply if necessary with another tone and trigger 
			 * telephony events via the call event interface we provided when creating the R2 context.
			 * openr2 will also call our I/O callbacks to retrieve audio from the channel and call our wait poll I/O registered callback
			 * and will not return from this function until the I/O poll callback returns no pending events
			 * */
			openr2_chan_process_mf_signaling(r2chan);
			if (R2CALL(ftdmchan)->state_ack_pending) {
				ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);
				ftdm_channel_complete_state(ftdmchan);
				R2CALL(ftdmchan)->state_ack_pending = 0;
			}
		} else {
			/* once the MF signaling has end we just loop here waiting for state changes */
			ftdm_sleep(interval);
		}

	}

endthread:

	closed_chan = ftdmchan;
	ftdm_channel_close(&closed_chan);
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_INTHREAD);
	ftdm_log(FTDM_LOG_DEBUG, "R2 channel %d thread ended.\n", ftdmchan->physical_chan_id);

	ftdm_mutex_lock(g_thread_count_mutex);
	g_thread_count--;
	ftdm_mutex_unlock(g_thread_count_mutex);

	return NULL;
}

static void *ftdm_r2_run(ftdm_thread_t *me, void *obj)
{
	openr2_chan_t *r2chan;
	ftdm_status_t status;
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_r2_data_t *r2data = span->signal_data;
	int waitms = 1000;
	int i;

	ftdm_log(FTDM_LOG_DEBUG, "OpenR2 monitor thread started.\n");
  r2chan = NULL;
	for (i = 1; i <= span->chan_count; i++) {
		r2chan = R2CALL(span->channels[i])->r2chan;
		openr2_chan_set_idle(r2chan);
		openr2_chan_process_cas_signaling(r2chan);
	}

	while (ftdm_running() && ftdm_test_flag(r2data, FTDM_R2_RUNNING)) {
		status = ftdm_span_poll_event(span, waitms);
		if (FTDM_FAIL == status) {
			ftdm_log(FTDM_LOG_ERROR, "Failure Polling event! [%s]\n", span->last_error);
			continue;
		}
		if (FTDM_SUCCESS == status) {
			ftdm_event_t *event;
			while (ftdm_span_next_event(span, &event) == FTDM_SUCCESS) {
				if (event->enum_id == FTDM_OOB_CAS_BITS_CHANGE) {
                    r2chan = R2CALL(event->channel)->r2chan;
					ftdm_log(FTDM_LOG_DEBUG, "Handling CAS on channel %d.\n", openr2_chan_get_number(r2chan));
					// we only expect CAS and other OOB events on this thread/loop, once a call is started
					// the MF events (in-band signaling) are handled in the call thread
					openr2_chan_process_cas_signaling(r2chan);
				} else {
					ftdm_log(FTDM_LOG_DEBUG, "Ignoring event %d on channel %d.\n", event->enum_id, openr2_chan_get_number(r2chan));
					// XXX TODO: handle alarms here XXX
				}
			}
		} else if (status != FTDM_TIMEOUT) {
			ftdm_log(FTDM_LOG_ERROR, "ftdm_span_poll_event returned %d.\n", status);
		} else {
			//ftdm_log(FTDM_LOG_DEBUG, "timed out waiting for event on span %d\n", span->span_id);
		}
	}

    /*
    FIXME: we should set BLOCKED but at this point I/O routines of freetdm caused segfault
	for (i = 1; i <= span->chan_count; i++) {
		r2chan = R2CALL(span->channels[i])->r2chan;
		openr2_chan_set_blocked(r2chan);
	}
    */

	ftdm_clear_flag(r2data, FTDM_R2_RUNNING);
	ftdm_log(FTDM_LOG_DEBUG, "R2 thread ending.\n");

	return NULL;

}

static FIO_API_FUNCTION(ftdm_r2_api)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;

	if (data) {
		mycmd = ftdm_strdup(data);
		argc = ftdm_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc == 2) {
		if (!strcasecmp(argv[0], "kill")) {
			int span_id = atoi(argv[1]);
			ftdm_span_t *span = NULL;

			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS || ftdm_span_find(span_id, &span) == FTDM_SUCCESS) {
				ftdm_r2_data_t *r2data = span->signal_data;

				if (span->start != ftdm_r2_start) {
					stream->write_function(stream, "-ERR invalid span.\n");
					goto done;
				}

				ftdm_clear_flag(r2data, FTDM_R2_RUNNING);
				stream->write_function(stream, "+OK killed.\n");
				goto done;
			} else {
				stream->write_function(stream, "-ERR invalid span.\n");
				goto done;
			}
		}

		if (!strcasecmp(argv[0], "status")) {
			int span_id = atoi(argv[1]);
			ftdm_r2_data_t *r2data = NULL;
			ftdm_span_t *span = NULL;
			openr2_chan_t *r2chan = NULL;
			openr2_context_t *r2context = NULL;
			int i = 0;

			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS || ftdm_span_find(span_id, &span) == FTDM_SUCCESS) {
				if (span->start != ftdm_r2_start) {
					stream->write_function(stream, "-ERR not an R2 span.\n");
					goto done;
				}
				if (!(r2data =  span->signal_data)) {
					stream->write_function(stream, "-ERR invalid span. No R2 singal data in span.\n");
					goto done;
				}
				r2context = r2data->r2context;
				openr2_variant_t r2variant = openr2_context_get_variant(r2context);
				stream->write_function(stream, 
						"Variant: %s\n"
						"Max ANI: %d\n"
						"Max DNIS: %d\n"
						"ANI First: %s\n"
						"Immediate Accept: %s\n",
						openr2_proto_get_variant_string(r2variant),
						openr2_context_get_max_ani(r2context),
						openr2_context_get_max_dnis(r2context),
						openr2_context_get_ani_first(r2context) ? "Yes" : "No",
						openr2_context_get_immediate_accept(r2context) ? "Yes" : "No");
				stream->write_function(stream, "\n");
				stream->write_function(stream, "%4s %-12.12s %-12.12s\n", "Channel", "Tx CAS", "Rx CAS");
				for (i = 1; i <= span->chan_count; i++) {
					if (i == 16) continue;
					r2chan = R2CALL(span->channels[i])->r2chan;
					stream->write_function(stream, "%4d    %-12.12s %-12.12s\n", 
							span->channels[i]->physical_chan_id,
							openr2_chan_get_tx_cas_string(r2chan),
							openr2_chan_get_rx_cas_string(r2chan));
				}
				stream->write_function(stream, "\n");
				stream->write_function(stream, "+OK.\n");
				goto done;
			} else {
				stream->write_function(stream, "-ERR invalid span.\n");
				goto done;
			}
		}

	}

	if (argc == 1) {
		if (!strcasecmp(argv[0], "threads")) {
			ftdm_mutex_lock(g_thread_count_mutex);
			stream->write_function(stream, "%d R2 channel threads up\n", g_thread_count);
			ftdm_mutex_unlock(g_thread_count_mutex);
			stream->write_function(stream, "+OK.\n");
			goto done;
		}

		if (!strcasecmp(argv[0], "version")) {
			stream->write_function(stream, "OpenR2 version: %s, revision: %s\n", openr2_get_version(), openr2_get_revision());
			stream->write_function(stream, "+OK.\n");
			goto done;
		}

		if (!strcasecmp(argv[0], "variants")) {
			int32_t numvariants = 0;
			const openr2_variant_entry_t *variants = openr2_proto_get_variant_list(&numvariants);
			if (!variants) {
				stream->write_function(stream, "-ERR failed to retrieve openr2 variant list.\n");
				goto done;
			}
#define VARIANT_FORMAT "%4s %40s\n"
			stream->write_function(stream, VARIANT_FORMAT, "Variant Code", "Country");
			numvariants--;
			for (; numvariants; numvariants--) {
				stream->write_function(stream, VARIANT_FORMAT, variants[numvariants].name, variants[numvariants].country);
			}
			stream->write_function(stream, "+OK.\n");
#undef VARIANT_FORMAT
			goto done;
		}
	}

	stream->write_function(stream, "-ERR invalid command.\n");

done:

	ftdm_safe_free(mycmd);

	return FTDM_SUCCESS;

}

static FIO_IO_LOAD_FUNCTION(ftdm_r2_io_init)
{
	assert(fio != NULL);
	memset(&g_ftdm_r2_interface, 0, sizeof(g_ftdm_r2_interface));

	g_ftdm_r2_interface.name = "r2";
	g_ftdm_r2_interface.api = ftdm_r2_api;

	*fio = &g_ftdm_r2_interface;

	return FTDM_SUCCESS;
}

static FIO_SIG_LOAD_FUNCTION(ftdm_r2_init)
{
	g_mod_data_hash = create_hashtable(10, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	if (!g_mod_data_hash) {
		return FTDM_FAIL;
	}
	ftdm_mutex_create(&g_thread_count_mutex);
	return FTDM_SUCCESS;
}

static FIO_SIG_UNLOAD_FUNCTION(ftdm_r2_destroy)
{
	ftdm_hash_iterator_t *i = NULL;
	ftdm_r2_span_pvt_t *spanpvt = NULL;
	const void *key = NULL;
	void *val = NULL;
	for (i = hashtable_first(g_mod_data_hash); i; i = hashtable_next(i)) {
		hashtable_this(i, &key, NULL, &val);
		if (key && val) {
			spanpvt = val;
			openr2_context_delete(spanpvt->r2context);
			hashtable_destroy(spanpvt->r2calls);
		}
	}
	hashtable_destroy(g_mod_data_hash);
	ftdm_mutex_destroy(&g_thread_count_mutex);
	return FTDM_SUCCESS;
}

ftdm_module_t ftdm_module = { 
	"r2",
	ftdm_r2_io_init,
	NULL,
	ftdm_r2_init,
	ftdm_r2_configure_span,
	ftdm_r2_destroy
};
	

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
