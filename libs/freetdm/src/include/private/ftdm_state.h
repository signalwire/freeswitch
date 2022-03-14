/*
 * Copyright (c) 2010, Sangoma Technologies
 * Moises Silva <moy@sangoma.com>
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

#ifndef __FTDM_STATE_H__
#define __FTDM_STATE_H__

/*! \file
 * \brief State handling definitions
 * \note Most, if not all of the state handling functions assume you have a lock acquired. Touching the channel
 *       state is a sensitive matter that requires checks and careful thought and is typically a process that
 *       is not encapsulated within a single function, therefore the lock must be explicitly acquired by the 
 *       caller (most of the time, signaling modules), process states, set a new state and process it, and 
 *       finally unlock the channel. See docs/locking.txt fore more info
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	FTDM_CHANNEL_STATE_ANY = -1,
	FTDM_CHANNEL_STATE_END = -1,
	FTDM_CHANNEL_STATE_DOWN,
	FTDM_CHANNEL_STATE_HOLD,
	FTDM_CHANNEL_STATE_SUSPENDED,
	FTDM_CHANNEL_STATE_DIALTONE,
	FTDM_CHANNEL_STATE_COLLECT,
	FTDM_CHANNEL_STATE_RING,
	FTDM_CHANNEL_STATE_RINGING,
	FTDM_CHANNEL_STATE_BUSY,
	FTDM_CHANNEL_STATE_ATTN,
	FTDM_CHANNEL_STATE_GENRING,
	FTDM_CHANNEL_STATE_DIALING,
	FTDM_CHANNEL_STATE_GET_CALLERID,
	FTDM_CHANNEL_STATE_CALLWAITING,
	FTDM_CHANNEL_STATE_RESTART,
	FTDM_CHANNEL_STATE_PROCEED,
	FTDM_CHANNEL_STATE_PROGRESS,
	FTDM_CHANNEL_STATE_PROGRESS_MEDIA,
	FTDM_CHANNEL_STATE_UP,
	FTDM_CHANNEL_STATE_TRANSFER,
	FTDM_CHANNEL_STATE_IDLE,
	FTDM_CHANNEL_STATE_TERMINATING,
	FTDM_CHANNEL_STATE_CANCEL,
	FTDM_CHANNEL_STATE_HANGUP,
	FTDM_CHANNEL_STATE_HANGUP_COMPLETE,
	FTDM_CHANNEL_STATE_IN_LOOP,
	FTDM_CHANNEL_STATE_RESET,
	FTDM_CHANNEL_STATE_INVALID
} ftdm_channel_state_t;
/* Purposely not adding ANY (-1) and END (-1) since FTDM_STR2ENUM_P works only on enums starting at zero */
#define CHANNEL_STATE_STRINGS "DOWN", "HOLD", "SUSPENDED", "DIALTONE", "COLLECT", \
		"RING", "RINGING", "BUSY", "ATTN", "GENRING", "DIALING", "GET_CALLERID", "CALLWAITING", \
		"RESTART", "PROCEED", "PROGRESS", "PROGRESS_MEDIA", "UP", "TRANSFER", "IDLE", "TERMINATING", "CANCEL", \
		"HANGUP", "HANGUP_COMPLETE", "IN_LOOP", "RESET", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_channel_state, ftdm_channel_state2str, ftdm_channel_state_t)

typedef struct {
	const char *file;
	const char *func;
	int line;
	ftdm_channel_state_t state; /*!< Current state (processed or not) */
	ftdm_channel_state_t last_state; /*!< Previous state */
	ftdm_time_t time; /*!< Time the state was set */
	ftdm_time_t end_time; /*!< Time the state processing was completed */
} ftdm_state_history_entry_t;

typedef ftdm_status_t (*ftdm_channel_state_processor_t)(ftdm_channel_t *fchan);

/*!
 * \brief Process channel states by invoking the channel state processing routine
 *        it will keep calling the processing routine while the state status
 *        is FTDM_STATE_STATUS_NEW, it will not do anything otherwise
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_advance_states(ftdm_channel_t *fchan);

FT_DECLARE(ftdm_status_t) _ftdm_channel_complete_state(const char *file, const char *function, int line, ftdm_channel_t *fchan);
#define ftdm_channel_complete_state(obj) _ftdm_channel_complete_state(__FILE__, __FTDM_FUNC__, __LINE__, obj)
FT_DECLARE(int) ftdm_check_state_all(ftdm_span_t *span, ftdm_channel_state_t state);

/*!
 * \brief Status of the current channel state 
 * \note A given state goes thru several status (yes, states for the state!)
 * The order is always FTDM_STATE_STATUS_NEW -> FTDM_STATE_STATUS_PROCESSED -> FTDM_STATUS_COMPLETED
 * However, is possible to go from NEW -> COMPLETED directly when the signaling module explicitly changes 
 * the state of the channel in the middle of processing the current state by calling the ftdm_set_state() API
 *
 * FTDM_STATE_STATUS_NEW - 
 *   Someone just set the state of the channel, either the signaling module or the user (implicitly through a call API). 
 *   This is accomplished by calling ftdm_channel_set_state() which changes the 'state' and 'last_state' memebers of 
 *   the ftdm_channel_t structure.
 *
 * FTDM_STATE_STATUS_PROCESSED -
 *   The signaling module did something based on the new state.
 *
 *   This is accomplished via ftdm_channel_advance_states()
 *
 *   When ftdm_channel_advance_states(), at the very least, if the channel has its state in FTDM_STATE_STATUS_NEW, it
 *   will move to FTDM_STATE_STATUS_PROCESSED, depending on what the signaling module does during the processing
 *   the state may move to FTDM_STATE_STATUS_COMPLETED right after or wait for a signaling specific event to complete it.
 *   It is also possible that more state transitions occur during the execution of ftdm_channel_advance_states() if one
 *   state processing/completion leads to another state change, the function will not return until the chain of events
 *   lead to a state that is not in FTDM_STATE_STATUS_NEW
 *
 * FTDM_STATE_STATUS_COMPLETED - 
 *   The signaling module completed the processing of the state and there is nothing further to be done for this state.
 *
 *   This is accomplished either explicitly by the signaling module by calling ftdm_channel_complete_state() or by
 *   the signaling module implicitly by trying to set the state of the channel to a new state via ftdm_set_state()
 *
 *   When working with blocking channels (FTDM_CHANNEL_NONBLOCK flag not set), the user thread is signaled and unblocked 
 *   so it can continue.
 *
 *   When a state moves to this status is also possible for a signal FTDM_SIGEVENT_INDICATION_COMPLETED to be delivered 
 *   by the core if the state change was associated to an indication requested by the user, 
 */
