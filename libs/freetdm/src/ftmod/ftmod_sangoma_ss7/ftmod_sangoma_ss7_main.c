/*
 * Copyright (c) 2009, Sangoma Technologies
 * Konrad Hammel <konrad@sangoma.com>
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
 *
 */

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
static sng_isup_event_interface_t sng_event;
static ftdm_io_interface_t g_ftdm_sngss7_interface;
ftdm_sngss7_data_t g_ftdm_sngss7_data;

/******************************************************************************/

/* PROTOTYPES *****************************************************************/
static void *ftdm_sangoma_ss7_run (ftdm_thread_t * me, void *obj);
static void ftdm_sangoma_ss7_process_stack_event (sngss7_event_data_t *sngss7_event);

static ftdm_status_t ftdm_sangoma_ss7_stop (ftdm_span_t * span);
static ftdm_status_t ftdm_sangoma_ss7_start (ftdm_span_t * span);
/******************************************************************************/


/* STATE MAP ******************************************************************/
ftdm_state_map_t sangoma_ss7_state_map = {
  {
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_END},
	{FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_DOWN,
	 FTDM_CHANNEL_STATE_IN_LOOP, FTDM_CHANNEL_STATE_COLLECT,
	 FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_PROGRESS,
	 FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP,
	 FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_TERMINATING,
	 FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_TERMINATING,
	 FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_HANGUP_COMPLETE,
	 FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_IDLE, FTDM_END}
	},
	{
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_IDLE, FTDM_END},
	{FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_COLLECT, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_COLLECT, FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_COLLECT, FTDM_CHANNEL_STATE_DOWN,
	 FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_COLLECT, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_RING, 
	 FTDM_CHANNEL_STATE_IDLE, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_RING, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
	 FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_END}
	},
	{
	 ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_RINGING, FTDM_END},
	{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
	 FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA,
	 FTDM_CHANNEL_STATE_UP, FTDM_END},
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_PROGRESS, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
	 FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
	 FTDM_CHANNEL_STATE_UP, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_UP, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_CANCEL, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END}
	},
   {
	ZSD_INBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_DOWN, FTDM_END}
	},
	/**************************************************************************/
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_END},
	{FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_DOWN,
	 FTDM_CHANNEL_STATE_IN_LOOP, FTDM_CHANNEL_STATE_DIALING,
	 FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA,
	 FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_CANCEL,
	 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
	 FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_RESTART, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_TERMINATING,
	 FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_HANGUP_COMPLETE,
	 FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_IDLE, FTDM_END}
	},
	{
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_IDLE, FTDM_END},
	{FTDM_CHANNEL_STATE_RESTART, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_DOWN, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_DIALING, FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_DIALING, FTDM_CHANNEL_STATE_DOWN, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_DIALING, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_TERMINATING,
	 FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS,
	 FTDM_CHANNEL_STATE_PROGRESS_MEDIA ,FTDM_CHANNEL_STATE_UP, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_PROGRESS, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
	 FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
	 FTDM_CHANNEL_STATE_UP, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_UP, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_CANCEL, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_HANGUP, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_HANGUP, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END}
	},
   {
	ZSD_OUTBOUND,
	ZSM_UNACCEPTABLE,
	{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END},
	{FTDM_CHANNEL_STATE_SUSPENDED, FTDM_CHANNEL_STATE_RESTART,
	 FTDM_CHANNEL_STATE_DOWN, FTDM_END}
	},
   }
};

static void handle_hw_alarm(ftdm_event_t *e)
{
	sngss7_chan_data_t *ss7_info = NULL;
	ftdm_channel_t *ftdmchan = NULL;
	int x = 0;

	ftdm_assert(e != NULL, "Null event!\n");

	for (x = (g_ftdm_sngss7_data.cfg.procId * MAX_CIC_MAP_LENGTH) + 1; g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0; x++) {
		if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
			ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
			ftdmchan = ss7_info->ftdmchan;
			
			if (e->channel->span_id == ftdmchan->physical_span_id && 
			    e->channel->chan_id == ftdmchan->physical_chan_id) {
				if (e->enum_id == FTDM_OOB_ALARM_TRAP) {
					sngss7_set_ckt_blk_flag(ss7_info, FLAG_GRP_HW_BLOCK_TX);
					if (ftdmchan->state != FTDM_CHANNEL_STATE_SUSPENDED) {
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
					}
				} else {
					sngss7_set_ckt_blk_flag(ss7_info, FLAG_GRP_HW_UNBLK_TX);
					sngss7_clear_ckt_blk_flag(ss7_info, FLAG_GRP_HW_BLOCK_TX);
					if (ftdmchan->state != FTDM_CHANNEL_STATE_SUSPENDED) {
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
					}
				}
			}
		}
	}
}

