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
#include "ss7_boost_client.h"
#include "zap_ss7_boost.h"

typedef uint16_t ss7_boost_request_id_t;

typedef enum {
	BST_FREE,
	BST_WAITING,
	BST_READY,
	BST_FAIL
} ss7_boost_request_status_t;

typedef struct {
	ss7_boost_request_status_t status;
	ss7bc_event_t event;
	zap_span_t *span;
	zap_channel_t *zchan;
} ss7_boost_request_t;

#define MAX_REQ_ID MAX_PENDING_CALLS

static ss7_boost_request_t OUTBOUND_REQUESTS[MAX_REQ_ID+1] = {{ 0 }};

static zap_mutex_t *request_mutex = NULL;
static zap_mutex_t *signal_mutex = NULL;

static uint8_t req_map[MAX_REQ_ID+1] = { 0 };

static void release_request_id(ss7_boost_request_id_t r)
{
	zap_mutex_lock(request_mutex);
	req_map[r] = 0;
	zap_mutex_unlock(request_mutex);
}

static ss7_boost_request_id_t next_request_id(void)
{
	ss7_boost_request_id_t r = 0;
	int ok = 0;
	
	while(!ok) {
		zap_mutex_lock(request_mutex);
		for (r = 1; r <= MAX_REQ_ID; r++) {
			if (!req_map[r]) {
				ok = 1;
				req_map[r] = 1;
				break;
			}
		}
		zap_mutex_unlock(request_mutex);
		if (!ok) {
			zap_sleep(5);
		}
	}
	return r;
}

static zap_channel_t *find_zchan(zap_span_t *span, ss7bc_event_t *event, int force)
{
	int i;
	zap_channel_t *zchan = NULL;

	zap_mutex_lock(signal_mutex);
	for(i = 0; i <= span->chan_count; i++) {
		if (span->channels[i].physical_span_id == event->span+1 && span->channels[i].physical_chan_id == event->chan+1) {
			zchan = &span->channels[i];
			if (force) {
				break;
			}
			if (zap_test_flag(zchan, ZAP_CHANNEL_INUSE) || zchan->state != ZAP_CHANNEL_STATE_DOWN) {
				if (zchan->state == ZAP_CHANNEL_STATE_DOWN || zchan->state >= ZAP_CHANNEL_STATE_TERMINATING) {
					int x = 0;
					zap_log(ZAP_LOG_WARNING, "Channel %d:%d ~ %d:%d is already in use waiting for it to become available.\n");

					zap_mutex_unlock(signal_mutex);
					for (x = 0; x < 200; x++) {
						if (!zap_test_flag(zchan, ZAP_CHANNEL_INUSE)) {
							break;
						}
						zap_sleep(5);
					}
					zap_mutex_lock(signal_mutex);
				}
				if (zap_test_flag(zchan, ZAP_CHANNEL_INUSE)) {
					zchan = NULL;
					zap_log(ZAP_LOG_ERROR, "Channel %d:%d ~ %d:%d is already in use.\n",
							span->channels[i].span_id,
							span->channels[i].chan_id,
							span->channels[i].physical_span_id,
							span->channels[i].physical_chan_id
							);
				}
			}
			break;
		}
	}
	
	zap_mutex_unlock(signal_mutex);

	return zchan;
}

