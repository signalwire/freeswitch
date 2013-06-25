/*
 * Copyright (c) 2007-2012, Anthony Minessale II
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
#include "sangoma_boost_client.h" 
#include "zap_sangoma_boost.h"
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifndef __WINDOWS__
#include <poll.h>
#endif

#define MAX_TRUNK_GROUPS 64
static time_t congestion_timeouts[MAX_TRUNK_GROUPS];

/**
 * \brief Strange flag
 */
typedef enum {
	SFLAG_FREE_REQ_ID = (1 << 0),
	SFLAG_SENT_FINAL_MSG = (1 << 1),
	SFLAG_SENT_ACK = (1 << 2),
	SFLAG_RECVD_ACK = (1 << 3),
	SFLAG_HANGUP = (1 << 4),
	SFLAG_TERMINATING = (1 << 5)
} sflag_t;

typedef uint16_t sangoma_boost_request_id_t;

/**
 * \brief SANGOMA boost request status
 */
typedef enum {
	BST_FREE,
	BST_WAITING,
	BST_ACK,
	BST_READY,
	BST_FAIL
} sangoma_boost_request_status_t;

/**
 * \brief SANGOMA boost request structure
 */
typedef struct {
	sangoma_boost_request_status_t status;
	sangomabc_short_event_t event;
	zap_span_t *span;
	zap_channel_t *zchan;
	int hangup_cause;
	int flags;
} sangoma_boost_request_t;

//#define MAX_REQ_ID ZAP_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN * ZAP_MAX_CHANNELS_PHYSICAL_SPAN
#define MAX_REQ_ID 6000

static uint16_t SETUP_GRID[ZAP_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN+1][ZAP_MAX_CHANNELS_PHYSICAL_SPAN+1] = {{ 0 }};

static sangoma_boost_request_t OUTBOUND_REQUESTS[MAX_REQ_ID+1] = {{ 0 }};

static zap_mutex_t *request_mutex = NULL;

static uint8_t req_map[MAX_REQ_ID+1] = { 0 };
static uint8_t nack_map[MAX_REQ_ID+1] = { 0 };

/**
 * \brief Releases span and channel from setup grid
 * \param span Span number
 * \param chan Channel number
 * \param func Calling function
 * \param line Line number on request
 * \return NULL if not found, channel otherwise
 */
static void __release_request_id_span_chan(int span, int chan, const char *func, int line)
{
	int id;

	zap_mutex_lock(request_mutex);
	if ((id = SETUP_GRID[span][chan])) {
		assert(id <= MAX_REQ_ID);
		req_map[id] = 0;
		SETUP_GRID[span][chan] = 0;
	}
	zap_mutex_unlock(request_mutex);
}
#define release_request_id_span_chan(s, c) __release_request_id_span_chan(s, c, __FUNCTION__, __LINE__)

/**
 * \brief Releases request ID
 * \param func Calling function
 * \param line Line number on request
 * \return NULL if not found, channel otherwise
 */
static void __release_request_id(sangoma_boost_request_id_t r, const char *func, int line)
{
	assert(r <= MAX_REQ_ID);
	zap_mutex_lock(request_mutex);
	req_map[r] = 0;
	zap_mutex_unlock(request_mutex);
}
#define release_request_id(r) __release_request_id(r, __FUNCTION__, __LINE__)

static sangoma_boost_request_id_t last_req = 0;

/**
 * \brief Gets the first available tank request ID
 * \param func Calling function
 * \param line Line number on request
 * \return 0 on failure, request ID on success
 */
static sangoma_boost_request_id_t __next_request_id(const char *func, int line)
{
	sangoma_boost_request_id_t r = 0, i = 0;
	int found=0;
	
	zap_mutex_lock(request_mutex);
	//r = ++last_req;
	//while(!r || req_map[r]) {

	for (i=1; i<= MAX_REQ_ID; i++){
		r = ++last_req;

		if (r >= MAX_REQ_ID) {
			r = i = last_req = 1;
		}

		if (req_map[r]) {
			/* Busy find another */
			continue;

		}

		req_map[r] = 1;
		found=1;
		break;

	}

	zap_mutex_unlock(request_mutex);

	if (!found) {
		return 0;
	}

	return r;
}
#define next_request_id() __next_request_id(__FUNCTION__, __LINE__)

/**
 * \brief Finds the channel that triggered an event
 * \param span Span where to search the channel
 * \param event SANGOMA event
 * \param force Do not wait for the channel to be available if in use
 * \return NULL if not found, channel otherwise
 */
static zap_channel_t *find_zchan(zap_span_t *span, sangomabc_short_event_t *event, int force)
{
	int i;
	zap_channel_t *zchan = NULL;

	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->physical_span_id == event->span+1 && span->channels[i]->physical_chan_id == event->chan+1) {
			zchan = span->channels[i];
			if (force || (zchan->state == ZAP_CHANNEL_STATE_DOWN && !zap_test_flag(zchan, ZAP_CHANNEL_INUSE))) {
				break;
			} else {
				zchan = NULL;
				zap_log(ZAP_LOG_DEBUG, "Channel %d:%d ~ %d:%d is already in use.\n",
						span->channels[i]->span_id,
						span->channels[i]->chan_id,
						span->channels[i]->physical_span_id,
						span->channels[i]->physical_chan_id
						);
				break;
			}
		}
	}

	return zchan;
}

static int check_congestion(int trunk_group)
{
	if (congestion_timeouts[trunk_group]) {
		time_t now = time(NULL);

		if (now >= congestion_timeouts[trunk_group]) {
			congestion_timeouts[trunk_group] = 0;
		} else {
			return 1;
		}
	}

	return 0;
}


/**
 * \brief Requests an sangoma boost channel on a span (outgoing call)
 * \param span Span where to get a channel
 * \param chan_id Specific channel to get (0 for any)
 * \param direction Call direction
 * \param caller_data Caller information
 * \param zchan Channel to initialise
 * \return Success or failure
 */