/* MONITIOR THREADS ***********************************************************/
static void *ftdm_sangoma_ss7_run(ftdm_thread_t * me, void *obj)
{
	ftdm_interrupt_t	*ftdm_sangoma_ss7_int[2];
	ftdm_span_t 		*ftdmspan = (ftdm_span_t *) obj;
	ftdm_channel_t 		*ftdmchan = NULL;
	ftdm_event_t 		*event = NULL;
	sngss7_event_data_t	*sngss7_event = NULL;
	sngss7_span_data_t	*sngss7_span = (sngss7_span_data_t *)ftdmspan->signal_data;

	int b_alarm_test = 1;
	sngss7_chan_data_t *ss7_info=NULL;

	ftdm_log (FTDM_LOG_INFO, "ftmod_sangoma_ss7 monitor thread for span=%u started.\n", ftdmspan->span_id);

	/* set IN_THREAD flag so that we know this thread is running */
	ftdm_set_flag (ftdmspan, FTDM_SPAN_IN_THREAD);

	/* get an interrupt queue for this span for channel state changes */
	if (ftdm_queue_get_interrupt (ftdmspan->pendingchans, &ftdm_sangoma_ss7_int[0]) != FTDM_SUCCESS) {
		SS7_CRITICAL ("Failed to get a ftdm_interrupt for span = %d for channel state changes!\n", ftdmspan->span_id);
		goto ftdm_sangoma_ss7_run_exit;
	}

	/* get an interrupt queue for this span for Trillium events */
	if (ftdm_queue_get_interrupt (sngss7_span->event_queue, &ftdm_sangoma_ss7_int[1]) != FTDM_SUCCESS) {
		SS7_CRITICAL ("Failed to get a ftdm_interrupt for span = %d for Trillium event queue!\n", ftdmspan->span_id);
		goto ftdm_sangoma_ss7_run_exit;
	}

	while (ftdm_running () && !(ftdm_test_flag (ftdmspan, FTDM_SPAN_STOP_THREAD))) {
		int x = 0;
		if (b_alarm_test) {
			b_alarm_test = 0;
			for (x = (g_ftdm_sngss7_data.cfg.procId * MAX_CIC_MAP_LENGTH) + 1; 
			     g_ftdm_sngss7_data.cfg.isupCkt[x].id != 0; x++) {	
				if (g_ftdm_sngss7_data.cfg.isupCkt[x].type == SNG_CKT_VOICE) {
					ss7_info = (sngss7_chan_data_t *)g_ftdm_sngss7_data.cfg.isupCkt[x].obj;
					ftdmchan = ss7_info->ftdmchan;
					if (!ftdmchan) {
						continue;
					}

					if (ftdmchan->alarm_flags != 0) { /* we'll send out block */
						sngss7_set_ckt_blk_flag(ss7_info, FLAG_GRP_HW_BLOCK_TX );
					}  else { /* we'll send out reset */
						sngss7_clear_ckt_blk_flag( ss7_info, FLAG_GRP_HW_BLOCK_TX );
						sngss7_clear_ckt_blk_flag( ss7_info, FLAG_GRP_HW_BLOCK_TX_DN );
						sngss7_set_ckt_blk_flag (ss7_info, FLAG_GRP_HW_UNBLK_TX);
					}
				}
				usleep(50);
			}
			ftdmchan = NULL;
		}

		/* check the channel state queue for an event*/	
		switch ((ftdm_interrupt_multiple_wait(ftdm_sangoma_ss7_int, 2, 100))) {
		/**********************************************************************/
		case FTDM_SUCCESS:	/* process all pending state changes */

			/* clean out all pending channel state changes */
			while ((ftdmchan = ftdm_queue_dequeue (ftdmspan->pendingchans))) {
				
				/*first lock the channel */
				ftdm_mutex_lock(ftdmchan->mutex);

				/* process state changes for this channel until they are all done */
				ftdm_channel_advance_states(ftdmchan);
 
				/* unlock the channel */
				ftdm_mutex_unlock (ftdmchan->mutex);				
			}

			/* clean out all pending stack events */
			while ((sngss7_event = ftdm_queue_dequeue(sngss7_span->event_queue))) {
				ftdm_sangoma_ss7_process_stack_event(sngss7_event);
				ftdm_safe_free(sngss7_event);
			}

			/* signal the core that sig events are queued for processing */
			ftdm_span_trigger_signals(ftdmspan);

			break;
		/**********************************************************************/
		case FTDM_TIMEOUT:
			SS7_DEVEL_DEBUG ("ftdm_interrupt_wait timed-out on span = %d\n",ftdmspan->span_id);

			break;
		/**********************************************************************/
		case FTDM_FAIL:
			SS7_ERROR ("ftdm_interrupt_wait returned error!\non span = %d\n", ftdmspan->span_id);

			break;
		/**********************************************************************/
		default:
			SS7_ERROR("ftdm_interrupt_wait returned with unknown code on span = %d\n",ftdmspan->span_id);

			break;
		/**********************************************************************/
		}

		/* check if there is a GRA to proccess on the span */
		if (ftdm_test_flag(sngss7_span, SNGSS7_RX_GRA_PENDING)) {
			check_if_rx_gra_started(ftdmspan);
		}

		/* check if there is a GRS being processed on the span */
		if (ftdm_test_flag(sngss7_span, SNGSS7_RX_GRS_PENDING)) {
			/* check if the rx_grs has started */
			check_if_rx_grs_started(ftdmspan);

			/* check if the rx_grs has cleared */
			check_if_rx_grs_processed(ftdmspan);
		}

		/* check if there is a UCIC to be processed on the span */
		if (ftdm_test_flag(sngss7_span, SNGSS7_UCIC_PENDING)) {
			/* process the span wide UCIC */
			process_span_ucic(ftdmspan);
		}

		/* check each channel on the span to see if there is an un-procressed SUS/RES flag */
		check_for_res_sus_flag(ftdmspan);

		/* check each channel on the span to see if it needs to be reconfigured */
		check_for_reconfig_flag(ftdmspan);

		/* Poll for events, e.g HW DTMF */
		switch (ftdm_span_poll_event(ftdmspan, 0, NULL)) {
		/**********************************************************************/
		case FTDM_SUCCESS:
			while (ftdm_span_next_event(ftdmspan, &event) == FTDM_SUCCESS) {
				if (event->e_type == FTDM_EVENT_OOB) {
					handle_hw_alarm(event);
				}
			}
			break;
		/**********************************************************************/
		case FTDM_TIMEOUT:
			/* No events pending */
			break;
		/**********************************************************************/
		default:
			SS7_ERROR("%s:Failed to poll span event\n", ftdmspan->name);
		/**********************************************************************/
		}
	}

	/* clear the IN_THREAD flag so that we know the thread is done */
	ftdm_clear_flag (ftdmspan, FTDM_SPAN_IN_THREAD);

	ftdm_log (FTDM_LOG_INFO,"ftmod_sangoma_ss7 monitor thread for span=%u stopping.\n",ftdmspan->span_id);

	return NULL;

ftdm_sangoma_ss7_run_exit:

	/* clear the IN_THREAD flag so that we know the thread is done */
	ftdm_clear_flag (ftdmspan, FTDM_SPAN_IN_THREAD);

	ftdm_log (FTDM_LOG_INFO,"ftmod_sangoma_ss7 monitor thread for span=%u stopping due to error.\n",ftdmspan->span_id);

	ftdm_sangoma_ss7_stop (ftdmspan);

	return NULL;
}

/******************************************************************************/
static void ftdm_sangoma_ss7_process_stack_event (sngss7_event_data_t *sngss7_event)
{
	sngss7_chan_data_t *sngss7_info = NULL;
	ftdm_channel_t *ftdmchan = NULL;

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(sngss7_event->circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", sngss7_event->circuit);
		return;
	}

	/* now that we have the right channel ... put a lock on it so no-one else can use it */
	ftdm_channel_lock(ftdmchan);

	/* while there's a state change present on this channel process it */
	ftdm_channel_advance_states(ftdmchan);

	/* figure out the type of event and send it to the right handler */
	switch (sngss7_event->event_id) {
	/**************************************************************************/
	case (SNGSS7_CON_IND_EVENT):
		handle_con_ind(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit,  &sngss7_event->event.siConEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_CON_CFM_EVENT):
		handle_con_cfm(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit,  &sngss7_event->event.siConEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_CON_STA_EVENT):
		handle_con_sta(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit,  &sngss7_event->event.siCnStEvnt, sngss7_event->evntType);
		break;
	/**************************************************************************/
	case (SNGSS7_REL_IND_EVENT):
		handle_rel_ind(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit,  &sngss7_event->event.siRelEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_REL_CFM_EVENT):
		handle_rel_cfm(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit,  &sngss7_event->event.siRelEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_DAT_IND_EVENT):
		handle_dat_ind(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit,  &sngss7_event->event.siInfoEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_FAC_IND_EVENT):
		handle_fac_ind(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit, sngss7_event->evntType,  &sngss7_event->event.siFacEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_FAC_CFM_EVENT):
		handle_fac_cfm(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit, sngss7_event->evntType,  &sngss7_event->event.siFacEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_UMSG_IND_EVENT):
		handle_umsg_ind(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit);
		break;
	/**************************************************************************/
	case (SNGSS7_STA_IND_EVENT):
		handle_sta_ind(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit, sngss7_event->globalFlg, sngss7_event->evntType,  &sngss7_event->event.siStaEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_SUSP_IND_EVENT):
		handle_susp_ind(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit,  &sngss7_event->event.siSuspEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_RESM_IND_EVENT):
		handle_resm_ind(sngss7_event->suInstId, sngss7_event->spInstId, sngss7_event->circuit,  &sngss7_event->event.siResmEvnt);
		break;
	/**************************************************************************/
	case (SNGSS7_SSP_STA_CFM_EVENT):
		break;
	/**************************************************************************/
	default:
		SS7_ERROR("Unknown Event Id!\n");
		break;
	/**************************************************************************/
	}

	/* while there's a state change present on this channel process it */
	ftdm_channel_advance_states(ftdmchan);

	/* unlock the channel */
	ftdm_channel_unlock(ftdmchan);

}