static ZIO_CHANNEL_REQUEST_FUNCTION(ss7_boost_channel_request)
{
	zap_ss7_boost_data_t *ss7_boost_data = span->signal_data;
	zap_status_t status = ZAP_FAIL;
	ss7_boost_request_id_t r;
	ss7bc_event_t event = {0};
	int sanity = 60000;

	if (zap_test_flag(span, ZAP_SPAN_SUSPENDED)) {
		zap_log(ZAP_LOG_CRIT, "SPAN is not online.\n");
		*zchan = NULL;
		return ZAP_FAIL;
	}

	if (span->active_count >= span->chan_count) {
		zap_log(ZAP_LOG_CRIT, "All circuits are busy.\n");
		*zchan = NULL;
		return ZAP_FAIL;
	}

	r = next_request_id();

	ss7bc_call_init(&event, caller_data->cid_num.digits, caller_data->ani.digits, r);
	zap_set_string(event.redirection_string, caller_data->rdnis.digits);
	

	OUTBOUND_REQUESTS[r].status = BST_WAITING;
	OUTBOUND_REQUESTS[r].span = span;

	if (ss7bc_connection_write(&ss7_boost_data->mcon, &event) <= 0) {
		zap_log(ZAP_LOG_CRIT, "Failed to tx on ISUP socket [%s]\n", strerror(errno));
		status = ZAP_FAIL;
		*zchan = NULL;
		goto done;
	}

	while(zap_running() && OUTBOUND_REQUESTS[r].status == BST_WAITING) {
		zap_sleep(1);
		if (!--sanity) {
			status = ZAP_FAIL;
			*zchan = NULL;
			goto done;
		}
	}

	if (OUTBOUND_REQUESTS[r].status == BST_READY && OUTBOUND_REQUESTS[r].zchan) {
		*zchan = OUTBOUND_REQUESTS[r].zchan;
		status = ZAP_SUCCESS;
		(*zchan)->init_state = ZAP_CHANNEL_STATE_PROGRESS_MEDIA;
	} else {
		status = ZAP_FAIL;
        *zchan = NULL;
	}

 done:
	
	OUTBOUND_REQUESTS[r].status = BST_FREE;

	return status;
}

static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(ss7_boost_outgoing_call)
{
	zap_status_t status = ZAP_SUCCESS;
	return status;
}

static void handle_call_start_ack(ss7bc_connection_t *mcon, ss7bc_event_t *event)
{
	zap_channel_t *zchan;

	OUTBOUND_REQUESTS[event->call_setup_id].event = *event;

	if ((zchan = find_zchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 0))) {
		if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
			zap_log(ZAP_LOG_ERROR, "OPEN ERROR [%s]\n", zchan->last_error);
			release_request_id(event->call_setup_id);
		} else {
			zap_set_flag(zchan, ZAP_CHANNEL_OUTBOUND);
			zap_set_flag_locked(zchan, ZAP_CHANNEL_INUSE);
			zchan->extra_id = event->call_setup_id;
			OUTBOUND_REQUESTS[event->call_setup_id].zchan = zchan;
			OUTBOUND_REQUESTS[event->call_setup_id].status = BST_READY;
			return;
		}
	} 

	zap_log(ZAP_LOG_CRIT, "START ACK CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	ss7bc_exec_command(mcon,
					   event->span,
					   event->chan,
					   event->call_setup_id,
					   SIGBOOST_EVENT_CALL_STOPPED,
					   ZAP_CAUSE_DESTINATION_OUT_OF_ORDER);
	OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;
	
}

static void handle_call_done(zap_span_t *span, ss7bc_connection_t *mcon, ss7bc_event_t *event)
{
	zap_channel_t *zchan;

	if ((zchan = find_zchan(span, event, 1))) {
		
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);		
	} else {
		zap_log(ZAP_LOG_CRIT, "DONE CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	}
}

static void handle_call_start_nack(zap_span_t *span, ss7bc_connection_t *mcon, ss7bc_event_t *event)
{
	
	if (event->call_setup_id) {
		OUTBOUND_REQUESTS[event->call_setup_id].event = *event;
		OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;

		ss7bc_exec_command(mcon,
						   event->span,
						   event->chan,
						   event->call_setup_id,
						   SIGBOOST_EVENT_CALL_START_NACK_ACK,
						   0);

		release_request_id(event->call_setup_id);
	} else {
		zap_channel_t *zchan;
		if ((zchan = find_zchan(span, event, 1))) {
			assert(!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND));
			zchan->caller_data.hangup_cause = event->release_cause;
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_CANCEL);
		}
	}

}

static void handle_call_stop(zap_span_t *span, ss7bc_connection_t *mcon, ss7bc_event_t *event)
{
	zap_channel_t *zchan;
	
	if ((zchan = find_zchan(span, event, 1))) {
		zchan->caller_data.hangup_cause = event->release_cause;
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_TERMINATING);
	} else {
		zap_log(ZAP_LOG_CRIT, "STOP CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
		ss7bc_exec_command(mcon,
						   event->span,
						   event->chan,
						   0,
						   SIGBOOST_EVENT_CALL_STOPPED_ACK,
						   0);
	}
	

	
}

