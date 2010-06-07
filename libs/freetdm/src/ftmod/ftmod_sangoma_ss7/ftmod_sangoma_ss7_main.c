/*
 * Copyright (c) 2009, Konrad Hammel <konrad@sangoma.com>
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

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
static sng_isup_event_interface_t   sng_event;
static ftdm_io_interface_t          g_ftdm_sngss7_interface;
ftdm_sngss7_data_t                  g_ftdm_sngss7_data;
ftdm_sched_t                        *sngss7_sched;
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
static void *ftdm_sangoma_ss7_run(ftdm_thread_t *me, void *obj);
static void ftdm_sangoma_ss7_process_state_change(ftdm_channel_t *ftdmchan);

static ftdm_status_t ftdm_sangoma_ss7_stop(ftdm_span_t *span);
static ftdm_status_t ftdm_sangoma_ss7_start(ftdm_span_t *span);
/******************************************************************************/

/* STATE MAP ******************************************************************/
ftdm_state_map_t sangoma_ss7_state_map = {
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
            {FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
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
/******************************************************************************/

/* MONITIOR THREADS ***********************************************************/
static void *ftdm_sangoma_ss7_run(ftdm_thread_t *me, void *obj)
{
    ftdm_interrupt_t    *ftdm_sangoma_ss7_int = NULL;
    ftdm_span_t         *span   = (ftdm_span_t *) obj;
    ftdm_channel_t      *ftdmchan = NULL;

    ftdm_log(FTDM_LOG_INFO, "ftmod_sangoma_ss7 monitor thread for span=%u started.\n", span->span_id);

    /* set IN_THREAD flag so that we know this thread is running */
    ftdm_set_flag(span, FTDM_SPAN_IN_THREAD);

    /* get an interrupt queue for this span */
    if(ftdm_queue_get_interrupt(span->pendingchans, &ftdm_sangoma_ss7_int) != FTDM_SUCCESS) {
        SS7_CRITICAL("Failed to get a ftdm_interrupt for span = %d!\n", span->span_id);
        goto ftdm_sangoma_ss7_run_exit;
    }

    while (ftdm_running() && !(ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD))) {

        /* find out why we returned from the interrupt queue */
        switch ((ftdm_interrupt_wait(ftdm_sangoma_ss7_int, 100))) {
        /**********************************************************************/
        case FTDM_SUCCESS:  /* there was a state change on the span */
            /* process all pending state changes */
            while ((ftdmchan = ftdm_queue_dequeue(span->pendingchans))) {
                /* double check that this channel has a state change pending */
                if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
                    ftdm_sangoma_ss7_process_state_change(ftdmchan);
                } else {
                    SS7_ERROR("ftdm_core reported state change, but state change flag not set on ft-span = %d, ft-chan = %d\n",
                                ftdmchan->span_id,
                                ftdmchan->chan_id);
                }
            } /* while ((ftdmchan = ftdm_queue_dequeue(span->pendingchans)))  */

            break;
        /**********************************************************************/
        case FTDM_TIMEOUT:
            SS7_DEVEL_DEBUG("ftdm_interrupt_wait timed-out on span = %d\n", span->span_id);
            break;
        /**********************************************************************/
        case FTDM_FAIL:
            SS7_ERROR("ftdm_interrupt_wait returned error!\non span = %d\n", span->span_id);
            break;
        /**********************************************************************/
        default:
            SS7_ERROR("ftdm_interrupt_wait returned with unknown code on span = %d\n", span->span_id);
            break;
        /**********************************************************************/
        } /* switch ((ftdm_interrupt_wait(ftdm_sangoma_ss7_int, 100))) */

    } /* master while loop */

    /* clear the IN_THREAD flag so that we know the thread is done */
    ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

    ftdm_log(FTDM_LOG_INFO, "ftmod_sangoma_ss7 monitor thread for span=%u stopping.\n", span->span_id);

    return NULL;

ftdm_sangoma_ss7_run_exit:

    /* clear the IN_THREAD flag so that we know the thread is done */
    ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

    ftdm_log(FTDM_LOG_INFO, "ftmod_sangoma_ss7 monitor thread for span=%u stopping due to error.\n", span->span_id);

    ftdm_sangoma_ss7_stop(span);

    return NULL;
}