/******************************************************************************/
ftdm_status_t ftdm_sangoma_ss7_process_state_change (ftdm_channel_t * ftdmchan)
{
	sngss7_chan_data_t *sngss7_info = ftdmchan->call_data;
	sng_isup_inf_t *isup_intf = NULL;
	int state_flag = 1; 
	int i = 0;

	SS7_DEBUG_CHAN(ftdmchan, "ftmod_sangoma_ss7 processing state %s\n", ftdm_channel_state2str (ftdmchan->state));

	/*check what state we are supposed to be in */
	switch (ftdmchan->state) {
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_COLLECT:	/* IAM received but wating on digits */

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		while (ftdmchan->caller_data.dnis.digits[i] != '\0'){
			i++;
		}

		/* check if the end of pulsing (ST) character has arrived or the right number of digits */
		if (ftdmchan->caller_data.dnis.digits[i-1] == 'F') {
			SS7_DEBUG_CHAN(ftdmchan, "Received the end of pulsing character %s\n", "");

			/* remove the ST */
			ftdmchan->caller_data.dnis.digits[i-1] = '\0';
			
			/*now go to the RING state */
			state_flag = 0;
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RING);
			
		} else if (i >= sngss7_info->circuit->min_digits) {
			SS7_DEBUG_CHAN(ftdmchan, "Received %d digits (min digits = %d)\n", i, sngss7_info->circuit->min_digits);

			/*now go to the RING state */
			state_flag = 0;
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RING);
			
		} else {
			/* if we are coming from idle state then we have already been here once before */
			if (ftdmchan->last_state != FTDM_CHANNEL_STATE_IDLE) {
				SS7_INFO_CHAN(ftdmchan,"Received %d out of %d so far: %s...starting T35\n",
										i,
										sngss7_info->circuit->min_digits,
										ftdmchan->caller_data.dnis.digits);
		
				/* start ISUP t35 */
				if (ftdm_sched_timer (sngss7_info->t35.sched,
										"t35",
										sngss7_info->t35.beat,
										sngss7_info->t35.callback,
										&sngss7_info->t35,
										&sngss7_info->t35.hb_timer_id)) {
		
					SS7_ERROR ("Unable to schedule timer, hanging up call!\n");
		
					ftdmchan->caller_data.hangup_cause = 41;
		
					/* set the flag to indicate this hangup is started from the local side */
					sngss7_set_ckt_flag (sngss7_info, FLAG_LOCAL_REL);
		
					/* end the call */
					state_flag = 0;
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);
				} /* if (ftdm_sched_timer(sngss7_info->t35.sched, */
			} /* if (ftdmchan->last_state != FTDM_CHANNEL_STATE_IDLE) */
		} /* checking ST/#digits */

	  break;

	/**************************************************************************/
	case FTDM_CHANNEL_STATE_RING:	/*incoming call request */

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		/* kill t35 if active */
		if (sngss7_info->t35.hb_timer_id) {
			ftdm_sched_cancel_timer (sngss7_info->t35.sched, sngss7_info->t35.hb_timer_id);
		}

		SS7_DEBUG_CHAN(ftdmchan, "Sending incoming call from %s to %s to FTDM core\n",
					ftdmchan->caller_data.ani.digits,
					ftdmchan->caller_data.dnis.digits);


		/* we have enough information to inform FTDM of the call */
		sngss7_send_signal(sngss7_info, FTDM_SIGEVENT_START);

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_DIALING:	/*outgoing call request */

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		SS7_DEBUG_CHAN(ftdmchan, "Sending outgoing call from \"%s\" to \"%s\" to LibSngSS7\n",
					   ftdmchan->caller_data.ani.digits,
					   ftdmchan->caller_data.dnis.digits);

		/*call sangoma_ss7_dial to make outgoing call */
		ft_to_sngss7_iam(ftdmchan);

		break;
	/**************************************************************************/
	/* We handle RING indication the same way we would indicate PROGRESS */
	case FTDM_CHANNEL_STATE_RINGING:
	case FTDM_CHANNEL_STATE_PROGRESS:

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		/*check if the channel is inbound or outbound */
		if (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
			/*OUTBOUND...so we were told by the line of this so noifiy the user */
			sngss7_send_signal(sngss7_info, FTDM_SIGEVENT_PROGRESS);

			/* move to progress media  */
			state_flag = 0;
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
		} else {
			/* inbound call so we need to send out ACM */
			if (!sngss7_test_ckt_flag(sngss7_info, FLAG_SENT_ACM)) {
				sngss7_set_ckt_flag(sngss7_info, FLAG_SENT_ACM);
				ft_to_sngss7_acm(ftdmchan);
			}
		}

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		if (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
			/* inform the user there is media avai */
			sngss7_send_signal(sngss7_info, FTDM_SIGEVENT_PROGRESS_MEDIA);
		} else {
			if (!sngss7_test_ckt_flag(sngss7_info, FLAG_SENT_ACM)) {
				sngss7_set_ckt_flag(sngss7_info, FLAG_SENT_ACM);
				ft_to_sngss7_acm(ftdmchan);
			}
			ft_to_sngss7_cpg(ftdmchan);
		}

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_UP:	/*call is accpeted...both incoming and outgoing */

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		/*check if the channel is inbound or outbound */
		if (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
			/*OUTBOUND...so we were told by the line that the other side answered */
			sngss7_send_signal(sngss7_info, FTDM_SIGEVENT_UP);
		} else {
			/*INBOUND...so FS told us it was going to answer...tell the stack */
			ft_to_sngss7_anm(ftdmchan);
		}

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_CANCEL:

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		SS7_ERROR_CHAN(ftdmchan,"Hanging up call before informing user%s\n", " ");

		/*now go to the HANGUP complete state */
		state_flag = 0;
		ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_TERMINATING:	/*call is hung up remotely */

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		/* set the flag to indicate this hangup is started from the remote side */
		sngss7_set_ckt_flag (sngss7_info, FLAG_REMOTE_REL);

		/*this state is set when the line is hanging up */
		sngss7_send_signal(sngss7_info, FTDM_SIGEVENT_STOP);

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_HANGUP:	/*call is hung up locally */

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		/* check for remote hangup flag */
		if (sngss7_test_ckt_flag (sngss7_info, FLAG_REMOTE_REL)) {
			/* remote release ...do nothing here */
			SS7_DEBUG_CHAN(ftdmchan,"Hanging up remotely requested call!%s\n", "");
		} else if (sngss7_test_ckt_flag (sngss7_info, FLAG_GLARE)) {
			/* release due to glare */
			SS7_DEBUG_CHAN(ftdmchan,"Hanging up requested call do to glare%s\n", "");
		} else 	{
			/* set the flag to indicate this hangup is started from the local side */
			sngss7_set_ckt_flag (sngss7_info, FLAG_LOCAL_REL);

			/*this state is set when FS is hanging up...so tell the stack */
			ft_to_sngss7_rel (ftdmchan);

			SS7_DEBUG_CHAN(ftdmchan,"Hanging up locally requested call!%s\n", "");
		}

		/*now go to the HANGUP complete state */
		state_flag = 0;
		ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);

		break;

	/**************************************************************************/
	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		if (sngss7_test_ckt_flag (sngss7_info, FLAG_REMOTE_REL)) {
			/* check if this hangup is from a tx RSC */
			if (sngss7_test_ckt_flag (sngss7_info, FLAG_RESET_TX)) {
				/* go to RESTART State until RSCa is received */
				state_flag = 0;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
			} else {
				/* if the hangup is from a rx RSC, rx GRS, or glare don't sent RLC */
				if (!(sngss7_test_ckt_flag(sngss7_info, FLAG_RESET_RX)) &&
					!(sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX)) &&
					!(sngss7_test_ckt_flag(sngss7_info, FLAG_GLARE))) {

					/* send out the release complete */
					ft_to_sngss7_rlc (ftdmchan);
				}

				/*now go to the DOWN state */
				state_flag = 0;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			}

			SS7_DEBUG_CHAN(ftdmchan,"Completing remotely requested hangup!%s\n", "");
		} else if (sngss7_test_ckt_flag (sngss7_info, FLAG_LOCAL_REL)) {

			/* if this hang up is do to a rx RESET we need to sit here till the RSP arrives */
			if (sngss7_test_ckt_flag (sngss7_info, FLAG_RESET_TX_RSP)) {
				/* go to the down state as we have already received RSC-RLC */
				state_flag = 0;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			}

			/* if it's a local release the user sends us to down */
			SS7_DEBUG_CHAN(ftdmchan,"Completing locally requested hangup!%s\n", "");
		} else if (sngss7_test_ckt_flag (sngss7_info, FLAG_GLARE)) {
			SS7_DEBUG_CHAN(ftdmchan,"Completing requested hangup due to glare!%s\n", "");
			state_flag = 0;
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		} else {
			SS7_DEBUG_CHAN(ftdmchan,"Completing requested hangup for unknown reason!%s\n", "");
		}

		break;

	/**************************************************************************/
	case FTDM_CHANNEL_STATE_DOWN:	/*the call is finished and removed */

		if (ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED) {
			SS7_DEBUG("re-entering state from processing block/unblock request ... do nothing\n");
			break;
		}

		/* check if there is a reset response that needs to be sent */
		if (sngss7_test_ckt_flag (sngss7_info, FLAG_RESET_RX)) {
			/* send a RSC-RLC */
			ft_to_sngss7_rsca (ftdmchan);

			/* clear the reset flag  */
			clear_rx_rsc_flags(sngss7_info);
		}

		/* check if there was a GRS that needs a GRA */
		if ((sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX)) &&
			(sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX_DN)) &&
			(sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX_CMPLT))) {

			/* check if this is the base circuit and send out the GRA
			 * we insure that this is the last circuit to have the state change queued */
			if (sngss7_info->rx_grs.range) {
				/* send out the GRA */
				ft_to_sngss7_gra(ftdmchan);

				/* clean out the spans GRS structure */
				clear_rx_grs_data(sngss7_info);
			}

			/* clear the grp reset flag */
			clear_rx_grs_flags(sngss7_info);
		}

		/* check if we got the reset response */
		if (sngss7_test_ckt_flag(sngss7_info, FLAG_RESET_TX_RSP)) {
			/* clear the reset flag  */
			clear_tx_rsc_flags(sngss7_info);
		}

		if (sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_TX_RSP)) {
			/* clear the reset flag  */
			clear_tx_grs_flags(sngss7_info);
			if (sngss7_info->rx_gra.range) {
				/* clean out the spans GRA structure */
				clear_rx_gra_data(sngss7_info);
			}
		}

		/* check if we came from reset (aka we just processed a reset) */
		if ((ftdmchan->last_state == FTDM_CHANNEL_STATE_RESTART) || 
			(ftdmchan->last_state == FTDM_CHANNEL_STATE_SUSPENDED)) {

			/* check if reset flags are up indicating there is more processing to do yet */
			if (!(sngss7_test_ckt_flag (sngss7_info, FLAG_RESET_TX)) &&
				!(sngss7_test_ckt_flag (sngss7_info, FLAG_RESET_RX)) &&
				!(sngss7_test_ckt_flag (sngss7_info, FLAG_GRP_RESET_TX)) &&
				!(sngss7_test_ckt_flag (sngss7_info, FLAG_GRP_RESET_RX))) {

				/* now check if there is an active block */
				if (!(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_LC_BLOCK_RX)) &&
					!(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX)) &&
					!(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX)) &&
					!(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX)) &&
					!(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_TX)) &&
					!(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_MN_BLOCK_RX)) &&
					!(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_MN_BLOCK_TX))) {
				
					/* check if the sig status is down, and bring it up if it isn't */
					if (!ftdm_test_flag (ftdmchan, FTDM_CHANNEL_SIG_UP)) {
						SS7_DEBUG_CHAN(ftdmchan,"All reset flags cleared %s\n", "");
						/* all flags are down so we can bring up the sig status */
						sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_UP);
					} /* if (!ftdm_test_flag (ftdmchan, FTDM_CHANNEL_SIG_UP)) */
				} /* if !blocked */
			} else {
				SS7_DEBUG_CHAN(ftdmchan,"Reset flags present (0x%X)\n", sngss7_info->ckt_flags);
			
				/* there is still another reset pending so go back to reset*/
				state_flag = 0;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
			}
		} /* if ((ftdmchan->last_state == FTDM_CHANNEL_STATE_RESTART) */

		/* check if t35 is active */
		if (sngss7_info->t35.hb_timer_id) {
			ftdm_sched_cancel_timer (sngss7_info->t35.sched, sngss7_info->t35.hb_timer_id);
		}

		/* clear all of the call specific data store in the channel structure */
		sngss7_info->suInstId = 0;
		sngss7_info->spInstId = 0;
		sngss7_info->globalFlg = 0;
		sngss7_info->spId = 0;

		/* clear any call related flags */
		sngss7_clear_ckt_flag (sngss7_info, FLAG_REMOTE_REL);
		sngss7_clear_ckt_flag (sngss7_info, FLAG_LOCAL_REL);
		sngss7_clear_ckt_flag (sngss7_info, FLAG_SENT_ACM);


		if (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_OPEN)) {
			ftdm_channel_t *close_chan = ftdmchan;
			/* close the channel */
			ftdm_channel_close (&close_chan);
		}

		/* check if there is a glared call that needs to be processed */
		if (sngss7_test_ckt_flag(sngss7_info, FLAG_GLARE)) {
			
			/* clear the glare flag */
			sngss7_clear_ckt_flag (sngss7_info, FLAG_GLARE);

			/* check if we have an IAM stored...if we don't have one just exit */
			if (sngss7_info->glare.circuit != 0) {
				/* send the saved call back in to us */
				handle_con_ind (0, 
								sngss7_info->glare.spInstId, 
								sngss7_info->glare.circuit, 
								&sngss7_info->glare.iam);

				/* clear the glare info */
				memset(&sngss7_info->glare, 0x0, sizeof(sngss7_glare_data_t));
			}
		}

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_RESTART:	/* CICs needs a Reset */

		if (sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_UCIC_BLOCK)) {
			if ((sngss7_test_ckt_flag(sngss7_info, FLAG_RESET_RX)) ||
				(sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX))) {

				SS7_DEBUG_CHAN(ftdmchan,"Incoming Reset request on CIC in UCIC block, removing UCIC block%s\n", "");

				/* set the unblk flag */
				sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_UCIC_UNBLK);

				/* clear the block flag */
				sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_UCIC_BLOCK);

				/* process the flag */
				state_flag = 0;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

				/* break out of the processing for now */
				break;
			}
		}


		/* check if this is an outgoing RSC */
		if ((sngss7_test_ckt_flag(sngss7_info, FLAG_RESET_TX)) &&
			!(sngss7_test_ckt_flag(sngss7_info, FLAG_RESET_SENT))) {

			/* send a reset request */
			ft_to_sngss7_rsc (ftdmchan);
			sngss7_set_ckt_flag(sngss7_info, FLAG_RESET_SENT);

		} /* if (sngss7_test_ckt_flag(sngss7_info, FLAG_RESET_TX)) */

		/* check if this is the first channel of a GRS (this flag is thrown when requesting reset) */
		if ( (sngss7_test_ckt_flag (sngss7_info, FLAG_GRP_RESET_TX)) &&
			!(sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_SENT)) &&
			(sngss7_test_ckt_flag(sngss7_info, FLAG_GRP_RESET_BASE))) {

				/* send out the grs */
				ft_to_sngss7_grs (ftdmchan);

		}
	
		/* if the sig_status is up...bring it down */
		if (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_SIG_UP)) {
			sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_DOWN);
		}

		if (sngss7_test_ckt_flag (sngss7_info, FLAG_GRP_RESET_RX)) {
			/* set the grp reset done flag so we know we have finished this reset */
			sngss7_set_ckt_flag(sngss7_info, FLAG_GRP_RESET_RX_DN);
		} /* if (sngss7_test_ckt_flag (sngss7_info, FLAG_GRP_RESET_RX)) */


		if (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_INUSE)) {
			/* bring the call down first...then process the rest of the reset */
			switch (ftdmchan->last_state){
			/******************************************************************/
			case (FTDM_CHANNEL_STATE_TERMINATING):
				state_flag = 0;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);	
				break;
			/******************************************************************/
			case (FTDM_CHANNEL_STATE_HANGUP):
			case (FTDM_CHANNEL_STATE_HANGUP_COMPLETE):
				/* go back to the last state after taking care of the rest of the restart state */
				state_flag = 0;
				ftdm_set_state(ftdmchan, ftdmchan->last_state);
			break;
			/******************************************************************/
			case (FTDM_CHANNEL_STATE_IN_LOOP):
				/* we screwed up in a COT/CCR, remove the loop */
				ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_LOOP, NULL);

				/* go to down */
				state_flag = 0;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
				break;
			/******************************************************************/
			default:
				/* KONRAD: find out what the cause code should be */
				ftdmchan->caller_data.hangup_cause = 41;

				/* change the state to terminatting, it will throw us back here
				 * once the call is done
				 */
				state_flag = 0;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
			break;
			/******************************************************************/
			} /* switch (ftdmchan->last_state) */
		} else {
			/* check if this an incoming RSC or we have a response already */
			if (sngss7_test_ckt_flag (sngss7_info, FLAG_RESET_RX) ||
				sngss7_test_ckt_flag (sngss7_info, FLAG_RESET_TX_RSP) ||
				sngss7_test_ckt_flag (sngss7_info, FLAG_GRP_RESET_TX_RSP) ||
				sngss7_test_ckt_flag (sngss7_info, FLAG_GRP_RESET_RX_CMPLT)) {
	
				SS7_DEBUG_CHAN(ftdmchan, "Reset processed moving to DOWN (0x%X)\n", sngss7_info->ckt_flags);
	
				/* go to a down state to clear the channel and send the response */
				state_flag = 0;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			} else {
				SS7_DEBUG_CHAN(ftdmchan, "Waiting on Reset Rsp/Grp Reset to move to DOWN (0x%X)\n", sngss7_info->ckt_flags);
			}
		}

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_SUSPENDED:	/* circuit has been blocked */

		  SS7_DEBUG_CHAN(ftdmchan,"Current flags: ckt=0x%X, blk=0x%X\n", 
									sngss7_info->ckt_flags,
									sngss7_info->blk_flags);

		/**********************************************************************/
		if (sngss7_test_ckt_flag(sngss7_info, FLAG_INFID_RESUME)) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing RESUME%s\n", "");

			/* clear the RESUME flag */
			sngss7_clear_ckt_flag(sngss7_info, FLAG_INFID_RESUME);

			/* clear the PAUSE flag */
			sngss7_clear_ckt_flag(sngss7_info, FLAG_INFID_PAUSED);

			/* if there are any resets present */
			if ((sngss7_test_ckt_flag (sngss7_info, FLAG_RESET_TX)) ||
				(sngss7_test_ckt_flag (sngss7_info, FLAG_RESET_RX)) ||
				(sngss7_test_ckt_flag (sngss7_info, FLAG_GRP_RESET_TX)) ||
				(sngss7_test_ckt_flag (sngss7_info, FLAG_GRP_RESET_RX))) {

				/* don't bring up the sig status but also move to reset */
				goto suspend_goto_restart;
			} else {
				/* bring the sig status back up */
				sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_UP);
			}
		} /* if (sngss7_test_flag(sngss7_info, FLAG_INFID_RESUME)) */

		if (sngss7_test_ckt_flag(sngss7_info, FLAG_INFID_PAUSED)) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing PAUSE%s\n", "");

			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP)) {
				/* bring the sig status down */
				sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_DOWN);
			}
		} /* if (sngss7_test_ckt_flag(sngss7_info, FLAG_INFID_PAUSED)) { */

		/**********************************************************************/
		if (sngss7_test_ckt_blk_flag (sngss7_info, FLAG_CKT_MN_BLOCK_RX) &&
			!sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX_DN)) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing CKT_MN_BLOCK_RX flag %s\n", "");

			/* bring the sig status down */
			sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_DOWN);

			/* send a BLA */
			ft_to_sngss7_bla (ftdmchan);

			/* throw the done flag */
			sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX_DN);

			/* check the last state and return to it to allow the call to finish */
			goto suspend_goto_last;
		}

		if (sngss7_test_ckt_blk_flag (sngss7_info, FLAG_CKT_MN_UNBLK_RX)){
			SS7_DEBUG_CHAN(ftdmchan, "Processing CKT_MN_UNBLK_RX flag %s\n", "");

			/* clear the block flags */
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX);
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX_DN);

			/* clear the unblock flag */
			sngss7_clear_ckt_blk_flag (sngss7_info, FLAG_CKT_MN_UNBLK_RX);

			/* not bring the cic up if there is a hardware block */
			if( !sngss7_test_ckt_blk_flag(sngss7_info, (FLAG_GRP_HW_BLOCK_TX | FLAG_GRP_HW_BLOCK_TX_DN) ) ) {
				/* bring the sig status up */
				sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_UP);

				/* send a uba */
				ft_to_sngss7_uba (ftdmchan);
			}

			/* check the last state and return to it to allow the call to finish */
			goto suspend_goto_last;
		}


		/**********************************************************************/
		/* hardware block/unblock tx */
		if (sngss7_test_ckt_blk_flag (sngss7_info, FLAG_GRP_HW_BLOCK_TX ) &&
			!sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_TX_DN )) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing FLAG_GRP_HW_BLOCK_TX flag %s\n", "");
			sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_DOWN);

			/* dont send block again if the channel is already blocked by maintenance */
			if( !sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX) &&
			     !sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX_DN) 
			   )  {
				ft_to_sngss7_blo(ftdmchan);
			}
			sngss7_set_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_TX_DN);

			goto suspend_goto_last;
		}

		if (sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_UNBLK_TX)) {
			SS7_DEBUG_CHAN(ftdmchan, "Processing FLAG_GRP_HW_UNBLK_TX flag %s\n", "");

			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_TX);
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_TX_DN);
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_UNBLK_TX);

			/* do not set the channel up if it's blocked by blo/cgb command OR blocked by receiving blo/cgb */
   			if (!sngss7_test_ckt_blk_flag(sngss7_info, ( FLAG_CKT_MN_BLOCK_TX
   								   | FLAG_CKT_MN_BLOCK_TX
   								   | FLAG_GRP_MN_BLOCK_TX
   								   | FLAG_GRP_MN_BLOCK_TX_DN
   								   | FLAG_CKT_MN_BLOCK_RX
   								   | FLAG_CKT_MN_BLOCK_RX_DN
   								   | FLAG_GRP_MN_BLOCK_RX
   								   | FLAG_GRP_MN_BLOCK_RX_DN
   								   )
   						      )
			) {
				sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_UP);
				ft_to_sngss7_ubl(ftdmchan);
			}

			goto suspend_goto_last;
		}
