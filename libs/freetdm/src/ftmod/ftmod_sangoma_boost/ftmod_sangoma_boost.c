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
 *
 * Contributors: 
 *
 * Moises Silva <moy@sangoma.com>
 * David Yat Sin <dyatsin@sangoma.com>
 * Nenad Corbic <ncorbic@sangoma.com>
 *
 */

/* NOTE:
On __WINDOWS__ platform this code works with sigmod ONLY, don't try to make sense of any socket code for win
I basically ifdef out everything that the compiler complained about
*/

#include "freetdm.h"
#include "sangoma_boost_client.h"
#include "ftdm_sangoma_boost.h"
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

/* Boost signaling modules global hash and its mutex */
ftdm_mutex_t *g_boost_modules_mutex = NULL;
ftdm_hash_t *g_boost_modules_hash = NULL;

#define MAX_TRUNK_GROUPS 64
//DAVIDY need to merge congestion_timeouts with ftdm_sangoma_boost_trunkgroups
static time_t congestion_timeouts[MAX_TRUNK_GROUPS];

static ftdm_sangoma_boost_trunkgroup_t *g_trunkgroups[MAX_TRUNK_GROUPS];

#define BOOST_QUEUE_SIZE 500

/* get freetdm span and chan depending on the span mode */
#define BOOST_SPAN(ftdmchan) ((ftdm_sangoma_boost_data_t*)(ftdmchan)->span->signal_data)->sigmod ? ftdmchan->physical_span_id : ftdmchan->physical_span_id-1
#define BOOST_CHAN(ftdmchan) ((ftdm_sangoma_boost_data_t*)(ftdmchan)->span->signal_data)->sigmod ? ftdmchan->physical_chan_id : ftdmchan->physical_chan_id-1

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
	ftdm_span_t *span;
	ftdm_channel_t *ftdmchan;
	int hangup_cause;
	int flags;
} sangoma_boost_request_t;

//#define MAX_REQ_ID FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN * FTDM_MAX_CHANNELS_PHYSICAL_SPAN
#define MAX_REQ_ID 6000

static uint16_t SETUP_GRID[FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN+1][FTDM_MAX_CHANNELS_PHYSICAL_SPAN+1] = {{ 0 }};

static sangoma_boost_request_t OUTBOUND_REQUESTS[MAX_REQ_ID+1] = {{ 0 }};

static ftdm_mutex_t *request_mutex = NULL;

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

	ftdm_mutex_lock(request_mutex);
	if ((id = SETUP_GRID[span][chan])) {
		assert(id <= MAX_REQ_ID);
		req_map[id] = 0;
		SETUP_GRID[span][chan] = 0;
	}
	ftdm_mutex_unlock(request_mutex);
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
	ftdm_mutex_lock(request_mutex);
	req_map[r] = 0;
	ftdm_mutex_unlock(request_mutex);
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
	
	ftdm_mutex_lock(request_mutex);
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

	ftdm_mutex_unlock(request_mutex);

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
static ftdm_channel_t *find_ftdmchan(ftdm_span_t *span, sangomabc_short_event_t *event, int force)
{
	uint32_t i;
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	uint32_t targetspan = event->span+1;
	uint32_t targetchan = event->chan+1;
	if (sangoma_boost_data->sigmod) {
		/* span is not strictly needed here since we're supposed to get only events for our span */
		targetspan = event->span;
		targetchan = event->chan;
	}

	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->physical_span_id == targetspan && span->channels[i]->physical_chan_id == targetchan) {
			ftdmchan = span->channels[i];
			if (force || (ftdmchan->state == FTDM_CHANNEL_STATE_DOWN && !ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE))) {
				break;
			} else {
				ftdmchan = NULL;
				ftdm_log(FTDM_LOG_DEBUG, "Channel %d:%d ~ %d:%d is already in use.\n",
						span->channels[i]->span_id,
						span->channels[i]->chan_id,
						span->channels[i]->physical_span_id,
						span->channels[i]->physical_chan_id
						);
				break;
			}
		}
	}

	return ftdmchan;
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
 * \param ftdmchan Channel to initialise
 * \return Success or failure
 */
static FIO_CHANNEL_REQUEST_FUNCTION(sangoma_boost_channel_request)
{
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	ftdm_status_t status = FTDM_FAIL;
	sangoma_boost_request_id_t r;
	sangomabc_event_t event = {0};
	/* sanity has to be more than 8 seconds.
	 * In PRI specs, timeout is 4 seconds for remote switch to respond to a SETUP,
	 * and PRI stack will retransmit a second SETUP after the first timeout, so
	 * we should allow for at least 8 seconds.
	 */
	int boost_request_timeout = 10000;
	sangoma_boost_request_status_t st;
	char dnis[128] = "";
	char *gr = NULL;
	uint32_t count = 0;
	int tg=0;
	
	if (sangoma_boost_data->sigmod) {
		ftdm_log(FTDM_LOG_CRIT, "This function should not be called when sigmod was configured in boost\n");
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}

	if (ftdm_test_flag(span, FTDM_SPAN_SUSPENDED)) {
		ftdm_log(FTDM_LOG_CRIT, "SPAN is not online.\n");
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}
	
	ftdm_set_string(dnis, caller_data->dnis.digits);

	r = next_request_id();
	if (r == 0) {
		ftdm_log(FTDM_LOG_CRIT, "All tanks ids are busy.\n");
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}
	sangomabc_call_init(&event, caller_data->cid_num.digits, dnis, r);

	if ((gr = strchr(dnis, '@'))) {
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
		ftdm_log(FTDM_LOG_CRIT, "All circuits are busy. Trunk Group=%i (BOOST REQUESTED BACK OFF)\n",tg+1);
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}

	ftdm_span_channel_use_count(span, &count);

	if (count >= span->chan_count) {
		ftdm_log(FTDM_LOG_CRIT, "All circuits are busy.\n");
		*ftdmchan = NULL;
		return FTDM_FAIL;
	}	

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
						ftdm_log(FTDM_LOG_WARNING, "Failed to determine huntgroup (%s)\n", gr);
									event.hunt_group = SIGBOOST_HUNTGRP_SEQ_ASC;
		}
	}

	ftdm_set_string(event.calling_name, caller_data->cid_name);
	ftdm_set_string(event.rdnis.digits, caller_data->rdnis.digits);
	if (strlen(caller_data->rdnis.digits)) {
			event.rdnis.digits_count = strlen(caller_data->rdnis.digits)+1;
			event.rdnis.ton = caller_data->rdnis.type;
			event.rdnis.npi = caller_data->rdnis.plan;
	}
    
	event.calling.screening_ind = caller_data->screen;
	event.calling.presentation_ind = caller_data->pres;

	event.calling.ton = caller_data->cid_num.type;
	event.calling.npi = caller_data->cid_num.plan;

	event.called.ton = caller_data->dnis.type;
	event.called.npi = caller_data->dnis.plan;

	OUTBOUND_REQUESTS[r].status = BST_WAITING;
	OUTBOUND_REQUESTS[r].span = span;

	if (sangomabc_connection_write(&sangoma_boost_data->mcon, &event) <= 0) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to tx boost event [%s]\n", strerror(errno));
		status = FTDM_FAIL;
		if (!sangoma_boost_data->sigmod) {
			*ftdmchan = NULL;
		}
		goto done;
	}

	while(ftdm_running() && OUTBOUND_REQUESTS[r].status == BST_WAITING) {
		ftdm_sleep(1);
		if (--boost_request_timeout <= 0) {
			status = FTDM_FAIL;
			if (!sangoma_boost_data->sigmod) {
				*ftdmchan = NULL;
			}
			ftdm_log(FTDM_LOG_CRIT, "s%dc%d: Csid:%d Timed out waiting for boost channel request response, current status: BST_WAITING\n", (*ftdmchan)->physical_span_id, (*ftdmchan)->physical_chan_id, r);
			goto done;
		}
	}

	if (OUTBOUND_REQUESTS[r].status == BST_READY && OUTBOUND_REQUESTS[r].ftdmchan) {
		*ftdmchan = OUTBOUND_REQUESTS[r].ftdmchan;
		status = FTDM_SUCCESS;
	} else {
		status = FTDM_FAIL;
		if (!sangoma_boost_data->sigmod) {
			*ftdmchan = NULL;
		}
	}

 done:
	
	st = OUTBOUND_REQUESTS[r].status;
	OUTBOUND_REQUESTS[r].status = BST_FREE;	

	if (status == FTDM_FAIL) {
		if (st == BST_FAIL) {
			caller_data->hangup_cause = OUTBOUND_REQUESTS[r].hangup_cause;
		} else {
			caller_data->hangup_cause = FTDM_CAUSE_RECOVERY_ON_TIMER_EXPIRE;
		}
	}
	
	if (st == BST_FAIL) {
		release_request_id(r);
	} else if (st != BST_READY) {
		ftdm_assert_return(r <= MAX_REQ_ID, FTDM_FAIL, "Invalid index\n");
		nack_map[r] = 1;
		if (sangoma_boost_data->sigmod) {
			sangomabc_exec_command(&sangoma_boost_data->mcon,
								BOOST_SPAN((*ftdmchan)),
								BOOST_CHAN((*ftdmchan)),
								r,
								SIGBOOST_EVENT_CALL_START_NACK,
								0, 0);
		} else {
			sangomabc_exec_command(&sangoma_boost_data->mcon,
								0,
								0,
								r,
								SIGBOOST_EVENT_CALL_START_NACK,
								0, 0);
		}
	}

	return status;
}