/******************************************************************************/
static void ftdm_sangoma_ss7_process_state_change(ftdm_channel_t *ftdmchan)
{
    ftdm_sigmsg_t           sigev;
    ftdm_signaling_status_t status;
    sngss7_chan_data_t      *sngss7_info = ftdmchan->call_data;

    memset(&sigev, 0, sizeof(sigev));

    sigev.chan_id = ftdmchan->chan_id;
    sigev.span_id = ftdmchan->span_id;
    sigev.channel = ftdmchan;

    /*first lock the channel*/
    ftdm_mutex_lock(ftdmchan->mutex);

    /*clear the state change flag...since we might be setting a new state*/
    ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE);

    /*check what state we are supposed to be in*/
    switch (ftdmchan->state) {
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_COLLECT:     /* IAM received but wating on digits */
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                            ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        int i = 0;

        while (ftdmchan->caller_data.cid_num.digits[i] != '\0') {
            i++;
        }

        /* check if the end of pulsing character has arrived or the right number of digits */
        if (ftdmchan->caller_data.cid_num.digits[i] == 0xF) {
            SS7_DEBUG("Received the end of pulsing character\n");

            /*now go to the RING state*/
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RING);
        } else if (i >= g_ftdm_sngss7_data.min_digits) {
            SS7_DEBUG("Received %d digits (min digits = %d)\n", i, g_ftdm_sngss7_data.min_digits);

            /*now go to the RING state*/
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RING);
        } else {
            SS7_INFO("Received %d out of %d so far: %s...starting T35\n",
                        i,
                        g_ftdm_sngss7_data.min_digits,
                        ftdmchan->caller_data.cid_num.digits);

            /* start ISUP t35 */
            if (ftdm_sched_timer(sngss7_info->t35.sched, 
                                 "t35", 
                                 sngss7_info->t35.beat, 
                                 sngss7_info->t35.callback, 
                                 &sngss7_info->t35,
                                 &sngss7_info->t35.heartbeat_timer)) {

                SS7_ERROR("Unable to schedule timer, hanging up call!\n");

                ftdmchan->caller_data.hangup_cause = 41;

                 /* set the flag to indicate this hangup is started from the local side */
                sngss7_set_flag(sngss7_info, FLAG_LOCAL_REL);

                /* end the call */
                ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);
            } else {
                if(ftdm_sched_free_run(sngss7_info->t35.sched)) {
                    SS7_ERROR("Unable to run timer, hanging up call!\n");

                    ftdmchan->caller_data.hangup_cause = 41;

                    /* set the flag to indicate this hangup is started from the local side */
                    sngss7_set_flag(sngss7_info, FLAG_LOCAL_REL);

                    /* end the call */
                    ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);
                } /* if(ftdm_sched_free_run(sngss7_info->t35.sched)) */
            } /* if (ftdm_sched_timer(sngss7_info->t35.sched, */

        } /* checking ST/#digits */

        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_RING:                      /*incoming call request*/
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        SS7_DEBUG("Sending incoming call from %s to %s to FTDM core\n",
                        ftdmchan->caller_data.ani.digits,
                        ftdmchan->caller_data.dnis.digits);


        /* we have enough information to inform FTDM of the call*/
        sigev.event_id = FTDM_SIGEVENT_START;
        ftdm_span_send_signal(ftdmchan->span, &sigev);


        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_DIALING:                   /*outgoing call request*/
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        /*call sangoma_ss7_dial to make outgoing call*/
        ft_to_sngss7_iam(ftdmchan);

        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_PROGRESS:
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        /*check if the channel is inbound or outbound*/
        if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
                /*OUTBOUND...so we were told by the line of this so noifiy the user*/
                sigev.event_id = FTDM_SIGEVENT_PROGRESS;
                ftdm_span_send_signal(ftdmchan->span, &sigev);

                ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);

        } else {
            /* inbound call so we need to send out ACM */
            ft_to_sngss7_acm(ftdmchan);
        }
        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        /* nothing to do at this time */
        break; 
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_UP: /*call is accpeted...both incoming and outgoing*/
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        /*check if the channel is inbound or outbound*/
        if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
                /*OUTBOUND...so we were told by the line that the other side answered*/
                sigev.event_id = FTDM_SIGEVENT_UP;
                ftdm_span_send_signal(ftdmchan->span, &sigev);

        } else {
                /*INBOUND...so FS told us it was going to answer...tell the stack*/
                ft_to_sngss7_anm(ftdmchan);
        }
        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_CANCEL:
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        SS7_ERROR("Hanging up call before informing user on span = %d, chan = %d!\n", ftdmchan->span_id,ftdmchan->chan_id);

        /*now go to the HANGUP complete state*/
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);

        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_TERMINATING:            /*call is hung up remotely*/
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        /* set the flag to indicate this hangup is started from the remote side */
        sngss7_set_flag(sngss7_info, FLAG_REMOTE_REL);

        /*this state is set when the line is hanging up*/
        sigev.event_id = FTDM_SIGEVENT_STOP;
        ftdm_span_send_signal(ftdmchan->span, &sigev);

        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_HANGUP:                  /*call is hung up locally*/
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        /* check for remote hangup flag */
        if (sngss7_test_flag(sngss7_info, FLAG_REMOTE_REL)) {
            SS7_DEBUG("Hanging up remotely requested call!\n"); 
        } else {
            SS7_DEBUG("Hanging up Locally requested call!\n");

             /* set the flag to indicate this hangup is started from the local side */
            sngss7_set_flag(sngss7_info, FLAG_LOCAL_REL);

            /*this state is set when FS is hanging up...so tell the stack*/
            ft_to_sngss7_rel(ftdmchan);
        }

        /*now go to the HANGUP complete state*/
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        if (sngss7_test_flag(sngss7_info, FLAG_REMOTE_REL)) {

            /* check if this hangup is from a tx RSC */
            if (sngss7_test_flag(sngss7_info, FLAG_RESET_TX)) {

                /* go to RESTART State until RSCa is received */
                ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
            } else {
                if (!(sngss7_test_flag(sngss7_info, FLAG_RESET_RX))) {
                    /* send out the release complete */
                    ft_to_sngss7_rlc(ftdmchan);
                }
                /*now go to the DOWN state*/
                ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
            }

            SS7_DEBUG("Completing remotely requested hangup!\n");
        } else {
            SS7_DEBUG("Completing locally requested hangup!\n");
        }

        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_DOWN:           /*the call is finished and removed*/
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
            SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
            break;
        }

        /* check if there is a reset response that needs to be sent */
        if (sngss7_test_flag(sngss7_info, FLAG_RESET_RX)) {
            /* send a RLC */
            ft_to_sngss7_rsca(ftdmchan);

            /* inform Ftdm that the "sig" is up now for this channel */
            status = FTDM_SIG_STATE_UP;
            sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
            sigev.raw_data = &status;
            ftdm_span_send_signal(ftdmchan->span, &sigev);
        }

        /* check if we got the reset response */
        if (sngss7_test_flag(sngss7_info, FLAG_RESET_TX)) {
            /* inform Ftdm that the "sig" is up now for this channel */
            status = FTDM_SIG_STATE_UP;
            sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
            sigev.raw_data = &status;
            ftdm_span_send_signal(ftdmchan->span, &sigev);
        }

        /* check if the circuit has the glare flag up */
        if (sngss7_test_flag(sngss7_info, FLAG_GLARE)) {
            SS7_DEBUG("Glare flag is up....spoofing incoming call on span=%, chan=%d\n",
                        ftdmchan->span_id,ftdmchan->chan_id);
            /* clear all the call specific data */
            sngss7_info->suInstId = 0;
            sngss7_info->spInstId = 0;
            sngss7_info->globalFlg = 0;
            sngss7_info->spId = 0;
            sngss7_info->flags = 0;

            /* close the channel */
            if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
                ftdm_channel_t *close_chan = ftdmchan;
                /* close the channel */
                ftdm_channel_close(&close_chan);
            }

            /* spoof an incoming call */
            sngss7_con_ind(sngss7_info->glare.suInstId, 
                           sngss7_info->glare.spInstId, 
                           sngss7_info->glare.circuit, 
                           &sngss7_info->glare.iam);
        } else {
            /* clear all of the call specific data store in the channel structure */
            sngss7_info->suInstId = 0;
            sngss7_info->spInstId = 0;
            sngss7_info->globalFlg = 0;
            sngss7_info->spId = 0;
            sngss7_info->flags = 0;
        
            if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
                ftdm_channel_t *close_chan = ftdmchan;
                /* close the channel */
                ftdm_channel_close(&close_chan);
            }
        }


        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_RESTART:                    /* CICs needs a Reset */
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

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
            if (sngss7_test_flag(sngss7_info, FLAG_RESET_RX)) {
                /* go to a down state to clear the channel and send RSCa */
                ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
            } /* if (sngss7_test_flag(sngss7_info, FLAG_RESET_RX)) */
        } /* if (inuse) */

        /* check if this is an outgoing RSC */
        if (sngss7_test_flag(sngss7_info, FLAG_RESET_TX)) {
    
            /* make sure we aren't coming from hanging up a call */
            if (ftdmchan->last_state != FTDM_CHANNEL_STATE_HANGUP_COMPLETE) {
                /* send a reset request */
                ft_to_sngss7_rsc(ftdmchan);
            }
    
            /* don't change to the DOWN state as we need to wait for the RSCa */
        } /* if (sngss7_test_flag(sngss7_info, FLAG_RESET_TX)) */
        
        /* send a sig event to the core to disable the channel */
        status = FTDM_SIG_STATE_DOWN;
        sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
        sigev.raw_data = &status;
        ftdm_span_send_signal(ftdmchan->span, &sigev);

        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_SUSPENDED:            /* circuit has been blocked */
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        /**********************************************************************/
        if (sngss7_test_flag(sngss7_info, FLAG_INFID_PAUSED)) {
            SS7_DEBUG("processing pause for span = %, chan = %d\n",ftdmchan->span_id,ftdmchan->chan_id);
            /* bring the channel signaling status to down */
            status = FTDM_SIG_STATE_DOWN;
            sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
            sigev.raw_data = &status;
            ftdm_span_send_signal(ftdmchan->span, &sigev);

            /* check the last state and return to it to allow the call to finish */
            ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);
        }
        if (sngss7_test_flag(sngss7_info, FLAG_INFID_RESUME)) {
            SS7_DEBUG("processing resume for span = %, chan = %d\n",ftdmchan->span_id,ftdmchan->chan_id);
            /* we just resumed...throw the channel into reset */
            sngss7_set_flag(sngss7_info, FLAG_RESET_TX);

            /* clear the resume flag */
            sngss7_clear_flag(sngss7_info, FLAG_INFID_RESUME);

            /* go to restart state */
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
        }
        /**********************************************************************/
        if (sngss7_test_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX)) {
            SS7_DEBUG("processing rx Mn ckt block for span = %, chan = %d\n",ftdmchan->span_id,ftdmchan->chan_id);
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
        if (sngss7_test_flag(sngss7_info, FLAG_CKT_MN_UNBLK_RX)) {
            SS7_DEBUG("processing rx Mn ckt unblock for span = %, chan = %d\n",ftdmchan->span_id,ftdmchan->chan_id);
            /* bring the channel signaling status to up */
            status = FTDM_SIG_STATE_UP;
            sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
            sigev.raw_data = &status;
            ftdm_span_send_signal(ftdmchan->span, &sigev);

            /* clear the unblock flag */
            sngss7_clear_flag(sngss7_info, FLAG_CKT_MN_UNBLK_RX);

            /* send a uba */
            ft_to_sngss7_uba(ftdmchan);

            /* check the last state and return to it to allow the call to finish */
            ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);
        }
        /**********************************************************************/
        if (sngss7_test_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX)) {
            SS7_DEBUG("processing tx Mn ckt block for span = %, chan = %d\n",ftdmchan->span_id,ftdmchan->chan_id);
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
        if (sngss7_test_flag(sngss7_info, FLAG_CKT_MN_UNBLK_TX)) {
            SS7_DEBUG("processing tx Mn ckt unblock for span = %, chan = %d\n",ftdmchan->span_id,ftdmchan->chan_id);
            /* bring the channel signaling status to up */
            status = FTDM_SIG_STATE_UP;
            sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
            sigev.raw_data = &status;
            ftdm_span_send_signal(ftdmchan->span, &sigev);

            /* clear the unblock flag */
            sngss7_clear_flag(sngss7_info, FLAG_CKT_MN_UNBLK_TX);

            /* send a ubl */
            ft_to_sngss7_ubl(ftdmchan);

            /* check the last state and return to it to allow the call to finish */
            ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);
        }
        /**********************************************************************/

        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_IN_LOOP:                              /* COT test */
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 processing state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);

        /* send the lpa */
        ft_to_sngss7_lpa(ftdmchan);

        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_HOLD:
    case FTDM_CHANNEL_STATE_DIALTONE:
    case FTDM_CHANNEL_STATE_BUSY:
    case FTDM_CHANNEL_STATE_ATTN:
    case FTDM_CHANNEL_STATE_GENRING:
    case FTDM_CHANNEL_STATE_GET_CALLERID:
    case FTDM_CHANNEL_STATE_CALLWAITING:
    case FTDM_CHANNEL_STATE_IDLE:
    case FTDM_CHANNEL_STATE_INVALID:
        ftdm_log(FTDM_LOG_DEBUG, "ftmod_sangoma_ss7 does not support state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);
        break;
    /**************************************************************************/
    default:
        /*this is bad...we're in an unknown state...should we kill this channel???*/
        ftdm_log(FTDM_LOG_ERROR, "ftmod_sangoma_ss7 in unknown state %s on span=%u,chan=%u\n",
                                    ftdm_channel_state2str(ftdmchan->state),ftdmchan->span_id,ftdmchan->chan_id);
        break;
    } /*switch (ftdmchan->state)*/

    /*unlock*/
    ftdm_mutex_unlock(ftdmchan->mutex);

    return;
}
/******************************************************************************/

