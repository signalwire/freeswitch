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
On WIN32 platform this code works with sigmod ONLY, don't try to make sense of any socket code for win32
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
	SFLAG_SENT_ACK = (1 << 2)
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
} sangoma_boost_request_t;

//#define MAX_REQ_ID FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN * FTDM_MAX_CHANNELS_PHYSICAL_SPAN
#define MAX_REQ_ID 6000

static uint16_t SETUP_GRID[FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN+1][FTDM_MAX_CHANNELS_PHYSICAL_SPAN+1] = {{ 0 }};

static sangoma_boost_request_t OUTBOUND_REQUESTS[MAX_REQ_ID+1] = {{ 0 }};

static ftdm_mutex_t *request_mutex = NULL;
static ftdm_mutex_t *signal_mutex = NULL;

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

	ftdm_mutex_lock(signal_mutex);
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
	ftdm_mutex_unlock(signal_mutex);

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
 * \brief determines whether media is ready
 * \param event boost event
 * \return 1 true, 0 for false
 */

static int boost_media_ready(sangomabc_event_t *event)
{
	/* FORMAT is of type: SMG003-EVI-1-MEDIA-# */
	char* p = NULL;
	p = strstr(event->isup_in_rdnis, "MEDIA");
	if (p) {
		int media_ready = 0;
		if ((sscanf(p, "MEDIA-%d", &media_ready)) == 1) {
			if (media_ready) {
				return 1;
			} else {
			}
		} else {
			ftdm_log(FTDM_LOG_ERROR, "Invalid boost isup_rdnis MEDIA format %s\n", p);
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
	int boost_request_timeout = 5000;
	sangoma_boost_request_status_t st;
	char dnis[128] = "";
	char *gr = NULL;
	uint32_t count = 0;
	int tg=0;
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

	if (sangoma_boost_data->sigmod) {
		*ftdmchan = span->channels[chan_id];

		event.span = (uint8_t) (*ftdmchan)->physical_span_id;
		event.chan = (uint8_t) (*ftdmchan)->physical_chan_id;

		ftdm_set_flag((*ftdmchan), FTDM_CHANNEL_OUTBOUND);
		ftdm_set_flag_locked((*ftdmchan), FTDM_CHANNEL_INUSE);

		OUTBOUND_REQUESTS[r].ftdmchan = *ftdmchan;
	} else {
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
	}

	ftdm_set_string(event.calling_name, caller_data->cid_name);
	ftdm_set_string(event.isup_in_rdnis, caller_data->rdnis.digits);
	if (strlen(caller_data->rdnis.digits)) {
			event.isup_in_rdnis_size = (uint16_t)strlen(caller_data->rdnis.digits)+1;
	}
    
	event.calling_number_screening_ind = caller_data->screen;
	event.calling_number_presentation = caller_data->pres;

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
			ftdm_log(FTDM_LOG_CRIT, "Timed out waiting for boost channel request response, current status: BST_WAITING\n");
			ftdm_log(FTDM_LOG_CRIT, "s%dc%d: Csid:%d Timed out waiting for boost channel request response, current status: BST_WAITING\n", (*ftdmchan)->physical_span_id, (*ftdmchan)->physical_chan_id, r);
			goto done;
		}
	}
	
	if (OUTBOUND_REQUESTS[r].status == BST_ACK && OUTBOUND_REQUESTS[r].ftdmchan) {
		*ftdmchan = OUTBOUND_REQUESTS[r].ftdmchan;
		status = FTDM_SUCCESS;
		(*ftdmchan)->init_state = FTDM_CHANNEL_STATE_PROGRESS;
		ftdm_log(FTDM_LOG_DEBUG, "Channel state changed to PROGRESS [Csid:%d]\n", r);
	}

	boost_request_timeout = 5000;
	while(ftdm_running() && OUTBOUND_REQUESTS[r].status == BST_ACK) {
		ftdm_sleep(1);
		if (--boost_request_timeout <= 0) {
			status = FTDM_FAIL;
			if (!sangoma_boost_data->sigmod) {
				*ftdmchan = NULL;
			}
			ftdm_log(FTDM_LOG_CRIT, "Timed out waiting for boost channel request response, current status: BST_ACK\n");
			goto done;
		}
		//printf("WTF %d\n", sanity);
	}

	if (OUTBOUND_REQUESTS[r].status == BST_READY && OUTBOUND_REQUESTS[r].ftdmchan) {
		*ftdmchan = OUTBOUND_REQUESTS[r].ftdmchan;
		status = FTDM_SUCCESS;
		(*ftdmchan)->init_state = FTDM_CHANNEL_STATE_PROGRESS_MEDIA;
		ftdm_log(FTDM_LOG_DEBUG, "Channel state changed to PROGRESS_MEDIA [Csid:%d]\n", r);
	} else {
		status = FTDM_FAIL;
		if (!sangoma_boost_data->sigmod) {
			*ftdmchan = NULL;
		}
	}

 done:
	
	st = OUTBOUND_REQUESTS[r].status;
	OUTBOUND_REQUESTS[r].status = BST_FREE;	
	
	if (st == BST_FAIL) {
		release_request_id(r);
	} else if (st != BST_READY) {
		assert(r <= MAX_REQ_ID);
		nack_map[r] = 1;
		if (sangoma_boost_data->sigmod) {
			sangomabc_exec_command(&sangoma_boost_data->mcon,
								BOOST_SPAN((*ftdmchan)),
								BOOST_CHAN((*ftdmchan)),
								r,
								SIGBOOST_EVENT_CALL_START_NACK,
								0);
		} else {
			sangomabc_exec_command(&sangoma_boost_data->mcon,
								0,
								0,
								r,
								SIGBOOST_EVENT_CALL_START_NACK,
								0);
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
	ftdm_status_t status = FTDM_SUCCESS;

	return status;
}

/**
 * \brief Handler for call start ack no media event
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_progress(sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_channel_t *ftdmchan;

	if (nack_map[event->call_setup_id]) {
		return;
	}

	//if we received a progress for this device already
	if (OUTBOUND_REQUESTS[event->call_setup_id].status == BST_ACK) {
		if (boost_media_ready((sangomabc_event_t*) event)) {
			OUTBOUND_REQUESTS[event->call_setup_id].status = BST_READY;
			ftdm_log(FTDM_LOG_DEBUG, "chan media ready %d:%d CSid:%d\n", event->span+1, event->chan+1, event->call_setup_id);
		}
		return;
	}

	OUTBOUND_REQUESTS[event->call_setup_id].event = *event;
	SETUP_GRID[event->span][event->chan] = event->call_setup_id;

	if ((ftdmchan = find_ftdmchan(OUTBOUND_REQUESTS[event->call_setup_id].span, (sangomabc_short_event_t*) event, 0))) {
		if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "OPEN ERROR [%s]\n", ftdmchan->last_error);
		} else {
			ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);
			ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_INUSE);
			ftdmchan->extra_id = event->call_setup_id;
			ftdm_log(FTDM_LOG_DEBUG, "Assign chan %d:%d (%d:%d) CSid=%d\n", ftdmchan->span_id, ftdmchan->chan_id, event->span+1,event->chan+1, event->call_setup_id);
			ftdmchan->sflags = 0;
			OUTBOUND_REQUESTS[event->call_setup_id].ftdmchan = ftdmchan;
			if (boost_media_ready((sangomabc_event_t*)event)) {
				OUTBOUND_REQUESTS[event->call_setup_id].status = BST_READY;
			} else {
				OUTBOUND_REQUESTS[event->call_setup_id].status = BST_ACK;
			}
			return;
		}
	} 
	
	//printf("WTF BAD ACK CSid=%d span=%d chan=%d\n", event->call_setup_id, event->span+1,event->chan+1);
	if ((ftdmchan = find_ftdmchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 1))) {
		//printf("WTF BAD ACK2 %d:%d (%d:%d) CSid=%d xtra_id=%d out=%d state=%s\n", ftdmchan->span_id, ftdmchan->chan_id, event->span+1,event->chan+1, event->call_setup_id, ftdmchan->extra_id, ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND), ftdm_channel_state2str(ftdmchan->state));
	}


	ftdm_log(FTDM_LOG_CRIT, "START PROGRESS CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	if (ftdmchan) {
		ftdm_set_sflag(ftdmchan, SFLAG_SENT_FINAL_MSG);
	}
	sangomabc_exec_command(mcon,
					   event->span,
					   event->chan,
					   event->call_setup_id,
					   SIGBOOST_EVENT_CALL_STOPPED,
					   FTDM_CAUSE_DESTINATION_OUT_OF_ORDER);
	OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;	
}

/**
 * \brief Handler for call start ack event
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_start_ack(sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	
	ftdm_channel_t *ftdmchan;
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
		ftdm_set_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);
		ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_INUSE);
	} else {
		ftdmchan = find_ftdmchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 0);
	}


	if (ftdmchan) {
		if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "OPEN ERROR [%s]\n", ftdmchan->last_error);
		} else {
			ftdmchan->extra_id = event->call_setup_id;
			ftdm_log(FTDM_LOG_DEBUG, "Assign chan %d:%d (%d:%d) CSid=%d\n", ftdmchan->span_id, ftdmchan->chan_id, event_span, event_chan, event->call_setup_id);
			ftdmchan->sflags = 0;
			OUTBOUND_REQUESTS[event->call_setup_id].ftdmchan = ftdmchan;
			OUTBOUND_REQUESTS[event->call_setup_id].status = BST_READY;
			return;
		}
	} 
	
	//printf("WTF BAD ACK CSid=%d span=%d chan=%d\n", event->call_setup_id, event->span+1,event->chan+1);
	if ((ftdmchan = find_ftdmchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 1))) {
		//printf("WTF BAD ACK2 %d:%d (%d:%d) CSid=%d xtra_id=%d out=%d state=%s\n", ftdmchan->span_id, ftdmchan->chan_id, event->span+1,event->chan+1, event->call_setup_id, ftdmchan->extra_id, ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND), ftdm_channel_state2str(ftdmchan->state));
	}

	ftdm_set_sflag(ftdmchan, SFLAG_SENT_FINAL_MSG);
	ftdm_log(FTDM_LOG_CRIT, "START ACK CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	sangomabc_exec_command(mcon,
					   event->span,
					   event->chan,
					   event->call_setup_id,
					   SIGBOOST_EVENT_CALL_STOPPED,
					   FTDM_CAUSE_DESTINATION_OUT_OF_ORDER);
	OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;
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
								0);
		}

		if (ftdmchan->state == FTDM_CHANNEL_STATE_DOWN || ftdmchan->state == FTDM_CHANNEL_STATE_HANGUP_COMPLETE) {
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

	ftdm_log(FTDM_LOG_DEBUG, "setting event->call_setup_id:%d to BST_FAIL\n", event->call_setup_id);
	OUTBOUND_REQUESTS[event->call_setup_id].event = *event;
	OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;
	if (!sangoma_boost_data->sigmod) {
		sangomabc_exec_command(mcon,
						   0,
						   0,
						   event->call_setup_id,
						   SIGBOOST_EVENT_CALL_START_NACK_ACK,
						   0);

		return;
	} else {
		if ((ftdmchan = find_ftdmchan(span, event, 1))) {
			int r = 0;
			assert(!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND));
	
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

#if 0
	if (ftdmchan) {
		ftdm_set_sflag_locked(ftdmchan, SFLAG_SENT_FINAL_MSG);
	}

	/* nobody else will do it so we have to do it ourselves */
	sangomabc_exec_command(mcon,
					   event->span,
					   event->chan,
					   0,
					   SIGBOOST_EVENT_CALL_START_NACK_ACK,
					   0);
#endif
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
		
		if (ftdmchan->state == FTDM_CHANNEL_STATE_HANGUP) {
			/* racing condition where both sides initiated a hangup 
			 * Do not change current state as channel is already clearing
			 * itself through local initiated hangup */
			
			sangomabc_exec_command(mcon,
						BOOST_SPAN(ftdmchan),
						BOOST_CHAN(ftdmchan),
						0,
						SIGBOOST_EVENT_CALL_STOPPED_ACK,
						0);
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
		int r = 0;

		if (ftdmchan->extra_id == event->call_setup_id && ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
			ftdm_mutex_lock(ftdmchan->mutex);
			if (ftdmchan->state == FTDM_CHANNEL_STATE_DOWN && ftdmchan->init_state != FTDM_CHANNEL_STATE_UP) {
				ftdmchan->init_state = FTDM_CHANNEL_STATE_UP;
				r = 1;
			} else {
				ftdm_set_state_r(ftdmchan, FTDM_CHANNEL_STATE_UP, 0, r);
			}
			ftdm_mutex_unlock(ftdmchan->mutex);
		} 
#if 0
		if (!r) {
			printf("WTF BAD ANSWER %d:%d (%d:%d) CSid=%d xtra_id=%d out=%d state=%s\n", ftdmchan->span_id, ftdmchan->chan_id, event->span+1,event->chan+1, event->call_setup_id, ftdmchan->extra_id, ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND), ftdm_channel_state2str(ftdmchan->state));
		}
#endif
	} else {
		ftdm_log(FTDM_LOG_CRIT, "ANSWER CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
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
	ftdm_channel_t *ftdmchan;

	if (!(ftdmchan = find_ftdmchan(span, (sangomabc_short_event_t*)event, 0))) {
		if (!(ftdmchan = find_ftdmchan(span, (sangomabc_short_event_t*)event, 1))) {
			ftdm_log(FTDM_LOG_CRIT, "START CANT FIND CHAN %d:%d AT ALL\n", event->span+1,event->chan+1);
			goto error;
		}
		/* this handles race conditions where state handlers are still pending to be executed for finished calls
		   but an incoming call arrives first, we must complete the channel states and then try again to get the 
		   ftdm channel */
		advance_chan_states(ftdmchan);
		if (!(ftdmchan = find_ftdmchan(span, (sangomabc_short_event_t*)event, 0))) {
			ftdm_log(FTDM_LOG_CRIT, "START CANT FIND CHAN %d:%d EVEN AFTER STATE ADVANCE\n", event->span+1,event->chan+1);
			goto error;
		}
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
	ftdm_set_string(ftdmchan->caller_data.cid_num.digits, (char *)event->calling_number_digits);
	ftdm_set_string(ftdmchan->caller_data.cid_name, (char *)event->calling_number_digits);
	if (strlen(event->calling_name)) {
		ftdm_set_string(ftdmchan->caller_data.cid_name, (char *)event->calling_name);
	}
	ftdm_set_string(ftdmchan->caller_data.ani.digits, (char *)event->calling_number_digits);
	ftdm_set_string(ftdmchan->caller_data.dnis.digits, (char *)event->called_number_digits);
	if (event->isup_in_rdnis_size) {
		char* p;

		//Set value of rdnis.digis in case prot daemon is still using older style RDNIS
		if (atoi((char *)event->isup_in_rdnis) > 0) {
			ftdm_set_string(ftdmchan->caller_data.rdnis.digits, (char *)event->isup_in_rdnis);
		}

		p = strstr((char*)event->isup_in_rdnis,"PRI001-ANI2-");
		if (p!=NULL) {
			int ani2 = 0;
			sscanf(p, "PRI001-ANI2-%d", &ani2);
			snprintf(ftdmchan->caller_data.aniII, 5, "%.2d", ani2);
		}	
		p = strstr((char*)event->isup_in_rdnis,"RDNIS-");
		if (p!=NULL) {
			sscanf(p, "RDNIS-%s", &ftdmchan->caller_data.rdnis.digits[0]);
		}
		
	}
	ftdmchan->caller_data.screen = event->calling_number_screening_ind;
	ftdmchan->caller_data.pres = event->calling_number_presentation;
	ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RING);
	return;

 error:
	sangomabc_exec_command(mcon,
					   event->span,
					   event->chan,
					   0,
					   SIGBOOST_EVENT_CALL_START_NACK,
					   0);
		
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
		ftdm_log(FTDM_LOG_CRIT, "Failed to tx on ISUP socket [%s]: %s\n", strerror(errno));
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
	
	mcon->hb_elapsed = 0;
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
 * \brief Handler for sangoma boost event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static int parse_sangoma_event(ftdm_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	ftdm_mutex_lock(signal_mutex);
	
	if (!ftdm_running()) {
		ftdm_log(FTDM_LOG_WARNING, "System is shutting down.\n");
		goto end;
	}

	assert(event->call_setup_id <= MAX_REQ_ID);
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
		handle_call_progress(mcon, event);
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
		//handle_call_loop_start(event);
		break;
    case SIGBOOST_EVENT_REMOVE_CHECK_LOOP:
		//handle_call_stop(event);
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

 end:

	ftdm_mutex_unlock(signal_mutex);

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

	ftdm_log(FTDM_LOG_DEBUG, "%d:%d STATE [%s]\n", ftdmchan->span_id, ftdmchan->chan_id, ftdm_channel_state2str(ftdmchan->state));
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;

	switch (ftdmchan->state) {
	case FTDM_CHANNEL_STATE_DOWN:
		{
			int call_stopped_ack_sent = 0;
			ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;
			if (ftdmchan->extra_id) {
				ftdmchan->extra_id = 0;
			}

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
									0);
					
				} else {
					/* we got a call stop msg, time to reply with call stopped ack  */
					sangomabc_exec_command(mcon,
									BOOST_SPAN(ftdmchan),
									BOOST_CHAN(ftdmchan),
									0,
									SIGBOOST_EVENT_CALL_STOPPED_ACK,
									0);
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
	case FTDM_CHANNEL_STATE_PROGRESS:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_PROGRESS_MEDIA;
				if ((status = sangoma_boost_data->signal_cb(&sig) != FTDM_SUCCESS)) {
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
											0);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RING:
		{
			if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_START;
				if ((status = sangoma_boost_data->signal_cb(&sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RESTART:
		{
			sig.event_id = FTDM_SIGEVENT_RESTART;
			status = sangoma_boost_data->signal_cb(&sig);
			ftdm_set_sflag_locked(ftdmchan, SFLAG_SENT_FINAL_MSG);
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;
	case FTDM_CHANNEL_STATE_UP:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sig.event_id = FTDM_SIGEVENT_UP;
				if ((status = sangoma_boost_data->signal_cb(&sig) != FTDM_SUCCESS)) {
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!(ftdm_test_flag(ftdmchan, FTDM_CHANNEL_PROGRESS) || ftdm_test_flag(ftdmchan, FTDM_CHANNEL_MEDIA))) {
					sangomabc_exec_command(mcon,
									   BOOST_SPAN(ftdmchan),
									   BOOST_CHAN(ftdmchan),								   
									   0,
									   SIGBOOST_EVENT_CALL_START_ACK,
									   0);
				}
				
				sangomabc_exec_command(mcon,
								   BOOST_SPAN(ftdmchan),
								   BOOST_CHAN(ftdmchan),								   
								   0,
								   SIGBOOST_EVENT_CALL_ANSWERED,
								   0);
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
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_TERMINATING ||
				ftdm_test_sflag(ftdmchan, SFLAG_SENT_FINAL_MSG)) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
			} else {
				ftdm_set_sflag_locked(ftdmchan, SFLAG_SENT_FINAL_MSG);
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_ANSWERED) || ftdm_test_flag(ftdmchan, FTDM_CHANNEL_PROGRESS) || ftdm_test_flag(ftdmchan, FTDM_CHANNEL_MEDIA)) {

					sangomabc_exec_command(mcon,
									   BOOST_SPAN(ftdmchan),
									   BOOST_CHAN(ftdmchan),
									   0,
									   SIGBOOST_EVENT_CALL_STOPPED,
									   ftdmchan->caller_data.hangup_cause);
				} else {
					sangomabc_exec_command(mcon,
									   BOOST_SPAN(ftdmchan),
									   BOOST_CHAN(ftdmchan),								   
									   0,
									   SIGBOOST_EVENT_CALL_START_NACK,
									   ftdmchan->caller_data.hangup_cause);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			sig.event_id = FTDM_SIGEVENT_STOP;
			status = sangoma_boost_data->signal_cb(&sig);
		}
		break;
	default:
		break;
	}
}

static __inline__ void advance_chan_states(ftdm_channel_t *ftdmchan)
{
	ftdm_mutex_lock(ftdmchan->mutex);
	while (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
		ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);
		state_advance(ftdmchan);
		ftdm_channel_complete_state(ftdmchan);
	}
	ftdm_mutex_unlock(ftdmchan->mutex);
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
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	int susp = ftdm_test_flag(span, FTDM_SPAN_SUSPENDED);
	
	if (susp && ftdm_check_state_all(span, FTDM_CHANNEL_STATE_DOWN)) {
		susp = 0;
	}

	if (ftdm_test_flag(span, FTDM_SPAN_STATE_CHANGE) || susp) {
		uint32_t j;
		ftdm_clear_flag_locked(span, FTDM_SPAN_STATE_CHANGE);
		for(j = 1; j <= span->chan_count; j++) {
			if (ftdm_test_flag((span->channels[j]), FTDM_CHANNEL_STATE_CHANGE) || susp) {
				ftdm_mutex_lock(span->channels[j]->mutex);
				ftdm_clear_flag((span->channels[j]), FTDM_CHANNEL_STATE_CHANGE);
				if (susp && span->channels[j]->state != FTDM_CHANNEL_STATE_DOWN) {
					ftdm_channel_set_state(span->channels[j], FTDM_CHANNEL_STATE_RESTART, 0);
				}
				state_advance(span->channels[j]);
				ftdm_channel_complete_state(span->channels[j]);
				ftdm_mutex_unlock(span->channels[j]->mutex);
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
							   0);	
			ftdm_clear_flag(sangoma_boost_data, FTDM_SANGOMA_BOOST_RESTARTING);
			ftdm_clear_flag_locked(span, FTDM_SPAN_SUSPENDED);
			ftdm_clear_flag((&sangoma_boost_data->mcon), MSU_FLAG_DOWN);
			sangoma_boost_data->mcon.hb_elapsed = 0;
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
	ftdm_sigmsg_t sigmsg;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

	memset(&sigmsg, 0, sizeof(sigmsg));	

	status = ftdm_span_poll_event(span, ms_timeout);

	switch(status) {
	case FTDM_SUCCESS:
		{
			ftdm_event_t *event;
			while (ftdm_span_next_event(span, &event) == FTDM_SUCCESS) {
				sigmsg.span_id = event->channel->span_id;
				sigmsg.chan_id = event->channel->chan_id;
				sigmsg.channel = event->channel;
				switch (event->enum_id) {
				case FTDM_OOB_ALARM_TRAP:
					sigmsg.event_id = FTDM_SIGEVENT_HWSTATUS_CHANGED;
					sigmsg.raw_data = (void *)FTDM_HW_LINK_DISCONNECTED;
					if (sangoma_boost_data->sigmod) {
						sangoma_boost_data->sigmod->on_hw_link_status_change(event->channel, FTDM_HW_LINK_DISCONNECTED);
					}
					sangoma_boost_data->signal_cb(&sigmsg);
					break;
				case FTDM_OOB_ALARM_CLEAR:
					sigmsg.event_id = FTDM_SIGEVENT_HWSTATUS_CHANGED;
					sigmsg.raw_data = (void *)FTDM_HW_LINK_CONNECTED;
					if (sangoma_boost_data->sigmod) {
						sangoma_boost_data->sigmod->on_hw_link_status_change(event->channel, FTDM_HW_LINK_CONNECTED);
					}
					sangoma_boost_data->signal_cb(&sigmsg);
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
	return FTDM_SUCCESS;
}

/*! 
  \brief wait for a boost event 
  \return -1 on error, 0 on timeout, 1 when there are events
 */
static int ftdm_boost_wait_event(ftdm_span_t *span, int ms)
{
#ifndef WIN32
		struct timeval tv = { 0, ms * 1000 };
		sangomabc_connection_t *mcon, *pcon;
		int max, activity;
#endif
		ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

		if (sangoma_boost_data->sigmod) {
			ftdm_status_t res;
			res =  ftdm_queue_wait(sangoma_boost_data->boost_queue, ms);
			if (FTDM_TIMEOUT == res) {
				return 0;
			}
			if (FTDM_SUCCESS != res) {
				return -1;
			}
			return 1;
		}
#ifndef WIN32
		mcon = &sangoma_boost_data->mcon;
		pcon = &sangoma_boost_data->pcon;

		FD_ZERO(&sangoma_boost_data->rfds);
		FD_ZERO(&sangoma_boost_data->efds);
		FD_SET(mcon->socket, &sangoma_boost_data->rfds);
		FD_SET(mcon->socket, &sangoma_boost_data->efds);
		FD_SET(pcon->socket, &sangoma_boost_data->rfds);
		FD_SET(pcon->socket, &sangoma_boost_data->efds);
		sangoma_boost_data->iteration = 0;

		max = ((pcon->socket > mcon->socket) ? pcon->socket : mcon->socket) + 1;
		if ((activity = select(max, &sangoma_boost_data->rfds, NULL, &sangoma_boost_data->efds, &tv)) < 0) {
			return -1;
		}

		if (FD_ISSET(pcon->socket, &sangoma_boost_data->efds) || FD_ISSET(mcon->socket, &sangoma_boost_data->efds)) {
			return -1;
		}

		return 1;
#endif
		return 0;
}


static sangomabc_event_t *ftdm_boost_read_event(ftdm_span_t *span)
{
	sangomabc_event_t *event = NULL;
	sangomabc_connection_t *mcon, *pcon;
	ftdm_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

	mcon = &sangoma_boost_data->mcon;
	pcon = &sangoma_boost_data->pcon;

	if (sangoma_boost_data->sigmod 
#ifndef WIN32
		|| FD_ISSET(pcon->socket, &sangoma_boost_data->rfds)
#endif
		) {
		event = sangomabc_connection_readp(pcon, sangoma_boost_data->iteration);
	}
#ifndef WIN32
	/* if there is no event and this is not a sigmod-driven span it's time to try the other connection for events */
	if (!event && !sangoma_boost_data->sigmod && FD_ISSET(mcon->socket, &sangoma_boost_data->rfds)) {
		event = sangomabc_connection_readp(mcon, sangoma_boost_data->iteration);
	}
#endif
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
	uint32_t ms = 10;
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
		int activity = 0;
		
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

		if ((activity = ftdm_boost_wait_event(span, ms)) < 0) {
			ftdm_log(FTDM_LOG_ERROR, "ftdm_boost_wait_event failed\n");
			goto error;
		}
		
		if (activity) {
			while ((event = ftdm_boost_read_event(span))) {
				parse_sangoma_event(span, pcon, (sangomabc_short_event_t*)event);
				sangoma_boost_data->iteration++;
			}
		}
		
		pcon->hb_elapsed += ms;

		if (ftdm_test_flag(span, FTDM_SPAN_SUSPENDED) || ftdm_test_flag(mcon, MSU_FLAG_DOWN)) {
			pcon->hb_elapsed = 0;
		}

		if (ftdm_running()) {
			check_state(span);
		}
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
	ftdm_mutex_create(&signal_mutex);
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
	ftdm_mutex_destroy(&signal_mutex);
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
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_PROGRESS, FTDM_END},
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
	ftdm_sangoma_boost_data_t *sangoma_boost_data = ftdmchan->span->signal_data;
	ftdm_log(FTDM_LOG_NOTICE, "%d:%d Signaling link status changed to %s\n", ftdmchan->span_id, ftdmchan->chan_id, ftdm_signaling_status2str(status));
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;
	sig.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
	sig.raw_data = &status;
	sangoma_boost_data->signal_cb(&sig);
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
		ftdm_set_string(sangoma_boost_data->mcon.cfg.local_ip, local_ip);
		sangoma_boost_data->mcon.cfg.local_port = local_port;
		ftdm_set_string(sangoma_boost_data->mcon.cfg.remote_ip, remote_ip);
		sangoma_boost_data->mcon.cfg.remote_port = remote_port;
	}
	sangoma_boost_data->signal_cb = sig_cb;
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
	if (sigmod_iface) {
		span->suggest_chan_id = 1;
	} else {
		span->suggest_chan_id = 0;
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
