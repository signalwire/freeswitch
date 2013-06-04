
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

#ifdef FTDM_DEBUG_CHAN_MEMORY
#include <sys/mman.h>
#endif

static void *ftdm_sangoma_isdn_run(ftdm_thread_t *me, void *obj);
static ftdm_status_t ftdm_sangoma_isdn_stop(ftdm_span_t *span);
static ftdm_status_t ftdm_sangoma_isdn_start(ftdm_span_t *span);
static ftdm_status_t ftdm_sangoma_isdn_dtmf(ftdm_channel_t *ftdmchan, const char* dtmf);

ftdm_channel_t* ftdm_sangoma_isdn_process_event_states(ftdm_span_t *span, sngisdn_event_data_t *sngisdn_event);
static void ftdm_sangoma_isdn_process_phy_events(ftdm_span_t *span, ftdm_oob_event_t event);
static ftdm_status_t ftdm_sangoma_isdn_process_state_change(ftdm_channel_t *ftdmchan);
static void ftdm_sangoma_isdn_process_stack_event (ftdm_span_t *span, sngisdn_event_data_t *sngisdn_event);
static void ftdm_sangoma_isdn_wakeup_phy(ftdm_channel_t *dchan);
static void ftdm_sangoma_isdn_dchan_set_queue_size(ftdm_channel_t *ftdmchan);

static ftdm_io_interface_t			g_sngisdn_io_interface;
static sng_isdn_event_interface_t	g_sngisdn_event_interface;

ftdm_sngisdn_data_t					g_sngisdn_data;

SNGISDN_ENUM_NAMES(SNGISDN_TRANSFER_TYPE_NAMES, SNGISDN_TRANSFER_TYPE_STRINGS)
SNGISDN_STR2ENUM(ftdm_str2sngisdn_transfer_type, sngisdn_transfer_type2str, sngisdn_transfer_type_t, SNGISDN_TRANSFER_TYPE_NAMES, SNGISDN_TRANSFER_INVALID)

ftdm_state_map_t sangoma_isdn_state_map = {
	{
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_ANY_STATE, FTDM_END},
		{FTDM_CHANNEL_STATE_RESET, FTDM_CHANNEL_STATE_RESTART, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_RESET, FTDM_END},
		{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
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
		{FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END}
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
		{FTDM_CHANNEL_STATE_COLLECT, FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_GET_CALLERID, FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_GET_CALLERID, FTDM_END},
		{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_CANCEL, FTDM_CHANNEL_STATE_IN_LOOP, FTDM_END}
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
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROCEED, FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_PROCEED, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_TRANSFER, FTDM_END}
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_RINGING, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_END},
	},
	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_PROGRESS, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_END},
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
		{FTDM_CHANNEL_STATE_TRANSFER, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END},
	},

	{
		ZSD_INBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_TRANSFER, FTDM_END},
		{FTDM_CHANNEL_STATE_PROCEED, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_TERMINATING,FTDM_END},
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
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_ANY_STATE, FTDM_END},
		{FTDM_CHANNEL_STATE_RESET, FTDM_CHANNEL_STATE_RESTART, FTDM_CHANNEL_STATE_TERMINATING, FTDM_END}
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_RESET, FTDM_END},
		{FTDM_CHANNEL_STATE_DOWN, FTDM_END}
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
		{FTDM_CHANNEL_STATE_DIALING, FTDM_CHANNEL_STATE_HANGUP_COMPLETE, FTDM_END}
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_DIALING, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
		 FTDM_CHANNEL_STATE_PROCEED, FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP,
		 FTDM_CHANNEL_STATE_DOWN, FTDM_END}
	},
	{
		ZSD_OUTBOUND,
		ZSM_UNACCEPTABLE,
		{FTDM_CHANNEL_STATE_PROCEED, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP,
		 FTDM_CHANNEL_STATE_RINGING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_END},
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
		{FTDM_CHANNEL_STATE_RINGING, FTDM_END},
		{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_END},
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

static void ftdm_sangoma_isdn_process_phy_events(ftdm_span_t *span, ftdm_oob_event_t event)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) span->signal_data;
	sngisdn_snd_event(signal_data, event);

	switch (event) {
		/* Check if the span woke up from power-saving mode */
		case FTDM_OOB_ALARM_CLEAR:
			if (FTDM_SPAN_IS_BRI(span)) {
				ftdm_channel_t *ftdmchan;
				sngisdn_chan_data_t *sngisdn_info;
				ftdm_iterator_t *chaniter = NULL;
				ftdm_iterator_t *curr = NULL;
								
				chaniter = ftdm_span_get_chan_iterator(span, NULL);
				for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
					ftdmchan = (ftdm_channel_t*)ftdm_iterator_current(curr);
					sngisdn_info = (sngisdn_chan_data_t*)ftdmchan->call_data;

					if (ftdm_test_flag(sngisdn_info, FLAG_ACTIVATING)) {
						ftdm_clear_flag(sngisdn_info, FLAG_ACTIVATING);

						ftdm_sched_timer(signal_data->sched, "delayed_setup", 1000, sngisdn_delayed_setup, (void*) ftdmchan->call_data, NULL);
					}
				}
				ftdm_iterator_free(chaniter);
			}
			break;
		default:
			/* Ignore other events for now */
			break;
	}
}

static void ftdm_sangoma_isdn_dchan_set_queue_size(ftdm_channel_t *dchan)
{
	ftdm_status_t 	ret_status;
	uint32_t queue_size;
	
	queue_size = SNGISDN_DCHAN_QUEUE_LEN;
	ret_status = ftdm_channel_command(dchan, FTDM_COMMAND_SET_RX_QUEUE_SIZE, &queue_size);
	ftdm_assert(ret_status == FTDM_SUCCESS, "Failed to set Rx Queue size");

	queue_size = SNGISDN_DCHAN_QUEUE_LEN;
	ret_status = ftdm_channel_command(dchan, FTDM_COMMAND_SET_TX_QUEUE_SIZE, &queue_size);
	ftdm_assert(ret_status == FTDM_SUCCESS, "Failed to set Tx Queue size");

	RETVOID;
}

static void ftdm_sangoma_isdn_wakeup_phy(ftdm_channel_t *dchan)
{
	ftdm_status_t 	ret_status;
	ftdm_channel_hw_link_status_t status = FTDM_HW_LINK_CONNECTED;
	ret_status = ftdm_channel_command(dchan, FTDM_COMMAND_SET_LINK_STATUS, &status);
	if (ret_status != FTDM_SUCCESS) {
		ftdm_log_chan_msg(dchan, FTDM_LOG_WARNING, "Failed to wake-up link\n");
	}
	return;
}