/**
 * \brief Starts an sangoma boost channel (outgoing call)
 * \param ftdmchan Channel to initiate call on
 * \return Success
 */
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(sangoma_boost_outgoing_call)
{
	char dnis[128] = "";
	sangoma_boost_request_id_t r;
	sangomabc_event_t event = {0};
	ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;
	if (!sangoma_boost_data->sigmod) {
		return FTDM_SUCCESS;
	}
	ftdm_set_string(dnis, ftdmchan->caller_data.dnis.digits);

	r = next_request_id();
	if (r == 0) {
		ftdm_log(FTDM_LOG_CRIT, "All boost request ids are busy.\n");
		return FTDM_FAIL;
	}
	
	ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);

	sangomabc_call_init(&event, ftdmchan->caller_data.cid_num.digits, dnis, r);

	event.span = (uint8_t)ftdmchan->physical_span_id;
	event.chan = (uint8_t)ftdmchan->physical_chan_id;

	ftdm_set_string(event.calling_name, ftdmchan->caller_data.cid_name);
	ftdm_set_string(event.rdnis.digits, ftdmchan->caller_data.rdnis.digits);
	if (strlen(ftdmchan->caller_data.rdnis.digits)) {
			event.rdnis.digits_count = strlen(ftdmchan->caller_data.rdnis.digits)+1;
			event.rdnis.ton = ftdmchan->caller_data.rdnis.type;
			event.rdnis.npi = ftdmchan->caller_data.rdnis.plan;
	}
    
	event.calling.screening_ind = ftdmchan->caller_data.screen;
	event.calling.presentation_ind = ftdmchan->caller_data.pres;

	event.calling.ton = ftdmchan->caller_data.cid_num.type;
	event.calling.npi = ftdmchan->caller_data.cid_num.plan;

	event.called.ton = ftdmchan->caller_data.dnis.type;
	event.called.npi = ftdmchan->caller_data.dnis.plan;

	OUTBOUND_REQUESTS[r].status = BST_WAITING;
	OUTBOUND_REQUESTS[r].span = ftdmchan->span;
	OUTBOUND_REQUESTS[r].ftdmchan = ftdmchan;

	ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DIALING);

	ftdm_log(FTDM_LOG_DEBUG, "Dialing number %s over boost channel with request id %d\n", event.called_number_digits, r);
	if (sangomabc_connection_write(&sangoma_boost_data->mcon, &event) <= 0) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to tx boost event [%s]\n", strerror(errno));
		return FTDM_FAIL;
	}
			
	return FTDM_SUCCESS;
}

/**
 * \brief Handler for call start ack no media event
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_progress(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_channel_t *ftdmchan;


	if ((ftdmchan = find_ftdmchan(span, event, 1))) {
		ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;
		ftdm_mutex_lock(ftdmchan->mutex);
		if (!sangoma_boost_data->sigmod && ftdmchan->state == FTDM_CHANNEL_STATE_HOLD) {
			if ((event->flags & SIGBOOST_PROGRESS_MEDIA)) {
				ftdmchan->init_state = FTDM_CHANNEL_STATE_PROGRESS_MEDIA;
				ftdm_log(FTDM_LOG_DEBUG, "Channel init state updated to PROGRESS_MEDIA [Csid:%d]\n", event->call_setup_id);
			} else if ((event->flags & SIGBOOST_PROGRESS_RING)) {
				ftdmchan->init_state = FTDM_CHANNEL_STATE_PROGRESS;
				ftdm_log(FTDM_LOG_DEBUG, "Channel init state updated to PROGRESS [Csid:%d]\n", event->call_setup_id);
			} else {
				ftdmchan->init_state = FTDM_CHANNEL_STATE_IDLE;
				ftdm_log(FTDM_LOG_DEBUG, "Channel init state updated to IDLE [Csid:%d]\n", event->call_setup_id);
			}			
		} else {
			if ((event->flags & SIGBOOST_PROGRESS_MEDIA)) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
			} else if ((event->flags & SIGBOOST_PROGRESS_RING)) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
			} else {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_IDLE);
			}
		}
		ftdm_mutex_unlock(ftdmchan->mutex);
	}
}

/**
 * \brief Handler for call start ack event
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_start_ack(sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	
	ftdm_channel_t *ftdmchan = NULL;
	uint32_t event_span = event->span+1;
	uint32_t event_chan = event->chan+1;

	if (nack_map[event->call_setup_id]) {
		return;
	}

	if (mcon->sigmod) {
		event_span = event->span;
		event_chan = event->chan;
	}

	OUTBOUND_REQUESTS[event->call_setup_id].event = *event;
	SETUP_GRID[event->span][event->chan] = event->call_setup_id;

	if (mcon->sigmod) {
		ftdmchan = OUTBOUND_REQUESTS[event->call_setup_id].ftdmchan;
	} else {
		ftdmchan = find_ftdmchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 0);
	}


	if (ftdmchan) {
		ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;
		if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to open FTDM channel [%s]\n", ftdmchan->last_error);
		} else {
			ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);
			ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_INUSE);
			ftdmchan->sflags = SFLAG_RECVD_ACK;

			if ((event->flags & SIGBOOST_PROGRESS_MEDIA)) {
				if (sangoma_boost_data->sigmod) {
					ftdm_log(FTDM_LOG_DEBUG, "Channel state changing to PROGRESS_MEDIA [Csid:%d]\n", event->call_setup_id);
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
				} else {
					ftdmchan->init_state = FTDM_CHANNEL_STATE_PROGRESS_MEDIA;
					ftdm_log(FTDM_LOG_DEBUG, "Channel init state changed to PROGRESS_MEDIA [Csid:%d]\n", event->call_setup_id);
				}
			} else if ((event->flags & SIGBOOST_PROGRESS_RING)) {
				if (sangoma_boost_data->sigmod) {
					ftdm_log(FTDM_LOG_DEBUG, "Channel state changing to PROGRESS [Csid:%d]\n", event->call_setup_id);
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
				} else {
					ftdmchan->init_state = FTDM_CHANNEL_STATE_PROGRESS;
					ftdm_log(FTDM_LOG_DEBUG, "Channel init state changed to PROGRESS [Csid:%d]\n", event->call_setup_id);
				}
			} else {
				if (sangoma_boost_data->sigmod) {
					/* should we set a state here? */
				} else {
					ftdmchan->init_state = FTDM_CHANNEL_STATE_IDLE;
					ftdm_log(FTDM_LOG_DEBUG, "Channel init state changed to IDLE [Csid:%d]\n", event->call_setup_id);
				}
			}
			if (!sangoma_boost_data->sigmod) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HOLD);
				ftdm_log(FTDM_LOG_DEBUG, "Assigned chan %d:%d (%d:%d) to CSid=%d\n", 
						ftdmchan->span_id, ftdmchan->chan_id, event_span, event_chan, event->call_setup_id);
				OUTBOUND_REQUESTS[event->call_setup_id].ftdmchan = ftdmchan;
			}
			OUTBOUND_REQUESTS[event->call_setup_id].flags = event->flags;
			OUTBOUND_REQUESTS[event->call_setup_id].status = BST_READY;
			return;
		}
	} 

	if (!ftdmchan) {
		ftdm_log(FTDM_LOG_CRIT, "START ACK CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	} else {
		/* only reason to be here is failed to open channel when we we're in sigmod  */
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
	}
	ftdm_set_sflag(ftdmchan, SFLAG_SENT_FINAL_MSG);
	sangomabc_exec_command(mcon,
					   event->span,
					   event->chan,
					   event->call_setup_id,
					   SIGBOOST_EVENT_CALL_STOPPED,
					   FTDM_CAUSE_DESTINATION_OUT_OF_ORDER, 0);
	OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;
	OUTBOUND_REQUESTS[event->call_setup_id].hangup_cause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
}

