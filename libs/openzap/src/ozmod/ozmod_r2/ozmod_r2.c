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
#include <pthread.h>
#include <openr2.h>
#include "openzap.h"

/* debug thread count for r2 legs */
static zap_mutex_t* g_thread_count_mutex;
static int32_t g_thread_count = 0;

/* when the users kills a span we clear this flag to kill the signaling thread */
/* FIXME: what about the calls that are already up-and-running? */
typedef enum {
	ZAP_R2_RUNNING = (1 << 0),
} zap_r2_flag_t;

/* private call information stored in zchan->call_data void* ptr */
#define R2CALL(zchan) ((zap_r2_call_t*)((zchan)->call_data))
typedef struct zap_r2_call_t {
    openr2_chan_t *r2chan;
	int accepted:1;
	int answer_pending:1;
	int state_ack_pending:1;
	int disconnect_rcvd:1;
	int zap_started:1;
	zap_channel_state_t chanstate;
	zap_size_t dnis_index;
	zap_size_t ani_index;
	char name[10];
} zap_r2_call_t;

/* this is just used as place holder in the stack when configuring the span to avoid using bunch of locals */
typedef struct oz_r2_conf_s {
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
} oz_r2_conf_t;

/* r2 configuration stored in span->signal_data */
typedef struct zap_r2_data_s {
	/* span flags */
	zap_r2_flag_t flags;
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
} zap_r2_data_t;

/* one element per span will be stored in g_mod_data_hash global var to keep track of them
   and destroy them on module unload */
typedef struct zap_r2_span_pvt_s {
	openr2_context_t *r2context; /* r2 context allocated for this span */
	zap_hash_t *r2calls; /* hash table of allocated call data per channel for this span */
} zap_r2_span_pvt_t;

/* span monitor thread */
static void *zap_r2_run(zap_thread_t *me, void *obj);

/* channel monitor thread */
static void *zap_r2_channel_run(zap_thread_t *me, void *obj);

/* hash of all the private span allocations
   we need to keep track of them to destroy them when unloading the module 
   since openzap does not notify signaling modules when destroying a span
   span -> zap_r2_mod_allocs_t */
static zap_hash_t *g_mod_data_hash;

/* IO interface for the command API */
static zap_io_interface_t g_zap_r2_interface;

static void oz_r2_clean_call(zap_r2_call_t *call)
{
    openr2_chan_t *r2chan = call->r2chan;
    memset(call, 0, sizeof(*call));
    call->r2chan = r2chan;
}

static void oz_r2_accept_call(zap_channel_t *zchan)
{
	openr2_chan_t *r2chan = R2CALL(zchan)->r2chan;
	// FIXME: not always accept as no charge, let the user decide that
	// also we should check the return code from openr2_chan_accept_call and handle error condition
	// hanging up the call with protocol error as the reason, this openr2 API will fail only when there something
	// wrong at the I/O layer or the library itself
	openr2_chan_accept_call(r2chan, OR2_CALL_NO_CHARGE);
	R2CALL(zchan)->accepted = 1;
}

static void oz_r2_answer_call(zap_channel_t *zchan)
{
	openr2_chan_t *r2chan = R2CALL(zchan)->r2chan;
	// FIXME
	// 1. check openr2_chan_answer_call return code
	// 2. The openr2_chan_answer_call_with_mode should be used depending on user settings
	// openr2_chan_answer_call_with_mode(r2chan, OR2_ANSWER_SIMPLE);
	openr2_chan_answer_call(r2chan);
	R2CALL(zchan)->answer_pending = 0;
}

static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(r2_outgoing_call)
{
	zap_status_t status;
	zap_mutex_lock(zchan->mutex);

	/* the channel may be down but the thread not quite done */
	zap_wait_for_flag_cleared(zchan, ZAP_CHANNEL_INTHREAD, 200);

	if (zap_test_flag(zchan, ZAP_CHANNEL_INTHREAD)) {
		zap_log(ZAP_LOG_ERROR, "%d:%d Yay! R2 outgoing call in channel that is already in thread.\n", 
				zchan->span_id, zchan->chan_id);
		zap_mutex_unlock(zchan->mutex);
		return ZAP_FAIL;
	}

	oz_r2_clean_call(zchan->call_data);
	R2CALL(zchan)->chanstate = ZAP_CHANNEL_STATE_DOWN;
	zap_channel_set_state(zchan, ZAP_CHANNEL_STATE_DIALING, 0);
	zap_set_flag(zchan, ZAP_CHANNEL_OUTBOUND);
	R2CALL(zchan)->zap_started = 1;
	zap_mutex_unlock(zchan->mutex);

	status = zap_thread_create_detached(zap_r2_channel_run, zchan);
	if (status == ZAP_FAIL) {
		zap_log(ZAP_LOG_ERROR, "%d:%d Cannot handle request to start call in channel, failed to create thread!\n", 
				zchan->span_id, zchan->chan_id);
		zap_channel_done(zchan);
		return ZAP_FAIL;
	}

	return ZAP_SUCCESS;
}

static zap_status_t zap_r2_start(zap_span_t *span)
{
	zap_r2_data_t *r2_data = span->signal_data;
	zap_set_flag(r2_data, ZAP_R2_RUNNING);
	return zap_thread_create_detached(zap_r2_run, span);
}