static ZIO_CHANNEL_REQUEST_FUNCTION(sangoma_boost_channel_request)
{
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	zap_status_t status = ZAP_FAIL;
	sangoma_boost_request_id_t r;
	sangomabc_event_t event = {0};

		/* sanity has to be more than 8 seconds.
	 * In PRI specs, timeout is 4 seconds for remote switch to respond to a SETUP,
	 * and PRI stack will retransmit a second SETUP after the first timeout, so
	 * we should allow for at least 8 seconds */

	int sanity = 10000;
	sangoma_boost_request_status_t st;
	char ani[128] = "";
	char *gr = NULL;
	uint32_t count = 0;
	int tg=0;
	
	if (zap_test_flag(span, ZAP_SPAN_SUSPENDED)) {
		zap_log(ZAP_LOG_CRIT, "SPAN is not online.\n");
		*zchan = NULL;
		return ZAP_FAIL;
	}
	
	zap_set_string(ani, caller_data->ani.digits);

	if ((gr = strchr(ani, '@'))) {
		*gr++ = '\0';
	}

	if (gr && *(gr+1)) {
		tg = atoi(gr+1);
		if (tg > 0) {
			tg--;
		}
	}
	event.trunk_group = tg;

	if (check_congestion(tg)) {
		zap_log(ZAP_LOG_CRIT, "All circuits are busy. Trunk Group=%i (BOOST REQUESTED BACK OFF)\n",tg+1);
		*zchan = NULL;
		return ZAP_FAIL;
	}

	zap_span_channel_use_count(span, &count);

	if (count >= span->chan_count) {
		zap_log(ZAP_LOG_CRIT, "All circuits are busy.\n");
		*zchan = NULL;
		return ZAP_FAIL;
	}

	r = next_request_id();
	if (r == 0) {
		zap_log(ZAP_LOG_CRIT, "All tanks ids are busy.\n");
		*zchan = NULL;
		return ZAP_FAIL;
	}

	/* sangomabc_call_init (event, calling, called, setup_id) */
	sangomabc_call_init(&event, caller_data->cid_num.digits, ani, r);
	//sangoma_bc_call_init will clear the trunk_group val so we need to set it again	
	event.trunk_group=tg;
	
	if (gr && *(gr+1)) {

		switch(*gr) {
        case 'g':
            event.hunt_group = SIGBOOST_HUNTGRP_SEQ_ASC;
            break;
        case 'G':
            event.hunt_group = SIGBOOST_HUNTGRP_SEQ_DESC;
            break;
        case 'r':
            event.hunt_group = SIGBOOST_HUNTGRP_RR_ASC;
            break;
        case 'R':
            event.hunt_group = SIGBOOST_HUNTGRP_RR_DESC;
            break;
        default:
			zap_log(ZAP_LOG_WARNING, "Failed to determine huntgroup (%s)\n", gr);
            event.hunt_group = SIGBOOST_HUNTGRP_SEQ_ASC;
		}
	}

	zap_set_string(event.calling_name, caller_data->cid_name);
	zap_set_string(event.rdnis.digits, caller_data->rdnis.digits);

	if (strlen(caller_data->rdnis.digits)) {
			event.rdnis.digits_count = strlen(caller_data->rdnis.digits)+1;
			event.rdnis.ton = caller_data->rdnis.type;
			event.rdnis.npi = caller_data->rdnis.plan;
	}

	event.calling.screening_ind = caller_data->screen;
	event.calling.presentation_ind = caller_data->pres;

	event.calling.ton = caller_data->cid_num.type;
	event.calling.npi = caller_data->cid_num.plan;

	event.called.ton = caller_data->ani.type;
	event.called.npi = caller_data->ani.plan;

	if (caller_data->raw_data_len) {
		zap_set_string((char *)event.isup_in_rdnis, (char *)caller_data->raw_data);
		event.isup_in_rdnis_size = caller_data->raw_data_len;
	}

	OUTBOUND_REQUESTS[r].status = BST_WAITING;
	OUTBOUND_REQUESTS[r].span = span;

	if (sangomabc_connection_write(&sangoma_boost_data->mcon, &event) <= 0) {
		zap_log(ZAP_LOG_CRIT, "Failed to tx on ISUP socket [%s]\n", strerror(errno));
		status = ZAP_FAIL;
		OUTBOUND_REQUESTS[r].status = ZAP_FAIL;
		*zchan = NULL;
		goto done;
	}

	while(zap_running() && OUTBOUND_REQUESTS[r].status == BST_WAITING) {
		zap_sleep(1);
		if (--sanity <= 0) {
			status = ZAP_FAIL;	
			*zchan = NULL;
			goto done;
		}
	}
	
	if (OUTBOUND_REQUESTS[r].status == BST_READY && OUTBOUND_REQUESTS[r].zchan) {
		*zchan = OUTBOUND_REQUESTS[r].zchan;
		status = ZAP_SUCCESS;
	} else {
		status = ZAP_FAIL;
        *zchan = NULL;
	}

 done:

	st = OUTBOUND_REQUESTS[r].status;
	OUTBOUND_REQUESTS[r].status = BST_FREE;	

	if (status == ZAP_FAIL) {
		if (st == BST_FAIL) {
			caller_data->hangup_cause = OUTBOUND_REQUESTS[r].hangup_cause;
		} else {
			caller_data->hangup_cause = ZAP_CAUSE_RECOVERY_ON_TIMER_EXPIRE;
		}
	}

	if (st == BST_FAIL) {
		release_request_id(r);
	} else if (st != BST_READY) {
		assert(r <= MAX_REQ_ID);
		nack_map[r] = 1;
		sangomabc_exec_command(&sangoma_boost_data->mcon,
							   0,
							   0,
							   r,
							   SIGBOOST_EVENT_CALL_START_NACK,
							   0,
							   0);
	}

	return status;
}

/**
 * \brief Starts an sangoma boost channel (outgoing call)
 * \param zchan Channel to initiate call on
 * \return Success
 */
static ZIO_CHANNEL_OUTGOING_CALL_FUNCTION(sangoma_boost_outgoing_call)
{
	zap_status_t status = ZAP_SUCCESS;

	return status;
}