/**
 * \brief Handler for call done event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_done(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_channel_t *ftdmchan;
	int r = 0;
	
	if ((ftdmchan = find_ftdmchan(span, event, 1))) {
		ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;
		ftdm_mutex_lock(ftdmchan->mutex);

		if (sangoma_boost_data->sigmod) {
			/* not really completely done, but if we ever get an incoming call before moving to HANGUP_COMPLETE
			 * handle_incoming_call() will take care of moving the state machine to release the channel */
			sangomabc_exec_command(&sangoma_boost_data->mcon,
								BOOST_SPAN(ftdmchan),
								BOOST_CHAN(ftdmchan),
								0,
								SIGBOOST_EVENT_CALL_RELEASED,
								0, 0);
		}

		if (ftdmchan->state == FTDM_CHANNEL_STATE_DOWN || ftdmchan->state == FTDM_CHANNEL_STATE_HANGUP_COMPLETE || ftdm_test_sflag(ftdmchan, SFLAG_TERMINATING)) {
			goto done;
		}

		ftdm_set_state_r(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE, 0, r);
		if (r) {
			ftdm_set_sflag(ftdmchan, SFLAG_FREE_REQ_ID);
			ftdm_mutex_unlock(ftdmchan->mutex);
			return;
		}
	} 

 done:
	
	if (ftdmchan) {
		ftdm_mutex_unlock(ftdmchan->mutex);
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
static void handle_call_start_nack(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_channel_t *ftdmchan;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

	if (event->release_cause == SIGBOOST_CALL_SETUP_NACK_ALL_CKTS_BUSY) {
		uint32_t count = 0;
		int delay = 0;
		int tg=event->trunk_group;

		ftdm_span_channel_use_count(span, &count);

		delay = (int) (count / 100) * 2;
		
		if (delay > 10) {
			delay = 10;
		} else if (delay < 1) {
			delay = 1;
		}

		if (tg < 0 || tg >= MAX_TRUNK_GROUPS) {
			ftdm_log(FTDM_LOG_CRIT, "Invalid All Ckt Busy trunk group number %i\n", tg);
			tg=0;
		}
		
		congestion_timeouts[tg] = time(NULL) + delay;
		event->release_cause = 17;

	} else if (event->release_cause == SIGBOOST_CALL_SETUP_CSUPID_DBL_USE) {
		event->release_cause = 17;
	}

	if (event->call_setup_id) {
		if (sangoma_boost_data->sigmod) {
			ftdmchan = OUTBOUND_REQUESTS[event->call_setup_id].ftdmchan;
			ftdmchan->call_data = (void*)(intptr_t)event->event_id;
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		} else {
			sangomabc_exec_command(mcon,
						   0,
						   0,
						   event->call_setup_id,
						   SIGBOOST_EVENT_CALL_START_NACK_ACK,
						   0, 0);
			OUTBOUND_REQUESTS[event->call_setup_id].event = *event;
			OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;
			OUTBOUND_REQUESTS[event->call_setup_id].hangup_cause = event->release_cause;
			ftdm_log(FTDM_LOG_DEBUG, "setting outbound request status %d to BST_FAIL\n", event->call_setup_id);
		}
		return;
	} else {
		if ((ftdmchan = find_ftdmchan(span, event, 1))) {
			int r = 0;

			/* if there is no call setup id this should not be an outbound channel for sure */
			ftdm_assert(!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND), "Yay, outbound flag should not be set here!\n");

			ftdmchan->call_data = (void*)(intptr_t)event->event_id;
			ftdm_mutex_lock(ftdmchan->mutex);
			ftdm_set_state_r(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING, 0, r);
			if (r == FTDM_STATE_CHANGE_SUCCESS) {
				ftdmchan->caller_data.hangup_cause = event->release_cause;
			}
			ftdm_mutex_unlock(ftdmchan->mutex);
			if (r) {
				return;
			}
		} 
	}

	if (ftdmchan) {
		ftdm_set_sflag_locked(ftdmchan, SFLAG_SENT_FINAL_MSG);
	}

	/* nobody else will do it so we have to do it ourselves */
	sangomabc_exec_command(mcon,
					   event->span,
					   event->chan,
					   0,
					   SIGBOOST_EVENT_CALL_START_NACK_ACK,
					   0, 0);
}

static void handle_call_released(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_channel_t *ftdmchan;
	
	if ((ftdmchan = find_ftdmchan(span, event, 1))) {
		ftdm_log(FTDM_LOG_DEBUG, "Releasing completely chan s%dc%d\n", event->span, event->chan);
		ftdm_channel_done(ftdmchan);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "Odd, We could not find chan: s%dc%d to release the call completely!!\n", event->span, event->chan);
	}
}

/**
 * \brief Handler for call stop event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_stop(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_channel_t *ftdmchan;
	
	if ((ftdmchan = find_ftdmchan(span, event, 1))) {
		int r = 0;

		ftdm_mutex_lock(ftdmchan->mutex);
		
		if (ftdm_test_sflag(ftdmchan, SFLAG_HANGUP)) {
			/* racing condition where both sides initiated a hangup 
			 * Do not change current state as channel is already clearing
			 * itself through local initiated hangup */
			
			sangomabc_exec_command(mcon,
						BOOST_SPAN(ftdmchan),
						BOOST_CHAN(ftdmchan),
						0,
						SIGBOOST_EVENT_CALL_STOPPED_ACK,
						0, 0);
			ftdm_mutex_unlock(ftdmchan->mutex);
			return;
		} else if (ftdmchan->state == FTDM_CHANNEL_STATE_HOLD) {
			ftdmchan->init_state = FTDM_CHANNEL_STATE_TERMINATING;
			ftdm_log(FTDM_LOG_DEBUG, "Channel init state updated to TERMINATING [Csid:%d]\n", event->call_setup_id);
			OUTBOUND_REQUESTS[event->call_setup_id].hangup_cause = event->release_cause;
			ftdmchan->caller_data.hangup_cause = event->release_cause;
			ftdm_mutex_unlock(ftdmchan->mutex);
			return;
		} else {
			ftdm_set_state_r(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING, 0, r);
		}

		if (r == FTDM_STATE_CHANGE_SUCCESS) {
			ftdmchan->caller_data.hangup_cause = event->release_cause;
		}

		if (r) {
			ftdm_set_sflag(ftdmchan, SFLAG_FREE_REQ_ID);
		}

		ftdm_mutex_unlock(ftdmchan->mutex);

		if (r) {
			return;
		}
	} else { /* we have to do it ourselves.... */
		ftdm_log(FTDM_LOG_ERROR, "Odd, We could not find chan: s%dc%d\n", event->span, event->chan);
		release_request_id_span_chan(event->span, event->chan);
	}
}

