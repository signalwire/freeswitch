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

#include "private/ftdm_core.h"

FTDM_ENUM_NAMES(CHANNEL_STATE_NAMES, CHANNEL_STATE_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_channel_state, ftdm_channel_state2str, ftdm_channel_state_t, CHANNEL_STATE_NAMES, FTDM_CHANNEL_STATE_INVALID)

FTDM_ENUM_NAMES(CHANNEL_STATE_STATUS_NAMES, CHANNEL_STATE_STATUS_STRINGS)
FTDM_STR2ENUM(ftdm_str2ftdm_state_status, ftdm_state_status2str, ftdm_state_status_t, CHANNEL_STATE_STATUS_NAMES, FTDM_STATE_STATUS_INVALID)

static ftdm_status_t ftdm_core_set_state(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_channel_state_t state, int waitrq);

FT_DECLARE(ftdm_status_t) _ftdm_channel_complete_state(const char *file, const char *func, int line, ftdm_channel_t *fchan)
{
	uint8_t hindex = 0;
	ftdm_time_t diff = 0;
	ftdm_channel_state_t state = fchan->state;

#if 0
	/*  I could not perform this sanity check without more disruptive changes. Ideally we should check here if the signaling module completing the state
		executed a state processor (called ftdm_channel_advance_states() which call fchan->span->state_processor(fchan)) for the state. That is just a 
		sanity check, as in the past we had at least one bug where the signaling module set the state and then accidentally changed the state to a new one 
		without calling ftdm_channel_advance_states(), meaning the state processor for the first state was not executed and that lead to unexpected behavior.

		If we want to be able to perform this kind of sanity check it would be nice to add a new state status (FTDM_STATE_STATUS_PROCESSING), the function
		ftdm_channel_advance_states() would set the state_status to PROCESSING and then the check below for STATUS_NEW would be valid. Currently is not
		valid because the signaling module may be completing the state at the end of the state_processor callback and therefore the state will still be
		in STATUS_NEW, and is perfectly valid ... */

	if (fchan->state_status == FTDM_STATE_STATUS_NEW) {
		ftdm_log_chan_ex(fchan, file, func, line, FTDM_LOG_LEVEL_CRIT, 
						"Asking to complete state change from %s to %s in %llums, but the state is still unprocessed (this might be a bug!)\n", 
						ftdm_channel_state2str(fchan->last_state), ftdm_channel_state2str(state), diff);
	}
#endif

	if (fchan->state_status == FTDM_STATE_STATUS_COMPLETED) {
		ftdm_assert_return(!ftdm_test_flag(fchan, FTDM_CHANNEL_STATE_CHANGE), FTDM_FAIL, "State change flag set but state is already completed\n");
		return FTDM_SUCCESS;
	}

	ftdm_usrmsg_free(&fchan->usrmsg);
	
	ftdm_clear_flag(fchan, FTDM_CHANNEL_STATE_CHANGE);

	if (state == FTDM_CHANNEL_STATE_PROGRESS) {
		ftdm_set_flag(fchan, FTDM_CHANNEL_PROGRESS);
	} else if (state == FTDM_CHANNEL_STATE_PROGRESS_MEDIA) {
		ftdm_set_flag(fchan, FTDM_CHANNEL_PROGRESS);	
		ftdm_test_and_set_media(fchan);
	} else if (state == FTDM_CHANNEL_STATE_UP) {
		ftdm_set_flag(fchan, FTDM_CHANNEL_PROGRESS);
		ftdm_set_flag(fchan, FTDM_CHANNEL_ANSWERED);	
		ftdm_test_and_set_media(fchan);
	} else if (state == FTDM_CHANNEL_STATE_DIALING) {
		ftdm_sigmsg_t msg;
		memset(&msg, 0, sizeof(msg));
		msg.channel = fchan;
		msg.event_id = FTDM_SIGEVENT_DIALING;
		ftdm_span_send_signal(fchan->span, &msg);
	} else if (state == FTDM_CHANNEL_STATE_HANGUP) {
		ftdm_set_echocancel_call_end(fchan);
	}

	/* MAINTENANCE WARNING
	 * we're assuming an indication performed 
	 * via state change will involve a single state change */
	ftdm_ack_indication(fchan, fchan->indication, FTDM_SUCCESS);

	hindex = (fchan->hindex == 0) ? (ftdm_array_len(fchan->history) - 1) : (fchan->hindex - 1);
	
	ftdm_assert(!fchan->history[hindex].end_time, "End time should be zero!\n");

	fchan->history[hindex].end_time = ftdm_current_time_in_ms();
	fchan->last_state_change_time = ftdm_current_time_in_ms();

	fchan->state_status = FTDM_STATE_STATUS_COMPLETED;

	diff = fchan->history[hindex].end_time - fchan->history[hindex].time;

	ftdm_log_chan_ex(fchan, file, func, line, FTDM_LOG_LEVEL_DEBUG, "Completed state change from %s to %s in %"FTDM_TIME_FMT" ms\n",
			ftdm_channel_state2str(fchan->last_state), ftdm_channel_state2str(state), diff);
	

	if (ftdm_test_flag(fchan, FTDM_CHANNEL_BLOCKING)) {
		ftdm_clear_flag(fchan, FTDM_CHANNEL_BLOCKING);
		ftdm_interrupt_signal(fchan->state_completed_interrupt);
	}

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) _ftdm_set_state(const char *file, const char *func, int line,
		ftdm_channel_t *fchan, ftdm_channel_state_t state)
{
	if (fchan->state_status != FTDM_STATE_STATUS_COMPLETED) {
		/* the current state is not completed, setting a new state from a signaling module
		   when the current state is not completed is equivalent to implicitly acknowledging 
		   the current state */
		_ftdm_channel_complete_state(file, func, line, fchan);
	}
	return ftdm_core_set_state(file, func, line, fchan, state, 0);
}

