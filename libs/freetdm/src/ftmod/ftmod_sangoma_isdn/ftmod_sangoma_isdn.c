/*
 * Copyright (c) 2010, Sangoma Technologies 
 * David Yat Sin <davidy@sangoma.com>
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

#include "ftmod_sangoma_isdn.h"

static void *ftdm_sangoma_isdn_run(ftdm_thread_t *me, void *obj);
static void ftdm_sangoma_isdn_process_state_change(ftdm_channel_t *ftdmchan);

static ftdm_status_t ftdm_sangoma_isdn_stop(ftdm_span_t *span);
static ftdm_status_t ftdm_sangoma_isdn_start(ftdm_span_t *span);

static ftdm_io_interface_t		    	g_sngisdn_io_interface;
static sng_isdn_event_interface_t		g_sngisdn_event_interface;

ftdm_sngisdn_data_t				g_sngisdn_data;

extern ftdm_status_t sng_isdn_activate_trace(ftdm_span_t *span, sngisdn_tracetype_t trace_opt);

ftdm_state_map_t sangoma_isdn_state_map = {
	{
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_ANY_STATE, FTDM_END},
		{FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_SUSPENDED, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
		{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_CANCEL, FTDM_END},
		{FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_END},
		{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_COLLECT, FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_DIALING,
		 FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP,
		 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		{FTDM_CHANNEL_STATE_COLLECT, FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_COLLECT, FTDM_END},
		{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END},
		{FTDM_CHANNEL_STATE_COLLECT, FTDM_CHANNEL_STATE_DOWN, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_RING, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_PROGRESS, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_UP, FTDM_END},
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_UP, FTDM_END},
		{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
		{FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
		{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
		{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
	},
	/**************************************************************************/
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_ANY_STATE, FTDM_END},
		{FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
		{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_CANCEL, FTDM_END},
		{FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_END},
		{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_COLLECT, FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_DIALING,
		 FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP,
		 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END}
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
		{FTDM_CHANNEL_STATE_DIALING, FTDM_END}
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_DIALING, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_DOWN, FTDM_END}
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_PROGRESS, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_END},
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_UP, FTDM_END},
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_UP, FTDM_END},
		{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
		{FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
		{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
		{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
	}
	}
};

static void *ftdm_sangoma_isdn_run(ftdm_thread_t *me, void *obj)
{
	ftdm_interrupt_t	*ftdm_sangoma_isdn_int = NULL;
	ftdm_span_t		*span	= (ftdm_span_t *) obj;
	ftdm_channel_t	*ftdmchan = NULL;

	ftdm_log(FTDM_LOG_INFO, "ftmod_sangoma_isdn monitor thread for span=%u started.\n", span->span_id);

	/* set IN_THREAD flag so that we know this thread is running */
	ftdm_set_flag(span, FTDM_SPAN_IN_THREAD);

	/* get an interrupt queue for this span */
	if (ftdm_queue_get_interrupt(span->pendingchans, &ftdm_sangoma_isdn_int) != FTDM_SUCCESS) {
 		ftdm_log(FTDM_LOG_CRIT, "%s:Failed to get a ftdm_interrupt for span = %s!\n", span->name);
		goto ftdm_sangoma_isdn_run_exit;
	}

	while (ftdm_running() && !(ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD))) {

		/* find out why we returned from the interrupt queue */
		switch ((ftdm_interrupt_wait(ftdm_sangoma_isdn_int, 100))) {
		case FTDM_SUCCESS:  /* there was a state change on the span */
			/* process all pending state changes */
			while ((ftdmchan = ftdm_queue_dequeue(span->pendingchans))) {
				/* double check that this channel has a state change pending */
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
					ftdm_sangoma_isdn_process_state_change(ftdmchan);
				} else {
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "reported state change but state change flag not set\n");
				}
			}

			break;

		case FTDM_TIMEOUT:
			/* twiddle */
			break;

		case FTDM_FAIL:
			ftdm_log(FTDM_LOG_ERROR,"ftdm_interrupt_wait returned error!\non span = %s\n", span->name);
			break;

		default:
			ftdm_log(FTDM_LOG_ERROR,"ftdm_interrupt_wait returned with unknown code on span = %s\n", span->name);
			break;

		}

	}

	/* clear the IN_THREAD flag so that we know the thread is done */
	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

	ftdm_log(FTDM_LOG_INFO, "ftmod_sangoma_isdn monitor thread for span %s stopping.\n", span->name);

	return NULL;