#if 0
//jz: there is no such thing of "remote hw block". for receiver, there are only block and unblock

		/**********************************************************************/
		// jz: hardware block/unblock rx
		if (sngss7_test_ckt_blk_flag (sngss7_info, FLAG_GRP_HW_BLOCK_RX ) &&
			!sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX_DN )) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing FLAG_GRP_HW_BLOCK_RX flag %s\n", "");

			sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_DOWN);
			ft_to_sngss7_bla(ftdmchan);
			sngss7_set_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX_DN);

			goto suspend_goto_last;
		}

		if (sngss7_test_ckt_blk_flag (sngss7_info, FLAG_GRP_HW_UNBLK_RX	)){
			SS7_DEBUG_CHAN(ftdmchan, "Processing FLAG_GRP_HW_UNBLK_RX flag %s\n", "");

			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX);
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX_DN);
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_UNBLK_RX);
			sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_UP);

			ft_to_sngss7_uba(ftdmchan);

			goto suspend_goto_last;
		}
#endif



		/**********************************************************************/
		if (sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX) &&
			!sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX_DN)) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing CKT_MN_BLOCK_TX flag %s\n", "");

			/* bring the sig status down */
			sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_DOWN);

			/* send a blo */
			ft_to_sngss7_blo (ftdmchan);

			/* throw the done flag */
			sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX_DN);

			/* check the last state and return to it to allow the call to finish */
			goto suspend_goto_last;
		}

		if (sngss7_test_ckt_blk_flag (sngss7_info, FLAG_CKT_MN_UNBLK_TX)) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing CKT_MN_UNBLK_TX flag %s\n", "");

			/* clear the block flags */
			sngss7_clear_ckt_blk_flag (sngss7_info, FLAG_CKT_MN_BLOCK_TX);
			sngss7_clear_ckt_blk_flag (sngss7_info, FLAG_CKT_MN_BLOCK_TX_DN);

			/* clear the unblock flag */
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_UNBLK_TX);

			/* not bring the cic up if there is a hardware block */
			if (!sngss7_test_ckt_blk_flag(sngss7_info, (FLAG_GRP_HW_BLOCK_TX | FLAG_GRP_HW_BLOCK_TX_DN))) {
				/* bring the sig status up */
				sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_UP);

				/* send a ubl */
				ft_to_sngss7_ubl(ftdmchan);
			}

			/* check the last state and return to it to allow the call to finish */
			goto suspend_goto_last;
		}

		/**********************************************************************/
		if (sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_LC_BLOCK_RX) &&
			!sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_LC_BLOCK_RX_DN)) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing CKT_LC_BLOCK_RX flag %s\n", "");

			/* send a BLA */
			/*ft_to_sngss7_bla(ftdmchan);*/

			/* throw the done flag */
			sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_LC_BLOCK_RX_DN);

			/* check the last state and return to it to allow the call to finish */
			goto suspend_goto_last;
		}

		if (sngss7_test_ckt_blk_flag (sngss7_info, FLAG_CKT_LC_UNBLK_RX)) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing CKT_LC_UNBLK_RX flag %s\n", "");

			/* clear the block flags */
			sngss7_clear_ckt_blk_flag (sngss7_info, FLAG_CKT_LC_BLOCK_RX);
			sngss7_clear_ckt_blk_flag (sngss7_info, FLAG_CKT_LC_BLOCK_RX_DN);
			
			/* clear the unblock flag */
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_LC_UNBLK_RX);

			/* send a uba */
			/*ft_to_sngss7_uba(ftdmchan);*/


			/* check the last state and return to it to allow the call to finish */
			goto suspend_goto_last;
		}
		/**********************************************************************/
		if (sngss7_test_ckt_blk_flag (sngss7_info, FLAG_CKT_UCIC_BLOCK) &&
			!sngss7_test_ckt_blk_flag (sngss7_info, FLAG_CKT_UCIC_BLOCK_DN)) {

			SS7_DEBUG_CHAN(ftdmchan, "Processing CKT_UCIC_BLOCK flag %s\n", "");

			/* bring the channel signaling status to down */
			sngss7_set_sig_status(sngss7_info, FTDM_SIG_STATE_DOWN);

			/* remove any reset flags */
			clear_rx_grs_flags(sngss7_info);
			clear_rx_grs_data(sngss7_info);
			clear_tx_grs_flags(sngss7_info);
			clear_tx_grs_data(sngss7_info);
			clear_rx_rsc_flags(sngss7_info);
			clear_tx_rsc_flags(sngss7_info);

			/* throw the done flag */
			sngss7_set_ckt_blk_flag(sngss7_info, FLAG_CKT_UCIC_BLOCK_DN);
			
			/* bring the channel down */
			goto suspend_goto_last;
		}

		if (sngss7_test_ckt_blk_flag (sngss7_info, FLAG_CKT_UCIC_UNBLK)) {
			SS7_DEBUG_CHAN(ftdmchan, "Processing CKT_UCIC_UNBLK flag %s\n", "");

			/* remove the UCIC block flag */
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_UCIC_BLOCK);
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_UCIC_BLOCK_DN);

			/* remove the UCIC unblock flag */
			sngss7_clear_ckt_blk_flag(sngss7_info, FLAG_CKT_UCIC_UNBLK);

			/* throw the channel into reset to sync states */
			sngss7_set_ckt_flag(sngss7_info, FLAG_RESET_TX);

			/* bring the channel into restart again */
			goto suspend_goto_restart;
		}

		SS7_DEBUG_CHAN(ftdmchan,"No block flag processed!%s\n", "");