/******************************************************************************/
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(ftdm_sangoma_ss7_outgoing_call)
{
    sngss7_chan_data_t  *sngss7_info;
    int c;

    /* lock the channel while we check whether it is availble */
    ftdm_mutex_lock(ftdmchan->mutex);

    switch (ftdmchan->state) {
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_DOWN:
        /* inform the monitor thread that we want to make a call */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DIALING);

        /* unlock the channel */
        ftdm_mutex_unlock(ftdmchan->mutex);

        /* now we have to wait for either the stack to reject the call because of
         * glare or for the network to acknowledge the call
         */
        c = 0;
        while (c < 20) {

            /* lock the channel while we check whether it is availble */
            ftdm_mutex_lock(ftdmchan->mutex);

            /* extract the sngss7_chan_data structure */
            sngss7_info = (sngss7_chan_data_t  *)ftdmchan->call_data;

            if (sngss7_test_flag(sngss7_info, FLAG_GLARE)) {
                SS7_ERROR("Glare flag on span=%d, chan=%d\n",
                            ftdmchan->span_id,ftdmchan->chan_id);

                /* move the channel to DOWN to clear the existing channel allocations */
                ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
                goto outgoing_glare;
            }

            switch (ftdmchan->state) {
            /******************************************************************/
            case FTDM_CHANNEL_STATE_DIALING:
                break;
            /******************************************************************/
            case FTDM_CHANNEL_STATE_PROGRESS:
            case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
            case FTDM_CHANNEL_STATE_UP:
                SS7_DEBUG("Outgoing call request successful on span=%d, chan=%d\n",
                            ftdmchan->span_id,ftdmchan->chan_id);
                goto outgoing_successful;
                break;
            /******************************************************************/
            default:
                SS7_ERROR("Channel in invalid state (%s) on span=%d, chan=%d...should not happen\n",
                        ftdm_channel_state2str(ftdmchan->state),
                        ftdmchan->span_id,
                        ftdmchan->chan_id);
                goto outgoing_fail;
                break;
            /******************************************************************/
            }

            /* unlock the channel */
            ftdm_mutex_unlock(ftdmchan->mutex);

            /* sleep for a bit to let the state change */
            ftdm_sleep(50);

            /* increment the timeout counter */
            c++;

        } /* while (c < 4) */

        /* if we got here we have timed-out waiting for acknowledgment, kill the call */
        SS7_DEBUG("Timeout waiting for outgoing call to be accepted by network, ok'ing outgoing call on span=%d, chan=%d\n",
                        ftdmchan->span_id,ftdmchan->chan_id);

        /* consider the call good .... for now */
        goto outgoing_successful;

        break;
    /**************************************************************************/
    default:
        /* the channel is already used...this can't be, end the request */
        SS7_ERROR("Outgoing call requested channel in already in use...indicating glare on span=%d, chan=%d\n",
                            ftdmchan->span_id,ftdmchan->chan_id);
        goto outgoing_glare;
        break;        
    /**************************************************************************/
    }

    /* we should not get to this here...all exit points above use goto */
    SS7_ERROR("WE SHOULD NOT HERE HERE!!!!\n");