/**
 * \brief Handler for call answer event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_answer(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_channel_t *ftdmchan;
	
	if ((ftdmchan = find_ftdmchan(span, event, 1))) {
		ftdm_mutex_lock(ftdmchan->mutex);
		if (ftdmchan->state == FTDM_CHANNEL_STATE_HOLD) {
			ftdmchan->init_state = FTDM_CHANNEL_STATE_UP;
		} else {
			int r = 0;
			ftdm_set_state_r(ftdmchan, FTDM_CHANNEL_STATE_UP, 0, r);
		}
		ftdm_mutex_unlock(ftdmchan->mutex);
	} else {
		ftdm_log(FTDM_LOG_CRIT, "ANSWER CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
		sangomabc_exec_command(mcon,
							   event->span,
							   event->chan,
							   event->call_setup_id,
							   SIGBOOST_EVENT_CALL_STOPPED,
							   FTDM_CAUSE_DESTINATION_OUT_OF_ORDER, 0);
	}
}

static __inline__ void advance_chan_states(ftdm_channel_t *ftdmchan);

/**
 * \brief Handler for call start event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_start(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_event_t *event)
{
	ftdm_channel_t *ftdmchan = NULL;
	int hangup_cause = FTDM_CAUSE_CALL_REJECTED;
	if (!(ftdmchan = find_ftdmchan(span, (sangomabc_short_event_t*)event, 0))) {
		if ((ftdmchan = find_ftdmchan(span, (sangomabc_short_event_t*)event, 1))) {
			int r;
			if (ftdmchan->state == FTDM_CHANNEL_STATE_UP) {
				ftdm_log(FTDM_LOG_CRIT, "ZCHAN STATE UP -> Changed to TERMINATING %d:%d\n", event->span+1,event->chan+1);
				ftdm_set_state_r(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING, 0, r);
			} else if (ftdm_test_sflag(ftdmchan, SFLAG_HANGUP)) { 
				ftdm_log(FTDM_LOG_CRIT, "ZCHAN STATE HANGUP -> Changed to HANGUP COMPLETE %d:%d\n", event->span+1,event->chan+1);
				ftdm_set_state_r(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE, 0, r);
			} else {
				ftdm_log(FTDM_LOG_CRIT, "ZCHAN STATE INVALID %s on IN CALL %d:%d\n", ftdm_channel_state2str(ftdmchan->state),event->span+1,event->chan+1);

			}
			ftdm_set_sflag(ftdmchan, SFLAG_SENT_FINAL_MSG);
			ftdmchan = NULL;
		}
		ftdm_log(FTDM_LOG_CRIT, "START CANT FIND CHAN %d:%d\n", event->span+1,event->chan+1);
		goto error;
	}

	if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "START CANT OPEN CHAN %d:%d\n", event->span+1,event->chan+1);
		goto error;
	}
	
	ftdm_log(FTDM_LOG_DEBUG, "Got call start from s%dc%d mapped to freetdm logical s%dc%d, physical s%dc%d\n", 
			event->span, event->chan, 
			ftdmchan->span_id, ftdmchan->chan_id,
			ftdmchan->physical_span_id, ftdmchan->physical_chan_id);

	ftdmchan->sflags = 0;
	ftdm_set_string(ftdmchan->caller_data.cid_num.digits, (char *)event->calling.digits);
	ftdm_set_string(ftdmchan->caller_data.cid_name, (char *)event->calling.digits);
	ftdm_set_string(ftdmchan->caller_data.ani.digits, (char *)event->calling.digits);
	ftdm_set_string(ftdmchan->caller_data.dnis.digits, (char *)event->called.digits);
	ftdm_set_string(ftdmchan->caller_data.rdnis.digits, (char *)event->rdnis.digits);

	if (strlen(event->calling_name)) {
		ftdm_set_string(ftdmchan->caller_data.cid_name, (char *)event->calling_name);
	}

	ftdmchan->caller_data.cid_num.plan = event->calling.npi;
	ftdmchan->caller_data.cid_num.type = event->calling.ton;

	ftdmchan->caller_data.ani.plan = event->calling.npi;
	ftdmchan->caller_data.ani.type = event->calling.ton;

	ftdmchan->caller_data.dnis.plan = event->called.npi;
	ftdmchan->caller_data.dnis.type = event->called.ton;

	ftdmchan->caller_data.rdnis.plan = event->rdnis.npi;
	ftdmchan->caller_data.rdnis.type = event->rdnis.ton;

	ftdmchan->caller_data.screen = event->calling.screening_ind;
	ftdmchan->caller_data.pres = event->calling.presentation_ind;

	if (event->custom_data_size) {
		char* p = NULL;

		p = strstr((char*)event->custom_data,"PRI001-ANI2-");
		if (p!=NULL) {
			int ani2 = 0;
			sscanf(p, "PRI001-ANI2-%d", &ani2);
			snprintf(ftdmchan->caller_data.aniII, 5, "%.2d", ani2);
		}	
	}

	ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RING);
	return;

 error:
	hangup_cause = ftdmchan ? ftdmchan->caller_data.hangup_cause : FTDM_CAUSE_REQUESTED_CHAN_UNAVAIL;
	sangomabc_exec_command(mcon,
						   event->span,
						   event->chan,
						   0,
						   SIGBOOST_EVENT_CALL_START_NACK,
						   hangup_cause, 0);
		
}

static void handle_call_loop_start(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_status_t res = FTDM_FAIL;
	ftdm_channel_t *ftdmchan;

	if (!(ftdmchan = find_ftdmchan(span, (sangomabc_short_event_t*)event, 0))) {
		ftdm_log(FTDM_LOG_CRIT, "CANNOT START LOOP, CHAN NOT AVAILABLE %d:%d\n", event->span+1,event->chan+1);
		return;
	}

	if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "CANNOT START LOOP, CANT OPEN CHAN %d:%d\n", event->span+1,event->chan+1);
		return;
	}

	ftdm_set_state_r(ftdmchan, FTDM_CHANNEL_STATE_IN_LOOP, 0, res);
	if (res != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "yay, could not set the state of the channel to IN_LOOP, loop will fail\n");
		ftdm_channel_done(ftdmchan);
		return;
	}
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_ENABLE_LOOP, NULL);
}

static void handle_call_loop_stop(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_channel_t *ftdmchan;
	ftdm_status_t res = FTDM_FAIL;
	if (!(ftdmchan = find_ftdmchan(span, (sangomabc_short_event_t*)event, 1))) {
		ftdm_log(FTDM_LOG_CRIT, "CANNOT STOP LOOP, INVALID CHAN REQUESTED %d:%d\n", event->span+1,event->chan+1);
		return;
	}
	if (ftdmchan->state != FTDM_CHANNEL_STATE_IN_LOOP) {
		ftdm_log(FTDM_LOG_ERROR, "Got stop loop request in a channel that is not in loop, ignoring ...\n");
		return;
	}
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_LOOP, NULL);
	/* even when we did not sent a msg we set this flag to avoid sending call stop in the DOWN state handler */
	ftdm_set_flag(ftdmchan, SFLAG_SENT_FINAL_MSG);
	ftdm_set_state_r(ftdmchan, FTDM_CHANNEL_STATE_DOWN, 0, res);
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
		ftdm_log(FTDM_LOG_CRIT, "Failed to tx on boost connection [%s]: %s\n", strerror(errno));
	}
    return;
}

/**
 * \brief Handler for restart ack event
 * \param mcon sangoma boost connection
 * \param span Span where event was fired
 * \param event Event to handle
 */
static void handle_restart_ack(sangomabc_connection_t *mcon, ftdm_span_t *span, sangomabc_short_event_t *event)
{
	ftdm_log(FTDM_LOG_DEBUG, "RECV RESTART ACK\n");
}

/**
 * \brief Handler for restart event
 * \param mcon sangoma boost connection
 * \param span Span where event was fired
 * \param event Event to handle
 */
static void handle_restart(sangomabc_connection_t *mcon, ftdm_span_t *span, sangomabc_short_event_t *event)
{
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

    mcon->rxseq_reset = 0;
	ftdm_set_flag((&sangoma_boost_data->mcon), MSU_FLAG_DOWN);
	ftdm_set_flag_locked(span, FTDM_SPAN_SUSPENDED);
	ftdm_set_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RESTARTING);
	
}

/**
 * \brief Handler for incoming digit event
 * \param mcon sangoma boost connection
 * \param span Span where event was fired
 * \param event Event to handle
 */
static void handle_incoming_digit(sangomabc_connection_t *mcon, ftdm_span_t *span, sangomabc_event_t *event)
{
	ftdm_channel_t *ftdmchan = NULL;
	char digits[MAX_DIALED_DIGITS + 2] = "";
	
	if (!(ftdmchan = find_ftdmchan(span, (sangomabc_short_event_t *)event, 1))) {
		ftdm_log(FTDM_LOG_ERROR, "Invalid channel\n");
		return;
	}
	
	if (event->called_number_digits_count == 0) {
		ftdm_log(FTDM_LOG_WARNING, "Error Incoming digit with len %s %d [w%dg%d]\n",
			   	event->called_number_digits,
			   	event->called_number_digits_count,
			   	event->span+1, event->chan+1);
		return;
	}

	ftdm_log(FTDM_LOG_WARNING, "Incoming digit with len %s %d [w%dg%d]\n",
			   	event->called_number_digits,
			   	event->called_number_digits_count,
			   	event->span+1, event->chan+1);

	memcpy(digits, event->called_number_digits, event->called_number_digits_count);
	ftdm_channel_queue_dtmf(ftdmchan, digits);

	return;
}


/**
 * \brief Checks if span has state changes pending and processes 
 * \param span Span where event was fired
 * \param event Event to handle
 * \return The locked FTDM channel associated to the event if any, NULL otherwise
 */
static ftdm_channel_t* event_process_states(ftdm_span_t *span, sangomabc_short_event_t *event) 
{
    ftdm_channel_t *ftdmchan = NULL;
    
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
        case SIGBOOST_EVENT_CALL_RELEASED:
            if (!(ftdmchan = find_ftdmchan(span, (sangomabc_short_event_t*)event, 1))) {
                ftdm_log(FTDM_LOG_DEBUG, "PROCESS STATES  CANT FIND CHAN %d:%d\n", event->span+1,event->chan+1);
                return NULL;
            }
            break;
        case SIGBOOST_EVENT_HEARTBEAT:
        case SIGBOOST_EVENT_SYSTEM_RESTART_ACK:
        case SIGBOOST_EVENT_SYSTEM_RESTART:
        case SIGBOOST_EVENT_AUTO_CALL_GAP_ABATE:
            return NULL;
        default:
            ftdm_log(FTDM_LOG_CRIT, "Unhandled event id:%d\n", event->event_id);
            return NULL;
    }

    ftdm_mutex_lock(ftdmchan->mutex);
    advance_chan_states(ftdmchan);
    return ftdmchan;
}