/* always called from the monitor thread */
static void zap_r2_on_call_init(openr2_chan_t *r2chan)
{
	zap_channel_t *zchan = openr2_chan_get_client_data(r2chan);
	zap_status_t status;
	zap_log(ZAP_LOG_NOTICE, "Received request to start call on chan %d\n", openr2_chan_get_number(r2chan));

	zap_mutex_lock(zchan->mutex);

	if (zchan->state != ZAP_CHANNEL_STATE_DOWN) {
		zap_log(ZAP_LOG_ERROR, "Cannot handle request to start call in channel %d, invalid state (%d)\n", 
				openr2_chan_get_number(r2chan), zchan->state);
		zap_mutex_unlock(zchan->mutex);
		return;
	}

	/* the channel may be down but the thread not quite done */
	zap_wait_for_flag_cleared(zchan, ZAP_CHANNEL_INTHREAD, 200);

	if (zap_test_flag(zchan, ZAP_CHANNEL_INTHREAD)) {
		zap_log(ZAP_LOG_ERROR, "Cannot handle request to start call in channel %d, already in thread!\n", 
				openr2_chan_get_number(r2chan));
		zap_mutex_unlock(zchan->mutex);
		return;
	}
	oz_r2_clean_call(zchan->call_data);
	R2CALL(zchan)->chanstate = ZAP_CHANNEL_STATE_DOWN;
	zap_channel_set_state(zchan, ZAP_CHANNEL_STATE_COLLECT, 0);
	zap_mutex_unlock(zchan->mutex);

	status = zap_thread_create_detached(zap_r2_channel_run, zchan);
	if (status == ZAP_FAIL) {
		zap_log(ZAP_LOG_ERROR, "Cannot handle request to start call in channel %d, failed to create thread!\n", 
				openr2_chan_get_number(r2chan));
	}
}

/* only called for incoming calls when the ANI, DNIS etc is complete and the user has to decide either to accept or reject the call */
static void zap_r2_on_call_offered(openr2_chan_t *r2chan, const char *ani, const char *dnis, openr2_calling_party_category_t category)
{
	zap_sigmsg_t sigev;
	zap_channel_t *zchan = openr2_chan_get_client_data(r2chan);

	zap_log(ZAP_LOG_NOTICE, "Call offered on chan %d, ANI = %s, DNIS = %s, Category = %s\n", openr2_chan_get_number(r2chan), 
			ani, dnis, openr2_proto_get_category_string(category));

	/* notify the user about the new call */
	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = zchan->chan_id;
	sigev.span_id = zchan->span_id;
	sigev.channel = zchan;
	sigev.event_id = ZAP_SIGEVENT_START;

	if (zap_span_send_signal(zchan->span, &sigev) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_NOTICE, "Failed to handle call offered on chan %d\n", openr2_chan_get_number(r2chan));
		openr2_chan_disconnect_call(r2chan, OR2_CAUSE_OUT_OF_ORDER);
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_CANCEL);
		return; 
	}
	zap_channel_use(zchan);
	R2CALL(zchan)->zap_started = 1; 
}

static void zap_r2_on_call_accepted(openr2_chan_t *r2chan, openr2_call_mode_t mode)
{
	zap_channel_t *zchan = openr2_chan_get_client_data(r2chan);
	zap_log(ZAP_LOG_NOTICE, "Call accepted on chan %d\n", openr2_chan_get_number(r2chan));
	/* at this point the MF signaling has ended and there is no point on keep reading */
	openr2_chan_disable_read(r2chan);
	if (OR2_DIR_BACKWARD == openr2_chan_get_direction(r2chan)) {
		R2CALL(zchan)->state_ack_pending = 1;
		if (R2CALL(zchan)->answer_pending) {
			zap_log(ZAP_LOG_DEBUG, "Answer was pending on chan %d, answering now.\n", openr2_chan_get_number(r2chan));
			oz_r2_answer_call(zchan);
			return;
		}
	} else {
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_PROGRESS);
	}
}

static void zap_r2_on_call_answered(openr2_chan_t *r2chan)
{
	zap_channel_t *zchan = openr2_chan_get_client_data(r2chan);
	zap_log(ZAP_LOG_NOTICE, "Call answered on chan %d\n", openr2_chan_get_number(r2chan));
	/* notify the upper layer of progress in the outbound call */
	if (OR2_DIR_FORWARD == openr2_chan_get_direction(r2chan)) {
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_UP);
	}
}

/* may be called in the signaling or media thread depending on whether the hangup is product of MF or CAS signaling */
static void zap_r2_on_call_disconnect(openr2_chan_t *r2chan, openr2_call_disconnect_cause_t cause)
{
	zap_sigmsg_t sigev;
	zap_r2_data_t *r2data;
	zap_channel_t *zchan = openr2_chan_get_client_data(r2chan);
	zap_log(ZAP_LOG_NOTICE, "Call disconnected on chan %d\n", openr2_chan_get_number(r2chan));

	zap_log(ZAP_LOG_DEBUG, "Got openr2 disconnection, clearing call on channel %d\n", zchan->physical_chan_id);

	R2CALL(zchan)->disconnect_rcvd = 1;

	/* acknowledge the hangup, cause will be ignored. From here to -> HANGUP once the openzap side hangs up as well */
	openr2_chan_disconnect_call(r2chan, OR2_CAUSE_NORMAL_CLEARING);

	/* if the call has not been started yet we must go to HANGUP right here */ 
	if (!R2CALL(zchan)->zap_started) {
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
		return;
	}

	/* FIXME: use the cause received from openr2 and map it to zap cause  */
	zchan->caller_data.hangup_cause  = ZAP_CAUSE_NORMAL_CLEARING; 

	/* notify the user of the call terminating */
	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = zchan->chan_id;
	sigev.span_id = zchan->span_id;
	sigev.channel = zchan;
	sigev.event_id = ZAP_SIGEVENT_STOP;
	r2data = zchan->span->signal_data;

	zap_span_send_signal(zchan->span, &sigev);
}

static void zap_r2_on_call_end(openr2_chan_t *r2chan)
{
	zap_channel_t *zchan = openr2_chan_get_client_data(r2chan);
	zap_log(ZAP_LOG_NOTICE, "Call finished on chan %d\n", openr2_chan_get_number(r2chan));
	/* this means the openzap side disconnected the call, therefore we must move to DOWN here */
	if (!R2CALL(zchan)->disconnect_rcvd) {
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
		return;
	}
}

static void zap_r2_on_call_read(openr2_chan_t *r2chan, const unsigned char *buf, int buflen)
{
	zap_log(ZAP_LOG_NOTICE, "Call read data on chan %d\n", openr2_chan_get_number(r2chan));
}