static void *ftdm_sangoma_isdn_io_run(ftdm_thread_t *me, void *obj)
{
	uint8_t data[8192];
	unsigned i = 0;
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_span_t *span = (ftdm_span_t*) obj;
	ftdm_size_t len = 0;
	ftdm_channel_t *ftdmchan = NULL;
	unsigned waitms = 10000;
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *citer = NULL;
	ftdm_event_t *event;
	short *poll_events = ftdm_malloc(sizeof(short) * span->chan_count);

	/* Initialize the d-channel */
	chaniter = ftdm_span_get_chan_iterator(span, NULL);
	if (!chaniter) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to allocate channel iterator for span %s!\n", span->name);
		goto done;
	}

	while (ftdm_running() && !(ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD))) {
		len = 1000;
		waitms = 1000;
		memset(poll_events, 0, sizeof(short)*span->chan_count);

		for (i = 0, citer = ftdm_span_get_chan_iterator(span, chaniter); citer; citer = ftdm_iterator_next(citer), i++) {
			ftdmchan = ftdm_iterator_current(citer);

			poll_events[i] |= FTDM_EVENTS;
			if (FTDM_IS_VOICE_CHANNEL(ftdmchan)) {
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_RX_DISABLED)) {
					poll_events[i] |= FTDM_READ;
					waitms = 20;
				}
			} else {
				/* We always read the d-channel */
				poll_events[i] |= FTDM_READ;
			}
		}

		status = ftdm_span_poll_event(span, waitms, poll_events);
		switch (status) {
			case FTDM_FAIL:
				ftdm_log(FTDM_LOG_CRIT, "Failed to poll span for IO\n");
				break;
			case FTDM_TIMEOUT:
				break;
			case FTDM_SUCCESS:
				/* Check if there are any channels that have data available */
				for (citer = ftdm_span_get_chan_iterator(span, chaniter); citer; citer = ftdm_iterator_next(citer)) {
					len = sizeof(data);
					ftdmchan = ftdm_iterator_current(citer);
					if (FTDM_IS_VOICE_CHANNEL(ftdmchan)) {
						if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_RX_DISABLED)) {
							if (ftdm_test_io_flag(ftdmchan, FTDM_CHANNEL_IO_READ)) {
								status = ftdm_raw_read(ftdmchan, data, &len);
								if (status != FTDM_SUCCESS) {
									ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "raw I/O read failed\n");
									continue;
								}

								status = ftdm_channel_process_media(ftdmchan, data, &len);
								if (status != FTDM_SUCCESS) {
									ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Failed to process media\n");
									continue;
								}
							}
						}
					} else {
						if (ftdm_test_io_flag(ftdmchan, FTDM_CHANNEL_IO_READ)) {
							status = ftdm_channel_read(ftdmchan, data, &len);
							if (status == FTDM_SUCCESS) {
								sngisdn_snd_data(ftdmchan, data, len);
							}
						}
					}
				}

				/* Check if there are any channels that have events available */
				while (ftdm_span_next_event(span, &event) == FTDM_SUCCESS) {
					ftdm_sangoma_isdn_process_phy_events(span, event->enum_id);
				}
				
				break;
			default:
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Unhandled IO event\n");
		}
	}
done:
	ftdm_iterator_free(chaniter);
	ftdm_safe_free(poll_events);
	return NULL;
}