/**
 * \brief Handler for sangoma boost event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static int parse_sangoma_event(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
    ftdm_channel_t* ftdmchan = NULL;
	
	if (!ftdm_running()) {
		ftdm_log(FTDM_LOG_WARNING, "System is shutting down.\n");
		return -1;
	}

    ftdm_assert_return(event->call_setup_id <= MAX_REQ_ID, -1, "Unexpected call setup id\n");

	/* process all pending state changes for that channel before
	 * processing the new boost event */
	ftdmchan = event_process_states(span, event);

    switch(event->event_id) {
    case SIGBOOST_EVENT_CALL_START:
		handle_call_start(span, mcon, (sangomabc_event_t*)event);
		break;
    case SIGBOOST_EVENT_CALL_STOPPED:
		handle_call_stop(span, mcon, event);
		break;
    case SIGBOOST_EVENT_CALL_RELEASED:
		handle_call_released(span, mcon, event);
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
		ftdm_log(FTDM_LOG_WARNING, "No handler implemented for [%s]\n", sangomabc_event_id_name(event->event_id));
		break;
    }

    if(ftdmchan != NULL) {
        advance_chan_states(ftdmchan);
        ftdm_mutex_unlock(ftdmchan->mutex);
    }

	return 0;

}

/**
 * \brief Handler for channel state change
 * \param ftdmchan Channel to handle
 */
static __inline__ void state_advance(ftdm_channel_t *ftdmchan)
{

	ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;
	sangomabc_connection_t *mcon = &sangoma_boost_data->mcon;
	ftdm_sigmsg_t sig;
	ftdm_status_t status;

	ftdm_log(FTDM_LOG_DEBUG, "%d:%d PROCESSING STATE [%s]\n", ftdmchan->span_id, ftdmchan->chan_id, ftdm_channel_state2str(ftdmchan->state));
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;

	switch (ftdmchan->state) {
	case FTDM_CHANNEL_STATE_DOWN:
		{
			int call_stopped_ack_sent = 0;
			ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;

			if (ftdm_test_sflag(ftdmchan, SFLAG_FREE_REQ_ID)) {
				release_request_id_span_chan(ftdmchan->physical_span_id-1, ftdmchan->physical_chan_id-1);
			}

			if (!ftdm_test_sflag(ftdmchan, SFLAG_SENT_FINAL_MSG)) {
				ftdm_set_sflag_locked(ftdmchan, SFLAG_SENT_FINAL_MSG);

				if (ftdmchan->call_data && ((uint32_t)(intptr_t)ftdmchan->call_data == SIGBOOST_EVENT_CALL_START_NACK)) {
					sangomabc_exec_command(mcon,
									BOOST_SPAN(ftdmchan),
									BOOST_CHAN(ftdmchan),
									0,
									SIGBOOST_EVENT_CALL_START_NACK_ACK,
									0, 0);
					
				} else {
					/* we got a call stop msg, time to reply with call stopped ack  */
					sangomabc_exec_command(mcon,
									BOOST_SPAN(ftdmchan),
									BOOST_CHAN(ftdmchan),
									0,
									SIGBOOST_EVENT_CALL_STOPPED_ACK,
									0, 0);
					call_stopped_ack_sent = 1;
				}
			}
			ftdmchan->sflags = 0;
			ftdmchan->call_data = NULL;
			if (sangoma_boost_data->sigmod && call_stopped_ack_sent) {
				/* we dont want to call ftdm_channel_done just yet until call released is received */
				ftdm_log(FTDM_LOG_DEBUG, "Waiting for call release confirmation before declaring chan %d:%d as available \n", 
						ftdmchan->span_id, ftdmchan->chan_id);
			} else {
				ftdm_channel_done(ftdmchan);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS_MEDIA;
				if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!ftdm_test_sflag(ftdmchan, SFLAG_SENT_ACK)) {
					ftdm_set_sflag(ftdmchan, SFLAG_SENT_ACK);
					sangomabc_exec_command(mcon,
								BOOST_SPAN(ftdmchan),
								BOOST_CHAN(ftdmchan),
								0,
								SIGBOOST_EVENT_CALL_START_ACK,
								0, SIGBOOST_PROGRESS_MEDIA);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_PROGRESS:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS;
				if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!ftdm_test_sflag(ftdmchan, SFLAG_SENT_ACK)) {
					ftdm_set_sflag(ftdmchan, SFLAG_SENT_ACK);
					sangomabc_exec_command(mcon,
								BOOST_SPAN(ftdmchan),
								BOOST_CHAN(ftdmchan),
								0,
								SIGBOOST_EVENT_CALL_START_ACK,
								0, SIGBOOST_PROGRESS_RING);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_IDLE:
	case FTDM_CHANNEL_STATE_HOLD:
		{
			/* twiddle */
		}
		break;
	case FTDM_CHANNEL_STATE_RING:
		{
			if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_START;
				if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RESTART:
		{
			sig.event_id = FTDM_SIGEVENT_RESTART;
			status = ftdm_span_send_signal(ftdmchan->span, &sig);
			ftdm_set_sflag_locked(ftdmchan, SFLAG_SENT_FINAL_MSG);
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;
	case FTDM_CHANNEL_STATE_UP:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_UP;
				if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!(ftdm_test_flag(ftdmchan, FTDM_CHANNEL_PROGRESS) || ftdm_test_flag(ftdmchan, FTDM_CHANNEL_MEDIA))) {
					sangomabc_exec_command(mcon,
									   BOOST_SPAN(ftdmchan),
									   BOOST_CHAN(ftdmchan),								   
									   0,
									   SIGBOOST_EVENT_CALL_START_ACK,
									   0, 0);
				}
				
				sangomabc_exec_command(mcon,
								   BOOST_SPAN(ftdmchan),
								   BOOST_CHAN(ftdmchan),								   
								   0,
								   SIGBOOST_EVENT_CALL_ANSWERED,
								   0, 0);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_DIALING:
		{
		}
		break;
	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
		{
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;
	case FTDM_CHANNEL_STATE_HANGUP:
		{
			ftdm_set_sflag_locked(ftdmchan, SFLAG_HANGUP);

			if (ftdm_test_sflag(ftdmchan, SFLAG_SENT_FINAL_MSG) || ftdm_test_sflag(ftdmchan, SFLAG_TERMINATING)) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
			} else {
				ftdm_set_sflag_locked(ftdmchan, SFLAG_SENT_FINAL_MSG);
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_ANSWERED) || 
						ftdm_test_flag(ftdmchan, FTDM_CHANNEL_PROGRESS) || 
						ftdm_test_flag(ftdmchan, FTDM_CHANNEL_MEDIA) ||
						ftdm_test_sflag(ftdmchan, SFLAG_RECVD_ACK)) {
					sangomabc_exec_command(mcon,
									   BOOST_SPAN(ftdmchan),
									   BOOST_CHAN(ftdmchan),
									   0,
									   SIGBOOST_EVENT_CALL_STOPPED,
									   ftdmchan->caller_data.hangup_cause, 0);
				} else {
					sangomabc_exec_command(mcon,
									   BOOST_SPAN(ftdmchan),
									   BOOST_CHAN(ftdmchan),								   
									   0,
									   SIGBOOST_EVENT_CALL_START_NACK,
									   ftdmchan->caller_data.hangup_cause, 0);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			ftdm_set_sflag_locked(ftdmchan, SFLAG_TERMINATING);
			sig.event_id = FTDM_SIGEVENT_STOP;
			status = ftdm_span_send_signal(ftdmchan->span, &sig);
		}
		break;
	case FTDM_CHANNEL_STATE_IN_LOOP:
		{
			/* nothing to do, we sent the FTDM_COMMAND_ENABLE_LOOP command in handle_call_loop_start() right away */
		}
		break;
	default:
		break;
	}
}

static __inline__ void advance_chan_states(ftdm_channel_t *ftdmchan)
{
	while (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
		ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);
		state_advance(ftdmchan);
		ftdm_channel_complete_state(ftdmchan);
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
static __inline__ void check_state(ftdm_span_t *span)
{
	ftdm_channel_t *ftdmchan = NULL;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	int susp = ftdm_test_flag(span, FTDM_SPAN_SUSPENDED);
	
	if (susp && ftdm_check_state_all(span, FTDM_CHANNEL_STATE_DOWN)) {
		susp = 0;
	}

	if (ftdm_test_flag(span, FTDM_SPAN_STATE_CHANGE) || susp) {
		uint32_t j;
		ftdm_clear_flag_locked(span, FTDM_SPAN_STATE_CHANGE);
		if (susp) {
			for (j = 0; j <= span->chan_count; j++) {
				ftdm_mutex_lock(span->channels[j]->mutex);
				ftdm_clear_flag((span->channels[j]), FTDM_CHANNEL_STATE_CHANGE);
				ftdm_channel_set_state(span->channels[j], FTDM_CHANNEL_STATE_RESTART, 0);
				state_advance(span->channels[j]);
				ftdm_channel_complete_state(span->channels[j]);
				ftdm_mutex_unlock(span->channels[j]->mutex);
			}
		} else {
			while ((ftdmchan = ftdm_queue_dequeue(span->pendingchans))) {
				/* it can happen that someone else processed the chan states
				 * but without taking the chan out of the queue, so check th
				 * flag before advancing the state */
				ftdm_mutex_lock(ftdmchan->mutex);
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
					ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);
					state_advance(ftdmchan);
					ftdm_channel_complete_state(ftdmchan);
				}
				ftdm_mutex_unlock(ftdmchan->mutex);
			}
		}
	}

	if (ftdm_test_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RESTARTING)) {
		if (ftdm_check_state_all(span, FTDM_CHANNEL_STATE_DOWN)) {
			sangomabc_exec_command(&sangoma_boost_data->mcon,
								   0,
								   0,
								   -1,
								   SIGBOOST_EVENT_SYSTEM_RESTART_ACK,
								   0, 0);	
			ftdm_clear_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RESTARTING);
			ftdm_clear_flag_locked(span, FTDM_SPAN_SUSPENDED);
			ftdm_clear_flag((&sangoma_boost_data->mcon), MSU_FLAG_DOWN);
			init_outgoing_array();
		}
	}
}


