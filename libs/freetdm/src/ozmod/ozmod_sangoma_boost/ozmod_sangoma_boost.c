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

#include "openzap.h"
#include "sangoma_boost_client.h"
#include "zap_sangoma_boost.h"
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

/* Boost signaling modules global hash and its mutex */
zap_mutex_t *g_boost_modules_mutex = NULL;
zap_hash_t *g_boost_modules_hash = NULL;

#define MAX_TRUNK_GROUPS 64
//DAVIDY need to merge congestion_timeouts with zap_sangoma_boost_trunkgroups
static time_t congestion_timeouts[MAX_TRUNK_GROUPS];

static zap_sangoma_boost_trunkgroup_t *g_trunkgroups[MAX_TRUNK_GROUPS];

#define BOOST_QUEUE_SIZE 500

/* get openzap span and chan depending on the span mode */
#define BOOST_SPAN(zchan) ((zap_sangoma_boost_data_t*)(zchan)->span->signal_data)->sigmod ? zchan->physical_span_id : zchan->physical_span_id-1
#define BOOST_CHAN(zchan) ((zap_sangoma_boost_data_t*)(zchan)->span->signal_data)->sigmod ? zchan->physical_chan_id : zchan->physical_chan_id-1

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
	zap_span_t *span;
	zap_channel_t *zchan;
} sangoma_boost_request_t;

//#define MAX_REQ_ID ZAP_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN * ZAP_MAX_CHANNELS_PHYSICAL_SPAN
#define MAX_REQ_ID 6000

static uint16_t SETUP_GRID[ZAP_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN+1][ZAP_MAX_CHANNELS_PHYSICAL_SPAN+1] = {{ 0 }};

static sangoma_boost_request_t OUTBOUND_REQUESTS[MAX_REQ_ID+1] = {{ 0 }};