/**
 * \brief Handler for call start ack no media event
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_progress(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_channel_t *zchan;


	if ((zchan = find_zchan(span, event, 1))) {
		zap_mutex_lock(zchan->mutex);
		if (zchan->state == ZAP_CHANNEL_STATE_HOLD) {
			if ((event->flags & SIGBOOST_PROGRESS_MEDIA)) {
				zchan->init_state = ZAP_CHANNEL_STATE_PROGRESS_MEDIA;
				zap_log(ZAP_LOG_DEBUG, "Channel init state updated to PROGRESS_MEDIA [Csid:%d]\n", event->call_setup_id);
			} else if ((event->flags & SIGBOOST_PROGRESS_RING)) {
				zchan->init_state = ZAP_CHANNEL_STATE_PROGRESS;
				zap_log(ZAP_LOG_DEBUG, "Channel init state updated to PROGRESS [Csid:%d]\n", event->call_setup_id);
			} else {
				zchan->init_state = ZAP_CHANNEL_STATE_IDLE;
				zap_log(ZAP_LOG_DEBUG, "Channel init state updated to IDLE [Csid:%d]\n", event->call_setup_id);
			}			
		} else {
			if ((event->flags & SIGBOOST_PROGRESS_MEDIA)) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_PROGRESS_MEDIA);
			} else if ((event->flags & SIGBOOST_PROGRESS_RING)) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_PROGRESS);
			} else {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_IDLE);
			}
		}
		zap_mutex_unlock(zchan->mutex);
	}
}

/**
 * \brief Handler for call start ack event
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_start_ack(sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_channel_t *zchan;

	if (nack_map[event->call_setup_id]) {
		return;
	}



	if ((zchan = find_zchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 0))) {
		if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
			zap_log(ZAP_LOG_ERROR, "OPEN ERROR [%s]\n", zchan->last_error);
		} else {

			/* Only bind the setup id to GRID when we are sure that channel is ready
			   otherwise we could overwite the original call */
			OUTBOUND_REQUESTS[event->call_setup_id].event = *event;
			SETUP_GRID[event->span][event->chan] = event->call_setup_id;

			zap_set_flag(zchan, ZAP_CHANNEL_OUTBOUND);
			zap_set_flag_locked(zchan, ZAP_CHANNEL_INUSE);
			zchan->extra_id = event->call_setup_id;
			zap_log(ZAP_LOG_DEBUG, "Assign chan %d:%d (%d:%d) CSid=%d\n", zchan->span_id, zchan->chan_id, event->span+1,event->chan+1, event->call_setup_id);
			zchan->sflags = SFLAG_RECVD_ACK;

			if ((event->flags & SIGBOOST_PROGRESS_MEDIA)) {
				zchan->init_state = ZAP_CHANNEL_STATE_PROGRESS_MEDIA;
				zap_log(ZAP_LOG_DEBUG, "Channel init state changed to PROGRESS_MEDIA [Csid:%d]\n", event->call_setup_id);
			} else if ((event->flags & SIGBOOST_PROGRESS_RING)) {
				zchan->init_state = ZAP_CHANNEL_STATE_PROGRESS;
				zap_log(ZAP_LOG_DEBUG, "Channel init state changed to PROGRESS [Csid:%d]\n", event->call_setup_id);
			} else {
				zchan->init_state = ZAP_CHANNEL_STATE_IDLE;
				zap_log(ZAP_LOG_DEBUG, "Channel init state changed to IDLE [Csid:%d]\n", event->call_setup_id);
			}
			
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HOLD);

			OUTBOUND_REQUESTS[event->call_setup_id].flags = event->flags;
			OUTBOUND_REQUESTS[event->call_setup_id].zchan = zchan;
			OUTBOUND_REQUESTS[event->call_setup_id].status = BST_READY;
			return;
		}
	} else {
		if ((zchan = find_zchan(OUTBOUND_REQUESTS[event->call_setup_id].span, (sangomabc_short_event_t*)event, 1))) {
			int r;
			
			/* NC: If we get CALL START ACK and channel is in active state
			       then we are completely out of sync with the other end.
			       Treat CALL START ACK as CALL STOP and hangup the current call.
			*/
			
			if (zchan->state == ZAP_CHANNEL_STATE_UP || 
				zchan->state == ZAP_CHANNEL_STATE_PROGRESS_MEDIA ||
			    zchan->state == ZAP_CHANNEL_STATE_PROGRESS) {
				zap_log(ZAP_LOG_CRIT, "ZCHAN CALL ACK STATE %s -> Changed to HANGUP %d:%d\n",
						zap_channel_state2str(zchan->state),event->span+1,event->chan+1);
				zap_set_state_r(zchan, ZAP_CHANNEL_STATE_TERMINATING, 0, r);

			} else if (zchan->state == ZAP_CHANNEL_STATE_HANGUP || zap_test_sflag(zchan, SFLAG_HANGUP)) {
				zap_log(ZAP_LOG_CRIT, "ZCHAN CALL ACK STATE HANGUP -> Changed to HANGUP %d:%d\n", event->span+1,event->chan+1);
				zap_set_state_r(zchan, ZAP_CHANNEL_STATE_HANGUP_COMPLETE, 0, r);
				/* Do nothing because outgoing STOP will generaate a stop ack */

			} else {
				zap_log(ZAP_LOG_CRIT, "ZCHAN CALL ACK STATE INVALID %s  s%dc%d\n",
						zap_channel_state2str(zchan->state),event->span+1,event->chan+1);
			}
			zap_set_sflag(zchan, SFLAG_SENT_FINAL_MSG);
			zchan=NULL;
		}
	}
	
	//printf("WTF BAD ACK CSid=%d span=%d chan=%d\n", event->call_setup_id, event->span+1,event->chan+1);
	if ((zchan = find_zchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 1))) {
		//printf("WTF BAD ACK2 %d:%d (%d:%d) CSid=%d xtra_id=%d out=%d state=%s\n", zchan->span_id, zchan->chan_id, event->span+1,event->chan+1, event->call_setup_id, zchan->extra_id, zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND), zap_channel_state2str(zchan->state));
	}

	if (zchan) {
		zap_set_sflag(zchan, SFLAG_SENT_FINAL_MSG);
	}

	zap_log(ZAP_LOG_CRIT, "START ACK CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	sangomabc_exec_command(mcon,
						   event->span,
						   event->chan,
						   event->call_setup_id,
						   SIGBOOST_EVENT_CALL_STOPPED,
						   ZAP_CAUSE_DESTINATION_OUT_OF_ORDER, 0);
	OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;
	OUTBOUND_REQUESTS[event->call_setup_id].hangup_cause = ZAP_CAUSE_DESTINATION_OUT_OF_ORDER;
	
}

/**
 * \brief Handler for call done event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_done(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_channel_t *zchan;
	int r = 0;

	if ((zchan = find_zchan(span, event, 1))) {
		zap_mutex_lock(zchan->mutex);

		if (zchan->state == ZAP_CHANNEL_STATE_DOWN || zchan->state == ZAP_CHANNEL_STATE_HANGUP_COMPLETE || zap_test_sflag(zchan, SFLAG_TERMINATING)) {
			goto done;
		}

		zap_set_state_r(zchan, ZAP_CHANNEL_STATE_HANGUP_COMPLETE, 0, r);
		if (r) {
			zap_set_sflag(zchan, SFLAG_FREE_REQ_ID);
			zap_mutex_unlock(zchan->mutex);
			return;
		}
	} 

 done:
	
	if (zchan) {
		zap_mutex_unlock(zchan->mutex);
	}

	if (event->call_setup_id) {
		release_request_id(event->call_setup_id);
	} else {
		release_request_id_span_chan(event->span, event->chan);
	}
}

/**
 * \brief Handler for call start nack event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_start_nack(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_channel_t *zchan;

	if (event->release_cause == SIGBOOST_CALL_SETUP_NACK_ALL_CKTS_BUSY) {
		uint32_t count = 0;
		int delay = 0;
		int tg=event->trunk_group;

		zap_span_channel_use_count(span, &count);

		delay = (int) (count / 100) * 2;
		
		if (delay > 10) {
			delay = 10;
		} else if (delay < 1) {
			delay = 1;
		}

		if (tg < 0 || tg >= MAX_TRUNK_GROUPS) {
			zap_log(ZAP_LOG_CRIT, "Invalid All Ckt Busy trunk group number %i\n", tg);
			tg=0;
		}
		
		congestion_timeouts[tg] = time(NULL) + delay;
		event->release_cause = 17;

	} else if (event->release_cause == SIGBOOST_CALL_SETUP_CSUPID_DBL_USE) {
		event->release_cause = 17;
	}

	if (event->call_setup_id) {

		sangomabc_exec_command(mcon,
							   0,
							   0,
							   event->call_setup_id,
							   SIGBOOST_EVENT_CALL_START_NACK_ACK,
							   0, 0);
		
		OUTBOUND_REQUESTS[event->call_setup_id].event = *event;
		OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;
		OUTBOUND_REQUESTS[event->call_setup_id].hangup_cause = event->release_cause;
		return;
	} else {
		if ((zchan = find_zchan(span, event, 1))) {
			int r = 0;
			assert(!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND));
	
			zchan->call_data = (void*)(intptr_t)event->event_id;

			zap_mutex_lock(zchan->mutex);
			zap_set_state_r(zchan, ZAP_CHANNEL_STATE_TERMINATING, 0, r);
			if (r == ZAP_STATE_CHANGE_SUCCESS) {
				zchan->caller_data.hangup_cause = event->release_cause;
			}
			zap_mutex_unlock(zchan->mutex);
			if (r) {
				return;
			}
		}
	}

	if (zchan) {
		zap_set_sflag_locked(zchan, SFLAG_SENT_FINAL_MSG);
	}

	/* nobody else will do it so we have to do it ourselves */
	sangomabc_exec_command(mcon,
						   event->span,
						   event->chan,
						   0,
						   SIGBOOST_EVENT_CALL_START_NACK_ACK,
						   0, 0);
}