static void zap_r2_on_hardware_alarm(openr2_chan_t *r2chan, int alarm)
{
	zap_log(ZAP_LOG_NOTICE, "Alarm on chan %d (%d)\n", openr2_chan_get_number(r2chan), alarm);
}

static void zap_r2_on_os_error(openr2_chan_t *r2chan, int errorcode)
{
	zap_log(ZAP_LOG_ERROR, "OS error on chan %d: %s\n", openr2_chan_get_number(r2chan), strerror(errorcode));
}

static void zap_r2_on_protocol_error(openr2_chan_t *r2chan, openr2_protocol_error_t reason)
{
	zap_sigmsg_t sigev;
	zap_r2_data_t *r2data;
	zap_channel_t *zchan = openr2_chan_get_client_data(r2chan);

	zap_log(ZAP_LOG_ERROR, "Protocol error on chan %d\n", openr2_chan_get_number(r2chan));

	R2CALL(zchan)->disconnect_rcvd = 1;

	if (!R2CALL(zchan)->zap_started) {
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
		return;
	}

	zchan->caller_data.hangup_cause  = ZAP_CAUSE_PROTOCOL_ERROR; 

	/* notify the user of the call terminating */
	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = zchan->chan_id;
	sigev.span_id = zchan->span_id;
	sigev.channel = zchan;
	sigev.event_id = ZAP_SIGEVENT_STOP;
	r2data = zchan->span->signal_data;

	zap_span_send_signal(zchan->span, &sigev);
}

static void zap_r2_on_line_blocked(openr2_chan_t *r2chan)
{
	zap_log(ZAP_LOG_NOTICE, "Far end blocked on chan %d\n", openr2_chan_get_number(r2chan));
}

static void zap_r2_on_line_idle(openr2_chan_t *r2chan)
{
	zap_log(ZAP_LOG_NOTICE, "Far end unblocked on chan %d\n", openr2_chan_get_number(r2chan));
}

static void zap_r2_write_log(openr2_log_level_t level, const char *message)
{
	switch (level) {
		case OR2_LOG_NOTICE:
			zap_log(ZAP_LOG_NOTICE, "%s", message);
			break;
		case OR2_LOG_WARNING:
			zap_log(ZAP_LOG_WARNING, "%s", message);
			break;
		case OR2_LOG_ERROR:
			zap_log(ZAP_LOG_ERROR, "%s", message);
			break;
		case OR2_LOG_STACK_TRACE:
		case OR2_LOG_MF_TRACE:
		case OR2_LOG_CAS_TRACE:
		case OR2_LOG_DEBUG:
		case OR2_LOG_EX_DEBUG:
			zap_log(ZAP_LOG_DEBUG, "%s", message);
			break;
		default:
			zap_log(ZAP_LOG_WARNING, "We should handle logging level %d here.\n", level);
			zap_log(ZAP_LOG_DEBUG, "%s", message);
			break;
	}
}

static void zap_r2_on_context_log(openr2_context_t *r2context, openr2_log_level_t level, const char *fmt, va_list ap)
{
#define CONTEXT_TAG "Context -"
	char logmsg[256];
	char completemsg[sizeof(logmsg) + sizeof(CONTEXT_TAG) - 1];
	vsnprintf(logmsg, sizeof(logmsg), fmt, ap);
	snprintf(completemsg, sizeof(completemsg), CONTEXT_TAG "%s", logmsg);
	zap_r2_write_log(level, completemsg);
#undef CONTEXT_TAG
}

static void zap_r2_on_chan_log(openr2_chan_t *r2chan, openr2_log_level_t level, const char *fmt, va_list ap)
{
#define CHAN_TAG "Chan "
	char logmsg[256];
	char completemsg[sizeof(logmsg) + sizeof(CHAN_TAG) - 1];
	vsnprintf(logmsg, sizeof(logmsg), fmt, ap);
	snprintf(completemsg, sizeof(completemsg), CHAN_TAG "%d: %s", openr2_chan_get_number(r2chan), logmsg);
	zap_r2_write_log(level, completemsg);
#undef CHAN_TAG
}

static int zap_r2_on_dnis_digit_received(openr2_chan_t *r2chan, char digit)
{
	zap_sigmsg_t sigev;
	zap_r2_data_t *r2data;
	zap_channel_t *zchan = openr2_chan_get_client_data(r2chan);
	zap_size_t collected_len = R2CALL(zchan)->dnis_index;

	zap_log(ZAP_LOG_DEBUG, "DNIS digit %d received chan %d\n", digit, openr2_chan_get_number(r2chan));

	/* save the digit we just received */ 
	zchan->caller_data.dnis.digits[collected_len] = digit;
	collected_len++;
	zchan->caller_data.dnis.digits[collected_len] = '\0';
	R2CALL(zchan)->dnis_index = collected_len;

	/* notify the user about the new digit and check if we should stop requesting more DNIS */
	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = zchan->chan_id;
	sigev.span_id = zchan->span_id;
	sigev.channel = zchan;
	sigev.event_id = ZAP_SIGEVENT_COLLECTED_DIGIT;
	r2data = zchan->span->signal_data;
	if (zap_span_send_signal(zchan->span, &sigev) == ZAP_BREAK) {
		zap_log(ZAP_LOG_NOTICE, "Requested to stop getting DNIS. Current DNIS = %s on chan %d\n", zchan->caller_data.dnis.digits, openr2_chan_get_number(r2chan));
		return OR2_STOP_DNIS_REQUEST; 
	}

	/* the only other reason to stop requesting DNIS is that there is no more room to save it */
	if (collected_len == (sizeof(zchan->caller_data.dnis.digits) - 1)) {
		zap_log(ZAP_LOG_NOTICE, "No more room for DNIS. Current DNIS = %s on chan %d\n", zchan->caller_data.dnis.digits, openr2_chan_get_number(r2chan));
		return OR2_STOP_DNIS_REQUEST;
	}

	return OR2_CONTINUE_DNIS_REQUEST; 
}