static zap_mutex_t *request_mutex = NULL;
static zap_mutex_t *signal_mutex = NULL;

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
	uint32_t i;
	zap_channel_t *zchan = NULL;
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	uint32_t targetspan = event->span+1;
	uint32_t targetchan = event->chan+1;
	if (sangoma_boost_data->sigmod) {
		/* span is not strictly needed here since we're supposed to get only events for our span */
		targetspan = event->span;
		targetchan = event->chan;
	}

	zap_mutex_lock(signal_mutex);
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->physical_span_id == targetspan && span->channels[i]->physical_chan_id == targetchan) {
			zchan = span->channels[i];
			if (force || (zchan->state == ZAP_CHANNEL_STATE_DOWN && !zap_test_flag(zchan, ZAP_CHANNEL_INUSE))) {
				break;
			} else {
				zchan = NULL;
				zap_log(ZAP_LOG_ERROR, "Channel %d:%d ~ %d:%d is already in use.\n",
						span->channels[i]->span_id,
						span->channels[i]->chan_id,
						span->channels[i]->physical_span_id,
						span->channels[i]->physical_chan_id
						);
				break;
			}
		}
	}
	zap_mutex_unlock(signal_mutex);

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
			zap_log(ZAP_LOG_ERROR, "Invalid boost isup_rdnis MEDIA format %s\n", p);
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
	int boost_request_timeout = 5000;
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

	r = next_request_id();
	if (r == 0) {
		zap_log(ZAP_LOG_CRIT, "All tanks ids are busy.\n");
		*zchan = NULL;
		return ZAP_FAIL;
	}
	sangomabc_call_init(&event, caller_data->cid_num.digits, ani, r);

	if (sangoma_boost_data->sigmod) {
		*zchan = span->channels[chan_id];

		event.span = (uint8_t) (*zchan)->physical_span_id;
		event.chan = (uint8_t) (*zchan)->physical_chan_id;

		zap_set_flag((*zchan), ZAP_CHANNEL_OUTBOUND);
		zap_set_flag_locked((*zchan), ZAP_CHANNEL_INUSE);

		OUTBOUND_REQUESTS[r].zchan = *zchan;
	} else {
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
	}

	zap_set_string(event.calling_name, caller_data->cid_name);
	zap_set_string(event.isup_in_rdnis, caller_data->rdnis.digits);
	if (strlen(caller_data->rdnis.digits)) {
			event.isup_in_rdnis_size = (uint16_t)strlen(caller_data->rdnis.digits)+1;
	}
    
	event.calling_number_screening_ind = caller_data->screen;
	event.calling_number_presentation = caller_data->pres;

	OUTBOUND_REQUESTS[r].status = BST_WAITING;
	OUTBOUND_REQUESTS[r].span = span;

	if (sangomabc_connection_write(&sangoma_boost_data->mcon, &event) <= 0) {
		zap_log(ZAP_LOG_CRIT, "Failed to tx boost event [%s]\n", strerror(errno));
		status = ZAP_FAIL;
		if (!sangoma_boost_data->sigmod) {
			*zchan = NULL;
		}
		goto done;
	}

	while(zap_running() && OUTBOUND_REQUESTS[r].status == BST_WAITING) {
		zap_sleep(1);
		if (--boost_request_timeout <= 0) {
			status = ZAP_FAIL;
			if (!sangoma_boost_data->sigmod) {
				*zchan = NULL;
			}
			zap_log(ZAP_LOG_CRIT, "Timed out waiting for boost channel request response, current status: BST_WAITING\n");
			zap_log(ZAP_LOG_CRIT, "DYDBG s%dc%d: Csid:%d Timed out waiting for boost channel request response, current status: BST_WAITING\n", (*zchan)->physical_span_id, (*zchan)->physical_chan_id, r);
			goto done;
		}
	}
	
	if (OUTBOUND_REQUESTS[r].status == BST_ACK && OUTBOUND_REQUESTS[r].zchan) {
		*zchan = OUTBOUND_REQUESTS[r].zchan;
		status = ZAP_SUCCESS;
		(*zchan)->init_state = ZAP_CHANNEL_STATE_PROGRESS;
		zap_log(ZAP_LOG_DEBUG, "Channel state changed to PROGRESS [Csid:%d]\n", r);
	}

	boost_request_timeout = 5000;
	while(zap_running() && OUTBOUND_REQUESTS[r].status == BST_ACK) {
		zap_sleep(1);
		if (--boost_request_timeout <= 0) {
			status = ZAP_FAIL;
			if (!sangoma_boost_data->sigmod) {
				*zchan = NULL;
			}
			zap_log(ZAP_LOG_CRIT, "Timed out waiting for boost channel request response, current status: BST_ACK\n");
			goto done;
		}
		//printf("WTF %d\n", sanity);
	}

	if (OUTBOUND_REQUESTS[r].status == BST_READY && OUTBOUND_REQUESTS[r].zchan) {
		*zchan = OUTBOUND_REQUESTS[r].zchan;
		status = ZAP_SUCCESS;
		(*zchan)->init_state = ZAP_CHANNEL_STATE_PROGRESS_MEDIA;
		zap_log(ZAP_LOG_DEBUG, "Channel state changed to PROGRESS_MEDIA [Csid:%d]\n", r);
	} else {
		status = ZAP_FAIL;
		if (!sangoma_boost_data->sigmod) {
			*zchan = NULL;
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
								(*zchan)->physical_span_id,
								(*zchan)->physical_chan_id,
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
static void handle_call_progress(sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_channel_t *zchan;

	if (nack_map[event->call_setup_id]) {
		return;
	}

	//if we received a progress for this device already
	if (OUTBOUND_REQUESTS[event->call_setup_id].status == BST_ACK) {
		if (boost_media_ready((sangomabc_event_t*) event)) {
			OUTBOUND_REQUESTS[event->call_setup_id].status = BST_READY;
			zap_log(ZAP_LOG_DEBUG, "chan media ready %d:%d CSid:%d\n", event->span+1, event->chan+1, event->call_setup_id);
		}
		return;
	}

	OUTBOUND_REQUESTS[event->call_setup_id].event = *event;
	SETUP_GRID[event->span][event->chan] = event->call_setup_id;

	if ((zchan = find_zchan(OUTBOUND_REQUESTS[event->call_setup_id].span, (sangomabc_short_event_t*) event, 0))) {
		if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
			zap_log(ZAP_LOG_ERROR, "OPEN ERROR [%s]\n", zchan->last_error);
		} else {
			zap_set_flag(zchan, ZAP_CHANNEL_OUTBOUND);
			zap_set_flag_locked(zchan, ZAP_CHANNEL_INUSE);
			zchan->extra_id = event->call_setup_id;
			zap_log(ZAP_LOG_DEBUG, "Assign chan %d:%d (%d:%d) CSid=%d\n", zchan->span_id, zchan->chan_id, event->span+1,event->chan+1, event->call_setup_id);
			zchan->sflags = 0;
			OUTBOUND_REQUESTS[event->call_setup_id].zchan = zchan;
			if (boost_media_ready((sangomabc_event_t*)event)) {
				OUTBOUND_REQUESTS[event->call_setup_id].status = BST_READY;
			} else {
				OUTBOUND_REQUESTS[event->call_setup_id].status = BST_ACK;
			}
			return;
		}
	} 
	
	//printf("WTF BAD ACK CSid=%d span=%d chan=%d\n", event->call_setup_id, event->span+1,event->chan+1);
	if ((zchan = find_zchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 1))) {
		//printf("WTF BAD ACK2 %d:%d (%d:%d) CSid=%d xtra_id=%d out=%d state=%s\n", zchan->span_id, zchan->chan_id, event->span+1,event->chan+1, event->call_setup_id, zchan->extra_id, zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND), zap_channel_state2str(zchan->state));
	}


	zap_log(ZAP_LOG_CRIT, "START PROGRESS CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	if (zchan) {
		zap_set_sflag(zchan, SFLAG_SENT_FINAL_MSG);
	}
	sangomabc_exec_command(mcon,
					   event->span,
					   event->chan,
					   event->call_setup_id,
					   SIGBOOST_EVENT_CALL_STOPPED,
					   ZAP_CAUSE_DESTINATION_OUT_OF_ORDER);
	OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;	
}