/**
 * \brief Handler for call stop event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_stop(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_channel_t *zchan;
	
	if ((zchan = find_zchan(span, event, 1))) {
		int r = 0;

		zap_mutex_lock(zchan->mutex);

		if (zap_test_sflag(zchan, SFLAG_HANGUP) || zchan->state == ZAP_CHANNEL_STATE_DOWN) {
	
			/* NC: Checking for state DOWN because ss7box can
				   send CALL_STOP twice in a row.  If we do not check for
				   STATE_DOWN we will set the state back to termnating
				   and block the channel forever
			*/
	
			/* racing condition where both sides initiated a hangup 
			 * Do not change current state as channel is already clearing
			 * itself through local initiated hangup */
			
			sangomabc_exec_command(mcon,
								   zchan->physical_span_id-1,
								   zchan->physical_chan_id-1,
								   0,
								   SIGBOOST_EVENT_CALL_STOPPED_ACK,
								   0, 0);
			zap_mutex_unlock(zchan->mutex);
			return;
		} else {
			if (zchan->state == ZAP_CHANNEL_STATE_HOLD) {
				zchan->init_state = ZAP_CHANNEL_STATE_TERMINATING;
				zap_log(ZAP_LOG_DEBUG, "Channel init state updated to TERMINATING [Csid:%d]\n", event->call_setup_id);			
				OUTBOUND_REQUESTS[event->call_setup_id].hangup_cause = event->release_cause;  
				zchan->caller_data.hangup_cause = event->release_cause;
				zap_mutex_unlock(zchan->mutex);
				return;
			} else {
				zap_set_state_r(zchan, ZAP_CHANNEL_STATE_TERMINATING, 0, r);
			}
		}

		if (r == ZAP_STATE_CHANGE_SUCCESS) {
			zchan->caller_data.hangup_cause = event->release_cause;
		}

		if (r) {
			zap_set_sflag(zchan, SFLAG_FREE_REQ_ID);
		}

		zap_mutex_unlock(zchan->mutex);

		if (r) {
			return;
		}
	} /* else we have to do it ourselves.... */

	zap_log(ZAP_LOG_WARNING, "We could not find chan: s%dc%d\n", event->span, event->chan);
	release_request_id_span_chan(event->span, event->chan);
}

/**
 * \brief Handler for call answer event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_answer(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_channel_t *zchan;
	
	if ((zchan = find_zchan(span, event, 1))) {
		zap_mutex_lock(zchan->mutex);
		if (zap_test_sflag(zchan, SFLAG_HANGUP) || 
		    zchan->state == ZAP_CHANNEL_STATE_DOWN || 
			zchan->state == ZAP_CHANNEL_STATE_TERMINATING) {
			/* NC: Do nothing here because we are in process
			       of stopping the call. So ignore the ANSWER. */
			zap_log(ZAP_LOG_CRIT, "ANSWER BUT CALL IS HANGUP %d:%d\n", event->span+1,event->chan+1);

		} else if (zchan->state == ZAP_CHANNEL_STATE_HOLD) {
			zchan->init_state = ZAP_CHANNEL_STATE_UP;
		} else {
			int r = 0;
			zap_set_state_r(zchan, ZAP_CHANNEL_STATE_UP, 0, r);
		}
		zap_mutex_unlock(zchan->mutex);
	} else {
		zap_log(ZAP_LOG_CRIT, "ANSWER CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
		sangomabc_exec_command(mcon,
							   event->span,
							   event->chan,
							   event->call_setup_id,
							   SIGBOOST_EVENT_CALL_STOPPED,
							   ZAP_CAUSE_DESTINATION_OUT_OF_ORDER, 0);
	}
}

static __inline__ void advance_chan_states(zap_channel_t *zchan);
static __inline__ void stop_loop(zap_channel_t *zchan);

/**
 * \brief Handler for call start event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_start(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_event_t *event)
{
	zap_channel_t *zchan;
	int hangup_cause = ZAP_CAUSE_CALL_REJECTED;
	int retry = 1;

tryagain:

	if (!(zchan = find_zchan(span, (sangomabc_short_event_t*)event, 0))) {
		if ((zchan = find_zchan(span, (sangomabc_short_event_t*)event, 1))) {
			int r;
			
			/* NC: If we get CALL START and channel is in active state
			       then we are completely out of sync with the other end.
			       Treat CALL START as CALL STOP and hangup the current call.
			*/

			if (zchan->state == ZAP_CHANNEL_STATE_UP ||
				zchan->state == ZAP_CHANNEL_STATE_PROGRESS_MEDIA ||
			    zchan->state == ZAP_CHANNEL_STATE_PROGRESS) {
				zap_log(ZAP_LOG_CRIT, "ZCHAN CALL STATE %s -> Changed to TERMINATING %d:%d\n",
						zap_channel_state2str(zchan->state),event->span+1,event->chan+1);
				zap_set_state_r(zchan, ZAP_CHANNEL_STATE_TERMINATING, 0, r);

			} else if (zchan->state == ZAP_CHANNEL_STATE_HANGUP || zap_test_sflag(zchan, SFLAG_HANGUP)) {
				zap_log(ZAP_LOG_CRIT, "ZCHAN CALL STATE HANGUP -> Changed to HANGUP %d:%d\n", event->span+1,event->chan+1);
				zap_set_state_r(zchan, ZAP_CHANNEL_STATE_HANGUP_COMPLETE, 0, r);
				/* Do nothing because outgoing STOP will generaate a stop ack */
			} else if (zchan->state == ZAP_CHANNEL_STATE_IN_LOOP && retry) {
				/* workaround ss7box sending us call start without sending first a loop stop */
				stop_loop(zchan);
				advance_chan_states(zchan);
				retry = 0;
				goto tryagain;
			} else {
				zap_log(ZAP_LOG_CRIT, "ZCHAN CALL ACK STATE INVALID %s s%dc%d\n",
						zap_channel_state2str(zchan->state),event->span+1,event->chan+1);
			}

			zap_set_sflag(zchan, SFLAG_SENT_FINAL_MSG);
			zchan=NULL;
		}
		zap_log(ZAP_LOG_CRIT, "START CANT FIND CHAN %d:%d\n", event->span+1,event->chan+1);
		goto error;
	}

	if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_CRIT, "START CANT OPEN CHAN %d:%d\n", event->span+1,event->chan+1);
		goto error;
	}
	
	zchan->sflags = 0;
	zap_set_string(zchan->caller_data.cid_num.digits, (char *)event->calling.digits);
	zap_set_string(zchan->caller_data.cid_name, (char *)event->calling.digits);
	zap_set_string(zchan->caller_data.ani.digits, (char *)event->calling.digits);
	zap_set_string(zchan->caller_data.dnis.digits, (char *)event->called.digits);
	zap_set_string(zchan->caller_data.rdnis.digits, (char *)event->rdnis.digits);
	if (event->custom_data_size) {
		zap_set_string(zchan->caller_data.raw_data, event->custom_data);
		zchan->caller_data.raw_data_len = event->custom_data_size;
	}

	if (strlen(event->calling_name)) {
		zap_set_string(zchan->caller_data.cid_name, (char *)event->calling_name);
	}

	zchan->caller_data.cid_num.plan = event->calling.npi;
	zchan->caller_data.cid_num.type = event->calling.ton;

	zchan->caller_data.ani.plan = event->calling.npi;
	zchan->caller_data.ani.type = event->calling.ton;

	zchan->caller_data.dnis.plan = event->called.npi;
	zchan->caller_data.dnis.type = event->called.ton;

	zchan->caller_data.rdnis.plan = event->rdnis.npi;
	zchan->caller_data.rdnis.type = event->rdnis.ton;

	zchan->caller_data.screen = event->calling.screening_ind;
	zchan->caller_data.pres = event->calling.presentation_ind;

	/* more info about custom data: http://www.ss7box.com/smg_manual.html#ISUP-IN-RDNIS-NEW */
	if (event->custom_data_size) {
		char* p = NULL;

		p = strstr(event->custom_data,"PRI001-ANI2-");
		if ( p != NULL) {
			int ani2 = 0;
			sscanf(p, "PRI001-ANI2-%d", &ani2);
			snprintf(zchan->caller_data.aniII, 5, "%.2d", ani2);
		}

	}

	zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RING);
	return;

 error:
	if (zchan) {
		hangup_cause = zchan->caller_data.hangup_cause;
	} else {
		zap_log(ZAP_LOG_CRIT, "START CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
		hangup_cause = ZAP_CAUSE_REQUESTED_CHAN_UNAVAIL;
	}
	sangomabc_exec_command(mcon,
						   event->span,
						   event->chan,
						   0,
						   SIGBOOST_EVENT_CALL_START_NACK,
						   hangup_cause, 0);
		
}