outgoing_fail:
    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);
    return FTDM_FAIL;

outgoing_glare:
    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);  
    return FTDM_BREAK;

outgoing_successful:
    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);
    return FTDM_SUCCESS;
}
/******************************************************************************/
#if 0
static FIO_CHANNEL_REQUEST_FUNCTION(ftdm_sangoma_ss7_request_chan)
{
    SS7_INFO("KONRAD-> I got called %s\n",__FUNCTION__);
    return FTDM_SUCCESS;
}
#endif

/******************************************************************************/

/* FT-CORE SIG STATUS FUNCTIONS ***********************************************/
static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(ftdm_sangoma_ss7_get_sig_status)
{
    
    if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP)) {
        *status = FTDM_SIG_STATE_UP;
    } else {
        *status = FTDM_SIG_STATE_DOWN;
    }

    return FTDM_SUCCESS;
}

static FIO_CHANNEL_SET_SIG_STATUS_FUNCTION(ftdm_sangoma_ss7_set_sig_status)
{
    SS7_ERROR("Cannot set channel status in this module\n");

    return FTDM_NOTIMPL;
}

/* FT-CORE SIG FUNCTIONS *******************************************************/
static ftdm_status_t ftdm_sangoma_ss7_start(ftdm_span_t *span)
{
    SS7_INFO("Starting span %s:%u.\n",span->name,span->span_id);

    /* clear the monitor thread stop flag */
    ftdm_clear_flag(span, FTDM_SPAN_STOP_THREAD);
    ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

    /* activate all the configured ss7 links */
    if (ft_to_sngss7_activate_all()) {
        SS7_CRITICAL("Failed to activate LibSngSS7!\n");
        return FTDM_FAIL;
    }

    /*start the span monitor thread*/
    if(ftdm_thread_create_detached(ftdm_sangoma_ss7_run, span) != FTDM_SUCCESS) {
        SS7_CRITICAL("Failed to start Span Monitor Thread!\n");
        return FTDM_FAIL;
    }

    SS7_DEBUG("Finished starting span %s:%u.\n",span->name,span->span_id);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t ftdm_sangoma_ss7_stop(ftdm_span_t *span)
{
    /*this function is called by the FT-Core to stop this span*/
    
    ftdm_log(FTDM_LOG_INFO, "Stopping span %s:%u.\n",span->name,span->span_id);
    
    /* throw the STOP_THREAD flag to signal monitor thread stop */
    ftdm_set_flag(span, FTDM_SPAN_STOP_THREAD);
    
    /* wait for the thread to stop */
    while(ftdm_test_flag(span, FTDM_SPAN_IN_THREAD)) {
        ftdm_log(FTDM_LOG_DEBUG, "Waiting for monitor thread to end for %s:%u.\n",span->name,span->span_id);
        ftdm_sleep(1);
    }

    /* KONRAD FIX ME - deconfigure any circuits, links, attached to this span */

    ftdm_log(FTDM_LOG_DEBUG, "Finished stopping span %s:%u.\n",span->name,span->span_id);

    return FTDM_SUCCESS;
}
/******************************************************************************/

/* SIG_FUNCTIONS ***************************************************************/
static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_sangoma_ss7_span_config)
{
    ftdm_log(FTDM_LOG_INFO, "Configuring ftmod_sangoma_ss7 span = %s(%d)...\n", 
                    span->name,
                    span->span_id);

    /* parse the configuration and apply to the global config structure */
    if (ftmod_ss7_parse_xml(ftdm_parameters, span)) {
        ftdm_log(FTDM_LOG_CRIT, "Failed to parse configuration!\n");

        return FTDM_FAIL;
    }

    /* configure libsngss7 */
    if (ft_to_sngss7_cfg()) {
        ftdm_log(FTDM_LOG_CRIT, "Failed to configure LibSngSS7!\n");
        return FTDM_FAIL;
    }

    /*setup the span structure with the info so far*/
    g_ftdm_sngss7_data.sig_cb = sig_cb;

    span->start                     = ftdm_sangoma_ss7_start;
    span->stop                      = ftdm_sangoma_ss7_stop;
    span->signal_type               = FTDM_SIGTYPE_SANGOMASS7;
    span->signal_data               = NULL;
    span->outgoing_call             = ftdm_sangoma_ss7_outgoing_call;
    span->channel_request           = NULL;
    span->signal_cb                 = sig_cb;
    span->get_channel_sig_status    = ftdm_sangoma_ss7_get_sig_status;
    span->set_channel_sig_status    = ftdm_sangoma_ss7_set_sig_status;
    span->state_map                 = &sangoma_ss7_state_map;
    ftdm_set_flag(span, FTDM_SPAN_USE_CHAN_QUEUE);

    ftdm_log(FTDM_LOG_INFO, "Finished configuring ftmod_sangoma_ss7 span = %s(%d)...\n", 
                    span->name,
                    span->span_id);
#if 0
    /* start the span up */
    ftdm_sangoma_ss7_start(span);
#endif
    return FTDM_SUCCESS;
}