ftdm_sangoma_isdn_run_exit:

	/* clear the IN_THREAD flag so that we know the thread is done */
	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

	ftdm_log(FTDM_LOG_INFO, "ftmod_sangoma_isdn monitor thread for span %s stopping due to error.\n", span->name);

	return NULL;
}

/******************************************************************************/
static void ftdm_sangoma_isdn_process_state_change(ftdm_channel_t *ftdmchan)
{
	ftdm_sigmsg_t		sigev;
	ftdm_signaling_status_t status;
	sngisdn_chan_data_t *sngisdn_info = ftdmchan->call_data;

	memset(&sigev, 0, sizeof(sigev));

	sigev.chan_id = ftdmchan->chan_id;
	sigev.span_id = ftdmchan->span_id;
	sigev.channel = ftdmchan;

	/*first lock the channel*/
	ftdm_mutex_lock(ftdmchan->mutex);

	/*clear the state change flag...since we might be setting a new state*/
	ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "processing state change to %s\n", ftdm_channel_state2str(ftdmchan->state));

	switch (ftdmchan->state) {

	case FTDM_CHANNEL_STATE_COLLECT:	 /* SETUP received but wating on digits */
		{
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {

				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "re-entering state from processing block/unblock request - do nothing\n");
				break;
			}

			/* TODO: Overlap receive not implemented yet - cannot do it the same way as PRI requires sending complete bit */

			/* Go straight to ring state for now */

			/*now go to the RING state*/
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RING);
		}
		break;
	case FTDM_CHANNEL_STATE_RING: /* incoming call request */
		{
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending incoming call from %s to %s to FTDM core\n", ftdmchan->caller_data.ani.digits, ftdmchan->caller_data.dnis.digits);

			/* we have enough information to inform FTDM of the call*/
			sigev.event_id = FTDM_SIGEVENT_START;
			ftdm_span_send_signal(ftdmchan->span, &sigev);
		}
		break;

	case FTDM_CHANNEL_STATE_DIALING: /* outgoing call request */
		{
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "re-entering state from processing block/unblock request ... do nothing \n");
				break;
			}
			sngisdn_snd_setup(ftdmchan);
		}
		break;
	case FTDM_CHANNEL_STATE_PROGRESS:
		{
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "re-entering state from processing block/unblock request ... do nothing \n");
				break;
			}

			/*check if the channel is inbound or outbound*/
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				/*OUTBOUND...so we were told by the line of this so noifiy the user*/
				sigev.event_id = FTDM_SIGEVENT_PROGRESS;
				ftdm_span_send_signal(ftdmchan->span, &sigev);
			} else {
				sngisdn_snd_proceed(ftdmchan);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sigev.event_id = FTDM_SIGEVENT_PROGRESS_MEDIA;
				ftdm_span_send_signal(ftdmchan->span, &sigev);
			} else {
				sngisdn_snd_progress(ftdmchan);
			}
		}
		break; 

	case FTDM_CHANNEL_STATE_UP: /* call is answered */
		{
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "re-entering state from processing block/unblock request ... do nothing \n");
				break;
			}

			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP &&
					((sngisdn_span_data_t*)ftdmchan->span->signal_data)->signalling == SNGISDN_SIGNALING_NET) {
					/* Assign the call to a specific equipment */
					sngisdn_snd_con_complete(ftdmchan);
				}
			}

			/* check if the channel is inbound or outbound */
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				/* OUTBOUND ... so we were told by the line that the other side answered */
				sigev.event_id = FTDM_SIGEVENT_UP;
				ftdm_span_send_signal(ftdmchan->span, &sigev);
			} else {
				/* INBOUND ... so FS told us it just answered ... tell the stack */
				sngisdn_snd_connect(ftdmchan);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_CANCEL:
		{
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "re-entering state from processing block/unblock request ... do nothing \n");
				break;
			}

			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Hanging up call before informing user!\n");

			/* Send a release complete */
			sngisdn_snd_release(ftdmchan);
			/*now go to the HANGUP complete state*/				
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
		}
		break;

	case FTDM_CHANNEL_STATE_TERMINATING: /* call is hung up by the remote end */
		{
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "re-entering state from processing block/unblock request ... do nothing \n");
				break;
			}

			/* this state is set when the line is hanging up */
			sigev.event_id = FTDM_SIGEVENT_STOP;
			ftdm_span_send_signal(ftdmchan->span, &sigev);
		}
		break;

	case FTDM_CHANNEL_STATE_HANGUP:	/* call is hung up locally */
		{
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "re-entering state from processing block/unblock request ... do nothing \n");
				break;
			}

			if (ftdm_test_flag(sngisdn_info, FLAG_REMOTE_REL)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Acknowledging remote hangup\n");
				sngisdn_snd_release(ftdmchan);
			} else if (ftdm_test_flag(sngisdn_info, FLAG_REMOTE_ABORT)) {
				/* Do not send any messages to remote switch as they aborted */
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Clearing local states from remote abort\n");
			} else {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Hanging up call upon local request!\n");

				 /* set the flag to indicate this hangup is started from the local side */
				ftdm_set_flag(sngisdn_info, FLAG_LOCAL_REL);

				/* If we never sent ack to incoming call, we need to send release instead of disconnect */
				if (ftdmchan->last_state == FTDM_CHANNEL_STATE_RING) {
					ftdm_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
					sngisdn_snd_release(ftdmchan);
				} else {
					sngisdn_snd_disconnect(ftdmchan);
				}
				
			}

			/* now go to the HANGUP complete state */
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
		}
		break;

	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
		{
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "re-entering state from processing block/unblock request ... do nothing \n");
				break;
			}

			if (ftdm_test_flag(sngisdn_info, FLAG_REMOTE_REL)) {			
				if (sngisdn_test_flag(sngisdn_info, FLAG_RESET_TX)) {
					/* go to RESTART State until RSCa is received */
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
				}
				/* Do nothing as we will receive a RELEASE COMPLETE from remote switch */
			} else if (ftdm_test_flag(sngisdn_info, FLAG_REMOTE_ABORT) ||
						ftdm_test_flag(sngisdn_info, FLAG_LOCAL_ABORT)) {
				/* If the remote side aborted, we will not get anymore message for this call */
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			} else {
				/* twiddle, waiting on remote confirmation before moving to down */
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Completing locally requested hangup\n");
			}
		}
		break;

	case FTDM_CHANNEL_STATE_DOWN: /* the call is finished and removed */
		{
			if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "re-entering state from processing block/unblock request ... do nothing \n");
				break;
			}

			/* check if there is a reset response that needs to be sent */
			if (sngisdn_test_flag(sngisdn_info, FLAG_RESET_RX)) {
				/* send a RLC */
				sngisdn_snd_release(ftdmchan);

				/* inform Ftdm that the "sig" is up now for this channel */
				status = FTDM_SIG_STATE_UP;
				sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
				sigev.raw_data = &status;
				ftdm_span_send_signal(ftdmchan->span, &sigev);
			}

			/* check if we got the reset response */
			if (sngisdn_test_flag(sngisdn_info, FLAG_RESET_TX)) {
				/* inform Ftdm that the "sig" is up now for this channel */
				status = FTDM_SIG_STATE_UP;
				sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
				sigev.raw_data = &status;
				ftdm_span_send_signal(ftdmchan->span, &sigev);
			}

			/* check if the circuit has the glare flag up */
			if (sngisdn_test_flag(sngisdn_info, FLAG_GLARE)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Glare flag is up....spoofing incoming call\n");
				/* clear all the call specific data */
				clear_call_data(sngisdn_info);

				/* close the channel */
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
					ftdm_channel_t *close_chan = ftdmchan;
					/* close the channel */
					ftdm_channel_close(&close_chan);
				}

				/* spoof an incoming call */
				sngisdn_rcv_con_ind(sngisdn_info->glare.suId,
									sngisdn_info->glare.suInstId,
									sngisdn_info->glare.spInstId,
									&sngisdn_info->glare.setup,
									sngisdn_info->glare.dChan,
									sngisdn_info->glare.ces);

			} else {
				/* clear all of the call specific data store in the channel structure */
				clear_call_data(sngisdn_info);
			
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
					ftdm_channel_t *close_chan = ftdmchan;
					/* close the channel */
					ftdm_channel_close(&close_chan);
				}
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RESTART:
		{
#if 0
			/* TODO: Go through channel restart call states. They do not make sense when running ISDN */
			
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE)) {
				/* bring the call down first...then process the rest of the reset */
				switch (ftdmchan->last_state) {
				/******************************************************************/
				case(FTDM_CHANNEL_STATE_TERMINATING):
				case(FTDM_CHANNEL_STATE_HANGUP):
				case(FTDM_CHANNEL_STATE_HANGUP_COMPLETE):
					/* go back to the last state after taking care of the rest of the restart state */
					ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);
					break;
				/******************************************************************/
				default:
					/* KONRAD: find out what the cause code should be */
					ftdmchan->caller_data.hangup_cause = 41;

					/* change the state to terminatting, it will throw us back here
					* once the call is done
					*/
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
					break;
				/******************************************************************/
				}
			} else {

				/* check if this an incoming RSC */
				if (sngisdn_test_flag(sngisdn_info, FLAG_RESET_RX)) {
					/* go to a down state to clear the channel and send RSCa */
					ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
				} /* if (sngisdn_test_flag(sngisdn_info, FLAG_RESET_RX)) */
			} /* if (inuse) */

			/* check if this is an outgoing RSC */
			if (sngisdn_test_flag(sngisdn_info, FLAG_RESET_TX)) {
		
				/* make sure we aren't coming from hanging up a call */
				if (ftdmchan->last_state != FTDM_CHANNEL_STATE_HANGUP_COMPLETE) {
					/* send a reset request */
					sngisdn_snd_reset(ftdmchan);
				}
		
				/* don't change to the DOWN state as we need to wait for the RSCa */
			} /* if (sngisdn_test_flag(sngisdn_info, FLAG_RESET_TX)) */
			
			/* send a sig event to the core to disable the channel */
			status = FTDM_SIG_STATE_DOWN;
			sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
			sigev.raw_data = &status;
			ftdm_span_send_signal(ftdmchan->span, &sigev);