static int ftdm_parse_state_map(ftdm_channel_t *ftdmchan, ftdm_channel_state_t state, ftdm_state_map_t *state_map)
{
	int x = 0, ok = 0;
	ftdm_state_direction_t direction = ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND) ? ZSD_OUTBOUND : ZSD_INBOUND;

	for(x = 0; x < FTDM_MAP_NODE_SIZE; x++) {
		int i = 0, proceed = 0;
		if (!state_map->nodes[x].type) {
			break;
		}

		if (state_map->nodes[x].direction != direction) {
			continue;
		}
		
		if (state_map->nodes[x].check_states[0] == FTDM_CHANNEL_STATE_ANY) {
			proceed = 1;
		} else {
			for(i = 0; i < FTDM_MAP_MAX; i++) {
				if (state_map->nodes[x].check_states[i] == ftdmchan->state) {
					proceed = 1;
					break;
				}
			}
		}

		if (!proceed) {
			continue;
		}
		
		for(i = 0; i < FTDM_MAP_MAX; i++) {
			ok = (state_map->nodes[x].type == ZSM_ACCEPTABLE);
			if (state_map->nodes[x].states[i] == FTDM_CHANNEL_STATE_END) {
				break;
			}
			if (state_map->nodes[x].states[i] == state) {
				ok = !ok;
				goto end;
			}
		}
	}
 end:
	
	return ok;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_cancel_state(const char *file, const char *func, int line, ftdm_channel_t *fchan)
{
	ftdm_time_t diff;
	ftdm_channel_state_t state;
	ftdm_channel_state_t last_state;
	uint8_t hindex = 0;

	if (!ftdm_test_flag(fchan, FTDM_CHANNEL_STATE_CHANGE)) {
		ftdm_log_chan(fchan, FTDM_LOG_WARNING, "Cannot cancel state change from %s to %s, it was already processed\n", 
				ftdm_channel_state2str(fchan->last_state), ftdm_channel_state2str(fchan->state));
		return FTDM_FAIL;
	}

	if (fchan->state_status != FTDM_STATE_STATUS_NEW) {
		ftdm_log_chan(fchan, FTDM_LOG_WARNING, "Failed to cancel state change from %s to %s, state is not new anymore\n", 
				ftdm_channel_state2str(fchan->last_state), ftdm_channel_state2str(fchan->state));
		return FTDM_FAIL;
	}

	/* compute the last history index */
	hindex = (fchan->hindex == 0) ? (ftdm_array_len(fchan->history) - 1) : (fchan->hindex - 1);
	diff = fchan->history[hindex].end_time - fchan->history[hindex].time;

	/* go back in time and revert the state to the previous state */
	state = fchan->state;
	last_state = fchan->last_state;

	fchan->state = fchan->last_state;
	fchan->state_status = FTDM_STATE_STATUS_COMPLETED;
	fchan->last_state = fchan->history[hindex].last_state;
	fchan->hindex = hindex;

	/* clear the state change flag */
	ftdm_clear_flag(fchan, FTDM_CHANNEL_STATE_CHANGE);

	/* ack any pending indications as cancelled */
	ftdm_ack_indication(fchan, fchan->indication, FTDM_ECANCELED);

	/* wake up anyone sleeping waiting for the state change to complete, it won't ever be completed */
	if (ftdm_test_flag(fchan, FTDM_CHANNEL_BLOCKING)) {
		ftdm_clear_flag(fchan, FTDM_CHANNEL_BLOCKING);
		ftdm_interrupt_signal(fchan->state_completed_interrupt);
	}

	/* NOTE
	 * we could potentially also take out the channel from the pendingchans queue, but I believe is easier just leave it,
	 * the only side effect will be a call to ftdm_channel_advance_states() for a channel that has nothing to advance */
	ftdm_log_chan_ex(fchan, file, func, line, FTDM_LOG_LEVEL_DEBUG, "Cancelled state change from %s to %s in %"FTDM_TIME_FMT" ms\n",
			ftdm_channel_state2str(last_state), ftdm_channel_state2str(state), diff);
	
	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_channel_set_state(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_channel_state_t state, int waitrq, ftdm_usrmsg_t *usrmsg)
{
	ftdm_channel_save_usrmsg(ftdmchan, usrmsg);
	
	if (ftdm_core_set_state(file, func, line, ftdmchan, state, waitrq) != FTDM_SUCCESS) {
		ftdm_usrmsg_free(&ftdmchan->usrmsg);
	}
	return FTDM_SUCCESS;
}

/* this function MUST be called with the channel lock held. If waitrq == 1, the channel will be unlocked/locked (never call it with waitrq == 1 with an lock recursivity > 1) */
#define DEFAULT_WAIT_TIME 1000
static ftdm_status_t ftdm_core_set_state(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_channel_state_t state, int waitrq)
{
	ftdm_status_t status;
	int ok = 1;
	int waitms = DEFAULT_WAIT_TIME;	

	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_READY)) {
		ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_ERROR, "Ignored state change request from %s to %s, the channel is not ready\n",
				ftdm_channel_state2str(ftdmchan->state), ftdm_channel_state2str(state));
		return FTDM_FAIL;
	}

	if (ftdmchan->state_status != FTDM_STATE_STATUS_COMPLETED) {
		ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_ERROR, 
		"Ignored state change request from %s to %s, the previous state change has not been processed yet (status = %s)\n",
		ftdm_channel_state2str(ftdmchan->state), ftdm_channel_state2str(state),
		ftdm_state_status2str(ftdmchan->state_status));
		return FTDM_FAIL;
	}

	if (ftdmchan->state == state) {
		ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_WARNING, "Why bother changing state from %s to %s\n", ftdm_channel_state2str(ftdmchan->state), ftdm_channel_state2str(state));
		return FTDM_FAIL;
	}

	if (!ftdmchan->state_completed_interrupt) {
		status = ftdm_interrupt_create(&ftdmchan->state_completed_interrupt, FTDM_INVALID_SOCKET, FTDM_NO_FLAGS);
		if (status != FTDM_SUCCESS) {
			ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_CRIT, 
					"Failed to create state change interrupt when moving from %s to %s\n", ftdm_channel_state2str(ftdmchan->state), ftdm_channel_state2str(state));
			return status;
		}
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_NATIVE_SIGBRIDGE)) {
		goto perform_state_change;
	}

	if (ftdmchan->span->state_map) {
		ok = ftdm_parse_state_map(ftdmchan, state, ftdmchan->span->state_map);
		goto end;
	}

	/* basic core state validation (by-passed if the signaling module provides a state_map) */
	switch(ftdmchan->state) {
	case FTDM_CHANNEL_STATE_HANGUP:
	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			ok = 0;
			switch(state) {
			case FTDM_CHANNEL_STATE_DOWN:
			case FTDM_CHANNEL_STATE_BUSY:
			case FTDM_CHANNEL_STATE_RESTART:
				ok = 1;
				break;
			default:
				break;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_UP:
		{
			ok = 1;
			switch(state) {
			case FTDM_CHANNEL_STATE_PROGRESS:
			case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
			case FTDM_CHANNEL_STATE_RING:
				ok = 0;
				break;
			default:
				break;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_DOWN:
		{
			ok = 0;
			
			switch(state) {
			case FTDM_CHANNEL_STATE_DIALTONE:
			case FTDM_CHANNEL_STATE_COLLECT:
			case FTDM_CHANNEL_STATE_DIALING:
			case FTDM_CHANNEL_STATE_RING:
			case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
			case FTDM_CHANNEL_STATE_PROGRESS:				
			case FTDM_CHANNEL_STATE_IDLE:				
			case FTDM_CHANNEL_STATE_GET_CALLERID:
			case FTDM_CHANNEL_STATE_GENRING:
				ok = 1;
				break;
			default:
				break;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_BUSY:
		{
			switch(state) {
			case FTDM_CHANNEL_STATE_UP:
				ok = 0;
				break;
			default:
				break;
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RING:
		{
			switch(state) {
			case FTDM_CHANNEL_STATE_UP:
				ok = 1;
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

end:

	if (!ok) {
		ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_WARNING, "VETO state change from %s to %s\n", ftdm_channel_state2str(ftdmchan->state), ftdm_channel_state2str(state));
		goto done;
	}

perform_state_change:

	ftdm_log_chan_ex(ftdmchan, file, func, line, FTDM_LOG_LEVEL_DEBUG, "Changed state from %s to %s\n", ftdm_channel_state2str(ftdmchan->state), ftdm_channel_state2str(state));
	ftdmchan->last_state = ftdmchan->state; 
	ftdmchan->state = state;
	ftdmchan->state_status = FTDM_STATE_STATUS_NEW;
	ftdmchan->history[ftdmchan->hindex].file = file;
	ftdmchan->history[ftdmchan->hindex].func = func;
	ftdmchan->history[ftdmchan->hindex].line = line;
	ftdmchan->history[ftdmchan->hindex].state = ftdmchan->state;
	ftdmchan->history[ftdmchan->hindex].last_state = ftdmchan->last_state;
	ftdmchan->history[ftdmchan->hindex].time = ftdm_current_time_in_ms();
	ftdmchan->history[ftdmchan->hindex].end_time = 0;
	ftdmchan->hindex++;
	if (ftdmchan->hindex == ftdm_array_len(ftdmchan->history)) {
		ftdmchan->hindex = 0;
	}
	ftdm_set_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);

	if (ftdmchan->span->pendingchans) {
		ftdm_queue_enqueue(ftdmchan->span->pendingchans, ftdmchan);
	} else {
		/* there is a potential deadlock here, if a signaling module is processing
		 * state changes while the ftdm_span_stop() function is called, the signaling
		 * thread will block until it can acquire the span lock, but the thread calling
		 * ftdm_span_stop() which holds the span lock is waiting on the signaling thread
		 * to finish ... The only reason to acquire the span lock is this flag, new
		 * signaling modules should use the pendingchans queue instead of this flag,
		 * as of today a few modules need still to be updated before we can get rid of
		 * this flag (ie, ftmod_libpri, ftmod_isdn, ftmod_analog) */
		ftdm_set_flag_locked(ftdmchan->span, FTDM_SPAN_STATE_CHANGE);
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_NONBLOCK)) {
		/* the channel should not block waiting for state processing */
		goto done;
	}

	if (!waitrq) {
		/* no waiting was requested */
		goto done;
	}

	/* let's wait for the state change to be completed by the signaling stack */
	ftdm_set_flag(ftdmchan, FTDM_CHANNEL_BLOCKING);

	ftdm_mutex_unlock(ftdmchan->mutex);

	status = ftdm_interrupt_wait(ftdmchan->state_completed_interrupt, waitms);

	ftdm_mutex_lock(ftdmchan->mutex);

	if (status != FTDM_SUCCESS) {
		ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_BLOCKING);
		ftdm_log_chan_ex(ftdmchan, file, func, line, 
				FTDM_LOG_LEVEL_WARNING, "state change from %s to %s was most likely not completed after aprox %dms\n",
				ftdm_channel_state2str(ftdmchan->last_state), ftdm_channel_state2str(state), DEFAULT_WAIT_TIME);
		ok = 0;
		goto done;
	}
done:
	return ok ? FTDM_SUCCESS : FTDM_FAIL;
}

FT_DECLARE(int) ftdm_channel_get_state(const ftdm_channel_t *ftdmchan)
{
	int state;
	ftdm_channel_lock(ftdmchan);
	state = ftdmchan->state;
	ftdm_channel_unlock(ftdmchan);
	return state;
}

FT_DECLARE(const char *) ftdm_channel_get_state_str(const ftdm_channel_t *ftdmchan)
{
	const char *state;
	ftdm_channel_lock(ftdmchan);
	state = ftdm_channel_state2str(ftdmchan->state);
	ftdm_channel_unlock(ftdmchan);
	return state;
}

FT_DECLARE(int) ftdm_channel_get_last_state(const ftdm_channel_t *ftdmchan)
{
	int last_state;
	ftdm_channel_lock(ftdmchan);
	last_state = ftdmchan->last_state;
	ftdm_channel_unlock(ftdmchan);
	return last_state;
}

FT_DECLARE(const char *) ftdm_channel_get_last_state_str(const ftdm_channel_t *ftdmchan)
{
	const char *state;
	ftdm_channel_lock(ftdmchan);
	state = ftdm_channel_state2str(ftdmchan->last_state);
	ftdm_channel_unlock(ftdmchan);
	return state;
}

static void write_history_entry(const ftdm_channel_t *fchan, ftdm_stream_handle_t *stream, int i, ftdm_time_t *prevtime)
{
	char func[255];
	char line[255];
	char states[255];
	const char *filename = NULL;
	snprintf(states, sizeof(states), "%-5.15s => %-5.15s", ftdm_channel_state2str(fchan->history[i].last_state), ftdm_channel_state2str(fchan->history[i].state));
	snprintf(func, sizeof(func), "[%s]", fchan->history[i].func);
	filename = strrchr(fchan->history[i].file, *FTDM_PATH_SEPARATOR);
	if (!filename) {
		filename = fchan->history[i].file;
	} else {
		filename++;
	}
	if (!(*prevtime)) {
		*prevtime = fchan->history[i].time;
	}
	snprintf(line, sizeof(func), "[%s:%d]", filename, fchan->history[i].line);
	stream->write_function(stream, "%-30.30s %-30.30s %-30.30s %lums\n", states, func, line, (fchan->history[i].time - *prevtime));
	*prevtime = fchan->history[i].time;
}

FT_DECLARE(char *) ftdm_channel_get_history_str(const ftdm_channel_t *fchan)
{
	uint8_t i = 0;
	ftdm_time_t currtime = 0;
	ftdm_time_t prevtime = 0;

	ftdm_stream_handle_t stream = { 0 };
	FTDM_STANDARD_STREAM(stream);
	if (!fchan->history[0].file) {
		stream.write_function(&stream, "-- No state history --\n");
		return stream.data;
	}

	stream.write_function(&stream, "%-30.30s %-30.30s %-30.30s %s", 
			"-- States --", "-- Function --", "-- Location --", "-- Time Offset --\n");

	for (i = fchan->hindex; i < ftdm_array_len(fchan->history); i++) {
		if (!fchan->history[i].file) {
			break;
		}
		write_history_entry(fchan, &stream, i, &prevtime);
	}

	for (i = 0; i < fchan->hindex; i++) {
		write_history_entry(fchan, &stream, i, &prevtime);
	}

	currtime = ftdm_current_time_in_ms();

	stream.write_function(&stream, "\nTime since last state change: %lums\n", (currtime - prevtime));

	return stream.data;
}

FT_DECLARE(ftdm_status_t) ftdm_channel_advance_states(ftdm_channel_t *fchan)
{
	ftdm_channel_state_t state;

	ftdm_assert_return(fchan->span->state_processor, FTDM_FAIL, "Cannot process states without a state processor!\n");
	
	while (fchan->state_status == FTDM_STATE_STATUS_NEW) {
		state = fchan->state;
		ftdm_log_chan(fchan, FTDM_LOG_DEBUG, "Executing state processor for %s\n", ftdm_channel_state2str(fchan->state));
		fchan->span->state_processor(fchan);
		if (state == fchan->state && fchan->state_status == FTDM_STATE_STATUS_NEW) {
			/* if the state did not change and is still NEW, the state status must go to PROCESSED
			 * otherwise we don't touch it since is a new state and the old state was
			 * already completed implicitly by the state_processor() function via some internal
			 * call to ftdm_set_state() */
			fchan->state_status = FTDM_STATE_STATUS_PROCESSED;
		}		
	}

	return FTDM_SUCCESS;
}

FT_DECLARE(int) ftdm_check_state_all(ftdm_span_t *span, ftdm_channel_state_t state)
{
	uint32_t j;
	for(j = 1; j <= span->chan_count; j++) {
		if (span->channels[j]->state != state || ftdm_test_flag(span->channels[j], FTDM_CHANNEL_STATE_CHANGE)) {
			return 0;
		}
	}
	return 1;
}

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