/**
 * \brief Handler for call start ack event
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_start_ack(sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	
	zap_channel_t *zchan;
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
		zchan = OUTBOUND_REQUESTS[event->call_setup_id].zchan;
		zap_set_flag(zchan, ZAP_CHANNEL_OUTBOUND);
		zap_set_flag_locked(zchan, ZAP_CHANNEL_INUSE);
	} else {
		zchan = find_zchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 0);
	}


	if (zchan) {
		if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
			zap_log(ZAP_LOG_ERROR, "OPEN ERROR [%s]\n", zchan->last_error);
		} else {
			zchan->extra_id = event->call_setup_id;
			zap_log(ZAP_LOG_DEBUG, "Assign chan %d:%d (%d:%d) CSid=%d\n", zchan->span_id, zchan->chan_id, event_span, event_chan, event->call_setup_id);
			zchan->sflags = 0;
			OUTBOUND_REQUESTS[event->call_setup_id].zchan = zchan;
			OUTBOUND_REQUESTS[event->call_setup_id].status = BST_READY;
			return;
		}
	} 
	
	//printf("WTF BAD ACK CSid=%d span=%d chan=%d\n", event->call_setup_id, event->span+1,event->chan+1);
	if ((zchan = find_zchan(OUTBOUND_REQUESTS[event->call_setup_id].span, event, 1))) {
		//printf("WTF BAD ACK2 %d:%d (%d:%d) CSid=%d xtra_id=%d out=%d state=%s\n", zchan->span_id, zchan->chan_id, event->span+1,event->chan+1, event->call_setup_id, zchan->extra_id, zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND), zap_channel_state2str(zchan->state));
	}

	zap_set_sflag(zchan, SFLAG_SENT_FINAL_MSG);
	zap_log(ZAP_LOG_CRIT, "START ACK CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	sangomabc_exec_command(mcon,
					   event->span,
					   event->chan,
					   event->call_setup_id,
					   SIGBOOST_EVENT_CALL_STOPPED,
					   ZAP_CAUSE_DESTINATION_OUT_OF_ORDER);
	OUTBOUND_REQUESTS[event->call_setup_id].status = BST_FAIL;
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

		if (zchan->state == ZAP_CHANNEL_STATE_DOWN || zchan->state == ZAP_CHANNEL_STATE_HANGUP_COMPLETE) {
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
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

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

	zap_log(ZAP_LOG_CRIT, "DYDBG setting event->call_setup_id:%d to BST_FAIL\n", event->call_setup_id);
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
		if ((zchan = find_zchan(span, event, 1))) {
			int r = 0;
			assert(!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND));

			zap_mutex_lock(zchan->mutex);
			zap_set_state_r(zchan, ZAP_CHANNEL_STATE_CANCEL, 0, r);
			if (r == ZAP_STATE_CHANGE_SUCCESS) {
				zchan->caller_data.hangup_cause = event->release_cause;
			}
			zap_mutex_unlock(zchan->mutex);
			if (r) {
				return;
			}
		}
	}

#if 0
	if (zchan) {
		zap_set_sflag_locked(zchan, SFLAG_SENT_FINAL_MSG);
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
		
		if (zchan->state == ZAP_CHANNEL_STATE_HANGUP) {
			/* racing condition where both sides initiated a hangup 
			 * Do not change current state as channel is already clearing
			 * itself through local initiated hangup */
			
			sangomabc_exec_command(mcon,
						BOOST_SPAN(zchan),
						BOOST_CHAN(zchan),
						0,
						SIGBOOST_EVENT_CALL_STOPPED_ACK,
						0);
			return;
		} else {
			zap_set_state_r(zchan, ZAP_CHANNEL_STATE_TERMINATING, 0, r);
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
		int r = 0;

		if (zchan->extra_id == event->call_setup_id && zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
			zap_mutex_lock(zchan->mutex);
			if (zchan->state == ZAP_CHANNEL_STATE_DOWN && zchan->init_state != ZAP_CHANNEL_STATE_UP) {
				zchan->init_state = ZAP_CHANNEL_STATE_UP;
				r = 1;
			} else {
				zap_set_state_r(zchan, ZAP_CHANNEL_STATE_UP, 0, r);
			}
			zap_mutex_unlock(zchan->mutex);
		} 
#if 0
		if (!r) {
			printf("WTF BAD ANSWER %d:%d (%d:%d) CSid=%d xtra_id=%d out=%d state=%s\n", zchan->span_id, zchan->chan_id, event->span+1,event->chan+1, event->call_setup_id, zchan->extra_id, zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND), zap_channel_state2str(zchan->state));
		}
#endif
	} else {
		zap_log(ZAP_LOG_CRIT, "ANSWER CANT FIND A CHAN %d:%d\n", event->span+1,event->chan+1);
	}
}