/**
 * \brief Checks for events on a span
 * \param span Span to check for events
 */
static __inline__ ftdm_status_t check_events(ftdm_span_t *span, int ms_timeout)
{
	ftdm_status_t status;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

	status = ftdm_span_poll_event(span, ms_timeout);

	switch(status) {
	case FTDM_SUCCESS:
		{
			ftdm_event_t *event;
			while (ftdm_span_next_event(span, &event) == FTDM_SUCCESS) {
				switch (event->enum_id) {
				case FTDM_OOB_ALARM_TRAP:
					if (sangoma_boost_data->sigmod) {
						sangoma_boost_data->sigmod->on_hw_link_status_change(event->channel, FTDM_HW_LINK_DISCONNECTED);
					}
					break;
				case FTDM_OOB_ALARM_CLEAR:
					if (sangoma_boost_data->sigmod) {
						sangoma_boost_data->sigmod->on_hw_link_status_change(event->channel, FTDM_HW_LINK_CONNECTED);
					}
					break;
				}
			}
		}
		break;
	case FTDM_FAIL:
		{
			if (!ftdm_running()) {
				break;
			}
			ftdm_log(FTDM_LOG_ERROR, "Boost Check Event Failure Failure: %s\n", span->last_error);
			return FTDM_FAIL;
		}
		break;
	default:
		break;
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Main thread function for sangoma boost span (monitor)
 * \param me Current thread
 * \param obj Span to run in this thread
 */
static void *ftdm_sangoma_events_run(ftdm_thread_t *me, void *obj)
{
    ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	unsigned errs = 0;

	while (ftdm_test_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RUNNING) && ftdm_running()) {
		if (check_events(span,100) != FTDM_SUCCESS) {
			if (errs++ > 50) {
				ftdm_log(FTDM_LOG_ERROR, "Too many event errors, quitting sangoma events thread\n");
				return NULL;
			}
		}
	}

	return NULL;
}

static ftdm_status_t ftdm_boost_connection_open(ftdm_span_t *span)
{
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	if (sangoma_boost_data->sigmod) {
		if (sangoma_boost_data->sigmod->start_span(span) != FTDM_SUCCESS) {
			return FTDM_FAIL;
		}
		ftdm_clear_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RESTARTING);
		ftdm_clear_flag_locked(span, FTDM_SPAN_SUSPENDED);
		ftdm_clear_flag((&sangoma_boost_data->mcon), MSU_FLAG_DOWN);
	} 

	sangoma_boost_data->pcon = sangoma_boost_data->mcon;

	/* when sigmod is present, all arguments: local_ip etc, are ignored by sangomabc_connection_open */
	if (sangomabc_connection_open(&sangoma_boost_data->mcon,
								  sangoma_boost_data->mcon.cfg.local_ip,
								  sangoma_boost_data->mcon.cfg.local_port,
								  sangoma_boost_data->mcon.cfg.remote_ip,
								  sangoma_boost_data->mcon.cfg.remote_port) < 0) {
		ftdm_log(FTDM_LOG_ERROR, "Error: Opening MCON Socket [%d] %s\n", sangoma_boost_data->mcon.socket, strerror(errno));
		return FTDM_FAIL;
	}

	if (sangomabc_connection_open(&sangoma_boost_data->pcon,
							  sangoma_boost_data->pcon.cfg.local_ip,
							  ++sangoma_boost_data->pcon.cfg.local_port,
							  sangoma_boost_data->pcon.cfg.remote_ip,
							  ++sangoma_boost_data->pcon.cfg.remote_port) < 0) {
		ftdm_log(FTDM_LOG_ERROR, "Error: Opening PCON Socket [%d] %s\n", sangoma_boost_data->pcon.socket, strerror(errno));
		return FTDM_FAIL;
    }

	/* try to create the boost sockets interrupt objects */
	if (ftdm_interrupt_create(&sangoma_boost_data->pcon.sock_interrupt, sangoma_boost_data->pcon.socket) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Span %s could not create its boost msock interrupt!\n", span->name);
		return FTDM_FAIL;
	}

	if (ftdm_interrupt_create(&sangoma_boost_data->mcon.sock_interrupt, sangoma_boost_data->mcon.socket) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Span %s could not create its boost psock interrupt!\n", span->name);
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
}

/*! 
  \brief wait for a boost event 
  \return -1 on error, 0 on timeout, 1 when there are events
 */
static int ftdm_boost_wait_event(ftdm_span_t *span)
{
		ftdm_status_t res;
		ftdm_interrupt_t *ints[3];
		int numints;
		ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

		ftdm_queue_get_interrupt(span->pendingchans, &ints[0]);
		numints = 1;
		/* if in queue mode wait for both the pendingchans queue and the boost msg queue */
		if (sangoma_boost_data->sigmod) {
			ftdm_queue_get_interrupt(sangoma_boost_data->boost_queue, &ints[1]);
			numints = 2;
		} 
#ifndef __WINDOWS__
		else {
			/* socket mode ... */
			ints[1] = sangoma_boost_data->mcon.sock_interrupt;
			ints[2] = sangoma_boost_data->pcon.sock_interrupt;
			numints = 3;
			sangoma_boost_data->iteration = 0;
		}
#endif
		res = ftdm_interrupt_multiple_wait(ints, numints, -1);
		if (FTDM_SUCCESS != res) {
			ftdm_log(FTDM_LOG_CRIT, "Unexpected return value from interrupt waiting: %d\n", res);
			return -1;
		}
		return 0;
}


static sangomabc_event_t *ftdm_boost_read_event(ftdm_span_t *span)
{
	sangomabc_event_t *event = NULL;
	sangomabc_connection_t *mcon, *pcon;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

	mcon = &sangoma_boost_data->mcon;
	pcon = &sangoma_boost_data->pcon;

	event = sangomabc_connection_readp(pcon, sangoma_boost_data->iteration);

	/* if there is no event and this is not a sigmod-driven span it's time to try the other connection for events */
	if (!event && !sangoma_boost_data->sigmod) {
		event = sangomabc_connection_read(mcon, sangoma_boost_data->iteration);
	}

	return event;
}

/**
 * \brief Main thread function for sangoma boost span (monitor)
 * \param me Current thread
 * \param obj Span to run in this thread
 */
static void *ftdm_sangoma_boost_run(ftdm_thread_t *me, void *obj)
{
	ftdm_span_t *span = (ftdm_span_t *) obj;
	sangomabc_connection_t *mcon, *pcon;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

	mcon = &sangoma_boost_data->mcon;
	pcon = &sangoma_boost_data->pcon;	

	/* sigmod overrides socket functionality if not null */
	if (sangoma_boost_data->sigmod) {
		mcon->span = span;
		pcon->span = span;
		/* everything could be retrieved through span, but let's use shortcuts */
		mcon->sigmod = sangoma_boost_data->sigmod;
		pcon->sigmod = sangoma_boost_data->sigmod;
		mcon->boost_queue = sangoma_boost_data->boost_queue;
		pcon->boost_queue = sangoma_boost_data->boost_queue;
	}

	if (ftdm_boost_connection_open(span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "ftdm_boost_connection_open failed\n");
		goto end;
	}

	init_outgoing_array();
	if (!sangoma_boost_data->sigmod) {
		sangomabc_exec_commandp(pcon,
						   0,
						   0,
						   -1,
						   SIGBOOST_EVENT_SYSTEM_RESTART,
						   0);
		ftdm_set_flag(mcon, MSU_FLAG_DOWN);
	}

	while (ftdm_test_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RUNNING)) {
		sangomabc_event_t *event = NULL;
		
		if (!ftdm_running()) {
			if (!sangoma_boost_data->sigmod) {
				sangomabc_exec_commandp(pcon,
								   0,
								   0,
								   -1,
								   SIGBOOST_EVENT_SYSTEM_RESTART,
								   0);
				ftdm_set_flag(mcon, MSU_FLAG_DOWN);
			}
			ftdm_log(FTDM_LOG_DEBUG, "ftdm is no longer running\n");
			break;
		}

		if (ftdm_boost_wait_event(span) < 0) {
			ftdm_log(FTDM_LOG_ERROR, "ftdm_boost_wait_event failed\n");
			goto error;
		}
		
		while ((event = ftdm_boost_read_event(span))) {
			parse_sangoma_event(span, pcon, (sangomabc_short_event_t*)event);
			sangoma_boost_data->iteration++;
		}
		
		check_state(span);
	}

	goto end;

error:
	ftdm_log(FTDM_LOG_CRIT, "Boost event processing Error!\n");

end:
	if (!sangoma_boost_data->sigmod) {
		sangomabc_connection_close(&sangoma_boost_data->mcon);
		sangomabc_connection_close(&sangoma_boost_data->pcon);
	}

	ftdm_clear_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RUNNING);

	ftdm_log(FTDM_LOG_DEBUG, "Sangoma Boost thread ended.\n");
	return NULL;
}