static void handle_call_loop_start(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_state_change_result_t res = ZAP_FAIL;
	zap_channel_t *zchan;

	if (!(zchan = find_zchan(span, (sangomabc_short_event_t*)event, 0))) {
		zap_log(ZAP_LOG_CRIT, "CANNOT START LOOP, CHAN NOT AVAILABLE %d:%d\n", event->span+1,event->chan+1);
		return;
	}

	if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_CRIT, "CANNOT START LOOP, CANT OPEN CHAN %d:%d\n", event->span+1,event->chan+1);
		return;
	}

	zap_set_state_r(zchan, ZAP_CHANNEL_STATE_IN_LOOP, 0, res);
	if (res != ZAP_STATE_CHANGE_SUCCESS) {
		zap_log(ZAP_LOG_CRIT, "yay, could not set the state of the channel to IN_LOOP, loop will fail\n");
		zap_channel_done(zchan);
		return;
	}
	zap_log(ZAP_LOG_DEBUG, "%d:%d starting loop\n", zchan->span_id, zchan->chan_id);
	zap_channel_command(zchan, ZAP_COMMAND_ENABLE_LOOP, NULL);
}

static __inline__ void stop_loop(zap_channel_t *zchan)
{
	zap_state_change_result_t res = ZAP_STATE_CHANGE_FAIL;
	zap_channel_command(zchan, ZAP_COMMAND_DISABLE_LOOP, NULL);
	/* even when we did not sent a msg we set this flag to avoid sending call stop in the DOWN state handler */
	zap_set_flag(zchan, SFLAG_SENT_FINAL_MSG);
	zap_set_state_r(zchan, ZAP_CHANNEL_STATE_DOWN, 0, res);
	if (res != ZAP_STATE_CHANGE_SUCCESS) {
		zap_log(ZAP_LOG_CRIT, "yay, could not set the state of the channel from IN_LOOP to DOWN\n");
	}
}

static void handle_call_loop_stop(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_channel_t *zchan;
	if (!(zchan = find_zchan(span, (sangomabc_short_event_t*)event, 1))) {
		zap_log(ZAP_LOG_CRIT, "CANNOT STOP LOOP, INVALID CHAN REQUESTED %d:%d\n", event->span+1,event->chan+1);
		return;
	}
	if (zchan->state != ZAP_CHANNEL_STATE_IN_LOOP) {
		zap_log(ZAP_LOG_ERROR, "Got stop loop request in a channel that is not in loop, ignoring ...\n");
		return;
	}
	stop_loop(zchan);
}

/**
 * \brief Handler for heartbeat event
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_heartbeat(sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	int err;
	
	err = sangomabc_connection_writep(mcon, (sangomabc_event_t*)event);
	
	if (err <= 0) {
		zap_log(ZAP_LOG_CRIT, "Failed to tx on ISUP socket [%s]: %s\n", strerror(errno));
	}
	
	mcon->hb_elapsed = 0;

    return;
}

/**
 * \brief Handler for restart ack event
 * \param mcon sangoma boost connection
 * \param span Span where event was fired
 * \param event Event to handle
 */
static void handle_restart_ack(sangomabc_connection_t *mcon, zap_span_t *span, sangomabc_short_event_t *event)
{
	zap_log(ZAP_LOG_DEBUG, "RECV RESTART ACK\n");
}

/**
 * \brief Handler for restart event
 * \param mcon sangoma boost connection
 * \param span Span where event was fired
 * \param event Event to handle
 */
static void handle_restart(sangomabc_connection_t *mcon, zap_span_t *span, sangomabc_short_event_t *event)
{
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

    mcon->rxseq_reset = 0;
	zap_set_flag((&sangoma_boost_data->mcon), MSU_FLAG_DOWN);
	zap_set_flag_locked(span, ZAP_SPAN_SUSPENDED);
	zap_set_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RESTARTING);
	
	mcon->hb_elapsed = 0;
}

/**
 * \brief Handler for incoming digit event
 * \param mcon sangoma boost connection
 * \param span Span where event was fired
 * \param event Event to handle
 */
static void handle_incoming_digit(sangomabc_connection_t *mcon, zap_span_t *span, sangomabc_event_t *event)
{
	zap_channel_t *zchan = NULL;
	char digits[MAX_DIALED_DIGITS + 2] = "";
	
	if (!(zchan = find_zchan(span, (sangomabc_short_event_t *)event, 1))) {
		zap_log(ZAP_LOG_ERROR, "Invalid channel\n");
		return;
	}
	
	if (event->called_number_digits_count == 0) {
		zap_log(ZAP_LOG_WARNING, "Error Incoming digit with len %s %d [w%dg%d]\n",
			   	event->called_number_digits,
			   	event->called_number_digits_count,
			   	event->span+1, event->chan+1);
		return;
	}

	zap_log(ZAP_LOG_WARNING, "Incoming digit with len %s %d [w%dg%d]\n",
			   	event->called_number_digits,
			   	event->called_number_digits_count,
			   	event->span+1, event->chan+1);

	memcpy(digits, event->called_number_digits, event->called_number_digits_count);
	zap_channel_queue_dtmf(zchan, digits);

	return;
}


/**
 * \brief Checks if span has state changes pending and processes 
 * \param span Span where event was fired
 * \param event Event to handle
 */
static zap_channel_t* event_process_states(zap_span_t *span, sangomabc_short_event_t *event) 
{
    zap_channel_t *zchan = NULL;
    
    switch (event->event_id) {
        case SIGBOOST_EVENT_CALL_START_NACK:
        case SIGBOOST_EVENT_CALL_START_NACK_ACK:
            if (event->call_setup_id) {
                return NULL;
            } 
            //if event->span and event->chan is valid, fall-through
        case SIGBOOST_EVENT_CALL_START:
        case SIGBOOST_EVENT_CALL_START_ACK:
        case SIGBOOST_EVENT_CALL_STOPPED:
        case SIGBOOST_EVENT_CALL_PROGRESS:
        case SIGBOOST_EVENT_CALL_ANSWERED:
        case SIGBOOST_EVENT_CALL_STOPPED_ACK:
        case SIGBOOST_EVENT_DIGIT_IN:
        case SIGBOOST_EVENT_INSERT_CHECK_LOOP:
        case SIGBOOST_EVENT_REMOVE_CHECK_LOOP:
            if (!(zchan = find_zchan(span, (sangomabc_short_event_t*)event, 1))) {
                zap_log(ZAP_LOG_DEBUG, "PROCESS STATES  CANT FIND CHAN %d:%d\n", event->span+1,event->chan+1);
                return NULL;
            }
            break;
        case SIGBOOST_EVENT_HEARTBEAT:
        case SIGBOOST_EVENT_SYSTEM_RESTART_ACK:
        case SIGBOOST_EVENT_SYSTEM_RESTART:
        case SIGBOOST_EVENT_AUTO_CALL_GAP_ABATE:
            return NULL;
        default:
            zap_log(ZAP_LOG_CRIT, "Unhandled event id:%d\n", event->event_id);
            return NULL;
    }

    zap_mutex_lock(zchan->mutex);
    advance_chan_states(zchan);
    return zchan;
}