suspend_goto_last:
		state_flag = 0;
		ftdm_set_state(ftdmchan, ftdmchan->last_state);
		break;

suspend_goto_restart:
		state_flag = 0;
		ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
		break;

	/**************************************************************************/
	case FTDM_CHANNEL_STATE_IN_LOOP:	/* COT test */

		isup_intf = &g_ftdm_sngss7_data.cfg.isupIntf[sngss7_info->circuit->infId];

		if (sngss7_test_options(isup_intf, SNGSS7_LPA_FOR_COT)) {
			/* send the lpa */
			ft_to_sngss7_lpa (ftdmchan);
		} 

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_IDLE:
			state_flag = 0;
			ftdm_set_state(ftdmchan, ftdmchan->last_state);
		break;
	/**************************************************************************/
	default:
		/* we don't handle any of the other states */
		SS7_ERROR_CHAN(ftdmchan, "ftmod_sangoma_ss7 does not support %s state\n",  ftdm_channel_state2str (ftdmchan->state));
		
		break;
	/**************************************************************************/
	}

	if (state_flag) {
		/* clear the state change flag...since we might be setting a new state */
		ftdm_channel_complete_state(ftdmchan);
	}
	return FTDM_SUCCESS;
}

/******************************************************************************/
static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(ftdm_sangoma_ss7_outgoing_call)
{
	sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;

	/* the core has this channel already locked so need to lock again */

	/* check if the channel sig state is UP */
	if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP)) {
		SS7_ERROR_CHAN(ftdmchan, "Requested channel sig state is down, cancelling call!%s\n", " ");
		goto outgoing_fail;
	}

	/* check if there is a remote block */
	if ((sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX)) ||
		(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX)) ||
		(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_MN_BLOCK_RX))) {

		/* the channel is blocked...can't send any calls here */
		SS7_ERROR_CHAN(ftdmchan, "Requested channel is remotely blocked, re-hunt channel!%s\n", " ");
		goto outgoing_break;
	}

	/* check if there is a local block */
	if ((sngss7_test_ckt_blk_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX)) ||
		(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_HW_BLOCK_TX)) ||
		(sngss7_test_ckt_blk_flag(sngss7_info, FLAG_GRP_MN_BLOCK_TX))) {

		/* KONRAD FIX ME : we should check if this is a TEST call and allow it */

		/* the channel is blocked...can't send any calls here */
		SS7_ERROR_CHAN(ftdmchan, "Requested channel is locally blocked, re-hunt channel!%s\n", " ");
		goto outgoing_break;
	}

	/* check the state of the channel */
	switch (ftdmchan->state){
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_DOWN:
		/* inform the monitor thread that we want to make a call by returning FTDM_SUCCESS */
		
		goto outgoing_successful;
		break;
	/**************************************************************************/
	default:
		/* the channel is already used...this can't be, end the request */
		SS7_ERROR("Outgoing call requested channel in already in use...indicating glare on span=%d,chan=%d\n",
					ftdmchan->physical_span_id,
					ftdmchan->physical_chan_id);

		goto outgoing_break;
		break;
	/**************************************************************************/
	} /* switch (ftdmchan->state) (original call) */