static void zap_r2_on_ani_digit_received(openr2_chan_t *r2chan, char digit)
{
	zap_channel_t *zchan = openr2_chan_get_client_data(r2chan);
	zap_size_t collected_len = R2CALL(zchan)->ani_index;

	/* check if we should drop ANI */
	if (collected_len == (sizeof(zchan->caller_data.ani.digits) - 1)) {
		zap_log(ZAP_LOG_NOTICE, "No more room for ANI %c on chan %d, digit dropped.\n", digit, openr2_chan_get_number(r2chan));
		return;
	}
	zap_log(ZAP_LOG_DEBUG, "ANI digit %c received chan %d\n", digit, openr2_chan_get_number(r2chan));

	/* save the digit we just received */ 
	zchan->caller_data.ani.digits[collected_len++] = digit;
	zchan->caller_data.ani.digits[collected_len] = '\0';
}

static openr2_event_interface_t zap_r2_event_iface = {
	.on_call_init = zap_r2_on_call_init,
	.on_call_offered = zap_r2_on_call_offered,
	.on_call_accepted = zap_r2_on_call_accepted,
	.on_call_answered = zap_r2_on_call_answered,
	.on_call_disconnect = zap_r2_on_call_disconnect,
	.on_call_end = zap_r2_on_call_end,
	.on_call_read = zap_r2_on_call_read,
	.on_hardware_alarm = zap_r2_on_hardware_alarm,
	.on_os_error = zap_r2_on_os_error,
	.on_protocol_error = zap_r2_on_protocol_error,
	.on_line_blocked = zap_r2_on_line_blocked,
	.on_line_idle = zap_r2_on_line_idle,
	/* cast seems to be needed to get rid of the annoying warning regarding format attribute  */
	.on_context_log = (openr2_handle_context_logging_func)zap_r2_on_context_log,
	.on_dnis_digit_received = zap_r2_on_dnis_digit_received,
	.on_ani_digit_received = zap_r2_on_ani_digit_received,
	/* so far we do nothing with billing pulses */
	.on_billing_pulse_received = NULL
};

static int zap_r2_io_set_cas(openr2_chan_t *r2chan, int cas)
{
	zap_channel_t *zap_chan = openr2_chan_get_fd(r2chan);
	zap_status_t status = zap_channel_command(zap_chan, ZAP_COMMAND_SET_CAS_BITS, &cas);
	if (ZAP_FAIL == status) {
		return -1;
	}
	return 0;
}

static int zap_r2_io_get_cas(openr2_chan_t *r2chan, int *cas)
{
	zap_channel_t *zap_chan = openr2_chan_get_fd(r2chan);
	zap_status_t status = zap_channel_command(zap_chan, ZAP_COMMAND_GET_CAS_BITS, cas);
	if (ZAP_FAIL == status) {
		return -1;
	}
	return 0;
}

static int zap_r2_io_flush_write_buffers(openr2_chan_t *r2chan)
{
	zap_channel_t *zap_chan = openr2_chan_get_fd(r2chan);
	zap_status_t status = zap_channel_command(zap_chan, ZAP_COMMAND_FLUSH_TX_BUFFERS, NULL);
	if (ZAP_FAIL == status) {
		return -1;
	}
	return 0;
}

static int zap_r2_io_write(openr2_chan_t *r2chan, const void *buf, int size)
{
	zap_channel_t *zap_chan = openr2_chan_get_fd(r2chan);
	zap_size_t outsize = size;
	zap_status_t status = zap_channel_write(zap_chan, (void *)buf, size, &outsize);
	if (ZAP_FAIL == status) {
		return -1;
	}
	return outsize;
}

static int zap_r2_io_read(openr2_chan_t *r2chan, const void *buf, int size)
{
	zap_channel_t *zap_chan = openr2_chan_get_fd(r2chan);
	zap_size_t outsize = size;
	zap_status_t status = zap_channel_read(zap_chan, (void *)buf, &outsize);
	if (ZAP_FAIL == status) {
		return -1;
	}
	return outsize;
}

static int zap_r2_io_wait(openr2_chan_t *r2chan, int *flags, int block)
{
	zap_status_t status;
	zap_wait_flag_t zapflags = 0;

	zap_channel_t *zap_chan = openr2_chan_get_fd(r2chan);
	int32_t timeout = block ? -1 : 0;

	if (*flags & OR2_IO_READ) {
		zapflags |= ZAP_READ;
	}
	if (*flags & OR2_IO_WRITE) {
		zapflags |= ZAP_WRITE;
	}
	if (*flags & OR2_IO_OOB_EVENT) {
		zapflags |= ZAP_EVENTS;
	}

	status = zap_channel_wait(zap_chan, &zapflags, timeout);

	if (ZAP_SUCCESS != status) {
		return -1;
	}

	*flags = 0;
	if (zapflags & ZAP_READ) {
		*flags |= OR2_IO_READ;
	}
	if (zapflags & ZAP_WRITE) {
		*flags |= OR2_IO_WRITE;
	}
	if (zapflags & ZAP_EVENTS) {
		*flags |= OR2_IO_OOB_EVENT;
	}

	return 0;
}

/* The following openr2 hooks never get called, read on for reasoning ... */
/* since openzap takes care of opening the file descriptor and using openr2_chan_new_from_fd, openr2 should never call this hook */
static openr2_io_fd_t zap_r2_io_open(openr2_context_t *r2context, int channo)
{
	zap_log(ZAP_LOG_ERROR, "I should not be called (I/O open)!!\n");
	return NULL;
}

/* since openzap takes care of closing the file descriptor and uses openr2_chan_new_from_fd, openr2 should never call this hook */
static int zap_r2_io_close(openr2_chan_t *r2chan)
{
	zap_log(ZAP_LOG_ERROR, "I should not be called (I/O close)!!\n");
	return 0;
}

/* since openzap takes care of opening the file descriptor and using openr2_chan_new_from_fd, openr2 should never call this hook */
static int zap_r2_io_setup(openr2_chan_t *r2chan)
{
	zap_log(ZAP_LOG_ERROR, "I should not be called (I/O Setup)!!\n");
	return 0;
}