/**
 * \brief Handler for sangoma boost event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static int parse_sangoma_event(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
    zap_channel_t* zchan = NULL;
	
	if (!zap_running()) {
		zap_log(ZAP_LOG_WARNING, "System is shutting down.\n");
		goto end;
	}

	assert(event->call_setup_id <= MAX_REQ_ID);

    /* process all pending state changes for that channel before
        processing the new boost event */
    zchan = event_process_states(span, event);

    switch(event->event_id) {
    case SIGBOOST_EVENT_CALL_START:
		handle_call_start(span, mcon, (sangomabc_event_t*)event);
		break;
    case SIGBOOST_EVENT_CALL_STOPPED:
		handle_call_stop(span, mcon, event);
		break;
    case SIGBOOST_EVENT_CALL_START_ACK:
		handle_call_start_ack(mcon, event);
		break;
    case SIGBOOST_EVENT_CALL_PROGRESS:
		handle_call_progress(span, mcon, event);
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
		handle_call_done(span, mcon, event);
		break;
    case SIGBOOST_EVENT_CALL_START_NACK_ACK:
		handle_call_done(span, mcon, event);
		nack_map[event->call_setup_id] = 0;
		break;
    case SIGBOOST_EVENT_INSERT_CHECK_LOOP:
		handle_call_loop_start(span, mcon, event);
		break;
    case SIGBOOST_EVENT_REMOVE_CHECK_LOOP:
		handle_call_loop_stop(span, mcon, event);
		break;
    case SIGBOOST_EVENT_SYSTEM_RESTART_ACK:
		handle_restart_ack(mcon, span, event);
		break;
	case SIGBOOST_EVENT_SYSTEM_RESTART:
		handle_restart(mcon, span, event);
		break;
    case SIGBOOST_EVENT_AUTO_CALL_GAP_ABATE:
		//handle_gap_abate(event);
		break;
	case SIGBOOST_EVENT_DIGIT_IN:
		handle_incoming_digit(mcon, span, (sangomabc_event_t*)event);
		break;
    default:
		zap_log(ZAP_LOG_WARNING, "No handler implemented for [%s]\n", sangomabc_event_id_name(event->event_id));
		break;
    }

 end:
    if(zchan != NULL) {
        advance_chan_states(zchan);
        zap_mutex_unlock(zchan->mutex);
    }

	return 0;
}

/**
 * \brief Handler for channel state change
 * \param zchan Channel to handle
 */
static __inline__ void state_advance(zap_channel_t *zchan)
{

	zap_sangoma_boost_data_t *sangoma_boost_data = zchan->span->signal_data;
	sangomabc_connection_t *mcon = &sangoma_boost_data->mcon;
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
			if (zchan->last_state == ZAP_CHANNEL_STATE_IN_LOOP) {
				/* nothing to do after a loop */
				zap_log(ZAP_LOG_DEBUG, "%d:%d terminating loop\n", zchan->span_id, zchan->chan_id);
			} else {
				/* Always try to clear the GRID */
				release_request_id_span_chan(zchan->physical_span_id-1, zchan->physical_chan_id-1);

				if (!zap_test_sflag(zchan, SFLAG_SENT_FINAL_MSG)) {
					zap_set_sflag_locked(zchan, SFLAG_SENT_FINAL_MSG);

					if (zchan->call_data && ((uint32_t)(intptr_t)zchan->call_data == SIGBOOST_EVENT_CALL_START_NACK)) {
						sangomabc_exec_command(mcon,
											   zchan->physical_span_id-1,
											   zchan->physical_chan_id-1,
											   0,
											   SIGBOOST_EVENT_CALL_START_NACK_ACK,
											   0, 0);
						
					} else {
						sangomabc_exec_command(mcon,
											   zchan->physical_span_id-1,
											   zchan->physical_chan_id-1,
											   0,
											   SIGBOOST_EVENT_CALL_STOPPED_ACK,
											   0, 0);
					}
				}
			} 
			zchan->extra_id = 0;
			zchan->sflags = 0;
			zchan->call_data = NULL;
			zap_channel_done(zchan);
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS_MEDIA;
				if ((status = zap_span_send_signal(zchan->span, &sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!zap_test_sflag(zchan, SFLAG_SENT_ACK)) {
					zap_set_sflag(zchan, SFLAG_SENT_ACK);
					sangomabc_exec_command(mcon,
										   zchan->physical_span_id-1,
										   zchan->physical_chan_id-1,
										   0,
										   SIGBOOST_EVENT_CALL_START_ACK,
										   0,
										   SIGBOOST_PROGRESS_MEDIA);
				}
			}
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS;
				if ((status = zap_span_send_signal(zchan->span, &sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!zap_test_sflag(zchan, SFLAG_SENT_ACK)) {
					zap_set_sflag(zchan, SFLAG_SENT_ACK);
					sangomabc_exec_command(mcon,
										   zchan->physical_span_id-1,
										   zchan->physical_chan_id-1,
										   0,
										   SIGBOOST_EVENT_CALL_START_ACK,
										   0,
										   SIGBOOST_PROGRESS_RING);
				}
			}
		}
		break;
	case ZAP_CHANNEL_STATE_IDLE:
	case ZAP_CHANNEL_STATE_HOLD:
		/* twiddle */
		break;
	case ZAP_CHANNEL_STATE_RING:
		{
			if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_START;
				if ((status = zap_span_send_signal(zchan->span, &sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RESTART:
		{
			sig.event_id = ZAP_SIGEVENT_RESTART;
			status = zap_span_send_signal(zchan->span, &sig);
			zap_set_sflag_locked(zchan, SFLAG_SENT_FINAL_MSG);
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
		}
		break;
	case ZAP_CHANNEL_STATE_UP:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_UP;
				if ((status = zap_span_send_signal(zchan->span, &sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!(zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS) || zap_test_flag(zchan, ZAP_CHANNEL_MEDIA))) {
					sangomabc_exec_command(mcon,
										   zchan->physical_span_id-1,
										   zchan->physical_chan_id-1,
										   0,
										   SIGBOOST_EVENT_CALL_START_ACK,
										   0, 0);
				}
				
				sangomabc_exec_command(mcon,
									   zchan->physical_span_id-1,
									   zchan->physical_chan_id-1,
									   0,
									   SIGBOOST_EVENT_CALL_ANSWERED,
									   0, 0);
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
			zap_set_sflag_locked(zchan, SFLAG_HANGUP);

			if (zap_test_sflag(zchan, SFLAG_SENT_FINAL_MSG) || zap_test_sflag(zchan, SFLAG_TERMINATING)) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP_COMPLETE);
			} else {
				zap_set_sflag_locked(zchan, SFLAG_SENT_FINAL_MSG);
				
				if (zap_test_flag(zchan, ZAP_CHANNEL_ANSWERED) || 
					zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS) || 
					zap_test_flag(zchan, ZAP_CHANNEL_MEDIA) || zap_test_sflag(zchan, SFLAG_RECVD_ACK)) {
					
					sangomabc_exec_command(mcon,
										   zchan->physical_span_id-1,
										   zchan->physical_chan_id-1,
										   0,
										   SIGBOOST_EVENT_CALL_STOPPED,
										   zchan->caller_data.hangup_cause, 0);
				} else {
					sangomabc_exec_command(mcon,
										   zchan->physical_span_id-1,
										   zchan->physical_chan_id-1,								   
										   0,
										   SIGBOOST_EVENT_CALL_START_NACK,
										   zchan->caller_data.hangup_cause, 0);
				}
			}
		}
		break;
	case ZAP_CHANNEL_STATE_TERMINATING:
		{
			zap_set_sflag_locked(zchan, SFLAG_TERMINATING);
			sig.event_id = ZAP_SIGEVENT_STOP;
			status = zap_span_send_signal(zchan->span, &sig);
		}
		break;
	case ZAP_CHANNEL_STATE_IN_LOOP:
		{
			/* nothing to do, we sent the ZAP_COMMAND_ENABLE_LOOP command in handle_call_loop_start() right away */
		}
		break;
	default:
		break;
	}
}

static __inline__ void advance_chan_states(zap_channel_t *zchan)
{
	while (zap_test_flag(zchan, ZAP_CHANNEL_STATE_CHANGE)) {
		zap_clear_flag(zchan, ZAP_CHANNEL_STATE_CHANGE);
		state_advance(zchan);
		zap_channel_complete_state(zchan);
	}
}

/**
 * \brief Initialises outgoing requests array
 */
static __inline__ void init_outgoing_array(void)
{
	memset(&OUTBOUND_REQUESTS, 0, sizeof(OUTBOUND_REQUESTS));

}

/**
 * \brief Checks current state on a span
 * \param span Span to check status on
 */
static __inline__ void check_state(zap_span_t *span)
{
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	int susp = zap_test_flag(span, ZAP_SPAN_SUSPENDED);
	
	if (susp && zap_check_state_all(span, ZAP_CHANNEL_STATE_DOWN)) {
		susp = 0;
	}

    if (zap_test_flag(span, ZAP_SPAN_STATE_CHANGE) || susp) {
        uint32_t j;
        zap_clear_flag_locked(span, ZAP_SPAN_STATE_CHANGE);
        for(j = 1; j <= span->chan_count; j++) {
            if (zap_test_flag((span->channels[j]), ZAP_CHANNEL_STATE_CHANGE) || susp) {
				zap_mutex_lock(span->channels[j]->mutex);
                zap_clear_flag((span->channels[j]), ZAP_CHANNEL_STATE_CHANGE);
				if (susp && span->channels[j]->state != ZAP_CHANNEL_STATE_DOWN) {
					zap_channel_set_state(span->channels[j], ZAP_CHANNEL_STATE_RESTART, 0);
				}
                state_advance(span->channels[j]);
                zap_channel_complete_state(span->channels[j]);
				zap_mutex_unlock(span->channels[j]->mutex);
            }
        }
    }

	if (zap_test_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RESTARTING)) {
		if (zap_check_state_all(span, ZAP_CHANNEL_STATE_DOWN)) {
			sangomabc_exec_command(&sangoma_boost_data->mcon,
								   0,
								   0,
								   -1,
								   SIGBOOST_EVENT_SYSTEM_RESTART_ACK,
								   0, 0);	
			zap_clear_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RESTARTING);
			zap_clear_flag_locked(span, ZAP_SPAN_SUSPENDED);
			zap_clear_flag((&sangoma_boost_data->mcon), MSU_FLAG_DOWN);
			sangoma_boost_data->mcon.hb_elapsed = 0;
			init_outgoing_array();
		}
	}
}