#endif
		}

		break;
	case FTDM_CHANNEL_STATE_SUSPENDED:
		{
			if (sngisdn_test_flag(sngisdn_info, FLAG_INFID_PAUSED)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "processing PAUSE\n");
				/* bring the channel signaling status to down */
				status = FTDM_SIG_STATE_DOWN;
				sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
				sigev.raw_data = &status;
				ftdm_span_send_signal(ftdmchan->span, &sigev);

				/* check the last state and return to it to allow the call to finish */
				ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);
			}
			if (sngisdn_test_flag(sngisdn_info, FLAG_INFID_RESUME)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "processing RESUME\n");
				
				/* we just resumed...throw the channel into reset */
				sngisdn_set_flag(sngisdn_info, FLAG_RESET_TX);

				/* clear the resume flag */
				sngisdn_clear_flag(sngisdn_info, FLAG_INFID_RESUME);

				/* go to restart state */
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
			}

#if 0
			/* CHECK the equivalent for ISDN */
			if (sngisdn_test_flag(sngisdn_info, FLAG_CKT_MN_BLOCK_RX)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "processing MN_BLOCK_RX\n");

				/* bring the channel signaling status to down */
				status = FTDM_SIG_STATE_DOWN;
				sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
				sigev.raw_data = &status;
				ftdm_span_send_signal(ftdmchan->span, &sigev);

				/* send a BLA */
				
				ft_to_sngss7_bla(ftdmchan);

				/* check the last state and return to it to allow the call to finish */
				ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);
			}
			if (sngisdn_test_flag(sngisdn_info, FLAG_CKT_MN_UNBLK_RX)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "processing MN_UNBLK_RX\n");
				/* bring the channel signaling status to up */
				status = FTDM_SIG_STATE_UP;
				sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
				sigev.raw_data = &status;
				ftdm_span_send_signal(ftdmchan->span, &sigev);

				/* clear the unblock flag */
				sngisdn_clear_flag(sngisdn_info, FLAG_CKT_MN_UNBLK_RX);

				/* send a uba */
				ft_to_sngss7_uba(ftdmchan);

				/* check the last state and return to it to allow the call to finish */
				ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);
			}
			/**********************************************************************/
			if (sngisdn_test_flag(sngisdn_info, FLAG_CKT_MN_BLOCK_TX)) {
				ftdm_log_chan_msg(ftdm_chan, FTDM_LOG_DEBUG, "processing MN_BLOCK_TX\n");
				/* bring the channel signaling status to down */
				status = FTDM_SIG_STATE_DOWN;
				sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
				sigev.raw_data = &status;
				ftdm_span_send_signal(ftdmchan->span, &sigev);

				/* send a blo */
				ft_to_sngss7_blo(ftdmchan);

				/* check the last state and return to it to allow the call to finish */
				ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);
			}
			if (sngisdn_test_flag(sngisdn_info, FLAG_CKT_MN_UNBLK_TX)) {
				ftdm_log_chan_msg(ftdm_chan, FTDM_LOG_DEBUG, "processing MN_UNBLOCK_TX\n");
				/* bring the channel signaling status to up */
				status = FTDM_SIG_STATE_UP;
				sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
				sigev.raw_data = &status;
				ftdm_span_send_signal(ftdmchan->span, &sigev);

				/* clear the unblock flag */
				sngisdn_clear_flag(sngisdn_info, FLAG_CKT_MN_UNBLK_TX);

				/* send a ubl */
				ft_to_sngss7_ubl(ftdmchan);

				/* check the last state and return to it to allow the call to finish */
				ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);
			}