/* since the signaling thread calls openr2_chan_process_cas_signaling directly, openr2 should never call this hook */
static int zap_r2_io_get_oob_event(openr2_chan_t *r2chan, openr2_oob_event_t *event)
{
	*event = 0;
	zap_log(ZAP_LOG_ERROR, "I should not be called (I/O get oob event)!!\n");
	return 0;
}

static openr2_io_interface_t zap_r2_io_iface = {
	.open = zap_r2_io_open, /* never called */
	.close = zap_r2_io_close, /* never called */
	.set_cas = zap_r2_io_set_cas,
	.get_cas = zap_r2_io_get_cas,
	.flush_write_buffers = zap_r2_io_flush_write_buffers,
	.write = zap_r2_io_write,
	.read = zap_r2_io_read,
	.setup = zap_r2_io_setup, /* never called */
	.wait = zap_r2_io_wait,
	.get_oob_event = zap_r2_io_get_oob_event /* never called */
};

static ZIO_SIG_CONFIGURE_FUNCTION(zap_r2_configure_span)
	//zap_status_t (zap_span_t *span, zio_signal_cb_t sig_cb, va_list ap)
{
	int i = 0;
	int conf_failure = 0;
	char *var = NULL;
	char *val = NULL;
	zap_r2_data_t *r2data = NULL;
	zap_r2_span_pvt_t *spanpvt = NULL;
	zap_r2_call_t *r2call = NULL;
	openr2_chan_t *r2chan = NULL;

	assert(sig_cb != NULL);

	oz_r2_conf_t r2conf = 
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
		return ZAP_FAIL;
	}

	while ((var = va_arg(ap, char *))) {
		zap_log(ZAP_LOG_DEBUG, "Reading R2 parameter %s for span %d\n", var, span->span_id);
		if (!strcasecmp(var, "variant")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (zap_strlen_zero_buf(val)) {
				zap_log(ZAP_LOG_NOTICE, "Ignoring empty R2 variant parameter\n");
				continue;
			}
			r2conf.variant = openr2_proto_get_variant(val);
			if (r2conf.variant == OR2_VAR_UNKNOWN) {
				zap_log(ZAP_LOG_ERROR, "Unknown R2 variant %s\n", val);
				conf_failure = 1;
				break;
			}
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d for variant %s\n", span->span_id, val);
		} else if (!strcasecmp(var, "category")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (zap_strlen_zero_buf(val)) {
				zap_log(ZAP_LOG_NOTICE, "Ignoring empty R2 category parameter\n");
				continue;
			}
			r2conf.category = openr2_proto_get_category(val);
			if (r2conf.category == OR2_CALLING_PARTY_CATEGORY_UNKNOWN) {
				zap_log(ZAP_LOG_ERROR, "Unknown R2 caller category %s\n", val);
				conf_failure = 1;
				break;
			}
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with default category %s\n", span->span_id, val);
		} else if (!strcasecmp(var, "logdir")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (zap_strlen_zero_buf(val)) {
				zap_log(ZAP_LOG_NOTICE, "Ignoring empty R2 logdir parameter\n");
				continue;
			}
			r2conf.logdir = val;
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with logdir %s\n", span->span_id, val);
		} else if (!strcasecmp(var, "logging")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (zap_strlen_zero_buf(val)) {
				zap_log(ZAP_LOG_NOTICE, "Ignoring empty R2 logging parameter\n");
				continue;
			}
			openr2_log_level_t tmplevel;
			char *clevel;
			char *logval = malloc(strlen(val)+1); /* alloca man page scared me, so better to use good ol' malloc  */
			if (!logval) {
				zap_log(ZAP_LOG_WARNING, "Ignoring R2 logging parameter: '%s', failed to alloc memory\n", val);
				continue;
			}
			strcpy(logval, val);
			while (logval) {
				clevel = strsep(&logval, ",");
				if (-1 == (tmplevel = openr2_log_get_level(clevel))) {
					zap_log(ZAP_LOG_WARNING, "Ignoring invalid R2 logging level: '%s'\n", clevel);
					continue;
				}
				r2conf.loglevel |= tmplevel;
				zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with loglevel %s\n", span->span_id, clevel);
			}
			free(logval);
		} else if (!strcasecmp(var, "advanced_protocol_file")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			if (zap_strlen_zero_buf(val)) {
				zap_log(ZAP_LOG_NOTICE, "Ignoring empty R2 advanced_protocol_file parameter\n");
				continue;
			}
			r2conf.advanced_protocol_file = val;
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with advanced protocol file %s\n", span->span_id, val);
		} else if (!strcasecmp(var, "allow_collect_calls")) {
			r2conf.allow_collect_calls = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with allow collect calls max ani = %d\n", span->span_id, r2conf.allow_collect_calls);
		} else if (!strcasecmp(var, "double_answer")) {
			r2conf.double_answer = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with double answer = %d\n", span->span_id, r2conf.double_answer);
		} else if (!strcasecmp(var, "immediate_accept")) {
			r2conf.immediate_accept = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with immediate accept = %d\n", span->span_id, r2conf.immediate_accept);
		} else if (!strcasecmp(var, "skip_category")) {
			r2conf.skip_category = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with skip category = %d\n", span->span_id, r2conf.skip_category);
		} else if (!strcasecmp(var, "forced_release")) {
			r2conf.forced_release = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with forced release = %d\n", span->span_id, r2conf.forced_release);
		} else if (!strcasecmp(var, "charge_calls")) {
			r2conf.charge_calls = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with charge calls = %d\n", span->span_id, r2conf.charge_calls);
		} else if (!strcasecmp(var, "get_ani_first")) {
			r2conf.get_ani_first = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with get ani first = %d\n", span->span_id, r2conf.get_ani_first);
		} else if (!strcasecmp(var, "call_files")) {
			r2conf.call_files = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with call files = %d\n", span->span_id, r2conf.call_files);
		} else if (!strcasecmp(var, "mfback_timeout")) {
			r2conf.mfback_timeout = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with MF backward timeout = %dms\n", span->span_id, r2conf.mfback_timeout);
		} else if (!strcasecmp(var, "metering_pulse_timeout")) {
			r2conf.metering_pulse_timeout = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with metering pulse timeout = %dms\n", span->span_id, r2conf.metering_pulse_timeout);
		} else if (!strcasecmp(var, "max_ani")) {
			r2conf.max_ani = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with max ani = %d\n", span->span_id, r2conf.max_ani);
		} else if (!strcasecmp(var, "max_dnis")) {
			r2conf.max_dnis = va_arg(ap, int);
			zap_log(ZAP_LOG_DEBUG, "Configuring R2 span %d with max dnis = %d\n", span->span_id, r2conf.max_dnis);
		} else {
			snprintf(span->last_error, sizeof(span->last_error), "Unknown R2 parameter [%s]", var);
			return ZAP_FAIL;
		}
	}

	if (conf_failure) {
		snprintf(span->last_error, sizeof(span->last_error), "R2 configuration error");
		return ZAP_FAIL;
	}

	r2data = malloc(sizeof(*r2data));
	if (!r2data) {
		snprintf(span->last_error, sizeof(span->last_error), "Failed to allocate R2 data.");
		return ZAP_FAIL;
	}
	memset(r2data, 0, sizeof(*r2data));

	spanpvt = malloc(sizeof(*spanpvt));
	if (!spanpvt) {
		snprintf(span->last_error, sizeof(span->last_error), "Failed to allocate private span data container.");
		goto fail;
	}
	memset(spanpvt, 0, sizeof(*spanpvt));

	r2data->r2context = openr2_context_new(r2conf.variant, &zap_r2_event_iface, r2conf.max_ani, r2conf.max_dnis);
	if (!r2data->r2context) {
		snprintf(span->last_error, sizeof(span->last_error), "Cannot create openr2 context for span.");
		goto fail;
	}
	openr2_context_set_io_type(r2data->r2context, OR2_IO_CUSTOM, &zap_r2_io_iface);
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

	spanpvt->r2calls = create_hashtable(ZAP_MAX_CHANNELS_SPAN, zap_hash_hashfromstring, zap_hash_equalkeys);
	if (!spanpvt->r2calls) {
		snprintf(span->last_error, sizeof(span->last_error), "Cannot create channel calls hash for span.");
		goto fail;
	}

	for (i = 1; (i <= span->chan_count) && (i <= ZAP_MAX_CHANNELS_SPAN); i++) {
		r2chan = openr2_chan_new_from_fd(r2data->r2context, span->channels[i], span->channels[i]->physical_chan_id);
		if (!r2chan) {
			snprintf(span->last_error, sizeof(span->last_error), "Cannot create all openr2 channels for span.");
			goto fail;
		}
		if (r2conf.call_files) {
			openr2_chan_enable_call_files(r2chan);
			openr2_chan_set_log_level(r2chan, r2conf.loglevel);
		}

		r2call = malloc(sizeof(*r2call));
		if (!r2call) {
			snprintf(span->last_error, sizeof(span->last_error), "Cannot create all R2 call data structures for the span.");
			zap_safe_free(r2chan);
			goto fail;
		}
		memset(r2call, 0, sizeof(*r2call));
		openr2_chan_set_logging_func(r2chan, zap_r2_on_chan_log);
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

	span->start = zap_r2_start;
	r2data->flags = 0;
	span->signal_cb = sig_cb;
	span->signal_type = ZAP_SIGTYPE_R2;
	span->signal_data = r2data;
	span->outgoing_call = r2_outgoing_call;

	return ZAP_SUCCESS;

fail:

	if (r2data && r2data->r2context) {
		openr2_context_delete(r2data->r2context);
	}
	if (spanpvt && spanpvt->r2calls) {
		hashtable_destroy(spanpvt->r2calls);
	}
	zap_safe_free(r2data);
	zap_safe_free(spanpvt);
	return ZAP_FAIL;

}