/******************************************************************************/
static FIO_SIG_LOAD_FUNCTION(ftdm_sangoma_ss7_init)
{
    /*this function is called by the FT-core to load the signaling module*/

    ftdm_log(FTDM_LOG_INFO, "Loading ftmod_sangoma_ss7...\n");

    /* default the global structure */
    memset(&g_ftdm_sngss7_data, 0x0, sizeof(ftdm_sngss7_data_t));

    sngss7_id = 0;

    /* global flag indicating that general configuration has been done */
    g_ftdm_sngss7_data.gen_config_done = 0;

    /* min. number of digitis to wait for */
    g_ftdm_sngss7_data.min_digits = 7;

    /* function trace initizalation */
    g_ftdm_sngss7_data.function_trace = 1;
    g_ftdm_sngss7_data.function_trace_level = 7;

    /* message (IAM, ACM, ANM, etc) trace initizalation */
    g_ftdm_sngss7_data.message_trace = 1;
    g_ftdm_sngss7_data.message_trace_level = 7;

    /* create a timer schedule */
    if (ftdm_sched_create(&sngss7_sched, "SngSS7_Schedule")) {
        SS7_CRITICAL("Unable to create timer schedule!\n");
        return FTDM_FAIL;
    }

    /* setup the call backs needed by Sangoma_SS7 library */
    sng_event.cc.sng_con_ind = sngss7_con_ind;
    sng_event.cc.sng_con_cfm = sngss7_con_cfm;
    sng_event.cc.sng_con_sta = sngss7_con_sta;
    sng_event.cc.sng_rel_ind = sngss7_rel_ind;
    sng_event.cc.sng_rel_cfm = sngss7_rel_cfm;
    sng_event.cc.sng_dat_ind = sngss7_dat_ind;
    sng_event.cc.sng_fac_ind = sngss7_fac_ind;
    sng_event.cc.sng_fac_cfm = sngss7_fac_cfm;
    sng_event.cc.sng_sta_ind = sngss7_sta_ind;
    sng_event.cc.sng_umsg_ind = sngss7_umsg_ind;
    sng_event.cc.sng_susp_ind = NULL;
    sng_event.cc.sng_resm_ind = NULL;
    sng_event.cc.sng_ssp_sta_cfm = NULL;

    sng_event.sm.sng_log = handle_sng_log;
    sng_event.sm.sng_mtp1_alarm = handle_sng_alarm;
    sng_event.sm.sng_mtp2_alarm = handle_sng_alarm;
    sng_event.sm.sng_mtp3_alarm = handle_sng_alarm;
    sng_event.sm.sng_isup_alarm = handle_sng_alarm;
    sng_event.sm.sng_cc_alarm = handle_sng_alarm;

    /* initalize sng_ss7 library */
    sng_isup_init(&sng_event);

    /* crash on assert fail */
    ftdm_global_set_crash_policy(FTDM_CRASH_ON_ASSERT);

    return FTDM_SUCCESS;
}