typedef enum {
	FTDM_STATE_STATUS_NEW,
	FTDM_STATE_STATUS_PROCESSED,
	FTDM_STATE_STATUS_COMPLETED,
	FTDM_STATE_STATUS_INVALID
} ftdm_state_status_t;
#define CHANNEL_STATE_STATUS_STRINGS "NEW", "PROCESSED", "COMPLETED", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_state_status, ftdm_state_status2str, ftdm_state_status_t)

typedef enum {
	ZSM_NONE,
	ZSM_UNACCEPTABLE,
	ZSM_ACCEPTABLE
} ftdm_state_map_type_t;

typedef enum {
	ZSD_INBOUND,
	ZSD_OUTBOUND,
} ftdm_state_direction_t;

#define FTDM_MAP_NODE_SIZE 512
#define FTDM_MAP_MAX FTDM_CHANNEL_STATE_INVALID+2

struct ftdm_state_map_node {
	ftdm_state_direction_t direction;
	ftdm_state_map_type_t type;
	ftdm_channel_state_t check_states[FTDM_MAP_MAX];
	ftdm_channel_state_t states[FTDM_MAP_MAX];
};
typedef struct ftdm_state_map_node ftdm_state_map_node_t;

struct ftdm_state_map {
	ftdm_state_map_node_t nodes[FTDM_MAP_NODE_SIZE];
};
typedef struct ftdm_state_map ftdm_state_map_t;

/*!\brief Cancel the state processing for a channel (the channel must be locked when calling this function)
 * \note Only the core should use this function
 */ 
FT_DECLARE(ftdm_status_t) ftdm_channel_cancel_state(const char *file, const char *func, int line,
		ftdm_channel_t *ftdmchan);

/*!\brief Set the state for a channel (the channel must be locked when calling this function)
 * \note Signaling modules should use ftdm_set_state macro instead
 * \note If this function is called with the wait parameter set to a non-zero value, the recursivity
 *       of the channel lock must be == 1 because the channel will be unlocked/locked when waiting */
FT_DECLARE(ftdm_status_t) ftdm_channel_set_state(const char *file, const char *func, int line,
		ftdm_channel_t *ftdmchan, ftdm_channel_state_t state, int wait, ftdm_usrmsg_t *usrmsg);

/*!\brief Set the state of a channel immediately and implicitly complete the previous state if needed 
 * \note FTDM_SIGEVENT_INDICATION_COMPLETED will be sent if the state change 
 *       is associated to some indication (ie FTDM_CHANNEL_INDICATE_PROCEED)
 * \note The channel must be locked when calling this function
 * */
FT_DECLARE(ftdm_status_t) _ftdm_set_state(const char *file, const char *func, int line,
			ftdm_channel_t *fchan, ftdm_channel_state_t state);
#define ftdm_set_state(obj, s) _ftdm_set_state(__FILE__, __FTDM_FUNC__, __LINE__, obj, s);									\

/*!\brief This macro is deprecated, signaling modules should always lock the channel themselves anyways since they must
 * process first the user pending state changes then set a new state before releasing the lock 
 * this macro is here for backwards compatibility, DO NOT USE IT in new code since it is *always* wrong to set
 * a state in a signaling module without checking and processing the current state first (and for that you must lock the channel)
 */
#define ftdm_set_state_locked(obj, s) \
	do { \
		ftdm_channel_lock(obj); \
		ftdm_channel_set_state(__FILE__, __FTDM_FUNC__, __LINE__, obj, s, 0, NULL);									\
		ftdm_channel_unlock(obj); \
	} while(0);

#define ftdm_set_state_r(obj, s, r) r = ftdm_channel_set_state(__FILE__, __FTDM_FUNC__, __LINE__, obj, s, 0);

#define ftdm_set_state_all(span, state) \
	do { \
		uint32_t _j; \
		ftdm_mutex_lock((span)->mutex); \
		for(_j = 1; _j <= (span)->chan_count; _j++) { \
			if (!FTDM_IS_DCHAN(span->channels[_j])) { \
				ftdm_set_state_locked((span->channels[_j]), state); \
			} \
		} \
		ftdm_mutex_unlock((span)->mutex); \
	} while (0);

#ifdef __cplusplus
} 
#endif

#endif

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