static void *zap_r2_channel_run(zap_thread_t *me, void *obj)
{
	zap_channel_t *closed_chan;
	uint32_t interval = 0;
	zap_sigmsg_t sigev;
	zap_channel_t *zchan = (zap_channel_t *)obj;
	openr2_chan_t *r2chan = R2CALL(zchan)->r2chan;

	zap_set_flag_locked(zchan, ZAP_CHANNEL_INTHREAD);

	zap_mutex_lock(g_thread_count_mutex);
	g_thread_count++;
	zap_mutex_unlock(g_thread_count_mutex);

	zap_log(ZAP_LOG_DEBUG, "R2 CHANNEL thread starting on %d in state %s.\n", 
			zchan->physical_chan_id,
			zap_channel_state2str(zchan->state));

	if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "OPEN ERROR [%s]\n", zchan->last_error);
		goto endthread;
	}

	zap_channel_command(zchan, ZAP_COMMAND_GET_INTERVAL, &interval);

	assert(interval != 0);
	zap_log(ZAP_LOG_DEBUG, "Got %d interval for chan %d\n", interval, zchan->physical_chan_id);

	if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
		/* FIXME: is this needed? */
		memset(zchan->caller_data.dnis.digits, 0, sizeof(zchan->caller_data.collected));
		memset(zchan->caller_data.ani.digits, 0, sizeof(zchan->caller_data.collected));
	}

	memset(&sigev, 0, sizeof(sigev));
	sigev.chan_id = zchan->chan_id;
	sigev.span_id = zchan->span_id;
	sigev.channel = zchan;

	while (zap_running()) {
		int32_t read_enabled = openr2_chan_get_read_enabled(r2chan);
		zap_wait_flag_t flags = read_enabled ? ( ZAP_READ | ZAP_WRITE ) : 0;

		if (zap_test_flag(zchan, ZAP_CHANNEL_STATE_CHANGE) && (R2CALL(zchan)->chanstate != zchan->state)) {

			zap_log(ZAP_LOG_DEBUG, "Executing state handler on %d:%d for %s\n", zchan->span_id, zchan->chan_id, zap_channel_state2str(zchan->state));
			R2CALL(zchan)->chanstate = zchan->state;

			if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND) && !R2CALL(zchan)->accepted &&
					(zchan->state == ZAP_CHANNEL_STATE_PROGRESS ||
					 zchan->state == ZAP_CHANNEL_STATE_PROGRESS_MEDIA ||
					 zchan->state == ZAP_CHANNEL_STATE_UP) ) {
				/* if an accept ack will be required we should not acknowledge the state change just yet, 
				   it will be done below after processing the MF signals, otherwise we have a race condition between openzap calling
				   openr2_chan_answer_call and openr2 accepting the call first, if openzap calls openr2_chan_answer_call before the accept cycle
				   completes, openr2 will fail to answer the call */
				zap_log(ZAP_LOG_DEBUG, "State ack in chan %d:%d for state %s will have to wait a bit\n", zchan->span_id, zchan->chan_id, zap_channel_state2str(zchan->state));
			} else if (zchan->state != ZAP_CHANNEL_STATE_DOWN){
				/* the down state will be completed in zap_channel_done below */
				zap_clear_flag_locked(zchan, ZAP_CHANNEL_STATE_CHANGE);
				zap_channel_complete_state(zchan);
			}

			switch (zchan->state) {

				/* starting an incoming call */
				case ZAP_CHANNEL_STATE_COLLECT: 
					{
						zap_log(ZAP_LOG_DEBUG, "COLLECT: Starting processing of incoming call in channel %d with interval %d\n", zchan->physical_chan_id, interval);
					}
					break;

					/* starting an outgoing call */
				case ZAP_CHANNEL_STATE_DIALING:
					{
						// FIXME: use user defined calling party
						zap_channel_use(zchan);
						zap_log(ZAP_LOG_DEBUG, "DIALING: Starting processing of outgoing call in channel %d with interval %d\n", zchan->physical_chan_id, interval);
						if (openr2_chan_make_call(r2chan, zchan->caller_data.cid_num.digits, zchan->caller_data.ani.digits, OR2_CALLING_PARTY_CATEGORY_NATIONAL_SUBSCRIBER)) {
							zap_log(ZAP_LOG_ERROR, "%d:%d Failed to make call in R2 channel, openr2_chan_make_call failed\n", zchan->span_id, zchan->chan_id);
							zchan->caller_data.hangup_cause = ZAP_CAUSE_DESTINATION_OUT_OF_ORDER;
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
						}
					}
					break;

					/* the call is ringing */
				case ZAP_CHANNEL_STATE_PROGRESS:
				case ZAP_CHANNEL_STATE_PROGRESS_MEDIA:
					{
						if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
							if (!R2CALL(zchan)->accepted) {
								zap_log(ZAP_LOG_DEBUG, "PROGRESS: Accepting call on channel %d\n", zchan->physical_chan_id);
								oz_r2_accept_call(zchan);
							} 
						} else {
							zap_log(ZAP_LOG_DEBUG, "PROGRESS: Notifying progress in channel %d\n", zchan->physical_chan_id);
							sigev.event_id = ZAP_SIGEVENT_PROGRESS;
							if (zap_span_send_signal(zchan->span, &sigev) != ZAP_SUCCESS) {
								zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
							}
						}
					}
					break;

					/* the call was answered */
				case ZAP_CHANNEL_STATE_UP:
					{
						zap_log(ZAP_LOG_DEBUG, "UP: Call was answered on channel %d\n", zchan->physical_chan_id);
						if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
							if (!R2CALL(zchan)->accepted) {
								zap_log(ZAP_LOG_DEBUG, "UP: Call has not been accepted, need to accept first\n");
								// the answering will be done in the on_call_accepted handler
								oz_r2_accept_call(zchan);
								R2CALL(zchan)->answer_pending = 1;
							} else {
								oz_r2_answer_call(zchan);
							}
						} else {
							zap_log(ZAP_LOG_DEBUG, "UP: Notifying of call answered in channel %d\n", zchan->physical_chan_id);
							sigev.event_id = ZAP_SIGEVENT_UP;
							if (zap_span_send_signal(zchan->span, &sigev) != ZAP_SUCCESS) {
								zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
							}
						}
					}
					break;

					/* just got hangup */
				case ZAP_CHANNEL_STATE_HANGUP: 
					{
						/* FIXME: the cause should be retrieved from zchan->caller_data.hangup_cause and translated from Q931 to R2 cause */
						zap_log(ZAP_LOG_DEBUG, "HANGUP: Clearing call on channel %d\n", zchan->physical_chan_id);
						if (!R2CALL(zchan)->disconnect_rcvd) {
							/* this will disconnect the call, but need to wait for the call end before moving to DOWN */
							openr2_chan_disconnect_call(r2chan, OR2_CAUSE_NORMAL_CLEARING);
						} else {
							/* at this point on_call_end possibly was already called, 
							 * but we needed to wait for the openzap confirmation before moving to DOWN */
							zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
						}
					}
					break;

					/* just got hangup from the openzap side due to abnormal failure */
				case ZAP_CHANNEL_STATE_CANCEL:
					{
						zap_log(ZAP_LOG_DEBUG, "CANCEL: Unable to receive call on channel %d\n", zchan->physical_chan_id);
						openr2_chan_disconnect_call(r2chan, OR2_CAUSE_OUT_OF_ORDER);
					}
					break;

					/* finished call for good */
				case ZAP_CHANNEL_STATE_DOWN: 
					{
						zap_log(ZAP_LOG_DEBUG, "DOWN: Placing channel %d back to the pool of available channels\n", zchan->physical_chan_id);
						zap_channel_done(zchan);
						goto endthread;
					}
					break;

				default:
					{
						zap_log(ZAP_LOG_ERROR, "%s: Unhandled channel state change in channel %d\n", zap_channel_state2str(zchan->state), zchan->physical_chan_id);
					}
					break;

			}
		}

		if (flags) {
			if (zap_channel_wait(zchan, &flags, interval * 2) != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_DEBUG, "zap_channel_wait did not return ZAP_SUCCESS\n");
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
			if (R2CALL(zchan)->state_ack_pending) {
				zap_clear_flag_locked(zchan, ZAP_CHANNEL_STATE_CHANGE);
				zap_channel_complete_state(zchan);
				R2CALL(zchan)->state_ack_pending = 0;
			}
		} else {
			/* once the MF signaling has end we just loop here waiting for state changes */
			zap_sleep(interval);
		}

	}