/**
 * \brief Loads sangoma boost signaling module
 * \param fio FreeTDM IO interface
 * \return Success
 */
static FIO_SIG_LOAD_FUNCTION(ftdm_sangoma_boost_init)
{
	g_boost_modules_hash = create_hashtable(10, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
	if (!g_boost_modules_hash) {
		return FTDM_FAIL;
	}
	ftdm_mutex_create(&request_mutex);
	ftdm_mutex_create(&g_boost_modules_mutex);
	memset(&g_trunkgroups[0], 0, sizeof(g_trunkgroups));
	return FTDM_SUCCESS;
}

static FIO_SIG_UNLOAD_FUNCTION(ftdm_sangoma_boost_destroy)
{
	ftdm_hash_iterator_t *i = NULL;
	boost_sigmod_interface_t *sigmod = NULL;
	const void *key = NULL;
	void *val = NULL;
	ftdm_dso_lib_t lib;

	for (i = hashtable_first(g_boost_modules_hash); i; i = hashtable_next(i)) {
		hashtable_this(i, &key, NULL, &val);
		if (key && val) {
			sigmod = val;
			lib = sigmod->pvt;
			ftdm_dso_destroy(&lib);
		}
	}

	hashtable_destroy(g_boost_modules_hash);
	ftdm_mutex_destroy(&request_mutex);
	ftdm_mutex_destroy(&g_boost_modules_mutex);
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_sangoma_boost_start(ftdm_span_t *span)
{
	int err;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	ftdm_set_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RUNNING);
	err=ftdm_thread_create_detached(ftdm_sangoma_boost_run, span);
	if (err) {
		ftdm_clear_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RUNNING);
		return err;
	}
	// launch the events thread to handle HW DTMF and possibly
	// other events in the future
	err=ftdm_thread_create_detached(ftdm_sangoma_events_run, span);
	if (err) {
		ftdm_clear_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RUNNING);
	}
	return err;
}

static ftdm_status_t ftdm_sangoma_boost_stop(ftdm_span_t *span)
{
	int cnt = 10;
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	if (sangoma_boost_data->sigmod) {

		/* FIXME: we should make sure the span thread is stopped (use pthread_kill or freetdm thread kill function) */
		/* I think stopping the span before destroying the queue makes sense
		   otherwise may be boost events would still arrive when the queue is already destroyed! */
		status = sangoma_boost_data->sigmod->stop_span(span);

		ftdm_queue_enqueue(sangoma_boost_data->boost_queue, NULL);
		while(ftdm_test_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RUNNING) && cnt-- > 0) {
			ftdm_log(FTDM_LOG_DEBUG, "Waiting for boost thread\n");
			ftdm_sleep(500);
		}
		ftdm_queue_destroy(&sangoma_boost_data->boost_queue);
		return status;
	}
	return status;
}

static ftdm_state_map_t boost_state_map = {
	{
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_ANY_STATE},
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_DIALING, FTDM_CHANNEL_STATE_IDLE, FTDM_CHANNEL_STATE_HOLD, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HOLD, FTDM_END},
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, 
			 FTDM_CHANNEL_STATE_IDLE, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_IDLE, FTDM_CHANNEL_STATE_DIALING, FTDM_END},
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_IDLE, FTDM_CHANNEL_STATE_DIALING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
		},

		/****************************************/
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_ANY_STATE},
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN},
			{FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_IN_LOOP},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
		},
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
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA,FTDM_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
		},
		

	}
};

static BOOST_WRITE_MSG_FUNCTION(ftdm_boost_write_msg)
{
	sangomabc_short_event_t *shortmsg = NULL;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = NULL;
	sangomabc_queue_element_t *element = NULL;

	ftdm_assert_return(msg != NULL, FTDM_FAIL, "Boost message to write was null");

	if (!span) {
		shortmsg = msg;
		ftdm_log(FTDM_LOG_ERROR, "Unexpected boost message %d\n", shortmsg->event_id);
		return FTDM_FAIL;
	}
	/* duplicate the event and enqueue it */
	element = ftdm_calloc(1, sizeof(*element));
	if (!element) {
		return FTDM_FAIL;
	}
	memcpy(element->boostmsg, msg, msglen);
	element->size = msglen;

	sangoma_boost_data = span->signal_data;
	return ftdm_queue_enqueue(sangoma_boost_data->boost_queue, element);
}

static BOOST_SIG_STATUS_CB_FUNCTION(ftdm_boost_sig_status_change)
{
	ftdm_sigmsg_t sig;
	ftdm_log(FTDM_LOG_NOTICE, "%d:%d Signaling link status changed to %s\n", ftdmchan->span_id, ftdmchan->chan_id, ftdm_signaling_status2str(status));
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;
	sig.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
	sig.raw_data = &status;
	ftdm_span_send_signal(ftdmchan->span, &sig);
	return;
}

static FIO_CHANNEL_SET_SIG_STATUS_FUNCTION(sangoma_boost_set_channel_sig_status)
{
	ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;
	if (!sangoma_boost_data->sigmod) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot set signaling status in boost channel with no signaling module configured\n");
		return FTDM_FAIL;
	}
	if (!sangoma_boost_data->sigmod->set_channel_sig_status) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot set signaling status in boost channel: method not implemented\n");
		return FTDM_NOTIMPL;
	}
	return sangoma_boost_data->sigmod->set_channel_sig_status(ftdmchan, status);
}

static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(sangoma_boost_get_channel_sig_status)
{
	ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;
	if (!sangoma_boost_data->sigmod) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot get signaling status in boost channel with no signaling module configured\n");
		return FTDM_FAIL;
	}
	if (!sangoma_boost_data->sigmod->get_channel_sig_status) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot get signaling status in boost channel: method not implemented\n");
		return FTDM_NOTIMPL;
	}
	return sangoma_boost_data->sigmod->get_channel_sig_status(ftdmchan, status);
}

static FIO_SPAN_SET_SIG_STATUS_FUNCTION(sangoma_boost_set_span_sig_status)
{
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	if (!sangoma_boost_data->sigmod) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot set signaling status in boost span with no signaling module configured\n");
		return FTDM_FAIL;
	}
	if (!sangoma_boost_data->sigmod->set_span_sig_status) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot set signaling status in boost span: method not implemented\n");
		return FTDM_NOTIMPL;
	}
	return sangoma_boost_data->sigmod->set_span_sig_status(span, status);
}

static FIO_SPAN_GET_SIG_STATUS_FUNCTION(sangoma_boost_get_span_sig_status)
{
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	if (!sangoma_boost_data->sigmod) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot get signaling status in boost span with no signaling module configured\n");
		return FTDM_FAIL;
	}
	if (!sangoma_boost_data->sigmod->get_span_sig_status) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot get signaling status in boost span: method not implemented\n");
		return FTDM_NOTIMPL;
	}
	return sangoma_boost_data->sigmod->get_span_sig_status(span, status);
}

/* TODO: move these ones to a common private header so other ISDN mods can use them */
static void ftdm_span_set_npi(const char *npi_string, uint8_t *target)
{
	if (!strcasecmp(npi_string, "isdn") || !strcasecmp(npi_string, "e164")) {
		*target = FTDM_NPI_ISDN;
	} else if (!strcasecmp(npi_string, "data")) {
		*target = FTDM_NPI_DATA;
	} else if (!strcasecmp(npi_string, "telex")) {
		*target = FTDM_NPI_TELEX;
	} else if (!strcasecmp(npi_string, "national")) {
		*target = FTDM_NPI_NATIONAL;
	} else if (!strcasecmp(npi_string, "private")) {
		*target = FTDM_NPI_PRIVATE;
	} else if (!strcasecmp(npi_string, "reserved")) {
		*target = FTDM_NPI_RESERVED;
	} else if (!strcasecmp(npi_string, "unknown")) {
		*target = FTDM_NPI_UNKNOWN;
	} else {
		ftdm_log(FTDM_LOG_WARNING, "Invalid NPI value (%s)\n", npi_string);
		*target = FTDM_NPI_UNKNOWN;
	}
}