static void handle_call_answer(zap_span_t *span, ss7bc_connection_t *mcon, ss7bc_event_t *event)
{
	zap_channel_t *zchan;
	
	if ((zchan = find_zchan(span, event, 1))) {
		assert(zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND));
		zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_UP);
	} else {
		zap_log(ZAP_LOG_CRIT, "ANSWER CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	}
}

static void handle_call_start(zap_span_t *span, ss7bc_connection_t *mcon, ss7bc_event_t *event)
{
	zap_channel_t *zchan;

	if (!(zchan = find_zchan(span, event, 0))) {
		goto error;
	}

	if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
		goto error;
	}

	zap_set_string(zchan->caller_data.cid_num.digits, (char *)event->calling_number_digits);
	zap_set_string(zchan->caller_data.cid_name, (char *)event->calling_number_digits);
	zap_set_string(zchan->caller_data.ani.digits, (char *)event->calling_number_digits);
	zap_set_string(zchan->caller_data.dnis.digits, (char *)event->called_number_digits);
	zap_set_string(zchan->caller_data.rdnis.digits, (char *)event->redirection_string);
	zchan->caller_data.screen = event->calling_number_screening_ind;
	zchan->caller_data.pres = event->calling_number_presentation;
	zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RING);
	return;

 error:

	zap_log(ZAP_LOG_CRIT, "START CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);

	ss7bc_exec_command(mcon,
					   event->span,
					   event->chan,
					   0,
					   SIGBOOST_EVENT_CALL_START_NACK,
					   0);
		
}


static void handle_heartbeat(ss7bc_connection_t *mcon, ss7bc_event_t *event)
{
	int err;

	
	err = ss7bc_connection_write(mcon, event);
	
	if (err <= 0) {
		zap_log(ZAP_LOG_CRIT, "Failed to tx on ISUP socket [%s]: %s\n", strerror(errno));
	}
	
	mcon->hb_elapsed = 0;

    return;
}

static void handle_restart_ack(ss7bc_connection_t *mcon, zap_span_t *span, ss7bc_event_t *event)
{
    mcon->rxseq_reset = 0;
	mcon->up = 1;
	zap_set_state_all(span, ZAP_CHANNEL_STATE_RESTART);
	zap_clear_flag_locked(span, ZAP_SPAN_SUSPENDED);
	mcon->hb_elapsed = 0;
}

static int parse_ss7_event(zap_span_t *span, ss7bc_connection_t *mcon, ss7bc_event_t *event)
{
	zap_mutex_lock(signal_mutex);
	
	if (!zap_running()) {
		zap_log(ZAP_LOG_WARNING, "System is shutting down.\n");
		goto end;
	}


	if (zap_test_flag(span, ZAP_SPAN_SUSPENDED) && 
		event->event_id != SIGBOOST_EVENT_SYSTEM_RESTART_ACK && event->event_id != SIGBOOST_EVENT_HEARTBEAT) {

		zap_log(ZAP_LOG_WARNING,
				"INVALID EVENT: %s:(%X) [w%dg%d] Rc=%i CSid=%i Seq=%i Cd=[%s] Ci=[%s]\n",
				ss7bc_event_id_name(event->event_id),
				event->event_id,
				event->span+1,
				event->chan+1,
				event->release_cause,
				event->call_setup_id,
				event->fseqno,
				(event->called_number_digits_count ? (char *) event->called_number_digits : "N/A"),
				(event->calling_number_digits_count ? (char *) event->calling_number_digits : "N/A")
				);
		
		goto end;
	}


	zap_log(ZAP_LOG_DEBUG,
			"RX EVENT: %s:(%X) [w%dg%d] Rc=%i CSid=%i Seq=%i Cd=[%s] Ci=[%s]\n",
			   ss7bc_event_id_name(event->event_id),
			   event->event_id,
			   event->span+1,
			   event->chan+1,
			   event->release_cause,
			   event->call_setup_id,
			   event->fseqno,
			   (event->called_number_digits_count ? (char *) event->called_number_digits : "N/A"),
			   (event->calling_number_digits_count ? (char *) event->calling_number_digits : "N/A")
			   );


	
    switch(event->event_id) {

    case SIGBOOST_EVENT_CALL_START:
		handle_call_start(span, mcon, event);
		break;
    case SIGBOOST_EVENT_CALL_STOPPED:
		handle_call_stop(span, mcon, event);
		break;
    case SIGBOOST_EVENT_CALL_START_ACK:
		handle_call_start_ack(mcon, event);
		break;
    case SIGBOOST_EVENT_CALL_START_NACK:
		handle_call_start_nack(span, mcon, event);
		break;
    case SIGBOOST_EVENT_CALL_ANSWERED:
		handle_call_answer(span, mcon, event);
		break;
    case SIGBOOST_EVENT_HEARTBEAT:
		handle_heartbeat(mcon, event);
		break;
    case SIGBOOST_EVENT_CALL_STOPPED_ACK:
    case SIGBOOST_EVENT_CALL_START_NACK_ACK:
		handle_call_done(span, mcon, event);
		break;
    case SIGBOOST_EVENT_INSERT_CHECK_LOOP:
		//handle_call_loop_start(event);
		break;
    case SIGBOOST_EVENT_REMOVE_CHECK_LOOP:
		//handle_call_stop(event);
		break;
    case SIGBOOST_EVENT_SYSTEM_RESTART_ACK:
		handle_restart_ack(mcon, span, event);
		break;
    case SIGBOOST_EVENT_AUTO_CALL_GAP_ABATE:
		//handle_gap_abate(event);
		break;
    default:
		zap_log(ZAP_LOG_WARNING, "No handler implemented for [%s]\n", ss7bc_event_id_name(event->event_id));
		break;
    }

 end:

	zap_mutex_unlock(signal_mutex);

	return 0;
}