/**
 * \brief Handler for call start event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static void handle_call_start(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_event_t *event)
{
	zap_channel_t *zchan;

	if (!(zchan = find_zchan(span, (sangomabc_short_event_t*)event, 0))) {
		goto error;
	}

	if (zap_channel_open_chan(zchan) != ZAP_SUCCESS) {
		goto error;
	}
	
	zap_log(ZAP_LOG_DEBUG, "Got call start from s%dc%d mapped to openzap logical s%dc%d, physical s%dc%d\n", 
			event->span, event->chan, 
			zchan->span_id, zchan->chan_id,
			zchan->physical_span_id, zchan->physical_chan_id);

	zchan->sflags = 0;
	zap_set_string(zchan->caller_data.cid_num.digits, (char *)event->calling_number_digits);
	zap_set_string(zchan->caller_data.cid_name, (char *)event->calling_number_digits);
	if (strlen(event->calling_name)) {
		zap_set_string(zchan->caller_data.cid_name, (char *)event->calling_name);
	}
	zap_set_string(zchan->caller_data.ani.digits, (char *)event->calling_number_digits);
	zap_set_string(zchan->caller_data.dnis.digits, (char *)event->called_number_digits);
	if (event->isup_in_rdnis_size) {
		char* p;

		//Set value of rdnis.digis in case prot daemon is still using older style RDNIS
		if (atoi((char *)event->isup_in_rdnis) > 0) {
			zap_set_string(zchan->caller_data.rdnis.digits, (char *)event->isup_in_rdnis);
		}

		p = strstr((char*)event->isup_in_rdnis,"PRI001-ANI2-");
		if (p!=NULL) {
			int ani2 = 0;
			sscanf(p, "PRI001-ANI2-%d", &ani2);
			snprintf(zchan->caller_data.aniII, 5, "%.2d", ani2);
		}	
		p = strstr((char*)event->isup_in_rdnis,"RDNIS-");
		if (p!=NULL) {
			sscanf(p, "RDNIS-%s", &zchan->caller_data.rdnis.digits[0]);
		}
		
	}
	zchan->caller_data.screen = event->calling_number_screening_ind;
	zchan->caller_data.pres = event->calling_number_presentation;
	zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RING);
	return;

 error:

	zap_log(ZAP_LOG_CRIT, "START CANT FIND A CHAN %d:%d\n", event->span,event->chan);

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
 * \brief Handler for sangoma boost event
 * \param span Span where event was fired
 * \param mcon sangoma boost connection
 * \param event Event to handle
 */