static void *ftdm_sangoma_isdn_run(ftdm_thread_t *me, void *obj)
{
	ftdm_interrupt_t	*ftdm_sangoma_isdn_int[3];
	ftdm_status_t		ret_status;
	ftdm_span_t		*span	= (ftdm_span_t *) obj;
	ftdm_channel_t	*ftdmchan = NULL;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;
	sngisdn_event_data_t *sngisdn_event = NULL;
	int32_t sleep = SNGISDN_EVENT_POLL_RATE;

	ftdm_log(FTDM_LOG_INFO, "ftmod_sangoma_isdn monitor thread for span=%u started.\n", span->span_id);

	/* set IN_THREAD flag so that we know this thread is running */
	ftdm_set_flag(span, FTDM_SPAN_IN_THREAD);

	/* get an interrupt queue for this span */
	if (ftdm_queue_get_interrupt(span->pendingchans, &ftdm_sangoma_isdn_int[0]) != FTDM_SUCCESS) {
 		ftdm_log(FTDM_LOG_CRIT, "Failed to get a ftdm_interrupt for span = %s!\n", span->name);
		goto ftdm_sangoma_isdn_run_exit;
	}
	
	if (ftdm_queue_get_interrupt(span->pendingsignals, &ftdm_sangoma_isdn_int[1]) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to get a signal interrupt for span = %s!\n", span->name);
		goto ftdm_sangoma_isdn_run_exit;
	}

	if (ftdm_queue_get_interrupt(signal_data->event_queue, &ftdm_sangoma_isdn_int[2]) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to get a event interrupt for span = %s!\n", span->name);
		goto ftdm_sangoma_isdn_run_exit;
	}

	while (ftdm_running() && !(ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD))) {
		/* Check if there are any timers to process */
		ftdm_sched_run(signal_data->sched);
		ftdm_span_trigger_signals(span);
		
		if (ftdm_sched_get_time_to_next_timer(signal_data->sched, &sleep) == FTDM_SUCCESS) {
			if (sleep < 0 || sleep > SNGISDN_EVENT_POLL_RATE) {
				sleep = SNGISDN_EVENT_POLL_RATE;
			}
		}
		ret_status = ftdm_interrupt_multiple_wait(ftdm_sangoma_isdn_int, 3, sleep);
		/* find out why we returned from the interrupt queue */
		switch (ret_status) {
			case FTDM_SUCCESS:  /* there was a state change on the span */
				/* process all pending state changes */			
				while ((ftdmchan = ftdm_queue_dequeue(span->pendingchans))) {
					/* double check that this channel has a state change pending */
					ftdm_channel_lock(ftdmchan);
					ftdm_channel_advance_states(ftdmchan);
					ftdm_channel_unlock(ftdmchan);
				}

				while ((sngisdn_event = ftdm_queue_dequeue(signal_data->event_queue))) {
					ftdm_sangoma_isdn_process_stack_event(span, sngisdn_event);
					ftdm_safe_free(sngisdn_event);
				}
				break;
			case FTDM_TIMEOUT:
				/* twiddle */
				break;
			case FTDM_FAIL:
				ftdm_log(FTDM_LOG_ERROR, "%s: ftdm_interrupt_wait returned error!\n", span->name);
				break;

			default:
				ftdm_log(FTDM_LOG_ERROR, "%s: ftdm_interrupt_wait returned with unknown code\n", span->name);
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


/**
 * \brief Checks if span has state changes pending and processes
 * \param span Span where event was fired
 * \param sngisdn_event Event to handle
 * \return The locked FTDM channel associated to the event if any, NULL otherwise
 */

ftdm_channel_t* ftdm_sangoma_isdn_process_event_states(ftdm_span_t *span, sngisdn_event_data_t *sngisdn_event)
{
	ftdm_channel_t *ftdmchan = NULL;
	switch (sngisdn_event->event_id) {
		/* Events that do not have a channel associated to them */
		case SNGISDN_EVENT_SRV_IND:
		case SNGISDN_EVENT_SRV_CFM:
		case SNGISDN_EVENT_RST_CFM:
		case SNGISDN_EVENT_RST_IND:
			return NULL;
			break;
		case SNGISDN_EVENT_CON_IND:
		case SNGISDN_EVENT_CON_CFM:
		case SNGISDN_EVENT_CNST_IND:
		case SNGISDN_EVENT_DISC_IND:
		case SNGISDN_EVENT_REL_IND:
		case SNGISDN_EVENT_DAT_IND:
		case SNGISDN_EVENT_SSHL_IND:
		case SNGISDN_EVENT_SSHL_CFM:
		case SNGISDN_EVENT_RMRT_IND:
		case SNGISDN_EVENT_RMRT_CFM:
		case SNGISDN_EVENT_FLC_IND:
		case SNGISDN_EVENT_FAC_IND:
		case SNGISDN_EVENT_STA_CFM:
			ftdmchan = sngisdn_event->sngisdn_info->ftdmchan;
			ftdm_assert_return(ftdmchan, NULL,"Event should have a channel associated\n");
			break;
	}
 	ftdm_channel_lock(ftdmchan);
	ftdm_channel_advance_states(ftdmchan);
	return ftdmchan;
}



static void ftdm_sangoma_isdn_process_stack_event (ftdm_span_t *span, sngisdn_event_data_t *sngisdn_event)
{
	ftdm_channel_t *ftdmchan = NULL;
	
	ftdmchan = ftdm_sangoma_isdn_process_event_states(span, sngisdn_event);
	switch(sngisdn_event->event_id) {
		case SNGISDN_EVENT_CON_IND:
			sngisdn_process_con_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_CON_CFM:
			sngisdn_process_con_cfm(sngisdn_event);
			break;
		case SNGISDN_EVENT_CNST_IND:
			sngisdn_process_cnst_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_DISC_IND:
			sngisdn_process_disc_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_REL_IND:
			sngisdn_process_rel_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_DAT_IND:
			sngisdn_process_dat_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_SSHL_IND:
			sngisdn_process_sshl_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_SSHL_CFM:
			sngisdn_process_sshl_cfm(sngisdn_event);
			break;
		case SNGISDN_EVENT_RMRT_IND:
			sngisdn_process_rmrt_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_RMRT_CFM:
			sngisdn_process_rmrt_cfm(sngisdn_event);
			break;
		case SNGISDN_EVENT_FLC_IND:
			sngisdn_process_flc_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_FAC_IND:
			sngisdn_process_fac_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_STA_CFM:
			sngisdn_process_sta_cfm(sngisdn_event);
			break;
		case SNGISDN_EVENT_SRV_IND:
			sngisdn_process_srv_ind(sngisdn_event);
			break;
		case SNGISDN_EVENT_SRV_CFM:
			sngisdn_process_srv_cfm(sngisdn_event);
			break;
		case SNGISDN_EVENT_RST_CFM:
			sngisdn_process_rst_cfm(sngisdn_event);
			break;
		case SNGISDN_EVENT_RST_IND:
			sngisdn_process_rst_ind(sngisdn_event);
			break;
	}
	if (ftdmchan != NULL) {
		ftdm_channel_advance_states(ftdmchan);
		ftdm_channel_unlock(ftdmchan);
	}
}

/* this function is called with the channel already locked by the core */
static ftdm_status_t ftdm_sangoma_isdn_process_state_change(ftdm_channel_t *ftdmchan)
{
	ftdm_sigmsg_t			sigev;
	ftdm_channel_state_t	initial_state;
	sngisdn_chan_data_t		*sngisdn_info = ftdmchan->call_data;
	uint8_t					state_change = 0;

	memset(&sigev, 0, sizeof(sigev));

	sigev.chan_id = ftdmchan->chan_id;
	sigev.span_id = ftdmchan->span_id;
	sigev.channel = ftdmchan;

#ifdef FTDM_DEBUG_CHAN_MEMORY
	if (ftdmchan->state == FTDM_CHANNEL_STATE_DIALING) {
		ftdm_assert(mprotect(ftdmchan, sizeof(*ftdmchan), PROT_READ) == 0, "Failed to mprotect");
	}
#endif
	
	/* Only needed for debugging */
	initial_state = ftdmchan->state;

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "processing state change to %s\n", ftdm_channel_state2str(ftdmchan->state));

	switch (ftdmchan->state) {
	case FTDM_CHANNEL_STATE_COLLECT:	 /* SETUP received but waiting on digits */
		{
			/* TODO: Re-implement this. There is a way to re-evaluate new incoming digits from dialplan as they come */
			sngisdn_snd_setup_ack(ftdmchan);
			/* Just wait in this state until we get enough digits or T302 timeout */
		}
		break;
	case FTDM_CHANNEL_STATE_GET_CALLERID:
		{
			/* By default, we do not send a progress indicator in the proceed */
			ftdm_sngisdn_progind_t prog_ind = {SNGISDN_PROGIND_LOC_USER, SNGISDN_PROGIND_DESCR_INVALID};
			sngisdn_snd_proceed(ftdmchan, prog_ind);

			/* Wait in this state until we get FACILITY msg */
		}
		break;
	case FTDM_CHANNEL_STATE_RING: /* incoming call request */
		{
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending incoming call from %s to %s to FTDM core\n", ftdmchan->caller_data.ani.digits, ftdmchan->caller_data.dnis.digits);

			/* we have enough information to inform FTDM of the call*/
			sngisdn_send_signal(sngisdn_info, FTDM_SIGEVENT_START);
		}
		break;
	case FTDM_CHANNEL_STATE_DIALING: /* outgoing call request */
		{			
			if (FTDM_SPAN_IS_BRI(ftdmchan->span) && ftdm_test_flag(ftdmchan->span, FTDM_SPAN_PWR_SAVING)) {
				ftdm_signaling_status_t sigstatus;
				ftdm_span_get_sig_status(ftdmchan->span, &sigstatus);
				if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_IN_ALARM)) {
					sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)ftdmchan->span->signal_data;
							
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Requesting Physical Line activation\n");
					sngisdn_set_flag(sngisdn_info, FLAG_ACTIVATING);
					ftdm_sangoma_isdn_wakeup_phy(ftdmchan);
					ftdm_sched_timer(signal_data->sched, "timer_t3", signal_data->timer_t3*1000, sngisdn_t3_timeout, (void*) sngisdn_info, NULL);
				} else if (sigstatus == FTDM_SIG_STATE_DOWN) {
					sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)ftdmchan->span->signal_data;
					
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Requesting Q.921 Line activation\n");
					sngisdn_set_flag(sngisdn_info, FLAG_ACTIVATING);
					sngisdn_snd_dl_req(ftdmchan);
					ftdm_sched_timer(signal_data->sched, "timer_t3", signal_data->timer_t3*1000, sngisdn_t3_timeout, (void*) sngisdn_info, NULL);
				} else {
					sngisdn_snd_setup(ftdmchan);
				}
			} else {
				sngisdn_snd_setup(ftdmchan);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_PROCEED:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				/*OUTBOUND...so we were told by the line of this so noifiy the user*/
				sngisdn_send_signal(sngisdn_info, FTDM_SIGEVENT_PROCEED);
				
				if (sngisdn_test_flag(sngisdn_info, FLAG_MEDIA_READY)) {
					state_change++;
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
				}
			} else {
				ftdm_sngisdn_progind_t prog_ind = {SNGISDN_PROGIND_LOC_USER, SNGISDN_PROGIND_DESCR_INVALID};
				sngisdn_snd_proceed(ftdmchan, prog_ind);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_RINGING:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				/* OUTBOUND...so we were told by the line of this so notify the user */
				sngisdn_send_signal(sngisdn_info, FTDM_SIGEVENT_RINGING);
				
				if (sngisdn_test_flag(sngisdn_info, FLAG_MEDIA_READY)) {
					state_change++;
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
				}
			} else {
				ftdm_sngisdn_progind_t prog_ind = {SNGISDN_PROGIND_LOC_USER, SNGISDN_PROGIND_DESCR_NETE_ISDN};
				sngisdn_snd_alert(ftdmchan, prog_ind);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_PROGRESS:
		{
			/*check if the channel is inbound or outbound*/
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				/*OUTBOUND...so we were told by the line of this so noifiy the user*/
				
				sngisdn_send_signal(sngisdn_info, FTDM_SIGEVENT_PROGRESS);
			} else {
				/* Send a progress message, indicating: Call is not end-to-end ISDN, further call progress may be available */
				ftdm_sngisdn_progind_t prog_ind = {SNGISDN_PROGIND_LOC_USER, SNGISDN_PROGIND_DESCR_NETE_ISDN};
				sngisdn_snd_progress(ftdmchan, prog_ind);				
			}
		}
		break;
	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		{
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				sngisdn_send_signal(sngisdn_info, FTDM_SIGEVENT_PROGRESS_MEDIA);
			} else {
				/* Send a progress message, indicating: In-band information/pattern available */
				ftdm_sngisdn_progind_t prog_ind = {SNGISDN_PROGIND_LOC_USER, SNGISDN_PROGIND_DESCR_IB_AVAIL};
				sngisdn_snd_progress(ftdmchan, prog_ind);
			}
		}
		break; 
	case FTDM_CHANNEL_STATE_UP: /* call is answered */
		{
			/* check if the channel is inbound or outbound */
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
				/* OUTBOUND ... so we were told by the line that the other side answered */
				
				sngisdn_send_signal(sngisdn_info, FTDM_SIGEVENT_UP);

				if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP &&
					((sngisdn_span_data_t*)ftdmchan->span->signal_data)->signalling == SNGISDN_SIGNALING_NET) {
					/* Assign the call to a specific equipment */
					sngisdn_snd_con_complete(ftdmchan);
				}
			} else {
				/* INBOUND ... so FS told us it just answered ... tell the stack */
				sngisdn_snd_connect(ftdmchan);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_CANCEL:
		{
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Hanging up call before informing user!\n");

			/* Send a release complete */
			sngisdn_snd_release(ftdmchan, 0);
			/*now go to the HANGUP complete state*/				
			state_change++;
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
		}
		break;
	case FTDM_CHANNEL_STATE_TERMINATING: /* call is hung up by the remote end */
		{
			/* this state is set when the line is hanging up */
			sngisdn_send_signal(sngisdn_info, FTDM_SIGEVENT_STOP);
		}
		break;
	case FTDM_CHANNEL_STATE_HANGUP:	/* call is hung up locally */
		{
			if (sngisdn_test_flag(sngisdn_info, FLAG_REMOTE_ABORT)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Acknowledging remote abort\n");
			} else if (sngisdn_test_flag(sngisdn_info, FLAG_REMOTE_REL)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Acknowledging remote hangup\n");
				sngisdn_snd_release(ftdmchan, 0);
			} else if (sngisdn_test_flag(sngisdn_info, FLAG_LOCAL_ABORT)) {
				/* We aborted this call before sending anything to the stack, so nothing to do anymore */
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Clearing local states from local abort\n");
			} else if (sngisdn_test_flag(sngisdn_info, FLAG_GLARE)) {
				/* We are hangup local call because there was a glare, we are waiting for a
				RELEASE on this call, before we can process the saved call */
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Waiting for RELEASE on hungup glared call\n");
			} else if (sngisdn_test_flag(sngisdn_info, FLAG_SEND_DISC)) {
				/* Remote side sent a PROGRESS message, but cause indicates disconnect or T310 expired*/
				sngisdn_snd_disconnect(ftdmchan);
			} else {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Hanging up call upon local request!\n");

				 /* set the flag to indicate this hangup is started from the local side */
				sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_REL);

				switch(ftdmchan->last_state) {
					case FTDM_CHANNEL_STATE_RING:
						/* If we never sent PROCEED/ALERT/PROGRESS/CONNECT on an incoming call, we need to send release instead of disconnect */
						sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
						sngisdn_snd_release(ftdmchan, 0);
						break;
					case FTDM_CHANNEL_STATE_DIALING:
						/* If we never received a PROCEED/ALERT/PROGRESS/CONNECT on an outgoing call, we need to send release instead of disconnect */
						sngisdn_snd_release(ftdmchan, 0);
						break;
					case FTDM_CHANNEL_STATE_PROCEED:
						if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
							if (((sngisdn_span_data_t*)(ftdmchan->span->signal_data))->switchtype == SNGISDN_SWITCH_4ESS ||
							    ((sngisdn_span_data_t*)(ftdmchan->span->signal_data))->switchtype == SNGISDN_SWITCH_5ESS) {
							
								/* When using 5ESS, if the user wants to clear an inbound call, the correct procedure is to send a PROGRESS with in-band info available, and play tones. Then send a DISCONNECT. If we reached this point, it means user did not try to play-tones, so send a RELEASE because remote side does not expect DISCONNECT in state 3 */
								sngisdn_snd_release(ftdmchan, 0);
								break;
							}
						}
						/* fall-through */
					default:
						sngisdn_snd_disconnect(ftdmchan);
						break;
				}
			}
			/* now go to the HANGUP complete state */
			state_change++;
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
		}
		break;
	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
		{
			if (sngisdn_test_flag(sngisdn_info, FLAG_REMOTE_ABORT) ||
				sngisdn_test_flag(sngisdn_info, FLAG_LOCAL_ABORT)) {
				/* If the remote side aborted, we will not get anymore message for this call */
				state_change++;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			} else {
				/* waiting on remote confirmation before moving to down */
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Waiting for release from stack\n");
			}
		}
		break;
	case FTDM_CHANNEL_STATE_DOWN: /* the call is finished and removed */
		{
			uint8_t glare = sngisdn_test_flag(sngisdn_info, FLAG_GLARE);
			/* clear all of the call specific data store in the channel structure */
			clear_call_data(sngisdn_info);

			/* Close the channel even if we had a glare, we will re-open it when processing state COLLECT for the
				"glared call" */
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
				ftdm_channel_t *close_chan = ftdmchan;
				/* close the channel */
				ftdm_channel_close(&close_chan);
			}
			if (glare) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Glare detected, processing saved call\n");
				/* We are calling sngisdn_rcv_con_ind with ftdmchan->mutex being locked,
					so no other threads will be able to touch this channel. The next time we will
					process this channel is in this function, and it should be in state COLLECT (set inside
					sngisdn_rcv_con_ind)*/
				sngisdn_rcv_con_ind(sngisdn_info->glare.suId, sngisdn_info->glare.suInstId, sngisdn_info->glare.spInstId, &sngisdn_info->glare.setup, sngisdn_info->glare.dChan, sngisdn_info->glare.ces);
			}
		}
		break;
	case FTDM_CHANNEL_STATE_TRANSFER:
		{
			/* sngisdn_transfer function will always result in a state change */
			sngisdn_transfer(ftdmchan);
			state_change++;
		}
		break;
	case FTDM_CHANNEL_STATE_RESTART:
		{
			/* IMPLEMENT ME */
		}
		break;
	case FTDM_CHANNEL_STATE_SUSPENDED:
		{
			/* IMPLEMENT ME */
		}
		break;
	case FTDM_CHANNEL_STATE_RESET:
		{
			sngisdn_snd_restart(ftdmchan);
		}
		break;
	default:
		{
			ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "unsupported sngisdn_rcvd state %s\n", ftdm_channel_state2str(ftdmchan->state));
		}
		break;
	}

	if (!state_change) {
		/* Acknowledge the state change */
		ftdm_channel_complete_state(ftdmchan);
	}
	
	/* If sngisdn_info->variables is not NULL, it means did not send any
	* sigevent to the user, therefore we have to free that hashtable */
	if (sngisdn_info->variables) {
		hashtable_destroy(sngisdn_info->variables);
		sngisdn_info->variables = NULL;
	}

	/* If sngisdn_info->raw_data is not NULL, it means did not send any
	* sigevent to the user, therefore we have to free that raw data */
	if (sngisdn_info->raw_data) {
		ftdm_safe_free(sngisdn_info->raw_data);
		sngisdn_info->raw_data = NULL;
		sngisdn_info->raw_data_len = 0;
	}

	if (ftdmchan->state == initial_state) {
		ftdm_assert(!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE), "state change flag is still set, but we did not change state\n");
	}