/******************************************************************************/
static FIO_SIG_UNLOAD_FUNCTION(ftdm_sangoma_ss7_unload)
{
    /*this function is called by the FT-core to unload the signaling module*/

    ftdm_log(FTDM_LOG_INFO, "Starting ftmod_sangoma_ss7 unload...\n");

    sng_isup_free(); 

    ftdm_log(FTDM_LOG_INFO, "Finished ftmod_sangoma_ss7 unload!\n");
    return FTDM_SUCCESS;
}

/******************************************************************************/
static FIO_API_FUNCTION(ftdm_sangoma_ss7_api)
{
    /* handle this in it's own file....so much to do */
    return (ftdm_sngss7_handle_cli_cmd(stream, data));
}

/******************************************************************************/
static FIO_IO_LOAD_FUNCTION(ftdm_sangoma_ss7_io_init)
{
    assert(fio != NULL);
    memset(&g_ftdm_sngss7_interface, 0, sizeof(g_ftdm_sngss7_interface));

    g_ftdm_sngss7_interface.name = "ss7";
    g_ftdm_sngss7_interface.api = ftdm_sangoma_ss7_api;

    *fio = &g_ftdm_sngss7_interface;

    return FTDM_SUCCESS;
}

/******************************************************************************/


/* START **********************************************************************/
ftdm_module_t ftdm_module =
{
    "sangoma_ss7",                  /*char name[256];                   */
    ftdm_sangoma_ss7_io_init,       /*fio_io_load_t                     */
    NULL,                           /*fio_io_unload_t                   */
    ftdm_sangoma_ss7_init,          /*fio_sig_load_t                    */
    NULL,                           /*fio_sig_configure_t               */
    ftdm_sangoma_ss7_unload,        /*fio_sig_unload_t                  */
    ftdm_sangoma_ss7_span_config    /*fio_configure_span_signaling_t    */
};
/******************************************************************************/

/*****************************************************************************/
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