endthread:

	closed_chan = zchan;
	zap_channel_close(&closed_chan);
	zap_clear_flag(zchan, ZAP_CHANNEL_INTHREAD);
	zap_log(ZAP_LOG_DEBUG, "R2 channel %d thread ended.\n", zchan->physical_chan_id);

	zap_mutex_lock(g_thread_count_mutex);
	g_thread_count--;
	zap_mutex_unlock(g_thread_count_mutex);

	return NULL;
}

static void *zap_r2_run(zap_thread_t *me, void *obj)
{
	openr2_chan_t *r2chan;
	zap_status_t status;
	zap_span_t *span = (zap_span_t *) obj;
	zap_r2_data_t *r2data = span->signal_data;
	int waitms = 1000;
	int i;

	zap_log(ZAP_LOG_DEBUG, "OpenR2 monitor thread started.\n");
  r2chan = NULL;
	for (i = 1; i <= span->chan_count; i++) {
		r2chan = R2CALL(span->channels[i])->r2chan;
		openr2_chan_set_idle(r2chan);
		openr2_chan_process_cas_signaling(r2chan);
	}

	while (zap_running() && zap_test_flag(r2data, ZAP_R2_RUNNING)) {
		status = zap_span_poll_event(span, waitms);
		if (ZAP_FAIL == status) {
			zap_log(ZAP_LOG_ERROR, "Failure Polling event! [%s]\n", span->last_error);
			continue;
		}
		if (ZAP_SUCCESS == status) {
			zap_event_t *event;
			while (zap_span_next_event(span, &event) == ZAP_SUCCESS) {
				if (event->enum_id == ZAP_OOB_CAS_BITS_CHANGE) {
                    r2chan = R2CALL(event->channel)->r2chan;
					zap_log(ZAP_LOG_DEBUG, "Handling CAS on channel %d.\n", openr2_chan_get_number(r2chan));
					// we only expect CAS and other OOB events on this thread/loop, once a call is started
					// the MF events (in-band signaling) are handled in the call thread
					openr2_chan_process_cas_signaling(r2chan);
				} else {
					zap_log(ZAP_LOG_DEBUG, "Ignoring event %d on channel %d.\n", event->enum_id, openr2_chan_get_number(r2chan));
					// XXX TODO: handle alarms here XXX
				}
			}
		} else if (status != ZAP_TIMEOUT) {
			zap_log(ZAP_LOG_ERROR, "zap_span_poll_event returned %d.\n", status);
		} else {
			//zap_log(ZAP_LOG_DEBUG, "timed out waiting for event on span %d\n", span->span_id);
		}
	}

    /*
    FIXME: we should set BLOCKED but at this point I/O routines of openzap caused segfault
	for (i = 1; i <= span->chan_count; i++) {
		r2chan = R2CALL(span->channels[i])->r2chan;
		openr2_chan_set_blocked(r2chan);
	}
    */

	zap_clear_flag(r2data, ZAP_R2_RUNNING);
	zap_log(ZAP_LOG_DEBUG, "R2 thread ending.\n");

	return NULL;

}