#endif
		}
		break;
	default:
		{
			ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "unsngisdn_rcvd state %s\n", ftdm_channel_state2str(ftdmchan->state));
		}
		break;

	}

	ftdm_mutex_unlock(ftdmchan->mutex);
	return;
}

static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(ftdm_sangoma_isdn_outgoing_call)
{
	sngisdn_chan_data_t  *sngisdn_info;
	int c;

	/* lock the channel while we check whether it is availble */
	ftdm_mutex_lock(ftdmchan->mutex);

	switch (ftdmchan->state) {

	case FTDM_CHANNEL_STATE_DOWN:
		{
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DIALING);

			/* unlock the channel */
			ftdm_mutex_unlock(ftdmchan->mutex);

			/* now we have to wait for either the stack to reject the call because of
			 * glare or for the network to acknowledge the call */
			c = 0;

			while (c < 100) {

				/* lock the channel while we check whether it is availble */
				ftdm_mutex_lock(ftdmchan->mutex);

				/* extract the sngisdn_chan_data structure */
				sngisdn_info = (sngisdn_chan_data_t  *)ftdmchan->call_data;

				if (ftdm_test_flag(sngisdn_info, FLAG_GLARE)) {
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Glare detected\n");
					goto outgoing_glare;
				}

				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_DIALING:
						break;
					case FTDM_CHANNEL_STATE_PROGRESS:
					case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
					case FTDM_CHANNEL_STATE_UP:
						{
							ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Outgoing call request successful\n");
							goto outgoing_successful;
						}
						break;
					case FTDM_CHANNEL_STATE_TERMINATING:
					case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
					case FTDM_CHANNEL_STATE_DOWN:
						{
							/* Remote switch aborted this call */
							ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Outgoing call request failed\n");
							goto outgoing_successful;
						}
						break;
					default:
						{
							ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Channel in invalid state (%s)\n", ftdm_channel_state2str(ftdmchan->state));
							goto outgoing_glare;
						}
						break;
				}

				/* unlock the channel */
				ftdm_mutex_unlock(ftdmchan->mutex);

				/* sleep for a bit to let the state change */
				ftdm_sleep(10);

				/* increment the timeout counter */
				c++;
			}

			/* only way we can get here is if we are still in STATE_DIALING. We did not get a glare, so exit thread and wait for PROCEED/PROGRESS/ALERT/CONNECT or RELEASE from remote switch  */
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Timeout waiting for outgoing call to be accepted by network, returning success anyways\n");

			/* consider the call good .... for now */
			goto outgoing_successful;
		}
		break;

	default:
		{
			/* the channel is already used...this can't be, end the request */
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Outgoing call requested channel in already in use\n");
			goto outgoing_glare;
		}
		break;		  
	}

	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "WE SHOULD NOT BE HERE!!!!\n");

	ftdm_mutex_unlock(ftdmchan->mutex);
	return FTDM_FAIL;