static int parse_sangoma_event(zap_span_t *span, sangomabc_connection_t *mcon, sangomabc_short_event_t *event)
{
	zap_mutex_lock(signal_mutex);
	
	if (!zap_running()) {
		zap_log(ZAP_LOG_WARNING, "System is shutting down.\n");
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
		zap_log(ZAP_LOG_WARNING, "No handler implemented for [%s]\n", sangomabc_event_id_name(event->event_id));
		break;
    }

 end:

	zap_mutex_unlock(signal_mutex);

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
			if (zchan->extra_id) {
				zchan->extra_id = 0;
			}

			if (zap_test_sflag(zchan, SFLAG_FREE_REQ_ID)) {
				release_request_id_span_chan(zchan->physical_span_id-1, zchan->physical_chan_id-1);
			}

			if (!zap_test_sflag(zchan, SFLAG_SENT_FINAL_MSG)) {
				zap_set_sflag_locked(zchan, SFLAG_SENT_FINAL_MSG);

				sangomabc_exec_command(mcon,
							   BOOST_SPAN(zchan),
							   BOOST_CHAN(zchan),
							   0,
							   SIGBOOST_EVENT_CALL_STOPPED_ACK,
							   0);
			}
			zchan->sflags = 0;
			zap_channel_done(zchan);
		}
		break;
	case ZAP_CHANNEL_STATE_PROGRESS_MEDIA:
	case ZAP_CHANNEL_STATE_PROGRESS:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_PROGRESS_MEDIA;
				if ((status = sangoma_boost_data->signal_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!zap_test_sflag(zchan, SFLAG_SENT_ACK)) {
					zap_set_sflag(zchan, SFLAG_SENT_ACK);
						sangomabc_exec_command(mcon,
											BOOST_SPAN(zchan),
											BOOST_CHAN(zchan),
											0,
											SIGBOOST_EVENT_CALL_START_ACK,
											0);
				}
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RING:
		{
			if (!zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_START;
				if ((status = sangoma_boost_data->signal_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			}
		}
		break;
	case ZAP_CHANNEL_STATE_RESTART:
		{
			sig.event_id = ZAP_SIGEVENT_RESTART;
			status = sangoma_boost_data->signal_cb(&sig);
			zap_set_sflag_locked(zchan, SFLAG_SENT_FINAL_MSG);
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
		}
		break;
	case ZAP_CHANNEL_STATE_UP:
		{
			if (zap_test_flag(zchan, ZAP_CHANNEL_OUTBOUND)) {
				sig.event_id = ZAP_SIGEVENT_UP;
				if ((status = sangoma_boost_data->signal_cb(&sig) != ZAP_SUCCESS)) {
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			} else {
				if (!(zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS) || zap_test_flag(zchan, ZAP_CHANNEL_MEDIA))) {
					sangomabc_exec_command(mcon,
									   BOOST_SPAN(zchan),
									   BOOST_CHAN(zchan),								   
									   0,
									   SIGBOOST_EVENT_CALL_START_ACK,
									   0);
				}
				
				sangomabc_exec_command(mcon,
								   BOOST_SPAN(zchan),
								   BOOST_CHAN(zchan),								   
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
			if (zchan->last_state == ZAP_CHANNEL_STATE_TERMINATING ||
				zap_test_sflag(zchan, SFLAG_SENT_FINAL_MSG)) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP_COMPLETE);
			} else {
				zap_set_sflag_locked(zchan, SFLAG_SENT_FINAL_MSG);
				if (zap_test_flag(zchan, ZAP_CHANNEL_ANSWERED) || zap_test_flag(zchan, ZAP_CHANNEL_PROGRESS) || zap_test_flag(zchan, ZAP_CHANNEL_MEDIA)) {

					sangomabc_exec_command(mcon,
									   BOOST_SPAN(zchan),
									   BOOST_CHAN(zchan),
									   0,
									   SIGBOOST_EVENT_CALL_STOPPED,
									   zchan->caller_data.hangup_cause);
				} else {
					sangomabc_exec_command(mcon,
									   BOOST_SPAN(zchan),
									   BOOST_CHAN(zchan),								   
									   0,
									   SIGBOOST_EVENT_CALL_START_NACK,
									   zchan->caller_data.hangup_cause);
				}
			}
		}
		break;
	case ZAP_CHANNEL_STATE_CANCEL:
		{
			sig.event_id = ZAP_SIGEVENT_STOP;
			status = sangoma_boost_data->signal_cb(&sig);
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
			zap_set_sflag_locked(zchan, SFLAG_SENT_FINAL_MSG);
			sangomabc_exec_command(mcon,
							   BOOST_SPAN(zchan),
							   BOOST_CHAN(zchan),
							   0,
							   SIGBOOST_EVENT_CALL_START_NACK_ACK,
							   0);
		}
		break;
	case ZAP_CHANNEL_STATE_TERMINATING:
		{
			sig.event_id = ZAP_SIGEVENT_STOP;
			status = sangoma_boost_data->signal_cb(&sig);
		}
		break;
	default:
		break;
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
							   0);	
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
static __inline__ zap_status_t check_events(zap_span_t *span, int ms_timeout)
{
	zap_status_t status;
	zap_sigmsg_t sigmsg;
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

	memset(&sigmsg, 0, sizeof(sigmsg));	

	status = zap_span_poll_event(span, ms_timeout);

	switch(status) {
	case ZAP_SUCCESS:
		{
			zap_event_t *event;
			while (zap_span_next_event(span, &event) == ZAP_SUCCESS) {
				sigmsg.span_id = event->channel->span_id;
				sigmsg.chan_id = event->channel->chan_id;
				sigmsg.channel = event->channel;
				switch (event->enum_id) {
				case ZAP_OOB_ALARM_TRAP:
					sigmsg.event_id = ZAP_SIGEVENT_HWSTATUS_CHANGED;
					sigmsg.raw_data = (void *)ZAP_HW_LINK_DISCONNECTED;
					if (sangoma_boost_data->sigmod) {
						sangoma_boost_data->sigmod->on_hw_link_status_change(event->channel, ZAP_HW_LINK_DISCONNECTED);
					}
					sangoma_boost_data->signal_cb(&sigmsg);
					break;
				case ZAP_OOB_ALARM_CLEAR:
					sigmsg.event_id = ZAP_SIGEVENT_HWSTATUS_CHANGED;
					sigmsg.raw_data = (void *)ZAP_HW_LINK_CONNECTED;
					if (sangoma_boost_data->sigmod) {
						sangoma_boost_data->sigmod->on_hw_link_status_change(event->channel, ZAP_HW_LINK_CONNECTED);
					}
					sangoma_boost_data->signal_cb(&sigmsg);
					break;
				}
			}
		}
		break;
	case ZAP_FAIL:
		{
			if (!zap_running()) {
				break;
			}
			zap_log(ZAP_LOG_ERROR, "Boost Check Event Failure Failure: %s\n", span->last_error);
			return ZAP_FAIL;
		}
		break;
	default:
		break;
	}

	return ZAP_SUCCESS;
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
	unsigned errs = 0;

	while (zap_test_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING) && zap_running()) {
		if (check_events(span,100) != ZAP_SUCCESS) {
			if (errs++ > 50) {
				zap_log(ZAP_LOG_ERROR, "Too many event errors, quitting sangoma events thread\n");
				return NULL;
			}
		}
	}

	return NULL;
}

static zap_status_t zap_boost_connection_open(zap_span_t *span)
{
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	if (sangoma_boost_data->sigmod) {
		if (sangoma_boost_data->sigmod->start_span(span) != ZAP_SUCCESS) {
			return ZAP_FAIL;
		}
		zap_clear_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RESTARTING);
		zap_clear_flag_locked(span, ZAP_SPAN_SUSPENDED);
		zap_clear_flag((&sangoma_boost_data->mcon), MSU_FLAG_DOWN);
	} 

	sangoma_boost_data->pcon = sangoma_boost_data->mcon;

	/* when sigmod is present, all arguments: local_ip etc, are ignored by sangomabc_connection_open */
	if (sangomabc_connection_open(&sangoma_boost_data->mcon,
								  sangoma_boost_data->mcon.cfg.local_ip,
								  sangoma_boost_data->mcon.cfg.local_port,
								  sangoma_boost_data->mcon.cfg.remote_ip,
								  sangoma_boost_data->mcon.cfg.remote_port) < 0) {
		zap_log(ZAP_LOG_ERROR, "Error: Opening MCON Socket [%d] %s\n", sangoma_boost_data->mcon.socket, strerror(errno));
		return ZAP_FAIL;
	}

	if (sangomabc_connection_open(&sangoma_boost_data->pcon,
							  sangoma_boost_data->pcon.cfg.local_ip,
							  ++sangoma_boost_data->pcon.cfg.local_port,
							  sangoma_boost_data->pcon.cfg.remote_ip,
							  ++sangoma_boost_data->pcon.cfg.remote_port) < 0) {
		zap_log(ZAP_LOG_ERROR, "Error: Opening PCON Socket [%d] %s\n", sangoma_boost_data->pcon.socket, strerror(errno));
		return ZAP_FAIL;
    }
	return ZAP_SUCCESS;
}