outgoing_fail:
	SS7_DEBUG_CHAN(ftdmchan, "Call Request failed%s\n", " ");
	return FTDM_FAIL;

outgoing_break:
	SS7_DEBUG_CHAN(ftdmchan, "Call Request re-hunt%s\n", " ");
	return FTDM_BREAK;

outgoing_successful:
	SS7_DEBUG_CHAN(ftdmchan, "Call Request successful%s\n", " ");
	return FTDM_SUCCESS;
}

/******************************************************************************/
#if 0
	  static FIO_CHANNEL_REQUEST_FUNCTION (ftdm_sangoma_ss7_request_chan)
	  {
	SS7_INFO ("KONRAD-> I got called %s\n", __FUNCTION__);
	return FTDM_SUCCESS;
	  }

#endif

/******************************************************************************/

/* FT-CORE SIG STATUS FUNCTIONS ********************************************** */
static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(ftdm_sangoma_ss7_get_sig_status)
{
	if (ftdm_test_flag (ftdmchan, FTDM_CHANNEL_SIG_UP)) {
		*status = FTDM_SIG_STATE_UP;
	} else {
		*status = FTDM_SIG_STATE_DOWN;
	}

	return FTDM_SUCCESS;
}

/******************************************************************************/
static FIO_CHANNEL_SET_SIG_STATUS_FUNCTION(ftdm_sangoma_ss7_set_sig_status)
{
	SS7_ERROR ("Cannot set channel status in this module\n");
	return FTDM_NOTIMPL;
}