static __inline__ void state_advance(zap_channel_t *zchan)
{

	zap_ss7_boost_data_t *ss7_boost_data = zchan->span->signal_data;
	ss7bc_connection_t *mcon = &ss7_boost_data->mcon;
	zap_sigmsg_t sig;
	zap_status_t status;

	zap_log(ZAP_LOG_DEBUG, "%d:%d STATE [%s]\n", zchan->span_id, zchan->chan_id, zap_channel_state2str(zchan->state));
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = zchan->chan_id;
	sig.span_id = zchan->span_id;
	sig.channel = zchan;

	switch (zchan->state) {
	case ZAP_CHANNEL_STATE_DOWN:
		{
			if (zchan->extra_id) {
				release_request_id((ss7_boost_request_id_t)zchan->extra_id);
				zchan->extra_id = 0;
			}
			zap_channel_done(zchan);			
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS_MEDIA:
	case ZAP_CHANNEL_STATE_PROGRESS:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS_MEDIA;
				if ((status = ss7_boost_data->signal_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				ss7bc_exec_command(mcon,
								   zchan->physical_span_id-1,
								   zchan->physical_chan_id-1,								   
								   0,
								   SIGBOOST_EVENT_CALL_START_ACK,
								   0);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RING:
		{
			if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_START;
				if ((status = ss7_boost_data->signal_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			}

		}
		break;
	case ZAP_CHANNEL_STATE_RESTART:
		{
			if (zchan->last_state != ZAP_CHANNEL_STATE_HANGUP && zchan->last_state != ZAP_CHANNEL_STATE_DOWN) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
			} else {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_UP:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_UP;
				if ((status = ss7_boost_data->signal_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!(zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS) || zap_test_flag(zchan, ZAP_CHANNEL_MEDIA))) {
					ss7bc_exec_command(mcon,
									   zchan->physical_span_id-1,
									   zchan->physical_chan_id-1,								   
									   0,
									   SIGBOOST_EVENT_CALL_START_ACK,
									   0);
				}
				
				ss7bc_exec_command(mcon,
								   zchan->physical_span_id-1,
								   zchan->physical_chan_id-1,								   
								   0,
								   SIGBOOST_EVENT_CALL_ANSWERED,
								   0);
			}
		}
		break;
	case ZAP_CHANNEL_STATE_DIALING:
		{
		}
		break;
	case ZAP_CHANNEL_STATE_HANGUP_COMPLETE:
		{
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
		}
		break;
	case ZAP_CHANNEL_STATE_HANGUP:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_ANSWERED) || zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS) || zap_test_flag(zchan, ZAP_CHANNEL_MEDIA)) {
				ss7bc_exec_command(mcon,
								   zchan->physical_span_id-1,
								   zchan->physical_chan_id-1,
								   0,
								   SIGBOOST_EVENT_CALL_STOPPED,
								   zchan->caller_data.hangup_cause);
			} else {
				ss7bc_exec_command(mcon,
								   zchan->physical_span_id-1,
								   zchan->physical_chan_id-1,								   
								   0,
								   SIGBOOST_EVENT_CALL_START_NACK,
								   zchan->caller_data.hangup_cause);
			}			
		}
		break;
	case ZAP_CHANNEL_STATE_CANCEL:
		{
			sig.event_id = ZAP_SIGEVENT_STOP;
			status = ss7_boost_data->signal_cb(&sig);
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
			ss7bc_exec_command(mcon,
							   zchan->physical_span_id-1,
							   zchan->physical_chan_id-1,
							   0,
							   SIGBOOST_EVENT_CALL_START_NACK_ACK,
							   0);
		}
		break;
	case ZAP_CHANNEL_STATE_TERMINATING:
		{
			sig.event_id = ZAP_SIGEVENT_STOP;
			status = ss7_boost_data->signal_cb(&sig);
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
			ss7bc_exec_command(mcon,
							   zchan->physical_span_id-1,
							   zchan->physical_chan_id-1,
							   0,
							   SIGBOOST_EVENT_CALL_STOPPED_ACK,
							   0);
		}
		break;
	default:
		break;
	}
}