/**
 * \brief Checks for events on a span
 * \param span Span to check for events
 */
static __inline__ void check_events(zap_span_t *span, int ms_timeout)
{
	zap_status_t status;

	status = zap_span_poll_event(span, ms_timeout);

	switch(status) {
	case ZAP_SUCCESS:
		{
			zap_event_t *event;
			while (zap_span_next_event(span, &event) == ZAP_SUCCESS) {
			// for now we do nothing with events, this is here
			// just to have the hardware layer to get any HW DTMF
			// events and enqueue the DTMF on the channel (done during zap_span_next_event())
			}
		}
		break;
	case ZAP_FAIL:
		{
			zap_log(ZAP_LOG_DEBUG, "Boost Check Event Failure Failure! %d\n", zap_running());
		}
		break;
	default:
		break;
	}

	return;
}

/**
 * \brief Main thread function for sangoma boost span (monitor)
 * \param me Current thread
 * \param obj Span to run in this thread
 */
static void *zap_sangoma_events_run(zap_thread_t *me, void *obj)
{
    zap_span_t *span = (zap_span_t *) obj;
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

	while (zap_test_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING) && zap_running()) {
		check_events(span,100);
	}

	return NULL;
}


#ifndef __WINDOWS__
static int waitfor_2sockets(int fda, int fdb, char *a, char *b, int timeout)
{
    struct pollfd pfds[2];
    int res = 0;
    int errflags = (POLLERR | POLLHUP | POLLNVAL);

    if (fda < 0 || fdb < 0) {
		return -1;
    }


waitfor_2sockets_tryagain:

    *a=0;
    *b=0;


    memset(pfds, 0, sizeof(pfds));

    pfds[0].fd = fda;
    pfds[1].fd = fdb;
    pfds[0].events = POLLIN | errflags;
    pfds[1].events = POLLIN | errflags;

    res = poll(pfds, 2, timeout); 

    if (res > 0) {
		res = 1;
		if ((pfds[0].revents & errflags) || (pfds[1].revents & errflags)) {
			res = -1;
		} else { 
			if ((pfds[0].revents & POLLIN)) {
				*a=1;
				res++;
			}
			if ((pfds[1].revents & POLLIN)) {
				*b=1;		
				res++;
			}
		}

		if (res == 1) {
			/* No event found what to do */
			res=-1;
		}
    } else if (res < 0) {
	
		if (errno == EINTR || errno == EAGAIN) {
			goto waitfor_2sockets_tryagain;
		}

    }
	
    return res;
}
#endif

/**
 * \brief Main thread function for sangoma boost span (monitor)
 * \param me Current thread
 * \param obj Span to run in this thread
 */