/* FT-CORE SIG FUNCTIONS ******************************************************/
static ftdm_status_t ftdm_sangoma_ss7_start(ftdm_span_t * span)
{
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_chan_data_t	*sngss7_info = NULL;
	sngss7_span_data_t 	*sngss7_span = NULL;
	sng_isup_inf_t		*sngss7_intf = NULL;
	int 				x;


	SS7_INFO ("Starting span %s:%u.\n", span->name, span->span_id);

	/* clear the monitor thread stop flag */
	ftdm_clear_flag (span, FTDM_SPAN_STOP_THREAD);
	ftdm_clear_flag (span, FTDM_SPAN_IN_THREAD);

	/* check the status of all isup interfaces */
	check_status_of_all_isup_intf();

	/* throw the channels in pause */
	for (x = 1; x < (span->chan_count + 1); x++) {
		/* extract the channel structure and sngss7 channel data */
		ftdmchan = span->channels[x];

		/* if there is no sig mod data move along */
		if (ftdmchan->call_data == NULL) continue;

		sngss7_info = ftdmchan->call_data;
		sngss7_span = ftdmchan->span->signal_data;
		sngss7_intf = &g_ftdm_sngss7_data.cfg.isupIntf[sngss7_info->circuit->infId];

		/* flag the circuit as active so we can receieve events on it */
		sngss7_set_flag(sngss7_info->circuit, SNGSS7_ACTIVE);

		/* if this is a non-voice channel, move along cause we're done with it */
		if (sngss7_info->circuit->type != SNG_CKT_VOICE) continue;

		/* lock the channel */
		ftdm_mutex_lock(ftdmchan->mutex);

		/* check if the interface is paused or resumed */
		if (sngss7_test_flag(sngss7_intf, SNGSS7_PAUSED)) {
			SS7_DEBUG_CHAN(ftdmchan, "ISUP intf %d is PAUSED\n", sngss7_intf->id);
			/* throw the pause flag */
			sngss7_clear_ckt_flag(sngss7_info, FLAG_INFID_RESUME);
			sngss7_set_ckt_flag(sngss7_info, FLAG_INFID_PAUSED);
		} else {
			SS7_DEBUG_CHAN(ftdmchan, "ISUP intf %d is RESUMED\n", sngss7_intf->id);
			/* throw the resume flag */
			sngss7_clear_ckt_flag(sngss7_info, FLAG_INFID_PAUSED);
			sngss7_set_ckt_flag(sngss7_info, FLAG_INFID_RESUME);
		}
#if 0
		/* throw the grp reset flag */
		sngss7_set_ckt_flag(sngss7_info, FLAG_GRP_RESET_TX);
		if (x == 1) {
			sngss7_set_flag(sngss7_info, FLAG_GRP_RESET_BASE);
			sngss7_span->tx_grs.circuit = sngss7_info->circuit->id;
			sngss7_span->tx_grs.range = span->chan_count -1;
		}
#else
		/* throw the channel into reset */
		sngss7_set_ckt_flag(sngss7_info, FLAG_RESET_TX);
#endif
		/* throw the channel to suspend */
		ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

		/* unlock the channel */
		ftdm_mutex_unlock(ftdmchan->mutex);
	}

	/* activate all the configured ss7 links */
	if (ft_to_sngss7_activate_all()) {
		SS7_CRITICAL ("Failed to activate LibSngSS7!\n");
		return FTDM_FAIL;
	}

	/*start the span monitor thread */
	if (ftdm_thread_create_detached (ftdm_sangoma_ss7_run, span) != FTDM_SUCCESS) {
		SS7_CRITICAL ("Failed to start Span Monitor Thread!\n");
		return FTDM_FAIL;
	}

	SS7_DEBUG ("Finished starting span %s:%u.\n", span->name, span->span_id);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t ftdm_sangoma_ss7_stop(ftdm_span_t * span)
{
	/*this function is called by the FT-Core to stop this span */

	ftdm_log (FTDM_LOG_INFO, "Stopping span %s:%u.\n", span->name,span->span_id);

	/* throw the STOP_THREAD flag to signal monitor thread stop */
	ftdm_set_flag (span, FTDM_SPAN_STOP_THREAD);

	/* wait for the thread to stop */
	while (ftdm_test_flag (span, FTDM_SPAN_IN_THREAD)) {
		ftdm_log (FTDM_LOG_DEBUG,"Waiting for monitor thread to end for %s:%u.\n",
									span->name,
									span->span_id);
		ftdm_sleep (1);
	}

	/* KONRAD FIX ME - deconfigure any circuits, links, attached to this span */

	ftdm_log (FTDM_LOG_DEBUG, "Finished stopping span %s:%u.\n", span->name, span->span_id);

	return FTDM_SUCCESS;
}

/* SIG_FUNCTIONS ***************************************************************/
static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_sangoma_ss7_span_config)
{
	sngss7_span_data_t	*ss7_span_info;

	ftdm_log (FTDM_LOG_INFO, "Configuring ftmod_sangoma_ss7 span = %s(%d)...\n",
								span->name,
								span->span_id);

	/* initalize the span's data structure */
	ss7_span_info = ftdm_calloc (1, sizeof (sngss7_span_data_t));

	/* create a timer schedule */
	if (ftdm_sched_create(&ss7_span_info->sched, "SngSS7_Schedule")) {
		SS7_CRITICAL("Unable to create timer schedule!\n");
		return FTDM_FAIL;
	}

	/* start the free run thread for the schedule */
	if (ftdm_sched_free_run(ss7_span_info->sched)) {
		SS7_CRITICAL("Unable to schedule free run!\n");
		return FTDM_FAIL;
	}

	/* create an event queue for this span */
	if ((ftdm_queue_create(&(ss7_span_info)->event_queue, SNGSS7_EVENT_QUEUE_SIZE)) != FTDM_SUCCESS) {
		SS7_CRITICAL("Unable to create event queue!\n");
		return FTDM_FAIL;
	}

	/*setup the span structure with the info so far */
	g_ftdm_sngss7_data.sig_cb 		= sig_cb;
	span->start 					= ftdm_sangoma_ss7_start;
	span->stop 						= ftdm_sangoma_ss7_stop;
	span->signal_type 				= FTDM_SIGTYPE_SS7;
	span->signal_data 				= NULL;
	span->outgoing_call 			= ftdm_sangoma_ss7_outgoing_call;
	span->channel_request 			= NULL;
	span->signal_cb 				= sig_cb;
	span->get_channel_sig_status	= ftdm_sangoma_ss7_get_sig_status;
	span->set_channel_sig_status 	= ftdm_sangoma_ss7_set_sig_status;
	span->state_map			 		= &sangoma_ss7_state_map;
	span->state_processor = ftdm_sangoma_ss7_process_state_change;
	span->signal_data					= ss7_span_info;

	/* set the flag to indicate that this span uses channel state change queues */
	ftdm_set_flag (span, FTDM_SPAN_USE_CHAN_QUEUE);
	/* set the flag to indicate that this span uses sig event queues */
	ftdm_set_flag (span, FTDM_SPAN_USE_SIGNALS_QUEUE);

	/* parse the configuration and apply to the global config structure */
	if (ftmod_ss7_parse_xml(ftdm_parameters, span)) {
		ftdm_log (FTDM_LOG_CRIT, "Failed to parse configuration!\n");
		return FTDM_FAIL;
	}

	/* configure libsngss7 */
	if (ft_to_sngss7_cfg_all()) {
		ftdm_log (FTDM_LOG_CRIT, "Failed to configure LibSngSS7!\n");
		return FTDM_FAIL;
	}

	ftdm_log (FTDM_LOG_INFO, "Finished configuring ftmod_sangoma_ss7 span = %s(%d)...\n",
								span->name,
								span->span_id);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static FIO_SIG_LOAD_FUNCTION(ftdm_sangoma_ss7_init)
{
	/*this function is called by the FT-core to load the signaling module */
	uint32_t major = 0;
	uint32_t minor = 0;
	uint32_t build = 0;

	ftdm_log (FTDM_LOG_INFO, "Loading ftmod_sangoma_ss7...\n");

	/* default the global structure */
	memset (&g_ftdm_sngss7_data, 0x0, sizeof (ftdm_sngss7_data_t));

	sngss7_id = 0;

	cmbLinkSetId = 0;

	/* initalize the global gen_config flag */
	g_ftdm_sngss7_data.gen_config = 0;

	/* function trace initizalation */
	g_ftdm_sngss7_data.function_trace = 1;
	g_ftdm_sngss7_data.function_trace_level = 7;

	/* message (IAM, ACM, ANM, etc) trace initizalation */
	g_ftdm_sngss7_data.message_trace = 1;
	g_ftdm_sngss7_data.message_trace_level = 6;

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
	sng_event.cc.sng_susp_ind = sngss7_susp_ind;
	sng_event.cc.sng_resm_ind = sngss7_resm_ind;

	sng_event.sm.sng_log = handle_sng_log;
	sng_event.sm.sng_mtp1_alarm = handle_sng_mtp1_alarm;
	sng_event.sm.sng_mtp2_alarm = handle_sng_mtp2_alarm;
	sng_event.sm.sng_mtp3_alarm = handle_sng_mtp3_alarm;
	sng_event.sm.sng_isup_alarm = handle_sng_isup_alarm;
	sng_event.sm.sng_cc_alarm = handle_sng_cc_alarm;
	sng_event.sm.sng_relay_alarm = handle_sng_relay_alarm;

	/* initalize sng_ss7 library */
	sng_isup_init_gen(&sng_event);

	/* print the version of the library being used */
	sng_isup_version(&major, &minor, &build);
	SS7_INFO("Loaded LibSng-SS7 %d.%d.%d\n", major, minor, build);

	/* crash on assert fail */
	ftdm_global_set_crash_policy (FTDM_CRASH_ON_ASSERT);

	return FTDM_SUCCESS;
}

/******************************************************************************/
static FIO_SIG_UNLOAD_FUNCTION(ftdm_sangoma_ss7_unload)
{
	/*this function is called by the FT-core to unload the signaling module */

	int x;

	ftdm_log (FTDM_LOG_INFO, "Starting ftmod_sangoma_ss7 unload...\n");


	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_CC_STARTED)) {
		sng_isup_free_cc();
		sngss7_clear_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_CC_STARTED);
	}

	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_ISUP_STARTED)) {
		ftmod_ss7_shutdown_isup();
		sng_isup_free_isup();
		sngss7_clear_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_ISUP_STARTED);
	}

	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP3_STARTED)) {
		ftmod_ss7_shutdown_mtp3();
		sng_isup_free_mtp3();
		sngss7_clear_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP3_STARTED);
	}

	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP2_STARTED)) {
		ftmod_ss7_shutdown_mtp2();
		sng_isup_free_mtp2();
		sngss7_clear_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP2_STARTED);
	}

	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP1_STARTED)) {
		sng_isup_free_mtp1();
		sngss7_clear_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_MTP1_STARTED);
	}

	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_RY_STARTED)) {
		/* go through all the relays channels and disable them */
		x = 1;
		while (x < (MAX_RELAY_CHANNELS)) {
			/* check if this relay channel has been configured already */
			if ((g_ftdm_sngss7_data.cfg.relay[x].flags & SNGSS7_CONFIGURED)) {
	
				/* send the specific configuration */
				if (ftmod_ss7_disable_relay_channel(x)) {
					SS7_CRITICAL("Relay Channel %d disable failed!\n", x);
					/* jz: dont leave like this 
					 * return 1; 
					 * */
				} else {
					SS7_INFO("Relay Channel %d disable DONE!\n", x);
				}
	
				/* set the SNGSS7_CONFIGURED flag */
				g_ftdm_sngss7_data.cfg.relay[x].flags &= ~(SNGSS7_CONFIGURED);
			} /* if !SNGSS7_CONFIGURED */
			x++;
		} /* while (x < (MAX_RELAY_CHANNELS)) */
		
		ftmod_ss7_shutdown_relay();
		sng_isup_free_relay();
		sngss7_clear_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_RY_STARTED);
	}


	if (sngss7_test_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_SM_STARTED)) {
		sng_isup_free_sm();
		sngss7_clear_flag(&g_ftdm_sngss7_data.cfg, SNGSS7_SM_STARTED);
	}

	sng_isup_free_gen();

	ftdm_log (FTDM_LOG_INFO, "Finished ftmod_sangoma_ss7 unload!\n");
	return FTDM_SUCCESS;
}