static __inline__ void init_outgoing_array(void)
{
	memset(&OUTBOUND_REQUESTS, 0, sizeof(OUTBOUND_REQUESTS));
}

static __inline__ void check_state(zap_span_t *span)
{
    if (zap_test_flag(span, ZAP_SPAN_STATE_CHANGE)) {
        uint32_t j;
        zap_clear_flag_locked(span, ZAP_SPAN_STATE_CHANGE);
        for(j = 1; j <= span->chan_count; j++) {
            if (zap_test_flag((&span->channels[j]), ZAP_CHANNEL_STATE_CHANGE)) {
                zap_clear_flag_locked((&span->channels[j]), ZAP_CHANNEL_STATE_CHANGE);
                state_advance(&span->channels[j]);
                zap_channel_complete_state(&span->channels[j]);
            }
        }
    }
}


static void *zap_ss7_boost_run(zap_thread_t *me, void *obj)
{
    zap_span_t *span = (zap_span_t *) obj;
    zap_ss7_boost_data_t *ss7_boost_data = span->signal_data;
	ss7bc_connection_t *mcon, *pcon;
	uint32_t ms = 10, too_long = 60000;
		

	ss7_boost_data->pcon = ss7_boost_data->mcon;

	if (ss7bc_connection_open(&ss7_boost_data->mcon,
							  ss7_boost_data->mcon.cfg.local_ip,
							  ss7_boost_data->mcon.cfg.local_port,
							  ss7_boost_data->mcon.cfg.remote_ip,
							  ss7_boost_data->mcon.cfg.remote_port) < 0) {
		zap_log(ZAP_LOG_DEBUG, "Error: Opening MCON Socket [%d] %s\n", ss7_boost_data->mcon.socket, strerror(errno));
		goto end;
    }

	if (ss7bc_connection_open(&ss7_boost_data->pcon,
							  ss7_boost_data->pcon.cfg.local_ip,
							  ++ss7_boost_data->pcon.cfg.local_port,
							  ss7_boost_data->pcon.cfg.remote_ip,
							  ss7_boost_data->pcon.cfg.remote_port) < 0) {
		zap_log(ZAP_LOG_DEBUG, "Error: Opening PCON Socket [%d] %s\n", ss7_boost_data->pcon.socket, strerror(errno));
		goto end;
    }
	
	mcon = &ss7_boost_data->mcon;
	pcon = &ss7_boost_data->pcon;

	top:

	init_outgoing_array();		

	ss7bc_exec_command(mcon,
					   0,
					   0,
					   -1,
					   SIGBOOST_EVENT_SYSTEM_RESTART,
					   0);
	
	while (zap_test_flag(ss7_boost_data, ZAP_SS7_BOOST_RUNNING)) {
		fd_set rfds, efds;
		struct timeval tv = { 0, ms * 1000 };
		int max, activity, i = 0;
		ss7bc_event_t *event = NULL;
		
		if (!zap_running()) {
			ss7bc_exec_command(mcon,
							   0,
							   0,
							   -1,
							   SIGBOOST_EVENT_SYSTEM_RESTART,
							   0);
			break;
		}

		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(mcon->socket, &rfds);
		FD_SET(mcon->socket, &efds);
		FD_SET(pcon->socket, &rfds);
		FD_SET(pcon->socket, &efds);

		max = ((pcon->socket > mcon->socket) ? pcon->socket : mcon->socket) + 1;
		
		if ((activity = select(max, &rfds, NULL, &efds, &tv)) < 0) {
			goto error;
		}
		
		if (activity) {
			if (FD_ISSET(pcon->socket, &efds) || FD_ISSET(mcon->socket, &efds)) {
				goto error;
			}

			if (FD_ISSET(pcon->socket, &rfds)) {
				if ((event = ss7bc_connection_readp(pcon, i))) {
					parse_ss7_event(span, mcon, event);
				} else goto top;
			}

			if (FD_ISSET(mcon->socket, &rfds)) {
				if ((event = ss7bc_connection_read(mcon, i))) {
					parse_ss7_event(span, mcon, event);
				} else goto top;
			}
		}
		
		check_state(span);
		mcon->hb_elapsed += ms;
		
		if (mcon->hb_elapsed >= too_long && (mcon->up || !zap_test_flag(span, ZAP_SPAN_SUSPENDED))) {
			zap_set_state_all(span, ZAP_CHANNEL_STATE_RESTART);
			zap_set_flag_locked(span, ZAP_SPAN_SUSPENDED);
			mcon->up = 0;
			zap_log(ZAP_LOG_CRIT, "Lost Heartbeat!\n");
		}

	}

	goto end;

 error:
	zap_log(ZAP_LOG_CRIT, "Socket Error!\n");

 end:

	ss7bc_connection_close(&ss7_boost_data->mcon);
	ss7bc_connection_close(&ss7_boost_data->pcon);

	zap_clear_flag(ss7_boost_data, ZAP_SS7_BOOST_RUNNING);

	zap_log(ZAP_LOG_DEBUG, "SS7_BOOST thread ended.\n");
	return NULL;
}