static ZIO_API_FUNCTION(zap_r2_api)
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
				zap_r2_data_t *r2data = span->signal_data;

				if (span->start != zap_r2_start) {
					stream->write_function(stream, "-ERR invalid span.\n");
					goto done;
				}

				zap_clear_flag(r2data, ZAP_R2_RUNNING);
				stream->write_function(stream, "+OK killed.\n");
				goto done;
			} else {
				stream->write_function(stream, "-ERR invalid span.\n");
				goto done;
			}
		}

		if (!strcasecmp(argv[0], "status")) {
			int span_id = atoi(argv[1]);
			zap_r2_data_t *r2data = NULL;
			zap_span_t *span = NULL;
			openr2_chan_t *r2chan = NULL;
			openr2_context_t *r2context = NULL;
			int i = 0;

			if (zap_span_find_by_name(argv[1], &span) == ZAP_SUCCESS || zap_span_find(span_id, &span) == ZAP_SUCCESS) {
				if (span->start != zap_r2_start) {
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
			zap_mutex_lock(g_thread_count_mutex);
			stream->write_function(stream, "%d R2 channel threads up\n", g_thread_count);
			zap_mutex_unlock(g_thread_count_mutex);
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

	zap_safe_free(mycmd);

	return ZAP_SUCCESS;

}

static ZIO_IO_LOAD_FUNCTION(zap_r2_io_init)
{
	assert(zio != NULL);
	memset(&g_zap_r2_interface, 0, sizeof(g_zap_r2_interface));

	g_zap_r2_interface.name = "r2";
	g_zap_r2_interface.api = zap_r2_api;

	*zio = &g_zap_r2_interface;

	return ZAP_SUCCESS;
}

static ZIO_SIG_LOAD_FUNCTION(zap_r2_init)
{
	g_mod_data_hash = create_hashtable(10, zap_hash_hashfromstring, zap_hash_equalkeys);
	if (!g_mod_data_hash) {
		return ZAP_FAIL;
	}
	zap_mutex_create(&g_thread_count_mutex);
	return ZAP_SUCCESS;
}

static ZIO_SIG_UNLOAD_FUNCTION(zap_r2_destroy)
{
	zap_hash_iterator_t *i = NULL;
	zap_r2_span_pvt_t *spanpvt = NULL;
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
	zap_mutex_destroy(&g_thread_count_mutex);
	return ZAP_SUCCESS;
}

zap_module_t zap_module = { 
	"r2",
	zap_r2_io_init,
	NULL,
	zap_r2_init,
	zap_r2_configure_span,
	zap_r2_destroy
};
	

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