/******************************************************************************/
static FIO_API_FUNCTION(ftdm_sangoma_ss7_api)
{
	/* handle this in it's own file....so much to do */
	return (ftdm_sngss7_handle_cli_cmd (stream, data));
}

/******************************************************************************/
static FIO_IO_LOAD_FUNCTION(ftdm_sangoma_ss7_io_init)
{
	assert (fio != NULL);
	memset (&g_ftdm_sngss7_interface, 0, sizeof (g_ftdm_sngss7_interface));

	g_ftdm_sngss7_interface.name = "ss7";
	g_ftdm_sngss7_interface.api = ftdm_sangoma_ss7_api;

	*fio = &g_ftdm_sngss7_interface;

	return FTDM_SUCCESS;
}

/******************************************************************************/


/* START **********************************************************************/
ftdm_module_t ftdm_module = {
	"sangoma_ss7",					/*char name[256];				   */
	ftdm_sangoma_ss7_io_init,		/*fio_io_load_t					 */
	NULL,							/*fio_io_unload_t				   */
	ftdm_sangoma_ss7_init,			/*fio_sig_load_t					*/
	NULL,							/*fio_sig_configure_t			   */
	ftdm_sangoma_ss7_unload,		/*fio_sig_unload_t				  */
	ftdm_sangoma_ss7_span_config	/*fio_configure_span_signaling_t	*/
};
/******************************************************************************/

/******************************************************************************/
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