#ifdef FTDM_DEBUG_CHAN_MEMORY
	if (ftdmchan->state == FTDM_CHANNEL_STATE_DIALING) {
		ftdm_assert(mprotect(ftdmchan, sizeof(*ftdmchan), PROT_READ|PROT_WRITE) == 0, "Failed to mprotect");
	}
#endif
	return FTDM_SUCCESS;
}

static FIO_CHANNEL_INDICATE_FUNCTION(ftdm_sangoma_isdn_indicate)
{
	ftdm_status_t status = FTDM_FAIL;

	switch (indication) {
		case FTDM_CHANNEL_INDICATE_FACILITY:
			sngisdn_snd_fac_req(ftdmchan);
			status = FTDM_SUCCESS;
			break;
		default:
			status = FTDM_NOTIMPL;
	}	
	return status;
}

static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(ftdm_sangoma_isdn_outgoing_call)
{
	sngisdn_chan_data_t  *sngisdn_info = ftdmchan->call_data;
	ftdm_status_t status = FTDM_FAIL;	

	if (ftdmchan->state == FTDM_CHANNEL_STATE_DOWN) {
		if (sngisdn_test_flag(sngisdn_info, FLAG_GLARE)) {
			/* A call came in after we called ftdm_channel_open_chan for this call, but before we got here */
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Glare detected - aborting outgoing call\n");

			sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);

			status = FTDM_BREAK;
		} else {
			status = FTDM_SUCCESS;
		}
	} else {
		/* the channel is already used...this can't be, end the request */
		ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Outgoing call requested channel in already in use (%s)\n", ftdm_channel_state2str(ftdmchan->state));
		status = FTDM_BREAK;
	}
	
	return status;
}
static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(ftdm_sangoma_isdn_get_chan_sig_status)
{
	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP)) {
		*status = FTDM_SIG_STATE_UP;
	} else {
		*status = FTDM_SIG_STATE_DOWN;
	}

	return FTDM_SUCCESS;
}