static void ftdm_span_set_ton(const char *ton_string, uint8_t *target)
{
	if (!strcasecmp(ton_string, "national")) {
		*target = FTDM_TON_NATIONAL;
	} else if (!strcasecmp(ton_string, "international")) {
		*target = FTDM_TON_INTERNATIONAL;
	} else if (!strcasecmp(ton_string, "local")) {
		*target = FTDM_TON_SUBSCRIBER_NUMBER;
	} else if (!strcasecmp(ton_string, "unknown")) {
		*target = FTDM_TON_UNKNOWN;
	} else {
		ftdm_log(FTDM_LOG_WARNING, "Invalid TON value (%s)\n", ton_string);
		*target = FTDM_TON_UNKNOWN;
	}
}


/**
 * \brief Initialises an sangoma boost span from configuration variables
 * \param span Span to configure
 * \param sig_cb Callback function for event signals
 * \param ap List of configuration variables
 * \return Success or failure
 */
static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_sangoma_boost_configure_span)
{
#define FAIL_CONFIG_RETURN(retstatus) \
		if (sangoma_boost_data) \
			ftdm_safe_free(sangoma_boost_data); \
		if (err) \
			ftdm_safe_free(err) \
		if (hash_locked) \
			ftdm_mutex_unlock(g_boost_modules_mutex); \
		if (lib) \
			ftdm_dso_destroy(&lib); \
		return retstatus;

	boost_sigmod_interface_t *sigmod_iface = NULL;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = NULL;
	const char *local_ip = "127.0.0.65", *remote_ip = "127.0.0.66";
	const char *sigmod = NULL;
	int local_port = 53000, remote_port = 53000;
	const char *var = NULL, *val = NULL;
	int hash_locked = 0;
	ftdm_dso_lib_t lib = NULL;
	char path[255] = "";
	char *err = NULL;
	unsigned paramindex = 0;
	ftdm_status_t rc = FTDM_SUCCESS;

	for (; ftdm_parameters[paramindex].var; paramindex++) {
		var = ftdm_parameters[paramindex].var;
		val = ftdm_parameters[paramindex].val;
		if (!strcasecmp(var, "sigmod")) {
			sigmod = val;
		} else if (!strcasecmp(var, "local_ip")) {
			local_ip = val;
		} else if (!strcasecmp(var, "remote_ip")) {
			remote_ip = val;
		} else if (!strcasecmp(var, "local_port")) {
			local_port = atoi(val);
		} else if (!strcasecmp(var, "remote_port")) {
			remote_port = atoi(val);
		} else if (!strcasecmp(var, "outbound-called-ton")) {
			ftdm_span_set_ton(val, &span->default_caller_data.dnis.type);
		} else if (!strcasecmp(var, "outbound-called-npi")) {
			ftdm_span_set_npi(val, &span->default_caller_data.dnis.plan);
		} else if (!strcasecmp(var, "outbound-calling-ton")) {
			ftdm_span_set_ton(val, &span->default_caller_data.cid_num.type);
		} else if (!strcasecmp(var, "outbound-calling-npi")) {
			ftdm_span_set_npi(val, &span->default_caller_data.cid_num.plan);
		} else if (!strcasecmp(var, "outbound-rdnis-ton")) {
			ftdm_span_set_ton(val, &span->default_caller_data.rdnis.type);
		} else if (!strcasecmp(var, "outbound-rdnis-npi")) {
			ftdm_span_set_npi(val, &span->default_caller_data.rdnis.plan);
		} else if (!sigmod) {
			snprintf(span->last_error, sizeof(span->last_error), "Unknown parameter [%s]", var);
			FAIL_CONFIG_RETURN(FTDM_FAIL);
		}
	}

	if (!sigmod) {
		if (!local_ip && local_port && remote_ip && remote_port && sig_cb) {
			ftdm_set_string(span->last_error, "missing Sangoma boost IP parameters");
			FAIL_CONFIG_RETURN(FTDM_FAIL);
		}
	}

	sangoma_boost_data = ftdm_calloc(1, sizeof(*sangoma_boost_data));
	if (!sangoma_boost_data) {
		FAIL_CONFIG_RETURN(FTDM_FAIL);
	}

	/* WARNING: be sure to release this mutex on errors inside this if() */
	ftdm_mutex_lock(g_boost_modules_mutex);
	hash_locked = 1;
	if (sigmod && !(sigmod_iface = hashtable_search(g_boost_modules_hash, (void *)sigmod))) {
		ftdm_build_dso_path(sigmod, path, sizeof(path));	
		lib = ftdm_dso_open(path, &err);
		if (!lib) {
			ftdm_log(FTDM_LOG_ERROR, "Error loading Sangoma boost signaling module '%s': %s\n", path, err);
			snprintf(span->last_error, sizeof(span->last_error), "Failed to load sangoma boost signaling module %s", path);

			FAIL_CONFIG_RETURN(FTDM_FAIL);
		}
		if (!(sigmod_iface = (boost_sigmod_interface_t *)ftdm_dso_func_sym(lib, BOOST_INTERFACE_NAME_STR, &err))) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to read Sangoma boost signaling module interface '%s': %s\n", path, err);
			snprintf(span->last_error, sizeof(span->last_error), "Failed to read Sangoma boost signaling module interface '%s': %s", path, err);

			FAIL_CONFIG_RETURN(FTDM_FAIL);
		}
		rc = sigmod_iface->on_load();
		if (rc != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to load Sangoma boost signaling module interface '%s': on_load method failed (%d)\n", path, rc);
			FAIL_CONFIG_RETURN(FTDM_FAIL);
		}
		sigmod_iface->pvt = lib;
		sigmod_iface->set_write_msg_cb(ftdm_boost_write_msg);
		sigmod_iface->set_sig_status_cb(ftdm_boost_sig_status_change);
		hashtable_insert(g_boost_modules_hash, (void *)sigmod_iface->name, sigmod_iface, HASHTABLE_FLAG_NONE);
		lib = NULL; /* destroying the lib will be done when going down and NOT on FAIL_CONFIG_RETURN */
	}
	ftdm_mutex_unlock(g_boost_modules_mutex);
	hash_locked = 0;

	if (sigmod_iface) {
		/* try to create the boost queue */
		if (ftdm_queue_create(&sangoma_boost_data->boost_queue, BOOST_QUEUE_SIZE) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Span %s could not create its boost queue!\n", span->name);
			FAIL_CONFIG_RETURN(FTDM_FAIL);
		}
		ftdm_log(FTDM_LOG_NOTICE, "Span %s will use Sangoma Boost Signaling Module %s\n", span->name, sigmod_iface->name);
		sangoma_boost_data->sigmod = sigmod_iface;
		sigmod_iface->configure_span(span, ftdm_parameters);
	} else {
		ftdm_log(FTDM_LOG_NOTICE, "Span %s will use boost socket mode\n", span->name);
		ftdm_set_string(sangoma_boost_data->mcon.cfg.local_ip, local_ip);
		sangoma_boost_data->mcon.cfg.local_port = local_port;
		ftdm_set_string(sangoma_boost_data->mcon.cfg.remote_ip, remote_ip);
		sangoma_boost_data->mcon.cfg.remote_port = remote_port;
	}
	span->signal_cb = sig_cb;
	span->start = ftdm_sangoma_boost_start;
	span->stop = ftdm_sangoma_boost_stop;
	span->signal_data = sangoma_boost_data;
	span->signal_type = FTDM_SIGTYPE_SANGOMABOOST;
	span->outgoing_call = sangoma_boost_outgoing_call;
	span->channel_request = sangoma_boost_channel_request;
	span->get_channel_sig_status = sangoma_boost_get_channel_sig_status;
	span->set_channel_sig_status = sangoma_boost_set_channel_sig_status;
	span->get_span_sig_status = sangoma_boost_get_span_sig_status;
	span->set_span_sig_status = sangoma_boost_set_span_sig_status;
	span->state_map = &boost_state_map;
	ftdm_clear_flag(span, FTDM_SPAN_SUGGEST_CHAN_ID);
	ftdm_set_flag(span, FTDM_SPAN_USE_CHAN_QUEUE);
	if (sigmod_iface) {
		/* the core will do the hunting */
		span->channel_request = NULL;
	} 
	ftdm_set_flag_locked(span, FTDM_SPAN_SUSPENDED);
	return FTDM_SUCCESS;
}

/**
 * \brief FreeTDM sangoma boost signaling module definition
 */
EX_DECLARE_DATA ftdm_module_t ftdm_module = { 
	/*.name =*/ "sangoma_boost",
	/*.io_load =*/ NULL,
	/*.io_unload =*/ NULL,
	/*.sig_load = */ ftdm_sangoma_boost_init,
	/*.sig_configure =*/ NULL,
	/*.sig_unload = */ftdm_sangoma_boost_destroy,
	/*.configure_span_signaling = */ ftdm_sangoma_boost_configure_span
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