/*! 
  \brief wait for a boost event 
  \return -1 on error, 0 on timeout, 1 when there are events
 */
static int zap_boost_wait_event(zap_span_t *span, int ms)
{
#ifndef WIN32
		struct timeval tv = { 0, ms * 1000 };
		sangomabc_connection_t *mcon, *pcon;
		int max, activity;
#endif
		zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

		if (sangoma_boost_data->sigmod) {
			zap_status_t res;
			res =  zap_queue_wait(sangoma_boost_data->boost_queue, ms);
			if (ZAP_TIMEOUT == res) {
				return 0;
			}
			if (ZAP_SUCCESS != res) {
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


static sangomabc_event_t *zap_boost_read_event(zap_span_t *span)
{
	sangomabc_event_t *event = NULL;
	sangomabc_connection_t *mcon, *pcon;
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

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
static void *zap_sangoma_boost_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	sangomabc_connection_t *mcon, *pcon;
	uint32_t ms = 10;
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;

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

	if (zap_boost_connection_open(span) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "zap_boost_connection_open failed\n");
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
		zap_set_flag(mcon, MSU_FLAG_DOWN);
	}

	while (zap_test_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING)) {
		sangomabc_event_t *event = NULL;
		int activity = 0;
		
		if (!zap_running()) {
			if (!sangoma_boost_data->sigmod) {
				sangomabc_exec_commandp(pcon,
								   0,
								   0,
								   -1,
								   SIGBOOST_EVENT_SYSTEM_RESTART,
								   0);
				zap_set_flag(mcon, MSU_FLAG_DOWN);
			}
			zap_log(ZAP_LOG_DEBUG, "zap is no longer running\n");
			break;
		}

		if ((activity = zap_boost_wait_event(span, ms)) < 0) {
			zap_log(ZAP_LOG_ERROR, "zap_boost_wait_event failed\n");
			goto error;
		}
		
		if (activity) {
			while ((event = zap_boost_read_event(span))) {
				parse_sangoma_event(span, pcon, (sangomabc_short_event_t*)event);
				sangoma_boost_data->iteration++;
			}
		}
		
		pcon->hb_elapsed += ms;

		if (zap_test_flag(span, ZAP_SPAN_SUSPENDED) || zap_test_flag(mcon, MSU_FLAG_DOWN)) {
			pcon->hb_elapsed = 0;
		}

		if (zap_running()) {
			check_state(span);
		}
	}

	goto end;

error:
	zap_log(ZAP_LOG_CRIT, "Boost event processing Error!\n");

end:
	if (!sangoma_boost_data->sigmod) {
		sangomabc_connection_close(&sangoma_boost_data->mcon);
		sangomabc_connection_close(&sangoma_boost_data->pcon);
	}

	zap_clear_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING);

	zap_log(ZAP_LOG_DEBUG, "Sangoma Boost thread ended.\n");
	return NULL;
}

/**
 * \brief Loads sangoma boost signaling module
 * \param zio Openzap IO interface
 * \return Success
 */
static ZIO_SIG_LOAD_FUNCTION(zap_sangoma_boost_init)
{
	int i;
	g_boost_modules_hash = create_hashtable(10, zap_hash_hashfromstring, zap_hash_equalkeys);
	if (!g_boost_modules_hash) {
		return ZAP_FAIL;
	}
	zap_mutex_create(&request_mutex);
	zap_mutex_create(&signal_mutex);
	zap_mutex_create(&g_boost_modules_mutex);

	for(i=0;i< MAX_TRUNK_GROUPS;i++) {
		g_trunkgroups[i]=NULL;
	}
	return ZAP_SUCCESS;
}