zap_status_t zap_ss7_boost_init(void)
{
	zap_mutex_create(&request_mutex);
	zap_mutex_create(&signal_mutex);

	return ZAP_SUCCESS;
}

zap_status_t zap_ss7_boost_start(zap_span_t *span)
{
	zap_ss7_boost_data_t *ss7_boost_data = span->signal_data;
	zap_set_flag(ss7_boost_data, ZAP_SS7_BOOST_RUNNING);
	return zap_thread_create_detached(zap_ss7_boost_run, span);
}

zap_status_t zap_ss7_boost_configure_span(zap_span_t *span,
										  const char *local_ip, int local_port, 
										  const char *remote_ip, int remote_port,
										  zio_signal_cb_t sig_cb)
{
	zap_ss7_boost_data_t *ss7_boost_data = NULL;
	
	if (!local_ip && local_port && remote_ip && remote_port && sig_cb) {
		return ZAP_FAIL;
	}

	ss7_boost_data = malloc(sizeof(*ss7_boost_data));
	assert(ss7_boost_data);
	memset(ss7_boost_data, 0, sizeof(*ss7_boost_data));
	
	zap_set_string(ss7_boost_data->mcon.cfg.local_ip, local_ip);
	ss7_boost_data->mcon.cfg.local_port = local_port;
	zap_set_string(ss7_boost_data->mcon.cfg.remote_ip, remote_ip);
	ss7_boost_data->mcon.cfg.remote_port = remote_port;
	ss7_boost_data->signal_cb = sig_cb;

	span->signal_data = ss7_boost_data;
    span->signal_type = ZAP_SIGTYPE_SS7BOOST;
    span->outgoing_call = ss7_boost_outgoing_call;
	span->channel_request = ss7_boost_channel_request;
	zap_set_flag_locked(span, ZAP_SPAN_SUSPENDED);

	return ZAP_SUCCESS;
}

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