static void *zap_sangoma_boost_run(zap_thread_t *me, void *obj)
{
    zap_span_t *span = (zap_span_t *) obj;
    zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	sangomabc_connection_t *mcon, *pcon;
	uint32_t ms = 10; //, too_long = 20000;
	int max, activity, i;
	sangomabc_event_t *event;
	struct timeval tv;
	fd_set rfds, efds;
#ifndef __WINDOWS__
	char a=0,b=0;
#endif

	sangoma_boost_data->pcon = sangoma_boost_data->mcon;

	if (sangomabc_connection_open(&sangoma_boost_data->mcon,
							  sangoma_boost_data->mcon.cfg.local_ip,
							  sangoma_boost_data->mcon.cfg.local_port,
							  sangoma_boost_data->mcon.cfg.remote_ip,
							  sangoma_boost_data->mcon.cfg.remote_port) < 0) {
		zap_log(ZAP_LOG_ERROR, "Error: Opening MCON Socket [%d] %s\n", sangoma_boost_data->mcon.socket, strerror(errno));
		goto end;
    }
 
	if (sangomabc_connection_open(&sangoma_boost_data->pcon,
							  sangoma_boost_data->pcon.cfg.local_ip,
							  ++sangoma_boost_data->pcon.cfg.local_port,
							  sangoma_boost_data->pcon.cfg.remote_ip,
							  ++sangoma_boost_data->pcon.cfg.remote_port) < 0) {
		zap_log(ZAP_LOG_ERROR, "Error: Opening PCON Socket [%d] %s\n", sangoma_boost_data->pcon.socket, strerror(errno));
		goto end;
    }
	
	mcon = &sangoma_boost_data->mcon;
	pcon = &sangoma_boost_data->pcon;

	init_outgoing_array();

	sangomabc_exec_commandp(pcon,
					   0,
					   0,
					   -1,
					   SIGBOOST_EVENT_SYSTEM_RESTART,
					   0);
	zap_set_flag(mcon, MSU_FLAG_DOWN);

	while (zap_test_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING)) {
		
		tv.tv_sec = 0;
		tv.tv_usec = ms* 1000;
		max=0;
		activity=0;
		i=0;
		event = NULL;
		
		if (!zap_running()) {
			sangomabc_exec_commandp(pcon,
							   0,
							   0,
							   -1,
							   SIGBOOST_EVENT_SYSTEM_RESTART,
							   0);
			zap_set_flag(mcon, MSU_FLAG_DOWN);
			break;
		}

		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(mcon->socket, &rfds);
		FD_SET(mcon->socket, &efds);
		FD_SET(pcon->socket, &rfds);
		FD_SET(pcon->socket, &efds);

		max = ((pcon->socket > mcon->socket) ? pcon->socket : mcon->socket) + 1;

#ifdef __WINDOWS__
		if ((activity = select(max, &rfds, NULL, &efds, &tv)) < 0) {
			goto error;
		}
		
		if (activity) {
			if (FD_ISSET(pcon->socket, &efds) || FD_ISSET(mcon->socket, &efds)) {
				goto error;
			}

			if (FD_ISSET(pcon->socket, &rfds)) {
				while ((event = sangomabc_connection_readp(pcon, i))) {
					parse_sangoma_event(span, pcon, (sangomabc_short_event_t*)event);
					i++;
				}
			}
			i=0;

			if (FD_ISSET(mcon->socket, &rfds)) {
				if ((event = sangomabc_connection_read(mcon, i))) {
					parse_sangoma_event(span, mcon, (sangomabc_short_event_t*)event);
					i++;
				}
			}

		}
#else
		
		a=0;
		b=0;
		i=0;
		tv.tv_sec=0;
		activity = waitfor_2sockets(pcon->socket,mcon->socket,&a,&b,ms);
		if (activity) {
			if (a) {
				while ((event = sangomabc_connection_readp(pcon, i))) {
					parse_sangoma_event(span, pcon, (sangomabc_short_event_t*)event);
					i++;
				}
			}
			i=0;

			if (b) {
				if ((event = sangomabc_connection_read(mcon, i))) {
					parse_sangoma_event(span, mcon, (sangomabc_short_event_t*)event);
					i++;
				}
			}
		} else if (activity < 0) {
			goto error;
		}

#endif		

		pcon->hb_elapsed += ms;

		if (zap_test_flag(span, ZAP_SPAN_SUSPENDED) || zap_test_flag(mcon, MSU_FLAG_DOWN)) {
			pcon->hb_elapsed = 0;
		}


#if 0
		if (pcon->hb_elapsed >= too_long) {
			zap_log(ZAP_LOG_CRIT, "Lost Heartbeat!\n");
			zap_set_flag_locked(span, ZAP_SPAN_SUSPENDED);
			zap_set_flag(mcon, MSU_FLAG_DOWN);
			sangomabc_exec_commandp(pcon,
								0,
								0,
								-1,
								SIGBOOST_EVENT_SYSTEM_RESTART,
								0);
		}
#endif

		if (zap_running()) {
			check_state(span);
		}
	}

	goto end;

 error:
	zap_log(ZAP_LOG_CRIT, "Socket Error!\n");

 end:

	sangomabc_connection_close(&sangoma_boost_data->mcon);
	sangomabc_connection_close(&sangoma_boost_data->pcon);

	zap_clear_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING);

	zap_log(ZAP_LOG_DEBUG, "SANGOMA_BOOST thread ended.\n");
	return NULL;
}

/**
 * \brief Loads sangoma boost signaling module
 * \param zio Openzap IO interface
 * \return Success
 */
static ZIO_SIG_LOAD_FUNCTION(zap_sangoma_boost_init)
{
	zap_mutex_create(&request_mutex);
	
	return ZAP_SUCCESS;
}

static zap_status_t zap_sangoma_boost_start(zap_span_t *span)
{
	int err;
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	zap_set_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING);
	err=zap_thread_create_detached(zap_sangoma_boost_run, span);
	if (err) {
		zap_clear_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING);
		return err;
	}
	// launch the events thread to handle HW DTMF and possibly
	// other events in the future
	err=zap_thread_create_detached(zap_sangoma_events_run, span);
	if (err) {
		zap_clear_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING);
	}
	return err;
}

static zap_state_map_t boost_state_map = {
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
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_IDLE, ZAP_CHANNEL_STATE_HOLD, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HOLD, ZAP_END},
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, 
			 ZAP_CHANNEL_STATE_IDLE, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_UP, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_IDLE, ZAP_END},
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_IDLE, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_UP, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_CHANNEL_STATE_HANGUP, ZAP_END}
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
			{ZAP_CHANNEL_STATE_DOWN},
			{ZAP_CHANNEL_STATE_IN_LOOP, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_IN_LOOP},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
			{ZAP_CHANNEL_STATE_RING, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_RING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_PROGRESS_MEDIA,ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_CHANNEL_STATE_HANGUP, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_UP, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_UP, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
		},
		

	}
};

/**
 * \brief Initialises an sangoma boost span from configuration variables
 * \param span Span to configure
 * \param sig_cb Callback function for event signals
 * \param ap List of configuration variables
 * \return Success or failure
 */
static ZIO_SIG_CONFIGURE_FUNCTION(zap_sangoma_boost_configure_span)
{
	zap_sangoma_boost_data_t *sangoma_boost_data = NULL;
	const char *local_ip = "127.0.0.65", *remote_ip = "127.0.0.66";
	int local_port = 53000, remote_port = 53000;
	char *var, *val;
	int *intval;

	while((var = va_arg(ap, char *))) {
		if (!strcasecmp(var, "local_ip")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			local_ip = val;
		} else if (!strcasecmp(var, "remote_ip")) {
			if (!(val = va_arg(ap, char *))) {
				break;
			}
			remote_ip = val;
		} else if (!strcasecmp(var, "local_port")) {
			if (!(intval = va_arg(ap, int *))) {
				break;
			}
			local_port = *intval;
		} else if (!strcasecmp(var, "remote_port")) {
			if (!(intval = va_arg(ap, int *))) {
				break;
			}
			remote_port = *intval;
		} else {
			snprintf(span->last_error, sizeof(span->last_error), "Unknown parameter [%s]", var);
			return ZAP_FAIL;
		}
	}


	if (!local_ip && local_port && remote_ip && remote_port && sig_cb) {
		zap_set_string(span->last_error, "missing params");
		return ZAP_FAIL;
	}

	sangoma_boost_data = malloc(sizeof(*sangoma_boost_data));
	assert(sangoma_boost_data);
	memset(sangoma_boost_data, 0, sizeof(*sangoma_boost_data));
	
	zap_set_string(sangoma_boost_data->mcon.cfg.local_ip, local_ip);
	sangoma_boost_data->mcon.cfg.local_port = local_port;
	zap_set_string(sangoma_boost_data->mcon.cfg.remote_ip, remote_ip);
	sangoma_boost_data->mcon.cfg.remote_port = remote_port;
	span->signal_cb = sig_cb;
	span->start = zap_sangoma_boost_start;
	span->signal_data = sangoma_boost_data;
    span->signal_type = ZAP_SIGTYPE_SANGOMABOOST;
    span->outgoing_call = sangoma_boost_outgoing_call;
	span->channel_request = sangoma_boost_channel_request;
	span->state_map = &boost_state_map;
	zap_set_flag_locked(span, ZAP_SPAN_SUSPENDED);

	return ZAP_SUCCESS;
}

/**
 * \brief Openzap sangoma boost signaling module definition
 */
zap_module_t zap_module = { 
	"sangoma_boost",
	NULL,
	NULL,
	zap_sangoma_boost_init,
	zap_sangoma_boost_configure_span,
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