static FIO_CHANNEL_SET_SIG_STATUS_FUNCTION(ftdm_sangoma_isdn_set_chan_sig_status)
{
	ftdm_log(FTDM_LOG_ERROR,"Cannot set channel status in this module\n");
	return FTDM_NOTIMPL;
}

static FIO_SPAN_GET_SIG_STATUS_FUNCTION(ftdm_sangoma_isdn_get_span_sig_status)
{	
	if (ftdm_test_flag(span->channels[1], FTDM_CHANNEL_SIG_UP)) {
		*status = FTDM_SIG_STATE_UP;
	} else {
		*status = FTDM_SIG_STATE_DOWN;
	}

	return FTDM_SUCCESS;
}

static FIO_SPAN_SET_SIG_STATUS_FUNCTION(ftdm_sangoma_isdn_set_span_sig_status)
{
	ftdm_log(FTDM_LOG_ERROR,"Cannot set span status in this module\n");
	return FTDM_NOTIMPL;
}

static ftdm_status_t ftdm_sangoma_isdn_dtmf(ftdm_channel_t *ftdmchan, const char* dtmf)
{
	sngisdn_chan_data_t *sngisdn_info = ftdmchan->call_data;
	switch(sngisdn_info->transfer_data.type) {
		case SNGISDN_TRANSFER_ATT_COURTESY_VRU:
		case SNGISDN_TRANSFER_ATT_COURTESY_VRU_DATA:
			return sngisdn_att_transfer_process_dtmf(ftdmchan, dtmf);
		default:
			/* We do not care about DTMF events, do nothing */
			break;
	}

	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_sangoma_isdn_perform_start(ftdm_span_t *span)
{
	sngisdn_span_data_t *signal_data = span->signal_data;

	ftdm_log(FTDM_LOG_DEBUG, "Actually starting span:%s\n", span->name);
	/* clear the monitor thread stop flag */
	ftdm_clear_flag(span, FTDM_SPAN_STOP_THREAD);
	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

	if (signal_data->trace_q921 == SNGISDN_OPT_TRUE ||
		signal_data->raw_trace_q921 == SNGISDN_OPT_TRUE) {
		
		sngisdn_activate_trace(span, SNGISDN_TRACE_Q921);
	}
	
	if (signal_data->trace_q931 == SNGISDN_OPT_TRUE ||
		signal_data->raw_trace_q931 == SNGISDN_OPT_TRUE) {

		sngisdn_activate_trace(span, SNGISDN_TRACE_Q931);
	}

	/*start the span monitor thread*/
	if (ftdm_thread_create_detached(ftdm_sangoma_isdn_run, span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT,"Failed to start Sangoma ISDN Span Monitor Thread!\n");
		return FTDM_FAIL;
	}

	/*start the dchan monitor thread*/
	if (ftdm_thread_create_detached(ftdm_sangoma_isdn_io_run, span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT,"Failed to start Sangoma ISDN d-channel Monitor Thread!\n");
		return FTDM_FAIL;
	}

	if (signal_data->restart_timeout) {
		ftdm_log(FTDM_LOG_DEBUG, "%s:Scheduling Restart timeout\n", signal_data->ftdm_span->name);
		ftdm_sched_timer(signal_data->sched, "restart_timeout", signal_data->restart_timeout,
						sngisdn_restart_timeout, (void*) signal_data, &signal_data->timers[SNGISDN_SPAN_TIMER_RESTART]);
	}

	ftdm_log(FTDM_LOG_DEBUG,"Finished starting span %s\n", span->name);
	return FTDM_SUCCESS;
}


static ftdm_status_t ftdm_sangoma_isdn_start(ftdm_span_t *span)
{
	sngisdn_span_data_t *signal_data = span->signal_data;

	ftdm_log(FTDM_LOG_INFO,"Starting span %s:%u.\n",span->name,span->span_id);

	if (signal_data->dchan) {
		ftdm_channel_set_feature(signal_data->dchan, FTDM_CHANNEL_FEATURE_IO_STATS);
		ftdm_channel_open_chan(signal_data->dchan);
		ftdm_sangoma_isdn_dchan_set_queue_size(signal_data->dchan);
	}

	if (signal_data->nfas.trunk) {
		if (signal_data->nfas.trunk->num_spans == signal_data->nfas.trunk->num_spans_configured) {
			int i;
			ftdm_log(FTDM_LOG_DEBUG, "Starting span for all spans within trunkgroup:%s\n", signal_data->nfas.trunk->name);

			sngisdn_stack_start(signal_data->nfas.trunk->dchan->ftdm_span);
			ftdm_sangoma_isdn_perform_start(signal_data->nfas.trunk->dchan->ftdm_span);

			if (signal_data->nfas.trunk->backup) {
				sngisdn_stack_start(signal_data->nfas.trunk->backup->ftdm_span);
				ftdm_sangoma_isdn_perform_start(signal_data->nfas.trunk->backup->ftdm_span);
			}
			
			for (i = 0; i < signal_data->nfas.trunk->num_spans; i++) {
				if (signal_data->nfas.trunk->spans[i] &&
					signal_data->nfas.trunk->spans[i]->nfas.sigchan == SNGISDN_NFAS_DCHAN_NONE) {
					sngisdn_stack_start(signal_data->nfas.trunk->spans[i]->ftdm_span);
					ftdm_sangoma_isdn_perform_start(signal_data->nfas.trunk->spans[i]->ftdm_span);
				}
			}

			return FTDM_SUCCESS;
		} else {
			ftdm_log(FTDM_LOG_DEBUG, "Delaying span start until all spans within trunkgroup are started: %s\n", signal_data->nfas.trunk->name);
			return FTDM_SUCCESS;
		}
	}

	if (sngisdn_stack_start(span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to start span %s\n", span->name);
		return FTDM_FAIL;
	}

	ftdm_sangoma_isdn_perform_start(span);
	
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_sangoma_isdn_stop(ftdm_span_t *span)
{	
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *curr = NULL;
	unsigned i;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) span->signal_data;
	ftdm_log(FTDM_LOG_INFO, "Stopping span %s\n", span->name);
	
	/* throw the STOP_THREAD flag to signal monitor thread stop */
	ftdm_set_flag(span, FTDM_SPAN_STOP_THREAD);

	/* wait for the thread to stop */
	while (ftdm_test_flag(span, FTDM_SPAN_IN_THREAD)) {
		ftdm_log(FTDM_LOG_DEBUG, "Waiting for monitor thread to end for span %s\n", span->name);
		ftdm_sleep(10);
	}

	if (sngisdn_stack_stop(span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to stop span %s\n", span->name);
	}
	
	chaniter = ftdm_span_get_chan_iterator(span, NULL);
	for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
		ftdm_safe_free(((ftdm_channel_t*)ftdm_iterator_current(curr))->call_data);
		((ftdm_channel_t*)ftdm_iterator_current(curr))->call_data = NULL;
	}
	ftdm_iterator_free(chaniter);

	ftdm_sched_destroy(&signal_data->sched);
	ftdm_queue_destroy(&signal_data->event_queue);
	for (i = 0 ; i < signal_data->num_local_numbers ; i++) {
		if (signal_data->local_numbers[i] != NULL) {
			ftdm_safe_free(signal_data->local_numbers[i]);
		}
	}
	ftdm_safe_free(span->signal_data);

	ftdm_log(FTDM_LOG_DEBUG, "Finished stopping span %s\n", span->name);

	return FTDM_SUCCESS;
}

static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_sangoma_isdn_span_config)
{
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *curr = NULL;

	sngisdn_span_data_t *signal_data;
	
	ftdm_log(FTDM_LOG_INFO, "Configuring ftmod_sangoma_isdn span = %s\n", span->name);	

	signal_data = ftdm_calloc(1, sizeof(sngisdn_span_data_t));
	signal_data->ftdm_span = span;
	span->signal_data = signal_data;
	
	chaniter = ftdm_span_get_chan_iterator(span, NULL);
	for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
		sngisdn_chan_data_t *chan_data = ftdm_calloc(1, sizeof(sngisdn_chan_data_t));
		chan_data->ftdmchan = ((ftdm_channel_t*)ftdm_iterator_current(curr));
		((ftdm_channel_t*)ftdm_iterator_current(curr))->call_data = chan_data;
		
	}
	ftdm_iterator_free(chaniter);

	if (ftmod_isdn_parse_cfg(ftdm_parameters, span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to parse configuration\n");
		return FTDM_FAIL;
	}

	if (signal_data->nfas.trunk) {
		if (signal_data->nfas.trunk->num_spans == ++signal_data->nfas.trunk->num_spans_configured) {
			int i;
			ftdm_log(FTDM_LOG_DEBUG, "Starting stack configuration for all spans within trunkgroup:%s\n", signal_data->nfas.trunk->name);

			sngisdn_stack_cfg(signal_data->nfas.trunk->dchan->ftdm_span);
			if (signal_data->nfas.trunk->backup) {
				sngisdn_stack_cfg(signal_data->nfas.trunk->backup->ftdm_span);
			}
			
			for (i = 0; i < signal_data->nfas.trunk->num_spans; i++) {
				if (signal_data->nfas.trunk->spans[i] &&
					signal_data->nfas.trunk->spans[i]->nfas.sigchan == SNGISDN_NFAS_DCHAN_NONE) {
					sngisdn_stack_cfg(signal_data->nfas.trunk->spans[i]->ftdm_span);
				}
			}
		} else {
			ftdm_log(FTDM_LOG_DEBUG, "Delaying span stack configuration until all spans within trunkgroup are started:%s\n", signal_data->nfas.trunk->name);
		}
	} else if (sngisdn_stack_cfg(span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Sangoma ISDN Stack configuration failed\n");
		return FTDM_FAIL;
	}

	if (signal_data->cid_name_method == SNGISDN_CID_NAME_AUTO) {
		switch (signal_data->switchtype) {
			case SNGISDN_SWITCH_EUROISDN:
				if (FTDM_SPAN_IS_BRI(span)) {
					signal_data->cid_name_method = SNGISDN_CID_NAME_USR_USR_IE;
				} else {
					signal_data->cid_name_method = SNGISDN_CID_NAME_DISPLAY_IE;
				}
				break;
			case SNGISDN_SWITCH_DMS100:
				signal_data->cid_name_method = SNGISDN_CID_NAME_DISPLAY_IE;
				break;
			case SNGISDN_SWITCH_NI2:
			case SNGISDN_SWITCH_5ESS:
			case SNGISDN_SWITCH_4ESS:
				signal_data->cid_name_method = SNGISDN_CID_NAME_FACILITY_IE;
				break;
			default:
				break;
		}
	}

	if (signal_data->send_cid_name == SNGISDN_OPT_DEFAULT) {
		switch (signal_data->switchtype) {
			case SNGISDN_SWITCH_EUROISDN:
#ifdef SNGISDN_SUPPORT_CALLING_NAME_IN_FACILITY
			case SNGISDN_SWITCH_NI2:
			case SNGISDN_SWITCH_5ESS:
			case SNGISDN_SWITCH_4ESS:
#endif
				if (signal_data->signalling == SNGISDN_SIGNALING_NET) {
					signal_data->send_cid_name = SNGISDN_OPT_TRUE;
				} else {
					signal_data->send_cid_name = SNGISDN_OPT_FALSE;
				}
				break;
			case SNGISDN_SWITCH_DMS100:
				signal_data->send_cid_name = SNGISDN_OPT_TRUE;
				break;
#ifndef SNGISDN_SUPPORT_CALLING_NAME_IN_FACILITY
			case SNGISDN_SWITCH_NI2:
			case SNGISDN_SWITCH_5ESS:
			case SNGISDN_SWITCH_4ESS:
				signal_data->send_cid_name = SNGISDN_OPT_FALSE;
				break;
#endif
			default:
				signal_data->send_cid_name = SNGISDN_OPT_FALSE;
				break;
		}
	} else if (signal_data->send_cid_name == SNGISDN_OPT_TRUE) {
		switch (signal_data->switchtype) {
			case SNGISDN_SWITCH_NI2:
			case SNGISDN_SWITCH_5ESS:
			case SNGISDN_SWITCH_4ESS:
#ifndef SNGISDN_SUPPORT_CALLING_NAME_IN_FACILITY
				ftdm_log(FTDM_LOG_WARNING, "Sending Calling Name in Facility IE not supported, please update your libsng_isdn library\n");
				signal_data->send_cid_name = SNGISDN_OPT_FALSE;
#endif
				break;
			case SNGISDN_SWITCH_INSNET: /* Don't know how to transmit caller ID name on INSNET */
			case SNGISDN_SWITCH_QSIG: /* It seems like QSIG does not support Caller ID */
				signal_data->send_cid_name = SNGISDN_OPT_FALSE;
				break;
			case SNGISDN_SWITCH_EUROISDN:
				break;
			default:
				signal_data->send_cid_name = SNGISDN_OPT_FALSE;
				break;
		}
	}

	span->start = ftdm_sangoma_isdn_start;
	span->stop = ftdm_sangoma_isdn_stop;
	span->signal_type = FTDM_SIGTYPE_ISDN;
	span->outgoing_call = ftdm_sangoma_isdn_outgoing_call;
	span->indicate = ftdm_sangoma_isdn_indicate;
	span->channel_request = NULL;
	span->signal_cb	= sig_cb;
	span->sig_dtmf = ftdm_sangoma_isdn_dtmf;
	span->get_channel_sig_status = ftdm_sangoma_isdn_get_chan_sig_status;
	span->set_channel_sig_status = ftdm_sangoma_isdn_set_chan_sig_status;
	span->get_span_sig_status = ftdm_sangoma_isdn_get_span_sig_status;
	span->set_span_sig_status = ftdm_sangoma_isdn_set_span_sig_status;
	span->state_map	= &sangoma_isdn_state_map;
	span->state_processor = ftdm_sangoma_isdn_process_state_change;
	ftdm_set_flag(span, FTDM_SPAN_USE_CHAN_QUEUE);
	ftdm_set_flag(span, FTDM_SPAN_USE_SIGNALS_QUEUE);
	ftdm_set_flag(span, FTDM_SPAN_USE_PROCEED_STATE);
	ftdm_set_flag(span, FTDM_SPAN_USE_SKIP_STATES);
	ftdm_set_flag(span, FTDM_SPAN_NON_STOPPABLE);
	ftdm_set_flag(span, FTDM_SPAN_USE_TRANSFER);

	if (FTDM_SPAN_IS_BRI(span)) {
		sngisdn_set_span_avail_rate(span, SNGISDN_AVAIL_PWR_SAVING);
	}

	/* Initialize scheduling context */
	ftdm_assert(ftdm_sched_create(&((sngisdn_span_data_t*)span->signal_data)->sched, "sngisdn_schedule") == FTDM_SUCCESS, "Failed to create a new schedule!!");

	/* Initialize the event queue */
	ftdm_assert(ftdm_queue_create(&((sngisdn_span_data_t*)span->signal_data)->event_queue, SNGISDN_EVENT_QUEUE_SIZE) == FTDM_SUCCESS, "Failed to create a new queue!!");

	ftdm_log(FTDM_LOG_INFO, "Finished configuring ftmod_sangoma_isdn span = %s\n", span->name);
	return FTDM_SUCCESS;
}

static FIO_SIG_LOAD_FUNCTION(ftdm_sangoma_isdn_init)
{
	unsigned i;
	ftdm_log(FTDM_LOG_INFO, "Loading ftmod_sangoma_isdn...\n");

	memset(&g_sngisdn_data, 0, sizeof(g_sngisdn_data));
	memset(&g_sngisdn_event_interface, 0, sizeof(g_sngisdn_event_interface));
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
	g_sngisdn_event_interface.cc.sng_srv_cfm	= sngisdn_rcv_srv_cfm;
	g_sngisdn_event_interface.cc.sng_rst_ind 	= sngisdn_rcv_rst_ind;
	g_sngisdn_event_interface.cc.sng_rst_cfm 	= sngisdn_rcv_rst_cfm;

	g_sngisdn_event_interface.lg.sng_log 		= sngisdn_rcv_sng_log;
	g_sngisdn_event_interface.lg.sng_assert 	= sngisdn_rcv_sng_assert;
	
	g_sngisdn_event_interface.sta.sng_phy_sta_ind 	= sngisdn_rcv_phy_ind;
	g_sngisdn_event_interface.sta.sng_q921_sta_ind	= sngisdn_rcv_q921_ind;
	g_sngisdn_event_interface.sta.sng_q921_trc_ind	= sngisdn_rcv_q921_trace;
	g_sngisdn_event_interface.sta.sng_q931_sta_ind	= sngisdn_rcv_q931_ind;
	g_sngisdn_event_interface.sta.sng_q931_trc_ind	= sngisdn_rcv_q931_trace;
	g_sngisdn_event_interface.sta.sng_cc_sta_ind	= sngisdn_rcv_cc_ind;

	g_sngisdn_event_interface.io.sng_l1_data_req	= sngisdn_rcv_l1_data_req;
	g_sngisdn_event_interface.io.sng_l1_cmd_req		= sngisdn_rcv_l1_cmd_req;
	
	for(i=1;i<=MAX_VARIANTS;i++) {		
		ftdm_mutex_create(&g_sngisdn_data.ccs[i].mutex);
	}
	
	/* initalize sng_isdn library */
	ftdm_assert_return(!sng_isdn_init(&g_sngisdn_event_interface), FTDM_FAIL, "Failed to initialize stack\n");

	/* Load Stack General Configuration */
	sngisdn_start_gen_cfg();

	return FTDM_SUCCESS;
}

static FIO_SIG_UNLOAD_FUNCTION(ftdm_sangoma_isdn_unload)
{
	unsigned i;
	ftdm_log(FTDM_LOG_INFO, "Starting ftmod_sangoma_isdn unload...\n");

	sng_isdn_free();
	
	for(i=1;i<=MAX_VARIANTS;i++) {		
		ftdm_mutex_destroy(&g_sngisdn_data.ccs[i].mutex);
	}

	ftdm_log(FTDM_LOG_INFO, "Finished ftmod_sangoma_isdn unload!\n");
	return FTDM_SUCCESS;
}

#define SANGOMA_ISDN_API_USAGE_TRACE 			"ftdm sangoma_isdn trace <q921|q931> <span name>\n"
#define SANGOMA_ISDN_API_USAGE_SHOW_L1_STATS	"ftdm sangoma_isdn l1_stats <span name>\n"
#define SANGOMA_ISDN_API_USAGE_SHOW_SPANS		"ftdm sangoma_isdn show_spans [<span name>]\n"

#define SANGOMA_ISDN_API_USAGE	"\t"SANGOMA_ISDN_API_USAGE_TRACE \
								"\t"SANGOMA_ISDN_API_USAGE_SHOW_L1_STATS \
								"\t"SANGOMA_ISDN_API_USAGE_SHOW_SPANS

static FIO_API_FUNCTION(ftdm_sangoma_isdn_api)
{
	ftdm_status_t status = FTDM_EINVAL;
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

	/* TODO: Move functions to table + function pointers */
	if (!strcasecmp(argv[0], "trace")) {
		char *trace_opt;
		
		ftdm_span_t *span;

		if (argc < 3) {
			ftdm_log(FTDM_LOG_ERROR, "Usage: %s\n", SANGOMA_ISDN_API_USAGE_TRACE);
			status = FTDM_FAIL;
			goto done;
		}
		trace_opt = argv[1];
		
		status = ftdm_span_find_by_name(argv[2], &span);
		if (FTDM_SUCCESS != status) {
			stream->write_function(stream, "-ERR failed to find span by name %s\n", argv[2]);

			status = FTDM_FAIL;
			goto done;
		}
		
		if (!strcasecmp(trace_opt, "q921")) {
			status = sngisdn_activate_trace(span, SNGISDN_TRACE_Q921);
		} else if (!strcasecmp(trace_opt, "q931")) {
			status = sngisdn_activate_trace(span, SNGISDN_TRACE_Q931);
		} else if (!strcasecmp(trace_opt, "disable")) {
			status = sngisdn_activate_trace(span, SNGISDN_TRACE_DISABLE);
		} else {
			stream->write_function(stream, "-ERR invalid trace option <q921|q931> <span name>\n");
			status = FTDM_FAIL;
		}
		goto done;
	}
	
	if (!strcasecmp(argv[0], "l1_stats")) {
		ftdm_span_t *span;
		if (argc < 2) {
			stream->write_function(stream, "Usage: %s\n", SANGOMA_ISDN_API_USAGE_SHOW_L1_STATS);
			status = FTDM_FAIL;
			goto done;
		}
		status = ftdm_span_find_by_name(argv[1], &span);
		if (FTDM_SUCCESS != status) {
			stream->write_function(stream, "-ERR failed to find span with name %s\n", argv[1]);

			status = FTDM_FAIL; 
			goto done;
		}
		status = sngisdn_show_l1_stats(stream, span);
		goto done;
	}
	
	if (!strcasecmp(argv[0], "show_spans")) {
		ftdm_span_t *span = NULL;
		if (argc == 2) {
			status = ftdm_span_find_by_name(argv[1], &span);
			if (FTDM_SUCCESS != status) {
				stream->write_function(stream, "-ERR failed to find span with name %s\n", argv[1]);
				
				stream->write_function(stream, "Usage: %s\n", SANGOMA_ISDN_API_USAGE_SHOW_SPANS);
				status = FTDM_FAIL;
				goto done;
			}
			status = sngisdn_show_span(stream, span);
			goto done;
		}
		status = sngisdn_show_spans(stream);
		goto done;
	}
	
	if (!strcasecmp(argv[0], "check_ids")) {
		status = sngisdn_check_free_ids();
		goto done;
	}
	if (!strcasecmp(argv[0], "check_mem")) {
		sngisdn_get_memory_info();
	}
done:
	switch (status) {
		case FTDM_SUCCESS:
			stream->write_function(stream, "Command executed OK\n");
			break;
		case FTDM_EINVAL:
			stream->write_function(stream, "Invalid arguments [%s]\n", mycmd);
			stream->write_function(stream, "Usage:\n%s\n", SANGOMA_ISDN_API_USAGE);
			break;		
		default:
			/* FTDM_FAIL - Do nothing since we already printed the cause of the error */
			break;
	}
	
	/* Return SUCCESS because we do not want to print the general FTDM usage list */
	status = FTDM_SUCCESS;

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

EX_DECLARE_DATA ftdm_module_t ftdm_module =
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