static ZIO_SIG_UNLOAD_FUNCTION(zap_sangoma_boost_destroy)
{
	zap_hash_iterator_t *i = NULL;
	boost_sigmod_interface_t *sigmod = NULL;
	const void *key = NULL;
	void *val = NULL;
	zap_dso_lib_t lib;

	for (i = hashtable_first(g_boost_modules_hash); i; i = hashtable_next(i)) {
		hashtable_this(i, &key, NULL, &val);
		if (key && val) {
			sigmod = val;
			lib = sigmod->pvt;
			zap_dso_destroy(&lib);
		}
	}

	hashtable_destroy(g_boost_modules_hash);
	zap_mutex_destroy(&request_mutex);
	zap_mutex_destroy(&signal_mutex);
	zap_mutex_destroy(&g_boost_modules_mutex);
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

static zap_status_t zap_sangoma_boost_stop(zap_span_t *span)
{
	int cnt = 10;
	zap_status_t status = ZAP_SUCCESS;
	zap_sangoma_boost_data_t *sangoma_boost_data = span->signal_data;
	if (sangoma_boost_data->sigmod) {

		/* FIXME: we should make sure the span thread is stopped (use pthread_kill or openzap thread kill function) */
		/* I think stopping the span before destroying the queue makes sense
		   otherwise may be boost events would still arrive when the queue is already destroyed! */
		status = sangoma_boost_data->sigmod->stop_span(span);

		zap_queue_enqueue(sangoma_boost_data->boost_queue, NULL);
		while(zap_test_flag(sangoma_boost_data, ZAP_SANGOMA_BOOST_RUNNING) && cnt-- > 0) {
			zap_log(ZAP_LOG_DEBUG, "Waiting for boost thread\n");
			zap_sleep(500);
		}
		zap_queue_destroy(&sangoma_boost_data->boost_queue);
		return status;
	}
	return status;
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
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, ZAP_END}
		},
		{
			ZSD_OUTBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_CHANNEL_STATE_PROGRESS, ZAP_END},
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
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
			{ZAP_CHANNEL_STATE_RING, ZAP_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_RING, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_CANCEL, ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_END}
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
			{ZAP_CHANNEL_STATE_CANCEL, ZAP_CHANNEL_STATE_HANGUP_COMPLETE, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
			{ZAP_CHANNEL_STATE_DOWN, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_PROGRESS, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_CANCEL, ZAP_CHANNEL_STATE_TERMINATING, ZAP_CHANNEL_STATE_UP, ZAP_CHANNEL_STATE_PROGRESS_MEDIA, ZAP_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{ZAP_CHANNEL_STATE_UP, ZAP_END},
			{ZAP_CHANNEL_STATE_HANGUP, ZAP_CHANNEL_STATE_TERMINATING, ZAP_END},
		},
		

	}
};

static BOOST_WRITE_MSG_FUNCTION(zap_boost_write_msg)
{
	sangomabc_short_event_t *shortmsg = NULL;
	zap_sangoma_boost_data_t *sangoma_boost_data = NULL;
	sangomabc_queue_element_t *element = NULL;

	zap_assert_return(msg != NULL, ZAP_FAIL, "Boost message to write was null");

	if (!span) {
		shortmsg = msg;
		zap_log(ZAP_LOG_ERROR, "Unexpected boost message %d\n", shortmsg->event_id);
		return ZAP_FAIL;
	}
	/* duplicate the event and enqueue it */
	element = zap_calloc(1, sizeof(*element));
	if (!element) {
		return ZAP_FAIL;
	}
	memcpy(element->boostmsg, msg, msglen);
	element->size = msglen;

	sangoma_boost_data = span->signal_data;
	return zap_queue_enqueue(sangoma_boost_data->boost_queue, element);
}

static BOOST_SIG_STATUS_CB_FUNCTION(zap_boost_sig_status_change)
{
	zap_sigmsg_t sig;
	zap_sangoma_boost_data_t *sangoma_boost_data = zchan->span->signal_data;
	zap_log(ZAP_LOG_DEBUG, "%d:%d Signaling link status changed to %s\n", zchan->span_id, zchan->chan_id, zap_sig_status2str(status));
	
	memset(&sig, 0, sizeof(sig));
	sig.chan_id = zchan->chan_id;
	sig.span_id = zchan->span_id;
	sig.channel = zchan;
	sig.event_id = ZAP_SIGEVENT_SIGSTATUS_CHANGED;
	sig.raw_data = &status;
	sangoma_boost_data->signal_cb(&sig);
	return;
}

static ZIO_CHANNEL_GET_SIG_STATUS_FUNCTION(sangoma_boost_get_sig_status)
{
	zap_sangoma_boost_data_t *sangoma_boost_data = zchan->span->signal_data;
	if (!sangoma_boost_data->sigmod) {
		zap_log(ZAP_LOG_ERROR, "Cannot get signaling status in boost channel with no signaling module configured\n");
		return ZAP_FAIL;
	}
	return sangoma_boost_data->sigmod->get_sig_status(zchan, status);
}

/**
 * \brief Initialises an sangoma boost span from configuration variables
 * \param span Span to configure
 * \param sig_cb Callback function for event signals
 * \param ap List of configuration variables
 * \return Success or failure
 */
static ZIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(zap_sangoma_boost_configure_span)
{
#define FAIL_CONFIG_RETURN(retstatus) \
		if (sangoma_boost_data) \
			zap_safe_free(sangoma_boost_data); \
		if (err) \
			zap_safe_free(err) \
		if (hash_locked) \
			zap_mutex_unlock(g_boost_modules_mutex); \
		if (lib) \
			zap_dso_destroy(&lib); \
		return retstatus;

	boost_sigmod_interface_t *sigmod_iface = NULL;
	zap_sangoma_boost_data_t *sangoma_boost_data = NULL;
	const char *local_ip = "127.0.0.65", *remote_ip = "127.0.0.66";
	const char *sigmod = NULL;
	int local_port = 53000, remote_port = 53000;
	const char *var = NULL, *val = NULL;
	int hash_locked = 0;
	zap_dso_lib_t lib = NULL;
	char path[255] = "";
	char *err = NULL;
	unsigned paramindex = 0;
	zap_status_t rc = ZAP_SUCCESS;

	for (; zap_parameters[paramindex].var; paramindex++) {
		var = zap_parameters[paramindex].var;
		val = zap_parameters[paramindex].val;
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
			FAIL_CONFIG_RETURN(ZAP_FAIL);
		}
	}

	if (!sigmod) {
		if (!local_ip && local_port && remote_ip && remote_port && sig_cb) {
			zap_set_string(span->last_error, "missing Sangoma boost IP parameters");
			FAIL_CONFIG_RETURN(ZAP_FAIL);
		}
	}

	sangoma_boost_data = zap_calloc(1, sizeof(*sangoma_boost_data));
	if (!sangoma_boost_data) {
		FAIL_CONFIG_RETURN(ZAP_FAIL);
	}

	/* WARNING: be sure to release this mutex on errors inside this if() */
	zap_mutex_lock(g_boost_modules_mutex);
	hash_locked = 1;
	if (sigmod && !(sigmod_iface = hashtable_search(g_boost_modules_hash, (void *)sigmod))) {
		zap_build_dso_path(sigmod, path, sizeof(path));	
		lib = zap_dso_open(path, &err);
		if (!lib) {
			zap_log(ZAP_LOG_ERROR, "Error loading Sangoma boost signaling module '%s': %s\n", path, err);
			snprintf(span->last_error, sizeof(span->last_error), "Failed to load sangoma boost signaling module %s", path);

			FAIL_CONFIG_RETURN(ZAP_FAIL);
		}
		if (!(sigmod_iface = (boost_sigmod_interface_t *)zap_dso_func_sym(lib, BOOST_INTERFACE_NAME_STR, &err))) {
			zap_log(ZAP_LOG_ERROR, "Failed to read Sangoma boost signaling module interface '%s': %s\n", path, err);
			snprintf(span->last_error, sizeof(span->last_error), "Failed to read Sangoma boost signaling module interface '%s': %s", path, err);

			FAIL_CONFIG_RETURN(ZAP_FAIL);
		}
		rc = sigmod_iface->on_load();
		if (rc != ZAP_SUCCESS) {
			zap_log(ZAP_LOG_ERROR, "Failed to load Sangoma boost signaling module interface '%s': on_load method failed (%d)\n", path, rc);
			FAIL_CONFIG_RETURN(ZAP_FAIL);
		}
		sigmod_iface->pvt = lib;
		sigmod_iface->set_write_msg_cb(zap_boost_write_msg);
		sigmod_iface->set_sig_status_cb(zap_boost_sig_status_change);
		hashtable_insert(g_boost_modules_hash, (void *)sigmod_iface->name, sigmod_iface, HASHTABLE_FLAG_NONE);
		lib = NULL; /* destroying the lib will be done when going down and NOT on FAIL_CONFIG_RETURN */
	}
	zap_mutex_unlock(g_boost_modules_mutex);
	hash_locked = 0;

	if (sigmod_iface) {
		/* try to create the boost queue */
		if (zap_queue_create(&sangoma_boost_data->boost_queue, BOOST_QUEUE_SIZE) != ZAP_SUCCESS) {
			zap_log(ZAP_LOG_ERROR, "Span %s could not create its boost queue!\n", span->name);
			FAIL_CONFIG_RETURN(ZAP_FAIL);
		}
		zap_log(ZAP_LOG_NOTICE, "Span %s will use Sangoma Boost Signaling Module %s\n", span->name, sigmod_iface->name);
		sangoma_boost_data->sigmod = sigmod_iface;
		sigmod_iface->configure_span(span, zap_parameters);
	} else {
		zap_set_string(sangoma_boost_data->mcon.cfg.local_ip, local_ip);
		sangoma_boost_data->mcon.cfg.local_port = local_port;
		zap_set_string(sangoma_boost_data->mcon.cfg.remote_ip, remote_ip);
		sangoma_boost_data->mcon.cfg.remote_port = remote_port;
	}
	sangoma_boost_data->signal_cb = sig_cb;
	span->start = zap_sangoma_boost_start;
	span->stop = zap_sangoma_boost_stop;
	span->signal_data = sangoma_boost_data;
    span->signal_type = ZAP_SIGTYPE_SANGOMABOOST;
    span->outgoing_call = sangoma_boost_outgoing_call;
	span->channel_request = sangoma_boost_channel_request;
	span->get_sig_status = sangoma_boost_get_sig_status;
	span->state_map = &boost_state_map;
	zap_set_flag_locked(span, ZAP_SPAN_SUSPENDED);
	return ZAP_SUCCESS;
}

/**
 * \brief Openzap sangoma boost signaling module definition
 */
EX_DECLARE_DATA zap_module_t zap_module = { 
	/*.name =*/ "sangoma_boost",
	/*.io_load =*/ NULL,
	/*.io_unload =*/ NULL,
	/*.sig_load = */ zap_sangoma_boost_init,
	/*.sig_configure =*/ NULL,
	/*.sig_unload = */zap_sangoma_boost_destroy,
	/*.configure_span_signaling = */ zap_sangoma_boost_configure_span
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