outgoing_glare:
	ftdm_mutex_unlock(ftdmchan->mutex);  
	return FTDM_BREAK;

outgoing_successful:
	ftdm_mutex_unlock(ftdmchan->mutex);
	return FTDM_SUCCESS;
}

static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(ftdm_sangoma_isdn_get_sig_status)
{
	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP)) {
		*status = FTDM_SIG_STATE_UP;
	} else {
		*status = FTDM_SIG_STATE_DOWN;
	}

	return FTDM_SUCCESS;
}

static FIO_CHANNEL_SET_SIG_STATUS_FUNCTION(ftdm_sangoma_isdn_set_sig_status)
{
	ftdm_log(FTDM_LOG_ERROR,"Cannot set channel status in this module\n");
	return FTDM_NOTIMPL;
}

static ftdm_status_t ftdm_sangoma_isdn_start(ftdm_span_t *span)
{
	ftdm_log(FTDM_LOG_INFO,"Starting span %s:%u.\n",span->name,span->span_id);
	if (sng_isdn_stack_activate(span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to activate span %s\n", span->name);
		return FTDM_FAIL;
	}
	/* clear the monitor thread stop flag */
	ftdm_clear_flag(span, FTDM_SPAN_STOP_THREAD);
	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

	/*start the span monitor thread*/
	if (ftdm_thread_create_detached(ftdm_sangoma_isdn_run, span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT,"Failed to start Sangoma ISDN Span Monitor Thread!\n");
		return FTDM_FAIL;
	}

	ftdm_log(FTDM_LOG_DEBUG,"Finished starting span %s\n", span->name);
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_sangoma_isdn_stop(ftdm_span_t *span)
{
	unsigned i;
	ftdm_log(FTDM_LOG_INFO, "Stopping span %s\n", span->name);
	
	/* throw the STOP_THREAD flag to signal monitor thread stop */
	ftdm_set_flag(span, FTDM_SPAN_STOP_THREAD);

	/* wait for the thread to stop */
	while (ftdm_test_flag(span, FTDM_SPAN_IN_THREAD)) {
		ftdm_log(FTDM_LOG_DEBUG, "Waiting for monitor thread to end for span %s\n", span->name);
		ftdm_sleep(10);
	}

	/* FIXME: deconfigure any circuits, links, attached to this span */
	/* TODO: confirm with Moy whether we should start channels at 1 or 0 */
	for (i=1;i<=span->chan_count;i++) {
		ftdm_safe_free(span->channels[i]->call_data);
	}
	ftdm_safe_free(span->signal_data);

	ftdm_log(FTDM_LOG_DEBUG, "Finished stopping span %s\n", span->name);

	return FTDM_SUCCESS;
}

static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_sangoma_isdn_span_config)
{
	sngisdn_span_data_t *span_data;
	
	ftdm_log(FTDM_LOG_INFO, "Configuring ftmod_sangoma_isdn span = %s\n", span->name);	

	span_data = ftdm_calloc(1, sizeof(sngisdn_span_data_t));
	span_data->ftdm_span = span;
	span->signal_data = span_data;

	unsigned i;
	for (i=1;i <= span->chan_count; i++) {
		sngisdn_chan_data_t *chan_data = ftdm_calloc(1, sizeof(sngisdn_chan_data_t));
		chan_data->ftdmchan = span->channels[i];
		span->channels[i]->call_data = chan_data;
	}

	if (ftmod_isdn_parse_cfg(ftdm_parameters, span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to parse configuration\n");
		return FTDM_FAIL;
	}

	if (sng_isdn_stack_cfg(span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Sangoma ISDN Stack configuration failed\n");
		return FTDM_FAIL;
	}


	span->start = ftdm_sangoma_isdn_start;
	span->stop = ftdm_sangoma_isdn_stop;
	span->signal_type = FTDM_SIGTYPE_ISDN;
#if 0
	span->signal_data = NULL;
#endif
	span->outgoing_call = ftdm_sangoma_isdn_outgoing_call;
	span->channel_request = NULL;
	span->signal_cb	= sig_cb;
	span->get_channel_sig_status = ftdm_sangoma_isdn_get_sig_status;
	span->set_channel_sig_status = ftdm_sangoma_isdn_set_sig_status;
	span->state_map	= &sangoma_isdn_state_map;
	ftdm_set_flag(span, FTDM_SPAN_USE_CHAN_QUEUE);

	ftdm_log(FTDM_LOG_INFO, "Finished configuring ftmod_sangoma_isdn span = %s\n", span->name);
	return FTDM_SUCCESS;
}

static FIO_SIG_LOAD_FUNCTION(ftdm_sangoma_isdn_init)
{
	unsigned i;
	ftdm_log(FTDM_LOG_INFO, "Loading ftmod_sangoma_isdn...\n");

	memset(&g_sngisdn_data, 0, sizeof(g_sngisdn_data));

	/* set callbacks */
	g_sngisdn_event_interface.cc.sng_con_ind 	= sngisdn_rcv_con_ind;
	g_sngisdn_event_interface.cc.sng_con_cfm 	= sngisdn_rcv_con_cfm;
	g_sngisdn_event_interface.cc.sng_cnst_ind 	= sngisdn_rcv_cnst_ind;
	g_sngisdn_event_interface.cc.sng_disc_ind 	= sngisdn_rcv_disc_ind;
	g_sngisdn_event_interface.cc.sng_rel_ind 	= sngisdn_rcv_rel_ind;
	g_sngisdn_event_interface.cc.sng_dat_ind 	= sngisdn_rcv_dat_ind;
	g_sngisdn_event_interface.cc.sng_sshl_ind 	= sngisdn_rcv_sshl_ind;
	g_sngisdn_event_interface.cc.sng_sshl_cfm 	= sngisdn_rcv_sshl_cfm;
	g_sngisdn_event_interface.cc.sng_rmrt_ind 	= sngisdn_rcv_rmrt_ind;
	g_sngisdn_event_interface.cc.sng_rmrt_cfm 	= sngisdn_rcv_rmrt_cfm;
	g_sngisdn_event_interface.cc.sng_flc_ind 	= sngisdn_rcv_flc_ind;
	g_sngisdn_event_interface.cc.sng_fac_ind 	= sngisdn_rcv_fac_ind;
	g_sngisdn_event_interface.cc.sng_sta_cfm 	= sngisdn_rcv_sta_cfm;
	g_sngisdn_event_interface.cc.sng_srv_ind 	= sngisdn_rcv_srv_ind;
	g_sngisdn_event_interface.cc.sng_srv_ind	= sngisdn_rcv_srv_cfm;
	g_sngisdn_event_interface.cc.sng_rst_ind 	= sngisdn_rcv_rst_cfm;
	g_sngisdn_event_interface.cc.sng_rst_ind 	= sngisdn_rcv_rst_ind;
	g_sngisdn_event_interface.cc.sng_rst_cfm	= sngisdn_rcv_rst_cfm;

	g_sngisdn_event_interface.lg.sng_log 			= sngisdn_rcv_sng_log;
	g_sngisdn_event_interface.sta.sng_phy_sta_ind 	= sngisdn_rcv_phy_ind;
	g_sngisdn_event_interface.sta.sng_q921_sta_ind	= sngisdn_rcv_q921_ind;
	g_sngisdn_event_interface.sta.sng_q921_trc_ind	= sngisdn_rcv_q921_trace;
	g_sngisdn_event_interface.sta.sng_q931_sta_ind	= sngisdn_rcv_q931_ind;
	g_sngisdn_event_interface.sta.sng_q931_trc_ind	= sngisdn_rcv_q931_trace;
	g_sngisdn_event_interface.sta.sng_cc_sta_ind	= sngisdn_rcv_cc_ind;

	for(i=1;i<=MAX_VARIANTS;i++) {		
		ftdm_mutex_create(&g_sngisdn_data.ccs[i].request_mutex);
	}
	/* initalize sng_isdn library */
	sng_isdn_init(&g_sngisdn_event_interface);

	/* crash on assert fail */
	ftdm_global_set_crash_policy(FTDM_CRASH_ON_ASSERT);
	return FTDM_SUCCESS;
}

static FIO_SIG_UNLOAD_FUNCTION(ftdm_sangoma_isdn_unload)
{
	unsigned i;
	ftdm_log(FTDM_LOG_INFO, "Starting ftmod_sangoma_isdn unload...\n");

	sng_isdn_free();
	
	for(i=1;i<=MAX_VARIANTS;i++) {		
		ftdm_mutex_destroy(&g_sngisdn_data.ccs[i].request_mutex);
	}

	ftdm_log(FTDM_LOG_INFO, "Finished ftmod_sangoma_isdn unload!\n");
	return FTDM_SUCCESS;
}

static FIO_API_FUNCTION(ftdm_sangoma_isdn_api)
{
	ftdm_status_t status = FTDM_SUCCESS;
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;

	if (data) {
		mycmd = ftdm_strdup(data);
		argc = ftdm_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	/*ftdm_log(FTDM_LOG_DEBUG, "Sangoma argc:%d argv[0]:%s argv[1]:%s argv[2]:%s \n", argc, argv[0], argv[1], argv[2]);*/
	if (argc <= 0) {
		ftdm_log(FTDM_LOG_ERROR, "No parameters provided\n");
		goto done;
	}

	if (!strcasecmp(argv[0], "trace")) {
		char *trace_opt;
		
		ftdm_span_t *span;

		if (argc < 3) {
			ftdm_log(FTDM_LOG_ERROR, "Usage: ftdm sangoma_isdn trace <q921|q931> <span name>\n");
			status = FTDM_FAIL;
			goto done;
		}
		trace_opt = argv[1];
		
		status = ftdm_span_find_by_name(argv[2], &span);
		if (FTDM_SUCCESS != status) {
			stream->write_function(stream, "-ERR failed to find span by name %s\n", argv[2]);
			goto done;
		}
		if (!strcasecmp(trace_opt, "q921")) {
			sng_isdn_activate_trace(span, SNGISDN_TRACE_Q921);
		} else if (!strcasecmp(trace_opt, "q931")) {
			sng_isdn_activate_trace(span, SNGISDN_TRACE_Q931);
		} else if (!strcasecmp(trace_opt, "disable")) {
			sng_isdn_activate_trace(span, SNGISDN_TRACE_DISABLE);
		} else {
			stream->write_function(stream, "-ERR invalid trace option <q921|q931> <span name>\n");
		}	
	}
done:
	ftdm_safe_free(mycmd);
	return status;
}

static FIO_IO_LOAD_FUNCTION(ftdm_sangoma_isdn_io_init)
{
	memset(&g_sngisdn_io_interface, 0, sizeof(g_sngisdn_io_interface));

	g_sngisdn_io_interface.name = "sangoma_isdn";
	g_sngisdn_io_interface.api = ftdm_sangoma_isdn_api;

	*fio = &g_sngisdn_io_interface;

	return FTDM_SUCCESS;
}

ftdm_module_t ftdm_module =
{
	"sangoma_isdn",	               /* char name[256]; */
	ftdm_sangoma_isdn_io_init,     /* fio_io_load_t */
	NULL,						   /* fio_io_unload_t */
	ftdm_sangoma_isdn_init,	       /* fio_sig_load_t */
	NULL,                          /* fio_sig_configure_t */
	ftdm_sangoma_isdn_unload,      /* fio_sig_unload_t */
	ftdm_sangoma_isdn_span_config  /* fio_configure_span_signaling_t */
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

/******************************************************************************/


